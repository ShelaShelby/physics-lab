// 0.59.0: Vulkan, step 1 of 5 (the Vulkan plan, done in 0.69.0 (git keeps it)): the window's Vulkan start and its frames. The instance (through volk: the PC's own Vulkan loader,
// found at run time), SDL3's surface, the device (a discrete card first), the swapchain (present mode by the same VSync rule as the OpenGL path: on =
// wait for the screen, FIFO; off = do not wait: IMMEDIATE, else MAILBOX, else FIFO), Dear ImGui's own Vulkan backend (the same version as the OpenGL one)
// and one small pipeline of our own (render/vk/fill.*: the picture area's background; the SPIR-V is made at build time). Every step is logged by the
// Vulkan guard (app/gpu_guard.h gpuVulkanGuard) and every VkResult is checked; a step that fails is kept by the guard and the app starts on OpenGL.
// 0.60.0 (step 2): the labs' pictures: renderer2d drawn by render/vk_draw2d.* into the picture area (initPicture, render(..., pic)).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SDL_Window;
struct ImDrawData;
namespace app { class GpuGuard; }

namespace render {

class Renderer2D;

class VkBackend {
public:
    struct Options {
        bool validation = false;     // the Khronos validation layers (PL_VK_VALIDATION=1: the tests); each message is counted and printed
        bool vsync = true;
        std::string failAt;          // tests (PL_VK_FAIL=<step>): that step returns an error, as a driver that says no would
    };
    VkBackend();
    ~VkBackend();
    VkBackend(const VkBackend&) = delete;
    VkBackend& operator=(const VkBackend&) = delete;

    // instance, surface, device, swapchain, pipelines (Dear ImGui's and ours): each a guarded step. Needs the Dear ImGui context. False: why says it
    // (the guard keeps it as "fail"); call shutdown() then.
    bool init(SDL_Window* window, app::GpuGuard& guard, const Options& o, std::string& why);
    void shutdown();                                         // everything made, in reverse (safe after a failed init)
    // 0.60.0 (step 2): the labs' pictures (renderer2d on Vulkan, render/vk_draw2d.h), three guarded steps: "renderer2d pipelines" (OpenGL's shaders as
    // SPIR-V, the passes and pipelines), "renderer2d textures" (the maps with their mip levels), "renderer2d check" (a tiny known picture drawn off screen
    // and read back against the values its shaders' formulas give; breakCheck: drawn wrong on purpose, PL_VK_CHECK=break). msaa: 4 samples where the card
    // has them (PL_NO_MSAA: 1). False: why says it (kept by the guard as "fail"; call shutdown() then and start on OpenGL).
    bool initPicture(Renderer2D& r, bool msaa, bool breakCheck, std::string& why);
    std::string pictureText() const;                         // the samples and the cloud layer's format ("" before initPicture)
    // 0.61.0 (step 3): the card programs on Vulkan (render/vk_card.h): the renderer then makes the superposition and jump groups, the outline, the
    // rocks and the Glow dots, each under its own guard (app/gpu_guard.h); why: a line for each that could not be made. False: none at all.
    bool initCards(Renderer2D& r, std::vector<std::string>& why);
    bool float64() const;                                    // the device has double precision in its shaders (shaderFloat64; PL_VK_NO_FLOAT64: no)
    std::size_t cardSpirvBytes() const;
    void newFrame();                                         // ImGui_ImplVulkan_NewFrame
    // the frame: cleared to clear[] (the theme's page colour), the picture area (pixels, from the top left) filled by our pipeline with fill[], then
    // Dear ImGui's draw data. False: nothing drawn this frame (the swapchain is being made again, or the window has no size)
    // 0.60.0: pic: the lab's picture (Renderer2D::endVulkan, cleared to clear[]) drawn there in place of the fill
    bool render(ImDrawData* dd, const float clear[4], const float fill[4], int px, int py, int pw, int ph, Renderer2D* pic = nullptr);
    void present();                                          // after render() (does nothing when render() drew nothing)
    // the frame just drawn (after render, before present), RGBA bytes, top row first; false when it cannot be read
    bool readback(std::vector<uint8_t>& rgba, int& w, int& h);
    // Dear ImGui's draw data drawn into an image of its own (w x h, cleared to clear[]), RGB bytes, top row first (📷 Save image)
    // 0.60.0: pic: the lab's picture under the text (one sample, as OpenGL's offscreen picture)
    bool renderToImage(ImDrawData* dd, int w, int h, const float clear[4], std::vector<uint8_t>& rgb, Renderer2D* pic = nullptr);
    void setVsync(bool on);                                  // takes effect at the next frame (the swapchain is made again)
    // 0.63.0 (step 5): the benchmark's timer on Vulkan: timestamp queries at the start and the end of the frame's command buffer and of each card batch
    // (render/vk_card.h setTiming). benchTimer(true): false when this queue has no timestamps. benchMark(): the next render() is timed (false: every
    // slot still waits for its result: that frame is not measured). benchCollect: the frames whose times have come back, oldest first: the card's busy
    // time (the frame's command buffer plus the batches submitted since the frame before, summed) and the span from the first start to the last end (ms).
    bool benchTimer(bool on);
    bool benchMark();
    void benchCollect(std::vector<std::pair<double, double>>& busySpanMs);
    // 0.63.1: frames whose timestamps were thrown away (a zero, an end before its start, or over 10 s: the owner's 0.63.0 run gave about 1e12 ms a frame)
    int benchRejected() const;
    // the timer's check (as OpenGL's GL_TIME_ELAPSED check): pic's picture drawn once into an image of its own, timed by the timestamps and by a clock
    // from the submit to the end of the wait. False: not timed.
    bool benchLoad(Renderer2D& pic, int w, int h, double& gpuMs, double& wallMs);

    const std::string& deviceName() const { return deviceName_; }
    const std::string& driverText() const { return driverText_; }   // "<driver name> <version>" as the driver reports it
    const std::string& apiText() const { return apiText_; }
    const std::string& presentText() const { return presentText_; }
    int validationErrors() const;
    int validationWarnings() const;
    bool ok() const { return ok_; }                          // init succeeded and no call failed since
    struct Impl;
private:
    Impl* d_;
    bool ok_ = false;
    std::string deviceName_, driverText_, apiText_, presentText_;
};

}  // namespace render
