#include "spin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "format.h"
#include "solar_system.h"
#include "units.h"

namespace orbit {

using namespace units;

namespace {

struct SpinData { const char* name; double ra0, raT, dec0, decT, W0, Wd; };   // deg, deg per century, deg, deg/century, deg, deg/day
const SpinData kSpin[] = {
    {"Mercury", 281.0103, -0.0328, 61.4155, -0.0049, 329.5988, 6.1385108},
    {"Venus", 272.76, 0, 67.16, 0, 160.20, -1.4813688},
    {"Earth", 0, -0.641, 90, -0.557, 190.147, 360.9856235},
    {"Mars", 317.269202, -0.10927547, 54.432516, -0.05827105, 176.049863, 350.891982443297},   // Mars and Neptune also wobble (below)
    {"Jupiter", 268.056595, -0.006499, 64.495303, 0.002413, 284.95, 870.536},
    {"Saturn", 40.589, -0.036, 83.537, -0.004, 38.90, 810.7939024},
    {"Uranus", 257.311, 0, -15.175, 0, 203.81, -501.1600928},
    {"Neptune", 299.36, 0, 43.46, 0, 249.978, 541.1397757},
    {"Pluto", 132.993, 0, -6.163, 0, 302.695, 56.3625225},
    {"Sun", 286.13, 0, 63.87, 0, 84.176, 14.1844},   // the standard reference rate (25.38 days); the gas surface turns faster at the equator
};
// wobble terms {amplitude deg, phase at J2000 deg, rate deg per century}: added to RA as sines, to Dec as cosines, to W as sines
struct Term { double amp, ph, rate; };
const Term kMarsA[] = {{0.000068, 198.991226, 19139.4819985}, {0.000238, 226.292679, 38280.8511281}, {0.000052, 249.663391, 57420.7251593}, {0.000009, 266.183510, 76560.6367950}, {0.419057, 79.398797, 0.5042615}};
const Term kMarsD[] = {{0.000051, 122.433576, 19139.9407476}, {0.000141, 43.058401, 38280.8753272}, {0.000031, 57.663379, 57420.7517205}, {0.000005, 79.476401, 76560.6495004}, {1.591274, 166.325722, 0.5042615}};
const Term kMarsW[] = {{0.000145, 129.071773, 19140.0328244}, {0.000157, 36.352167, 38281.0473591}, {0.000040, 56.668646, 57420.9295360}, {0.000001, 67.364003, 76560.2552215}, {0.000001, 104.792680, 95700.4387578}, {0.584542, 95.391654, 0.5042615}};
const Term kNepA[] = {{0.70, 357.85, 52.316}}, kNepD[] = {{-0.51, 357.85, 52.316}}, kNepW[] = {{-0.48, 357.85, 52.316}};

const SpinData* find(const std::string& name) {
    for (const auto& d : kSpin) if (name == d.name) return &d;
    return nullptr;
}

constexpr double RAD = kPi / 180;
const double kObliquity = 23.4392911 * RAD;                // Earth's axial tilt at J2000: turns equatorial into ecliptic coordinates

Vec3 toEcliptic(Vec3 v) { return {v.x, v.y * std::cos(kObliquity) + v.z * std::sin(kObliquity), -v.y * std::sin(kObliquity) + v.z * std::cos(kObliquity)}; }

template <std::size_t N>
double terms(const Term (&list)[N], double T, double (*fn)(double)) {
    double t = 0;
    for (const Term& q : list) t += q.amp * fn((q.ph + q.rate * T) * RAD);
    return t;
}
double sinf_(double x) { return std::sin(x); }
double cosf_(double x) { return std::cos(x); }
double wrap360(double a) { return std::fmod(std::fmod(a, 360.0) + 360.0, 360.0); }

}  // namespace

bool hasSpin(const std::string& name) { return find(name) != nullptr; }
double spinRateDegPerDay(const std::string& name) { const SpinData* d = find(name); return d ? d->Wd : 0; }

bool spinFrame(const std::string& name, double jd, SpinFrame& out) {
    const SpinData* d = find(name);
    if (!d) return false;
    const double dd = jd - 2451545.0 + (name == "Earth" ? 0 : 69.184 / 86400), T = dd / 36525;   // days and centuries since J2000 (TT; Earth's W uses UT)
    double ra = (d->ra0 + d->raT * T) * RAD, de = (d->dec0 + d->decT * T) * RAD, W = wrap360(d->W0 + d->Wd * dd) * RAD;
    if (name == "Mars") { ra += terms(kMarsA, T, sinf_) * RAD; de += terms(kMarsD, T, cosf_) * RAD; W = wrap360(d->W0 + d->Wd * dd + terms(kMarsW, T, sinf_)) * RAD; }
    if (name == "Neptune") { ra += terms(kNepA, T, sinf_) * RAD; de += terms(kNepD, T, cosf_) * RAD; W = wrap360(d->W0 + d->Wd * dd + terms(kNepW, T, sinf_)) * RAD; }
    const Vec3 P = {std::cos(de) * std::cos(ra), std::cos(de) * std::sin(ra), std::sin(de)};   // pole (equatorial)
    const Vec3 N = {-std::sin(ra), std::cos(ra), 0};      // the body's equator crossing Earth's equator
    const Vec3 Q = cross(P, N);
    const Vec3 A = N * std::cos(W) + Q * std::sin(W), B = cross(P, A);
    out = {toEcliptic(P), toEcliptic(A), toEcliptic(B), W / RAD};
    return true;
}

double turnsPerSecond(const std::string& name, double tspeed) {
    return std::fabs(spinRateDegPerDay(name)) / 360 * (kSimTime / kDay * std::pow(10.0, tspeed));
}

std::vector<std::string> wrapWords(const std::string& t, std::size_t n) {
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
}

static std::string tooFast(double rps) {
    return "Turns " + (rps >= 100 ? fmt::commas(rps) : fmt::fixed(rps, 0)) + " times a second at this Time speed: too fast to see, so it is held still";
}

std::vector<std::string> spinLines(const Body& b, const SolarSystem& sys, double tspeed, bool paused) {
    SpinFrame f;
    if (!spinFrame(b.name, sys.julianDay(), f)) return {};
    const double rate = spinRateDegPerDay(b.name), sg = rate < 0 ? -1 : 1;
    const Vec3 L = cross(b.pos, b.vel);                   // the orbit's "up" (angular momentum)
    const double nL = std::max(length(L), 1e-300);
    const double tilt = std::acos(std::clamp(sg * dot(f.P, L) / nL, -1.0, 1.0)) / RAD;   // 0 = upright on its orbit, over 90 = upside down
    const double days = 360 / std::fabs(rate);
    const long long tm = std::llround(days * 1440);       // minutes
    const std::string per = days < 2 ? std::to_string(tm / 60) + " h " + std::to_string(tm % 60) + " min" : (days < 100 ? fmt::fixed(days, 1) : fmt::fixed(days, 0)) + " days";
    std::vector<std::string> lines = {"Turns once in " + per, "Axis leans " + fmt::fixed(tilt, 1) + "° from upright"};
    if (tilt > 90) lines.push_back("Upside down, so it turns backwards");
    const double rps = turnsPerSecond(b.name, tspeed);
    if (rps > 1.2 && !paused) for (auto& l : wrapWords(tooFast(rps), 44)) lines.push_back(l);
    return lines;
}

std::vector<std::string> sunSpinLines(const SolarSystem& sys, double tspeed, bool paused) {
    SpinFrame f;
    spinFrame("Sun", sys.julianDay(), f);
    const double tilt = std::acos(std::clamp(f.P.z, -1.0, 1.0)) / RAD;   // upright = straight up from the ecliptic
    std::vector<std::string> lines = {"Turns once in " + fmt::fixed(360 / spinRateDegPerDay("Sun"), 1) + " days (standard rate)"};
    for (auto& l : wrapWords("It is gas, so the equator turns faster: about 25 days, the poles about 34", 44)) lines.push_back(l);
    lines.push_back("Axis leans " + fmt::fixed(tilt, 1) + "° from upright");
    const double rps = turnsPerSecond("Sun", tspeed);
    if (rps > 1.2 && !paused) for (auto& l : wrapWords(tooFast(rps), 44)) lines.push_back(l);
    return lines;
}

std::vector<std::string> seasonLines(const Body& b, const SolarSystem& sys) {
    // Earth's season from the real tilt and Earth's place on its orbit
    if (b.name != "Earth") return {};
    const OrbitShape o = orbitOf(b);
    if (!o.bound || o.e > 0.9) return {};
    const double GM = kGMSun, TAU = 2 * kPi;
    SpinFrame sf;
    spinFrame("Earth", sys.julianDay(), sf);
    const Vec3 P = sf.P, r = b.pos, v = b.vel;
    const double rn = length(r);
    const Vec3 h = cross(r, v), L = h * (1 / length(h)), nv = cross(P, L);
    const double nn = length(nv);
    if (nn < 0.05) return {};
    const Vec3 nx = nv * (1 / nn), m = cross(L, nx), sun = r * (-1 / rn);   // nx: where the Sun is seen from Earth at the March equinox
    const auto wrap = [&](double a) { return std::fmod(std::fmod(a, TAU) + TAU, TAU); };
    const double psi = wrap(std::atan2(dot(sun, m), dot(sun, nx)));          // the Sun's angle from it says the season
    const double v2 = dot(v, v), rv = dot(r, v);
    const Vec3 ev = (r * (v2 - GM / rn) - v * rv) * (1 / GM);                // eccentricity vector: points to the perihelion
    const double th = std::atan2(dot(ev, m), dot(ev, nx)), e = o.e;
    const auto M = [&](double nu) {                       // true anomaly -> mean anomaly (Kepler)
        nu = wrap(nu + kPi) - kPi;
        const double E = 2 * std::atan2(std::sqrt(1 - e) * std::sin(nu / 2), std::sqrt(1 + e) * std::cos(nu / 2));
        return E - e * std::sin(E);
    };
    const double nu0 = std::atan2(dot(r, m), dot(r, nx)) - th, Mnow = M(nu0), rate = TAU / (o.periodYears * 365.25);   // radians per day
    const int k = std::min(3, static_cast<int>(std::floor(psi / (kPi / 2))));
    const auto at = [&](int j) { return M((j * kPi / 2 - kPi) - th); };
    const double since = wrap(Mnow - at(k)) / rate, until = wrap(at((k + 1) % 4) - Mnow) / rate;
    const double dlat = std::asin(std::clamp(dot(sun, P), -1.0, 1.0)) / RAD;   // latitude where the Sun is straight overhead
    static const char* N[] = {"spring", "summer", "autumn", "winter"};
    static const char* S[] = {"autumn", "winter", "spring", "summer"};
    const auto dy = [](double x) { return x < 0.5 ? std::string("today") : std::llround(x) == 1 ? std::string("1 day ago") : std::to_string(std::llround(x)) + " days ago"; };
    const auto dy2 = [](double x) { return std::llround(x) <= 0 ? std::string("today") : std::llround(x) == 1 ? std::string("in 1 day") : "in " + std::to_string(std::llround(x)) + " days"; };
    std::string capN = N[k];
    capN[0] = static_cast<char>(std::toupper(capN[0]));
    return {std::string("Northern ") + N[k] + ", southern " + S[k],
            capN + " began " + dy(since) + "; " + N[(k + 1) % 4] + " starts " + dy2(until),
            std::fabs(dlat) < 0.05 ? "Sun straight overhead on the equator" : "Sun straight overhead at " + fmt::fixed(std::fabs(dlat), 1) + "° " + (dlat > 0 ? "north" : "south")};
}

}  // namespace orbit
