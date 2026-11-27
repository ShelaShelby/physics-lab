// 0.94.0: NASA SDO/HMI's photo of the day (see sun_photo.h for the source, the terms and the geometry).
#include "sun_photo.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
#include "render/planet_maps.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace orbit {

namespace {
constexpr double kPi = 3.14159265358979323846;
double civilToJD(int y, int mo, int d, double dayFrac) {   // Meeus, Astronomical Algorithms (2nd ed.), ch. 7, Gregorian calendar
    if (mo <= 2) { y -= 1; mo += 12; }
    const int A = y / 100, B = 2 - A + A / 4;
    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (mo + 1)) + d + dayFrac + B - 1524.5;
}
void jdToCivil(double jd, int& y, int& mo, int& d) {      // Meeus ch. 7 (the day's fraction dropped)
    const double z = std::floor(jd + 0.5);
    const double alpha = std::floor((z - 1867216.25) / 36524.25);
    const double A = z + 1 + alpha - std::floor(alpha / 4), B = A + 1524, C = std::floor((B - 122.1) / 365.25), D = std::floor(365.25 * C), E = std::floor((B - D) / 30.6001);
    d = static_cast<int>(B - D - std::floor(30.6001 * E));
    mo = static_cast<int>(E < 14 ? E - 1 : E - 13);
    y = static_cast<int>(mo > 2 ? C - 4716 : C - 4715);
}
bool readFile(const std::filesystem::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream s; s << f.rdbuf();
    out = s.str();
    return true;
}
bool writeFileAtomic(const std::filesystem::path& p, const std::string& data) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::filesystem::path tmp = p; tmp += ".part";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!f) return false;
    }
    std::filesystem::rename(tmp, p, ec);
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}
bool xmlNumber(const std::string& xml, const char* key, double& v) {
    const std::string open = std::string("<") + key + ">";
    const std::size_t i = xml.find(open);
    if (i == std::string::npos) return false;
    const std::size_t a = i + open.size(), b = xml.find('<', a);
    if (b == std::string::npos) return false;
    const std::string s = xml.substr(a, b - a);
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end && end != s.c_str() && std::isfinite(v);
}
std::string jsonString(const std::string& json, const char* key) {
    const std::string k = std::string("\"") + key + "\":";
    std::size_t i = json.find(k);
    if (i == std::string::npos) return "";
    i += k.size();
    while (i < json.size() && json[i] == ' ') i++;
    if (i < json.size() && json[i] == '"') {
        const std::size_t e = json.find('"', i + 1);
        return e == std::string::npos ? "" : json.substr(i + 1, e - i - 1);
    }
    std::size_t e = i;
    while (e < json.size() && json[e] != ',' && json[e] != '}') e++;
    return json.substr(i, e - i);
}
// least squares: grey(mu) = Sum a_n mu^n, n = 0..5 (normal equations, Gaussian elimination with partial pivoting)
bool fitQuintic(const std::vector<double>& mu, const std::vector<double>& y, double a[6]) {
    double M[6][7] = {};
    for (std::size_t k = 0; k < mu.size(); k++) {
        double p[6]; p[0] = 1; for (int n = 1; n < 6; n++) p[n] = p[n - 1] * mu[k];
        for (int i = 0; i < 6; i++) { for (int j = 0; j < 6; j++) M[i][j] += p[i] * p[j]; M[i][6] += p[i] * y[k]; }
    }
    for (int c = 0; c < 6; c++) {
        int piv = c;
        for (int r = c + 1; r < 6; r++) if (std::fabs(M[r][c]) > std::fabs(M[piv][c])) piv = r;
        if (std::fabs(M[piv][c]) < 1e-300) return false;
        for (int j = 0; j < 7; j++) std::swap(M[c][j], M[piv][j]);
        for (int r = 0; r < 6; r++) if (r != c) { const double f = M[r][c] / M[c][c]; for (int j = 0; j < 7; j++) M[r][j] -= f * M[c][j]; }
    }
    for (int i = 0; i < 6; i++) a[i] = M[i][6] / M[i][i];
    return true;
}
double sample(const SunPhoto& p, double u, double v) {   // bilinear, pixel centres at whole numbers, clamped at the edges (as the GPU's sampler)
    u = std::clamp(u, 0.0, p.w - 1.0); v = std::clamp(v, 0.0, p.h - 1.0);
    const int i = std::min(static_cast<int>(u), p.w - 2), j = std::min(static_cast<int>(v), p.h - 2);
    const double fu = u - i, fv = v - j;
    const auto g = [&](int x, int y) { return static_cast<double>(p.grey[std::size_t(y) * std::size_t(p.w) + std::size_t(x)]); };
    return (1 - fv) * ((1 - fu) * g(i, j) + fu * g(i + 1, j)) + fv * ((1 - fu) * g(i, j + 1) + fu * g(i + 1, j + 1));
}
}  // namespace

double jdFromUtText(const std::string& s) {
    int y = 0, mo = 0, d = 0, hh = 0, mm = 0;
    double ss = 0;
    char sep = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%d%c%d:%d:%lf", &y, &mo, &d, &sep, &hh, &mm, &ss) != 7 || (sep != ' ' && sep != 'T') || mo < 1 || mo > 12 || d < 1 || d > 31)
        return std::numeric_limits<double>::quiet_NaN();
    return civilToJD(y, mo, d, (hh + (mm + ss / 60) / 60) / 24);
}
std::string photoDayKey(double jdUT) {
    int y, mo, d;
    jdToCivil(std::floor(jdUT) + 0.5, y, mo, d);
    char b[16];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d", y, mo, d);
    return b;
}
double photoDayJD(const std::string& key) { return jdFromUtText(key + " 00:00:00"); }

bool parseClosestReply(const std::string& json, SunPhotoMeta& m, std::string& why) {
    const std::string id = jsonString(json, "id"), date = jsonString(json, "date"), name = jsonString(json, "name");
    if (id.empty() || date.empty()) { why = "Helioviewer's reply has no picture"; return false; }
    if (name != "HMI Int") { why = "Helioviewer's reply is not HMI's continuum picture (" + name + ")"; return false; }
    m.id = std::atoll(id.c_str());
    m.date = date;
    m.jd = jdFromUtText(date);
    if (m.id <= 0 || !std::isfinite(m.jd)) { why = "Helioviewer's reply cannot be read"; return false; }
    return true;
}

bool parseHeaderXml(const std::string& xml, SunPhotoMeta& m, std::string& why) {
    double n1 = 0, n2 = 0;
    if (!xmlNumber(xml, "NAXIS1", n1) || !xmlNumber(xml, "NAXIS2", n2) || !xmlNumber(xml, "CRPIX1", m.crpix1) || !xmlNumber(xml, "CRPIX2", m.crpix2) ||
        !xmlNumber(xml, "CDELT1", m.cdelt1) || !xmlNumber(xml, "RSUN_OBS", m.rsunObs) || !xmlNumber(xml, "CRLN_OBS", m.crlnObs) || !xmlNumber(xml, "CRLT_OBS", m.crltObs) ||
        !xmlNumber(xml, "CROTA2", m.crota2) || !xmlNumber(xml, "WAVELNTH", m.wavelnth)) { why = "the picture's header lacks a key"; return false; }
    m.naxis1 = static_cast<int>(n1); m.naxis2 = static_cast<int>(n2);
    if (xml.find("<CONTENT>CONTINUUM INTENSITY") == std::string::npos) { why = "the header does not say continuum intensity"; return false; }
    if (std::fabs(m.crota2) > 0.01) { why = "the picture is turned (CROTA2 not 0)"; return false; }
    if (m.naxis1 <= 0 || m.naxis2 <= 0 || m.cdelt1 <= 0 || m.rsunObs <= 0) { why = "the header's sizes cannot be used"; return false; }
    return true;
}

SunPhoto loadSunPhoto(const std::vector<std::uint8_t>& png, const std::string& xml, const std::string& json) {
    SunPhoto p;
    if (!parseClosestReply(json, p.meta, p.why)) return p;
    if (!parseHeaderXml(xml, p.meta, p.why)) return p;
    if (!render::decodeGreyPng(png.data(), png.size(), p.w, p.h, p.grey)) { p.why = "the picture cannot be read"; return p; }
    // the PNG is the picture shrunk by a whole factor (Helioviewer downloadImage's scale): 1, 2, 4 or 8
    p.scale = p.w > 0 ? p.meta.naxis1 / p.w : 0;
    if (p.w < 64 || (p.scale != 1 && p.scale != 2 && p.scale != 4 && p.scale != 8) || p.w * p.scale != p.meta.naxis1 || p.h * p.scale != p.meta.naxis2) {
        p.why = "the picture's size does not match its header"; p.grey.clear(); return p;
    }
    const double s = p.scale;
    p.cx = (p.meta.crpix1 - (s + 1) / 2) / s;                       // FITS pixel centres at 1, 2, ...; a PNG pixel is the block of s x s
    p.cy = (p.meta.naxis2 - (s - 1) / 2 - p.meta.crpix2) / s;       // FITS y up, PNG rows down
    p.rpx = p.meta.rsunObs / (p.meta.cdelt1 * s);
    if (p.rpx < 20 || p.cx - p.rpx < -2 || p.cx + p.rpx > p.w + 1 || p.cy - p.rpx < -2 || p.cy + p.rpx > p.h + 1) { p.why = "the Sun is not inside the picture"; p.grey.clear(); return p; }
    const double L0 = p.meta.crlnObs * kPi / 180, B0 = p.meta.crltObs * kPi / 180;
    p.O[0] = std::cos(B0) * std::cos(L0); p.O[1] = std::cos(B0) * std::sin(L0); p.O[2] = std::sin(B0);
    p.W[0] = -std::sin(L0); p.W[1] = std::cos(L0); p.W[2] = 0;
    p.N[0] = -std::sin(B0) * std::cos(L0); p.N[1] = -std::sin(B0) * std::sin(L0); p.N[2] = std::cos(B0);
    // the limb fit: the median of each ring 0.005 of the radius wide, out to 0.99 (mu 0.14; spots and faculae fall outside the middle of a ring's values)
    constexpr int kRings = 198;
    std::vector<std::vector<std::uint8_t>> ring(kRings);
    for (int j = 0; j < p.h; j++)
        for (int i = 0; i < p.w; i++) {
            const double r = std::hypot(i - p.cx, j - p.cy) / p.rpx;
            const int k = static_cast<int>(r / 0.005);
            if (k < kRings) ring[std::size_t(k)].push_back(p.grey[std::size_t(j) * std::size_t(p.w) + std::size_t(i)]);
        }
    std::vector<double> mus, ys;
    for (int k = 0; k < kRings; k++) {
        auto& v = ring[std::size_t(k)];
        if (v.size() < 8) continue;
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
        const double r = (k + 0.5) * 0.005;
        mus.push_back(std::sqrt(1 - r * r)); ys.push_back(v[v.size() / 2]);
    }
    if (mus.size() < 50 || !fitQuintic(mus, ys, p.fit) || photoFitAt(p, 1) < 20) { p.why = "the picture's edge darkening cannot be fitted"; p.grey.clear(); return p; }
    p.ok = true;
    return p;
}

double photoFitAt(const SunPhoto& p, double mu) { double s = 0, m = 1; for (int n = 0; n < 6; n++) { s += p.fit[n] * m; m *= mu; } return s; }

void photoAt(const SunPhoto& p, const double b[3], double& rel, double& weight) {
    rel = 1; weight = 0;
    if (!p.ok) return;
    const double mu = b[0] * p.O[0] + b[1] * p.O[1] + b[2] * p.O[2];
    if (mu <= kPhotoMuNone) return;
    const double t = std::clamp((mu - kPhotoMuNone) / (kPhotoMuFull - kPhotoMuNone), 0.0, 1.0);
    weight = t * t * (3 - 2 * t);
    const double x = b[0] * p.W[0] + b[1] * p.W[1] + b[2] * p.W[2], y = b[0] * p.N[0] + b[1] * p.N[1] + b[2] * p.N[2];
    rel = sample(p, p.cx + x * p.rpx, p.cy - y * p.rpx) / std::max(photoFitAt(p, mu), 1.0);
}

// ---- the download (Windows: WinHTTP) ----
namespace {
#ifdef _WIN32
bool httpGet(const std::string& path, std::string& body, std::string& why) {
    body.clear();
    const std::wstring wpath(path.begin(), path.end());   // the path is plain ASCII (a date, a number)
    HINTERNET s = WinHttpOpen(L"PhysicsLab", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { why = "no internet connection could be opened"; return false; }
    WinHttpSetTimeouts(s, 10000, 10000, 20000, 30000);
    HINTERNET c = WinHttpConnect(s, L"api.helioviewer.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr);
    DWORD code = 0, sz = sizeof(code);
    if (ok) ok = WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX) && code == 200;
    if (!ok) why = code ? "Helioviewer answered " + std::to_string(code) : "Helioviewer could not be reached";
    while (ok) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(r, &avail)) { ok = false; why = "the download was cut off"; break; }
        if (avail == 0) break;
        if (body.size() + avail > (32u << 20)) { ok = false; why = "the download is too large"; break; }
        const std::size_t at = body.size();
        body.resize(at + avail);
        DWORD got = 0;
        if (!WinHttpReadData(r, &body[at], avail, &got)) { ok = false; why = "the download was cut off"; break; }
        body.resize(at + got);
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}
#else
bool httpGet(const std::string&, std::string& body, std::string& why) { body.clear(); why = "this build does not download"; return false; }
#endif
}  // namespace

struct SunPhotoSource::Job {
    std::string day, dir;
    bool net = false;
    std::atomic<bool> done{false};
    std::thread th;
    SunPhoto photo;
    bool fromNet = false, noPhoto = false;
    double ms = 0;
    std::string why;
    void run() {
        const auto t0 = std::chrono::steady_clock::now();
        namespace fs = std::filesystem;
        const fs::path base = fs::u8path(dir);
        const fs::path fj = base / fs::u8path(day + ".json"), fx = base / fs::u8path(day + ".xml"), fp = base / fs::u8path(day + ".png");
        const double want = photoDayJD(day);
        std::string json, xml, png;
        SunPhotoMeta m;
        std::string w;
        const bool haveJson = !dir.empty() && readFile(fj, json);
        if (haveJson && parseClosestReply(json, m, w) && std::fabs(m.jd - want) > kPhotoMaxAwayDays) noPhoto = true;   // a saved "no picture near this day"
        else if (haveJson && readFile(fx, xml) && readFile(fp, png)) {
            photo = loadSunPhoto(std::vector<std::uint8_t>(png.begin(), png.end()), xml, json);
            if (!photo.ok) why = "the saved photo cannot be read (" + photo.why + ")";
        } else if (!net) {
            why = "not downloaded";
        } else {
            fromNet = true;
            if (!httpGet("/v2/getClosestImage/?date=" + day + "T00:00:00Z&sourceId=18", json, why)) {}
            else if (!parseClosestReply(json, m, why)) {}
            else if (std::fabs(m.jd - want) > kPhotoMaxAwayDays) { noPhoto = true; if (!dir.empty()) writeFileAtomic(fj, json); }
            else if (!httpGet("/v2/getJP2Header/?id=" + std::to_string(m.id), xml, why)) {}
            else if (!httpGet("/v2/downloadImage/?id=" + std::to_string(m.id) + "&scale=" + std::to_string(kPhotoScale) + "&type=png", png, why)) {}
            else {
                photo = loadSunPhoto(std::vector<std::uint8_t>(png.begin(), png.end()), xml, json);
                if (!photo.ok) why = "the downloaded photo cannot be read (" + photo.why + ")";
                else if (!dir.empty()) { writeFileAtomic(fx, xml); writeFileAtomic(fp, png); writeFileAtomic(fj, json); }   // the reply last: it marks the day complete
            }
        }
        ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        done.store(true, std::memory_order_release);
    }
};

SunPhotoSource& SunPhotoSource::get() {
    static SunPhotoSource s;
    static struct Joiner { ~Joiner() { auto& j = SunPhotoSource::get().job_; if (j && j->th.joinable()) j->th.join(); } } joiner;   // the worker ends before the app does
    return s;
}
void SunPhotoSource::setCacheDir(const std::string& dir) {
    const char* e = std::getenv("PL_SUNPHOTO_CACHE");
    dir_ = e && *e ? std::string(e) : dir;
}
bool SunPhotoSource::canDownload() const {
#ifdef _WIN32
    const char* e = std::getenv("PL_SUNPHOTO_OFFLINE");
    return !(e && *e == '1');
#else
    return false;
#endif
}
void SunPhotoSource::poll() {
    if (!job_ || !job_->done.load(std::memory_order_acquire)) return;
    if (job_->th.joinable()) job_->th.join();
    Job& j = *job_;
    last_ = SunPhotoLoad{true, j.fromNet, j.photo.ok, j.ms, j.day};
    if (j.photo.ok) { photo_ = std::make_unique<SunPhoto>(std::move(j.photo)); shownDay_ = j.day; version_++; }
    else if (j.noPhoto) noPhotoDay_ = j.day;
    else { failDay_ = j.day; failWhy_ = j.why; }
    job_.reset();
}
void SunPhotoSource::want(double jdUT, bool closeUp) {
    poll();
    if (!closeUp || held_ || !std::isfinite(jdUT)) return;
    const std::string day = photoDayKey(jdUT);
    wantDay_ = day;
    if (day == shownDay_ || day == noPhotoDay_ || day == failDay_ || job_) return;
    if (photoDayJD(day) < kPhotoFirstJD) return;
    job_ = std::make_shared<Job>();
    job_->day = day; job_->dir = dir_; job_->net = canDownload();
    Job* jp = job_.get();
    job_->th = std::thread([jp] { jp->run(); });
}
const SunPhoto* SunPhotoSource::shown() const { return !held_ && photo_ && shownDay_ == wantDay_ && !wantDay_.empty() ? photo_.get() : nullptr; }
void SunPhotoSource::dropMemory() {
    if (job_ && job_->th.joinable()) job_->th.join();
    job_.reset(); photo_.reset(); shownDay_.clear(); noPhotoDay_.clear(); failDay_.clear(); last_ = SunPhotoLoad();
}
void SunPhotoSource::dropCacheDay(const std::string& day) {
    if (dir_.empty()) return;
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const char* x : {".json", ".xml", ".png"}) fs::remove(fs::u8path(dir_) / fs::u8path(day + x), ec);
}

std::vector<std::string> SunPhotoSource::cardLines(double jdUT) const {
    std::vector<std::string> l;
    const std::string day = photoDayKey(jdUT);
    if (const SunPhoto* p = shown(); p && shownDay_ == day) {
        l.push_back("Surface: NASA SDO/HMI's photo of " + p->meta.date.substr(0, 16) + " UT, the one nearest 00:00 UT of the shown day (Courtesy of NASA/SDO and the AIA, EVE, "
                    "and HMI science teams; served by the Helioviewer Project)");
        l.push_back("The photo shows the real spots, bright faculae and grain of that day. It is taken at one wavelength (617.3 nm), so its contrast is not white light's");
        l.push_back("Only its brightness relative to its own edge darkening is used, given the Sun's measured colour and edge");
        l.push_back("It is seen from Earth's side at its own time (up to 12 hours from the shown time); the far side is drawn (granulation and the record's spots)");
        return l;
    }
    l.push_back("Surface: drawn. The granulation is drawn statistically: its cell size (1.2 Mm), contrast and lifetime (about 10 minutes) are measured "
                "(Hathaway et al. 2015; Mathew, Zakharov & Solanki 2009), the cells are not the real ones of that day");
    l.push_back("No faculae (bright patches) are drawn: no record gives their places (they show in NASA's photo)");
    const double jd0 = photoDayJD(day);
    if (jd0 < kPhotoFirstJD) l.push_back("NASA's photo of the surface: only from 6 Dec 2010 (SDO/HMI)");
    else if (day == noPhotoDay_) l.push_back("NASA's photo of the surface: none within an hour of 00:00 UT of this day");
    else if (job_ && job_->day == day) l.push_back("NASA's photo of the surface: loading");
    else if (day == failDay_) l.push_back("NASA's photo of the surface: " + failWhy_ + (canDownload() ? "" : " (no internet, or this build does not download)"));
    else l.push_back("NASA's photo of the surface: shown when you zoom in on the Sun");
    return l;
}

}  // namespace orbit
