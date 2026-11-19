// 0.64.0: Saturn's rings on the graphics card (Vulkan; render/ring_card.h, render/shaders.h kRingsFS), made and checked step by step within their own guard
// (app/gpu_guard.h gpuVkRingsGuard), the pattern of the Atom Lab's card programs (atom/atom_card_paths.h): nothing before Saturn's rings are on screen for 3
// frames; then a step a frame, each logged with its time and guarded by a marker: the pipelines (the drawing's and the probe's), the check against the
// processor (it must pass, or the processor's triangles are kept and Settings, Performance says why), the first full use (3 frames drawn by the card).
// A step that does not finish within 10 s switches it off. OpenGL draws the processor's triangles always (no card program for the rings there).
//
// THE CHECK (limits written before any card's result was looked at): three views (the orbit.saturn set-up's kind of view, the rings almost edge-on, the
// planet's shadow across the rings), each a 400 x 250 picture of one sample per pixel (multisampling off): the processor's triangles (orbit/ring_draw.h,
// rasterised here at the pixel centres and blended over black, back half then front half) against the card's function at the same pixel centres (the
// probe), 8-bit values. A view passes when at most 1 % of its ring pixels (a pixel either side covers) differ by more than 8 levels in a channel.
#pragma once

#include <string>
#include <vector>

#include "orbit/ring_draw.h"
#include "render/renderer2d.h"

namespace orbit {

constexpr double kRingTint[3] = {0.91, 0.83, 0.66};      // 0.50 (review item 7): ONE drawn tint for every ring system, not measured (orbit_view.cpp)

// Saturn's drawn bands from Cassini's profile (orbit/saturn_profile.h) for a planet of r points: bins of at least a pixel (and at most 700 over the profile),
// opacity and light at mu (the viewer's) and mu0 (the Sun's), the face you see lit or not; kept while the four inputs stay the same
const std::vector<RingBand>& saturnDrawnBands(double eqKm, double r, double mu, double mu0, bool sunlitFace);

// the processor's picture of one ring system over black (both halves), w x h pixels of one sample, RGB 0..1 (3 floats a pixel, rows from the top); and
// the card's from its probe's answers (4 floats a pixel for the back half, then the front half). Shared by the start-up check and the tests.
void ringPictureCpu(const RingFrame& f, const std::vector<RingBand>& bands, int w, int h, std::vector<float>& rgb);
void ringPictureCard(const std::vector<float>& back, const std::vector<float>& front, int w, int h, std::vector<float>& rgb);
struct RingCompare { long ringPixels = 0, over8 = 0; int worst = 0; bool ok() const { return ringPixels > 0 && over8 * 100 <= ringPixels; } };
RingCompare compareRingPictures(const std::vector<float>& cpu, const std::vector<float>& card, int w, int h);
// the check's three views (a picture of w x h points)
struct RingCheckView { const char* name; RingFrame f; std::vector<RingBand> bands; };
std::vector<RingCheckView> ringCheckViews(int w, int h);
// what the card is given for one half
render::RingsDraw ringsDraw(const RingFrame& f, const std::vector<RingBand>& bands, bool back, double W, double H);

// once per frame where Saturn's rings are drawn (both halves call it; it moves on once a frame): true when this half was given to the card
bool drawRingsOnCard(render::Renderer2D& R, const RingFrame& f, const std::vector<RingBand>& bands, bool back, double W, double H);
// 1 the card (its check passed), 0 not decided yet, -1 the processor; one line for the benchmark and Settings
int ringCardState();
std::string ringCardPathText(bool vulkan);

}  // namespace orbit
