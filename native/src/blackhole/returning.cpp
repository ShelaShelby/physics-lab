// Black Hole Lab (0.87.0): returning radiation (returning.h; Cunningham 1976 (7a), (8a) written as sums over rays)
#include "blackhole/returning.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

#include "blackhole/disc.h"
#include "blackhole/kerr.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

double returnY(double a, double r) { const double q = horizon(a) / r; return q * q; }
double returnR(double a, double y) { return horizon(a) / std::sqrt(y); }
double returnFg(double a, double r) { return discFlux(r, a) / (3 / (8 * kPi * r * r * r)); }

double returnShare(double a, double r) {
    const double rms = innermostStable(a, true);
    return std::clamp(std::log(r / rms) / std::log(kReturnFar / rms), 0.0, 1.0);
}
std::vector<Vec3> returnLooks(int nMu, int nPsi, double sh) {
    const double pm = 1 + 3 * sh, pq = 1 + 2 * sh;
    std::vector<Vec3> v;
    v.reserve(static_cast<std::size_t>(nMu) * nPsi);
    for (int i = 0; i < nMu; i++) {
        const double t = (i + 0.5) / nMu, mu = std::pow(t, pm), s = std::sqrt(1 - mu * mu);
        for (int j = 0; j < nPsi; j++) {
            const double w = 2 * (j + 0.5) / nPsi - 1, p = kPi * (1 + w * std::pow(std::fabs(w), pq - 1));   // crowded towards the hole (psi = pi)
            v.push_back(Vec3{s * std::cos(p), -mu, s * std::sin(p)});       // towards where the light comes from, above the disc (-e_theta is up)
        }
    }
    return v;
}

std::vector<double> returnWeights(int nMu, int nPsi, double sh) {
    const double pm = 1 + 3 * sh, pq = 1 + 2 * sh;
    std::vector<double> w;
    w.reserve(static_cast<std::size_t>(nMu) * nPsi);
    for (int i = 0; i < nMu; i++) {
        const double t = (i + 0.5) / nMu, w0 = 2 * pm * std::pow(t, 2 * pm - 1);
        for (int j = 0; j < nPsi; j++) { const double v = 2 * (j + 0.5) / nPsi - 1; w.push_back(w0 * pq * std::pow(std::fabs(v), pq - 1)); }
    }
    return w;
}
double returnNode(double a, int k, int nodes) {
    const double rms = innermostStable(a, true), x = double(k) / (nodes - 1);
    return rms * std::pow(kReturnFar / rms, x * x);                         // even in (ln(r / r_ms))^(1/2): crowded at r_ms, where f_o bends
}

std::vector<double> returnPsiChi(double a, double r, const std::vector<Vec3>& looks) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    const double Om = 1 / (r * std::sqrt(r) + a), V = (Om - m.omega) * m.varpi / m.alpha;   // the gas's speed against the FIDO, along e_phi
    std::vector<double> pc;
    pc.reserve(2 * looks.size());
    for (const Vec3& N : looks) {
        const Vec3 nF = aberrate(N, V, Vec3{0, 0, 1});                     // the light's direction of motion in the FIDO's frame
        const Vec3 L{-nF.x, -nF.y, -nF.z};                                  // the FIDO's look
        const double psi = std::acos(std::clamp(-L.x, -1.0, 1.0));
        double chi = std::atan2(L.z, L.y);
        if (chi < 0) chi += 2 * kPi;
        pc.push_back(psi); pc.push_back(chi);
    }
    return pc;
}

ReturnHit returnHitOf(double a, double rO, double b, const double* u, int nU) {
    ReturnHit h;
    int k = 0;                                                              // the start on the plane itself, where a path reports it (at a/M = 0.99 the
    while (k < nU && u[k] > 0 && std::fabs(1 / u[k] - rO) < 1e-7 * rO) k++;   // closed forms do for some rays): left out (a true return to r_o has no measure)
    if (k >= nU || !(u[k] > 0)) return h;
    const double r = 1 / u[k];
    if (!(r >= innermostStable(a, true)) || !std::isfinite(r)) return h;   // inside r_ms: absorbed by the falling gas, not sent again (returning.h)
    const double g = discShiftKerr(r, b, a) / discShiftKerr(rO, b, a);
    h.rE = r; h.g4 = g * g * g * g;
    return h;
}

std::vector<ReturnHit> returnRaysCpu(double a, double rO, int nMu, int nPsi) {
    const std::vector<double> pc = returnPsiChi(a, rO, returnLooks(nMu, nPsi, returnShare(a, rO)));
    const Camera cam{rO, kPi / 2, 0};
    std::vector<ReturnHit> out(pc.size() / 2);
    const int nT = std::max(1u, std::thread::hardware_concurrency());
    std::atomic<std::size_t> next{0};
    const auto work = [&] {
        for (;;) {
            const std::size_t k = next.fetch_add(64);
            if (k >= out.size()) return;
            for (std::size_t i = k; i < std::min(out.size(), k + 64); i++) {
                const SpinHit h = spinRayClosed(a, cam, pc[2 * i], pc[2 * i + 1], 1e-10);
                out[i] = returnHitOf(a, rO, h.b, h.u, kSpinCross);
            }
        }
    };
    std::vector<std::thread> th;
    for (int t = 1; t < nT; t++) th.emplace_back(work);
    work();
    for (std::thread& t : th) t.join();
    return out;
}

double ReturnFlux::foAt(double r) const {
    if (lr.empty()) return 0;
    const double l = std::log(r);
    if (l <= lr.front()) return fo.front();
    if (l >= lr.back()) return fo.back();
    const auto it = std::upper_bound(lr.begin(), lr.end(), l);
    const std::size_t i = static_cast<std::size_t>(it - lr.begin()) - 1;
    const double t = (l - lr[i]) / (lr[i + 1] - lr[i]);
    return fo[i] + t * (fo[i + 1] - fo[i]);
}
double ReturnFlux::fluxAt(double r) const {
    if (!(r >= rms) || lr.empty()) return 0;
    return foAt(r) * 3 / (8 * kPi * r * r * r);
}

ReturnFlux returnSolve(double a, const std::vector<std::vector<ReturnHit>>& hits, const std::vector<std::vector<double>>& weights, double tol) {
    ReturnFlux F;
    F.a = a; F.rp = horizon(a); F.rms = innermostStable(a, true);
    const int n = static_cast<int>(hits.size());
    struct W { double w, r, fg; };
    std::vector<std::vector<W>> rays(hits.size());
    std::vector<double> r3(hits.size());
    for (int k = 0; k < n; k++) {
        const double r = returnNode(a, k, n);
        F.lr.push_back(std::log(r));
        r3[std::size_t(k)] = r * r * r / std::max<std::size_t>(1, hits[std::size_t(k)].size());   // r_o^3 / N
        const std::vector<ReturnHit>& H = hits[std::size_t(k)];
        for (std::size_t q = 0; q < H.size(); q++)
            if (H[q].rE > 0) rays[std::size_t(k)].push_back(W{weights[std::size_t(k)][q] * H[q].g4 / (H[q].rE * H[q].rE * H[q].rE), H[q].rE, returnFg(a, H[q].rE)});
    }
    F.fo.assign(hits.size(), 0.0);
    for (int it = 0; it < 500; it++) {
        std::vector<double> nx(hits.size(), 0.0);
        double ch = 0;
        for (int k = 0; k < n; k++) {
            double s = 0;
            for (const W& w : rays[std::size_t(k)]) s += w.w * (w.fg + F.foAt(w.r));
            nx[std::size_t(k)] = r3[std::size_t(k)] * s;
            ch = std::max(ch, std::fabs(nx[std::size_t(k)] - F.fo[std::size_t(k)]));
        }
        F.fo = nx;
        F.iterations = it + 1;
        if (ch < tol) break;
    }
    return F;
}

ReturnFlux returnFluxCpu(double a, int nodes, int nMu, int nPsi) {
    std::vector<std::vector<ReturnHit>> hits(static_cast<std::size_t>(nodes));
    std::vector<std::vector<double>> w(static_cast<std::size_t>(nodes));
    for (int k = 0; k < nodes; k++) {
        const double r = returnNode(a, k, nodes);
        hits[std::size_t(k)] = returnRaysCpu(a, r, nMu, nPsi);
        w[std::size_t(k)] = returnWeights(nMu, nPsi, returnShare(a, r));
    }
    return returnSolve(a, hits, w);
}

namespace {
constexpr int kSlots = 4096;
std::array<std::atomic<const ReturnFlux*>, kSlots> g_slot{};
std::atomic<int> g_used{0};
std::mutex g_add;
}  // namespace

const ReturnFlux* returnFluxFor(double a) {
    const int n = std::min(g_used.load(std::memory_order_acquire), kSlots);
    for (int i = n - 1; i >= 0; i--) {
        const ReturnFlux* f = g_slot[std::size_t(i)].load(std::memory_order_acquire);
        if (f && f->a == a) return f;
    }
    return nullptr;
}
void returnFluxStore(const ReturnFlux& f) {
    std::lock_guard<std::mutex> lk(g_add);
    if (returnFluxFor(f.a)) return;
    const int i = g_used.load();
    if (i >= kSlots) return;                                                // (4,096 spins in one run: later ones are drawn without returning light)
    g_slot[std::size_t(i)].store(new ReturnFlux(f), std::memory_order_release);   // never freed (returning.h)
    g_used.store(i + 1, std::memory_order_release);
}
const ReturnFlux* returnFluxNow(double a) {
    if (const ReturnFlux* f = returnFluxFor(a)) return f;
    returnFluxStore(returnFluxCpu(a));
    return returnFluxFor(a);
}

}  // namespace blackhole
