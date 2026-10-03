#include "sky.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "card_text.h"
#include "date_text.h"
#include "format.h"
#include "solar_system.h"
#include "units.h"

namespace orbit {

using namespace units;

#include "sky_data.inc"

static constexpr int kFarStatic = sizeof(kFar) / sizeof(kFar[0]);
static constexpr int kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);

static int base36(const char* s, int n) {
    char buf[8] = {};
    std::memcpy(buf, s, n);
    return static_cast<int>(std::strtol(buf, nullptr, 36));
}

Vec3 lonLat(double lo, double la) {
    lo *= kPi / 180; la *= kPi / 180;
    return {std::cos(la) * std::cos(lo), std::cos(la) * std::sin(lo), std::sin(la)};
}

std::array<int, 3> bvRGB(double bv) {
    const double T = 4600 * (1 / (0.92 * bv + 1.7) + 1 / (0.92 * bv + 0.62)), t = T / 100;   // Ballesteros: temperature from B−V, K
    // then a blackbody colour (a common curve fit), each channel 0–255
    const double r = t <= 66 ? 255 : 329.698727446 * std::pow(t - 60, -0.1332047592);
    const double g = t <= 66 ? 99.4708025861 * std::log(t) - 161.1195681661 : 288.1221695283 * std::pow(t - 60, -0.0755148492);
    const double b = t >= 66 ? 255 : t <= 19 ? 0 : 138.5177312231 * std::log(t - 10) - 305.0447927307;
    std::array<int, 3> out;
    const double v[3] = {r, g, b};
    for (int k = 0; k < 3; k++) out[k] = static_cast<int>(std::lround(std::clamp(v[k], 0.0, 255.0) * 0.7 + 255 * 0.3));
    return out;
}

double spBV(const char* sp) {
    switch (sp && sp[0] ? sp[0] : 'G') {
    case 'O': return -0.3; case 'B': return -0.15; case 'A': return 0.05; case 'F': return 0.4;
    case 'G': return 0.65; case 'K': return 1.0; case 'M': return 1.5; default: return 0.65;
    }
}

// The night sky's map (see sky.h). Moved here from the drawing code so it can be tested.
bool skyScreen(const Vec3& v, double W, double H, double ca, double sa, double cb, double sb, double& X, double& Y) {
    const double cx = W / 2, cy = H / 2, K = std::hypot(W, H) / 2 / 1.75;           // K: points per radian away from the middle
    const double x1 = v.x * ca - v.y * sa, y1 = v.x * sa + v.y * ca, y2 = y1 * cb - v.z * sb, dep = y1 * sb + v.z * cb;
    const double th = std::acos(std::clamp(-dep, -1.0, 1.0));                    // the viewer is on the +depth side and looks the other way
    if (th > 1.75) return false;
    const double rho = std::max(std::hypot(x1, y2), 1e-9), r = th * K;
    X = cx + x1 / rho * r; Y = cy - y2 / rho * r;
    return true;
}

// The Milky Way band's points on the picture. Every sample that is on the map is drawn, as long as it has at least one neighbour on the map; the direction of the band
// there is taken from the neighbours on the map (both: the difference between them; one: the difference to it). The circle is closed, so sample 0 and sample
// kBandPieces are the same point and the neighbours wrap round. (Before 0.10.1 the neighbour test read the flags of points that the same loop had just switched
// off, so after the first gap in the visible stretch every later point was dropped: the band stopped, or was only a thin arc, or was missing.)
void milkyWayBand(const Sky& S, double W, double H, double ca, double sa, double cb, double sb, std::vector<BandPoint>& band) {
    const double cx = W / 2, cy = H / 2, K = std::hypot(W, H) / 2 / 1.75;
    const Vec3 A = S.galCentre, P = S.galPole, B = cross(P, A);
    const int n = kBandPieces;
    band.assign(n + 1, BandPoint{});
    std::vector<char> onMap(n + 1, 0);
    for (int i = 0; i <= n; i++) {
        const double l = i / double(n) * 2 * kPi, c = std::cos(l);
        BandPoint& p = band[i];
        onMap[i] = skyScreen(A * c + B * std::sin(l), W, H, ca, sa, cb, sb, p.X, p.Y);
        p.bright = 0.4 + 0.6 * (0.5 + 0.5 * c);
        p.fade = onMap[i] ? std::clamp((1.75 - std::hypot(p.X - cx, p.Y - cy) / K) / 0.2, 0.0, 1.0) : 0.0;   // fades out where the map ends
    }
    const double diag = std::hypot(W, H) / 2;
    for (int i = 0; i <= n; i++) {                                                // the direction along the band on screen, from the neighbours on the map
        if (!onMap[i]) continue;
        const int pv = i == 0 ? n - 1 : i - 1, nx = i == n ? 1 : i + 1;
        const BandPoint& p = band[i];
        const BandPoint& a = onMap[pv] ? band[pv] : p;                           // (a missing neighbour: use the point itself, a one-sided difference)
        const BandPoint& b = onMap[nx] ? band[nx] : p;
        if (!onMap[pv] && !onMap[nx]) continue;                                   // alone: no direction
        const double dx = b.X - a.X, dy = b.Y - a.Y, len = std::hypot(dx, dy);
        if (len < 1e-9 || len > 0.35 * diag) continue;                            // (a jump across the picture would be a break in the map; none happens inside the map)
        band[i].tx = -dy / len; band[i].ty = dx / len;                            // the unit normal
        band[i].ok = true;
    }
}

const Sky& skyData() {
    static Sky S;
    static bool made = false;
    if (made) return S;
    made = true;
    const std::size_t n = std::strlen(kSkyStars) / 10;
    S.stars.reserve(n);
    for (std::size_t i = 0; i < n; i++) {
        const char* t = kSkyStars + i * 10;
        const double lon = base36(t, 3) / 100.0, lat = base36(t + 3, 3) / 100.0 - 90, mag = base36(t + 6, 2) / 10.0 - 2, bv = base36(t + 8, 2) / 50.0 - 0.5;
        const int bucket = std::clamp(static_cast<int>(std::lround((bv + 0.3) / 2.3 * (kSkyColourBuckets - 1))), 0, kSkyColourBuckets - 1);
        S.stars.push_back({lonLat(lon, lat), static_cast<float>(mag), bucket});
    }
    for (int k = 0; k < kSkyColourBuckets; k++) S.colours[k] = bvRGB(-0.3 + 2.3 * k / (kSkyColourBuckets - 1));
    for (const char* L : kSkyLines) {
        std::vector<std::pair<int, int>> pairs;
        for (std::size_t k = 0; k + 6 <= std::strlen(L); k += 6) pairs.push_back({base36(L + k, 3), base36(L + k + 3, 3)});
        S.lines.push_back(std::move(pairs));
    }
    for (const auto& c : kSkyCons) S.cons.push_back({c.name, lonLat(c.lon, c.lat)});
    for (const auto& nm : kSkyNames) S.names.push_back({nm.star, nm.name});
    S.galCentre = lonLat(kGalCentre[0], kGalCentre[1]);
    S.galPole = lonLat(kGalPole[0], kGalPole[1]);
    return S;
}

int farCount() { return kFarStatic + kProbeCount; }

double voyagerKms() { return kProbes[0].kms; }
double voyagerYearsFromSun(double au) { return au / (kProbes[0].kms * kAuPerYearPerKms); }
int probeLaunchYear(const char* key) {
    for (int i = 0; i < kProbeCount; i++) if (std::strcmp(key, kProbes[i].key) == 0) return kProbes[i].year;
    return 0;
}
double farDistanceLy(const char* key) {
    for (int i = 0; i < kFarStatic; i++) if (std::strcmp(key, kFar[i].key) == 0) return kFar[i].ly;
    return 0;
}

int farIndexOf(const std::string& key) {
    for (int i = 0; i < kFarStatic; i++) if (key == kFar[i].key) return i;
    for (int i = 0; i < kProbeCount; i++) if (key == kProbes[i].key) return kFarStatic + i;
    return -1;
}

bool farItem(const SolarSystem& sys, int index, FarItem& f) {
    if (index < 0 || index >= farCount()) return false;
    f = FarItem{};
    f.index = index;
    if (index < kFarStatic) {
        const FarData& d = kFar[index];
        f.key = d.key; f.name = d.name; f.kind = d.kind; f.far = &d;
        f.dir = lonLat(d.lon, d.lat);
        f.ly = d.ly; f.au = d.ly * kLightYearAU;
        f.pos = f.dir * (f.au * kAU);
        return true;
    }
    const ProbeData& p = kProbes[index - kFarStatic];
    const double nowMs = sys.epochMs + sys.t * 1000.0;
    if (nowMs < kProbesShownFromMs) return false;              // before 1 Jan 1990: not shown (as in the web version)
    const double yrs = (nowMs - kSkyT0Ms) / (365.25 * 86400000.0);   // years since the positions were worked out
    f.key = p.key; f.name = p.name; f.kind = "probe"; f.probe = &p;
    f.dir = lonLat(p.lon, p.lat);
    f.au = p.au + p.kms * kAuPerYearPerKms * yrs;           // a straight line outwards at its real speed (0.21095 AU per year for each km/s)
    f.pos = f.dir * (f.au * kAU);
    return true;
}

std::string voyagerCaption(const SolarSystem& sys) {
    FarItem f;
    if (farItem(sys, farIndexOf("v1"), f))
        return "Voyager 1 is the most distant spacecraft: " + fmt::commas(f.au) + " AU out, " + fmt::fixed(f.au * 499.005 / 3600, 0) + " light-hours. It left the Sun’s wind (the heliopause) at " +
               fmt::plainNumber(f.probe->cross) + " AU in " + std::to_string(f.probe->crossYear) + ".";
    return voyagerFallbackCaption(fmt::yearUTC(sys.currentUnixMs()), probeLaunchYear("v1"), kProbesShownFromYear);
}

void farCard(const FarItem& f, Card& c) {
    const auto wrap = [](const std::string& t, std::size_t n) {   // words to lines of at most n letters (as the web's wrapTxt)
        std::vector<std::string> out;
        std::string cur, word;
        for (char ch : t + " ") {
            if (ch != ' ') { word += ch; continue; }
            if (word.empty()) continue;
            if (!cur.empty() && (cur + " " + word).size() > n) { out.push_back(cur); cur = word; }
            else cur = cur.empty() ? word : cur + " " + word;
            word.clear();
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    };
    c = Card{};
    if (f.probe) {
        const double hrs = f.au * 499.005 / 3600;          // light needs 499.005 s per AU
        c.title = f.name + " · spacecraft";
        c.lines = {"Distance from the Sun: " + fmt::fixed(f.au, 1) + " AU", "Speed: " + fmt::kms(f.probe->kms) + " away from the Sun",
                   "A radio signal takes about " + fmt::fixed(hrs, 1) + " hours from the Sun",
                   "Launched in " + std::to_string(f.probe->year) + "; it left the Sun’s wind",
                   "(the heliopause) at " + fmt::plainNumber(f.probe->cross) + " AU in " + std::to_string(f.probe->crossYear)};
        for (auto& l : wrap("Position: its real place on 30 Sep 2026, then a straight line at its real speed.", 46)) c.lines.push_back(l);
        return;
    }
    const FarData& d = *f.far;
    const auto fmtLy = [](double v) { return v < 10 ? fmt::fixed(v, 2) : v < 100 ? fmt::fixed(v, 1) : fmt::commas(std::stod(fmt::precision(v, 3))); };
    const double yrsV = voyagerYearsFromSun(f.au);          // Voyager 1's years to get there
    if (d.sp[0]) c.lines.push_back(std::string(d.sp) + " star, in " + d.con); else c.lines.push_back(std::string("In ") + d.con);
    c.lines.push_back("Distance: " + fmtLy(d.ly) + " light years (" + fmt::commas(std::stod(fmt::precision(f.au, 3))) + " AU)");
    c.lines.push_back("The light we see left it " + fmtLy(d.ly) + " years ago");
    if (d.ly < 1000) c.lines.push_back("Voyager 1 would take about " + fmt::commas(std::stod(fmt::precision(yrsV, 2))) + " years");
    for (auto& l : wrap(d.note, 46)) c.lines.push_back(l);
    c.title = f.kind == "bh" ? "Galactic centre · " + f.name : f.name;
}

}  // namespace orbit
