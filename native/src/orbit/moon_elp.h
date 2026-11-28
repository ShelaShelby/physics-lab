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

// 0.96.0 (outside review of 28 Nov 2026, part E): outside 1800 to 1 Jan 2100 (where ELP2000-82B is not checked) the Moon is on a precessing Kepler ellipse fitted to
// ELP2000-82B: a, e and i fixed; the mean longitude L, the longitude of perigee varpi and of the node Omega linear in time (days from J2000 TDB); geocentric, J2000 ecliptic and
// equinox, as moonGeocentricElp. Least-squares fit to ELP every 0.25 day over 1800-2100 (native/tools/fit_moon_ellipse.py, residual = position difference / 384,400 km).
// It leaves out every periodic term (the evection 1.27 deg, the variation 0.66 deg, the annual equation 0.19 deg ...), so its place is only roughly right; the angles' rates
// are mean ones (no slow change of the rates, no tidal slowing). Before 0.96.0 the Moon there started at a made-up angle.
struct MoonEllipse { double a, e, i, L0, Ldot, w0, wdot, O0, Odot; };   // m; rad; rad and rad/day at J2000 (JD 2451545.0 TDB)
constexpr MoonEllipse kMoonEllipse = {384358415.88540298, 0.054850110634847037, 0.089814145606997672, 3.8103420678851392, 0.2299708361122921,
                                      1.4547732741060277, 0.0019436992420316054, 2.1825239946813721, -0.00092488560694645541};
// its worst error against ELP2000-82B over the fit's samples (1800-2100 every 0.25 day): the direction seen from Earth in degrees, the position in km (tests/test_moon.cpp
// testEllipse checks they are the measured maxima). Quoted on the Moon's card. Against ELP over 1400-1800 and 2100-2500 the fit's tool measured at most 2.41 deg (ELP itself is
// not checked there).
constexpr double kMoonEllipseWorstDeg = 2.416, kMoonEllipseWorstKm = 15324.2;   // measured 2.415920 deg, 15324.180 km (rounded up)
// and its worst error against JPL Horizons (DE441) at the 444 dates of tests/reference/moon_outside_table.inc (every 5 years 1600-1795 and 2105-2300, daily through 2100 after
// ELP's span): measured 2.2015 deg and 14,027.0 km (rounded up). Quoted on the Moon's card; not measured between those dates or further out.
constexpr double kMoonEllipseOutsideDeg = 2.2016, kMoonEllipseOutsideKm = 14027.1;
Vec3 moonGeocentricEllipse(double jdTDB);

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
