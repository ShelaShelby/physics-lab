// 0.68.0: the Molecule Lab's Density surface on Vulkan (render/surface_card.h): the surface kept on the card, the frame's steps (kSurfaceCS: the vertices,
// the keys, the sort) in the card batch before the picture when the view or the surface changed, the start-up check's steps read back. Built and checked
// by molecule/surface_check.cpp under gpuVkSurfaceGuard. 0.90.0: the vibrating surface made on the card (kSurfaceMakeCS) into the same buffers: its
// sources sent once for each molecule and mode, a new phase only a few numbers; its counts stay on the card (kSurfaceCS reads them, the draw is indirect).
#include "surface_card.h"

#include <algorithm>
#include <cstring>

#include "vk_card.h"
#include "r2d_surface_comp.h"
#include "r2d_surfmake_comp.h"

namespace render {

namespace {
constexpr double kA0AngstromVk = 0.529177210544;   // the Bohr radius in angstrom, CODATA 2022 (as molecule/density.h kA0Angstrom)
constexpr int kSortLevels = 24;                    // the made surface's sort: at most 24 passes' sizes (k = 1024 .. 2^33: never reached)
constexpr unsigned kIndWords = 17 + 6 * kSortLevels;   // its counts (kSurfaceMakeCS step 5): the draw, nv, nt, then kSurfaceCS's workgroups
int pow2At(int n) { int p = 512; while (p < n) p *= 2; return p; }
bool sameView(const SurfaceView& a, const SurfaceView& b) {
    return std::equal(a.R, a.R + 9, b.R) && a.px == b.px && a.py == b.py && a.cx == b.cx && a.cy == b.cy && a.s == b.s && a.potRange == b.potRange && a.pot == b.pot &&
           std::equal(a.plain, a.plain + 4, b.plain);
}
}  // namespace

class SurfaceCardVk : public SurfaceCard {
public:
    explicit SurfaceCardVk(VkCards& c) : c_(c) {}
    ~SurfaceCardVk() override {
        while (!jobs_.empty()) release(jobs_.back().id);
        while (!makeJobs_.empty()) releaseMake(makeJobs_.back().id);
        drop(frame_); dropSrc(frameSrc_); c_.dropKernel(k_); c_.dropKernel(km_);
    }
    bool build(std::string& why) override {
        if (k_.pipe) return true;
        if (forcedFail("surface")) { why = "made to fail on purpose (PL_VK_CARDFAIL=surface)"; return false; }
        return c_.makeKernel(k_r2d_surface_comp, sizeof k_r2d_surface_comp, k_, why);
    }
    bool built() const override { return k_.pipe != VK_NULL_HANDLE; }
    bool buildMake(std::string& why) override {
        if (km_.pipe) return true;
        if (forcedFail("surfacemake")) { why = "made to fail on purpose (PL_VK_CARDFAIL=surfacemake)"; return false; }
        if (c_.limits().maxPerStageDescriptorStorageBuffers < 6) { why = "this card gives a compute program fewer than 6 storage buffers"; return false; }
        return c_.makeKernel(k_r2d_surfmake_comp, sizeof k_r2d_surfmake_comp, km_, why);
    }
    bool madeBuilt() const override { return km_.pipe != VK_NULL_HANDLE; }
    long makes() const override { return makes_; }
    long runs() const override { return runs_; }
    long sends() const override { return sends_; }

    // the frame's: the surface (when new) and the steps (when the view or the surface changed) in the open batch; false: nothing to draw from the card
    bool record(const SurfaceDraw& d) {
        if (d.make) return recordMade(d);
        if (!built() || !c_.ok() || d.mesh.nv <= 0 || d.mesh.nt <= 0 || !d.mesh.p) return false;
        const bool newMesh = !have_ || frame_.made || d.mesh.rev != frame_.rev || d.mesh.nv != frame_.nv || d.mesh.nt != frame_.nt;
        if (!newMesh && sameView(d.view, last_)) return true;
        if (newMesh) {
            if (!send(frame_, d.mesh, false)) { have_ = false; return false; }
            sends_++;
        }
        if (!steps(d, frame_, VkCards::kToVertex)) { have_ = false; return false; }
        last_ = d.view; have_ = true; runs_++;
        return true;
    }
    bool buffers(VkBuffer b[4]) const { b[0] = frame_.vert.b; b[1] = frame_.tri.b; b[2] = frame_.keys.b; b[3] = VK_NULL_HANDLE; return have_ && b[0] && b[1] && b[2]; }
    VkBuffer indirect() const { return have_ && frame_.made ? frame_.ind.b : VK_NULL_HANDLE; }

    // 0.90.0: the check's and the tests' made surface: the sources, the steps, everything read back
    int beginMake(const SurfaceMake& m) override {
        if (!madeBuilt() || !c_.ok() || makeJobs_.size() >= 8) return 0;
        MakeJob j;
        j.id = nextId_++;
        if (!sendSrc(j.src, m, true) || !roomFor(j.b, j.src, true) || !makeSteps(m, j.src, j.b, VkCards::kToHost)) { dropSrc(j.src); drop(j.b); return 0; }
        j.ticket = c_.flush(false);
        if (!j.ticket) { dropSrc(j.src); drop(j.b); return 0; }
        makeJobs_.push_back(std::move(j));
        return makeJobs_.back().id;
    }
    JobState pollMake(int job, SurfaceMade& out) override {
        out = SurfaceMade();
        for (MakeJob& j : makeJobs_) {
            if (j.id != job) continue;
            if (!c_.done(j.ticket)) return JobState::Pending;
            const bool good = !c_.failed();
            if (good) {
                const std::uint32_t* ind = static_cast<const std::uint32_t*>(j.b.ind.map);
                const std::uint32_t* cnt = static_cast<const std::uint32_t*>(j.src.cnt.map);
                out.nv = long(ind[4]); out.nt = long(ind[5]);
                const float* v = static_cast<const float*>(j.b.mesh.map);
                out.verts.assign(v, v + 8 * std::size_t(out.nv));
                const std::uint32_t* t = static_cast<const std::uint32_t*>(j.b.tri.map);
                out.tri.assign(t, t + 3 * std::size_t(out.nt));
                for (int e = 0; e < j.src.ne; e++) if (cnt[e + 1] != cnt[e]) out.keys.push_back(e);   // (vertex cnt[e] is on edge e: in order of e)
            }
            releaseMake(job);
            return good ? JobState::Done : JobState::Failed;
        }
        return JobState::Failed;
    }

    int beginCheck(const SurfaceDraw& d) override {
        if (!built() || !c_.ok() || d.mesh.nv <= 0 || d.mesh.nt <= 0 || jobs_.size() >= 8) return 0;
        Job j;
        j.id = nextId_++;
        if (!send(j.b, d.mesh, true) || !steps(d, j.b, VkCards::kToHost)) { drop(j.b); return 0; }
        j.ticket = c_.flush(false);
        if (!j.ticket) { drop(j.b); return 0; }
        jobs_.push_back(std::move(j));
        return jobs_.back().id;
    }
    JobState pollCheck(int job, std::vector<float>& verts, std::vector<std::uint32_t>& order) override {
        for (Job& j : jobs_) {
            if (j.id != job) continue;
            if (!c_.done(j.ticket)) return JobState::Pending;
            const bool good = !c_.failed();
            if (good) {
                const float* v = static_cast<const float*>(j.b.vert.map);
                verts.assign(v, v + 8 * std::size_t(j.b.nv));
                const std::uint32_t* k = static_cast<const std::uint32_t*>(j.b.keys.map);
                order.resize(std::size_t(j.b.nt));
                for (int t = 0; t < j.b.nt; t++) order[std::size_t(t)] = k[2 * std::size_t(t) + 1];
            }
            release(job);
            return good ? JobState::Done : JobState::Failed;
        }
        return JobState::Failed;
    }
    void cancel(int job) override { release(job); }

private:
    struct Bufs { VkCards::Buf mesh, tri, vert, keys, ind; int nv = 0, nt = 0, P = 0, capV = 0, capT = 0; std::uint64_t rev = 0; bool made = false; };
    struct Job { int id = 0; std::uint64_t ticket = 0; Bufs b; };
    // 0.90.0: a made surface's sources (the three densities and the three potentials), the coarse ln rho, the edges' and the cubes' counts
    struct Src { VkCards::Buf src, fld, cnt; std::uint64_t key = 0; int g[4] = {0, 0, 0, 0}, ne = 0, nCube = 0; bool have = false; };
    struct MakeJob { int id = 0; std::uint64_t ticket = 0; Src src; Bufs b; };
    void drop(Bufs& b) { c_.dropBuffer(b.mesh); c_.dropBuffer(b.tri); c_.dropBuffer(b.vert); c_.dropBuffer(b.keys); c_.dropBuffer(b.ind); b = Bufs(); }
    void dropSrc(Src& s) { c_.dropBuffer(s.src); c_.dropBuffer(s.fld); c_.dropBuffer(s.cnt); s = Src(); }
    // the sources on the card (sent when the key changes); host: the counts readable (the check)
    bool sendSrc(Src& s, const SurfaceMake& m, bool host) {
        if (s.have && s.key == m.key) return true;
        dropSrc(s);
        for (int k = 0; k < 3; k++) if (!m.rho[k] || !m.pot[k] || m.n[k] < 2 || m.pn[k] < 2) return false;
        if (m.stride < 1) return false;
        const std::size_t nf = std::size_t(m.n[0]) * m.n[1] * m.n[2], np = std::size_t(m.pn[0]) * m.pn[1] * m.pn[2];
        for (int k = 0; k < 3; k++) s.g[k] = (m.n[k] - 1) / m.stride + 1;
        s.g[3] = m.stride;
        const std::size_t nc = std::size_t(s.g[0]) * s.g[1] * s.g[2];
        s.ne = static_cast<int>(8 * nc);
        s.nCube = (s.g[0] - 1) * (s.g[1] - 1) * (s.g[2] - 1);
        const VkBufferUsageFlags st = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (!c_.makeBuffer(VkDeviceSize(3 * (nf + np)) * 4u, st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, s.src, "the surface's densities") ||
            !c_.makeBuffer(VkDeviceSize(nc) * 4u, st, false, s.fld, "the surface's ln rho") ||
            !c_.makeBuffer(VkDeviceSize(std::size_t(s.ne) + std::size_t(s.nCube) + 2) * 4u, st, host, s.cnt, "the surface's edges and cubes")) { dropSrc(s); return false; }
        VkDeviceSize at = 0;
        for (int k = 0; k < 3; k++) { c_.copyIn(m.rho[k], nf * 4, s.src, at); at += VkDeviceSize(nf) * 4u; }
        for (int k = 0; k < 3; k++) { c_.copyIn(m.pot[k], np * 4, s.src, at); at += VkDeviceSize(np) * 4u; }
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        s.key = m.key; s.have = true;
        return true;
    }
    // room for a made surface: every crossed edge a vertex (at most all of them), every cube at most 12 triangles (6 tetrahedra, 2 each): it always fits
    bool roomFor(Bufs& b, const Src& s, bool host) {
        const int capV = s.ne, capT = 12 * s.nCube, P = pow2At(capT);
        if (b.mesh.b && b.ind.b && b.capV >= capV && b.capT >= P) return true;
        drop(b);
        const VkBufferUsageFlags st = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (!c_.makeBuffer(VkDeviceSize(capV) * 32u, st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, host, b.mesh, "the surface's vertices") ||
            !c_.makeBuffer(VkDeviceSize(P) * 12u, st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, host, b.tri, "the surface's triangles") ||
            !c_.makeBuffer(VkDeviceSize(capV) * 32u, st, false, b.vert, "the surface on the picture") ||
            !c_.makeBuffer(VkDeviceSize(P) * 8u, st, false, b.keys, "the surface's order") ||
            !c_.makeBuffer(4u * kIndWords, st | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, host, b.ind, "the surface's counts")) { drop(b); return false; }
        b.capV = capV; b.capT = P;
        return true;
    }
    // the seven steps into b (its mesh, triangles and counts)
    bool makeSteps(const SurfaceMake& m, const Src& s, Bufs& b, unsigned consumer) {
        VkUniforms& u = km_.u;
        const std::size_t nf = std::size_t(m.n[0]) * m.n[1] * m.n[2], np = std::size_t(m.pn[0]) * m.pn[1] * m.pn[2];
        const int G[4] = {s.g[0], s.g[1], s.g[2], s.g[3]}, F[4] = {m.n[0], m.n[1], m.n[2], static_cast<int>(nf)}, P[4] = {m.pn[0], m.pn[1], m.pn[2], static_cast<int>(np)};
        const float O[4] = {m.o[0], m.o[1], m.o[2], m.h}, PO[4] = {m.po[0], m.po[1], m.po[2], m.ph};
        const float B[4] = {m.a, m.b, m.iso, static_cast<float>(kA0AngstromVk)};
        const int cap[4] = {b.capV, 12 * s.nCube, b.capT, 0};
        u.i("uG", G, 4); u.i("uF", F, 4); u.f("uO", O, 4); u.i("uP", P, 4); u.f("uPO", PO, 4); u.f("uB", B, 4); u.i("uCap", cap, 4);
        const std::uint32_t nc = static_cast<std::uint32_t>(s.ne / 8);
        const std::uint32_t groups[7] = {(nc + 255u) / 256u, (std::uint32_t(s.ne) + 255u) / 256u, 1u, (std::uint32_t(s.ne) + 255u) / 256u,
                                         (std::uint32_t(s.nCube) + 255u) / 256u, 1u, (std::uint32_t(s.nCube) + 255u) / 256u};
        for (int step = 0; step < 7; step++) {
            u.i("uStep", &step, 1);
            if (!c_.dispatch(km_, {&s.src, &s.fld, &s.cnt, &b.mesh, &b.tri, &b.ind}, groups[step])) return false;
            c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, step == 6 ? consumer : static_cast<unsigned>(VkCards::kToCompute) | (step == 5 ? static_cast<unsigned>(VkCards::kToIndirect) : 0u));
        }
        b.made = true; b.nv = b.capV; b.nt = 12 * s.nCube; b.P = b.capT;
        return true;
    }
    // the frame's made surface: made again when the sources or the phase changed, its view's steps when that or the view changed
    bool recordMade(const SurfaceDraw& d) {
        const SurfaceMake& m = *d.make;
        if (!built() || !madeBuilt() || !c_.ok()) return false;
        const bool newSrc = !frameSrc_.have || frameSrc_.key != m.key;
        if (!sendSrc(frameSrc_, m, false) || !roomFor(frame_, frameSrc_, false)) { have_ = false; return false; }
        const bool newPhase = newSrc || !have_ || !frame_.made || m.a != lastMake_.a || m.b != lastMake_.b || m.iso != lastMake_.iso;
        if (newPhase) {
            if (!makeSteps(m, frameSrc_, frame_, VkCards::kToCompute | VkCards::kToIndirect)) { have_ = false; return false; }
            lastMake_ = m; makes_++;
        } else if (sameView(d.view, last_)) return true;
        if (!steps(d, frame_, VkCards::kToVertex | VkCards::kToIndirect)) { have_ = false; return false; }
        last_ = d.view; have_ = true; runs_++;
        return true;
    }
    void releaseMake(int id) {
        for (std::size_t i = 0; i < makeJobs_.size(); i++)
            if (makeJobs_[i].id == id) { dropSrc(makeJobs_[i].src); drop(makeJobs_[i].b); makeJobs_.erase(makeJobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
    bool send(Bufs& b, const SurfaceMesh& m, bool host) {
        const int P = pow2At(m.nt);
        if (!b.mesh.b || b.capV < m.nv || b.capT < P) {
            drop(b);
            const VkBufferUsageFlags st = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            const int capV = m.nv + m.nv / 4, capT = P;
            if (!c_.makeBuffer(VkDeviceSize(capV) * 32u, st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, b.mesh, "the surface's vertices") ||
                !c_.makeBuffer(VkDeviceSize(capT) * 12u, st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, b.tri, "the surface's triangles") ||
                !c_.makeBuffer(VkDeviceSize(capV) * 32u, st, host, b.vert, "the surface on the picture") ||
                !c_.makeBuffer(VkDeviceSize(capT) * 8u, st, host, b.keys, "the surface's order")) return false;
            b.capV = capV; b.capT = capT;
        }
        std::vector<float> v(8 * std::size_t(m.nv));
        for (int i = 0; i < m.nv; i++) {
            float* o = &v[8 * std::size_t(i)];
            for (int k = 0; k < 3; k++) { o[k] = m.p[3 * i + k]; o[4 + k] = m.nrm[3 * i + k]; }
            o[3] = m.value ? m.value[i] : 0.0f; o[7] = 0;
        }
        c_.copyIn(v.data(), v.size() * sizeof(float), b.mesh, 0);
        c_.copyIn(m.tri, std::size_t(m.nt) * 3 * sizeof(int), b.tri, 0);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute | VkCards::kToVertex);
        b.nv = m.nv; b.nt = m.nt; b.P = P; b.rev = m.rev; b.made = false;
        return true;
    }
    // the steps: the vertices, the keys, then the bitonic sort (the 512-blocks at once; then for each longer run its strides of 512 and more one at a time
    // and the rest in a block)
    bool steps(const SurfaceDraw& d, Bufs& b, unsigned consumer) {
        VkUniforms& u = k_.u;
        const SurfaceView& v = d.view;
        const float r0[4] = {v.R[0], v.R[1], v.R[2], 0}, r1[4] = {v.R[3], v.R[4], v.R[5], 0}, r2[4] = {v.R[6], v.R[7], v.R[8], 0};
        const float cam[4] = {v.px, v.py, v.cx, v.cy}, sc[4] = {v.s, v.potRange, v.pot ? 1.0f : 0.0f, 0};
        const int n[4] = {b.nv, b.nt, b.P, b.made ? 1 : 0};   // (0.90.0 made: nv and nt are the room's, the counts are read on the card)
        u.f("uR0", r0, 4); u.f("uR1", r1, 4); u.f("uR2", r2, 4); u.f("uCam", cam, 4); u.f("uS", sc, 4); u.f("uPlain", v.plain, 4); u.i("uN", n, 4);
        // (0.90.0 made: each pass's workgroups from the counts the card made, b.ind from word 8: kSurfaceMakeCS step 5; passes past its size have none)
        const auto run = [&](int step, int kk, int jj, std::uint32_t groups, int entry, unsigned to) {
            const int kj[2] = {kk, jj};
            u.i("uStep", &step, 1); u.i("uKJ", kj, 2);
            const bool ok = b.made ? c_.dispatchIndirect(k_, {&b.mesh, &b.tri, &b.vert, &b.keys, &b.ind}, b.ind, VkDeviceSize(4u * unsigned(entry)))
                                   : c_.dispatch(k_, {&b.mesh, &b.tri, &b.vert, &b.keys, &b.mesh}, groups);   // (binding 4 read only when made)
            if (!ok) return false;
            c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, to);
            return true;
        };
        const std::uint32_t gv = static_cast<std::uint32_t>((b.nv + 255) / 256), gp = static_cast<std::uint32_t>(b.P / 256), gb = static_cast<std::uint32_t>(b.P / 512);
        if (!run(0, 0, 0, gv, 8, VkCards::kToCompute) || !run(1, 0, 0, gp, 11, VkCards::kToCompute) || !run(2, 0, 0, gb, 14, VkCards::kToCompute)) return false;
        int level = 0;
        for (int k = 1024; k <= b.P; k *= 2, level++) {
            for (int j = k / 2; j >= 512; j /= 2) if (!run(3, k, j, gp, 17 + 6 * level, VkCards::kToCompute)) return false;
            if (!run(4, k, 256, gb, 20 + 6 * level, VkCards::kToCompute)) return false;
        }
        c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, consumer);
        return true;
    }
    void release(int id) {
        for (std::size_t i = 0; i < jobs_.size(); i++)
            if (jobs_[i].id == id) { drop(jobs_[i].b); jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
    VkCards& c_;
    VkCards::Kernel k_;
    VkCards::Kernel km_;                                 // 0.90.0: kSurfaceMakeCS
    Bufs frame_;
    Src frameSrc_;
    SurfaceMake lastMake_;
    std::vector<MakeJob> makeJobs_;
    long makes_ = 0;
    SurfaceView last_;
    bool have_ = false;
    long runs_ = 0, sends_ = 0;
    std::vector<Job> jobs_;
    int nextId_ = 1;
};

SurfaceCard* makeVkSurfaceCard(VkCards& c) { return new SurfaceCardVk(c); }
bool vkSurfaceRecord(SurfaceCard* c, const SurfaceDraw& d) { SurfaceCardVk* v = dynamic_cast<SurfaceCardVk*>(c); return v && v->record(d); }
bool vkSurfaceBuffers(SurfaceCard* c, VkBuffer bufs[4]) { const SurfaceCardVk* v = dynamic_cast<const SurfaceCardVk*>(c); return v && v->buffers(bufs); }
VkBuffer vkSurfaceIndirect(SurfaceCard* c) { const SurfaceCardVk* v = dynamic_cast<const SurfaceCardVk*>(c); return v ? v->indirect() : VK_NULL_HANDLE; }

}  // namespace render
