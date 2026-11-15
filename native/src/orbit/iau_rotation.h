// 0.50 (review item 10): the IAU WGCCRE rotation models of the moons drawn with a map (Io, Europa, Ganymede, Callisto, Rhea, Triton, Charon, Phobos),
// from NAIF's pck00011.tpc (tools/make_iau_rotation.py -> iau_rotation.inc). Same frame rule as the Moon (moon_orientation.h): the pole's RA and Dec are
// in ICRF, turned to the lab's J2000 ecliptic by the obliquity 84381.448".
#pragma once

#include <cmath>
#include <cstring>

#include "orbit/vec3.h"

namespace orbit {

namespace iau_detail {
#include "orbit/iau_rotation.inc"
}

// Body axes (x: the 0° meridian, y: 90° E, z: the north pole) in the J2000 ecliptic, for a Julian date (TDB). False if the model is not in the table.
inline bool iauBodyAxes(const char* name, double jdTDB, Vec3 axes[3]) {
    using namespace iau_detail;
    const IauRotation* r = nullptr;
    for (const auto& q : kIauRotations) if (std::strcmp(q.name, name) == 0) r = &q;
    if (!r) return false;
    const IauAngles* A = nullptr;
    for (const auto& q : kIauAngles) if (q.system == r->system) A = &q;
    const double kD = 3.14159265358979323846 / 180, d = jdTDB - 2451545.0, T = d / 36525.0;
    double ra = r->ra[0] + r->ra[1] * T + r->ra[2] * T * T, dec = r->dec[0] + r->dec[1] * T + r->dec[2] * T * T, W = r->pm[0] + r->pm[1] * d + r->pm[2] * d * d;
    const int n = A ? A->count : 0, k = A ? A->degree + 1 : 1;
    for (int i = 0; i < n; i++) {
        double a = 0, p = 1;
        for (int j = 0; j < k; j++) { a += A->coef[k * i + j] * p; p *= T; }
        a *= kD;
        if (i < r->nra) ra += r->nutRa[i] * std::sin(a);
        if (i < r->ndec) dec += r->nutDec[i] * std::cos(a);
        if (i < r->npm) W += r->nutPm[i] * std::sin(a);
    }
    ra *= kD; dec *= kD; W = std::fmod(W, 360.0) * kD;
    const Vec3 z{std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)}, Q{-std::sin(ra), std::cos(ra), 0};
    const Vec3 x = Q * std::cos(W) + cross(z, Q) * std::sin(W), y = cross(z, x);
    const double e = 84381.448 / 3600 * kD, ce = std::cos(e), se = std::sin(e);
    const auto ecl = [&](Vec3 v) { return Vec3{v.x, ce * v.y + se * v.z, -se * v.y + ce * v.z}; };
    axes[0] = ecl(x); axes[1] = ecl(y); axes[2] = ecl(z);
    return true;
}

// The axes a moon is drawn with, given its place from its planet (km, J2000 ecliptic): its IAU model when the table has it; otherwise (until 0.49.4, for every
// moon) the 0° meridian towards the planet and the axis upright to the ecliptic.
inline void satDrawnAxes(const char* name, double jdTDB, const Vec3& fromPlanet, Vec3 axes[3]) {
    if (iauBodyAxes(name, jdTDB, axes)) return;
    const Vec3 P{0, 0, 1};
    Vec3 A = fromPlanet * -1.0;
    A = A - P * dot(A, P);
    A = A * (1 / length(A));
    axes[0] = A; axes[1] = cross(P, A); axes[2] = P;
}

}  // namespace orbit
