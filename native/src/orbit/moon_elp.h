// The Moon's geocentric position from the lunar theory ELP2000-82B (Chapront-Touze and Chapront, Astron. Astrophys. 190, 342, 1988; CDS catalogue VI/79).
// 0.10.4 stage 1: data and validation only, not used by the app yet. See moon_elp.cpp for what the series is and what it leaves out, and
// native/REPORT.md for the measured error against JPL Horizons (DE441).
#pragma once

#include "orbit/vec3.h"

namespace orbit {

// The span it is meant for (and was checked over): 1 January 12:00 TDB of 1800 to the same of 2100, as Julian dates (TDB).
constexpr double kMoonElpFirstJD = 2378497.0;
constexpr double kMoonElpLastJD = 2488070.0;

// The Moon's centre seen from the Earth's centre, in metres, in the mean dynamical ecliptic and inertial equinox of J2000 (the axes of the lab's other J2000 ecliptic
// data), for a Julian date in TDB. Geometric position: no light time, no aberration. It also works outside 1800..2100, but the error there is not measured.
Vec3 moonGeocentricElp(double jdTDB);

}  // namespace orbit
