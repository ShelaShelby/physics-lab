// Units and constants for Orbit Lab. Everything inside the program is SI:
// metres, seconds, kilograms, metres per second.
//
// 1 AU = 100^(1/3) sim lengths, and 1 year = 2π sim times. The constants below
// are those same numbers converted to SI.
#pragma once

#include <cmath>

namespace units {

constexpr double kPi = 3.14159265358979323846;

// --- length -------------------------------------------------------------------
constexpr double kKm = 1000.0;                        // m
// The astronomical unit: the IAU value, exactly 149,597,870,700 m.
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
constexpr double kYear = 365.25 * kDay;               // s, a Julian year
// A light year: the distance light travels in one Julian year (365.25 days), here worked out from c and the au above, in AU: 63,241.077 AU. (0.10.5 stage 5: it was a typed 63241.077, the IAU
// value written from knowledge; the IAU text itself could not be opened, so only the arithmetic is the lab's.)
constexpr double kLightYearAU = kLightSpeed * kYear / kAU;

// 1 sim length = 1 AU / 100^(1/3); 1 sim time = 1 year / (2π).
inline const double kSimLength = kAU / std::cbrt(100.0);   // m   (about 3.223e10 m)
constexpr double kSimTime = kYear / (2.0 * kPi);            // s   (about 58.13 days)

// --- gravity of the Sun -----------------------------------------------------------
// JPL's GM of the Sun, DE440 (https://ssd.jpl.nasa.gov/astro_par.html, read 2026-10-03: 1.32712440041279419e11 km³/s²; the same value as the Real mode's, nbody.cpp kRealGM[0]).
// 0.46.7 (outside review): until 0.46.6 the Simple mode used 4π² AU³ / (Julian year)² = 1.3271745e20, 3.8e-5 too large (Kepler's law with a 365.25-day year; the Gaussian year
// is 365.2568983 d), so its speeds were 1.9e-5 too fast. In sim units this GM is 99.9962 (1 sim length and 1 sim time keep their definitions above).
constexpr double kGMSun = 1.32712440041279419e20;   // m³/s²

constexpr double kStandardGravity = 9.80665;           // m/s^2: standard gravity g_n, exact by definition (3rd CGPM 1901; NIST SP 330). 0.46.3: the cards' "g"
constexpr double kEarthMassKg = 5.9722e24;            // kg: NASA Planetary Fact Sheet, Earth (5.9722 × 10^24 kg). (0.22 stage 6: was 5.972e24 with no source)
constexpr double kSunInEarthMasses = 332946.0487;     // the Sun's mass over Earth's: IAU 2009 System of Astronomical Constants (Luzum et al. 2011, Celest. Mech. Dyn. Astr. 110, 293), best estimate 332 946.0487 ± 0.0007. (0.22 stage 6: was 333,000, rounded, no source)

// --- integrator settings ----------------------------
constexpr double kPhysicsStep = 1.0 / 240.0;          // s of real time per physics step
constexpr double kMaxFrameDt = 0.05;                  // s, a slow frame never counts as more than this
// 0.75.2 (outside review, the Sun's "crash radius" 0.15 AU = 32 solar radii): nothing is integrated into the Sun any more (Simple places every body from its elements, solar_system.cpp step), so there
// is no crash radius. This radius is only the DRAWN Sun in "Easy to see" (the Sun's own radius, kSunRadius above, is 0.00465 AU); it is 0.15 AU so the Sun can be found, and the
// texts say so (shell_orbit.cpp, the Learn pages: "about 32 times too big").
inline const double kCrashRadius = 0.7 * kSimLength;

// --- Time speed slider -------------------------------------------------------------
// The slider value v means: 1 real second = 58.13 × 10^v days of simulated time,
// i.e. kSimTime × 10^v seconds.
constexpr double kTspeedMin = -6.7009;                // 1 s = 1 s (real time)
constexpr double kTspeedMax = 2.5;                    // 1 s = about 50 years
constexpr double kTspeedDefault = -0.3;               // 1 s = about 29 days

// Zoom slider: pixels per metre = min(width, height) / 34 / kSimLength × 10^zoom
constexpr double kZoomMin = -9.0;
constexpr double kZoomMax = 5.5;

}  // namespace units
