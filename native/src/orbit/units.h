// Units and constants for Orbit Lab. Everything inside the program is SI:
// metres, seconds, kilograms, metres per second.
//
// The web version (index.html) works in "sim units": the Sun has GM = 100,
// 1 AU = 100^(1/3) sim lengths, and 1 year = 2π sim times. The constants below
// are those same numbers converted to SI, so the native app follows exactly
// the same orbits as the web app (the unit tests check this).
#pragma once

#include <cmath>

namespace units {

constexpr double kPi = 3.14159265358979323846;

// --- length -------------------------------------------------------------------
constexpr double kKm = 1000.0;                        // m
constexpr double kAU = 149.6e9;                       // m. The web uses AU_KM = 149.6e6 km (IAU value: 149,597,870.7 km)
constexpr double kEarthRadius = 6371.0 * kKm;         // m, used to turn "× Earth" sizes into metres
constexpr double kSunRadius = 696000.0 * kKm;         // m
constexpr double kLightSpeed = 299792.458 * kKm;      // m/s
constexpr double kLightYearAU = 63241.077;            // AU per light year

// --- time ---------------------------------------------------------------------
constexpr double kDay = 86400.0;                      // s
constexpr double kYear = 365.25 * kDay;               // s, a Julian year (the web counts years as 365.25 days)

// --- the web's sim units in SI --------------------------------------------------
// 1 sim length = 1 AU / 100^(1/3); 1 sim time = 1 year / (2π).
inline const double kSimLength = kAU / std::cbrt(100.0);   // m   (about 3.223e10 m)
constexpr double kSimTime = kYear / (2.0 * kPi);            // s   (about 58.13 days)

// --- gravity of the Sun -----------------------------------------------------------
// GM of the Sun as the web version uses it: GM = 100 sim units, which is the same
// as Kepler's third law with 1 AU and 1 Julian year:  GM = 4π² · AU³ / year².
// = 1.32724e20 m³/s². (The IAU value is 1.32712e20; the 0.009 % difference comes
// from the web's rounded AU and 365.25-day year, and is kept so both versions agree.)
constexpr double kGMSun = 4.0 * kPi * kPi * kAU * kAU * kAU / (kYear * kYear);   // m³/s²

constexpr double kSunMassKg = 1.989e30;               // kg (shown on the Sun's card)
constexpr double kEarthMassKg = 5.972e24;             // kg
constexpr double kSunInEarthMasses = 333000.0;        // the web's barycentre uses "the Sun weighs 333,000 Earths"

// --- integrator settings (from the web Orbit Lab step()) ----------------------------
constexpr double kPhysicsStep = 1.0 / 240.0;          // s of real time per physics step (fixed, as in the web's DT)
constexpr double kMaxFrameDt = 0.05;                  // s, a slow frame never counts as more than this (as in the web)
inline const double kMaxSubStep = 0.004 * kSimTime;   // s of simulated time: leapfrog sub-steps are never longer (about 5.6 hours)
inline const double kSoftening = 1e-6 * kSimLength * kSimLength * kSimLength;   // m³, the web's "+ 1e-6" in GM/(r³ + 1e-6)
inline const double kCrashRadius = 0.7 * kSimLength;  // m, the web's STAR_R: a body this close to the Sun is removed
inline const double kLostRadius = 800.0 * kSimLength; // m, the web's LOST_R: a body this far away has escaped and is removed

// --- Time speed slider -------------------------------------------------------------
// The slider value v means: 1 real second = 58.13 × 10^v days of simulated time,
// i.e. kSimTime × 10^v seconds. Range and default copied from the web.
constexpr double kTspeedMin = -6.7009;                // 1 s = 1 s (real time)
constexpr double kTspeedMax = 2.5;                    // 1 s = about 50 years
constexpr double kTspeedDefault = -0.3;               // 1 s = about 29 days

// Zoom slider: pixels per metre = min(width, height) / 34 / kSimLength × 10^zoom (as in the web)
constexpr double kZoomMin = -9.0;
constexpr double kZoomMax = 5.5;
constexpr double kZoomDefault = -1.2;                 // the web opens the real Solar System at zoom -1.2

}  // namespace units
