// Black Hole Lab (0.55.3): the looks, Real and Film (plan section 3, step 4 first part). Plain data and pure functions, no graphics; the view
// (blackhole_view.cpp) and the panel (ui/shell_blackhole.cpp) read what a look draws from here, so no number of the Film look lives in the UI.
//
// REAL (the dot): the picture of 0.55.2 unchanged: the user's spin, the standard thin disc (disc.h) with the colour and brightness shift of each ray
// (C75 (A9), Riazuelo (53)), true colour by the CIE 1931 observer into sRGB (its white is D65), the stated exposure, no lens effects.
//
// FILM: the choices O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, CQG 32, 065001 (2015), arXiv:1502.03808v2 ("J15") states for the film's disc,
// read from the paper (data/blackhole/arxiv/1502.03808_james2015.txt.gz in the private sources repo; the lines used are copied exactly in
// native/data/blackhole/arxiv/1502.03808_james2015.extract.txt, which tests/test_blackhole.cpp testLooks reads). Pages are arXiv v2's.
//   - spin a/M = 0.6: section 4.1.1 p. 26 ("for visual purposes Nolan and Franklin slowed the spin to a/M = 0.6"), section 1.2 p. 5 (from a/M ~ 1, "required
//     to explain the huge time losses"), App. A.3.1 p. 39 ("Gargantua, when visualized, has a moderate spin a/M = 0.6"). The reason, p. 26: "the flattened
//     left edge of the black-hole shadow, and the multiple disk images alongside that left edge, and the off-centred disk would be too confusing for a mass
//     audience". The paper's own disc figures 13 and 14 use a/M = 0.999 (above this lab's top, 0.99).
//   - the disc: section 4.1.1 p. 26: "physically thin and marginally optically thick", in the equatorial plane, "not currently accreting", "cooled to a
//     position-independent temperature T = 4500K, at which it emits a black-body spectrum". Drawn here as the model disc's geometry (the same thin opaque
//     disc in the equatorial plane, the same ray tables) with one temperature: "marginally optically thick" is drawn opaque (the lab has no partly
//     transparent disc). 0.81.0: partly see-through and soft-edged, the lab's own (below, kFilmFade, kFilmTau).
//   - its edges: the caption of Figure 13 p. 24: "inner and outer radii r = 9.26M and r = 18.70M" (the paint-swatch disc; Figure 14's artists' disc is drawn
//     "with the same geometry", p. 25, and Figure 15a is that disc at a/M = 0.6, p. 27). The paper gives no other edges for the film's disc.
//   - no shift: Figure 15a and its caption p. 27, Figure 16's caption p. 28 ("no colour or brightness shifts"); section 4.1.1 p. 24: the light's specific
//     intensity carried along each ray "unchanged, as would be appropriate in flat spacetime". The reason, section 4.2 p. 26: the fully realistic disc
//     (Figure 15c) "is exceedingly lopsided, with the hole's shadow barely discernible".
//   - the colour: App. A.6 p. 43: "We set the white balance of our virtual camera to render a 6500 K blackbody spectrum with equal red, green and blue pixel
//     values by applying a simple gain to each colour channel." The paper's camera used "the published spectral sensitivity curves of a typical motion
//     picture film" (its ref. [64], not in the paper): here the CIE 1931 observer and sRGB (disc.h blackBodyLinear, like every black body in the app) take
//     their place, with one gain per channel so that a 6500 K black body has equal red, green and blue (and its own luminance Y).
//   - the glow: Figure 16's caption p. 28 ("veiling flare", "the look of a soft glow"), section 4.3.1 p. 29: made by convolving the picture with the measured
//     point-spread function of the IMAX lenses, which the paper does not publish. The app's glow is its OWN, a simple blur of the disc's bright parts (glow* below);
//     the label says so ("our own glow, in place of the film's lens flare").
//   - the paper's view: Figure 13's caption p. 24: "a camera at rc = 74.1M and theta_c = 1.511 (86.56 deg)". The caption gives the camera's place only (not
//     its motion nor its field of view); the lab's camera hovers there.
//   NOT in the paper, so not in the app: the black hole's mass (no number is given), the artists' textures (section 4.3.2, App. A.6), the measured lens flare,
//   the film's colour grading and the "embellishments" (section 4.2 p. 28).
#pragma once

#include <array>
#include <string>
#include <vector>

namespace blackhole {

enum class Look { Real = 0, Film = 1 };
const char* lookKey(Look l);                        // "real", "film" (the settings file)
bool lookFromKey(const std::string& k, Look& out);  // false if unknown

// The Film look's numbers, each from J15 (see above)
struct FilmChoices {
    double spin = 0.6;                              // section 4.1.1 p. 26; section 1.2 p. 5; App. A.3.1 p. 39
    double paperSpin = 0.999;                       // Figures 13, 14 (captions pp. 24, 25) and Figure 15's "slowed from a/M = 0.999" (p. 27)
    double discT = 4500;                            // K, section 4.1.1 p. 26
    double whiteT = 6500;                           // K, App. A.6 p. 43
    double rIn = 9.26, rOut = 18.70;                // M, Figure 13's caption p. 24
    double camR = 74.1;                             // M, Figure 13's caption p. 24
    double camThetaDeg = 86.56, camThetaPrinted = 1.511;   // degrees and radians from the spin axis, Figure 13's caption p. 24 ("1.511 (86.56o)")
    double camTheta = 86.56 * 3.14159265358979323846 / 180;   // the camera's angle used: the degrees (1.51076 rad; it rounds to the printed 1.511, while
                                                              // 1.511 rad = 86.574 deg does not round to 86.56)
    double approachShift = 1.5, recedeShift = 0.4;  // section 4.1.2 p. 26: the factors the film left out ("of order 1.5 and 0.4")
    double gasSpeed = 0.55;                         // c, section 4.1.2 p. 26 ("speeds of roughly 0.55c")
};
const FilmChoices& filmChoices();

// What a look draws
struct LookRule {
    Look look = Look::Real;
    bool shifts = true;                             // each ray's g changes the disc's colour and brightness (Real) or not (Film)
    bool oneTemperature = false;                    // one temperature all over the disc (Film: discT)
    double discT = 0;
    bool whiteBalance = false;                      // Film: a black body at whiteT drawn with equal channels
    double whiteT = 0;
    bool glow = false;                              // Film: the app's own glow (glow* below)
    bool fixedSpin = false;                         // Film: the spin and the disc are the film's, the controls locked
    double spin = 0;
    double rIn = 0, rOut = 0;
    double fade = 0, tau = 0;                       // 0.81.0, Film: the lab's own soft edges and see-through disc (below); 0: hard edges, opaque (Real)
};
LookRule lookRule(Look l);

// One gain per linear sRGB channel so that a black body at whiteT (disc.h blackBodyLinear) has equal channels, each its luminance Y (so Y is kept for it)
std::array<double, 3> whiteBalanceGains(double whiteT);
// The Film disc's light: the black body at discT through the white balance, linear sRGB in units of the Sun's surface luminance, and its luminance Y
std::array<double, 3> filmDiscLinear();
double filmDiscY();

// THE GLOW (the app's own; no source: J15's flare is a measured point-spread function it does not publish). Its rule: the part of each pixel's linear light
// FROM THE DISC AND ITS IMAGES ONLY (0.55.4: the film's flare came from its bright disc; the sky and the stars are drawn unchanged under the glow; until 0.55.3
// the whole picture, so the Milky Way was washed out) above kGlowThreshold of the screen's white (per channel), averaged over 4 x 4 pixel blocks, blurred by a Gaussian of standard deviation kGlowSigma x the
// picture's height (in two passes, rows then columns, over at most 3 sigma, light outside the picture counted as none), and kGlowStrength of it added back
// (bilinear between the blocks' centres) in linear light. It adds light only, never darkens a pixel; it does not conserve light: away from the picture's
// edges it adds kGlowStrength x the light above the threshold, spread out (the kernel sums to 1).
constexpr double kGlowThreshold = 0.5, kGlowStrength = 0.6, kGlowSigma = 1.0 / 80;
constexpr int kGlowBlock = 4;
// the blur's standard deviation and reach in block units for a picture H pixels high
double glowSigmaBlocks(int H);
int glowRadiusBlocks(int H);
// The reference (double): lin is W x H x 3 linear light of the disc alone over the screen's white, row by row; returns the glow to add (same layout), not yet times the strength
std::vector<double> glowReference(const std::vector<double>& lin, int W, int H);

// 0.81.0 (PLAN_DISC.md D4): THE FILM DISC'S SOFT EDGES AND SEE-THROUGH. Both are THE LAB'S OWN, NOT THE FILM'S. J15 says the disc is "physically thin and
// marginally optically thick" (section 4.1.1 p. 26). Its App. A.6 (pp. 42-43) samples a thin disc "at each intersection" with the beam attenuated "by the
// optical thickness", and builds a volume disc in Houdini (about 17 million voxels). It publishes no texture, density, voxel or edge-profile numbers.
//   - the fade: the disc's cover goes from 0 at J15's edges (rIn, rOut) to 1 at kFilmFade inside them, in ln r, by the smoothstep 3x^2 - 2x^3 (value and slope
//     continuous; slope 0 at both ends). The width is the lab's choice: tools/discproto's 0.12 (docs/blackhole/disc081_film.txt shows 0.06, 0.12, 0.2).
//   - the see-through: each crossing of the disc's plane covers c = 1 - exp(-tau) of what is behind it, tau = kFilmTau x S x edge (S: the streaks' factor,
//     gas.h; edge: the fade). kFilmTau = 1.5 is the lab's choice standing for "marginally optically thick" (tools/discproto's). The crossings are laid front to
//     back: the pixel gets L1 c1 + L2 c2 (1 - c1) + ..., and the part (1 - c1)(1 - c2)... of the sky behind. A crossing's light L is the film's one colour
//     (times the moving gas's factor), unchanged by tau: tau sets only how much of the pixel it covers. As tau -> infinity every crossing inside the edges
//     covers 1 and the opaque disc of 0.55-0.80 comes back exactly.
constexpr double kFilmFade = 0.12, kFilmTau = 1.5;
double filmEdge(double r, double rIn, double rOut, double fade = kFilmFade);   // the fade (1 well inside, 0 at and beyond the edges)
double filmTau(double S, double edge, double scale = kFilmTau);              // scale x S x edge (0 where S or edge is 0, also for an infinite scale)
double filmCover(double tau);                                                // 1 - exp(-tau) (1 for an infinite tau)
struct FilmLayer { std::array<double, 3> light{}; double cover = 0; };
// the crossings laid front to back (layers[0] nearest the camera): sum of L_k c_k prod_{j<k} (1 - c_j); left (if given): prod (1 - c_k), the part of what is behind
std::array<double, 3> layFrontToBack(const std::vector<FilmLayer>& layers, double* left = nullptr);

// The guide's table "What the film changed": one row per choice
struct FilmRow { std::string physics, choice, reason, where; };
std::vector<FilmRow> filmRows();

}  // namespace blackhole
