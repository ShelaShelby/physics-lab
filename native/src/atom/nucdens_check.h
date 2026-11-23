// 0.67.0: the nucleus' Density look on the graphics card (Vulkan; render/nucdens_card.h, render/shaders.h kNucDensCS), made and checked step by step within
// its own guard (app/gpu_guard.h gpuVkNucDensGuard, log lines "[vulkan] [nucdensity] "), the pattern of the Atom Lab's other card programs
// (atom/classic_check.h): nothing before the Density look is on screen for 3 frames; then a step a frame, logged and guarded: the pipeline, the check
// against the processor, the first full use.
//
// THE CHECK (the limit written before any card's result was looked at): three nuclei, each on a grid of 128 x 128 cells through the card's first two steps,
// read back, against atom_core.cpp projectChargeDensity (double precision): U-238 (deformed: the rays through the table) seen at a slant, Pb-208 (round:
// the radial tables) and the deuteron in spin state M = 0 (its L = 2 part: kk != 0). A nucleus passes when every cell differs from the processor's by at
// most 1e-4 of the brightest cell. The limit from single precision: a ray's sum of up to about 100 table values, each read at an index rounded to 2^-24
// of its size (rp^2 up to the rays' reach squared, about 150 fm^2, against table steps of about 0.2 fm^2: an index off by up to about 4e-5 of a step,
// so a value off by up to 4e-5 of the largest table value) and summed with a rounding of up to 100 x 2^-24 = 6e-6: a few 1e-5 of the brightest cell;
// the drawing turns a cell into 255 levels (4e-3 a level), so 1e-4 is 40 times below a level.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "render/renderer2d.h"

namespace atom {

// what the card is given for the grid of projectChargeDensity(d, e1, e2, e3, G, cellFm) (its table into `table`, which must outlive the draw), placed on the
// picture with its top left at (x0, y0) points in cells of cellPts, in colour col (rgba). The table is filled only when empty: the caller empties it for a
// new model (and gives a new version)
render::NucDensDraw nucDensDraw(const NucleusChargeDensity& d, const double e1[3], const double e2[3], const double e3[3], int G, double cellFm, std::vector<float>& table,
                                std::uint64_t version, double x0, double y0, double cellPts, const float col[4]);
// the check's cases and their comparison (the start-up check and the tests)
struct NucDensCase { const char* name; NucleusChargeDensity d; double e1[3], e2[3], e3[3]; int G; double cellFm; };
std::vector<NucDensCase> nucDensCheckCases();
struct NucDensCompare { long cells = 0, over = 0; double worst = 0, peak = 0; bool ok() const { return cells > 0 && peak > 0 && over == 0; } };
NucDensCompare compareNucDens(const std::vector<float>& cpu, const std::vector<float>& card);
constexpr double kNucDensLimit = 1e-4;                       // of the brightest cell (see above)

// once per frame where the Density look is drawn: true when the card drew it (false: the caller makes the processor's squares)
bool drawNucDensOnCard(render::Renderer2D& R, const render::NucDensDraw& d);
int nucDensCardState();                                      // 1 the card (its check passed), 0 not decided yet, -1 the processor
std::string nucDensCardPathText(bool vulkan);

}  // namespace atom
