// 0.50 (review item 9): the Moon's orientation in space, for drawing its map (orbit_view.cpp) and for the tests (tests/test_orbit.cpp, testMoonOrientation050).
#pragma once

#include <algorithm>
#include <cmath>

#include "orbit/moon_elp.h"
#include "orbit/vec3.h"

namespace orbit {

// The Moon's body axes (x: its 0° meridian, y: 90° E, z: its north pole) as unit vectors in the lab's frame (mean ecliptic and equinox of J2000), for a Julian date (TDB).
void moonBodyAxes(double jdTDB, Vec3 axes[3]);

// Where Earth's centre is overhead on the Moon: east longitude and latitude (degrees, longitude 0..360) of the direction Moon -> Earth in the body axes, with the
// Moon's ELP2000-82B place (geometric: no light time). The drawn sub-Earth point.
inline void moonSubEarthDeg(double jdTDB, double& lonDeg, double& latDeg) {
    Vec3 ax[3];
    moonBodyAxes(jdTDB, ax);
    const Vec3 g = moonGeocentricElp(jdTDB), u = g * (-1 / length(g));
    const double kD = 180 / 3.14159265358979323846;
    lonDeg = std::atan2(dot(u, ax[1]), dot(u, ax[0])) * kD;
    if (lonDeg < 0) lonDeg += 360;
    latDeg = std::asin(std::clamp(dot(u, ax[2]), -1.0, 1.0)) * kD;   // 0.50.3: clamped, as its siblings
}

// The IAU WGCCRE model of the Moon's rotation (the 2009 report, Archinal et al. 2011, Celest. Mech. Dyn. Astr. 109, 101; the 2015 report, Archinal et al. 2018,
// Celest. Mech. Dyn. Astr. 130, 22, gives no new values for the Moon and keeps these). Numbers as NAIF's PCK kernel pck00011.tpc lists them
// (data/jpl/naif_pck00011.tpc, saved as received): BODY301_POLE_RA, _POLE_DEC, _PM, _NUT_PREC_RA, _DEC, _PM and the 13 angles E1..E13
// (BODY3_NUT_PREC_ANGLES, given there in degrees per day as "the original constants"). d = days from J2000.0 TDB, T = Julian centuries.
// The pole's right ascension and declination are in ICRF (the J2000 equator); they are turned to the J2000 ecliptic by the obliquity 84381.448" (IAU 1976 /
// IERS J2000.0, the value of the lab's ecliptic data). The model follows the Moon's mean-Earth / polar-axis frame (the one JPL Horizons reports) closely.
inline void moonBodyAxes(double jdTDB, Vec3 axes[3]) {
    const double kD = 3.14159265358979323846 / 180, d = jdTDB - 2451545.0, T = d / 36525.0;
    static const double kE0[13] = {125.045, 250.089, 260.008, 176.625, 357.529, 311.589, 134.963, 276.617, 34.226, 15.134, 119.743, 239.961, 25.053};
    static const double kE1[13] = {-0.0529921, -0.1059842, 13.0120009, 13.3407154, 0.9856003, 26.4057084, 13.0649930, 0.3287146, 1.7484877, -0.1589763, 0.0036096, 0.1643573, 12.9590088};
    static const double kRa[13] = {-3.8787, -0.1204, 0.0700, -0.0172, 0.0, 0.0072, 0.0, 0.0, 0.0, -0.0052, 0.0, 0.0, 0.0043};
    static const double kDec[13] = {1.5419, 0.0239, -0.0278, 0.0068, 0.0, -0.0029, 0.0009, 0.0, 0.0, 0.0008, 0.0, 0.0, -0.0009};
    static const double kW[13] = {3.5610, 0.1208, -0.0642, 0.0158, 0.0252, -0.0066, -0.0047, -0.0046, 0.0028, 0.0052, 0.0040, 0.0019, -0.0044};
    double ra = 269.9949 + 0.0031 * T, dec = 66.5392 + 0.0130 * T, W = 38.3213 + 13.17635815 * d - 1.4e-12 * d * d;
    for (int i = 0; i < 13; i++) {
        const double E = (kE0[i] + kE1[i] * d) * kD;
        ra += kRa[i] * std::sin(E); dec += kDec[i] * std::cos(E); W += kW[i] * std::sin(E);
    }
    ra *= kD; dec *= kD; W = std::fmod(W, 360.0) * kD;
    const Vec3 z{std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};   // the pole, ICRF
    const Vec3 Q{-std::sin(ra), std::cos(ra), 0};                                             // the ascending node of the Moon's equator on the ICRF equator
    const Vec3 x = Q * std::cos(W) + cross(z, Q) * std::sin(W), y = cross(z, x);
    const double e = 84381.448 / 3600 * kD, ce = std::cos(e), se = std::sin(e);
    const auto ecl = [&](Vec3 v) { return Vec3{v.x, ce * v.y + se * v.z, -se * v.y + ce * v.z}; };   // equator -> ecliptic (rotation about x by the obliquity)
    axes[0] = ecl(x); axes[1] = ecl(y); axes[2] = ecl(z);
}

}  // namespace orbit
