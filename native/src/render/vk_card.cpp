// 0.61.0: Vulkan, step 3 of 5: the card programs on Vulkan (render/vk_card.h has the how).
#include "app/frame_cost.h"
#include "render/vk_card.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/gpu_guard.h"
#include "atom_card.h"
#include "group_compute.h"
#include "molecule_card.h"
#include "outline_compute.h"
#include "renderer2d.h"
#include "rock_compute.h"
#include "shaders.h"

#include "r2d_groups_comp.h"
#include "r2d_outdens_comp.h"
#include "r2d_outlevel_comp.h"
#include "r2d_rocks_comp.h"
#include "r2d_slice_comp.h"
#include "r2d_jump_comp.h"
#include "r2d_moldots_comp.h"
#include "r2d_cgrid_vert.h"
#include "r2d_mgrid_vert.h"
#include "r2d_grid_frag.h"

namespace render {

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::string msText(double s) { char b[48]; std::snprintf(b, sizeof b, "%.0f ms", 1000 * s); return b; }
// PL_VK_CARDFAIL=<name>[,<name>] (tests): that program's pipeline fails as a driver that says no would
bool forcedFail(const char* name) {
    const char* e = std::getenv("PL_VK_CARDFAIL");
    if (!e) return false;
    const std::string list = std::string(",") + e + ",";
    return list.find(std::string(",") + name + ",") != std::string::npos;
}
constexpr std::uint32_t kUboBinding = 7;                     // the compute shaders' uniform block (CMakeLists.txt: --sdub uDefault 0 7)
constexpr int kSlots = 8;                                    // batches in flight at most
constexpr double kStepLimitS = 10.0;                         // a start-up step's time limit (as the Atom Lab's card steps)
}  // namespace

std::string vkCardId(const char* name, std::initializer_list<const char*> texts) {
    unsigned long long h = 1469598103934665603ULL;           // FNV-1a, 64 bit
    for (const char* t : texts) for (const char* c = t; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[64];
    std::snprintf(b, sizeof b, "vulkan %s %016llx", name, h);
    return b;
}

std::string vkCardVersion(const std::string& name) {
    if (name == "groups") return vkCardId("groups", {kGroupsCS});
    if (name == "outline") return vkCardId("outline", {kOutlineDensityCS, kOutlineLevelCS});
    if (name == "rocks") return vkCardId("rocks", {kRocksCS});
    if (name == "cloud") return vkCardId("cloud", {kCloudVS, kCloudGridVS, kCloudGridFS, kBlobFS});
    if (name == "slice") return "vulkan " + sliceShaderId();
    if (name == "jump") return "vulkan " + jumpShaderId();
    if (name == "lightpaths") return vkCardId("lightpaths", {kKerrTableCS});                       // 0.62.0 (step 4): the Black Hole Lab's
    if (name == "sky") return vkCardId("sky", {kKerrSkyTinyFS, kKerrSkyHeadPart, kEllipticPart, kKerrSkyMathPart, kKerrSkyTracePart, kKerrSkyMainPart});
    if (name == "lens") return vkCardId("lens", {kCompositeVS, kLensFS, kKerrFS, kMaxDownFS, kGlowDownFS, kGlowBlurFS, kGlowAddFS, kGlareDownFS, kGlareAddFS, kCompositeFS});
    return "vulkan " + molCloudShaderId();
}

void VkUniforms::set(const char* name, const void* v, std::size_t compBytes, int comps, int count) {
    const auto it = blk.m.find(name);
    if (it == blk.m.end()) return;                           // (a name the program does not use, as glUniform* with location -1)
    const VkDraw2D::Member& m = it->second;
    const std::size_t n = compBytes * static_cast<std::size_t>(comps);
    for (int i = 0; i < count; i++) {
        const std::size_t at = m.offset + static_cast<std::size_t>(i) * m.arrayStride;
        if (at + n > blk.data.size()) break;
        std::memcpy(blk.data.data() + at, static_cast<const std::uint8_t*>(v) + static_cast<std::size_t>(i) * n, n);
        if (m.arrayStride == 0) break;
    }
}

// =====================================================================================================================================================
// the engine
// =====================================================================================================================================================
VkCards::~VkCards() { destroy(); }

bool VkCards::init(const VkDevCtx& c, VkDraw2D* d2, bool float64, std::string& why) {
    c_ = c;
    d2_ = d2;
    float64_ = float64;
    if (!c_.compute) { why = "the window's queue on this card does not run compute shaders"; return false; }
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = c_.family;
    if (!c_.check(vkCreateCommandPool(c_.device, &pi, nullptr, &pool_), "vkCreateCommandPool (card programs)")) { pool_ = VK_NULL_HANDLE; why = "vkCreateCommandPool failed"; return false; }
    ok_ = true;
    return true;
}

void VkCards::freeBuffer(Buf& b) {
    if (b.m && b.map) vkUnmapMemory(c_.device, b.m);
    if (b.b) vkDestroyBuffer(c_.device, b.b, nullptr);
    if (b.m) vkFreeMemory(c_.device, b.m, nullptr);
    b = Buf();
}

void VkCards::freeGrid(Grid& g) {
    if (g.view) vkDestroyImageView(c_.device, g.view, nullptr);
    if (g.img) vkDestroyImage(c_.device, g.img, nullptr);
    if (g.m) vkFreeMemory(c_.device, g.m, nullptr);
    freeBuffer(g.host);
    g = Grid();
}

void VkCards::destroy() {
    if (!c_.device) return;
    vkDeviceWaitIdle(c_.device);
    for (auto& d : drop_) d.second();
    drop_.clear();
    for (Slot& s : slots_) {
        for (VkDescriptorPool p : s.pools) vkDestroyDescriptorPool(c_.device, p, nullptr);
        freeBuffer(s.ubo); freeBuffer(s.stage);
        for (Buf& b : s.spare) freeBuffer(b);
        if (s.fence) vkDestroyFence(c_.device, s.fence, nullptr);
    }
    slots_.clear();
    if (tq_) { vkDestroyQueryPool(c_.device, tq_, nullptr); tq_ = VK_NULL_HANDLE; }
    timing_ = false; times_.clear();
    for (Grid& g : grids_) freeGrid(g);
    grids_.clear();
    for (VkPipeline& p : gridPipe_) if (p) { vkDestroyPipeline(c_.device, p, nullptr); p = VK_NULL_HANDLE; }
    if (gridLayout_) vkDestroyPipelineLayout(c_.device, gridLayout_, nullptr);
    if (gridDsl_) vkDestroyDescriptorSetLayout(c_.device, gridDsl_, nullptr);
    gridLayout_ = VK_NULL_HANDLE; gridDsl_ = VK_NULL_HANDLE;
    if (pool_) vkDestroyCommandPool(c_.device, pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
    open_ = -1;
    ok_ = false;
    c_ = VkDevCtx();
}

bool VkCards::makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buf& b, const char* what) {
    b = Buf();
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = std::max<VkDeviceSize>(size, 16);
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!c_.check(vkCreateBuffer(c_.device, &bi, nullptr, &b.b), what)) { b.b = VK_NULL_HANDLE; failed_ = true; return false; }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(c_.device, b.b, &req);
    // host: coherent (and cached when the card offers it: the read-backs); else on the card
    std::uint32_t type = UINT32_MAX;
    const VkMemoryPropertyFlags hostWant = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkMemoryPropertyFlags tries[2] = {host ? hostWant | VK_MEMORY_PROPERTY_HOST_CACHED_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, host ? hostWant : VkMemoryPropertyFlags(0)};
    for (const VkMemoryPropertyFlags want : tries)
        for (std::uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++)
            if ((req.memoryTypeBits & (1u << i)) && (c_.mem.memoryTypes[i].propertyFlags & want) == want) type = i;
    if (type == UINT32_MAX) { c_.check(VK_ERROR_FEATURE_NOT_PRESENT, what); vkDestroyBuffer(c_.device, b.b, nullptr); b.b = VK_NULL_HANDLE; failed_ = true; return false; }
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (!c_.check(vkAllocateMemory(c_.device, &mi, nullptr, &b.m), what)) { b.m = VK_NULL_HANDLE; vkDestroyBuffer(c_.device, b.b, nullptr); b.b = VK_NULL_HANDLE; failed_ = true; return false; }
    if (!c_.check(vkBindBufferMemory(c_.device, b.b, b.m, 0), what)) { freeBuffer(b); failed_ = true; return false; }
    if (host && !c_.check(vkMapMemory(c_.device, b.m, 0, VK_WHOLE_SIZE, 0, &b.map), what)) { b.map = nullptr; freeBuffer(b); failed_ = true; return false; }
    b.size = size;
    return true;
}

void VkCards::dropBuffer(Buf& b) {
    if (!b.b && !b.m) return;
    const VkDevice dev = c_.device;
    const Buf x = b;
    drop_.push_back({open_ >= 0 ? nextTicket_ : nextTicket_ - 1, [dev, x] { if (x.m && x.map) vkUnmapMemory(dev, x.m); if (x.b) vkDestroyBuffer(dev, x.b, nullptr); if (x.m) vkFreeMemory(dev, x.m, nullptr); }});
    b = Buf();
}

void VkCards::collect() {
    std::size_t k = 0;
    for (std::size_t i = 0; i < drop_.size(); i++) {
        if (done(drop_[i].first)) {
            if (d2_) d2_->later(std::move(drop_[i].second));   // a frame on the card may still use it: once those frames are done
            else drop_[i].second();
        } else drop_[k++] = std::move(drop_[i]);
    }
    drop_.resize(k);
}

bool VkCards::openSlot() {
    if (open_ >= 0) return true;
    if (!ok_ || failed_) return false;
    if (timing_) readTimes();                                 // (0.63.0: a finished batch's times kept before its slot is used again)
    int pick = -1;
    for (int i = 0; i < static_cast<int>(slots_.size()) && pick < 0; i++) {
        const Slot& s = slots_[static_cast<std::size_t>(i)];
        if (!s.submitted || vkGetFenceStatus(c_.device, s.fence) == VK_SUCCESS) pick = i;
    }
    if (pick < 0 && static_cast<int>(slots_.size()) < kSlots) {
        Slot s;
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool_;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (!c_.check(vkAllocateCommandBuffers(c_.device, &ai, &s.cmd), "vkAllocateCommandBuffers (card programs)") ||
            !c_.check(vkCreateFence(c_.device, &fi, nullptr, &s.fence), "vkCreateFence (card programs)")) { failed_ = true; return false; }
        if (!makeBuffer(1u << 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, s.ubo, "the card programs' uniforms")) return false;
        if (!makeBuffer(1u << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, s.stage, "the card programs' staging buffer")) return false;
        slots_.push_back(s);
        pick = static_cast<int>(slots_.size()) - 1;
    }
    if (pick < 0) {                                           // every batch still on the card (rare): wait for the oldest
        std::uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < static_cast<int>(slots_.size()); i++) if (slots_[static_cast<std::size_t>(i)].ticket < oldest) { oldest = slots_[static_cast<std::size_t>(i)].ticket; pick = i; }
        app::FrameCostLabel l("the oldest card batch (every batch slot was still on the card)");
        app::FrameCostSpan span(app::FrameCostKind::Readback);   // (0.63.1: the benchmark's worst frames)
        if (!c_.check(vkWaitForFences(c_.device, 1, &slots_[static_cast<std::size_t>(pick)].fence, VK_TRUE, UINT64_MAX), "vkWaitForFences (card programs)")) { failed_ = true; return false; }
    }
    Slot& s = slots_[static_cast<std::size_t>(pick)];
    if (s.submitted) {
        doneTicket_ = std::max(doneTicket_, s.ticket);
        if (!c_.check(vkResetFences(c_.device, 1, &s.fence), "vkResetFences (card programs)")) { failed_ = true; return false; }
        s.submitted = false;
    }
    for (VkDescriptorPool p : s.pools) vkResetDescriptorPool(c_.device, p, 0);
    s.pool = 0; s.uboUsed = 0; s.stageUsed = 0;
    for (Buf& b : s.spare) freeBuffer(b);
    s.spare.clear();
    if (!c_.check(vkResetCommandBuffer(s.cmd, 0), "vkResetCommandBuffer (card programs)")) { failed_ = true; return false; }
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!c_.check(vkBeginCommandBuffer(s.cmd, &bi), "vkBeginCommandBuffer (card programs)")) { failed_ = true; return false; }
    // after everything submitted before it: the frames that drew from buffers this batch may write again (their vertex input and shaders, write after
    // read), the batches whose writes this one reads (compute, transfer)
    vkMemoryBarrier2(s.cmd, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                     VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                     VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);   // (0.63.0: synchronization2)
    s.timed = false;
    if (timing_ && tq_) {                                     // 0.63.0: the benchmark's timer: the batch's start
        const std::uint32_t q = 2u * static_cast<std::uint32_t>(pick);
        vkCmdResetQueryPool(s.cmd, tq_, q, 2);
        vkCmdWriteTimestamp2(s.cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, tq_, q);
        s.timed = true;
    }
    open_ = pick;
    return true;
}

bool VkCards::setTiming(bool on) {
    if (!on) { timing_ = false; times_.clear(); return true; }
    if (!tq_) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(c_.phys, &p);
        std::uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(c_.phys, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vkGetPhysicalDeviceQueueFamilyProperties(c_.phys, &nq, qf.data());
        const std::uint32_t bits = c_.family < nq ? qf[c_.family].timestampValidBits : 0;
        if (bits == 0 || !(p.limits.timestampPeriod > 0)) return false;
        tsNs_ = p.limits.timestampPeriod;
        tsMask_ = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        VkQueryPoolCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qi.queryCount = 2u * static_cast<std::uint32_t>(kSlots);
        if (!c_.check(vkCreateQueryPool(c_.device, &qi, nullptr, &tq_), "vkCreateQueryPool (the benchmark's batch times)")) { tq_ = VK_NULL_HANDLE; return false; }
    }
    timing_ = true;
    return true;
}

void VkCards::readTimes() {
    if (!tq_) return;
    for (std::size_t i = 0; i < slots_.size(); i++) {
        Slot& s = slots_[i];
        if (!s.timed || !s.submitted || vkGetFenceStatus(c_.device, s.fence) != VK_SUCCESS) continue;
        if (!s.frame) { s.timed = false; continue; }          // (0.63.1: a worker's batch: not the frame's)
        std::uint64_t r[4] = {0, 0, 0, 0};                    // (value, available) twice
        const VkResult res = vkGetQueryPoolResults(c_.device, tq_, 2u * static_cast<std::uint32_t>(i), 2, sizeof r, r, 2 * sizeof(std::uint64_t),
                                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if (res == VK_NOT_READY || !r[1] || !r[3]) continue;
        s.timed = false;
        if (res != VK_SUCCESS) continue;
        times_[s.ticket] = {r[0] & tsMask_, r[2] & tsMask_};
    }
    while (times_.size() > 4096) times_.erase(times_.begin());
}

bool VkCards::batchTimes(std::uint64_t first, std::uint64_t last, double& busyMs, double& t0Ns, double& t1Ns) {
    busyMs = 0; t0Ns = -1; t1Ns = -1;
    if (last < first) return true;
    readTimes();
    // (0.63.1: only the frame thread's batches: a worker's long batch in the range no longer holds the frame's result back)
    for (const Slot& s : slots_) if (s.frame && s.timed && s.submitted && s.ticket >= first && s.ticket <= last) return false;   // (its times not yet there)
    for (auto it = times_.lower_bound(first); it != times_.end() && it->first <= last; it = times_.erase(it)) {
        const double a = static_cast<double>(it->second.first) * tsNs_, b = static_cast<double>(it->second.second) * tsNs_;
        if (it->second.first == 0 || it->second.second < it->second.first || (b - a) / 1e6 >= 1e4) { busyMs = -1; continue; }   // 0.63.1: not a time: the frame is thrown away
        if (busyMs >= 0) busyMs += (b - a) / 1e6;
        if (t0Ns < 0 || a < t0Ns) t0Ns = a;
        if (b > t1Ns) t1Ns = b;
    }
    while (!times_.empty() && times_.begin()->first < first) times_.erase(times_.begin());
    return true;
}

VkBuffer VkCards::stageBuffer() const { return open_ >= 0 ? slots_[static_cast<std::size_t>(open_)].stage.b : VK_NULL_HANDLE; }

VkCommandBuffer VkCards::cmd() { return openSlot() ? slots_[static_cast<std::size_t>(open_)].cmd : VK_NULL_HANDLE; }

VkDeviceSize VkCards::stage(const void* data, std::size_t bytes) {
    if (!openSlot()) return 0;
    Slot& s = slots_[static_cast<std::size_t>(open_)];
    VkDeviceSize at = (s.stageUsed + 15u) / 16u * 16u;
    if (at + bytes > s.stage.size) {                          // a bigger one; the old one is kept until this batch is done (copies recorded from it)
        Buf nb;
        if (!makeBuffer(std::max<VkDeviceSize>(2 * s.stage.size, VkDeviceSize(bytes) + (1u << 20)), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, nb, "the card programs' staging buffer")) return 0;
        s.spare.push_back(s.stage);
        s.stage = nb;
        at = 0;
    }
    std::memcpy(static_cast<std::uint8_t*>(s.stage.map) + at, data, bytes);
    s.stageUsed = at + bytes;
    return at;
}

void VkCards::copyIn(const void* data, std::size_t bytes, const Buf& dst, VkDeviceSize at) {
    if (!bytes || !dst.b) return;
    const VkDeviceSize from = stage(data, bytes);
    if (open_ < 0 || failed_) return;
    Slot& s = slots_[static_cast<std::size_t>(open_)];
    const VkBufferCopy cp{from, at, bytes};
    vkCmdCopyBuffer(s.cmd, s.stage.b, dst.b, 1, &cp);
}

void VkCards::barrier(VkPipelineStageFlags by, unsigned to) {
    if (!openSlot()) return;
    VkPipelineStageFlags dst = 0;
    VkAccessFlags da = 0;
    if (to & kToCompute) { dst |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; da |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT; }
    if (to & kToVertex) { dst |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; da |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT; }
    if (to & kToHost) { dst |= VK_PIPELINE_STAGE_HOST_BIT; da |= VK_ACCESS_HOST_READ_BIT; }
    if (to & kToTransfer) { dst |= VK_PIPELINE_STAGE_TRANSFER_BIT; da |= VK_ACCESS_TRANSFER_READ_BIT; }
    vkMemoryBarrier2(slots_[static_cast<std::size_t>(open_)].cmd, static_cast<VkPipelineStageFlags2>(by),        // (0.63.0: synchronization2; the legacy stage and
                     by == VK_PIPELINE_STAGE_TRANSFER_BIT ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_SHADER_WRITE_BIT,    // access bits have the same values in it)
                     static_cast<VkPipelineStageFlags2>(dst), static_cast<VkAccessFlags2>(da));
}

bool VkCards::uniformBlock(const VkUniforms& u, VkDescriptorBufferInfo& bi) {
    if (!openSlot()) return false;
    Slot& s = slots_[static_cast<std::size_t>(open_)];
    const VkDeviceSize align = std::max<VkDeviceSize>(16, c_.limits.minUniformBufferOffsetAlignment);
    const VkDeviceSize n = std::max<VkDeviceSize>(16, u.blk.size);
    VkDeviceSize at = (s.uboUsed + align - 1) / align * align;
    if (at + n > s.ubo.size) {
        Buf nb;
        if (!makeBuffer(std::max<VkDeviceSize>(2 * s.ubo.size, n + 4096), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, nb, "the card programs' uniforms")) return false;
        s.spare.push_back(s.ubo);
        s.ubo = nb;
        at = 0;
    }
    std::memset(static_cast<std::uint8_t*>(s.ubo.map) + at, 0, n);
    if (u.blk.size) std::memcpy(static_cast<std::uint8_t*>(s.ubo.map) + at, u.blk.data.data(), u.blk.size);
    s.uboUsed = at + n;
    bi.buffer = s.ubo.b;
    bi.offset = at;
    bi.range = n;
    return true;
}

VkDescriptorSet VkCards::descriptorSet(VkDescriptorSetLayout dsl) {
    if (!openSlot()) return VK_NULL_HANDLE;
    Slot& s = slots_[static_cast<std::size_t>(open_)];
    for (int attempt = 0; attempt < 2; attempt++) {
        if (s.pool >= s.pools.size()) {
            const VkDescriptorPoolSize ps[3] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
                                                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256}};   // (0.62.0: the Black Hole Lab's textures)
            VkDescriptorPoolCreateInfo pi{};
            pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pi.maxSets = 64;
            pi.poolSizeCount = 3;
            pi.pPoolSizes = ps;
            VkDescriptorPool p = VK_NULL_HANDLE;
            if (!c_.check(vkCreateDescriptorPool(c_.device, &pi, nullptr, &p), "vkCreateDescriptorPool (card programs)")) { failed_ = true; return VK_NULL_HANDLE; }
            s.pools.push_back(p);
        }
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = s.pools[s.pool];
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &dsl;
        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult r = vkAllocateDescriptorSets(c_.device, &ai, &set);
        if (r == VK_SUCCESS) return set;
        if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL) { c_.check(r, "vkAllocateDescriptorSets (card programs)"); failed_ = true; return VK_NULL_HANDLE; }
        s.pool++;                                             // this pool is full: the next (made when needed)
    }
    c_.check(VK_ERROR_OUT_OF_POOL_MEMORY, "vkAllocateDescriptorSets (card programs)");
    failed_ = true;
    return VK_NULL_HANDLE;
}

bool VkCards::makeKernel(const std::uint32_t* words, std::size_t bytes, Kernel& k, std::string& why, VkPipelineCache cache) {
    k = Kernel();
    VkDraw2D::Shader sh;
    if (!reflectSpirv(words, bytes / 4, sh, why)) return false;
    if (!sh.block.m.empty() && sh.blockBinding != static_cast<int>(kUboBinding)) { why = "its uniform block is not at binding 7"; return false; }
    k.u.blk = sh.block;
    k.storage = sh.storage;
    std::sort(k.storage.begin(), k.storage.end());
    for (std::size_t i = 0; i < k.storage.size(); i++) if (k.storage[i] != static_cast<int>(i)) { why = "its storage buffers are not at bindings 0, 1, 2 ..."; return false; }
    std::vector<VkDescriptorSetLayoutBinding> b;
    for (int s : k.storage) b.push_back({static_cast<std::uint32_t>(s), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
    b.push_back({kUboBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = static_cast<std::uint32_t>(b.size());
    li.pBindings = b.data();
    if (!c_.check(vkCreateDescriptorSetLayout(c_.device, &li, nullptr, &k.dsl), "vkCreateDescriptorSetLayout (card program)")) { k.dsl = VK_NULL_HANDLE; why = "vkCreateDescriptorSetLayout failed"; return false; }
    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &k.dsl;
    if (!c_.check(vkCreatePipelineLayout(c_.device, &pli, nullptr, &k.pl), "vkCreatePipelineLayout (card program)")) { k.pl = VK_NULL_HANDLE; dropKernel(k); why = "vkCreatePipelineLayout failed"; return false; }
    VkShaderModuleCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    si.codeSize = bytes;
    si.pCode = words;
    if (!c_.check(vkCreateShaderModule(c_.device, &si, nullptr, &k.mod), "vkCreateShaderModule (card program)")) { k.mod = VK_NULL_HANDLE; dropKernel(k); why = "vkCreateShaderModule failed"; return false; }
    VkComputePipelineCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ci.stage.module = k.mod;
    ci.stage.pName = "main";
    ci.layout = k.pl;
    app::FrameCostSpan made(app::FrameCostKind::Pipeline);   // (0.63.1)
    if (!c_.check(vkCreateComputePipelines(c_.device, cache, 1, &ci, nullptr, &k.pipe), "vkCreateComputePipelines (card program)")) { k.pipe = VK_NULL_HANDLE; dropKernel(k); why = "vkCreateComputePipelines failed"; return false; }
    spirv_ += bytes;
    return true;
}

void VkCards::dropKernel(Kernel& k) {
    if (!c_.device || (!k.pipe && !k.mod && !k.pl && !k.dsl)) { k = Kernel(); return; }
    const VkDevice dev = c_.device;
    const VkPipeline pipe = k.pipe;
    const VkShaderModule mod = k.mod;
    const VkPipelineLayout pl = k.pl;
    const VkDescriptorSetLayout dsl = k.dsl;
    drop_.push_back({open_ >= 0 ? nextTicket_ : nextTicket_ - 1, [dev, pipe, mod, pl, dsl] {   // (a batch on the card may still use it)
        if (pipe) vkDestroyPipeline(dev, pipe, nullptr);
        if (mod) vkDestroyShaderModule(dev, mod, nullptr);
        if (pl) vkDestroyPipelineLayout(dev, pl, nullptr);
        if (dsl) vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
    }});
    k = Kernel();
}

bool VkCards::dispatch(Kernel& k, std::initializer_list<const Buf*> bufs, std::uint32_t gx, std::uint32_t gy, std::uint32_t gz) {
    if (!k.pipe || bufs.size() != k.storage.size() || !openSlot()) return false;
    const VkDescriptorSet set = descriptorSet(k.dsl);
    VkDescriptorBufferInfo ub{};
    if (!set || !uniformBlock(k.u, ub)) return false;
    std::vector<VkDescriptorBufferInfo> bi(bufs.size());
    std::vector<VkWriteDescriptorSet> wr(bufs.size() + 1);
    std::size_t i = 0;
    for (const Buf* b : bufs) {
        if (!b || !b->b) return false;
        bi[i] = {b->b, 0, VK_WHOLE_SIZE};
        wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr[i].dstSet = set;
        wr[i].dstBinding = static_cast<std::uint32_t>(i);
        wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[i].pBufferInfo = &bi[i];
        i++;
    }
    wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[i].dstSet = set;
    wr[i].dstBinding = kUboBinding;
    wr[i].descriptorCount = 1;
    wr[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wr[i].pBufferInfo = &ub;
    vkUpdateDescriptorSets(c_.device, static_cast<std::uint32_t>(wr.size()), wr.data(), 0, nullptr);
    const VkCommandBuffer cb = slots_[static_cast<std::size_t>(open_)].cmd;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, k.pipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, k.pl, 0, 1, &set, 0, nullptr);
    if (gx && gy && gz) vkCmdDispatch(cb, gx, gy, gz);
    return true;
}

std::uint64_t VkCards::flush(bool wait) {
    gridsUsed_ = 0;
    if (open_ < 0) {
        const std::uint64_t last = nextTicket_ - 1;
        if (wait && last && !waitFor(last, 1e9)) return 0;
        return last;
    }
    Slot& s = slots_[static_cast<std::size_t>(open_)];
    const std::uint32_t q = 2u * static_cast<std::uint32_t>(open_);
    open_ = -1;
    if (failed_) return 0;
    if (s.timed && tq_) vkCmdWriteTimestamp2(s.cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, tq_, q + 1);   // 0.63.0: the batch's end
    if (!c_.check(vkEndCommandBuffer(s.cmd), "vkEndCommandBuffer (card programs)")) { failed_ = true; return 0; }
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s.cmd;
    if (!c_.check(vkQueueSubmit(c_.queue, 1, &si, s.fence), "vkQueueSubmit (card programs)")) { failed_ = true; return 0; }
    s.ticket = nextTicket_++;
    s.submitted = true;
    s.frame = app::frameCostFrameThread();                    // (0.63.1)
    if (wait) {
        app::FrameCostSpan span(app::FrameCostKind::Readback);   // (0.63.1)
        if (!c_.check(vkWaitForFences(c_.device, 1, &s.fence, VK_TRUE, UINT64_MAX), "vkWaitForFences (card programs)")) { failed_ = true; return 0; }
    }
    return s.ticket;
}

bool VkCards::done(std::uint64_t ticket) {
    if (ticket == 0 || ticket <= doneTicket_) return true;
    if (open_ >= 0 && ticket >= nextTicket_) return false;    // (the open batch: not submitted yet)
    // PL_VK_SYNC (picture tests only): a batch asked about is waited for, so its results arrive at the first frame that asks, every run (as OpenGL's
    // software driver does here); never in normal use
    static const bool sync = std::getenv("PL_VK_SYNC") != nullptr;
    for (const Slot& s : slots_)
        if (s.submitted && s.ticket == ticket) {
            const VkResult r = sync ? vkWaitForFences(c_.device, 1, &s.fence, VK_TRUE, UINT64_MAX) : vkGetFenceStatus(c_.device, s.fence);
            if (r == VK_SUCCESS) return true;
            if (r != VK_NOT_READY) { c_.check(r, "vkGetFenceStatus (card programs)"); failed_ = true; return true; }
            return false;
        }
    return true;                                              // (its slot was used again: it was done)
}

bool VkCards::waitFor(std::uint64_t ticket, double seconds) {
    for (const Slot& s : slots_)
        if (s.submitted && s.ticket == ticket) {
            app::FrameCostSpan span(app::FrameCostKind::Readback);   // (0.63.1)
            const VkResult r = vkWaitForFences(c_.device, 1, &s.fence, VK_TRUE, static_cast<std::uint64_t>(seconds * 1e9));
            if (r == VK_TIMEOUT) return false;
            if (r != VK_SUCCESS) { c_.check(r, "vkWaitForFences (card programs)"); failed_ = true; return false; }
            return true;
        }
    return done(ticket);
}

// ---- the exposure's grid ----
bool VkCards::makeGrid(int which, std::string& why) {
    which &= 1;
    if (gridPipe_[which]) return true;
    if (!ok_) { why = "the card programs have not started"; return false; }
    if (!gridLayout_) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(c_.phys, VK_FORMAT_R32_SFLOAT, &fp);
        const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
        if ((fp.optimalTilingFeatures & need) != need) { why = "this card cannot add up into an R32_SFLOAT picture (the exposure's grid, OpenGL's GL_R32F)"; return false; }
        VkDescriptorSetLayoutBinding b{2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = 1;
        li.pBindings = &b;
        if (!c_.check(vkCreateDescriptorSetLayout(c_.device, &li, nullptr, &gridDsl_), "vkCreateDescriptorSetLayout (grid)")) { gridDsl_ = VK_NULL_HANDLE; why = "vkCreateDescriptorSetLayout failed"; return false; }
        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &gridDsl_;
        if (!c_.check(vkCreatePipelineLayout(c_.device, &pli, nullptr, &gridLayout_), "vkCreatePipelineLayout (grid)")) { gridLayout_ = VK_NULL_HANDLE; why = "vkCreatePipelineLayout failed"; return false; }
    }
    const std::uint32_t* vw = which == 0 ? k_r2d_cgrid_vert : k_r2d_mgrid_vert;
    const std::size_t vb = which == 0 ? sizeof k_r2d_cgrid_vert : sizeof k_r2d_mgrid_vert;
    VkDraw2D::Shader vs, fs;
    if (!reflectSpirv(vw, vb / 4, vs, why) || !reflectSpirv(k_r2d_grid_frag, sizeof k_r2d_grid_frag / 4, fs, why)) return false;
    if (vs.blockBinding != 2) { why = "the grid's vertex shader has its uniform block elsewhere than binding 2"; return false; }
    for (const auto& [name, loc] : fs.in) { const auto it = vs.out.find(name); if (it == vs.out.end() || it->second != loc) { why = "the grid's stages do not meet at " + name; return false; } }
    gridU_[which].blk = vs.block;
    if (which == 0) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(c_.phys, VK_FORMAT_R8_USCALED, &fp);
        if (!(fp.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)) { why = "this card cannot read R8_USCALED vertex data (the dots' colour groups)"; return false; }
    }
    VkShaderModule mod[2] = {};
    for (int i = 0; i < 2; i++) {
        VkShaderModuleCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        si.codeSize = i ? sizeof k_r2d_grid_frag : vb;
        si.pCode = i ? k_r2d_grid_frag : vw;
        if (!c_.check(vkCreateShaderModule(c_.device, &si, nullptr, &mod[i]), "vkCreateShaderModule (grid)")) mod[i] = VK_NULL_HANDLE;
    }
    VkPipelineShaderStageCreateInfo st[2]{};
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = mod[0]; st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = mod[1]; st[1].pName = "main";
    std::vector<VkVertexInputBindingDescription> bind;
    std::vector<VkVertexInputAttributeDescription> attr;
    if (which == 0) {                                          // kCloudGridVS: aPos (location 1), aGroup (location 2, a byte as a float)
        bind.push_back({0, 12, VK_VERTEX_INPUT_RATE_VERTEX}); bind.push_back({1, 1, VK_VERTEX_INPUT_RATE_VERTEX});
        attr.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}); attr.push_back({2, 1, VK_FORMAT_R8_USCALED, 0});
    } else {                                                   // kMolGridVS: aDot (location 1)
        bind.push_back({0, 16, VK_VERTEX_INPUT_RATE_VERTEX});
        attr.push_back({1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0});
    }
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bind.size());
    vi.pVertexBindingDescriptions = bind.data();
    vi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attr.size());
    vi.pVertexAttributeDescriptions = attr.data();
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;            // (OpenGL: GL_POINTS, gl_PointSize 1)
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
    VkPipelineColorBlendAttachmentState ba{};                  // glBlendFunc(GL_ONE, GL_ONE): the footprints added up
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = ba.dstColorBlendFactor = ba.srcAlphaBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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
    gi.layout = gridLayout_;
    const VkFormat gridFormat = VK_FORMAT_R32_SFLOAT;             // 0.63.0: dynamic rendering (no render pass)
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &gridFormat;
    gi.pNext = &ri;
    app::FrameCostSpan made(app::FrameCostKind::Pipeline);   // (0.63.1)
    bool ok = mod[0] && mod[1] && c_.check(vkCreateGraphicsPipelines(c_.device, VK_NULL_HANDLE, 1, &gi, nullptr, &gridPipe_[which]), "vkCreateGraphicsPipelines (grid)");
    if (!ok) { gridPipe_[which] = VK_NULL_HANDLE; why = "the grid's pipeline could not be made"; }
    for (VkShaderModule m : mod) if (m) vkDestroyShaderModule(c_.device, m, nullptr);
    if (ok) spirv_ += vb + (which == 0 ? sizeof k_r2d_grid_frag : 0);
    return ok;
}

bool VkCards::gridTarget(Grid& g, int w, int h) {
    if (g.img && g.w == w && g.h == h) return true;
    freeGrid(g);                                              // (a grid's batch has always been waited for before the next one is recorded)
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R32_SFLOAT;
    ii.extent = {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!c_.check(vkCreateImage(c_.device, &ii, nullptr, &g.img), "vkCreateImage (grid)")) { g.img = VK_NULL_HANDLE; failed_ = true; return false; }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(c_.device, g.img, &req);
    std::uint32_t type = UINT32_MAX;
    for (std::uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++)
        if ((req.memoryTypeBits & (1u << i)) && (c_.mem.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) type = i;
    for (std::uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++) if (req.memoryTypeBits & (1u << i)) type = i;
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (type == UINT32_MAX || !c_.check(vkAllocateMemory(c_.device, &mi, nullptr, &g.m), "vkAllocateMemory (grid)")) { g.m = VK_NULL_HANDLE; freeGrid(g); failed_ = true; return false; }
    if (!c_.check(vkBindImageMemory(c_.device, g.img, g.m, 0), "vkBindImageMemory (grid)")) { freeGrid(g); failed_ = true; return false; }
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = g.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (!c_.check(vkCreateImageView(c_.device, &vi, nullptr, &g.view), "vkCreateImageView (grid)")) { g.view = VK_NULL_HANDLE; freeGrid(g); failed_ = true; return false; }
    if (!makeBuffer(VkDeviceSize(w) * VkDeviceSize(h) * 4u, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, g.host, "the grid's read-back")) { freeGrid(g); return false; }
    g.w = w; g.h = h;
    return true;
}

int VkCards::grid(int which, int gw, int gh, VkBuffer b0, VkBuffer b1, std::uint32_t points) {
    which &= 1;
    if (!gridPipe_[which] || gw <= 0 || gh <= 0 || !b0 || (which == 0 && !b1) || !openSlot()) return -1;
    if (static_cast<int>(grids_.size()) <= gridsUsed_) grids_.resize(static_cast<std::size_t>(gridsUsed_) + 1);
    const int index = gridsUsed_++;
    Grid& g = grids_[static_cast<std::size_t>(index)];
    if (!gridTarget(g, gw, gh)) return -1;
    const VkDescriptorSet set = descriptorSet(gridDsl_);
    VkDescriptorBufferInfo ub{};
    if (!set || !uniformBlock(gridU_[which], ub)) return -1;
    VkWriteDescriptorSet wr{};
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = set;
    wr.dstBinding = 2;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wr.pBufferInfo = &ub;
    vkUpdateDescriptorSets(c_.device, 1, &wr, 0, nullptr);
    const VkCommandBuffer cb = slots_[static_cast<std::size_t>(open_)].cmd;
    // 0.63.0: dynamic rendering, synchronization2: the last read-back of this grid (a transfer) before it is cleared and added into again
    vkImageBarrier2(cb, g.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, 0,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderingAttachmentInfo at{};
    at.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    at.imageView = g.view;
    at.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea.extent = {static_cast<std::uint32_t>(gw), static_cast<std::uint32_t>(gh)};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &at;
    vkCmdBeginRendering(cb, &ri);
    // OpenGL's glViewport(0, 0, gw, gh): y = -1 is its first row, and glReadPixels gives that row first; here y = -1 is the image's first row too
    const VkViewport v{0.0f, 0.0f, static_cast<float>(gw), static_cast<float>(gh), 0.0f, 1.0f};
    const VkRect2D sc{{0, 0}, {static_cast<std::uint32_t>(gw), static_cast<std::uint32_t>(gh)}};
    vkCmdSetViewport(cb, 0, 1, &v);
    vkCmdSetScissor(cb, 0, 1, &sc);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipe_[which]);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gridLayout_, 0, 1, &set, 0, nullptr);
    const VkBuffer bufs[2] = {b0, b1};
    const VkDeviceSize offs[2] = {0, 0};
    vkCmdBindVertexBuffers(cb, 0, which == 0 ? 2u : 1u, bufs, offs);
    if (points) vkCmdDraw(cb, points, 1, 0, 0);
    vkCmdEndRendering(cb);
    vkImageBarrier2(cb, g.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);   // (the sums, then copied out)
    VkBufferImageCopy c{};
    c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    c.imageSubresource.layerCount = 1;
    c.imageExtent = {static_cast<std::uint32_t>(gw), static_cast<std::uint32_t>(gh), 1};
    vkCmdCopyImageToBuffer(cb, g.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, g.host.b, 1, &c);
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, kToHost);
    return index;
}

const float* VkCards::cells(int index) const {
    return index >= 0 && index < static_cast<int>(grids_.size()) ? static_cast<const float*>(grids_[static_cast<std::size_t>(index)].host.map) : nullptr;
}

bool VkCards::firstUseTurn(FirstUse& f) { return f.state != 0 || !f.guard || f.guard->hold(); }

void VkCards::firstUse(FirstUse& f) {
    if (f.state != 0 || !f.guard) return;
    f.guard->begin(f.step);
    f.state = 1;
    f.ticket = pending() ? pending() : nextTicket_ - 1;
    f.at = nowS();
}

bool VkCards::pollFirstUse(FirstUse& f, double limitS) {
    if (f.state != 1) return f.state != -1;
    if (done(f.ticket)) {
        if (failed_) { f.guard->end(f.step, "FAILED (a Vulkan call failed)"); f.guard->finished(false, "a Vulkan call failed"); f.state = -1; return false; }
        f.guard->end(f.step, "finished after " + msText(nowS() - f.at));
        f.guard->finished(true);
        f.state = 2;
        return true;
    }
    if (nowS() - f.at > limitS) { f.guard->timedOut(f.step); f.state = -1; return false; }
    return true;
}

// =====================================================================================================================================================
// the start-up steps of the programs that OpenGL makes with the renderer (no guard there): a guard each on Vulkan
// =====================================================================================================================================================
namespace {
struct Steps {
    app::GpuGuard& G;
    std::string& why;
    bool fail(const std::string& step, const std::string& text) { why = text; G.failed(step, text); return false; }
    // a small run: submitted, waited for with the step's time limit
    bool run(VkCards& c, const std::string& step) {
        const std::uint64_t t = c.flush(false);
        if (!t || c.failed()) return fail(step, "a Vulkan call failed (the log has it)");
        app::FrameCostLabel l("a card program's start-up check");
        if (!c.waitFor(t, kStepLimitS)) { G.timedOut(step); why = "the step did not finish in time"; return false; }
        return true;
    }
};
}  // namespace

// ---- the superposition and jump groups (kGroupsCS) ----
class VkGroupCompute : public IGroupCompute {
public:
    explicit VkGroupCompute(VkCards& c) : c_(c) {}
    ~VkGroupCompute() override { for (VkCards::Buf* b : {&super_, &jumpA_, &jumpB_, &out_}) c_.dropBuffer(*b); c_.dropKernel(k_); }
    bool make(std::string& why) { return c_.makeKernel(k_r2d_groups_comp, sizeof k_r2d_groups_comp, k_, why); }
    bool ok() const override { return k_.pipe != VK_NULL_HANDLE && c_.ok(); }
    bool superGroups(const float* amps, long count, std::uint64_t epoch, long dirtyLo, long dirtyHi, const SuperGroupParams& p, unsigned char* out) override {
        if (!ok() || count <= 0 || !c_.firstUseTurn(use)) return false;   // (0.63.0: the first full use waits for its guard's turn: this frame on the processor)
        const bool full = epoch != superEpoch_ || count != superCount_;
        if (!upload(super_, amps, count, full, dirtyLo, dirtyHi)) return false;
        superEpoch_ = epoch; superCount_ = count;
        float p0[4] = {p.a, p.b, p.emRe, p.emIm}, p1[4] = {0, 0, 0, 0};
        return run(count, 0, 0, p0, p1, p.overlap ? 1 : 0, out);
    }
    bool jumpGroups(const float* ampsA, const float* ampsB, long jn, std::uint64_t epoch, const JumpGroupParams& p, unsigned char* out) override {
        if (!ok() || jn <= 0 || !c_.firstUseTurn(use)) return false;
        if (epoch != jumpEpoch_ || jn != jumpCount_) {
            if (!upload(jumpA_, ampsA, jn, true, 0, 0) || !upload(jumpB_, ampsB, jn, true, 0, 0)) return false;
            jumpEpoch_ = epoch; jumpCount_ = jn;
        }
        float p0[4] = {p.shareA, p.shareB, 0, 0}, p1[4] = {0, 0, 0, 0};
        return run(2 * jn, 1, static_cast<int>(jn), p0, p1, p.wheel ? 2 : 0, out);
    }
    VkCards::FirstUse use;
private:
    // as OpenGL's: all the dots when the array is new, else only [lo, hi)
    bool upload(VkCards::Buf& b, const float* data, long dots, bool full, long lo, long hi) {
        if (full) {
            const VkDeviceSize bytes = VkDeviceSize(dots) * 16u;
            if (b.size != bytes) { c_.dropBuffer(b); if (!c_.makeBuffer(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, b, "the groups' amplitudes")) return false; }
            c_.copyIn(data, static_cast<std::size_t>(bytes), b, 0);
        } else {
            lo = std::max(0L, lo); hi = std::min(dots, hi);
            if (hi > lo) c_.copyIn(data + 4 * lo, static_cast<std::size_t>(hi - lo) * 16u, b, VkDeviceSize(lo) * 16u);
        }
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        return !c_.failed();
    }
    bool run(long count, int mode, int jn, float p0[4], float p1[4], int flags, unsigned char* out) {
        const long words = (count + 3) / 4;
        if (out_.size < VkDeviceSize(words) * 4u) { c_.dropBuffer(out_); if (!c_.makeBuffer(VkDeviceSize(words) * 4u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, out_, "the groups' read-back")) return false; }
        if (!jumpB_.b && !c_.makeBuffer(16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, jumpB_, "the groups' second buffer")) return false;
        const int cnt = static_cast<int>(count);
        k_.u.i("uMode", &mode, 1); k_.u.i("uCount", &cnt, 1); k_.u.i("uJn", &jn, 1);
        k_.u.f("uP0", p0, 4); k_.u.f("uP1", p1, 4); k_.u.i("uFlags", &flags, 1);
        if (!c_.dispatch(k_, {mode == 0 ? &super_ : &jumpA_, &jumpB_, &out_}, static_cast<std::uint32_t>((words + 63) / 64))) return false;
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToHost);
        c_.firstUse(use);
        app::FrameCostLabel l("the superposition or jump groups");
        if (!c_.flush(true) || c_.failed()) return false;     // waits, as OpenGL's glGetBufferSubData: the processor needs the groups to order the dots
        std::memcpy(out, out_.map, static_cast<std::size_t>(count));
        return true;
    }
    VkCards& c_;
    VkCards::Kernel k_;
    VkCards::Buf super_, jumpA_, jumpB_, out_;
    std::uint64_t superEpoch_ = 0, jumpEpoch_ = 0;
    long superCount_ = -1, jumpCount_ = -1;
};

IGroupCompute* makeVkGroupCompute(VkCards& c, std::string& why) {
    app::GpuGuard& G = app::gpuVkGroupsGuard();
    Steps S{G, why};
    if (G.started() && !G.allowed()) { why = G.offText().empty() ? "its guard switched it off" : G.offText(); return nullptr; }
    const std::string pipe = "the groups' pipeline", small = "the groups' first small run";
    G.begin(pipe);
    VkGroupCompute* g = new VkGroupCompute(c);
    std::string w;
    if (forcedFail("groups")) { delete g; return S.fail(pipe, "made to fail on purpose (PL_VK_CARDFAIL=groups)"), nullptr; }
    if (!g->make(w)) { delete g; return S.fail(pipe, "kGroupsCS on Vulkan: " + w), nullptr; }
    G.end(pipe, "kGroupsCS as SPIR-V, " + std::to_string(sizeof k_r2d_groups_comp) + " bytes");
    // four dots of a jump with both shares 1 (every dot shown), Wheel colours: their phases 0, pi/2, pi, -pi/2 are the bins 0, 6, 12, 18 (atom/phase_wheel.h)
    G.begin(small);
    const float A[8] = {1, 0, 0, 0, 0, 1, 0, 0}, B[8] = {0, 0, -1, 0, 0, 0, 0, -1};
    unsigned char out[4] = {0, 0, 0, 0};
    JumpGroupParams p;
    p.shareA = 1; p.shareB = 1; p.wheel = 1;
    const double t0 = nowS();
    if (!g->jumpGroups(A, B, 2, 1, p, out)) { delete g; return S.fail(small, "the run could not be made"), nullptr; }
    char b[160];
    std::snprintf(b, sizeof b, "groups %d %d %d %d (want 0 6 12 18) after %s", out[0], out[1], out[2], out[3], msText(nowS() - t0).c_str());
    if (out[0] != 0 || out[1] != 6 || out[2] != 12 || out[3] != 18) { delete g; return S.fail(small, std::string("wrong: ") + b), nullptr; }
    G.end(small, b);
    g->use.guard = &G;
    g->use.step = "the groups' first full use";
    return g;
}

// ---- the 90 % outline (kOutlineDensityCS, kOutlineLevelCS) ----
class VkOutlineCompute : public IOutlineCompute {
public:
    explicit VkOutlineCompute(VkCards& c) : c_(c) {}
    ~VkOutlineCompute() override { while (!jobs_.empty()) release(jobs_.back().id); c_.dropKernel(dens_); c_.dropKernel(level_); }
    bool make(std::string& why) {
        const VkPhysicalDeviceLimits& L = c_.limits();
        if (L.maxComputeWorkGroupInvocations < 1024 || L.maxComputeWorkGroupSize[0] < 1024) { why = "this card's work groups are smaller than the 1,024 threads kOutlineLevelCS uses"; return false; }
        return c_.makeKernel(k_r2d_outdens_comp, sizeof k_r2d_outdens_comp, dens_, why) && c_.makeKernel(k_r2d_outlevel_comp, sizeof k_r2d_outlevel_comp, level_, why);
    }
    bool ok() const override { return dens_.pipe && level_.pipe && c_.ok(); }
    int beginGrid(const OutlineDensity& d, int N, float half, float scale, float target) override { return begin(d, 0, N, half, false, 0, scale, target); }
    int beginPlane(const OutlineDensity& d, int n, float half, bool horizontal, float offset) override { return begin(d, 1, n, half, horizontal, offset, 0, 0); }
    JobState pollGrid(int job, std::vector<float>& grid, float& level) override {
        Job* j = find(job);
        if (!j || !j->grid) return JobState::Failed;
        if (!c_.done(j->ticket)) return JobState::Pending;
        const bool good = !c_.failed();
        if (good) {
            grid.resize(static_cast<std::size_t>(j->count));
            std::memcpy(grid.data(), j->vals.map, static_cast<std::size_t>(j->count) * 4u);
            unsigned res[2] = {0, 0};
            std::memcpy(res, j->level.map, 8);
            std::memcpy(&level, &res[0], 4);
        }
        release(job);
        return good ? JobState::Done : JobState::Failed;
    }
    JobState pollPlane(int job, std::vector<float>& values) override {
        Job* j = find(job);
        if (!j || j->grid) return JobState::Failed;
        if (!c_.done(j->ticket)) return JobState::Pending;
        const bool good = !c_.failed();
        if (good) { values.resize(static_cast<std::size_t>(j->count)); std::memcpy(values.data(), j->vals.map, static_cast<std::size_t>(j->count) * 4u); }
        release(job);
        return good ? JobState::Done : JobState::Failed;
    }
    void cancel(int job) override { release(job); }
    VkCards::FirstUse use;
    bool counting = true;                                     // (the start-up step's own run is not the first full use)
    bool waiting() const override { return waiting_; }
private:
    bool waiting_ = false;
    struct Job { int id = 0; bool grid = false; std::uint64_t ticket = 0; VkCards::Buf vals, level; long count = 0; };
    Job* find(int id) { for (Job& j : jobs_) if (j.id == id) return &j; return nullptr; }
    void release(int id) {
        for (std::size_t i = 0; i < jobs_.size(); i++)
            if (jobs_[i].id == id) { c_.dropBuffer(jobs_[i].vals); c_.dropBuffer(jobs_[i].level); jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
    int begin(const OutlineDensity& d, int mode, int n, float half, bool horizontal, float offset, float scale, float target) {
        waiting_ = false;
        if (!ok() || n < 4 || n % 4 != 0 || jobs_.size() >= 8) return 0;
        if (counting && !c_.firstUseTurn(use)) { waiting_ = true; return 0; }   // (0.63.0: its first full use waits for the guards' turn)
        Job j;
        j.id = nextId_++; j.grid = mode == 0;
        j.count = mode == 0 ? static_cast<long>(n) * n * n : static_cast<long>(n) * n;
        if (!c_.makeBuffer(VkDeviceSize(j.count) * 4u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.vals, "the outline's values")) return 0;
        const int hor = horizontal ? 1 : 0;
        const float cw[2] = {d.cIm, d.weight};
        dens_.u.i("uMode", &mode, 1); dens_.u.i("uN", &n, 1); dens_.u.f("uHalf", &half, 1); dens_.u.i("uHorizontal", &hor, 1); dens_.u.f("uOffset", &offset, 1);
        dens_.u.f("uLag", d.lag, 4); dens_.u.f("uNq", &d.n, 1); dens_.u.i("uAngRe", &d.ang, 1); dens_.u.i("uAngIm", &d.angIm, 1); dens_.u.f("uCw", cw, 2);
        bool ok = c_.dispatch(dens_, {&j.vals}, static_cast<std::uint32_t>(n / 4), static_cast<std::uint32_t>(n / 4), static_cast<std::uint32_t>(mode == 0 ? n / 4 : 1));
        if (ok && mode == 0) {                                // the 90 % level, in one work group, straight from the grid on the card
            ok = c_.makeBuffer(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.level, "the outline's level");
            c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToCompute);
            const int cnt = static_cast<int>(j.count);
            level_.u.i("uCount", &cnt, 1); level_.u.f("uScale", &scale, 1); level_.u.f("uTarget", &target, 1);
            ok = ok && c_.dispatch(level_, {&j.vals, &j.level}, 1);
        }
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToHost);
        if (counting) c_.firstUse(use);
        j.ticket = ok ? c_.flush(false) : 0;
        if (!j.ticket) { c_.dropBuffer(j.vals); c_.dropBuffer(j.level); return 0; }
        jobs_.push_back(j);
        return j.id;
    }
    VkCards& c_;
    VkCards::Kernel dens_, level_;
    std::vector<Job> jobs_;
    int nextId_ = 1;
};

IOutlineCompute* makeVkOutlineCompute(VkCards& c, std::string& why) {
    app::GpuGuard& G = app::gpuVkOutlineGuard();
    Steps S{G, why};
    if (G.started() && !G.allowed()) { why = G.offText().empty() ? "its guard switched it off" : G.offText(); return nullptr; }
    const std::string pipe = "the outline's pipelines", small = "the outline's first small run";
    G.begin(pipe);
    VkOutlineCompute* o = new VkOutlineCompute(c);
    std::string w;
    if (forcedFail("outline")) { delete o; return S.fail(pipe, "made to fail on purpose (PL_VK_CARDFAIL=outline)"), nullptr; }
    if (!o->make(w)) { delete o; return S.fail(pipe, "the outline's programs on Vulkan: " + w), nullptr; }
    G.end(pipe, "kOutlineDensityCS and kOutlineLevelCS as SPIR-V, " + std::to_string(sizeof k_r2d_outdens_comp + sizeof k_r2d_outlevel_comp) + " bytes");
    // the 1s density on a 4 x 4 x 4 grid over [-1, 1]^3: each cell's value is e^(-2 r) (atom::density at Z = 1); the level is one of the cells' values
    G.begin(small);
    o->counting = false;
    const OutlineDensity d;
    const double t0 = nowS();
    double sum = 0;                                           // (the scale makes the cells hold a probability of 1 in all)
    for (int k = 0; k < 64; k++) { const double x = -0.75 + 0.5 * (k % 4), y = -0.75 + 0.5 * (k / 4 % 4), z = -0.75 + 0.5 * (k / 16); sum += std::exp(-2 * std::sqrt(x * x + y * y + z * z)); }
    const int job = o->beginGrid(d, 4, 1.0f, static_cast<float>(1.0 / sum), 0.9f);
    if (!job || !S.run(c, small)) { delete o; return job ? nullptr : (S.fail(small, "the run could not be made"), nullptr); }
    std::vector<float> g;
    float level = -1;
    if (o->pollGrid(job, g, level) != JobState::Done || g.size() != 64) { delete o; return S.fail(small, "nothing came back"), nullptr; }
    double worst = 0;
    bool levelIsCell = false;
    for (int k = 0; k < 4; k++) for (int jy = 0; jy < 4; jy++) for (int i = 0; i < 4; i++) {
        const double x = -1 + (i + 0.5) * 0.5, y = -1 + (jy + 0.5) * 0.5, z = -1 + (k + 0.5) * 0.5, want = std::exp(-2 * std::sqrt(x * x + y * y + z * z));
        const float got = g[static_cast<std::size_t>((k * 4 + jy) * 4 + i)];
        worst = std::max(worst, std::abs(got - want) / want);
        if (got == level) levelIsCell = true;
    }
    char b[200];
    std::snprintf(b, sizeof b, "64 cells, the largest relative difference from e^(-2r) %.2g (limit 1e-4), the level %s a cell's value, after %s", worst, levelIsCell ? "is" : "is NOT", msText(nowS() - t0).c_str());
    if (!(worst <= 1e-4) || !levelIsCell) { delete o; return S.fail(small, std::string("wrong: ") + b), nullptr; }
    G.end(small, b);
    o->counting = true;
    o->use.guard = &G;
    o->use.step = "the outline's first full use";
    return o;
}

// ---- the belts' rocks (kRocksCS, double precision) ----
class VkRockCompute : public IRockCompute {
public:
    explicit VkRockCompute(VkCards& c) : c_(c) {}
    ~VkRockCompute() override { for (int i = 0; i < kSlotsR; i++) freeSlot(i); c_.dropBuffer(read_); c_.dropKernel(k_); }
    bool make(std::string& why) { return c_.makeKernel(k_r2d_rocks_comp, sizeof k_r2d_rocks_comp, k_, why); }
    bool ok() const override { return k_.pipe != VK_NULL_HANDLE && c_.ok(); }
    void keepOnly(const std::vector<std::uint64_t>& live) override {
        for (int i = 0; i < kSlotsR; i++) if (slots_[i].used && std::find(live.begin(), live.end(), slots_[i].serial) == live.end()) freeSlot(i);
    }
    int slotFor(const RockSet& s) override {
        if (!ok() || s.count <= 0 || !s.el) return -1;
        int freeAt = -1;
        for (int i = 0; i < kSlotsR; i++) {
            if (!slots_[i].used) { if (freeAt < 0) freeAt = i; continue; }
            if (slots_[i].serial == s.serial && slots_[i].count == s.count && slots_[i].isStatic == s.isStatic) return i;   // already on the card
        }
        if (freeAt < 0) return -1;
        Slot& sl = slots_[freeAt];
        const VkBufferUsageFlags in = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (!c_.makeBuffer(VkDeviceSize(s.count) * kRockStride * 4u, in, false, sl.el, "the rocks' elements") ||
            !c_.makeBuffer(s.nn ? VkDeviceSize(s.count) * 8u : 8u, in, false, sl.nn, "the rocks' mean motions") ||
            !c_.makeBuffer(VkDeviceSize(s.count) * 8u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, sl.pos, "the rocks' places")) {
            sl.used = true; freeSlot(freeAt); return -1;
        }
        sl.used = true;
        sl.serial = s.serial; sl.count = s.count; sl.isStatic = s.isStatic;
        c_.copyIn(s.el, static_cast<std::size_t>(s.count) * kRockStride * 4u, sl.el, 0);
        if (s.nn) c_.copyIn(s.nn, static_cast<std::size_t>(s.count) * 8u, sl.nn, 0);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        uploads_++;
        return freeAt;
    }
    bool dispatch(int slot, const RockFrame& f) override {
        if (!ok() || slot < 0 || slot >= kSlotsR || !slots_[slot].used || (counting && !c_.firstUseTurn(use))) return false;
        const Slot& sl = slots_[slot];
        const float tHi = static_cast<float>(f.t), tLo = static_cast<float>(f.t - static_cast<double>(tHi));   // t = tHi + tLo exactly (24 + 24 bits), as OpenGL's
        const int st = sl.isStatic ? 1 : 0, d3 = f.d3 ? 1 : 0;
        const float tt[2] = {tHi, tLo}, lib = static_cast<float>(f.libPhase);
        const double bc[4] = {f.bc[0], f.bc[1], f.bc[2], 0}, cam[4] = {f.ca, f.sa, f.cb, f.sb}, view[4] = {f.px, f.W, f.H, f.D}, off[4] = {f.cd, f.ox, f.oy, 0};
        k_.u.i("uCount", &sl.count, 1); k_.u.i("uStatic", &st, 1); k_.u.i("uD3", &d3, 1); k_.u.f("uT", tt, 2); k_.u.f("uLibPhase", &lib, 1);
        k_.u.d("uBc", bc, 4); k_.u.d("uCam", cam, 4); k_.u.d("uView", view, 4); k_.u.d("uOff", off, 4);
        if (!c_.dispatch(k_, {&sl.el, &sl.nn, &sl.pos}, static_cast<std::uint32_t>((sl.count + 63) / 64))) return false;
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToVertex);   // the draw reads the places as vertex data; no wait
        if (counting) c_.firstUse(use);
        return true;
    }
    unsigned positionsBuffer(int slot) const override { return slot >= 0 && slot < kSlotsR && slots_[slot].used ? static_cast<unsigned>(slot + 1) : 0u; }
    VkBuffer vkPositions(int slot) const { return slot >= 0 && slot < kSlotsR && slots_[slot].used ? slots_[slot].pos.b : VK_NULL_HANDLE; }
    bool readPositions(int slot, std::vector<float>& xy) override {
        if (!ok() || slot < 0 || slot >= kSlotsR || !slots_[slot].used) return false;
        const VkDeviceSize bytes = VkDeviceSize(slots_[slot].count) * 8u;
        if (read_.size < bytes) { c_.dropBuffer(read_); if (!c_.makeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, read_, "the rocks' read-back")) return false; }
        const VkCommandBuffer cb = c_.cmd();
        if (!cb) return false;
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToTransfer);
        const VkBufferCopy cp{0, 0, bytes};
        vkCmdCopyBuffer(cb, slots_[slot].pos.b, read_.b, 1, &cp);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToHost);
        app::FrameCostLabel l("the rocks' places");
        if (!c_.flush(true) || c_.failed()) return false;
        xy.resize(static_cast<std::size_t>(slots_[slot].count) * 2u);
        std::memcpy(xy.data(), read_.map, static_cast<std::size_t>(bytes));
        return true;
    }
    int uploads() const override { return uploads_; }
    VkCards::FirstUse use;
    bool counting = true;
private:
    static constexpr int kSlotsR = 16;
    struct Slot { bool used = false; std::uint64_t serial = 0; int count = 0; bool isStatic = false; VkCards::Buf el, nn, pos; };
    void freeSlot(int i) { if (!slots_[i].used) return; c_.dropBuffer(slots_[i].el); c_.dropBuffer(slots_[i].nn); c_.dropBuffer(slots_[i].pos); slots_[i] = Slot(); }
    VkCards& c_;
    VkCards::Kernel k_;
    Slot slots_[kSlotsR];
    VkCards::Buf read_;
    int uploads_ = 0;
};

IRockCompute* makeVkRockCompute(VkCards& c, std::string& why) {
    app::GpuGuard& G = app::gpuVkRocksGuard();
    Steps S{G, why};
    if (G.started() && !G.allowed()) { why = G.offText().empty() ? "its guard switched it off" : G.offText(); return nullptr; }
    const std::string pipe = "the rocks' pipeline", small = "the rocks' first small run";
    if (!c.float64()) {                                       // the precision rule (DECISIONS.md): the rocks' time stays in double; no single-precision copy
        why = "this card has no double precision in its shaders (shaderFloat64)";
        G.log("not made: " + why + "; the belts' rocks are placed by the processor (the precision rule keeps their time in double)");
        return nullptr;
    }
    G.begin(pipe);
    VkRockCompute* r = new VkRockCompute(c);
    std::string w;
    if (forcedFail("rocks")) { delete r; return S.fail(pipe, "made to fail on purpose (PL_VK_CARDFAIL=rocks)"), nullptr; }
    if (!r->make(w)) { delete r; return S.fail(pipe, "kRocksCS on Vulkan: " + w), nullptr; }
    G.end(pipe, "kRocksCS as SPIR-V (double precision: shaderFloat64), " + std::to_string(sizeof k_r2d_rocks_comp) + " bytes");
    // two rocks with the camera looking straight down (no turn, 1 point per 1e9 m, the picture 800 x 600): a still one at (1e11, 2e11, 0) m lands at
    // (400 + 100, 300 - 200); a moving one at t = 0 on a circle of 1 AU from M0 = 0 lands at (400 + 149.5978707, 300)
    G.begin(small);
    r->counting = false;
    float el[2 * kRockStride] = {};
    el[5] = 1e11f; el[6] = 2e11f;
    float* m = el + kRockStride;
    m[1] = 1.0f; m[5] = 1.0f; m[9] = 1.0f;                   // a = 1 AU, P = (1, 0, 0), Q = (0, 1, 0)
    const double nn[1] = {1.99e-7};
    RockSet still, moving;
    still.serial = 1; still.count = 1; still.isStatic = true; still.el = el;
    moving.serial = 2; moving.count = 1; moving.el = m; moving.nn = nn;
    RockFrame f;
    f.px = 1e-9; f.W = 800; f.H = 600; f.D = 1e12;
    const double t0 = nowS();
    const int s0 = r->slotFor(still), s1 = r->slotFor(moving);
    std::vector<float> a, b;
    if (s0 < 0 || s1 < 0 || !r->dispatch(s0, f) || !r->dispatch(s1, f) || !r->readPositions(s0, a) || !r->readPositions(s1, b) || a.size() != 2 || b.size() != 2) {
        delete r; return S.fail(small, "the run could not be made or read back"), nullptr;
    }
    const double e = std::max({std::abs(a[0] - 500.0), std::abs(a[1] - 100.0), std::abs(b[0] - (400.0 + 149.5978707)), std::abs(b[1] - 300.0)});
    char t[220];
    std::snprintf(t, sizeof t, "(%.4f, %.4f) and (%.4f, %.4f), want (500, 100) and (549.5979, 300): the largest difference %.2g point (limit 1e-3), after %s",
                  a[0], a[1], b[0], b[1], e, msText(nowS() - t0).c_str());
    r->keepOnly({});
    if (!(e <= 1e-3)) { delete r; return S.fail(small, std::string("wrong: ") + t), nullptr; }
    G.end(small, t);
    r->counting = true;
    r->use.guard = &G;
    r->use.step = "the rocks' first full use";
    return r;
}

VkBuffer vkRockPositions(IRockCompute* r, int slot) {
    const VkRockCompute* v = dynamic_cast<const VkRockCompute*>(r);
    return v ? v->vkPositions(slot) : VK_NULL_HANDLE;
}

// =====================================================================================================================================================
// the Atom Lab's Slice dots and jump groups (kSliceDotsCS, kJumpShowCS): built and checked by atom/atom_card_paths.cpp under their guards, as on OpenGL
// =====================================================================================================================================================
class AtomCardVk : public AtomCard {
public:
    explicit AtomCardVk(VkCards& c) : c_(c) {}
    ~AtomCardVk() override { reset(); }
    bool build(int which, KerrSteps* steps, std::string& why) override {
        which &= 1;
        if (k_[which].pipe) return true;
        const std::string name = programName(which), cName = "compile " + name, lName = "link " + name;
        const std::uint32_t* w = which == kSlice ? k_r2d_slice_comp : k_r2d_jump_comp;
        const std::size_t bytes = which == kSlice ? sizeof k_r2d_slice_comp : sizeof k_r2d_jump_comp;
        // on Vulkan the text was compiled at build time: "compile" makes the shader module from its SPIR-V, "link" the pipeline
        if (steps) steps->begin(cName);
        double t = nowS();
        std::string w2;
        VkDraw2D::Shader sh;
        const bool good = reflectSpirv(w, bytes / 4, sh, w2);
        if (steps) steps->end(cName, std::string(good ? "the SPIR-V read" : "FAILED") + " in " + msText(nowS() - t) + " (" + std::to_string(bytes) + " bytes, made at build time from shaders.h)");
        if (!good) { why = "the SPIR-V of " + name + " is broken: " + w2; return false; }
        if (steps) steps->begin(lName);
        t = nowS();
        const bool forced = forcedFail(which == kSlice ? "slice" : "jump");
        const bool made = !forced && c_.makeKernel(w, bytes, k_[which], w2);
        if (steps) steps->end(lName, std::string(made ? "the pipeline made" : "FAILED") + " in " + msText(nowS() - t) + (made ? std::string() : ": " + (forced ? std::string("made to fail on purpose (PL_VK_CARDFAIL)") : w2)));
        if (!made) { why = forced ? std::string("the pipeline of ") + name + " was made to fail on purpose (PL_VK_CARDFAIL)" : "the Vulkan driver did not make the pipeline of " + name; return false; }
        return true;
    }
    bool built(int which) const override { return k_[which & 1].pipe != VK_NULL_HANDLE; }
    void reset() override {
        while (!jobs_.empty()) release(jobs_.back().id);
        for (VkCards::Kernel& k : k_) c_.dropKernel(k);
        c_.dropBuffer(fixed_); c_.dropBuffer(out_);
        jumpEpoch_ = 0; jumpN_ = 0; fenceTicket_ = 0;
        sliceReady_ = jumpReady_ = sliceUsed_ = false;
    }
    int beginSlice(const SliceDotsJob& s) override {
        if (!built(kSlice) || !c_.ok() || s.tries <= 0 || s.tries > (1 << 20) || s.nh <= 0 || s.nh > 16 || s.stride < 3 || s.stride > 4 || s.first < 0 || s.first > s.stride - 3 || jobs_.size() >= 8) return 0;
        firstBatch();
        Job j;
        j.id = nextId_++;
        j.bytes = static_cast<long>(s.tries) * 16;
        if (!c_.makeBuffer(VkDeviceSize(j.bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.out, "the Slice's dots")) return 0;
        VkUniforms& u = k_[kSlice].u;
        u.u("uSeed", &s.seed, 1); u.i("uTries", &s.tries, 1); u.i("uAxis", &s.axis, 1); u.f("uOff", &s.off, 1); u.f("uX0", &s.x0, 1); u.f("uLnG0", &s.lnG0, 1);
        u.f("uScale", &s.scale, 1); u.f("uBound", &s.bound, 1); u.i("uAng", &s.ang, 1); u.i("uAngIm", &s.angIm, 1); u.f("uCIm", &s.cIm, 1); u.i("uNh", &s.nh, 1);
        u.f("uH", s.h, 1, 16); u.f("uC", s.c, 1, 16); u.i("uStride", &s.stride, 1); u.i("uFirst", &s.first, 1);
        if (!c_.dispatch(k_[kSlice], {&j.out}, static_cast<std::uint32_t>((s.tries + 63) / 64))) { c_.dropBuffer(j.out); return 0; }
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToHost);
        j.ticket = c_.flush(false);
        if (!j.ticket) { c_.dropBuffer(j.out); return 0; }
        jobs_.push_back(j);
        return j.id;
    }
    JobState pollSlice(int job, std::vector<float>& res) override {
        Job* j = find(job);
        if (!j) return JobState::Failed;
        if (!c_.done(j->ticket)) return JobState::Pending;
        const bool good = !c_.failed();
        if (good) { res.resize(static_cast<std::size_t>(j->bytes / 4)); std::memcpy(res.data(), j->out.map, static_cast<std::size_t>(j->bytes)); }
        release(job);
        return good ? JobState::Done : JobState::Failed;
    }
    void cancelSlice(int job) override { release(job); }
    bool jumpGroups(const unsigned char* fixedGroups, long jn, std::uint64_t epoch, std::uint64_t t0, std::uint64_t t1) override {
        if (!built(kJump) || !c_.ok() || jn <= 0 || !fixedGroups) return false;
        const long bytes = (2 * jn + 3) / 4 * 4;
        if (epoch != jumpEpoch_ || jn != jumpN_) {             // a new jump: its fixed groups sent once (the last word padded with "not shown")
            std::vector<unsigned char> padded(fixedGroups, fixedGroups + 2 * jn);
            padded.resize(static_cast<std::size_t>(bytes), 255);
            if (fixed_.size != VkDeviceSize(bytes)) {
                c_.dropBuffer(fixed_); c_.dropBuffer(out_);
                if (!c_.makeBuffer(VkDeviceSize(bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, fixed_, "the jump's fixed groups") ||
                    !c_.makeBuffer(VkDeviceSize(bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false, out_, "the jump's groups")) return false;
            }
            c_.copyIn(padded.data(), padded.size(), fixed_, 0);
            c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
            jumpEpoch_ = epoch; jumpN_ = jn;
        }
        if (!dispatchJump(fixed_, out_, jn, t0, t1)) return false;
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToVertex);   // read by the cloud's grid and dots as their group attribute
        return true;
    }
    GLuint jumpGroupsBuffer() const override { return out_.b ? 1u : 0u; }
    VkBuffer vkJump() const { return out_.b; }
    int beginJumpRead(const unsigned char* fixedGroups, long jn, std::uint64_t t0, std::uint64_t t1) override {
        if (!built(kJump) || !c_.ok() || jn <= 0 || !fixedGroups || jobs_.size() >= 8) return 0;
        const long bytes = (2 * jn + 3) / 4 * 4;
        std::vector<unsigned char> padded(fixedGroups, fixedGroups + 2 * jn);
        padded.resize(static_cast<std::size_t>(bytes), 255);
        Job j;
        j.id = nextId_++;
        j.bytes = 2 * jn;
        if (!c_.makeBuffer(VkDeviceSize(bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.in, "the jump's check (in)") ||
            !c_.makeBuffer(VkDeviceSize(bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.out, "the jump's check (out)")) { c_.dropBuffer(j.in); c_.dropBuffer(j.out); return 0; }
        std::memcpy(j.in.map, padded.data(), padded.size());   // (host-visible: the processor's writes are seen by the batch submitted after them)
        const bool ok = dispatchJump(j.in, j.out, jn, t0, t1);
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToHost);
        j.ticket = ok ? c_.flush(false) : 0;
        if (!j.ticket) { c_.dropBuffer(j.in); c_.dropBuffer(j.out); return 0; }
        jobs_.push_back(j);
        return j.id;
    }
    JobState pollJumpRead(int job, std::vector<unsigned char>& groups) override {
        Job* j = find(job);
        if (!j) return JobState::Failed;
        if (!c_.done(j->ticket)) return JobState::Pending;
        const bool good = !c_.failed();
        if (good) { groups.resize(static_cast<std::size_t>(j->bytes)); std::memcpy(groups.data(), j->out.map, static_cast<std::size_t>(j->bytes)); }
        release(job);
        return good ? JobState::Done : JobState::Failed;
    }
    void fence() override { fenceTicket_ = c_.pending() ? c_.pending() : c_.flush(false); }
    int fenceState() override {
        if (!fenceTicket_ || c_.failed()) return -1;
        if (!c_.done(fenceTicket_)) return 0;
        fenceTicket_ = 0;
        return 1;
    }
private:
    struct Job { int id = 0; std::uint64_t ticket = 0; VkCards::Buf in, out; long bytes = 0; };
    Job* find(int id) { for (Job& j : jobs_) if (j.id == id) return &j; return nullptr; }
    void release(int id) {
        for (std::size_t i = 0; i < jobs_.size(); i++)
            if (jobs_[i].id == id) { c_.dropBuffer(jobs_[i].in); c_.dropBuffer(jobs_[i].out); jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
    bool dispatchJump(const VkCards::Buf& fixedBuf, const VkCards::Buf& outBuf, long jn, std::uint64_t t0, std::uint64_t t1) {
        VkUniforms& u = k_[kJump].u;
        const int n = static_cast<int>(jn);
        const std::uint32_t a[2] = {static_cast<std::uint32_t>(t0 & 0xFFFFFFFFu), static_cast<std::uint32_t>(t0 >> 32)}, b[2] = {static_cast<std::uint32_t>(t1 & 0xFFFFFFFFu), static_cast<std::uint32_t>(t1 >> 32)};
        u.i("uJn", &n, 1); u.u("uT0", a, 2); u.u("uT1", b, 2);
        const long words = (2 * jn + 3) / 4;
        return c_.dispatch(k_[kJump], {&fixedBuf, &outBuf}, static_cast<std::uint32_t>((words + 63) / 64));
    }
    VkCards& c_;
    VkCards::Kernel k_[2];
    std::vector<Job> jobs_;
    int nextId_ = 1;
    VkCards::Buf fixed_, out_;
    std::uint64_t jumpEpoch_ = 0, fenceTicket_ = 0;
    long jumpN_ = 0;
};

AtomCard* makeVkAtomCard(VkCards& c) { return new AtomCardVk(c); }
VkBuffer vkJumpGroups(AtomCard* a) { const AtomCardVk* v = dynamic_cast<const AtomCardVk*>(a); return v ? v->vkJump() : VK_NULL_HANDLE; }

// =====================================================================================================================================================
// the Molecule Lab's Density cloud (kMolDotsCS, kMolGridVS + kCloudGridFS, kMolDotVS + kBlobFS): built and checked by molecule/molecule_card_paths.cpp
// =====================================================================================================================================================
class MoleculeCardVk : public MoleculeCard {
public:
    explicit MoleculeCardVk(VkCards& c) : c_(c) {}
    ~MoleculeCardVk() override { reset(); }
    bool build(KerrSteps* steps, std::string& why) override {
        if (built()) return true;
        const std::string cName = "compile the Density cloud's shaders", lName = "link the Density cloud's shaders";
        if (steps) steps->begin(cName);
        double t = nowS();
        std::string w;
        VkDraw2D::Shader sh;
        const bool good = reflectSpirv(k_r2d_moldots_comp, sizeof k_r2d_moldots_comp / 4, sh, w);
        if (steps) steps->end(cName, std::string(good ? "the SPIR-V read" : "FAILED") + " in " + msText(nowS() - t) + " (made at build time from shaders.h)");
        if (!good) { why = "the SPIR-V of the Density cloud's dots is broken: " + w; return false; }
        if (steps) steps->begin(lName);
        t = nowS();
        const bool forced = forcedFail("molcloud");
        bool made = !forced && c_.makeKernel(k_r2d_moldots_comp, sizeof k_r2d_moldots_comp, k_, w) && c_.makeGrid(1, w);
        if (made && c_.draw2d()) made = c_.draw2d()->addProgram(VkDraw2D::kMolDot, w);
        if (steps) steps->end(lName, std::string(made ? "the pipelines made" : "FAILED") + " in " + msText(nowS() - t) + (made ? std::string() : ": " + (forced ? std::string("made to fail on purpose (PL_VK_CARDFAIL)") : w)));
        if (!made) { c_.dropKernel(k_); why = forced ? "the Density cloud's pipelines were made to fail on purpose (PL_VK_CARDFAIL)" : "the Vulkan driver did not make the Density cloud's pipelines: " + w; return false; }
        return true;
    }
    bool built() const override { return k_.pipe && c_.hasGrid(1) && (!c_.draw2d() || c_.draw2d()->hasProgram(VkDraw2D::kMolDot)); }
    void reset() override {
        while (!jobs_.empty()) release(jobs_.back().id);
        c_.dropKernel(k_);
        for (Set& s : sets_) c_.dropBuffer(s.buf);
        sets_.clear();
        c_.dropBuffer(out_);
        fenceTicket_ = 0;
        ready_ = false;
    }
    int beginProject(const float* pts, long n, const MolCloudCam& cam) override {
        if (!k_.pipe || !c_.ok() || !pts || n <= 0 || n > (1 << 20) || jobs_.size() >= 16) return 0;
        Job j;
        j.id = nextId_++;
        j.n = n;
        if (!c_.makeBuffer(VkDeviceSize(n) * 12u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.pts, "the Density check's dots") ||
            !c_.makeBuffer(VkDeviceSize(n) * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.out, "the Density check's results")) { c_.dropBuffer(j.pts); c_.dropBuffer(j.out); return 0; }
        std::memcpy(j.pts.map, pts, static_cast<std::size_t>(n) * 12u);
        const bool ok = dispatch(j.pts, j.out, n, cam);
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToHost);
        j.ticket = ok ? c_.flush(false) : 0;
        if (!j.ticket) { c_.dropBuffer(j.pts); c_.dropBuffer(j.out); return 0; }
        jobs_.push_back(j);
        return j.id;
    }
    JobState pollProject(int job, std::vector<float>& out) override {
        Job* j = nullptr;
        for (Job& q : jobs_) if (q.id == job) j = &q;
        if (!j) return JobState::Failed;
        if (!c_.done(j->ticket)) return JobState::Pending;
        const bool good = !c_.failed();
        if (good) { out.resize(static_cast<std::size_t>(4 * j->n)); std::memcpy(out.data(), j->out.map, static_cast<std::size_t>(j->n) * 16u); }
        release(job);
        return good ? JobState::Done : JobState::Failed;
    }
    int fenceState() override {
        if (!fenceTicket_ || c_.failed()) return -1;
        if (!c_.done(fenceTicket_)) return 0;
        fenceTicket_ = 0;
        return 1;
    }
    // the frame's cloud: the set sent if new, the dots made, the grid summed (in the open batch); the grid's index
    int record(const MolCloudDraw& d) {
        if (!built() || !d.pts || d.cnt <= 0 || !c_.ok()) return -1;
        Set* set = nullptr;
        for (Set& s : sets_) if (s.epoch == d.epoch && s.n == d.cnt) set = &s;
        if (!set) {
            if (sets_.size() < 18) sets_.push_back(Set{});    // the still cloud and a vibration's 17 moments, as OpenGL's
            Set* old = &sets_[0];
            for (Set& s : sets_) if (s.used < old->used) old = &s;
            set = old;
            c_.dropBuffer(set->buf);
            if (!c_.makeBuffer(VkDeviceSize(d.cnt) * 12u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, set->buf, "the Density cloud's dots")) return -1;
            c_.copyIn(d.pts, static_cast<std::size_t>(d.cnt) * 12u, set->buf, 0);
            c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
            bytes_ += d.cnt * 3 * static_cast<long>(sizeof(float));
            set->epoch = d.epoch; set->n = d.cnt;
        }
        set->used = ++useClock_;
        if (out_.size < VkDeviceSize(d.cnt) * 16u) {
            c_.dropBuffer(out_);
            if (!c_.makeBuffer(VkDeviceSize(d.cnt) * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false, out_, "the Density cloud's made dots")) return -1;
        }
        if (!dispatch(set->buf, out_, d.cnt, d.cam)) return -1;
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkCards::kToVertex);
        const int gw = static_cast<int>(d.cam.w / 8) + 2, gh = static_cast<int>(d.cam.h / 8) + 2;
        const float g[2] = {static_cast<float>(gw), static_cast<float>(gh)};
        c_.gridUniforms(1).f("uGrid", g, 2);
        const int index = c_.grid(1, gw, gh, out_.b, VK_NULL_HANDLE, static_cast<std::uint32_t>(d.cnt));
        if (d.fenceAfter && !fenceTicket_) fenceTicket_ = c_.pending();   // (once, as OpenGL's)
        return index;
    }
    VkBuffer dots() const { return out_.b; }
private:
    struct Job { int id = 0; std::uint64_t ticket = 0; VkCards::Buf pts, out; long n = 0; };
    struct Set { std::uint64_t epoch = 0; VkCards::Buf buf; long n = 0; unsigned used = 0; };
    void release(int id) {
        for (std::size_t i = 0; i < jobs_.size(); i++)
            if (jobs_[i].id == id) { c_.dropBuffer(jobs_[i].pts); c_.dropBuffer(jobs_[i].out); jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
    bool dispatch(const VkCards::Buf& pts, const VkCards::Buf& out, long n, const MolCloudCam& c) {
        const float r0[4] = {c.R[0], c.R[1], c.R[2], c.panX}, r1[4] = {c.R[3], c.R[4], c.R[5], c.panY}, r2[4] = {c.R[6], c.R[7], c.R[8], c.ext}, scr[4] = {c.cx, c.cy, c.s, 0.f}, view[2] = {c.w, c.h};
        const int nn = static_cast<int>(n);
        k_.u.f("uR0", r0, 4); k_.u.f("uR1", r1, 4); k_.u.f("uR2", r2, 4); k_.u.f("uScr", scr, 4); k_.u.f("uView", view, 2); k_.u.i("uN", &nn, 1);
        return c_.dispatch(k_, {&pts, &out}, static_cast<std::uint32_t>((n + 63) / 64));
    }
    VkCards& c_;
    VkCards::Kernel k_;
    std::vector<Job> jobs_;
    int nextId_ = 1;
    std::vector<Set> sets_;
    unsigned useClock_ = 0;
    VkCards::Buf out_;
    std::uint64_t fenceTicket_ = 0;
};

MoleculeCard* makeVkMoleculeCard(VkCards& c) { return new MoleculeCardVk(c); }
int vkMolCloudRecord(MoleculeCard* m, const MolCloudDraw& d) { MoleculeCardVk* v = dynamic_cast<MoleculeCardVk*>(m); return v ? v->record(d) : -1; }
VkBuffer vkMolCloudDots(MoleculeCard* m) { const MoleculeCardVk* v = dynamic_cast<const MoleculeCardVk*>(m); return v ? v->dots() : VK_NULL_HANDLE; }

// =====================================================================================================================================================
// the Cloud 3D Glow dots (Renderer2D::cloudGpu: kCloudVS + kBlobFS, kCloudGridVS + kCloudGridFS): their places kept on the card (only the dots added
// since the last frame are sent), their groups sent each frame or made there (a jump: AtomCard::jumpGroups), the exposure's grid summed and read back
// =====================================================================================================================================================
class VkGlow {
public:
    explicit VkGlow(VkCards& c) : c_(c) {}
    ~VkGlow() { c_.dropBuffer(pos_); c_.dropBuffer(grp_); }
    int record(const Renderer2D::CloudGpuDraw& d, VkBuffer jump, long& bytes) {
        if (!c_.ok() || d.cnt <= 0 || !d.pts || (!d.grp && !jump) || (counting && !c_.firstUseTurn(use))) return -1;
        // 1. the places: the whole ring when it is another cloud (or it was changed other than by adding), else only the dots added since the last frame
        if (d.epoch != epoch_ || d.cap != cap_ || d.k < k_ || d.k - k_ >= d.cap) {
            if (pos_.size != VkDeviceSize(d.cap) * 12u) {
                c_.dropBuffer(pos_);
                if (!c_.makeBuffer(VkDeviceSize(d.cap) * 12u, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, pos_, "the cloud's places")) return -1;
            }
            c_.copyIn(d.pts, static_cast<std::size_t>(d.cap) * 12u, pos_, 0);
            bytes += static_cast<long>(d.cap * 3 * static_cast<long>(sizeof(float)));
        } else if (d.k > k_) {
            const long lo = k_ % d.cap, hi = d.k % d.cap;
            const auto send = [&](long a, long b) { if (b > a) { c_.copyIn(d.pts + 3 * a, static_cast<std::size_t>(b - a) * 12u, pos_, VkDeviceSize(a) * 12u); bytes += static_cast<long>((b - a) * 3 * static_cast<long>(sizeof(float))); } };
            if (lo < hi) send(lo, hi); else { send(lo, d.cap); send(0, hi); }
        }
        epoch_ = d.epoch; cap_ = d.cap; k_ = d.k;
        // 2. the colour groups (one byte a dot) unless the card made them
        if (!jump) {
            if (grp_.size < VkDeviceSize(d.cnt)) {
                c_.dropBuffer(grp_);
                if (!c_.makeBuffer(VkDeviceSize(d.cnt) + VkDeviceSize(d.cnt) / 2, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, grp_, "the cloud's groups")) return -1;
            }
            c_.copyIn(d.grp, static_cast<std::size_t>(d.cnt), grp_, 0);
        }
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToVertex);
        groups_ = jump ? jump : grp_.b;
        // 3. the exposure's grid
        const int gw = static_cast<int>(d.w / 8) + 2, gh = static_cast<int>(d.h / 8) + 2;
        VkUniforms& u = c_.gridUniforms(0);
        const float cam0[4] = {d.cx, d.cy, d.pxm, d.ext}, cam1[4] = {d.ca, d.sa, d.cb, d.sb}, view[2] = {d.w, d.h}, g[2] = {static_cast<float>(gw), static_cast<float>(gh)};
        u.f("uCam0", cam0, 4); u.f("uCam1", cam1, 4); u.f("uView", view, 2); u.f("uGrid", g, 2);
        const int index = c_.grid(0, gw, gh, pos_.b, groups_, static_cast<std::uint32_t>(d.cnt));
        if (index >= 0 && counting) c_.firstUse(use);
        return index;
    }
    VkBuffer places() const { return pos_.b; }
    VkBuffer groups() const { return groups_; }
    VkCards::FirstUse use;
    bool counting = true;
private:
    VkCards& c_;
    VkCards::Buf pos_, grp_;
    VkBuffer groups_ = VK_NULL_HANDLE;
    std::uint64_t epoch_ = 0;
    long cap_ = 0, k_ = -1;
};

VkGlow* makeVkGlow(VkCards& c, std::string& why) {
    app::GpuGuard& G = app::gpuVkCloudGuard();
    Steps S{G, why};
    if (G.started() && !G.allowed()) { why = G.offText().empty() ? "its guard switched it off" : G.offText(); return nullptr; }
    const std::string pipe = "the Glow dots' pipelines", small = "the Glow dots' first small run";
    G.begin(pipe);
    std::string w;
    if (forcedFail("cloud")) return S.fail(pipe, "made to fail on purpose (PL_VK_CARDFAIL=cloud)"), nullptr;
    if (!c.makeGrid(0, w) || (c.draw2d() && !c.draw2d()->addProgram(VkDraw2D::kCloud, w))) return S.fail(pipe, "the Glow dots' programs on Vulkan: " + w), nullptr;
    G.end(pipe, "kCloudVS, kCloudGridVS and kCloudGridFS as SPIR-V (with kBlobFS)");
    // one dot at the middle of a 100 x 100 picture, at depth 0: r = 4.6, weight 0.75, so its cell (6, 6) of the 14 x 14 grid holds 0.75 x 4.6^2 x 0.785
    G.begin(small);
    VkGlow* g = new VkGlow(c);
    g->counting = false;
    const float pts[3] = {0, 0, 0};
    const unsigned char grp[1] = {0};
    Renderer2D::CloudGpuDraw d;
    d.pts = pts; d.cnt = 1; d.cap = 1; d.epoch = 1; d.k = 1; d.grp = grp;
    d.cx = 50; d.cy = 50; d.pxm = 1; d.ext = 1; d.w = 100; d.h = 100;
    long bytes = 0;
    const double t0 = nowS();
    const int index = g->record(d, VK_NULL_HANDLE, bytes);
    if (index < 0 || !S.run(c, small)) { delete g; return index < 0 ? (S.fail(small, "the run could not be made"), nullptr) : nullptr; }
    const float* cells = c.cells(index);
    const double want = 0.75 * 4.6 * 4.6 * 0.785;
    double rest = 0;
    for (int i = 0; i < 14 * 14; i++) if (i != 6 * 14 + 6) rest += std::abs(cells[i]);
    const double got = cells[6 * 14 + 6], rel = std::abs(got - want) / want;
    char t[200];
    std::snprintf(t, sizeof t, "cell (6, 6) %.6g, want %.6g (relative difference %.2g, limit 1e-5); the other cells add up to %g; after %s", got, want, rel, rest, msText(nowS() - t0).c_str());
    if (!(rel <= 1e-5) || rest != 0) { delete g; return S.fail(small, std::string("wrong: ") + t), nullptr; }
    G.end(small, t);
    g->counting = true;
    g->use.guard = &G;
    g->use.step = "the Glow dots' first full use";
    return g;
}

void dropVkGlow(VkGlow* g) { delete g; }
int vkGlowRecord(VkGlow* g, const Renderer2D::CloudGpuDraw& d, VkBuffer jump, long& bytes) { return g ? g->record(d, jump, bytes) : -1; }
VkBuffer vkGlowPlaces(VkGlow* g) { return g ? g->places() : VK_NULL_HANDLE; }
VkBuffer vkGlowGroups(VkGlow* g) { return g ? g->groups() : VK_NULL_HANDLE; }

// each frame: the programs' first full use, ended when its batch has finished (or timed out: false in its bit: 1 groups, 2 outline, 4 rocks, 8 Glow)
int vkPollFirstUses(VkCards& c, IGroupCompute* g, IOutlineCompute* o, IRockCompute* r, VkGlow* w) {
    int off = 0;
    if (VkGroupCompute* v = dynamic_cast<VkGroupCompute*>(g)) if (!c.pollFirstUse(v->use, kStepLimitS)) off |= 1;
    if (VkOutlineCompute* v = dynamic_cast<VkOutlineCompute*>(o)) if (!c.pollFirstUse(v->use, kStepLimitS)) off |= 2;
    if (VkRockCompute* v = dynamic_cast<VkRockCompute*>(r)) if (!c.pollFirstUse(v->use, kStepLimitS)) off |= 4;
    if (w && !c.pollFirstUse(w->use, kStepLimitS)) off |= 8;
    return off;
}

}  // namespace render
