// 0.64.0: the Classic Cloud 3D dots on the graphics card (Vulkan; render/classic_card.h, render/shaders.h kClassicCS), made and checked step by step within
// their own guard (app/gpu_guard.h gpuVkClassicGuard), the pattern of the Atom Lab's other card programs (atom/atom_card_paths.h): nothing before the
// Classic Cloud 3D is on screen for 3 frames; then a step a frame, logged and guarded: the pipeline, the check against the processor, the first full use.
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
// 0.65.0: the side view with every electron (cloud_dots.cpp sideDots: x across, z up): Classic squares of half size 1 and opacity `weight`, or (glow) the Glow's
// soft dots of radius 4.4 (their light from the exposure, ClassicDraw::exposure); dc: the colours as drawn (the Glow's darkened on a light page)
render::ClassicDraw sideDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, double cx, double cy, double pxm, double w, double h,
                             const DotColours& dc, double weight, bool glow);
// 0.65.0: the Slice (atom_view.cpp's Slice loop): Classic squares of half size 1.25 at X + 0.25 of opacity `weight`, or (glow) soft dots of radius 4.2;
// hz: a horizontal cut; off, th: the cut's offset and the slab's half thickness (m)
render::ClassicDraw sliceDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, double cx, double cy, double pxm, double w, double h,
                              const DotColours& dc, double weight, bool glow, bool hz, double off, double th);
// the Glow grid from the card's counts per cell: glowBlobs' float sum of each dot's footprint (weight 1, radius r: 4.4 the side view, 4.2 the Slice), added count times
void glowGridOfCounts(const std::vector<float>& counts, float r, std::vector<float>& grid);
// the side view's Glow grid from the card's counts per cell: glowBlobs' float sum of each dot's footprint (weight 1, radius 4.4), added count times
void sideGlowGrid(const std::vector<float>& counts, std::vector<float>& grid);
inline render::ClassicDraw withGroupsEpoch(render::ClassicDraw d, std::uint64_t grpEpoch) { d.grpEpoch = grpEpoch; return d; }   // 0.65.0
// the check's cloud and its comparison (the start-up check and the tests)
struct ClassicCase { std::vector<float> pts; std::vector<unsigned char> grp; CloudCamera cam; double w, h; DotColours dc; double weight;
                    bool side = false, soft = false;                         // 0.65.0: the side view (Classic squares, or the Glow's soft dots and cells)
                    int slice = 0; double off = 0, th = 0; };                // 0.65.0: the Slice (1 vertical, 2 horizontal cut), its cut and slab (m)
ClassicCase classicCheckCase();
ClassicCase classicSideCase(bool glow);                                     // 0.65.0: the same cloud seen from the side, part of it beyond the picture
ClassicCase classicSliceCase(bool glow, bool hz);                           // 0.65.0: the same cloud as a Slice's dots (some off the slab, some off the picture)
render::ClassicDraw classicCaseDraw(const ClassicCase& c);                  // what the card is given for a case
struct ClassicCompare { long dots = 0, shown = 0, groupBad = 0, bandFlip = 0, bandBad = 0, placeBad = 0, orderBad = 0, squareBad = 0; double worstPlace = 0;
                        long cells = 0, cellBad = 0, cellEdge = 0;   // 0.65.0: the side view's Glow: cells, beyond their limit, differing by a dot at a cell's edge
                        bool ok() const { return dots > 0 && shown > 0 && groupBad == 0 && bandBad == 0 && placeBad == 0 && orderBad == 0 && squareBad == 0 && cellBad == 0; } };
ClassicCompare compareClassic(const ClassicCase& c, const std::vector<std::uint32_t>& keys, const std::vector<float>& squares, long total,
                              const std::vector<std::uint32_t>& cells = std::vector<std::uint32_t>());
std::string classicCompareText(const ClassicCompare& c);

// once per frame where the Classic Cloud 3D is drawn: true when the card drew this frame's dots (false: the caller makes the processor's squares)
bool drawClassicOnCard(render::Renderer2D& R, const render::ClassicDraw& d, bool mayStart = true);   // (0.65.0: mayStart false: its start-up waits)
int classicCardState();
std::string classicCardPathText(bool vulkan, const std::string& what = "the Classic dots");   // (0.65.0: what: "the side view's dots" too)

}  // namespace atom
