// Kepler orbits: from orbital elements to a position and velocity.
#pragma once

#include "vec3.h"

namespace orbit {

// Kepler's equation  M = E − e·sin(E): the eccentric anomaly E (radians) from the
// mean anomaly M (degrees, any value; it is wrapped to ±180°). Newton's method,
// with a bisection safety net for orbits that are nearly straight lines (e > 0.8).
double keplerE(double e, double meanAnomalyDeg);

struct State {
    Vec3 pos;  // m, ecliptic coordinates, Sun at the origin
    Vec3 vel;  // m/s
};

// Orbital elements -> position and velocity around the Sun (GM = units::kGMSun).
//   a      semi-major axis, m
//   e      eccentricity (0 = circle, < 1 = ellipse)
//   I      inclination to the ecliptic, deg
//   node   longitude of the ascending node Ω, deg
//   peri   argument of perihelion ω, deg
//   M      mean anomaly, deg
State keplerState(double a, double e, double I, double node, double peri, double M);
// 0.73.0: the same about any centre of mass GM (m^3/s^2): the Alpha Centauri system (orbit/alphacen.h). The Sun's call above is this with GM = kGMSun,
// bit for bit (tests/test_orbit.cpp testAlphaCen073 checks both against 0.72.1's numbers). The frame is the elements' own: x, y, z of Rz(node) Rx(I) Rz(peri).
State keplerState(double a, double e, double I, double node, double peri, double M, double GM);

struct CometData;
// Where a comet is on a given Julian day: its mean anomaly grows by 360° per period,
// counted from its perihelion date.
State cometState(const CometData& c, double jd);

// Points along a comet's orbit, packed tightly around the comet itself (a step
// under a pixel next to it, wider further away), so at any zoom the comet sits
// exactly on its dotted line. pixelsPerMetre is the current zoom.
void cometLocalPath(const CometData& c, double jd, double pixelsPerMetre, std::vector<Vec3>& out);

}  // namespace orbit
