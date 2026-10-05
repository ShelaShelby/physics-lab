// OpenGL 4.3 compute for the 90 % outline (0.13 stage 3): the density grid and the 90 % level on the GPU, and the Slice's plane of density values.
// The first use of an outline (the 64 x 64 x 64 pass, once per orbital) cost about 30 ms on the CPU: 13 ms to evaluate the density in 262,144 cells and 17 ms to sort them to find the 90 % level
// (measured in stage 3, REPORT.md). Both now run in compute shaders (shaders.h, kOutlineDensityCS and kOutlineLevelCS); the CPU asks, carries on, and takes the answer a frame or two later
// WITHOUT waiting: a job is started, a fence marks its end, and each frame the CPU only asks "is it done?" (glClientWaitSync with a zero timeout). When it is, the grid is read back ONCE
// (1 MB, once per orbital) and the CPU builds the outline's lines from it with the same code as before. The Slice's 200 x 200 values are made the same way, one frame late.
// The C++ functions (outline.cpp) are the reference and the fallback. The types here have no graphics calls, so the atom code and its tests can use them; the OpenGL class is in outline_compute.cpp.
#pragma once

#include <vector>

namespace render {

// The density |psi|^2 of one state at Z = 1 (lengths in Bohr radii), as the kernel needs it. A direct description of atom::density: |psi|^2 = R~(r)^2 * weight * (Re^2 + cIm^2 Im^2),
// R~ = L(2r/n) e^(-r/n) with L the Laguerre polynomial with coefficients lag[0] + lag[1] x + lag[2] x^2 + lag[3] x^3; Re and Im are the angular polynomials ang and angIm (atom_core.cpp, angular()).
// A real orbital: weight 1, cIm 0. A complex state (Re + i c Im) / sqrt(2): weight 1/2, cIm = c.
struct OutlineDensity {
    float n = 1;
    float lag[4] = {1, 0, 0, 0};
    int ang = 0, angIm = 0;
    float cIm = 0, weight = 1;
};

enum class JobState { Pending, Done, Failed };      // Failed: unknown job, or the GPU could not do it (the caller then uses the CPU path)

class IOutlineCompute {
public:
    virtual ~IOutlineCompute() {}
    virtual bool ok() const = 0;                     // false: no OpenGL 4.3 (the CPU path is used)
    // The density on an N x N x N grid over [-half, half]^3 (cell centres at -half + (i + 1/2) h, h = 2 half / N, index (k N + j) N + i for x = i, y = j, z = k), and the level c of the densest
    // cells that hold `target` of the probability: the largest cell value c such that the sum of (value * scale) over the cells with value >= c is >= target (scale = dV / norm).
    // Returns a job number (> 0, never reused), or 0 if it could not be started.
    virtual int beginGrid(const OutlineDensity& d, int N, float half, float scale, float target) = 0;
    // The density on an n x n plane: horizontal: the plane z = offset (a = x, b = y), else y = offset (a = x, b = z); values[b n + a], cell centres as above.
    virtual int beginPlane(const OutlineDensity& d, int n, float half, bool horizontal, float offset) = 0;
    // Never blocks. Done: the job's results are filled in and the job is released. Pending: ask again next frame. Failed: nothing to take, the job is released.
    virtual JobState pollGrid(int job, std::vector<float>& grid, float& level) = 0;
    virtual JobState pollPlane(int job, std::vector<float>& values) = 0;
    virtual void cancel(int job) = 0;                // forget a job (its results are never read)
};

// Makes the OpenGL 4.3 implementation for the current context, or nullptr when there is none (a 3.3 context, a missing function, a shader that does not compile). The caller owns the result.
IOutlineCompute* makeOutlineCompute();

}  // namespace render
