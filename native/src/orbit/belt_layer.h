// The belts' rocks on the Orbit Lab picture (0.13 stage 4 part A): one square draw per belt, the places worked out either by the CPU (orbit::beltPoint and orbit::beltScreen for every rock,
// the way it always was: the reference, and the path on an OpenGL 3.3 PC) or on the GPU (render/rock_compute.h, OpenGL 4.3: the rocks are sent once and every frame only the time and the camera).
// No Dear ImGui in here, so the tests can drive it.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "belts.h"
#include "render/renderer2d.h"
#include "solar_system.h"

namespace orbit {

class BeltLayer {
public:
    enum class Path { None, Cpu, Gpu };
    // Adds every belt of `sys` to R. selectedBelt: the index of the belt shown bigger and in `highlight` (or -1). gpuRocks: the "GPU rocks" switch. Returns the path used.
    Path draw(render::Renderer2D& R, const SolarSystem& sys, const BeltCamera& cam, int selectedBelt, render::Color highlight, bool gpuRocks);
    Path lastPath() const { return last_; }
    // The GPU's copy of this group's rocks (made once per serial, kept until the rocks are gone): for the renderer and for tests. A group needs a serial of its own (makeBelts gives one).
    render::RockSet gpuSet(const BeltGroup& g);
    long cpuRocksPlaced() const { return cpuPlaced_; }      // for tests: how many rocks the CPU path has run beltPoint for, in all (the GPU path adds none)
private:
    struct Packed { int count = -1; std::vector<float> el; std::vector<double> nn; };
    const Packed& packed(const BeltGroup& g);
    std::map<std::uint64_t, Packed> packed_;                // the GPU's copy of each set of rocks, made once per serial
    // the CPU path: positions only change with the simulated time, so they are worked out once per time step (not again while paused, or when only the view moves)
    std::vector<std::vector<Vec3>> pos_;
    std::vector<std::uint64_t> posSerials_;
    double posT_ = -1;
    std::int64_t posEpoch_ = 0;
    std::vector<float> screen_;
    Path last_ = Path::None;
    long cpuPlaced_ = 0;
};

}  // namespace orbit
