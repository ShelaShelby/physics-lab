// 0.60.0: Vulkan, step 2 of 5 (native/PLAN_VULKAN.md): Renderer2D's end() on Vulkan. The shapes, blobs and segments are recorded by the same code as
// for OpenGL (render/renderer2d.cpp); this file only draws them, segment by segment in their order, with the same shaders (render/shaders.h as SPIR-V),
// the same uniforms and the same blend of each step as Renderer2D::end (render/vk_draw2d.h has the how).
#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include "shading.h"
#include "vk_draw2d.h"

namespace render {

const float* fuzzTableVk();   // render/renderer2d.cpp (the fuzzy nucleon's table, as end() sends it)
bool logDrawKinds();

static_assert(sizeof(MeshVertex) == 48, "the mesh's vertex input (render/vk_draw2d.cpp kMesh) is 48 bytes");
static_assert(sizeof(Renderer2D::Blob) == 80, "the blobs' instance input (render/vk_draw2d.cpp kBlob) is 80 bytes");

void Renderer2D::useVulkan(VkDraw2D* d) {
    vk_ = d;
    vkMeshCreate_ = d ? +[](VkDraw2D* v, const void* p, std::size_t n) { return v->meshCreate(p, n); } : nullptr;
    vkMeshDelete_ = d ? +[](VkDraw2D* v, int id) { v->meshDelete(id); } : nullptr;
    if (!d) for (bool& m : vkMap_) m = false;
}

bool Renderer2D::loadMapsVulkan(std::string& why) {
    if (!vk_) { why = "no Vulkan"; return false; }
    // as init(): a map that cannot be read is left out (that body is drawn as a plain ball, the sky without the photo); the planets' maps are sRGB (decoded
    // to linear light by the card), the sky's is not
    for (int m = 1; m < static_cast<int>(Map::Count); m++) {
        MapImage img;
        if (!decodeMap(static_cast<Map>(m), img)) { std::fprintf(stderr, "map %s missing\n", mapInfo(static_cast<Map>(m)).file); continue; }
        if (!vk_->upload(m, img.w, img.h, img.rgba, m != static_cast<int>(Map::Sky), why)) return false;
        vkMap_[m] = true;
        texW_[m] = img.w;
    }
    // Earth's air table (render/shading.h airTableM, airTransmit): set once, the program keeps it (as init())
    float t[3 * kAirTableN];
    for (int i = 0; i < kAirTableN; i++) for (int c = 0; c < 3; c++) t[3 * i + c] = static_cast<float>(airTransmit(c, airTableM(i)));
    vk_->setF(VkDraw2D::kPlanet, "uAirT", t, 3, kAirTableN);
    const float k3[3] = {static_cast<float>(kEarthAirK), static_cast<float>(airTableLnLo()), static_cast<float>(airTableLnHi())};
    vk_->setF(VkDraw2D::kPlanet, "uAirK", k3, 3);
    return true;
}

std::string Renderer2D::drawKindsText() const {
    static const char* names[kdKindCount] = {"rect", "line", "triangle", "disc", "ring", "sphere", "sunDisc", "fadeDisc", "maskedDisc", "squares", "blobs", "spheres", "mesh",
                                             "softLayer", "cloudLayer", "planet", "planetMap", "planetRings", "planetEclipse", "planetAir", "skyMap", "gray",
                                             "blob0", "blob1", "blob2", "blob3", "blob4", "blob5", "blob6", "blob7", "blob8", "blob9"};
    std::string s;
    for (int i = 0; i < kdKindCount; i++) s += std::string(i ? " " : "") + names[i] + "=" + std::to_string(kinds_[i]);
    return s;
}

void Renderer2D::endVulkan(int fbW, int fbH, const float clear[4], bool forShot) {
    VkDraw2D& V = *vk_;
    closeTriangles();
    lastCalls_ = 0; lastInstances_ = 0; lastVerts_ = static_cast<long>(verts_.size());
    for (const Segment& sg : segments_) {
        if (sg.kind != Kind::LayerBegin) lastCalls_++;
        if (sg.kind == Kind::Squares || sg.kind == Kind::Blobs || sg.kind == Kind::RocksGpu || sg.kind == Kind::CloudGpu || sg.kind == Kind::MolCloud) lastInstances_ += sg.count;
    }
    if (segments_.empty()) { V.noPicture(); return; }                         // (OpenGL: nothing drawn, the page shows)
    if (gray_ && logDrawKinds()) kinds_[kdGray]++;
    const float bg[4] = {grayBg_.r, grayBg_.g, grayBg_.b, 1.0f};               // Monochrome: the page behind the picture, so the grey copy is opaque (as end())
    const std::size_t bytes = verts_.size() * sizeof(Vertex) + inst_.size() * sizeof(float) + blobs_.size() * sizeof(Blob) + 64;
    if (!V.beginPicture(fbW, fbH, gray_ ? bg : clear, gray_ || forShot, bytes, static_cast<int>(segments_.size()) + 4, forShot)) return;
    const VkDeviceSize vb = V.put(verts_.data(), verts_.size() * sizeof(Vertex));
    const VkDeviceSize ib = V.put(inst_.data(), inst_.size() * sizeof(float));
    const VkDeviceSize bb = V.put(blobs_.data(), blobs_.size() * sizeof(Blob));
    const float size[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float space[3] = {space_.r, space_.g, space_.b};
    for (const Segment& sg : segments_) {
        switch (sg.kind) {
        case Kind::Lens: case Kind::CloudGpu: case Kind::MolCloud: case Kind::RocksGpu:
            break;                                                             // card programs: never recorded on Vulkan (their objects are not made; step 3)
        case Kind::LayerBegin:
            if (sg.hdr) V.beginLayer(true, fbW, fbH);                          // 0.40: the cloud's float picture, the screen's size
            else V.beginLayer(false, std::max(1, static_cast<int>(std::ceil(fbW * layerScale_))), std::max(1, static_cast<int>(std::ceil(fbH * layerScale_))));
            break;
        case Kind::LayerEnd:
            V.endLayer();
            V.composite(sg.hdr, sg.hdr && sg.additive ? VkDraw2D::kOneOne : VkDraw2D::kPremul);   // summed light added / premultiplied over
            break;
        case Kind::Blobs: {
            const int one = 1, zero = 0;
            V.setF(VkDraw2D::kBlob, "uSize", size, 2);
            V.setF(VkDraw2D::kBlob, "uFuzz", fuzzTableVk(), 1, 17);
            V.setI(VkDraw2D::kBlob, "uPalWheel", sg.pal.wheel ? &one : &zero, 1);
            V.setI(VkDraw2D::kBlob, "uPalLight", sg.pal.light ? &one : &zero, 1);
            const float plus[4] = {sg.pal.sign[0].r, sg.pal.sign[0].g, sg.pal.sign[0].b, sg.pal.sign[0].a}, minus[4] = {sg.pal.sign[1].r, sg.pal.sign[1].g, sg.pal.sign[1].b, sg.pal.sign[1].a};
            V.setF(VkDraw2D::kBlob, "uPalPlus", plus, 4);
            V.setF(VkDraw2D::kBlob, "uPalMinus", minus, 4);
            if (sg.spheres) {                                                  // 0.40: kind 9 tests each pixel against every sphere of its call
                float sph[4 * kMaxSpheres];
                const int n = std::min(sg.count, kMaxSpheres);
                for (int k = 0; k < n; k++) { const Blob& o = blobs_[std::size_t(sg.first + k)]; sph[4 * k] = o.x; sph[4 * k + 1] = o.y; sph[4 * k + 2] = o.fy; sph[4 * k + 3] = o.r; }
                V.setF(VkDraw2D::kBlob, "uSph", sph, 4, n);
                V.setI(VkDraw2D::kBlob, "uSphN", &n, 1);
            } else V.setI(VkDraw2D::kBlob, "uSphN", &zero, 1);
            V.draw(VkDraw2D::kBlob, sg.additive ? VkDraw2D::kAdditive : VkDraw2D::kNormal, 0, 0, 6, bb, sg.count, sg.first);
            break;
        }
        case Kind::Mesh: {
            const Mesh& m = meshes_[std::size_t(sg.slot)];
            if (!m.used || m.vk < 0) break;
            const MeshProjection& P = sg.mp;
            const float rot[4] = {static_cast<float>(P.cosY), static_cast<float>(P.sinY), static_cast<float>(P.cosP), static_cast<float>(P.sinP)};
            const float cen[4] = {static_cast<float>(P.cx), static_cast<float>(P.cy), static_cast<float>(P.K), static_cast<float>(P.fx)};
            const float fit[4] = {static_cast<float>(P.fy), P.identity ? 1.f : 0.f, P.flat ? 1.f : 0.f, 0.f};
            V.setF(VkDraw2D::kMesh, "uSize", size, 2);
            V.setF(VkDraw2D::kMesh, "uRot", rot, 4);
            V.setF(VkDraw2D::kMesh, "uCen", cen, 4);
            V.setF(VkDraw2D::kMesh, "uFit", fit, 4);
            const int zero = 0;
            V.setI(VkDraw2D::kMesh, "uMaskOn", &zero, 1);                       // (OpenGL never sets it for the mesh program: 0, its default)
            V.drawMesh(m.vk, sg.first, sg.count);
            break;
        }
        case Kind::Planet: {
            const int mode = sg.mode, lunar = sg.lunar ? 1 : 0;
            V.setF(VkDraw2D::kPlanet, "uSize", size, 2);
            V.setF(VkDraw2D::kPlanet, "uSpace", space, 3);
            V.setMat3T(VkDraw2D::kPlanet, "uK", sg.K);
            V.setI(VkDraw2D::kPlanet, "uMode", &mode, 1);
            V.setI(VkDraw2D::kPlanet, "uLunar", &lunar, 1);
            const float tw = static_cast<float>(std::max(texW_[sg.map], 1));
            // the mipmap level: texels per pixel. A planet: across its middle; the sky: along the galactic equator (as end())
            const float lod = sg.mode == 2 ? std::max(0.0f, std::log2(tw / 6.2831853f / std::max(sg.r, 1.0f))) : std::max(0.0f, std::log2(tw / (3.14159f * std::max(2.0f * sg.r, 1.0f))));
            V.setF(VkDraw2D::kPlanet, "uLod", &lod, 1);
            const float lon0 = static_cast<float>(mapInfo(static_cast<Map>(sg.map)).lonCentreDeg * 3.14159265358979 / 180);
            V.setF(VkDraw2D::kPlanet, "uLon0", &lon0, 1);
            const int ringN = sg.mode == 2 ? 0 : sg.nring;
            V.setI(VkDraw2D::kPlanet, "uRingN", &ringN, 1);
            if (sg.nring > 0 && sg.mode != 2) V.setF(VkDraw2D::kPlanet, "uRingB", sg.ring, 3, sg.nring);
            const float shape[2] = {sg.mode == 2 || sg.shape[0] <= 0 ? 1.0f : sg.shape[0], sg.mode == 2 || sg.shape[1] <= 0 ? 1.0f : sg.shape[1]};
            V.setF(VkDraw2D::kPlanet, "uShape", shape, 2);
            const int occN = sg.mode == 2 ? 0 : sg.nocc;
            V.setI(VkDraw2D::kPlanet, "uOccN", &occN, 1);
            if (sg.nocc > 0 && sg.mode != 2) { V.setF(VkDraw2D::kPlanet, "uOcc", sg.occ, 4, sg.nocc); V.setF(VkDraw2D::kPlanet, "uSunAng", &sg.sunAng, 1); }
            const int air = sg.mode != 2 && sg.air ? 1 : 0;
            V.setI(VkDraw2D::kPlanet, "uAir", &air, 1);
            // (OpenGL binds tex_[sg.map]: 0, no texture, for a plain ball; the white one here: the shader does not read it then)
            V.draw(VkDraw2D::kPlanet, VkDraw2D::kNormal, vb, sg.first, sg.count, 0, 0, 0, vkMap_[sg.map] ? sg.map : 0);
            break;
        }
        case Kind::Triangles: {
            const int zero = 0;
            const int origin[2] = {0, 0};
            V.setF(VkDraw2D::kTri, "uSize", size, 2);
            V.setF(VkDraw2D::kTri, "uSpace", space, 3);
            V.setI(VkDraw2D::kTri, "uMaskOn", &zero, 1);                        // (the open-sky mask: the Black Hole Lab's, not drawn on Vulkan in this step)
            V.setI(VkDraw2D::kTri, "uMaskOrigin", origin, 2);
            V.draw(VkDraw2D::kTri, VkDraw2D::kNormal, vb, sg.first, sg.count);
            break;
        }
        case Kind::Squares: {
            const float col[4] = {sg.c.r, sg.c.g, sg.c.b, sg.c.a};
            V.setF(VkDraw2D::kSquare, "uSize", size, 2);
            V.setF(VkDraw2D::kSquare, "uSide", &sg.size, 1);
            V.setF(VkDraw2D::kSquare, "uColor", col, 4);
            V.draw(VkDraw2D::kSquare, VkDraw2D::kNormal, 0, 0, 6, ib, sg.count, sg.first);
            break;
        }
        }
    }
    V.endPicture(gray_);
}

}  // namespace render
