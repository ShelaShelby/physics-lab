// 0.96.0 (outside review of 28 Nov 2026, part C): how Jupiter's and Saturn's discs darken towards their edges and change with the phase angle. The C++ twin of
// kPlanetFS's uGiant step (render/shaders.h); the shader gets the numbers giantLight() works out for each ball (Renderer2D::planet), and
// tests/planet_picture.cpp draws the same ball with them. Unit tests: tests/test_orbit.cpp testGiantPhotometry.
//
// Both laws replace Lambert's cos i (the light factor "lit" of render/shading.h). Each is scaled so that the full disc at opposition (phase 0) reflects
// what a Lambert ball does (2/3 of the map's albedo): the maps keep their measured-albedo calibration (render/planet_maps.h mapAlbedo), and the laws
// change only how the light is spread over the disc and how it falls with the phase angle.
//
// Jupiter. U. Dyudina et al. 2016, ApJ 822, 76 (arXiv:1511.04415), Eq. (1): I/F = mu0 P(alpha), a Lambert-shaped disc times a phase function P.
// P is taken from Cassini's measured phase curves of the whole disc: L. C. Mayorga et al. 2016, AJ 152, 209 (arXiv:1610.07679), Table 2, 5th-order
// polynomials in x = alpha / 180 deg, 1 at full phase. For I/F = mu0 P(alpha) the disc's phase curve is P(alpha) Phi_L(alpha), with Phi_L the Lambert
// sphere's phase function [sin a + (pi - a) cos a] / pi, so P_c = Phi_Mayorga,c / Phi_L reproduces Mayorga's curve exactly. Channels: the app's red,
// green and blue are Mayorga's RED (647 nm), GRN (568 nm) and BL1 (463 nm) (Table 1, the wide-angle camera's effective wavelengths). The fits are
// "untrustworthy beyond ~130 deg" (section 4; their stated range 0-135 deg), so past 135 deg P keeps its 135 deg value (a choice of this app; there the
// thin crescent is under 15 % lit). Saved: data/planets/photometry/1610.07679_mayorga2016.txt.gz, extract native/data/planets/photometry/1610.07679_mayorga2016.extract.txt.
//
// Saturn. Dyudina et al. 2016 Eq. (4), the Barkstrom law I/F = (A / mu) (mu mu0 / (mu + mu0))^B, with A and B against the phase angle from Table 3
// (Dones et al. 1993, fitted to Pioneer 11's retrievals of Tomasko & Doose 1984), the red row (0.64 um), in all three channels. Between the table's
// 30 deg steps A and B are interpolated linearly (a choice of this app). The blue row (0.44 um) is not used: integrated over the disc as printed it gives
// a spherical albedo of 0.453 where the paper's Table 4 says 0.39 (REPORT 0.96.0). Saved: data/planets/photometry/1511.04415_dyudina2016.txt.gz,
// extract native/data/planets/photometry/1511.04415_dyudina2016.extract.txt.
//
// Limits (said in the app's text, ui/shell_orbit.cpp): whole-disc laws, the same for every latitude (the belts and zones are not given their own),
// at the filters' wavelengths, not across the eye's whole band; Saturn's rings are not part of the law (their shadow on the ball is drawn as before).
#pragma once

#include <algorithm>
#include <cmath>

namespace render {

// Mayorga et al. 2016 Table 2: coefficients of 1, x, x^2, ... x^5 (x = alpha / 180 deg), in the app's channel order red, green, blue = RED, GRN, BL1
constexpr double kMayorgaJupiter[3][6] = {
    {1.000, -0.882, -3.923, 8.142, -5.776, 1.439},    // RED, 647 nm
    {1.000, -1.507, -0.363, -0.062, 2.809, -1.876},   // GRN, 568 nm
    {1.000, -1.311, -2.382, 5.893, -4.046, 0.846},    // BL1, 463 nm
};
constexpr double kMayorgaMaxDeg = 135;   // the end of the fits' stated range (section 4)

// Dyudina et al. 2016 Table 3 (Dones et al. 1993), Saturn, red (0.64 um), at alpha = 0, 30, ... 180 deg
constexpr double kSaturnA[7] = {1.69, 1.59, 1.45, 1.34, 1.37, 2.23, 3.09};
constexpr double kSaturnB[7] = {1.48, 1.48, 1.46, 1.42, 1.36, 1.34, 1.31};

constexpr double kGiantPi = 3.14159265358979323846;

// Mayorga's polynomial: the disc's brightness at phase alphaDeg over its brightness at full phase
inline double mayorgaPhase(const double c[6], double alphaDeg) {
    const double x = alphaDeg / 180;
    double v = 0, m = 1;
    for (int i = 0; i < 6; i++) { v += c[i] * m; m *= x; }
    return v;
}

// the Lambert sphere's phase function, 1 at full phase (Russell 1916; e.g. Dyudina et al. 2016's "Lambertian" row: Ag 2/3, AS 1)
inline double lambertPhase(double alphaDeg) {
    const double a = alphaDeg * kGiantPi / 180;
    return (std::sin(a) + (kGiantPi - a) * std::cos(a)) / kGiantPi;
}

// Jupiter's phase factor P_c (channel 0 red, 1 green, 2 blue): Mayorga's curve over Lambert's, held at its 135 deg value past 135 deg; P_c(0) = 1
inline double jupiterP(int ch, double alphaDeg) {
    const double a = std::clamp(alphaDeg, 0.0, kMayorgaMaxDeg);
    if (a < 1e-9) return 1.0;
    return mayorgaPhase(kMayorgaJupiter[ch], a) / lambertPhase(a);
}

// Saturn's Barkstrom coefficients at phase alphaDeg (linear between the table's 30 deg steps)
inline void saturnAB(double alphaDeg, double& A, double& B) {
    const double u = std::clamp(alphaDeg, 0.0, 180.0) / 30;
    const int i = std::min(static_cast<int>(u), 5);
    const double t = u - i;
    A = kSaturnA[i] + t * (kSaturnA[i + 1] - kSaturnA[i]);
    B = kSaturnB[i] + t * (kSaturnB[i + 1] - kSaturnB[i]);
}

// the Barkstrom law, Dyudina et al. 2016 Eq. (4): I/F = (A / mu) (mu mu0 / (mu + mu0))^B = A mu0^B mu^(B - 1) / (mu + mu0)^B; 0 off the lit, seen side
inline double barkstrom(double A, double B, double mu0, double mu) {
    if (mu0 <= 0 || mu <= 0) return 0;
    return A * std::pow(mu0, B) * std::pow(mu, B - 1) / std::pow(mu + mu0, B);
}

// its full disc's geometric albedo at phase 0 (mu = mu0): 2 int_0^1 A mu^(B-1) 2^-B mu dmu = 2 A / (2^B (B + 1))
inline double barkstromAg0(double A, double B) { return 2 * A / (std::pow(2.0, B) * (B + 1)); }

// Saturn's scale: a Lambert ball's 2/3 over the red law's own geometric albedo (0.4886 from Table 3; the paper's Table 4: 0.483)
inline double saturnScale() { return (2.0 / 3.0) / barkstromAg0(kSaturnA[0], kSaturnB[0]); }

// What a ball's shader step needs (Renderer2D::planet, kPlanetFS uGiant / uGiantP): kind 0 none (Lambert, McEwen ...), 1 Jupiter: p = (P_r, P_g, P_b),
// 2 Saturn: p = (A, B, scale). L.z (the unit direction to the Sun, z towards the viewer) gives the phase angle, alpha = acos(L.z).
struct GiantLight { int kind = 0; double p[3] = {1, 1, 1}; };
inline GiantLight giantLight(int kind, double Lz) {
    GiantLight g;
    const double alpha = std::acos(std::clamp(Lz, -1.0, 1.0)) * 180 / kGiantPi;
    if (kind == 1) { g.kind = 1; for (int c = 0; c < 3; c++) g.p[c] = jupiterP(c, alpha); }
    else if (kind == 2) { g.kind = 2; saturnAB(alpha, g.p[0], g.p[1]); g.p[2] = saturnScale(); }
    return g;
}

// The light factor of one pixel and channel under the law (cos i = mu0, cos e = mu): it replaces Lambert's max(mu0, 0) (and is times the ring shadow after)
inline double giantLit(const GiantLight& g, int ch, double mu0, double mu) {
    if (g.kind == 1) return std::max(mu0, 0.0) * g.p[ch];
    if (g.kind == 2) return g.p[2] * barkstrom(g.p[0], g.p[1], mu0, mu);
    return std::max(mu0, 0.0);
}

}  // namespace render
