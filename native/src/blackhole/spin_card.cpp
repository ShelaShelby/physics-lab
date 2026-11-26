#include "blackhole/spin_card.h"
#include "blackhole/returning.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "blackhole/kerr.h"
#include "blackhole/spin.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double angle(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)); }
std::string fmt(const char* f, double a, double b = 0, double c = 0) { char buf[160]; std::snprintf(buf, sizeof buf, f, a, b, c); return buf; }
SpinHit hitOf(const double* n) {
    SpinHit h;
    h.dir = Vec3{n[0], n[1], n[2]};
    h.b = n[3];
    for (int k = 0; k < kSpinCross; k++) h.u[k] = n[4 + k];
    h.sky = n[8] == 1;
    return h;
}
}  // namespace

std::vector<double> cardParams(double a, double rC, double thC, double rtol) {
    const KerrMetric m = kerrMetric(a, rC, thC);
    // the trapped orbits' radii and b at their ends (spin.cpp fromHorizonPos and trappedRadiusOfB)
    const double r1 = 2 * (1 + std::cos(2.0 / 3.0 * std::acos(-a))), r2 = 2 * (1 + std::cos(2.0 / 3.0 * std::acos(a)));
    const auto trappedB = [a](double r) { return -(r * r * (r - 3) + a * a * (r + 1)) / (a * (r - 1)); };
    const double pio2 = kPi / 2, p1 = std::ldexp(std::floor(std::ldexp(pio2, 30)), -30), p2 = pio2 - p1;
    const double ln2 = std::log(2.0), l1 = std::ldexp(std::floor(std::ldexp(ln2, 32)), -32), l2 = ln2 - l1;
    return {a, rC, thC, rtol, rtol * 0.1, std::max(1e5, 1e4 * rC), horizon(a) + 1e-3,                       // 0..6 (KerrBL: rFar 1e5, TraceOptions atol = rtol / 10)
            m.alpha, m.omega, m.varpi, std::sqrt(m.rho2), std::sqrt(m.Delta), std::cos(thC), std::sin(thC),   // 7..13
            std::min(r1, r2), std::max(r1, r2), trappedB(r2), trappedB(r1),                                    // 14..17
            SpinTable::sOf(kSpinXlo), kSpinXc,                                                                  // 18, 19
            p1, p2, std::cos(pio2),                                                // 20..22: pi/2 in three parts (the last: pi/2 less its double, = cos of that double)
            l1, l2, kPi, 2 / kPi, 1 / ln2, std::sqrt(0.5),                          // 23..28
            2.3190468138462996e-17};                                                 // 29: ln 2 less its double (ln 2 = 0.693147180559945309417232121458...)
}

void cardRays(render::IKerrCompute& K, double a, double rC, double thC, const std::vector<double>& psiChi, std::vector<CardRay>& out, double rtol) {
    std::vector<double> raw;
    K.rays(cardParams(a, rC, thC, rtol), psiChi, raw);
    out.assign(psiChi.size() / 2, CardRay{});
    for (std::size_t k = 0; k < out.size(); k++) {
        const double* n = raw.data() + 16 * k;
        CardRay& r = out[k];
        r.h = hitOf(n);
        r.end = static_cast<int>(n[8]); r.steps = static_cast<int>(n[9]);
        for (int i = 0; i < 5; i++) r.y[i] = n[10 + i];
        r.q = n[15];
    }
}

ReturnFlux cardReturnFlux(render::IKerrCompute& K, double a, int nodes, int nMu, int nPsi, int* unanswered) {
    std::vector<std::vector<ReturnHit>> hits(static_cast<std::size_t>(nodes));
    std::vector<std::vector<double>> w(static_cast<std::size_t>(nodes));
    int bad = 0;
    for (int k = 0; k < nodes; k++) {
        const double rO = returnNode(a, k, nodes), sh = returnShare(a, rO);
        const std::vector<double> pc = returnPsiChi(a, rO, returnLooks(nMu, nPsi, sh)), par = cardParams(a, rO, kPi / 2);
        w[std::size_t(k)] = returnWeights(nMu, nPsi, sh);
        std::vector<ReturnHit>& H = hits[std::size_t(k)];
        H.assign(pc.size() / 2, ReturnHit{});
        for (std::size_t at = 0; at < H.size(); at += render::IKerrCompute::kPiece) {
            const std::size_t n = std::min<std::size_t>(render::IKerrCompute::kPiece, H.size() - at);
            std::vector<double> raw;
            K.rays(par, std::vector<double>(pc.begin() + std::ptrdiff_t(2 * at), pc.begin() + std::ptrdiff_t(2 * (at + n))), raw);
            for (std::size_t q = 0; q < n; q++) {
                if (16 * q + 16 > raw.size() || !std::isfinite(raw[16 * q + 3])) { bad++; continue; }
                H[at + q] = returnHitOf(a, rO, raw[16 * q + 3], &raw[16 * q + 4], kSpinCross);
            }
        }
    }
    if (unanswered) *unanswered = bad;
    return returnSolve(a, hits, w);
}

void cardEdges(render::IKerrCompute& K, double a, double rC, double thC, const std::vector<double>& chi, std::vector<double>& out) {
    K.edges(cardParams(a, rC, thC), chi, out);
}

std::shared_ptr<SpinTable> cardTableNow(render::IKerrCompute& K, double a, double rC, double thC, int nChi, int nS, int nEdge, double rtol) {
    const double t0 = nowMs();
    const std::vector<double> par = cardParams(a, rC, thC, rtol);
    std::vector<double> chi(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) chi[static_cast<std::size_t>(i)] = 2 * kPi * i / nEdge;   // (the processor's azimuths, the same expression)
    K.passA(par, chi);
    if (!K.done(true)) return nullptr;                     // (0.55.7: a wait is bounded, IKerrCompute::kWaitLimitMs)
    SpinTable::Parts p;
    std::vector<double> probe;
    K.readA(p.edge, probe);
    std::vector<Vec3> dirs(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) dirs[static_cast<std::size_t>(i)] = hitOf(probe.data() + 10 * i).dir;
    SpinTable::rowsOf(p.edge, dirs, nChi, p.tau, p.rowChi);
    K.passB(par, p.rowChi, nS);
    bool fin = false;
    for (int k = 0; k < kCardWholeFactor && !fin; k++) fin = K.done(true);
    if (!fin) return nullptr;
    p.nodes = K.mapB(p.rowEdge);
    if (!p.nodes) { K.release(); return nullptr; }
    p.ms = nowMs() - t0;
    auto t = std::make_shared<SpinTable>(a, rC, thC, nChi, nS, nEdge, p);
    K.release();
    return t;
}

double cardStepLimitMs() {
    static const double ms = [] {
        const char* e = std::getenv("PL_BH_CARDLIMIT");
        const double v = e ? std::atof(e) : 0;
        return v > 0 ? v * 1000 : 10000.0;
    }();
    return ms;
}

CardCheck cardSelfCheck(render::IKerrCompute& K) {
    CardStartup s(K, nullptr, 1e300);
    int r = 0;
    while ((r = s.pump(true)) == 0) {}
    return s.check();
}

CardStartup::CardStartup(render::IKerrCompute& K, app::GpuGuard* guard, double limitMs) : K_(K), g_(guard), limit_(limitMs) {}

void CardStartup::begin(const std::string& s) { if (g_) g_->begin("check: " + s); }
void CardStartup::end(const std::string& s, const std::string& r) { if (g_) g_->end("check: " + s, r); }
void CardStartup::queued(const std::string& what) { wait_ = "fence wait, " + what; tq_ = nowMs(); begin(wait_); }

// the cases: (spin, distance, theta); per case 4 edges, then 16 rays at the processor's edges (two sides, inside and outside)
namespace { struct CheckCase { double a, r, th; }; const CheckCase kChecks[2] = {{0.9, 20, 1.3}, {kSpinTop, 6, 1.4}}; }

int CardStartup::pump(bool wait) {
    if (stage_ == 0) t0_ = nowMs();
    for (;;) {
        if (!wait_.empty()) {                              // a dispatch is under way: finished, or out of time?
            const bool fin = K_.done(wait);
            if (!fin) {
                if (wait || K_.queuedMs() > limit_) {
                    late_ = "check: " + wait_ + fmt(" (not finished after %.0f s)", (wait ? render::IKerrCompute::kWaitLimitMs : limit_) / 1000);
                    c_.ok = false; c_.why = "a step of the card's start-up check did not finish in time (" + wait_ + ")";
                    c_.ms = nowMs() - t0_;
                    return -2;
                }
                return 0;
            }
            end(wait_, fmt("%.1f ms", nowMs() - tq_));
            wait_.clear();
        }
        const int k = (stage_ - 2) / 3, part = (stage_ - 2) % 3;
        if (stage_ == 0) {
            const std::vector<double> x = {0.5, -1.25, 2.0, 3.75, -0.3, 10.0, 0.01, 100.0};
            ref_ = x;
            begin("dispatch 1: the shader's double functions at 8 values (one work group, the smallest dispatch)");
            K_.queueMath(cardParams(0.5, 20, 1.3), x);
            end("dispatch 1");
            queued("dispatch 1");
            stage_ = 1;
            continue;
        }
        if (stage_ == 1) {
            begin("read back dispatch 1"); std::vector<double> out; K_.readMath(out); end("read back dispatch 1");
            for (std::size_t j = 0; j < ref_.size(); j++) {
                const double v = ref_[j], ax = std::fabs(v);
                const double r8[8] = {std::sin(v), std::cos(v), std::exp(v), std::log(ax), std::atan2(v, 1 - v), std::acos(v / (1 + std::fabs(v))), std::cbrt(v), std::pow(ax, -0.2)};
                for (int i = 0; i < 8; i++) {
                    const double g = j * 8 + static_cast<std::size_t>(i) < out.size() ? out[j * 8 + static_cast<std::size_t>(i)] : std::nan("");
                    const double d = i < 2 ? std::fabs(g - r8[i]) : std::fabs(g - r8[i]) / std::fabs(r8[i]);
                    c_.math = std::max(c_.math, std::isfinite(d) ? d : 1.0);
                }
            }
            if (g_) g_->log(fmt("check: the shader's double functions against the C++ library's: largest difference %.2e (limit %.0e)", c_.math, kCardMathTol));
            stage_ = 2;
            continue;
        }
        if (k >= 2) break;
        const CheckCase& cs = kChecks[k];
        const Camera cam{cs.r, cs.th, 0};
        const std::string tag = "case " + std::to_string(k + 1) + fmt(" (spin %.2f, r %.0f, theta %.1f)", cs.a, cs.r, cs.th);
        if (part == 0) {
            chis_ = {0.3, 1.7, 3.1, 4.5};
            begin(tag + ": dispatch the shadow's edge at 4 azimuths");
            K_.queueEdges(cardParams(cs.a, cs.r, cs.th), chis_);
            end(tag + ": dispatch the shadow's edge at 4 azimuths");
            queued(tag + " edges");
            stage_++;
            continue;
        }
        if (part == 1) {
            begin(tag + ": read back the edges"); std::vector<double> e; K_.readEdges(e); end(tag + ": read back the edges");
            pc_.clear();
            for (std::size_t j = 0; j < chis_.size(); j++) {
                const double ref = shadowEdge(cs.a, cam, chis_[j]);
                c_.edge = std::max(c_.edge, j < e.size() && std::isfinite(e[j]) ? std::fabs(e[j] - ref) : 1.0);
                c_.edges++;
                for (const double f : {1e-2, 0.1, 0.5, -0.1}) { pc_.push_back(ref + f * ref); pc_.push_back(chis_[j]); }
            }
            begin(tag + ": dispatch 16 rays");
            K_.queueRays(cardParams(cs.a, cs.r, cs.th), pc_);
            end(tag + ": dispatch 16 rays");
            queued(tag + " rays");
            stage_++;
            continue;
        }
        begin(tag + ": read back the rays"); std::vector<double> raw; K_.readRays(raw); end(tag + ": read back the rays");
        for (std::size_t j = 0; j < pc_.size() / 2; j++) {
            const SpinHit ref = spinRay(cs.a, cam, pc_[2 * j], pc_[2 * j + 1], 1e-10);
            c_.rays++;
            if (16 * j + 16 > raw.size()) { c_.endsDiffer++; continue; }
            const double* n = raw.data() + 16 * j;
            const SpinHit h = hitOf(n);
            const int endK = static_cast<int>(n[8]);
            if (h.sky != ref.sky || endK == 0) { c_.endsDiffer++; continue; }   // (a lost ray: none of these is near a trapped orbit)
            if (ref.sky) c_.dir = std::max(c_.dir, std::isfinite(angle(h.dir, ref.dir)) ? angle(h.dir, ref.dir) : 1.0);
            const double db = std::fabs(h.b - ref.b) / (1 + std::fabs(ref.b));
            c_.b = std::max(c_.b, std::isfinite(db) ? db : 1.0);
            for (int q = 0; q < kSpinCross; q++) {
                const double du = std::fabs(h.u[q] - ref.u[q]) / (1 + std::fabs(ref.u[q]));
                c_.u = std::max(c_.u, std::isfinite(du) ? du : 1.0);
            }
        }
        stage_++;
    }
    c_.ms = nowMs() - t0_;
    c_.ok = c_.endsDiffer == 0 && c_.math <= kCardMathTol && c_.edge <= kCardEdgeTol && c_.dir <= kCardDirTol && c_.b <= kCardBTol && c_.u <= kCardUTol;
    if (!c_.ok) {
        char buf[300];
        std::snprintf(buf, sizeof buf, "the card's light paths differed from the processor's (edge %.1e rad, direction %.1e rad, b %.1e, u %.1e, %d ends differ, functions %.1e)",
                      c_.edge, c_.dir, c_.b, c_.u, c_.endsDiffer, c_.math);
        c_.why = buf;
    }
    if (g_) {
        char buf[300];
        std::snprintf(buf, sizeof buf, "check: %d edges, %d rays: edge %.2e rad (limit %.0e), direction %.2e rad (%.0e), b %.2e (%.0e), u %.2e (%.0e), %d ends differ; %.1f ms: %s",
                      c_.edges, c_.rays, c_.edge, kCardEdgeTol, c_.dir, kCardDirTol, c_.b, kCardBTol, c_.u, kCardUTol, c_.endsDiffer, c_.ms, c_.ok ? "PASSED" : "FAILED");
        g_->log(buf);
    }
    return c_.ok ? 1 : -1;
}

bool CardFill::late(double limit) {
    if (K_.queuedMs() <= limit) return false;
    late_ = wait_ + fmt(" (not finished after %.0f s)", limit / 1000);
    say("the step '" + wait_ + "' did not finish in time: the fill is dropped");
    stage_ = 0; run_.reset(); wait_.clear();
    return true;
}

void CardFill::say(const std::string& line) { if (g_ && stepwise_) g_->log("fill: " + line); }

void CardFill::start(const Settings& s, double W, double H, double cx, double cy, int nChi, int nS, int nEdge) {
    run_ = std::make_shared<CardTableData>();
    run_->s = s; run_->W = W; run_->H = H; run_->cx = cx; run_->cy = cy;
    run_->nChi = nChi; run_->nS = nS; run_->nEdge = nEdge;
    par_ = cardParams(s.spin, s.dist, s.pitch);
    edgeChi_.resize(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) edgeChi_[static_cast<std::size_t>(i)] = 2 * kPi * i / nEdge;
    t0_ = tStep_ = nowMs();
    stepwise_ = fills_ == 0;                               // (0.55.7: the run's first fill, step by step)
    if (stepwise_ && g_) g_->begin(fmt("fill 1: dispatch pass A (the shadow's edge and a probe ray at %.0f azimuths)", nEdge));
    K_.passA(par_, edgeChi_);
    wait_ = "fill 1: fence wait, pass A";
    if (stepwise_ && g_) { g_->end("fill 1: dispatch pass A"); g_->begin(wait_); }
    stage_ = 1;
}

std::shared_ptr<CardTableData> CardFill::pump() {
    const bool tell = stepwise_ && g_ != nullptr;
    if (stage_ == 3) {                                     // the last table handed out: released once the worker has made it (or nobody holds it any more)
        if (out_ && !out_->used->load() && out_.use_count() > 1) return nullptr;
        K_.release();
        out_.reset();
        stage_ = 0;
        if (reading_) {
            reading_ = false;
            if (tell) { g_->end("fill 1: the worker reads the nodes from mapped memory", fmt("released after %.1f ms", nowMs() - tStep_)); g_->finished(true); }
        }
        return nullptr;
    }
    if (stage_ == 1) {
        if (!K_.done(false)) { late(limit_); return nullptr; }
        if (tell) g_->end(wait_, fmt("%.1f ms", nowMs() - tStep_));
        if (tell) g_->begin("fill 1: read back pass A");
        std::vector<double> probe;
        K_.readA(run_->parts.edge, probe);
        if (tell) g_->end("fill 1: read back pass A");
        std::vector<Vec3> dirs(static_cast<std::size_t>(run_->nEdge));
        for (int i = 0; i < run_->nEdge; i++) dirs[static_cast<std::size_t>(i)] = hitOf(probe.data() + 10 * i).dir;
        SpinTable::rowsOf(run_->parts.edge, dirs, run_->nChi, run_->parts.tau, run_->parts.rowChi);
        tStep_ = nowMs();
        if (stepwise_) {
            if (tell) g_->begin(fmt("fill 1: dispatch the rows' edges (%.0f rows)", run_->nChi));
            if (!K_.passBRows(par_, run_->parts.rowChi, run_->nS)) { stage_ = 0; run_.reset(); return nullptr; }
            wait_ = "fill 1: fence wait, the rows' edges";
            if (tell) { g_->end("fill 1: dispatch the rows' edges"); g_->begin(wait_); }
            next_ = 0; piece_ = kFirstPiece; pieces_ = 0;
            stage_ = 4;
        } else {
            K_.passB(par_, run_->parts.rowChi, run_->nS);
            wait_ = "fill " + std::to_string(fills_ + 1) + ": fence wait, pass B";
            stage_ = 2;
        }
    }
    if (stage_ == 4) {                                     // (the first fill: the rows' edges or a piece finished? then the next piece, or the copy)
        if (!K_.done(false)) { late(limit_); return nullptr; }
        if (tell) g_->end(wait_, fmt("%.1f ms", nowMs() - tStep_));
        tStep_ = nowMs();
        const int n = K_.nodes();
        if (next_ < n) {
            const int m = std::min(piece_, n - next_);
            pieces_++;
            const std::string what = "fill 1: piece " + std::to_string(pieces_) + " (rays " + std::to_string(next_) + " to " + std::to_string(next_ + m - 1) + " of " + std::to_string(n) + ")";
            if (tell) g_->begin("dispatch " + what);
            K_.passBPiece(next_, m);
            wait_ = "fence wait, " + what;
            if (tell) { g_->end("dispatch " + what); g_->begin(wait_); }
            next_ += m;
            piece_ = std::min(piece_ * 4, render::IKerrCompute::kPiece);
            return nullptr;
        }
        if (tell) g_->begin("fill 1: copy the nodes for the processor");
        K_.passBCopy();
        wait_ = "fill 1: fence wait, the copy";
        if (tell) { g_->end("fill 1: copy the nodes for the processor"); g_->begin(wait_); }
        stage_ = 2;
        return nullptr;
    }
    if (stage_ == 2) {
        if (!K_.done(false)) { late(stepwise_ ? limit_ : limit_ * kCardWholeFactor); return nullptr; }
        if (tell) { g_->end(wait_, fmt("%.1f ms", nowMs() - tStep_)); g_->begin("fill 1: map the nodes"); }
        run_->parts.nodes = K_.mapB(run_->parts.rowEdge);
        if (tell) g_->end("fill 1: map the nodes", run_->parts.nodes ? "mapped" : "FAILED (no memory)");
        wait_.clear();
        if (!run_->parts.nodes) { K_.release(); stage_ = 0; run_.reset(); if (tell) g_->finished(false, "the nodes could not be mapped"); return nullptr; }
        lastMs_ = run_->parts.ms = nowMs() - t0_;
        if (tell) {
            g_->log(fmt("fill 1: %.0f rays in %.0f pieces, %.1f ms in all", K_.nodes(), pieces_, lastMs_));
            g_->begin("fill 1: the worker reads the nodes from mapped memory");
            reading_ = true; tStep_ = nowMs();
        }
        fills_++;
        out_ = std::move(run_);
        stage_ = 3;
        return out_;
    }
    return nullptr;
}

}  // namespace blackhole
