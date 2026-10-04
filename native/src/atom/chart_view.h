// The chart of nuclides as a landscape (0.11 stage 3): each tile gets a HEIGHT from its half-life, the chart is drawn in a fixed isometric projection (back to front, no free camera), and the
// user can tilt it between the flat chart of stage 2 and the full landscape, zoom and pan. Everything here is plain logic (the height mapping, the projection, the painter's order, the hit test,
// the zoom and pan limits) so it can be tested without a screen; the drawing is in chart_draw.cpp.
#pragma once

#include <vector>

#include "chart.h"

namespace atom {

// ---- the height of a tile: a CHOSEN MAPPING, not a measurement ----
// Height = (log10(half-life in seconds) - kHeightLog10Lo) / (kHeightLog10Hi - kHeightLog10Lo), cut to 0 .. 1, in "scale heights". The base is 10 (each step up is a factor of ten in half-life) and the
// scale runs from 10^-23 s to 10^32 s: its ends are the shortest (8.6e-23 s, log10 = -22.07) and the longest (7.1e31 s, log10 = 31.85) half-life in the lab's table, rounded outwards to a whole power
// of ten, so the whole table fits and nothing is cut. A scale height is drawn kHeightCells tile widths tall (before the tilt foreshortens it); that number was chosen by looking, so that the tallest
// block is clearly a ridge and the shortest still show. Nothing else is behind these numbers: a different base or range would draw the same data differently. The legend says all of this.
inline constexpr double kHeightLog10Lo = -23, kHeightLog10Hi = 32, kHeightCells = 12;
// A stable nuclide has no half-life, so it has no place on the scale: all stable nuclides stand at ONE fixed height above the top of the scale (kStableHeight scale heights), drawn as "no decay seen",
// not as a very long number. A longer half-life than any in the table does not exist, so a nuclide with a huge half-life (up to 7.1e31 s) is on the scale, below that plateau.
inline constexpr double kStableHeight = 1.1;
double chartHeightFromHalfLife(double seconds);          // the mapping above: 1e-23 s -> 0, 1e32 s -> 1, 1 s -> 23/55; cut to 0 .. 1

// What a tile's height means, for the six cases the data has (counts in REPORT.md):
//  Measured      a half-life value: a solid block at the height of the value.
//  Stable        the lab's table says stable (no decay seen): a solid block at the plateau.
//  StableNoted   the same, and NUBASE2020 lists a half-life limit and/or decay entries marked "?" for it: the plateau and a small mark on top.
//  Approx        an approximate value ("~" in the lab's table or NUBASE): a pale block at the height of the value.
//  Limit         only a limit (> or <): a pale block at the height of the limit.
//  Extrapolated  NUBASE2020 has only an extrapolation from systematics: NO height is invented; a flat square with a pale fill and an outline.
//  Unknown       no half-life at all: NO height; a flat, hollow square (outline only).
enum class HeightKind : unsigned char { Measured, Stable, StableNoted, Approx, Limit, Extrapolated, Unknown };
struct ChartHeight { double h; HeightKind kind; };       // h in scale heights (0 .. kStableHeight)
ChartHeight chartHeightOf(int Z, int N);                 // for a tile of the chart (built once)
int chartHeightCount(HeightKind k);                      // tiles of that kind
const char* chartHeightWords();                          // the sentence(s) for the legend
const char* chartHeightShort();

// ---- the view: tilt, zoom, pan ----
struct ChartViewState {
    double tilt = 0;                                     // 0 = the flat chart of stage 2, 1 = the full landscape
    double zoom = 1;                                     // 1 = the whole chart fitted into the picture
    double panX = 0, panY = 0;                           // points, only while zoom > 1
    bool flat() const { return tilt == 0 && zoom == 1 && panX == 0 && panY == 0; }   // exactly stage 2's picture
    bool operator==(const ChartViewState& o) const { return tilt == o.tilt && zoom == o.zoom && panX == o.panX && panY == o.panY; }
};
inline constexpr double kChartZoomMax = 16;
// The fixed isometric angles: at tilt 1 the chart is turned about the vertical axis by 45 degrees and then tipped away from straight-down by 54.736 degrees (= arccos(1/sqrt(3)), so the camera looks
// along a cube's diagonal, 35.264 degrees above the horizon): then the three axes (neutrons, protons, height) all look 0.8165 times their length, equally long. At tilt t both angles are t times
// that. At tilt 0 nothing is turned and nothing is tipped, so the chart is exactly the flat one.
inline constexpr double kChartYawDeg = 45, kChartPitchDeg = 54.735610317245346;

// World: a (across, = the flat layout's x, neutrons), b (down the flat layout, = the flat layout's y, protons going down), z (up, points). The projection (before fitting and zooming):
//   u = a - cx, v = b - cy (relative to the grid's centre), x = u cos(yaw) - v sin(yaw), y = u sin(yaw) + v cos(yaw);
//   screen x = x, screen y = y cos(pitch) - z sin(pitch), depth = y sin(pitch) + z cos(pitch) (larger = nearer to the viewer).
// Fitting scales and centres it so the whole landscape is in the picture at zoom 1; zoom is about the picture's centre; pan moves it.
struct ChartProjection {
    bool identity = true;                                // tilt 0, zoom 1, no pan: the map is the identity (so the flat chart is bit for bit stage 2's)
    bool flat = true;                                    // tilt 0
    double cosY = 1, sinY = 0, cosP = 1, sinP = 0;
    double cx = 0, cy = 0;                               // the grid's centre in the flat layout
    double K = 1, fx = 0, fy = 0;                        // final = F + K (raw - C)
    void map(double a, double b, double z, double& sx, double& sy, double& depth) const;
    void unmap(double sx, double sy, double depth, double& a, double& b, double& z) const;   // the inverse: the world point that is at this screen point and this depth
};

// A tile as a block: the top face and the two faces that face the viewer (the one at the larger b and the one at the larger a), as screen quadrilaterals (x, y of 4 corners each).
struct ChartBlock {
    short Z, N;
    ChartClass cls;
    HeightKind kind;
    double h;                                            // scale heights
    float top[8], faceB[8], faceA[8];
    float x0, y0, x1, y1;                                // screen bounding box of all three
    bool sides;                                          // the two side faces have area
};

class ChartScene {
public:
    // Builds the blocks for a picture w x h points: only when something changed (the picture's size or the view); the order is fixed (see order()), so nothing is sorted per frame.
    void build(double w, double h, const ChartViewState& st);
    const std::vector<ChartBlock>& blocks() const { return blocks_; }       // PAINTER'S ORDER: far to near, draw first to last
    const ChartProjection& proj() const { return proj_; }
    const ChartLayout& layout() const { return L_; }
    double heightPoints(double hScale) const { return hScale * kHeightCells * L_.cell; }   // in flat-layout points (the projection scales it)
    // the tile whose block is drawn on top at this point (the last one drawn that covers it), or false: the rule for hover and click
    bool pick(double px, double py, int& Z, int& N) const;
    // room: how tall a box can be, from the top margin or up from the bottom margin, across x0..x1 before it would cover a block (a conservative measure from the blocks' bounding boxes)
    double freeHeight(bool fromTop, double x0, double x1) const;
    // the screen bounding box of everything (blocks) and the pan that keeps enough of it in the picture
    void bbox(double& x0, double& y0, double& x1, double& y1) const { x0 = bx0_; y0 = by0_; x1 = bx1_; y1 = by1_; }
    ChartViewState clamped(const ChartViewState& st);                       // zoom 1 .. 16; pan 0 at zoom 1; else at least a strip of the chart stays in the picture
    double w() const { return w_; }
    double h() const { return h_; }
private:
    double w_ = -1, h_ = -1;
    ChartViewState st_;
    ChartLayout L_;
    ChartProjection proj_;
    std::vector<ChartBlock> blocks_;
    std::vector<float> colTop_, colBottom_;              // per picture column: the lowest top edge and the highest bottom edge of any block there
    double bx0_ = 0, by0_ = 0, bx1_ = 0, by1_ = 0;
};
// The order the blocks are drawn in, as indices into chartTiles(): by N + (zMax - Z), then N. The viewer is on the side of large N and small Z, and a block can only be hidden by blocks with N
// and (zMax - Z) both at least as large, so any order that respects that (this one does) draws a block after everything that could hide it, at every tilt from just above 0 to 1.
const std::vector<int>& chartPaintOrder();

}  // namespace atom
