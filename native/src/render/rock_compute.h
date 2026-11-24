// Compute on the graphics card for the Orbit Lab's rocks (0.13 stage 4 part A; 0.61.0: on Vulkan, render/vk_card.cpp makeVkRockCompute): the position of every belt rock on its Kepler ellipse, and its place on the picture, worked out on the GPU.
// Up to 0.13 stage 3 the CPU did it every frame for every rock (orbit::beltPoint, then the camera): 6,000 rocks in scene 1, 48,000 in scene 6 (the benchmark's measured cost, REPORT.md). The
// CPU functions stay (orbit/belts.h: beltPoint, beltScreen) as the reference and as the path where the card's program is not made.
//
// What goes to the GPU once (per set of rocks, found by BeltGroup::serial): 12 floats a rock (the Kepler elements) and the mean motion n of each rock as a DOUBLE.
// What goes every frame: the simulated time t (a double, sent as two floats, hi and lo: t = hi + lo exactly), the Trojans' sway phase reduced to 0..2 pi in double, and the camera.
// PRECISION RULE (DECISIONS.md): times and angles that grow without limit stay in DOUBLE: the shader forms the mean anomaly M = M0 + n t in double (double precision: Vulkan's shaderFloat64, queried;
// a card without it draws the rocks by the CPU) and reduces it to -pi..pi in double; only that small number goes into 32-bit float for Kepler's equation and the position. The types have no graphics calls, so the
// Orbit code and its tests can use them.
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

// One frame's camera and time. The camera is orbit::BeltCamera's (orbit/belts.h, orbit/camera.h; orbit::rockFrameCamera) in plain doubles; t is the simulated
// time in seconds since the start date. 2D: (sx, sy) is the barycentre's place on the picture. 0.75.0, 3D: the pinhole camera: (sx, sy) the principal point,
// d its distance from the look-at plane at depth cd, (lx, ly) its axis, F the focal length (points), zNear the near plane (m).
struct RockFrame {
    double t = 0;
    double libPhase = 0;                // t / year / 150 * 6.2832 brought into 0..2 pi, in double (the Trojans' sway)
    double bc[3] = {0, 0, 0};
    double ca = 1, sa = 0, cb = 1, sb = 0, px = 1;
    double sx = 0, sy = 0, d = 1, cd = 0, lx = 0, ly = 0, F = 1, zNear = 0;
    bool d3 = false;
};

class IRockCompute {
public:
    virtual ~IRockCompute() {}
    virtual bool ok() const = 0;                                       // false: not on the card (the CPU path is used)
    // The sets of rocks that are alive now (the serials of the belts being drawn): every other set is dropped from the GPU. Called at the start of each frame's belts, so a slot number given out
    // during a frame stays valid until the next frame.
    virtual void keepOnly(const std::vector<std::uint64_t>& live) = 0;
    // The slot holding these rocks, sent to the GPU first if this serial is not there yet (at most 32 sets at a time with the comets' grains; 0.78.0: was 16). -1: could not (all 32 are in use: the caller draws by the CPU).
    virtual int slotFor(const RockSet& set) = 0;
    // Works out the screen position (x, y points, 2 floats a rock) of every rock of the slot into its buffer. Needs no CPU wait: the draw that follows uses the buffer.
    virtual bool dispatch(int slot, const RockFrame& f) = 0;
    virtual unsigned positionsBuffer(int slot) const = 0;              // the card's buffer of the last dispatch's positions (Vulkan: not 0 once made)
    virtual bool readPositions(int slot, std::vector<float>& xy) = 0;  // for tests: waits for the GPU and copies the positions back
    virtual int uploads() const = 0;                                   // how many times a set of rocks was sent (for tests: a rebuild only when the rocks change)
    // 0.78.0: the comets' dust grains (orbit/comet_tail.h), run by the same program object and guard (kGrainsCS). One slot a comet holds a ring of `count`
    // grains: kGrainStride floats each (r0 in AU, v0 in AU/day, mu in AU^3/day^2, below 0: no grain) and a DOUBLE release time (days since J2000, TDB).
    // The ring is rewritten in parts as releases change. For grains, RockFrame::t is the time now in days since J2000 (TDB), not the simulated seconds.
    virtual bool grainsOk() const { return false; }
    // the slot for this comet's ring (made if new: fresh = true, the caller then sends every grain); -1: could not
    virtual int grainSlot(std::uint64_t id, int count, bool& fresh) { (void)id; (void)count; fresh = false; return -1; }
    virtual bool grainWrite(int slot, int first, int n, const float* el, const double* t0) { (void)slot; (void)first; (void)n; (void)el; (void)t0; return false; }
    virtual bool grainDispatch(int slot, const RockFrame& f) { (void)slot; (void)f; return false; }
    virtual void keepOnlyGrains(const std::vector<std::uint64_t>& live) { (void)live; }   // the comets' rings that are drawn this frame: the others leave the GPU
};

constexpr int kGrainStride = 8;   // (orbit::tail::kStride)

}  // namespace render
