// 0.57.0: the Atom Lab's two heavy loops on the graphics card, the types the atom code needs (no graphics calls here, so atom_core and its tests can use them;
// the GPU class is render/atom_card.h). The processor's code stays the exact reference and the fallback:
//   the Slice view's new dots on the moved plane: atom/cloud_sample.cpp PlaneSampler::tryPoint (shaders.h kSliceDotsCS);
//   the groups of a jump with every electron, drawn from the card's cloud buffer: atom_sim.cpp AtomLab::jumpGroups (shaders.h kJumpShowCS).
#pragma once

#include <cstdint>
#include <vector>

#include "outline_compute.h"

namespace render {

// One batch of Slice attempts: attempt t takes the generator's numbers 3t, 3t + 1, 3t + 2 after the state `seed` (orbit::Mulberry32 is a counter).
// Everything in single precision; the processor's numbers are rounded once into these fields.
struct SliceDotsJob {
    std::uint32_t seed = 0;
    int tries = 0;
    int axis = 1;                  // 1: the plane y = off, 2: z = off (Bohr radii)
    float off = 0, x0 = 0;         // the plane, and x0 = 2 Z |off| / n
    float lnG0 = 0;                // ln of the tail above the plane, G(x0)
    float scale = 1;               // n / (2 Z): x to Bohr radii
    float bound = 1;               // the bound M on |P(u)|^2
    int ang = 0, angIm = -1;       // the real angular polynomial; a complex state: Re and Im (angIm >= 0) with cIm
    float cIm = 0;
    int nh = 0;                    // coefficients of H (the tail's polynomial) and of f (the density on the plane in x), at most 16
    float h[16] = {}, c[16] = {};
    int stride = 3, first = 0;     // 0.64.0: attempt t takes the numbers stride t + first .. + 2 (one state: 3, 0; a superposition's state: 4, 1, after the choice)
};

// 0.91.0: one batch of a Dirac state's Slice attempts (atom/dirac_cloud.cpp tryDiracOnPlane): attempt t takes the numbers stride t, + 1 (+ 2: the keep test of a
// lined-up state with L = j - 1/2 > 0; stride 3, else 2). The radius is found from the TAIL of the plane's distribution, 1 - pc, so single precision keeps its
// relative digits far out (ROADMAP: inverting pc itself lost 3.5e-5 of the radius at the 99.99 % radius). tab: 4 floats a node (r, 1 - pc, -pd, 0), n nodes.
struct DiracSliceJob {
    std::uint32_t seed = 0;
    int tries = 0;
    int axis = 1;                  // 1: the plane y = off, 2: z = off (Bohr radii)
    float off = 0, a = 0;          // the plane, and |off|
    float tail0 = 1;               // 1 - pc(|off|): the plane's tail beyond |off|
    int L = 0, keep = 0;           // L = j - 1/2; keep: the keep test is made (lined up and L > 0)
    int stride = 3;
    int n = 0;                     // nodes of the table
    std::vector<float> tab;
};

// What the atom code asks of the card for the Slice (null or !ready(): the processor makes the dots). Never blocks: begin starts a batch and returns its
// number (0: could not start); poll gives Pending (ask again later), Done (res holds 4 floats per attempt: x, y, z and 1 kept / 0 rejected / -1 the
// radius came out 0) or Failed.
class ISliceCompute {
public:
    virtual ~ISliceCompute() {}
    virtual bool ready() const = 0;
    virtual int beginSlice(const SliceDotsJob& j) = 0;
    virtual int beginDirac(const DiracSliceJob&) { return 0; }        // 0.91.0: a Dirac state's batch (0: could not start); polled with pollSlice, the same 4 floats an attempt
    virtual JobState pollSlice(int job, std::vector<float>& res) = 0;
    virtual void cancelSlice(int job) = 0;
};

// 53-bit thresholds of a jump's shares for the card: a dot is shown when its fixed random number (z >> 11) / 2^53 < share, that is z >> 11 < ceil(share 2^53)
inline std::uint64_t jumpShareThreshold(double share) {
    if (!(share > 0)) return 0;
    if (share >= 1) return std::uint64_t(1) << 53;
    const double t = share * 9007199254740992.0;                    // exact: a power of two
    std::uint64_t u = std::uint64_t(t);
    if (double(u) < t) u++;
    return u;
}

}  // namespace render
