#include "belt_layer.h"

#include <cmath>

#include "units.h"

namespace orbit {

using render::Color;

const BeltLayer::Packed& BeltLayer::packed(const BeltGroup& g) {
    auto it = packed_.find(g.serial);
    if (it != packed_.end() && it->second.count == g.n) return it->second;
    Packed& p = packed_[g.serial];
    p.count = g.n;
    p.el.assign(std::size_t(g.n) * render::kRockStride, 0.f);
    p.nn.clear();
    for (int k = 0; k < g.n; k++) {
        float* e = &p.el[std::size_t(k) * render::kRockStride];
        if (g.isStatic) { e[5] = g.x[k]; e[6] = g.y[k]; e[7] = g.z[k]; continue; }
        e[0] = g.M0[k]; e[1] = g.a[k]; e[2] = g.e[k]; e[3] = g.libA[k]; e[4] = g.libPh[k];
        e[5] = g.Px[k]; e[6] = g.Py[k]; e[7] = g.Pz[k]; e[8] = g.Qx[k]; e[9] = g.Qy[k]; e[10] = g.Qz[k];
        p.nn.push_back(beltMeanMotion(g, k));
    }
    return p;
}

render::RockSet BeltLayer::gpuSet(const BeltGroup& g) {
    const Packed& p = packed(g);
    render::RockSet set;
    set.serial = g.serial; set.count = g.n; set.isStatic = g.isStatic; set.el = p.el.data(); set.nn = g.isStatic ? nullptr : p.nn.data();
    return set;
}

BeltLayer::Path BeltLayer::draw(render::Renderer2D& R, const SolarSystem& sys, const BeltCamera& cam, int selectedBelt, Color highlight, bool gpuRocks) {
    last_ = Path::None;
    if (sys.belts.empty()) return last_;
    const auto style = [&](std::size_t gi, double& size, Color& c) {
        const BeltGroup& g = sys.belts[gi];
        const bool on = int(gi) == selectedBelt;                          // the selected belt: bigger dots, in the look-here colour
        size = on ? 2.6 : 1.4; c = on ? highlight : render::hexColor(g.color, g.alpha);
    };
    if (gpuRocks && R.rockCompute() && R.rockCompute()->ok()) {
        render::RockFrame f;
        f.t = sys.t;
        const double lp = sys.t / units::kYear / 150 * 6.2832;             // the Trojans' sway phase (belts.cpp), brought into 0..2 pi in double
        f.libPhase = lp - 2 * units::kPi * std::floor(lp / (2 * units::kPi));
        f.bc[0] = cam.bx; f.bc[1] = cam.by; f.bc[2] = cam.bz;
        f.ca = cam.ca; f.sa = cam.sa; f.cb = cam.cb; f.sb = cam.sb; f.px = cam.px; f.W = cam.W; f.H = cam.H; f.D = cam.D; f.cd = cam.cd; f.ox = cam.ox; f.oy = cam.oy; f.d3 = cam.d3;
        std::vector<std::uint64_t> live;
        for (const BeltGroup& g : sys.belts) live.push_back(g.serial);
        R.rockCompute()->keepOnly(live);                                    // (sets of rocks that are gone leave the GPU first)
        bool all = true;
        for (std::size_t gi = 0; gi < sys.belts.size() && all; gi++) {
            const BeltGroup& g = sys.belts[gi];
            const render::RockSet set = gpuSet(g);
            double size; Color c; style(gi, size, c);
            all = R.rocksGpu(set, f, size, c);
        }
        if (all) {                                                          // (forget the packed copies of rocks that are gone: a new Solar System date makes new belts)
            for (auto it = packed_.begin(); it != packed_.end();) {
                bool alive = false;
                for (const BeltGroup& g : sys.belts) alive = alive || g.serial == it->first;
                it = alive ? std::next(it) : packed_.erase(it);
            }
            return last_ = Path::Gpu;
        }
    }
    // the CPU path, as it always was
    bool fresh = posT_ == sys.t && posEpoch_ == sys.epochMs && pos_.size() == sys.belts.size() && posSerials_.size() == sys.belts.size();
    for (std::size_t gi = 0; fresh && gi < sys.belts.size(); gi++) fresh = posSerials_[gi] == sys.belts[gi].serial && int(pos_[gi].size()) == sys.belts[gi].n;
    if (!fresh) {
        pos_.resize(sys.belts.size());
        posSerials_.resize(sys.belts.size());
        for (std::size_t gi = 0; gi < sys.belts.size(); gi++) {
            const BeltGroup& g = sys.belts[gi];
            pos_[gi].resize(std::size_t(g.n));
            for (int k = 0; k < g.n; k++) pos_[gi][std::size_t(k)] = beltPoint(g, k, sys.t);
            cpuPlaced_ += g.n;
            posSerials_[gi] = g.serial;
        }
        posT_ = sys.t; posEpoch_ = sys.epochMs;
    }
    for (std::size_t gi = 0; gi < sys.belts.size(); gi++) {
        screen_.clear();
        for (const Vec3& p : pos_[gi]) {
            double X, Y;
            beltScreen(cam, p, X, Y);
            if (X > -2 && X < cam.W + 2 && Y > -2 && Y < cam.H + 2) { screen_.push_back(static_cast<float>(X)); screen_.push_back(static_cast<float>(Y)); }
        }
        double size; Color c; style(gi, size, c);
        R.squares(screen_.data(), static_cast<int>(screen_.size() / 2), size, c);
    }
    return last_ = Path::Cpu;
}

}  // namespace orbit
