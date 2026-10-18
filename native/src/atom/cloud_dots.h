// How the electron cloud's dots are put together for drawing (no graphics calls in here, so it can be tested).
//
// 0.13 stage 1, phase colours on the GPU. Up to 0.12 the Classic cloud was drawn colour by colour: for each of the three depth bands and each of the 24 Wheel
// colours (2 for Sign) the CPU went over ALL dots and picked the ones of that band and colour (72 passes over 48,000 dots), and every dot carried its colour.
// Now (GPU colours On, the default) every dot carries only its colour INDEX (the bin of its phase, 0..23, or 0 / 1 for Sign) and the shader picks the colour
// (render/shaders.h, a port of atom::phaseRgb in phase_wheel.h). The CPU puts the dots in the SAME order as before with one counting sort (two passes), so
// the picture is the same: the draw order matters because the dots are alpha-blended. The old way is kept, behind lab.gpuColours = false, as the reference
// the new one is tested against and for the owner to compare on a real PC.
//
// The colour index itself is still worked out on the CPU, with phaseBin() (phase_wheel.h), in both ways: a phase in a 32-bit float on the GPU could fall in the
// next bin for a phase within about 1e-7 of a bin edge, and a jump or a superposition already works its indices out on the CPU.
#pragma once

#include <vector>

#include "atom_sim.h"
#include "phase_wheel.h"
#include "render/group_compute.h"
#include "render/renderer2d.h"

namespace atom {

constexpr unsigned char kHiddenGroup = 255;        // a dot that is not shown now (a superposition's thinning, a jump's odds)
constexpr unsigned char kNoBand = 255;             // a dot that is not drawn (off the screen, or no depth)

// ---- the colours ----
// The colour groups of a cloud: Wheel: kPhaseBins colours (phaseRgb of the middle of each bin); Sign: 2 (the + and the − colour).
struct DotColours {
    bool gpu = true;                               // true: a dot carries its colour index; false: it carries its colour (the v0.12 way)
    bool wheel = false, light = false;             // Wheel or Sign; a light page (the Wheel has its own light-page colours)
    int n = 2;                                     // the number of groups
    render::Color c[kPhaseBins];                   // each group's colour. The CPU path puts these in the dots; the GPU path takes only the alpha from here
    render::Renderer2D::PhasePalette palette() const;   // what the shader needs to pick the colour of a group
};
DotColours makeDotColours(bool gpu, bool wheel, bool light, render::Color plus, render::Color minus);

// One round (kind 4) or square (kind 2) dot of colour group g, with the light `alpha` (clamped to 0..1 and multiplied by the group colour's own alpha).
render::Renderer2D::Blob squareDot(const DotColours& dc, double x, double y, double half, int g, double alpha);
render::Renderer2D::Blob softDot(const DotColours& dc, double x, double y, double r, int g, double alpha);

// The colour group of every plain dot (no jump, no superposition): Wheel: the bin of its phase; Sign: 0 where the wave is + and 1 where it is −.
void plainGroups(bool wheel, const float* ph, const signed char* sg, long cnt, unsigned char* grp);

// Every dot's colour group for this frame (cnt dots: a jump in flight: both populations, 2 jn; a superposition; or a plain cloud). The jump and the superposition come from the GPU (0.13
// stage 2, render/group_compute.h) when the lab's gpuCompute switch is on and `gc` is a working OpenGL 4.3 compute object; otherwise, and always when gc is null (an OpenGL 3.3 PC),
// from the CPU functions AtomLab::jumpGroups / superGroup, which are the reference. Returns which one did the work (so a test and the Performance card can say).
enum class GroupsBy { Cpu, Gpu };
GroupsBy fillGroups(AtomLab& L, render::IGroupCompute* gc, long cnt, unsigned char* grp);

// ---- Classic Cloud 3D: three depth bands, far = faint and small, near = bright and larger ----
constexpr int kDepthBands = 3;
struct DepthBand { double alpha, size; };
constexpr DepthBand kDepthBand[kDepthBands] = {{0.15, 1.6}, {0.3, 2}, {0.55, 2.4}};   // as in 0.12
// The band of a dot from its depth v (the dot's depth over the cloud's radius: > 0 farther away): 0 far (v > 0.35), 1 middle, 2 near (v < -0.35); kNoBand for NaN (off screen).
inline unsigned char depthBandOf(float v) {
    if (v > 0.35f) return 0;
    if (v <= 0.35f && v >= -0.35f) return 1;
    if (v < -0.35f) return 2;
    return kNoBand;
}

// The cloud on the picture: where its middle is, points per metre and the camera turn.
struct CloudCamera { double cx, cy, pxm, ca, sa, cb, sb, ext; };   // ext: the cloud's radius, m (it sets the depth scale and the mild perspective)
// Puts every dot on the picture: PX, PY in points, PD the depth over ext (NaN when the dot is off the picture w × h). Fills the three vectors.
void projectCloud(const float* pts, long cnt, const CloudCamera& cam, double w, double h, std::vector<float>& PX, std::vector<float>& PY, std::vector<float>& PD);

// The order the Classic dots are drawn in: band by band, inside a band group by group, inside a group in dot order (dots with band == kNoBand or a group >= nGroups are left out).
// The same list in two ways, which must agree: the v0.12 scan (a pass over all dots for each band and group) and a counting sort (two passes).
void drawOrderByScan(const unsigned char* band, const unsigned char* grp, long cnt, int nBands, int nGroups, std::vector<int>& out);
void drawOrderByCounting(const unsigned char* band, const unsigned char* grp, long cnt, int nBands, int nGroups, std::vector<int>& out);

// The Classic Cloud 3D dots, in the order above, one blob each (one draw call for the lot). weight = cloudDotWeight(cap).
// dc.gpu picks the way: false: the v0.12 scan, true: the counting sort (and blobs that carry the colour index).
void buildClassic3D(const float* PX, const float* PY, const float* PD, const unsigned char* grp, long cnt, const DotColours& dc, double weight, std::vector<render::Renderer2D::Blob>& out);

// ---- Glow (the app's default cloud style): soft round dots. Dark pages: the light adds up (additive), so dense places glow and thin ones fade. Light pages: ordinary
// alpha blending with darker, more saturated colours (opacity builds up as 1 - exp(-light)). `base` is the brightest a single dot may be.
// The exposure is chosen from the brightest 8 x 8 pixel place so it stays just under white; it is one number for all dots, smoothed over frames in `expo`.
// 0.21: moved here from atom_view.cpp (unchanged) so the Molecule Lab's Realistic cloud is drawn exactly as the Atom Lab's. Fills B (one soft dot each, in order); false when D is empty.
// Draw with blobsPalette(B, !light, dc.palette()): dc is changed here (the darker light-page colours).
struct GlowDot { float x, y, r, wt; unsigned char g; };   // place, size, depth weight (1 = the plain dot) and the colour group (Sign: 0 = +, 1 = −; Wheel: the phase bin)
bool glowBlobs(const std::vector<GlowDot>& D, long cap, bool light, DotColours& dc, bool darken, double base, double w, double h, double& expo, std::vector<render::Renderer2D::Blob>& B);

// ---- Cloud 2D side view: one band; the dots are drawn group by group ----
struct SideDot { float x, y; unsigned char g; };
// Every dot on the picture (x, z) in the order: group by group, inside a group in dot order (the same two ways as above, chosen by `counting`).
void sideDots(const float* pts, long cnt, const unsigned char* grp, int nGroups, double cx, double cy, double pxm, double w, double h, bool counting, std::vector<SideDot>& out);
// One square blob (squareDot, half-size 1, light `weight`) for each side dot, in the same order, into `out` (cleared first). 0.13 stage 4 part C: the vector is meant to be KEPT from frame to frame
// (AtomView::blobs_): reserve() is a no-op once it is big enough, so a frame allocates nothing. Up to 0.13 stage 3 the side view started from an empty vector every frame and grew it by doubling
// (about 17 reallocations of up to 3.8 MB for the 48,000 dots of Ultra), which was most of why scene 10 cost more than scene 8 (REPORT.md).
void sideBlobs(const std::vector<SideDot>& side, const DotColours& dc, double weight, std::vector<render::Renderer2D::Blob>& out);

}  // namespace atom
