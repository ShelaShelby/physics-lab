// The colour of a dot by the phase of the wave: a CYCLIC colour map, so that phase 0 and 2 pi have the same colour. No graphics in here, so it can be tested.
//
// The map is a hue circle of constant lightness in CIE 1976 L*a*b* ("CIELAB"): a* = C cos(arg), b* = C sin(arg), at a fixed L* and chroma C, converted to sRGB with the standard
// D65 formulas (CIE XYZ -> linear sRGB matrix and gamma of IEC 61966-2-1); colours outside the sRGB range are clipped. Constant L* keeps the brightness the same for every
// phase, so the picture shows the phase and not a brightness pattern. phase 0 = +a* (a pink-red), pi/2 = +b* (yellow-orange), pi = -a* (green), 3 pi/2 = -b* (blue).
// (The formulas are the standard ones; they were written from knowledge, not looked up in this session.) Dark pages draw dots as added light, so L* is higher; light pages
// draw darker, more saturated colours.
#pragma once

#include <algorithm>
#include <cmath>

namespace atom {

struct PhaseRgb { float r, g, b; };
constexpr int kPhaseBins = 24;                     // the wheel is cut into this many colours when dots are drawn in batches

inline PhaseRgb phaseRgb(double arg, bool lightPage) {
    const double L = lightPage ? 52 : 72, C = lightPage ? 58 : 52;
    const double a = C * std::cos(arg), b = C * std::sin(arg);
    const double fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
    const auto finv = [](double t) { return t > 6.0 / 29 ? t * t * t : 3 * (6.0 / 29) * (6.0 / 29) * (t - 4.0 / 29); };
    const double X = 0.95047 * finv(fx), Y = finv(fy), Z = 1.08883 * finv(fz);          // D65 white point
    const double lin[3] = {3.2406 * X - 1.5372 * Y - 0.4986 * Z, -0.9689 * X + 1.8758 * Y + 0.0415 * Z, 0.0557 * X - 0.2040 * Y + 1.0570 * Z};
    float c[3];
    for (int i = 0; i < 3; i++) {
        const double v = std::max(0.0, std::min(1.0, lin[i]));
        c[i] = float(v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055);
    }
    return {c[0], c[1], c[2]};
}

// which of the kPhaseBins colours a phase falls in (arg in any range; bin 0 is centred on phase 0), and the phase at a bin's middle
inline int phaseBin(double arg) {
    const double t = std::fmod(arg / (2 * 3.14159265358979323846) + 0.5 / kPhaseBins, 1.0);
    return std::min(kPhaseBins - 1, int((t < 0 ? t + 1 : t) * kPhaseBins));
}
inline double phaseOfBin(int bin) { return 2 * 3.14159265358979323846 * bin / kPhaseBins; }

}  // namespace atom
