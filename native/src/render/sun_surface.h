// 0.94.0: the Sun's surface on the Orbit Lab's Sun (kPlanetFS uMode 4): dark sunspots (umbra and penumbra), granulation, and NASA SDO/HMI's photo of the day.
// The C++ twin the tests check the shader against (tests/planet_picture.cpp sun, tests/test_sun094.inc). Every number is from a named source (native/data/sun/README.txt).
//
// Sunspots (one round spot of the group's measured area: orbit/sunspots.h; the records give no layout):
//  - The umbra: P. Maltby et al., ApJ 306, 284 (1986), Table 3, "broad-band umbra/photosphere intensity ratios corrected for limb darkening", the middle phase of the cycle
//    (t/t0 = 0.5), 0.387-2.35 um (extract native/data/sun/spots/maltby_1986_table3.extract.txt). These are the dark cores of large spots: smaller umbrae are brighter.
//    Linear in wavelength between the table's rows; below 387 nm the 387 nm value. The ratio is to the photosphere at the same place on the disc (Maltby: no
//    detectable centre-to-limb change), so the umbra is the drawn photosphere times this ratio.
//  - The penumbra: S. K. Mathew et al., A&A 465, 291 (2007), Table 2: the mean penumbral intensity against the spot's radius, a linear fit for radii above 10 arcsec,
//    0.8561 - 0.0016 r (r in arcsec), from SOHO/MDI's continuum near the Ni I 676.8 nm line (extract mathew_2007_table2.extract.txt). One wavelength: the same
//    factor in every channel (no measured penumbral contrast against wavelength was found: REPORT 0.94.0). The fit is used from 10 to 30 arcsec (section 4.2:
//    "approximately linear" for 10-30 arcsec) and held at its ends outside. r is the spot's radius seen from 1 au (1 arcsec = 1 au x pi / 648000).
//  - The split of a spot's area: B. K. Jha, S. Mandal, D. Banerjee, Sol. Phys. 294, 72 (2019), Kodaikanal 1923-2011: the penumbra to umbra area ratio q "tends to
//    settle down to a value of ~6" for spots above 100 millionths of the hemisphere, and stays "~4.5" for smaller spots (extract jha_2019_ratio.extract.txt).
//    The umbra is the round middle of the spot with 1 / (1 + q) of its area.
//  - Per screen channel c: the ratio weighted by the disc centre's spectrum and the channel's CIE weight, as render/sun_limb.h does:
//    U_c = Sum w_c(l) E(l) / (F/I)(l) U(l) / Sum w_c(l) E(l) / (F/I)(l).
// Granulation (drawn statistically: the cells are not the real ones of that day):
//  - Cell size: D. H. Hathaway et al., ApJ 811, 105 (2015): the photospheric velocity spectrum rises "to a peak at a wavenumber of about 3500 (wavelength of about
//    1200 km), which is typical of granules"; lifetimes "of about 10 minutes (Bray et al. 1984)" (extract granulation/hathaway_2015.extract.txt).
//  - Contrast: S. K. Mathew, V. Zakharov, S. K. Solanki, A&A (2009), doi:10.1051/0004-6361/200911975, Table 2: the rms contrast of quiet-Sun Hinode/SOT BFI images
//    after the stray-light correction (deconvolved): 0.215 at 450.4 nm, 0.158 at 555.0 nm, 0.111 at 668.4 nm (the continuum bands; the CN and G bands are molecular
//    bands and are not used). Linear in wavelength between them, held at the ends outside. Weighted per channel as above. Disc-centre values: the change of the
//    contrast towards the limb is not drawn.
//  - The pattern: bright round granules with dark lanes between: minus the distance to the nearest of jittered points on a cubic grid (3D, one point a grid
//    cell, on the Sun's sphere), zero mean and unit rms by the pattern's own measured mean and rms (kGranMean, kGranRms; tests/test_sun094.inc checks them).
//    Its spatial spectrum (power per ring of wavenumber) has a broad peak at 1.778 grid spacings (measured here: 16 patterns, 1024 x 1024 points over 64
//    spacings, smoothed over +-17 %), so the grid spacing is 1.2 Mm / 1.778 = 0.675 Mm and the peak is the measured 1.2 Mm (the test checks the peak).
//    (0.94.0's first pattern, the distance between the nearest and second-nearest points, drew a net of thin straight lanes: not used.)
//  - Time: each pattern fades in over half a lifetime and out over the next half (weights cos and sin of pi f / 2: the rms stays 1). Patterns a lifetime apart
//    share nothing.
//  - The grain adds no light: the pattern has zero mean, so the disc's mean stays Neckel & Labs's (the test checks it).
//  - Smaller than a pixel: the contrast fades with the pixel's footprint w on the Sun (in cells): full at 3 pixels a cell and more, none at 1.5 pixels a cell and
//    less (smoothstep). The pixel then shows the mean.
// The photo (orbit/sun_photo.h): HMI's continuum intensity near 617.3 nm, divided by its own limb fit, times the drawn photosphere (colour and Neckel & Labs edge).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "sun_limb.h"

namespace render {

// ---- the sources' numbers ----
// Maltby et al. 1986, Table 3, wavelength (um) and the middle phase's ratio (0.387 um has only the middle phase)
constexpr int kMaltbyN = 10;
constexpr double kMaltbyUm[kMaltbyN] = {0.387, 0.579, 0.669, 0.876, 1.215, 1.54, 1.67, 1.73, 2.09, 2.35};
constexpr double kMaltbyMid[kMaltbyN] = {0.008, 0.066, 0.090, 0.215, 0.342, 0.495, 0.548, 0.577, 0.589, 0.581};
// Mathew et al. 2007, Table 2, the penumbra's mean intensity: constant and gradient (per arcsec of spot radius), the range used (arcsec)
constexpr double kPenConst = 0.8561, kPenGrad = -0.0016, kPenRMin = 10, kPenRMax = 30;
// Jha, Mandal & Banerjee 2019: penumbra to umbra area ratio, above and below 100 millionths of the hemisphere
constexpr double kRatioBig = 6.0, kRatioSmall = 4.5, kRatioSplitArea = 100;
// Mathew, Zakharov & Solanki 2009, Table 2 (deconvolved rms), the continuum bands: wavelength (nm), rms contrast
constexpr int kGranCN = 3;
constexpr double kGranCNm[kGranCN] = {450.4, 555.0, 668.4};
constexpr double kGranCRms[kGranCN] = {0.215, 0.158, 0.111};
// Hathaway et al. 2015: the granular peak's wavelength (Mm) and the granules' lifetime (minutes)
constexpr double kGranCellMm = 1.2, kGranLifeMin = 10;
constexpr double kGranPeakSpacings = 1.778;      // the pattern's spectrum peak in grid spacings (measured here, see above)
constexpr double kGranGridMm = kGranCellMm / kGranPeakSpacings;
constexpr double kSunRadiusMm = 695.7;           // IAU 2015 B3 nominal solar radius (orbit/units.h kSunRadius)
constexpr double kArcsecPerRadAt1Au = 648000.0 / 3.14159265358979323846;   // one radian at 1 au in arcsec
constexpr double kAuMm = 149597.8707;            // IAU 2012 B2: 1 au = 149,597,870.7 km
// The pattern's own mean and rms of F1 (grid of unit spacing), measured over 16 x 10^6 random points of the ball (runs with other seeds differ by 1e-4; tests/test_sun094.inc checks both to 0.5 %)
constexpr double kGranMean = 0.52047, kGranRms = 0.17230;

inline double tableAt(const double* x, const double* y, int n, double v) {   // linear between rows, held at the ends
    if (v <= x[0]) return y[0];
    for (int i = 0; i < n - 1; i++) if (v <= x[i + 1]) return y[i] + (v - x[i]) / (x[i + 1] - x[i]) * (y[i + 1] - y[i]);
    return y[n - 1];
}
inline double umbraAt(double nm) { return tableAt(kMaltbyUm, kMaltbyMid, kMaltbyN, nm / 1000); }
inline double granContrastAt(double nm) { return tableAt(kGranCNm, kGranCRms, kGranCN, nm); }
inline double penumbraAt(double rArcsec) { return kPenConst + kPenGrad * std::clamp(rArcsec, kPenRMin, kPenRMax); }
inline double penumbraRatioQ(double areaMillionths) { return areaMillionths >= kRatioSplitArea ? kRatioBig : kRatioSmall; }
// a spot's angular radius on the Sun (radians at the centre) seen from 1 au, in arcsec
inline double spotArcsec(double rho) { return kSunRadiusMm * std::sin(rho) / kAuMm * kArcsecPerRadAt1Au; }
// the umbra's angular radius for a spot of area A (millionths of the hemisphere) and radius rho: a cap of 1 / (1 + q) of the area
inline double umbraRho(double areaMillionths) {
    const double a = areaMillionths * 1e-6 / (1 + penumbraRatioQ(areaMillionths));
    return std::acos(std::clamp(1 - a, -1.0, 1.0));
}

// a quantity against wavelength, per screen channel, weighted by the disc centre's spectrum (render/sun_limb.h's weights)
template <class F> std::array<double, 3> perChannel(F f) {
    using namespace sunlimb;
    std::array<double, 3> num{}, den{};
    for (int k = 0; k < kSunEtrN; k++) {
        const double nm = kSunEtrNm0 + k * kSunEtrStepNm;
        double A[6]; nl94At(nm, A);
        const cie::V3 rgb = cie::linearSrgb(cie::cmf(nm));
        const double e = kSunEtr[k] / limbFlux(A), v = f(nm);
        for (int c = 0; c < 3; c++) { num[std::size_t(c)] += rgb[std::size_t(c)] * e * v; den[std::size_t(c)] += rgb[std::size_t(c)] * e; }
    }
    for (int c = 0; c < 3; c++) num[std::size_t(c)] /= den[std::size_t(c)];
    return num;
}
inline const std::array<double, 3>& umbraRgb() { static const auto u = perChannel(umbraAt); return u; }
inline const std::array<double, 3>& granContrastRgb() { static const auto g = perChannel(granContrastAt); return g; }

// ---- the granulation pattern (the GLSL twin: kPlanetFS pcgHash, granRaw, granAt) ----
inline std::uint32_t pcgHash(std::uint32_t v) {      // M. O'Neill's PCG, one step and its output (the RXS-M-XS variant), 32 bits
    const std::uint32_t s = v * 747796405u + 2891336453u;
    const std::uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
// F1 at q (in grid spacings): the distance to the nearest jittered point of the grid, pattern k
// Each pattern's grid is shifted by its own offset (less than a cell), so patterns do not share the grid (without it, patterns a lifetime apart correlated by 0.02).
inline double granRaw(const double qIn[3], std::uint32_t k) {
    const std::uint32_t hk = pcgHash(k);
    const double q[3] = {qIn[0] + pcgHash(hk + 1u) * (1.0 / 4294967296.0), qIn[1] + pcgHash(hk + 2u) * (1.0 / 4294967296.0), qIn[2] + pcgHash(hk + 3u) * (1.0 / 4294967296.0)};
    const double fx = std::floor(q[0]), fy = std::floor(q[1]), fz = std::floor(q[2]);
    double f1 = 9;
    for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                const double cx = fx + dx, cy = fy + dy, cz = fz + dz;
                const std::uint32_t h = pcgHash(static_cast<std::uint32_t>(static_cast<std::int32_t>(cx)) + pcgHash(static_cast<std::uint32_t>(static_cast<std::int32_t>(cy)) +
                                                pcgHash(static_cast<std::uint32_t>(static_cast<std::int32_t>(cz)) + hk)));
                const std::uint32_t h2 = pcgHash(h), h3 = pcgHash(h2);
                const double px = cx + h * (1.0 / 4294967296.0), py = cy + h2 * (1.0 / 4294967296.0), pz = cz + h3 * (1.0 / 4294967296.0);
                const double d = std::sqrt((q[0] - px) * (q[0] - px) + (q[1] - py) * (q[1] - py) + (q[2] - pz) * (q[2] - pz));
                f1 = std::min(f1, d);
            }
    return f1;
}
// the fade for a pixel footprint of w cells (of the 1.2 Mm peak): 1 at 3 pixels a cell and more, 0 at 1.5 and less
inline double granFade(double cellsPerPixel) {
    if (!(cellsPerPixel > 0)) return 1;
    const double t = std::clamp((1 / cellsPerPixel - 1.5) / 1.5, 0.0, 1.0);
    return t * t * (3 - 2 * t);
}
// The time of the pattern: tau = t / (lifetime / 2); patterns k and k + 1 with weights cos, sin (pi f / 2). Computed in double on the CPU (the shader gets k and f).
struct GranTime { std::uint32_t k = 0; double f = 0; };
inline GranTime granTime(double jdUT) {
    const double tau = jdUT * 1440.0 / (kGranLifeMin / 2);
    const double kk = std::floor(tau);
    GranTime g;
    g.k = static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(kk)) & 0xffffffffu);
    g.f = tau - kk;
    return g;
}
// the normalised pattern (zero mean, unit rms) at the unit vector b on the Sun's ball (its own frame)
inline double granAt(const double b[3], const GranTime& t) {
    const double s = kSunRadiusMm / kGranGridMm;
    const double q[3] = {b[0] * s, b[1] * s, b[2] * s};
    const double g0 = (kGranMean - granRaw(q, t.k)) / kGranRms, g1 = (kGranMean - granRaw(q, t.k + 1u)) / kGranRms;
    const double a = 1.5707963267948966 * t.f;
    return std::cos(a) * g0 + std::sin(a) * g1;
}

// ---- one spot as the shader draws it ----
struct SunSpotDraw { double v[3] = {0, 0, 1}; double rho = 0, rhoU = 0, pen = 1; };   // centre (unit vector, Sun's frame), whole and umbra radius (radians), penumbra factor
inline SunSpotDraw spotDraw(const double v[3], double rho, double areaMillionths) {
    SunSpotDraw s;
    for (int i = 0; i < 3; i++) s.v[i] = v[i];
    s.rho = rho; s.rhoU = umbraRho(areaMillionths); s.pen = penumbraAt(spotArcsec(rho));
    return s;
}
inline double smoothstep01(double e0, double e1, double x) { const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0); return t * t * (3 - 2 * t); }
// The surface factor per channel at b (spots and grain; the photo is applied by the caller), pw the pixel's size on the unit ball (radians).
// spotCover returns how much of the pixel is in a spot (the grain is not drawn there).
inline void spotsFactor(const double b[3], double pw, const SunSpotDraw* spots, int n, const std::array<double, 3>& U, double out[3], double& cover) {
    out[0] = out[1] = out[2] = 1; cover = 0;
    for (int i = 0; i < n; i++) {
        const SunSpotDraw& s = spots[i];
        const double ang = std::acos(std::clamp(b[0] * s.v[0] + b[1] * s.v[1] + b[2] * s.v[2], -1.0, 1.0));
        const double inW = 1 - smoothstep01(s.rho - 0.5 * pw, s.rho + 0.5 * pw, ang), inU = 1 - smoothstep01(s.rhoU - 0.5 * pw, s.rhoU + 0.5 * pw, ang);
        for (int c = 0; c < 3; c++) out[c] *= 1 - inW * (1 - s.pen) - inU * (s.pen - U[std::size_t(c)]);
        cover = std::max(cover, inW);
    }
}

}  // namespace render
