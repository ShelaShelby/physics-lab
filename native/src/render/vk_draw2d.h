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
    std::uint32_t family = 0;                                                 // 0.61.0: the queue's family, and whether it also runs compute shaders
    bool compute = false;
};

// 0.63.0 (Vulkan step 5): synchronization2 everywhere (as render/vk_blackhole.cpp): one image's layout change and the writes before it made visible to the
// reads and writes after it; one memory barrier (buffers)
inline void vkImageBarrier2(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags2 ss, VkAccessFlags2 sa, VkPipelineStageFlags2 ds,
                            VkAccessFlags2 da, std::uint32_t baseMip = 0, std::uint32_t mips = 1) {
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = ss; b.srcAccessMask = sa; b.dstStageMask = ds; b.dstAccessMask = da;
    b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = baseMip;
    b.subresourceRange.levelCount = mips;
    b.subresourceRange.layerCount = 1;
    VkDependencyInfo d{};
    d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}
inline void vkMemoryBarrier2(VkCommandBuffer cb, VkPipelineStageFlags2 ss, VkAccessFlags2 sa, VkPipelineStageFlags2 ds, VkAccessFlags2 da) {
    VkMemoryBarrier2 m{};
    m.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    m.srcStageMask = ss; m.srcAccessMask = sa; m.dstStageMask = ds; m.dstAccessMask = da;
    VkDependencyInfo d{};
    d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    d.memoryBarrierCount = 1;
    d.pMemoryBarriers = &m;
    vkCmdPipelineBarrier2(cb, &d);
}

class VkDraw2D {
public:
    enum Prog { kTri, kPlanet, kSquare, kBlob, kMesh, kComposite, kGray, kCloud, kMolDot, kRings, kLine, kSurf, kProgCount };   // 0.61.0: kCloud (kCloudVS), kMolDot (kMolDotVS), with kBlobFS; 0.64.0: kRings (kVertexShader, kRingsFS); 0.68.0: kLine (kLinesVS, kFragmentShader), kSurf (kSurfaceVS, kFragmentShader)
    enum Blend { kNoBlend, kNormal, kAdditive, kPremul, kOneOne, kBlendCount };   // as Renderer2D::end: normalBlend; additive blobs; premultiplied over; GL_ONE, GL_ONE
    enum Pass { kPassMs, kPass8, kPassF, kPassCount };                        // the picture (S samples), an 8-bit picture of 1 sample, the cloud's float picture

    ~VkDraw2D();
    // the shaders (their uniform blocks read from the SPIR-V), the passes, the pipelines (every program x blend x pass used), the samplers and the white
    // texture; windowFormat: the window's format (the copy into the picture area). samples: 4 wanted (1: off, PL_NO_MSAA). False: why says it.
    bool init(const VkDevCtx& c, VkFormat windowFormat, int samples, std::string& why);   // (0.63.0: the window's format: dynamic rendering, no render passes)
    void destroy();
    bool upload(int map, int w, int h, const std::vector<std::uint8_t>& rgba, bool srgb, std::string& why);   // a picture map with its mip levels
    bool hasMap(int map) const { return map >= 0 && map < 32 && maps_[map].img != VK_NULL_HANDLE; }
    // 0.62.0 (step 4): the Black Hole Lab's (render/vk_blackhole.h): a map's view and its sampler (the sky's), and two pictures of its own the frame draws from:
    // draw(..., tex) -3 the open-sky mask (kTri's uMask), -4 the finished bent picture (kComposite, kNoBlend)
    VkImageView mapView(int map) const { return hasMap(map) ? maps_[map].view : white_.view; }
    VkSampler mapSampler() const { return mapSampler_; }
    void setExternal(int k, VkImageView v) { if (k >= 0 && k < 2) ext_[k] = v; }
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
    // 0.61.0 (Vulkan step 3): the card programs' draws, from buffers on the card: kSquare with the rocks' places (binding 1, 2 floats an instance), kCloud
    // with the dots' places (binding 1, 3 floats) and groups (binding 2, one byte, read as a float as OpenGL's GL_UNSIGNED_BYTE), kMolDot with the dots made
    // by kMolDotsCS (binding 1, 4 floats); the corners at binding 0
    void drawInst(Prog p, Blend b, VkBuffer inst1, VkBuffer inst2, int instances);
    // 0.68.0: the programs whose vertex shader reads storage buffers (a second descriptor set, four buffers; a null one takes the first's place): the
    // Orbit Lab's lines (kLine, its vertex count from the card: indirect) and the Molecule Lab's Density surface (kSurf, count vertices); the normal blend
    void drawStorage(Prog p, const VkBuffer bufs[4], VkBuffer indirect, std::uint32_t count = 0);
    // kCloud and kMolDot are made when their card program is first built (their own guarded steps, render/vk_card.cpp), not at init: false, why says it
    // (0.64.0: kRings too, Saturn's rings per pixel, the normal blend only)
    bool addProgram(Prog p, std::string& why);
    bool hasProgram(Prog p) const { return prog_[p].pipe[kPass8][kNormal] != VK_NULL_HANDLE; }
    // a buffer or other object that a frame on the card may still use: f() is called once the frames up to this one are done
    void later(std::function<void()> f) { garbage_.push_back({frameNo_, std::move(f)}); }
    // meshes (render/mesh.h MeshVertex), kept on the card
    int meshCreate(const void* v, std::size_t bytes);
    void meshDelete(int id);
    void drawMesh(int id, int first, int count);
    void endPicture(bool gray);                                             // the picture's last pass; gray: the grey pass (kGray) into its own image
    void noPicture() { drawn_ = false; }
    // 0.67.0: the window's picture drawn last shown again, nothing drawn (the Orbit Lab at rest): true when that picture is whole and of this size and grey
    // choice; false: it must be drawn
    bool keepPicture(int w, int h, bool gray);

    // ---- used by the window's backend ----
    bool pictureDrawn() const { return drawn_; }
    // inside the window's (or Save image's) render pass: the picture copied into the area (x, y, w, h: pixels from the top left)
    void present(VkCommandBuffer cmd, int x, int y, int w, int h);
    void dropShot();                                                        // Save image's own images (after its picture was used)

    struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void* map = nullptr; VkDeviceSize size = 0; };
    struct Img { VkImage img = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView view = VK_NULL_HANDLE; int w = 0, h = 0; VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; };
    struct Member { std::uint32_t offset = 0, arrayStride = 0, matrixStride = 0; };
    struct Block { std::uint32_t size = 0; std::map<std::string, Member> m; std::vector<std::uint8_t> data; };
    struct Shader { std::map<std::string, int> in, out; Block block; int blockBinding = -1, samplerBinding = -1; std::vector<int> storage; std::map<std::string, int> samplers; };   // (0.62.0: every texture by name)   // (reflection of one stage; 0.61.0: its storage buffers' bindings)

private:
    struct Targets { int w = 0, h = 0, s = 1; Img ms, pic, gray; };
    struct Layer { int w = 0, h = 0; Img img; };
    struct Slot { Buf buf; VkDeviceSize used = 0; VkDescriptorPool pool = VK_NULL_HANDLE; std::uint32_t poolSets = 0; std::uint64_t frame = 0;
                  VkDescriptorPool linesPool = VK_NULL_HANDLE; };   // (0.68.0: the lines' storage sets)
    struct Program { Block blk[2]; VkPipeline pipe[kPassCount][kBlendCount] = {}; };
    struct Mesh { Buf buf; bool used = false; };

    bool makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buf& b, const char* what);
    bool makeImage(int w, int h, VkFormat f, VkSampleCountFlagBits s, VkImageUsageFlags usage, int mips, Img& i, const char* what);
    void dropBuffer(Buf& b);                                                // kept until the frames that may use it are done
    void dropImage(Img& i);
    void collect();                                                          // what is no longer used, destroyed
    bool targetsFor(Targets& t, int w, int h, int s);
    bool layerFor(Layer& l, bool hdr, int w, int h);
    // 0.63.0: dynamic rendering into color (resolved into *resolve: the multisampled picture), synchronization2 barriers before and after (each image's layout tracked)
    void beginPass(Pass p, Img& color, Img* resolve, int w, int h, bool clear, const float c[4]);
    Img* passColor_ = nullptr;
    Img* passResolve_ = nullptr;
    void endPass();
    VkDescriptorSet descriptors(const Program& pr, VkImageView tex, VkSampler smp);
    VkPipeline makePipeline(int prog, Blend b, Pass p, VkShaderModule vs, VkShaderModule fs);

    VkDevCtx c_;
    bool ok_ = false, failed_ = false, drawn_ = false;
    int samples_ = 1;
    std::string floatText_, samplesText_;
    std::size_t spirvBytes_ = 0;
    VkFormat floatFormat_ = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE, presentSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE, presentLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout linesSetLayout_ = VK_NULL_HANDLE;   // 0.68.0: kLine's set 1 (four storage buffers) and its pipeline layout
    VkPipelineLayout linesLayout_ = VK_NULL_HANDLE;
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
    bool screenKept_ = false, screenGray_ = false;   // 0.67.0: the window's picture is whole (its last pass ended), and whether it used the grey pass
    VkImageView ext_[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
};

// SPIR-V reflection of one stage (exposed for the start-up step's log and for tests)
bool reflectSpirv(const std::uint32_t* words, std::size_t count, VkDraw2D::Shader& out, std::string& why);

}  // namespace render
