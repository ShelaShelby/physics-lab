// 0.56.0: the spinning hole's sky and disc drawn per pixel by the graphics card from the closed forms in single precision (render/shaders.h kKerrSkyFS and
// its pieces; what it computes: blackhole/kerr_closed.h). This file makes the shaders, step by step, and runs the elliptic functions alone for the tests;
// the pass itself is drawn by Renderer2D::drawLens (LensSky::kerrSky). Making it is a sequence of steps the caller guards and logs (app/gpu_guard.h):
// the tiny shader, the elliptic functions alone, the whole, each compiled and linked as its own step, in that rising order.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "gl_loader.h"
#include "kerr_compute.h"

namespace render {

// "sky " and a 64-bit FNV-1a hash of the three shaders' text, in hex: what the sky's guard keeps (app/gpu_guard.h), so that only a changed shader (or driver)
// tries the path again after it stopped the app once
std::string kerrSkyShaderId();
// the assembled sources (the order tests/check_shaders.py uses)
std::string kerrSkySource(int stage);

class KerrSky {
public:
    void reset();                                      // the programs deleted (the renderer's shutdown; no destructor: the context may be gone by then)
    static constexpr int kStages = 3;                  // 0 the tiny shader, 1 the elliptic functions alone, 2 the whole
    static const char* stageName(int stage);
    // compile and link one stage: two steps told to steps (begin, end with the driver's log and the time in ms); false: it failed (why)
    bool build(int stage, KerrSteps* steps, std::string& why);
    GLuint program(int stage) const { return stage >= 0 && stage < kStages ? prog_[stage] : 0; }
    bool ready() const { return vkReady_ ? vkReady_() : prog_[2] != 0; }
    // 0.62.0 (Vulkan, render/vk_blackhole.h): the stages made by Vulkan's side (build: -1 failed, 0 under way on its thread, 1 made; ready: the whole made);
    // build() then returns false only on a failure, and building() says the stage is still under way (the caller asks again in a later frame). Null: OpenGL's
    void useVulkan(std::function<int(int, KerrSteps*, std::string&)> build, std::function<bool()> ready) { vkBuild_ = std::move(build); vkReady_ = std::move(ready); building_ = false; }
    bool building() const { return building_; }
    double buildMs(int stage) const { return stage >= 0 && stage < kStages ? ms_[stage] : 0; }
    // tests: the elliptic functions in single precision on argument sets of 12 floats (x, y, z, p,  n, 1 - n, sin phi, cos phi,  1 - m, u, 0, 0): per set
    // 8 numbers (R_F(x, y, z), R_J(x, y, z, p), R_C(x, y), am(u | m),  F(phi | m), Pi(n; phi | m), E(phi | m), K(m)). Waits. vao: a quad of two triangles.
    void math(const std::vector<float>& in, std::vector<float>& out, GLuint vao);
private:
    GLuint prog_[kStages] = {0, 0, 0};
    double ms_[kStages] = {0, 0, 0};
    std::function<int(int, KerrSteps*, std::string&)> vkBuild_;
    std::function<bool()> vkReady_;
    bool building_ = false;
};

}  // namespace render
