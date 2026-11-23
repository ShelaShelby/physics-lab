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
// 0.29: the same with only the `terms` largest terms of the kept series (the terms are sorted by size). For the Earth-Moon split of the Real planets' pulls (nbody.cpp), which
// needs the Moon's place only roughly (its error enters the tiny tidal difference); the worst difference from the full function over 1800-2100 is measured by tests/test_moon.cpp.
Vec3 moonGeocentricElp(double jdTDB, int terms);

// The measured worst error of moonGeocentricElp against JPL Horizons (DE441) over the 170 dates of tests/reference/moon_table.inc (every 5 years 1800-2100, two lunar cycles daily, two days hourly):
// the position in km and the angle seen from Earth in arcseconds (0.10.4 stage 1, REPORT history). Quoted in the app's Real note and on the Moon card; tests/test_moon.cpp checks that they are
// the measured maxima. Between the dates it is not measured.
constexpr double kMoonElpWorstKm = 5.2116, kMoonElpWorstArcsec = 2.8789;

// The Moon's phase (0.10.4 stage 2), from the Moon's and the Sun's directions seen from the Earth's centre (any common frame, the ecliptic here; only directions are used).
//  elongationDeg: the angle at the Earth between the Sun and the Moon, 0 .. 180 degrees (the geocentric elongation).
//  fraction: the lit part of the disc, (1 + cos i) / 2 with i the phase angle, the angle at the Moon between the Sun and the Earth (0.75.2: exact; until 0.75.1 it was the first-order
//    (1 - cos(elongation)) / 2, which differs from it by up to 0.0013 near the quarters because the Sun is not infinitely far: 384,400 km / 149.598 million km = 0.0026 rad).
//  index/name: eight names, chosen so that a name never disagrees with the lit percent: by the lit fraction, with waxing or waning from the Moon's ecliptic-longitude difference from the Sun
//    (Moon minus Sun, 0 to 180 degrees = east of the Sun = waxing). new moon: under 2 % lit; crescent: 2 to 40 %; quarter: 40 to 60 %; gibbous: 60 to 98 %; full moon: over 98 %.
//    index: 0 new moon, 1 waxing crescent, 2 first quarter, 3 waxing gibbous, 4 full moon, 5 waning gibbous, 6 last quarter, 7 waning crescent. The thresholds are a convention (the
//    percentages are mine, not an official definition); the principal phases themselves (new, first quarter, full, last quarter) are exact instants, where the longitude difference is 0, 90, 180, 270 degrees.
struct MoonPhase {
    double elongationDeg;
    double fraction;
    int index;
    const char* name;
};
MoonPhase moonPhase(Vec3 moonFromEarth, Vec3 sunFromEarth);

}  // namespace orbit
