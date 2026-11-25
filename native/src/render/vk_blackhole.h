// The Black Hole Lab's GPU programs on Vulkan, from the shader text in render/shaders.h (
// made into SPIR-V at build time: CMakeLists.txt PL_VK2D_SHADERS; kKerrSkyFS assembled from its parts as render/kerr_sky.cpp does). Renderer2D's Black Hole
// calls (lensSky, skyProbe, kerrSky's stages, the exposure, the glow and the glare) hand their work to this object on Vulkan. The rules:
// the picture drawn anew only when the view changed, the exposure reduced on the card, the lower resolution while
// moving on a slow card, the open-sky mask kept with the picture, the Film glow and the eye's glare (its veil made on a thread of its own).
//
// The passes are recorded into the card programs' batch (render/vk_card.h VkCards) that is submitted before the frame that shows them; the frame then lays
// the finished picture over the lab's picture (VkDraw2D kComposite, no blend). Vulkan 1.3 is used here from the start: dynamic
// rendering (no render passes or framebuffers) and synchronization2's barriers, each image's layout and last use tracked so every barrier names exactly the
// stages and accesses before and after it. Read-backs (the exposure's white, the probe, the glare's blocks) go into host-visible buffers and are read once
// their batch's fence has signalled: never a wait in a normal frame. The card's time of the sky pass: timestamp
// queries (timestampPeriod, timestampValidBits; none: no times), read back without waiting.
//
// The pipelines of the big shaders (the lens pass, the spinning hole's table pass, the three sky stages, the light paths' compute shader) are made on a
// worker thread, never in a frame; a VkPipelineCache kept in the settings folder (vulkan-pipeline-cache.bin: our header with its size and FNV-1a hash, then
// the driver's data, whose own header must name this card) makes later starts fast. A file that does not pass is ignored, never trusted.
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <volk.h>

#include "kerr_compute.h"
#include "renderer2d.h"
#include "vk_card.h"

namespace render {

class VkBlackHole {
public:
    using LensSky = Renderer2D::LensSky;
    // the programs (each its SPIR-V from shaders.h's text); the first six are made on the worker thread
    enum Prog { pLens, pKerr, pSkyTiny, pSkyMath, pSky, pMaxDown, pGlowDown, pGlowBlur, pGlowAdd, pGlareDown, pGlareAdd, pCopy, pExpSum, pGlareLevel, pGlareSum, pProgCount };
    static const char* progName(int p);
    static std::size_t spirvBytes(int p);

    ~VkBlackHole();
    // the formats (queried), the samplers, the query pool, the pipeline cache (read from cacheDir, "" = none) and the small programs; the lens and the
    // spinning hole's table programs are then made on the worker thread (their guard: "[vulkan] [lens] "). False: why says it.
    bool init(VkCards& c, const std::string& cacheDir, std::string& why);
    void destroy();                                      // waits for the worker and the card; the cache written back
    bool ok() const { return ok_; }

    // ---- KerrSky's stages on Vulkan (render/kerr_sky.h): 0 the tiny shader, 1 the elliptic functions alone, 2 the whole sky shader. The first call starts
    // the stage's pipeline on the worker (its steps told to steps from there); later calls ask: -1 failed (why), 0 under way, 1 made
    int buildSky(int stage, KerrSteps* steps, std::string& why);
    bool skyReady() const;
    double pipelineMs(int p) const;
    std::string pipelineText() const;                    // each pipeline made: its time and SPIR-V bytes (the log)

    // ---- the frame's work (Renderer2D::endVulkan, before the picture's pass): records into the card programs' open batch ----
    // ptW, ptH: the picture in points (Renderer2D's width_, height_), skyTexW: the sky map's width (its mip level), fbW x fbH: the picture in pixels
    void lens(const LensSky& L, double ptW, double ptH, int skyTexW, int fbW, int fbH);
    VkImageView picture() const { return pictureView_; } // the finished picture to lay over (null: nothing to draw)
    VkImageView mask() const { return maskValid_ ? mask_.view : VK_NULL_HANDLE; }   // the open-sky mask (maskedDisc), null when not valid
    // the start-up check and the tests (Renderer2D::skyProbe): 16 floats per pixel, rows from the bottom; queue: read later (probeDone, probeRead)
    void probe(const LensSky& L, int w, int h, bool queue, std::vector<float>* out);
    bool probeDone();
    void probeRead(std::vector<float>& out);
    // (the tests' hooks, PL_BH_COMPARE): the float outputs of one picture (LensSky::debug, debugDisc, the
    // sky's skyMode 1..4) drawn off screen into a float image, fbW x fbH x 4 floats, rows from the bottom, waited for (empty: not drawn); the elliptic
    // functions alone (KerrSky stage 1, kKerrSkyMathPart) on argument sets of 12 floats, 8 numbers a set (render/kerr_sky.h), waited for; the exposure's
    // last read-back, waited for (0: none since the last call)
    void debugPicture(const LensSky& L, double ptW, double ptH, int skyTexW, int fbW, int fbH, std::vector<float>& out);
    void math(const std::vector<float>& in, std::vector<float>& out);
    void veilTest(int W, int H, double cx, double cy, double K, int b, const std::vector<float>& src, std::vector<float>& out,
                  void (*tables)(int, int, double, double, double, std::vector<float>&, std::vector<float>&, double[2]));   // 0.85.5: the card's glare veil
    double whiteWait();

    const Renderer2D::SkyStats& skyStats() const { return skyStats_; }
    const Renderer2D::GlareStats& glareStats() const { return glareStats_; }
    long redraws() const { return redraws_; }
    // 0.85.3: the lens pass's pipeline: 0 not asked, 1 being made, 2 made, -1 failed. The sky's first full picture needs it made.
    int lensState() const { return prog_[pLens].state.load(); }
    // (0.72.0: a veil being made again only because the moving gas moved on, while a veil is shown, is not "under way": the picture is finished)
    bool glareBusy() const { return (glareStage_ != 0 && !(want_.gasOnly && glareValid_)) || (want_.gen != 0 && !glareValid_); }
    VkPipelineCache cache() const { return cache_; }
    void stopGlareThread();
    // PL_VK_CARDFAIL=sky / lens (tests): that program's pipeline fails as a driver that says no would
    static bool forced(const char* name);

private:
    struct Tex {
        VkImage img = VK_NULL_HANDLE; VkDeviceMemory mem = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE;
        VkFormat fmt = VK_FORMAT_UNDEFINED; int w = 0, h = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE; VkAccessFlags2 access = VK_ACCESS_2_NONE;
    };
    struct Program {
        VkUniforms u;
        std::map<std::string, int> samplers;             // name -> binding (from the SPIR-V)
        int blockBinding = -1, outputs = 1;
        VkShaderModule fs = VK_NULL_HANDLE;
        VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
        VkPipelineLayout pl = VK_NULL_HANDLE;
        VkPipeline pipe[3] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};   // [0] its full set of targets, [1] one RGBA32F target (the exposure, the probe), [2] one R8
        std::atomic<int> state{0};                       // 0 not asked, 1 being made, 2 made, -1 failed
        std::string why;
        double ms = 0;
    };
    struct Bind { const char* name; VkImageView view; VkSampler sampler; };
    struct HostBuf { VkCards::Buf b; std::uint64_t ticket = 0; bool busy = false; };

    // ---- the engine ----
    bool makeTex(Tex& t, int w, int h, VkFormat f, VkImageUsageFlags usage, const char* what);
    void dropTex(Tex& t);                                // once the batches and frames that may use it are done
    void use(VkCommandBuffer cb, Tex& t, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access);
    bool prepare(int p, std::string& why);               // reflection, layouts, module (made where it is called)
    bool makePipelines(int p, std::string& why);         // the pipelines (the worker or the caller)
    void startWorker(int p, KerrSteps* steps, const std::string& step);
    // one pass: targets (their views and formats; variant 1 = one RGBA32F target), the viewport in the targets' pixels, the textures by name
    void begin(VkCommandBuffer cb, std::initializer_list<Tex*> targets, int vx, int vy, int vw, int vh, bool clear);
    void draw(VkCommandBuffer cb, int p, int variant, std::initializer_list<Bind> tex, int vx, int vy, int vw, int vh);
    void end(VkCommandBuffer cb);
    void upload(VkCommandBuffer cb, Tex& t, int w, int h, VkFormat f, const float* data, int comps, const char* what);
    void readBack(VkCommandBuffer cb, Tex& t, int x, int y, int w, int h, HostBuf& hb);
    void setMat3T(int p, const char* name, const float K[9]);

    // ---- the Black Hole Lab's passes (render/renderer2d.cpp's, ported) ----
    void lensPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, int ox, int oy, bool disc, bool bend, bool exp, bool raw, int variant);
    void kerrPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, bool kdisc, bool raw, int variant = 0);
    void skyPass(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, int ox, int oy, double scale, int mode, bool raw, int variant, int vx, int vy, int vw, int vh);
    void exposure(VkCommandBuffer cb, const LensSky& L, const std::function<void(int nx, int ny)>& sample);
    void times();
    void skyPicture(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH, bool raw);
    void makeGlow(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH);
    void glareDown(VkCommandBuffer cb, int fbW, int fbH, int b, int nx, int ny);
    void glareUpload(VkCommandBuffer cb, int nx, int ny, const std::vector<float>& veil);
    void makeGlare(const LensSky& L, int fbW, int fbH);
    void glareWant(const LensSky& L, int fbW, int fbH);
    void glarePump();
    void compose(VkCommandBuffer cb, const LensSky& L, int fbW, int fbH);
    void readCache(const std::string& dir);
    void writeCache();
public:
    // 0.67.0: the helper process's cache file (app/gpu_helper.h) merged into this one at the start, before any pipeline is made
    void mergeCache(const std::string& path);
private:

    VkCards* c_ = nullptr;
    VkDevice dev_ = VK_NULL_HANDLE;
    bool ok_ = false, linearFloat_ = false, timestamps_ = false;
    double tsPeriodNs_ = 1;
    std::uint64_t tsMask_ = ~std::uint64_t(0);
    VkSampler nearest_ = VK_NULL_HANDLE, linear_ = VK_NULL_HANDLE;
    VkShaderModule vs_ = VK_NULL_HANDLE;
    VkCards::Buf corners_;
    Tex white_;
    Program prog_[pProgCount];
    VkPipelineCache cache_ = VK_NULL_HANDLE;
    std::string cachePath_, cacheText_;
    std::mutex workMx_;
    std::vector<std::thread> workers_;
    std::atomic<int> skyStage_[3] = {{0}, {0}, {0}};    // per KerrSky stage: as Program::state
    // the textures
    Tex lensTex_, layer_[2], mask_, out_, low_[4], glow_[2], glareSrc_, glareVeil_, probe_, debug_, args_, mathOut_;   // (0.69.0: the tests' three)
    void toneUniforms(VkUniforms& u, const LensSky& L);   // 0.70.0
    void gasTextures(VkCommandBuffer cb, const LensSky& L);   // 0.79.0: the streaks' field and the thickness table to the card when new
    void gasUniforms(VkUniforms& u, const LensSky& L);    // 0.72.0
    std::vector<Tex> exp_;
    std::vector<Tex> sum_;                 // 0.70.0: the log-average's chain (kExpSumFS); its last texel is the whole exposure
    Tex table_, discU_, discP_, discC_, kA_, kB_, kE_, kT_, kF_, discTt_;
    Tex streak_, hgt_;                                     // 0.79.0: the streaks' field (sent once), the disc's thickness table
    std::uint64_t hgtVer_ = ~std::uint64_t(0);
    bool discTtOk_ = false;                                // 0.72.0: discTt_ holds the travel times of the disc table now sent
    std::uint64_t tableVer_ = ~std::uint64_t(0), discVer_ = ~std::uint64_t(0), colVer_ = ~std::uint64_t(0), kVer_ = ~std::uint64_t(0), kFVer_ = ~std::uint64_t(0);
    int lensN_ = 0;
    std::vector<int> expW_, expH_;
    HostBuf whiteBuf_, probeBuf_, glareBuf_, debugBuf_;
    int probeW_ = 0, probeH_ = 0;
    VkQueryPool queries_ = VK_NULL_HANDLE;
    std::uint64_t qTicket_[4] = {0, 0, 0, 0};
    int qHead_ = 0, qTail_ = 0;
    double qScale_[4] = {1, 1, 1, 1};
    bool qReset_ = false;
    // the kept picture
    LensSky last_{};
    double lastPtW_ = 0, ptW_ = 1, ptH_ = 1;
    int skyTexW_ = 1;
    bool lensValid_ = false, skyDrawn_ = false, maskValid_ = false, glowValid_ = false, glareValid_ = false, composed_ = false;
    VkImageView pictureView_ = VK_NULL_HANDLE;
    long redraws_ = 0;
    int guardState_ = 0;                                 // the lens guard: 0 its pipelines, 1 its first full picture under way, 2 done, -1 failed
    std::uint64_t firstTicket_ = 0;
    Renderer2D::SkyStats skyStats_;
    Renderer2D::GlareStats glareStats_;
    // the glare (as Renderer2D's GlareJob; stage 0 idle, 3 the blocks read back under way, 2 the veil on its thread)
    struct GlareJob { int fbW = 0, fbH = 0, b = 1, nx = 0, ny = 0; double cx = 0, cy = 0, K = 0; std::uint64_t gen = 0; bool gasOnly = false;
                      void (*veil)(int, int, double, double, double, const std::vector<float>&, std::vector<float>&, double[3]) = nullptr; void (*lower)() = nullptr;
                      void (*tables)(int, int, double, double, double, std::vector<float>&, std::vector<float>&, double[2]) = nullptr; };
    GlareJob want_, job_;
    std::chrono::steady_clock::time_point glareLastStart_{};   // 0.72.0
    std::uint64_t glareGen_ = 0;
    int glareStage_ = 0, glareNx_ = 0, glareNy_ = 0, glareB_ = 1;
    std::thread glareThread_;
    std::atomic<bool> glareDone_{false};
    std::vector<float> glareSrcBuf_, glareVeilBuf_;
    double glareOut_[3] = {0, 0, 0};
    std::chrono::steady_clock::time_point glareT0_;
    float glareKeep_ = 0;
    // 0.85.5: the card's veil: per level its light, directions, spreads (two textures) and veil; the near weights and flat ratios (for the finest block of
    // glareTabB_ pixels at glareTabK_ pixels per radian). veilCard_: the veil shown is the card's (gL_[0].S), else the processor's upload (glareVeil_).
    struct GlareLevel { Tex E, D, M1, M2, S; int nx = 0, ny = 0, f = 1; };
    std::vector<GlareLevel> gL_;
    Tex glareNear_, glareCorr_;
    int glareTabB_ = 0;
    double glareTabK_ = 0;
    bool veilCard_ = false;
    float glareEye_[2] = {0, 0};                         // the eye's age and pigmentation (light.h), from the tables' call
    // the veil on the card from glareSrc_ (kGlareLevelFS, kGlareSumFS) into gL_[0].S, recorded into cb; false: it cannot be made (the processor's then)
    bool glareOnCard(VkCommandBuffer cb, const GlareJob& j);
    bool glareCard() const { return prog_[pGlareLevel].state == 2 && prog_[pGlareSum].state == 2; }
    // the frame's pass state
    VkCommandBuffer cb_ = VK_NULL_HANDLE;
    VkFormat passFmt_[4] = {};
    int passN_ = 0;
};

// ---- the light paths on Vulkan (kKerrTableCS, double precision: shaderFloat64), the interface of render/kerr_compute.h. The pipeline is
// made on a thread of its own (IKerrCompute::building() until then; its step told to steps from there); every queue* call submits its batch, done() asks
// its fence. Null: this card has no double precision (why).
IKerrCompute* makeVkKerrCompute(VkCards& c, VkPipelineCache cache, KerrSteps* steps, std::string& why);
int vkKerrState(IKerrCompute* k, std::string& why);        // 1 being made, 2 made, -1 failed (why)
std::size_t vkKerrSpirvBytes();

}  // namespace render
