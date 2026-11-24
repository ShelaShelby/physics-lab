// Black Hole Lab (0.56.0): the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms in single precision (render/kerr_sky.h,
// render/shaders.h kKerrSkyFS; the formulas: kerr_closed.h). Here: the camera's numbers for the pass (made in double), the probe pictures (the shader's own
// numbers per pixel) and their comparison with kerr_closed.h's traceClosed in double, used by the start-up check on the user's card and by the tests.
#pragma once

#include <string>
#include <vector>

#include "blackhole/gas.h"
#include "blackhole/kerr_closed.h"
#include "render/renderer2d.h"

namespace blackhole {

// L's camera for the sky pass: spin a, a hovering camera at (r, theta): (a, r, cos theta, sin theta), J15 (A.1), (A.2)'s alpha, omega, varpi, rho there,
// Delta^(1/2), r+, r- (= a^2 / r+), the rays' end r+ + 1e-3 (kerr_closed.h)
void skyCamera(render::Renderer2D::LensSky& L, double a, double r, double theta);

// a probe picture: w x h pixels, the hole in the middle, K pixels per radian, e_theta up and e_phi to the left (holeView's frame with no turn: the picture
// spinPixel reads); the disc's edges for the first image in [rIn, rOut]
struct SkyCase { const char* name; double a, r, th; int w, h; double K, rIn, rOut; };
render::Renderer2D::LensSky skyProbeLens(const SkyCase& c);
Clump skyGasClump(const SkyCase& c);           // 0.72.0: the probe's moving gas: the clump at 8 M (or the disc's nearest edge); 0.79.0: the streaks (no case's own)

// The probe (16 floats per pixel, Renderer2D::skyProbe) against traceClosed in double at each pixel's centre. The reference's shadow (its ends) gives each
// pixel its distance in pixels from the edge (to the nearest pixel that ends the other way, up to 12). Counted: pixels whose end differs (and the farthest of
// them from the edge), the far direction's error (largest, and pixels over dirLimit, each 2 or more pixels from the edge; within 2 pixels reported apart),
// b's relative error, the disc's first image in [rIn, rOut]: another image (pixels; farthest from the edge, nearest to rIn or rOut relative), r's and g's
// relative errors where both have the same image, and the rays lost on the card. 0.56.2: discWrongOut, those 2 pixels or more from the edge whose radius is more than rLimit (relative) from rIn and rOut; u, uBad: every
// crossing (up to four) the reference makes at a radius in [rIn, rOut], its radius's relative error 2 pixels or more from the edge (over rLimit: uBad).
// PL_BH_SKYDUMP prints every pixel over a limit with its place.
struct SkyCompare {
    long px = 0, sky = 0, ends = 0, endsFar = 0, lost = 0, refLost = 0, dirBad = 0, discPx = 0, discWrong = 0, rBad = 0, discWrongOut = 0, uBad = 0;
    double endsFarPx = 0, dir = 0, dirNear = 0, dirBadFar = 0, b = 0, r = 0, g = 0, discWrongFar = 0, discWrongEdge = 1, u = 0, ms = 0;
    long timePx = 0, timeBad = 0, gasPx = 0, gasBad = 0;   // 0.72.0: the moving gas (uMode 5): the first image's phi and t; its factor and clump
    double phi = 0, t = 0, gas = 0;
};
SkyCompare skyCompare(const SkyCase& c, const std::vector<float>& probe, double dirLimit, double rLimit);
// 0.57.2: the same in two parts: the reference (no probe needed: any thread, made ahead) and the comparison with it (the same numbers; ms counts both)
struct SkyRef { std::vector<ClosedEnd> ref; std::vector<signed char> kind; std::vector<float> edge; double ms = 0; };
SkyRef skyReference(const SkyCase& c);
SkyCompare skyCompare(const SkyCase& c, const SkyRef& ref, const std::vector<float>& probe, double dirLimit, double rLimit);
std::string skyCompareText(const SkyCase& c, const SkyCompare& m);   // one line "case=... px=..." (the tests read it)

// The start-up check (and its limits, measured on Mesa's llvmpipe with room; REPORT 0.56.0): two cases (the benchmark's camera and the Film's), each a
// probe of kSkyCheckW x kSkyCheckH pixels; passes when no end differs farther than 1.5 pixels from the edge, no ray is lost, the far direction is within
// kSkyDirLimit 2 pixels or more from the edge, b within 1e-4, and the disc's radius within kSkyRLimit and g within kSkyRLimit where the image is the same.
constexpr int kSkyCheckW = 48, kSkyCheckH = 40;
constexpr double kSkyDirLimit = 2e-3, kSkyRLimit = 1e-3;
// 0.72.0, the moving gas (fixed before any card result): at the first disc image 2 pixels or more from the edge, phi within kSkyPhiLimit rad and t within
// kSkyTLimit of itself (the far direction's and the radius's limits) of traceClosed's (timed); the pattern's factor (relative) and the clump's n (absolute)
// within kSkyGasLimit of gas.cpp's at the card's own r, phi and t (single precision: about 1e-5 expected; the epoch's split of the time tested at kSkyGasT,
// 10^6 M of running: the card's tau below the epoch's 64 M (0.72-0.78: 1024 M), gas.h gasCard)
constexpr double kSkyPhiLimit = 2e-3, kSkyTLimit = 1e-3, kSkyGasLimit = 1e-3, kSkyGasT = 1e6 + 345.6;
const SkyCase& skyCheckCase(int k);   // k = 0, 1
// 0.58.0, the light check (both "Light" modes): the probe's rows h..2h-1 (Renderer2D::skyProbe, kKerrSkyFS uMode 11): the card's light of black bodies at
// w x h temperatures (10^3 to 10^7 K, even in log T; the card's own T in .a) from the card's colour table (cardColourTable), the eye's (columns 0..w-1) and
// All light (w..2w-1, uAll = 1 / T_sun as float), against the same lookup in double from the card's own T and the same float table and constants.
// Each pixel's limit is derived from the shader's operations before any card result (u = 2^-24, GLSL 4.60 / Vulkan: log within 3 ulp of its result outside
// [0.5, 2], +, -, *, / correctly rounded or within the 2.5 ulp of division, mix in 3 operations): the table's index c = (ln T log10 e - l0) / dl is off by at
// most dc = (3 ulp(ln T) log10 e + 4 u |log10 T| + 2 u |c dl|) / dl cells (the subtraction's and division's roundings), so each channel by at most
// D dc + 3 u (D: the largest step of the two table entries used, over the brightest channel); All light: also (T u_all)^4 s in 4 multiplications after
// one (8 u), the luminance's dot product (5 u), the division and the product (2 u), and the table's error twice (the colour and its luminance):
// 2 (D dc + 3 u) + 15 u. Doubled for room (the error counted is of the brightest channel; a pixel passes when every channel is within it).
struct SkyLight { long px = 0, bad = 0; double worst = 0; };   // pixels per mode (eye, all); over the limit; the largest error over its limit
const std::vector<float>& cardColourTable(int& n, double& log0, double& dlog);   // 512 colours, linear sRGB in the Sun's surface units, even in log10 T from kTmin to kTmax
void skyLightCompare(const std::vector<float>& probe, int w, int h, SkyLight out[2]);
// 0.70.0: the camera curve (tone.h) on the card against toneScaled in double: one colour (0.6, 0.9, 1.3) from 10^-3 to 10^4 times it at a burn-out white of
// kSkyToneLw (the probe's upper row, its third w columns). Limit, fixed before looking: 1e-5 of the screen's white in any channel (float: a few 2^-24).
constexpr double kSkyToneLw = 30, kSkyToneLimit = 1e-5;
SkyLight skyToneCompare(const std::vector<float>& probe, int w, int h);
struct SkyCheck { bool ok = false; std::string why; SkyCompare m[2]; SkyLight light[2]; SkyLight tone; double ms = 0; };
// refs (0.57.2): the two cases' references made ahead (skyReference), or null: made here
SkyCheck skyCheckOf(const std::vector<float>* probes[2], const SkyRef* refs = nullptr);

// tests (PL_BH_COMPARE=sky): the shader's elliptic functions against elliptic.cpp on a grid of arguments, the probe cases, the exposure (printed lines)
void skyCompareHook(render::Renderer2D& R);

}  // namespace blackhole
