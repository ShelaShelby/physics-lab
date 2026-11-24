#include "tail_layer.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "solar_data.h"
#include "units.h"

namespace orbit {

void TailLayer::prepare(render::Renderer2D& R, const SolarSystem& sys, bool gpu) {
    jd_ = sys.julianDayTDB();
    last_ = Path::None;
    std::vector<std::uint64_t> live;
    for (const Body& b : sys.bodies) {
        if (b.type != BodyKind::Comet || !b.comet) continue;
        tail::CometGrains& G = g_[b.comet];
        if (G.serial() == 0) G.setSerial(static_cast<std::uint64_t>(b.comet - kComets) + 1);
        G.update(*b.comet, jd_);
        if (G.live() > 0) live.push_back(G.serial());
    }
    render::IRockCompute* rc = R.rockCompute();
    if (gpu && rc && rc->ok() && rc->grainsOk()) {
        rc->keepOnlyGrains(live);
        for (auto it = slot_.begin(); it != slot_.end();) {          // (the slots of comets with no grains are gone from the card)
            const auto g = g_.find(it->first);
            bool alive = false;
            for (std::uint64_t s : live) alive = alive || (g != g_.end() && g->second.serial() == s);
            it = alive ? std::next(it) : slot_.erase(it);
        }
    } else slot_.clear();
}

const tail::CometGrains* TailLayer::grains(const CometData* c) const {
    const auto it = g_.find(c);
    return it == g_.end() ? nullptr : &it->second;
}

int TailLayer::gpuSlot(const Body& b) const {
    const auto it = slot_.find(b.comet);
    return it == slot_.end() ? -1 : it->second;
}

const TailLayer::Cpu& TailLayer::cpuPlaces(const Body& b) {
    Cpu& C = cpu_[b.comet];
    const tail::CometGrains& G = g_[b.comet];
    if (C.now != G.now()) {                                           // the places change only with the simulated time
        C.now = G.now();
        C.p.resize(tail::kGrains); C.on.assign(tail::kGrains, 0);
        for (int g = 0; g < tail::kGrains; g++) {
            Vec3 r;
            if (G.position(g, r)) { C.p[std::size_t(g)] = r * units::kAU; C.on[std::size_t(g)] = 1; cpuPlaced_++; }
        }
    }
    return C;
}

void TailLayer::cpuScreen(const Body& b, const BeltCamera& cam, std::vector<float>& xy) {
    const Cpu& C = cpuPlaces(b);
    xy.assign(std::size_t(tail::kGrains) * 2, -1e9f);
    for (int g = 0; g < tail::kGrains; g++) {
        if (!C.on[std::size_t(g)]) continue;
        double X, Y;
        beltScreen(cam, C.p[std::size_t(g)], X, Y);
        xy[2 * std::size_t(g)] = static_cast<float>(X); xy[2 * std::size_t(g) + 1] = static_cast<float>(Y);
    }
}

TailLayer::Path TailLayer::drawDust(render::Renderer2D& R, const Body& b, const BeltCamera& cam, bool gpu, render::Color c, double size) {
    if (!b.comet) return Path::None;
    tail::CometGrains& G = g_[b.comet];
    if (G.live() <= 0) { G.clearDirty(); return Path::None; }
    render::IRockCompute* rc = R.rockCompute();
    if (gpu && rc && rc->ok() && rc->grainsOk()) {
        bool fresh = false;
        const int slot = rc->grainSlot(G.serial(), tail::kGrains, fresh);
        bool sent = slot >= 0;
        if (sent && fresh) sent = rc->grainWrite(slot, 0, tail::kGrains, G.el().data(), G.t0().data());
        else if (sent) for (const auto& d : G.dirty()) sent = sent && rc->grainWrite(slot, d.first, d.second, G.el().data() + std::size_t(d.first) * tail::kStride, G.t0().data() + d.first);
        if (sent) {
            G.clearDirty();
            render::RockFrame f;
            rockFrameCamera(cam, f);
            f.t = G.now();                                            // (days since J2000, TDB: kGrainsCS's time)
            if (R.grainsGpu(slot, tail::kGrains, f, size, c)) { slot_[b.comet] = slot; return last_ = Path::Gpu; }
        }
    }
    G.clearDirty();
    slot_.erase(b.comet);
    const Cpu& C = cpuPlaces(b);
    screen_.clear();
    for (int g = 0; g < tail::kGrains; g++) {
        if (!C.on[std::size_t(g)]) continue;
        double X, Y;
        beltScreen(cam, C.p[std::size_t(g)], X, Y);
        if (X > -2 && X < cam.W + 2 && Y > -2 && Y < cam.H + 2) { screen_.push_back(static_cast<float>(X)); screen_.push_back(static_cast<float>(Y)); }
    }
    R.squares(screen_.data(), static_cast<int>(screen_.size() / 2), size, c);
    return last_ = Path::Cpu;
}

bool TailLayer::gasTail(const Body& b, Vec3& dir, double& lengthM) const {
    if (!b.comet || length(b.pos) > tail::kSwitchOnAU * units::kAU) return false;
    const tail::CometGrains* G = grains(b.comet);
    Vec3 far;
    if (!G || !G->farthest(far)) return false;
    dir = tail::ionTailDirection(b.pos, b.vel);
    lengthM = length(far * units::kAU - b.pos);
    return lengthM > 0;
}

}  // namespace orbit

namespace orbit {
std::string TailLayer::pathText() const {
    return last_ == Path::Gpu ? "the comets' dust grains: the graphics card (kGrainsCS, Vulkan; the rocks' program object)" :
           last_ == Path::Cpu ? "the comets' dust grains: the processor (no grain program on the card, or GPU rocks switched off)" : "the comets' dust grains: none drawn";
}
}  // namespace orbit
