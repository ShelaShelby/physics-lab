#include "renderer2d.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <thread>

#include "group_compute.h"
#include "outline_compute.h"
#include "rock_compute.h"
#include "shaders.h"
#include "shading.h"


namespace render {

Color rgba(int r, int g, int b, float a) { return {r / 255.f, g / 255.f, b / 255.f, a}; }

Color hexColor(const char* hex, float alpha) {
    if (!hex || hex[0] != '#') return {1, 1, 1, alpha};
    const unsigned long v = std::strtoul(hex + 1, nullptr, 16);
    return rgba(static_cast<int>((v >> 16) & 255), static_cast<int>((v >> 8) & 255), static_cast<int>(v & 255), alpha);
}


// 0.60.0 (tests, PL_LOG_DRAWKINDS): which drawing calls the views make (the coverage of the Vulkan checks); nothing is counted otherwise
static bool logKinds() { static const bool on = std::getenv("PL_LOG_DRAWKINDS") != nullptr; return on; }
#define PL_KIND(k) do { if (logKinds()) kinds_[k]++; } while (0)

void Renderer2D::disableCompute() { noCompute_ = true; delete compute_; compute_ = nullptr; delete outline_; outline_ = nullptr; delete rocks_; rocks_ = nullptr; delete kerr_; kerr_ = nullptr; kerrTried_ = true; kerrWhy_ = "switched off (PL_NO_COMPUTE)"; }

// 0.55.6: made at the first call (0.69.0: Vulkan only: render/renderer2d_vk.cpp)
IKerrCompute* Renderer2D::kerrCompute(KerrSteps* steps) {
    return vk_ && vkKerr_ ? vkKerr_(*this, steps) : nullptr;      // 0.62.0: Vulkan's
}

void Renderer2D::begin(double w, double h) {
    keep_ = false;
    width_ = w > 1 ? w : 1;
    height_ = h > 1 ? h : 1;
    verts_.clear();
    inst_.clear();
    blobs_.clear();
    clouds_.clear();
    molClouds_.clear();
    ringDraws_.clear();
    classicDraws_.clear();
    nucDraws_.clear();
    linesDraws_.clear();
    surfaceDraws_.clear();
    lens_.clear();
    segments_.clear();
    sunSpots_.clear();
    triStart_ = 0;
}

void Renderer2D::lensSky(const LensSky& L) {
    if (!hasMap(Map::Sky) || L.kRad <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Lens; sg.slot = static_cast<int>(lens_.size());
    lens_.push_back(L);
    segments_.push_back(sg);
}

bool Renderer2D::lensSameView(const LensSky& P, const LensSky& L, bool ksky, bool skyDrawn, double width, double lastPtW) {
    return P.cx == L.cx && P.cy == L.cy && P.kRad == L.kRad && std::equal(L.M, L.M + 9, P.M) && std::equal(L.fido, L.fido + 2, P.fido) && P.bright == L.bright && P.bend == L.bend &&
                      P.alphaSh == L.alphaSh && P.s0 == L.s0 && P.ds == L.ds && P.tableVersion == L.tableVersion && width == lastPtW &&
                      P.sky == L.sky && P.disc == L.disc && (!L.disc || (P.discVersion == L.discVersion && P.colVersion == L.colVersion && std::equal(L.discAxis, L.discAxis + 3, P.discAxis) &&
                      P.rO == L.rO && P.rIn == L.rIn && P.rOut == L.rOut && P.tScale == L.tScale && P.white == L.white &&
                      std::equal(L.discLight, L.discLight + 4, P.discLight) && P.retVersion == L.retVersion)) &&
                      P.kerr == L.kerr && (!L.kerr || (P.kVersion == L.kVersion && std::equal(L.kM, L.kM + 9, P.kM) && std::equal(L.kO, L.kO + 4, P.kO) && P.spin == L.spin &&
                      P.discFVersion == L.discFVersion)) && std::equal(L.discFix, L.discFix + 4, P.discFix) &&
                      P.glareKeep == L.glareKeep && P.glareVeil == L.glareVeil &&
                      P.kerrSky == L.kerrSky && (!L.kerrSky || (std::equal(L.skyCam, L.skyCam + 4, P.skyCam) && std::equal(L.skyMet, L.skyMet + 4, P.skyMet) &&
                      std::equal(L.skyMet2, L.skyMet2 + 4, P.skyMet2) && std::equal(L.filmRaw, L.filmRaw + 4, P.filmRaw) && std::equal(L.kM, L.kM + 9, P.kM) &&
                      std::equal(L.kO, L.kO + 4, P.kO) && P.whiteMode == L.whiteMode && P.expStep == L.expStep && P.moving == L.moving && P.spin == L.spin &&
                      P.discFVersion == L.discFVersion && ksky == skyDrawn)) && (L.kerrSky || !skyDrawn) && P.whiteMode == L.whiteMode && P.expStep == L.expStep &&
                      P.gas == L.gas && (!L.gas || P.gasVer == L.gasVer);   // 0.72.0: the moving gas: a new picture whenever its time or settings change
}

void Renderer2D::skyProbe(const LensSky& L, int w, int h, bool queue, std::vector<float>* out) {
    if (vk_ && vkProbe_) vkProbe_(*this, L, w, h, queue, out);              // 0.62.0: Vulkan's (render/vk_blackhole.cpp)
    else if (out) out->clear();
}

bool Renderer2D::skyProbeDone() { return vk_ && vkProbeDone_ && vkProbeDone_(*this); }

void Renderer2D::skyProbeRead(std::vector<float>& out) {
    out.clear();
    if (vk_ && vkProbeRead_) vkProbeRead_(*this, out);
}

void Renderer2D::maskedDisc(double x, double y, double r, Color c) {
    PL_KIND(kdMaskedDisc);
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 6, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::closeTriangles() {
    if (verts_.size() > triStart_) {
        Segment sg{};
        sg.kind = Kind::Triangles; sg.first = static_cast<int>(triStart_); sg.count = static_cast<int>(verts_.size() - triStart_);
        segments_.push_back(sg);
    }
    triStart_ = verts_.size();
}

void Renderer2D::planet(double x, double y, double r, Color base, double lx, double ly, double lz, const float K[9], Map map, const float* rings, int nRings, double shapeY, double shapeZ, bool lunar,
                        const PlanetLight* light) {
    PL_KIND(kdPlanet); if (logKinds()) { if (map != Map::None && map != Map::Sky && hasMap(map)) kinds_[kdPlanetMap]++; if (rings && nRings > 0) kinds_[kdPlanetRings]++; if (light && light->nOcc > 0) kinds_[kdPlanetEclipse]++; if (light && light->air) kinds_[kdPlanetAir]++; }
    if (r <= 0) return;
    closeTriangles();
    const double half = r + 1.5;
    quad(x, y, half, base, 2, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(half / r), static_cast<float>(lz));
    Segment sg{};
    const bool has = map != Map::None && map != Map::Sky && hasMap(map);
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = has ? static_cast<int>(map) : 0; sg.mode = has ? 1 : 0; sg.r = static_cast<float>(r);
    for (int i = 0; i < 9; i++) sg.K[i] = K[i];
    sg.nring = rings ? std::min(nRings, kMaxRingBands) : 0;
    for (int i = 0; i < 3 * sg.nring; i++) sg.ring[i] = rings[i];
    sg.lunar = lunar;
    if (has) { const MapAlbedo* ma = mapAlbedo(map); sg.albedoK = ma ? static_cast<float>(ma->factor) : 1.0f; }   // 0.77.0: the map's measured brightness
    if (light) {
        sg.nocc = std::clamp(light->nOcc, 0, 4);
        for (int i = 0; i < 4 * sg.nocc; i++) sg.occ[i] = light->occ[i];
        sg.sunAng = light->sunAng; sg.air = light->air; sg.lightK = light->k;
    }
    static const bool logPlanet = std::getenv("PL_LOG_PLANET") != nullptr;   // 0.77.0, for the GPU-vs-CPU picture check only (tests/planet_picture.cpp reads these fields)
    if (logPlanet && has)
        std::printf("PLPLANET %d %.4f %.4f %.4f %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %.7g %d %.7g %d %d %d %.6f %.6f %.6f %.9g\n", static_cast<int>(map), x, y, r,
                    lx, ly, lz, K[0], K[1], K[2], K[3], K[4], K[5], K[6], K[7], K[8], shapeY, shapeZ, lunar ? 1 : 0, double(sg.albedoK), sg.nring, sg.nocc, sg.air ? 1 : 0,
                    double(space_.r), double(space_.g), double(space_.b), double(sg.lightK));   // (0.85.0: + the sunlight factor)
    if (logPlanet && has) {   // then: the Sun's angular radius, the ring bands (3 each), the eclipsing bodies (4 each)
        std::printf("PLPLANETX %.9g", double(sg.sunAng));
        for (int i = 0; i < 3 * sg.nring; i++) std::printf(" %.9g", double(sg.ring[i]));
        for (int i = 0; i < 4 * sg.nocc; i++) std::printf(" %.9g", double(sg.occ[i]));
        std::printf("\n");
    }
    sg.shape[0] = static_cast<float>(shapeY); sg.shape[1] = static_cast<float>(shapeZ);   // (the quad of half side r + 1.5 holds it: no radius is over r)
    segments_.push_back(sg);
    triStart_ = verts_.size();
}

void Renderer2D::skyMap(double W, double H, double kRad, const float M[9], double bright) {
    PL_KIND(kdSkyMap);
    if (!hasMap(Map::Sky) || bright <= 0 || kRad <= 0) return;
    closeTriangles();
    const float u0 = static_cast<float>(-W / 2 / kRad), u1 = static_cast<float>(W / 2 / kRad), v0 = static_cast<float>(-H / 2 / kRad), v1 = static_cast<float>(H / 2 / kRad);
    const Color c{1, 1, 1, static_cast<float>(std::min(1.0, bright))};
    push(0, 0, u0, v0, c, 2); push(W, 0, u1, v0, c, 2); push(W, H, u1, v1, c, 2);
    push(0, 0, u0, v0, c, 2); push(W, H, u1, v1, c, 2); push(0, H, u0, v1, c, 2);
    Segment sg{};
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = static_cast<int>(Map::Sky); sg.mode = 2; sg.r = static_cast<float>(kRad);
    for (int i = 0; i < 9; i++) sg.K[i] = M[i];
    segments_.push_back(sg);
    triStart_ = verts_.size();
}

void Renderer2D::skyMapPinhole(double W, double H, double ppx, double ppy, double F, const float M[9], double bright) {
    PL_KIND(kdSkyMap);
    if (!hasMap(Map::Sky) || bright <= 0 || F <= 0) return;
    closeTriangles();
    const float u0 = static_cast<float>(-ppx / F), u1 = static_cast<float>((W - ppx) / F), v0 = static_cast<float>(-ppy / F), v1 = static_cast<float>((H - ppy) / F);
    const Color c{1, 1, 1, static_cast<float>(std::min(1.0, bright))};
    push(0, 0, u0, v0, c, 2); push(W, 0, u1, v0, c, 2); push(W, H, u1, v1, c, 2);
    push(0, 0, u0, v0, c, 2); push(W, H, u1, v1, c, 2); push(0, H, u0, v1, c, 2);
    Segment sg{};
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = static_cast<int>(Map::Sky); sg.mode = 3; sg.r = static_cast<float>(F);   // (mode 3: kPlanetFS's pinhole sky)
    for (int i = 0; i < 9; i++) sg.K[i] = M[i];
    segments_.push_back(sg);
    triStart_ = verts_.size();
}

void Renderer2D::squares(const float* xy, int count, double size, Color c) {
    PL_KIND(kdSquares);
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Squares; sg.first = static_cast<int>(inst_.size() / 2); sg.count = count; sg.size = static_cast<float>(size); sg.c = c;
    segments_.push_back(sg);
    inst_.insert(inst_.end(), xy, xy + 2 * count);
}

int Renderer2D::meshCreate(const std::vector<MeshVertex>& v) {
    int id = -1;
    for (int i = 0; i < int(meshes_.size()); i++) if (!meshes_[std::size_t(i)].used) { id = i; break; }
    if (id < 0) { id = int(meshes_.size()); meshes_.emplace_back(); }
    Mesh& m = meshes_[std::size_t(id)];
    m.used = true; m.count = static_cast<int>(v.size());
    m.vk = vk_ && vkMeshCreate_ ? vkMeshCreate_(vk_, v.data(), v.size() * sizeof(MeshVertex)) : -1;   // 0.60.0: a buffer on the card made once (render/vk_draw2d.cpp)
    if (m.vk < 0) { m = Mesh(); return -1; }
    meshUploads_ += static_cast<long>(v.size());
    return id;
}

void Renderer2D::meshDelete(int id) {
    if (id < 0 || id >= int(meshes_.size()) || !meshes_[std::size_t(id)].used) return;
    Mesh& m = meshes_[std::size_t(id)];
    if (m.vk >= 0 && vkMeshDelete_) vkMeshDelete_(vk_, m.vk);                 // 0.60.0
    m = Mesh();
}

int Renderer2D::meshesAlive() const { int n = 0; for (const Mesh& m : meshes_) n += m.used; return n; }

void Renderer2D::drawMesh(int id, int first, int count, const MeshProjection& P) {
    PL_KIND(kdMesh);
    if (id < 0 || id >= int(meshes_.size()) || !meshes_[std::size_t(id)].used || count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Mesh; sg.slot = id; sg.first = first; sg.count = count; sg.mp = P;
    segments_.push_back(sg);
}

bool Renderer2D::rocksGpu(const RockSet& set, const RockFrame& f, double size, Color c) {
    if (!rocks_ || !rocks_->ok() || set.count <= 0) return false;
    const int slot = rocks_->slotFor(set);
    if (slot < 0) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::RocksGpu; sg.first = 0; sg.count = set.count; sg.size = static_cast<float>(size); sg.c = c; sg.slot = slot; sg.rf = f;
    segments_.push_back(sg);
    return true;
}

bool Renderer2D::grainsGpu(int slot, int count, const RockFrame& f, double size, Color c) {
    if (!rocks_ || !rocks_->ok() || !rocks_->grainsOk() || slot < 0 || count <= 0) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::RocksGpu; sg.first = 0; sg.count = count; sg.size = static_cast<float>(size); sg.c = c; sg.slot = slot; sg.rf = f; sg.grains = true;
    segments_.push_back(sg);
    return true;
}

void Renderer2D::blobs(const Blob* b, int count, bool additive) { blobsPalette(b, count, additive, PhasePalette()); }

void Renderer2D::blobsPalette(const Blob* b, int count, bool additive, const PhasePalette& pal) {
    PL_KIND(kdBlobs); if (logKinds()) for (int i = 0; i < count; i++) kindOf(b[i]);
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.additive = additive; sg.pal = pal;
    segments_.push_back(sg);
    blobs_.insert(blobs_.end(), b, b + count);
}

void Renderer2D::glowStacks(const Blob* b, int count, bool additive, const float* table, int n) {
    PL_KIND(kdBlobs); if (logKinds()) for (int i = 0; i < count; i++) kindOf(b[i]);
    if (count <= 0 || n <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.additive = additive;
    sg.glowN = std::min(n, kMaxGlow);
    std::copy(table, table + 4 * sg.glowN, sg.glow);
    segments_.push_back(sg);
    blobs_.insert(blobs_.end(), b, b + count);
}

void Renderer2D::spheres(const Blob* b, int count) {
    PL_KIND(kdSpheres);
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.spheres = true;
    segments_.push_back(sg);
    blobs_.insert(blobs_.end(), b, b + count);
}

void Renderer2D::blobsPaletteTake(std::vector<Blob>& b, bool additive, const PhasePalette& pal) {
    PL_KIND(kdBlobs); if (logKinds()) for (const Blob& x : b) kindOf(x);
    if (b.empty()) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = static_cast<int>(b.size()); sg.additive = additive; sg.pal = pal;
    segments_.push_back(sg);
    if (blobs_.empty()) blobs_.swap(b);
    else blobs_.insert(blobs_.end(), b.begin(), b.end());
    b.clear();
}

PorkchopCard& Renderer2D::porkchopCard() {
    if (!porkCard_) porkCard_ = vk_ && vkc_ && vkPorkCard_ ? vkPorkCard_(*vkc_) : new PorkchopCardNone();   // 0.92.0
    return *porkCard_;
}

AtomCard& Renderer2D::atomCard() {
    if (!atomCard_) atomCard_ = vk_ && vkc_ && vkAtomCard_ ? vkAtomCard_(*vkc_) : new AtomCardNone();   // 0.61.0: Vulkan's
    return *atomCard_;
}

MoleculeCard& Renderer2D::moleculeCard() {
    if (!molCard_) molCard_ = vk_ && vkc_ && vkMolCard_ ? vkMolCard_(*vkc_) : new MoleculeCardNone();   // 0.61.0: Vulkan's
    return *molCard_;
}

RingCard* Renderer2D::ringCard() {   // 0.64.0
    if (!ringCard_ && vk_ && vkc_ && vkRingCard_) ringCard_ = vkRingCard_(*vkc_);
    return ringCard_;
}

bool Renderer2D::ringsCard(const RingsDraw& d) {   // 0.64.0: a box over the rings, each pixel worked out by kRingsFS
    if (!ringCard_ || !ringCard_->ready || d.bands.size() < 4 || !(d.x1 > d.x0) || !(d.y1 > d.y0)) return false;
    closeTriangles();
    const Color c{1, 1, 1, 1};
    const auto corner = [&](float x, float y) { push(x, y, x, y, c, 0); };   // (the point itself as vUV)
    corner(d.x0, d.y0); corner(d.x1, d.y0); corner(d.x1, d.y1);
    corner(d.x0, d.y0); corner(d.x1, d.y1); corner(d.x0, d.y1);
    Segment sg{};
    sg.kind = Kind::Rings; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.slot = static_cast<int>(ringDraws_.size());
    segments_.push_back(sg);
    ringDraws_.push_back(d);
    triStart_ = verts_.size();
    return true;
}

ClassicCard* Renderer2D::classicCard() {   // 0.64.0
    if (!classicCard_ && vk_ && vkc_ && vkClassicCard_) classicCard_ = vkClassicCard_(*vkc_);
    return classicCard_;
}

bool Renderer2D::cloudClassic(const ClassicDraw& d) {   // 0.64.0
    if (!classicCard_ || !classicCard_->ready || d.cnt <= 0 || !d.pts || !d.grp) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::CloudClassic; sg.slot = static_cast<int>(classicDraws_.size()); sg.count = static_cast<int>(d.cnt);
    segments_.push_back(sg);
    classicDraws_.push_back(d);
    return true;
}

NucDensCard* Renderer2D::nucDensCard() {   // 0.67.0
    if (!nucDensCard_ && vk_ && vkc_ && vkNucDensCard_) nucDensCard_ = vkNucDensCard_(*vkc_);
    return nucDensCard_;
}

long Renderer2D::nucDensRuns() const { return nucDensCard_ ? nucDensCard_->runs() : 0; }   // 0.67.0

bool Renderer2D::nucDensity(const NucDensDraw& d) {   // 0.67.0
    if (!nucDensCard_ || !nucDensCard_->ready || d.G <= 0 || !d.table || d.tableN == 0) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::NucDens; sg.slot = static_cast<int>(nucDraws_.size()); sg.count = d.G * d.G;
    segments_.push_back(sg);
    nucDraws_.push_back(d);
    return true;
}

LinesCard* Renderer2D::linesCard() {   // 0.68.0
    if (!linesCard_ && vk_ && vkc_ && vkLinesCard_) linesCard_ = vkLinesCard_(*vkc_);
    return linesCard_;
}

bool Renderer2D::lines(const LinesDraw& d) {   // 0.68.0
    if (!linesCard_ || !linesCard_->ready || d.pieces <= 0 || d.runs.empty()) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Lines; sg.slot = static_cast<int>(linesDraws_.size()); sg.count = d.pieces;
    segments_.push_back(sg);
    linesDraws_.push_back(d);
    return true;
}

// 0.68.0: the drawing programs of the lines and the surface (kLine, kSurf: render/vk_draw2d.h), made by their guards' pipeline steps with the card's steps
bool Renderer2D::linesProgram(std::string& why) {
    if (!vk_ || !vkAddProgram_) { why = "no Vulkan"; return false; }
    return vkAddProgram_(vk_, 10, why);                  // (VkDraw2D::kLine)
}
bool Renderer2D::surfaceProgram(std::string& why) {
    if (!vk_ || !vkAddProgram_) { why = "no Vulkan"; return false; }
    return vkAddProgram_(vk_, 11, why);                  // (VkDraw2D::kSurf)
}

SurfaceCard* Renderer2D::surfaceCard() {   // 0.68.0
    if (!surfaceCard_ && vk_ && vkc_ && vkSurfaceCard_) surfaceCard_ = vkSurfaceCard_(*vkc_);
    return surfaceCard_;
}

bool Renderer2D::surface(const SurfaceDraw& d) {   // 0.68.0
    if (!surfaceCard_ || !surfaceCard_->ready) return false;
    if (d.make ? !surfaceCard_->makeReady : (d.mesh.nv <= 0 || d.mesh.nt <= 0)) return false;   // (0.90.0 make: the card makes the surface, its counts stay there)
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Surface; sg.slot = static_cast<int>(surfaceDraws_.size()); sg.count = d.make ? 0 : d.mesh.nt;
    segments_.push_back(sg);
    surfaceDraws_.push_back(d);
    return true;
}

bool Renderer2D::molCloud(const MolCloudDraw& d) {
    if (!molCard_ || !molCard_->ready() || d.cnt <= 0 || !d.pts) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::MolCloud; sg.slot = static_cast<int>(molClouds_.size()); sg.count = static_cast<int>(d.cnt); sg.additive = d.additive;
    segments_.push_back(sg);
    molClouds_.push_back(d);
    return true;
}

void Renderer2D::cloudGpu(const CloudGpuDraw& d) {
    if (!cloudGpuOk() || d.cnt <= 0 || !d.pts || (!d.grp && !d.grpBuf)) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::CloudGpu; sg.slot = static_cast<int>(clouds_.size()); sg.count = static_cast<int>(d.cnt); sg.additive = d.additive; sg.pal = d.pal;
    segments_.push_back(sg);
    clouds_.push_back(d);
}

void Renderer2D::beginSoftLayer(float scale) {
    PL_KIND(kdSoftLayer);
    closeTriangles();
    layerScale_ = scale;
    Segment sg{};
    sg.kind = Kind::LayerBegin;
    segments_.push_back(sg);
}

void Renderer2D::beginCloudLayer() {
    PL_KIND(kdCloudLayer);
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerBegin; sg.hdr = true;
    segments_.push_back(sg);
}

void Renderer2D::endCloudLayer(bool additive) {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerEnd; sg.hdr = true; sg.additive = additive;
    segments_.push_back(sg);
}

void Renderer2D::endSoftLayer() {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerEnd;
    segments_.push_back(sg);
}

// the fuzzy nucleon's profile: a proton's charge density falls off as exp(−r/r0), r0 = rms radius/√12 (a neutron has no net charge: for it this is only a size profile);
// seen from the side that is the column density x·K1(x), x = b/r0. Tabulated out to 1.9× the rms radius.
static const float* fuzzTable() {
    static float t[17];
    static bool done = false;
    if (!done) {
        auto K1 = [](double x) { double sum = 0; for (double u = 0; u < 12; u += 0.02) sum += std::exp(-x * std::cosh(u)) * std::cosh(u) * 0.02; return sum; };
        for (int i = 0; i < 17; i++) { const double x = i / 16.0 * 1.9 / (1 / std::sqrt(12.0)); t[i] = i == 0 ? 1.f : static_cast<float>(std::min(1.0, x * K1(x))); }
        done = true;
    }
    return t;
}

const float* fuzzTableVk() { return fuzzTable(); }   // 0.60.0 (render/renderer2d_vk.cpp: the same table)
bool logDrawKinds() { return logKinds(); }

float Renderer2D::fuzzAt(float d) {
    if (!(d < 1.f)) return 0.f;
    const float* t = fuzzTable();
    const float x = std::max(0.f, d) * 16.f;
    const int i = std::min(15, int(x));
    return t[i] + (t[i + 1] - t[i]) * (x - float(i));
}

void Renderer2D::push(double x, double y, float u, float v, Color c, float mode, float p1, float p2, float p3) {
    verts_.push_back({static_cast<float>(x), static_cast<float>(y), u, v, c.r, c.g, c.b, c.a, mode, p1, p2, p3});
}

void Renderer2D::rect(double x, double y, double w, double h, Color c) {
    PL_KIND(kdRect);
    push(x, y, 0, 0, c, 0); push(x + w, y, 0, 0, c, 0); push(x + w, y + h, 0, 0, c, 0);
    push(x, y, 0, 0, c, 0); push(x + w, y + h, 0, 0, c, 0); push(x, y + h, 0, 0, c, 0);
}

void Renderer2D::triangle(double x0, double y0, Color c0, double x1, double y1, Color c1, double x2, double y2, Color c2) {
    PL_KIND(kdTriangle);
    push(x0, y0, 0, 0, c0, 0); push(x1, y1, 0, 0, c1, 0); push(x2, y2, 0, 0, c2, 0);
}

void Renderer2D::line(double x0, double y0, double x1, double y1, double width, Color c) {
    PL_KIND(kdLine);
    const double dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);   // (0.66.0: not std::hypot, which guards against overflow at 1e154 points: 13,000 lines a frame in orbit.heaviest)
    if (len < 1e-9) return;
    // thin lines are drawn 1 point wide with lower alpha, so they do not flicker
    double w = width;
    if (w < 1) { c.a *= static_cast<float>(w); w = 1; }
    const double nx = -dy / len * w / 2, ny = dx / len * w / 2;
    push(x0 + nx, y0 + ny, 0, 0, c, 0); push(x1 + nx, y1 + ny, 0, 0, c, 0); push(x1 - nx, y1 - ny, 0, 0, c, 0);
    push(x0 + nx, y0 + ny, 0, 0, c, 0); push(x1 - nx, y1 - ny, 0, 0, c, 0); push(x0 - nx, y0 - ny, 0, 0, c, 0);
}

void Renderer2D::quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float s, float p3) {
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2, p3); push(cx + half, cy - half, s, -s, c, mode, p1, p2, p3); push(cx + half, cy + half, s, s, c, mode, p1, p2, p3);
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2, p3); push(cx + half, cy + half, s, s, c, mode, p1, p2, p3); push(cx - half, cy + half, -s, s, c, mode, p1, p2, p3);
}

void Renderer2D::disc(double x, double y, double r, Color c) {
    PL_KIND(kdDisc);
    if (r <= 0) return;
    const double half = r + 1.5;                          // a little room for the smooth edge
    quad(x, y, half, c, 1, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::sphere(double x, double y, double r, Color c, double lx, double ly, double lz, double lightK) {
    PL_KIND(kdSphere);
    if (r <= 0) return;
    const double half = r + 1.5;
    // 0.85.0: the light's direction carries the sunlight factor as its length (kFragmentShader mode 2)
    const double ll = std::sqrt(lx * lx + ly * ly + lz * lz), k = ll > 0 ? lightK / ll : 0;
    quad(x, y, half, c, 2, static_cast<float>(lx * k), static_cast<float>(ly * k), static_cast<float>(half / r), static_cast<float>(lz * k));
}

void Renderer2D::sunBall(double x, double y, double r, const float* K, const float* spots, int nSpots) {
    PL_KIND(kdSunDisc);
    if (r <= 0) return;
    closeTriangles();
    const double half = r + 1.5;
    quad(x, y, half, Color{1, 1, 1, 1}, 2, 0, 0, static_cast<float>(half / r), 1);
    Segment sg{};
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = 0; sg.mode = 4; sg.r = static_cast<float>(r);
    static const float I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (int i = 0; i < 9; i++) sg.K[i] = K ? K[i] : I[i];
    sg.spotN = spots ? std::clamp(nSpots, 0, kMaxSunSpots) : 0;
    sg.spotFirst = static_cast<int>(sunSpots_.size());
    sunSpots_.insert(sunSpots_.end(), spots, spots + 4 * sg.spotN);
    segments_.push_back(sg);
    triStart_ = verts_.size();
    static const bool logPlanet = std::getenv("PL_LOG_PLANET") != nullptr;   // for the GPU-vs-CPU picture check (tests/planet_picture.cpp sun)
    if (logPlanet) {
        std::printf("PLSUN %.4f %.4f %.4f", x, y, r);
        for (int i = 0; i < 9; i++) std::printf(" %.7g", double(sg.K[i]));
        std::printf(" %d", sg.spotN);
        for (int i = 0; i < 4 * sg.spotN; i++) std::printf(" %.9g", double(spots[i]));
        std::printf("\n");
    }
}

void Renderer2D::sunDisc(double x, double y, double r, Color c) {
    PL_KIND(kdSunDisc);
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 3, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::fadeDisc(double x, double y, double r, Color c) {
    PL_KIND(kdFadeDisc);
    if (r <= 0) return;
    quad(x, y, r, c, 5, 0, 0, 1);
}

void Renderer2D::ring(double x, double y, double r, double width, Color c) {
    PL_KIND(kdRing);
    const int n = static_cast<int>(std::fmin(720, std::fmax(24, r * 0.75)));
    double px = x + r, py = y;
    for (int i = 1; i <= n; i++) {
        const double t = i * 6.283185307179586 / n, qx = x + r * std::cos(t), qy = y + r * std::sin(t);
        line(px, py, qx, qy, width, c);
        px = qx; py = qy;
    }
}

}  // namespace render
