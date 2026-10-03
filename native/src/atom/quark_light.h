// How the quarks' colours are drawn on a LIGHT theme (no graphics in here, so it can be tested).
// The glowing look of the Cinematic quarks adds up light, which only shows on a dark page. On a light page the quarks, their halos, the gluon field and the
// sea pairs are drawn with ordinary alpha blending in these darker, more saturated colours instead, so no dark backdrop is needed behind them.
// Dark themes use the colours unchanged. (The cloud's glow, cloud_light.h, does the same thing for the electron cloud.)
#pragma once

namespace atom {

// colour charge (red, green, blue), 0-255 (the web version's colours)
constexpr double kColourCharge[3][3] = {{255, 72, 72}, {56, 214, 112}, {84, 132, 255}};
// How far the colours are mixed towards black on a light theme (0 = not at all, 1 = black).
constexpr double kLightQuarkDark = 0.35;

inline void lightQuarkColour(const double in[3], double out[3]) {
    const double r = in[0], g = in[1], b = in[2];
    out[0] = r * (1 - kLightQuarkDark); out[1] = g * (1 - kLightQuarkDark); out[2] = b * (1 - kLightQuarkDark);
}

}  // namespace atom
