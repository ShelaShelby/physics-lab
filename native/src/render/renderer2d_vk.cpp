// 0.60.0: Vulkan, step 2 of 5 (native/PLAN_VULKAN.md): Renderer2D's end() on Vulkan. The shapes, blobs and segments are recorded by the same code as
// for OpenGL (render/renderer2d.cpp); this file only draws them, segment by segment in their order, with the same shaders (render/shaders.h as SPIR-V),
// the same uniforms and the same blend of each step as Renderer2D::end (render/vk_draw2d.h has the how).
#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include "app/gpu_guard.h"
#include "shading.h"
#include "vk_blackhole.h"
#include "vk_card.h"
#include "vk_draw2d.h"

namespace render {

const float* fuzzTableVk();   // render/renderer2d.cpp (the fuzzy nucleon's table, as end() sends it)
bool logDrawKinds();

static_assert(sizeof(MeshVertex) == 48, "the mesh's vertex input (render/vk_draw2d.cpp kMesh) is 48 bytes");
static_assert(sizeof(Renderer2D::Blob) == 80, "the blobs' instance input (render/vk_draw2d.cpp kBlob) is 80 bytes");

void Renderer2D::useVulkan(VkDraw2D* d) {
    if (!d) useVulkanCards(nullptr);
    vk_ = d;
    vkMeshCreate_ = d ? +[](VkDraw2D* v, const void* p, std::size_t n) { return v->meshCreate(p, n); } : nullptr;
    vkMeshDelete_ = d ? +[](VkDraw2D* v, int id) { v->meshDelete(id); } : nullptr;
    if (!d) for (bool& m : vkMap_) m = false;
}

// 0.61.0: the card programs on Vulkan; null: Vulkan's objects deleted (the labs' card paths then see none)
void Renderer2D::useVulkanCards(VkCards* c, std::vector<std::string>* why) {
    if (atomCard_) { atomCard_->reset(); delete atomCard_; atomCard_ = nullptr; }
    if (molCard_) { molCard_->reset(); delete molCard_; molCard_ = nullptr; }
    if (vkc_ || !c) {
        delete compute_; compute_ = nullptr; delete outline_; outline_ = nullptr; delete rocks_; rocks_ = nullptr;
        dropVkGlow(vkGlow_); vkGlow_ = nullptr;
        delete kerr_; kerr_ = nullptr; kerrTried_ = false; kerrWhy_.clear();   // 0.62.0: the light paths and the Black Hole Lab's passes (Vulkan's)
        delete vkbh_; vkbh_ = nullptr;
        sky_.useVulkan(nullptr, nullptr);
        vkProbe_ = nullptr; vkProbeDone_ = nullptr; vkProbeRead_ = nullptr; vkKerr_ = nullptr; vkGlareBusy_ = false;
        if (vkc_) vkc_->collect();
    }
    vkc_ = c;
    vkAtomCard_ = c ? +[](VkCards& v) { return makeVkAtomCard(v); } : nullptr;
    vkMolCard_ = c ? +[](VkCards& v) { return makeVkMoleculeCard(v); } : nullptr;
    rocksWhy_.clear();
    if (!c) return;
    std::string w;
    const auto note = [&](const char* what) { if (why) why->push_back(std::string(what) + ": " + w); w.clear(); };
    if (!(compute_ = makeVkGroupCompute(*c, w))) note("the superposition and jump groups");
    if (!(outline_ = makeVkOutlineCompute(*c, w))) note("the 90 % outline");
    if (!(rocks_ = makeVkRockCompute(*c, w))) { rocksWhy_ = w; note("the belts' rocks"); }
    if (!(vkGlow_ = makeVkGlow(*c, w))) note("the Cloud 3D Glow dots");
    // 0.62.0 (step 4): the Black Hole Lab's programs: the small ones now, the lens and the table pass on a thread of their own; the sky's stages when the
    // view asks (KerrSky's steps, its guard gpuSkyGuard's Vulkan twin), the light paths at their first use (gpuGuard's twin)
    vkbh_ = new VkBlackHole();
    if (!vkbh_->init(*c, app::gpuVulkanGuard().folder(), w)) { delete vkbh_; vkbh_ = nullptr; note("the Black Hole Lab's pictures"); }
    else if (!w.empty()) note("the Black Hole Lab's pictures");
    VkBlackHole* bh = vkbh_;
    sky_.useVulkan([bh](int stage, KerrSteps* steps, std::string& why) { if (!bh) { why = "the Black Hole Lab's programs did not start on Vulkan"; return -1; } return bh->buildSky(stage, steps, why); },
                   [bh] { return bh && bh->skyReady(); });
    vkProbe_ = +[](Renderer2D& r, const LensSky& L, int pw, int ph, bool queue, std::vector<float>* out) { if (r.vkbh_) r.vkbh_->probe(L, pw, ph, queue, out); };
    vkProbeDone_ = +[](Renderer2D& r) { return r.vkbh_ && r.vkbh_->probeDone(); };
    vkProbeRead_ = +[](Renderer2D& r, std::vector<float>& out) { if (r.vkbh_) r.vkbh_->probeRead(out); };
    vkKerr_ = +[](Renderer2D& r, KerrSteps* steps) -> IKerrCompute* {
        if (!r.kerrTried_) {
            r.kerrTried_ = true;
            std::string kw;
            r.kerr_ = r.vkc_ ? makeVkKerrCompute(*r.vkc_, r.vkbh_ ? r.vkbh_->cache() : VK_NULL_HANDLE, steps, kw) : nullptr;
            r.kerrWhy_ = r.kerr_ ? std::string() : r.vkc_ ? kw : std::string("Vulkan's card programs did not start");
        }
        std::string kw;
        if (r.kerr_ && vkKerrState(r.kerr_, kw) == -1) { delete r.kerr_; r.kerr_ = nullptr; r.kerrWhy_ = kw; }
        return r.kerr_;
    };
}

// 0.61.0: the card programs' work of this frame in one batch before the picture: the rocks' places, the Glow and Density clouds' grids (and what the
// lab recorded before: a jump's groups); the batch is waited for only when a grid must be read back (OpenGL reads it back at once too, glReadPixels)
bool Renderer2D::vkCardsFrame(bool forShot, int fbW, int fbH) {
    const std::size_t n = segments_.size();
    vkGrid_.assign(n, -1);
    vkLight_.assign(n, -1.0);
    vkRocks_.assign(n, 0);
    if (!vkc_) return true;
    if (!forShot) {                                                            // each program's first full use (its guarded step), once a frame
        const int off = vkPollFirstUses(*vkc_, compute_, outline_, rocks_, vkGlow_);
        if (off & 1) { delete compute_; compute_ = nullptr; }
        if (off & 2) { delete outline_; outline_ = nullptr; }
        if (off & 4) { delete rocks_; rocks_ = nullptr; rocksWhy_ = "its first full use did not finish in time"; }
        if (off & 8) { dropVkGlow(vkGlow_); vkGlow_ = nullptr; }
    }
    bool grids = false;
    for (std::size_t i = 0; i < n; i++) {
        const Segment& sg = segments_[i];
        if (sg.kind == Kind::RocksGpu) vkRocks_[i] = rocks_ && rocks_->dispatch(sg.slot, sg.rf) ? 1 : 0;
        else if (sg.kind == Kind::CloudGpu && vkGlow_) {
            const CloudGpuDraw& d = clouds_[std::size_t(sg.slot)];
            const VkBuffer jump = d.grpBuf ? vkJumpGroups(atomCard_) : VK_NULL_HANDLE;
            if (!d.grpBuf || jump) vkGrid_[i] = vkGlowRecord(vkGlow_, d, jump, clBytes_);
        } else if (sg.kind == Kind::MolCloud && molCard_) vkGrid_[i] = vkMolCloudRecord(molCard_, molClouds_[std::size_t(sg.slot)]);
        else if (sg.kind == Kind::Lens && vkbh_) {                             // 0.62.0: the Black Hole Lab's passes, before the frame (render/vk_blackhole.h)
            vkbh_->lens(lens_[std::size_t(sg.slot)], width_, height_, texW_[static_cast<int>(Map::Sky)], fbW, fbH);
            skyStats_ = vkbh_->skyStats(); glareStats_ = vkbh_->glareStats(); lensRedraws_ = vkbh_->redraws(); vkGlareBusy_ = vkbh_->glareBusy();
            maskValid_ = vkbh_->mask() != VK_NULL_HANDLE;
        }
        grids = grids || vkGrid_[i] >= 0;
    }
    if (!vkc_->flush(grids) && vkc_->failed()) return false;
    std::vector<float> cells;
    for (std::size_t i = 0; i < n; i++) {
        if (vkGrid_[i] < 0) continue;
        const Segment& sg = segments_[i];
        const bool glow = sg.kind == Kind::CloudGpu;
        const float w = glow ? clouds_[std::size_t(sg.slot)].w : molClouds_[std::size_t(sg.slot)].cam.w, h = glow ? clouds_[std::size_t(sg.slot)].h : molClouds_[std::size_t(sg.slot)].cam.h;
        const std::size_t cnt = std::size_t(static_cast<int>(w / 8) + 2) * std::size_t(static_cast<int>(h / 8) + 2);
        const float* c = vkc_->cells(vkGrid_[i]);
        if (!c) continue;
        cells.assign(c, c + cnt);
        const auto& exposure = glow ? clouds_[std::size_t(sg.slot)].exposure : molClouds_[std::size_t(sg.slot)].exposure;
        vkLight_[i] = exposure ? exposure(cells) : -1;
    }
    vkc_->collect();
    return !vkc_->failed();
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
    maskValid_ = false;
    if (!vkCardsFrame(forShot, fbW, fbH)) std::fprintf(stderr, "[vulkan] the card programs' batch failed this frame\n");   // 0.61.0 (before the picture's pass)
    if (!V.beginPicture(fbW, fbH, gray_ ? bg : clear, gray_ || forShot, bytes, static_cast<int>(segments_.size()) + 4, forShot)) return;
    const VkDeviceSize vb = V.put(verts_.data(), verts_.size() * sizeof(Vertex));
    const VkDeviceSize ib = V.put(inst_.data(), inst_.size() * sizeof(float));
    const VkDeviceSize bb = V.put(blobs_.data(), blobs_.size() * sizeof(Blob));
    const float size[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float space[3] = {space_.r, space_.g, space_.b};
    for (std::size_t si = 0; si < segments_.size(); si++) {
        const Segment& sg = segments_[si];
        switch (sg.kind) {
        case Kind::Lens:                                                       // 0.62.0: the bent picture laid over (opaque, as OpenGL's drawLens)
            if (vkbh_ && vkbh_->picture()) {
                V.setExternal(1, vkbh_->picture());
                V.draw(VkDraw2D::kComposite, VkDraw2D::kNoBlend, 0, 0, 6, 0, 0, 0, -4);
            }
            break;
        case Kind::RocksGpu: {                                                 // 0.61.0: the squares at the places the card worked out
            if (!vkRocks_[si]) break;
            const float col[4] = {sg.c.r, sg.c.g, sg.c.b, sg.c.a};
            V.setF(VkDraw2D::kSquare, "uSize", size, 2);
            V.setF(VkDraw2D::kSquare, "uSide", &sg.size, 1);
            V.setF(VkDraw2D::kSquare, "uColor", col, 4);
            V.drawInst(VkDraw2D::kSquare, VkDraw2D::kNormal, vkRockPositions(rocks_, sg.slot), VK_NULL_HANDLE, sg.count);
            break;
        }
        case Kind::CloudGpu: {                                                 // 0.61.0: the dots, instanced, in dot order (as the processor's list)
            if (!(vkLight_[si] >= 0) || !vkGlow_) break;
            const CloudGpuDraw& d = clouds_[std::size_t(sg.slot)];
            const float cam0[4] = {d.cx, d.cy, d.pxm, d.ext}, cam1[4] = {d.ca, d.sa, d.cb, d.sb}, view[2] = {d.w, d.h}, light = static_cast<float>(vkLight_[si]);
            const int wheel = sg.pal.wheel ? 1 : 0, lightPage = sg.pal.light ? 1 : 0, zero = 0;
            const float plus[4] = {sg.pal.sign[0].r, sg.pal.sign[0].g, sg.pal.sign[0].b, sg.pal.sign[0].a}, minus[4] = {sg.pal.sign[1].r, sg.pal.sign[1].g, sg.pal.sign[1].b, sg.pal.sign[1].a};
            V.setF(VkDraw2D::kCloud, "uSize", size, 2);
            V.setF(VkDraw2D::kCloud, "uCam0", cam0, 4); V.setF(VkDraw2D::kCloud, "uCam1", cam1, 4); V.setF(VkDraw2D::kCloud, "uView", view, 2);
            V.setF(VkDraw2D::kCloud, "uLight", &light, 1);
            V.setI(VkDraw2D::kCloud, "uPalWheel", &wheel, 1); V.setI(VkDraw2D::kCloud, "uPalLight", &lightPage, 1);
            V.setF(VkDraw2D::kCloud, "uPalPlus", plus, 4); V.setF(VkDraw2D::kCloud, "uPalMinus", minus, 4);
            V.setF(VkDraw2D::kCloud, "uFuzz", fuzzTableVk(), 1, 17); V.setI(VkDraw2D::kCloud, "uSphN", &zero, 1);
            V.drawInst(VkDraw2D::kCloud, sg.additive ? VkDraw2D::kAdditive : VkDraw2D::kNormal, vkGlowPlaces(vkGlow_), vkGlowGroups(vkGlow_), sg.count);
            break;
        }
        case Kind::MolCloud: {                                                 // 0.61.0: the Density cloud's soft dots, from the dots the card made
            if (!(vkLight_[si] >= 0) || !molCard_) break;
            const MolCloudDraw& d = molClouds_[std::size_t(sg.slot)];
            const float light = static_cast<float>(vkLight_[si]);
            const int zero = 0;
            V.setF(VkDraw2D::kMolDot, "uSize", size, 2);
            V.setF(VkDraw2D::kMolDot, "uLight", &light, 1);
            V.setF(VkDraw2D::kMolDot, "uColour", d.colour, 4);
            V.setF(VkDraw2D::kMolDot, "uFuzz", fuzzTableVk(), 1, 17); V.setI(VkDraw2D::kMolDot, "uSphN", &zero, 1);
            V.drawInst(VkDraw2D::kMolDot, d.additive ? VkDraw2D::kAdditive : VkDraw2D::kNormal, vkMolCloudDots(molCard_), VK_NULL_HANDLE, sg.count);
            break;
        }
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
            const int maskOn = vkbh_ && maskValid_ && vkbh_->mask() ? 1 : 0;   // 0.62.0: the spinning hole's open-sky mask (maskedDisc), as OpenGL's
            V.setI(VkDraw2D::kTri, "uMaskOn", maskOn ? &maskOn : &zero, 1);
            V.setI(VkDraw2D::kTri, "uMaskOrigin", origin, 2);
            if (maskOn) V.setExternal(0, vkbh_->mask());
            V.draw(VkDraw2D::kTri, VkDraw2D::kNormal, vb, sg.first, sg.count, 0, 0, 0, maskOn ? -3 : 0);
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
