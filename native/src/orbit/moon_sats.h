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
// or outside 1800-2100 (then it is drawn on its simple ellipse with a made-up starting angle, as before 0.27).
bool satState(const std::string& moon, double jdTDB, Vec3& posKm, Vec3& velKmPerDay);
// The worst distance from Horizons measured by tests/test_orbit.cpp at its 90 dates, km, rounded up (the test checks they are the measured maxima).
double satWorstErrKm(const std::string& moon);

}  // namespace orbit
