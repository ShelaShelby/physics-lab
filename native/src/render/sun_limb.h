// 0.85.0 (outside review, PLAN_REVIEW.md R2): the Sun's limb darkening from a measured law, for the Sun's disc (kPlanetFS uMode 4) and the eclipses
// (kPlanetFS sunlightPast). The C++ twin the tests check the shaders against.
//
// Source: H. Neckel, D. Labs, "Solar limb darkening 1986-1990 (lambda lambda 303 to 1099 nm)", Solar Physics 153, 91-114 (1994), Table I and Equations (5), (6)
// (extract native/data/sun/neckel_labs_1994_SoPh153_91.extract.txt). I(mu) / I(1) = P5(mu) = A0 + A1 mu + ... + A5 mu^5, mu = cos theta on the disc, Sum A_n = 1.
// The disc's mean over its centre, F/I = 2 Sum A_n / (n + 2) (Equation 6).
// Valid 303-1099 nm. Measured 1986-1990. The last 7 arcsec of the radius were not measured (section 3.2): there the polynomial is extrapolated.
//
// One channel c of the screen (linear sRGB): I_c(mu) = Sum_lambda w_c(lambda) E(lambda) / (F/I)(lambda) P5(mu, lambda), with
//   w_c the linear-sRGB weight of the CIE 1931 observer (render/cie1931.h), E the ASTM G173-03 extraterrestrial spectrum (sun_limb_data.inc),
//   E / (F/I): the disc-centre spectrum (the ASTM spectrum is the whole disc's light; Equation 6).
// The A_n are interpolated linearly in wavelength between Table I's rows (the app's choice; NL94 p. 97: "almost smooth functions of wavelength").
// F/I at a wavelength is Equation (6) of the interpolated A_n. Samples every 5 nm from 380 to 780 nm (the CIE table's grid).
// A sum of quintics is a quintic: each channel is 6 coefficients, scaled to sum to 1 (I_c(1) = 1). The disc centre's colour has its brightest channel 1:
// the picture's exposure is set so the middle of the Sun just reaches the screen's white.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "cie1931.h"

namespace render {
namespace sunlimb {
#include "sun_limb_data.inc"
}

struct SunLimb {
    double poly[3][6] = {};   // channel r, g, b: P_c(mu) = Sum_n poly[c][n] mu^n, P_c(1) = 1
    double centre[3] = {};    // the disc centre's linear sRGB, brightest channel 1
};

// Equation (6): the disc mean over the centre for coefficients A
inline double limbFlux(const double A[6]) { double f = 0; for (int n = 0; n < 6; n++) f += 2 * A[n] / (n + 2); return f; }
inline double limbP(const double A[6], double mu) { double p = 0, m = 1; for (int n = 0; n < 6; n++) { p += A[n] * m; m *= mu; } return p; }

// Table I's coefficients at a wavelength (nm), linear between the rows; outside 303.327-1098.950 nm: the end row
inline void nl94At(double nm, double A[6]) {
    using namespace sunlimb;
    int i = 0;
    while (i < kNl94N - 2 && nm > kNl94[i + 1][0]) i++;
    const double f = std::clamp((nm - kNl94[i][0]) / (kNl94[i + 1][0] - kNl94[i][0]), 0.0, 1.0);
    for (int n = 0; n < 6; n++) A[n] = kNl94[i][1 + n] + f * (kNl94[i + 1][1 + n] - kNl94[i][1 + n]);
}

// One channel's normalised coefficients from samples (nm[k], weight[k]), each weight a share of the disc-AVERAGE light (it is divided by F/I here)
inline void limbFromSamples(int n, const double* nm, const double* weight, double out[6], double* centre = nullptr) {
    double s[6] = {}, c = 0;
    for (int k = 0; k < n; k++) {
        double A[6]; nl94At(nm[k], A);
        const double w = weight[k] / limbFlux(A);
        c += w;
        for (int q = 0; q < 6; q++) s[q] += w * A[q];
    }
    double p1 = 0;
    for (int q = 0; q < 6; q++) p1 += s[q];
    for (int q = 0; q < 6; q++) out[q] = s[q] / p1;   // P(1) = 1 exactly (Table I's rows sum to 1 within 3e-5)
    if (centre) *centre = c;
}

inline const SunLimb& sunLimb() {
    static const SunLimb L = [] {
        using namespace sunlimb;
        SunLimb r;
        std::array<double, kSunEtrN> nm{}, w[3];
        for (int k = 0; k < kSunEtrN; k++) {
            nm[std::size_t(k)] = kSunEtrNm0 + k * kSunEtrStepNm;
            const cie::V3 rgb = cie::linearSrgb(cie::cmf(nm[std::size_t(k)]));
            for (int c = 0; c < 3; c++) w[c][std::size_t(k)] = rgb[std::size_t(c)] * kSunEtr[k];
        }
        for (int c = 0; c < 3; c++) limbFromSamples(kSunEtrN, nm.data(), w[c].data(), r.poly[c], &r.centre[c]);
        const double m = std::max({r.centre[0], r.centre[1], r.centre[2]});
        for (double& v : r.centre) v /= m;
        return r;
    }();
    return L;
}

// ---- the eclipses with the measured edge ----
// The share of the Sun's light hidden by a round body, one channel with coefficients A (Sum A_n = 1): the Sun's disc of angular radius a, the body's of b,
// centres theta apart (radians; flat sky, as render/shading.h sunCovered). In units of a: r the distance from the Sun's centre, mu = sqrt(1 - r^2).
// covered = Int_0^1 P(mu(r)) arc(r) r dr / (pi F), arc(r) the angle of the circle of radius r inside the body's disc (2 pi where it is all inside).
// The part where the whole circle is inside (r < b - theta) is exact: Int_0^R mu^n 2 r dr = 2 / (n + 2) (1 - (1 - R^2)^((n + 2) / 2)).
// The part where the circle is cut: r = r0 + (r1 - r0) (1 - cos t) / 2, t from 0 to pi, the midpoint rule in t with kLimbSteps steps (the square-root
// ends of arc(r) and of mu at r = 1 become smooth in t), integrated by 16-point Gauss-Legendre (its nodes and weights: kGl16, the positive half).
// The C++ twin of kPlanetFS sunCoveredLimb.
constexpr double kGl16x[8] = {0.095012509837637441, 0.28160355077925892, 0.45801677765722737, 0.61787624440264377, 0.755404408355003, 0.86563120238783176, 0.9445750230732326, 0.98940093499164994};
constexpr double kGl16w[8] = {0.18945061045506864, 0.18260341504492364, 0.16915651939500265, 0.14959598881657671, 0.12462897125553407, 0.095158511682492605, 0.062253523938647456, 0.027152459411754176};
inline double sunCoveredLimb(double a, double b, double theta, const double A[6]) {
    if (a <= 0 || b <= 0 || theta >= a + b) return 0;
    const double B = b / a, T = theta / a, F = limbFlux(A);
    double full = 0;                                         // the inner part, whole circles
    const double rin = std::min(1.0, std::max(0.0, B - T));
    if (rin > 0) { const double q = 1 - rin * rin; for (int n = 0; n < 6; n++) full += A[n] * 2 / (n + 2) * (1 - std::pow(q, (n + 2) / 2.0)); }
    const double r0 = std::max(rin, std::fabs(T - B)), r1 = std::min(1.0, T + B);
    double cut = 0;
    if (r1 > r0 && T > 0) {
        const double pi = 3.14159265358979323846;
        for (int k = 0; k < 16; k++) {
            const double x = k < 8 ? -kGl16x[7 - k] : kGl16x[k - 8], w = k < 8 ? kGl16w[7 - k] : kGl16w[k - 8];
            const double t = pi / 2 * (x + 1);
            const double r = r0 + (r1 - r0) * (1 - std::cos(t)) / 2, dr = (r1 - r0) * std::sin(t) / 2 * pi / 2 * w;
            const double ca = std::clamp((r * r + T * T - B * B) / (2 * r * T), -1.0, 1.0);
            cut += limbP(A, std::sqrt(std::max(0.0, 1 - r * r))) * 2 * std::acos(ca) * r * dr;
        }
        cut /= pi;                                           // (the arc's share of 2 pi, times 2 r dr)
    }
    return std::clamp((full + cut) / F, 0.0, 1.0);
}

// render/shading.h sunlightPast with the measured edge, per channel (out[c] = 1: full sunlight)
inline void sunlightPastLimb(const double s[3], const double o[3], double ro, const double L[3], double sunAng, double out[3], const SunLimb& lim = sunLimb()) {
    const double d[3] = {o[0] - s[0], o[1] - s[1], o[2] - s[2]};
    const double dist = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (dist <= ro) { out[0] = out[1] = out[2] = 0; return; }
    const double along = d[0] * L[0] + d[1] * L[1] + d[2] * L[2];
    out[0] = out[1] = out[2] = 1;
    if (along <= 0) return;
    const double cx = d[1] * L[2] - d[2] * L[1], cy = d[2] * L[0] - d[0] * L[2], cz = d[0] * L[1] - d[1] * L[0];
    const double theta = std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), along);
    for (int c = 0; c < 3; c++) out[c] = 1 - sunCoveredLimb(sunAng, std::asin(ro / dist), theta, lim.poly[c]);
}

}  // namespace render
