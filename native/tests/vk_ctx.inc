// 0.69.0: vk_compare_tests' Vulkan device (Mesa's lavapipe here, no window): the instance with the validation layers whenever they are installed (every
// message of severity error is counted and fails the run), a device with a graphics and compute queue, dynamic rendering and synchronization2 as the app
// asks for them, and the context the app's Vulkan objects take (render/vk_draw2d.h VkDevCtx). (Until 0.68.1 in tests/gl_vulkan.inc.)
#include <volk.h>

#include "render/vk_draw2d.h"

namespace vkt {

int g_errors = 0;
VKAPI_ATTR VkBool32 VKAPI_CALL onMessage(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* d, void*) {
    if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { g_errors++; if (g_errors <= 5) std::printf("VULKAN VALIDATION ERROR: %s\n", d && d->pMessage ? d->pMessage : "?"); }
    return VK_FALSE;
}

struct Ctx {
    VkInstance inst = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT msg = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue q = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::uint32_t fam = 0;
    bool f64 = false, validation = false;
    render::VkDevCtx c;
};

bool start(Ctx& h, std::string& why) {
    if (volkInitialize() != VK_SUCCESS) { why = "no Vulkan loader"; return false; }
    std::vector<const char*> layers, exts;
    {                                                                            // the validation layers whenever they are installed (PL_VK_VALIDATION=0: off)
        const char* v = std::getenv("PL_VK_VALIDATION");
        std::uint32_t n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> lp(n);
        vkEnumerateInstanceLayerProperties(&n, lp.data());
        for (const VkLayerProperties& l : lp) if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0 && !(v && std::string(v) == "0")) layers.push_back("VK_LAYER_KHRONOS_validation");
        h.validation = !layers.empty();
        if (h.validation) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "vk_compare_tests";
    ai.apiVersion = VK_API_VERSION_1_3;                                           // 0.62.0: Vulkan 1.3 (the Black Hole Lab's passes)
    VkInstanceCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &ai;
    ii.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    ii.ppEnabledLayerNames = layers.data();
    ii.enabledExtensionCount = static_cast<std::uint32_t>(exts.size());
    ii.ppEnabledExtensionNames = exts.data();
    if (vkCreateInstance(&ii, nullptr, &h.inst) != VK_SUCCESS) { why = "vkCreateInstance failed"; return false; }
    volkLoadInstance(h.inst);
    if (h.validation) {
        VkDebugUtilsMessengerCreateInfoEXT mi{};
        mi.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        mi.pfnUserCallback = onMessage;
        vkCreateDebugUtilsMessengerEXT(h.inst, &mi, nullptr, &h.msg);
    }
    std::uint32_t nd = 0;
    vkEnumeratePhysicalDevices(h.inst, &nd, nullptr);
    std::vector<VkPhysicalDevice> devs(nd);
    vkEnumeratePhysicalDevices(h.inst, &nd, devs.data());
    for (VkPhysicalDevice d : devs) {
        std::uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, qf.data());
        for (std::uint32_t q = 0; q < nq && !h.phys; q++)
            if ((qf[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qf[q].queueFlags & VK_QUEUE_COMPUTE_BIT)) { h.phys = d; h.fam = q; }
    }
    if (!h.phys) { why = "no Vulkan device with a graphics and compute queue"; return false; }
    VkPhysicalDeviceFeatures have{}, want{};
    vkGetPhysicalDeviceFeatures(h.phys, &have);
    h.f64 = have.shaderFloat64 == VK_TRUE;
    want.shaderFloat64 = have.shaderFloat64;
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = h.fam;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    VkDeviceCreateInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.pEnabledFeatures = &want;
    VkPhysicalDeviceVulkan13Features f13{};                                      // 0.62.0: dynamic rendering and synchronization2, as the app asks for them
    f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    di.pNext = &f13;
    if (vkCreateDevice(h.phys, &di, nullptr, &h.dev) != VK_SUCCESS) { why = "vkCreateDevice failed"; return false; }
    volkLoadDevice(h.dev);
    vkGetDeviceQueue(h.dev, h.fam, 0, &h.q);
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.queueFamilyIndex = h.fam;
    vkCreateCommandPool(h.dev, &pi, nullptr, &h.pool);
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(h.phys, &p);
    std::printf("Vulkan: %s (Vulkan %u.%u), double precision in shaders: %s, validation %s\n", p.deviceName, VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion), h.f64 ? "yes" : "no", h.validation ? "on" : "off");
    h.c.phys = h.phys; h.c.device = h.dev; h.c.queue = h.q; h.c.limits = p.limits; h.c.family = h.fam; h.c.compute = true;
    vkGetPhysicalDeviceMemoryProperties(h.phys, &h.c.mem);
    h.c.check = [](VkResult r, const char* what) { if (r >= 0) return true; std::printf("Vulkan call failed: %s (%d)\n", what, static_cast<int>(r)); return false; };
    Ctx* hp = &h;
    h.c.once = [hp](const std::function<void(VkCommandBuffer)>& f) {
        VkCommandBufferAllocateInfo a{};
        a.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        a.commandPool = hp->pool;
        a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        a.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(hp->dev, &a, &cb) != VK_SUCCESS) return false;
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cb, &bi);
        f(cb);
        vkEndCommandBuffer(cb);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        const bool ok = vkQueueSubmit(hp->q, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS && vkQueueWaitIdle(hp->q) == VK_SUCCESS;
        vkFreeCommandBuffers(hp->dev, hp->pool, 1, &cb);
        return ok;
    };
    return true;
}

void stop(Ctx& h) {
    if (h.dev) { vkDeviceWaitIdle(h.dev); if (h.pool) vkDestroyCommandPool(h.dev, h.pool, nullptr); vkDestroyDevice(h.dev, nullptr); }
    if (h.msg) vkDestroyDebugUtilsMessengerEXT(h.inst, h.msg, nullptr);
    if (h.inst) vkDestroyInstance(h.inst, nullptr);
    h = Ctx();
}

// a polled job, as the app polls it (no wait in between but a millisecond's sleep), until it is done or 20 s have passed
template <class F> render::JobState poll(F f) {
    for (int i = 0; i < 20000; i++) { const render::JobState st = f(); if (st != render::JobState::Pending) return st; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    return render::JobState::Pending;
}

}  // namespace vkt
