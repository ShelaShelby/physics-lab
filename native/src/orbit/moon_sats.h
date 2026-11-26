// 0.88.0: outside 1800 to 1 Jan 2100 12:00 TDB the same 13 moons follow JPL's own table of mean elements (the rows in moon_jpl_elements.inc: size, shape, plane and period from
// https://ssd.jpl.nasa.gov/sats/elem/), no longer a Kepler ellipse with a made-up angle and a plane tilted by the planet's tilt. The table's numbers are rounded, so where the moon is on
// its orbit stays right only near J2000 (satMeanDriftYears). Earth's Moon is not in this: ROADMAP.
// 0.27 (owner's choice, 5 Nov 2026): the real places of the 13 big moons other than ours (Phobos, Deimos, the four Galilean moons, Enceladus,
// Rhea, Titan, Titania, Oberon, Triton, Charon). Each moves on a precessing ellipse whose mean elements were fitted by least squares to NASA/JPL
// Horizons vectors over 1800-2100 (tools/make_moon_sats.py, data in moon_sats_data.inc), the same kind of model as JPL's own table of satellite
// mean elements. tests/test_orbit.cpp measures each against Horizons at 90 other dates; the worst error per moon is below and in the app.
// Earth's Moon is not here: in Real it is ELP2000-82B (moon_elp.h).
#pragma once

#include <string>

#include "vec3.h"

namespace orbit {

constexpr double kSatFirstJD = 2378497.0;   // 1 Jan 1800 12:00 TDB ...
constexpr double kSatLastJD = 2488070.0;    // ... to 1 Jan 2100 12:00 TDB: the span of the fit and the test

bool hasRealPlace(const std::string& moon);
// The moon's place from its planet's centre, ecliptic and mean equinox of J2000, km; and its velocity, km/day. False if not one of the 13
// or outside 1800-2100 (there satMeanState / satPlace give JPL's mean-element place; before 0.88.0 a simple ellipse with a made-up starting angle).
bool satState(const std::string& moon, double jdTDB, Vec3& posKm, Vec3& velKmPerDay);
// 0.88.0: JPL's mean elements of the moon, on any date (a precessing ellipse in the moon's reference plane, the same model as satState). False if not one of the 13.
bool satMeanState(const std::string& moon, double jdTDB, Vec3& posKm, Vec3& velKmPerDay);
// The place the app draws: the real one (satState) from 1800 to 2100, else JPL's mean elements (satMeanState). `real` tells which. False if not one of the 13.
bool satPlace(const std::string& moon, double jdTDB, Vec3& posKm, Vec3& velKmPerDay, bool* real = nullptr);

// One row of JPL's table as printed (the strings keep their rounding), and how this lab reads it (see moon_jpl_elements.inc).
struct SatTableRow {
    const char *name, *frame, *a, *e, *w, *M, *i, *node, *P, *Pw, *Pn, *ra, *dec;   // km, -, deg, deg, deg, deg, days, years, years, deg, deg
    char kind;                    // 'A': P is the period of the mean anomaly; 'S': of the mean longitude
    int apsisSign, nodeSign;      // the page prints the precession periods without a sign
    bool longitudeFromFit;        // the mean longitude at J2000 is the lab's Horizons fit's (JPL's is far from Horizons)
};
const SatTableRow* satTableRow(const std::string& moon);
double satTableNumber(const char* printed);      // the printed number
double satTableHalfUnit(const char* printed);    // half of the printed number's last digit: the most its rounding can move it
// From the model built from the row: the period of what the table's P describes (days), the reference plane's pole (ICRF, deg; for the equatorial frames the planet's IAU 2015 pole),
// and the real mean motion of the lab's Horizons fit (rad/day) and its mean longitude at J2000 (rad), which the rows' kind and signs were read against.
double satMeanPeriodDays(const std::string& moon);
bool satMeanPole(const std::string& moon, double& raDeg, double& decDeg);
double satFitMeanMotion(const std::string& moon);
double satFitLongitudeJ2000(const std::string& moon);
// Years after which the rounding of the table's periods alone turns the angle along the orbit by a full turn (the angle is right only well inside this, around J2000).
double satMeanDriftYears(const std::string& moon);

// The worst distance from Horizons measured by tests/test_orbit.cpp at its 90 dates, km, rounded up (the test checks they are the measured maxima).
double satWorstErrKm(const std::string& moon);

}  // namespace orbit
