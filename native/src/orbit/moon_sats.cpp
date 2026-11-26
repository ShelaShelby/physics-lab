#include "moon_sats.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace orbit {
namespace {

struct SatEl { const char* name; int id; double ra, dec, a, e, w0, wdot, inc, O0, Odot, L0, n, ndot; };
const SatEl kSats[] = {
#include "moon_sats_data.inc"
};

// Measured by tests/test_orbit.cpp (testMoonSats) against tests/reference/moon_sats_table.inc, rounded UP to a whole km.
struct Worst { const char* name; double km; };
const Worst kWorst[] = {
#include "moon_sats_worst.inc"
};

const SatEl* find(const std::string& name) {
    for (const SatEl& s : kSats)
        if (name == s.name) return &s;
    return nullptr;
}

}  // namespace

bool hasRealPlace(const std::string& moon) { return find(moon) != nullptr; }

double satWorstErrKm(const std::string& moon) {
    for (const Worst& w : kWorst)
        if (moon == w.name) return w.km;
    return -1;
}

static Vec3 satPos(const SatEl& s, double jd) {
    constexpr double kDeg = 3.14159265358979323846 / 180;
    const double obl = 84381.448 / 3600 * kDeg;   // the obliquity of Horizons' ecliptic of J2000 (IAU 1976), as the fit (tools/make_moon_sats.py)
    const double t = jd - 2451545.0;
    const double L = s.L0 + s.n * t + 0.5 * s.ndot * t * t, w = s.w0 + s.wdot * t, O = s.O0 + s.Odot * t;
    const double M = L - w;
    double E = M + s.e * std::sin(M);
    for (int k = 0; k < 8; k++) E -= (E - s.e * std::sin(E) - M) / (1 - s.e * std::cos(E));
    const double xo = s.a * (std::cos(E) - s.e), yo = s.a * std::sqrt(1 - s.e * s.e) * std::sin(E);
    const double om = w - O, x1 = xo * std::cos(om) - yo * std::sin(om), y1 = xo * std::sin(om) + yo * std::cos(om);
    const double x2 = x1, y2 = y1 * std::cos(s.inc), z2 = y1 * std::sin(s.inc);
    const Vec3 r{x2 * std::cos(O) - y2 * std::sin(O), x2 * std::sin(O) + y2 * std::cos(O), z2};   // in the reference plane's frame
    // that frame's axes in ICRF equatorial: Z = its pole, X = its node on the equator (z x P), Y = Z x X; then to the ecliptic
    const double ra = s.ra * kDeg, de = s.dec * kDeg;
    const Vec3 P{std::cos(de) * std::cos(ra), std::cos(de) * std::sin(ra), std::sin(de)};
    Vec3 X{-P.y, P.x, 0};
    X = X * (1 / length(X));
    const Vec3 Y = cross(P, X), q = X * r.x + Y * r.y + P * r.z;
    return {q.x, std::cos(obl) * q.y + std::sin(obl) * q.z, -std::sin(obl) * q.y + std::cos(obl) * q.z};
}

bool satState(const std::string& moon, double jd, Vec3& pos, Vec3& vel) {
    const SatEl* s = find(moon);
    if (!s || jd < kSatFirstJD || jd > kSatLastJD) return false;
    pos = satPos(*s, jd);
    const double h = 1e-3;   // days
    vel = (satPos(*s, jd + h) - satPos(*s, jd - h)) * (0.5 / h);
    return true;
}

namespace {
// ---- 0.88.0: JPL's table of mean elements, on any date -------------------------------------------------------------------------------------
// The rows (moon_jpl_elements.inc) are JPL's printed numbers at J2000. They become the same precessing ellipse satPos draws: the periapsis longitude (omega + node) turns by
// 2 pi / P apsis, the node by 2 pi / P node (years of 365.25 days), and the mean longitude by 2 pi / P (kind 'S') or by 2 pi / P plus both precession rates (kind 'A', where P is the
// period of the mean anomaly). The reference plane is the table's Laplace pole; JPL gives none for the equatorial frames (Titania, Oberon, Triton, Charon), where it is the
// planet's IAU 2015 pole, as in the 0.27 fit (moon_sats_data.inc).
const SatTableRow kRows[] = {
#include "moon_jpl_elements.inc"
};
constexpr double kPi = 3.14159265358979323846, kYear = 365.25;

const SatTableRow* findRow(const std::string& moon) {
    for (const SatTableRow& r : kRows)
        if (moon == r.name) return &r;
    return nullptr;
}

double rate(const char* periodYears, int sign) { const double P = std::atof(periodYears); return P > 0 ? sign * 2 * kPi / (P * kYear) : 0.0; }   // rad/day; "0.000" or "-": no precession

SatEl meanElements(const SatTableRow& r) {
    const SatEl* fit = find(r.name);   // pole of the equatorial frames, and the Horizons mean longitude of the four rows marked longitudeFromFit
    constexpr double d = kPi / 180;
    SatEl s{};
    s.name = r.name; s.id = fit ? fit->id : 0;
    s.ra = r.ra[0] ? std::atof(r.ra) : fit->ra;
    s.dec = r.ra[0] ? std::atof(r.dec) : fit->dec;
    s.a = std::atof(r.a); s.e = std::atof(r.e); s.inc = std::atof(r.i) * d;
    const double om = std::atof(r.w) * d, M0 = std::atof(r.M) * d, O0 = std::atof(r.node) * d;
    const double wd = rate(r.Pw, r.apsisSign), Od = rate(r.Pn, r.nodeSign);
    s.w0 = om + O0; s.wdot = wd + Od;      // longitude of periapsis = argument + node
    s.O0 = O0; s.Odot = Od;
    s.n = 2 * kPi / std::atof(r.P) + (r.kind == 'A' ? wd + Od : 0.0);
    s.L0 = r.longitudeFromFit ? fit->L0 : M0 + om + O0;
    s.ndot = 0;
    return s;
}

const SatEl* meanFind(const std::string& moon) {
    static const std::vector<SatEl> all = [] { std::vector<SatEl> v; for (const SatTableRow& r : kRows) v.push_back(meanElements(r)); return v; }();
    for (const SatEl& s : all)
        if (moon == s.name) return &s;
    return nullptr;
}

}  // namespace

bool satMeanState(const std::string& moon, double jd, Vec3& pos, Vec3& vel) {
    const SatEl* s = meanFind(moon);
    if (!s) return false;
    pos = satPos(*s, jd);
    const double h = 1e-3;   // days
    vel = (satPos(*s, jd + h) - satPos(*s, jd - h)) * (0.5 / h);
    return true;
}

bool satPlace(const std::string& moon, double jd, Vec3& pos, Vec3& vel, bool* real) {
    if (satState(moon, jd, pos, vel)) { if (real) *real = true; return true; }
    if (real) *real = false;
    return satMeanState(moon, jd, pos, vel);
}

const SatTableRow* satTableRow(const std::string& moon) { return findRow(moon); }
double satTableNumber(const char* printed) { return std::atof(printed); }
double satTableHalfUnit(const char* printed) {
    const char* dot = std::strchr(printed, '.');
    return 0.5 * std::pow(10.0, -(dot ? static_cast<int>(std::strlen(dot + 1)) : 0));
}

double satMeanPeriodDays(const std::string& moon) {
    const SatTableRow* r = findRow(moon);
    const SatEl* s = meanFind(moon);
    if (!r || !s) return 0;
    return 2 * kPi / (r->kind == 'A' ? s->n - s->wdot : s->n);   // wdot is the periapsis longitude's rate: omega's plus the node's
}
bool satMeanPole(const std::string& moon, double& raDeg, double& decDeg) {
    const SatEl* s = meanFind(moon);
    if (!s) return false;
    raDeg = s->ra; decDeg = s->dec;
    return true;
}
double satFitMeanMotion(const std::string& moon) { const SatEl* s = find(moon); return s ? s->n : 0; }
double satFitLongitudeJ2000(const std::string& moon) { const SatEl* s = find(moon); return s ? s->L0 : 0; }

double satMeanDriftYears(const std::string& moon) {
    const SatTableRow* r = findRow(moon);
    if (!r) return 0;
    const double P = std::atof(r->P), Pw = std::atof(r->Pw), Pn = std::atof(r->Pn);
    double d = 2 * kPi * satTableHalfUnit(r->P) / (P * P);   // rad/day the mean longitude's rate can be off by, from the rounding of P ...
    if (r->kind == 'A') {                                    // ... and, where P is the mean anomaly's, of the two precession periods
        if (Pw > 0) d += 2 * kPi * satTableHalfUnit(r->Pw) * kYear / ((Pw * kYear) * (Pw * kYear));
        if (Pn > 0) d += 2 * kPi * satTableHalfUnit(r->Pn) * kYear / ((Pn * kYear) * (Pn * kYear));
    }
    return 2 * kPi / d / kYear;
}

}  // namespace orbit
