// 0.57.0: the Atom Lab's two heavy loops on the graphics card, the types the atom code needs (no graphics calls here, so atom_core and its tests can use them;
// the OpenGL class is render/atom_card.h). The processor's code stays the exact reference and the fallback:
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

// What the atom code asks of the card for the Slice (null or !ready(): the processor makes the dots). Never blocks: begin starts a batch and returns its
// number (0: could not start); poll gives Pending (ask again later), Done (res holds 4 floats per attempt: x, y, z and 1 kept / 0 rejected / -1 the
// radius came out 0) or Failed.
class ISliceCompute {
public:
    virtual ~ISliceCompute() {}
    virtual bool ready() const = 0;
    virtual int beginSlice(const SliceDotsJob& j) = 0;
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
