// 0.56.0: the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms in single precision (render/shaders.h kKerrSkyFS and
// its pieces; what it computes: blackhole/kerr_closed.h). This file assembles the shaders' text and hands each stage to Vulkan's side to make (0.69.0:
// Vulkan only; render/vk_blackhole.cpp buildSky, which also draws the pass: LensSky::kerrSky). Making it is a sequence of steps the caller guards and logs
// (app/gpu_guard.h): the tiny shader, the elliptic functions alone, the whole, each made as its own step, in that rising order.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "kerr_compute.h"

namespace render {

// "sky " and a 64-bit FNV-1a hash of the three shaders' text, in hex: what the sky's guard keeps (app/gpu_guard.h), so that only a changed shader (or driver)
// tries the path again after it stopped the app once
std::string kerrSkyShaderId();
// the assembled sources (the order tests/check_shaders.py uses)
std::string kerrSkySource(int stage);

class KerrSky {
public:
    void reset();                                      // the stages' times forgotten
    static constexpr int kStages = 3;                  // 0 the tiny shader, 1 the elliptic functions alone, 2 the whole
    static const char* stageName(int stage);
    // compile and link one stage: two steps told to steps (begin, end with the driver's log and the time in ms); false: it failed (why)
    bool build(int stage, KerrSteps* steps, std::string& why);
    bool ready() const { return vkReady_ && vkReady_(); }
    // 0.62.0 (Vulkan, render/vk_blackhole.h): the stages made by Vulkan's side (build: -1 failed, 0 under way on its thread, 1 made; ready: the whole made);
    // build() then returns false only on a failure, and building() says the stage is still under way (the caller asks again in a later frame). Null: none (every stage fails)
    void useVulkan(std::function<int(int, KerrSteps*, std::string&)> build, std::function<bool()> ready) { vkBuild_ = std::move(build); vkReady_ = std::move(ready); building_ = false; }
    bool building() const { return building_; }
    double buildMs(int stage) const { return stage >= 0 && stage < kStages ? ms_[stage] : 0; }
private:
    double ms_[kStages] = {0, 0, 0};
    std::function<int(int, KerrSteps*, std::string&)> vkBuild_;
    std::function<bool()> vkReady_;
    bool building_ = false;
};

}  // namespace render
