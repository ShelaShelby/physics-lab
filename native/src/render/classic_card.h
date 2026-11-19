// 0.64.0: the Classic Cloud 3D dots from the card (Vulkan; render/shaders.h kClassicCS): the cloud's places sent as the Glow cloud's are (only the dots added
// since the last frame), the groups each frame, then each dot put on the picture, banded and its square written on the card in the processor's draw order
// (atom/cloud_dots.cpp buildClassic3D, the reference and the fallback), drawn with the renderer's blob program. Built and checked by atom/classic_check.cpp
// under its guard (app/gpu_guard.h gpuVkClassicGuard).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

struct ClassicDraw {
    const float* pts = nullptr; long cnt = 0, cap = 0;          // places (m, x y z), dots shown (0 .. cnt), the ring's size
    std::uint64_t epoch = 0; long k = 0;                        // which cloud and dots recorded so far (as Renderer2D::CloudGpuDraw)
    const unsigned char* grp = nullptr;                         // each dot's colour group (255: not shown)
    std::uint64_t grpEpoch = 0;                                 // 0.65.0: the groups' epoch (atom/cloud_dots.h keepGroups): the same as the last frame's: not sent again; 0: sent
    float cx = 0, cy = 0, pxm = 0, ext = 1, ca = 1, sa = 0, cb = 1, sb = 0, w = 0, h = 0;
    float weight = 1;                                           // cloudDotWeight
    int ng = 2;                                                 // the number of groups (at most 24)
    float groupA[24] = {};                                      // each group colour's own opacity
    bool wheel = false, light = false;                          // the palette (Renderer2D::PhasePalette)
    float plus[4] = {}, minus[4] = {};
    // 0.65.0: the side view with every electron (atom/cloud_dots.cpp sideDots: the reference): x across, z up, one band of opacity 1 and the square's half
    // size `half` (Classic: 1, kind 5) or the soft dot's radius (Glow: 4.4, kind 6, soft). Glow: its dots counted per 8 x 8-point cell, `exposure` gets the
    // counts (as floats, every cell) and returns the dots' light (cloud_light.h cloudDotLight; negative: draw nothing), used as `weight`; additive: dark pages.
    bool side = false, soft = false, additive = false;
    float half = 1;
    // 0.65.0: the Slice (atom_view.cpp's Slice loop: the reference): 1 a vertical cut (x across, z up; the slab in y), 2 a horizontal one (x, y; the slab in z):
    // each dot at its own index, in dot order; shown within sliceTh (m) of the cut at sliceOff (m); a square (half 1.25, moved 0.25 points) or a soft dot (4.2)
    int slice = 0;
    float sliceOff = 0, sliceTh = 0;
    std::function<double(const std::vector<float>&)> exposure;
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
    // 0.65.0: a side view's Glow check: its cells (the counts per 8 x 8-point cell, row by row) of the job just polled Done
    virtual std::vector<std::uint32_t> checkCells() const = 0;
    bool ready = false;                                         // its start-up check passed: Renderer2D::cloudClassic draws with it
};

}  // namespace render
