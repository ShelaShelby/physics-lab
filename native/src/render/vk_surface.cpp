// 0.68.0: the Molecule Lab's Density surface on Vulkan (render/surface_card.h): the surface kept on the card, the frame's steps (kSurfaceCS: the vertices,
// the keys, the sort) in the card batch before the picture when the view or the surface changed, the start-up check's steps read back. Built and checked
// by molecule/surface_check.cpp under gpuVkSurfaceGuard.
#include "surface_card.h"

#include <algorithm>
#include <cstring>

#include "vk_card.h"
#include "r2d_surface_comp.h"

namespace render {

namespace {
int pow2At(int n) { int p = 512; while (p < n) p *= 2; return p; }
bool sameView(const SurfaceView& a, const SurfaceView& b) {
    return std::equal(a.R, a.R + 9, b.R) && a.px == b.px && a.py == b.py && a.cx == b.cx && a.cy == b.cy && a.s == b.s && a.potRange == b.potRange && a.pot == b.pot &&
           std::equal(a.plain, a.plain + 4, b.plain);
}
}  // namespace

class SurfaceCardVk : public SurfaceCard {
public:
    explicit SurfaceCardVk(VkCards& c) : c_(c) {}
    ~SurfaceCardVk() override { while (!jobs_.empty()) release(jobs_.back().id); drop(frame_); c_.dropKernel(k_); }
    bool build(std::string& why) override {
        if (k_.pipe) return true;
        if (forcedFail("surface")) { why = "made to fail on purpose (PL_VK_CARDFAIL=surface)"; return false; }
        return c_.makeKernel(k_r2d_surface_comp, sizeof k_r2d_surface_comp, k_, why);
    }
    bool built() const override { return k_.pipe != VK_NULL_HANDLE; }
    long runs() const override { return runs_; }
    long sends() const override { return sends_; }

    // the frame's: the surface (when new) and the steps (when the view or the surface changed) in the open batch; false: nothing to draw from the card
    bool record(const SurfaceDraw& d) {
        if (!built() || !c_.ok() || d.mesh.nv <= 0 || d.mesh.nt <= 0 || !d.mesh.p) return false;
        const bool newMesh = !have_ || d.mesh.rev != frame_.rev || d.mesh.nv != frame_.nv || d.mesh.nt != frame_.nt;
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
    struct Bufs { VkCards::Buf mesh, tri, vert, keys; int nv = 0, nt = 0, P = 0, capV = 0, capT = 0; std::uint64_t rev = 0; };
    struct Job { int id = 0; std::uint64_t ticket = 0; Bufs b; };
    void drop(Bufs& b) { c_.dropBuffer(b.mesh); c_.dropBuffer(b.tri); c_.dropBuffer(b.vert); c_.dropBuffer(b.keys); b = Bufs(); }
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
        b.nv = m.nv; b.nt = m.nt; b.P = P; b.rev = m.rev;
        return true;
    }
    // the steps: the vertices, the keys, then the bitonic sort (the 512-blocks at once; then for each longer run its strides of 512 and more one at a time
    // and the rest in a block)
    bool steps(const SurfaceDraw& d, Bufs& b, unsigned consumer) {
        VkUniforms& u = k_.u;
        const SurfaceView& v = d.view;
        const float r0[4] = {v.R[0], v.R[1], v.R[2], 0}, r1[4] = {v.R[3], v.R[4], v.R[5], 0}, r2[4] = {v.R[6], v.R[7], v.R[8], 0};
        const float cam[4] = {v.px, v.py, v.cx, v.cy}, sc[4] = {v.s, v.potRange, v.pot ? 1.0f : 0.0f, 0};
        const int n[4] = {b.nv, b.nt, b.P, 0};
        u.f("uR0", r0, 4); u.f("uR1", r1, 4); u.f("uR2", r2, 4); u.f("uCam", cam, 4); u.f("uS", sc, 4); u.f("uPlain", v.plain, 4); u.i("uN", n, 4);
        const auto run = [&](int step, int kk, int jj, std::uint32_t groups, unsigned to) {
            const int kj[2] = {kk, jj};
            u.i("uStep", &step, 1); u.i("uKJ", kj, 2);
            if (!c_.dispatch(k_, {&b.mesh, &b.tri, &b.vert, &b.keys}, groups)) return false;
            c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, to);
            return true;
        };
        const std::uint32_t gv = static_cast<std::uint32_t>((b.nv + 255) / 256), gp = static_cast<std::uint32_t>(b.P / 256), gb = static_cast<std::uint32_t>(b.P / 512);
        if (!run(0, 0, 0, gv, VkCards::kToCompute) || !run(1, 0, 0, gp, VkCards::kToCompute) || !run(2, 0, 0, gb, VkCards::kToCompute)) return false;
        for (int k = 1024; k <= b.P; k *= 2) {
            for (int j = k / 2; j >= 512; j /= 2) if (!run(3, k, j, gp, VkCards::kToCompute)) return false;
            if (!run(4, k, 256, gb, VkCards::kToCompute)) return false;
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
    Bufs frame_;
    SurfaceView last_;
    bool have_ = false;
    long runs_ = 0, sends_ = 0;
    std::vector<Job> jobs_;
    int nextId_ = 1;
};

SurfaceCard* makeVkSurfaceCard(VkCards& c) { return new SurfaceCardVk(c); }
bool vkSurfaceRecord(SurfaceCard* c, const SurfaceDraw& d) { SurfaceCardVk* v = dynamic_cast<SurfaceCardVk*>(c); return v && v->record(d); }
bool vkSurfaceBuffers(SurfaceCard* c, VkBuffer bufs[4]) { const SurfaceCardVk* v = dynamic_cast<const SurfaceCardVk*>(c); return v && v->buffers(bufs); }

}  // namespace render
