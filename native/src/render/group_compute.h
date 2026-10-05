// OpenGL 4.3 compute for the electron cloud (0.13 stage 2): the colour group of every dot of a superposition or a cloud jump, worked out on the GPU.
// The per-dot, per-frame work of those two views (thinning each dot against its fixed random number, the phase of the superposed wave, its colour bin) used to be a loop on the CPU
// (AtomLab::superGroup, AtomLab::jumpGroups: they stay, as the reference and as the path on a PC that has no OpenGL 4.3). The amplitudes of every dot are stored on the GPU once
// (they are made when the dot is made); each frame the CPU sends the beat phase and the weights, a compute shader (shaders.h, kGroupsCS) writes one byte per dot, and the CPU reads
// that back and draws exactly as before (the dots are projected, put in their draw order and drawn by the code of stage 1).
// The types here have no graphics calls, so the atom code and its tests can use them; the OpenGL class is in group_compute.cpp.
#pragma once

#include <cstdint>

namespace render {

// What the superposition kernel needs each frame (AtomLab::superGpuParams makes it).
struct SuperGroupParams {
    float a = 1, b = 0;                 // the amplitudes of the two states (their shares are a^2 and b^2)
    float emRe = 1, emIm = 0;           // exp(-i theta), theta = the beat phase now
    int overlap = 0;                    // 1: the two states overlap, so there is no beat and every dot is shown
};
// What the jump kernel needs each frame (AtomLab::jumpGpuParams): the terms of jumpTerms(), and whether the colours are the phase wheel.
struct JumpGroupParams {
    float cth = 1, sth = 0, lam = 1;    // cos theta, sin theta, cos^2 theta
    float emRe = 1, emIm = 0;           // exp(-i phi)
    float epRe = 1, epIm = 0;           // exp(i phi u)
    int wheel = 1;                      // 1: Wheel (the phase bin), 0: Sign
};

// What the picture asks for. The OpenGL class implements it; a test can stand in for it.
class IGroupCompute {
public:
    virtual ~IGroupCompute() {}
    virtual bool ok() const = 0;                     // false: no OpenGL 4.3 (the CPU path is used)
    // out[i] = the colour group of dot i (255 = not shown). amps: 4 floats a dot, the normalised amplitudes (A re, A im, B re, B im).
    // epoch changes whenever the array was made again; the dots in [dirtyLo, dirtyHi) were written since the last call (so only those are sent again).
    virtual bool superGroups(const float* amps, long count, std::uint64_t epoch, long dirtyLo, long dirtyHi, const SuperGroupParams& p, unsigned char* out) = 0;
    // out has 2 jn entries: the old dots, then the new ones. The jump's amplitudes are fixed once it is built, so they are sent once per epoch.
    virtual bool jumpGroups(const float* ampsA, const float* ampsB, long jn, std::uint64_t epoch, const JumpGroupParams& p, unsigned char* out) = 0;
};

// Makes the OpenGL 4.3 implementation for the current context, or returns nullptr when there is none (a 3.3 context, a missing function, a shader that does not compile).
// The caller owns the result. Needs the loader's 3.3 functions loaded; loads the 4.3 ones itself.
IGroupCompute* makeGroupCompute();

}  // namespace render
