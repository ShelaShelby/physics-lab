#include "sky.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "card_text.h"
#include "date_text.h"
#include "format.h"
#include "kepler.h"
#include "solar_system.h"
#include "units.h"

namespace orbit {

using namespace units;

#include "sky_data.inc"
struct VoyagerRow { double jd, x, y, z; };   // 0.49: JPL Horizons, data/jpl/voyager/
#include "voyager_data.inc"

static constexpr int kFarStatic = sizeof(kFar) / sizeof(kFar[0]);
static constexpr int kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);
// 0.85.0 (PLAN_REVIEW.md R4, the owner's answer of 25 Nov 2026): Sedna, 2012 VP113 and 2015 TG387, "called inner Oort cloud objects by their discoverers; JPL lists them
// as trans-Neptunian objects" (Brown, Trujillo & Rabinowitz 2004; Sheppard et al. 2019: native/data/oort/arxiv; JPL SBDB orbit class "TNO":
// native/data/oort/sbdb_orbit_class.extract.txt). Their heliocentric osculating elements, ecliptic J2000, from the JPL Small-Body Database API with full precision
// (data/oort/sbdb_full_*.json, read 2026-11-25): a (au), e, i, node, peri (deg), mean anomaly (deg) at the epoch (JD, TDB), mean motion n (deg/day), JPL's solution.
// Moved on that fixed ellipse (two bodies: the Sun and it). Measured here against JPL Horizons (data/oort/horizons_*.txt): 0.0001 au off on 2026-11-25, about
// 0.1 au (0.13 % of its distance) in 1990 and 2060 (tests/test_review085.inc).
struct TnoData { const char* key; const char* name; double a, e, i, om, w, ma, n, epochJD; const char* soln; const char* discovery; };
static const TnoData kTnos[] = {
    {"sedna", "Sedna", 543.7195289104732, .8598824585187618, 11.92527582847476, 144.5061662673739, 311.0987725939751, 358.5956944005428, 7.773948799642578E-5, 2461200.5, "JPL#51",
     "Brown, Trujillo & Rabinowitz 2004 (arXiv:astro-ph/0404456), its discoverers: \"a candidate inner Oort cloud planetoid\""},
    {"vp113", "2012 VP113", 267.5209861941053, .6985589359876868, 24.00179208811373, 90.8926204194851, 294.2852296107872, 3.834281215098069, .0002252512910019064, 2461200.5, "JPL#11",
     "Sheppard et al. 2019 (AJ, doi:10.3847/1538-3881/ab0895): \"the third known IOC after Sedna and 2012 VP113\""},
    {"tg387", "2015 TG387 (Leleakuhonua)", 1344.905387006164, .951905319172072, 11.67887107280718, 301.1344316943008, 118.2293603285907, 359.6185683300886, 1.998326622088789E-5, 2461200.5, "JPL#9",
     "Sheppard et al. 2019 (AJ, doi:10.3847/1538-3881/ab0895), its discoverers: \"the third known IOC\""},
};
static constexpr int kTnoCount = sizeof(kTnos) / sizeof(kTnos[0]);

static int base36(const char* s, int n) {
    char buf[8] = {};
    std::memcpy(buf, s, n);
    return static_cast<int>(std::strtol(buf, nullptr, 36));
}

Vec3 lonLat(double lo, double la) {
    lo *= kPi / 180; la *= kPi / 180;
    return {std::cos(la) * std::cos(lo), std::cos(la) * std::sin(lo), std::sin(la)};
}

// 0.50 (review item 4): a star's colour = the colour of a black body at its colour temperature. The temperature from B-V is Ballesteros 2012 (EPL 97, 34008,
// arXiv:1201.1809, eq. 14; data/stars), the black body under the CIE 1931 2-degree observer to sRGB, largest channel 1, no white mixed in: tools/star_colour.py
// wrote the table star_colours.inc (B-V in steps of 0.01). Until 0.49.4: an unnamed curve fit for the black body's RGB, then 30 % white.
#include "star_colours.inc"
double bucketBV(int k) { return -0.3 + 2.3 * k / (kSkyColourBuckets - 1); }   // (0.84.0: named; the same steps as before)
double bvTemperatureK(double bv) { return 4600 * (1 / (0.92 * bv + 1.7) + 1 / (0.92 * bv + 0.62)); }   // Ballesteros 2012, eq. (14)
std::array<int, 3> bvRGB(double bv) {
    const int i = std::clamp(static_cast<int>(std::lround((bv - kStarBvLo) / kStarBvStep)), 0, kStarBvCount - 1);
    return {kStarColours[i].r, kStarColours[i].g, kStarColours[i].b};
}

// 0.50: the measured B-V of the far-away stars, from HYG v4.1 (the sky's own catalogue; column "ci", Hipparcos), rows saved in data/stars/hyg_v41_far_stars_rows.csv.
// Alpha Centauri: A's value (the brighter star). Was: a rough B-V from the spectral letter, no source (spBV, removed).
double farBV(const char* key) {
    static const struct { const char* key; double bv; } kBV[] = {
        {"acen", 0.71}, {"prox", 1.807}, {"barn", 1.57}, {"sirius", 0.009}, {"lal", 1.502}, {"eri", 0.881}, {"procyon", 0.432}, {"cyg61", 1.069},
        {"tau", 0.727}, {"altair", 0.221}, {"vega", -0.001}, {"fom", 0.145}, {"arct", 1.239}, {"pol", 0.636}, {"bet", 1.5}, {"rig", -0.03}};
    for (const auto& r : kBV) if (std::strcmp(r.key, key) == 0) return r.bv;
    return std::nan("");
}

StarDraw starDraw(double mag) {
    return starDrawLight(kStarFaintLight * std::pow(10.0, -0.4 * kStarGamma * (mag - kStarFaintMag)));   // opacity x area, square pixels
}

StarDraw starDrawLight(double light) {
    const double a0 = kPi * kStarMinRadius * kStarMinRadius;
    if (light <= a0) return {kStarMinRadius, light / a0};
    return {std::sqrt(light / kPi), 1.0};
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

bool skyDirection(double X, double Y, double W, double H, double ca, double sa, double cb, double sb, Vec3& v) {
    const double K = std::hypot(W, H) / 2 / 1.75, dx = (X - W / 2) / K, dy = -(Y - H / 2) / K, th = std::hypot(dx, dy);
    if (th > 1.75) return false;
    const double s = th > 1e-12 ? std::sin(th) / th : 1;
    const double x1 = s * dx, y2 = s * dy, dep = -std::cos(th);
    const double y1 = y2 * cb + dep * sb, z = -y2 * sb + dep * cb;   // undo the pitch
    v = {x1 * ca + y1 * sa, -x1 * sa + y1 * ca, z};                    // undo the yaw
    return true;
}

bool skyScreenLens(const Vec3& v, double W, double H, double ca, double sa, double cb, double sb, const SkyLens& L, double& X, double& Y) {
    if (!L.pinhole) return skyScreen(v, W, H, ca, sa, cb, sb, X, Y);
    return pinholeDirection(v, ca, sa, cb, sb, L.ppx, L.ppy, L.F, X, Y);   // (orbit/camera.h: the scene's own formula)
}

bool skyDirectionLens(double X, double Y, double W, double H, double ca, double sa, double cb, double sb, const SkyLens& L, Vec3& v) {
    if (!L.pinhole) return skyDirection(X, Y, W, H, ca, sa, cb, sb, v);
    const double x1 = (X - L.ppx) / L.F, y2 = -(Y - L.ppy) / L.F, dep = -1, n = std::sqrt(x1 * x1 + y2 * y2 + 1);   // the pinhole turned back: (x/z, y/z, 1)
    const double y1 = (y2 * cb + dep * sb) / n, z = (-y2 * sb + dep * cb) / n, x = x1 / n;
    v = {x * ca + y1 * sa, -x * sa + y1 * ca, z};
    return true;
}

void skyViewToGalactic(const Sky& S, double ca, double sa, double cb, double sb, double M[9]) {
    // view = R * ecliptic with the rows of R below (as skyScreen), so ecliptic = R^T * view and galactic = G * R^T * view
    const double R[3][3] = {{ca, -sa, 0}, {sa * cb, ca * cb, -sb}, {sa * sb, ca * sb, cb}};
    const Vec3 G[3] = {S.galCentre, cross(S.galPole, S.galCentre), S.galPole};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) M[3 * i + j] = G[i].x * R[j][0] + G[i].y * R[j][1] + G[i].z * R[j][2];
}

// The Milky Way band's points on the picture. Every sample that is on the map is drawn, as long as it has at least one neighbour on the map; the direction of the band
// there is taken from the neighbours on the map (both: the difference between them; one: the difference to it). The circle is closed, so sample 0 and sample
// kBandPieces are the same point and the neighbours wrap round. (Before 0.10.1 the neighbour test read the flags of points that the same loop had just switched
// off, so after the first gap in the visible stretch every later point was dropped: the band stopped, or was only a thin arc, or was missing.)
void milkyWayBand(const Sky& S, double W, double H, double ca, double sa, double cb, double sb, std::vector<BandPoint>& band, const SkyLens& L) {
    const double cx = W / 2, cy = H / 2, K = std::hypot(W, H) / 2 / 1.75;
    const Vec3 A = S.galCentre, P = S.galPole, B = cross(P, A);
    const int n = kBandPieces;
    band.assign(n + 1, BandPoint{});
    std::vector<char> onMap(n + 1, 0);
    for (int i = 0; i <= n; i++) {
        const double l = i / double(n) * 2 * kPi, c = std::cos(l);
        BandPoint& p = band[i];
        onMap[i] = skyScreenLens(A * c + B * std::sin(l), W, H, ca, sa, cb, sb, L, p.X, p.Y);
        p.bright = 0.4 + 0.6 * (0.5 + 0.5 * c);
        // fades out where the map ends (0.75.0: the pinhole has no such edge on the picture: its field is 45 degrees high)
        p.fade = onMap[i] ? (L.pinhole ? 1.0 : std::clamp((1.75 - std::hypot(p.X - cx, p.Y - cy) / K) / 0.2, 0.0, 1.0)) : 0.0;
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
    for (int k = 0; k < kSkyColourBuckets; k++) S.colours[k] = bvRGB(bucketBV(k));
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

int farCount() { return kFarStatic + kProbeCount + kTnoCount; }

Vec3 tnoPositionAU(int k, double jdTDB) {
    const TnoData& t = kTnos[k];
    const double d2r = kPi / 180, E = keplerE(t.e, std::fmod(t.ma + t.n * (jdTDB - t.epochJD), 360.0));
    const double xv = t.a * (std::cos(E) - t.e), yv = t.a * std::sqrt(1 - t.e * t.e) * std::sin(E);
    const double v = std::atan2(yv, xv), r = std::hypot(xv, yv), u = v + t.w * d2r, O = t.om * d2r, I = t.i * d2r;
    return {r * (std::cos(O) * std::cos(u) - std::sin(O) * std::sin(u) * std::cos(I)), r * (std::sin(O) * std::cos(u) + std::cos(O) * std::sin(u) * std::cos(I)), r * std::sin(u) * std::sin(I)};
}
int tnoIndex(const std::string& key) { for (int k = 0; k < kTnoCount; k++) if (key == kTnos[k].key) return k; return -1; }

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
    for (int i = 0; i < kTnoCount; i++) if (key == kTnos[i].key) return kFarStatic + kProbeCount + i;
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
    if (index >= kFarStatic + kProbeCount) {                   // 0.85.0: Sedna, 2012 VP113, 2015 TG387 on their JPL orbits
        const int k = index - kFarStatic - kProbeCount;
        const TnoData& t = kTnos[k];
        f.key = t.key; f.name = t.name; f.kind = "tno";
        const Vec3 q = tnoPositionAU(k, sys.julianDayTDB());
        f.au = length(q); f.dir = q * (1 / f.au); f.pos = q * kAU;
        f.tno = k;
        return true;
    }
    const ProbeData& p = kProbes[index - kFarStatic];
    const double nowMs = sys.epochMs + sys.t * 1000.0;
    if (nowMs < kProbesShownFromMs) return false;              // before 1 Jan 1990: not shown
    f.key = p.key; f.name = p.name; f.kind = "probe"; f.probe = &p;
    f.year = fmt::yearUTC(static_cast<int64_t>(nowMs));
    if (nowMs >= kSkyT0Ms) {
        const double yrs = (nowMs - kSkyT0Ms) / (365.25 * 86400000.0);   // years since the positions were worked out
        f.dir = lonLat(p.lon, p.lat);
        f.au = p.au + p.kms * kAuPerYearPerKms * yrs;       // a straight line outwards at its real speed (0.21095 AU per year for each km/s)
    } else {
        // 0.49 (outside review: the straight line was also run BACKWARDS, so in 1990 Voyager 2 was drawn 19.6 AU from its real place, inside Neptune's orbit and
        // 38° below the ecliptic, and Voyager 1 8.8 AU off): JPL Horizons' place on 1 January of every year 1990-2026 (voyager_data.inc), joined by straight
        // lines; the last piece ends on the kProbes place of 30 Sep 2026 (within 0.02 AU of Horizons'), so nothing jumps there.
        const VoyagerRow* t = index - kFarStatic == 0 ? kVoyager1 : kVoyager2;
        constexpr int n = 37;
        const auto ms = [](double jd) { return (jd - 2440587.5) * 86400000.0; };   // (TDB taken as UTC: the minute between them moves a Voyager by 0.0001 AU)
        int k = 0;
        while (k + 1 < n && ms(t[k + 1].jd) <= nowMs) k++;
        const Vec3 a{t[k].x, t[k].y, t[k].z};
        const Vec3 b = k + 1 < n ? Vec3{t[k + 1].x, t[k + 1].y, t[k + 1].z} : lonLat(p.lon, p.lat) * p.au;
        const double ta = ms(t[k].jd), tb = k + 1 < n ? ms(t[k + 1].jd) : kSkyT0Ms;
        const double u = std::min(1.0, std::max(0.0, (nowMs - ta) / (tb - ta)));
        const Vec3 q = a + (b - a) * u;                     // au
        f.au = length(q);
        f.dir = q * (1 / f.au);
    }
    f.pos = f.dir * (f.au * kAU);
    return true;
}

std::string voyagerCaption(const SolarSystem& sys) {
    FarItem f;
    if (farItem(sys, farIndexOf("v1"), f)) {
        // 0.49 (outside review): before 17 Feb 1998 Pioneer 10 was farther away (NASA, data/jpl/voyager/nasa_voyager1_read_2026-11-11.html: "On Feb. 17, 1998, Voyager 1 became the
        // most distant human-made object in existence when, at a distance of 69.4 AU from the Sun, it overtook Pioneer 10"), and before 2012 it had not left the Sun's wind yet
        const bool farthest = static_cast<double>(sys.currentUnixMs()) >= kVoyager1PassesPioneer10Ms;
        std::string s = (farthest ? "Voyager 1 is the most distant spacecraft: " : "Voyager 1 is ") + fmt::commas(f.au) + " AU out, " + fmt::fixed(f.au * 499.005 / 3600, 0) + " light-hours.";
        if (!farthest) s += " Pioneer 10 is still farther away: Voyager 1 passes it on 17 Feb 1998, at 69.4 AU (NASA).";
        s += (f.year < f.probe->crossYear ? " It will leave the Sun’s wind (the heliopause) at " : " It left the Sun’s wind (the heliopause) at ") + fmt::plainNumber(f.probe->cross) + " AU in " +
             std::to_string(f.probe->crossYear) + ".";
        return s;
    }
    return voyagerFallbackCaption(fmt::yearUTC(sys.currentUnixMs()), probeLaunchYear("v1"), kProbesShownFromYear);
}

void farCard(const FarItem& f, Card& c) {
    const auto wrap = [](const std::string& t, std::size_t n) {   // words to lines of at most n letters
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
    if (f.kind == "tno" && f.tno >= 0) {
        const TnoData& t = kTnos[f.tno];
        const double q = t.a * (1 - t.e), Q = t.a * (1 + t.e), years = 360 / t.n / 365.25;
        c.title = f.name + " · trans-Neptunian object";
        c.lines = {"Distance from the Sun: " + fmt::fixed(f.au, 1) + " AU",
                   "Orbit: " + fmt::fixed(q, 0) + " to " + fmt::commas(std::stod(fmt::precision(Q, 3))) + " AU from the Sun, once in about " + fmt::commas(std::stod(fmt::precision(years, 2))) + " years"};
        for (auto& l : wrap("Called inner Oort cloud objects by their discoverers; JPL lists them as trans-Neptunian objects.", 46)) c.lines.push_back(l);
        for (auto& l : wrap(std::string("Source: ") + t.discovery + ".", 46)) c.lines.push_back(l);
        for (auto& l : wrap(std::string("Orbit: JPL Small-Body Database (") + t.soln + "), a fixed ellipse round the Sun: about 0.1 AU off JPL Horizons in 1990 and 2060.", 46)) c.lines.push_back(l);
        return;
    }
    if (f.probe) {
        const double hrs = f.au * 499.005 / 3600;          // light needs 499.005 s per AU
        c.title = f.name + " · spacecraft";
        c.lines = {"Distance from the Sun: " + fmt::fixed(f.au, 1) + " AU", "Speed: " + fmt::kms(f.probe->kms) + " away from the Sun",
                   "A radio signal takes about " + fmt::fixed(hrs, 1) + " hours from the Sun",
                   "Launched in " + std::to_string(f.probe->year) + (f.year != 0 && f.year < f.probe->crossYear ? "; it will leave the Sun’s wind" : "; it left the Sun’s wind"),
                   "(the heliopause) at " + fmt::plainNumber(f.probe->cross) + " AU in " + std::to_string(f.probe->crossYear)};
        for (auto& l : wrap("Position: JPL Horizons, once a year from 1990 to 30 Sep 2026 (straight lines in between), then a straight line at its speed.", 46)) c.lines.push_back(l);
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
