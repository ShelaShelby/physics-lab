// 0.61.0: Vulkan, step 3 of 5 (native/PLAN_VULKAN.md): the card programs of the Atom, Molecule and Orbit Labs on Vulkan, from the same shader text as
// OpenGL's (render/shaders.h, made into SPIR-V at build time: CMakeLists.txt PL_VK2D_SHADERS). Each program implements the interface its OpenGL class does
// (IGroupCompute, IOutlineCompute, IRockCompute, AtomCard, MoleculeCard; the Cloud 3D Glow dots of Renderer2D::cloudGpu), so the labs' code is the same;
// Renderer2D gives these objects back on Vulkan (render/renderer2d_vk.cpp useVulkanCards).
//
// The work goes to the card in batches of its own (VkCards: a command buffer, a fence, its uniforms and its staging buffer), submitted on the window's
// queue before the frame that uses their results. Nothing waits for the card in a normal frame where OpenGL does not wait either: a job is submitted,
// its fence marks its end, later frames ask with a zero timeout (the outline, the Slice's dots, the start-up checks); where OpenGL reads back at once
// (the superposition and jump groups the processor needs to order the dots; the exposure's grid of the Glow and Density clouds, glReadPixels) the batch
// is waited for, as there. Buffers the frame draws from (the rocks' places, the dots, their groups) are kept on the card; one that is replaced is
// destroyed only once the batches and the frames that may use it are done. Barriers name the stages exactly: transfer -> compute, compute -> vertex input,
// compute -> host, colour output -> transfer -> host; every batch begins with one that orders it after the frames before it (they may still read
// what it writes). The rocks' program needs double precision in its shader (shaderFloat64): without it the belts take the processor's path.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <volk.h>

#include "renderer2d.h"
#include "vk_draw2d.h"

namespace app { class GpuGuard; }

namespace render {

class IGroupCompute;
class IOutlineCompute;
class IRockCompute;
class AtomCard;
class MoleculeCard;

// one program's loose uniforms (the block glslang made of them, its layout read from the SPIR-V), set by name as glUniform* would
struct VkUniforms {
    VkDraw2D::Block blk;
    void set(const char* name, const void* v, std::size_t compBytes, int comps, int count = 1);
    void f(const char* name, const float* v, int comps, int count = 1) { set(name, v, 4, comps, count); }
    void i(const char* name, const int* v, int comps, int count = 1) { set(name, v, 4, comps, count); }
    void u(const char* name, const std::uint32_t* v, int comps) { set(name, v, 4, comps); }
    void d(const char* name, const double* v, int comps) { set(name, v, 8, comps); }
};

class VkCards {
public:
    struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void* map = nullptr; VkDeviceSize size = 0; };
    struct Kernel { VkShaderModule mod = VK_NULL_HANDLE; VkDescriptorSetLayout dsl = VK_NULL_HANDLE; VkPipelineLayout pl = VK_NULL_HANDLE; VkPipeline pipe = VK_NULL_HANDLE;
                    VkUniforms u; std::vector<int> storage; bool graphics = false; };
    enum Consumer : unsigned { kToCompute = 1, kToVertex = 2, kToHost = 4, kToTransfer = 8 };

    ~VkCards();
    // the command pool and what every batch needs; d2: the renderer's Vulkan drawing (its frames keep replaced buffers alive), null in the tests.
    // float64: the device was made with shaderFloat64 (the rocks' program). False: why says it.
    bool init(const VkDevCtx& c, VkDraw2D* d2, bool float64, std::string& why);
    void destroy();                                          // waits for the card; every program made from this must be gone first
    bool ok() const { return ok_ && !failed_; }
    bool float64() const { return float64_; }
    const VkDevCtx& ctx() const { return c_; }
    VkDraw2D* draw2d() const { return d2_; }
    const VkPhysicalDeviceLimits& limits() const { return c_.limits; }
    std::size_t spirvBytes() const { return spirv_; }

    // a compute program from its SPIR-V (its uniform block at binding 7, its storage buffers at their bindings); false: why
    bool makeKernel(const std::uint32_t* words, std::size_t bytes, Kernel& k, std::string& why, VkPipelineCache cache = VK_NULL_HANDLE);   // (0.62.0: a cache)
    void dropKernel(Kernel& k);
    bool makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buf& b, const char* what);
    void dropBuffer(Buf& b);                                 // destroyed once the batches and frames that may use it are done

    // ---- the open batch (opened by the first call that records) ----
    VkCommandBuffer cmd();
    VkDeviceSize stage(const void* data, std::size_t bytes);  // into the batch's staging buffer (host-visible); its offset
    void copyIn(const void* data, std::size_t bytes, const Buf& dst, VkDeviceSize at);   // staged and copied (a transfer: barrier() after)
    // the barrier after writes (by: VK_PIPELINE_STAGE_TRANSFER_BIT or COMPUTE_SHADER_BIT) to their consumers (Consumer bits)
    void barrier(VkPipelineStageFlags by, unsigned to);
    bool dispatch(Kernel& k, std::initializer_list<const Buf*> bufs, std::uint32_t gx, std::uint32_t gy = 1, std::uint32_t gz = 1);
    std::uint64_t pending() const { return open_ >= 0 ? nextTicket_ : 0; }   // the ticket the open batch will get (0: none open)
    // submits the open batch (wait: and waits for it); its ticket (> 0), or the last one when nothing was open; 0: it could not be submitted
    std::uint64_t flush(bool wait);
    bool done(std::uint64_t ticket);                         // never waits; true once the batch has finished (or failed: failed() says it)
    bool waitFor(std::uint64_t ticket, double seconds);      // the start-up steps only (with a time limit)
    bool failed() const { return failed_; }
    void collect();                                          // what is no longer used, destroyed (called once a frame)
    // 0.62.0 (step 4, the Black Hole Lab's passes: render/vk_blackhole.h): a descriptor set of any layout from the open batch's pools (uniform buffers,
    // storage buffers, textures), a uniform block in the batch's buffer, the staging buffer the last stage() wrote into, f() once the open batch (or the
    // last submitted one) and the frames after it are done
    VkDescriptorSet descriptorSet(VkDescriptorSetLayout dsl);
    bool uniformBlock(const VkUniforms& u, VkDescriptorBufferInfo& bi);
    VkBuffer stageBuffer() const;
    void later(std::function<void()> f) { drop_.push_back({open_ >= 0 ? nextTicket_ : nextTicket_ - 1, std::move(f)}); }

    // ---- the exposure's grid (the Glow and Density clouds): points summed into a small float picture, read back after flush(true) ----
    // which: 0 the Cloud 3D dots (kCloudGridVS: their places at binding 0, groups at binding 1), 1 the Density dots (kMolGridVS: the dots at binding 0).
    // Returns the index of the grid's read-back (cells()), -1 on failure.
    bool makeGrid(int which, std::string& why);
    bool hasGrid(int which) const { return gridPipe_[which & 1] != VK_NULL_HANDLE; }
    VkUniforms& gridUniforms(int which) { return gridU_[which & 1]; }
    int grid(int which, int gw, int gh, VkBuffer b0, VkBuffer b1, std::uint32_t points);
    const float* cells(int index) const;

    // ---- the guarded first use of a program (the step "... first full use" ends when the batch with its first real work has finished) ----
    struct FirstUse { app::GpuGuard* guard = nullptr; std::string step; std::uint64_t ticket = 0; double at = 0; int state = 0; };   // state 0 waiting, 1 begun, 2 done, -1 failed
    void firstUse(FirstUse& f);                              // called where the program's real work is recorded
    bool firstUseTurn(FirstUse& f);                          // 0.63.0: before it is recorded: false = another guard's step is open (the use moves to a later frame)
    bool pollFirstUse(FirstUse& f, double limitS);           // each frame: false when the step failed or timed out (the program goes off)

    // ---- 0.63.0: the benchmark's timer: each batch's time on the card (a timestamp at its start and at its end), while on ----
    bool setTiming(bool on);                                 // false: this queue has no timestamps (nothing is timed)
    std::uint64_t lastTicket() const { return nextTicket_ - 1; }   // the last batch submitted
    // the batches first..last (tickets) once all have finished: their times summed (ms), the first start and the last end (the card's ticks, in ns);
    // false while one is still on the card. Untimed batches count 0.
    bool batchTimes(std::uint64_t first, std::uint64_t last, double& busyMs, double& t0Ns, double& t1Ns);

private:
    struct Slot { VkCommandBuffer cmd = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE; std::uint64_t ticket = 0; bool submitted = false, timed = false, frame = false;
                  std::vector<VkDescriptorPool> pools; std::size_t pool = 0; Buf ubo; VkDeviceSize uboUsed = 0; Buf stage; VkDeviceSize stageUsed = 0;
                  std::vector<Buf> spare; };
    struct Grid { VkImage img = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE; Buf host; int w = 0, h = 0; };
    bool openSlot();
    void readTimes();
    VkQueryPool tq_ = VK_NULL_HANDLE;
    bool timing_ = false;
    double tsNs_ = 0;
    std::uint64_t tsMask_ = ~std::uint64_t(0);
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> times_;   // ticket -> the batch's start and end ticks (read once it has finished)
    // 0.63.1: only batches submitted on the frame's thread are timed: a worker's (the light paths' fill, a start-up check) is not the frame's work and ran
    // beside it (here it made a frame's card time longer than the frame)
    void freeBuffer(Buf& b);
    bool gridTarget(Grid& g, int w, int h);
    void freeGrid(Grid& g);

    VkDevCtx c_;
    VkDraw2D* d2_ = nullptr;
    bool ok_ = false, failed_ = false, float64_ = false;
    VkCommandPool pool_ = VK_NULL_HANDLE;
    std::vector<Slot> slots_;
    int open_ = -1;
    std::uint64_t nextTicket_ = 1, doneTicket_ = 0;
    std::vector<std::pair<std::uint64_t, std::function<void()>>> drop_;   // destroyed once the batch of that ticket (and the frames after it) are done
    std::size_t spirv_ = 0;
    VkDescriptorSetLayout gridDsl_ = VK_NULL_HANDLE;
    VkPipelineLayout gridLayout_ = VK_NULL_HANDLE;
    VkPipeline gridPipe_[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkUniforms gridU_[2];
    std::vector<Grid> grids_;
    int gridsUsed_ = 0;
};

// the programs, each its own guarded steps (render/vk_card.cpp); null when it cannot be made (why says it)
IGroupCompute* makeVkGroupCompute(VkCards& c, std::string& why);
IOutlineCompute* makeVkOutlineCompute(VkCards& c, std::string& why);
IRockCompute* makeVkRockCompute(VkCards& c, std::string& why);
AtomCard* makeVkAtomCard(VkCards& c);
RingCard* makeVkRingCard(VkCards& c);
// 0.64.0: the Classic Cloud 3D dots (render/classic_card.h): made, recorded into the frame's batch, the squares' buffer the frame draws
ClassicCard* makeVkClassicCard(VkCards& c);
bool vkClassicRecord(ClassicCard* c, const ClassicDraw& d, long& bytes);
VkBuffer vkClassicSquares(ClassicCard* c);
bool vkClassicCells(ClassicCard* c, std::vector<float>& cells);                      // 0.65.0: the side view's Glow (the counts per 8 x 8-point cell)
bool vkClassicFinish(ClassicCard* c, const ClassicDraw& d, float light);   // 0.64.0: Saturn's rings per pixel (render/ring_card.h)
MoleculeCard* makeVkMoleculeCard(VkCards& c);
// 0.67.0: the nucleus' Density look (render/nucdens_card.h): made, its steps recorded into the frame's batch when something changed, the squares' buffer
class NucDensCard; struct NucDensDraw;
NucDensCard* makeVkNucDensCard(VkCards& c);
bool vkNucDensRecord(NucDensCard* c, const NucDensDraw& d);
VkBuffer vkNucDensSquares(NucDensCard* c);
VkBuffer vkRockPositions(IRockCompute* r, int slot);         // the rocks' places on the card (VK_NULL_HANDLE: not a Vulkan object)
VkBuffer vkJumpGroups(AtomCard* a);                          // the groups the jump's program made this frame
// the Density cloud's frame: the dots made and the grid summed in the open batch (record), then, after the batch, the dots drawn (draw)
int vkMolCloudRecord(MoleculeCard* m, const MolCloudDraw& d);
VkBuffer vkMolCloudDots(MoleculeCard* m);
// the Cloud 3D Glow dots (Renderer2D::cloudGpu) on Vulkan
class VkGlow;
VkGlow* makeVkGlow(VkCards& c, std::string& why);
void dropVkGlow(VkGlow* g);
int vkGlowRecord(VkGlow* g, const Renderer2D::CloudGpuDraw& d, VkBuffer jump, long& bytes);
VkBuffer vkGlowPlaces(VkGlow* g);
VkBuffer vkGlowGroups(VkGlow* g);
int vkPollFirstUses(VkCards& c, IGroupCompute* g, IOutlineCompute* o, IRockCompute* r, VkGlow* w);
// what each Vulkan guard keeps as its version (app/gpu_guard.h vulkanCardGuards' names): "vulkan <name> <hash of its programs' text>"
std::string vkCardVersion(const std::string& name);
// FNV-1a of a program's text, as the OpenGL guards' versions ("vulkan <name> <hash>")
std::string vkCardId(const char* name, std::initializer_list<const char*> texts);

}  // namespace render
