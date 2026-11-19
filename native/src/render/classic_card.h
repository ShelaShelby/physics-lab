// 0.64.0: the Classic Cloud 3D dots from the card (Vulkan; render/shaders.h kClassicCS): the cloud's places sent as the Glow cloud's are (only the dots added
// since the last frame), the groups each frame, then each dot put on the picture, banded and its square written on the card in the processor's draw order
// (atom/cloud_dots.cpp buildClassic3D, the reference and the fallback), drawn with the renderer's blob program. Built and checked by atom/classic_check.cpp
// under its guard (app/gpu_guard.h gpuVkClassicGuard).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

struct ClassicDraw {
    const float* pts = nullptr; long cnt = 0, cap = 0;          // places (m, x y z), dots shown (0 .. cnt), the ring's size
    std::uint64_t epoch = 0; long k = 0;                        // which cloud and dots recorded so far (as Renderer2D::CloudGpuDraw)
    const unsigned char* grp = nullptr;                         // each dot's colour group (255: not shown)
    float cx = 0, cy = 0, pxm = 0, ext = 1, ca = 1, sa = 0, cb = 1, sb = 0, w = 0, h = 0;
    float weight = 1;                                           // cloudDotWeight
    int ng = 2;                                                 // the number of groups (at most 24)
    float groupA[24] = {};                                      // each group colour's own opacity
    bool wheel = false, light = false;                          // the palette (Renderer2D::PhasePalette)
    float plus[4] = {}, minus[4] = {};
};

class ClassicCard {
public:
    virtual ~ClassicCard() = default;
    virtual bool build(std::string& why) = 0;
    virtual bool built() const = 0;
    // the start-up check and the tests: one cloud through the card's steps into buffers read back (never waits): per dot its key (band x groups + group,
    // or 0xFFFFFFFF) and its X, Y (as float bits), 3 words a dot; the squares, 20 floats each (cnt of them); the total
    virtual int beginCheck(const ClassicDraw& d) = 0;
    virtual JobState pollCheck(int job, std::vector<std::uint32_t>& keys, std::vector<float>& squares, long& total) = 0;
    bool ready = false;                                         // its start-up check passed: Renderer2D::cloudClassic draws with it
};

}  // namespace render
