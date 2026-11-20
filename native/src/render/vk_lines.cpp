// 0.68.0: the Orbit Lab's lines on Vulkan (render/lines_card.h): the points' store kept on the card, the frame's steps (kLinesCS steps 0 and 1) in the
// card batch before the picture, the start-up check's steps 0 to 2 read back. Built and checked by orbit/lines_check.cpp under gpuVkLinesGuard.
#include "lines_card.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

#include "vk_card.h"
#include "r2d_lines_comp.h"

namespace render {

bool forcedFail(const char* name);   // render/vk_card.cpp (PL_VK_CARDFAIL)

namespace {

constexpr VkDeviceSize kPointBytes = 32;   // a point: its high floats (vec4) and its low floats (vec4)
constexpr double kTwoPi = 6.283185307179586476925286766559;

void hiLo(const double* xyz, float* out8) {   // the double as a high float and the float of what is left
    for (int k = 0; k < 3; k++) {
        const float h = static_cast<float>(xyz[k]);
        out8[k] = h;
        out8[4 + k] = static_cast<float>(xyz[k] - static_cast<double>(h));
    }
    out8[3] = 0; out8[7] = 0;
}

std::uint32_t pow2At(std::uint32_t n) { std::uint32_t c = 64; while (c < n) c *= 2; return c; }

}  // namespace

class LinesCardVk : public LinesCard {
public:
    explicit LinesCardVk(VkCards& c) : c_(c) {}
    ~LinesCardVk() override {
        while (!jobs_.empty()) release(jobs_.back().id);
        for (Seg& s : segs_) dropSeg(s);
        c_.dropBuffer(store_);
        c_.dropKernel(k_);
    }
    bool build(std::string& why) override {
        if (k_.pipe) return true;
        if (forcedFail("lines")) { why = "made to fail on purpose (PL_VK_CARDFAIL=lines)"; return false; }
        return c_.makeKernel(k_r2d_lines_comp, sizeof k_r2d_lines_comp, k_, why);
    }
    bool built() const override { return k_.pipe != VK_NULL_HANDLE; }

    // ---- the store ----
    bool ring(std::uint64_t key, int n, const std::function<void(int, double*)>& at, LinesRun& r) override {
        if (n <= 0) return false;
        Block* b = block(key, std::uint32_t(n) <= 1024 ? 1024u : pow2At(std::uint32_t(n)));
        if (!b) return false;
        b->used = frame_;
        double p[3];
        bool same = false;
        if (b->m > 0 && !b->whole) {                     // what was added at the head (s) and lost at the tail (q): the back point found again near the end
            const int lim = std::min(n, 256);
            for (int s = 0; s < lim && !same; s++) {
                at(n - 1 - s, p);
                if (!equal(p, b->mirror, b->slotOf(b->m - 1))) continue;
                const int q = int(b->m) + s - n;
                if (q < 0 || q >= int(b->m)) continue;
                at(0, p);
                if (!equal(p, b->mirror, b->slotOf(std::uint32_t(q)))) continue;
                b->start = (b->start + std::uint32_t(q)) % b->cap;
                b->m -= std::uint32_t(q);
                for (int j = 0; j < s; j++) { at(n - s + j, p); put(*b, b->m, p); b->m++; }
                same = true;
            }
        }
        if (!same) {                                     // sent whole
            b->start = 0; b->m = 0; b->whole = false;
            for (int i = 0; i < n; i++) { at(i, p); put(*b, b->m, p); b->m++; }
        }
        r.slot = b->base; r.cap = b->cap; r.start = b->start; r.n = b->m;
        return true;
    }
    bool list(std::uint64_t key, const double* xyz, int n, LinesRun& r) override {
        if (n <= 0) return false;
        Block* b = block(key, pow2At(std::uint32_t(n)));
        if (!b) return false;
        b->used = frame_;
        const bool same = !b->whole && b->m == std::uint32_t(n) && b->start == 0 && std::memcmp(b->mirror.data(), xyz, std::size_t(n) * 3 * sizeof(double)) == 0;
        if (!same) {
            b->start = 0; b->m = 0; b->whole = false;
            for (int i = 0; i < n; i++) { put(*b, b->m, xyz + 3 * i); b->m++; }
        }
        r.slot = b->base; r.cap = b->cap; r.start = 0; r.n = b->m;
        return true;
    }
    std::uint32_t circleSlot() override {
        std::vector<double> c(3 * 257);
        for (int i = 0; i <= 256; i++) { const double E = kTwoPi * i / 256; c[std::size_t(3 * i)] = std::cos(E); c[std::size_t(3 * i + 1)] = std::sin(E); c[std::size_t(3 * i + 2)] = 0; }
        LinesRun r;
        if (!list(~std::uint64_t(0), c.data(), 257, r)) return 0;
        return r.slot;
    }
    void frameDone() override {
        frame_++;
        for (auto it = blocks_.begin(); it != blocks_.end();) {
            if (frame_ - it->second.used > 120) { free_[it->second.cap].push_back(it->second.base); it = blocks_.erase(it); }
            else ++it;
        }
    }
    long uploadedPoints() const override { return uploaded_; }

    // ---- the frame's: the store's new points, then steps 0 and 1 for the segment seg of this frame; false: nothing to draw from the card ----
    bool record(int seg, const LinesDraw& d) {
        if (!built() || !c_.ok() || d.pieces <= 0 || d.runs.empty()) return false;
        if (!sendStore()) return false;
        if (seg >= int(segs_.size())) segs_.resize(std::size_t(seg) + 1);
        Seg& s = segs_[std::size_t(seg)];
        if (!sized(s, d, false)) return false;
        c_.copyIn(d.runs.data(), d.runs.size() * sizeof(LinesRun), s.runs, 0);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        if (!steps(d, store_, s, 2, VkCards::kToVertex | VkCards::kToIndirect)) return false;
        return true;
    }
    VkBuffer storeBuf() const { return store_.b; }
    VkBuffer runsBuf(int seg) const { return seg < int(segs_.size()) ? segs_[std::size_t(seg)].runs.b : VK_NULL_HANDLE; }
    VkBuffer offBuf(int seg) const { return seg < int(segs_.size()) ? segs_[std::size_t(seg)].off.b : VK_NULL_HANDLE; }
    VkBuffer endsBuf(int seg) const { return seg < int(segs_.size()) ? segs_[std::size_t(seg)].scr.b : VK_NULL_HANDLE; }
    VkBuffer indBuf(int seg) const { return seg < int(segs_.size()) ? segs_[std::size_t(seg)].ind.b : VK_NULL_HANDLE; }

    // ---- the check ----
    int beginCheck(const LinesDraw& d) override {
        if (!built() || !c_.ok() || d.pieces <= 0 || d.checkPts.empty() || jobs_.size() >= 8) return 0;
        Job j;
        j.id = nextId_++; j.pieces = d.pieces;
        j.points = std::uint32_t(std::max(d.checkRoom, d.pieces));   // (room for the steps' points: the check gives twice the processor's count)
        const std::size_t np = d.checkPts.size() / 3;
        std::vector<float> pts(np * 8);
        for (std::size_t i = 0; i < np; i++) hiLo(&d.checkPts[3 * i], &pts[8 * i]);
        if (!c_.makeBuffer(VkDeviceSize(np) * kPointBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, j.store, "the lines' check points") ||
            !sized(j.seg, d, true) || !c_.makeBuffer(VkDeviceSize(j.points) * 8u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, j.chk, "the lines' check steps")) { dropJob(j); return 0; }
        c_.copyIn(pts.data(), pts.size() * sizeof(float), j.store, 0);
        c_.copyIn(d.runs.data(), d.runs.size() * sizeof(LinesRun), j.seg.runs, 0);
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute);
        if (!steps(d, j.store, j.seg, 3, VkCards::kToHost, &j.chk)) { dropJob(j); return 0; }
        j.ticket = c_.flush(false);
        if (!j.ticket) { dropJob(j); return 0; }
        jobs_.push_back(std::move(j));
        return jobs_.back().id;
    }
    JobState pollCheck(int job, std::vector<float>& ends, std::vector<std::uint32_t>& offsets, std::vector<float>& points) override {
        for (Job& j : jobs_) {
            if (j.id != job) continue;
            if (!c_.done(j.ticket)) return JobState::Pending;
            const bool good = !c_.failed();
            if (good) {
                ends.assign(static_cast<const float*>(j.seg.scr.map), static_cast<const float*>(j.seg.scr.map) + 4 * std::size_t(j.pieces));
                offsets.assign(static_cast<const std::uint32_t*>(j.seg.off.map), static_cast<const std::uint32_t*>(j.seg.off.map) + std::size_t(j.pieces) + 1);
                const std::uint32_t n = std::min(offsets.back(), j.points);
                points.assign(static_cast<const float*>(j.chk.map), static_cast<const float*>(j.chk.map) + 2 * std::size_t(n));
            }
            release(job);
            return good ? JobState::Done : JobState::Failed;
        }
        return JobState::Failed;
    }
    void cancel(int job) override { release(job); }

private:
    struct Block {
        std::uint32_t base = 0, cap = 0, start = 0, m = 0;   // its slots, the ring's start, the points it holds
        std::uint64_t used = 0;
        bool whole = true;                               // (new: to be sent whole)
        std::vector<double> mirror;                      // what the card holds, by slot (cap x 3)
        std::uint32_t slotOf(std::uint32_t i) const { return (start + i) % cap; }
    };
    struct Seg { VkCards::Buf runs, off, scr, ind; std::size_t runN = 0; int pieceN = 0; };
    struct Job { int id = 0, pieces = 0; std::uint32_t points = 0; std::uint64_t ticket = 0; VkCards::Buf store, chk; Seg seg; };

    static bool equal(const double* p, const std::vector<double>& m, std::uint32_t slot) { return std::memcmp(p, &m[3 * std::size_t(slot)], 3 * sizeof(double)) == 0; }
    void put(Block& b, std::uint32_t i, const double* p) {
        const std::uint32_t s = b.slotOf(i);
        std::memcpy(&b.mirror[3 * std::size_t(s)], p, 3 * sizeof(double));
        float hl[8];
        hiLo(p, hl);
        pending_.push_back({b.base + s, {hl[0], hl[1], hl[2], hl[3], hl[4], hl[5], hl[6], hl[7]}});
    }
    Block* block(std::uint64_t key, std::uint32_t cap) {
        auto it = blocks_.find(key);
        if (it != blocks_.end() && it->second.cap >= cap) return &it->second;
        if (it != blocks_.end()) { free_[it->second.cap].push_back(it->second.base); blocks_.erase(it); }
        Block b;
        b.cap = cap;
        auto& fl = free_[cap];
        if (!fl.empty()) { b.base = fl.back(); fl.pop_back(); }
        else {
            if (top_ + cap > (1u << 26)) return nullptr;  // (64 million points: never in this app)
            b.base = top_; top_ += cap;
        }
        b.mirror.assign(std::size_t(cap) * 3, 0.0);
        return &(blocks_[key] = std::move(b));
    }
    // the points added since the last frame into the store (a bigger store first, the old one copied in, when the slots outgrew it)
    bool sendStore() {
        const VkDeviceSize need = VkDeviceSize(std::max<std::uint32_t>(top_, 1024u)) * kPointBytes;
        if (store_.size < need) {
            VkCards::Buf nb;
            if (!c_.makeBuffer(std::max<VkDeviceSize>(need, 2 * store_.size), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, nb, "the lines' points")) return false;
            if (store_.b) {
                const VkBufferCopy cp{0, 0, store_.size};
                vkCmdCopyBuffer(c_.cmd(), store_.b, nb.b, 1, &cp);
                vkMemoryBarrier2(c_.cmd(), VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT);   // (the new points after the old ones are in)
                c_.dropBuffer(store_);
            }
            store_ = nb;
        }
        if (pending_.empty()) return true;
        std::vector<VkBufferCopy> cps;
        std::vector<float> data;
        data.reserve(pending_.size() * 8);
        for (std::size_t i = 0; i < pending_.size(); i++) {      // runs of consecutive slots: one region each
            const std::uint32_t s = pending_[i].slot;
            if (!cps.empty() && cps.back().dstOffset + cps.back().size == VkDeviceSize(s) * kPointBytes && i > 0 && pending_[i - 1].slot + 1 == s) cps.back().size += kPointBytes;
            else cps.push_back({VkDeviceSize(data.size()) * 4u, VkDeviceSize(s) * kPointBytes, kPointBytes});
            data.insert(data.end(), pending_[i].v, pending_[i].v + 8);
        }
        const VkDeviceSize at = c_.stage(data.data(), data.size() * sizeof(float));
        if (!c_.ok()) return false;
        for (VkBufferCopy& cp : cps) cp.srcOffset += at;
        vkCmdCopyBuffer(c_.cmd(), c_.stageBuffer(), store_.b, static_cast<std::uint32_t>(cps.size()), cps.data());
        c_.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VkCards::kToCompute | VkCards::kToVertex);
        uploaded_ += long(pending_.size());
        pending_.clear();
        return true;
    }
    void dropSeg(Seg& s) { c_.dropBuffer(s.runs); c_.dropBuffer(s.off); c_.dropBuffer(s.scr); c_.dropBuffer(s.ind); s = Seg(); }
    void dropJob(Job& j) { c_.dropBuffer(j.store); c_.dropBuffer(j.chk); dropSeg(j.seg); }
    bool sized(Seg& s, const LinesDraw& d, bool host) {
        if (s.runs.b && s.runN >= d.runs.size() && s.pieceN >= d.pieces) return true;
        dropSeg(s);
        const std::size_t runN = std::max<std::size_t>(d.runs.size() + d.runs.size() / 2, 16);
        const int pieceN = std::max(d.pieces + d.pieces / 2, 256);
        const VkBufferUsageFlags st = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        const bool ok = c_.makeBuffer(VkDeviceSize(runN) * sizeof(LinesRun), st | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, s.runs, "the lines' runs") &&
                        c_.makeBuffer(VkDeviceSize(pieceN + 1) * 4u, st, host, s.off, "the lines' offsets") &&
                        c_.makeBuffer(VkDeviceSize(pieceN) * 16u, st, host, s.scr, "the lines' ends") &&
                        c_.makeBuffer(16u, st | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, false, s.ind, "the lines' draw");
        if (ok) { s.runN = runN; s.pieceN = pieceN; }
        return ok;
    }
    // steps 0 .. to - 1 (0 the ends and the steps a piece, 1 the offsets and the draw, 2 every step's point: the check)
    bool steps(const LinesDraw& d, VkCards::Buf& store, Seg& s, int to, unsigned consumer, VkCards::Buf* chk = nullptr) {
        VkUniforms& u = k_.u;
        const LinesCamera& c = d.cam;
        const int n[4] = {int(d.runs.size()), d.pieces, chk ? int(chk->size / 8u) : 0, 0};
        u.f("uCHi", c.cHi, 3); u.f("uCLo", c.cLo, 3); u.f("uRot", c.rot, 4); u.f("uProj", c.proj, 4); u.f("uScr", c.scr, 4); u.f("uView", c.view, 4); u.i("uN", n, 4);
        VkCards::Buf& out = chk ? *chk : s.ind;          // (step 2 writes into it only in the check)
        for (int step = 0; step < to; step++) {
            u.i("uStep", &step, 1);
            const std::uint32_t groups = step == 1 ? 1u : static_cast<std::uint32_t>((d.pieces + 255) / 256);
            if (!c_.dispatch(k_, {&store, &s.runs, &s.off, &s.scr, &s.ind, &out}, groups)) return false;
            c_.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, step + 1 == to ? consumer : unsigned(VkCards::kToCompute));
        }
        return true;
    }
    void release(int id) {
        for (std::size_t i = 0; i < jobs_.size(); i++)
            if (jobs_[i].id == id) { dropJob(jobs_[i]); jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }

    struct Pending { std::uint32_t slot; float v[8]; };
    VkCards& c_;
    VkCards::Kernel k_;
    VkCards::Buf store_;
    std::map<std::uint64_t, Block> blocks_;
    std::map<std::uint32_t, std::vector<std::uint32_t>> free_;
    std::uint32_t top_ = 0;
    std::uint64_t frame_ = 1;
    std::vector<Pending> pending_;
    std::vector<Seg> segs_;
    std::vector<Job> jobs_;
    int nextId_ = 1;
    long uploaded_ = 0;
};

LinesCard* makeVkLinesCard(VkCards& c) { return new LinesCardVk(c); }
bool vkLinesRecord(LinesCard* c, int seg, const LinesDraw& d) { LinesCardVk* v = dynamic_cast<LinesCardVk*>(c); return v && v->record(seg, d); }
bool vkLinesBuffers(LinesCard* c, int seg, VkBuffer bufs[4], VkBuffer& indirect) {
    const LinesCardVk* v = dynamic_cast<const LinesCardVk*>(c);
    if (!v) return false;
    bufs[0] = v->storeBuf(); bufs[1] = v->runsBuf(seg); bufs[2] = v->offBuf(seg); bufs[3] = v->endsBuf(seg);
    indirect = v->indBuf(seg);
    return bufs[0] && bufs[1] && bufs[2] && bufs[3] && indirect;
}

}  // namespace render
