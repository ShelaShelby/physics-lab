#include "blackhole/spin_card.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "blackhole/kerr.h"
#include "blackhole/spin.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double angle(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)); }
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
            l1, l2, kPi, 2 / kPi, 1 / ln2, std::sqrt(0.5)};                         // 23..28
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

void cardEdges(render::IKerrCompute& K, double a, double rC, double thC, const std::vector<double>& chi, std::vector<double>& out) {
    K.edges(cardParams(a, rC, thC), chi, out);
}

std::shared_ptr<SpinTable> cardTableNow(render::IKerrCompute& K, double a, double rC, double thC, int nChi, int nS, int nEdge, double rtol) {
    const double t0 = nowMs();
    const std::vector<double> par = cardParams(a, rC, thC, rtol);
    std::vector<double> chi(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) chi[static_cast<std::size_t>(i)] = 2 * kPi * i / nEdge;   // (the processor's azimuths, the same expression)
    K.passA(par, chi);
    K.done(true);
    SpinTable::Parts p;
    std::vector<double> probe;
    K.readA(p.edge, probe);
    std::vector<Vec3> dirs(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) dirs[static_cast<std::size_t>(i)] = hitOf(probe.data() + 10 * i).dir;
    SpinTable::rowsOf(p.edge, dirs, nChi, p.tau, p.rowChi);
    K.passB(par, p.rowChi, nS);
    K.done(true);
    p.nodes = K.mapB(p.rowEdge);
    if (!p.nodes) { K.release(); return nullptr; }
    p.ms = nowMs() - t0;
    auto t = std::make_shared<SpinTable>(a, rC, thC, nChi, nS, nEdge, p);
    K.release();
    return t;
}

CardCheck cardSelfCheck(render::IKerrCompute& K) {
    CardCheck c;
    const double t0 = nowMs();
    struct Case { double a, r, th; };
    for (const Case cs : {Case{0.9, 20, 1.3}, Case{kSpinTop, 6, 1.4}}) {
        const Camera cam{cs.r, cs.th, 0};
        const std::vector<double> chis = {0.3, 1.7, 3.1, 4.5};
        std::vector<double> e;
        cardEdges(K, cs.a, cs.r, cs.th, chis, e);
        std::vector<double> pc;
        for (std::size_t k = 0; k < chis.size(); k++) {
            const double ref = shadowEdge(cs.a, cam, chis[k]);
            c.edge = std::max(c.edge, k < e.size() && std::isfinite(e[k]) ? std::fabs(e[k] - ref) : 1.0);
            c.edges++;
            for (const double f : {1e-2, 0.1, 0.5, -0.1}) { pc.push_back(ref + f * ref); pc.push_back(chis[k]); }
        }
        std::vector<CardRay> got;
        cardRays(K, cs.a, cs.r, cs.th, pc, got);
        for (std::size_t k = 0; k < pc.size() / 2; k++) {
            const SpinHit ref = spinRay(cs.a, cam, pc[2 * k], pc[2 * k + 1], 1e-10);
            c.rays++;
            if (k >= got.size() || got[k].h.sky != ref.sky || (got[k].end == 0) != (length(ref.dir) == 0 && !ref.sky && got[k].end != 2)) { c.endsDiffer++; continue; }
            const SpinHit& h = got[k].h;
            if (ref.sky) c.dir = std::max(c.dir, std::isfinite(angle(h.dir, ref.dir)) ? angle(h.dir, ref.dir) : 1.0);
            const double db = std::fabs(h.b - ref.b) / (1 + std::fabs(ref.b));
            c.b = std::max(c.b, std::isfinite(db) ? db : 1.0);
            for (int q = 0; q < kSpinCross; q++) {
                const double du = std::fabs(h.u[q] - ref.u[q]) / (1 + std::fabs(ref.u[q]));
                c.u = std::max(c.u, std::isfinite(du) ? du : 1.0);
            }
        }
    }
    c.ms = nowMs() - t0;
    c.ok = c.endsDiffer == 0 && c.edge <= kCardEdgeTol && c.dir <= kCardDirTol && c.b <= kCardBTol && c.u <= kCardUTol;
    if (!c.ok) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "the card's light paths differed from the processor's (edge %.1e rad, direction %.1e rad, b %.1e, u %.1e, %d ends differ)", c.edge, c.dir, c.b, c.u, c.endsDiffer);
        c.why = buf;
    }
    return c;
}

void CardFill::start(const Settings& s, double W, double H, double cx, double cy, int nChi, int nS, int nEdge) {
    run_ = std::make_shared<CardTableData>();
    run_->s = s; run_->W = W; run_->H = H; run_->cx = cx; run_->cy = cy;
    run_->nChi = nChi; run_->nS = nS; run_->nEdge = nEdge;
    par_ = cardParams(s.spin, s.dist, s.pitch);
    edgeChi_.resize(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) edgeChi_[static_cast<std::size_t>(i)] = 2 * kPi * i / nEdge;
    t0_ = nowMs();
    K_.passA(par_, edgeChi_);
    stage_ = 1;
}

std::shared_ptr<CardTableData> CardFill::pump() {
    if (stage_ == 3) {                                     // the last table handed out: released once the worker has made it (or nobody holds it any more)
        if (out_ && !out_->used->load() && out_.use_count() > 1) return nullptr;
        K_.release();
        out_.reset();
        stage_ = 0;
        return nullptr;
    }
    if (stage_ == 1) {
        if (!K_.done(false)) return nullptr;
        std::vector<double> probe;
        K_.readA(run_->parts.edge, probe);
        std::vector<Vec3> dirs(static_cast<std::size_t>(run_->nEdge));
        for (int i = 0; i < run_->nEdge; i++) dirs[static_cast<std::size_t>(i)] = hitOf(probe.data() + 10 * i).dir;
        SpinTable::rowsOf(run_->parts.edge, dirs, run_->nChi, run_->parts.tau, run_->parts.rowChi);
        K_.passB(par_, run_->parts.rowChi, run_->nS);
        stage_ = 2;
    }
    if (stage_ == 2) {
        if (!K_.done(false)) return nullptr;
        run_->parts.nodes = K_.mapB(run_->parts.rowEdge);
        if (!run_->parts.nodes) { K_.release(); stage_ = 0; run_.reset(); return nullptr; }
        lastMs_ = run_->parts.ms = nowMs() - t0_;
        fills_++;
        out_ = std::move(run_);
        stage_ = 3;
        return out_;
    }
    return nullptr;
}

}  // namespace blackhole
