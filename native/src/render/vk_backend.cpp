// 0.59.0: Vulkan, step 1 of 5 (render/vk_backend.h, the Vulkan plan, done in 0.69.0 (git keeps it))
#include "app/frame_cost.h"
#include "render/vk_backend.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <volk.h>                 // (its functions are compiled once, in volk's own library)

#include "app/gpu_guard.h"
#include "imgui.h"
#include "render/renderer2d.h"
#include "render/vk_card.h"
#include "render/vk_draw2d.h"
#include "imgui_impl_vulkan.h"

// the SPIR-V of render/vk/fill.vert and fill.frag, made at build time by glslang (CMakeLists.txt: PL_VK_SHADERS)
#include "fill_vert.h"
#include "fill_frag.h"

namespace render {

namespace {

const char* resultName(VkResult r) {
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    default: return "another VkResult";
    }
}
std::string resultText(VkResult r) { return std::string(resultName(r)) + " (" + std::to_string(static_cast<int>(r)) + ")"; }

// Dear ImGui's backend reports its own calls' results here (CheckVkResultFn has no user pointer: one backend at a time)
VkBackend::Impl* g_impl = nullptr;
void checkFromImGui(VkResult r);

// the driver's version as the vendors write it (VkPhysicalDeviceProperties::driverVersion is packed differently by NVIDIA and Intel on Windows)
std::string driverVersionText(uint32_t vendor, uint32_t v) {
    char b[64];
    if (vendor == 0x10DE) std::snprintf(b, sizeof b, "%u.%u.%u.%u", (v >> 22) & 0x3ffu, (v >> 14) & 0xffu, (v >> 6) & 0xffu, v & 0x3fu);   // NVIDIA
#ifdef _WIN32
    else if (vendor == 0x8086) std::snprintf(b, sizeof b, "%u.%u", v >> 14, v & 0x3fffu);                                                // Intel on Windows
#endif
    else std::snprintf(b, sizeof b, "%u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
    return b;
}
const std::string kNeeded = "Vulkan 1.3, or 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2";   // 0.69.0 (owner decision 20 Nov 2026)
std::string versionText(uint32_t v) { return std::to_string(VK_API_VERSION_MAJOR(v)) + "." + std::to_string(VK_API_VERSION_MINOR(v)) + "." + std::to_string(VK_API_VERSION_PATCH(v)); }
const char* typeName(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "processor (software)";
    default: return "other";
    }
}
int typeScore(VkPhysicalDeviceType t) {   // a discrete card first, then the one in the processor's package, then the rest
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
    default: return 0;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL onDebug(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);

}  // namespace

struct VkBackend::Impl {
    app::GpuGuard* guard = nullptr;
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Window wd;
    VkImageUsageFlags extraUsage = 0;          // TRANSFER_SRC when the surface allows it (readback)
    uint32_t minImages = 2;
    uint32_t apiVersion = VK_API_VERSION_1_0;
    bool imguiOn = false, rebuild = false, drewFrame = false, vsync = true;
    VkPipelineLayout fillLayout = VK_NULL_HANDLE;
    VkPipeline fill = VK_NULL_HANDLE;
    VkCommandPool oncePool = VK_NULL_HANDLE;   // one-time commands: readback, the picture for 📷 Save image
    VkPhysicalDeviceMemoryProperties mem{};
    VkDraw2D* d2 = nullptr;                    // 0.60.0: the labs' pictures (initPicture)
    VkCards* cards = nullptr;                  // 0.61.0: the card programs (initCards)
    bool float64 = false, computeQueue = false, sync2 = false, khr12 = false;   // (0.69.0: khr12: a Vulkan 1.2 card with the two extensions)
    VkDevCtx ctx;
    // 0.63.0: the benchmark's timer (benchTimer): kBenchSlots frames in flight, two timestamps each, then two for benchLoad
    static constexpr int kBenchSlots = 8;
    VkQueryPool benchPool = VK_NULL_HANDLE;
    bool benchOn = false, benchNext = false, benchImage = false;
    int benchSlot = -1;
    double tsNs = 0, imageGpuMs = -1, imageWallMs = -1;
    std::uint64_t tsMask = ~std::uint64_t(0), benchTicket = 0;
    bool benchUsed[kBenchSlots] = {};
    struct BenchFrame { int slot; std::uint64_t first, last, serial; int image; };
    // 0.63.1: a frame's timestamps are read only once its fence has signalled (serial <= doneSerial): availability alone gave nonsense on the owner's card
    std::uint64_t frameSerial = 0, doneSerial = 0;
    std::vector<std::uint64_t> fenceSerial;                // per swapchain image: the serial of the frame last submitted with its fence
    int benchRejected = 0;
    std::vector<BenchFrame> benchPending;
    int errors = 0, warnings = 0, failedCalls = 0, rebuilds = 0;
    std::string lastFailure;

    bool check(VkResult r, const char* what) {
        if (r == VK_SUCCESS) return true;
        if (r > 0) return true;                // VK_SUBOPTIMAL_KHR, VK_INCOMPLETE: not errors (handled where they matter)
        failedCalls++;
        lastFailure = std::string(what) + ": " + resultText(r);
        std::fprintf(stderr, "[vulkan] %s\n", lastFailure.c_str());
        if (guard && failedCalls <= 20) guard->log("error: " + lastFailure);
        return false;
    }
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags want) const {
        for (uint32_t i = 0; i < mem.memoryTypeCount; i++)
            if ((bits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & want) == want) return i;
        return UINT32_MAX;
    }
    VkPresentModeKHR choosePresent(bool on) const {
        uint32_t n = 0;
        std::vector<VkPresentModeKHR> modes;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &n, nullptr) == VK_SUCCESS && n > 0) {
            modes.resize(n);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &n, modes.data()) < 0) modes.clear();
            modes.resize(n);
        }
        const auto has = [&](VkPresentModeKHR m) { return std::find(modes.begin(), modes.end(), m) != modes.end(); };
        if (!on) {                             // as SDL_GL_SetSwapInterval(0): do not wait for the screen
            if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
            if (has(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
        }
        return VK_PRESENT_MODE_FIFO_KHR;       // as SDL_GL_SetSwapInterval(1); every Vulkan driver has it
    }
    static const char* presentName(VkPresentModeKHR m) {
        return m == VK_PRESENT_MODE_FIFO_KHR ? "FIFO (waits for the screen)" : m == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE (does not wait)" :
               m == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX (does not wait)" : "other";
    }
    bool makeSwapchain(int w, int h) {
        wd.UseDynamicRendering = true;                                    // 0.63.0 (Vulkan step 5): no render pass, no framebuffers (Vulkan 1.3)
        ImGui_ImplVulkanH_CreateOrResizeWindow(instance, phys, device, &wd, family, nullptr, w, h, minImages, extraUsage);
        return wd.Swapchain != VK_NULL_HANDLE && wd.ImageCount >= 2 && wd.Frames.Size == static_cast<int>(wd.ImageCount) && wd.Frames[0].BackbufferView != VK_NULL_HANDLE;
    }
};

namespace {
void checkFromImGui(VkResult r) {
    if (g_impl) g_impl->check(r, "Dear ImGui's Vulkan backend");
}
VKAPI_ATTR VkBool32 VKAPI_CALL onDebug(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    VkBackend::Impl* d = static_cast<VkBackend::Impl*>(user);
    const bool error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    if (d) { if (error) d->errors++; else d->warnings++; }
    const char* msg = data && data->pMessage ? data->pMessage : "(no text)";
    std::fprintf(stderr, "PLVKVALIDATION %s: %s\n", error ? "error" : "warning", msg);
    if (d && d->guard && d->errors + d->warnings <= 10) d->guard->log(std::string("validation ") + (error ? "error: " : "warning: ") + msg);
    return VK_FALSE;
}
}  // namespace

VkBackend::VkBackend() : d_(new Impl) {}
VkBackend::~VkBackend() { shutdown(); delete d_; }

int VkBackend::validationErrors() const { return d_->errors; }
int VkBackend::validationWarnings() const { return d_->warnings; }

bool VkBackend::init(SDL_Window* window, app::GpuGuard& guard, const Options& o, std::string& why) {
    Impl& d = *d_;
    d.guard = &guard;
    d.window = window;
    d.vsync = o.vsync;
    g_impl = &d;
    const auto fail = [&](const std::string& step, const std::string& text) {
        why = text;
        guard.failed(step, text);
        return false;
    };
    const auto forced = [&](const char* step) { return !o.failAt.empty() && o.failAt == step; };

    // ---- instance ----
    guard.begin("instance");
    PFN_vkGetInstanceProcAddr gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!gipa) return fail("instance", std::string("no Vulkan loader on this PC (") + SDL_GetError() + ")");
    volkInitializeCustom(gipa);
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion && vkEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) loaderVersion = VK_API_VERSION_1_0;
    // 0.62.0 (owner decision 18 Nov 2026): dynamic rendering and synchronization2 are needed (the Black Hole Lab's passes, and since 0.63.0 every pass);
    // 0.69.0 (owner decision 20 Nov 2026): Vulkan 1.3 or 1.4 (asked for where the loader has it), or 1.2 with VK_KHR_dynamic_rendering and
    // VK_KHR_synchronization2 (the card's extensions, queried below); below that the app says what is needed (main.cpp noVulkan)
    d.apiVersion = std::min<uint32_t>(VK_MAKE_API_VERSION(0, VK_API_VERSION_MAJOR(loaderVersion), VK_API_VERSION_MINOR(loaderVersion), 0), VK_API_VERSION_1_4);
    if (d.apiVersion < VK_API_VERSION_1_2)
        return fail("instance", "this PC's Vulkan loader offers Vulkan " + std::to_string(VK_API_VERSION_MAJOR(loaderVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(loaderVersion)) +
                                "; Physics Lab needs " + kNeeded);
    std::vector<const char*> exts;
    {
        Uint32 n = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&n);
        if (!sdlExts) return fail("instance", std::string("SDL found no Vulkan surface extensions (") + SDL_GetError() + ")");
        for (Uint32 i = 0; i < n; i++) exts.push_back(sdlExts[i]);
    }
    uint32_t extCount = 0;
    std::vector<VkExtensionProperties> have;
    if (!d.check(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr), "vkEnumerateInstanceExtensionProperties")) return fail("instance", d.lastFailure);
    have.resize(extCount);
    if (!d.check(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, have.data()), "vkEnumerateInstanceExtensionProperties")) return fail("instance", d.lastFailure);
    const auto instanceHas = [&](const char* e) { for (const VkExtensionProperties& p : have) if (std::strcmp(p.extensionName, e) == 0) return true; return false; };
    for (const char* e : exts) if (!instanceHas(e)) return fail("instance", std::string("the Vulkan driver lacks the instance extension ") + e);
    std::vector<const char*> layers;
    bool validation = false;
    if (o.validation) {
        uint32_t ln = 0;
        std::vector<VkLayerProperties> lp;
        if (vkEnumerateInstanceLayerProperties(&ln, nullptr) == VK_SUCCESS) { lp.resize(ln); if (vkEnumerateInstanceLayerProperties(&ln, lp.data()) < 0) ln = 0; lp.resize(ln); }
        for (const VkLayerProperties& l : lp) if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) validation = true;
        if (validation && instanceHas(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) { layers.push_back("VK_LAYER_KHRONOS_validation"); exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }
        else { validation = false; guard.log("validation layers asked for (PL_VK_VALIDATION) but not installed: none"); std::fprintf(stderr, "PLVKVALIDATION missing\n"); }
    }
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "Physics Lab";
    ai.applicationVersion = 1;
    ai.pEngineName = "Physics Lab";
    ai.engineVersion = 1;
    ai.apiVersion = d.apiVersion;
    VkDebugUtilsMessengerCreateInfoEXT dm{};
    dm.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dm.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dm.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dm.pfnUserCallback = onDebug;
    dm.pUserData = &d;
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pNext = validation ? &dm : nullptr;   // also the messages of vkCreateInstance itself
    ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.data();
    if (forced("instance")) return fail("instance", "made to fail on purpose (PL_VK_FAIL=instance)");
    if (!d.check(vkCreateInstance(&ci, nullptr, &d.instance), "vkCreateInstance")) { d.instance = VK_NULL_HANDLE; return fail("instance", d.lastFailure); }
    volkLoadInstance(d.instance);
    if (validation && vkCreateDebugUtilsMessengerEXT && !d.check(vkCreateDebugUtilsMessengerEXT(d.instance, &dm, nullptr, &d.messenger), "vkCreateDebugUtilsMessengerEXT"))
        return fail("instance", d.lastFailure);
    guard.end("instance", "Vulkan loader " + versionText(loaderVersion) + ", asked for " + versionText(d.apiVersion) + (validation ? ", validation layers on" : ""));

    // ---- surface (SDL3: Win32, X11, Wayland, or the headless one of the automatic checks) ----
    guard.begin("surface");
    if (forced("surface")) return fail("surface", "made to fail on purpose (PL_VK_FAIL=surface)");
    if (!SDL_Vulkan_CreateSurface(window, d.instance, nullptr, &d.surface)) { d.surface = VK_NULL_HANDLE; return fail("surface", std::string("SDL_Vulkan_CreateSurface: ") + SDL_GetError()); }
    guard.end("surface", SDL_GetCurrentVideoDriver() ? std::string("video driver ") + SDL_GetCurrentVideoDriver() : std::string());

    // ---- device: every card that can draw on this window and present; a discrete card first ----
    guard.begin("device");
    uint32_t nDev = 0;
    if (!d.check(vkEnumeratePhysicalDevices(d.instance, &nDev, nullptr), "vkEnumeratePhysicalDevices")) return fail("device", d.lastFailure);
    std::vector<VkPhysicalDevice> devs(nDev);
    if (nDev && !d.check(vkEnumeratePhysicalDevices(d.instance, &nDev, devs.data()), "vkEnumeratePhysicalDevices")) return fail("device", d.lastFailure);
    devs.resize(nDev);
    int best = -1, bestScore = -1;
    uint32_t bestFamily = 0;
    // PL_VK_PRETEND_API=1.2 (tests): every card is taken to offer that version, as an older driver would
    uint32_t pretend = 0;
    if (const char* pa = std::getenv("PL_VK_PRETEND_API")) { unsigned ma = 0, mi = 0; if (std::sscanf(pa, "%u.%u", &ma, &mi) == 2) pretend = VK_MAKE_API_VERSION(0, ma, mi, 0); }
    std::string oldCard;                                                  // the newest version a card that cannot be used offers (the reason when no card is left)
    uint32_t oldVersion = 0;
    bool oldKhr[2] = {false, false};                                      // 0.69.0: that card's VK_KHR_dynamic_rendering, VK_KHR_synchronization2
    bool bestKhr = false;                                                 // the chosen card is a 1.2 one, with the two extensions
    for (std::size_t i = 0; i < devs.size(); i++) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(devs[i], &p);
        uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &nq, qf.data());
        int family = -1;
        for (uint32_t q = 0; q < nq && family < 0; q++) {
            VkBool32 present = VK_FALSE;
            if ((qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], q, d.surface, &present) == VK_SUCCESS && present == VK_TRUE) family = static_cast<int>(q);
        }
        uint32_t ne = 0;
        bool swap = false, khrDr = false, khrS2 = false;
        if (vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &ne, nullptr) == VK_SUCCESS) {
            std::vector<VkExtensionProperties> de(ne);
            if (vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &ne, de.data()) >= 0)
                for (uint32_t k = 0; k < ne; k++) {
                    if (std::strcmp(de[k].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) swap = true;
                    if (std::strcmp(de[k].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) khrDr = true;
                    if (std::strcmp(de[k].extensionName, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0) khrS2 = true;
                }
        }
        if (const char* nx = std::getenv("PL_VK_PRETEND_NOEXT")) { if (std::strstr(nx, "dynamic_rendering")) khrDr = false; if (std::strstr(nx, "synchronization2")) khrS2 = false; }   // (tests)
        const uint32_t api = pretend ? pretend : p.apiVersion;
        const uint32_t apiMm = VK_MAKE_API_VERSION(0, VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), 0);
        const bool new13 = apiMm >= VK_API_VERSION_1_3;
        const bool khr12 = !new13 && apiMm >= VK_API_VERSION_1_2 && d.apiVersion >= VK_API_VERSION_1_2 && khrDr && khrS2;   // 0.69.0: 1.2 with the two extensions
        const bool usable = family >= 0 && swap && (new13 || khr12);
        if (!usable && family >= 0 && swap && api >= oldVersion) { oldVersion = api; oldCard = p.deviceName; oldKhr[0] = khrDr; oldKhr[1] = khrS2; }
        guard.log(std::string("card ") + std::to_string(i) + ": " + p.deviceName + " (" + typeName(p.deviceType) + ", Vulkan " + versionText(api) + (pretend ? " (PL_VK_PRETEND_API)" : "") +
                  ", driver " + driverVersionText(p.vendorID, p.driverVersion) + (new13 ? "" : std::string(", VK_KHR_dynamic_rendering ") + (khrDr ? "yes" : "no") + ", VK_KHR_synchronization2 " + (khrS2 ? "yes" : "no")) + ")" +
                  (usable ? (khr12 ? ": Vulkan 1.2 with the two extensions" : "") : family < 0 ? ": cannot draw on this window" : !swap ? ": no swapchain" : ": cannot be used (" + kNeeded + ")"));
        if (usable && typeScore(p.deviceType) > bestScore) { best = static_cast<int>(i); bestScore = typeScore(p.deviceType); bestFamily = static_cast<uint32_t>(family); bestKhr = khr12; }
    }
    if (forced("device")) return fail("device", "made to fail on purpose (PL_VK_FAIL=device)");
    if (best < 0 && !oldCard.empty())
        return fail("device", "this graphics card offers Vulkan " + std::to_string(VK_API_VERSION_MAJOR(oldVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(oldVersion)) +
                              (VK_API_VERSION_MINOR(oldVersion) == 2 && VK_API_VERSION_MAJOR(oldVersion) == 1 ? std::string(oldKhr[0] ? "" : " without VK_KHR_dynamic_rendering") +
                                   (oldKhr[1] ? "" : (oldKhr[0] ? " without" : " and") + std::string(" VK_KHR_synchronization2")) : std::string()) +
                              "; Physics Lab needs " + kNeeded + " (" + oldCard + ")");
    if (best < 0) return fail("device", nDev ? "no graphics card here can draw on this window with Vulkan" : "the Vulkan driver lists no graphics card");
    d.phys = devs[static_cast<std::size_t>(best)];
    d.family = bestFamily;
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d.phys, &p);
        deviceName_ = p.deviceName;
        driverText_ = driverVersionText(p.vendorID, p.driverVersion);
        apiText_ = versionText(p.apiVersion);
        if (vkGetPhysicalDeviceProperties2 && p.apiVersion >= VK_API_VERSION_1_2 && d.apiVersion >= VK_API_VERSION_1_1) {   // the driver's own name and text (Vulkan 1.2)
            VkPhysicalDeviceDriverProperties dp{};
            dp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 p2{};
            p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            p2.pNext = &dp;
            vkGetPhysicalDeviceProperties2(d.phys, &p2);
            if (dp.driverName[0]) driverText_ = std::string(dp.driverName) + " " + driverText_ + (dp.driverInfo[0] ? std::string(" (") + dp.driverInfo + ")" : std::string());
        }
        vkGetPhysicalDeviceMemoryProperties(d.phys, &d.mem);
    }
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = d.family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    const char* devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    const uint32_t nDevExts = bestKhr ? 3u : 1u;                         // 0.69.0: the two extensions only on a 1.2 card (where they are present)
    // 0.61.0 (step 3): double precision in shaders where the card has it (the belts' rocks: kRocksCS); PL_VK_NO_FLOAT64 (tests) pretends it has not
    VkPhysicalDeviceFeatures feat{}, want{};
    vkGetPhysicalDeviceFeatures(d.phys, &feat);
    d.float64 = feat.shaderFloat64 == VK_TRUE && !std::getenv("PL_VK_NO_FLOAT64");
    want.shaderFloat64 = d.float64 ? VK_TRUE : VK_FALSE;
    {
        uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nq, qf.data());
        d.computeQueue = d.family < nq && (qf[d.family].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
    }
    // 0.62.0: Vulkan 1.3's dynamic rendering and synchronization2 (the Black Hole Lab's passes, render/vk_blackhole.cpp), as queried; 0.69.0: on a 1.2
    // card the same two features from their extensions (VK_KHR_dynamic_rendering, VK_KHR_synchronization2: their own feature structures, queried)
    VkPhysicalDeviceVulkan13Features have13{}, want13{};
    have13.sType = want13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR haveDr{}, wantDr{};
    haveDr.sType = wantDr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    VkPhysicalDeviceSynchronization2FeaturesKHR haveS2{}, wantS2{};
    haveS2.sType = wantS2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 f2{};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    if (bestKhr) { haveDr.pNext = &haveS2; f2.pNext = &haveDr; }
    else f2.pNext = &have13;
    vkGetPhysicalDeviceFeatures2(d.phys, &f2);
    const bool dr = bestKhr ? haveDr.dynamicRendering == VK_TRUE : have13.dynamicRendering == VK_TRUE;
    const bool s2 = bestKhr ? haveS2.synchronization2 == VK_TRUE : have13.synchronization2 == VK_TRUE;
    if (!dr || !s2) return fail("device", std::string("this graphics card's Vulkan ") + (bestKhr ? "1.2 extensions do" : "1.3 does") + " not offer dynamic rendering or synchronization2");
    void* featChain = nullptr;
    if (bestKhr) { wantDr.dynamicRendering = VK_TRUE; wantS2.synchronization2 = VK_TRUE; wantDr.pNext = &wantS2; featChain = &wantDr; }
    else { want13.dynamicRendering = VK_TRUE; want13.synchronization2 = VK_TRUE; featChain = &want13; }
    d.sync2 = true;
    d.khr12 = bestKhr;
    VkDeviceCreateInfo di{};
    di.pNext = featChain;
    di.pEnabledFeatures = &want;
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = nDevExts;
    di.ppEnabledExtensionNames = devExts;
    if (!d.check(vkCreateDevice(d.phys, &di, nullptr, &d.device), "vkCreateDevice")) { d.device = VK_NULL_HANDLE; return fail("device", d.lastFailure); }
    volkLoadDevice(d.device);
    if (bestKhr) {                                     // 0.69.0: on 1.2 the extensions' functions (their KHR names) in the core names' places, which every pass calls
        vkCmdBeginRendering = vkCmdBeginRenderingKHR; vkCmdEndRendering = vkCmdEndRenderingKHR;
        vkCmdPipelineBarrier2 = vkCmdPipelineBarrier2KHR; vkQueueSubmit2 = vkQueueSubmit2KHR; vkCmdWriteTimestamp2 = vkCmdWriteTimestamp2KHR;
        vkCmdSetEvent2 = vkCmdSetEvent2KHR; vkCmdResetEvent2 = vkCmdResetEvent2KHR; vkCmdWaitEvents2 = vkCmdWaitEvents2KHR;
        if (!vkCmdBeginRendering || !vkCmdEndRendering || !vkCmdPipelineBarrier2 || !vkQueueSubmit2 || !vkCmdWriteTimestamp2)
            return fail("device", "this graphics card's driver lists VK_KHR_dynamic_rendering and VK_KHR_synchronization2 but not their functions");
    }
    vkGetDeviceQueue(d.device, d.family, 0, &d.queue);
    guard.end("device", deviceName_ + ", driver " + driverText_ + ", Vulkan " + apiText_ + " (the card's; asked for " + versionText(d.apiVersion) +
                        (bestKhr ? ", dynamic rendering and synchronization2 on from VK_KHR_dynamic_rendering and VK_KHR_synchronization2" : ", dynamic rendering and synchronization2 on") + "), queue family " + std::to_string(d.family) +
                        (d.computeQueue ? " (graphics and compute)" : " (graphics; no compute)") + ", double precision in shaders (shaderFloat64): " +
                        (d.float64 ? "yes" : feat.shaderFloat64 ? "switched off (PL_VK_NO_FLOAT64)" : "no"));

    // ---- swapchain ----
    guard.begin("swapchain");
    VkSurfaceCapabilitiesKHR caps{};
    if (!d.check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(d.phys, d.surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")) return fail("swapchain", d.lastFailure);
    // 0.69.0: the lab's picture is copied in by a blit (its rows turned over; present.frag is gone), so the window's images take transfers: queried
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) return fail("swapchain", "the window's images cannot be written by a copy (no TRANSFER_DST usage)");
    d.extraUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | ((caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
    const VkFormat wantFormats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};   // not *_SRGB: the colours go to the screen as written
    d.wd.Surface = d.surface;
    d.wd.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(d.phys, d.surface, wantFormats, 2, VK_COLORSPACE_SRGB_NONLINEAR_KHR);
    if (d.wd.SurfaceFormat.format != VK_FORMAT_B8G8R8A8_UNORM && d.wd.SurfaceFormat.format != VK_FORMAT_R8G8B8A8_UNORM)
        return fail("swapchain", "the window offers no 8-bit RGBA format without sRGB conversion (format " + std::to_string(static_cast<int>(d.wd.SurfaceFormat.format)) + ")");
    d.wd.PresentMode = d.choosePresent(d.vsync);
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (forced("swapchain")) return fail("swapchain", "made to fail on purpose (PL_VK_FAIL=swapchain)");
    if (w <= 0 || h <= 0 || !d.makeSwapchain(w, h)) return fail("swapchain", "the swapchain could not be made (" + std::to_string(w) + " x " + std::to_string(h) + ")");
    presentText_ = Impl::presentName(d.wd.PresentMode);
    guard.end("swapchain", std::to_string(d.wd.Width) + " x " + std::to_string(d.wd.Height) + ", " + std::to_string(d.wd.ImageCount) + " images, " +
              (d.wd.SurfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM ? "B8G8R8A8_UNORM" : "R8G8B8A8_UNORM") + ", " + presentText_ +
              ((d.extraUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ? ", can be read back" : ", cannot be read back (no screenshots)"));

    // ---- pipelines: Dear ImGui's (its own SPIR-V) and ours (render/vk/fill.*) ----
    guard.begin("pipelines");
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE},
                                    {VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE}};
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.maxSets = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;
    if (!d.check(vkCreateDescriptorPool(d.device, &pi, nullptr, &d.pool), "vkCreateDescriptorPool")) { d.pool = VK_NULL_HANDLE; return fail("pipelines", d.lastFailure); }
    ImGui_ImplVulkan_InitInfo ii{};
    ii.ApiVersion = d.apiVersion;
    ii.Instance = d.instance;
    ii.PhysicalDevice = d.phys;
    ii.Device = d.device;
    ii.QueueFamily = d.family;
    ii.Queue = d.queue;
    ii.DescriptorPool = d.pool;
    ii.MinImageCount = d.minImages;
    ii.ImageCount = d.wd.ImageCount;
    ii.UseDynamicRendering = true;                                       // 0.63.0: Dear ImGui's pipeline for dynamic rendering into the window's format
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &d.wd.SurfaceFormat.format;
    ii.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ii.CheckVkResultFn = checkFromImGui;
    const int failed0 = d.failedCalls;
    if (!ImGui_ImplVulkan_Init(&ii) || d.failedCalls != failed0) {
        if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplVulkan_Shutdown();
        return fail("pipelines", "Dear ImGui's Vulkan backend did not start" + (d.lastFailure.empty() ? std::string() : " (" + d.lastFailure + ")"));
    }
    d.imguiOn = true;
    {   // ours: one triangle over the target, one colour from the push constant; viewport and scissor set when drawn
        VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        si.codeSize = sizeof k_fill_vert;
        si.pCode = k_fill_vert;
        if (!d.check(vkCreateShaderModule(d.device, &si, nullptr, &vs), "vkCreateShaderModule (fill.vert)")) return fail("pipelines", d.lastFailure);
        si.codeSize = sizeof k_fill_frag;
        si.pCode = k_fill_frag;
        if (!d.check(vkCreateShaderModule(d.device, &si, nullptr, &fs), "vkCreateShaderModule (fill.frag)")) { vkDestroyShaderModule(d.device, vs, nullptr); return fail("pipelines", d.lastFailure); }
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size = 4 * sizeof(float);
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        bool ok = d.check(vkCreatePipelineLayout(d.device, &li, nullptr, &d.fillLayout), "vkCreatePipelineLayout (fill)");
        if (ok) {
            VkPipelineShaderStageCreateInfo st[2]{};
            st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = vs; st[0].pName = "main";
            st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = fs; st[1].pName = "main";
            VkPipelineVertexInputStateCreateInfo vi{};
            vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VkPipelineInputAssemblyStateCreateInfo ia{};
            ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{};
            vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            vp.viewportCount = 1;
            vp.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo rs{};
            rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode = VK_CULL_MODE_NONE;
            rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rs.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo ms{};
            ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState ba{};
            ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo cb{};
            cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            cb.attachmentCount = 1;
            cb.pAttachments = &ba;
            const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo ds{};
            ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            ds.dynamicStateCount = 2;
            ds.pDynamicStates = dyn;
            VkGraphicsPipelineCreateInfo gi{};
            gi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            gi.stageCount = 2;
            gi.pStages = st;
            gi.pVertexInputState = &vi;
            gi.pInputAssemblyState = &ia;
            gi.pViewportState = &vp;
            gi.pRasterizationState = &rs;
            gi.pMultisampleState = &ms;
            gi.pColorBlendState = &cb;
            gi.pDynamicState = &ds;
            gi.layout = d.fillLayout;
            VkPipelineRenderingCreateInfo ri{};                              // 0.63.0: dynamic rendering into the window's format
            ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            ri.colorAttachmentCount = 1;
            ri.pColorAttachmentFormats = &d.wd.SurfaceFormat.format;
            gi.pNext = &ri;
            ok = d.check(vkCreateGraphicsPipelines(d.device, VK_NULL_HANDLE, 1, &gi, nullptr, &d.fill), "vkCreateGraphicsPipelines (fill)");
        }
        vkDestroyShaderModule(d.device, vs, nullptr);
        vkDestroyShaderModule(d.device, fs, nullptr);
        if (!ok) { d.fill = VK_NULL_HANDLE; return fail("pipelines", d.lastFailure); }
    }
    VkCommandPoolCreateInfo cpi{};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = d.family;
    if (!d.check(vkCreateCommandPool(d.device, &cpi, nullptr, &d.oncePool), "vkCreateCommandPool")) { d.oncePool = VK_NULL_HANDLE; return fail("pipelines", d.lastFailure); }
    if (forced("pipelines")) return fail("pipelines", "made to fail on purpose (PL_VK_FAIL=pipelines)");
    guard.end("pipelines", std::string("Dear ImGui's and the picture area's (SPIR-V ") + std::to_string(sizeof k_fill_vert + sizeof k_fill_frag) + " bytes)");
    ok_ = true;
    return true;
}

void VkBackend::shutdown() {
    Impl& d = *d_;
    if (d.device) vkDeviceWaitIdle(d.device);
    if (d.cards) { delete d.cards; d.cards = nullptr; }   // 0.61.0 (the programs made from it are gone: Renderer2D::useVulkan(nullptr))
    if (d.benchPool && d.device) { vkDestroyQueryPool(d.device, d.benchPool, nullptr); d.benchPool = VK_NULL_HANDLE; }   // 0.63.0
    d.benchOn = false; d.benchPending.clear();
    if (d.d2) { delete d.d2; d.d2 = nullptr; }  // 0.60.0 (its destructor frees what it made)
    if (d.imguiOn) { ImGui_ImplVulkan_Shutdown(); d.imguiOn = false; }
    if (d.device) {
        if (d.oncePool) vkDestroyCommandPool(d.device, d.oncePool, nullptr);
        if (d.fill) vkDestroyPipeline(d.device, d.fill, nullptr);
        if (d.fillLayout) vkDestroyPipelineLayout(d.device, d.fillLayout, nullptr);
        if (d.wd.Swapchain || d.wd.RenderPass) ImGui_ImplVulkanH_DestroyWindow(d.instance, d.device, &d.wd, nullptr);
        if (d.pool) vkDestroyDescriptorPool(d.device, d.pool, nullptr);
        vkDestroyDevice(d.device, nullptr);
    }
    d.oncePool = VK_NULL_HANDLE; d.fill = VK_NULL_HANDLE; d.fillLayout = VK_NULL_HANDLE; d.pool = VK_NULL_HANDLE; d.device = VK_NULL_HANDLE;
    d.wd = ImGui_ImplVulkanH_Window();
    if (d.instance) {
        if (d.surface) vkDestroySurfaceKHR(d.instance, d.surface, nullptr);
        if (d.messenger && vkDestroyDebugUtilsMessengerEXT) vkDestroyDebugUtilsMessengerEXT(d.instance, d.messenger, nullptr);
        vkDestroyInstance(d.instance, nullptr);
    }
    d.surface = VK_NULL_HANDLE; d.messenger = VK_NULL_HANDLE; d.instance = VK_NULL_HANDLE;
    if (g_impl == &d) g_impl = nullptr;
    ok_ = false;
}

void VkBackend::newFrame() { ImGui_ImplVulkan_NewFrame(); }

void VkBackend::setVsync(bool on) {
    Impl& d = *d_;
    if (!d.device || on == d.vsync) return;
    d.vsync = on;
    const VkPresentModeKHR m = d.choosePresent(on);
    if (m != d.wd.PresentMode) { d.wd.PresentMode = m; d.rebuild = true; }
}

bool VkBackend::render(ImDrawData* dd, const float clear[4], const float fill[4], int px, int py, int pw, int ph, Renderer2D* pic) {
    Impl& d = *d_;
    d.drewFrame = false;
    if (!d.device) return false;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(d.window, &w, &h);
    if (w <= 0 || h <= 0) return false;                                   // minimised: nothing to draw on
    if (d.rebuild || w != d.wd.Width || h != d.wd.Height) {               // resized, out of date, or VSync changed
        ImGui_ImplVulkan_SetMinImageCount(d.minImages);
        app::FrameCostSpan made(app::FrameCostKind::Swapchain);   // (0.63.1: the benchmark's worst frames)
        if (!d.makeSwapchain(w, h)) { d.check(VK_ERROR_INITIALIZATION_FAILED, "the swapchain could not be made again"); ok_ = false; return false; }
        presentText_ = Impl::presentName(d.wd.PresentMode);
        if (d.guard && ++d.rebuilds <= 50) d.guard->log("swapchain made again: " + std::to_string(d.wd.Width) + " x " + std::to_string(d.wd.Height) + ", " + presentText_);
        d.wd.FrameIndex = 0;
        d.rebuild = false;
        d.doneSerial = d.frameSerial;                                     // (0.63.1: the swapchain is made after the device is idle: every frame done)
        d.fenceSerial.assign(static_cast<std::size_t>(d.wd.ImageCount), 0);
    }
    ImGui_ImplVulkanH_Window& wd = d.wd;
    VkSemaphore acquired = wd.FrameSemaphores[static_cast<int>(wd.SemaphoreIndex)].ImageAcquiredSemaphore;
    VkSemaphore complete = wd.FrameSemaphores[static_cast<int>(wd.SemaphoreIndex)].RenderCompleteSemaphore;
    VkResult r = VK_SUCCESS;
    {   app::FrameCostSpan span(app::FrameCostKind::Acquire);   // (0.63.1: counted apart from the frame's own work)
        r = vkAcquireNextImageKHR(d.device, wd.Swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &wd.FrameIndex); }
    if (r == VK_ERROR_OUT_OF_DATE_KHR) { d.rebuild = true; return false; }
    if (r == VK_SUBOPTIMAL_KHR) d.rebuild = true;                         // drawn and shown, then made again
    else if (!d.check(r, "vkAcquireNextImageKHR")) { ok_ = false; return false; }
    ImGui_ImplVulkanH_Frame& fd = wd.Frames[static_cast<int>(wd.FrameIndex)];
    {   app::FrameCostSpan span(app::FrameCostKind::CardWait);   // (0.63.1: the frame that used this image before: waiting for the card)
        if (!d.check(vkWaitForFences(d.device, 1, &fd.Fence, VK_TRUE, UINT64_MAX), "vkWaitForFences")) { ok_ = false; return false; } }
    if (d.fenceSerial.size() < static_cast<std::size_t>(wd.ImageCount)) d.fenceSerial.resize(static_cast<std::size_t>(wd.ImageCount), 0);
    d.doneSerial = std::max(d.doneSerial, d.fenceSerial[static_cast<std::size_t>(wd.FrameIndex)]);   // (0.63.1)
    if (!d.check(vkResetFences(d.device, 1, &fd.Fence), "vkResetFences")) { ok_ = false; return false; }
    if (!d.check(vkResetCommandPool(d.device, fd.CommandPool, 0), "vkResetCommandPool")) { ok_ = false; return false; }
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!d.check(vkBeginCommandBuffer(fd.CommandBuffer, &bi), "vkBeginCommandBuffer")) { ok_ = false; return false; }
    const int timed = d.benchOn && d.benchNext ? d.benchSlot : -1;       // 0.63.0: the benchmark's timer: the frame's start
    d.benchNext = false;
    if (timed >= 0) {
        vkCmdResetQueryPool(fd.CommandBuffer, d.benchPool, static_cast<uint32_t>(2 * timed), 2);
        vkCmdWriteTimestamp2(fd.CommandBuffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, d.benchPool, static_cast<uint32_t>(2 * timed));
    }
    bool pictured = false;
    if (pic && d.d2 && pw > 0 && ph > 0) {                                // 0.60.0: the lab's picture first, in passes of its own (render/vk_draw2d.cpp)
        d.d2->beginFrame(static_cast<int>(wd.FrameIndex), static_cast<int>(wd.ImageCount), fd.CommandBuffer);
        pic->endVulkan(pw, ph, clear, false);
        if (d.d2->failed()) { d.check(VK_ERROR_UNKNOWN, "renderer2d (the lab's picture)"); ok_ = false; }
        pictured = d.d2->pictureDrawn();
    }
    VkClearValue cv{};
    for (int i = 0; i < 4; i++) cv.color.float32[i] = clear[i];
    const int x0 = std::clamp(px, 0, wd.Width), y0 = std::clamp(py, 0, wd.Height), x1 = std::clamp(px + pw, 0, wd.Width), y1 = std::clamp(py + ph, 0, wd.Height);
    static const bool noCopy = std::getenv("PL_VK_NOCOPY") != nullptr;   // 0.61.0 (measuring only): the picture is drawn but not copied into the window
    const bool copy = pictured && x1 > x0 && y1 > y0 && !noCopy;
    // 0.63.0 (step 5): dynamic rendering into the swapchain's image (synchronization2): from the acquire (its semaphore waits at the colour output stage).
    // 0.69.0: with a picture: the image cleared and the picture copied in by transfers first (VkDraw2D::copyTo), then the pass loads them
    if (copy) d.d2->copyTo(fd.CommandBuffer, fd.Backbuffer, cv.color, x0, y0, x1 - x0, y1 - y0);
    else vkImageBarrier2(fd.CommandBuffer, fd.Backbuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderingAttachmentInfo at{};
    at.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    at.imageView = fd.BackbufferView;
    at.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at.loadOp = copy ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at.clearValue = cv;
    VkRenderingInfo rinfo{};
    rinfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rinfo.renderArea.extent = {static_cast<uint32_t>(wd.Width), static_cast<uint32_t>(wd.Height)};
    rinfo.layerCount = 1;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachments = &at;
    vkCmdBeginRendering(fd.CommandBuffer, &rinfo);
    if (!copy && !(pictured && noCopy) && d.fill && x1 > x0 && y1 > y0) {                              // the picture area without a picture (the theme's background)
        VkViewport vp{0.0f, 0.0f, static_cast<float>(wd.Width), static_cast<float>(wd.Height), 0.0f, 1.0f};
        VkRect2D sc{{x0, y0}, {static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)}};
        vkCmdBindPipeline(fd.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, d.fill);
        vkCmdSetViewport(fd.CommandBuffer, 0, 1, &vp);
        vkCmdSetScissor(fd.CommandBuffer, 0, 1, &sc);
        vkCmdPushConstants(fd.CommandBuffer, d.fillLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 * sizeof(float), fill);
        vkCmdDraw(fd.CommandBuffer, 3, 1, 0, 0);
    }
    ImGui_ImplVulkan_RenderDrawData(dd, fd.CommandBuffer);
    vkCmdEndRendering(fd.CommandBuffer);
    vkImageBarrier2(fd.CommandBuffer, fd.Backbuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0);   // (to the present: the semaphore signalled after it)
    if (timed >= 0) vkCmdWriteTimestamp2(fd.CommandBuffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, d.benchPool, static_cast<uint32_t>(2 * timed + 1));   // (its end)
    if (!d.check(vkEndCommandBuffer(fd.CommandBuffer), "vkEndCommandBuffer")) { ok_ = false; return false; }
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &acquired;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fd.CommandBuffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &complete;
    if (!d.check(vkQueueSubmit(d.queue, 1, &si, fd.Fence), "vkQueueSubmit")) { ok_ = false; return false; }
    d.drewFrame = true;
    d.fenceSerial[static_cast<std::size_t>(wd.FrameIndex)] = ++d.frameSerial;   // (0.63.1)
    if (d.benchOn) {                                                      // (the card batches submitted since the frame before belong to this frame)
        const std::uint64_t last = d.cards ? d.cards->lastTicket() : 0;
        if (timed >= 0) d.benchPending.push_back({timed, d.benchTicket + 1, last, d.frameSerial, static_cast<int>(wd.FrameIndex)});
        d.benchTicket = last;
    }
    return true;
}

bool VkBackend::benchTimer(bool on) {
    Impl& d = *d_;
    if (!on) {
        d.benchOn = false; d.benchNext = false; d.benchPending.clear();
        for (bool& u : d.benchUsed) u = false;
        if (d.cards) d.cards->setTiming(false);
        return true;
    }
    if (!d.device) return false;
    if (!d.benchPool) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d.phys, &p);
        uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nq, qf.data());
        const uint32_t bits = d.family < nq ? qf[d.family].timestampValidBits : 0;
        if (bits == 0 || !(p.limits.timestampPeriod > 0)) return false;
        d.tsNs = p.limits.timestampPeriod;
        d.tsMask = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        VkQueryPoolCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qi.queryCount = 2 * Impl::kBenchSlots + 2;
        if (!d.check(vkCreateQueryPool(d.device, &qi, nullptr, &d.benchPool), "vkCreateQueryPool (the benchmark's timer)")) { d.benchPool = VK_NULL_HANDLE; return false; }
    }
    if (d.cards && !d.cards->setTiming(true)) return false;
    d.benchOn = true; d.benchNext = false; d.benchPending.clear(); d.benchRejected = 0;
    for (bool& u : d.benchUsed) u = false;
    d.benchTicket = d.cards ? d.cards->lastTicket() : 0;
    return true;
}

bool VkBackend::benchMark() {
    Impl& d = *d_;
    if (!d.benchOn) return false;
    if (d.benchNext) return true;
    for (int s = 0; s < Impl::kBenchSlots; s++)
        if (!d.benchUsed[s]) { d.benchUsed[s] = true; d.benchSlot = s; d.benchNext = true; return true; }
    return false;
}

void VkBackend::benchCollect(std::vector<std::pair<double, double>>& out) {
    Impl& d = *d_;
    out.clear();
    if (!d.benchOn) return;
    while (!d.benchPending.empty()) {
        const Impl::BenchFrame f = d.benchPending.front();
        if (f.serial > d.doneSerial) {                                    // 0.63.1: its fence first (never waits)
            const std::size_t im = static_cast<std::size_t>(f.image);
            if (im >= d.fenceSerial.size() || d.fenceSerial[im] != f.serial || im >= static_cast<std::size_t>(d.wd.ImageCount) ||
                vkGetFenceStatus(d.device, d.wd.Frames[f.image].Fence) != VK_SUCCESS) break;
            d.doneSerial = f.serial;
        }
        std::uint64_t r[4] = {0, 0, 0, 0};                                // (value, available) twice; never waits
        const VkResult res = vkGetQueryPoolResults(d.device, d.benchPool, static_cast<uint32_t>(2 * f.slot), 2, sizeof r, r, 2 * sizeof(std::uint64_t),
                                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if (res == VK_NOT_READY || !r[1] || !r[3]) break;
        double cardsMs = 0, c0 = -1, c1 = -1;
        if (d.cards && !d.cards->batchTimes(f.first, f.last, cardsMs, c0, c1)) break;
        const std::uint64_t ra = r[0] & d.tsMask, rb = r[2] & d.tsMask;
        const double a = static_cast<double>(ra) * d.tsNs, b = static_cast<double>(rb) * d.tsNs;
        const double busy = (b - a) / 1e6 + cardsMs;
        const double first = c0 >= 0 && c0 < a ? c0 : a, last = c1 > b ? c1 : b;
        // 0.63.1: a zero, an end before its start or a frame over 10 s on the card is not a time (cardsMs < 0: a batch of the frame was thrown away)
        const bool sane = ra != 0 && rb >= ra && cardsMs >= 0 && busy < 1e4 && last >= first && (last - first) / 1e6 < 1e4;
        if (res == VK_SUCCESS && sane) out.push_back({busy, (last - first) / 1e6});
        else if (res == VK_SUCCESS) d.benchRejected++;
        d.benchUsed[f.slot] = false;
        d.benchPending.erase(d.benchPending.begin());
    }
}

bool VkBackend::benchLoad(Renderer2D& pic, int w, int h, double& gpuMs, double& wallMs) {
    Impl& d = *d_;
    gpuMs = wallMs = -1;
    if (!d.benchPool) return false;
    const float clear[4] = {0, 0, 0, 1};
    ImDrawData none;                                                       // (no panels: Dear ImGui draws nothing for a draw list of size 0)
    std::vector<uint8_t> rgb;
    d.benchImage = true; d.imageGpuMs = d.imageWallMs = -1;
    const bool ok = renderToImage(&none, w, h, clear, rgb, &pic);
    d.benchImage = false;
    gpuMs = d.imageGpuMs; wallMs = d.imageWallMs;
    return ok && gpuMs >= 0 && wallMs >= 0;
}

int VkBackend::benchRejected() const { return d_->benchRejected; }

void VkBackend::present() {
    Impl& d = *d_;
    if (!d.drewFrame) return;
    d.drewFrame = false;
    ImGui_ImplVulkanH_Window& wd = d.wd;
    VkSemaphore complete = wd.FrameSemaphores[static_cast<int>(wd.SemaphoreIndex)].RenderCompleteSemaphore;
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &complete;
    pi.swapchainCount = 1;
    pi.pSwapchains = &wd.Swapchain;
    pi.pImageIndices = &wd.FrameIndex;
    VkResult r = VK_SUCCESS;
    {   app::FrameCostSpan span(app::FrameCostKind::Present);   // (0.63.1)
        r = vkQueuePresentKHR(d.queue, &pi); }
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) d.rebuild = true;   // resized, moved to another screen: made again at the next frame
    else if (!d.check(r, "vkQueuePresentKHR")) ok_ = false;
    wd.SemaphoreIndex = (wd.SemaphoreIndex + 1) % wd.SemaphoreCount;
}

namespace {
// a buffer the processor can read, its memory bound
struct HostBuffer {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    ~HostBuffer() { if (buf) vkDestroyBuffer(device, buf, nullptr); if (memory) vkFreeMemory(device, memory, nullptr); }
};
}  // namespace

// one-time commands: record(cmd), submit, wait; false on any error
template <class F> static bool runOnce(VkBackend::Impl& d, F record) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = d.oncePool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!d.check(vkAllocateCommandBuffers(d.device, &ai, &cmd), "vkAllocateCommandBuffers")) return false;
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bool ok = d.check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer (once)");
    if (ok) { record(cmd); ok = d.check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (once)"); }
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (ok) ok = d.check(vkCreateFence(d.device, &fi, nullptr, &fence), "vkCreateFence (once)");
    if (ok) {
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        const auto t0 = std::chrono::steady_clock::now();
        ok = d.check(vkQueueSubmit(d.queue, 1, &si, fence), "vkQueueSubmit (once)") && d.check(vkWaitForFences(d.device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences (once)");
        if (d.benchImage) d.imageWallMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();   // 0.63.0 (benchLoad)
    }
    if (fence) vkDestroyFence(d.device, fence, nullptr);
    vkFreeCommandBuffers(d.device, d.oncePool, 1, &cmd);
    return ok;
}

static bool makeHostBuffer(VkBackend::Impl& d, VkDeviceSize size, HostBuffer& hb) {
    hb.device = d.device;
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!d.check(vkCreateBuffer(d.device, &bi, nullptr, &hb.buf), "vkCreateBuffer (readback)")) { hb.buf = VK_NULL_HANDLE; return false; }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(d.device, hb.buf, &req);
    const uint32_t type = d.memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (type == UINT32_MAX) { d.check(VK_ERROR_FEATURE_NOT_PRESENT, "no memory the processor can read"); return false; }
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (!d.check(vkAllocateMemory(d.device, &mi, nullptr, &hb.memory), "vkAllocateMemory (readback)")) { hb.memory = VK_NULL_HANDLE; return false; }
    return d.check(vkBindBufferMemory(d.device, hb.buf, hb.memory, 0), "vkBindBufferMemory (readback)");
}

static void barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {   // (0.63.0: synchronization2; the legacy bits have the same values there)
    vkImageBarrier2(cmd, image, from, to, static_cast<VkPipelineStageFlags2>(srcStage), static_cast<VkAccessFlags2>(srcAccess), static_cast<VkPipelineStageFlags2>(dstStage),
                    static_cast<VkAccessFlags2>(dstAccess));
}

// the bytes of an 8-bit BGRA or RGBA image to RGBA (or RGB), top row first
static void unpack(const uint8_t* src, int w, int h, bool bgra, int channels, std::vector<uint8_t>& out) {
    out.resize(std::size_t(w) * std::size_t(h) * std::size_t(channels));
    for (std::size_t i = 0, n = std::size_t(w) * std::size_t(h); i < n; i++) {
        const uint8_t* s = src + i * 4;
        uint8_t* o = out.data() + i * std::size_t(channels);
        o[0] = bgra ? s[2] : s[0]; o[1] = s[1]; o[2] = bgra ? s[0] : s[2];
        if (channels == 4) o[3] = 255;
    }
}

bool VkBackend::readback(std::vector<uint8_t>& rgba, int& w, int& h) {
    Impl& d = *d_;
    if (!d.drewFrame || !(d.extraUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) return false;
    w = d.wd.Width; h = d.wd.Height;
    app::FrameCostLabel label("the window's picture read back (a screenshot, the benchmark's own check)");
    app::FrameCostSpan span(app::FrameCostKind::Readback);   // (0.63.1: the frame read back: the benchmark's own checks)
    if (!d.check(vkQueueWaitIdle(d.queue), "vkQueueWaitIdle (readback)")) return false;
    HostBuffer hb;
    const VkDeviceSize size = VkDeviceSize(w) * VkDeviceSize(h) * 4;
    if (!makeHostBuffer(d, size, hb)) return false;
    const VkImage image = d.wd.Frames[static_cast<int>(d.wd.FrameIndex)].Backbuffer;
    const bool ok = runOnce(d, [&](VkCommandBuffer cmd) {
        barrier(cmd, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy c{};
        c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        c.imageSubresource.layerCount = 1;
        c.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, hb.buf, 1, &c);
        barrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_READ_BIT, 0,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });
    if (!ok) return false;
    void* p = nullptr;
    if (!d.check(vkMapMemory(d.device, hb.memory, 0, size, 0, &p), "vkMapMemory (readback)")) return false;
    unpack(static_cast<const uint8_t*>(p), w, h, d.wd.SurfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM, 4, rgba);
    vkUnmapMemory(d.device, hb.memory);
    return true;
}

bool VkBackend::renderToImage(ImDrawData* dd, int w, int h, const float clear[4], std::vector<uint8_t>& rgb, Renderer2D* pic) {
    Impl& d = *d_;
    if (!d.device || w <= 0 || h <= 0) return false;
    if (!d.check(vkQueueWaitIdle(d.queue), "vkQueueWaitIdle (image)")) return false;   // Dear ImGui's vertex buffers rotate per call: none may be in use
    const VkFormat fmt = d.wd.SurfaceFormat.format;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    HostBuffer hb;
    bool ok = true;
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = fmt;
    ii.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;   // (0.69.0: DST: the picture's copy)
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ok = d.check(vkCreateImage(d.device, &ii, nullptr, &image), "vkCreateImage (picture)");
    if (ok) {
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(d.device, image, &req);
        VkMemoryAllocateInfo mi{};
        mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mi.allocationSize = req.size;
        mi.memoryTypeIndex = d.memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mi.memoryTypeIndex == UINT32_MAX) mi.memoryTypeIndex = d.memoryType(req.memoryTypeBits, 0);
        ok = mi.memoryTypeIndex != UINT32_MAX && d.check(vkAllocateMemory(d.device, &mi, nullptr, &memory), "vkAllocateMemory (picture)") &&
             d.check(vkBindImageMemory(d.device, image, memory, 0), "vkBindImageMemory (picture)");
    }
    if (ok) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = fmt;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        ok = d.check(vkCreateImageView(d.device, &vi, nullptr, &view), "vkCreateImageView (picture)");
    }
    // (0.63.0: dynamic rendering into the image: no render pass, no framebuffer; Dear ImGui's pipeline is made for the window's format, the image's too)
    const VkDeviceSize size = VkDeviceSize(w) * VkDeviceSize(h) * 4;
    if (ok && !d.benchImage) ok = makeHostBuffer(d, size, hb);           // (0.63.1: the timer's check needs no copy to the processor: the start's waste)
    bool pictured = false;
    const uint32_t tq = 2 * Impl::kBenchSlots;                            // 0.63.0 (benchLoad): the picture's time on the card
    const bool timeIt = d.benchImage && d.benchPool;
    if (ok) ok = runOnce(d, [&](VkCommandBuffer cmd) {
        if (timeIt) { vkCmdResetQueryPool(cmd, d.benchPool, tq, 2); vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, d.benchPool, tq); }
        if (pic && d.d2) {                                                // 0.60.0: the lab's picture (one sample, as OpenGL's offscreen picture)
            d.d2->beginFrame(static_cast<int>(d.wd.FrameIndex), std::max(1, static_cast<int>(d.wd.ImageCount)), cmd);
            pic->endVulkan(w, h, clear, true);
            pictured = d.d2->pictureDrawn() && !d.d2->failed();
        }
        VkClearValue cv{};
        for (int i = 0; i < 4; i++) cv.color.float32[i] = clear[i];
        if (pictured) d.d2->copyTo(cmd, image, cv.color, 0, 0, w, h);    // 0.69.0: cleared and the picture copied in (as the window's frame), then loaded
        else barrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderingAttachmentInfo at{};
        at.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        at.imageView = view;
        at.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        at.loadOp = pictured ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        at.clearValue = cv;
        VkRenderingInfo ri{};
        ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        ri.renderArea.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        ri.layerCount = 1;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &at;
        vkCmdBeginRendering(cmd, &ri);
        if (dd) ImGui_ImplVulkan_RenderDrawData(dd, cmd);
        vkCmdEndRendering(cmd);
        if (timeIt) vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, d.benchPool, tq + 1);
        if (d.benchImage) return;                                         // (0.63.1)
        barrier(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy c{};
        c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        c.imageSubresource.layerCount = 1;
        c.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, hb.buf, 1, &c);
    });
    if (ok && timeIt) {
        std::uint64_t r[4] = {0, 0, 0, 0};
        if (vkGetQueryPoolResults(d.device, d.benchPool, tq, 2, sizeof r, r, 2 * sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) == VK_SUCCESS && r[1] && r[3]) {
            const double a = static_cast<double>(r[0] & d.tsMask) * d.tsNs, b = static_cast<double>(r[2] & d.tsMask) * d.tsNs;
            d.imageGpuMs = b > a ? (b - a) / 1e6 : 0;
        }
    }
    if (ok && !d.benchImage) {
        void* p = nullptr;
        ok = d.check(vkMapMemory(d.device, hb.memory, 0, size, 0, &p), "vkMapMemory (picture)");
        if (ok) { unpack(static_cast<const uint8_t*>(p), w, h, fmt == VK_FORMAT_B8G8R8A8_UNORM, 3, rgb); vkUnmapMemory(d.device, hb.memory); }
    }
    if (d.d2) { if (d.d2->failed()) ok = false; d.d2->dropShot(); }
    if (view) vkDestroyImageView(d.device, view, nullptr);
    if (image) vkDestroyImage(d.device, image, nullptr);
    if (memory) vkFreeMemory(d.device, memory, nullptr);
    return ok;
}

// 0.60.0 (step 2): renderer2d on Vulkan, three guarded steps; the last draws a tiny known picture off screen and reads it back
bool VkBackend::initPicture(Renderer2D& r, bool msaa, bool breakCheck, std::string& why) {
    Impl& d = *d_;
    if (!d.device || !d.guard) { why = "Vulkan has not started"; return false; }
    app::GpuGuard& guard = *d.guard;
    const auto fail = [&](const std::string& step, const std::string& text) { why = text; guard.failed(step, text); return false; };
    guard.begin("renderer2d pipelines");
    VkDevCtx c;
    c.phys = d.phys;
    c.device = d.device;
    c.queue = d.queue;
    c.mem = d.mem;
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d.phys, &p);
        c.limits = p.limits;
    }
    c.check = [&d](VkResult res, const char* what) { return d.check(res, what); };
    c.once = [&d](const std::function<void(VkCommandBuffer)>& f) { return runOnce(d, [&](VkCommandBuffer cmd) { f(cmd); }); };
    c.guard = d.guard;
    c.family = d.family;
    c.compute = d.computeQueue;
    d.ctx = c;
    d.d2 = new VkDraw2D();
    std::string w;
    if (!d.d2->init(c, d.wd.SurfaceFormat.format, msaa ? 4 : 1, w)) return fail("renderer2d pipelines", "renderer2d on Vulkan: " + w);
    guard.end("renderer2d pipelines", "OpenGL's shaders as SPIR-V (" + std::to_string(d.d2->spirvBytes()) + " bytes; the picture area's fill: " +
              std::to_string(sizeof k_fill_vert + sizeof k_fill_frag) + "), " + d.d2->samplesText() + ", the cloud's layer " + d.d2->floatFormatText());
    r.useVulkan(d.d2);
    guard.begin("renderer2d textures");
    if (!r.loadMapsVulkan(w)) { r.useVulkan(nullptr); return fail("renderer2d textures", "the picture maps on Vulkan: " + w); }
    guard.end("renderer2d textures", "the picture maps with their mip levels");
    // the check: 32 x 16 points on 32 x 16 pixels over the colour B: a flat rect (normal blend), a flat disc blob added (additive), a rect in the soft layer
    // laid over (a textured quad, premultiplied); the values each must give, from the shaders' formulas and the blend factors (8-bit layer rounded)
    guard.begin("renderer2d check");
    const float B[3] = {0.25f, 0.5f, 0.75f};                               // (held in 8 bits by the picture: the formulas below use the stored value)
    const Color rc{1.0f, 0.2f, 0.4f, 0.5f}, bc{0.2f, 0.3f, 0.1f, 1.0f}, lc{0.0f, 1.0f, 0.5f, 0.75f};
    r.begin(32, 16);
    r.rect(0, 0, 8, 16, breakCheck ? rc.withAlpha(0.25f) : rc);
    Renderer2D::Blob b{};
    b.x = 12; b.y = 8; b.r = 3; b.kind = 3; b.c0 = bc;
    r.blobs(&b, 1, true);
    r.beginSoftLayer(1.0f);
    r.rect(16, 0, 16, 16, lc);
    r.endSoftLayer();
    std::vector<uint8_t> rgb;
    const float clear[4] = {B[0], B[1], B[2], 1.0f};
    const bool drawn = renderToImage(nullptr, 32, 16, clear, rgb, &r);
    r.begin(1, 1);
    if (!drawn || rgb.size() != 32u * 16u * 3u) { r.useVulkan(nullptr); return fail("renderer2d check", "the check's picture could not be drawn and read back"); }
    const auto q = [](double v) { return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    const double La = q(lc.a) / 255.0;                                     // the soft layer holds 8 bits: its alpha and premultiplied colour rounded
    int want[3][3];
    for (int k = 0; k < 3; k++) {
        const double Bk = q(B[k]) / 255.0;
        const double rcK = k == 0 ? rc.r : k == 1 ? rc.g : rc.b, bcK = k == 0 ? bc.r : k == 1 ? bc.g : bc.b, lcK = k == 0 ? lc.r : k == 1 ? lc.g : lc.b;
        want[0][k] = q(rcK * rc.a + Bk * (1.0 - rc.a));                     // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
        want[1][k] = q(Bk + bcK * bc.a);                                    // additive: SRC_ALPHA, ONE
        want[2][k] = q(q(lcK * lc.a) / 255.0 + Bk * (1.0 - La));            // the layer: c a; laid over: ONE, ONE_MINUS_SRC_ALPHA
    }
    const int at[3][2] = {{4, 8}, {12, 8}, {24, 8}};
    std::string got, bad;
    for (int i = 0; i < 3; i++) {
        const uint8_t* p = rgb.data() + (std::size_t(at[i][1]) * 32u + std::size_t(at[i][0])) * 3u;
        char t[96];
        std::snprintf(t, sizeof t, "%s(%d,%d,%d) want (%d,%d,%d)", i ? "; " : "", p[0], p[1], p[2], want[i][0], want[i][1], want[i][2]);
        got += t;
        for (int k = 0; k < 3; k++) if (std::abs(int(p[k]) - want[i][k]) > 2) bad += std::string(bad.empty() ? "" : ", ") + (i == 0 ? "the flat rect" : i == 1 ? "the added blob" : "the soft layer");
    }
    if (!bad.empty()) { r.useVulkan(nullptr); return fail("renderer2d check", "the check's picture is wrong (" + bad.substr(0, bad.find(',')) + "): " + got); }
    guard.end("renderer2d check", "flat rect, added blob, soft layer: " + got);
    return true;
}

// 0.61.0 (step 3): the card programs (render/vk_card.h); the renderer makes each under its own guard
bool VkBackend::initCards(Renderer2D& r, std::vector<std::string>& why) {
    Impl& d = *d_;
    if (!d.d2 || !d.device) { why.push_back("Vulkan's picture has not started"); return false; }
    d.cards = new VkCards();
    std::string w;
    if (!d.cards->init(d.ctx, d.d2, d.float64, w)) { why.push_back("the card programs: " + w); delete d.cards; d.cards = nullptr; return false; }
    r.useVulkanCards(d.cards, &why);
    return true;
}

bool VkBackend::float64() const { return d_->float64; }
std::size_t VkBackend::cardSpirvBytes() const { return d_->cards ? d_->cards->spirvBytes() : 0; }

std::string VkBackend::pictureText() const { return d_->d2 ? d_->d2->samplesText() + "; the cloud's layer " + d_->d2->floatFormatText() : std::string(); }

}  // namespace render
