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

// 0.73.0: the same for another star (Alpha Centauri A, B and Proxima): Kopparapu et al. 2014 eq. (4), Seff = Seff_sun + a T + b T^2 + c T^3 + d T^4 with
// T = Teff - 5780 K, and eq. (5), d = (L / L_sun / Seff)^0.5 AU; Table 1, 1 Earth mass (re-read in the saved PDF, data/hz/kopparapu_2014_arxiv1404.5292v2.pdf).
// Recent Venus and early Mars are the optimistic edges, runaway and maximum greenhouse the conservative ones (the band the lab draws). The paper fits
// 2,600-7,200 K. At T = 0 the runaway and maximum-greenhouse edges are the Sun's two above, bit for bit.
enum class Hz { RecentVenus, Runaway, MaxGreenhouse, EarlyMars };
inline double hzEdgeAU(double lumSun, double teffK, Hz k) {
    static const double C[4][5] = {{1.776, 2.136e-4, 2.533e-8, -1.332e-11, -3.097e-15},
                                   {kHzSeffRunaway, 1.332e-4, 1.58e-8, -8.308e-12, -1.931e-15},
                                   {kHzSeffMaxGreenhouse, 6.171e-5, 1.698e-9, -3.198e-12, -5.575e-16},
                                   {0.32, 5.547e-5, 1.526e-9, -2.874e-12, -5.011e-16}};
    const double* c = C[static_cast<int>(k)];
    const double T = teffK - 5780;
    const double seff = T == 0 ? c[0] : c[0] + c[1] * T + c[2] * T * T + c[3] * T * T * T + c[4] * T * T * T * T;
    return std::sqrt(lumSun / seff);
}

}  // namespace sunfacts
