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

// once per frame where the surface is drawn: true when the card may draw it this frame (false: the caller's triangles)
bool surfaceOnCard(render::Renderer2D& R);
int surfaceCardState();                              // 1 the card (its check passed), 0 not decided yet, -1 the processor
std::string surfaceCardPathText(bool vulkan);

void surfaceFrameEnd(render::Renderer2D& R);         // 0.68.0: once a frame (main.cpp), after the labs drew

}  // namespace molecule
