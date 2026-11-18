// 0.59.0: Vulkan, step 1 of 5 (native/PLAN_VULKAN.md): the window's Vulkan start and its frames. The instance (through volk: the PC's own Vulkan loader,
// found at run time), SDL3's surface, the device (a discrete card first), the swapchain (present mode by the same VSync rule as the OpenGL path: on =
// wait for the screen, FIFO; off = do not wait: IMMEDIATE, else MAILBOX, else FIFO), Dear ImGui's own Vulkan backend (the same version as the OpenGL one)
// and one small pipeline of our own (render/vk/fill.*: the picture area's background; the SPIR-V is made at build time). Every step is logged by the
// Vulkan guard (app/gpu_guard.h gpuVulkanGuard) and every VkResult is checked; a step that fails is kept by the guard and the app starts on OpenGL.
// The labs' pictures are not drawn on Vulkan in this step (renderer2d comes in step 2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
struct ImDrawData;
namespace app { class GpuGuard; }

namespace render {

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
    void newFrame();                                         // ImGui_ImplVulkan_NewFrame
    // the frame: cleared to clear[] (the theme's page colour), the picture area (pixels, from the top left) filled by our pipeline with fill[], then
    // Dear ImGui's draw data. False: nothing drawn this frame (the swapchain is being made again, or the window has no size)
    bool render(ImDrawData* dd, const float clear[4], const float fill[4], int px, int py, int pw, int ph);
    void present();                                          // after render() (does nothing when render() drew nothing)
    // the frame just drawn (after render, before present), RGBA bytes, top row first; false when it cannot be read
    bool readback(std::vector<uint8_t>& rgba, int& w, int& h);
    // Dear ImGui's draw data drawn into an image of its own (w x h, cleared to clear[]), RGB bytes, top row first (📷 Save image)
    bool renderToImage(ImDrawData* dd, int w, int h, const float clear[4], std::vector<uint8_t>& rgb);
    void setVsync(bool on);                                  // takes effect at the next frame (the swapchain is made again)

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
