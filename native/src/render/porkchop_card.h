// 0.92.0: the Orbit Lab's porkchop grid on the graphics card (single precision; Vulkan: render/vk_card.cpp): Lambert's problem solved for every cell of a grid of departure
// and arrival dates by one compute shader (shaders.h kPorkchopCS). Made step by step under its own guard (app/gpu_guard.h gpuPorkchopGuard; ui/porkchop_runner.cpp names and
// logs the steps): compile, link, a small run checked against the processor (orbit/lambert.h porkchopCpu is the reference and the fallback), then the lab's grids.
// Nothing here waits for the card in a frame: begin starts a batch, poll asks later.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "kerr_compute.h"
#include "outline_compute.h"

namespace render {

// One grid. dep, arr: 8 floats a date (position x y z, 0, velocity vx vy vz, 0) in AU and canonical speed (mu = 1). Cell (i, j) has the time of flight off + arrStep j - depStep i
// (days); tu: days a canonical time unit; vk: km/s a canonical speed. The result: 2 floats a cell, i * nArr + j: C3 (km^2/s^2) and the arrival v_inf (km/s); (-1, -1): no transfer.
struct PorkchopJob {
    int nDep = 0, nArr = 0;
    float off = 0, depStep = 1, arrStep = 1, tu = 1, vk = 1;
    std::vector<float> dep, arr;
};

std::string porkchopShaderId();     // "porkchop <hash>": what its guard keeps (a changed shader tries again by itself)

class PorkchopCard {
public:
    virtual ~PorkchopCard() {}
    // compile and link: two steps told to steps (begin, end with the time); false: it failed (why)
    virtual bool build(KerrSteps* steps, std::string& why) = 0;
    virtual bool built() const = 0;
    virtual void reset() = 0;                                    // the program and buffers deleted (the renderer's shutdown)
    bool ready() const { return ready_ && built(); }
    void setReady(bool on) { ready_ = on; }                      // after the check passed (the guard's steps)
    virtual int begin(const PorkchopJob& j) = 0;                 // a job number, 0: could not start
    virtual JobState poll(int job, std::vector<float>& res) = 0; // Pending, Done (res filled) or Failed
    virtual void cancel(int job) = 0;
protected:
    bool ready_ = false;
};

// no card program (a start whose card programs could not be made): every call says no
class PorkchopCardNone : public PorkchopCard {
public:
    bool build(KerrSteps*, std::string& why) override { why = "the graphics card's programs are not available on this start"; return false; }
    bool built() const override { return false; }
    void reset() override { ready_ = false; }
    int begin(const PorkchopJob&) override { return 0; }
    JobState poll(int, std::vector<float>&) override { return JobState::Failed; }
    void cancel(int) override {}
};

}  // namespace render
