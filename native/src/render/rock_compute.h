// OpenGL 4.3 compute for the Orbit Lab's rocks (0.13 stage 4 part A): the position of every belt rock on its Kepler ellipse, and its place on the picture, worked out on the GPU.
// Up to 0.13 stage 3 the CPU did it every frame for every rock (orbit::beltPoint, then the camera): 6,000 rocks in scene 1, 48,000 in scene 6 (the benchmark's measured cost, REPORT.md). The
// CPU functions stay (orbit/belts.h: beltPoint, beltScreen) as the reference and as the path on a PC without OpenGL 4.3.
//
// What goes to the GPU once (per set of rocks, found by BeltGroup::serial): 12 floats a rock (the Kepler elements) and the mean motion n of each rock as a DOUBLE.
// What goes every frame: the simulated time t (a double, sent as two floats, hi and lo: t = hi + lo exactly), the Trojans' sway phase reduced to 0..2 pi in double, and the camera.
// PRECISION RULE (DECISIONS.md): times and angles that grow without limit stay in DOUBLE: the shader forms the mean anomaly M = M0 + n t in double (GLSL 4.00 double precision, which every
// OpenGL 4.3 context has) and reduces it to -pi..pi in double; only that small number goes into 32-bit float for Kepler's equation and the position. The types have no graphics calls, so the
// Orbit code and its tests can use them; the OpenGL class is in rock_compute.cpp.
#pragma once

#include <cstdint>
#include <vector>

namespace render {

// the floats of one rock in RockSet::el (RockSet::stride of them): moving rock: M0 (rad), a (AU), e, libration size (rad), libration phase, P (3), Q (3), 0; still rock (the Oort cloud): x, y, z (m) in the P slots
constexpr int kRockStride = 12;

struct RockSet {
    std::uint64_t serial = 0;           // which rocks: the same serial and count means the same data (it is not looked at again)
    int count = 0;
    bool isStatic = false;              // the Oort cloud: positions that never move
    const float* el = nullptr;          // count x kRockStride floats
    const double* nn = nullptr;         // count mean motions, rad/s (null for still rocks)
};

// One frame's camera and time. The camera is orbit::BeltCamera's (orbit/belts.h) in plain doubles; t is the simulated time in seconds since the start date.
struct RockFrame {
    double t = 0;
    double libPhase = 0;                // t / year / 150 * 6.2832 brought into 0..2 pi, in double (the Trojans' sway)
    double bc[3] = {0, 0, 0};
    double ca = 1, sa = 0, cb = 1, sb = 0, px = 1, W = 1, H = 1, D = 1, cd = 0, ox = 0, oy = 0;
    bool d3 = false;
};

class IRockCompute {
public:
    virtual ~IRockCompute() {}
    virtual bool ok() const = 0;                                       // false: no OpenGL 4.3 (the CPU path is used)
    // The sets of rocks that are alive now (the serials of the belts being drawn): every other set is dropped from the GPU. Called at the start of each frame's belts, so a slot number given out
    // during a frame stays valid until the next frame.
    virtual void keepOnly(const std::vector<std::uint64_t>& live) = 0;
    // The slot holding these rocks, sent to the GPU first if this serial is not there yet (at most 16 sets at a time). -1: could not (all 16 are in use: the caller draws by the CPU).
    virtual int slotFor(const RockSet& set) = 0;
    // Works out the screen position (x, y points, 2 floats a rock) of every rock of the slot into its buffer. Needs no CPU wait: the draw that follows uses the buffer.
    virtual bool dispatch(int slot, const RockFrame& f) = 0;
    virtual unsigned positionsBuffer(int slot) const = 0;              // the OpenGL buffer of the last dispatch's positions
    virtual bool readPositions(int slot, std::vector<float>& xy) = 0;  // for tests: waits for the GPU and copies the positions back
    virtual int uploads() const = 0;                                   // how many times a set of rocks was sent (for tests: a rebuild only when the rocks change)
};

// The OpenGL 4.3 implementation for the current context, or nullptr (a 3.3 context, a missing function, a shader that does not compile). The caller owns it.
IRockCompute* makeRockCompute();

}  // namespace render
