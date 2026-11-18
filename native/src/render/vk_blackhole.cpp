// 0.62.0: Vulkan, step 4 of 5: the Black Hole Lab's card programs on Vulkan (render/vk_blackhole.h has the how). Each pass below is renderer2d.cpp's OpenGL
// pass of the same name with the same uniforms, the same textures and the same viewport; only the API differs.
#include "render/vk_blackhole.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#include "app/gpu_guard.h"
#include "shaders.h"

#include "r2d_cmp_vert.h"
#include "r2d_cmp_frag.h"
#include "r2d_lens_frag.h"
#include "r2d_kerr_frag.h"
#include "r2d_skytiny_frag.h"
#include "r2d_skymath_frag.h"
#include "r2d_sky_frag.h"
#include "r2d_maxdown_frag.h"
#include "r2d_glowdown_frag.h"
#include "r2d_glowblur_frag.h"
#include "r2d_glowadd_frag.h"
#include "r2d_glaredown_frag.h"
#include "r2d_glareadd_frag.h"
#include "r2d_kerrtable_comp.h"

namespace render {

namespace {
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Code { const std::uint32_t* w; std::size_t bytes; };
Code codeOf(int p) {
    switch (p) {
    case VkBlackHole::pLens: return {k_r2d_lens_frag, sizeof k_r2d_lens_frag};
    case VkBlackHole::pKerr: return {k_r2d_kerr_frag, sizeof k_r2d_kerr_frag};
    case VkBlackHole::pSkyTiny: return {k_r2d_skytiny_frag, sizeof k_r2d_skytiny_frag};
    case VkBlackHole::pSkyMath: return {k_r2d_skymath_frag, sizeof k_r2d_skymath_frag};
    case VkBlackHole::pSky: return {k_r2d_sky_frag, sizeof k_r2d_sky_frag};
    case VkBlackHole::pMaxDown: return {k_r2d_maxdown_frag, sizeof k_r2d_maxdown_frag};
    case VkBlackHole::pGlowDown: return {k_r2d_glowdown_frag, sizeof k_r2d_glowdown_frag};
    case VkBlackHole::pGlowBlur: return {k_r2d_glowblur_frag, sizeof k_r2d_glowblur_frag};
    case VkBlackHole::pGlowAdd: return {k_r2d_glowadd_frag, sizeof k_r2d_glowadd_frag};
    case VkBlackHole::pGlareDown: return {k_r2d_glaredown_frag, sizeof k_r2d_glaredown_frag};
    case VkBlackHole::pGlareAdd: return {k_r2d_glareadd_frag, sizeof k_r2d_glareadd_frag};
    default: return {k_r2d_cmp_frag, sizeof k_r2d_cmp_frag};
    }
}
constexpr VkFormat kRgba8 = VK_FORMAT_R8G8B8A8_UNORM, kF32 = VK_FORMAT_R32G32B32A32_SFLOAT, kR8 = VK_FORMAT_R8_UNORM, kR32 = VK_FORMAT_R32_SFLOAT;
// each program's targets: variant 0 its own set, 1 one RGBA32F target, 2 one R8 target (the mask's stretch); UNDEFINED: not made
struct Variants { VkFormat v[3][4]; int n[3]; };
Variants variantsOf(int p) {
    Variants o{};
    const auto set = [&](int k, std::initializer_list<VkFormat> f) { int i = 0; for (VkFormat x : f) o.v[k][i++] = x; o.n[k] = i; };
    switch (p) {
    case VkBlackHole::pLens: set(0, {kRgba8, kF32, kF32}); set(1, {kF32}); break;   // (1: the exposure's samples at spin 0)
    case VkBlackHole::pKerr: set(0, {kRgba8, kF32, kF32}); break;
    case VkBlackHole::pSky: set(0, {kRgba8, kF32, kF32, kR8}); set(1, {kF32}); break;   // (1: the exposure's samples, the probe)
    case VkBlackHole::pSkyTiny: case VkBlackHole::pSkyMath: case VkBlackHole::pMaxDown: case VkBlackHole::pGlowDown: case VkBlackHole::pGlowBlur:
    case VkBlackHole::pGlareDown: set(1, {kF32}); break;
    case VkBlackHole::pGlowAdd: case VkBlackHole::pGlareAdd: set(0, {kRgba8}); break;
    default: set(0, {kRgba8}); set(1, {kF32}); set(2, {kR8}); break;               // pCopy: the low layers stretched over the picture's own
    }
    return o;
}
// a guard's steps (the sky's stages are told to KerrSky's caller's steps; the lens programs to their own guard)
struct GuardSteps : KerrSteps {
    app::GpuGuard* g;
    explicit GuardSteps(app::GpuGuard* x) : g(x) {}
    void begin(const std::string& s) override { g->begin(s); }
    void end(const std::string& s, const std::string& r) override { g->end(s, r); }
};
}  // namespace

const char* VkBlackHole::progName(int p) {
    static const char* const n[pProgCount] = {"lens (kLensFS)", "spinning hole's table pass (kKerrFS)", "the tiny shader (kKerrSkyTinyFS)", "the elliptic functions alone",
                                              "the whole sky shader (kKerrSkyFS)", "exposure (kMaxDownFS)", "glow down (kGlowDownFS)", "glow blur (kGlowBlurFS)",
                                              "glow add (kGlowAddFS)", "glare down (kGlareDownFS)", "glare add (kGlareAddFS)", "copy (kCompositeFS)"};
    return p >= 0 && p < pProgCount ? n[p] : "?";
}
std::size_t VkBlackHole::spirvBytes(int p) { return codeOf(p).bytes; }

bool VkBlackHole::forced(const char* name) {
    const char* e = std::getenv("PL_VK_CARDFAIL");
    if (!e) return false;
    const std::string list = std::string(",") + e + ",";
    return list.find(std::string(",") + name + ",") != std::string::npos;
}

VkBlackHole::~VkBlackHole() { destroy(); }

// =====================================================================================================================================================
// the engine
// =====================================================================================================================================================
bool VkBlackHole::init(VkCards& c, const std::string& cacheDir, std::string& why) {
    c_ = &c;
    dev_ = c.ctx().device;
    const VkDevCtx& x = c.ctx();
    // ---- the formats, as queried ----
    const auto has = [&](VkFormat f, VkFormatFeatureFlags need) { VkFormatProperties fp{}; vkGetPhysicalDeviceFormatProperties(x.phys, f, &fp); return (fp.optimalTilingFeatures & need) == need; };
    const VkFormatFeatureFlags target = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (!has(kRgba8, target)) { why = "R8G8B8A8_UNORM cannot be drawn into and sampled here"; return false; }
    if (!has(kF32, target | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) { why = "R32G32B32A32_SFLOAT cannot be drawn into, sampled, copied here (the disc's light, the exposure)"; return false; }
    if (!has(kR8, target | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) { why = "R8_UNORM cannot be drawn into and sampled here (the open-sky mask)"; return false; }
    if (!has(kR32, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) { why = "R32_SFLOAT cannot be sampled here (the tables)"; return false; }
    linearFloat_ = has(kF32, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);   // (the lower resolution while moving stretches float layers)
    // ---- the timestamps (the sky pass's time on the card) ----
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(x.phys, &p);
        std::uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(x.phys, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(x.phys, &nq, qf.data());
        const std::uint32_t bits = x.family < nq ? qf[x.family].timestampValidBits : 0;
        tsPeriodNs_ = p.limits.timestampPeriod;
        timestamps_ = bits > 0 && tsPeriodNs_ > 0;
        tsMask_ = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        if (timestamps_) {
            VkQueryPoolCreateInfo qi{};
            qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
            qi.queryCount = 8;
            if (!x.check(vkCreateQueryPool(dev_, &qi, nullptr, &queries_), "vkCreateQueryPool (the sky's times)")) { queries_ = VK_NULL_HANDLE; timestamps_ = false; }
        }
        char b[200];
        std::snprintf(b, sizeof b, "the sky pass's time on the card: %s (timestampValidBits %u, timestampPeriod %.3g ns)", timestamps_ ? "timestamp queries" : "none, this queue has no timestamps",
                      bits, static_cast<double>(p.limits.timestampPeriod));
        cacheText_ = b;
    }
    // ---- the samplers (nearest: OpenGL's GL_NEAREST textures; linear: the low layers, GL_LINEAR), the corners, the white texture, the shared vertex shader ----
    VkSamplerCreateInfo s{};
    s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    s.magFilter = s.minFilter = VK_FILTER_NEAREST;
    s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.maxLod = 0.0f;
    if (!x.check(vkCreateSampler(dev_, &s, nullptr, &nearest_), "vkCreateSampler (black hole)")) { nearest_ = VK_NULL_HANDLE; why = "a sampler could not be made"; return false; }
    s.magFilter = s.minFilter = VK_FILTER_LINEAR;
    if (!x.check(vkCreateSampler(dev_, &s, nullptr, &linear_), "vkCreateSampler (black hole)")) { linear_ = VK_NULL_HANDLE; why = "a sampler could not be made"; return false; }
    const float corners[12] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};   // (Renderer2D::init's square)
    if (!c.makeBuffer(sizeof corners, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, corners_, "the black hole's corners")) { why = "a buffer could not be made"; return false; }
    std::memcpy(corners_.map, corners, sizeof corners);
    VkShaderModuleCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    si.codeSize = sizeof k_r2d_cmp_vert;
    si.pCode = k_r2d_cmp_vert;
    if (!x.check(vkCreateShaderModule(dev_, &si, nullptr, &vs_), "vkCreateShaderModule (kCompositeVS)")) { vs_ = VK_NULL_HANDLE; why = "vkCreateShaderModule failed"; return false; }
    ok_ = true;
    {   VkCommandBuffer cb = c.cmd();
        const float one[4] = {1, 1, 1, 1};
        upload(cb, white_, 1, 1, kRgba8, one, 4, "the white texture");
        if (!white_.img) { ok_ = false; why = "the white texture could not be made"; return false; } }
    readCache(cacheDir);
    // ---- the small programs now (each a few kilobytes); the lens and the table pass on the worker thread (their guard) ----
    for (int p : {pMaxDown, pGlowDown, pGlowBlur, pGlowAdd, pGlareDown, pGlareAdd, pCopy}) {
        std::string w;
        if (!prepare(p, w) || !makePipelines(p, w)) { ok_ = false; why = std::string(progName(p)) + ": " + w; return false; }
        prog_[p].state = 2;
    }
    app::GpuGuard& G = app::gpuVkLensGuard();
    if (G.started() && !G.allowed()) { why = G.offText().empty() ? "switched off by its guard" : G.offText(); prog_[pLens].state = -1; prog_[pKerr].state = -1; return true; }
    static GuardSteps steps(&app::gpuVkLensGuard());
    startWorker(pLens, &steps, "the lens pass's pipeline");
    startWorker(pKerr, &steps, "the spinning hole's table pass's pipeline");
    return true;
}

void VkBlackHole::destroy() {
    for (std::thread& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
    stopGlareThread();
    if (!dev_) return;
    vkDeviceWaitIdle(dev_);
    writeCache();
    for (Tex* t : {&lensTex_, &layer_[0], &layer_[1], &mask_, &out_, &low_[0], &low_[1], &low_[2], &low_[3], &glow_[0], &glow_[1], &glareSrc_, &glareVeil_, &probe_,
                   &table_, &discU_, &discP_, &discC_, &kA_, &kB_, &kE_, &kT_, &kF_, &white_}) dropTex(*t);
    for (Tex& t : exp_) dropTex(t);
    exp_.clear();
    for (HostBuf* h : {&whiteBuf_, &probeBuf_, &glareBuf_}) { c_->dropBuffer(h->b); h->busy = false; }
    c_->dropBuffer(corners_);
    for (Program& p : prog_) {
        const VkDevice d = dev_;
        const VkPipeline a = p.pipe[0], b = p.pipe[1], e = p.pipe[2];
        const VkPipelineLayout pl = p.pl;
        const VkDescriptorSetLayout dsl = p.dsl;
        const VkShaderModule fs = p.fs;
        c_->later([d, a, b, e, pl, dsl, fs] {
            for (VkPipeline q : {a, b, e}) if (q) vkDestroyPipeline(d, q, nullptr);
            if (pl) vkDestroyPipelineLayout(d, pl, nullptr);
            if (dsl) vkDestroyDescriptorSetLayout(d, dsl, nullptr);
            if (fs) vkDestroyShaderModule(d, fs, nullptr);
        });
        p.pipe[0] = p.pipe[1] = p.pipe[2] = VK_NULL_HANDLE; p.pl = VK_NULL_HANDLE; p.dsl = VK_NULL_HANDLE; p.fs = VK_NULL_HANDLE; p.state = 0;
    }
    if (vs_) vkDestroyShaderModule(dev_, vs_, nullptr);
    for (VkSampler q : {nearest_, linear_}) if (q) vkDestroySampler(dev_, q, nullptr);
    if (queries_) vkDestroyQueryPool(dev_, queries_, nullptr);
    if (cache_) vkDestroyPipelineCache(dev_, cache_, nullptr);
    vs_ = VK_NULL_HANDLE; nearest_ = linear_ = VK_NULL_HANDLE; queries_ = VK_NULL_HANDLE; cache_ = VK_NULL_HANDLE;
    c_->collect();
    dev_ = VK_NULL_HANDLE;
    ok_ = false;
}

bool VkBlackHole::makeTex(Tex& t, int w, int h, VkFormat f, VkImageUsageFlags usage, const char* what) {
    dropTex(t);
    const VkDevCtx& x = c_->ctx();
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = f;
    ii.extent = {static_cast<std::uint32_t>(std::max(w, 1)), static_cast<std::uint32_t>(std::max(h, 1)), 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!x.check(vkCreateImage(dev_, &ii, nullptr, &t.img), what)) { t.img = VK_NULL_HANDLE; return false; }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(dev_, t.img, &req);
    std::uint32_t type = UINT32_MAX;
    for (const VkMemoryPropertyFlags want : {VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), VkMemoryPropertyFlags(0)})
        for (std::uint32_t i = 0; i < x.mem.memoryTypeCount && type == UINT32_MAX; i++)
            if ((req.memoryTypeBits & (1u << i)) && (x.mem.memoryTypes[i].propertyFlags & want) == want) type = i;
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (type == UINT32_MAX || !x.check(vkAllocateMemory(dev_, &mi, nullptr, &t.mem), what)) { t.mem = VK_NULL_HANDLE; dropTex(t); return false; }
    if (!x.check(vkBindImageMemory(dev_, t.img, t.mem, 0), what)) { dropTex(t); return false; }
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = t.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = f;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (!x.check(vkCreateImageView(dev_, &vi, nullptr, &t.view), what)) { t.view = VK_NULL_HANDLE; dropTex(t); return false; }
    t.fmt = f; t.w = w; t.h = h;
    t.layout = VK_IMAGE_LAYOUT_UNDEFINED; t.stage = VK_PIPELINE_STAGE_2_NONE; t.access = VK_ACCESS_2_NONE;
    return true;
}

void VkBlackHole::dropTex(Tex& t) {
    if (!t.img && !t.mem && !t.view) { t = Tex(); return; }
    const VkDevice d = dev_;
    const VkImage img = t.img;
    const VkImageView view = t.view;
    const VkDeviceMemory mem = t.mem;
    c_->later([d, img, view, mem] { if (view) vkDestroyImageView(d, view, nullptr); if (img) vkDestroyImage(d, img, nullptr); if (mem) vkFreeMemory(d, mem, nullptr); });
    t = Tex();
}

// synchronization2: the image from its last use (stage, access, layout) to the next; none between two reads in the same layout
void VkBlackHole::use(VkCommandBuffer cb, Tex& t, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access) {
    if (!t.img) return;
    const VkAccessFlags2 writes = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    if (t.layout == layout && !(t.access & writes) && !(access & writes)) { t.stage |= stage; t.access |= access; return; }
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = t.stage;
    b.srcAccessMask = t.access & writes;                     // (only writes need making available; a read before a write needs the execution order alone)
    b.dstStageMask = stage;
    b.dstAccessMask = access;
    b.oldLayout = t.layout;
    b.newLayout = layout;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = t.img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &di);
    t.layout = layout; t.stage = stage; t.access = access;
}

bool VkBlackHole::prepare(int p, std::string& why) {
    Program& P = prog_[p];
    if (P.fs) return true;
    const Code code = codeOf(p);
    VkDraw2D::Shader sh;
    if (!reflectSpirv(code.w, code.bytes / 4, sh, why)) return false;
    if (!sh.block.m.empty() && sh.blockBinding != 1) { why = "its uniform block is not at binding 1"; return false; }
    P.u.blk = sh.block;
    P.blockBinding = sh.block.m.empty() ? -1 : 1;
    P.samplers = sh.samplers;
    P.outputs = static_cast<int>(sh.out.size());
    std::vector<VkDescriptorSetLayoutBinding> b;
    if (P.blockBinding >= 0) b.push_back({1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    for (const auto& [name, at] : P.samplers) {
        if (at < 0 || at == 1) { why = "its texture " + name + " has no binding of its own"; return false; }
        b.push_back({static_cast<std::uint32_t>(at), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    }
    const VkDevCtx& x = c_->ctx();
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = static_cast<std::uint32_t>(b.size());
    li.pBindings = b.data();
    if (!x.check(vkCreateDescriptorSetLayout(dev_, &li, nullptr, &P.dsl), "vkCreateDescriptorSetLayout (black hole)")) { P.dsl = VK_NULL_HANDLE; why = "vkCreateDescriptorSetLayout failed"; return false; }
    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &P.dsl;
    if (!x.check(vkCreatePipelineLayout(dev_, &pli, nullptr, &P.pl), "vkCreatePipelineLayout (black hole)")) { P.pl = VK_NULL_HANDLE; why = "vkCreatePipelineLayout failed"; return false; }
    VkShaderModuleCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    si.codeSize = code.bytes;
    si.pCode = code.w;
    if (!x.check(vkCreateShaderModule(dev_, &si, nullptr, &P.fs), "vkCreateShaderModule (black hole)")) { P.fs = VK_NULL_HANDLE; why = "vkCreateShaderModule failed"; return false; }
    return true;
}

bool VkBlackHole::makePipelines(int p, std::string& why) {
    Program& P = prog_[p];
    const Variants V = variantsOf(p);
    const double t0 = nowMs();
    for (int k = 0; k < 3; k++) {
        if (V.n[k] == 0) continue;
        if ((p == pSky && forced("sky")) || (p == pLens && forced("lens"))) { why = std::string("made to fail on purpose (PL_VK_CARDFAIL=") + (p == pSky ? "sky" : "lens") + ")"; return false; }
        VkPipelineRenderingCreateInfo ri{};
        ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        ri.colorAttachmentCount = static_cast<std::uint32_t>(V.n[k]);
        ri.pColorAttachmentFormats = V.v[k];
        VkPipelineShaderStageCreateInfo st[2]{};
        st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = vs_; st[0].pName = "main";
        st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = P.fs; st[1].pName = "main";
        const VkVertexInputBindingDescription vb{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
        const VkVertexInputAttributeDescription va{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vb;
        vi.vertexAttributeDescriptionCount = 1;
        vi.pVertexAttributeDescriptions = &va;
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
        VkPipelineColorBlendAttachmentState ba[4]{};
        for (VkPipelineColorBlendAttachmentState& a : ba) a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{};                     // (the passes are opaque: OpenGL's drawLens disables blending)
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = static_cast<std::uint32_t>(V.n[k]);
        cb.pAttachments = ba;
        const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        ds.dynamicStateCount = 2;
        ds.pDynamicStates = dyn;
        VkGraphicsPipelineCreateInfo gi{};
        gi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gi.pNext = &ri;
        gi.stageCount = 2;
        gi.pStages = st;
        gi.pVertexInputState = &vi;
        gi.pInputAssemblyState = &ia;
        gi.pViewportState = &vp;
        gi.pRasterizationState = &rs;
        gi.pMultisampleState = &ms;
        gi.pColorBlendState = &cb;
        gi.pDynamicState = &ds;
        gi.layout = P.pl;
        VkPipeline pipe = VK_NULL_HANDLE;
        if (!c_->ctx().check(vkCreateGraphicsPipelines(dev_, cache_, 1, &gi, nullptr, &pipe), "vkCreateGraphicsPipelines (black hole)")) { why = "vkCreateGraphicsPipelines failed"; return false; }
        P.pipe[k] = pipe;
    }
    P.ms = nowMs() - t0;
    return true;
}

// the pipeline of program p on a thread of its own, its step told to steps before and after (from that thread: the guard's marker is written first)
void VkBlackHole::startWorker(int p, KerrSteps* steps, const std::string& step) {
    Program& P = prog_[p];
    if (P.state != 0) return;
    std::string why;
    if (!prepare(p, why)) {
        if (steps) { steps->begin(step); steps->end(step, "FAILED: " + why); }
        P.why = why; P.state = -1; return;
    }
    P.state = 1;
    workers_.emplace_back([this, p, steps, step] {
        Program& Q = prog_[p];
        if (steps) steps->begin(step);
        std::string w;
        const bool ok = makePipelines(p, w);
        char b[300];
        // (beside the sizes known to link on the owner's card on OpenGL, plain / inlined: kKerrSkyFS 103.5 / 64.7 KB, kKerrFS 28.4 / 20.1 KB)
        const char* known = p == pSky ? "; on OpenGL kKerrSkyFS linked at 103.5 / 64.7 KB plain / inlined" : p == pKerr ? "; on OpenGL kKerrFS linked at 28.4 / 20.1 KB plain / inlined" : "";
        std::snprintf(b, sizeof b, "%s in %.0f ms (SPIR-V %zu bytes%s%s)", ok ? "made" : "FAILED", Q.ms, codeOf(p).bytes, cache_ ? ", with the pipeline cache" : "", known);
        if (steps) steps->end(step, ok ? std::string(b) : std::string(b) + ": " + w);
        std::printf("PLVKBH pipeline %s %s ms=%.1f spirv=%zu\n", progName(p), ok ? "made" : "FAILED", Q.ms, codeOf(p).bytes);
        std::fflush(stdout);
        if (ok) writeCache();
        Q.why = w;
        Q.state = ok ? 2 : -1;
    });
}

double VkBlackHole::pipelineMs(int p) const { return p >= 0 && p < pProgCount ? prog_[p].ms : 0; }

std::string VkBlackHole::pipelineText() const {
    std::string t;
    for (int p = 0; p < pProgCount; p++) {
        if (prog_[p].state != 2) continue;
        char b[160];
        std::snprintf(b, sizeof b, "%s%s %.0f ms, %zu bytes", t.empty() ? "" : "; ", progName(p), prog_[p].ms, codeOf(p).bytes);
        t += b;
    }
    return t;
}

int VkBlackHole::buildSky(int stage, KerrSteps* steps, std::string& why) {
    if (stage < 0 || stage > 2) return -1;
    const int p = stage == 0 ? pSkyTiny : stage == 1 ? pSkyMath : pSky;
    Program& P = prog_[p];
    if (P.state == 0) {
        static const char* const step[3] = {"the tiny shader's pipeline", "the elliptic functions' pipeline", "the whole sky shader's pipeline"};
        startWorker(p, steps, step[stage]);
    }
    if (P.state == 1) return 0;
    if (P.state == 2) return 1;
    why = std::string("Vulkan did not make ") + progName(p) + (P.why.empty() ? std::string() : ": " + P.why);
    return -1;
}

bool VkBlackHole::skyReady() const { return prog_[pSky].state == 2; }

void VkBlackHole::begin(VkCommandBuffer cb, std::initializer_list<Tex*> targets, int vx, int vy, int vw, int vh, bool clear) {
    VkRenderingAttachmentInfo at[4]{};
    int n = 0, w = 0, h = 0;
    for (Tex* t : targets) {
        if (n >= 4) break;
        use(cb, *t, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            clear ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);   // (a load reads it)
        at[n].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        at[n].imageView = t->view;
        at[n].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        at[n].loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;   // (OpenGL's textures keep what a pass does not cover)
        at[n].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        passFmt_[n] = t->fmt;
        if (n == 0) { w = t->w; h = t->h; }
        n++;
    }
    passN_ = n;
    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea.extent = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
    ri.layerCount = 1;
    ri.colorAttachmentCount = static_cast<std::uint32_t>(n);
    ri.pColorAttachments = at;
    vkCmdBeginRendering(cb, &ri);
    (void)vx; (void)vy; (void)vw; (void)vh;
}

void VkBlackHole::end(VkCommandBuffer cb) { vkCmdEndRendering(cb); passN_ = 0; }

void VkBlackHole::draw(VkCommandBuffer cb, int p, int variant, std::initializer_list<Bind> tex, int vx, int vy, int vw, int vh) {
    Program& P = prog_[p];
    const VkPipeline pipe = P.state == 2 && variant >= 0 && variant < 3 ? P.pipe[variant] : VK_NULL_HANDLE;
    if (!pipe || vw <= 0 || vh <= 0) return;
    const VkDescriptorSet set = c_->descriptorSet(P.dsl);
    if (!set) return;
    VkDescriptorBufferInfo ub{};
    std::vector<VkDescriptorImageInfo> ii;
    std::vector<VkWriteDescriptorSet> wr;
    ii.reserve(P.samplers.size());
    if (P.blockBinding >= 0) {
        if (!c_->uniformBlock(P.u, ub)) return;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = 1;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &ub;
        wr.push_back(w);
    }
    for (const auto& [name, at] : P.samplers) {
        VkDescriptorImageInfo d{nearest_, white_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        for (const Bind& b : tex)
            if (name == b.name && b.view) { d.imageView = b.view; d.sampler = b.sampler ? b.sampler : nearest_; }
        ii.push_back(d);
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = static_cast<std::uint32_t>(at);
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii.back();
        wr.push_back(w);
    }
    vkUpdateDescriptorSets(dev_, static_cast<std::uint32_t>(wr.size()), wr.data(), 0, nullptr);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, P.pl, 0, 1, &set, 0, nullptr);
    const VkViewport v{static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vw), static_cast<float>(vh), 0.0f, 1.0f};   // OpenGL's rows (as vk_draw2d)
    const VkRect2D sc{{vx, vy}, {static_cast<std::uint32_t>(vw), static_cast<std::uint32_t>(vh)}};
    vkCmdSetViewport(cb, 0, 1, &v);
    vkCmdSetScissor(cb, 0, 1, &sc);
    const VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &corners_.b, &off);
    vkCmdDraw(cb, 6, 1, 0, 0);
}

// a table or picture from the processor: (re)made at its size, staged and copied, then ready to be sampled (RGB tables get alpha 1, as OpenGL's GL_RGB32F)
void VkBlackHole::upload(VkCommandBuffer cb, Tex& t, int w, int h, VkFormat f, const float* data, int comps, const char* what) {
    if (w <= 0 || h <= 0 || !cb) return;
    if (!t.img || t.w != w || t.h != h || t.fmt != f)
        if (!makeTex(t, w, h, f, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, what)) return;
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    std::size_t bytes = 0;
    VkDeviceSize at = 0;
    if (f == kRgba8) {
        std::vector<std::uint8_t> px(n * 4);
        for (std::size_t i = 0; i < n * 4; i++) px[i] = static_cast<std::uint8_t>(std::lround(std::clamp(data[i % static_cast<std::size_t>(comps)], 0.0f, 1.0f) * 255.0f));
        bytes = px.size();
        at = c_->stage(px.data(), bytes);
    } else if (f == kR32) {
        bytes = n * 4;
        at = c_->stage(data, bytes);
    } else {
        std::vector<float> px(n * 4, 1.0f);
        for (std::size_t i = 0; i < n; i++) for (int k = 0; k < comps && k < 4; k++) px[4 * i + static_cast<std::size_t>(k)] = data[static_cast<std::size_t>(comps) * i + static_cast<std::size_t>(k)];
        bytes = px.size() * 4;
        at = c_->stage(px.data(), bytes);
    }
    const VkBuffer sb = c_->stageBuffer();
    if (!sb) return;
    use(cb, t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy cp{};
    cp.bufferOffset = at;
    cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    cp.imageExtent = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1};
    vkCmdCopyBufferToImage(cb, sb, t.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    use(cb, t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

// a rectangle of t (RGBA32F) into a host-visible buffer, its batch's ticket kept: read once that batch is done (never waited for in a frame)
void VkBlackHole::readBack(VkCommandBuffer cb, Tex& t, int x, int y, int w, int h, HostBuf& hb) {
    const VkDeviceSize bytes = VkDeviceSize(w) * VkDeviceSize(h) * 16u;
    if (hb.busy || !hb.b.b || hb.b.size < bytes) {                  // (one still being read back, or too small: a new one; the old one goes once its batch is done)
        c_->dropBuffer(hb.b);
        if (!c_->makeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, hb.b, "a black hole read-back")) return;
    }
    use(cb, t, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    VkBufferImageCopy cp{};
    cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    cp.imageOffset = {x, y, 0};
    cp.imageExtent = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1};
    vkCmdCopyImageToBuffer(cb, t.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, hb.b.b, 1, &cp);
    VkMemoryBarrier2 mb{};
    mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    mb.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    mb.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    mb.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    VkDependencyInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    di.memoryBarrierCount = 1;
    di.pMemoryBarriers = &mb;
    vkCmdPipelineBarrier2(cb, &di);
    hb.ticket = c_->pending();
    hb.busy = true;
}

void VkBlackHole::setMat3T(int p, const char* name, const float K[9]) {
    VkDraw2D::Block& b = prog_[p].u.blk;
    const auto it = b.m.find(name);
    if (it == b.m.end()) return;
    const std::uint32_t stride = it->second.matrixStride ? it->second.matrixStride : 16u;
    for (int col = 0; col < 3; col++) {                              // column c of the matrix whose rows are K's rows (glUniformMatrix3fv with GL_TRUE)
        const float v[3] = {K[col], K[3 + col], K[6 + col]};
        const std::size_t at = it->second.offset + static_cast<std::size_t>(col) * stride;
        if (at + 12u <= b.data.size()) std::memcpy(b.data.data() + at, v, 12u);
    }
}

// ---- the pipeline cache: our header (magic, size, FNV-1a of the data), then the driver's data, whose own header must name this card ----
void VkBlackHole::readCache(const std::string& dir) {
    const VkDevCtx& x = c_->ctx();
    cachePath_ = dir.empty() ? std::string() : dir + "vulkan-pipeline-cache.bin";
    std::vector<char> data;
    std::string note = "none yet";
    if (!cachePath_.empty()) {
        if (std::getenv("PL_VK_CACHE_CORRUPT")) {                    // (tests: the file's bytes damaged before it is read)
            std::fstream f(cachePath_, std::ios::in | std::ios::out | std::ios::binary);
            if (f) { f.seekp(40); const char junk[16] = {'b', 'r', 'o', 'k', 'e', 'n', '!', 0, 1, 2, 3, 4, 5, 6, 7, 8}; f.write(junk, sizeof junk); }
        }
        std::ifstream in(cachePath_, std::ios::binary);
        std::vector<char> all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!all.empty()) {
            std::uint64_t size = 0, hash = 0;
            const char magic[8] = {'P', 'L', 'V', 'K', 'P', 'C', '1', 0};
            if (all.size() >= 24) { std::memcpy(&size, all.data() + 8, 8); std::memcpy(&hash, all.data() + 16, 8); }
            std::uint64_t h = 1469598103934665603ULL;
            if (all.size() >= 24 && size == all.size() - 24) for (std::size_t i = 24; i < all.size(); i++) { h ^= static_cast<unsigned char>(all[i]); h *= 1099511628211ULL; }
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(x.phys, &p);
            VkPipelineCacheHeaderVersionOne hd{};
            if (all.size() >= 24 + sizeof hd) std::memcpy(&hd, all.data() + 24, sizeof hd);
            if (all.size() < 24 + sizeof hd || std::memcmp(all.data(), magic, 8) != 0 || size != all.size() - 24 || h != hash) note = "ignored: the file is damaged (its size or hash does not match)";
            else if (hd.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE || hd.vendorID != p.vendorID || hd.deviceID != p.deviceID ||
                     std::memcmp(hd.pipelineCacheUUID, p.pipelineCacheUUID, VK_UUID_SIZE) != 0) note = "ignored: made by another card or driver";
            else { data.assign(all.begin() + 24, all.end()); note = "read, " + std::to_string(data.size()) + " bytes"; }
        }
    }
    VkPipelineCacheCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    ci.initialDataSize = data.size();
    ci.pInitialData = data.empty() ? nullptr : data.data();
    if (!x.check(vkCreatePipelineCache(dev_, &ci, nullptr, &cache_), "vkCreatePipelineCache")) {
        cache_ = VK_NULL_HANDLE;
        ci.initialDataSize = 0; ci.pInitialData = nullptr;              // (a driver that refuses the data: an empty cache)
        if (!x.check(vkCreatePipelineCache(dev_, &ci, nullptr, &cache_), "vkCreatePipelineCache")) cache_ = VK_NULL_HANDLE;
        note += ", refused by the driver";
    }
    const std::string line = "[cache] " + (cachePath_.empty() ? std::string("no settings folder: not kept") : cachePath_ + ": " + note);
    app::gpuVulkanGuard().log(line);
    std::printf("PLVKCACHE %s\n", note.c_str());
}

void VkBlackHole::writeCache() {
    std::lock_guard<std::mutex> g(workMx_);
    if (!cache_ || cachePath_.empty()) return;
    std::size_t n = 0;
    if (vkGetPipelineCacheData(dev_, cache_, &n, nullptr) != VK_SUCCESS || n == 0) return;
    std::vector<char> data(n);
    if (vkGetPipelineCacheData(dev_, cache_, &n, data.data()) < 0) return;
    data.resize(n);
    std::uint64_t h = 1469598103934665603ULL;
    for (char ch : data) { h ^= static_cast<unsigned char>(ch); h *= 1099511628211ULL; }
    const std::uint64_t size = data.size();
    const char magic[8] = {'P', 'L', 'V', 'K', 'P', 'C', '1', 0};
    const std::string tmp = cachePath_ + ".new";
    {   std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) return;
        o.write(magic, 8);
        o.write(reinterpret_cast<const char*>(&size), 8);
        o.write(reinterpret_cast<const char*>(&h), 8);
        o.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!o) return; }
    std::remove(cachePath_.c_str());
    std::rename(tmp.c_str(), cachePath_.c_str());
}

// =====================================================================================================================================================
// the passes (renderer2d.cpp's drawLens, skyPass, skyExposure, skyTimes, skyPicture, skyProbe, makeGlow, makeGlare, glarePump)
// =====================================================================================================================================================
void VkBlackHole::lensPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, int ox, int oy, bool disc, bool bend, bool exp, bool raw, int variant) {
    const int p = pLens;
    VkUniforms& u = prog_[p].u;
    const double ppp = fbW / ptW_;                                   // pixels per point
    const double cxPx = std::round(L.cx * ppp), cyPx = std::round(L.cy * ppp), Kpx = L.kRad * ppp;
    const double e = L.alphaSh * Kpx, e2 = e * e;
    const float e2hi = static_cast<float>(e2), e2lo = static_cast<float>(e2 - static_cast<double>(e2hi));
    const auto f1 = [&](const char* n, double v) { const float x = static_cast<float>(v); u.f(n, &x, 1); };
    const auto i1 = [&](const char* n, int v) { u.i(n, &v, 1); };
    setMat3T(p, "uK", L.M);
    const float centre[2] = {static_cast<float>(ox + cxPx), static_cast<float>(oy + fbH - cyPx)};
    u.f("uCentre", centre, 2);
    f1("uKpx", Kpx);
    const float edge[3] = {static_cast<float>(e), e2hi, e2lo};
    u.f("uEdge", edge, 3);
    const float tt[2] = {static_cast<float>(std::log(Kpx) + L.s0), static_cast<float>(L.ds)};
    u.f("uT", tt, 2);
    i1("uN", lensN_);
    i1("uBend", bend ? 1 : 0);
    i1("uDebug", L.debugDisc ? 2 : L.debug ? 1 : 0);
    i1("uSky", L.sky ? 1 : 0);
    i1("uDisc", disc ? 1 : 0);
    i1("uRaw", raw ? 1 : 0);
    const double st = L.expStep * ppp;                               // 0.56.0: the card's exposure (spin 0)
    const bool cw = L.whiteMode == 1 && disc && !exp_.empty() && !exp;
    i1("uExp", exp ? 1 : 0);
    const float samp[4] = {static_cast<float>(ox + 0.5 * st), static_cast<float>(st), static_cast<float>(oy + fbH - 0.5 * st), static_cast<float>(st)};
    u.f("uSamp", samp, 4);
    i1("uWhiteMode", cw ? 1 : 0);
    if (disc) {
        const int dn[3] = {L.discS, L.discT, L.discC};
        u.i("uDN", dn, 3);
        const float ds[4] = {static_cast<float>(L.discS0), static_cast<float>(L.discDs0), static_cast<float>(L.discDs1), static_cast<float>(L.discXc)};
        u.f("uDS", ds, 4);
        u.f("uDA", L.discAxis, 3);
        const float dr[4] = {static_cast<float>(L.rO), static_cast<float>(L.rO / std::sqrt(1 - 2 / L.rO)), static_cast<float>(L.rIn), static_cast<float>(L.rOut)};
        u.f("uDR", dr, 4);
        const float dt[4] = {static_cast<float>(L.tScale), static_cast<float>(1 / L.white), static_cast<float>(L.colLog0), static_cast<float>(L.colDlog)};
        u.f("uDT", dt, 4);
        f1("uAll", L.allInvTsun);
    }
    f1("uBright", std::clamp(L.bright, 0.0, 1.0));
    const float tw = static_cast<float>(std::max(skyTexW_, 1));
    f1("uLod", std::max(0.0f, std::log2(tw / 6.2831853f / std::max(static_cast<float>(L.kRad), 1.0f))));
    VkDraw2D* d2 = c_->draw2d();
    const VkImageView sky = d2 ? d2->mapView(static_cast<int>(Map::Sky)) : VK_NULL_HANDLE;
    const VkSampler skyS = d2 ? d2->mapSampler() : VK_NULL_HANDLE;
    const int vw = exp ? (exp_.empty() ? 0 : exp_[0].w) : fbW, vh = exp ? (exp_.empty() ? 0 : exp_[0].h) : fbH;
    draw(cb, p, variant, {{"uTex", sky, skyS}, {"uTable", table_.view, nearest_}, {"uDU", discU_.view, nearest_}, {"uDP", discP_.view, nearest_},
                          {"uDC", discC_.view, nearest_}, {"uWhite", cw ? exp_.back().view : white_.view, nearest_}}, 0, 0, vw, vh);
}

void VkBlackHole::kerrPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, bool kdisc, bool raw) {
    const int p = pKerr;
    VkUniforms& u = prog_[p].u;
    const double ppp = fbW / ptW_;
    const double cxPx = std::round(L.cx * ppp), cyPx = std::round(L.cy * ppp), Kpx = L.kRad * ppp;
    const auto f1 = [&](const char* n, double v) { const float x = static_cast<float>(v); u.f(n, &x, 1); };
    const auto i1 = [&](const char* n, int v) { u.i(n, &v, 1); };
    setMat3T(p, "uKs", L.kM);
    const float centre[2] = {static_cast<float>(cxPx), static_cast<float>(fbH - cyPx)};
    u.f("uCentre", centre, 2);
    f1("uKpx", Kpx);
    const int kn[3] = {L.kChi, L.kS, L.kEdgeN};
    u.i("uKN", kn, 3);
    const float ks[4] = {static_cast<float>(L.kSLo), static_cast<float>(L.kXc), static_cast<float>(L.kXlo), static_cast<float>(L.spin)};
    u.f("uKS", ks, 4);
    u.f("uKO", L.kO, 4);
    i1("uDebug", L.debugDisc ? 2 : L.debug ? 1 : 0);
    i1("uSky", L.sky ? 1 : 0);
    i1("uDisc", kdisc ? 1 : 0);
    f1("uBright", std::clamp(L.bright, 0.0, 1.0));
    const float dr[4] = {static_cast<float>(L.rIn), static_cast<float>(L.rOut), static_cast<float>(L.discFLog0), static_cast<float>(L.discFDl)};
    u.f("uDR", dr, 4);
    const float dt[4] = {static_cast<float>(L.tScale), static_cast<float>(1 / L.white), static_cast<float>(L.colLog0), static_cast<float>(L.colDlog)};
    u.f("uDT", dt, 4);
    f1("uAll", L.allInvTsun);
    const int dn[3] = {L.discFN, L.discC, 0};
    u.i("uDN", dn, 3);
    u.f("uDFix", L.discFix, 4);
    i1("uRaw", raw ? 1 : 0);
    VkDraw2D* d2 = c_->draw2d();
    const VkImageView sky = d2 ? d2->mapView(static_cast<int>(Map::Sky)) : VK_NULL_HANDLE;
    draw(cb, p, 0, {{"uTex", sky, d2 ? d2->mapSampler() : VK_NULL_HANDLE}, {"uKA", kA_.view, nearest_}, {"uKB", kB_.view, nearest_}, {"uKE", kE_.view, nearest_},
                    {"uKT", kT_.view, nearest_}, {"uDC", discC_.view, nearest_}, {"uDF", kF_.view, nearest_}}, 0, 0, fbW, fbH);
}

// 0.56.0's skyPass: one pass of the spinning hole per pixel; (vx, vy, vw, vh): the viewport in the bound targets' pixels (OpenGL's glViewport)
void VkBlackHole::skyPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, int ox, int oy, double scale, int mode, bool raw, int variant, int vx, int vy, int vw, int vh) {
    const int p = pSky;
    VkUniforms& u = prog_[p].u;
    const double ppp = fbW / ptW_;
    const double cxPx = std::round(L.cx * ppp), cyPx = std::round(L.cy * ppp), Kpx = L.kRad * ppp;
    const bool disc = L.disc && L.discF && L.discCol && L.discFN >= 2 && L.discC >= 2;
    const auto f1 = [&](const char* n, double v) { const float x = static_cast<float>(v); u.f(n, &x, 1); };
    const auto i1 = [&](const char* n, int v) { u.i(n, &v, 1); };
    u.f("uCam", L.skyCam, 4);
    u.f("uMet", L.skyMet, 4);
    u.f("uMet2", L.skyMet2, 4);
    setMat3T(p, "uKs", L.kM);
    const float centre[2] = {static_cast<float>((ox + cxPx) * scale), static_cast<float>((oy + fbH - cyPx) * scale)};
    u.f("uCentre", centre, 2);
    f1("uKpx", Kpx * scale);
    u.f("uKO", L.kO, 4);
    i1("uMode", mode);
    f1("uBright", std::clamp(L.bright, 0.0, 1.0));
    i1("uSky", L.sky ? 1 : 0);
    i1("uDisc", disc ? 1 : 0);
    const float dr[4] = {static_cast<float>(L.rIn), static_cast<float>(L.rOut), static_cast<float>(L.discFLog0), static_cast<float>(L.discFDl)};
    u.f("uDR", dr, 4);
    const float dt[4] = {static_cast<float>(L.tScale), static_cast<float>(1 / L.white), static_cast<float>(L.colLog0), static_cast<float>(L.colDlog)};
    u.f("uDT", dt, 4);
    f1("uAll", L.allInvTsun);
    u.f("uLP", L.lp, 4);
    const int dn[3] = {L.discFN, L.discC, 0};
    u.i("uDN", dn, 3);
    u.f("uDFix", L.filmRaw, 4);
    const bool cardWhite = L.whiteMode == 1 && disc && !exp_.empty() && mode == 0;
    i1("uWhiteMode", cardWhite ? 1 : 0);
    f1("uWhiteSet", L.white);
    const double st = L.expStep * ppp;
    const float samp[4] = {static_cast<float>(ox + 0.5 * st), static_cast<float>(st), static_cast<float>(oy + fbH - 0.5 * st), static_cast<float>(st)};
    u.f("uSamp", samp, 4);
    i1("uRaw", raw ? 1 : 0);
    VkDraw2D* d2 = c_->draw2d();
    const VkImageView sky = d2 ? d2->mapView(static_cast<int>(Map::Sky)) : VK_NULL_HANDLE;
    draw(cb, p, variant, {{"uTex", sky, d2 ? d2->mapSampler() : VK_NULL_HANDLE}, {"uDC", discC_.view, nearest_}, {"uDF", kF_.view, nearest_},
                          {"uWhite", cardWhite ? exp_.back().view : white_.view, nearest_}}, vx, vy, vw, vh);
}

// 0.56.0's skyExposure: the samples into the chain's first level, 4 x 4 blocks to their largest down to one texel, read back without waiting
void VkBlackHole::exposure(VkCommandBuffer cb, const LensSky& L, const std::function<void(int, int)>& sample) {
    if (prog_[pMaxDown].state != 2) return;
    int nx = 0, ny = 0;
    for (double X = L.expStep / 2; X < ptW_; X += L.expStep) nx++;
    for (double Y = L.expStep / 2; Y < ptH_; Y += L.expStep) ny++;
    if (nx == 0 || ny == 0) return;
    std::vector<int> w{nx}, h{ny};
    while (w.back() > 1 || h.back() > 1) { w.push_back((w.back() + 3) / 4); h.push_back((h.back() + 3) / 4); }
    if (w != expW_ || h != expH_) {
        for (Tex& t : exp_) dropTex(t);
        exp_.assign(w.size(), Tex());
        for (std::size_t k = 0; k < w.size(); k++)
            if (!makeTex(exp_[k], w[k], h[k], kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "the exposure's chain")) { exp_.clear(); expW_.clear(); return; }
        expW_ = w; expH_ = h;
    }
    begin(cb, {&exp_[0]}, 0, 0, nx, ny, false);
    sample(nx, ny);
    end(cb);
    VkUniforms& u = prog_[pMaxDown].u;
    for (std::size_t k = 1; k < w.size(); k++) {
        use(cb, exp_[k - 1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        const int n[2] = {w[k - 1], h[k - 1]};
        u.i("uN", n, 2);
        begin(cb, {&exp_[k]}, 0, 0, w[k], h[k], false);
        draw(cb, pMaxDown, 1, {{"uTex", exp_[k - 1].view, nearest_}}, 0, 0, w[k], h[k]);
        end(cb);
    }
    skyStats_.expN = nx * ny;
    if (!whiteBuf_.busy) readBack(cb, exp_.back(), 0, 0, 1, 1, whiteBuf_);   // (one read-back at a time; a newer one waits for it)
    use(cb, exp_.back(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

// 0.56.0's skyTimes: what has come back from the card without waiting: the pass's times and the exposure's texel
void VkBlackHole::times() {
    while (qTail_ < qHead_) {
        const int t = qTail_ % 4;
        if (!c_->done(qTicket_[t])) break;
        if (timestamps_) {
            std::uint64_t r[4] = {0, 0, 0, 0};                         // (value, available) twice
            const VkResult res = vkGetQueryPoolResults(dev_, queries_, static_cast<std::uint32_t>(2 * t), 2, sizeof r, r, 2 * sizeof(std::uint64_t),
                                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
            if (res == VK_NOT_READY || !r[1] || !r[3]) break;
            const std::uint64_t a = r[0] & tsMask_, b = r[2] & tsMask_;
            const double ms = b > a ? static_cast<double>(b - a) * tsPeriodNs_ / 1e6 : 0, sc = qScale_[t];
            skyStats_.lastMs = ms;
            skyStats_.fullMs = ms / (sc * sc);
        }
        skyStats_.timed++;                                             // (no timestamps on this queue: counted, with no time: fullMs stays -1)
        qTail_++;
    }
    if (whiteBuf_.busy && c_->done(whiteBuf_.ticket)) {
        whiteBuf_.busy = false;
        const float* v = static_cast<const float*>(whiteBuf_.b.map);
        if (v) {
            skyStats_.white = v[0] > 0 ? v[0] : 1;
            skyStats_.whiteY = v[1];
            skyStats_.whiteReads++;
        }
    }
}

// 0.56.0's skyPicture: the mask, the exposure first (whiteMode 1), then the pass, timed; THE RESOLUTION RULE of 0.56.2 (renderer2d.cpp has it in full)
void VkBlackHole::skyPicture(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, bool raw) {
    if (!mask_.img || mask_.w != fbW || mask_.h != fbH)
        if (!makeTex(mask_, fbW, fbH, kR8, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the open-sky mask")) return;
    const bool disc = L.disc && L.discF && L.discCol && L.discFN >= 2 && L.discC >= 2;
    if (L.whiteMode == 1 && disc) {
        const auto e0 = std::chrono::steady_clock::now();
        exposure(cb, L, [&](int nx, int ny) { skyPass(cb, L, fbW, fbH, 0, 0, 1, 10, false, 1, 0, 0, nx, ny); });
        skyStats_.expMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - e0).count();
    }
    double scale = 1;
    if (L.moving && L.lowRes && linearFloat_) {                       // (a card that cannot filter float pictures: every pixel always)
        const double full = L.slowMs > 0 ? L.slowMs : skyStats_.fullMs;
        if (full > L.sharpMs && L.frameBudgetMs > 0 && full > L.frameBudgetMs) scale = std::clamp(std::floor(std::sqrt(L.frameBudgetMs / full) * 8) / 8, 0.25, 1.0);
    }
    skyStats_.scale = scale;
    const bool timed = qHead_ - qTail_ < 4;
    const int slot = qHead_ % 4;
    if (timed && timestamps_) {
        vkCmdResetQueryPool(cb, queries_, static_cast<std::uint32_t>(2 * slot), 2);
        vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queries_, static_cast<std::uint32_t>(2 * slot));
    }
    if (scale >= 1) {
        begin(cb, {&lensTex_, &layer_[0], &layer_[1], &mask_}, 0, 0, fbW, fbH, false);
        skyPass(cb, L, fbW, fbH, 0, 0, 1, 0, raw, 0, 0, 0, fbW, fbH);
        end(cb);
    } else {
        const int w = std::max(1, static_cast<int>(std::ceil(fbW * scale))), h = std::max(1, static_cast<int>(std::ceil(fbH * scale)));
        if (low_[0].w != w || low_[0].h != h || !low_[0].img) {
            const VkFormat fmt[4] = {kRgba8, kF32, kF32, kR8};
            for (int k = 0; k < 4; k++) if (!makeTex(low_[k], w, h, fmt[k], VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "a lower-resolution layer")) return;
        }
        begin(cb, {&low_[0], &low_[1], &low_[2], &low_[3]}, 0, 0, w, h, false);
        skyPass(cb, L, fbW, fbH, 0, 0, scale, 0, raw, 0, 0, 0, w, h);
        end(cb);
        Tex* full[4] = {&lensTex_, &layer_[0], &layer_[1], &mask_};      // stretched over the picture's own layers (bilinear, as OpenGL's GL_LINEAR)
        const int variant[4] = {0, 1, 1, 2};
        for (int k = 0; k < 4; k++) {
            use(cb, low_[k], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            begin(cb, {full[k]}, 0, 0, fbW, fbH, false);
            draw(cb, pCopy, variant[k], {{"uTex", low_[k].view, linear_}}, 0, 0, fbW, fbH);
            end(cb);
        }
        skyStats_.lowDraws++;
    }
    if (timed) {
        if (timestamps_) vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queries_, static_cast<std::uint32_t>(2 * slot + 1));
        qScale_[slot] = scale;
        qTicket_[slot] = c_->pending();
        qHead_++;
    }
    skyStats_.draws++;
}

// 0.55.3's makeGlow: the disc's light down to 4 x 4 blocks above the threshold, then the Gaussian along rows and columns (kept in glow_[0])
void VkBlackHole::makeGlow(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH) {
    const int gw = (fbW + 3) / 4, gh = (fbH + 3) / 4;
    if (glow_[0].w != gw || glow_[0].h != gh || !glow_[0].img)
        for (Tex& t : glow_) if (!makeTex(t, gw, gh, kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the glow")) return;
    const double sig = L.glow[2] * fbH / 4;
    const int R = std::max(1, static_cast<int>(std::ceil(3 * sig)));
    use(cb, layer_[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    {   VkUniforms& u = prog_[pGlowDown].u;
        const int sz[2] = {fbW, fbH};
        u.i("uSize", sz, 2);
        u.f("uThr", &L.glow[1], 1);
        begin(cb, {&glow_[0]}, 0, 0, gw, gh, false);
        draw(cb, pGlowDown, 1, {{"uTex", layer_[0].view, nearest_}}, 0, 0, gw, gh);
        end(cb); }
    VkUniforms& u = prog_[pGlowBlur].u;
    const int n[2] = {gw, gh};
    u.i("uN", n, 2);
    u.i("uR", &R, 1);
    const float s = static_cast<float>(sig);
    u.f("uSig", &s, 1);
    for (int pass = 0; pass < 2; pass++) {                             // rows: 0 -> 1, then columns: 1 -> 0
        Tex& from = glow_[pass];
        Tex& into = glow_[1 - pass];
        const int dir[2] = {pass == 0 ? 1 : 0, pass == 0 ? 0 : 1};
        u.i("uDir", dir, 2);
        use(cb, from, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        begin(cb, {&into}, 0, 0, gw, gh, false);
        draw(cb, pGlowBlur, 1, {{"uTex", from.view, nearest_}}, 0, 0, gw, gh);
        end(cb);
    }
    glowValid_ = true;
}

// the disc's light averaged over blocks of b pixels into glareSrc_ (nx x ny)
void VkBlackHole::glareDown(VkCommandBuffer cb, int fbW, int fbH, int b, int nx, int ny) {
    if (glareSrc_.w != nx || glareSrc_.h != ny || !glareSrc_.img)
        if (!makeTex(glareSrc_, nx, ny, kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "the glare's blocks")) return;
    glareB_ = b;
    VkUniforms& u = prog_[pGlareDown].u;
    const int sz[2] = {fbW, fbH};
    u.i("uSize", sz, 2);
    u.i("uB", &b, 1);
    use(cb, layer_[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    begin(cb, {&glareSrc_}, 0, 0, nx, ny, false);
    draw(cb, pGlareDown, 1, {{"uTex", layer_[0].view, nearest_}}, 0, 0, nx, ny);
    end(cb);
}

void VkBlackHole::glareUpload(VkCommandBuffer cb, int nx, int ny, const std::vector<float>& veil) {
    upload(cb, glareVeil_, nx, ny, kF32, veil.data(), 3, "the glare's veil");
    glareNx_ = nx; glareNy_ = ny;
}

// 0.55.4's makeGlare, all in this frame (the automatic checks): the blocks drawn, read back (the batch waited for, as OpenGL's glReadPixels), the veil
void VkBlackHole::makeGlare(const LensSky& L, int fbW, int fbH) {
    const auto t0 = std::chrono::steady_clock::now();
    const double ppp = fbW / ptW_;
    const double cxPx = std::round(L.cx * ppp), cyGl = fbH - std::round(L.cy * ppp), Kpx = L.kRad * ppp;
    const int b = std::max(1, L.glareBlock(fbW, fbH, cxPx, cyGl, Kpx)), nx = (fbW + b - 1) / b, ny = (fbH + b - 1) / b;
    VkCommandBuffer cb = c_->cmd();
    if (!cb) return;
    glareDown(cb, fbW, fbH, b, nx, ny);
    readBack(cb, glareSrc_, 0, 0, nx, ny, glareBuf_);
    c_->flush(true);
    glareBuf_.busy = false;
    const float* rgba = static_cast<const float*>(glareBuf_.b.map);
    std::vector<float> src(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny) * 3), veil;
    if (rgba) for (std::size_t q = 0; q < src.size() / 3; q++) for (int k = 0; k < 3; k++) src[3 * q + static_cast<std::size_t>(k)] = rgba[4 * q + static_cast<std::size_t>(k)];
    const double readMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double out[3] = {0, 0, 0};
    L.glareVeil(fbW, fbH, cxPx, cyGl, Kpx, src, veil, out);
    if (veil.size() != src.size()) veil.assign(src.size(), 0.0f);
    glareUpload(c_->cmd(), nx, ny, veil);
    glareStats_.b = b; glareStats_.nx = nx; glareStats_.ny = ny; glareStats_.readMs = readMs; glareStats_.veilMs = out[0];
    glareStats_.spread = out[1]; glareStats_.laid = out[2]; glareStats_.made++;
    glareStats_.waitMs = glareStats_.frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    glareValid_ = true;
    composed_ = false;
}

void VkBlackHole::glareWant(const LensSky& L, int fbW, int fbH) {
    const double ppp = fbW / ptW_;
    GlareJob j;
    j.fbW = fbW; j.fbH = fbH;
    j.cx = std::round(L.cx * ppp); j.cy = fbH - std::round(L.cy * ppp); j.K = L.kRad * ppp;
    j.b = std::max(1, L.glareBlock(fbW, fbH, j.cx, j.cy, j.K)); j.nx = (fbW + j.b - 1) / j.b; j.ny = (fbH + j.b - 1) / j.b;
    j.veil = L.glareVeil; j.lower = L.glareThread;
    j.gen = ++glareGen_;
    want_ = j;
}

// 0.55.5's glarePump, never waiting: the newest picture's blocks are drawn and copied back in the batch after the picture (stage 3: read back under way);
// once that batch is done the veil is made on its own thread (stage 2) and shown if its picture is still the newest
void VkBlackHole::glarePump() {
    const auto t0 = std::chrono::steady_clock::now();
    const auto since = [](std::chrono::steady_clock::time_point a) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count(); };
    if (glareStage_ == 2 && glareDone_.load()) {
        glareThread_.join();
        glareStage_ = 0;
        if (job_.gen == want_.gen) {
            if (glareVeilBuf_.size() != glareSrcBuf_.size()) glareVeilBuf_.assign(glareSrcBuf_.size(), 0.0f);
            glareUpload(c_->cmd(), job_.nx, job_.ny, glareVeilBuf_);
            glareStats_.b = job_.b; glareStats_.nx = job_.nx; glareStats_.ny = job_.ny; glareStats_.veilMs = glareOut_[0];
            glareStats_.spread = glareOut_[1]; glareStats_.laid = glareOut_[2]; glareStats_.made++;
            glareStats_.waitMs = since(glareT0_);
            glareValid_ = true;
            composed_ = false;
        }
    }
    if (glareStage_ == 3 && c_->done(glareBuf_.ticket)) {                // the blocks are back: the veil on its own thread
        glareStage_ = 0;
        glareBuf_.busy = false;
        if (job_.gen == want_.gen && glareBuf_.b.map) {
            const std::size_t n = static_cast<std::size_t>(job_.nx) * static_cast<std::size_t>(job_.ny);
            const float* rgba = static_cast<const float*>(glareBuf_.b.map);
            glareSrcBuf_.resize(n * 3);
            for (std::size_t q = 0; q < n; q++) for (int k = 0; k < 3; k++) glareSrcBuf_[3 * q + static_cast<std::size_t>(k)] = rgba[4 * q + static_cast<std::size_t>(k)];
            glareStats_.readMs = since(glareT0_);
            glareDone_ = false;
            const GlareJob j = job_;
            const std::vector<float>* src = &glareSrcBuf_;
            std::vector<float>* veil = &glareVeilBuf_;
            double* out = glareOut_;
            std::atomic<bool>* done = &glareDone_;
            glareThread_ = std::thread([j, src, veil, out, done] {
                if (j.lower) j.lower();
                j.veil(j.fbW, j.fbH, j.cx, j.cy, j.K, *src, *veil, out);
                done->store(true);
            });
            glareStage_ = 2;
        }
    }
    if (glareStage_ == 0 && want_.gen != 0 && !glareValid_ && want_.gen != job_.gen && layer_[0].img) {   // the newest picture: its blocks, after it in the batch
        job_ = want_;
        glareT0_ = t0;
        VkCommandBuffer cb = c_->cmd();
        if (cb) {
            glareDown(cb, job_.fbW, job_.fbH, job_.b, job_.nx, job_.ny);
            readBack(cb, glareSrc_, 0, 0, job_.nx, job_.ny, glareBuf_);
            glareStage_ = 3;
        }
    }
    glareStats_.frameMs = since(t0);
}

void VkBlackHole::stopGlareThread() {
    if (glareThread_.joinable()) glareThread_.join();
    glareDone_ = false;
    if (glareStage_ == 2) glareStage_ = 0;
}

// the finished picture: with the eye's glare (kGlareAddFS) or the Film glow (kGlowAddFS) into out_, else the kept picture itself (OpenGL draws these
// into the window each frame; here once, when one of them changed: the same 8-bit values)
void VkBlackHole::compose(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH) {
    const bool glareOn = L.glareKeep > 0 && L.glareBlock && L.glareVeil && prog_[pGlareAdd].state == 2;
    const bool glow = L.glow[0] > 0 && prog_[pGlowAdd].state == 2;
    composed_ = true;
    if ((glareOn && glareValid_) || (glow && glowValid_)) {
        if (out_.w != fbW || out_.h != fbH || !out_.img)
            if (!makeTex(out_, fbW, fbH, kRgba8, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the finished picture")) { pictureView_ = lensTex_.view; return; }
        if (glareOn && glareValid_) {
            VkUniforms& u = prog_[pGlareAdd].u;
            const int n[2] = {glareNx_, glareNy_};
            u.i("uN", n, 2);
            u.i("uB", &glareB_, 1);
            u.f("uKeep", &L.glareKeep, 1);
            for (Tex* t : {&layer_[0], &layer_[1], &glareVeil_}) use(cb, *t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            begin(cb, {&out_}, 0, 0, fbW, fbH, false);
            draw(cb, pGlareAdd, 0, {{"uDisc", layer_[0].view, nearest_}, {"uSky", layer_[1].view, nearest_}, {"uVeil", glareVeil_.view, nearest_}}, 0, 0, fbW, fbH);
            end(cb);
        } else {
            VkUniforms& u = prog_[pGlowAdd].u;
            const int n[2] = {glow_[0].w, glow_[0].h};
            u.i("uN", n, 2);
            u.f("uStr", &L.glow[0], 1);
            for (Tex* t : {&lensTex_, &glow_[0]}) use(cb, *t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            begin(cb, {&out_}, 0, 0, fbW, fbH, false);
            draw(cb, pGlowAdd, 0, {{"uTex", lensTex_.view, nearest_}, {"uGlow", glow_[0].view, nearest_}}, 0, 0, fbW, fbH);
            end(cb);
        }
        pictureView_ = out_.view;
    } else pictureView_ = lensTex_.view;
}

void VkBlackHole::lens(const LensSky& L, double ptW, double ptH, int skyTexW, int fbW, int fbH) {
    glareStats_.frameMs = 0;
    pictureView_ = VK_NULL_HANDLE;
    app::GpuGuard& G = app::gpuVkLensGuard();                          // its pipelines (made on the worker), then its first full picture
    if (guardState_ == 0 && (prog_[pLens].state == -1 || prog_[pKerr].state == -1)) {
        guardState_ = -1;
        const bool lensBad = prog_[pLens].state == -1;
        G.failed(lensBad ? "the lens pass's pipeline" : "the spinning hole's table pass's pipeline", prog_[lensBad ? pLens : pKerr].why);
    }
    if (guardState_ == 1 && c_->done(firstTicket_)) { G.end("the first full picture", "drawn"); G.finished(true); guardState_ = 2; }
    if (!ok_ || prog_[pLens].state != 2 || fbW <= 0 || fbH <= 0) return;   // (OpenGL: no lens program, nothing drawn)
    VkCommandBuffer cb = c_->cmd();
    if (!cb) return;
    ptW_ = ptW; ptH_ = ptH; skyTexW_ = skyTexW;
    const int n = L.table ? static_cast<int>(L.table->size()) : 0;
    const bool bend = L.bend && n >= 4;
    if (bend && L.tableVersion != tableVer_) {                          // a new table: to the card once
        std::vector<float> f(L.table->begin(), L.table->end());
        upload(cb, table_, n, 1, kR32, f.data(), 1, "the lens table");
        tableVer_ = L.tableVersion; lensN_ = n;
    }
    const bool disc = bend && L.disc && L.discU && L.discP && L.discCol && L.discS >= 4 && L.discT >= 4 && L.discC >= 2;
    const bool glareOn = L.glareKeep > 0 && L.glareBlock && L.glareVeil && prog_[pGlareDown].state == 2 && prog_[pGlareAdd].state == 2;
    if (disc && L.discVersion != discVer_) {
        upload(cb, discU_, L.discT, 2 * L.discS, kR32, L.discU->data(), 1, "the disc's table");
        upload(cb, discP_, 2 * L.discS, 1, kR32, L.discP->data(), 1, "the disc's table");
        discVer_ = L.discVersion;
    }
    if (disc && L.colVersion != colVer_) { upload(cb, discC_, L.discC, 1, kF32, L.discCol->data(), 3, "the colours"); colVer_ = L.colVersion; }
    const bool kerr = L.kerr && prog_[pKerr].state == 2 && L.bend && L.kA && L.kB && L.kEdge && L.kTau && L.kChi >= 4 && L.kS >= 4 && L.kEdgeN >= 4;
    const bool kdisc = kerr && L.disc && L.discF && L.discCol && L.discFN >= 2 && L.discC >= 2;
    if (kerr && L.kVersion != kVer_) {
        upload(cb, kA_, L.kS, 2 * L.kChi, kF32, L.kA->data(), 4, "the spin table");
        upload(cb, kB_, L.kS, 2 * L.kChi, kF32, L.kB->data(), 4, "the spin table");
        upload(cb, kE_, L.kEdgeN, 1, kR32, L.kEdge->data(), 1, "the spin table");
        upload(cb, kT_, L.kEdgeN + 1, 1, kR32, L.kTau->data(), 1, "the spin table");
        kVer_ = L.kVersion;
    }
    const bool ksky = L.kerrSky && skyReady() && L.bend;
    const bool skdisc = ksky && L.disc && L.discF && L.discCol && L.discFN >= 2 && L.discC >= 2;
    if ((kdisc || skdisc) && L.discFVersion != kFVer_) { upload(cb, kF_, L.discFN, 1, kR32, L.discF->data(), 1, "the disc's flux"); kFVer_ = L.discFVersion; }
    if ((kdisc || skdisc) && L.colVersion != colVer_) { upload(cb, discC_, L.discC, 1, kF32, L.discCol->data(), 3, "the colours"); colVer_ = L.colVersion; }
    if (L.debug || L.debugDisc || (ksky && L.skyMode > 0)) return;     // (the tests' float outputs: OpenGL's)
    times();
    if (fbW != lensTex_.w || fbH != lensTex_.h || !lensTex_.img) {
        lensValid_ = false;
        if (!makeTex(lensTex_, fbW, fbH, kRgba8, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the bent sky") ||
            !makeTex(layer_[0], fbW, fbH, kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the disc's light") ||
            !makeTex(layer_[1], fbW, fbH, kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "the sky behind it")) return;
    }
    const bool same = lensValid_ && Renderer2D::lensSameView(last_, L, ksky, skyDrawn_, ptW, lastPtW_);
    const bool glow = L.glow[0] > 0 && prog_[pGlowDown].state == 2 && prog_[pGlowBlur].state == 2 && prog_[pGlowAdd].state == 2;
    const bool layers = glow || glareOn;
    if (!same) {
        if (ksky) skyPicture(cb, L, fbW, fbH, glareOn);
        else {
            if (L.whiteMode == 1 && disc) {
                exposure(cb, L, [&](int, int) { lensPass(cb, L, fbW, fbH, 0, 0, disc, bend, true, glareOn, 1); });
            }
            begin(cb, {&lensTex_, &layer_[0], &layer_[1]}, 0, 0, fbW, fbH, false);
            if (kerr) kerrPass(cb, L, fbW, fbH, kdisc, glareOn);
            else lensPass(cb, L, fbW, fbH, 0, 0, disc, bend, false, glareOn, 0);
            end(cb);
        }
        skyDrawn_ = ksky;
        maskValid_ = ksky;
        last_ = L; last_.table = nullptr; last_.discU = last_.discP = last_.discCol = nullptr;
        last_.kA = last_.kB = last_.kEdge = last_.kTau = last_.discF = nullptr; lastPtW_ = ptW; lensValid_ = true; redraws_++;
        if (guardState_ == 0 && prog_[pKerr].state == 2) { G.begin("the first full picture"); firstTicket_ = c_->pending(); guardState_ = 1; }
        glowValid_ = false;
        glareValid_ = false;
        composed_ = false;
        if (glow) makeGlow(cb, L, fbW, fbH);
        if (glareOn && !L.glareAsync) makeGlare(L, fbW, fbH);
        if (glareOn && L.glareAsync) glareWant(L, fbW, fbH);
        else want_.gen = 0;
        (void)layers;
    }
    if (glareOn && L.glareAsync) glarePump();
    else if (glareStage_ != 0 || want_.gen != 0) { want_.gen = 0; glarePump(); }
    cb = c_->cmd();                                                    // (makeGlare submitted the batch it drew in: the rest goes into a new one)
    if (!cb) return;
    if (!composed_) compose(cb, L, fbW, fbH);
    else pictureView_ = out_.img && ((glareOn && glareValid_) || (glow && glowValid_)) ? out_.view : lensTex_.view;
    // the frame reads the picture and the mask in its fragment shaders
    Tex& pic = pictureView_ == out_.view && out_.img ? out_ : lensTex_;
    use(cb, pic, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    if (maskValid_) use(cb, mask_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

// 0.56.0's skyProbe (the start-up check, the tests): its uMode 1..4 outputs side by side, the light check (uMode 11) above them; the probe's own picture
// has one point per pixel
void VkBlackHole::probe(const LensSky& L, int w, int h, bool queue, std::vector<float>* out) {
    if (!ok_ || !skyReady() || w <= 0 || h <= 0) return;
    VkCommandBuffer cb = c_->cmd();
    if (!cb) return;
    if (probeW_ != w || probeH_ != h || !probe_.img) {
        if (!makeTex(probe_, 4 * w, 2 * h, kF32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "the probe")) return;
        probeW_ = w; probeH_ = h;
    }
    if (L.discCol && L.discC >= 2 && L.colVersion != colVer_) { upload(cb, discC_, L.discC, 1, kF32, L.discCol->data(), 3, "the colours"); colVer_ = L.colVersion; }
    const double w0 = ptW_, h0 = ptH_;
    ptW_ = w; ptH_ = h;
    begin(cb, {&probe_}, 0, 0, 4 * w, 2 * h, true);
    for (int m = 1; m <= 4; m++) skyPass(cb, L, w, h, (m - 1) * w, 0, 1, m, false, 1, (m - 1) * w, 0, w, h);
    if (L.discCol && L.discC >= 2) {
        for (int m = 0; m < 2; m++) {
            LensSky Lm = L;
            Lm.allInvTsun = m == 0 ? 0 : L.allInvTsun;
            Lm.lp[0] = static_cast<float>(m * w); Lm.lp[1] = static_cast<float>(h); Lm.lp[2] = static_cast<float>(w); Lm.lp[3] = static_cast<float>(w * h);
            skyPass(cb, Lm, w, h, m * w, h, 1, 11, false, 1, m * w, h, w, h);
        }
    }
    end(cb);
    ptW_ = w0; ptH_ = h0;
    readBack(cb, probe_, 0, 0, 4 * w, 2 * h, probeBuf_);
    if (queue) { c_->flush(false); return; }                           // (submitted now: read in a later frame)
    c_->flush(true);
    probeBuf_.busy = false;
    const std::size_t nf = static_cast<std::size_t>(4 * w) * static_cast<std::size_t>(2 * h) * 4;
    if (out && probeBuf_.b.map) { const float* v = static_cast<const float*>(probeBuf_.b.map); out->assign(v, v + nf); }
}

bool VkBlackHole::probeDone() {
    if (!probeBuf_.busy) return false;
    if (c_->pending() == probeBuf_.ticket) c_->flush(false);
    return c_->done(probeBuf_.ticket);
}

void VkBlackHole::probeRead(std::vector<float>& out) {
    out.clear();
    if (!probeBuf_.busy || !probeBuf_.b.map) return;
    probeBuf_.busy = false;
    const std::size_t nf = static_cast<std::size_t>(4 * probeW_) * static_cast<std::size_t>(2 * probeH_) * 4;
    const float* v = static_cast<const float*>(probeBuf_.b.map);
    out.assign(v, v + nf);
}

// =====================================================================================================================================================
// the light paths (kKerrTableCS) on Vulkan: render/kerr_compute.cpp's KerrCompute with the card programs' engine
// =====================================================================================================================================================
std::size_t vkKerrSpirvBytes() { return sizeof k_r2d_kerrtable_comp; }

namespace {
class VkKerr : public IKerrCompute {
public:
    VkKerr(VkCards& c, VkPipelineCache cache) : c_(c), cache_(cache) {}
    ~VkKerr() override {
        if (th_.joinable()) th_.join();
        if (c_.ctx().device) vkDeviceWaitIdle(c_.ctx().device);
        for (VkCards::Buf& b : buf_) c_.dropBuffer(b);
        c_.dropKernel(k_);
        c_.collect();
    }
    void start(KerrSteps* steps) {
        hang_ = std::getenv("PL_BH_CARDHANG") != nullptr;
        state_ = 1;
        th_ = std::thread([this, steps] {
            const std::string step = "the light paths' pipeline";
            if (steps) steps->begin(step);
            const double t0 = nowMs();
            std::string w;
            bool ok = false;
            if (VkBlackHole::forced("lightpaths")) w = "made to fail on purpose (PL_VK_CARDFAIL=lightpaths)";
            else ok = c_.makeKernel(k_r2d_kerrtable_comp, sizeof k_r2d_kerrtable_comp, k_, w, cache_);
            ms_ = nowMs() - t0;
            char b[160];
            std::snprintf(b, sizeof b, "%s in %.0f ms (SPIR-V %zu bytes)", ok ? "made" : "FAILED", ms_, sizeof k_r2d_kerrtable_comp);
            if (steps) steps->end(step, ok ? std::string(b) : std::string(b) + ": " + w);
            std::printf("PLVKBH pipeline lightpaths %s ms=%.1f spirv=%zu\n", ok ? "made" : "FAILED", ms_, sizeof k_r2d_kerrtable_comp);
            std::fflush(stdout);
            why_ = w;
            state_ = ok ? 2 : -1;
        });
    }
    bool building() const override { return state_ == 1; }
    int state() const { return state_; }
    const std::string& why() const { return why_; }
    void join() { if (th_.joinable() && state_ != 1) th_.join(); }

    void passA(const std::vector<double>& par, const std::vector<double>& chi) override {
        nA_ = static_cast<int>(chi.size());
        put(kPar, par); put(kInA, chi);
        room(kEdgeA, VkDeviceSize(nA_) * 8, true); room(kOutA, VkDeviceSize(nA_) * 80, true);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        run(0, nA_, 0, kPar, kInA, kEdgeA, kOutA);
        run(2, nA_, 0, kPar, kInA, kEdgeA, kOutA);
        fence();
    }
    bool rowsOnly(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) {
        if (mapped_) return false;
        nChi_ = static_cast<int>(rowChi.size()); nS_ = nS;
        const int n = nodes_ = 2 * nChi_ * nS_;
        put(kPar, par); put(kInB, rowChi);
        room(kEdgeB, VkDeviceSize(nChi_) * 8, true); room(kOutB, VkDeviceSize(n) * 80, false); room(kHost, VkDeviceSize(n) * 80, true);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        run(0, nChi_, 0, kPar, kInB, kEdgeB, kOutB);
        return true;
    }
    void passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) override {
        if (!rowsOnly(par, rowChi, nS)) return;
        for (int first = 0; first < nodes_; first += kPiece) run(3, nodes_ - first < kPiece ? nodes_ - first : kPiece, first, kPar, kInB, kEdgeB, kOutB);
        copyOut();
        fence();
    }
    bool passBRows(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) override { if (!rowsOnly(par, rowChi, nS)) return false; fence(); return true; }
    void passBPiece(int first, int n) override { run(3, n, first, kPar, kInB, kEdgeB, kOutB); fence(); }
    void passBCopy() override { copyOut(); fence(); }
    bool done(bool wait) override {
        if (!ticket_) return true;
        const double t0 = nowMs();
        if (hang_) {                                                    // (PL_BH_CARDHANG, tests: as if the fence never signalled)
            if (!wait) return false;
            while (nowMs() - t0 < kWaitLimitMs) std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return false;
        }
        if (c_.done(ticket_)) { ticket_ = 0; return !c_.failed(); }
        if (!wait) return false;
        if (!c_.waitFor(ticket_, kWaitLimitMs / 1000)) return false;
        ticket_ = 0;
        return !c_.failed();
    }
    double queuedMs() const override { return ticket_ ? nowMs() - fenceAt_ : 0; }
    void queueRays(const std::vector<double>& par, const std::vector<double>& psiChi) override {
        const int n = nA_ = static_cast<int>(psiChi.size() / 2);
        put(kPar, par); put(kInA, psiChi);
        room(kEdgeA, 8, true); room(kOutA, VkDeviceSize(n) * 128, true);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        run(1, n, 0, kPar, kInA, kEdgeA, kOutA);
        fence();
    }
    void readRays(std::vector<double>& out) override { out.assign(static_cast<std::size_t>(nA_) * 16, 0.0); copy(kOutA, out); }
    void queueEdges(const std::vector<double>& par, const std::vector<double>& chi) override {
        const int n = nA_ = static_cast<int>(chi.size());
        put(kPar, par); put(kInA, chi);
        room(kEdgeA, VkDeviceSize(n) * 8, true); room(kOutA, 8, true);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        run(0, n, 0, kPar, kInA, kEdgeA, kOutA);
        fence();
    }
    void readEdges(std::vector<double>& out) override { out.assign(static_cast<std::size_t>(nA_), 0.0); copy(kEdgeA, out); }
    void queueMath(const std::vector<double>& par, const std::vector<double>& x) override {
        const int n = nA_ = static_cast<int>(x.size());
        put(kPar, par); put(kInA, x);
        room(kEdgeA, 8, true); room(kOutA, VkDeviceSize(n) * 64, true);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        run(4, n, 0, kPar, kInA, kEdgeA, kOutA);
        fence();
    }
    void readMath(std::vector<double>& out) override { out.assign(static_cast<std::size_t>(nA_) * 8, 0.0); copy(kOutA, out); }
    void readA(std::vector<double>& edge, std::vector<double>& probe) override {
        edge.assign(static_cast<std::size_t>(nA_), 0.0); probe.assign(static_cast<std::size_t>(nA_) * 10, 0.0);
        copy(kEdgeA, edge); copy(kOutA, probe);
    }
    const double* mapB(std::vector<double>& rowEdge) override {
        rowEdge.assign(static_cast<std::size_t>(nChi_), 0.0);
        copy(kEdgeB, rowEdge);
        mapped_ = buf_[kHost].map != nullptr;
        return static_cast<const double*>(buf_[kHost].map);
    }
    void release() override { mapped_ = false; }
    bool mapped() const override { return mapped_; }
    void rays(const std::vector<double>& par, const std::vector<double>& psiChi, std::vector<double>& out) override {
        queueRays(par, psiChi);
        if (done(true)) readRays(out); else out.assign(psiChi.size() / 2 * 16, std::nan(""));
    }
    void edges(const std::vector<double>& par, const std::vector<double>& chi, std::vector<double>& out) override {
        queueEdges(par, chi);
        if (done(true)) readEdges(out); else out.assign(chi.size(), std::nan(""));
    }
    void math(const std::vector<double>& par, const std::vector<double>& x, std::vector<double>& out) override {
        queueMath(par, x);
        if (done(true)) readMath(out); else out.assign(x.size() * 8, std::nan(""));
    }
    double ms() const { return ms_; }

private:
    enum { kPar, kInA, kEdgeA, kOutA, kInB, kEdgeB, kOutB, kHost, kBufs };
    VkCards& c_;
    VkPipelineCache cache_;
    VkCards::Kernel k_;
    std::thread th_;
    std::atomic<int> state_{0};
    std::string why_;
    double ms_ = 0, fenceAt_ = 0;
    VkCards::Buf buf_[kBufs];
    std::uint64_t ticket_ = 0;
    int nA_ = 0, nChi_ = 0, nS_ = 0;
    bool mapped_ = false, hang_ = false;

    // the inputs on the card (copied in by the batch); the outputs the processor reads in memory it can see (kOutB on the card, copied to kHost)
    void room(int b, VkDeviceSize bytes, bool host) {
        bytes = std::max<VkDeviceSize>(bytes, 16);
        if (buf_[b].b && buf_[b].size >= bytes) return;
        c_.dropBuffer(buf_[b]);
        VkBufferUsageFlags use = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        c_.makeBuffer(bytes, use, host, buf_[b], "the light paths' buffer");
    }
    void put(int b, const std::vector<double>& v) {
        room(b, VkDeviceSize(v.size() * sizeof(double)), false);
        if (!v.empty()) c_.copyIn(v.data(), v.size() * sizeof(double), buf_[b], 0);
    }
    void run(int mode, int n, int first, int par, int in, int edge, int out) {
        if (!k_.pipe) return;
        k_.u.i("uMode", &mode, 1); k_.u.i("uN", &n, 1); k_.u.i("uFirst", &first, 1); k_.u.i("uNChi", &nChi_, 1); k_.u.i("uNS", &nS_, 1);
        c_.dispatch(k_, {&buf_[par], &buf_[in], &buf_[edge], &buf_[out]}, static_cast<std::uint32_t>(n > 0 ? (n + 63) / 64 : 0));
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToCompute | VkCards::kToHost | VkCards::kToTransfer);   // (as glMemoryBarrier)
    }
    void copyOut() {
        const VkCommandBuffer cb = c_.cmd();
        if (!cb || !buf_[kOutB].b || !buf_[kHost].b) return;
        const VkBufferCopy cp{0, 0, VkDeviceSize(nodes_) * 80};
        if (nodes_ > 0) vkCmdCopyBuffer(cb, buf_[kOutB].b, buf_[kHost].b, 1, &cp);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToHost);
    }
    void fence() { ticket_ = c_.flush(false); fenceAt_ = nowMs(); }
    void copy(int b, std::vector<double>& out) {
        if (!buf_[b].map || out.empty()) return;
        std::memcpy(out.data(), buf_[b].map, std::min<std::size_t>(out.size() * sizeof(double), static_cast<std::size_t>(buf_[b].size)));
    }
};
}  // namespace

IKerrCompute* makeVkKerrCompute(VkCards& c, VkPipelineCache cache, KerrSteps* steps, std::string& why) {
    if (!c.float64()) { why = "this card has no double precision in its shaders (shaderFloat64)"; return nullptr; }
    VkKerr* k = new VkKerr(c, cache);
    k->start(steps);
    return k;
}
// the light paths' state on Vulkan: 1 its pipeline being made, 2 made, -1 failed (why)
int vkKerrState(IKerrCompute* k, std::string& why) {
    VkKerr* v = dynamic_cast<VkKerr*>(k);
    if (!v) return 2;
    if (v->state() == -1) { v->join(); why = "Vulkan did not make the light paths' pipeline: " + v->why(); }
    return v->state();
}

}  // namespace render
