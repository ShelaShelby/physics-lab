// 0.68.0: the Orbit Lab's lines on the graphics card (Vulkan; render/lines_card.h, render/shaders.h kLinesCS and kLinesVS), made and checked step by
// step within its own guard (app/gpu_guard.h gpuVkLinesGuard, log lines "[vulkan] [lines] "), the pattern of the other card programs (atom/nucdens_check.h):
// nothing before the Orbit Lab's lines are on screen for 3 frames; then a step a frame, logged and guarded: the pipelines (the card's steps and the
// drawing program), the check against the processor, the first full use. OpenGL draws them on the processor always (no card program for them there).
//
// THE CHECK (the limits written before any card's result was looked at): one scene of four orbits (1, 1.52, 5.2 and 39.5 AU; e up to 0.25, tilted up to
// 17 degrees), a trail of 150 points on each and six distance rings, seen three ways: the heavy view's camera (3D, zoom x1, 1280 x 800), the closest
// zoom in 2D following the 1 AU body (a point is about 4 km there) and a 3D close-up following the 5.2 AU body. The processor's lines (orbit/orbit_lines.h,
// the code that draws them) give each pen's points; the card's steps give each run's points. A case passes when every run has the processor's number of
// points (the same pieces made into curves, in the same number of steps) and every point is within
//     0.01 point + 4e-6 x px f |X - C| + px f S
// of the processor's: px f the scale at that point (points per metre), X its place in space, C the view's reference point, S the size of the run's
// curve steps: 0 for a ring, 1e-5 x the run's longest piece for a trail, 1e-6 x dE (|P a| + |Q b|) for an orbit. From single precision: a point less C
// is exact to 2^-24 of the difference (the high and low floats), its turn and perspective add a few roundings of the same size (about 8 x 2^-24 =
// 5e-7 of px f |X - C|; 4e-6 leaves 8 times that). A trail's curve step is worked out from its piece's first point a with its neighbours as exact
// differences d0, d1, d3 (up to twice the piece, L): the cubic's terms reach 0.5 (2 + 7 + 5) L = 7 L, each through about 8 roundings, about 4.2e-6
// of L at worst (5e-6 of 2 L x the longest piece is used). An orbit's curve step is P a (cos E - cos E0) + Q b (sin E - sin E0) from the corner
// before it, its float sines and cosines off by about 4 x 2^-24 of the step's size (at most dE (|P a| + |Q b|)): 2.4e-7, 1e-6 leaves 4 times that.
// 0.01 point covers the processor's own double-precision sums.
// HOW THIS LIMIT WAS MADE (said plainly, 0.68.1): the FIRST limit of 0.68.0 left out the curve steps' own size (the S term) and FAILED the closest zoom's
// case (up to 0.05 point at 10,000 points off the picture); it was derived AGAIN, after looking at that result, and the derivation above is the second one. It was
// not tuned to the measured differences; 0.68.1 re-checked the cubic's terms in float without them (native/REPORT.md).
// The drawing itself (each step clipped and widened as Renderer2D::line, in the vertex shader) is checked by the pictures (check_orbit_ui.py cardlines).
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "orbit/orbit_lines.h"
#include "orbit/solar_system.h"
#include "render/renderer2d.h"

namespace orbit {

struct LinesCase {
    const char* name = "";
    LineCam cam;
    std::vector<Body> bodies;                        // the orbits (orbitPath of each)
    std::vector<std::deque<Vec3>> trails;            // a trail for each body, ending at its place (pos)
    std::vector<double> rings;                       // distance rings, m
};
std::vector<LinesCase> linesCheckCases();
// the processor's pens (drawn with rec: each pen's points, 5 numbers a point) and the card's draw of the same lines (its own points: LinesDraw::checkPts)
void linesCpu(const LinesCase& c, std::vector<std::vector<double>>& pens);
render::LinesDraw linesDrawOf(const LinesCase& c);
struct LinesCompare {
    long runs = 0, points = 0, over = 0, runsBad = 0;
    double worst = 0;                                // the largest difference over its limit (1 = at the limit)
    double worstPt = 0;                              // the largest difference, points
    bool ok() const { return runs > 0 && points > 0 && over == 0 && runsBad == 0; }
};
LinesCompare compareLines(const LinesCase& c, const render::LinesDraw& d, const std::vector<std::vector<double>>& pens, const std::vector<float>& ends,
                          const std::vector<std::uint32_t>& offsets, const std::vector<float>& points);
std::string linesCompareText(const LinesCompare& k);

// once per frame where the Orbit Lab's lines are drawn: true when the card may draw them this frame (false: the caller's LinePen)
bool linesOnCard(render::Renderer2D& R);
int linesCardState();                                // 1 the card (its check passed), 0 not decided yet, -1 the processor
// its steps are over (the card's with its first full use, or the processor's): the Orbit Lab may keep its picture at rest (main.cpp OrbitKeep). Until then
// the picture is drawn every frame, so the steps (which run while it is drawn) go on and the kept picture is the one the lines end with
bool linesCardSettled(bool vulkan);
std::string linesCardPathText(bool vulkan);

void linesFrameEnd(render::Renderer2D& R);         // 0.68.0: once a frame (main.cpp), after the labs drew

}  // namespace orbit
