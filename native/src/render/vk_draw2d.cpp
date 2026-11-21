// 0.60.0: Vulkan, step 2 of 5: renderer2d's drawing on Vulkan (render/vk_draw2d.h)
#include "app/frame_cost.h"
#include "render/vk_draw2d.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "app/gpu_guard.h"

// SPIR-V made at build time from render/shaders.h (CMakeLists.txt PL_VK2D_SHADERS) (0.69.0: render/vk/present.* are gone: the copy is a blit)
#include "r2d_tri_vert.h"
#include "r2d_tri_frag.h"
#include "r2d_sq_vert.h"
#include "r2d_sq_frag.h"
#include "r2d_blob_vert.h"
#include "r2d_blob_frag.h"
#include "r2d_cmp_vert.h"
#include "r2d_cmp_frag.h"
#include "r2d_gray_frag.h"
#include "r2d_planet_frag.h"
#include "r2d_chart_vert.h"
#include "r2d_cloud_vert.h"
#include "r2d_moldot_vert.h"
#include "r2d_rings_frag.h"
#include "r2d_lines_vert.h"
#include "r2d_surface_vert.h"

namespace render {

namespace {

constexpr VkFormat kRgba8 = VK_FORMAT_R8G8B8A8_UNORM;   // OpenGL's GL_RGBA8 (the window, the soft layer, the grey picture)

std::string readString(const std::uint32_t* w, std::size_t maxWords) {
    std::string s;
    for (std::size_t i = 0; i < maxWords; i++)
        for (int b = 0; b < 4; b++) {
            const char ch = static_cast<char>((w[i] >> (8 * b)) & 0xffu);
            if (!ch) return s;
            s += ch;
        }
    return s;
}

struct Code { const std::uint32_t* w; std::size_t bytes; };

}  // namespace

// ---- SPIR-V reflection: the stage's inputs and outputs with their locations, its default uniform block (members, offsets, strides), its texture ----
bool reflectSpirv(const std::uint32_t* w, std::size_t n, VkDraw2D::Shader& out, std::string& why) {
    if (n < 5 || w[0] != 0x07230203u) { why = "not SPIR-V"; return false; }
    std::map<std::uint32_t, std::string> names;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::string> memberNames;
    std::map<std::uint32_t, std::uint32_t> location, binding, arrayStride, constant;
    std::map<std::uint32_t, bool> isBlock, isBufferBlock;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> offset, matrixStride;
    struct Type { std::uint32_t op = 0; std::vector<std::uint32_t> a; };
    std::map<std::uint32_t, Type> types;
    struct Var { std::uint32_t type = 0, storage = 0; };
    std::map<std::uint32_t, Var> vars;
    for (std::size_t i = 5; i < n;) {
        const std::uint32_t wc = w[i] >> 16, op = w[i] & 0xffffu;
        if (wc == 0 || i + wc > n) { why = "a broken SPIR-V instruction"; return false; }
        const std::uint32_t* o = w + i + 1;
        const std::size_t m = wc - 1;
        switch (op) {
        case 5: if (m >= 1) names[o[0]] = readString(o + 1, m - 1); break;                                       // OpName
        case 6: if (m >= 2) memberNames[{o[0], o[1]}] = readString(o + 2, m - 2); break;                          // OpMemberName
        case 71:                                                                                                  // OpDecorate
            if (m >= 2) {
                if (o[1] == 2) isBlock[o[0]] = true;
                else if (o[1] == 3) isBufferBlock[o[0]] = true;                                                   // (0.61.0: a storage buffer)
                else if (m >= 3 && o[1] == 6) arrayStride[o[0]] = o[2];
                else if (m >= 3 && o[1] == 30) location[o[0]] = o[2];
                else if (m >= 3 && o[1] == 33) binding[o[0]] = o[2];
            }
            break;
        case 72:                                                                                                  // OpMemberDecorate
            if (m >= 4 && o[2] == 35) offset[{o[0], o[1]}] = o[3];
            else if (m >= 4 && o[2] == 7) matrixStride[{o[0], o[1]}] = o[3];
            break;
        case 21: case 22: case 23: case 24: case 28: case 30: case 32: case 25: case 26: case 27:                 // the types
            if (m >= 1) { Type t; t.op = op; t.a.assign(o + 1, o + m); types[o[0]] = t; }
            break;
        case 43: if (m >= 3) constant[o[1]] = o[2]; break;                                                        // OpConstant (32 bits)
        case 59: if (m >= 3) vars[o[1]] = {o[0], o[2]}; break;                                                    // OpVariable
        default: break;
        }
        i += wc;
    }
    // the bytes a member of the block takes (scalars and vectors of 32 bits, matrices by their stride, arrays by theirs)
    std::function<std::uint32_t(std::uint32_t, std::uint32_t)> sizeOf = [&](std::uint32_t t, std::uint32_t mstride) -> std::uint32_t {
        const auto it = types.find(t);
        if (it == types.end()) return 0;
        const Type& T = it->second;
        if (T.op == 21 || T.op == 22) return T.a.empty() ? 4u : T.a[0] / 8u;      // (0.61.0: by its width: a double is 8 bytes)
        if (T.op == 23 && T.a.size() >= 2) return sizeOf(T.a[0], 0) * T.a[1];
        if (T.op == 24 && T.a.size() >= 2) return (mstride ? mstride : sizeOf(T.a[0], 0)) * T.a[1];
        if (T.op == 28 && T.a.size() >= 2) return arrayStride[t] * constant[T.a[1]];
        return 0;
    };
    for (const auto& [id, v] : vars) {
        const auto pt = types.find(v.type);
        if (pt == types.end() || pt->second.op != 32 || pt->second.a.size() < 2) continue;
        const std::uint32_t pointee = pt->second.a[1];
        const std::string& nm = names[id];
        if ((v.storage == 1 || v.storage == 3) && location.count(id) && nm.rfind("gl_", 0) != 0) (v.storage == 1 ? out.in : out.out)[nm] = static_cast<int>(location[id]);
        if ((v.storage == 2 && isBufferBlock[pointee]) || (v.storage == 12 && isBlock[pointee])) {               // 0.61.0: a storage buffer (compute)
            out.storage.push_back(binding.count(id) ? static_cast<int>(binding[id]) : -1);
            continue;
        }
        if (v.storage == 2 && isBlock[pointee]) {
            const Type& S = types[pointee];
            out.blockBinding = binding.count(id) ? static_cast<int>(binding[id]) : -1;
            std::uint32_t end = 0;
            for (std::uint32_t j = 0; j < S.a.size(); j++) {
                VkDraw2D::Member mb;
                mb.offset = offset[{pointee, j}];
                mb.matrixStride = matrixStride[{pointee, j}];
                const auto mt = types.find(S.a[j]);
                if (mt != types.end() && mt->second.op == 28) mb.arrayStride = arrayStride[S.a[j]];
                end = std::max(end, mb.offset + sizeOf(S.a[j], mb.matrixStride));
                out.block.m[memberNames[{pointee, j}]] = mb;
            }
            out.block.size = (end + 15u) / 16u * 16u;
            out.block.data.assign(out.block.size, 0);
        }
        if (v.storage == 0) {
            const auto tt = types.find(pointee);
            if (tt != types.end() && tt->second.op == 27) { out.samplerBinding = binding.count(id) ? static_cast<int>(binding[id]) : -1; out.samplers[nm] = out.samplerBinding; }
        }
    }
    return true;
}

VkDraw2D::~VkDraw2D() { destroy(); }

bool VkDraw2D::makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buf& b, const char* what) {
    b = Buf();
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!c_.check(vkCreateBuffer(c_.device, &bi, nullptr, &b.b), what)) { b.b = VK_NULL_HANDLE; return false; }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(c_.device, b.b, &req);
    const VkMemoryPropertyFlags want = host ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++)
        if ((req.memoryTypeBits & (1u << i)) && (c_.mem.memoryTypes[i].propertyFlags & want) == want) type = i;
    if (type == UINT32_MAX && !host)
        for (uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++) if (req.memoryTypeBits & (1u << i)) type = i;
    if (type == UINT32_MAX) { c_.check(VK_ERROR_FEATURE_NOT_PRESENT, what); vkDestroyBuffer(c_.device, b.b, nullptr); b.b = VK_NULL_HANDLE; return false; }
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (!c_.check(vkAllocateMemory(c_.device, &mi, nullptr, &b.m), what)) { b.m = VK_NULL_HANDLE; vkDestroyBuffer(c_.device, b.b, nullptr); b.b = VK_NULL_HANDLE; return false; }
    if (!c_.check(vkBindBufferMemory(c_.device, b.b, b.m, 0), what)) { dropBuffer(b); return false; }
    if (host && !c_.check(vkMapMemory(c_.device, b.m, 0, VK_WHOLE_SIZE, 0, &b.map), what)) { b.map = nullptr; dropBuffer(b); return false; }   // kept mapped
    b.size = size;
    return true;
}

bool VkDraw2D::makeImage(int w, int h, VkFormat f, VkSampleCountFlagBits s, VkImageUsageFlags usage, int mips, Img& im, const char* what) {
    im = Img();
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = f;
    ii.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    ii.mipLevels = static_cast<uint32_t>(mips);
    ii.arrayLayers = 1;
    ii.samples = s;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!c_.check(vkCreateImage(c_.device, &ii, nullptr, &im.img), what)) { im.img = VK_NULL_HANDLE; return false; }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(c_.device, im.img, &req);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++)
        if ((req.memoryTypeBits & (1u << i)) && (c_.mem.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) type = i;
    for (uint32_t i = 0; i < c_.mem.memoryTypeCount && type == UINT32_MAX; i++) if (req.memoryTypeBits & (1u << i)) type = i;
    VkMemoryAllocateInfo mi{};
    mi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mi.allocationSize = req.size;
    mi.memoryTypeIndex = type;
    if (type == UINT32_MAX || !c_.check(vkAllocateMemory(c_.device, &mi, nullptr, &im.m), what)) { im.m = VK_NULL_HANDLE; vkDestroyImage(c_.device, im.img, nullptr); im.img = VK_NULL_HANDLE; return false; }
    if (!c_.check(vkBindImageMemory(c_.device, im.img, im.m, 0), what)) { dropImage(im); return false; }
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = im.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = f;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = static_cast<uint32_t>(mips);
    vi.subresourceRange.layerCount = 1;
    if (!c_.check(vkCreateImageView(c_.device, &vi, nullptr, &im.view), what)) { im.view = VK_NULL_HANDLE; dropImage(im); return false; }
    im.w = w; im.h = h;
    return true;
}

// a replaced buffer, image or framebuffer may still be read by a frame on the card: destroyed once the frames up to this one are done
void VkDraw2D::dropBuffer(Buf& b) {
    if (b.b == VK_NULL_HANDLE && b.m == VK_NULL_HANDLE) return;
    const VkDevice dev = c_.device;
    const VkBuffer bb = b.b;
    const VkDeviceMemory mm = b.m;
    garbage_.push_back({frameNo_, [dev, bb, mm] { if (bb) vkDestroyBuffer(dev, bb, nullptr); if (mm) vkFreeMemory(dev, mm, nullptr); }});
    b = Buf();
}
void VkDraw2D::dropImage(Img& i) {
    if (i.img == VK_NULL_HANDLE && i.m == VK_NULL_HANDLE) return;
    const VkDevice dev = c_.device;
    const VkImage im = i.img;
    const VkImageView v = i.view;
    const VkDeviceMemory mm = i.m;
    garbage_.push_back({frameNo_, [dev, im, v, mm] { if (v) vkDestroyImageView(dev, v, nullptr); if (im) vkDestroyImage(dev, im, nullptr); if (mm) vkFreeMemory(dev, mm, nullptr); }});
    i = Img();
}
void VkDraw2D::collect() {
    std::size_t k = 0;
    for (std::size_t i = 0; i < garbage_.size(); i++) {
        if (garbage_[i].first <= doneUpTo_) garbage_[i].second();
        else garbage_[k++] = std::move(garbage_[i]);
    }
    garbage_.resize(k);
}

VkPipeline VkDraw2D::makePipeline(int prog, Blend b, Pass p, VkShaderModule vs, VkShaderModule fs) {
    VkPipelineShaderStageCreateInfo st[2]{};
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = vs; st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = fs; st[1].pName = "main";
    std::vector<VkVertexInputBindingDescription> bind;
    std::vector<VkVertexInputAttributeDescription> attr;
    const auto at = [&](uint32_t loc, uint32_t bn, VkFormat f, uint32_t off) { attr.push_back({loc, bn, f, off}); };
    const VkFormat f2 = VK_FORMAT_R32G32_SFLOAT, f3 = VK_FORMAT_R32G32B32_SFLOAT, f4 = VK_FORMAT_R32G32B32A32_SFLOAT;
    switch (prog) {
    case kTri: case kPlanet: case kRings:             // Renderer2D::Vertex: x, y, u, v, r, g, b, a, mode, p1, p2, p3
        bind.push_back({0, 48, VK_VERTEX_INPUT_RATE_VERTEX});
        at(0, 0, f2, 0); at(1, 0, f2, 8); at(2, 0, f4, 16); at(3, 0, f4, 32);
        break;
    case kSquare:                                     // the corners (per vertex) and the centres (per instance)
        bind.push_back({0, 8, VK_VERTEX_INPUT_RATE_VERTEX}); bind.push_back({1, 8, VK_VERTEX_INPUT_RATE_INSTANCE});
        at(0, 0, f2, 0); at(1, 1, f2, 0);
        break;
    case kBlob:                                       // Renderer2D::Blob: x y r kind, fx fy f0 s1, c0, c1, c2 (80 bytes)
        bind.push_back({0, 8, VK_VERTEX_INPUT_RATE_VERTEX}); bind.push_back({1, 80, VK_VERTEX_INPUT_RATE_INSTANCE});
        at(0, 0, f2, 0); for (uint32_t i = 0; i < 5; i++) at(1 + i, 1, f4, 16 * i);
        break;
    case kCloud:                                      // 0.61.0: the corners; the dots' places (3 floats) and groups (a byte as a float: OpenGL's GL_UNSIGNED_BYTE, not normalised)
        bind.push_back({0, 8, VK_VERTEX_INPUT_RATE_VERTEX}); bind.push_back({1, 12, VK_VERTEX_INPUT_RATE_INSTANCE}); bind.push_back({2, 1, VK_VERTEX_INPUT_RATE_INSTANCE});
        at(0, 0, f2, 0); at(1, 1, f3, 0); at(2, 2, VK_FORMAT_R8_USCALED, 0);
        break;
    case kMolDot:                                     // 0.61.0: the corners; the dots made by kMolDotsCS (X, Y, r, wt)
        bind.push_back({0, 8, VK_VERTEX_INPUT_RATE_VERTEX}); bind.push_back({1, 16, VK_VERTEX_INPUT_RATE_INSTANCE});
        at(0, 0, f2, 0); at(1, 1, f4, 0);
        break;
    case kLine: case kSurf:                           // 0.68.0: no vertex input (the vertex shader reads storage buffers by gl_VertexIndex)
        break;
    case kMesh:                                       // render/mesh.h MeshVertex: p0, p1, rgba, corner, height (48 bytes)
        bind.push_back({0, 48, VK_VERTEX_INPUT_RATE_VERTEX});
        at(0, 0, f3, 0); at(1, 0, f3, 12); at(2, 0, f4, 24); at(3, 0, f2, 40);
        break;
    default:                                          // the composite and grey passes: the corners
        bind.push_back({0, 8, VK_VERTEX_INPUT_RATE_VERTEX});
        at(0, 0, f2, 0);
        break;
    }
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bind.size());
    vi.pVertexBindingDescriptions = bind.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attr.size());
    vi.pVertexAttributeDescriptions = attr.data();
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
    rs.cullMode = VK_CULL_MODE_NONE;                 // (OpenGL: GL_CULL_FACE off)
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = p == kPassMs ? static_cast<VkSampleCountFlagBits>(samples_) : VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_ADD;
    switch (b) {                                      // OpenGL's glBlendFunc(Separate) of Renderer2D::end, factor for factor
    case kNormal: ba.blendEnable = VK_TRUE; ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                  ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
    case kAdditive: ba.blendEnable = VK_TRUE; ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE; break;
    case kPremul: ba.blendEnable = VK_TRUE; ba.srcColorBlendFactor = ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                  ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
    case kOneOne: ba.blendEnable = VK_TRUE; ba.srcColorBlendFactor = ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                  ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE; break;
    default: ba.blendEnable = VK_FALSE; break;
    }
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
    gi.layout = prog == kLine || prog == kSurf ? linesLayout_ : layout_;
    const VkFormat fmt = p == kPassF ? floatFormat_ : kRgba8;           // 0.63.0: dynamic rendering: the pass's format in place of a render pass
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &fmt;
    gi.pNext = &ri;
    VkPipeline pipe = VK_NULL_HANDLE;
    app::FrameCostSpan made(app::FrameCostKind::Pipeline);   // (0.63.1: the benchmark's worst frames)
    if (!c_.check(vkCreateGraphicsPipelines(c_.device, VK_NULL_HANDLE, 1, &gi, nullptr, &pipe), "vkCreateGraphicsPipelines (renderer2d)")) return VK_NULL_HANDLE;
    return pipe;
}

bool VkDraw2D::init(const VkDevCtx& c, VkFormat windowFormat, int samples, std::string& why) {
    c_ = c;
    // ---- the shaders: OpenGL's text as SPIR-V; the stages' inputs and outputs must meet at the same locations, the blocks and the texture where expected ----
    const Code code[] = {{k_r2d_tri_vert, sizeof k_r2d_tri_vert}, {k_r2d_tri_frag, sizeof k_r2d_tri_frag}, {k_r2d_planet_frag, sizeof k_r2d_planet_frag},
                         {k_r2d_sq_vert, sizeof k_r2d_sq_vert}, {k_r2d_sq_frag, sizeof k_r2d_sq_frag}, {k_r2d_blob_vert, sizeof k_r2d_blob_vert},
                         {k_r2d_blob_frag, sizeof k_r2d_blob_frag}, {k_r2d_chart_vert, sizeof k_r2d_chart_vert}, {k_r2d_cmp_vert, sizeof k_r2d_cmp_vert},
                         {k_r2d_cmp_frag, sizeof k_r2d_cmp_frag}, {k_r2d_gray_frag, sizeof k_r2d_gray_frag}};
    const char* codeName[] = {"tri.vert", "tri.frag", "planet.frag", "square.vert", "square.frag", "blob.vert", "blob.frag", "chart.vert", "composite.vert", "composite.frag", "grey.frag"};
    constexpr int nCode = 11;
    // program: its vertex and fragment shader (indices into code[])
    const int pair[kCloud][2] = {{0, 1}, {0, 2}, {3, 4}, {5, 6}, {7, 1}, {8, 9}, {8, 10}};   // (kCloud, kMolDot: addProgram)
    Shader sh[nCode];
    VkShaderModule mod[nCode] = {};
    spirvBytes_ = 0;
    bool ok = true;
    for (int i = 0; i < nCode && ok; i++) {
        std::string w;
        if (!reflectSpirv(code[i].w, code[i].bytes / 4, sh[i], w)) { ok = false; why = std::string(codeName[i]) + ": " + w; break; }
        const bool vert = std::string(codeName[i]).find(".vert") != std::string::npos;
        if (!sh[i].block.m.empty() && sh[i].blockBinding != (vert ? 2 : 1)) { ok = false; why = std::string(codeName[i]) + ": its uniform block is not at the binding expected"; break; }
        if (sh[i].samplerBinding >= 0 && (vert || sh[i].samplerBinding != 0)) { ok = false; why = std::string(codeName[i]) + ": its texture is not at binding 0"; break; }
        VkShaderModuleCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        si.codeSize = code[i].bytes;
        si.pCode = code[i].w;
        if (!c_.check(vkCreateShaderModule(c_.device, &si, nullptr, &mod[i]), "vkCreateShaderModule (renderer2d)")) { mod[i] = VK_NULL_HANDLE; ok = false; why = std::string(codeName[i]) + ": vkCreateShaderModule failed"; }
        spirvBytes_ += code[i].bytes;
    }
    for (int p = 0; p < kCloud && ok; p++) {
        const Shader& v = sh[pair[p][0]];
        const Shader& f = sh[pair[p][1]];
        for (const auto& [name, loc] : f.in) {
            const auto it = v.out.find(name);
            if (it == v.out.end() || it->second != loc) { ok = false; why = std::string(codeName[pair[p][1]]) + ": its input " + name + " does not meet " + codeName[pair[p][0]] + " at the same location"; }
        }
        prog_[p].blk[0] = v.block;
        prog_[p].blk[1] = f.block;
    }
    // ---- the formats and the samples, as queried ----
    samples_ = 1;
    if (ok && samples >= 4) {
        VkImageFormatProperties ip{};
        const VkResult r = vkGetPhysicalDeviceImageFormatProperties(c_.phys, kRgba8, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &ip);
        if (r == VK_SUCCESS && (ip.sampleCounts & VK_SAMPLE_COUNT_4_BIT) && (c_.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT)) samples_ = 4;
        samplesText_ = samples_ == 4 ? "4 samples (multisampling, as OpenGL's window)" : "1 sample: this card offers no 4x multisampling for RGBA8 pictures";
    } else samplesText_ = "1 sample (multisampling off: PL_NO_MSAA)";
    if (ok) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(c_.phys, kRgba8, &fp);
        const VkFormatFeatureFlags need8 = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((fp.optimalTilingFeatures & need8) != need8) { ok = false; why = "R8G8B8A8_UNORM cannot be drawn into, blended and sampled here"; }
        const VkFormatFeatureFlags needF = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        floatFormat_ = VK_FORMAT_UNDEFINED;
        for (const VkFormat f : {VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}) {
            vkGetPhysicalDeviceFormatProperties(c_.phys, f, &fp);
            if (floatFormat_ == VK_FORMAT_UNDEFINED && (fp.optimalTilingFeatures & needF) == needF) floatFormat_ = f;
        }
        if (floatFormat_ == VK_FORMAT_UNDEFINED) { ok = false; why = "no float format can be drawn into with blending here (the electron cloud's layer)"; }
        floatText_ = floatFormat_ == VK_FORMAT_R32G32B32A32_SFLOAT ? "R32G32B32A32_SFLOAT (as OpenGL's GL_RGBA32F)" : "R16G16B16A16_SFLOAT (32-bit float cannot blend on this card)";
    }
    // (0.63.0: no render passes: dynamic rendering, beginPass)
    // ---- the layouts: binding 2 the vertex stage's block, 1 the fragment stage's, 0 its texture (the white one where it has none) ----
    if (ok) {
        VkDescriptorSetLayoutBinding b[3]{};
        b[0] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        b[1] = {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        b[2] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = 3;
        li.pBindings = b;
        ok = c_.check(vkCreateDescriptorSetLayout(c_.device, &li, nullptr, &setLayout_), "vkCreateDescriptorSetLayout (renderer2d)");
        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &setLayout_;
        if (ok) ok = c_.check(vkCreatePipelineLayout(c_.device, &pli, nullptr, &layout_), "vkCreatePipelineLayout (renderer2d)");
        if (!ok) why = "the pipeline layouts could not be made";
    }
    // ---- the pipelines: each program with the blends and passes it is drawn with ----
    if (ok) {
        const Pass all[] = {kPassMs, kPass8, kPassF};
        const auto want = [&](int p, Blend b) {
            for (const Pass ps : all) {
                if (ps == kPassMs && samples_ == 1) continue;
                if (!(prog_[p].pipe[ps][b] = makePipeline(p, b, ps, mod[pair[p][0]], mod[pair[p][1]]))) return false;
            }
            return true;
        };
        ok = want(kTri, kNormal) && want(kPlanet, kNormal) && want(kSquare, kNormal) && want(kBlob, kNormal) && want(kBlob, kAdditive) && want(kMesh, kNormal) &&
             want(kComposite, kPremul) && want(kComposite, kOneOne) && want(kGray, kNoBlend) && want(kComposite, kNoBlend);   // (0.62.0: the bent picture laid over)
        if (!ok) why = "a pipeline could not be made";
    }
    for (VkShaderModule m : mod) if (m) vkDestroyShaderModule(c_.device, m, nullptr);
    // ---- 0.69.0: the copy into the picture area is a blit (copyTo; present.frag is gone): the picture's format must be a blit's source and the
    // window's its destination (both mandatory for these formats in the Vulkan spec; queried all the same) ----
    if (ok) {
        VkFormatProperties src{}, dst{};
        vkGetPhysicalDeviceFormatProperties(c_.phys, kRgba8, &src);
        vkGetPhysicalDeviceFormatProperties(c_.phys, windowFormat, &dst);
        if (!(src.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(dst.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            ok = false;
            why = "the graphics card cannot copy the picture into the window (no blit for these formats)";
        }
    }
    // ---- the samplers: the soft layer and the grey pass GL_LINEAR, the cloud's float layer GL_NEAREST (both clamped), the maps as Renderer2D::init ----
    if (ok) {
        VkSamplerCreateInfo s{};
        s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        s.magFilter = s.minFilter = VK_FILTER_LINEAR;
        s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.maxLod = 0.0f;
        ok = c_.check(vkCreateSampler(c_.device, &s, nullptr, &linearClamp_), "vkCreateSampler");
        s.magFilter = s.minFilter = VK_FILTER_NEAREST;
        if (ok) ok = c_.check(vkCreateSampler(c_.device, &s, nullptr, &nearestClamp_), "vkCreateSampler");
        s.magFilter = s.minFilter = VK_FILTER_LINEAR;                       // GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR; longitude GL_REPEAT, latitude GL_CLAMP_TO_EDGE
        s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        s.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        s.maxLod = VK_LOD_CLAMP_NONE;
        if (ok) ok = c_.check(vkCreateSampler(c_.device, &s, nullptr, &mapSampler_), "vkCreateSampler");
        if (!ok) why = "a sampler could not be made";
    }
    // ---- the corners of the instanced squares (as Renderer2D::init's) and the white texture ----
    if (ok) {
        const float corners[12] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
        ok = makeBuffer(sizeof corners, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, corners_, "the corners' buffer");
        if (ok) std::memcpy(corners_.map, corners, sizeof corners);
        const std::vector<std::uint8_t> one = {255, 255, 255, 255};
        if (ok) ok = upload(0, 1, 1, one, false, why);
        if (ok) { white_ = maps_[0]; maps_[0] = Img(); }
        if (!ok && why.empty()) why = "the white texture could not be made";
    }
    ok_ = ok;
    return ok;
}

bool VkDraw2D::upload(int map, int w, int h, const std::vector<std::uint8_t>& rgba, bool srgb, std::string& why) {
    if (map < 0 || map >= 32 || w <= 0 || h <= 0 || rgba.size() < std::size_t(w) * std::size_t(h) * 4u) { why = "a map of the wrong size"; return false; }
    const VkFormat f = srgb ? VK_FORMAT_R8G8B8A8_SRGB : kRgba8;              // OpenGL: GL_SRGB8_ALPHA8 (the planets: decoded to linear light), GL_RGBA8 (the sky)
    VkFormatProperties fp{};
    vkGetPhysicalDeviceFormatProperties(c_.phys, f, &fp);
    const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    if ((fp.optimalTilingFeatures & need) != need) { why = std::string(srgb ? "R8G8B8A8_SRGB" : "R8G8B8A8_UNORM") + " cannot be filtered and blitted here (the maps' mip levels)"; return false; }
    int mips = 1;
    while ((std::max(w, h) >> mips) > 0) mips++;                             // as glGenerateMipmap: down to 1 x 1
    Buf stage;
    const VkDeviceSize bytes = VkDeviceSize(w) * VkDeviceSize(h) * 4u;
    if (!makeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, stage, "the map's upload buffer")) { why = "the map's upload buffer"; return false; }
    std::memcpy(stage.map, rgba.data(), static_cast<std::size_t>(bytes));
    Img im;
    if (!makeImage(w, h, f, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mips, im, "the map's image")) {
        vkUnmapMemory(c_.device, stage.m); vkDestroyBuffer(c_.device, stage.b, nullptr); vkFreeMemory(c_.device, stage.m, nullptr);
        why = "the map's image"; return false;
    }
    const auto barrier = [](VkCommandBuffer cmd, VkImage img, int level, int count, VkImageLayout from, VkImageLayout to, VkAccessFlags2 sa, VkAccessFlags2 da, VkPipelineStageFlags2 ss, VkPipelineStageFlags2 ds) {
        vkImageBarrier2(cmd, img, from, to, ss, sa, ds, da, static_cast<uint32_t>(level), static_cast<uint32_t>(count));   // (0.63.0: synchronization2)
    };
    const bool ok = c_.once([&](VkCommandBuffer cmd) {
        barrier(cmd, im.img, 0, mips, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
        VkBufferImageCopy cp{};
        cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.imageSubresource.layerCount = 1;
        cp.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyBufferToImage(cmd, stage.b, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
        int lw = w, lh = h;
        for (int i = 1; i < mips; i++) {                                     // each level from the one above, filtered (the sRGB maps in linear light, as OpenGL's)
            barrier(cmd, im.img, i - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
            const int nw = std::max(1, lw / 2), nh = std::max(1, lh / 2);
            VkImageBlit bl{};
            bl.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i - 1), 0, 1};
            bl.srcOffsets[1] = {lw, lh, 1};
            bl.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i), 0, 1};
            bl.dstOffsets[1] = {nw, nh, 1};
            vkCmdBlitImage(cmd, im.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bl, VK_FILTER_LINEAR);
            barrier(cmd, im.img, i - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
            lw = nw; lh = nh;
        }
        barrier(cmd, im.img, mips - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    });
    vkUnmapMemory(c_.device, stage.m);
    vkDestroyBuffer(c_.device, stage.b, nullptr);
    vkFreeMemory(c_.device, stage.m, nullptr);
    if (!ok) { vkDestroyImageView(c_.device, im.view, nullptr); vkDestroyImage(c_.device, im.img, nullptr); vkFreeMemory(c_.device, im.m, nullptr); why = "the map's upload failed"; return false; }
    if (maps_[map].img) dropImage(maps_[map]);
    im.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    maps_[map] = im;
    return true;
}

void VkDraw2D::destroy() {
    if (!c_.device) return;
    vkDeviceWaitIdle(c_.device);                                             // (the end of the app, or a failed start)
    doneUpTo_ = ~std::uint64_t(0);
    for (Targets* t : {&screen_, &shot_}) { dropImage(t->ms); dropImage(t->pic); dropImage(t->gray); *t = Targets(); }
    for (Layer* l : {&soft_, &hdr_}) { dropImage(l->img); *l = Layer(); }
    for (Img& m : maps_) dropImage(m);
    dropImage(white_);
    for (Mesh& m : meshes_) dropBuffer(m.buf);
    meshes_.clear();
    for (Slot& s : slots_) { dropBuffer(s.buf); if (s.pool) vkDestroyDescriptorPool(c_.device, s.pool, nullptr); if (s.linesPool) vkDestroyDescriptorPool(c_.device, s.linesPool, nullptr); }
    slots_.clear();
    dropBuffer(corners_);
    collect();
    for (Program& p : prog_) for (auto& row : p.pipe) for (VkPipeline& pl : row) if (pl) { vkDestroyPipeline(c_.device, pl, nullptr); pl = VK_NULL_HANDLE; }
    for (VkSampler s : {linearClamp_, nearestClamp_, mapSampler_}) if (s) vkDestroySampler(c_.device, s, nullptr);
    if (layout_) vkDestroyPipelineLayout(c_.device, layout_, nullptr);
    if (setLayout_) vkDestroyDescriptorSetLayout(c_.device, setLayout_, nullptr);
    if (linesLayout_) vkDestroyPipelineLayout(c_.device, linesLayout_, nullptr);
    if (linesSetLayout_) vkDestroyDescriptorSetLayout(c_.device, linesSetLayout_, nullptr);
    linesLayout_ = VK_NULL_HANDLE; linesSetLayout_ = VK_NULL_HANDLE;
    linearClamp_ = nearestClamp_ = mapSampler_ = VK_NULL_HANDLE; layout_ = VK_NULL_HANDLE; setLayout_ = VK_NULL_HANDLE;
    c_ = VkDevCtx();
    ok_ = false;
}

void VkDraw2D::beginFrame(int slot, int slots, VkCommandBuffer cmd) {
    if (static_cast<int>(slots_.size()) < std::max(slots, slot + 1)) slots_.resize(static_cast<std::size_t>(std::max(slots, slot + 1)));
    slot_ = slot;
    cmd_ = cmd;
    doneUpTo_ = std::max(doneUpTo_, slots_[static_cast<std::size_t>(slot)].frame);   // this slot's fence was waited for: its frame and every earlier one are done
    slots_[static_cast<std::size_t>(slot)].frame = ++frameNo_;
    collect();
    drawn_ = false;
    inPass_ = inLayer_ = false;
}

bool VkDraw2D::targetsFor(Targets& t, int w, int h, int s) {
    if (t.w == w && t.h == h && t.s == s && t.pic.img) return true;
    dropImage(t.ms); dropImage(t.pic); dropImage(t.gray);
    t = Targets();
    const VkImageUsageFlags use = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;   // (0.69.0: the copy's source)
    bool ok = makeImage(w, h, kRgba8, VK_SAMPLE_COUNT_1_BIT, use, 1, t.pic, "the picture") &&
              makeImage(w, h, kRgba8, VK_SAMPLE_COUNT_1_BIT, use, 1, t.gray, "the grey picture");
    if (ok && s > 1) ok = makeImage(w, h, kRgba8, static_cast<VkSampleCountFlagBits>(s), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, t.ms, "the multisampled picture");
    if (!ok) { failed_ = true; return false; }
    t.w = w; t.h = h; t.s = s;
    return true;
}

bool VkDraw2D::layerFor(Layer& l, bool hdr, int w, int h) {
    if (l.w == w && l.h == h && l.img.img) return true;
    dropImage(l.img);
    l = Layer();
    if (!makeImage(w, h, hdr ? floatFormat_ : kRgba8, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1, l.img, hdr ? "the cloud's layer" : "the soft layer")) { failed_ = true; return false; }
    l.w = w; l.h = h;
    return true;
}

// 0.63.0 (Vulkan step 5): dynamic rendering and synchronization2 (as render/vk_blackhole.cpp), in place of 0.60.0's render passes and their dependencies.
// Before: the last reads of this image (a later pass or the window's copy, fragment shader) and its writes are done before it is drawn again; after: its
// writes are made visible to the passes that sample it. The multisampled image stays an attachment (a kept pass draws on); the others become sampled.
void VkDraw2D::beginPass(Pass p, Img& color, Img* resolve, int w, int h, bool clear, const float c[4]) {
    const VkPipelineStageFlags2 before = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    const VkAccessFlags2 rw = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    vkImageBarrier2(cmd_, color.img, clear ? VK_IMAGE_LAYOUT_UNDEFINED : color.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, before, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, rw);
    color.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (resolve) {                                                            // (wholly written by the resolve)
        vkImageBarrier2(cmd_, resolve->img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, before, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, rw);
        resolve->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    VkRenderingAttachmentInfo at{};
    at.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    at.imageView = color.view;
    at.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    for (int i = 0; i < 4; i++) at.clearValue.color.float32[i] = c ? c[i] : 0.0f;
    if (resolve) {
        at.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;                         // (the render pass's resolve attachment: the average of the samples)
        at.resolveImageView = resolve->view;
        at.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &at;
    vkCmdBeginRendering(cmd_, &ri);
    const VkViewport vp{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f};   // OpenGL's rows: NDC y = -1 is the image's first row
    const VkRect2D sc{{0, 0}, {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}};
    vkCmdSetViewport(cmd_, 0, 1, &vp);
    vkCmdSetScissor(cmd_, 0, 1, &sc);
    inPass_ = true;
    pass_ = p;
    passW_ = w; passH_ = h;
    passColor_ = &color; passResolve_ = resolve;
    vbBound_ = ~VkDeviceSize(0);
}

void VkDraw2D::endPass() {
    if (!inPass_) return;
    vkCmdEndRendering(cmd_);
    inPass_ = false;
    const VkPipelineStageFlags2 after = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkAccessFlags2 reads = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    Img* sampled = passResolve_ ? passResolve_ : passColor_;
    if (sampled) {
        vkImageBarrier2(cmd_, sampled->img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, after, reads);
        sampled->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    passColor_ = passResolve_ = nullptr;
}

bool VkDraw2D::beginPicture(int w, int h, const float clear[4], bool oneSample, std::size_t bytes, int sets, bool forShot) {
    drawn_ = false;
    if (!ok_ || failed_ || !cmd_ || w <= 0 || h <= 0) return false;
    oneSample_ = oneSample || samples_ == 1;
    Targets& t = forShot ? shot_ : screen_;
    if (!forShot) screenKept_ = false;
    if (!targetsFor(t, w, h, oneSample_ ? 1 : samples_)) return false;
    cur_ = &t;
    Slot& s = slots_[static_cast<std::size_t>(slot_)];
    const VkDeviceSize align = std::max<VkDeviceSize>(16, c_.limits.minUniformBufferOffsetAlignment);
    std::size_t blocks = 0;
    for (const Program& p : prog_) blocks = std::max<std::size_t>(blocks, p.blk[0].size + p.blk[1].size);
    const VkDeviceSize need = VkDeviceSize(bytes) + VkDeviceSize(sets + 4) * (VkDeviceSize(blocks) + 2 * align + 64) + 4096;
    if (s.buf.size < need) {                                                 // this slot's frame is done (its fence): its old buffer can go at once
        if (s.buf.b) { vkDestroyBuffer(c_.device, s.buf.b, nullptr); vkFreeMemory(c_.device, s.buf.m, nullptr); s.buf = Buf(); }
        if (!makeBuffer(std::max<VkDeviceSize>(need + need / 2, 1u << 20), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, s.buf, "the frame's buffer")) { failed_ = true; return false; }
    }
    s.used = 0;
    const std::uint32_t wantSets = static_cast<std::uint32_t>(sets + 8);
    if (!s.pool || s.poolSets < wantSets) {
        if (s.pool) vkDestroyDescriptorPool(c_.device, s.pool, nullptr);
        s.pool = VK_NULL_HANDLE;
        s.poolSets = std::max<std::uint32_t>(wantSets + wantSets / 2, 256);
        VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * s.poolSets}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s.poolSets}};
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets = s.poolSets;
        pi.poolSizeCount = 2;
        pi.pPoolSizes = ps;
        if (!c_.check(vkCreateDescriptorPool(c_.device, &pi, nullptr, &s.pool), "vkCreateDescriptorPool (renderer2d)")) { s.pool = VK_NULL_HANDLE; s.poolSets = 0; failed_ = true; return false; }
    } else if (!c_.check(vkResetDescriptorPool(c_.device, s.pool, 0), "vkResetDescriptorPool")) { failed_ = true; return false; }
    if (s.linesPool && !c_.check(vkResetDescriptorPool(c_.device, s.linesPool, 0), "vkResetDescriptorPool (lines)")) { failed_ = true; return false; }
    beginPass(oneSample_ ? kPass8 : kPassMs, oneSample_ ? t.pic : t.ms, oneSample_ ? nullptr : &t.pic, w, h, true, clear);
    return true;
}

VkDeviceSize VkDraw2D::put(const void* data, std::size_t bytes) {
    Slot& s = slots_[static_cast<std::size_t>(slot_)];
    const VkDeviceSize at = (s.used + 15u) / 16u * 16u;
    if (at + bytes > s.buf.size) { if (!failed_) c_.check(VK_ERROR_OUT_OF_DEVICE_MEMORY, "the frame's buffer is too small (renderer2d)"); failed_ = true; return 0; }
    if (bytes) std::memcpy(static_cast<std::uint8_t*>(s.buf.map) + at, data, bytes);
    s.used = at + bytes;
    return at;
}

void VkDraw2D::beginLayer(bool hdr, int w, int h) {
    if (!cur_ || failed_) return;
    endPass();
    Layer& l = hdr ? hdr_ : soft_;
    if (!layerFor(l, hdr, w, h)) return;
    const float zero[4] = {0, 0, 0, 0};
    beginPass(hdr ? kPassF : kPass8, l.img, nullptr, w, h, true, zero);
    inLayer_ = true;
    layerHdr_ = hdr;
}

void VkDraw2D::endLayer() {
    if (!cur_ || failed_ || !inLayer_) return;
    endPass();
    inLayer_ = false;
    beginPass(oneSample_ ? kPass8 : kPassMs, oneSample_ ? cur_->pic : cur_->ms, oneSample_ ? nullptr : &cur_->pic, cur_->w, cur_->h, false, nullptr);
}

void VkDraw2D::composite(bool hdr, Blend b) {
    if (!cur_ || failed_ || (hdr ? hdr_ : soft_).img.img == VK_NULL_HANDLE) return;
    layerHdr_ = hdr;
    draw(kComposite, b, 0, 0, 6, 0, 0, 0, -1);
}

void VkDraw2D::setF(Prog p, const char* name, const float* v, int comps, int count) {
    for (Block& b : prog_[p].blk) {
        const auto it = b.m.find(name);
        if (it == b.m.end()) continue;
        const Member& m = it->second;
        for (int i = 0; i < count; i++) {
            const std::size_t at = m.offset + static_cast<std::size_t>(i) * m.arrayStride;
            if (at + static_cast<std::size_t>(comps) * 4u > b.data.size()) break;
            std::memcpy(b.data.data() + at, v + i * comps, static_cast<std::size_t>(comps) * 4u);
            if (m.arrayStride == 0) break;
        }
    }
}
void VkDraw2D::setI(Prog p, const char* name, const int* v, int comps, int count) {
    static_assert(sizeof(int) == 4, "32-bit ints");
    setF(p, name, reinterpret_cast<const float*>(v), comps, count);          // (the bits, as std140 holds them)
}
void VkDraw2D::setMat3T(Prog p, const char* name, const float K[9]) {
    for (Block& b : prog_[p].blk) {
        const auto it = b.m.find(name);
        if (it == b.m.end()) continue;
        const Member& m = it->second;
        const std::uint32_t stride = m.matrixStride ? m.matrixStride : 16u;
        for (int c = 0; c < 3; c++) {                                        // column c of the matrix whose rows are K's rows
            const float col[3] = {K[c], K[3 + c], K[6 + c]};
            const std::size_t at = m.offset + static_cast<std::size_t>(c) * stride;
            if (at + 12u <= b.data.size()) std::memcpy(b.data.data() + at, col, 12u);
        }
    }
}

VkDescriptorSet VkDraw2D::descriptors(const Program& pr, VkImageView tex, VkSampler smp) {
    Slot& s = slots_[static_cast<std::size_t>(slot_)];
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = s.pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &setLayout_;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (!c_.check(vkAllocateDescriptorSets(c_.device, &ai, &set), "vkAllocateDescriptorSets (renderer2d)")) { failed_ = true; return VK_NULL_HANDLE; }
    VkDescriptorBufferInfo bi[2]{};
    const VkDeviceSize align = std::max<VkDeviceSize>(16, c_.limits.minUniformBufferOffsetAlignment);
    for (int k = 0; k < 2; k++) {
        const Block& b = pr.blk[k];
        static const std::uint8_t zeros[16] = {};
        s.used = (s.used + align - 1) / align * align;
        const VkDeviceSize at = s.used;
        const std::size_t n = b.size ? b.size : 16u;
        if (at + n > s.buf.size) { if (!failed_) c_.check(VK_ERROR_OUT_OF_DEVICE_MEMORY, "the frame's buffer is too small (uniforms)"); failed_ = true; return VK_NULL_HANDLE; }
        std::memcpy(static_cast<std::uint8_t*>(s.buf.map) + at, b.size ? b.data.data() : zeros, n);
        s.used = at + n;
        bi[k].buffer = s.buf.b;
        bi[k].offset = at;
        bi[k].range = n;
    }
    VkDescriptorImageInfo ii{};
    ii.sampler = smp;
    ii.imageView = tex;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wr[3]{};
    for (int k = 0; k < 3; k++) {
        wr[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr[k].dstSet = set;
        wr[k].dstBinding = k == 0 ? 2u : k == 1 ? 1u : 0u;                   // the vertex block, the fragment block, the texture
        wr[k].descriptorCount = 1;
        wr[k].descriptorType = k < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        if (k < 2) wr[k].pBufferInfo = &bi[k]; else wr[k].pImageInfo = &ii;
    }
    vkUpdateDescriptorSets(c_.device, 3, wr, 0, nullptr);
    return set;
}

void VkDraw2D::draw(Prog p, Blend b, VkDeviceSize vbOff, int first, int count, VkDeviceSize ibOff, int instances, int firstInstance, int tex) {
    if (!inPass_ || failed_ || count <= 0) return;
    const Program& pr = prog_[p];
    const VkPipeline pl = pr.pipe[pass_][b];
    if (!pl) { if (!failed_) c_.check(VK_ERROR_FEATURE_NOT_PRESENT, "renderer2d: no pipeline for this program, blend and pass"); failed_ = true; return; }
    VkImageView view = white_.view;
    VkSampler smp = nearestClamp_;
    if (tex > 0 && tex < 32 && maps_[tex].view) { view = maps_[tex].view; smp = mapSampler_; }
    else if (tex == -1) { view = (layerHdr_ ? hdr_ : soft_).img.view; smp = layerHdr_ ? nearestClamp_ : linearClamp_; }
    else if (tex == -2 && cur_) { view = cur_->pic.view; smp = linearClamp_; }
    else if ((tex == -3 || tex == -4) && ext_[-3 - tex]) { view = ext_[-3 - tex]; smp = nearestClamp_; }   // 0.62.0: the Black Hole Lab's
    const VkDescriptorSet set = descriptors(pr, view, smp);
    if (!set) return;
    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
    const Slot& s = slots_[static_cast<std::size_t>(slot_)];
    if (p == kTri || p == kPlanet || p == kRings) {
        vkCmdBindVertexBuffers(cmd_, 0, 1, &s.buf.b, &vbOff);
        vkCmdDraw(cmd_, static_cast<uint32_t>(count), 1, static_cast<uint32_t>(first), 0);
    } else if (p == kSquare || p == kBlob) {
        const VkBuffer bufs[2] = {corners_.b, s.buf.b};
        const VkDeviceSize offs[2] = {0, ibOff};
        vkCmdBindVertexBuffers(cmd_, 0, 2, bufs, offs);
        vkCmdDraw(cmd_, 6, static_cast<uint32_t>(instances), 0, static_cast<uint32_t>(firstInstance));
    } else {
        const VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd_, 0, 1, &corners_.b, &off);
        vkCmdDraw(cmd_, 6, 1, 0, 0);
    }
}

// 0.61.0: kCloud or kMolDot with the blob fragment shader (kBlobFS), every pass, the normal and the added blend
bool VkDraw2D::addProgram(Prog p, std::string& why) {
    if (!ok_ || (p != kCloud && p != kMolDot && p != kRings && p != kLine && p != kSurf)) { why = "renderer2d on Vulkan has not started"; return false; }
    if (hasProgram(p)) return true;
    const Code vc = p == kCloud ? Code{k_r2d_cloud_vert, sizeof k_r2d_cloud_vert} : p == kRings ? Code{k_r2d_tri_vert, sizeof k_r2d_tri_vert}
                  : p == kLine ? Code{k_r2d_lines_vert, sizeof k_r2d_lines_vert} : p == kSurf ? Code{k_r2d_surface_vert, sizeof k_r2d_surface_vert} : Code{k_r2d_moldot_vert, sizeof k_r2d_moldot_vert};
    const Code fc = p == kRings ? Code{k_r2d_rings_frag, sizeof k_r2d_rings_frag} : p == kLine || p == kSurf ? Code{k_r2d_tri_frag, sizeof k_r2d_tri_frag} : Code{k_r2d_blob_frag, sizeof k_r2d_blob_frag};
    if ((p == kLine || p == kSurf) && !linesLayout_) {                                       // 0.68.0: set 0 as every program's, set 1 the four storage buffers of the vertex stage
        if (c_.limits.maxPerStageDescriptorStorageBuffers < 4) { why = "this card gives the vertex stage fewer than 4 storage buffers"; return false; }
        VkDescriptorSetLayoutBinding b[4]{};
        for (std::uint32_t i = 0; i < 4; i++) b[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = 4;
        li.pBindings = b;
        if (!c_.check(vkCreateDescriptorSetLayout(c_.device, &li, nullptr, &linesSetLayout_), "vkCreateDescriptorSetLayout (lines)")) { linesSetLayout_ = VK_NULL_HANDLE; why = "vkCreateDescriptorSetLayout failed"; return false; }
        const VkDescriptorSetLayout sets[2] = {setLayout_, linesSetLayout_};
        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 2;
        pli.pSetLayouts = sets;
        if (!c_.check(vkCreatePipelineLayout(c_.device, &pli, nullptr, &linesLayout_), "vkCreatePipelineLayout (lines)")) { linesLayout_ = VK_NULL_HANDLE; why = "vkCreatePipelineLayout failed"; return false; }
    }
    Shader vs, fs;
    if (!reflectSpirv(vc.w, vc.bytes / 4, vs, why) || !reflectSpirv(fc.w, fc.bytes / 4, fs, why)) return false;
    if (vs.blockBinding != 2) { why = "its vertex shader's uniform block is not at binding 2"; return false; }
    if (p == kRings) {                                                       // 0.64.0: its fragment block (the bands) at binding 1, within this card's uniform range
        if (fs.blockBinding != 1) { why = "the rings' fragment shader's uniform block is not at binding 1"; return false; }
        if (fs.block.size > c_.limits.maxUniformBufferRange) { why = "the rings' uniform block (" + std::to_string(fs.block.size) + " bytes) is larger than this card's uniform range"; return false; }
    }
    for (const auto& [name, loc] : fs.in) {
        const auto it = vs.out.find(name);
        if (it == vs.out.end() || it->second != loc) { why = "the blob fragment shader's input " + name + " does not meet the vertex shader at the same location"; return false; }
    }
    if (p == kCloud) {                                                       // the groups' format, as queried (OpenGL's GL_UNSIGNED_BYTE attribute)
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(c_.phys, VK_FORMAT_R8_USCALED, &fp);
        if (!(fp.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)) { why = "this card cannot read R8_USCALED vertex data (the dots' colour groups)"; return false; }
    }
    VkShaderModule mod[2] = {};
    bool ok = true;
    for (int i = 0; i < 2 && ok; i++) {
        VkShaderModuleCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        si.codeSize = i ? fc.bytes : vc.bytes;
        si.pCode = i ? fc.w : vc.w;
        if (!c_.check(vkCreateShaderModule(c_.device, &si, nullptr, &mod[i]), "vkCreateShaderModule (card draw)")) { mod[i] = VK_NULL_HANDLE; ok = false; why = "vkCreateShaderModule failed"; }
    }
    prog_[p].blk[0] = vs.block;
    prog_[p].blk[1] = fs.block;
    for (const Pass ps : {kPassMs, kPass8, kPassF}) {
        if (!ok || (ps == kPassMs && samples_ == 1)) continue;
        for (const Blend b : {kNormal, kAdditive})
            if (ok && ((p != kRings && p != kLine && p != kSurf) || b == kNormal) && !((p == kLine || p == kSurf) && ps == kPassF) && !(prog_[p].pipe[ps][b] = makePipeline(p, b, ps, mod[0], mod[1]))) { ok = false; why = "vkCreateGraphicsPipelines failed"; }
    }
    for (VkShaderModule m : mod) if (m) vkDestroyShaderModule(c_.device, m, nullptr);
    if (!ok) for (auto& row : prog_[p].pipe) for (VkPipeline& pl : row) if (pl) { vkDestroyPipeline(c_.device, pl, nullptr); pl = VK_NULL_HANDLE; }
    spirvBytes_ += ok ? vc.bytes : 0;
    return ok;
}

void VkDraw2D::drawInst(Prog p, Blend b, VkBuffer inst1, VkBuffer inst2, int instances) {
    if (!inPass_ || failed_ || instances <= 0 || !inst1 || (p == kCloud && !inst2)) return;
    const Program& pr = prog_[p];
    const VkPipeline pl = pr.pipe[pass_][b];
    if (!pl) { if (!failed_) c_.check(VK_ERROR_FEATURE_NOT_PRESENT, "renderer2d: no pipeline for this card program, blend and pass"); failed_ = true; return; }
    const VkDescriptorSet set = descriptors(pr, white_.view, nearestClamp_);
    if (!set) return;
    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
    const VkBuffer bufs[3] = {corners_.b, inst1, inst2};
    const VkDeviceSize offs[3] = {0, 0, 0};
    vkCmdBindVertexBuffers(cmd_, 0, p == kCloud ? 3u : 2u, bufs, offs);
    vkCmdDraw(cmd_, 6, static_cast<uint32_t>(instances), 0, 0);
}

// 0.68.0: the Orbit Lab's lines and the Density surface: set 0 as every program's (the vertex and fragment blocks, the white texture), set 1 the storage
// buffers; the lines' count from the card (kLinesCS step 1 wrote it, before this frame's picture), the surface's from the caller
void VkDraw2D::drawStorage(Prog p, const VkBuffer b0[4], VkBuffer indirect, std::uint32_t count) {
    if (!inPass_ || failed_ || (!indirect && count == 0) || !linesLayout_ || !b0[0]) return;
    VkBuffer bufs[4];
    for (int i = 0; i < 4; i++) bufs[i] = b0[i] ? b0[i] : b0[0];
    const Program& pr = prog_[p];
    const VkPipeline pl = pr.pipe[pass_][kNormal];
    if (!pl) { if (!failed_) c_.check(VK_ERROR_FEATURE_NOT_PRESENT, "renderer2d: no pipeline for this storage program in this pass"); failed_ = true; return; }
    Slot& s = slots_[static_cast<std::size_t>(slot_)];
    if (!s.linesPool) {
        const VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * 64};
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets = 64;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;
        if (!c_.check(vkCreateDescriptorPool(c_.device, &pi, nullptr, &s.linesPool), "vkCreateDescriptorPool (lines)")) { s.linesPool = VK_NULL_HANDLE; failed_ = true; return; }
    }
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = s.linesPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &linesSetLayout_;
    VkDescriptorSet st = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(c_.device, &ai, &st) != VK_SUCCESS) return;   // (more than 64 line draws in one picture: the rest not drawn; never in this app)
    VkDescriptorBufferInfo bi[4]{};
    VkWriteDescriptorSet wr[4]{};
    for (std::uint32_t i = 0; i < 4; i++) {
        bi[i] = {bufs[i], 0, VK_WHOLE_SIZE};
        wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr[i].dstSet = st;
        wr[i].dstBinding = i;
        wr[i].descriptorCount = 1;
        wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(c_.device, 4, wr, 0, nullptr);
    const VkDescriptorSet set = descriptors(pr, white_.view, nearestClamp_);
    if (!set) return;
    const VkDescriptorSet sets[2] = {set, st};
    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, linesLayout_, 0, 2, sets, 0, nullptr);
    if (indirect) vkCmdDrawIndirect(cmd_, indirect, 0, 1, 16);
    else vkCmdDraw(cmd_, count, 1, 0, 0);
}

int VkDraw2D::meshCreate(const void* v, std::size_t bytes) {
    if (!ok_ || bytes == 0) return -1;
    int id = -1;
    for (int i = 0; i < static_cast<int>(meshes_.size()); i++) if (!meshes_[static_cast<std::size_t>(i)].used) { id = i; break; }
    if (id < 0) { id = static_cast<int>(meshes_.size()); meshes_.emplace_back(); }
    Mesh& m = meshes_[static_cast<std::size_t>(id)];
    Buf stage;
    if (!makeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, stage, "the mesh's upload buffer")) return -1;
    std::memcpy(stage.map, v, bytes);
    bool ok = makeBuffer(bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, m.buf, "the mesh's buffer");
    if (ok) ok = c_.once([&](VkCommandBuffer cmd) {                          // once, as OpenGL's GL_STATIC_DRAW buffer (the mesh is then only drawn)
        const VkBufferCopy cp{0, 0, bytes};
        vkCmdCopyBuffer(cmd, stage.b, m.buf.b, 1, &cp);
        vkMemoryBarrier2(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);   // (0.63.0)
    });
    vkUnmapMemory(c_.device, stage.m);
    vkDestroyBuffer(c_.device, stage.b, nullptr);
    vkFreeMemory(c_.device, stage.m, nullptr);
    if (!ok) { dropBuffer(m.buf); return -1; }
    m.used = true;
    return id;
}

void VkDraw2D::meshDelete(int id) {
    if (id < 0 || id >= static_cast<int>(meshes_.size())) return;
    Mesh& m = meshes_[static_cast<std::size_t>(id)];
    dropBuffer(m.buf);                                                       // (a frame on the card may still draw it)
    m.used = false;
}

void VkDraw2D::drawMesh(int id, int first, int count) {
    if (!inPass_ || failed_ || id < 0 || id >= static_cast<int>(meshes_.size()) || !meshes_[static_cast<std::size_t>(id)].used || count <= 0) return;
    const Program& pr = prog_[kMesh];
    const VkPipeline pl = pr.pipe[pass_][kNormal];
    const VkDescriptorSet set = descriptors(pr, white_.view, nearestClamp_);
    if (!pl || !set) return;
    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
    const VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd_, 0, 1, &meshes_[static_cast<std::size_t>(id)].buf.b, &off);
    vkCmdDraw(cmd_, static_cast<uint32_t>(count), 1, static_cast<uint32_t>(first), 0);
}

void VkDraw2D::endPicture(bool gray) {
    if (!cur_) return;
    if (inLayer_) endLayer();                                                // (a layer left open: closed, as OpenGL's next segment would draw into it)
    endPass();
    usedGray_ = false;
    if (failed_) return;
    if (gray) {                                                              // OpenGL: the picture drawn into its own image, then copied over in grey (kGrayFS)
        beginPass(kPass8, cur_->gray, nullptr, cur_->w, cur_->h, true, nullptr);
        draw(kGray, kNoBlend, 0, 0, 6, 0, 0, 0, -2);
        endPass();
        usedGray_ = true;
    }
    drawn_ = !failed_;
    if (cur_ == &screen_) { screenKept_ = drawn_; screenGray_ = usedGray_; }
}

bool VkDraw2D::keepPicture(int w, int h, bool gray) {
    drawn_ = false;
    if (!ok_ || failed_ || !cmd_ || !screenKept_ || screen_.w != w || screen_.h != h || screenGray_ != gray || !screen_.pic.img) return false;
    Slot& s = slots_[static_cast<std::size_t>(slot_)];
    if (!s.pool) return false;
    if (!c_.check(vkResetDescriptorPool(c_.device, s.pool, 0), "vkResetDescriptorPool")) { failed_ = true; return false; }   // (present's set comes from it)
    s.used = 0;
    cur_ = &screen_;
    usedGray_ = screenGray_;
    drawn_ = true;
    return true;
}

// 0.69.0 (present.frag's place): the target cleared to clear[] and the picture copied into its area (x, y, w, h: pixels from the top left) by a blit,
// its rows turned over (the picture has OpenGL's rows: its first row at the bottom) and nearest, one texel to one pixel: the same pixels the shader copied
// (its target's row q took the picture's row h - 1 - q). The target goes from undefined to the colour attachment's layout, the picture back to sampled.
void VkDraw2D::copyTo(VkCommandBuffer cmd, VkImage dst, const VkClearColorValue& clear, int x, int y, int w, int h) {
    vkImageBarrier2(cmd, dst, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &all);
    const bool picture = drawn_ && cur_ && !failed_ && w > 0 && h > 0;
    Img* src = picture ? (usedGray_ ? &cur_->gray : &cur_->pic) : nullptr;
    if (src && src->img) {
        vkMemoryBarrier2(cmd, VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);   // (the clear, then the copy over it)
        vkImageBarrier2(cmd, src->img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        const int ww = std::min(w, cur_->w), hh = std::min(h, cur_->h);
        VkImageBlit b{};
        b.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        b.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        b.srcOffsets[0] = {0, cur_->h, 0};
        b.srcOffsets[1] = {ww, cur_->h - hh, 1};
        b.dstOffsets[0] = {x, y, 0};
        b.dstOffsets[1] = {x + ww, y + hh, 1};
        vkCmdBlitImage(cmd, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &b, VK_FILTER_NEAREST);
        vkImageBarrier2(cmd, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    vkImageBarrier2(cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}

void VkDraw2D::dropShot() {
    dropImage(shot_.ms); dropImage(shot_.pic); dropImage(shot_.gray);
    shot_ = Targets();
    if (cur_ == &shot_) cur_ = nullptr;
}

}  // namespace render
