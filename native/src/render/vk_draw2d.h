// 0.60.0: Vulkan, step 2 of 5 (native/PLAN_VULKAN.md): renderer2d's drawing on Vulkan. The shapes are recorded by Renderer2D exactly as for OpenGL (one
// code path: the vertex and blob lists and the segments in order); only end() differs: Renderer2D::endVulkan (render/renderer2d_vk.cpp) hands the
// segments to this object, which draws them with the SAME shader text as OpenGL (render/shaders.h, made into SPIR-V at build time: CMakeLists.txt
// PL_VK2D_SHADERS), the same blend modes, texture filters, mip levels, the soft layer, the cloud's float layer and the grey pass.
//
// The picture is drawn with OpenGL's rows (its first row at the bottom, so the shaders, gl_FragCoord and the layers' texture coordinates are exactly
// OpenGL's), into an image of its own (4x multisampled with a resolve, as OpenGL's window; 1 sample in the grey pass and for Save image, as OpenGL's
// offscreen pictures), then copied into the window's picture area with its rows turned over (render/vk/present.*). Per frame in flight: one buffer the
// processor writes once (vertices, instances, the shaders' uniform blocks; persistently mapped) and one descriptor pool. The maps and the meshes are
// uploaded once. Nothing waits for the card in a normal frame; an image or buffer that is replaced is kept until the frames that used it are done.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <volk.h>

namespace app { class GpuGuard; }

namespace render {

// what the window's Vulkan backend (render/vk_backend.cpp) lends
struct VkDevCtx {
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties mem{};
    VkPhysicalDeviceLimits limits{};
    std::function<bool(VkResult, const char*)> check;                         // false (and logged) on an error
    std::function<bool(const std::function<void(VkCommandBuffer)>&)> once;   // one-time commands, submitted and waited for (start-up, uploads, Save image)
    app::GpuGuard* guard = nullptr;
};

class VkDraw2D {
public:
    enum Prog { kTri, kPlanet, kSquare, kBlob, kMesh, kComposite, kGray, kProgCount };
    enum Blend { kNoBlend, kNormal, kAdditive, kPremul, kOneOne, kBlendCount };   // as Renderer2D::end: normalBlend; additive blobs; premultiplied over; GL_ONE, GL_ONE
    enum Pass { kPassMs, kPass8, kPassF, kPassCount };                        // the picture (S samples), an 8-bit picture of 1 sample, the cloud's float picture

    ~VkDraw2D();
    // the shaders (their uniform blocks read from the SPIR-V), the passes, the pipelines (every program x blend x pass used), the samplers and the white
    // texture; swapPass: the window's render pass (the copy into the picture area). samples: 4 wanted (1: off, PL_NO_MSAA). False: why says it.
    bool init(const VkDevCtx& c, VkRenderPass swapPass, int samples, std::string& why);
    void destroy();
    bool upload(int map, int w, int h, const std::vector<std::uint8_t>& rgba, bool srgb, std::string& why);   // a picture map with its mip levels
    bool hasMap(int map) const { return map >= 0 && map < 32 && maps_[map].img != VK_NULL_HANDLE; }
    int samples() const { return samples_; }
    const std::string& floatFormatText() const { return floatText_; }
    const std::string& samplesText() const { return samplesText_; }
    std::size_t spirvBytes() const { return spirvBytes_; }

    // the frame: slot (the swapchain image's frame, whose fence was waited for), its command buffer, the number of slots
    void beginFrame(int slot, int slots, VkCommandBuffer cmd);
    bool failed() const { return failed_; }

    // ---- used by Renderer2D::endVulkan ----
    // the picture w x h pixels cleared to clear[]; samples: kPassMs (the window) or kPass8 (1 sample: grey pass, Save image); bytes: what will be written
    // this frame (vertices, instances, uniforms: an upper bound), sets: the draws
    bool beginPicture(int w, int h, const float clear[4], bool oneSample, std::size_t bytes, int sets, bool forShot);
    VkDeviceSize put(const void* data, std::size_t bytes);                   // into this frame's buffer; its offset
    void beginLayer(bool hdr, int w, int h);                                // the soft layer (8 bits) or the cloud's float layer, cleared to 0
    void endLayer();                                                        // back to the picture (its pixels kept)
    void composite(bool hdr, Blend b);                                      // the layer laid over the picture (kComposite)
    // uniforms by name, as glUniform* (kept per program until changed, as OpenGL's): a name the program does not use is ignored
    void setF(Prog p, const char* name, const float* v, int comps, int count = 1);
    void setI(Prog p, const char* name, const int* v, int comps, int count = 1);
    void setMat3T(Prog p, const char* name, const float K[9]);              // glUniformMatrix3fv(..., GL_TRUE, K)
    // a draw with the current pass: vertices from this frame's buffer at vbOff (binding 0) and, for the instanced programs, the corners at binding 0 and
    // the instances at ibOff (binding 1) (vertex buffer bindings). tex: 0 the white texture, 1..31 a map, -1 the current layer (composite), -2 the picture (grey pass)
    void draw(Prog p, Blend b, VkDeviceSize vbOff, int first, int count, VkDeviceSize ibOff = 0, int instances = 0, int firstInstance = 0, int tex = 0);
    // meshes (render/mesh.h MeshVertex), kept on the card
    int meshCreate(const void* v, std::size_t bytes);
    void meshDelete(int id);
    void drawMesh(int id, int first, int count);
    void endPicture(bool gray);                                             // the picture's last pass; gray: the grey pass (kGray) into its own image
    void noPicture() { drawn_ = false; }

    // ---- used by the window's backend ----
    bool pictureDrawn() const { return drawn_; }
    // inside the window's (or Save image's) render pass: the picture copied into the area (x, y, w, h: pixels from the top left)
    void present(VkCommandBuffer cmd, int x, int y, int w, int h);
    void dropShot();                                                        // Save image's own images (after its picture was used)

    struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void* map = nullptr; VkDeviceSize size = 0; };
    struct Img { VkImage img = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE; int w = 0, h = 0; };
    struct Member { std::uint32_t offset = 0, arrayStride = 0, matrixStride = 0; };
    struct Block { std::uint32_t size = 0; std::map<std::string, Member> m; std::vector<std::uint8_t> data; };
    struct Shader { std::map<std::string, int> in, out; Block block; int blockBinding = -1, samplerBinding = -1; };   // (reflection of one stage)

private:
    struct Targets { int w = 0, h = 0, s = 1; Img ms, pic, gray; VkFramebuffer fbPic = VK_NULL_HANDLE, fbGray = VK_NULL_HANDLE; };
    struct Layer { int w = 0, h = 0; Img img; VkFramebuffer fb = VK_NULL_HANDLE; };
    struct Slot { Buf buf; VkDeviceSize used = 0; VkDescriptorPool pool = VK_NULL_HANDLE; std::uint32_t poolSets = 0; std::uint64_t frame = 0; };
    struct Program { Block blk[2]; VkPipeline pipe[kPassCount][kBlendCount] = {}; };
    struct Mesh { Buf buf; bool used = false; };

    bool makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buf& b, const char* what);
    bool makeImage(int w, int h, VkFormat f, VkSampleCountFlagBits s, VkImageUsageFlags usage, int mips, Img& i, const char* what);
    void dropBuffer(Buf& b);                                                // kept until the frames that may use it are done
    void dropImage(Img& i);
    void dropFramebuffer(VkFramebuffer& f);
    void collect();                                                          // what is no longer used, destroyed
    bool targetsFor(Targets& t, int w, int h, int s);
    bool layerFor(Layer& l, bool hdr, int w, int h);
    void beginPass(Pass p, VkFramebuffer fb, int w, int h, bool clear, const float c[4]);
    void endPass();
    VkDescriptorSet descriptors(const Program& pr, VkImageView tex, VkSampler smp);
    VkPipeline makePipeline(int prog, Blend b, Pass p, VkShaderModule vs, VkShaderModule fs);

    VkDevCtx c_;
    bool ok_ = false, failed_ = false, drawn_ = false;
    int samples_ = 1;
    std::string floatText_, samplesText_;
    std::size_t spirvBytes_ = 0;
    VkFormat floatFormat_ = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkRenderPass passes_[kPassCount][2] = {};                                // [pass][0 clear, 1 load]
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE, presentSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE, presentLayout_ = VK_NULL_HANDLE;
    VkPipeline presentPipe_ = VK_NULL_HANDLE;
    VkSampler linearClamp_ = VK_NULL_HANDLE, nearestClamp_ = VK_NULL_HANDLE, mapSampler_ = VK_NULL_HANDLE;
    Program prog_[kProgCount];
    Img white_, maps_[32];
    Buf corners_;
    std::vector<Slot> slots_;
    int slot_ = 0;
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    std::uint64_t frameNo_ = 0, doneUpTo_ = 0;
    std::vector<std::pair<std::uint64_t, std::function<void()>>> garbage_;
    Targets screen_, shot_, *cur_ = nullptr;
    Layer soft_, hdr_;
    bool inPass_ = false, inLayer_ = false, layerHdr_ = false, oneSample_ = false;
    Pass pass_ = kPassMs;
    int passW_ = 0, passH_ = 0;
    VkDeviceSize vbBound_ = ~VkDeviceSize(0);
    std::vector<Mesh> meshes_;
    bool usedGray_ = false;
};

// SPIR-V reflection of one stage (exposed for the start-up step's log and for tests)
bool reflectSpirv(const std::uint32_t* words, std::size_t count, VkDraw2D::Shader& out, std::string& why);

}  // namespace render
