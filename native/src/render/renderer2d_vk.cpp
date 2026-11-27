// Renderer2D's end() on Vulkan. The shapes, blobs and segments are recorded by
// render/renderer2d.cpp; this file only draws them, segment by segment in their order, with the same shaders (render/shaders.h as SPIR-V),
// the uniforms and the blend of each step (render/vk_draw2d.h has the how).
#include "app/frame_cost.h"
#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/gpu_guard.h"
#include "shading.h"
#include "sun_limb.h"
#include "sun_surface.h"
#include "vk_blackhole.h"
#include "vk_card.h"
#include "vk_draw2d.h"

namespace render {

const float* fuzzTableVk();   // render/renderer2d.cpp (the fuzzy nucleon's table, as end() sends it)
bool logDrawKinds();

static_assert(sizeof(MeshVertex) == 48, "the mesh's vertex input (render/vk_draw2d.cpp kMesh) is 48 bytes");
static_assert(sizeof(Renderer2D::Blob) == 80, "the blobs' instance input (render/vk_draw2d.cpp kBlob) is 80 bytes");
static_assert(VkDraw2D::kLine == 10 && VkDraw2D::kSurf == 11, "Renderer2D::linesProgram and surfaceProgram name kLine and kSurf by number (render/renderer2d.cpp)");

void Renderer2D::useVulkan(VkDraw2D* d) {
    if (!d) useVulkanCards(nullptr);
    vk_ = d;
    vkMeshCreate_ = d ? +[](VkDraw2D* v, const void* p, std::size_t n) { return v->meshCreate(p, n); } : nullptr;
    vkMeshDelete_ = d ? +[](VkDraw2D* v, int id) { v->meshDelete(id); } : nullptr;
    vkAddProgram_ = d ? +[](VkDraw2D* v, int p, std::string& why) { return v->addProgram(static_cast<VkDraw2D::Prog>(p), why); } : nullptr;   // 0.68.0
    if (!d) for (bool& m : vkMap_) m = false;
}

// 0.61.0: the card programs on Vulkan; null: Vulkan's objects deleted (the labs' card paths then see none)
void Renderer2D::useVulkanCards(VkCards* c, std::vector<std::string>* why) {
    if (atomCard_) { atomCard_->reset(); delete atomCard_; atomCard_ = nullptr; }
    if (porkCard_) { porkCard_->reset(); delete porkCard_; porkCard_ = nullptr; }   // 0.92.0
    if (molCard_) { molCard_->reset(); delete molCard_; molCard_ = nullptr; }
    delete ringCard_; ringCard_ = nullptr;   // 0.64.0
    delete classicCard_; classicCard_ = nullptr;
    delete nucDensCard_; nucDensCard_ = nullptr;   // 0.67.0
    delete linesCard_; linesCard_ = nullptr;   // 0.68.0
    delete surfaceCard_; surfaceCard_ = nullptr;
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
    vkPorkCard_ = c ? +[](VkCards& v) { return makeVkPorkchopCard(v); } : nullptr;
    vkAtomCard_ = c ? +[](VkCards& v) { return makeVkAtomCard(v); } : nullptr;
    vkMolCard_ = c ? +[](VkCards& v) { return makeVkMoleculeCard(v); } : nullptr;
    vkRingCard_ = c ? +[](VkCards& v) { return makeVkRingCard(v); } : nullptr;
    vkClassicCard_ = c ? +[](VkCards& v) { return makeVkClassicCard(v); } : nullptr;
    vkNucDensCard_ = c ? +[](VkCards& v) { return makeVkNucDensCard(v); } : nullptr;   // 0.67.0
    vkLinesCard_ = c ? +[](VkCards& v) { return makeVkLinesCard(v); } : nullptr;       // 0.68.0
    vkSurfaceCard_ = c ? +[](VkCards& v) { return makeVkSurfaceCard(v); } : nullptr;
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
// lab recorded before: a jump's groups); the batch is waited for only when a grid must be read back (the CPU needs it at once)
bool Renderer2D::vkCardsFrame(bool forShot, int fbW, int fbH) {
    const std::size_t n = segments_.size();
    vkGrid_.assign(n, -1);
    vkLight_.assign(n, -1.0);
    vkRocks_.assign(n, 0);
    classicOk_.assign(n, 0);
    if (!vkc_) return true;
    if (!forShot) {                                                            // each program's first full use (its guarded step), once a frame
        const int off = vkPollFirstUses(*vkc_, compute_, outline_, rocks_, vkGlow_);
        if (off & 1) { delete compute_; compute_ = nullptr; }
        if (off & 2) { delete outline_; outline_ = nullptr; }
        if (off & 4) { delete rocks_; rocks_ = nullptr; rocksWhy_ = "its first full use did not finish in time"; }
        if (off & 8) { dropVkGlow(vkGlow_); vkGlow_ = nullptr; }
    }
    bool grids = false;
    vkc_->gridFrame(true);                                                    // 0.90.0: this frame's grids in their bank (the frame after reads them)
    for (std::size_t i = 0; i < n; i++) {
        const Segment& sg = segments_[i];
        if (sg.kind == Kind::RocksGpu) vkRocks_[i] = rocks_ && (sg.grains ? rocks_->grainDispatch(sg.slot, sg.rf) : rocks_->dispatch(sg.slot, sg.rf)) ? 1 : 0;   // (0.78.0: or a comet's grains)
        else if (sg.kind == Kind::CloudGpu && vkGlow_) {
            const CloudGpuDraw& d = clouds_[std::size_t(sg.slot)];
            const VkBuffer jump = d.grpBuf ? vkJumpGroups(atomCard_) : VK_NULL_HANDLE;
            if (!d.grpBuf || jump) vkGrid_[i] = vkGlowRecord(vkGlow_, d, jump, clBytes_);
        } else if (sg.kind == Kind::MolCloud && molCard_) vkGrid_[i] = vkMolCloudRecord(molCard_, molClouds_[std::size_t(sg.slot)]);
        else if (sg.kind == Kind::CloudClassic) {                             // 0.64.0
            const ClassicDraw& d = classicDraws_[std::size_t(sg.slot)];
            classicOk_[i] = vkClassicRecord(classicCard_, d, clBytes_) ? 1 : 0;
            if (classicOk_[i] && d.soft) { classicOk_[i] = 2; grids = true; }   // 0.65.0: the side view's Glow: its cells read back with the grids
        }
        else if (sg.kind == Kind::NucDens) classicOk_[i] = vkNucDensRecord(nucDensCard_, nucDraws_[std::size_t(sg.slot)]) ? 1 : 0;   // 0.67.0
        else if (sg.kind == Kind::Lines) classicOk_[i] = vkLinesRecord(linesCard_, sg.slot, linesDraws_[std::size_t(sg.slot)]) ? 1 : 0;   // 0.68.0
        else if (sg.kind == Kind::Surface) classicOk_[i] = vkSurfaceRecord(surfaceCard_, surfaceDraws_[std::size_t(sg.slot)]) ? 1 : 0;
        else if (sg.kind == Kind::Lens && vkbh_) {                             // 0.62.0: the Black Hole Lab's passes, before the frame (render/vk_blackhole.h)
            vkbh_->lens(lens_[std::size_t(sg.slot)], width_, height_, texW_[static_cast<int>(Map::Sky)], fbW, fbH);
            skyStats_ = vkbh_->skyStats(); glareStats_ = vkbh_->glareStats(); lensRedraws_ = vkbh_->redraws(); vkGlareBusy_ = vkbh_->glareBusy();
            maskValid_ = vkbh_->mask() != VK_NULL_HANDLE;
        }
        grids = grids || vkGrid_[i] >= 0;
    }
    vkc_->gridFrame(false);
    // 0.90.0: each Glow or Density cloud's exposure from its grid of the frame before (the same kind and number in the frame, the same size, made in the
    // frame just before): the frame does not wait for its own grid. The exposure is then one frame late. The Glow core's dots come from that frame too
    // (its pass and its places), so the exposure is the one that frame would have had. Only a cloud with no such grid (its first frame, a new size), the
    // side view's Glow (its last steps need the light in the same batch) and PL_GRID_WAIT (tests: the frame's own grid, as before) wait.
    static const bool ownGrid = std::getenv("PL_GRID_WAIT") != nullptr;
    const long frame = ++cardsFrame_;
    std::vector<int> prevOf(n, -1);
    bool wait = false;
    int nthGlow = 0, nthMol = 0;
    std::vector<int> nth(n, 0);
    for (std::size_t i = 0; i < n; i++) {
        if (classicOk_[i] == 2) wait = true;
        if (vkGrid_[i] < 0) continue;
        const Segment& sg = segments_[i];
        const bool glow = sg.kind == Kind::CloudGpu;
        nth[i] = glow ? nthGlow++ : nthMol++;
        const float w = glow ? clouds_[std::size_t(sg.slot)].w : molClouds_[std::size_t(sg.slot)].cam.w, h = glow ? clouds_[std::size_t(sg.slot)].h : molClouds_[std::size_t(sg.slot)].cam.h;
        const int gw = static_cast<int>(w / 8) + 2, gh = static_cast<int>(h / 8) + 2;
        for (std::size_t k = 0; k < prevGrids_.size() && !ownGrid; k++) {
            const PrevGrid& p = prevGrids_[k];
            if (p.frame == frame - 1 && p.kind == static_cast<int>(sg.kind) && p.nth == nth[i] && p.gw == gw && p.gh == gh) { prevOf[i] = static_cast<int>(k); break; }
        }
        if (prevOf[i] < 0) wait = true;
    }
    std::uint64_t ticket = 0;
    {   app::FrameCostLabel l(wait && grids ? "the exposure grids (Glow or Density cloud)" : "the frame's card batch");
        ticket = vkc_->flush(wait && grids);
        if (!ticket && vkc_->failed()) return false; }
    std::vector<float> cells;
    bool more = false;
    for (std::size_t i = 0; i < n; i++) {                                     // 0.65.0: the side view's Glow: the dots' light from its cells, then its last two steps
        if (classicOk_[i] != 2) continue;
        const ClassicDraw& d = classicDraws_[std::size_t(segments_[i].slot)];
        const double light = vkClassicCells(classicCard_, cells) && d.exposure ? d.exposure(cells) : -1;
        classicOk_[i] = light >= 0 && vkClassicFinish(classicCard_, d, static_cast<float>(light)) ? 1 : 0;
        more = more || classicOk_[i] == 1;
    }
    if (more) {
        app::FrameCostLabel l("the frame's card batch");
        if (!vkc_->flush(false) && vkc_->failed()) return false;
    }
    GlowNearDots nearDots;
    std::vector<PrevGrid> now;
    for (std::size_t i = 0; i < n; i++) {
        if (vkGrid_[i] < 0) continue;
        const Segment& sg = segments_[i];
        const bool glow = sg.kind == Kind::CloudGpu;
        const float w = glow ? clouds_[std::size_t(sg.slot)].w : molClouds_[std::size_t(sg.slot)].cam.w, h = glow ? clouds_[std::size_t(sg.slot)].h : molClouds_[std::size_t(sg.slot)].cam.h;
        const int gw = static_cast<int>(w / 8) + 2, gh = static_cast<int>(h / 8) + 2;
        const std::size_t cnt = std::size_t(gw) * std::size_t(gh);
        PrevGrid me;
        me.kind = static_cast<int>(sg.kind); me.nth = nth[i]; me.index = vkGrid_[i]; me.gw = gw; me.gh = gh; me.ticket = ticket; me.frame = frame;
        if (glow) { me.draw = clouds_[std::size_t(sg.slot)]; me.nearBank = vkGlowNearBank(vkGlow_); }
        now.push_back(me);
        int index = vkGrid_[i];
        const PrevGrid* p = !wait && prevOf[i] >= 0 ? &prevGrids_[std::size_t(prevOf[i])] : nullptr;   // (a frame that waited uses its own grids, as before)
        if (p && !vkc_->done(p->ticket)) {                                  // (the frame before's batch not finished yet: waited for, counted)
            app::FrameCostLabel l("the exposure grids of the frame before");
            app::FrameCostSpan span(app::FrameCostKind::Readback);
            const double t0 = app::frameCostNowMs();
            const bool ok = vkc_->waitFor(p->ticket, 2.0);
            gridStats_.waited++; gridStats_.waitMs += app::frameCostNowMs() - t0;
            if (!ok) { p = nullptr; if (!vkc_->waitFor(ticket, 2.0)) continue; }
        }
        if (p) { index = p->index; gridStats_.late++; } else gridStats_.own++;
        // the core's dots of the same frame as the grid (the frame before: its own nearFound with its pass's dots and places)
        if (glow && p && p->draw.near && p->draw.nearFound && vkGlowNear(vkGlow_, p->nearBank, nearDots)) p->draw.nearFound(&nearDots);
        else if (glow && clouds_[std::size_t(sg.slot)].near && clouds_[std::size_t(sg.slot)].nearFound) {
            const CloudGpuDraw& d = clouds_[std::size_t(sg.slot)];
            d.nearFound(!p && vkGlowNear(vkGlow_, vkGlowNearBank(vkGlow_), nearDots) ? &nearDots : nullptr);   // (this frame's: its pass, or its own over every dot)
        }
        const float* c = vkc_->cells(index);
        if (!c) continue;
        cells.assign(c, c + cnt);
        const auto& exposure = glow ? clouds_[std::size_t(sg.slot)].exposure : molClouds_[std::size_t(sg.slot)].exposure;
        vkLight_[i] = exposure ? exposure(cells) : -1;
    }
    prevGrids_.swap(now);
    vkc_->collect();
    return !vkc_->failed();
}

bool Renderer2D::cloudNearGpuOk() const {   // 0.90.0
    static const bool never = std::getenv("PL_NO_GLOW_NEAR_GPU") != nullptr;   // (tests and timing: the processor's pass over every dot)
    return !never && vk_ && vkGlowNearOk(vkGlow_);
}

// 0.69.0: the Black Hole Lab's test hooks on Vulkan (render/vk_blackhole.h)
void Renderer2D::lensFloat(const LensSky& L, int w, int h, std::vector<float>& out) {
    out.clear();
    if (vkbh_) vkbh_->debugPicture(L, w, h, texW_[static_cast<int>(Map::Sky)], w, h, out);
}
void Renderer2D::skyMath(const std::vector<float>& in, std::vector<float>& out) {
    if (vkbh_) vkbh_->math(in, out);
    else out.assign(in.size() / 12 * 8, 0.0f);
}
void Renderer2D::glareVeilCard(int W, int H, double cx, double cy, double K, int b, const std::vector<float>& src, std::vector<float>& out,
                               void (*tables)(int, int, double, double, double, std::vector<float>&, std::vector<float>&, double[2])) {
    out.clear();
    if (vkbh_) vkbh_->veilTest(W, H, cx, cy, K, b, src, out, tables);
}
int Renderer2D::skyLensState() const { return vkbh_ ? vkbh_->lensState() : 2; }
double Renderer2D::skyWhiteWait() { if (!vkbh_) return 0; const double w = vkbh_->whiteWait(); skyStats_ = vkbh_->skyStats(); return w; }   // (0.70.0: the stats too, at once)

bool Renderer2D::uploadSunPhoto(int w, int h, const std::vector<std::uint8_t>& rgba, std::string& why) {   // 0.94.0: linear grey (UNORM), with mip levels
    if (!vk_) { why = "no Vulkan"; return false; }
    return vk_->upload(kSunPhotoSlot, w, h, rgba, false, why);
}

bool Renderer2D::loadMapsVulkan(std::string& why) {
    if (!vk_) { why = "no Vulkan"; return false; }
    // as init(): a map that cannot be read is left out (that body is drawn as a plain ball, the sky without the photo); the planets' maps are sRGB (decoded
    // to linear light by the card; Pluto's is linear since 0.88.0), the sky's is not
    for (int m = 1; m < static_cast<int>(Map::Count); m++) {
        MapImage img;
        if (!decodeMap(static_cast<Map>(m), img)) { std::fprintf(stderr, "map %s missing\n", mapInfo(static_cast<Map>(m)).file); continue; }
        if (!vk_->upload(m, img.w, img.h, img.rgba, m != static_cast<int>(Map::Sky) && !mapInfo(static_cast<Map>(m)).linear, why)) return false;   // 0.88.0: a linear map (Pluto) is a UNORM texture: the card reads byte / 255, as mapDecode
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
                                             "blob0", "blob1", "blob2", "blob3", "blob4", "blob5", "blob6", "blob7", "blob8", "blob9", "blob10"};
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
    if (segments_.empty()) { V.noPicture(); return; }                         // (nothing drawn, the page shows)
    if (keep_ && !forShot && V.keepPicture(fbW, fbH, gray_)) return;          // 0.67.0: the kept picture (keepLast)
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
        case Kind::Lens:                                                       // 0.62.0: the bent picture laid over (opaque)
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
            } else if (sg.glowN > 0) { V.setF(VkDraw2D::kBlob, "uSph", sg.glow, 4, sg.glowN); V.setI(VkDraw2D::kBlob, "uSphN", &sg.glowN, 1); }   // 0.66.0: kind 10's table
            else V.setI(VkDraw2D::kBlob, "uSphN", &zero, 1);
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
            V.setI(VkDraw2D::kMesh, "uMaskOn", &zero, 1);                       // (0 for the mesh program, its default)
            V.drawMesh(m.vk, sg.first, sg.count);
            break;
        }
        case Kind::NucDens: {                                                  // 0.67.0: the nucleus' Density squares kNucDensCS wrote (kBlob, kind 2)
            if (!classicOk_[si]) break;
            const int zero = 0;
            V.setF(VkDraw2D::kBlob, "uSize", size, 2);
            V.setF(VkDraw2D::kBlob, "uFuzz", fuzzTableVk(), 1, 17);
            V.setI(VkDraw2D::kBlob, "uPalWheel", &zero, 1);
            V.setI(VkDraw2D::kBlob, "uPalLight", &zero, 1);
            V.setI(VkDraw2D::kBlob, "uSphN", &zero, 1);
            V.drawInst(VkDraw2D::kBlob, VkDraw2D::kNormal, vkNucDensSquares(nucDensCard_), VK_NULL_HANDLE, sg.count);
            break;
        }
        case Kind::Lines: {                                                    // 0.68.0: the Orbit Lab's lines, as the card's steps laid them out (kLine)
            VkBuffer bufs[4] = {};
            VkBuffer ind = VK_NULL_HANDLE;
            if (!classicOk_[si] || !vkLinesBuffers(linesCard_, sg.slot, bufs, ind)) break;
            const LinesCamera& c = linesDraws_[std::size_t(sg.slot)].cam;
            const int n[4] = {static_cast<int>(linesDraws_[std::size_t(sg.slot)].runs.size()), sg.count, 0, 0};
            V.setF(VkDraw2D::kLine, "uSize", size, 2);
            V.setF(VkDraw2D::kLine, "uCHi", c.cHi, 3); V.setF(VkDraw2D::kLine, "uCLo", c.cLo, 3); V.setF(VkDraw2D::kLine, "uRot", c.rot, 4);
            V.setF(VkDraw2D::kLine, "uProj", c.proj, 4); V.setF(VkDraw2D::kLine, "uScr", c.scr, 4); V.setF(VkDraw2D::kLine, "uView", c.view, 4);
            V.setI(VkDraw2D::kLine, "uN", n, 4);
            const int zero = 0;
            V.setI(VkDraw2D::kLine, "uMaskOn", &zero, 1);
            V.setF(VkDraw2D::kLine, "uSpace", space, 3);
            V.drawStorage(VkDraw2D::kLine, bufs, ind);
            break;
        }
        case Kind::Surface: {                                                  // 0.68.0: the Density surface in the order the card sorted it (kSurf)
            VkBuffer bufs[4] = {};
            if (!classicOk_[si] || !vkSurfaceBuffers(surfaceCard_, bufs)) break;
            const SurfaceView& sv = surfaceDraws_[std::size_t(sg.slot)].view;
            const float alpha[2] = {sv.aFront, sv.aBack};
            const int zero = 0;
            V.setF(VkDraw2D::kSurf, "uSize", size, 2);
            V.setF(VkDraw2D::kSurf, "uAlpha", alpha, 2);
            V.setI(VkDraw2D::kSurf, "uMaskOn", &zero, 1);
            V.setF(VkDraw2D::kSurf, "uSpace", space, 3);
            const VkBuffer ind = vkSurfaceIndirect(surfaceCard_);            // 0.90.0: the surface made on the card: its count from the card
            V.drawStorage(VkDraw2D::kSurf, bufs, ind, ind ? 0u : static_cast<std::uint32_t>(3 * sg.count));
            break;
        }
        case Kind::CloudClassic: {                                             // 0.64.0: the squares kClassicCS wrote, in the processor's order (kBlob)
            if (!classicOk_[si]) break;
            const ClassicDraw& d = classicDraws_[std::size_t(sg.slot)];
            const int one = 1, zero = 0;
            V.setF(VkDraw2D::kBlob, "uSize", size, 2);
            V.setF(VkDraw2D::kBlob, "uFuzz", fuzzTableVk(), 1, 17);
            V.setI(VkDraw2D::kBlob, "uPalWheel", d.wheel ? &one : &zero, 1);
            V.setI(VkDraw2D::kBlob, "uPalLight", d.light ? &one : &zero, 1);
            V.setF(VkDraw2D::kBlob, "uPalPlus", d.plus, 4);
            V.setF(VkDraw2D::kBlob, "uPalMinus", d.minus, 4);
            V.setI(VkDraw2D::kBlob, "uSphN", &zero, 1);
            V.drawInst(VkDraw2D::kBlob, d.additive ? VkDraw2D::kAdditive : VkDraw2D::kNormal, vkClassicSquares(classicCard_), VK_NULL_HANDLE, sg.count);   // (0.65.0: the side view's Glow on a dark page adds up)
            break;
        }
        case Kind::Rings: {                                                    // 0.64.0: Saturn's rings per pixel (kRingsFS)
            const RingsDraw& d = ringDraws_[std::size_t(sg.slot)];
            ringsUniforms(d, [&](const char* name, const float* v, int comps, int count) { V.setF(VkDraw2D::kRings, name, v, comps, count); },
                             [&](const char* name, const int* v, int comps) { V.setI(VkDraw2D::kRings, name, v, comps); });
            V.setF(VkDraw2D::kRings, "uSize", size, 2);
            const int aa = gray_ || forShot || V.samples() <= 1 ? 0 : 1;           // (the picture's own multisampling: the 4 samples averaged in the shader)
            const float px = fbW > 0 ? static_cast<float>(width_ / fbW) : 1.0f;
            V.setI(VkDraw2D::kRings, "uAA", &aa, 1);
            V.setF(VkDraw2D::kRings, "uPx", &px, 1);
            V.draw(VkDraw2D::kRings, VkDraw2D::kNormal, vb, sg.first, sg.count);
            break;
        }
        case Kind::Planet: {
            const int mode = sg.mode, lunar = sg.lunar ? 1 : 0;
            V.setF(VkDraw2D::kPlanet, "uSize", size, 2);
            V.setF(VkDraw2D::kPlanet, "uSpace", space, 3);
            V.setMat3T(VkDraw2D::kPlanet, "uK", sg.K);
            V.setI(VkDraw2D::kPlanet, "uMode", &mode, 1);
            V.setI(VkDraw2D::kPlanet, "uLunar", &lunar, 1);
            const float albedoK = sg.mode == 1 ? sg.albedoK : 1.0f;   // 0.77.0 (the sky, modes 2 and 3, and the Sun, mode 4, do not read it)
            V.setF(VkDraw2D::kPlanet, "uAlbedoK", &albedoK, 1);
            const int clouds = sg.mode == 1 && sg.map == static_cast<int>(Map::Earth) ? 1 : 0;   // 0.77.0: Earth's map carries its clouds in alpha
            V.setI(VkDraw2D::kPlanet, "uClouds", &clouds, 1);
            const float tw = static_cast<float>(std::max(texW_[sg.map], 1));
            // the mipmap level: texels per pixel. A planet: across its middle; the sky: along the galactic equator (as end())
            const float lod = (sg.mode == 2 || sg.mode == 3) ? std::max(0.0f, std::log2(tw / 6.2831853f / std::max(sg.r, 1.0f))) : std::max(0.0f, std::log2(tw / (3.14159f * std::max(2.0f * sg.r, 1.0f))));
            V.setF(VkDraw2D::kPlanet, "uLod", &lod, 1);
            const float lon0 = static_cast<float>(mapInfo(static_cast<Map>(sg.map)).lonCentreDeg * 3.14159265358979 / 180);
            V.setF(VkDraw2D::kPlanet, "uLon0", &lon0, 1);
            const int ringN = (sg.mode == 2 || sg.mode == 3) ? 0 : sg.nring;
            V.setI(VkDraw2D::kPlanet, "uRingN", &ringN, 1);
            if (sg.nring > 0 && (sg.mode != 2 && sg.mode != 3)) V.setF(VkDraw2D::kPlanet, "uRingB", sg.ring, 3, sg.nring);
            const float shape[2] = {(sg.mode == 2 || sg.mode == 3) || sg.shape[0] <= 0 ? 1.0f : sg.shape[0], (sg.mode == 2 || sg.mode == 3) || sg.shape[1] <= 0 ? 1.0f : sg.shape[1]};
            V.setF(VkDraw2D::kPlanet, "uShape", shape, 2);
            const int occN = (sg.mode == 2 || sg.mode == 3) ? 0 : sg.nocc;
            V.setI(VkDraw2D::kPlanet, "uOccN", &occN, 1);
            if (sg.nocc > 0 && (sg.mode != 2 && sg.mode != 3)) { V.setF(VkDraw2D::kPlanet, "uOcc", sg.occ, 4, sg.nocc); V.setF(VkDraw2D::kPlanet, "uSunAng", &sg.sunAng, 1); }
            const int air = (sg.mode != 2 && sg.mode != 3) && sg.air ? 1 : 0;
            V.setI(VkDraw2D::kPlanet, "uAir", &air, 1);
            V.setF(VkDraw2D::kPlanet, "uLightK", &sg.lightK, 1);                 // 0.85.0 (R1)
            {   // 0.85.0 (R2): the Sun's measured edge (the Sun's disc, uMode 4, and the eclipses), and (R3) the sunspot groups' outlines
                const SunLimb& lim = sunLimb();
                float limb[18], sc[3];
                for (int n = 0; n < 6; n++) for (int c = 0; c < 3; c++) limb[3 * n + c] = static_cast<float>(lim.poly[c][n]);
                for (int c = 0; c < 3; c++) sc[c] = static_cast<float>(lim.centre[c]);
                V.setF(VkDraw2D::kPlanet, "uLimb", limb, 3, 6);
                V.setF(VkDraw2D::kPlanet, "uSunC", sc, 3);
                const int spotN = sg.mode == 4 ? sg.spotN : 0;
                V.setI(VkDraw2D::kPlanet, "uSpotN", &spotN, 1);
                if (spotN > 0) { V.setF(VkDraw2D::kPlanet, "uSpot", sunSpots_.data() + 4 * sg.spotFirst, 4, spotN); V.setF(VkDraw2D::kPlanet, "uSpotB", sunSpotsB_.data() + 2 * sg.spotFirst, 2, spotN); }
                if (sg.mode == 4) {   // 0.94.0: the Sun's surface (render/sun_surface.h, orbit/sun_photo.h)
                    const SunSurface& s = sunSurf_;
                    const auto& U = umbraRgb();
                    const auto& G = granContrastRgb();
                    const float u3[3] = {static_cast<float>(U[0]), static_cast<float>(U[1]), static_cast<float>(U[2])};
                    const float g3[3] = {static_cast<float>(G[0]), static_cast<float>(G[1]), static_cast<float>(G[2])};
                    const float gran[4] = {static_cast<float>(s.gridPerRad), static_cast<float>(s.granF), static_cast<float>(s.cellsPerRad), s.gran ? 1.0f : 0.0f};
                    const int k = static_cast<int>(s.granK & 0x7fffffffu), photo = s.photoVersion > 0 ? 1 : 0;
                    V.setF(VkDraw2D::kPlanet, "uUmbra", u3, 3);
                    V.setF(VkDraw2D::kPlanet, "uGranC", g3, 3);
                    V.setF(VkDraw2D::kPlanet, "uGran", gran, 4);
                    V.setI(VkDraw2D::kPlanet, "uGranK", &k, 1);
                    V.setI(VkDraw2D::kPlanet, "uPhoto", &photo, 1);
                    V.setF(VkDraw2D::kPlanet, "uPhotoO", s.photoO, 3);
                    V.setF(VkDraw2D::kPlanet, "uPhotoW", s.photoWest, 3);
                    V.setF(VkDraw2D::kPlanet, "uPhotoN", s.photoNorth, 3);
                    V.setF(VkDraw2D::kPlanet, "uPhotoGeo", s.photoGeo, 3);
                    V.setF(VkDraw2D::kPlanet, "uPhotoFit", s.photoFit, 1, 6);
                }
            }
            // (a plain ball binds the white texture: the shader does not read it then; 0.94.0: the Sun binds its photo when it has one)
            const int tex = sg.mode == 4 ? (sunSurf_.photoVersion > 0 ? kSunPhotoSlot : 0) : (vkMap_[sg.map] ? sg.map : 0);
            V.draw(VkDraw2D::kPlanet, VkDraw2D::kNormal, vb, sg.first, sg.count, 0, 0, 0, tex);
            break;
        }
        case Kind::Triangles: {
            const int zero = 0;
            const int origin[2] = {0, 0};
            V.setF(VkDraw2D::kTri, "uSize", size, 2);
            V.setF(VkDraw2D::kTri, "uSpace", space, 3);
            const int maskOn = vkbh_ && maskValid_ && vkbh_->mask() ? 1 : 0;   // 0.62.0: the spinning hole's open-sky mask (maskedDisc)
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
