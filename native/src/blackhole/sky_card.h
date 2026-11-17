// Black Hole Lab (0.56.0): the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms in single precision (render/kerr_sky.h,
// render/shaders.h kKerrSkyFS; the formulas: kerr_closed.h). Here: the camera's numbers for the pass (made in double), the probe pictures (the shader's own
// numbers per pixel) and their comparison with kerr_closed.h's traceClosed in double, used by the start-up check on the user's card and by the tests.
#pragma once

#include <string>
#include <vector>

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
const SkyCase& skyCheckCase(int k);   // k = 0, 1
struct SkyCheck { bool ok = false; std::string why; SkyCompare m[2]; double ms = 0; };
// refs (0.57.2): the two cases' references made ahead (skyReference), or null: made here
SkyCheck skyCheckOf(const std::vector<float>* probes[2], const SkyRef* refs = nullptr);

// tests (PL_BH_COMPARE=sky): the shader's elliptic functions against elliptic.cpp on a grid of arguments, the probe cases, the exposure (printed lines)
void skyCompareHook(render::Renderer2D& R);

}  // namespace blackhole
