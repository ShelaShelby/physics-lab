// 0.64.0: the Classic Cloud 3D dots on the graphics card (Vulkan; render/classic_card.h, render/shaders.h kClassicCS), made and checked step by step within
// their own guard (app/gpu_guard.h gpuVkClassicGuard), the pattern of the Atom Lab's other card programs (atom/atom_card_paths.h): nothing before the
// Classic Cloud 3D is on screen for 3 frames; then a step a frame, logged and guarded: the pipeline, the check against the processor, the first full use.
// OpenGL makes the processor's squares always (no card program for them there).
//
// THE CHECK (limits as the Glow dots' and the Slice's, written before any card's result was looked at): a cloud of 6,000 dots in 6 groups (a fixed
// pattern) seen by a turned camera, the card's steps read back against atom/cloud_dots.cpp projectCloud + buildClassic3D. Per dot: the same group; the
// same band, except where the processor's depth lies within 2e-5 of a band's edge (single precision); its place within 2e-3 points. The squares: in the
// processor's order of the keys (band, then group), every key's squares the card's own dots of that key (their places exactly), size and opacity exactly
// as the processor's (opacity within 2^-22 relative), nothing after the last.
#pragma once

#include <string>
#include <vector>

#include "atom/cloud_dots.h"
#include "render/renderer2d.h"

namespace atom {

// what the card is given (the processor's buildClassic3D takes the same numbers)
render::ClassicDraw classicDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, const CloudCamera& cam, double w, double h,
                                const DotColours& dc, double weight);
// the check's cloud and its comparison (the start-up check and the tests)
struct ClassicCase { std::vector<float> pts; std::vector<unsigned char> grp; CloudCamera cam; double w, h; DotColours dc; double weight; };
ClassicCase classicCheckCase();
struct ClassicCompare { long dots = 0, shown = 0, groupBad = 0, bandFlip = 0, bandBad = 0, placeBad = 0, orderBad = 0, squareBad = 0; double worstPlace = 0;
                        bool ok() const { return dots > 0 && shown > 0 && groupBad == 0 && bandBad == 0 && placeBad == 0 && orderBad == 0 && squareBad == 0; } };
ClassicCompare compareClassic(const ClassicCase& c, const std::vector<std::uint32_t>& keys, const std::vector<float>& squares, long total);
std::string classicCompareText(const ClassicCompare& c);

// once per frame where the Classic Cloud 3D is drawn: true when the card drew this frame's dots (false: the caller makes the processor's squares)
bool drawClassicOnCard(render::Renderer2D& R, const render::ClassicDraw& d);
int classicCardState();
std::string classicCardPathText(bool vulkan);

}  // namespace atom
