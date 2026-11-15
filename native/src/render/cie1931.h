#pragma once
// 0.52.1: the app's one copy of the CIE 1931 2-degree standard observer (1 nm, 360-830 nm) and the CIE D65 spectrum (5 nm, 380-780 nm), from
// colour-science 0.4.7's tables saved in data/stars and data/atmosphere (cie1931_data.inc, tools/make_cie_data.py). Used by Earth's air (render/shading.h)
// and the spectral-line colours of the Atom Lab (atom_core.cpp lamColor). XYZ -> linear sRGB and the sRGB curve: IEC 61966-2-1 (the matrix the
// app's other colour code uses: phase_wheel.h, shaders.h, tools/star_colour.py).
#include <algorithm>
#include <array>
#include <cmath>

namespace cie {
#include "cie1931_data.inc"

using V3 = std::array<double, 3>;
inline constexpr double kSrgbFromXyz[3][3] = {{3.2406, -1.5372, -0.4986}, {-0.9689, 1.8758, 0.0415}, {0.0557, -0.2040, 1.0570}};

// xbar, ybar, zbar at a wavelength (nm): linear between the 1 nm rows, zero outside 360-830 nm
inline V3 cmf(double nm) {
    const double u = (nm - kCmfNm0) / kCmfStepNm;
    if (!(u >= 0) || u > kCmfN - 1) return {0, 0, 0};
    const int i = std::min(int(u), kCmfN - 2);
    const double f = u - i;
    return {kCmf[i][0] + f * (kCmf[i + 1][0] - kCmf[i][0]), kCmf[i][1] + f * (kCmf[i + 1][1] - kCmf[i][1]), kCmf[i][2] + f * (kCmf[i + 1][2] - kCmf[i][2])};
}
inline V3 linearSrgb(const V3& X) {
    V3 r{};
    for (int c = 0; c < 3; c++) r[c] = kSrgbFromXyz[c][0] * X[0] + kSrgbFromXyz[c][1] * X[1] + kSrgbFromXyz[c][2] * X[2];
    return r;
}
inline double srgbEncode(double v) { v = std::clamp(v, 0.0, 1.0); return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055; }
// D65's XYZ with Y = 1: the D65 spectrum under the observer, summed over its 5 nm samples (380-780 nm). Its linear sRGB is (1, 1, 1) within 0.03 %.
inline const V3& d65WhiteXyz() {
    static const V3 w = [] {
        V3 s{};
        for (int k = 0; k < kD65N; k++) { const V3 c = cmf(kD65Nm0 + k * kD65StepNm); for (int j = 0; j < 3; j++) s[j] += kD65[k] * c[j]; }
        return V3{s[0] / s[1], 1.0, s[2] / s[1]};
    }();
    return w;
}
// The displayable colour of one wavelength, in linear sRGB with the largest channel 1. A single wavelength lies outside the sRGB gamut (one channel < 0):
// D65 white is added (in XYZ, which moves the chromaticity straight towards the white point, so the dominant wavelength stays the line's) until every
// channel is >= 0, then the colour is scaled so its largest channel is 1 (the brightest the screen can show at that chromaticity). Zero outside 360-830 nm.
inline V3 spectralLinear(double nm) {
    V3 r = linearSrgb(cmf(nm));
    if (r[0] == 0 && r[1] == 0 && r[2] == 0) return r;
    const V3 w = linearSrgb(d65WhiteXyz());
    double t = 0;
    for (int c = 0; c < 3; c++) t = std::max(t, -r[c] / w[c]);
    for (int c = 0; c < 3; c++) r[c] = std::max(0.0, r[c] + t * w[c]);
    const double m = std::max({r[0], r[1], r[2]});
    for (double& v : r) v /= m;
    return r;
}
}  // namespace cie
