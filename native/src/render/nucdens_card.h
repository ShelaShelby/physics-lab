// 0.67.0: the nucleus' Density look on the graphics card (Vulkan; render/shaders.h kNucDensCS): the line-of-sight sums of atom_core.cpp projectChargeDensity
// in single precision and the cells' squares written on the card, drawn by the renderer's blob program from the card's buffer (no square sent by the
// processor). Made and checked by atom/nucdens_check.cpp under its guard (app/gpu_guard.h gpuVkNucDensGuard); the processor's sums and squares stay the
// reference and the fallback.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

struct NucDensDraw {
    // the model (atom_core.h NucleusChargeDensity): the deformed shapes' table T over (across^2, |along|), nT x nT; or (radial) I0, K0, K2, I2, nR each
    int radial = 0, kk = 0, nT = 0, qn = 64;
    const float* table = nullptr;
    std::size_t tableN = 0;
    std::uint64_t tableVersion = 0;                      // a new model: a new number (its table sent again)
    float rE2 = 0, invT = 0, invU = 0, dl = 1;           // the rays' reach (fm^2), 1 / hT, 1 / hU, the trapezoids' step (fm)
    float n1 = 0, n2 = 0, n3 = 1, nperp = 0, sx = 1, sy = 0, hq = 1, hR = 1;
    // the grid: G x G cells of cellFm, centred; on the picture from (x0, y0) (points) in cells of cellPts
    int G = 0;
    float halfFm = 0, cellFm = 0;
    float x0 = 0, y0 = 0, cellPts = 0;
    float colour[4] = {1, 1, 1, 1};
};

class NucDensCard {
public:
    virtual ~NucDensCard() = default;
    virtual bool build(std::string& why) = 0;
    virtual bool built() const = 0;
    // the start-up check and the tests: steps 0 and 1 into a buffer read back (never waits): the G x G cells' values, row 0 at the top
    virtual int beginCheck(const NucDensDraw& d) = 0;
    virtual JobState pollCheck(int job, std::vector<float>& grid) = 0;
    virtual void cancel(int job) = 0;
    virtual long runs() const { return 0; }              // how many times the frame's steps ran (they run only when something changed)
    bool ready = false;                                  // its start-up check passed: Renderer2D::nucDensity draws with it
};

}  // namespace render
