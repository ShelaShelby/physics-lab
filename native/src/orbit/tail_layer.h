// 0.78.0: the comets' tails on the Orbit Lab picture. The dust grains (orbit/comet_tail.h) are placed either by the GPU (kGrainsCS, run by the rocks'
// program object: render/rock_compute.h; each comet's ring of grains stays on the card and only new releases are sent) or by the CPU (tail::CometGrains::
// position, double: the reference and the fallback). The gas tail is a direction (tail::ionTailDirection) drawn as long as the dust tail.
// No Dear ImGui in here, so the tests can drive it.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "belts.h"
#include "comet_tail.h"
#include "render/renderer2d.h"
#include "solar_system.h"

namespace orbit {

class TailLayer {
public:
    enum class Path { None, Cpu, Gpu };
    // once a frame, before the bodies: every comet's grains brought to the time now; the GPU keeps only the rings of comets that have grains
    void prepare(render::Renderer2D& R, const SolarSystem& sys, bool gpu);
    // one comet's dust grains as squares of `size` points in colour c. gpu: the "GPU rocks" switch. Returns the path used (None: no grains).
    Path drawDust(render::Renderer2D& R, const Body& b, const BeltCamera& cam, bool gpu, render::Color c, double size);
    // the gas tail: its direction (unit) and length (m, the dust tail's reach); false: none (the nucleus is beyond tail::kSwitchOnAU or has no dust)
    bool gasTail(const Body& b, Vec3& dir, double& lengthM) const;
    const tail::CometGrains* grains(const CometData* c) const;
    // for tests: the screen places the CPU path gives (x, y a grain; -1e9 for no grain), at the last prepare's time
    void cpuScreen(const Body& b, const BeltCamera& cam, std::vector<float>& xy);
    // for tests: the GPU slot of this comet's grains (-1: none)
    int gpuSlot(const Body& b) const;
    long cpuGrainsPlaced() const { return cpuPlaced_; }
    Path lastPath() const { return last_; }                   // the path of the last comet drawn with grains (None: none had any)
    // the benchmark's Path line
    std::string pathText() const;
private:
    struct Cpu { double now = 1e300; std::vector<Vec3> p; std::vector<char> on; };
    const Cpu& cpuPlaces(const Body& b);
    std::map<const CometData*, tail::CometGrains> g_;
    std::map<const CometData*, Cpu> cpu_;
    std::map<const CometData*, int> slot_;
    std::vector<float> screen_;
    double jd_ = 0;
    long cpuPlaced_ = 0;
    Path last_ = Path::None;
};

}  // namespace orbit
