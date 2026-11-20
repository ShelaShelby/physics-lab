// 0.67.0: the helper process's side (app/gpu_helper.h): PhysicsLab --gpu-helper <folder> <device> program|variant <name>. No window: its own Vulkan device on the
// card the app chose, the program's pipelines made with the app's own SPIR-V (into the helpers' cache file), each step's line written and committed to the
// result file before it begins, so a driver that kills the process leaves the step it died in. Exit codes: 0 done (passed or the driver said no), 2 the
// device could not be made (the app then makes the program itself, guarded as before); anything else: it died.
#include "render/vk_helper.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "app/gpu_guard.h"
#include "app/gpu_helper.h"
#include "render/vk_card.h"

#include "r2d_kerrtable_comp.h"
#include "r2d_classic_comp.h"
#include "r2d_nucdens_comp.h"
#include "r2d_kerrvar_math.h"
#include "r2d_kerrvar_step.h"
#include "r2d_kerrvar_edge.h"
#include "r2d_kerrvar_ray.h"
#include "r2d_kerrvar_short.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace render {

namespace {
const char kMagic[8] = {'P', 'L', 'V', 'K', 'P', 'C', '1', 0};
std::uint64_t fnv(const char* p, std::size_t n) { std::uint64_t h = 1469598103934665603ULL; for (std::size_t i = 0; i < n; i++) { h ^= static_cast<unsigned char>(p[i]); h *= 1099511628211ULL; } return h; }
}  // namespace

bool vkReadCacheFile(const std::string& path, VkPhysicalDevice phys, std::vector<char>& data, std::string& note) {
    data.clear();
    std::ifstream in(path, std::ios::binary);
    std::vector<char> all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (all.empty()) { note = "none yet"; return false; }
    std::uint64_t size = 0, hash = 0;
    if (all.size() >= 24) { std::memcpy(&size, all.data() + 8, 8); std::memcpy(&hash, all.data() + 16, 8); }
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(phys, &p);
    VkPipelineCacheHeaderVersionOne hd{};
    if (all.size() >= 24 + sizeof hd) std::memcpy(&hd, all.data() + 24, sizeof hd);
    if (all.size() < 24 + sizeof hd || std::memcmp(all.data(), kMagic, 8) != 0 || size != all.size() - 24 || fnv(all.data() + 24, all.size() - 24) != hash) { note = "ignored: the file is damaged"; return false; }
    if (hd.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE || hd.vendorID != p.vendorID || hd.deviceID != p.deviceID || std::memcmp(hd.pipelineCacheUUID, p.pipelineCacheUUID, VK_UUID_SIZE) != 0) {
        note = "ignored: made by another card or driver"; return false;
    }
    data.assign(all.begin() + 24, all.end());
    note = "read, " + std::to_string(data.size()) + " bytes";
    return true;
}

void vkWriteCacheFile(const std::string& path, VkDevice dev, VkPipelineCache cache) {
    std::size_t n = 0;
    if (!cache || vkGetPipelineCacheData(dev, cache, &n, nullptr) != VK_SUCCESS || n == 0) return;
    std::vector<char> data(n);
    if (vkGetPipelineCacheData(dev, cache, &n, data.data()) < 0) return;
    data.resize(n);
    const std::uint64_t size = data.size(), h = fnv(data.data(), data.size());
    const std::string tmp = path + ".new";
    {   std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) return;
        o.write(kMagic, 8);
        o.write(reinterpret_cast<const char*>(&size), 8);
        o.write(reinterpret_cast<const char*>(&h), 8);
        o.write(data.data(), static_cast<std::streamsize>(data.size())); }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
}

}  // namespace render

namespace app {

namespace {
struct Program { const char* step; const std::uint32_t* words; std::size_t bytes; bool f64; };
// the programs the helper knows: each guarded program's own pipeline step, by its guard's settings name (the step names are the app's own)
std::vector<Program> programsOf(const std::string& kind, const std::string& name) {
    if (kind == "variant") {
        if (name == "math") return {{"kKerrTableCS variant math (the double-precision functions alone)", k_r2d_kerrvar_math, sizeof k_r2d_kerrvar_math, true}};
        if (name == "step") return {{"kKerrTableCS variant step (a ray's start and one step, no loop)", k_r2d_kerrvar_step, sizeof k_r2d_kerrvar_step, true}};
        if (name == "edge") return {{"kKerrTableCS variant edge (the shadow's edge alone)", k_r2d_kerrvar_edge, sizeof k_r2d_kerrvar_edge, true}};
        if (name == "ray") return {{"kKerrTableCS variant ray (one whole ray with its end handling)", k_r2d_kerrvar_ray, sizeof k_r2d_kerrvar_ray, true}};
        if (name == "short") return {{"kKerrTableCS variant short (the whole shader, its step loops bounded at 64)", k_r2d_kerrvar_short, sizeof k_r2d_kerrvar_short, true}};
        return {};
    }
    if (name == "lightpaths") return {{"the light paths' pipeline", k_r2d_kerrtable_comp, sizeof k_r2d_kerrtable_comp, true}};
    if (name == "classic") return {{"the Classic dots' pipeline", k_r2d_classic_comp, sizeof k_r2d_classic_comp, false}};
    if (name == "nucdensity") return {{"the nucleus density's pipeline", k_r2d_nucdens_comp, sizeof k_r2d_nucdens_comp, false}};
    return {};
}
std::string g_result, g_text;
void put(const std::string& line) { g_text += line + "\n"; writeCommitted(g_result, g_text); }   // (committed: a crash right after still leaves it)
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}  // namespace

bool gpuHelperKnows(const std::string& name) { return !programsOf("program", name).empty(); }

int gpuHelperMain(int argc, char** argv) {
    if (argc < 6) return 2;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);          // a driver's crash ends the helper quietly (no Windows error box to wait for)
#endif
    const std::string folder = argv[2], device = argv[3], kind = argv[4], name = argv[5];
    g_result = folder + "gpu-helper-result.txt";
    const std::vector<Program> progs = programsOf(kind, name);
    if (progs.empty()) { put("no such program: " + name); return 2; }
    if (volkInitialize() != VK_SUCCESS) { put("no Vulkan loader"); return 2; }
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "Physics Lab (helper)";
    ai.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &ai;
    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ii, nullptr, &inst) != VK_SUCCESS) { put("vkCreateInstance failed"); return 2; }
    volkLoadInstance(inst);
    std::uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    std::vector<VkPhysicalDevice> devs(nd);
    vkEnumeratePhysicalDevices(inst, &nd, devs.data());
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    std::uint32_t fam = 0;
    for (int pass = 0; pass < 2 && !phys; pass++)                           // the app's card by its name first, else the first that computes
        for (VkPhysicalDevice d : devs) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(d, &p);
            if (pass == 0 && device != p.deviceName) continue;
            std::uint32_t nq = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, nullptr);
            std::vector<VkQueueFamilyProperties> qf(nq);
            vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, qf.data());
            for (std::uint32_t q = 0; q < nq && !phys; q++)
                if ((qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qf[q].queueFlags & VK_QUEUE_COMPUTE_BIT)) { phys = d; fam = q; }
            if (phys) break;
        }
    if (!phys) { put("no Vulkan device with a graphics and compute queue"); vkDestroyInstance(inst, nullptr); return 2; }
    VkPhysicalDeviceFeatures have{}, want{};
    vkGetPhysicalDeviceFeatures(phys, &have);
    const bool f64 = have.shaderFloat64 == VK_TRUE;
    want.shaderFloat64 = have.shaderFloat64;
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = fam; qi.queueCount = 1; qi.pQueuePriorities = &prio;
    VkPhysicalDeviceVulkan13Features f13{};
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    f13.dynamicRendering = VK_TRUE; f13.synchronization2 = VK_TRUE;
    VkDeviceCreateInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.pNext = &f13; di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi; di.pEnabledFeatures = &want;
    VkDevice dev = VK_NULL_HANDLE;
    if (vkCreateDevice(phys, &di, nullptr, &dev) != VK_SUCCESS) { put("vkCreateDevice failed"); vkDestroyInstance(inst, nullptr); return 2; }
    volkLoadDevice(dev);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    put(std::string("device ") + props.deviceName);
    render::VkDevCtx c;
    c.phys = phys; c.device = dev; c.limits = props.limits; c.family = fam; c.compute = true;
    vkGetDeviceQueue(dev, fam, 0, &c.queue);
    vkGetPhysicalDeviceMemoryProperties(phys, &c.mem);
    c.check = [](VkResult r, const char* what) { if (r >= 0) return true; put(std::string("a Vulkan call failed: ") + what); return false; };
    c.once = [](const std::function<void(VkCommandBuffer)>&) { return false; };
    int code = 0;
    {
        render::VkCards cards;
        std::string why;
        if (!cards.init(c, nullptr, f64, why)) { put("the card programs' engine did not start: " + why); code = 2; }
        // the helpers' own cache file (the app merges it into its own: render/vk_blackhole.cpp mergeCache)
        const std::string cachePath = gpuHelperCacheFile(folder);
        std::vector<char> data;
        std::string note;
        render::vkReadCacheFile(cachePath, phys, data, note);
        VkPipelineCacheCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        ci.initialDataSize = data.size(); ci.pInitialData = data.empty() ? nullptr : data.data();
        VkPipelineCache cache = VK_NULL_HANDLE;
        if (vkCreatePipelineCache(dev, &ci, nullptr, &cache) != VK_SUCCESS) { ci.initialDataSize = 0; ci.pInitialData = nullptr; if (vkCreatePipelineCache(dev, &ci, nullptr, &cache) != VK_SUCCESS) cache = VK_NULL_HANDLE; }
        const char* die = std::getenv("PL_GPU_DIE");                         // (tests: as if the driver had crashed inside that step)
        for (const Program& p : progs) {
            if (code) break;
            if (p.f64 && !f64) { put("begin " + std::string(p.step)); put("end " + std::string(p.step) + "|FAILED|this card has no double precision in its shaders"); continue; }
            put("begin " + std::string(p.step));
            if (die && *die && std::string(p.step).compare(0, std::strlen(die), die) == 0) std::_Exit(3);
            const double t0 = nowMs();
            render::VkCards::Kernel k;
            const bool ok = cards.makeKernel(p.words, p.bytes, k, why, cache);
            char ms[32];
            std::snprintf(ms, sizeof ms, "%.0f", nowMs() - t0);
            put("end " + std::string(p.step) + (ok ? "|ok|" + std::string(ms) : "|FAILED|" + why));
            if (ok) cards.dropKernel(k);
        }
        if (cache) { if (!code) render::vkWriteCacheFile(cachePath, dev, cache); vkDestroyPipelineCache(dev, cache, nullptr); }
        cards.destroy();
    }
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    if (!code) put("done");
    return code;
}

}  // namespace app
