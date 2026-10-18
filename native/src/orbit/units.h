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
// The astronomical unit: the IAU value, exactly 149,597,870,700 m. Owner decision of 3 Oct 2026 (DECISIONS.md): the lab follows the IAU, not the web version's 149.6e9 m (0.10.5 stage 4).
// JPL's Astrodynamic Parameters page (https://ssd.jpl.nasa.gov/astro_par.html, read 2026-10-03) states: "astronomical unit au 149597870700 m IAU 2012 Resolution B1"
// (the page's own wording). The owner's decision names IAU 2012 Resolution B2. 0.10.5 stage 5 tried to read the IAU's own text (iauarchive.eso.org/static/resolutions/IAU2012_English.pdf) and
// the SYRTE page (syrte.obspm.fr/IAU_resolutions/IAUResol_2012_0.html): neither was reachable from the session (the proxy refused), so B2 is NOT confirmed from the IAU here and JPL's "B1" stays quoted.
constexpr double kAU = 149597870700.0;                // m
constexpr double kEarthRadius = 6371.0 * kKm;         // m, used to turn "× Earth" sizes into metres
// The Sun's radius: IAU 2015 nominal solar radius, 695,700 km. JPL Horizons' Sun header (API, COMMAND='10', OBJ_DATA='YES', read 2026-10-03) says:
//   "Vol. mean radius, km  = 695700"  and  "Solar radius (IAU2015)= 695700 km"      (0.10.5 stage 3; before it the lab used 696,000 km, 4.3e-4 more)
constexpr double kSunRadius = 695700.0 * kKm;         // m
// The speed of light: JPL's Astrodynamic Parameters page (https://ssd.jpl.nasa.gov/astro_par.html, read 2026-10-03) lists "speed of light in vacuum c 299792458 m/s 2018 CODATA recommended values".
constexpr double kLightSpeed = 299792.458 * kKm;      // m/s

// --- time ---------------------------------------------------------------------
constexpr double kDay = 86400.0;                      // s
constexpr double kYear = 365.25 * kDay;               // s, a Julian year (the web counts years as 365.25 days)
// A light year: the distance light travels in one Julian year (365.25 days), here worked out from c and the au above, in AU: 63,241.077 AU. (0.10.5 stage 5: it was a typed 63241.077, the IAU
// value written from knowledge; the IAU text itself could not be opened, so only the arithmetic is the lab's.)
constexpr double kLightYearAU = kLightSpeed * kYear / kAU;

// --- the web's sim units in SI --------------------------------------------------
// 1 sim length = 1 AU / 100^(1/3); 1 sim time = 1 year / (2π).
inline const double kSimLength = kAU / std::cbrt(100.0);   // m   (about 3.223e10 m)
constexpr double kSimTime = kYear / (2.0 * kPi);            // s   (about 58.13 days)

// --- gravity of the Sun -----------------------------------------------------------
// GM of the Sun as the web version uses it: GM = 100 sim units, which is the same
// as Kepler's third law with 1 AU and 1 Julian year:  GM = 4π² · AU³ / year².
// = 1.3271745e20 m³/s² with the IAU au (before 0.10.5 stage 4, with the web's 149.6e9 m, it was 1.3272312e20). JPL's GM of the Sun (DE440, astro_par page) is 1.32712440041279419e20:
// this one is 3.8e-5 larger, which comes from the 365.25-day Julian year (the Gaussian year is 365.2568983 d). Kept on purpose for now: the year and this GM are not changed in this stage.
constexpr double kGMSun = 4.0 * kPi * kPi * kAU * kAU * kAU / (kYear * kYear);   // m³/s²

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

}  // namespace units
