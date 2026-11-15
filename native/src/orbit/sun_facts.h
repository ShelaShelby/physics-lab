// 0.49.4: the Sun's colour and its habitable zone, each from a named source (no drawing here, so the tests can check them).
#pragma once
#include <cmath>

namespace sunfacts {

// The Sun's colour on an sRGB screen: the ASTM G173-03 extraterrestrial spectrum (data/sun: two copies, pvlib's and solcore's, identical) under the CIE 1931
// 2-degree observer, to sRGB (IEC 61966-2-1, D65 white), the largest channel scaled to 1 (tools/sun_colour.py): CIE x = 0.3234, y = 0.3327, about 5,930 K. Nearly white.
// Until 0.49.3 it was (247, 207, 72), from a typed table with no source.
constexpr int kSunSrgb[3] = {255, 242, 239};

// 0.55.1: the Sun's effective temperature, 5,772 K: JPL Horizons' Sun header (data/jpl/horizons-sun.txt, line 18: "Effective temp, K     = 5772"). The app's one
// copy (the Orbit Lab's star card and the Black Hole Lab's "times the Sun's surface" use it).
constexpr double kSunTeffK = 5772;

// The habitable zone for an Earth-mass planet: Kopparapu et al. 2014, ApJL 787, L29 (arXiv:1404.5292v2, data/hz), Table 1, Seff_sun (1 M_Earth):
// runaway greenhouse 1.107 (inner edge), maximum greenhouse 0.356 (outer edge); Eq. (5): d = (L / L_sun / Seff)^0.5 AU, here L = L_sun.
constexpr double kHzSeffRunaway = 1.107, kHzSeffMaxGreenhouse = 0.356;
inline double hzInnerAU() { return std::sqrt(1 / kHzSeffRunaway); }        // 0.9504 AU
inline double hzOuterAU() { return std::sqrt(1 / kHzSeffMaxGreenhouse); }  // 1.6760 AU

}  // namespace sunfacts
