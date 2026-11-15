#include "moon_sats.h"

#include <cmath>
#include <cstring>

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

}  // namespace orbit
