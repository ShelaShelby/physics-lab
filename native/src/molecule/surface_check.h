// 0.68.0: the Molecule Lab's Density surface on the graphics card (Vulkan; render/surface_card.h, render/shaders.h kSurfaceCS and kSurfaceVS), made and
// checked step by step within its own guard (app/gpu_guard.h gpuVkSurfaceGuard, log lines "[vulkan] [surface] "), the pattern of the other card programs
// (orbit/lines_check.h): nothing before the surface is on screen for 3 frames; then a step a frame, logged and guarded: the pipelines, the check against
// the processor, the first full use.
//
// THE CHECK (the limits written before any card's result was looked at): three surfaces at the default isovalue (0.002 e/bohr^3) coloured by the potential,
// turned at a slant and drawn 90 points to the bohr about (640, 400): benzene (C6H6, the most triangles), water (H2O) and water's first vibrating mode at
// 0.7 of its swing (a phase's own surface). The processor's code (molecule/surface_draw.h, the code that draws it) against the card's steps read back:
//   - every vertex on the picture within 2e-6 x (s (r + |pan|) + |cx| + |cy|) points (r the surface's largest radius): the card's single precision
//     rounds the turn's entries, three products and sums, the pan, the scale and the centre (about 8 roundings of 2^-24 = 4.8e-7 of those sizes;
//     2e-6 leaves 4 times that);
//   - every vertex's colour within 1e-4 a channel: the light's d^24 (the card's pow is exp2(24 log2 d), within (3 + 2 |24 log2 d|) ulp by Vulkan's rules:
//     at most about 3.5e-6 of the highlight where it is not below 1e-5) and a few float roundings of the rest, far under 1e-4 (a 255-level step is 3.9e-3);
//   - the card's order a permutation of the triangles along which the processor's depths never fall by more than 2 x 6e-6 r (bohr): a vertex's depth is
//     off by at most 6 roundings of r (3.6e-7 r), a triangle's sum of three by 1.1e-6 r plus its own rounding; 6e-6 r leaves 5 times that, twice for the
//     two triangles compared. Triangles whose depths are closer than that may come in either order (the processor's own float sums decide them too).
// The drawing (each triangle's opacity by its winding, in the vertex shader) is checked by the pictures (check_molecule.py cardsurface).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "molecule/density.h"
#include "molecule/surface_draw.h"
#include "render/renderer2d.h"

namespace molecule {

struct SurfaceCase { std::string name; dens::Mesh mesh; double potRange = 1, R[9] = {}, px = 0, py = 0, cx = 640, cy = 400, s = 90, radius = 0; };
std::vector<SurfaceCase> surfaceCheckCases();
render::SurfaceView surfaceView(const double R[9], double px, double py, double cx, double cy, double s, bool pot, double potRange, render::Color plain, float aFront, float aBack);
render::SurfaceDraw surfaceDrawOf(const SurfaceCase& c);
struct SurfaceCompare {
    long vertices = 0, triangles = 0, posOver = 0, colourOver = 0, orderBad = 0;
    double posWorst = 0, colourWorst = 0, dropWorst = 0;   // the largest differences (points, a channel) and the processor's largest fall along the card's order (bohr)
    double posLimit = 0, dropLimit = 0;
    bool ok() const { return vertices > 0 && triangles > 0 && posOver == 0 && colourOver == 0 && orderBad == 0; }
};
SurfaceCompare compareSurface(const SurfaceCase& c, const std::vector<float>& verts, const std::vector<std::uint32_t>& order);
std::string surfaceCompareText(const SurfaceCompare& k);

// ---- 0.90.0: the surface made on the card (render/surface_card.h SurfaceMake, kSurfaceMakeCS) against the processor's dens::densitySurface ----
// THE CHECK (the limits derived before any card's result was looked at): the processor's surface of the blended density (dens::blendVib, then
// densitySurface at the same isovalue and stride) against the card's, edge by edge (the vertices' grid edges, dens::marchTets keys):
//   - the same vertices (the same crossed grid edges) and the same triangles (each a cycle of three edges, the same way round) in the same order;
//   - every vertex within its own limit, worked out from the processor's corner values c_lo, c_hi of its edge (ln rho) and t, its place along the edge:
//     each corner's ln rho may differ by e_c = 8 u M / rho (the card's blend in single precision: about 6 roundings of the terms, M = |f0| + |a| (|f+| +
//     |f-|) + |b| (|f+| + |f-| + 2 |f0|), and a and b rounded) + u (the processor's blend rounded to a float) + 3 ulp of ln rho (Vulkan's log outside
//     [0.5, 2]; 2^-21 inside) + 1 ulp (the processor's logf), u = 2^-24. Then t may differ by ((1 - t) e_lo + t e_hi + e_iso) / |c_hi - c_lo| + 3 u t,
//     a coordinate by H dt (H the coarse step) plus 4 u (|o| + H (i + 1)) for the card's sums, u |x| for each side's float, and the angstrom factor's
//     rounding (2 u |x| more);
//   - a corner within its e_c of the isovalue could be called inside on one side and outside on the other: counted and reported ("near corners").
struct MadeCase {
    std::string name, id;
    int mode = -1;
    double phase = 0;
    dens::Data at;                                   // the processor's blended density (the still one when not vibrating)
    dens::Mesh mesh;
    std::vector<long long> keys;
    render::SurfaceMake make;
};
// full: water's first mode with a density at the phases -1, -0.35, 0.4, 1, and carbon dioxide and nitrogen (no vibrating density in the app: the still
// surface, made by the card's blend with all three sources the still density); not full (the start-up check): water at -1 and 0.7
std::vector<MadeCase> surfaceMadeCases(bool full);
// the card's sources and phase for the density d, vibrating in mode vm at phase s (vm null: the still density), the isovalue iso (e/bohr^3)
render::SurfaceMake surfaceMakeOf(const dens::Data& d, const dens::VibDensityMode* vm, double s, double iso, std::uint64_t key);
struct MadeCompare {
    long vertices = 0, triangles = 0, cardVertices = 0, cardTriangles = 0, missing = 0, extra = 0, outOfOrder = 0, posOver = 0, nearCorners = 0;
    double posWorst = 0, limitWorst = 0, limitLeast = 1e300;   // angstrom: the largest difference and the largest and smallest limits
    bool ok() const { return vertices > 0 && triangles > 0 && cardVertices == vertices && cardTriangles == triangles && missing == 0 && extra == 0 && outOfOrder == 0 && posOver == 0; }
};
MadeCompare compareMade(const MadeCase& c, const render::SurfaceMade& g);
std::string madeCompareText(const MadeCompare& k);

// once per frame where the surface is drawn: true when the card may draw it this frame (false: the caller's triangles)
bool surfaceOnCard(render::Renderer2D& R);
int surfaceCardState();                              // 1 the card (its check passed), 0 not decided yet, -1 the processor
bool surfaceMakeOnCard(render::Renderer2D& R);       // 0.90.0: after surfaceOnCard said yes: the vibrating surface may be made on the card
std::string surfaceCardPathText(bool vulkan);

void surfaceFrameEnd(render::Renderer2D& R);         // 0.68.0: once a frame (main.cpp), after the labs drew

}  // namespace molecule
