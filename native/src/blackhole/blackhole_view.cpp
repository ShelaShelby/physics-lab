#include "app/frame_cost.h"
#include "blackhole/blackhole_view.h"
#include "blackhole/returning.h"
#include "blackhole/sky_card.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/gpu_guard.h"
#include "app/shot.h"
#include "blackhole/light.h"
#include "blackhole/low_priority.h"
#include "blackhole/spin.h"
#include "blackhole/kerr.h"
#include "blackhole/kerr_closed.h"
#include "blackhole/gas.h"
#include "orbit/sky.h"
#include <thread>

namespace blackhole {
namespace {
bool g_hgtHookOff = false;   // 0.79.0 (the hooks' checks of the plane's crossings: the surface off; 0.95.0: also spinDisc's, surfaceOf)
const HeightTable* surfaceOf(const DiscGeom& g) { return g_hgtHookOff ? nullptr : g.height; }
}  // namespace

namespace {
constexpr double kPi = 3.14159265358979323846;
// 0.55.3: a table's version number, unique across every view of the process: the renderer keeps the last table it was sent by its number only, so two views
// (the picture tests draw several) must never hand it the same number for different tables
std::uint64_t nextVersion() { static std::atomic<std::uint64_t> n{0}; return ++n; }
double msSince(std::chrono::steady_clock::time_point t0) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
// the brightest catalogue star seen without the hole: the cap of a drawn image (blackhole_view.h)
float brightestMag() {
    static const float m = [] { float b = 99; for (const orbit::SkyStar& s : orbit::skyData().stars) b = std::min(b, s.mag); return b; }();
    return m;
}
// 0.55.4: the eye's glare for the renderer (light.h): the finest block and the veil, on the picture's own grid
// 0.85.5: the veil on the card (render/vk_blackhole.cpp glareOnCard) unless PL_BH_GLARE_CPU is set (the processor's, for comparing)
bool glareCardOn() { static const bool cpu = std::getenv("PL_BH_GLARE_CPU") != nullptr; return !cpu; }
void glareTablesFn(int W, int H, double cx, double cy, double K, std::vector<float>& nearW, std::vector<float>& corr, double eye[2]) {   // 0.85.5: the card's veil
    glareTables(glareGrid(W, H, cx, cy, K), nearW, corr);
    eye[0] = kEyeAge; eye[1] = kEyePigment;
}
int glareBlockFn(int W, int H, double cx, double cy, double K) { return glareGrid(W, H, cx, cy, K).b; }
void glareVeilFn(int W, int H, double cx, double cy, double K, const std::vector<float>& src, std::vector<float>& veil, double out[3]) {
    GlareSums g;
    glareVeil(glareGrid(W, H, cx, cy, K), src, veil, &g);
    out[0] = g.ms; out[1] = g.spread; out[2] = g.laid;
}
// 0.55.4: the sRGB encoding of a linear opacity (IEC 61966-2-1), so a dot laid over black at this opacity shows the star's true light
// 0.55.6: a star image found on a worker thread, laid down afterwards in the catalogue's order (its place, magnitude, the star, its order, behind the disc)
struct Cand { double X, Y, mag; int star, order; bool hidden; double f = 1; };   // (0.84.0: f, the held camera's own shift for this image's ray)
constexpr int kStarChunk = 64;                       // stars per job
double encodeSrgb(double l) { l = std::clamp(l, 0.0, 1.0); return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055; }
}  // namespace

float catalogueBrightestMag() { return brightestMag(); }

// One star image as a dot (both pictures' rule): Shown: the night sky's tone map (orbit/sky.h starDraw); 0.55.4, the true exposure (light.h): the star's own
// light on the disc's scale, spread over its dot, at the screen's white; an image below the screen's faintest step is not drawn (it would draw nothing).
// 0.84.0: a catalogue star seen by the held camera at its own shift f (lab.h starSeen): the colour of a black body at f x the colour bucket's temperature
// (orbit/sky.h bucketBV, bvTemperatureK), its V magnitude by the black body's eye's light at f T. Spin 0: one f for every image; a spinning hole: each
// image's own (its ray's b from the table at the image's place). Gravity's bending off: no hole, no shift.
static StarSeen starSeenBucket(int bucket, double f) {
    const orbit::Sky& S = orbit::skyData();
    const int k = std::clamp(bucket, 0, orbit::kSkyColourBuckets - 1);
    return starSeen(S.colours[std::size_t(k)], orbit::bvTemperatureK(orbit::bucketBV(k)), f);
}
bool BlackHoleView::addDot(const Settings& s, const HoleView& v, double X, double Y, double mag, const std::array<int, 3>& c) {
    orbit::StarDraw d;
    double a;
    if (trueSkyOn(s)) {
        d = orbit::starDrawLight(starTrueLight(mag, st_.look.white, pointSolidAngle(v.K, v.cx, v.cy, X, Y)));
        if (!(d.alpha >= kScreenStep)) { st_.trueHidden++; return false; }
        a = encodeSrgb(d.alpha);
        st_.trueShown++;
    } else {
        d = orbit::starDraw(mag);
        a = d.alpha;
    }
    dots_.push_back({static_cast<float>(X), static_cast<float>(Y), static_cast<float>(d.radius), render::rgba(c[0], c[1], c[2], static_cast<float>(a))});
    return true;
}

// 0.55.4: the glare waits until the view has rested (blackhole_view.h kGlareRest)
void BlackHoleView::glareRest(const Key& k) {
    static const bool wait = std::getenv("PL_BH_SPINWAIT") != nullptr;   // (automatic checks: at once)
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!(k == glareKey_)) { glareKey_ = k; glareChangedAt_ = now; }
    glareReady_ = glareNow || wait || now - glareChangedAt_ >= kGlareRest;
}

// 0.55.4: the readout's numbers for the true exposure (light.h trueSkyNumbers): the brightest catalogue star at the middle of the picture beside the disc's brightest part
void BlackHoleView::trueNumbers(const Settings& s, const HoleView& v) {
    static const int bucket = [] {                                        // the brightest catalogue star's colour bucket
        int k = 0;
        float b = 99;
        for (const orbit::SkyStar& t : orbit::skyData().stars) {
            if (t.mag < b) { b = t.mag; k = t.bucket; }
        }
        return k;
    }();
    st_.brightMag = brightestMag() + starSeenBucket(bucket, s.bend ? heldCameraShift(s, 0) : 1.0).dmag;   // 0.84.0: as the camera sees it at the middle (b = 0)
    st_.ptSr = 1 / (v.K * v.K);
    st_.trueSky = trueSkyNumbers(st_.brightMag, st_.look.brightY, st_.look.white, st_.ptSr);
}

bool BlackHoleView::Key::operator==(const Key& o) const {
    return W == o.W && H == o.H && cx == o.cx && cy == o.cy && dist == o.dist && fov == o.fov && yaw == o.yaw && pitch == o.pitch && bend == o.bend && sky == o.sky &&
           disc == o.disc && outer == o.outer && feed == o.feed && mass == o.mass && exp == o.exp && expAuto == o.expAuto && spin == o.spin && spinVer == o.spinVer && look == o.look &&
           trueSky == o.trueSky && glare == o.glare && allLight == o.allLight && ret == o.ret;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.2: the spinning hole
namespace {
double nowSeconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
// the view's frame: right (row 0), up (row 1), towards the camera (row 2) on the sky frame (orbit/sky.h skyScreen; tests/check of compareHook)
void viewRows(const HoleView& v, Vec3 rows[3]) {
    rows[0] = Vec3{v.ca, -v.sa, 0};
    rows[1] = Vec3{v.sa * v.cb, v.ca * v.cb, -v.sb};
    rows[2] = Vec3{v.sa * v.sb, v.ca * v.sb, v.cb};
}
// the camera's phi round the spin axis (the sky frame's z): its place is row 2 = (sin th cos phi, sin th sin phi, cos th)
double cameraPhi(const HoleView& v) { return std::atan2(v.ca * v.sb, v.sa * v.sb); }
// the camera's frame on the sky frame: e_r (from the hole to the camera), e_theta, e_phi
void cameraFrame(const HoleView& v, double th, Vec3& er, Vec3& et, Vec3& ep) {
    const double ph = cameraPhi(v), st = std::sin(th), ct = std::cos(th), sp = std::sin(ph), cp = std::cos(ph);
    er = Vec3{st * cp, st * sp, ct}; et = Vec3{ct * cp, ct * sp, -st}; ep = Vec3{-sp, cp, 0};
}
}  // namespace

BlackHoleView::BlackHoleView() {
    if (const char* e = std::getenv("PL_BH_GPUPATHS")) gpuPaths = std::atoi(e) != 0;   // 0.55.6 (tests): the switch's start value
    if (const char* e = std::getenv("PL_BH_GPUSKY")) gpuSky = std::atoi(e) != 0;       // 0.56.0 (tests): the sky switch's start value
}

BlackHoleView::~BlackHoleView() {
    stopWork();
    stopSpinWorker();
    if (retThread_.joinable()) retThread_.join();                          // 0.87.0
}

// 0.87.0 R11 (blackhole_view.h, returning.h): the returning light's table for the spin shown. On the card: each node's rays in pieces of 8,192 (render/
// kerr_compute.h queueRays: the light paths' own double-precision integrator, the same as the table's; spin_card.h cardParams for a camera at rest in the
// FIDO's frame on the equator at the node), read back a frame later; the hits as returnHitOf, the solve on the processor (returnSolve). Off the card: one
// thread of the processor (returnFluxCpu, spinRayClosed). Either way the table goes to returning.h's store; the picture's key changes with it.
struct BlackHoleView::ReturnJob {
    double a = 0;
    int node = 0;
    std::size_t at = 0;
    bool queued = false;
    std::vector<double> pc, par;
    std::vector<std::vector<ReturnHit>> hits;
    std::vector<std::vector<double>> w;
};
bool BlackHoleView::retQueued() const { return retJob_ && retJob_->queued; }
void BlackHoleView::pumpReturn(render::Renderer2D& R, const Settings& s, bool card) {
    const bool want = s.disc && s.look == Look::Real && discLightRules().returning && !returnFluxFor(s.spin);
    if (retJob_) {                                                          // a job on the card: on to its end (its table is kept for its spin)
        render::IKerrCompute* K = R.kerrCompute();
        ReturnJob& J = *retJob_;
        if (!K) { retJob_.reset(); return; }
        if (J.queued) {
            if (!K->done(false)) return;
            std::vector<double> raw;
            K->readRays(raw);
            const double rO = returnNode(J.a, J.node, kReturnNodes);
            for (std::size_t k = 0; 16 * k + 16 <= raw.size() && J.at + k < J.hits[std::size_t(J.node)].size(); k++)
                J.hits[std::size_t(J.node)][J.at + k] = returnHitOf(J.a, rO, raw[16 * k + 3], &raw[16 * k + 4], kSpinCross);
            J.at += raw.size() / 16;
            J.queued = false;
            if (J.at >= J.hits[std::size_t(J.node)].size()) { J.node++; J.at = 0; J.pc.clear(); }
        }
        if (J.node >= kReturnNodes) {
            returnFluxStore(returnSolve(J.a, J.hits, J.w));
            retJob_.reset();
            return;
        }
        if (!card || !cardFill_ || !cardFill_->idle()) return;              // (the card's own table fill first)
        if (J.pc.empty()) {
            const double rO = returnNode(J.a, J.node, kReturnNodes), sh = returnShare(J.a, rO);
            J.pc = returnPsiChi(J.a, rO, returnLooks(kReturnMu, kReturnPsi, sh));
            J.par = cardParams(J.a, rO, kPi / 2);
            J.hits[std::size_t(J.node)].assign(J.pc.size() / 2, ReturnHit{});
            J.w[std::size_t(J.node)] = returnWeights(kReturnMu, kReturnPsi, sh);
        }
        const std::size_t n = std::min<std::size_t>(render::IKerrCompute::kPiece, J.pc.size() / 2 - J.at);
        K->queueRays(J.par, std::vector<double>(J.pc.begin() + std::ptrdiff_t(2 * J.at), J.pc.begin() + std::ptrdiff_t(2 * (J.at + n))));
        J.queued = true;
        return;
    }
    if (!want || retThreadBusy_) return;
    if (card && R.kerrCompute()) {
        retJob_ = std::make_unique<ReturnJob>();
        retJob_->a = s.spin;
        retJob_->hits.resize(kReturnNodes);
        retJob_->w.resize(kReturnNodes);
        return;
    }
    if (retThread_.joinable()) retThread_.join();
    retThreadBusy_ = true;
    retThread_ = std::thread([this, a = s.spin] { returnFluxStore(returnFluxCpu(a)); retThreadBusy_ = false; });
}

void BlackHoleView::stopWork() {
    {   std::lock_guard<std::mutex> g(workMx_);                             // 0.55.5: the worker stops after its job (a table fill stops at once)
        workQuit_ = true; }
    stale_ = true;
    workCv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void BlackHoleView::stopSpinWorker() {
    spinCancel_ = true;
    if (spinWorker_.joinable()) spinWorker_.join();
    spinCancel_ = false;
    spinDone_ = false;
    std::lock_guard<std::mutex> g(spinMx_);
    spinReady_.reset();
}

// 0.87.0 R11: the flux the disc sends, F + F_o (returning.h), for the GPU's temperature table (null: F alone)
static const ReturnFlux* retFor(double spin) { return discLightRules().returning ? returnFluxFor(spin) : nullptr; }
static double fluxSent(double r, double spin, const ReturnFlux* ret) { return discFlux(r, spin) + (ret ? ret->fluxAt(r) : 0.0); }
void BlackHoleView::ensureDiscF(double spin) {
    const ReturnFlux* ret = retFor(spin);
    if (discFSpin_ == spin && discF_ && !discF_->empty() && discFRet_ == ret) return;
    discFRet_ = ret;
    // 0.57's grid kept point for point (1024 values from r_ms to 1000 M; the eye's picture bit for bit), continued with the same step past kOuterMax (0.58.0: 2000)
    const double r0 = discInner(spin), l0 = std::log(r0), dl = (std::log(1000.0) - l0) / (1024 - 1);
    const int n = 1024 + static_cast<int>(std::ceil((std::log(kOuterMax) - std::log(1000.0)) / dl)) + 1;
    auto f = std::make_shared<std::vector<float>>(static_cast<std::size_t>(n));
    for (int i = 0; i < n; i++) (*f)[static_cast<std::size_t>(i)] = static_cast<float>(std::pow(std::max(0.0, fluxSent(std::exp(l0 + i * dl), spin, ret)), 0.25));
    discF_ = std::move(f);
    discFSpin_ = spin; discFLog0_ = l0; discFDl_ = dl;
    discFVer_ = nextVersion();
}

void BlackHoleView::ensureSpin(const Settings& s, bool wait) { ensureSpinTable(s, wait, nullptr); }

// (false: the coarse fill was stopped by cancel; the table shown is unchanged)
bool BlackHoleView::ensureSpinTable(const Settings& s, bool wait, const std::atomic<bool>* cancel) {
    const double a = s.spin, r = s.dist, th = s.pitch;
    const auto mine = [&](const SpinTable& t) { return t.spin() == a && t.rC() == r && t.thC() == th; };
    const auto show = [&](std::shared_ptr<const SpinTable> t) {
        spin_ = std::move(t);
        spinVer_ = nextVersion();
        const auto t0 = std::chrono::steady_clock::now();
        spinStars_ = std::make_unique<SpinStars>(*spin_);
        st_.spinStarsMs = msSince(t0);
        st_.spinBins++;
    };
    const bool offered = fineExternal_ && fineOffer_ && mine(*fineOffer_);   // 0.55.5 (the worker's view): the owner's fine table
    const auto showOffered = [&] { st_.spinFineMs = fineOffer_->fillMs(); st_.spinFineFills++; st_.spinFine = true; show(fineOffer_); };
    if (!spinMatches(s)) {                                                  // settings changed: drop the fine one being made, the coarse one now
        if (offered) { showOffered(); return true; }
        if (!fineExternal_) stopSpinWorker();
        static const bool old = std::getenv("PL_BH_MOVING558") != nullptr;  // 0.55.9 (the check's failing run): 0.55.8's moving table, its size and rays
        auto c = old ? std::make_shared<SpinTable>(a, r, th, 32, 64, kSpinEdgeCoarse, cancel, 0, kSpinMovingMino)
                     : [&] {
                           static const double* tryv = [] { static double v[4] = {kSpinChiCoarse, kSpinSCoarse, kSpinMoving.xc, kSpinMoving.xlo};   // (PL_BH_MOVINGTRY, tests: rows, columns, xc, xlo)
                                                            if (const char* e = std::getenv("PL_BH_MOVINGTRY")) std::sscanf(e, "%lf,%lf,%lf,%lf", &v[0], &v[1], &v[2], &v[3]); return v; }();
                           SpinFill f = kSpinMoving; f.xc = tryv[2]; f.xlo = tryv[3];
                           return std::make_shared<SpinTable>(a, r, th, static_cast<int>(tryv[0]), static_cast<int>(tryv[1]), kSpinEdgeCoarse, cancel, 0, f); }();
        if (!c->ok()) return false;
        st_.spinCoarseMs = c->fillMs();
        st_.spinCoarseFills++;
        st_.spinFine = false;
        show(std::move(c));
        spinChangedAt_ = nowSeconds();
        spinFineStarted_ = false;
    }
    if (st_.spinFine) return true;
    if (fineExternal_) { if (offered) showOffered(); return true; }
    if (wait) {
        stopSpinWorker();
        auto f = std::make_shared<SpinTable>(a, r, th, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, fineFill);
        st_.spinFineMs = f->fillMs(); st_.spinFineFills++; st_.spinFine = true;
        show(std::move(f));
        return true;
    }
    if (!spinFineStarted_ && nowSeconds() - spinChangedAt_ >= kSpinRest) {   // the view has rested: the fine one on a thread of its own
        spinFineStarted_ = true;
        spinDone_ = false;
        const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
        const SpinFill fill = fineFill;
        const std::function<void()> job = [this, a, r, th, threads, fill] {
            lowerThisThread();                                              // 0.55.5: below normal priority (low_priority.h)
            auto f = std::make_shared<SpinTable>(a, r, th, kSpinChiFine, kSpinSFine, kSpinEdge, &spinCancel_, threads, fill);
            if (f->ok()) { std::lock_guard<std::mutex> g(spinMx_); spinReady_ = std::move(f); }
            spinDone_ = true;
        };
        spinWorker_ = std::thread(job);
    }
    if (spinFineStarted_ && spinDone_) {
        if (spinWorker_.joinable()) { app::FrameCostSpan w(app::FrameCostKind::WorkerWait); spinWorker_.join(); }   // (0.63.1: the benchmark's worst frames)
        std::shared_ptr<const SpinTable> f;
        { std::lock_guard<std::mutex> g(spinMx_); f = std::move(spinReady_); }
        spinDone_ = false;
        if (f && mine(*f)) { st_.spinFineMs = f->fillMs(); st_.spinFineFills++; st_.spinFine = true; show(std::move(f)); }
    }
    return true;
}

void BlackHoleView::spinEdgePoints(const Settings& s, double W, double H, double cx, double cy, int n, std::vector<double>& out) const {
    out.clear();
    if (!spin_) return;
    const HoleView v = holeView(s, W, H, cx, cy);
    for (int k = 0; k < n; k++) {
        const double chi = 2 * kPi * (k + 0.5) / n, R = spin_->edge(chi) * v.K;
        const double ux = -std::sin(chi), uy = -std::cos(chi);              // chi from up (e_theta) towards left (e_phi); y down on the picture
        out.insert(out.end(), {v.cx + R * ux, v.cy + R * uy, ux, uy});
    }
}

DiscPixel BlackHoleView::spinPixel(const Settings& s, const HoleView& v, double X, double Y, bool light) const {
    DiscPixel o;
    if (!spin_) return o;
    const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
    if (!(th <= kPi)) return o;
    const double nx = d > 0 ? dx / d : 0, ny = d > 0 ? dy / d : 1;
    double chi = std::atan2(-nx, ny);                                        // e_theta is up on the picture, e_phi is left (holeView: no roll)
    if (chi < 0) chi += 2 * kPi;
    const SpinHit h = spin_->lookup(th, chi);
    const DiscGeom g = discGeom(s);
    const SpinDisc sd = spinDisc(h, s.spin, g.rIn, g.rOut, surfaceOf(g));
    o.hit.order = sd.order; o.hit.r = sd.r; o.hit.g = sd.g; o.hit.b = h.b; o.hit.Lz = h.b;
    if (sd.order < 0 || !light) return o;
    discLightSeen(s, sd.r, sd.g, h.b, spinEmissionCos(s, th, chi, sd.g, sd.r), o);   // 0.55.3: by the look's rule (lab.h); 0.84.0: the camera's own shift for this ray's b
    return o;
}

// 0.95.0: the spinning hole's disc pixel from the closed forms in double (traceClosed with the disc's surface: kerr_closed.h), as the sky pass draws it (the
// tests' reference for kKerrSkyFS; spinPixel is the table's, the fallback's)
DiscPixel spinPixelExact(const Settings& s, const HoleView& v, double X, double Y, bool light) {
    DiscPixel o;
    const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
    if (!(th <= kPi)) return o;
    const double nx = d > 0 ? dx / d : 0, ny = d > 0 ? dy / d : 1;
    double chi = std::atan2(-nx, ny);
    if (chi < 0) chi += 2 * kPi;
    const DiscGeom g = discGeom(s);
    const ClosedEnd e = traceClosed(s.spin, Camera{s.dist, s.pitch, 0}, lookAt(th, chi), kClosedCross, false, &g);
    for (int k = 0; k < e.nCross && k < kSpinCross; k++) {
        const double r = k < kSurfaceOrders && e.uS[k] > 0 ? 1 / e.uS[k] : 1 / e.u[k];
        if (!(r >= g.rIn && r <= g.rOut)) continue;
        o.hit.order = k; o.hit.r = r; o.hit.b = e.b; o.hit.Lz = e.b; o.hit.g = discShiftKerr(r, e.b, s.spin);
        if (light) discLightSeen(s, r, o.hit.g, e.b, emissionCos(o.hit.g, e.q, r), o);
        return o;
    }
    return o;
}

void BlackHoleView::ensureDisc(double dist, bool moving) {
    if (disc_ && discDist_ == dist && (moving || !discCoarse_)) return;
    const auto t0 = std::chrono::steady_clock::now();
    disc_ = moving ? std::make_shared<const DiscTable>(dist, kDiscMovingRows, kDiscMovingCols) : std::make_shared<const DiscTable>(dist);
    discCoarse_ = moving;
    discDist_ = dist;
    discVer_ = nextVersion();
    st_.discMs = msSince(t0);
    st_.discFills++;
}

void BlackHoleView::discSamples(const Settings& s0, double W, double H, double cx, double cy, int n, std::vector<double>& on, std::vector<double>& off) {
    Settings s = s0;
    s.disc = true;
    ensureDisc(s.dist);
    const HoleView v = holeView(s, W, H, cx, cy);
    const auto hit = [&](double X, double Y) { return discPixel(*disc_, s, v, X, Y, false).hit.order >= 0; };
    std::vector<double> all[2];
    for (int j = 1; j < 40; j++)
        for (int i = 1; i < 64; i++) {
            const double X = std::floor(W * i / 64) + 0.5, Y = std::floor(H * j / 40) + 0.5;
            const bool h = hit(X, Y);
            bool same = true;
            for (double dx : {-3.0, 0.0, 3.0}) for (double dy : {-3.0, 0.0, 3.0}) if (hit(X + dx, Y + dy) != h) same = false;
            if (!same) continue;
            all[h ? 0 : 1].push_back(X); all[h ? 0 : 1].push_back(Y);
        }
    for (int k = 0; k < 2; k++) {                                            // n of them, spread over the whole list
        std::vector<double>& o = k == 0 ? on : off;
        o.clear();
        const int m = static_cast<int>(all[k].size() / 2);
        for (int q = 0; q < std::min(n, m); q++) { const int at = static_cast<int>(static_cast<long>(q) * m / std::min(n, m)); o.push_back(all[k][2 * at]); o.push_back(all[k][2 * at + 1]); }
    }
    discGridOn_ = static_cast<int>(all[0].size() / 2);
}

const std::vector<float>& BlackHoleView::colourTable(int& n, double& log0, double& dlog) { return cardColourTable(n, log0, dlog); }   // (0.58.0: in sky_card, the light check's too)

void BlackHoleView::ensureTable(double dist) {
    if (table_ && tableDist_ == dist) return;
    const auto t0 = std::chrono::steady_clock::now();
    table_ = std::make_shared<const LensTable>(dist);
    tableDist_ = dist;
    tableVer_ = nextVersion();
    st_.tableMs = msSince(t0);
    st_.tableFills++;
}

namespace {
double skySlowEnv() {                                                       // (tests: the pass taken as this slow, ms)
    static const double slow = std::getenv("PL_BH_SKYSLOW") ? std::atof(std::getenv("PL_BH_SKYSLOW")) : 0;
    return slow;
}
}  // namespace
double BlackHoleView::skySlowMs() const { return skyState_ == 1 ? (skySlowEnv() > 0 ? skySlowEnv() : st_.skyGpuMs) : -1; }

// 0.70.0: the card's exposure as read back (its brightest channel, brightest Y, log-average: tone.h); any false: nothing read yet
DiscLook cardReadLook(const render::Renderer2D::SkyStats& k) {
    DiscLook c;
    if (k.whiteReads > 0 && k.white > 0 && k.whiteY > 0 && k.logAvg > 0) { c.brightMax = k.white; c.brightY = k.whiteY; c.logAvg = k.logAvg; c.any = true; }
    return c;
}
// 0.70.0: the exposure's fields of a picture (Real: the tone curve, k = 1 / white; Film: the screen's white), from the processor's or the last good look
void BlackHoleView::toneFields(render::Renderer2D::LensSky& L, const Settings& s) const {
    L.white = st_.look.white > 0 ? st_.look.white : 1;
    L.tone = s.look == Look::Real ? 1 : 0;
    L.ev = exposureStops(s);
    L.toneLw = st_.look.lw;
    L.toneKey = kToneKey; L.toneDelta = kToneDelta;
}

// 0.72.0: the moving gas's fields of a pass (renderer2d.h LensSky::gas; gas.h gasCard): the pattern (SKH06) and the clump (BL06) of the shown settings s at the
// far time T; gasVer: a new number for a new T or a changed choice (the kept picture is drawn anew)
namespace {
bool g_gasHookOff = false;   // (the comparison hooks: their reference pictures are still; 2a turns the gas on for its own check)
void gasFields(render::Renderer2D::LensSky& L, const Settings& s, double T) {
    if (g_gasHookOff || !s.gasMoving || !(s.gasClump || s.gasPattern)) return;
    const DiscGeom g = discGeom(s);
    Clump c;
    c.a = s.spin; c.rc = gasClumpRadius(s);
    c.init();
    GasCard k;
    const StreakField* sf = s.gasPattern ? streakFieldReady() : nullptr;   // 0.79.0: (null while a worker makes it: no streaks yet)
    gasCard(sf != nullptr, s.spin, s.gasClump ? &c : nullptr, T, k);       // 0.79.0: the streaks (gas.h) in place of 0.72's patches
    L.gas = true;
    std::copy(k.T, k.T + 4, L.gasT); std::copy(k.C, k.C + 4, L.gasC); std::copy(k.G, k.G + 4, L.gasG);
    std::copy(k.V, k.V + 32, L.strkV); std::copy(k.W, k.W + 8, L.strkW); std::copy(k.B, k.B + 4 * kStreakBins, L.strkB);
    if (sf) { static int w = 0, h = 0; static const std::vector<float> atlas = sf->atlas(w, h); L.streak = &atlas; L.streakW = w; L.streakH = h; }   // (every field made is the same: one seed)
    L.gasTau = k.tau;
    L.timeR[0] = static_cast<float>(0.8 * g.rIn); L.timeR[1] = static_cast<float>(1.25 * g.rOut);
    std::uint64_t h = 1469598103934665603ULL;
    const auto mix = [&h](double x) { std::uint64_t b; std::memcpy(&b, &x, sizeof b); h = (h ^ b) * 1099511628211ULL; };
    for (double x : {T, s.spin, g.rIn, g.rOut, c.rc, s.gasClump ? 1.0 : 0.0, sf ? 1.0 : 0.0}) mix(x);
    L.gasVer = h | 1;
}
}  // namespace

// 0.87.0 (disc.h, THE DISC'S REAL LIGHT): the shaders' uDL for these settings (render/renderer2d.h LensSky::discLight); tFix: the gas's own temperature over
// tScale F^(1/4) (spin 0: 1 / the camera's own shift folded into tScale; else 1)
static void discLightFields(render::Renderer2D::LensSky& L, const Settings& s, double tFix) {
    const bool real = lookRule(s.look).shifts;
    const DiscLightRules& R = discLightRules();
    L.discLight[0] = real && R.fcol ? static_cast<float>(kFcolLow) : 1e30f;
    L.discLight[1] = 0.0f;
    L.discLight[2] = static_cast<float>(tFix);
    L.discLight[3] = 0.0f;
    const std::array<double, kLimbFineN> fine = limbFine();
    for (int k = 0; k < kLimbFineN; k++) L.limb[k] = real && R.limb ? static_cast<float>(fine[static_cast<std::size_t>(k)]) : 1.0f;
}
// 0.79.0 (0.95.0: every spin): the disc's thickness table for the passes (kLensFS, kKerrFS, kKerrSkyFS uHgt, uHgtR), where discThickDrawn
static void heightFields(render::Renderer2D::LensSky& L, const Settings& s, const DiscGeom& g) {
    const HeightTable* ht = surfaceOf(g);
    if (!ht) return;
    L.hgt = &ht->h;
    L.hgtR[0] = static_cast<float>(ht->ln0); L.hgtR[1] = static_cast<float>(ht->dln); L.hgtR[2] = static_cast<float>(ht->h.size()); L.hgtR[3] = 1;
    std::uint64_t hh = 1469598103934665603ULL;
    for (double x : {s.feed, s.massSun, s.spin}) { std::uint64_t b; std::memcpy(&b, &x, sizeof b); hh = (hh ^ b) * 1099511628211ULL; }
    L.hgtVersion = hh | 1;
}

render::Renderer2D::LensSky BlackHoleView::lensFor(const Settings& s, const HoleView& v) const {
    render::Renderer2D::LensSky L;
    L.cx = v.cx; L.cy = v.cy; L.kRad = v.K;
    double M[9];
    orbit::skyViewToGalactic(orbit::skyData(), v.ca, v.sa, v.cb, v.sb, M);
    for (int k = 0; k < 9; k++) L.M[k] = static_cast<float>(M[k]);
    L.bright = 1;
    L.bend = s.bend && table_;
    if (table_) {
        L.alphaSh = kPi - table_->edge();
        L.table = &table_->values();
        L.s0 = table_->s0(); L.ds = table_->ds();
        L.tableVersion = tableVer_;
    }
    L.sky = s.skyOurs && !trueSkyOn(s);                                    // 0.55.4: under the true exposure the map is not drawn (light.h)
    const auto spinFrame = [&](float kM[9], float kO[4]) {                  // the chart's axes to galactic, the screen directions of e_theta and e_phi
        Vec3 rows[3];
        viewRows(v, rows);
        const double ph = cameraPhi(v), c = std::cos(ph), sn = std::sin(ph);
        double RV[3][3];
        for (int i = 0; i < 3; i++) {
            const Vec3 col[3] = {Vec3{c, sn, 0}, Vec3{-sn, c, 0}, Vec3{0, 0, 1}};
            for (int j = 0; j < 3; j++) RV[i][j] = dot(rows[i], col[j]);
        }
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) kM[3 * i + j] = static_cast<float>(M[3 * i] * RV[0][j] + M[3 * i + 1] * RV[1][j] + M[3 * i + 2] * RV[2][j]);
        Vec3 er, et, ep;
        cameraFrame(v, s.pitch, er, et, ep);
        kO[0] = static_cast<float>(dot(et, rows[0])); kO[1] = static_cast<float>(dot(et, rows[1]));
        kO[2] = static_cast<float>(dot(ep, rows[0])); kO[3] = static_cast<float>(dot(ep, rows[1]));
    };
    if (lookRule(s.look).shifts && s.spin > 0) {                            // 0.84.0: the held camera's own shift per ray, f = 1 / alpha - b omega / alpha (lab.h heldCameraShift)
        const KerrMetric m = kerrMetric(s.spin, s.dist, s.pitch);
        L.fido[0] = static_cast<float>(1 / m.alpha); L.fido[1] = static_cast<float>(m.omega / m.alpha);
    }
    if (skyUse_ && s.spin > 0 && s.bend) {                                   // 0.56.0: the spinning hole per pixel on the card (no table)
        L.kerrSky = true;
        L.bend = true;
        spinFrame(L.kM, L.kO);
        skyCamera(L, s.spin, s.dist, s.pitch);
        L.moving = skyMoving_;
        L.frameBudgetMs = 500 / std::max(refreshHz, 1.0);
        L.slowMs = skySlowEnv();
        L.lowRes = lowResMode == 0;
        L.sharpMs = kSkySharpMs;
        if (s.disc && skyF_) {
            L.disc = true;
            L.discF = skyF_.get(); L.discFN = static_cast<int>(skyF_->size()); L.discFLog0 = skyFLog0_; L.discFDl = skyFDl_; L.discFVersion = skyFVer_;
            double l0, dl;
            L.discCol = &colourTable(L.discC, l0, dl);
            L.colLog0 = l0; L.colDlog = dl; L.colVersion = 1;
            const DiscGeom g = discGeom(s);
            L.rIn = g.rIn; L.rOut = g.rOut;
            L.tScale = std::pow(discPhys(s).fluxScale() / stefanBoltzmann(), 0.25);
            L.allInvTsun = allLightOn(s) ? 1 / sunTeff() : 0;               // 0.58.0
            discLightFields(L, s, 1);                                       // 0.87.0
            heightFields(L, s, g);                                          // 0.95.0: the surface at every spin
            L.whiteMode = 1;                                                // (0.70.0: your choice too: stops from the card's automatic)
            toneFields(L, s);
            L.expStep = 4;
            gasFields(L, s, gasT);                                          // 0.72.0: the moving gas (the crossings' phi from the camera's)
            L.gasCam = static_cast<float>(std::remainder(cameraPhi(v), 2 * kPi));
            if (!lookRule(s.look).shifts) {                                 // Film: the one colour before the white (the card divides and scales it)
                DiscPixel q;
                discLight(s, g.rIn, 1, q);
                for (int k = 0; k < 3; k++) L.filmRaw[k] = static_cast<float>(q.rgb[static_cast<std::size_t>(k)]);
                L.filmRaw[3] = 1;
            }
            if (glareOn(s) && glareReady_) { L.glareKeep = static_cast<float>(1 - glareShare()); L.glareBlock = glareBlockFn; L.glareVeil = glareVeilFn; }
            L.glareAsync = async && !glareNow; L.glareThread = lowerThisThread; L.glareTables = glareCardOn() ? glareTablesFn : nullptr;
        }
        return L;
    }
    if (s.spin > 0 && s.bend && spinMatches(s)) {                            // 0.55.2: the spinning hole
        const SpinTable& T = *spin_;
        L.kerr = true;
        L.bend = true;
        L.kA = &T.texA(); L.kB = &T.texB(); L.kC = &T.texC(); L.kEdge = &T.texEdge(); L.kTau = &T.texTau();
        L.kChi = T.nChi(); L.kS = T.nS(); L.kEdgeN = T.nEdge();
        L.kSLo = T.sLo(); L.kXc = T.xc(); L.kXlo = T.xlo(); L.spin = s.spin;
        L.kVersion = spinVer_;
        Vec3 rows[3];
        viewRows(v, rows);
        const double ph = cameraPhi(v), c = std::cos(ph), sn = std::sin(ph);
        // chart (camera at phi = 0) -> sky frame: Rz(phi); sky -> view: rows; view -> galactic: M
        double RV[3][3];
        for (int i = 0; i < 3; i++) {
            const Vec3 col[3] = {Vec3{c, sn, 0}, Vec3{-sn, c, 0}, Vec3{0, 0, 1}};   // the chart's axes on the sky frame
            for (int j = 0; j < 3; j++) RV[i][j] = dot(rows[i], col[j]);
        }
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) L.kM[3 * i + j] = static_cast<float>(M[3 * i] * RV[0][j] + M[3 * i + 1] * RV[1][j] + M[3 * i + 2] * RV[2][j]);
        Vec3 er, et, ep;
        cameraFrame(v, s.pitch, er, et, ep);
        L.kO[0] = static_cast<float>(dot(et, rows[0])); L.kO[1] = static_cast<float>(dot(et, rows[1]));
        L.kO[2] = static_cast<float>(dot(ep, rows[0])); L.kO[3] = static_cast<float>(dot(ep, rows[1]));
        if (s.disc) {
            L.disc = true;
            L.discF = discF_.get(); L.discFN = discF_ ? static_cast<int>(discF_->size()) : 0; L.discFLog0 = discFLog0_; L.discFDl = discFDl_; L.discFVersion = discFVer_;
            double l0, dl;
            L.discCol = &colourTable(L.discC, l0, dl);
            L.colLog0 = l0; L.colDlog = dl; L.colVersion = 1;
            const DiscGeom g = discGeom(s);
            L.rIn = g.rIn; L.rOut = g.rOut;
            L.tScale = std::pow(discPhys(s).fluxScale() / stefanBoltzmann(), 0.25);
            L.allInvTsun = allLightOn(s) ? 1 / sunTeff() : 0;               // 0.58.0
            discLightFields(L, s, 1);                                       // 0.87.0
            heightFields(L, s, g);                                          // 0.95.0: the surface at every spin (with texC's slopes)
            skyCamera(L, s.spin, s.dist, s.pitch);                          // 0.87.0 R10: the camera's numbers for each ray's Carter constant (kKerrFS uCam, uMet)
            toneFields(L, s);
            if (!lookRule(s.look).shifts) {                                 // 0.55.3, Film: one colour all over the disc (lab.h discLight), over the screen's white as discColour does
                DiscPixel q;
                discLight(s, g.rIn, 1, q);
                double c[3] = {q.rgb[0] / L.white, q.rgb[1] / L.white, q.rgb[2] / L.white};
                const double m = std::max({c[0], c[1], c[2]});
                for (int k = 0; k < 3; k++) L.discFix[k] = static_cast<float>(m > 1 ? c[k] / m : c[k]);
                L.discFix[3] = 1;
            }
            if (glareOn(s) && glareReady_) { L.glareKeep = static_cast<float>(1 - glareShare()); L.glareBlock = glareBlockFn; L.glareVeil = glareVeilFn; }   // 0.55.4
            L.glareAsync = async && !glareNow; L.glareThread = lowerThisThread; L.glareTables = glareCardOn() ? glareTablesFn : nullptr;   // 0.55.5
        }
        return L;
    }
    if (s.disc && s.bend && disc_ && discDist_ == s.dist) {
        L.disc = true;
        L.discU = &disc_->uValues(); L.discP = &disc_->phiValues();
        L.discS = disc_->rows(); L.discT = disc_->cols();
        L.discS0 = disc_->s0(0); L.discDs0 = disc_->ds(0); L.discDs1 = disc_->ds(1); L.discXc = kDiscXc;
        L.discVersion = discVer_;
        double l0, dl;
        L.discCol = &colourTable(L.discC, l0, dl);
        L.colLog0 = l0; L.colDlog = dl; L.colVersion = 1;
        const Vec3 N = discAxisView(v);
        L.discAxis[0] = static_cast<float>(N.x); L.discAxis[1] = static_cast<float>(N.y); L.discAxis[2] = static_cast<float>(N.z);
        L.rO = s.dist;
        const DiscGeom g = discGeom(s);
        L.rIn = g.rIn; L.rOut = g.rOut;
        const DiscPhys p = discPhys(s);
        L.tScale = std::pow(p.fluxScale() / stefanBoltzmann(), 0.25);       // T = tScale F(r)^(1/4), F for Mdot = M = 1 (disc.h)
        L.tScale *= heldCameraShift(s, 0);                                  // 0.84.0: spin 0: the camera's own shift is one factor for every ray (lab.h)
        L.allInvTsun = allLightOn(s) ? 1 / sunTeff() : 0;               // 0.58.0
        discLightFields(L, s, 1 / heldCameraShift(s, 0));                  // 0.87.0
        toneFields(L, s);
        if (lookUse_) { L.whiteMode = 1; L.expStep = 4; }                   // 0.56.0: the card finds the exposure (spin 0 too; 0.70.0: your choice too)
        gasFields(L, s, gasT);                                              // 0.72.0: the moving gas: the travel times, the sky frame's x and y on the view
        L.discTime = &disc_->tValues();
        if (const ReturnFlux* ret = discReturn(s)) {                        // 0.87.0 R11: spin 0's returning flux (kLensFS uRet), ensureDiscF's grid
            if (retFOf_ != ret || !retF_) {
                const double r0 = discInner(0), l0 = std::log(r0), dl = (std::log(1000.0) - l0) / (1024 - 1);
                const int n = 1024 + static_cast<int>(std::ceil((std::log(kOuterMax) - std::log(1000.0)) / dl)) + 1;
                auto f = std::make_shared<std::vector<float>>(static_cast<std::size_t>(n));
                for (int i = 0; i < n; i++) (*f)[static_cast<std::size_t>(i)] = static_cast<float>(ret->fluxAt(std::exp(l0 + i * dl)));
                retF_ = std::move(f); retFOf_ = ret; retFLog0_ = l0; retFDl_ = dl; retFVer_ = nextVersion();
            }
            L.retF = retF_.get(); L.retN = static_cast<int>(retF_->size()); L.retLog0 = retFLog0_; L.retDl = retFDl_; L.retVersion = retFVer_;
        }
        heightFields(L, s, g);                                              // 0.79.0: the disc's true thickness (lab.h discHeight, discThickDrawn)
        {   Vec3 rows[3];
            viewRows(v, rows);
            for (int i = 0; i < 3; i++) { L.gasE1[i] = static_cast<float>(rows[i].x); L.gasE2[i] = static_cast<float>(rows[i].y); } }
        if (glareOn(s) && glareReady_) { L.glareKeep = static_cast<float>(1 - glareShare()); L.glareBlock = glareBlockFn; L.glareVeil = glareVeilFn; }   // 0.55.4
        L.glareAsync = async && !glareNow; L.glareThread = lowerThisThread; L.glareTables = glareCardOn() ? glareTablesFn : nullptr;   // 0.55.5
    }
    return L;
}

int BlackHoleView::lookStep(double W, double H, bool exact) {
    if (exact) return 4;
    return std::max(4, static_cast<int>(std::ceil(4 * std::sqrt(std::max(1.0, W * H) / (1280.0 * 800.0)) - 1e-9)));
}

BlackHoleView::Key BlackHoleView::keyOf(const Settings& s, double W, double H, double cx, double cy) {
    const bool spinning = s.spin > 0 && s.bend;
    const bool disc = spinning ? s.disc : s.disc && s.bend;
    return Key{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expEV, s.expAuto, spinning ? s.spin : 0.0, 0,
               static_cast<int>(s.look), trueSkyOn(s), glareOn(s), allLightOn(s), disc ? discReturn(s) : nullptr};
}

// 0.55.5: the processor's part of a picture (until 0.55.4 the first half of draw): the tables, the exposure and the stars' images, only when the view changed
bool BlackHoleView::prepare(const Settings& s, double W, double H, double cx, double cy, const std::atomic<bool>* cancel) {
    if (s.spin > 0 && s.bend) return prepareSpin(s, W, H, cx, cy, cancel);
    ensureTable(s.dist);
    const HoleView v = holeView(s, W, H, cx, cy);
    st_.shadowPx = s.bend ? (kPi - table_->edge()) * v.K : 0;
    st_.cx = cx; st_.cy = cy;
    const bool disc = s.disc && s.bend;
    if (!s.skyOurs && !disc) { dots_.clear(); key_ = Key{}; lit_ = false; return true; }   // Sky: Black and no disc: nothing is lit, the picture is black
    lit_ = true;
    if (disc) ensureDisc(s.dist, movingEngine_);
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expEV, s.expAuto, 0.0, 0, static_cast<int>(s.look), trueSkyOn(s), glareOn(s), allLightOn(s),
                disc ? discReturn(s) : nullptr};
    const bool changed = !(k == key_) || (exact_ && !keyExact_);
    DiscLook cardLook = skyLook_;                                           // 0.70.0: the card's last exposure, at these settings' look and stops
    const bool cardOk = cardLook.any && finishLook(cardLook, s);
    if (changed && disc && lookEngine_ && cardOk) {                         // 0.56.0: the card finds the exposure; the stars use what it last said
        st_.look = cardLook;                                                // (0.70.0: until the card has said anything, the processor's below)
        st_.lookMs = 0;
        st_.lookFills++;
        made_[0] = s.yaw; made_[1] = s.pitch; made_[2] = s.dist; made_[3] = 0;
    } else if (changed) {                                                   // the exposure first: the shader needs the white
        const auto tl = std::chrono::steady_clock::now();
        st_.look = disc ? discLook(*disc_, s, v, W, H, lookStep(W, H, exact_)) : DiscLook{};
        st_.lookMs = msSince(tl);
        st_.lookFills++;
        made_[0] = s.yaw; made_[1] = s.pitch; made_[2] = s.dist; made_[3] = 0;
    }
    if (!s.skyOurs) { dots_.clear(); key_ = k; keyExact_ = exact_; return true; }
    if (changed) {                                                          // the stars' images only when the view changed
        key_ = k;
        keyExact_ = exact_;
        int hidden = 0;
        const auto t0 = std::chrono::steady_clock::now();
        dots_.clear();
        st_.trueShown = st_.trueHidden = 0;
        trueNumbers(s, v);
        const orbit::Sky& S = orbit::skyData();
        const Vec3 hole = viewMiddle(v);
        const float cap = brightestMag();
        int stars = 0, images = 0, extra = 0, capped = 0;
        std::array<StarSeen, orbit::kSkyColourBuckets> seen;               // 0.84.0: spin 0: the camera's one shift for every image
        for (int b = 0; b < orbit::kSkyColourBuckets; b++) seen[std::size_t(b)] = starSeenBucket(b, s.bend ? heldCameraShift(s, 0) : 1.0);
        // 0.55.6: the stars' images on all cores (each star on its own), then laid down in the catalogue's order as before: the same dots in the same order
        const std::vector<orbit::SkyStar>& all = S.stars;
        const int nStar = static_cast<int>(all.size()), nChunk = (nStar + kStarChunk - 1) / kStarChunk;
        std::vector<std::vector<Cand>> found(static_cast<std::size_t>(nChunk));
        parallelFor(nChunk, [&](int c) {
            std::vector<StarImage> sc;
            std::vector<Cand>& out = found[static_cast<std::size_t>(c)];
            for (int k = c * kStarChunk; k < std::min(nStar, (c + 1) * kStarChunk); k++) {
                const orbit::SkyStar& st = all[static_cast<std::size_t>(k)];
                sc.clear();
                if (s.bend) starImagesFast(*table_, hole, st.dir, st.mag, kImageMagLimit, sc);
                else sc.push_back({st.dir, 1.0, st.mag, 0, 0});
                for (const StarImage& im : sc) {
                    double X, Y;
                    if (!viewScreen(v, im.dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
                    const bool behind = disc && discPixel(*disc_, s, v, X, Y, false).hit.order >= 0;   // behind the opaque disc
                    out.push_back({X, Y, im.mag, k, im.order, behind});
                }
            }
        });
        int last = -1;
        bool any = false;
        for (const std::vector<Cand>& ch : found)
            for (const Cand& q : ch) {
                if (q.star != last) { if (any) stars++; any = false; last = q.star; }
                if (q.hidden) { hidden++; continue; }
                double mag = q.mag;
                if (mag < cap) { mag = cap; capped++; }
                const StarSeen sn = seen[std::size_t(std::clamp(all[static_cast<std::size_t>(q.star)].bucket, 0, orbit::kSkyColourBuckets - 1))];   // 0.84.0
                if (!addDot(s, v, q.X, q.Y, mag + sn.dmag, sn.rgb)) continue;
                images++;
                if (q.order > 0) extra++;
                any = true;
            }
        if (any) stars++;
        st_.stars = stars; st_.images = images; st_.extraImages = extra; st_.capped = capped; st_.hidden = hidden;
        st_.starsMs = msSince(t0);
        st_.starFills++;
    }
    return true;
}

bool BlackHoleView::prepareSpin(const Settings& s, double W, double H, double cx, double cy, const std::atomic<bool>* cancel) {
    static const bool wait = std::getenv("PL_BH_SPINWAIT") != nullptr;      // (automatic checks: the fine table at once, on this thread)
    if (!ensureSpinTable(s, wait, cancel)) return false;
    const HoleView v = holeView(s, W, H, cx, cy);
    const SpinTable& T = *spin_;
    double mean = 0;
    for (int i = 0; i < T.nEdge(); i++) mean += T.edgeSample(i) / T.nEdge();
    st_.shadowPx = mean * v.K;
    st_.cx = cx; st_.cy = cy;
    const bool disc = s.disc;
    if (!s.skyOurs && !disc) { dots_.clear(); key_ = Key{}; lit_ = false; return true; }
    lit_ = true;
    if (disc) ensureDiscF(s.spin);
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expEV, s.expAuto, s.spin, spinVer_, static_cast<int>(s.look), trueSkyOn(s), glareOn(s), allLightOn(s),
                disc ? discReturn(s) : nullptr};
    const bool changed = !(k == key_) || (exact_ && !keyExact_);
    if (changed) {
        made_[0] = s.yaw; made_[1] = s.pitch; made_[2] = s.dist; made_[3] = s.spin;
        st_.lookFills++;
    }
    DiscLook cardLook = skyLook_;                                           // 0.70.0: as prepare
    const bool cardOk = cardLook.any && finishLook(cardLook, s);
    if (changed && disc && skyEngine_ && cardOk) {                          // 0.56.0: the card finds the exposure; the stars use what it last said
        st_.look = cardLook;
        st_.lookMs = 0;
    } else if (changed && disc) {                                           // the exposure first (lab.h discLook's rule, through the spin table)
        const auto tl = std::chrono::steady_clock::now();
        DiscLook L;
        const int step = lookStep(W, H, exact_);                            // 0.55.5: 4 at rest (as before); while moving, no more points than 1280 x 800 in steps of 4
        // 0.55.5: the rows shared out over threads (the largest values are the same in any order: the result is exactly the one-thread scan's)
        std::vector<double> rows;
        for (double Y = step / 2.0; Y < H; Y += step) rows.push_back(Y);
        const int nth = std::max(1, std::min(16, static_cast<int>(std::thread::hardware_concurrency()) - (thisThreadLow() ? 1 : 0)));   // (0.55.6: up to 16, one core left for the frame)
        std::vector<DiscLook> part(static_cast<std::size_t>(nth));
        std::vector<std::vector<double>> ys(static_cast<std::size_t>(nth));   // (0.70.0: each sample's Y, for the log-average)
        const std::function<void(int)> scan = [&](int k) {
            DiscLook& P = part[static_cast<std::size_t>(k)];
            for (std::size_t j = static_cast<std::size_t>(k); j < rows.size(); j += static_cast<std::size_t>(nth))
                for (double X = step / 2.0; X < W; X += step) {
                    const DiscPixel q = spinPixel(s, v, X, rows[j]);
                    if (q.hit.order < 0) continue;
                    P.any = true;
                    P.brightY = std::max(P.brightY, q.Y);
                    P.brightMax = std::max({P.brightMax, q.rgb[0], q.rgb[1], q.rgb[2]});
                    ys[static_cast<std::size_t>(k)].push_back(q.Y);
                }
        };
        {   const bool low = thisThreadLow();
            std::vector<std::thread> pool;
            for (int k = 1; k < nth; k++) pool.emplace_back([&scan, k, low] { if (low) lowerThisThread(); scan(k); });
            scan(0);
            for (std::thread& th : pool) th.join(); }
        for (const DiscLook& P : part) {
            L.any = L.any || P.any;
            L.brightY = std::max(L.brightY, P.brightY);
            L.brightMax = std::max(L.brightMax, P.brightMax);
        }
        if (L.brightY > 0) {                                                // 0.70.0: the log-average (tone.h; lab.cpp discLook's rule)
            const double delta = kToneDelta * L.brightY;
            double sum = 0;
            std::size_t n = 0;
            for (const std::vector<double>& y : ys) { for (double q : y) sum += std::log(delta + q); n += y.size(); }
            L.logAvg = std::exp(sum / static_cast<double>(n));
        }
        if (!finishLook(L, s)) L.white = 1;
        st_.look = L;
        st_.lookMs = msSince(tl);
    } else if (changed) st_.look = DiscLook{};
    if (!s.skyOurs) { dots_.clear(); key_ = k; keyExact_ = exact_; return true; }
    if (changed) {                                                          // the stars' images by inverting the table (spin_sky.h SpinStars)
        key_ = k;
        keyExact_ = exact_;
        int hidden = 0, stars = 0, images = 0, capped = 0;
        const auto t0 = std::chrono::steady_clock::now();
        dots_.clear();
        st_.trueShown = st_.trueHidden = 0;
        trueNumbers(s, v);
        const orbit::Sky& S = orbit::skyData();
        const float cap = brightestMag();
        const double ph = cameraPhi(v), c = std::cos(ph), sn = std::sin(ph);
        Vec3 er, et, ep;
        cameraFrame(v, s.pitch, er, et, ep);
        // 0.55.6: the stars' images on all cores (each star on its own), then laid down in the catalogue's order as before: the same dots in the same order
        const std::vector<orbit::SkyStar>& all = S.stars;
        const int nStar = static_cast<int>(all.size()), nChunk = (nStar + kStarChunk - 1) / kStarChunk;
        std::vector<std::vector<Cand>> found(static_cast<std::size_t>(nChunk));
        const SpinStars& SS = *spinStars_;
        const bool shifts = lookRule(s.look).shifts;                         // 0.84.0 (Film: none)
        parallelFor(nChunk, [&](int ck) {
            std::vector<SpinImage> sc;
            std::vector<Cand>& out = found[static_cast<std::size_t>(ck)];
            for (int k = ck * kStarChunk; k < std::min(nStar, (ck + 1) * kStarChunk); k++) {
                const orbit::SkyStar& st = all[static_cast<std::size_t>(k)];
                const Vec3 d{c * st.dir.x + sn * st.dir.y, -sn * st.dir.x + c * st.dir.y, st.dir.z};   // the sky frame turned back by the camera's phi: the chart's axes
                sc.clear();
                SS.images(d, sc);
                for (const SpinImage& im : sc) {
                    if (!(im.mag > 0)) continue;
                    const double mag = st.mag - 2.5 * std::log10(im.mag);
                    if (mag > kImageMagLimit) continue;
                    const Vec3 N = lookAt(im.psi, im.chi), dir = er * N.x + et * N.y + ep * N.z;
                    double X, Y;
                    if (!viewScreen(v, dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
                    const bool behind = disc && !skyEngine_ && spinPixel(s, v, X, Y, false).hit.order >= 0;   // behind the opaque disc (0.56.0: the card's own disc hides it)
                    const double fb = shifts ? heldCameraShift(s, spin_->lookup(im.psi, im.chi).b) : 1.0;     // 0.84.0: this image's ray's own b
                    out.push_back({X, Y, mag, k, 0, behind, fb});
                }
            }
        });
        int last = -1;
        bool any = false;
        for (const std::vector<Cand>& ch : found)
            for (const Cand& q : ch) {
                if (q.star != last) { if (any) stars++; any = false; last = q.star; }
                if (q.hidden) { hidden++; continue; }
                double m2 = q.mag;
                if (m2 < cap) { m2 = cap; capped++; }
                const StarSeen sn = starSeenBucket(all[static_cast<std::size_t>(q.star)].bucket, q.f);   // 0.84.0
                if (!addDot(s, v, q.X, q.Y, m2 + sn.dmag, sn.rgb)) continue;
                images++;
                any = true;
            }
        if (any) stars++;
        st_.stars = stars; st_.images = images; st_.extraImages = 0; st_.capped = capped; st_.hidden = hidden;
        st_.starsMs = msSince(t0);
        st_.starFills++;
    }
    return true;
}

// hands the prepared picture to the renderer (until 0.55.4 the second half of draw)
void BlackHoleView::emit(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy, const Key& restKey) {
    if (!lit_) return;
    glareRest(restKey);
    if (async && (st_.behind || !shownExact_)) glareReady_ = glareNow;     // 0.55.5: the glare only on the picture of the settings asked for, at rest
    const HoleView v = holeView(s, W, H, cx, cy);
    R.lensSky(lensFor(s, v));
    for (const Dot& d : dots_) R.disc(d.x, d.y, d.r, d.c);
}

void BlackHoleView::draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    const auto t0 = std::chrono::steady_clock::now();
    R.rect(0, 0, W, H, render::Color{0, 0, 0, 1});                          // space: black, whatever the theme's page
    if (async) drawAsync(R, s, W, H, cx, cy);
    else {
        exact_ = true;
        if (s.disc && s.look == Look::Real && discLightRules().returning) returnFluxNow(s.spin);   // 0.87.0: the exact picture with its returning light
        prepare(s, W, H, cx, cy, nullptr);
        shownS_ = s; shownExact_ = true; hasShown_ = true;
        st_.behind = false; st_.behindMs = 0; st_.lagMs = 0;
        std::copy(made_, made_ + 4, st_.made);
        emit(R, s, W, H, cx, cy, key_);
    }
    st_.frameMs = msSince(t0);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.5: the worker

// a request to the worker and the finished picture it hands back: the view's whole state for the frame, of one set of settings
struct BlackHoleView::Request {
    Settings s;
    double W = 0, H = 0, cx = 0, cy = 0;
    bool exact = true;
    std::shared_ptr<const SpinTable> fine;
    Key want;
    std::uint64_t seq = 0;
    std::shared_ptr<CardTableData> card;                                    // 0.55.6: a table filled by the card: the worker makes it (the fine one) first
    bool sky = false;                                                       // 0.56.0: the card draws sky and disc (the worker: the stars only, no exposure scan)
    bool cardLook = false, moving = false;                                  //   the card finds the exposure (also at spin 0); the view is moving (spin 0: the moving disc table)
    DiscLook skyLook;                                                       //   and the card's exposure as last read back (0.70.0: all of it; any false: none yet)
};
struct BlackHoleView::Picture {
    Request req;
    std::shared_ptr<const LensTable> table;
    double tableDist = -1;
    std::uint64_t tableVer = 0;
    std::shared_ptr<const DiscTable> disc;
    double discDist = -1;
    std::uint64_t discVer = 0;
    std::shared_ptr<const SpinTable> spin;
    std::uint64_t spinVer = 0;
    std::shared_ptr<const std::vector<float>> discF;
    double discFSpin = -1, discFLog0 = 0, discFDl = 1;
    std::uint64_t discFVer = 0;
    std::vector<Dot> dots;
    ViewStats st;
    bool lit = false;
    double made[4] = {0, 0, 0, 0};
    double workMs = 0;
    bool discCoarse = false;                                                // 0.56.0: made with the moving disc table (spin 0)
};

void BlackHoleView::ensureWorker() {
    if (engine_) return;
    engine_ = std::make_unique<BlackHoleView>();
    engine_->fineExternal_ = true;
    const std::function<void()> loop = [this] { workLoop(); };
    worker_ = std::thread(loop);
}

void BlackHoleView::workLoop() {
    lowerThisThread();
    BlackHoleView& E = *engine_;
    {   int n = 0; double l0 = 0, dl = 0;                                   // the tables made once, here rather than in the frame that first needs them
        colourTable(n, l0, dl);
        (void)brightestMag(); }
    for (;;) {
        std::unique_ptr<Request> r;
        {   std::unique_lock<std::mutex> lk(workMx_);
            workBusy_ = false;
            workCv_.wait(lk, [this] { return workQuit_ || req_ != nullptr; });
            if (workQuit_) return;
            r = std::move(req_);
            workBusy_ = true;
        }
        const auto t0 = std::chrono::steady_clock::now();
        if (r->card) {                                                      // 0.55.6: the card's rays into a table (read from the card's mapped memory)
            r->fine = r->card->make();
            r->card->used->store(true);
            r->card.reset();
        }
        E.fineOffer_ = r->fine;
        E.exact_ = r->exact;
        E.skyEngine_ = r->sky; E.skyLook_ = r->skyLook; E.lookEngine_ = r->cardLook; E.movingEngine_ = r->moving;
        if (!E.prepare(r->s, r->W, r->H, r->cx, r->cy, &stale_)) continue;  // (stopped: the view is closing)
        auto p = std::make_unique<Picture>();
        p->table = E.table_; p->tableDist = E.tableDist_; p->tableVer = E.tableVer_;
        p->disc = E.disc_; p->discDist = E.discDist_; p->discVer = E.discVer_;
        p->spin = E.spin_; p->spinVer = E.spinVer_;
        p->discF = E.discF_; p->discFSpin = E.discFSpin_; p->discFLog0 = E.discFLog0_; p->discFDl = E.discFDl_; p->discFVer = E.discFVer_;
        p->dots = E.dots_;
        p->st = E.st_;
        p->lit = E.lit_;
        std::copy(E.made_, E.made_ + 4, p->made);
        p->workMs = msSince(t0);
        p->discCoarse = E.discCoarse_ && p->disc != nullptr;
        p->req = std::move(*r);
        std::unique_ptr<Picture> old;                                       // (a picture the frame did not take is freed here, outside the lock)
        {   std::lock_guard<std::mutex> g(workMx_);
            old = std::move(done_);
            done_ = std::move(p); }
    }
}

// 0.55.9 (automatic checks, PL_BH_DISCSHIFT=1): when the picture at rest (the fine table) replaces a moving one of the same spin, distance and tilt (the
// moving table): where the disc is on the picture under each (spinPixel, the lookup the card does), every second pixel both ways: how many pixels differ,
// and how far each differing one is from the nearest pixel where the picture at rest changes between disc and no disc (its outline): the largest and the
// mean, in pixels. (0.55.8: seen nearly edge on, the moving table's 32 rows missed the disc's thin near side; REPORT 0.55.9.)
namespace {
void discShift(const BlackHoleView& v, const SpinTable& moving, const SpinTable& rest, const Settings& s, double W, double H, double cx, double cy) {
    // 0.56.0: every pixel (was every second), and the largest connected patch (4-neighbours) of the pixels that change between disc and no disc
    const HoleView hv = holeView(s, W, H, cx, cy);
    const DiscGeom g = discGeom(s);
    const int nx = static_cast<int>(W), ny = static_cast<int>(H);
    std::vector<signed char> mr(static_cast<std::size_t>(nx) * ny), mm(mr.size());
    const auto on = [&](const SpinTable& T, double X, double Y) {
        const double dx = X - hv.cx, dy = hv.cy - Y, d = std::hypot(dx, dy), th = d / hv.K;
        if (!(th <= kPi)) return false;
        double chi = std::atan2(-(d > 0 ? dx / d : 0), d > 0 ? dy / d : 1);
        if (chi < 0) chi += 2 * kPi;
        return spinDisc(T.lookup(th, chi), s.spin, g.rIn, g.rOut, surfaceOf(g)).order >= 0;
    };
    parallelFor(ny, [&](int j) {
        for (int i = 0; i < nx; i++) {
            const std::size_t k = static_cast<std::size_t>(j) * nx + i;
            mr[k] = on(rest, i + 0.5, j + 0.5); mm[k] = on(moving, i + 0.5, j + 0.5);
        }
    });
    long pix = 0, differ = 0;
    for (std::size_t k = 0; k < mr.size(); k++) { pix += mr[k]; differ += mr[k] != mm[k]; }
    double worst = 0, sum = 0;
    std::vector<int> stack;
    std::vector<char> seen(mr.size(), 0);
    long patch = 0;
    for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) {
        const std::size_t k = static_cast<std::size_t>(j) * nx + i;
        if (mr[k] == mm[k]) continue;
        double best = 1e9;                                                  // the nearest pixel of the other kind in the picture at rest (its outline)
        for (int r = 1; r <= 40 && best > r - 1; r++)
            for (int jj = std::max(0, j - r); jj <= std::min(ny - 1, j + r); jj++) for (int ii = std::max(0, i - r); ii <= std::min(nx - 1, i + r); ii++)
                if (mr[static_cast<std::size_t>(jj) * nx + ii] != mr[k]) best = std::min(best, std::hypot(ii - i, jj - j));
        best = std::min(best, 40.0) - 0.5;                                  // (the outline lies half a pixel past the pixel of the other kind)
        worst = std::max(worst, best); sum += best;
        if (seen[k]) continue;
        long n = 0;
        stack.assign(1, static_cast<int>(k)); seen[k] = 1;
        while (!stack.empty()) {
            const int q = stack.back(); stack.pop_back(); n++;
            const int qi = q % nx, qj = q / nx;
            const int nb[4][2] = {{qi - 1, qj}, {qi + 1, qj}, {qi, qj - 1}, {qi, qj + 1}};
            for (const auto& p : nb) {
                if (p[0] < 0 || p[0] >= nx || p[1] < 0 || p[1] >= ny) continue;
                const std::size_t m = static_cast<std::size_t>(p[1]) * nx + p[0];
                if (!seen[m] && mr[m] != mm[m]) { seen[m] = 1; stack.push_back(static_cast<int>(m)); }
            }
        }
        patch = std::max(patch, n);
    }
    (void)v;
    if (const char* dump = std::getenv("PL_BH_DISCSHIFTDUMP")) {           // (tests: the two masks as a picture: rest white, moving red, both yellow)
        std::FILE* f = std::fopen(dump, "wb");
        if (f) {
            std::fprintf(f, "P6 %d %d 255\n", nx, ny);
            for (std::size_t k = 0; k < mr.size(); k++) { const unsigned char c[3] = {static_cast<unsigned char>(mr[k] || mm[k] ? 255 : 0), static_cast<unsigned char>(mr[k] ? 255 : 0), static_cast<unsigned char>(mr[k] && !mm[k] ? 255 : 0)}; std::fwrite(c, 1, 3, f); }
            std::fclose(f);
        }
    }
    std::printf("PLBHDISCSHIFT a=%g dist=%g pitch=%.6f look=%d disc=%ld differ=%ld maxpx=%.1f meanpx=%.2f patch=%ld moving=%dx%d\n", s.spin, s.dist, s.pitch, static_cast<int>(s.look),
                pix, differ, worst, differ ? sum / differ : 0.0, patch, moving.nChi(), moving.nS());
    std::fflush(stdout);
}
}  // namespace

void BlackHoleView::install(Picture& p) {
    static const bool shift = std::getenv("PL_BH_DISCSHIFT") != nullptr;
    if (shift && spin_ && p.spin && p.spin != spin_ && !spin_->filledByCard() && spin_->nChi() != kSpinChiFine && p.spin->nChi() == kSpinChiFine && p.req.s.disc &&
        spin_->spin() == p.spin->spin() && spin_->rC() == p.spin->rC() && spin_->thC() == p.spin->thC())
        discShift(*this, *spin_, *p.spin, p.req.s, p.req.W, p.req.H, p.req.cx, p.req.cy);
    table_ = std::move(p.table); tableDist_ = p.tableDist; tableVer_ = p.tableVer;
    disc_ = std::move(p.disc); discDist_ = p.discDist; discVer_ = p.discVer;
    spin_ = std::move(p.spin); spinVer_ = p.spinVer;
    discF_ = std::move(p.discF); discFSpin_ = p.discFSpin; discFLog0_ = p.discFLog0; discFDl_ = p.discFDl; discFVer_ = p.discFVer;
    dots_ = std::move(p.dots);
    st_ = p.st;
    st_.workMs = p.workMs;
    lit_ = p.lit;
    std::copy(p.made, p.made + 4, made_);
    shownS_ = p.req.s; shownW_ = p.req.W; shownH_ = p.req.H; shownCx_ = p.req.cx; shownCy_ = p.req.cy;
    shownKey_ = p.req.want; shownExact_ = p.req.exact; shownSeq_ = p.req.seq;
    shownWanted_ = -1;                                                  // 0.83.0: when its view was last asked for (lagMs)
    for (int i = 0, at = wantsAt_; i < wantsN_; i++, at = (at + int(wants_.size()) - 1) % int(wants_.size()))
        if (wants_[std::size_t(at)].k == shownKey_) { shownWanted_ = wants_[std::size_t(at)].last; break; }
    hasShown_ = true;
    installs_++;
    shownDiscCoarse_ = p.discCoarse;
    st_.spinCard = spin_ && spin_->filledByCard() && p.req.s.spin > 0 && p.req.s.bend;
    if (st_.spinCard) cardFine_ = spin_;                                    // 0.55.6: later pictures of the same spin, distance and tilt use it (turning)
}

// the fine spin table (async): made on a thread of its own, below normal priority, once the spin, distance and tilt have rested kSpinRest seconds;
// dropped when they change (the thread stops at its next ray and is joined once it has, never waited for)
void BlackHoleView::pollFine(const Settings& s) {
    const bool want = s.spin > 0 && s.bend;
    const bool same = want && fineA_ == s.spin && fineR_ == s.dist && fineTh_ == s.pitch;
    if (!same) {
        if (spinWorker_.joinable()) spinCancel_ = true;
        fineA_ = want ? s.spin : -1; fineR_ = s.dist; fineTh_ = s.pitch;
        spinChangedAt_ = nowSeconds();
        spinFineStarted_ = false;
    }
    if (spinWorker_.joinable() && spinDone_) {
        { app::FrameCostSpan w(app::FrameCostKind::WorkerWait); spinWorker_.join(); }   // (0.63.1)
        spinDone_ = false;
        spinCancel_ = false;
        std::shared_ptr<const SpinTable> f;
        { std::lock_guard<std::mutex> g(spinMx_); f = std::move(spinReady_); }
        if (f && want && f->spin() == fineA_ && f->rC() == fineR_ && f->thC() == fineTh_) fine_ = std::move(f);
    }
    const bool have = want && fine_ && fine_->spin() == s.spin && fine_->rC() == s.dist && fine_->thC() == s.pitch;
    if (want && !have && !spinFineStarted_ && !spinWorker_.joinable() && nowSeconds() - spinChangedAt_ >= kSpinRest) {
        spinFineStarted_ = true;
        spinDone_ = false;
        spinCancel_ = false;
        const double a = s.spin, r = s.dist, th = s.pitch;
        const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
        const SpinFill fill = fineFill;
        const std::function<void()> job = [this, a, r, th, threads, fill] {
            lowerThisThread();
            auto f = std::make_shared<SpinTable>(a, r, th, kSpinChiFine, kSpinSFine, kSpinEdge, &spinCancel_, threads, fill);
            if (f->ok()) { std::lock_guard<std::mutex> g(spinMx_); spinReady_ = std::move(f); }
            spinDone_ = true;
        };
        spinWorker_ = std::thread(job);
    }
}

bool BlackHoleView::settled() {
    if (!async) return true;
    if (st_.behind || spinWorker_.joinable() || (fineA_ > 0 && !st_.spinFine && !st_.spinCard) || (cardFill_ && !cardFill_->idle()) || cardState_ == 2) return false;
    if (retJob_ || retThreadBusy_ || (hasShown_ && shownS_.disc && shownS_.look == Look::Real && discLightRules().returning && !returnFluxFor(shownS_.spin))) return false;   // 0.87.0
    std::lock_guard<std::mutex> g(workMx_);
    return !workBusy_ && !req_ && !done_;
}

void BlackHoleView::makeFineNow(const Settings& s) {
    if (!async) { ensureSpin(s, true); return; }
    if (gpuPaths && cardState_ >= 0) return;                               // 0.55.6: the card's path fills its own (if its check fails, the fine thread does)
    if (!(s.spin > 0 && s.bend)) return;
    fine_ = std::make_shared<SpinTable>(s.spin, s.dist, s.pitch, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, fineFill);
    fineA_ = s.spin; fineR_ = s.dist; fineTh_ = s.pitch;
    spinFineStarted_ = true;
}

// 0.55.6: the card's path, decided once: the card's program made (0.62.0: Vulkan's), and the start-up check on this card (spin_card.h CardStartup).
// 0.55.7: within the guard (app/gpu_guard.h): off at once when a past run died inside the path (or a step ran out of time) with this app version and driver:
// the shader is not compiled; nothing of it before the view's third frame (the first frames are on screen, the window usable); the check runs a step per
// frame and a step that does not finish within cardStepLimitMs() switches the path off
namespace {
struct GuardSteps : render::KerrSteps {
    void begin(const std::string& step) override { app::gpuGuard().begin(step); }
    void end(const std::string& step, const std::string& result) override { app::gpuGuard().end(step, result); }
};
}  // namespace

void BlackHoleView::cardOff(const std::string& why) {
    cardState_ = -1;
    cardWhy_ = why;
    std::printf("PLBHCARD state=%d checkms=%.1f edge=%.2e dir=%.2e b=%.2e u=%.2e ends=%d why=%s\n", cardState_, cardCheck_.ms, cardCheck_.edge, cardCheck_.dir,
                cardCheck_.b, cardCheck_.u, cardCheck_.endsDiffer, cardWhy_.empty() ? "-" : cardWhy_.c_str());
}

bool BlackHoleView::cardReady(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuGuard();
    if (guardGen_ != G.generation()) {                                     // Settings, Performance, "Try again": the path is tried afresh
        guardGen_ = G.generation();
        if (cardState_ == -1) { cardState_ = 0; cardWhy_.clear(); cardCheck_ = CardCheck{}; cardStartup_.reset(); if (cardFill_) cardFill_->clearLate(); }
    }
    if (cardState_ == 0) {
        if (G.started() && !G.allowed()) { cardOff(G.offText().empty() ? std::string("the graphics card's light paths are switched off") : G.offText()); return false; }
        if (++asyncDraws_ < 3) return false;                               // (the first frames on screen first)
        if (!G.hold()) return false;                                       // 0.63.0: its steps (the pipeline, the check, the first fill) wait for the guards' turn
        static GuardSteps steps;
        render::IKerrCompute* K = R.kerrCompute(&steps);
        if (!K) { G.finished(false, R.kerrWhy()); cardOff(R.kerrWhy()); return false; }
        if (K->building()) return false;                                   // 0.62.0 (Vulkan): its pipeline is made on a thread of its own; asked again next frame
        cardStartup_ = std::make_unique<CardStartup>(*K, &G, cardStepLimitMs());
        cardState_ = 2;
    }
    if (cardState_ == 2) {
        const int r = cardStartup_->pump(false);
        if (r == 0) return false;
        cardCheck_ = cardStartup_->check();
        const std::string late = cardStartup_->lateStep();
        cardStartup_.reset();
        static const bool forced = std::getenv("PL_BH_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced && r == 1) { cardCheck_.ok = false; cardCheck_.why = "the check was made to fail (test switch PL_BH_CARDFAIL)"; }
        if (r == -2) { G.timedOut(late); cardOff(cardCheck_.why); return false; }
        if (!cardCheck_.ok) { G.finished(false, cardCheck_.why); cardOff(cardCheck_.why); return false; }
        cardState_ = 1;
        cardWhy_.clear();
        G.begin("between the check and the first fill");
        if (!cardFill_) cardFill_ = std::make_unique<CardFill>(*R.kerrCompute(), &G, cardStepLimitMs());
        std::printf("PLBHCARD state=%d checkms=%.1f edge=%.2e dir=%.2e b=%.2e u=%.2e ends=%d why=-\n", cardState_, cardCheck_.ms, cardCheck_.edge, cardCheck_.dir,
                    cardCheck_.b, cardCheck_.u, cardCheck_.endsDiffer);
    }
    return cardState_ == 1;
}

// 0.55.6: the card's fill, as far as the card has got (never waits): a finished table goes to the worker with the settings it was started for; then the
// next fill starts if the settings' spin, distance or tilt have no table yet (none is under way or with the worker for them)
void BlackHoleView::pumpCard(const Settings& s, double W, double H, double cx, double cy, bool start) {
    static const std::array<int, 3> grid = [] {
        std::array<int, 3> g{kSpinChiFine, kSpinSFine, kSpinEdge};
        if (const char* e = std::getenv("PL_BH_CARDGRID")) std::sscanf(e, "%d,%d,%d", &g[0], &g[1], &g[2]);   // (tests: a smaller grid on the software renderer)
        return g;
    }();
    for (int guard = 0; guard < 4; guard++) {
        std::shared_ptr<CardTableData> d = cardFill_->pump();
        if (d && !start) { d.reset(); continue; }                           // (the view left the card's path meanwhile: dropped, let go at the next pump)
        if (d) {
            auto r = std::make_unique<Request>();
            r->s = d->s; r->W = d->W; r->H = d->H; r->cx = d->cx; r->cy = d->cy;
            r->exact = lookStep(d->W, d->H, false) == 4;
            r->want = keyOf(d->s, d->W, d->H, d->cx, d->cy);
            r->seq = ++reqSeq_;
            r->card = d;
            cardPosted_ = d; cardPostedUsed_ = d->used;
            cardPostedKey_[0] = d->s.spin; cardPostedKey_[1] = d->s.dist; cardPostedKey_[2] = d->s.pitch;
            posted_ = true; postedKey_ = r->want; postedExact_ = r->exact; postedFine_ = nullptr;
            {   std::lock_guard<std::mutex> g(workMx_);
                req_ = std::move(r); }
            workCv_.notify_one();
            continue;
        }
        if (!cardFill_->idle() || !start || retQueued()) break;            // (0.87.0: not while a piece of the returning light is on the card)
        const bool have = cardFine_ && cardFine_->spin() == s.spin && cardFine_->rC() == s.dist && cardFine_->thC() == s.pitch;
        const bool withWorker = cardPostedKey_[0] == s.spin && cardPostedKey_[1] == s.dist && cardPostedKey_[2] == s.pitch &&
                                (!cardPosted_.expired() || (cardPostedUsed_ && cardPostedUsed_->load()));
        if (have || withWorker) break;
        cardFill_->start(s, W, H, cx, cy, grid[0], grid[1], grid[2]);
    }
}

void BlackHoleView::drawAsync(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    ensureWorker();
    const bool spinning = s.spin > 0 && s.bend;
    const bool card = spinning && gpuPaths && cardReady(R);                // 0.55.6: the card fills the table (no coarse table, no fine thread)
    const bool cardWait = spinning && gpuPaths && (cardState_ == 0 || cardState_ == 2);   // 0.55.7: its check is under way (or starts in a frame or two)
    if (!card && !cardWait) pollFine(s);
    const double now = nowSeconds();
    const Key want = keyOf(s, W, H, cx, cy);
    if (!(want == wantKey_)) { wantKey_ = want; wantSince_ = now; }
    if (wantsN_ == 0 || !(wants_[std::size_t(wantsAt_)].k == want)) {   // 0.83.0: the views asked for, newest last (lagMs)
        wantsAt_ = (wantsAt_ + 1) % int(wants_.size()); wants_[std::size_t(wantsAt_)].k = want; wantsN_ = std::min(wantsN_ + 1, int(wants_.size()));
    }
    wants_[std::size_t(wantsAt_)].last = now;
    const bool sky = spinning && gpuSky && skyReady(R);                    // 0.56.0: sky and disc per pixel on the card
    skyUse_ = sky;
    if (sky) {
        ensureSkyF(s.spin);
        if (!(want == skyKey_)) { skyKey_ = want; skyChangedAt_ = now; }
        skyMoving_ = now - skyChangedAt_ < kSkyRest;
    }
    const bool exact = lookStep(W, H, false) == 4 || now - wantSince_ >= kGlareRest;   // the exposure in steps of 4 once the view rests
    const std::shared_ptr<const SpinTable>& have = card ? cardFine_ : fine_;
    std::shared_ptr<const SpinTable> fine = spinning && have && have->spin() == s.spin && have->rC() == s.dist && have->thC() == s.pitch ? have : nullptr;
    std::unique_ptr<Picture> p;
    {   std::lock_guard<std::mutex> g(workMx_);
        if (done_) p = std::move(done_); }
    if (p) install(*p);
    if (card && !fine && cardFine_ && cardFine_->spin() == s.spin && cardFine_->rC() == s.dist && cardFine_->thC() == s.pitch) fine = cardFine_;   // (just installed)
    if (cardFill_) {                                                        // (also when the card's path is not in use now: a finished fill must be let go)
        pumpCard(s, W, H, cx, cy, card);
        st_.cardMs = cardFill_->lastMs(); st_.cardFills = cardFill_->fills();
        if (!cardFill_->lateStep().empty() && cardState_ == 1) {           // 0.55.7: a step of a fill ran out of time: the processor's path from now on
            app::gpuGuard().timedOut(cardFill_->lateStep());
            cardOff("a step of the card's fill did not finish in time (" + cardFill_->lateStep() + ")");
        } else if (cardState_ == 1 && cardFill_->fills() > 0 && cardFill_->lastMs() > (cardFill_->fills() == 1 ? kCardFirstFillSlowMs : kCardFillSlowMs)) {
            // 0.85.3: a card whose double precision is slow (the owner's RX 9060 XT: 147,456 rays in 1,856 ms) holds the GPU's queue for the whole fill,
            // so frames wait (blackhole.close_spin: worst frame 1,780 ms, 3.6 fps) and each new view waits for a whole fill. The processor's table
            // gives a first picture in tens of ms there. So a fill slower than kCardFillSlowMs keeps the light paths off for this shader and driver.
            char b[200];
            std::snprintf(b, sizeof b, "the whole fill took %.0f ms (more than %.0f ms): the processor's table is faster on this card", cardFill_->lastMs(),
                          cardFill_->fills() == 1 ? kCardFirstFillSlowMs : kCardFillSlowMs);
            app::gpuGuard().timedOut(b);
            cardOff(b);
        }
    }
    pumpReturn(R, s, cardState_ == 1 && cardFill_ != nullptr);              // 0.87.0 R11: the returning light's table (card or processor)
    const bool current = hasShown_ && shownKey_ == want;
    const bool movingNow = now - wantSince_ < kGlareRest;
    const bool cardLook = sky || (!spinning && gpuSky && s.disc && s.bend);   // 0.56.0: the exposure on the card (also at spin 0)
    lookUse_ = cardLook;
    const bool upToDate = current && (shownExact_ || !exact) && (!fine || st_.spinFine) && !(shownDiscCoarse_ && !movingNow);
    const bool asked = posted_ && postedKey_ == want && (postedExact_ || !exact) && postedFine_ == fine.get() && !(postedMoving_ && !movingNow);
    if (!upToDate && !asked && !((card || cardWait) && !fine)) {                         // (card: a view of a new spin, distance or tilt is the card's to start)
        auto r = std::make_unique<Request>();
        r->s = s; r->W = W; r->H = H; r->cx = cx; r->cy = cy; r->exact = exact; r->fine = fine; r->want = want; r->seq = ++reqSeq_;
        r->sky = sky; r->skyLook = cardReadLook(R.skyStats()); r->cardLook = cardLook; r->moving = movingNow;
        posted_ = true; postedKey_ = want; postedExact_ = exact; postedFine_ = fine.get(); postedMoving_ = movingNow;
        {   std::lock_guard<std::mutex> g(workMx_);
            req_ = std::move(r); }                                          // (a request not yet taken is replaced: the newest wins)
        workCv_.notify_one();
    }
    st_.behind = !current;
    if (!st_.behind) behindSince_ = 0;
    else if (behindSince_ == 0) behindSince_ = now;
    st_.behindMs = st_.behind ? 1000 * (now - behindSince_) : 0;
    st_.lagMs = !st_.behind ? 0 : 1000 * (now - (shownWanted_ >= 0 ? std::max(shownWanted_, behindSince_) : behindSince_));   // 0.83.0
    st_.picture = shownSeq_;
    std::copy(made_, made_ + 4, st_.made);
    const render::Renderer2D::SkyStats& ks = R.skyStats();
    st_.skyCard = sky; st_.skyGpuMs = ks.fullMs; st_.skyScale = ks.scale; st_.skyPictures = ks.draws; st_.starPictures = installs_;
    st_.lookCard = cardLook && s.disc;
    if (st_.lookCard) { DiscLook c = cardReadLook(ks); if (c.any && finishLook(c, s)) st_.look = c; }   // (0.70.0: the whole exposure, your stops too)
    if (sky) {                                                              // 0.56.0: the sky and the disc of the settings asked for, the stars of the last picture
        glareRest(want);
        if (skyMoving_) glareReady_ = glareNow;
        R.lensSky(lensFor(s, holeView(s, W, H, cx, cy)));
        if (hasShown_ && lit_) for (const Dot& d : dots_) R.maskedDisc(d.x, d.y, d.r, d.c);
        return;
    }
    if (hasShown_) emit(R, shownS_, shownW_, shownH_, shownCx_, shownCy_, want);
}

// 0.56.0: the sky's path, decided once per run (and again after "Try again"): within its own guard (app/gpu_guard.h gpuSkyGuard), off at once when a past
// run died inside it with this sky shader and driver; nothing of it before the view's third frame; then a step per frame, each guarded and logged with its
// time: the three shaders made in rising order (the tiny one, the elliptic functions alone, the whole), a first small draw (the check's first probe,
// read back without waiting), the check (the second probe; both against kerr_closed.h in double: sky_card.h skyCheckOf), the first full picture (until the
// card's timer has come back for it). A step that does not finish within cardStepLimitMs() switches the path off (kept as "slow").
namespace {
struct SkySteps : render::KerrSteps {
    void begin(const std::string& step) override { app::gpuSkyGuard().begin(step); }
    void end(const std::string& step, const std::string& result) override { app::gpuSkyGuard().end(step, result); }
};
}  // namespace

void BlackHoleView::skyOff(const std::string& why) {
    skyState_ = -1;
    skyWhy_ = why;
    std::printf("PLBHSKY state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

bool BlackHoleView::skyReady(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuSkyGuard();
    if (skyGen_ != G.generation()) {
        skyGen_ = G.generation();
        if (skyState_ == -1) { skyState_ = 0; skyWhy_.clear(); skyCheck_ = SkyCheck{}; }
    }
    const double now = nowSeconds(), limit = cardStepLimitMs() / 1000;
    const auto stepText = [&](const char* what) { char b[96]; std::snprintf(b, sizeof b, "%s after %.0f ms", what, 1000 * (now - skyStepAt_)); return std::string(b); };
    if (skyState_ == 0) {
        if (G.started() && !G.allowed()) { skyOff(G.offText().empty() ? std::string("the spinning hole on the graphics card is switched off") : G.offText()); return false; }
        if (++skyFrames_ < 3) return false;                                // (the first frames on screen first)
        // 0.85.3: the sky's first full picture is drawn by the lens pass. Its pipeline is made on a worker under the lens guard, which waits for the
        // guards' turn. Holding the turn before that pipeline is made was a circular wait: the sky held the turn until its first picture, the lens
        // pipeline waited for the turn, and nothing was drawn until the sky's 10 s limit (native/docs/hardware/pc_0.85.2.md C1).
        // So the sky takes the turn only once the lens pipeline is made.
        const int lens = R.skyLensState();
        if (lens == -1) { skyOff("the lens pass's pipeline could not be made, so the card's sky cannot be shown"); return false; }
        if (lens != 2) return false;                                       // (asked again next frame; the turn is not held)
        if (!G.hold()) return false;                                       // 0.63.0: its steps wait for the guards' turn (a later frame)
        skyState_ = 2; skyStep_ = 0;
        // 0.57.2: the check's reference (traceClosed at the probes' 3,840 pixels, about 13 ms here) on a thread of its own while the card builds and draws: the
        // frame only compares (it was made on the frame, one frame of 25-48 ms in blackhole.film_tilt: REPORT 0.57.2). Made once per run (the cases are fixed).
        if (!skyRefs_ && !skyRefJob_.valid())
            skyRefJob_ = std::async(std::launch::async, [] {
                lowerThisThread();
                auto r = std::make_shared<std::array<SkyRef, 2>>();
                for (int k = 0; k < 2; k++) (*r)[static_cast<std::size_t>(k)] = skyReference(skyCheckCase(k));
                return std::shared_ptr<const std::array<SkyRef, 2>>(std::move(r));
            });
    }
    if (skyState_ == 2) {
        static SkySteps steps;
        std::string why;
        if (skyStep_ <= 2) {
            if (!R.kerrSky().build(skyStep_, &steps, why)) { G.finished(false, why); skyOff(why); return false; }
            if (R.kerrSky().building()) return false;                      // 0.62.0 (Vulkan): the stage's pipeline is made on a thread of its own
            skyStep_++;
            return false;
        }
        if (skyStep_ == 3) {
            G.begin("a first small draw");
            R.skyProbe(skyProbeLens(skyCheckCase(0)), kSkyCheckW, kSkyCheckH, true, nullptr);
            skyStepAt_ = now; skyStep_ = 4;
            return false;
        }
        if (skyStep_ == 4 || skyStep_ == 5) {
            const char* name = skyStep_ == 4 ? "a first small draw" : "the check";
            if (!R.skyProbeDone()) {
                if (now - skyStepAt_ > limit) { G.timedOut(name); skyOff(std::string("a step of the card's sky did not finish in time (") + name + ")"); }
                return false;
            }
            R.skyProbeRead(skyProbe_[skyStep_ - 4]);
            if (skyStep_ == 4) {
                G.end(name, stepText("read back"));
                G.begin("the check");
                R.skyProbe(skyProbeLens(skyCheckCase(1)), kSkyCheckW, kSkyCheckH, true, nullptr);
                skyStepAt_ = now; skyStep_ = 5;
                return false;
            }
            skyReadText_ = stepText("read back");
            skyStep_ = 6;
        }
        if (skyStep_ == 6) {                                               // 0.57.2: the reference made on its thread? (never waited for: the next frame asks again)
            const char* name = "the check";
            if (!skyRefs_) {
                if (!skyRefJob_.valid() || skyRefJob_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
                skyRefs_ = skyRefJob_.get();
            }
            const std::vector<float>* p[2] = {&skyProbe_[0], &skyProbe_[1]};
            skyCheck_ = skyCheckOf(p, skyRefs_->data());
            static const bool forced = std::getenv("PL_BH_SKYFAIL") != nullptr;   // (tests: as if the check had failed)
            if (forced) { skyCheck_.ok = false; skyCheck_.why = "the check was made to fail (test switch PL_BH_SKYFAIL)"; }
            char b[600];
            std::snprintf(b, sizeof b, "%s: %s; far direction %.1e / %.1e rad, b %.1e / %.1e, disc r %.1e / %.1e, g %.1e / %.1e (benchmark / Film); light %.2f / %.2f of "
                          "its limit (eye / all light); %.0f ms on the processor",
                          skyReadText_.c_str(), skyCheck_.ok ? "passed" : "FAILED", skyCheck_.m[0].dir, skyCheck_.m[1].dir, skyCheck_.m[0].b, skyCheck_.m[1].b,
                          skyCheck_.m[0].r, skyCheck_.m[1].r, skyCheck_.m[0].g, skyCheck_.m[1].g, skyCheck_.light[0].worst, skyCheck_.light[1].worst, skyCheck_.ms);
            G.end(name, b);
            for (int k = 0; k < 2; k++) std::printf("PLBHSKYCHECKCASE %s\n", skyCompareText(skyCheckCase(k), skyCheck_.m[k]).c_str());
            std::printf("PLBHSKYCHECKLIGHT eyepx=%ld eyebad=%ld eyeworst=%.3f allpx=%ld allbad=%ld allworst=%.3f\n", skyCheck_.light[0].px, skyCheck_.light[0].bad,
                        skyCheck_.light[0].worst, skyCheck_.light[1].px, skyCheck_.light[1].bad, skyCheck_.light[1].worst);
            std::printf("PLBHSKYCHECKTONE px=%ld bad=%ld worst=%.3f\n", skyCheck_.tone.px, skyCheck_.tone.bad, skyCheck_.tone.worst);   // 0.70.0
            if (!skyCheck_.ok) { G.finished(false, skyCheck_.why); skyOff(skyCheck_.why); return false; }
            G.begin("the first full picture");
            skyTimed0_ = R.skyStats().timed;
            skyStepAt_ = now; skyFirst_ = true;
            skyState_ = 1;
            std::printf("PLBHSKY state=1 why=-\n");
            std::fflush(stdout);
        }
    }
    if (skyState_ == 1 && skyFirst_) {
        if (R.skyStats().timed > skyTimed0_) {
            char b[128];
            std::snprintf(b, sizeof b, "drawn, %.1f ms on the card for the whole picture", R.skyStats().fullMs);
            G.end("the first full picture", stepText(b));
            G.finished(true);
            skyFirst_ = false;
        } else if (now - skyStepAt_ > limit) { G.timedOut("the first full picture"); skyOff("a step of the card's sky did not finish in time (the first full picture)"); return false; }
    }
    return skyState_ == 1;
}

void BlackHoleView::ensureSkyF(double spin) {
    const ReturnFlux* ret = retFor(spin);
    if (skyFSpin_ == spin && skyF_ && skyFRet_ == ret) return;
    skyFRet_ = ret;
    // 0.57's grid kept point for point (1024 values from r_ms to 1000 M; the eye's picture bit for bit), continued with the same step past kOuterMax (0.58.0: 2000)
    const double r0 = discInner(spin), l0 = std::log(r0), dl = (std::log(1000.0) - l0) / (1024 - 1);
    const int n = 1024 + static_cast<int>(std::ceil((std::log(kOuterMax) - std::log(1000.0)) / dl)) + 1;
    auto f = std::make_shared<std::vector<float>>(static_cast<std::size_t>(n));
    for (int i = 0; i < n; i++) (*f)[static_cast<std::size_t>(i)] = static_cast<float>(std::pow(std::max(0.0, fluxSent(std::exp(l0 + i * dl), spin, ret)), 0.25));
    skyF_ = std::move(f);
    skyFSpin_ = spin; skyFLog0_ = l0; skyFDl_ = dl;
    skyFVer_ = nextVersion();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Tests: PL_BH_COMPARE=1
namespace {
// The tests' pictures on Vulkan: what R recorded, drawn off screen by the test shot (main.cpp: the backend's
// renderToImage, one sample) and read back, RGBA, rows from the bottom; the float outputs come from R.lensFloat
struct Fbo { std::vector<unsigned char> px; };
Fbo makeFbo(int, int, bool) { return Fbo(); }
void dropFbo(Fbo&) {}
void endFbo(render::Renderer2D& R, int W, int H, Fbo& f) {
    if (!testShot() || !testShot()(R, W, H, f.px)) f.px.assign(static_cast<std::size_t>(W) * static_cast<std::size_t>(H) * 4, 0);
}
double angleBetween(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)); }
}  // namespace

// 0.55.3: one saved case of the Real look (the feeding preset at spin 0.6 seen from the paper's view, 74.1 M and 1.511 rad, field of view 30 degrees,
// automatic exposure), drawn as the app draws it (sky, disc and stars) into an 8-bit picture: its FNV-1a hash over the RGBA bytes. The test's number was made
// by 0.55.2's code before this job changed anything (REPORT 0.55.3): Real stays the same to the pixel.
namespace {
std::uint64_t realCaseHash(render::Renderer2D& R, int W, int H, const SpinFill& fill, std::vector<unsigned char>* keep) {
    BlackHoleView view;
    view.fineFill = fill;
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    s.outer = 30;                                                       // 0.58.0: the saved case's disc (6 to 30 M, the default until 0.57)
    setSpin(s, 0.6);
    setDistance(s, 74.1);
    s.pitch = 1.511;
    setFov(s, 30);
    s.glare = false;                                                    // 0.55.4: Glare Off (and Sky Shown, the default) is 0.55.3's Real
    view.ensureSpin(s, true);
    Fbo f = makeFbo(W, H, false);
    R.begin(W, H);
    view.draw(R, s, W, H, W / 2.0, H / 2.0);
    endFbo(R, W, H, f);
    std::vector<unsigned char> px(static_cast<std::size_t>(W) * H * 4);
    std::memcpy(px.data(), f.px.data(), f.px.size());
    dropFbo(f);
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : px) { h ^= c; h *= 1099511628211ull; }
    if (keep) *keep = px;
    if (const char* dump = std::getenv("PL_BH_REALDUMP")) {
        if (FILE* fo = std::fopen(dump, "wb")) { std::fwrite(px.data(), 1, px.size(), fo); std::fclose(fo); }
    }
    return h;
}
}  // namespace

// 0.55.6 (PL_BH_COMPARE=card): the card's light paths against the processor's. The shader's double functions against the C++ library's; the start-up check;
// for the benchmark's case, the Film case and spin 0.99, the card's whole fine table against the processor's node by node (the processor's table is the
// reference: blackhole_physics tests it against the integrator); the conservation laws on rays the card followed (the null condition and Carter's q at the end)
namespace {
void cardCompare(render::Renderer2D& R) {
    render::IKerrCompute* K = R.kerrCompute();
    for (int t = 0; K && K->building() && t < 6000; t++) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); K = R.kerrCompute(); }   // 0.69.0: Vulkan makes its pipeline on a thread
    if (!K) { std::printf("PLBHCARDNONE why=%s\n", R.kerrWhy().c_str()); return; }
    {   std::vector<double> x, out;
        for (int k = 0; k < 4000; k++) x.push_back(-40 + 80.0 * k / 3999 + 1e-3 * std::sin(k));   // -40 .. 40 (the angles, the exponents of the step's rule)
        for (int k = 0; k < 400; k++) x.push_back(std::ldexp(1.0 + k / 400.0, -30 + k / 10));       // 1e-9 .. 1e3 (small and large logs)
        K->math(cardParams(0.5, 20, 1.3), x, out);
        double e[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (std::size_t k = 0; k < x.size(); k++) {
            const double v = x[k], ax = std::fabs(v) + 1e-300;
            const double ref[8] = {std::sin(v), std::cos(v), std::exp(v), std::log(ax), std::atan2(v, 1 - v), std::acos(v / (1 + std::fabs(v))), std::cbrt(v), std::pow(ax, -0.2)};
            for (int i = 0; i < 8; i++) {
                const double g = out[8 * k + static_cast<std::size_t>(i)];
                if (!std::isfinite(ref[i]) || ref[i] == 0) continue;                        // (exp beyond 709: the shader stops at 709; it never needs more)
                const double d = i < 2 ? std::fabs(g - ref[i]) : std::fabs(g - ref[i]) / std::fabs(ref[i]);   // sin, cos: absolute; the rest relative
                e[i] = std::max(e[i], std::isfinite(d) ? d : 1.0);
            }
        }
        std::printf("PLBHCARDMATH n=%zu sin=%.2e cos=%.2e exp=%.2e log=%.2e atan2=%.2e acos=%.2e cbrt=%.2e pow=%.2e\n", x.size(), e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
    }
    {   const CardCheck c = cardSelfCheck(*K);
        std::printf("PLBHCARDCHECK ok=%d edges=%d rays=%d ends=%d edge=%.2e dir=%.2e b=%.2e u=%.2e ms=%.1f\n", c.ok ? 1 : 0, c.edges, c.rays, c.endsDiffer, c.edge, c.dir, c.b, c.u, c.ms);
    }
    // 0.87.0 R11: the returning light's table made on the card against the processor's, the same rays (a smaller grid than the lab's, 8 nodes x 16 x 256
    // rays, for the software renderer's time): the flux sent, F + F_o, at 400 radii from r_ms to 2000 M (the limit, 1 %, as test_blackhole.cpp R11 (c))
    for (const double a : {0.0, kSpinTop}) {
        const auto t0 = std::chrono::steady_clock::now();
        int bad = 0;
        const ReturnFlux G = cardReturnFlux(*K, a, 8, 16, 256, &bad);
        const double gms = msSince(t0);
        const ReturnFlux C = returnFluxCpu(a, 8, 16, 256);
        double w = 0;
        for (int k = 0; k <= 400; k++) {
            const double r = discInner(a) * std::exp(std::log(kOuterMax / discInner(a)) * k / 400) * (k == 0 ? 1.0001 : 1.0), fg = discFlux(r, a);
            w = std::max(w, std::fabs((fg + G.fluxAt(r)) / (fg + C.fluxAt(r)) - 1));
        }
        std::printf("PLBHRET a=%g rel=%.3e unanswered=%d fo_card=%.5f fo_cpu=%.5f cardms=%.0f\n", a, w, bad, G.fo.front(), C.fo.front(), gms);
    }
    const FilmChoices film;
    struct Case { const char* name; double a, r, th; };
    const Case cases[3] = {{"benchmark", 0.9, 20, 80 * kPi / 180}, {"film", film.spin, film.camR, film.camThetaDeg * kPi / 180}, {"top", kSpinTop, 6, 1.4}};
    for (const Case& cs : cases) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::shared_ptr<SpinTable> G = cardTableNow(*K, cs.a, cs.r, cs.th, kSpinChiFine, kSpinSFine, kSpinEdge);
        const double gms = msSince(t0);
        const auto t1 = std::chrono::steady_clock::now();
        const SpinTable P(cs.a, cs.r, cs.th, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, SpinFill{});   // (KerrBL, as the card's rays)
        const double pms = msSince(t1);
        if (!G) { std::printf("PLBHCARDT case=%s none=1\n", cs.name); continue; }
        long nodes = 0, ends = 0, lost = 0;
        double dir[3] = {0, 0, 0}, db = 0, du[2] = {0, 0}, de = 0, dc = 0, drow = 0, dsl = 0;   // (0.95.0: dsl, the slopes and u+^(1/2) of the disc's surface)
        long slOne = 0;
        long cnt[3] = {0, 0, 0};
        for (int i = 0; i < kSpinEdge; i++) de = std::max(de, std::fabs(G->edgeSample(i) - P.edgeSample(i)));
        for (int i = 0; i < kSpinChiFine; i++) { dc = std::max(dc, std::fabs(G->rowChi(i) - P.rowChi(i))); drow = std::max(drow, std::fabs(G->rowEdge(i) - P.rowEdge(i))); }
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < kSpinChiFine; i++)
                for (int j = 0; j < kSpinSFine; j++) {
                    const SpinHit &g = G->node(side, i, j), &p = P.node(side, i, j);
                    nodes++;
                    if (g.sky != p.sky) { ends++; continue; }
                    if (!g.sky && length(g.dir) == 0 && side == 0) lost++;
                    const double xpe = std::fabs(P.nodePsi(side, i, j) - P.rowEdge(i)) / P.rowEdge(i);
                    const int bd = xpe < 1e-3 ? 0 : xpe < 0.1 ? 1 : 2;
                    if (g.sky) { dir[bd] = std::max(dir[bd], angleBetween(g.dir, p.dir)); cnt[bd]++; }
                    db = std::max(db, std::fabs(g.b - p.b) / (1 + std::fabs(p.b)));
                    for (int k = 0; k < kSpinCross; k++) {
                        const double d = std::fabs(g.u[k] - p.u[k]) / (1 + std::fabs(p.u[k]));
                        du[bd == 0 ? 1 : 0] = std::max(du[bd == 0 ? 1 : 0], std::isfinite(d) ? d : 1.0);
                    }
                    for (int k = 0; k < kSurfaceOrders; k++) {                  // 0.95.0: where both made the crossing (a crossing one of them did not make: 0 on that side)
                        if (!(g.sl[k] != 0 && p.sl[k] != 0)) continue;
                        const double d = std::fabs(g.sl[k] - p.sl[k]) / (1 + std::fabs(p.sl[k]));
                        dsl = std::max(dsl, std::isfinite(d) ? d : 1.0);
                    }
                    if (g.cn > 0 && p.cn > 0) dsl = std::max(dsl, std::fabs(g.cn - p.cn));
                    else if (g.cn > 0 || p.cn > 0) slOne++;                     // (one side has none: the processor's rays followed by mino.h, its fallback)
                }
        // the conservation laws on 300 of the card's rays (camera sky between 1e-3 of the edge and the far side)
        std::vector<double> pc;
        unsigned seed = 7;
        const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
        for (int k = 0; k < 300; k++) {
            const double chi = 2 * kPi * rnd(), pe = P.edge(chi), side = k % 3 == 0 ? -1 : 1, x = pe * std::pow(10.0, -3 + 3 * rnd());
            const double psi = std::clamp(pe + side * x, 1e-3, kPi - 1e-3);
            pc.push_back(psi); pc.push_back(chi);
        }
        std::vector<CardRay> cr;
        cardRays(*K, cs.a, cs.r, cs.th, pc, cr);
        const KerrBL ch(cs.a);
        double hmax = 0, qmax = 0, hcpu = 0, qcpu = 0;
        int ended = 0;
        const Camera cam{cs.r, cs.th, 0};
        TraceOptions o;
        o.rtol = 1e-10; o.atol = 1e-11;                                           // (the table's tolerance, as spinRay)
        for (std::size_t k = 0; k < cr.size(); k++) {
            const CardRay& c = cr[k];
            if (c.end == 0) continue;
            ended++;
            Ray ray; ray.n = 5; ray.c[0] = c.h.b; ray.c[1] = c.q;
            hmax = std::max(hmax, std::fabs(ch.nullCondition(ray, c.y)));
            qmax = std::max(qmax, std::fabs(ch.carterQ(c.h.b, c.y) - c.q) / (1 + std::fabs(c.q)));
            const Vec3 look = lookAt(pc[2 * k], pc[2 * k + 1]);                    // the processor's same ray, for its own numbers at this tolerance
            Ray r0;
            ch.launch(cam, look, r0);
            const RayEnd e = trace(ch, cam, look, o);
            hcpu = std::max(hcpu, std::fabs(ch.nullCondition(r0, e.y)));
            qcpu = std::max(qcpu, std::fabs(ch.carterQ(r0.c[0], e.y) - r0.c[1]) / (1 + std::fabs(r0.c[1])));
        }
        std::printf("PLBHCARDT case=%s a=%g dist=%g pitch=%.4f nodes=%ld endsdiffer=%ld lost=%ld dirnear=%.2e dirmid=%.2e dirfar=%.2e nnear=%ld nmid=%ld nfar=%ld b=%.2e u=%.2e unear=%.2e "
                    "edge=%.2e rowchi=%.2e rowedge=%.2e mism=%ld rays=%zu ended=%d null=%.2e qdrift=%.2e cpunull=%.2e cpuqdrift=%.2e cardms=%.0f cpums=%.0f sl=%.2e slone=%ld fallbacks=%ld\n",
                    cs.name, cs.a, cs.r, cs.th, nodes, ends, lost, dir[0], dir[1], dir[2], cnt[0], cnt[1], cnt[2], db, du[0], du[1], de, dc, drow, G->mismatches(), cr.size(), ended, hmax, qmax, hcpu, qcpu, gms, pms, dsl, slOne, P.fallbacks());
    }
}
}  // namespace

TestShot& testShot() { static TestShot s; return s; }

// 0.70.0: the Real look's camera curve, the card's picture against the processor's (PL_BH_COMPARE=sky, after the sky's own checks: its pass is ready then)
void toneCompare(render::Renderer2D& R) {
    // every disc pixel of the card's picture (8 bits, Sky: Black, no glare, multisampling off) against the processor's: the double lookup (lab.h discPixel at
    // spin 0, the fine spin table's spinPixel at 0.9) through tone.h toneRGB at the card's exposure as read back, encoded as the shader does; the pixels next
    // to another image or the disc's edge left out (their coverage is the shader's own). Limit: at most 1 % of them over 8 levels of 255 in any channel.
    const int W = 640, H = 400;
    // 0.79.0, case "streaks": the feeding preset at the lab's starting view with its surface, the streaks and the clump at 10^6 M of running (kSkyGasT), read
    // with no footprint (kLensFS uGasFoot 0); the processor's reference: discPixel's surface hit, phi and t at its swept angle (DiscTable::lookupT), gas.h's
    // factor and clump, T x (S (1 + A n))^(1/4) seen at g T. The same limit.
    struct Tc { const char* name; double spin, dist, fov, tiltDeg, mass = 0; };   // (0.87.0: mass, M_sun; 0: the preset's)
    // 0.84.0: the held camera's own shift (lab.h heldCameraShift) at the closest distance (kDistMin) with no spin and the top spin (kSpinTop), and from over
    // the pole (8 degrees from the axis, the top spin): the same limit
    for (const Tc& c : {Tc{"near", 0, 20, 90, 0}, Tc{"far", 0, 50, 45, 85}, Tc{"spin", 0.9, 30, 60, 80}, Tc{"streaks", 0, 50, 45, 0},
                        Tc{"close", 0, kDistMin, kFovMax, 0}, Tc{"closespin", kSpinTop, kDistMin, 150, 0}, Tc{"pole", kSpinTop, kDistMin, 150, 8},
                        Tc{"closetable", kSpinTop, kDistMin, 150, 0},
                        // 0.87.0 (disc.h, THE DISC'S REAL LIGHT): a stellar-mass disc (the colour correction well above 1: R9), and the top spin seen 85 degrees
                        // from the pole through the sky pass and the table pass (the limb darkening's grazing rays: R10)
                        Tc{"hot", 0, 30, 60, 60, 10}, Tc{"edgesky", kSpinTop, 30, 60, 85}, Tc{"edgetable", kSpinTop, 30, 60, 85},
                        // 0.95.0: the disc at its thickness round a spinning hole, 5 degrees above the plane at 20 GM/c^2: the sky pass (the exact surface,
                        // kerr_closed.h) at spins 0.5 and 0.99 and the table pass (the line through each crossing) at 0.99, against the processor's spin
                        // table (spin_sky.cpp spinDisc, the same line)
                        Tc{"thick05", 0.5, 20, 60, 85}, Tc{"thick99", kSpinTop, 20, 60, 85}, Tc{"thick99table", kSpinTop, 20, 60, 85}}) {
        const bool table = std::strcmp(c.name, "closetable") == 0 || std::strcmp(c.name, "edgetable") == 0 || std::strcmp(c.name, "thick99table") == 0;   // 0.84.0: the table's pass (kKerrFS, the fallback without the sky pass), at the processor's exposure
        const bool streaks = std::strcmp(c.name, "streaks") == 0;
        BlackHoleView view;
        Settings u = settingsDefaults();
        applyPreset(u, kFeedingKey);
        u.skyOurs = false; u.glare = false;
        setDistance(u, c.dist); setFov(u, c.fov);
        if (c.tiltDeg > 0) u.pitch = c.tiltDeg * kPi / 180;
        setSpin(u, c.spin);
        if (c.mass > 0) setMass(u, c.mass);
        const Settings s = shown(u);
        returnFluxNow(s.spin);                                          // 0.87.0: every case with its returning light (R11), the card's and the processor's alike
        view.exact_ = true;
        if (s.spin > 0) view.ensureSpin(s, true);
        view.prepare(s, W, H, W / 2.0, H / 2.0, nullptr);
        const HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
        view.skyUse_ = s.spin > 0 && !table; view.lookUse_ = true;
        if (s.spin > 0) view.ensureSkyF(s.spin);
        if (streaks) { view.gasT = kSkyGasT; g_gasHookOff = false; }
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        if (streaks) { view.gasT = 0; g_gasHookOff = true; L.gasNoFoot = true; }
        L.glareKeep = 0;
        Fbo f = makeFbo(W, H, false);
        R.skyWhiteWait();                                               // (a read-back still under way from before: taken first, so this one is ours)
        R.begin(W, H); R.lensSky(L); endFbo(R, W, H, f);
        R.skyWhiteWait();
        DiscLook cl = table ? view.st_.look : cardReadLook(R.skyStats());
        const bool ok = table ? cl.any && L.kerr && !L.kerrSky : cl.any && finishLook(cl, s);
        ToneExposure e; e.k = cl.k; e.lw = cl.lw; e.ok = ok;
        std::vector<int> ord(static_cast<std::size_t>(W) * H, -1);
        std::vector<double> rad(static_cast<std::size_t>(W) * H, 0);       // (0.87.0: each disc pixel's radius, for the shader's coverage below)
        std::vector<std::array<double, 3>> ref(static_cast<std::size_t>(W) * H);
        static const bool dbgPx = std::getenv("PL_BH_TONEDBG") != nullptr;
        std::vector<DiscPixel> refPx(dbgPx ? static_cast<std::size_t>(W) * H : 0);   // (PL_BH_TONEDBG: the pixels over 8 levels, with the processor's numbers)
        Clump clp;
        clp.a = 0; clp.rc = gasClumpRadius(s); clp.init();
        Vec3 rows[3];
        viewRows(v, rows);
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++) {
                const double X = i + 0.5, Y = H - 1 - j + 0.5;
                // (0.95.0: the sky pass against the closed forms with the disc's surface; the table pass against the table)
                DiscPixel q = s.spin > 0 ? (table ? view.spinPixel(s, v, X, Y) : spinPixelExact(s, v, X, Y, true)) : discPixel(*view.discTable(), s, v, X, Y);
                const std::size_t at = static_cast<std::size_t>(j) * W + i;
                ord[at] = q.hit.order;
                rad[at] = q.hit.r;
                if (q.hit.order >= 0 && streaks) {                          // 0.79.0: the moving gas's light on the reference
                    const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), nx = dx / d, ny = dy / d, psi = q.hit.psi;
                    const Vec3 pos{std::sin(psi) * nx, std::sin(psi) * ny, std::cos(psi)};
                    const double phi = std::atan2(pos.x * rows[0].y + pos.y * rows[1].y + pos.z * rows[2].y, pos.x * rows[0].x + pos.y * rows[1].x + pos.z * rows[2].x);
                    double tl = 0;
                    if (!view.discTable()->lookupT(d / v.K - view.discTable()->alphaSh(), psi, tl)) { ord[at] = -2; continue; }
                    const double te = kSkyGasT - tl, gf = streakField().factor(q.hit.r, phi, te, 0) * (1 + clp.A * clp.density(q.hit.r, phi, te));
                    DiscPixel z;                                            // (0.84.0: the camera's own shift; 0.87.0: the real light at T x gf^(1/4))
                    discLightT(s, discPhys(s).temperature(q.hit.r) * std::pow(gf, 0.25), q.hit.g * heldCameraShift(s, q.hit.b), q.mu, z);
                    q.rgb = z.rgb;
                }
                if (q.hit.order >= 0) ref[at] = toneRGB(q.rgb, e);
                if (dbgPx) refPx[at] = q;
            }
        std::vector<float> gpuProbe;
        if (dbgPx) { render::Renderer2D::LensSky Ld = L; Ld.debugDisc = true; R.lensFloat(Ld, W, H, gpuProbe); gpuProbe.resize(static_cast<std::size_t>(W) * H * 4); }
        long n = 0, over = 0; int maxd = 0;
        const auto enc = [](double l) { l = std::clamp(l, 0.0, 1.0); return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055; };
        for (int j = 1; j + 1 < H; j++)
            for (int i = 1; i + 1 < W; i++) {
                const std::size_t at = static_cast<std::size_t>(j) * W + i;
                if (ord[at] < 0) continue;
                bool inner = true;
                for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) inner = inner && ord[static_cast<std::size_t>(j + dj) * W + i + di] == ord[at];
                if (!inner || f.px.size() < at * 4 + 3) continue;
                // 0.87.0: also left out, as the test means to ("their coverage is the shader's own"): a pixel whose radius range reaches past the disc's edges.
                // The shader covers the part of r +- fwidth(r)/2 on [rIn, rOut]; here w = |dr/dx| + |dr/dy| from the neighbours, and the range r +- w (twice the
                // shader's, for the neighbours' own spread). Grazing rays (case close's corners at 2.2 GM/c^2) move r by over 13 M a pixel.
                const double w = 0.5 * (std::fabs(rad[at + 1] - rad[at - 1]) + std::fabs(rad[at + W] - rad[at - W]));
                const DiscGeom dg = discGeom(s);
                if (rad[at] - w < dg.rIn || rad[at] + w > dg.rOut) continue;
                n++;
                int d = 0;
                for (int k = 0; k < 3; k++) d = std::max(d, std::abs(int(f.px[at * 4 + k]) - int(std::lround(255 * enc(ref[at][static_cast<std::size_t>(k)])))));
                maxd = std::max(maxd, d);
                if (d > 8) over++;
                if (d > 8 && dbgPx && over <= 25) {
                    const DiscPixel& q = refPx[at];
                    std::printf("TONEDBG over case=%s px=%d,%d d=%d order=%d r=%.4f g=%.4f mu=%.4f T=%.1f fcol=%.4f card=%d,%d,%d ref=%.1f,%.1f,%.1f gpu=%g,%.4f,%.4f\n", c.name, i, j, d, q.hit.order, q.hit.r,
                                q.hit.g, q.mu, q.T, q.fcol, f.px[at * 4], f.px[at * 4 + 1], f.px[at * 4 + 2], 255 * enc(ref[at][0]), 255 * enc(ref[at][1]), 255 * enc(ref[at][2]),
                                gpuProbe[at * 4], gpuProbe[at * 4 + 1], gpuProbe[at * 4 + 2]);
                }
            }
        if (std::getenv("PL_BH_TONEDBG")) {
            const render::Renderer2D::SkyStats& ks = R.skyStats();
            std::printf("TONEDBG reads=%ld white=%g whiteY=%g logavg=%g cpu: logavg=%g brightY=%g k=%g lw=%g\n", ks.whiteReads, ks.white, ks.whiteY, ks.logAvg,
                        view.st_.look.logAvg, view.st_.look.brightY, view.st_.look.k, view.st_.look.lw);
            for (int q = 0; q < 6; q++) {
                const int i = W / 2 + (q - 3) * 40, j = H / 4 + q * 20;
                const std::size_t at = static_cast<std::size_t>(j) * W + i;
                std::printf("TONEDBG px %d,%d ord=%d card=%d,%d,%d ref=%.4f,%.4f,%.4f\n", i, j, ord[at], f.px[at * 4], f.px[at * 4 + 1], f.px[at * 4 + 2],
                            255 * enc(ref[at][0]), 255 * enc(ref[at][1]), 255 * enc(ref[at][2]));
            }
        }
        // 0.85.2: ok is the whole check, the test's own limit (at most 1 % of the disc pixels over 8 levels) with the exposure read. It was the exposure alone, so a
        // case over the limit printed ok=1 (the owner's Radeon, case close: 1.99 %). expok is the exposure alone
        const double share = n ? double(over) / n : 1.0;
        const bool pass = ok && n > 0 && share <= 0.01;
        std::printf("PLBHTONE case=%s spin=%g ok=%d expok=%d k=%.6g lw=%.6g pixels=%ld over8=%ld share=%.5f maxdiff=%d\n", c.name, s.spin, pass ? 1 : 0, ok ? 1 : 0, cl.k, cl.lw,
                    n, over, share, maxd);
        dropFbo(f);
    }
}

// 0.95.0 (PL_BH_COMPARE=skytime, PL_BH_SKYTIME="w,h,reps"): the spinning hole's sky pass alone timed by GPU timestamps (Renderer2D::skyTime), the median and
// the fastest of reps pictures, at two views: "spin" (toneCompare's: 0.9, 30 GM/c^2, 60 degrees, 80 degrees from the pole) and "thick" (the Hard scene
// blackhole.thick_spin's view: the top spin, the closest distance, the camera at the disc's surface rule, the widest view). PL_BH_SKYSTOP (kerr_sky.cpp)
// cuts the shader after its set-up (1), the radial case (2) or the elliptic integrals and crossings (3): the split of REPORT 0.95.0.
void skyTimeHook(render::Renderer2D& R) {
    double v[3] = {1280, 800, 9};
    if (const char* e = std::getenv("PL_BH_SKYTIME")) std::sscanf(e, "%lf,%lf,%lf", &v[0], &v[1], &v[2]);
    const int W = static_cast<int>(v[0]), H = static_cast<int>(v[1]), reps = std::max(1, static_cast<int>(v[2]));
    render::KerrSky& K = R.kerrSky();
    struct NoSteps : render::KerrSteps { void begin(const std::string&) override {} void end(const std::string&, const std::string&) override {} } steps;
    std::string why;
    for (int st = 0; st < render::KerrSky::kStages; st++)
        for (int t = 0;; t++) {
            if (!K.build(st, &steps, why) || t > 6000) { std::printf("PLBHSKYTIME none why=%s\n", why.c_str()); return; }
            if (!K.building()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    struct Tv { const char* name; double spin, dist, fov, tiltDeg; bool gas; };   // (gas: the moving gas drawn, as the lab shows the feeding preset)
    for (const Tv& c : {Tv{"spin", 0.9, 30, 60, 80, false}, Tv{"spingas", 0.9, 30, 60, 80, true}, Tv{"thick", kSpinTop, kDistMin, kFovMax, -1, true}}) {
        g_gasHookOff = !c.gas;
        BlackHoleView view;
        Settings u = settingsDefaults();
        applyPreset(u, kFeedingKey);
        u.glare = false;
        setDistance(u, c.dist); setFov(u, c.fov);
        setSpin(u, c.spin);
        u.pitch = c.tiltDeg > 0 ? c.tiltDeg * kPi / 180 : camDiscPitch(u, c.dist, 1);
        const Settings s = shown(u);
        returnFluxNow(s.spin);
        view.exact_ = true;
        view.ensureSpin(s, true);
        view.prepare(s, W, H, W / 2.0, H / 2.0, nullptr);
        const HoleView hv = holeView(s, W, H, W / 2.0, H / 2.0);
        view.skyUse_ = true; view.lookUse_ = true;
        view.ensureSkyF(s.spin);
        view.gasT = c.gas ? kSkyGasT : 0;
        static const bool noSurf = std::getenv("PL_BH_SKYTIME_PLANE") != nullptr;   // (by hand: the same shader with the disc in its plane)
        g_hgtHookOff = noSurf;
        render::Renderer2D::LensSky L = view.lensFor(s, hv);
        g_hgtHookOff = false;
        view.gasT = 0;
        L.glareKeep = 0;
        std::vector<double> ms;
        R.skyTime(L, W, H, reps, ms);
        if (ms.empty()) { std::printf("PLBHSKYTIME case=%s none\n", c.name); continue; }
        std::vector<double> o = ms;
        std::sort(o.begin(), o.end());
        const char* stop = std::getenv("PL_BH_SKYSTOP");
        std::printf("PLBHSKYTIME case=%s stop=%s w=%d h=%d reps=%d median=%.3f min=%.3f pitch=%.6f gas=%d\n", c.name, stop ? stop : "0", W, H, reps, o[o.size() / 2], o[0], s.pitch, L.gas ? 1 : 0);
        std::fflush(stdout);
    }
    g_gasHookOff = true;
}

void compareHook(render::Renderer2D& R, const char* spec) {
    if (!R.vulkan()) { std::printf("PLBHCOMPARE skipped: no Vulkan\n"); return; }   // on Vulkan
    g_gasHookOff = true;                                                    // 0.72.0: the hooks' pictures without the moving gas (but 2a's)
    if (spec && std::string(spec) == "card") { cardCompare(R); return; }
    if (spec && std::string(spec) == "skytime") { skyTimeHook(R); return; }   // 0.95.0
    if (spec && std::string(spec) == "sky") {                              // 0.56.0
        skyCompareHook(R);
        // the exposure found on the card (the brightest channel of the first disc image at the scan's points, reduced on the card) against the processor's
        // scan of the same points (prepare: lab.h discLook's rule through the fine table, or the disc table at spin 0), 640 x 400, at rest
        const int W = 640, H = 400;
        const FilmChoices film;
        struct Ex { const char* name; double spin; bool filmLook; bool all; };   // 0.58.0: all, the Light mode All light
        for (const Ex& e : {Ex{"benchmark", 0.9, false, false}, Ex{"film", 0, true, false}, Ex{"spin0", 0, false, false}, Ex{"all", 0.9, false, true}, Ex{"allspin0", 0, false, true}}) {
            BlackHoleView view;
            Settings u = settingsDefaults();
            applyPreset(u, kFeedingKey);
            u.dist = 20; u.pitch = 80 * kPi / 180;
            if (e.filmLook) { setLook(u, Look::Film); paperView(u); setFov(u, 30); }
            else setSpin(u, e.spin);
            u.allLight = e.all;
            const Settings s = shown(u);
            view.exact_ = true;
            if (s.spin > 0) view.ensureSpin(s, true);
            view.prepare(s, W, H, W / 2.0, H / 2.0, nullptr);
            const double cpu = view.st_.look.white;
            const HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
            view.skyUse_ = s.spin > 0; view.lookUse_ = true;
            if (s.spin > 0) view.ensureSkyF(s.spin);
            render::Renderer2D::LensSky L = view.lensFor(s, v);
            L.glareKeep = 0;
            Fbo f = makeFbo(W, H, false);
            R.begin(W, H); R.lensSky(L); endFbo(R, W, H, f);
            R.skyWhiteWait();
            DiscLook cl = cardReadLook(R.skyStats());                       // 0.70.0: the whole exposure (Real: k from the log-average; Film: the brightest channel)
            const double card = cl.any && finishLook(cl, s) ? cl.white : 0;
            dropFbo(f);
            double la = view.st_.look.logAvg;
            const double lc = cl.logAvg, lt = la;
            if (s.spin > 0 && s.look == Look::Real) {                       // 0.70.0: the log-average's reference at a spin: every sample traced exactly (kerr_closed.h
                std::vector<double> ys;                                     // traceClosed; the spin table's far disc is not exact enough for it, ROADMAP)
                const Camera cam{s.dist, s.pitch, 0};
                const DiscGeom g = discGeom(s);
                double ym = 0;
                for (double Y = 2; Y < H; Y += 4)
                    for (double X = 2; X < W; X += 4) {
                        const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                        if (!(th <= kPi)) continue;
                        double chi = std::atan2(d > 0 ? -dx / d : 0, d > 0 ? dy / d : 1);
                        if (chi < 0) chi += 2 * kPi;
                        const ClosedEnd ce = traceClosed(s.spin, cam, lookAt(th, chi), kClosedCross, false, &g);   // (0.95.0: with the disc's surface)
                        for (int k = 0; k < ce.nCross && k < kSpinCross; k++) {
                            const double r = k < kSurfaceOrders && ce.uS[k] > 0 ? 1 / ce.uS[k] : (ce.u[k] > 0 ? 1 / ce.u[k] : 0);
                            if (!(r >= g.rIn && r <= g.rOut)) continue;
                            DiscPixel q;
                            const double ge = discShiftKerr(r, ce.b, s.spin);
                            discLightSeen(s, r, ge, ce.b, emissionCos(ge, ce.q, r), q);   // (0.84.0: with the camera's own shift; 0.87.0: the emission angle)
                            ys.push_back(q.Y); ym = std::max(ym, q.Y);
                            break;
                        }
                    }
                if (ym > 0) la = logAverage(ys, kToneDelta * ym);
            }
            std::printf("PLBHSKYEXP case=%s spin=%g whitemode=%d cpu=%.6g card=%.6g rel=%.2e n=%d logavg=%.6g tablelogavg=%.6g cardlogavg=%.6g lrel=%.2e\n", e.name, s.spin, L.whiteMode,
                        cpu, card, card > 0 ? std::fabs(card / cpu - 1) : 1.0, R.skyStats().expN, la, lt, lc, la > 0 ? std::fabs(lc / la - 1) : 1.0);
        }
        toneCompare(R);                                                     // 0.70.0
        // the card's moving picture and its picture at rest come from the same formulas: at the same resolution not one pixel differs (Film, the paper's
        // view and 3 degrees below the plane); at half the resolution (a pass forced slow) the pixels that differ by more than 8 levels of 255 are counted
        for (double tilt : {film.camTheta, kPi / 2 + 3 * kPi / 180}) {
            BlackHoleView view;
            Settings u = settingsDefaults();
            applyPreset(u, kFeedingKey);
            setLook(u, Look::Film); paperView(u); setFov(u, 30);
            u.pitch = tilt;
            const Settings s = shown(u);
            const HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
            view.skyUse_ = true; view.lookUse_ = true;
            view.ensureSkyF(s.spin);
            std::vector<unsigned char> pic[3];
            for (int k = 0; k < 3; k++) {
                render::Renderer2D::LensSky L = view.lensFor(s, v);
                L.moving = k != 1; L.frameBudgetMs = k == 0 ? 1e9 : 8; L.slowMs = k == 2 ? 32 : 0; L.white = 1; L.whiteMode = 0;
                Fbo f = makeFbo(W, H, false);
                R.begin(W, H); R.lensSky(L); endFbo(R, W, H, f);
                pic[k].resize(static_cast<std::size_t>(W) * H * 4);
                std::memcpy(pic[k].data(), f.px.data(), f.px.size());
                dropFbo(f);
            }
            long same = 0, half = 0;
            for (std::size_t i = 0; i < pic[0].size(); i += 4)
                for (int c = 0; c < 3; c++) {
                    if (pic[0][i + c] != pic[1][i + c]) { same++; break; }
                }
            for (std::size_t i = 0; i < pic[0].size(); i += 4)
                for (int c = 0; c < 3; c++) if (std::abs(int(pic[2][i + c]) - int(pic[1][i + c])) > 8) { half++; break; }
            std::printf("PLBHSKYSTOP tilt=%.5f samesize=%ld halfsize=%ld scale=%.3f pixels=%d\n", tilt, same, half, R.skyStats().scale, W * H);
        }
        std::fflush(stdout);
        return;
    }
    // 0. 0.55.3, the looks: Real's saved case to the pixel; Film at the paper's view (J15 Figure 13: 74.1 M, 86.56 deg; field of view 30 degrees) against the double
    //    lookup of the same table: the disc's image and edges where the module puts the paper's edges, one colour over the whole disc (0.86.0: J15's opaque,
    //    hard-edged disc; the lab's own glow and see-through disc are gone, PLAN_REVIEW.md R6).
    {
        // 0.55.9: the fine table's rays now come from the closed forms (spin_sky.h kSpinFineFill, kerr_closed.h): the same picture drawn with 0.55.8's rays
        // (Mino's form, kSpinFineFillMino) gives 0.55.8's saved hash, and the two pictures differ by at most one level of 255 (their largest difference and
        // the pixels that differ at all). (0.55.8 did the same against 0.55.7's rays.)
        // 0.95.0: drawn with the disc in its plane (g_hgtHookOff, as 2a): the check compares the two forms' crossings, and mino.h's rays carry no slopes
        std::vector<unsigned char> pNew, pOld;
        g_hgtHookOff = true;
        const std::uint64_t hNew = realCaseHash(R, 1280, 800, kSpinFineFill, &pNew), hOld = realCaseHash(R, 1280, 800, kSpinFineFillMino, &pOld);
        g_hgtHookOff = false;
        int maxDiff = 0;
        long differ = 0;
        for (std::size_t i = 0; i < pNew.size() && pNew.size() == pOld.size(); i += 4) {
            int d = 0;
            for (int c = 0; c < 4; c++) d = std::max(d, std::abs(int(pNew[i + c]) - int(pOld[i + c])));
            maxDiff = std::max(maxDiff, d);
            if (d) differ++;
        }
        std::printf("PLBHREAL hash=%016llx old=%016llx maxdiff=%d differ=%ld pixels=%zu\n", static_cast<unsigned long long>(hNew), static_cast<unsigned long long>(hOld), maxDiff,
                    differ, pNew.size() / 4);
        const int W0 = 1280, H0 = 800;
        BlackHoleView view;
        Settings u = settingsDefaults();
        applyPreset(u, kFeedingKey);
        setLook(u, Look::Film);
        paperView(u);
        setFov(u, 30);
        const Settings s = shown(u);
        view.ensureSpin(s, true);
        const SpinTable& T = *view.spinTable();
        const HoleView v = holeView(s, W0, H0, W0 / 2.0, H0 / 2.0);
        {   Fbo f = makeFbo(W0, H0, false);                                     // a frame as the app draws it: the exposure (st_.look) for these settings
            R.begin(W0, H0); view.draw(R, s, W0, H0, W0 / 2.0, H0 / 2.0); endFbo(R, W0, H0, f); dropFbo(f); }
        const DiscGeom g = discGeom(s);
        const auto readF = [&](const render::Renderer2D::LensSky& L, std::vector<float>& out) {
            R.lensFloat(L, W0, H0, out);   // (0.69.0: the float outputs on Vulkan)
            out.resize(static_cast<std::size_t>(W0) * H0 * 4);
        };
        const auto read8 = [&](const render::Renderer2D::LensSky& L, std::vector<unsigned char>& out) {
            Fbo f = makeFbo(W0, H0, false);
            R.begin(W0, H0); R.lensSky(L); endFbo(R, W0, H0, f);
            out.resize(static_cast<std::size_t>(W0) * H0 * 4);
            std::memcpy(out.data(), f.px.data(), f.px.size());
            dropFbo(f);
        };
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        L.debugDisc = true;
        std::vector<float> dbg;
        readF(L, dbg);
        render::Renderer2D::LensSky Lc = view.lensFor(s, v);
        std::vector<unsigned char> discOnly;
        Lc.sky = false;                                                     // the disc's own light (0.86.0: Film is J15's hard-edged opaque disc, no glow)
        read8(Lc, discOnly);
        // the double lookup at every pixel (row 0 at the bottom, as read back)
        std::vector<int> kk(static_cast<std::size_t>(W0) * H0, -2), km(kk.size(), 0);
        std::vector<double> rr(kk.size(), 0);
        // 0.86.0 (PLAN_REVIEW.md R6): the CPU Film reference: J15's opaque disc, so a pixel whose ray crosses the disc in [rIn, rOut] is the film's one colour
        // (the first crossing covers it all); km, the mask of its crossings on the disc. (0.81.0 to 0.85: the lab's own see-through layers.)
        double fixD[3] = {Lc.discFix[0], Lc.discFix[1], Lc.discFix[2]};
        const double* fix = fixD;
        std::vector<std::array<double, 3>> refC(kk.size(), std::array<double, 3>{0, 0, 0});
        for (int j = 0; j < H0; j++)
            for (int i = 0; i < W0; i++) {
                const double X = i + 0.5, Y = H0 - 1 - j + 0.5, dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                if (!(th <= kPi) || d == 0) continue;
                double chi = std::atan2(-dx / d, dy / d);
                if (chi < 0) chi += 2 * kPi;
                const SpinHit h = T.lookup(th, chi);
                const SpinDisc sd = spinDisc(h, s.spin, g.rIn, g.rOut, surfaceOf(g));
                const std::size_t at = static_cast<std::size_t>(j) * W0 + i;
                kk[at] = sd.order; rr[at] = sd.r;
                for (int k = 0; k < kSpinCross && k < 4; k++) {
                    if (!(h.u[k] > 0) || 1 / h.u[k] < g.rIn || 1 / h.u[k] > g.rOut) continue;
                    km[at] |= 1 << k;
                }
                if (km[at]) refC[at] = {fix[0], fix[1], fix[2]};
            }
        const auto enc8 = [](double l) { l = std::clamp(l, 0.0, 1.0); return static_cast<int>(std::lround(255 * (l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055))); };
        int maxOff = 0;
        int discPx = 0, wrong = 0, edgeWrong = 0, colours = 0, offColour = 0;
        double rMin = 1e9, rMax = 0, shift = 0;
        unsigned char want[3];
        for (int k = 0; k < 3; k++) {
            const double l = std::clamp(fix[k], 0.0, 1.0), e = l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055;
            want[k] = static_cast<unsigned char>(std::lround(e * 255));
        }
        std::vector<std::uint32_t> seen;
        for (int j = 1; j + 1 < H0; j++)
            for (int i = 1; i + 1 < W0; i++) {
                const std::size_t at = static_cast<std::size_t>(j) * W0 + i;
                if (kk[at] == -2) continue;
                const int gk = static_cast<int>(std::lround(dbg[at * 4]));
                bool edge = false;
                for (const std::size_t nb : {at - 1, at + 1, at - W0, at + W0}) if (kk[nb] != kk[at]) edge = true;
                if (gk != kk[at]) { if (edge) edgeWrong++; else wrong++; continue; }
                if (kk[at] < 0) continue;
                rMin = std::min(rMin, static_cast<double>(dbg[at * 4 + 1])); rMax = std::max(rMax, static_cast<double>(dbg[at * 4 + 1]));
                if (edge) {                                                     // an image's edge: how far the card moves it, in pixels
                    double grad = 0;
                    for (const std::size_t nb : {at - 1, at + 1, at - W0, at + W0}) if (kk[nb] == kk[at]) grad = std::max(grad, std::fabs(rr[nb] - rr[at]));
                    if (grad > 0) shift = std::max(shift, std::fabs(dbg[at * 4 + 1] - rr[at]) / grad);
                    continue;
                }
                // the colour where the pixel is fully on the disc: its 8 neighbours on the same image and its radius more than twice the largest step to a
                // neighbour from either edge (the card's anti-aliasing covers a pixel's radius range r +- fwidth/2)
                bool inner = true;
                double step = 0;
                for (int dj = -1; dj <= 1; dj++)
                    for (int di = -1; di <= 1; di++) {
                        const std::size_t nb = at + static_cast<std::size_t>(dj * W0 + di);
                        if (kk[nb] != kk[at] || km[nb] != km[at]) inner = false; else step = std::max(step, std::fabs(rr[nb] - rr[at]));
                    }
                if (!inner || rr[at] - g.rIn < 2 * step || g.rOut - rr[at] < 2 * step) continue;
                discPx++;
                const unsigned char* q = &discOnly[at * 4];                     // (the disc alone)
                const std::uint32_t key = (static_cast<std::uint32_t>(q[0]) << 16) | (static_cast<std::uint32_t>(q[1]) << 8) | q[2];
                if (std::find(seen.begin(), seen.end(), key) == seen.end()) seen.push_back(key);
                int off = 0;                                                    // 0.86.0: the one colour want everywhere again (0.81.0 to 0.85: the laid colour)
                for (int k = 0; k < 3; k++) off = std::max(off, std::abs(int(q[k]) - enc8(refC[at][static_cast<std::size_t>(k)])));
                maxOff = std::max(maxOff, off);
                if (off > 1) {
                    offColour++;
                    if (std::getenv("PL_BH_DBG")) std::printf("DBGFILM i=%d j=%d order=%d mask=%d r=%.6f rcpu=%.6f rgb=%d,%d,%d ref=%d,%d,%d nb=%.5f,%.5f,%.5f,%.5f\n", i, j, kk[at], km[at], dbg[at * 4 + 1], rr[at], q[0], q[1], q[2],
                                                              enc8(refC[at][0]), enc8(refC[at][1]), enc8(refC[at][2]), rr[at - 1], rr[at + 1], rr[at - W0], rr[at + W0]);
                }
            }
        colours = static_cast<int>(seen.size());
        std::printf("PLBHFILM spin=%g dist=%g theta=%.6f rin=%g rout=%g pixels=%d wrong=%d edgewrong=%d edgeshift=%.3e rmin=%.6f rmax=%.6f colours=%d offcolour=%d maxoff=%d want=%d,%d,%d white=%.6g\n",
                    s.spin, s.dist, s.pitch, g.rIn, g.rOut, discPx, wrong, edgeWrong, shift, rMin, rMax, colours, offColour, maxOff, want[0], want[1], want[2], view.st_.look.white);
    }
    // 0b. 0.55.4, the eye's glare on Real's saved case: the light the veil lays on the picture against the light it spreads (light leaving the picture: the
    //     difference, which is small and positive here), the picture with Glare Off = without the glare's passes (the hash above), the glare's time at
    //     1280 x 800 and 3840 x 2160 (the picture drawn anew with and without it, best of 3), the share of the light kept
    {
        BlackHoleView view;
        view.glareNow = true;
        Settings s = settingsDefaults();
        applyPreset(s, kFeedingKey);
        setSpin(s, 0.6);
        setDistance(s, 74.1);
        s.pitch = 1.511;
        setFov(s, 30);
        view.ensureSpin(s, true);
        for (const auto& sz : {std::array<int, 2>{1280, 800}, std::array<int, 2>{3840, 2160}}) {
            double best[2] = {1e30, 1e30};
            render::Renderer2D::GlareStats gs;
            Fbo f = makeFbo(sz[0], sz[1], false);
            for (int withGlare = 0; withGlare < 2; withGlare++) {
                Settings s2 = s;
                s2.glare = withGlare == 1;
                for (int rep = 0; rep < 3; rep++) {
                    s2.yaw += 0.001;
                    R.begin(sz[0], sz[1]);
                    view.draw(R, s2, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);   // (the exposure for this view first, as the app does)
                    endFbo(R, sz[0], sz[1], f);
                    const HoleView v2 = holeView(s2, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
                    render::Renderer2D::LensSky L2 = view.lensFor(s2, v2);
                    L2.cx += 1e-9;                                              // (a new picture: drawn anew)
                    R.begin(sz[0], sz[1]);
                    R.lensSky(L2);
                    const auto t0 = std::chrono::steady_clock::now();
                    endFbo(R, sz[0], sz[1], f);
                    best[withGlare] = std::min(best[withGlare], msSince(t0));
                    if (withGlare) gs = R.glareStats();
                }
            }
            dropFbo(f);
            std::printf("PLBHGLARE size=%dx%d b=%d grid=%dx%d spread=%.9g laid=%.9g keep=%.9g readms=%.1f veilms=%.1f withoutms=%.1f withms=%.1f glarems=%.1f\n", sz[0], sz[1], gs.b, gs.nx, gs.ny,
                        gs.spread, gs.laid, 1 - glareShare(), gs.readMs, gs.veilMs, best[0], best[1], best[1] - best[0]);
        }
    }
    // 0c. 0.85.5, the eye's glare veil on the card against the processor's (light.cpp glareVeil, the reference): a ring of light like a disc seen at a slant,
    //     three bright points and a faint patch, on two grids (1280 x 800 in blocks of 4 pixels, 800 x 500 in blocks of 2). The difference is over the largest veil value.
    for (int g = 0; g < 2; g++) {
        const int W = g == 0 ? 1280 : 800, H = g == 0 ? 800 : 500;     // (the second smaller: the software card on the build machine is slow)
        const double cx = g == 0 ? 640.3 : 400.3, cy = g == 0 ? 400.7 : 250.7, K = g == 0 ? 1222.3 : 600.0;
        const GlareGrid G = glareGrid(W, H, cx, cy, K);
        std::vector<float> src(static_cast<std::size_t>(G.nx) * G.ny * 3, 0.0f), cpu, card;
        for (int j = 0; j < G.ny; j++)
            for (int i = 0; i < G.nx; i++) {
                const double X = (i + 0.5) * G.b - cx, Y = (j + 0.5) * G.b - cy, e = std::hypot(X / 220.0, Y / 70.0);
                float* p = &src[3 * (static_cast<std::size_t>(j) * G.nx + i)];
                if (e > 0.8 && e < 1.2) { const float v = static_cast<float>(3.0 + 2.0 * X / 220.0); p[0] = 1.3f * v; p[1] = v; p[2] = 0.7f * v; }
                if (X > 300 && X < 360 && Y > 150 && Y < 200) p[0] = p[1] = p[2] = 0.05f;
            }
        const int pts[3][2] = {{G.nx / 5, G.ny / 4}, {G.nx - 3, G.ny / 2}, {G.nx / 2 + 1, G.ny / 2}};
        for (const auto& q : pts) for (int k = 0; k < 3; k++) src[3 * (static_cast<std::size_t>(q[1]) * G.nx + q[0]) + k] = 400.0f;
        const auto t0 = std::chrono::steady_clock::now();
        glareVeil(G, src, cpu);
        const double cpuMs = msSince(t0);
        const auto t1 = std::chrono::steady_clock::now();
        R.glareVeilCard(W, H, cx, cy, K, G.b, src, card, glareTablesFn);
        const double cardMs = msSince(t1);
        double peak = 0, worst = 0, worstRel = 0;
        for (float v : cpu) peak = std::max(peak, static_cast<double>(v));
        if (card.size() == cpu.size())
            for (std::size_t k = 0; k < cpu.size(); k++) {
                const double d = std::fabs(static_cast<double>(card[k]) - cpu[k]);
                worst = std::max(worst, d);
                if (cpu[k] > 1e-2 * peak) worstRel = std::max(worstRel, d / cpu[k]);
            }
        std::printf("PLBHGLARECARD grid=%dx%d b=%d made=%d peak=%.6g diff=%.3g reldiff=%.3g cpums=%.1f cardms=%.1f\n", G.nx, G.ny, G.b, card.size() == cpu.size() ? 1 : 0, peak,
                    peak > 0 ? worst / peak : 1.0, worstRel, cpuMs, cardMs);
    }
    if (std::getenv("PL_BH_COMPARE_LOOKS")) { R.begin(1280, 800); return; }   // (by hand: the looks part only)
    const bool onlySpin = std::getenv("PL_BH_COMPARE_SPIN") != nullptr;   // (by hand: the spin part only)
    const int W = 1280, H = 800;
    BlackHoleView view;
    // 1. the card against the double-precision reference: one far camera, one at 27 M, one inside the photon orbit
    for (double dist : {1000.0, 27.0, 2.5}) {
        if (onlySpin) break;
        Settings s = settingsDefaults();
        setDistance(s, dist);
        if (dist < 3) setFov(s, 150);                                       // (inside 3M the shadow is more than half the sky: a wide view shows its edge)
        view.ensureTable(dist);
        HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        L.debug = true;
        std::vector<float> px(static_cast<std::size_t>(W) * H * 4);
        R.lensFloat(L, W, H, px);   // (0.69.0: the float outputs on Vulkan)
        px.resize(static_cast<std::size_t>(W) * H * 4);
        const LensTable& T = *view.table();
        const double e = (kPi - T.edge()) * v.K;
        double edgeErr = 0, band[4] = {0, 0, 0, 0};                         // worst direction difference for x >= 1, 0.1..1, 0.01..0.1 pixel, and < 0.01
        int wrong = 0, compared = 0, edgePx = 0;
        double wrongWorst = 0;
        const double R0[3][3] = {{v.ca, -v.sa, 0}, {v.sa * v.cb, v.ca * v.cb, -v.sb}, {v.sa * v.sb, v.ca * v.sb, v.cb}};
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++) {
                const float* g = &px[(static_cast<std::size_t>(j) * W + i) * 4];
                const double X = i + 0.5, Y = H - 1 - j + 0.5;
                const double xr = std::hypot(X - v.cx, Y - v.cy) - e;          // pixels from the edge, the reference
                Vec3 d;
                const int k = lensPixel(T, v, X, Y, d);
                if (k < 0) continue;
                if (std::fabs(xr) < 3) { edgeErr = std::max(edgeErr, std::fabs(g[3] - xr)); edgePx++; }
                const bool gpuShadow = g[3] <= 0;
                if (gpuShadow != (k == 1)) { wrong++; wrongWorst = std::max(wrongWorst, std::fabs(xr)); continue; }
                if (k == 1) continue;
                const Vec3 gv{g[0], g[1], g[2]};
                const Vec3 ge{R0[0][0] * gv.x + R0[1][0] * gv.y + R0[2][0] * gv.z, R0[0][1] * gv.x + R0[1][1] * gv.y + R0[2][1] * gv.z, R0[0][2] * gv.x + R0[1][2] * gv.y + R0[2][2] * gv.z};
                const double a = angleBetween(ge * (1 / length(ge)), d);
                band[xr >= 1 ? 0 : xr >= 0.1 ? 1 : xr >= 0.01 ? 2 : 3] = std::max(band[xr >= 1 ? 0 : xr >= 0.1 ? 1 : xr >= 0.01 ? 2 : 3], a);
                compared++;
            }
        std::printf("PLBHCMP dist=%g shadowpx=%.3f pixels=%d edgepx=%d edgeerr=%.3e wrong=%d wrongworst=%.3e dir1=%.3e dir01=%.3e dir001=%.3e dir0=%.3e\n",
                    dist, e, compared, edgePx, edgeErr, wrong, wrongWorst, band[0], band[1], band[2], band[3]);
    }
    // 2. bending off = the Orbit Lab's sky projection (skyMap) at the same scale, over black (a scale where no corner reaches the map's fade at 1.55 rad)
    if (!onlySpin) {
        Settings s = settingsDefaults();
        s.bend = false;
        HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
        v.K = std::hypot(W, H) / 2 / 1.5;
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        L.bend = false;
        std::vector<unsigned char> a(static_cast<std::size_t>(W) * H * 4), b(a.size());
        for (int pass = 0; pass < 2; pass++) {
            Fbo f = makeFbo(W, H, false);
            R.begin(W, H);
            R.rect(0, 0, W, H, render::Color{0, 0, 0, 1});
            if (pass == 0) R.skyMap(W, H, v.K, L.M, 1); else R.lensSky(L);
            endFbo(R, W, H, f);
            std::memcpy((pass == 0 ? a : b).data(), f.px.data(), f.px.size());
            dropFbo(f);
        }
        int maxd = 0; long nd = 0, sum = 0;
        for (std::size_t q = 0; q < a.size(); q++) {
            if (q % 4 == 3) continue;
            const int d = std::abs(int(a[q]) - int(b[q]));
            maxd = std::max(maxd, d); sum += d; if (d > 1) nd++;
        }
        std::printf("PLBHPLAIN maxdiff=%d over1=%ld mean=%.4f\n", maxd, nd, double(sum) / (W * H * 3.0));
    }
    // 2a. 0.72.0, the moving gas at spin 0 (kLensFS uDebug 3) at 10^6 M of running (sky_card.h kSkyGasT): at every pixel where the card and the double lookup
    //     see the same first image, its phi against the geometry in double and its t against the double lookup (DiscTable::lookupT); at every 16th pixel
    //     in each direction t against the reference (Dormand-Prince on Binet's equation with t: disc.h rayTime); the factor and the clump at the card's own
    //     r, phi, t against gas.cpp's (limits: sky_card.h kSkyPhiLimit, kSkyTLimit, kSkyGasLimit). Two cameras (27 M at 60 degrees from the disc, 50 M at 10).
    //     0.79.0: the disc's surface (Real, spin 0: disc.h surfaceStep) on both; the factor is the streaks' (gas.h StreakField); the card's radius of the
    //     surface against the double lookup's: pixels over 3e-4 relative counted (limit fixed before looking: 0.1 % of the pixels compared)
    struct GasCam { double dist, incl; };
    for (const GasCam& gc : {GasCam{27.0, 60.0}, GasCam{50.0, 10.0}}) {
        const double dc[2] = {gc.dist, gc.incl};
        if (onlySpin) break;
        Settings s = settingsDefaults();
        applyPreset(s, kFeedingKey);
        s.outer = 30;
        setDistance(s, dc[0]);
        s.pitch = (90 - dc[1]) * kPi / 180;
        view.ensureTable(s.dist);
        view.ensureDisc(s.dist);
        view.gasT = kSkyGasT;
        g_gasHookOff = false;
        HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
        view.st_.look = discLook(*view.discTable(), s, v, W, H);
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        g_gasHookOff = true;
        std::vector<float> po(static_cast<std::size_t>(W) * H * 4), pg(static_cast<std::size_t>(W) * H * 4);
        L.debugDisc = true;
        R.lensFloat(L, W, H, po);
        L.debugDisc = false; L.debugGas = true;
        R.lensFloat(L, W, H, pg);
        po.resize(static_cast<std::size_t>(W) * H * 4); pg.resize(po.size());
        const DiscTable& T = *view.discTable();
        Vec3 rows[3];
        viewRows(v, rows);
        const DiscGeom gg = discGeom(s);
        const StreakField& sf = streakField();
        Clump cl;
        cl.a = 0; cl.rc = gasClumpRadius(s); cl.init();
        double dphi = 0, dt = 0, dtr = 0, dgas = 0, dsr = 0;
        long n = 0, nRef = 0, nGas = 0, bad = 0, rBad = 0;
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++) {
                const std::size_t at = static_cast<std::size_t>(j) * W + i;
                const float* o = &po[at * 4];
                const float* q = &pg[at * 4];
                if (!(q[1] >= 0) || o[0] < 0) continue;
                const double X = i + 0.5, Y = H - 1 - j + 0.5, dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                const DiscPixel dp = discPixel(T, s, v, X, Y, false);
                const double rc = std::clamp(static_cast<double>(o[1]), gg.rIn, gg.rOut), te = kSkyGasT - static_cast<double>(q[1]);
                const double f = sf.factor(rc, q[0], te, 0), cn = cl.density(rc, q[0], te), eg = std::max(std::fabs(q[2] - f) / f, static_cast<double>(std::fabs(q[3] - cn)));
                dgas = std::max(dgas, eg); nGas++;
                if (eg > kSkyGasLimit) bad++;
                if (dp.hit.order != static_cast<int>(std::lround(o[0])) || d <= 0) continue;
                {   const double er = std::fabs(static_cast<double>(o[1]) - dp.hit.r) / dp.hit.r;   // 0.79.0: the surface's radius, the card against the double lookup
                    dsr = std::max(dsr, er);
                    if (er > 3e-4) rBad++; }
                const double nx = dx / d, ny = dy / d, psi = dp.hit.psi;      // (0.79.0: the surface's swept angle; the crossing's where it has none)
                const Vec3 pos{std::sin(psi) * nx, std::sin(psi) * ny, std::cos(psi)};
                const double phi = std::atan2(pos.x * rows[0].y + pos.y * rows[1].y + pos.z * rows[2].y, pos.x * rows[0].x + pos.y * rows[1].x + pos.z * rows[2].x);
                double tl = 0;
                if (!T.lookupT(th - T.alphaSh(), psi, tl)) continue;
                const double ep = std::fabs(std::remainder(q[0] - phi, 2 * kPi)), et = std::fabs(q[1] - tl) / std::max(1.0, tl);
                dphi = std::max(dphi, ep); dt = std::max(dt, et); n++;
                if (ep > kSkyPhiLimit || et > kSkyTLimit) bad++;
                if (i % 16 == 0 && j % 16 == 0) {
                    const double tr = rayTime(s.dist, kPi - th, psi), er = std::fabs(tl - tr) / std::max(1.0, tr);
                    dtr = std::max(dtr, er); nRef++;
                    if (er > kSkyTLimit) bad++;
                }
            }
        std::printf("PLBHGAS0 dist=%g incl=%g px=%ld phi=%.3g t=%.3g refpx=%ld tref=%.3g gaspx=%ld gas=%.3g bad=%ld dr=%.3g rbad=%ld\n", dc[0], dc[1], n, dphi, dt, nRef, dtr, nGas, dgas, bad, dsr, rBad);
    }
    view.gasT = 0;
    // 2e. 0.82.1, the disc seen nearly edge on (PL_BH_COMPARE_VIEWS="dist,pitch,feed;..." in GM/c^2, radians, L/L_Edd; the Sagittarius A* preset with its disc, spin 0,
    //     the surface on): the card against the double lookup at every pixel where the lookup sees the disc (the order or the radius off by 3 % = about 8 levels
    //     of 255 in the colour), and the lookup's own pixel-to-pixel spread: pixels whose order differs from all four neighbours (isolated), and radii that differ
    //     from the mean of their two row neighbours by 3 %. A rough lookup means the model is at fault, not the card.
    if (const char* vs = std::getenv("PL_BH_COMPARE_VIEWS")) {
        const std::string list = vs;
        for (std::size_t a = 0; a < list.size();) {
            std::size_t e = list.find(';', a);
            if (e == std::string::npos) e = list.size();
            double dist = 0, pitch = 0, feed = 0.11;
            std::sscanf(list.substr(a, e - a).c_str(), "%lf,%lf,%lf", &dist, &pitch, &feed);
            a = e + 1;
            Settings s = settingsDefaults();
            applyPreset(s, "sgra");
            s.disc = true; s.feed = feed;
            setDistance(s, dist);
            s.pitch = pitch;
            view.ensureTable(s.dist);
            view.ensureDisc(s.dist);
            HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
            view.st_.look = discLook(*view.discTable(), s, v, W, H);
            render::Renderer2D::LensSky L = view.lensFor(s, v);
            L.debugDisc = true;
            std::vector<float> po(static_cast<std::size_t>(W) * H * 4);
            R.lensFloat(L, W, H, po);
            po.resize(static_cast<std::size_t>(W) * H * 4);
            const DiscTable& T = *view.discTable();
            std::vector<int> ko(static_cast<std::size_t>(W) * H, -1);
            std::vector<double> ro(ko.size(), 0);
            long n = 0, bad = 0, badOrder = 0;
            for (int j = 0; j < H; j++)
                for (int i = 0; i < W; i++) {
                    const std::size_t at = static_cast<std::size_t>(j) * W + i;
                    const DiscPixel q = discPixel(T, s, v, i + 0.5, H - 1 - j + 0.5, false);
                    ko[at] = q.hit.order; ro[at] = q.hit.r;
                    if (q.hit.order < 0) continue;
                    n++;
                    const float* o = &po[at * 4];
                    const bool ord = static_cast<int>(std::lround(o[0])) != q.hit.order, rad = !ord && std::fabs(o[1] / q.hit.r - 1) > 0.03;
                    if (ord || rad) { bad++; if (ord) badOrder++; }
                }
            // the picture as the app draws it (8 bit, the lens pass): where the lookup's disc is smooth (this pixel and its four neighbours on the disc, the
            // radius within 1 % of its neighbours'), no pixel may differ from the mean of its four neighbours by over 8 levels of 255: the limit of the Vulkan
            // pictures (at most 1 % of such pixels)
            long smooth = 0, over8 = 0;
            {   render::Renderer2D::LensSky L2 = view.lensFor(s, v);
                L2.white = 1; L2.whiteMode = 0;
                Fbo f = makeFbo(W, H, false);
                R.begin(W, H); R.lensSky(L2); endFbo(R, W, H, f);
                const auto px = [&](int i, int j, int c) { return static_cast<int>(f.px[(static_cast<std::size_t>(j) * W + i) * 4 + c]); };   // (rows from the bottom, as the lookup's here)
                for (int j = 1; j + 1 < H; j++)
                    for (int i = 1; i + 1 < W; i++) {
                        const std::size_t at = static_cast<std::size_t>(j) * W + i;
                        if (ko[at] < 0 || f.px.size() < static_cast<std::size_t>(W) * H * 4) continue;
                        bool ok = true;
                        for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) ok = ok && ko[nb] == ko[at] && std::fabs(ro[nb] - ro[at]) < 0.01 * ro[at];
                        if (!ok) continue;
                        smooth++;
                        int worst = 0;
                        for (int c = 0; c < 3; c++) worst = std::max(worst, std::abs(px(i, j, c) - (px(i - 1, j, c) + px(i + 1, j, c) + px(i, j - 1, c) + px(i, j + 1, c) + 2) / 4));
                        if (worst > 8) over8++;
                    }
            }
            long iso = 0, rough = 0, ciso = 0, crough = 0;
            for (int j = 1; j + 1 < H; j++)
                for (int i = 1; i + 1 < W; i++) {
                    const std::size_t at = static_cast<std::size_t>(j) * W + i;
                    {   const auto kc = [&](std::size_t x) { return static_cast<int>(std::lround(po[x * 4])); };       // the card's own spread, the same measures
                        const auto rc = [&](std::size_t x) { return static_cast<double>(po[x * 4 + 1]); };
                        if (kc(at) >= 0 && kc(at - 1) != kc(at) && kc(at + 1) != kc(at) && kc(at - W) != kc(at) && kc(at + W) != kc(at)) ciso++;
                        if (kc(at) >= 0 && kc(at - 1) == kc(at) && kc(at + 1) == kc(at) && std::fabs(rc(at) - 0.5 * (rc(at - 1) + rc(at + 1))) > 0.03 * rc(at)) crough++; }
                    if (ko[at] < 0) continue;
                    if (ko[at - 1] != ko[at] && ko[at + 1] != ko[at] && ko[at - W] != ko[at] && ko[at + W] != ko[at]) iso++;
                    if (ko[at - 1] == ko[at] && ko[at + 1] == ko[at] && std::fabs(ro[at] - 0.5 * (ro[at - 1] + ro[at + 1])) > 0.03 * ro[at]) rough++;
                }
            const HeightTable* ht = discGeom(s).height;
            std::printf("PLBHEDGEON dist=%g pitch=%.6f feed=%g height=%.4g camz=%.4g pixels=%ld bad=%ld badorder=%ld isolated=%ld rough=%ld cardisolated=%ld cardrough=%ld smooth=%ld over8=%ld\n", dist, pitch, feed,
                        ht && ht->on() ? ht->at(dist) : 0.0, dist * std::cos(pitch), n, bad, badOrder, iso, rough, ciso, crough, smooth, over8);
        }
        return;
    }
    // 2b. 0.55.1, the disc: the card against the double-precision table lookup (DiscTable in double: lab.h discPixel) at every pixel, and against the reference
    //     (Binet by Dormand-Prince: disc.h discRay) at every 16th pixel in each direction; two distances, two inclinations (a disc of 6 to 30 M, the feeding preset)
    for (double dist : {27.0, 4.0})
        for (double incl : {10.0, 60.0}) {
            if (onlySpin) break;
            Settings s = settingsDefaults();
            applyPreset(s, kFeedingKey);
            s.outer = 30;                                                   // 0.58.0: this test's disc, 6 to 30 M (the default until 0.57); the far disc: REPORT 0.58.0
            setDistance(s, dist);
            s.pitch = (90 - incl) * kPi / 180;
            if (dist < 6) setFov(s, 150);
            view.ensureTable(dist);
            view.ensureDisc(dist);
            HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
            view.st_.look = discLook(*view.discTable(), s, v, W, H);
            g_hgtHookOff = true;                                            // 0.79.0: this check is the plane's crossings (the surface: 2a)
            render::Renderer2D::LensSky L = view.lensFor(s, v);
            g_hgtHookOff = false;
            L.debugDisc = true;
            std::vector<float> px(static_cast<std::size_t>(W) * H * 4);
            R.lensFloat(L, W, H, px);   // (0.69.0: the float outputs on Vulkan)
            px.resize(static_cast<std::size_t>(W) * H * 4);
            const DiscTable& T = *view.discTable();
            DiscGeom g = discGeom(s);
            g.height = nullptr;
            const Vec3 N = discAxisView(v);
            std::vector<double> rr(static_cast<std::size_t>(W) * H, -1);
            std::vector<int> kk(static_cast<std::size_t>(W) * H, -1);
            double dr = 0, dg = 0, refR = 0, refG = 0, edgeWorst = 0;
            int disc = 0, wrong = 0, refN = 0, refWrong = 0;
            for (int j = 0; j < H; j++)
                for (int i = 0; i < W; i++) {
                    const double X = i + 0.5, Y = H - 1 - j + 0.5;
                    const DiscPixel q = discPixel(T, s, v, X, Y, true, true);
                    const std::size_t at = static_cast<std::size_t>(j) * W + i;
                    kk[at] = q.hit.order; rr[at] = q.hit.r;
                    const float* gpx = &px[at * 4];
                    const int gk = static_cast<int>(std::lround(gpx[0]));
                    if (gk != q.hit.order) { wrong++; continue; }
                    if (q.hit.order < 0) continue;
                    disc++;
                    dr = std::max(dr, std::fabs(gpx[1] / q.hit.r - 1));
                    dg = std::max(dg, std::fabs(gpx[2] / q.hit.g - 1));
                    if (i % 16 == 0 && j % 16 == 0) {                       // the reference
                        const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                        const double nx = dx / d, ny = dy / d;
                        const DiscHit h = discRay(s.dist, kPi - th, N.z, nx * N.x + ny * N.y, ny * N.x - nx * N.y, g);
                        refN++;
                        if (h.order != q.hit.order) { refWrong++; continue; }
                        refR = std::max(refR, std::fabs(h.r / q.hit.r - 1));
                        refG = std::max(refG, std::fabs(h.g / q.hit.g - 1));
                    }
                }
            // pixels where the card and the double lookup disagree about the order (an edge between them): how far the edge is from the pixel's centre, in pixels,
            // from the double lookup's radius and its change to the neighbouring pixel
            for (int j = 1; j + 1 < H; j++)
                for (int i = 1; i + 1 < W; i++) {
                    const std::size_t at = static_cast<std::size_t>(j) * W + i;
                    const int gk = static_cast<int>(std::lround(px[at * 4]));
                    if (gk == kk[at]) continue;
                    const double rc = gk >= 0 ? px[at * 4 + 1] : rr[at];
                    if (!(rc > 0)) { edgeWorst = std::max(edgeWorst, 1.0); continue; }
                    const double e = std::fabs(rc - g.rIn) < std::fabs(rc - g.rOut) ? g.rIn : g.rOut;
                    double grad = 0;
                    for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) {
                        const DiscPixel q = discPixel(T, s, v, (nb % W) + 0.5, H - 1 - static_cast<double>(nb / W) + 0.5, true, true);
                        const double rn = q.hit.order >= 0 ? q.hit.r : 0;
                        if (rn > 0) grad = std::max(grad, std::fabs(rn - rc));
                    }
                    edgeWorst = std::max(edgeWorst, grad > 0 ? std::fabs(rc - e) / grad : 1.0);
                }
            // pixels next to an edge of the disc's image (a 4-neighbour of another order): how far the card moves that edge, in pixels: the card's radius against the
            // double lookup's, over the radius's change to the neighbour of the same order
            double shift = 0; int edgeN = 0;
            for (int j = 1; j + 1 < H; j++)
                for (int i = 1; i + 1 < W; i++) {
                    const std::size_t at = static_cast<std::size_t>(j) * W + i;
                    if (kk[at] < 0 || static_cast<int>(std::lround(px[at * 4])) != kk[at]) continue;
                    bool edge = false; double grad = 0;
                    for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) {
                        if (kk[nb] != kk[at]) edge = true;
                        else grad = std::max(grad, std::fabs(rr[nb] - rr[at]));
                    }
                    if (!edge || !(grad > 0)) continue;
                    edgeN++;
                    shift = std::max(shift, std::fabs(px[at * 4 + 1] - rr[at]) / grad);
                }
            std::printf("PLBHDISC dist=%g incl=%g pixels=%d wrong=%d wrongedgepx=%.3e edgepixels=%d edgeshift=%.3e dr=%.3e dg=%.3e refpixels=%d refwrong=%d refdr=%.3e refdg=%.3e white=%.4g\n",
                        dist, incl, disc, wrong, edgeWorst, edgeN, shift, dr, dg, refN, refWrong, refR, refG, view.st_.look.white);
        }
    // 2c. 0.55.2, the spinning hole: the card against the double-precision lookup of the same table (SpinTable::lookup) at every pixel, the shadow's edge
    //     against the closed test (spin.h shadowEdge) at every pixel within 3 pixels of it, and the table against the integrator (spinRay) at every 16th pixel;
    //     two spins (moderate, the top), two distances, two latitudes (near the disc's plane, near the axis); the feeding preset's disc (6 .. 30 M)
    for (double a : {0.5, kSpinTop})
        for (double dist : {20.0, 4.0})
            for (double pitch : {1.5, 0.35}) {
                Settings s = settingsDefaults();
                applyPreset(s, kFeedingKey);
                s.outer = 30;                                                   // 0.58.0: this test's disc, 6 to 30 M (the default until 0.57); the far disc: REPORT 0.58.0
                setDistance(s, dist);
                setSpin(s, a);
                s.pitch = pitch;
                if (dist < 6) setFov(s, 150);
                view.ensureSpin(s, true);
                view.ensureDiscF(s.spin);
                const SpinTable& T = *view.spinTable();
                HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
                view.st_.look = DiscLook{};
                view.st_.look.white = 1;
                std::vector<float> px[2];
                for (int mode = 0; mode < 2; mode++) {
                    g_hgtHookOff = true;                                // 0.95.0: as 2a, this check is the plane's crossings (the surface at spin: tests/test_blackhole.cpp
                    render::Renderer2D::LensSky L = view.lensFor(s, v); // testSpinSurface095, check_blackhole.py sky's thick05, thick99, thick99table)
                    g_hgtHookOff = false;
                    if (mode == 0) L.debug = true; else L.debugDisc = true;
                    px[mode].resize(static_cast<std::size_t>(W) * H * 4);
                    R.lensFloat(L, W, H, px[mode]);   // (0.69.0: the float outputs on Vulkan)
                    px[mode].resize(static_cast<std::size_t>(W) * H * 4);
                }
                const Camera cam{dist, pitch, 0};
                const DiscGeom g = discGeom(s);
                // the double lookup at every pixel: x in pixels from the table's edge, the far direction, the disc's order and r
                std::vector<double> xr(static_cast<std::size_t>(W) * H, -1e9), rr(xr.size(), 0);
                std::vector<int> kk(xr.size(), -2);
                std::vector<Vec3> dd(xr.size());
                for (int j = 0; j < H; j++)
                    for (int i = 0; i < W; i++) {
                        const std::size_t at = static_cast<std::size_t>(j) * W + i;
                        const double X = i + 0.5, Y = H - 1 - j + 0.5, dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                        if (!(th <= kPi) || d == 0) continue;
                        double chi = std::atan2(-dx / d, dy / d);
                        if (chi < 0) chi += 2 * kPi;
                        xr[at] = d - T.edge(chi) * v.K;
                        const SpinHit h = T.lookup(th, chi);
                        dd[at] = h.dir;
                        const SpinDisc sd = spinDisc(h, s.spin, g.rIn, g.rOut, nullptr);
                        kk[at] = sd.order; rr[at] = sd.r;
                    }
                double refShift = 0, edgeLook = 0, edgeClosed = 0, band[4] = {0, 0, 0, 0}, refBand[3] = {0, 0, 0}, dr = 0, dg = 0, refR[2] = {0, 0}, refG[2] = {0, 0}, shift = 0;
                int wrongSide = 0, edgeN = 0, discPx = 0, discWrong = 0, discEdgeWrong = 0, refN = 0, refWrong = 0;
                for (int j = 1; j + 1 < H; j++)
                    for (int i = 1; i + 1 < W; i++) {
                        const std::size_t at = static_cast<std::size_t>(j) * W + i;
                        if (kk[at] == -2) continue;
                        const float* q0 = &px[0][at * 4];
                        const float* q1 = &px[1][at * 4];
                        const double X = i + 0.5, Y = H - 1 - j + 0.5, dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                        double chi = std::atan2(-dx / d, dy / d);
                        if (chi < 0) chi += 2 * kPi;
                        if (std::fabs(xr[at]) < 3) {                                   // the edge: card against the double lookup and against the closed test
                            const double xc = d - shadowEdge(s.spin, cam, chi) * v.K;
                            edgeLook = std::max(edgeLook, std::fabs(q0[3] - xr[at]));
                            edgeClosed = std::max(edgeClosed, std::fabs(q0[3] - xc));
                            if ((q0[3] > 0) != (xc > 0) && std::fabs(xc) > 1e-3) wrongSide++;
                            edgeN++;
                        }
                        if (xr[at] > 0) {                                              // the far direction, card against the double lookup, by distance from the edge in pixels
                            const Vec3 gv{q0[0], q0[1], q0[2]};
                            const int bd = xr[at] >= 1 ? 0 : xr[at] >= 0.1 ? 1 : xr[at] >= 0.01 ? 2 : 3;
                            band[bd] = std::max(band[bd], angleBetween(gv * (1 / length(gv)), dd[at]));
                        }
                        // the disc: the same image at every pixel centre away from an edge of an image; r and g where all four neighbours have the same image
                        const int gk = static_cast<int>(std::lround(q1[0]));
                        bool edge = false;
                        for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) if (kk[nb] != kk[at]) edge = true;
                        if (gk != kk[at]) { if (edge) discEdgeWrong++; else discWrong++; }
                        else if (kk[at] >= 0 && !edge && std::fabs(xr[at]) > 1) {
                            discPx++;
                            const SpinHit h = T.lookup(th, chi);
                            dr = std::max(dr, std::fabs(q1[1] / rr[at] - 1));
                            const double gc = discShiftKerr(rr[at], h.b, s.spin);
                            if (std::fabs(q1[2] / gc - 1) > dg && std::getenv("PL_BH_DBG")) std::printf("DBGG X %.1f Y %.1f r %.6f %.6f b %.6f g card %.6f cpu %.6f gfloat %.6f\n", X, Y, q1[1], rr[at], h.b, q1[2], gc, discShiftKerr(q1[1], h.b, s.spin));
                            dg = std::max(dg, std::fabs(q1[2] / gc - 1));
                        }
                        if (kk[at] >= 0 && gk == kk[at] && edge) {                     // an image's edge: how far the card moves it, in pixels
                            double grad = 0;
                            for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) if (kk[nb] == kk[at]) grad = std::max(grad, std::fabs(rr[nb] - rr[at]));
                            if (grad > 0) shift = std::max(shift, std::fabs(q1[1] - rr[at]) / grad);
                        }
                        if (i % 16 == 0 && j % 16 == 0) {                               // the table against the reference (the integrator)
                            const SpinHit ref = spinRay(s.spin, cam, th, chi, 1e-11), h = T.lookup(th, chi);
                            refN++;
                            const double pe = shadowEdge(s.spin, cam, chi), xpe = std::fabs(th - pe) / pe;
                            if (ref.sky && th > pe) refBand[xpe < 1e-3 ? 0 : xpe < 0.1 ? 1 : 2] = std::max(refBand[xpe < 1e-3 ? 0 : xpe < 0.1 ? 1 : 2], angleBetween(h.dir, ref.dir));
                            const SpinDisc rd = spinDisc(ref, s.spin, g.rIn, g.rOut, nullptr), td = spinDisc(h, s.spin, g.rIn, g.rOut, nullptr);
                            bool fold = false;
                            for (int k = 0; k + 1 < kSpinCross; k++) if (ref.u[k] > 0 && ref.u[k + 1] > 0 && std::fabs(ref.u[k] - ref.u[k + 1]) < 0.3 * ref.u[k]) fold = true;
                            if (rd.order != td.order) { if (!fold && !edge && xpe > 1e-2) refWrong++; }
                            else if (rd.order >= 0 && !fold && !edge && xpe > 1e-2) {
                                const int o = rd.order <= 1 ? 0 : 1;
                                refR[o] = std::max(refR[o], std::fabs(td.r / rd.r - 1)); refG[o] = std::max(refG[o], std::fabs(td.g / rd.g - 1));
                                double grad = 0;                                         // the same error as a shift of the picture, in pixels
                                for (const std::size_t nb : {at - 1, at + 1, at - W, at + W}) if (kk[nb] == kk[at]) grad = std::max(grad, std::fabs(rr[nb] - rr[at]));
                                if (grad > 0 && std::fabs(td.r - rd.r) / grad > refShift && std::getenv("PL_BH_DBG"))
                                    std::printf("DBGSH X %.1f Y %.1f order %d r ref %.5f table %.5f grad %.3e xpe %.3e th %.5f chi %.5f u %.5f %.5f %.5f %.5f\n", X, Y, rd.order, rd.r, td.r, grad, xpe, th, chi, ref.u[0], ref.u[1], ref.u[2], ref.u[3]);
                                if (grad > 0) refShift = std::max(refShift, std::fabs(td.r - rd.r) / grad);
                            }
                        }
                    }
                std::printf("PLBHSPIN a=%g dist=%g pitch=%g edgepx=%d edgelook=%.3e edgeclosed=%.3e wrongside=%d dir1=%.3e dir01=%.3e dir001=%.3e dir0=%.3e refnear=%.3e refmid=%.3e reffar=%.3e "
                            "discpx=%d discwrong=%d discedgewrong=%d edgeshift=%.3e dr=%.3e dg=%.3e refpx=%d refwrong=%d refdr=%.3e refdg=%.3e refdr2=%.3e refdg2=%.3e refshift=%.3e\n", a, dist, pitch, edgeN, edgeLook, edgeClosed, wrongSide,
                            band[0], band[1], band[2], band[3], refBand[0], refBand[1], refBand[2], discPx, discWrong, discEdgeWrong, shift, dr, dg, refN, refWrong, refR[0], refG[0], refR[1], refG[1], refShift);
            }
    if (onlySpin) { R.begin(W, H); return; }
    // 3. times: the bent sky drawn anew at 1280 x 800 and 3840 x 2160 (the software renderer here; Settings > Benchmark measures a real card); the table and the stars
    const int sizes[2][2] = {{1280, 800}, {3840, 2160}};
    for (const auto& sz : sizes) {
        Settings s = settingsDefaults();
        view.ensureTable(s.dist);
        Fbo f = makeFbo(sz[0], sz[1], false);
        double best = 1e30;
        for (int rep = 0; rep < 3; rep++) {
            s.yaw += 0.001;                                                 // a new view each time: drawn anew, not the kept picture
            const HoleView v = holeView(s, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
            R.begin(sz[0], sz[1]);
            R.lensSky(view.lensFor(s, v));
            const auto t0 = std::chrono::steady_clock::now();
            endFbo(R, sz[0], sz[1], f);
            best = std::min(best, msSince(t0));
        }
        dropFbo(f);
        std::printf("PLBHTIME size=%dx%d lensms=%.1f\n", sz[0], sz[1], best);
        // 0.55.1: the same with the disc (the feeding preset)
        applyPreset(s, kFeedingKey);
        view.ensureDisc(s.dist);
        Fbo f2 = makeFbo(sz[0], sz[1], false);
        best = 1e30;
        for (int rep = 0; rep < 3; rep++) {
            s.yaw += 0.001;
            const HoleView v = holeView(s, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
            view.st_.look = discLook(*view.discTable(), s, v, sz[0], sz[1]);
            R.begin(sz[0], sz[1]);
            R.lensSky(view.lensFor(s, v));
            const auto t0 = std::chrono::steady_clock::now();
            endFbo(R, sz[0], sz[1], f2);
            best = std::min(best, msSince(t0));
        }
        dropFbo(f2);
        std::printf("PLBHTIME size=%dx%d discms=%.1f\n", sz[0], sz[1], best);
    }
    // 0.55.2: the spinning hole (spin 0.9, the feeding preset at 20 M, 10 degrees above the disc): the card's pass with the sky only and with the disc
    for (const auto& sz : sizes) {
        Settings s = settingsDefaults();
        applyPreset(s, kFeedingKey);
        setDistance(s, 20);
        setSpin(s, 0.9);
        s.pitch = 80 * kPi / 180;
        view.ensureSpin(s, true);
        view.ensureDiscF(s.spin);
        for (int withDisc = 0; withDisc < 2; withDisc++) {
            s.disc = withDisc == 1;
            Fbo f = makeFbo(sz[0], sz[1], false);
            double best = 1e30;
            for (int rep = 0; rep < 3; rep++) {
                s.yaw += 0.001;
                const HoleView v = holeView(s, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
                view.st_.look.white = 600;
                R.begin(sz[0], sz[1]);
                R.lensSky(view.lensFor(s, v));
                const auto t0 = std::chrono::steady_clock::now();
                endFbo(R, sz[0], sz[1], f);
                best = std::min(best, msSince(t0));
            }
            dropFbo(f);
            std::printf("PLBHTIME spin=0.9 size=%dx%d %s=%.1f\n", sz[0], sz[1], withDisc ? "discms" : "lensms", best);
        }
    }
    {   // the stars' images by inverting the table, and a turn (the stars again; no new table)
        BlackHoleView v2;
        Settings s = settingsDefaults();
        setDistance(s, 20);
        setSpin(s, 0.9);
        v2.ensureSpin(s, true);
        R.begin(W, H);
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);
        s.yaw += 0.01;
        const auto t0 = std::chrono::steady_clock::now();
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);
        std::printf("PLBHTIME spin=0.9 invertms=%.1f starsms=%.1f turnms=%.1f images=%d coarsems=%.0f finems=%.0f\n", v2.stats().spinStarsMs, v2.stats().starsMs, msSince(t0), v2.stats().images,
                    v2.stats().spinCoarseMs, v2.stats().spinFineMs);
    }
    for (double dist : {27.0, 2.5}) {
        BlackHoleView v2;
        Settings s = settingsDefaults();
        setDistance(s, dist);
        R.begin(W, H);
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);
        s.yaw += 0.01;
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);                              // turning: the stars again, the table kept
        std::printf("PLBHTIME dist=%g tablems=%.3f starsms=%.3f tablefills=%ld starfills=%ld images=%d extra=%d capped=%d\n", dist, v2.stats().tableMs, v2.stats().starsMs,
                    v2.stats().tableFills, v2.stats().starFills, v2.stats().images, v2.stats().extraImages, v2.stats().capped);
        applyPreset(s, kFeedingKey);                                          // 0.55.1: the disc's table, its look and the stars it hides
        const auto t1 = std::chrono::steady_clock::now();
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);
        const double first = msSince(t1);
        s.yaw += 0.01;                                                        // turning with the disc: its look and the stars again, the tables kept
        const auto t2 = std::chrono::steady_clock::now();
        v2.draw(R, s, W, H, W / 2.0, H / 2.0);
        std::printf("PLBHTIME dist=%g discms=%.3f discdrawms=%.3f turnms=%.3f discfills=%ld hidden=%d\n", dist, v2.stats().discMs, first, msSince(t2), v2.stats().discFills, v2.stats().hidden);
    }
    R.begin(W, H);                                                          // (nothing of this is left for the first frame)
}

}  // namespace blackhole
