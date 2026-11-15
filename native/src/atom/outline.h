// The 90 % outline of the electron cloud: the boundary of the smallest region that holds 90 % of the probability of finding the electron.
// No graphics in here, so it can be tested.
//
// `orb` below is a real orbital's index or a complex state's id (atom_core.h): the region is {|psi|^2 >= c} for either.
// The model is the one the cloud already uses (atom_core.h psi()): ONE electron, non-relativistic, hydrogen-like orbitals with the nuclear charge Z.
// The region is {|psi|^2 >= c}, with c chosen so that the region holds exactly 90 % of the probability: for a fixed probability the region where the density
// is highest is the one with the smallest volume (the standard textbook definition of "the boundary surface that contains 90 % of the electron density").
// It is NOT a sphere with 90 % inside, except for s orbitals, where the two are the same. Orbitals with nodes (2s, 3s, 3p, ...) can give an inner and
// outer surfaces; all of them are in the region's boundary. For an atom with more electrons the real shape differs.
//
// psi(Z) is psi(1) with all lengths divided by Z (psi(orb, Z, x, y, z) = psi(orb, 1, Zx, Zy, Zz) up to a constant), so the outline is worked out once per
// orbital at Z = 1 (lengths in Bohr radii) and only scaled by 1/Z for the atom on screen. It is worked out on first use and kept.
#pragma once

#include <cstddef>
#include <vector>

#include "render/outline_compute.h"

namespace atom {

struct OutSeg2 { float x0, y0, x1, y1; };
struct OutSeg3 { float x0, y0, z0, x1, y1, z1; };

constexpr int kOutlineGrid = 64;           // points per side of the grid the region is found on
constexpr double kOutlineProb = 0.9;

struct Outline {
    int orb = -1;
    double norm = 0;                       // integral of |psi|^2 over all space (psi is not normalised), a0^3
    double c = 0;                          // the level: the region is |psi|^2 >= c (the same units as psi^2)
    double half = 0;                       // the grid covers [-half, half]^3, a0
    double mass = 0;                       // the probability the grid says is inside the region (about 0.9)
    double rmax = 0;                       // the farthest the region reaches from the nucleus, a0 (to a grid step)
    std::vector<OutSeg3> wire;             // contour lines of the region's boundary in 5 planes (0, +-1/3, +-2/3 of its reach) for each of x, y, z = const (a faint wireframe for the 3D view), a0
    std::vector<OutSeg2> side;             // the outline of the region seen from the side (projected along y, in the x-z plane), a0
};

double orbitalNorm(int orb);               // integral of |psi|^2 over all space at Z = 1, a0^3: EXACT since 0.13 stage 3 (closed form, see outline.cpp; before, a numerical sum with an error of 3e-6 to 8.8e-5)
const Outline& outlineOf(int orb);         // cached; worked out on the CPU, now (the reference: the 64^3 pass takes about 30 ms)

// ---- 0.13 stage 3: the same on the GPU, without waiting (render/outline_compute.h) ----
// The app gives the outline code its compute object once (null: none, an OpenGL 3.3 PC, or PL_NO_COMPUTE); the CPU path is used when there is none.
void setOutlineCompute(render::IOutlineCompute* gc);
render::IOutlineCompute* outlineCompute();
// The outline of `orb` if it is READY, else nullptr, and it never waits. Ready: cached; or just finished on the GPU; or no usable GPU (useGpu false, or no compute object, or the job failed): then it is
// worked out now on the CPU, exactly as outlineOf does. nullptr: the GPU is working on it (the first call starts the job; draw nothing yet and ask again next frame).
const Outline* outlineReady(int orb, bool useGpu);
// The pieces of outlineOf, public so the GPU's results can be compared with the CPU's, one by one (tests) and so the GPU path builds its outline with the SAME code.
render::OutlineDensity outlineDensityOf(int orb);                          // the description of |psi|^2 at Z = 1 the kernel needs
std::vector<float> outlineGridCpu(int orb);                                // the kOutlineGrid^3 density grid, float, as outlineOf makes it (the CPU reference of the GPU's grid)
float outlineLevelCpu(const std::vector<float>& grid, double dV, double norm);   // the 90 % level of a grid: sort densest first and add up until 0.9 (the CPU reference of the GPU's level)
std::vector<float> outlinePlaneCpu(int orb, double half, bool horizontal, double offset, int n);   // the density on a plane, as outlineSlice makes it (the CPU reference of the GPU's plane)
Outline outlineFromGrid(int orb, const std::vector<float>& grid, float level);   // the rest of outlineOf (mass, reach, side view, wireframe) from a grid and its level; uncached
// For the benchmark's "first use of an outline" scene (and its test): empty the cache so the next outlineOf runs the whole 64^3 pass again; how many are cached.
// Only call between frames (nothing may hold an outlineOf reference).
void clearOutlineCache();
std::size_t outlineCacheSize();
// the contour |psi|^2 = c in a plane: vertical (y = offset, axes x and z) or horizontal (z = offset, axes x and y); offset in a0 at Z = 1.
std::vector<OutSeg2> outlineSlice(int orb, bool horizontal, double offset, int n = 200);
// The same contour from density values already made (the GPU's plane): marching squares at the outline's level.
std::vector<OutSeg2> outlineSliceFromValues(const Outline& O, const std::vector<float>& values, int n = 200);

// The Slice view's contour, kept from frame to frame (0.13 stage 3). CPU path: made now when the orbital, the plane or the cut changed (as before). GPU path (the compute object of
// setOutlineCompute, and `gpu` true): a compute job makes the plane's density values while the CPU carries on; every frame update() only asks whether the job has finished and, when it has,
// builds the contour from its values; the next job is for the cut as it is by then, so while the cut moves the contour is a frame or two behind it. At most one job is in flight. A job that fails
// or cannot start is replaced by the CPU's contour for that frame. The contour of another orbital or plane is never shown (shows()).
class SliceContour {
public:
    void update(const Outline& O, int orb, bool hz, double off0, bool gpu);   // once per frame
    bool shows(int orb, bool hz) const { return orb_ == orb && hz_ == hz; }   // is the contour held the one for this orbital and plane?
    const std::vector<OutSeg2>& segments() const { return segs_; }
    bool jobInFlight() const { return job_ != 0; }
private:
    int orb_ = -1; bool hz_ = false; double off_ = 0; std::vector<OutSeg2> segs_;   // the contour held, and what it is for
    int job_ = 0, jobOrb_ = -1; bool jobHz_ = false; double jobOff_ = 0;           // the job in flight (0: none) and what it is for
};

}  // namespace atom
