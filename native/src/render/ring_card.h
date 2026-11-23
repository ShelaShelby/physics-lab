// 0.64.0: Saturn's rings per pixel on the graphics card (Vulkan; render/shaders.h kRingsFS, its probe kRingsProbeCS). What the Orbit Lab gives the card for
// one half of one ring system in one frame, and the card's program: built and checked by orbit/ring_check.cpp under its guard (app/gpu_guard.h
// gpuVkRingsGuard), against the processor's triangles (orbit/ring_draw.h), which stay the reference and the fallback (OpenGL draws them always).
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

constexpr int kRingCardBands = 768;                       // the shader's uBands (12 KB of uniforms; the profile gives at most about 700)

struct RingsDraw {
    float cx = 0, cy = 0, r = 1;                         // the planet's middle and equatorial radius (points)
    float e1[3] = {1, 0, 0}, e2[3] = {0, 1, 0};          // the ring plane's axes (screen frame: right, up, towards you)
    float K[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};            // the screen frame to the planet's frame (identity without a spin model)
    float sz = 1;                                        // equatorial / polar radius
    float Lb[3] = {0, 0, 1};                             // the Sun in the planet's frame
    int back = 0;                                        // 1: the half away from you (drawn before the ball)
    float tint[3] = {0.91f, 0.83f, 0.66f};               // the drawn tint (sRGB)
    std::vector<float> bands;                            // 4 floats a band, sorted: a0, a1 (planet radii), opacity, light k
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;                // the area to cover (points): the rings' box clipped to the picture
    float pp[2] = {0, 0}, zr = 0, wn = 0;                // 0.75.0: the pinhole camera (orbit/ring_draw.h RingFrame; zr 0: the flat map)
};

// the uniforms of kRingsPart (the draw's and the probe's), set through f(name, values, comps, count) and i(name, values, comps)
template <class F, class I>
void ringsUniforms(const RingsDraw& d, F&& f, I&& i) {
    const float cen[2] = {d.cx, d.cy};
    f("uCen", cen, 2, 1); f("uR", &d.r, 1, 1); f("uE1", d.e1, 3, 1); f("uE2", d.e2, 3, 1);
    f("uK0", d.K, 3, 1); f("uK1", d.K + 3, 3, 1); f("uK2", d.K + 6, 3, 1); f("uSz", &d.sz, 1, 1); f("uLb", d.Lb, 3, 1);
    i("uBack", &d.back, 1); f("uTint", d.tint, 3, 1);
    f("uPP", d.pp, 2, 1); f("uZr", &d.zr, 1, 1); f("uWn", &d.wn, 1, 1);
    const int nb = static_cast<int>(std::min<std::size_t>(d.bands.size() / 4, static_cast<std::size_t>(kRingCardBands)));
    i("uNb", &nb, 1);
    if (nb > 0) f("uBands", d.bands.data(), 4, nb);
}

class RingCard {
public:
    virtual ~RingCard() = default;
    virtual bool build(std::string& why) = 0;            // the drawing's pipeline and the probe's (false: why says it)
    virtual bool built() const = 0;
    // the shader's function (ringAt) at n points (pixel centres, points): 4 floats a point (r, g, b, opacity); never waits
    virtual int beginProbe(const RingsDraw& d, const std::vector<float>& xy) = 0;
    virtual JobState pollProbe(int job, std::vector<float>& rgba) = 0;
    virtual void cancel(int job) = 0;
    bool ready = false;                                  // its start-up check passed: Renderer2D::ringsCard draws with it
};

}  // namespace render
