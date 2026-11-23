// 0.50 (review item 3): how a planet or moon is lit, in LINEAR light. The C++ twin of the shading step of render/shaders.h (kFragmentShader mode 2, the
// lit ball, and kPlanetFS, the planet), so the rule can be tested without a graphics card (tests/test_orbit.cpp, testShading050).
//
// The colours the app holds (the photo maps, the measured colours) are sRGB-encoded: a byte is not proportional to light. Lambert's law (reflected light
// proportional to cos i, i the angle of incidence) holds for light, so each colour is first DECODED to linear light with the IEC 61966-2-1 (sRGB) transfer
// function, multiplied by cos i (0 where the Sun is below the horizon), then ENCODED back for the screen. No ambient floor (space has no light from all
// round: the night side is dark) and no white highlight (it had no source). The photo maps are uploaded as GL_SRGB8_ALPHA8 so the GPU decodes them
// (and averages the mipmaps in linear light); the measured colours (vColor) are decoded in the shader. The encoding is done in these shaders only:
// GL_FRAMEBUFFER_SRGB stays off because the UI shares the framebuffer.
// IEC 61966-2-1:1999, sRGB: decode c <= 0.04045: c / 12.92, else ((c + 0.055) / 1.055)^2.4; encode l <= 0.0031308: 12.92 l, else 1.055 l^(1/2.4) - 0.055.
#pragma once

#include <algorithm>
#include <cmath>
#include <array>
#include "cie1931.h"

namespace render {

inline double srgbDecode(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
inline double srgbEncode(double l) {
    l = std::clamp(l, 0.0, 1.0);
    return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1.0 / 2.4) - 0.055;
}

// One channel of a lit surface: its sRGB-encoded colour (0..1) and the light factor (cos i, times the ring shadow's transmission; clamped at 0).
// space: the sRGB colour of empty space behind the picture (Renderer2D::setSpace, the shaders' uSpace): the screen's "no light" level, so a night side
// is drawn in the sky's own colour (black space drawn as the picture's dark background). The surface's light is added to it in linear light.
// Returns the sRGB-encoded value drawn on the screen.
inline double shadeEncoded(double srgb, double lit, double space = 0) { return srgbEncode(srgbDecode(space) + srgbDecode(srgb) * std::max(lit, 0.0)); }

// 0.77.0: a map's linear colour scaled to the body's measured brightness (render/planet_maps.h mapAlbedo): each channel times the factor k, then a
// pixel above 1 in any channel divided by its largest channel (its hue kept, its brightness clipped). The C++ twin of kPlanetFS's uAlbedoK step.
inline void albedoScaled(double rgb[3], double k) {
    for (int c = 0; c < 3; c++) rgb[c] *= k;
    const double mx = std::max(1.0, std::max(rgb[0], std::max(rgb[1], rgb[2])));
    for (int c = 0; c < 3; c++) rgb[c] /= mx;
}

// 0.77.0: Earth's clouds over its ground, in linear light: a cover a (0..1, the cloud map's byte / 255) of white cloud over the ground's colour,
// rho' = rho (1 - a) + a. The rule that best rebuilds NASA's own composite of the same two layers (Blue Marble 2002, land_ocean_ice_cloud_2048.jpg,
// from cloud_combined_2048.jpg: REPORT 0.77.0). Flat clouds of one moment: no height, no shadows; lit by the Sun and seen through the air like the ground.
// The C++ twin of kPlanetFS's uClouds step.
inline void cloudsOver(double rgb[3], double a) { for (int c = 0; c < 3; c++) rgb[c] = rgb[c] * (1 - a) + a; }

// The light factor of a ball's pixel: n the unit surface normal, L the unit direction to the Sun (screen frame: right, up, towards you).
inline double lambertLit(const double n[3], const double L[3]) { return std::max(0.0, n[0] * L[0] + n[1] * L[1] + n[2] * L[2]); }

// 0.50: the Moon is not a Lambert surface (the full Moon is nearly flat to the limb). McEwen's lunar photometric function, as given in Driver et al. 2025,
// "Stereophotoclinometry Revisited", arXiv:2504.08252, Table 1, Eq. (23) (saved: data/photometry/arxiv_2504.08252_html_2026-11-11.html):
//   d = (1 - g(phi)) cos i + g(phi) 2 cos i / (cos i + cos e),   g(phi) = exp(-phi / 60), phi the phase angle in degrees,
// a Lambert and a Lommel-Seeliger term weighted by the phase; the weight was fit to Galileo images of the Moon (McEwen 1991, Icarus 92, 298; McEwen 1996, LPSC 27, 841)
// and the exponential is the approximation to McEwen's polynomial that the paper uses (its ref. [6], Gaskell et al. 2008). i incidence, e emission angle.
// Returns the light factor that replaces cos i (0 where the Sun is below the horizon or the point faces away).
inline double mcewenLunar(double cosI, double cosE, double phaseDeg) {
    if (cosI <= 0 || cosE <= 0) return 0;
    const double g = std::exp(-phaseDeg / 60.0);
    return (1 - g) * cosI + g * 2 * cosI / (cosI + cosE);
}

// ---- 0.51.2: eclipse shadows ----
// The share of the Sun's disc that a round body hides, seen from a point: two discs on the sky, the Sun's of angular radius a, the body's of angular
// radius b, their centres theta apart (all radians). The overlap of two circles (the lens area: the two circular segments), over the Sun's disc pi a^2.
// Flat-sky geometry: exact for small discs; the Sun is at most 0.83 deg across from Mercury, and a body much bigger than the Sun is only crossed by it
// near its own edge, where its outline is straight to within (a/b)^2. The Sun is taken as evenly bright (its limb darkening is left out: the app says so).
// 0 = no eclipse, 1 = total (the body's disc covers the Sun's).
inline double sunCovered(double a, double b, double theta) {
    if (a <= 0 || b <= 0 || theta >= a + b) return 0;
    if (theta <= std::fabs(a - b)) return b >= a ? 1.0 : (b * b) / (a * a);   // total, or annular (the body inside the Sun's disc)
    const double d = theta;
    const double p = std::acos(std::clamp((d * d + a * a - b * b) / (2 * d * a), -1.0, 1.0)), q = std::acos(std::clamp((d * d + b * b - a * a) / (2 * d * b), -1.0, 1.0));
    const double lens = a * a * p + b * b * q - 0.5 * std::sqrt(std::max(0.0, (-d + a + b) * (d + a - b) * (d - a + b) * (d + a + b)));
    return std::clamp(lens / (3.14159265358979323846 * a * a), 0.0, 1.0);
}

// The sunlight that reaches a point s of a body (screen frame, units of the body's radius) past a round occulter at o of radius ro (same units):
// L the unit direction to the Sun, sunAng the Sun's angular radius there. 1 = full sunlight. The occulter must be on the Sun's side (o - s along L).
inline double sunlightPast(const double s[3], const double o[3], double ro, const double L[3], double sunAng) {
    const double d[3] = {o[0] - s[0], o[1] - s[1], o[2] - s[2]};
    const double dist = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (dist <= ro) return 0;
    const double along = d[0] * L[0] + d[1] * L[1] + d[2] * L[2];
    if (along <= 0) return 1;
    const double cx = d[1] * L[2] - d[2] * L[1], cy = d[2] * L[0] - d[0] * L[2], cz = d[0] * L[1] - d[1] * L[0];
    const double theta = std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), along);   // atan2: exact for the tiny angles of an eclipse (acos loses them)
    return 1 - sunCovered(sunAng, std::asin(ro / dist), theta);
}

// ---- 0.51.2: Earth's air (Rayleigh scattering, single scattering) ----
// The air's Rayleigh optical depth every 5 nm (tools/make_rayleigh_data.py) and the linear-sRGB weights of the CIE 1931 observer under D65 sunlight
// (0.52.1: made here from the app's one CIE table, cie1931.h; until 0.52.0 make_rayleigh_data.py wrote them, rounded to 7 digits, from the same table).
namespace air {
#include "rayleigh_data.inc"
static_assert(kAirN == cie::kD65N && kAirLambdaNm0 == cie::kD65Nm0 && kAirLambdaStepNm == cie::kD65StepNm, "the air table and D65 share their 5 nm samples");
// w_c(k) = sum_j M[c][j] D65(k) cmf_j(k) / sum_k D65 ybar, each channel then scaled to sum to exactly 1 (white light stays (1, 1, 1); the sums are 1 within 0.2 % before)
inline const std::array<std::array<double, kAirN>, 3>& weights() {
    static const auto w = [] {
        std::array<std::array<double, kAirN>, 3> t{};
        double ysum = 0;
        for (int k = 0; k < kAirN; k++) ysum += cie::kD65[k] * cie::cmf(kAirLambdaNm0 + k * kAirLambdaStepNm)[1];
        for (int k = 0; k < kAirN; k++) {
            const cie::V3 rgb = cie::linearSrgb(cie::cmf(kAirLambdaNm0 + k * kAirLambdaStepNm));
            for (int c = 0; c < 3; c++) t[c][k] = cie::kD65[k] * rgb[c] / ysum;
        }
        for (auto& row : t) { double s = 0; for (double v : row) s += v; for (double& v : row) v /= s; }
        return t;
    }();
    return w;
}
}
// The path length through the air, in units of the vertical column, seen at cos(zenith angle) mu, for a homogeneous shell of height H on a ball of
// radius R (k = R / H): the straight line's length through the shell, sqrt((k mu)^2 + 2k + 1) - k mu (exact geometry of the shell; 1/mu overhead, about
// sqrt(2k) at the horizon instead of infinity). A homogeneous shell of height H holds the same air as the exponential atmosphere of scale height H.
inline double airMass(double mu, double k) { mu = std::clamp(mu, 0.0, 1.0); return std::sqrt(k * k * mu * mu + 2 * k + 1) - k * mu; }
// Earth: mean radius 6,371 km and the air's scale height 8.5 km (NSSDC Earth fact sheet, data/nssdc/earthfact.html).
constexpr double kEarthAirK = 6371.0 / 8.5;
// The share of D65 sunlight in channel c (0 r, 1 g, 2 b; linear sRGB) that crosses m vertical air columns unscattered: sum_k w_c(k) exp(-tau(k) m).
// Exact for a grey surface; for a coloured one it is the channel's band average (an approximation).
inline double airTransmit(int c, double m) {
    double t = 0;
    for (int k = 0; k < air::kAirN; k++) t += air::weights()[c][k] * std::exp(-air::kAirTau[k] * m);
    return t;
}
// One channel of a point on Earth's day side, in the units of shadeEncoded's light (pi I / F: a white Lambert surface lit straight on is 1):
// rho the surface's linear colour (the map), mu0 = cos(Sun's zenith angle), mu = cos(the viewer's zenith angle), cosScat the cosine of the scattering
// angle (between the sunlight's direction of travel and the direction to the viewer). The air in single scattering (Chandrasekhar 1960, Radiative
// Transfer: the same thin-layer formula as the rings, saturn_rings.h shine): the surface's light dimmed on the way in and out, rho mu0 exp(-tau m),
// plus the light the air scatters towards you, P(Theta) / 4 * mu0 / (mu0 + mu) * (1 - exp(-tau m)), with Rayleigh's phase function
// P = 3/4 (1 + cos^2 Theta) and m = 1/mu0 + 1/mu. Here 1/mu0 and 1/mu are the shell's air masses (airMass) and mu0 / (mu0 + mu) is X / (X0 + X).
// Left out (the app says so): light scattered more than once (it adds more blue, mostly near the edge), the light the surface sends back up into the
// air, the air's glow beyond the edge of the disc (a few kilometres: under a pixel), twilight beyond the terminator, ozone, aerosols and clouds.
inline double earthAirLight(int c, double rho, double mu0, double mu, double cosScat, double k = kEarthAirK) {
    if (mu0 <= 0 || mu <= 0) return 0;
    const double X0 = airMass(mu0, k), X = airMass(mu, k), T = airTransmit(c, X0 + X);
    const double P = 0.75 * (1 + cosScat * cosScat);
    return rho * mu0 * T + P / 4 * X / (X0 + X) * (1 - T);
}

// The table the planet shader reads (kPlanetFS uAirT): airTransmit at kAirTableN path lengths m, evenly spaced in ln m from 2 (Sun and viewer overhead)
// to twice the horizon's air mass; the shader interpolates linearly in ln m (airTableLookup, its twin; tests/test_orbit.cpp checks it against airTransmit).
constexpr int kAirTableN = 48;
inline double airTableLnLo() { return std::log(2.0); }
inline double airTableLnHi(double k = kEarthAirK) { return std::log(2 * airMass(0, k)); }
inline double airTableM(int i, double k = kEarthAirK) { return std::exp(airTableLnLo() + (airTableLnHi(k) - airTableLnLo()) * i / (kAirTableN - 1)); }
inline double airTableLookup(const double* table, int c, double m, double k = kEarthAirK) {   // table: 3 values (r, g, b) a row, kAirTableN rows
    const double u = std::clamp((std::log(std::max(m, 2.0)) - airTableLnLo()) / (airTableLnHi(k) - airTableLnLo()) * (kAirTableN - 1), 0.0, kAirTableN - 1.0);
    const int i = std::min(static_cast<int>(u), kAirTableN - 2);
    const double f = u - i;
    return table[3 * i + c] * (1 - f) + table[3 * (i + 1) + c] * f;
}

}  // namespace render
