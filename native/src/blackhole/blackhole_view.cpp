#include "blackhole/blackhole_view.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

#include "blackhole/light.h"
#include "blackhole/spin.h"
#include "orbit/sky.h"
#include "render/gl_loader.h"

namespace blackhole {

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
int glareBlockFn(int W, int H, double cx, double cy, double K) { return glareGrid(W, H, cx, cy, K).b; }
void glareVeilFn(int W, int H, double cx, double cy, double K, const std::vector<float>& src, std::vector<float>& veil, double out[3]) {
    GlareSums g;
    glareVeil(glareGrid(W, H, cx, cy, K), src, veil, &g);
    out[0] = g.ms; out[1] = g.spread; out[2] = g.laid;
}
// 0.55.4: the sRGB encoding of a linear opacity (IEC 61966-2-1), so a dot laid over black at this opacity shows the star's true light
double encodeSrgb(double l) { l = std::clamp(l, 0.0, 1.0); return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055; }
}  // namespace

float catalogueBrightestMag() { return brightestMag(); }

// One star image as a dot (both pictures' rule): Shown: the night sky's tone map (orbit/sky.h starDraw); 0.55.4, the true exposure (light.h): the star's own
// light on the disc's scale, spread over its dot, at the screen's white; an image below the screen's faintest step is not drawn (it would draw nothing).
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
void BlackHoleView::trueNumbers(const HoleView& v) {
    st_.brightMag = brightestMag();
    st_.ptSr = 1 / (v.K * v.K);
    st_.trueSky = trueSkyNumbers(st_.brightMag, st_.look.brightY, st_.look.white, st_.ptSr);
}

bool BlackHoleView::Key::operator==(const Key& o) const {
    return W == o.W && H == o.H && cx == o.cx && cy == o.cy && dist == o.dist && fov == o.fov && yaw == o.yaw && pitch == o.pitch && bend == o.bend && sky == o.sky &&
           disc == o.disc && outer == o.outer && feed == o.feed && mass == o.mass && exp == o.exp && expAuto == o.expAuto && spin == o.spin && spinVer == o.spinVer && look == o.look &&
           trueSky == o.trueSky && glare == o.glare;
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

BlackHoleView::~BlackHoleView() { stopSpinWorker(); }

void BlackHoleView::stopSpinWorker() {
    spinCancel_ = true;
    if (spinWorker_.joinable()) spinWorker_.join();
    spinCancel_ = false;
    spinDone_ = false;
    std::lock_guard<std::mutex> g(spinMx_);
    spinReady_.reset();
}

void BlackHoleView::ensureDiscF(double spin) {
    if (discFSpin_ == spin && !discF_.empty()) return;
    const int n = 1024;
    const double r0 = discInner(spin), l0 = std::log(r0), l1 = std::log(kOuterMax), dl = (l1 - l0) / (n - 1);
    discF_.resize(n);
    for (int i = 0; i < n; i++) discF_[static_cast<std::size_t>(i)] = static_cast<float>(std::pow(std::max(0.0, discFlux(std::exp(l0 + i * dl), spin)), 0.25));
    discFSpin_ = spin; discFLog0_ = l0; discFDl_ = dl;
    discFVer_ = nextVersion();
}

void BlackHoleView::ensureSpin(const Settings& s, bool wait) {
    const double a = s.spin, r = s.dist, th = s.pitch;
    const auto mine = [&](const SpinTable& t) { return t.spin() == a && t.rC() == r && t.thC() == th; };
    const auto show = [&](std::shared_ptr<const SpinTable> t) {
        spin_ = std::move(t);
        spinVer_ = nextVersion();
        const auto t0 = std::chrono::steady_clock::now();
        spinStars_ = std::make_unique<SpinStars>(*spin_);
        st_.spinStarsMs = msSince(t0);
    };
    if (!spinMatches(s)) {                                                  // settings changed: drop the fine one being made, the coarse one now
        stopSpinWorker();
        auto c = std::make_shared<SpinTable>(a, r, th, kSpinChiCoarse, kSpinSCoarse, kSpinEdgeCoarse, nullptr, 0, 1e-8);
        st_.spinCoarseMs = c->fillMs();
        st_.spinCoarseFills++;
        st_.spinFine = false;
        show(std::move(c));
        spinChangedAt_ = nowSeconds();
        spinFineStarted_ = false;
    }
    if (st_.spinFine) return;
    if (wait) {
        stopSpinWorker();
        auto f = std::make_shared<SpinTable>(a, r, th, kSpinChiFine, kSpinSFine, kSpinEdge);
        st_.spinFineMs = f->fillMs(); st_.spinFineFills++; st_.spinFine = true;
        show(std::move(f));
        return;
    }
    if (!spinFineStarted_ && nowSeconds() - spinChangedAt_ >= kSpinRest) {   // the view has rested: the fine one on a thread of its own
        spinFineStarted_ = true;
        spinDone_ = false;
        const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
        spinWorker_ = std::thread([this, a, r, th, threads] {
            auto f = std::make_shared<SpinTable>(a, r, th, kSpinChiFine, kSpinSFine, kSpinEdge, &spinCancel_, threads);
            if (f->ok()) { std::lock_guard<std::mutex> g(spinMx_); spinReady_ = std::move(f); }
            spinDone_ = true;
        });
    }
    if (spinFineStarted_ && spinDone_) {
        if (spinWorker_.joinable()) spinWorker_.join();
        std::shared_ptr<const SpinTable> f;
        { std::lock_guard<std::mutex> g(spinMx_); f = std::move(spinReady_); }
        spinDone_ = false;
        if (f && mine(*f)) { st_.spinFineMs = f->fillMs(); st_.spinFineFills++; st_.spinFine = true; show(std::move(f)); }
    }
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
    const SpinDisc sd = spinDisc(h, s.spin, g.rIn, g.rOut);
    o.hit.order = sd.order; o.hit.r = sd.r; o.hit.g = sd.g; o.hit.b = h.b; o.hit.Lz = h.b;
    if (sd.order < 0 || !light) return o;
    discLight(s, sd.r, sd.g, o);                                             // 0.55.3: by the look's rule (lab.h)
    return o;
}

void BlackHoleView::ensureDisc(double dist) {
    if (disc_ && discDist_ == dist) return;
    const auto t0 = std::chrono::steady_clock::now();
    disc_ = std::make_unique<DiscTable>(dist);
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

const std::vector<float>& BlackHoleView::colourTable(int& n, double& log0, double& dlog) {
    static const int N = 512;
    static const double l0 = std::log10(kTmin), dl = (std::log10(kTmax) - l0) / (N - 1);
    static const std::vector<float> c = [] {
        std::vector<float> v(3 * N);
        for (int i = 0; i < N; i++) {
            const auto l = blackBodyLinear(std::pow(10.0, l0 + i * dl));
            for (int k = 0; k < 3; k++) v[3 * i + k] = static_cast<float>(l[k]);
        }
        return v;
    }();
    n = N; log0 = l0; dlog = dl;
    return c;
}

void BlackHoleView::ensureTable(double dist) {
    if (table_ && tableDist_ == dist) return;
    const auto t0 = std::chrono::steady_clock::now();
    table_ = std::make_unique<LensTable>(dist);
    tableDist_ = dist;
    tableVer_ = nextVersion();
    st_.tableMs = msSince(t0);
    st_.tableFills++;
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
    if (lookRule(s.look).glow) { L.glow[0] = static_cast<float>(kGlowStrength); L.glow[1] = static_cast<float>(kGlowThreshold); L.glow[2] = static_cast<float>(kGlowSigma); }   // 0.55.3 (looks.h)
    if (s.spin > 0 && s.bend && spinMatches(s)) {                            // 0.55.2: the spinning hole
        const SpinTable& T = *spin_;
        L.kerr = true;
        L.bend = true;
        L.kA = &T.texA(); L.kB = &T.texB(); L.kEdge = &T.texEdge(); L.kTau = &T.texTau();
        L.kChi = T.nChi(); L.kS = T.nS(); L.kEdgeN = T.nEdge();
        L.kSLo = T.sLo(); L.kXc = kSpinXc; L.kXlo = kSpinXlo; L.spin = s.spin;
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
            L.discF = &discF_; L.discFN = static_cast<int>(discF_.size()); L.discFLog0 = discFLog0_; L.discFDl = discFDl_; L.discFVersion = discFVer_;
            double l0, dl;
            L.discCol = &colourTable(L.discC, l0, dl);
            L.colLog0 = l0; L.colDlog = dl; L.colVersion = 1;
            const DiscGeom g = discGeom(s);
            L.rIn = g.rIn; L.rOut = g.rOut;
            L.tScale = std::pow(discPhys(s).fluxScale() / stefanBoltzmann(), 0.25);
            L.white = st_.look.white;
            if (!lookRule(s.look).shifts) {                                 // 0.55.3, Film: one colour all over the disc (lab.h discLight), over the screen's white as discColour does
                DiscPixel q;
                discLight(s, g.rIn, 1, q);
                double c[3] = {q.rgb[0] / L.white, q.rgb[1] / L.white, q.rgb[2] / L.white};
                const double m = std::max({c[0], c[1], c[2]});
                for (int k = 0; k < 3; k++) L.discFix[k] = static_cast<float>(m > 1 ? c[k] / m : c[k]);
                L.discFix[3] = 1;
            }
            if (glareOn(s) && glareReady_) { L.glareKeep = static_cast<float>(1 - glareShare()); L.glareBlock = glareBlockFn; L.glareVeil = glareVeilFn; }   // 0.55.4
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
        L.white = st_.look.white;
        if (glareOn(s) && glareReady_) { L.glareKeep = static_cast<float>(1 - glareShare()); L.glareBlock = glareBlockFn; L.glareVeil = glareVeilFn; }   // 0.55.4
    }
    return L;
}

void BlackHoleView::draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    R.rect(0, 0, W, H, render::Color{0, 0, 0, 1});                          // space: black, whatever the theme's page
    if (s.spin > 0 && s.bend) { drawSpin(R, s, W, H, cx, cy); return; }
    ensureTable(s.dist);
    const HoleView v = holeView(s, W, H, cx, cy);
    st_.shadowPx = s.bend ? (kPi - table_->edge()) * v.K : 0;
    st_.cx = cx; st_.cy = cy;
    const bool disc = s.disc && s.bend;
    if (!s.skyOurs && !disc) { dots_.clear(); key_ = Key{}; return; }      // Sky: Black and no disc: nothing is lit, the picture is black
    if (disc) ensureDisc(s.dist);
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expLog, s.expAuto, 0.0, 0, static_cast<int>(s.look), trueSkyOn(s), glareOn(s)};
    const bool changed = !(k == key_);
    glareRest(k);
    if (changed) st_.look = disc ? discLook(*disc_, s, v, W, H) : DiscLook{};   // the exposure first: the shader needs the white
    R.lensSky(lensFor(s, v));
    if (!s.skyOurs) { dots_.clear(); key_ = k; return; }
    if (changed) {                                                          // the stars' images only when the view changed
        key_ = k;
        int hidden = 0;
        const auto t0 = std::chrono::steady_clock::now();
        dots_.clear();
        st_.trueShown = st_.trueHidden = 0;
        trueNumbers(v);
        const orbit::Sky& S = orbit::skyData();
        const Vec3 hole = viewMiddle(v);
        const float cap = brightestMag();
        int stars = 0, images = 0, extra = 0, capped = 0;
        for (const orbit::SkyStar& st : S.stars) {
            scratch_.clear();
            if (s.bend) starImagesFast(*table_, hole, st.dir, st.mag, kImageMagLimit, scratch_);
            else scratch_.push_back({st.dir, 1.0, st.mag, 0, 0});
            bool any = false;
            for (const StarImage& im : scratch_) {
                double X, Y;
                if (!viewScreen(v, im.dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
                if (disc && discPixel(*disc_, s, v, X, Y, false).hit.order >= 0) { hidden++; continue; }   // behind the opaque disc
                double mag = im.mag;
                if (mag < cap) { mag = cap; capped++; }
                if (!addDot(s, v, X, Y, mag, S.colours[st.bucket])) continue;
                images++;
                if (im.order > 0) extra++;
                any = true;
            }
            if (any) stars++;
        }
        st_.stars = stars; st_.images = images; st_.extraImages = extra; st_.capped = capped; st_.hidden = hidden;
        st_.starsMs = msSince(t0);
        st_.starFills++;
    }
    for (const Dot& d : dots_) R.disc(d.x, d.y, d.r, d.c);
}

void BlackHoleView::drawSpin(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    static const bool wait = std::getenv("PL_BH_SPINWAIT") != nullptr;      // (automatic checks: the fine table at once, on this thread)
    ensureSpin(s, wait);
    const HoleView v = holeView(s, W, H, cx, cy);
    const SpinTable& T = *spin_;
    double mean = 0;
    for (int i = 0; i < T.nEdge(); i++) mean += T.edgeSample(i) / T.nEdge();
    st_.shadowPx = mean * v.K;
    st_.cx = cx; st_.cy = cy;
    const bool disc = s.disc;
    if (!s.skyOurs && !disc) { dots_.clear(); key_ = Key{}; return; }
    if (disc) ensureDiscF(s.spin);
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expLog, s.expAuto, s.spin, spinVer_, static_cast<int>(s.look), trueSkyOn(s), glareOn(s)};
    const bool changed = !(k == key_);
    glareRest(k);
    if (changed && disc) {                                                  // the exposure first (lab.h discLook's rule, through the spin table)
        DiscLook L;
        const int step = 4;
        for (double Y = step / 2.0; Y < H; Y += step)
            for (double X = step / 2.0; X < W; X += step) {
                const DiscPixel q = spinPixel(s, v, X, Y);
                if (q.hit.order < 0) continue;
                L.any = true;
                L.brightY = std::max(L.brightY, q.Y);
                L.brightMax = std::max({L.brightMax, q.rgb[0], q.rgb[1], q.rgb[2]});
            }
        L.white = s.expAuto ? (L.brightMax > 0 ? L.brightMax : 1) : std::pow(10.0, s.expLog);
        st_.look = L;
    } else if (changed) st_.look = DiscLook{};
    R.lensSky(lensFor(s, v));
    if (!s.skyOurs) { dots_.clear(); key_ = k; return; }
    if (changed) {                                                          // the stars' images by inverting the table (spin_sky.h SpinStars)
        key_ = k;
        int hidden = 0, stars = 0, images = 0, capped = 0;
        const auto t0 = std::chrono::steady_clock::now();
        dots_.clear();
        st_.trueShown = st_.trueHidden = 0;
        trueNumbers(v);
        const orbit::Sky& S = orbit::skyData();
        const float cap = brightestMag();
        const double ph = cameraPhi(v), c = std::cos(ph), sn = std::sin(ph);
        Vec3 er, et, ep;
        cameraFrame(v, s.pitch, er, et, ep);
        for (const orbit::SkyStar& st : S.stars) {
            const Vec3 d{c * st.dir.x + sn * st.dir.y, -sn * st.dir.x + c * st.dir.y, st.dir.z};   // the sky frame turned back by the camera's phi: the chart's axes
            spinScratch_.clear();
            spinStars_->images(d, spinScratch_);
            bool any = false;
            for (const SpinImage& im : spinScratch_) {
                if (!(im.mag > 0)) continue;
                const double mag = st.mag - 2.5 * std::log10(im.mag);
                if (mag > kImageMagLimit) continue;
                const Vec3 N = lookAt(im.psi, im.chi), dir = er * N.x + et * N.y + ep * N.z;
                double X, Y;
                if (!viewScreen(v, dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
                if (disc && spinPixel(s, v, X, Y, false).hit.order >= 0) { hidden++; continue; }   // behind the opaque disc
                double m2 = mag;
                if (m2 < cap) { m2 = cap; capped++; }
                if (!addDot(s, v, X, Y, m2, S.colours[st.bucket])) continue;
                images++;
                any = true;
            }
            if (any) stars++;
        }
        st_.stars = stars; st_.images = images; st_.extraImages = 0; st_.capped = capped; st_.hidden = hidden;
        st_.starsMs = msSince(t0);
        st_.starFills++;
    }
    for (const Dot& d : dots_) R.disc(d.x, d.y, d.r, d.c);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Tests: PL_BH_COMPARE=1
namespace {
struct Fbo { GLuint fbo = 0, tex = 0; };
Fbo makeFbo(int w, int h, bool floats) {
    Fbo f;
    plgl::glGenFramebuffers(1, &f.fbo);
    plgl::glGenTextures(1, &f.tex);
    plgl::glBindTexture(GL_TEXTURE_2D, f.tex);
    plgl::glTexImage2D(GL_TEXTURE_2D, 0, floats ? GL_RGBA32F : GL_RGBA8, w, h, 0, GL_RGBA, floats ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    plgl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    plgl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
    plgl::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f.tex, 0);
    plgl::glBindTexture(GL_TEXTURE_2D, 0);
    return f;
}
void dropFbo(Fbo& f) { plgl::glDeleteFramebuffers(1, &f.fbo); plgl::glDeleteTextures(1, &f.tex); plgl::glBindFramebuffer(GL_FRAMEBUFFER, 0); }
double angleBetween(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)); }
}  // namespace

// 0.55.3: one saved case of the Real look (the feeding preset at spin 0.6 seen from the paper's view, 74.1 M and 1.511 rad, field of view 30 degrees,
// automatic exposure), drawn as the app draws it (sky, disc and stars) into an 8-bit picture: its FNV-1a hash over the RGBA bytes. The test's number was made
// by 0.55.2's code before this job changed anything (REPORT 0.55.3): Real stays the same to the pixel.
namespace {
std::uint64_t realCaseHash(render::Renderer2D& R, int W, int H) {
    BlackHoleView view;
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    setSpin(s, 0.6);
    setDistance(s, 74.1);
    s.pitch = 1.511;
    setFov(s, 30);
    s.glare = false;                                                    // 0.55.4: Glare Off (and Sky Shown, the default) is 0.55.3's Real
    view.ensureSpin(s, true);
    Fbo f = makeFbo(W, H, false);
    R.begin(W, H);
    view.draw(R, s, W, H, W / 2.0, H / 2.0);
    R.end(W, H, 0, f.fbo);
    std::vector<unsigned char> px(static_cast<std::size_t>(W) * H * 4);
    plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
    plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
    plgl::glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    dropFbo(f);
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : px) { h ^= c; h *= 1099511628211ull; }
    if (const char* dump = std::getenv("PL_BH_REALDUMP")) {
        if (FILE* fo = std::fopen(dump, "wb")) { std::fwrite(px.data(), 1, px.size(), fo); std::fclose(fo); }
    }
    return h;
}
}  // namespace

void compareHook(render::Renderer2D& R, const char* spec) {
    (void)spec;
    // 0. 0.55.3, the looks: Real's saved case to the pixel; Film at the paper's view (J15 Figure 13: 74.1 M, 86.56 deg; field of view 30 degrees) against the double
    //    lookup of the same table: the disc's image and edges where the module puts the paper's edges, one colour over the whole disc before the glow; the glow
    //    against its double reference (looks.h glowReference) on the same picture, no pixel darker; the glow pass's time at 1280 x 800 and 3840 x 2160.
    {
        std::printf("PLBHREAL hash=%016llx\n", static_cast<unsigned long long>(realCaseHash(R, 1280, 800)));
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
            R.begin(W0, H0); view.draw(R, s, W0, H0, W0 / 2.0, H0 / 2.0); R.end(W0, H0, 0, f.fbo); dropFbo(f); }
        const DiscGeom g = discGeom(s);
        const auto readF = [&](const render::Renderer2D::LensSky& L, std::vector<float>& out) {
            Fbo f = makeFbo(W0, H0, true);
            R.begin(W0, H0); R.lensSky(L); R.end(W0, H0, 0, f.fbo);
            out.resize(static_cast<std::size_t>(W0) * H0 * 4);
            plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
            plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
            plgl::glReadPixels(0, 0, W0, H0, GL_RGBA, GL_FLOAT, out.data());
            dropFbo(f);
        };
        const auto read8 = [&](const render::Renderer2D::LensSky& L, std::vector<unsigned char>& out) {
            Fbo f = makeFbo(W0, H0, false);
            R.begin(W0, H0); R.lensSky(L); R.end(W0, H0, 0, f.fbo);
            out.resize(static_cast<std::size_t>(W0) * H0 * 4);
            plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
            plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
            plgl::glReadPixels(0, 0, W0, H0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
            dropFbo(f);
        };
        render::Renderer2D::LensSky L = view.lensFor(s, v);
        L.debugDisc = true;
        std::vector<float> dbg;
        readF(L, dbg);
        render::Renderer2D::LensSky Lc = view.lensFor(s, v);
        const float glow0[3] = {Lc.glow[0], Lc.glow[1], Lc.glow[2]};
        Lc.glow[0] = Lc.glow[1] = Lc.glow[2] = 0;
        std::vector<unsigned char> plain, glowed, discOnly;
        read8(Lc, plain);
        Lc.sky = false;                                                     // 0.55.4: the disc's own light (the glow is made from it alone)
        read8(Lc, discOnly);
        Lc.sky = true;
        Lc.glow[0] = glow0[0]; Lc.glow[1] = glow0[1]; Lc.glow[2] = glow0[2];
        read8(Lc, glowed);
        // the double lookup at every pixel (row 0 at the bottom, as read back)
        std::vector<int> kk(static_cast<std::size_t>(W0) * H0, -2);
        std::vector<double> rr(kk.size(), 0);
        for (int j = 0; j < H0; j++)
            for (int i = 0; i < W0; i++) {
                const double X = i + 0.5, Y = H0 - 1 - j + 0.5, dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy), th = d / v.K;
                if (!(th <= kPi) || d == 0) continue;
                double chi = std::atan2(-dx / d, dy / d);
                if (chi < 0) chi += 2 * kPi;
                const SpinDisc sd = spinDisc(T.lookup(th, chi), s.spin, g.rIn, g.rOut);
                kk[static_cast<std::size_t>(j) * W0 + i] = sd.order; rr[static_cast<std::size_t>(j) * W0 + i] = sd.r;
            }
        const double* fix = nullptr;
        double fixD[3] = {Lc.discFix[0], Lc.discFix[1], Lc.discFix[2]};
        fix = fixD;
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
                        if (kk[nb] != kk[at]) inner = false; else step = std::max(step, std::fabs(rr[nb] - rr[at]));
                    }
                if (!inner || rr[at] - g.rIn < 2 * step || g.rOut - rr[at] < 2 * step) continue;
                discPx++;
                const unsigned char* q = &plain[at * 4];
                const std::uint32_t key = (static_cast<std::uint32_t>(q[0]) << 16) | (static_cast<std::uint32_t>(q[1]) << 8) | q[2];
                if (std::find(seen.begin(), seen.end(), key) == seen.end()) seen.push_back(key);
                if (q[0] != want[0] || q[1] != want[1] || q[2] != want[2]) {
                    offColour++;
                    if (std::getenv("PL_BH_DBG")) std::printf("DBGFILM i=%d j=%d order=%d r=%.6f rcpu=%.6f rgb=%d,%d,%d nb=%.5f,%.5f,%.5f,%.5f\n", i, j, kk[at], dbg[at * 4 + 1], rr[at], q[0], q[1], q[2], rr[at - 1], rr[at + 1], rr[at - W0], rr[at + W0]);
                }
            }
        colours = static_cast<int>(seen.size());
        // the glow against its reference (glowReference works top-down; at a height divisible by 4 the blocks are the same either way): 0.55.4: made from the
        // disc's own light (the picture with the sky black), added to the picture with the sky; where the reference adds nothing the picture is the plain one
        std::vector<double> lin(static_cast<std::size_t>(W0) * H0 * 3), dl(lin.size());
        for (std::size_t q = 0; q < static_cast<std::size_t>(W0) * H0; q++)
            for (int k = 0; k < 3; k++) {
                const double e = plain[q * 4 + k] / 255.0, e2 = discOnly[q * 4 + k] / 255.0;
                lin[q * 3 + k] = e <= 0.04045 ? e / 12.92 : std::pow((e + 0.055) / 1.055, 2.4);
                dl[q * 3 + k] = e2 <= 0.04045 ? e2 / 12.92 : std::pow((e2 + 0.055) / 1.055, 2.4);
            }
        for (std::size_t q = 0; q < static_cast<std::size_t>(W0) * H0; q++)   // (a pixel wholly on the disc: its colour unrounded, as the glow pass reads it)
            if (discOnly[q * 4] == want[0] && discOnly[q * 4 + 1] == want[1] && discOnly[q * 4 + 2] == want[2]) for (int k = 0; k < 3; k++) dl[q * 3 + k] = std::clamp(fix[k], 0.0, 1.0);
        const std::vector<double> gref = glowReference(dl, W0, H0);
        int skyTouched = 0, skyFree = 0;
        for (std::size_t q = 0; q < static_cast<std::size_t>(W0) * H0; q++) {
            if (gref[q * 3] > 0 || gref[q * 3 + 1] > 0 || gref[q * 3 + 2] > 0) continue;
            skyFree++;
            for (int k = 0; k < 3; k++) if (glowed[q * 4 + k] != plain[q * 4 + k]) { skyTouched++; break; }
        }
        {   const std::vector<double> gAll = glowReference(lin, W0, H0);  // what 0.55.3's rule (from the whole picture) would have added beyond the disc's glow
            double extra = 0;
            for (std::size_t q = 0; q < gAll.size(); q++) extra += std::max(0.0, gAll[q] - gref[q]);
            std::printf("PLBHGLOWSKY free=%d touched=%d oldextra=%.6g\n", skyFree, skyTouched, extra); }
        int darker = 0, glowDiff = 0, brighter = 0;
        double added = 0, addedRef = 0;
        for (std::size_t q = 0; q < static_cast<std::size_t>(W0) * H0; q++)
            for (int k = 0; k < 3; k++) {
                const int a0 = plain[q * 4 + k], a1 = glowed[q * 4 + k];
                if (a1 < a0) darker++;
                if (a1 > a0) brighter++;
                const double l = std::clamp(lin[q * 3 + k] + kGlowStrength * gref[q * 3 + k], 0.0, 1.0), e = l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055;
                glowDiff = std::max(glowDiff, std::abs(a1 - static_cast<int>(std::lround(e * 255))));
                const double e1 = a1 / 255.0;
                added += (e1 <= 0.04045 ? e1 / 12.92 : std::pow((e1 + 0.055) / 1.055, 2.4)) - lin[q * 3 + k];
                addedRef += std::min(1.0, lin[q * 3 + k] + kGlowStrength * gref[q * 3 + k]) - lin[q * 3 + k];
            }
        std::printf("PLBHFILM spin=%g dist=%g theta=%.6f rin=%g rout=%g pixels=%d wrong=%d edgewrong=%d edgeshift=%.3e rmin=%.6f rmax=%.6f colours=%d offcolour=%d want=%d,%d,%d white=%.6g "
                    "darker=%d brighter=%d glowdiff=%d added=%.6g addedref=%.6g\n", s.spin, s.dist, s.pitch, g.rIn, g.rOut, discPx, wrong, edgeWrong, shift, rMin, rMax, colours, offColour,
                    want[0], want[1], want[2], view.st_.look.white, darker, brighter, glowDiff, added, addedRef);
        // the glow pass's time: the picture drawn anew with and without it (best of 3), the difference
        for (const auto& sz : {std::array<int, 2>{1280, 800}, std::array<int, 2>{3840, 2160}}) {
            Settings s2 = s;
            double best[2] = {1e30, 1e30};
            for (int withGlow = 0; withGlow < 2; withGlow++) {
                Fbo f = makeFbo(sz[0], sz[1], false);
                for (int rep = 0; rep < 3; rep++) {
                    s2.yaw += 0.001;                                            // a new view each time: drawn anew
                    const HoleView v2 = holeView(s2, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
                    render::Renderer2D::LensSky L2 = view.lensFor(s2, v2);
                    if (!withGlow) L2.glow[0] = L2.glow[1] = L2.glow[2] = 0;
                    R.begin(sz[0], sz[1]);
                    R.lensSky(L2);
                    const auto t0 = std::chrono::steady_clock::now();
                    R.end(sz[0], sz[1], 0, f.fbo);
                    plgl::glFinish();
                    best[withGlow] = std::min(best[withGlow], msSince(t0));
                }
                dropFbo(f);
            }
            std::printf("PLBHTIME film size=%dx%d discms=%.1f withglowms=%.1f glowms=%.1f\n", sz[0], sz[1], best[0], best[1], best[1] - best[0]);
        }
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
                    R.end(sz[0], sz[1], 0, f.fbo);
                    const HoleView v2 = holeView(s2, sz[0], sz[1], sz[0] / 2.0, sz[1] / 2.0);
                    render::Renderer2D::LensSky L2 = view.lensFor(s2, v2);
                    L2.cx += 1e-9;                                              // (a new picture: drawn anew)
                    R.begin(sz[0], sz[1]);
                    R.lensSky(L2);
                    const auto t0 = std::chrono::steady_clock::now();
                    R.end(sz[0], sz[1], 0, f.fbo);
                    plgl::glFinish();
                    best[withGlare] = std::min(best[withGlare], msSince(t0));
                    if (withGlare) gs = R.glareStats();
                }
            }
            dropFbo(f);
            std::printf("PLBHGLARE size=%dx%d b=%d grid=%dx%d spread=%.9g laid=%.9g keep=%.9g readms=%.1f veilms=%.1f withoutms=%.1f withms=%.1f glarems=%.1f\n", sz[0], sz[1], gs.b, gs.nx, gs.ny,
                        gs.spread, gs.laid, 1 - glareShare(), gs.readMs, gs.veilMs, best[0], best[1], best[1] - best[0]);
        }
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
        Fbo f = makeFbo(W, H, true);
        R.begin(W, H);
        R.lensSky(L);
        R.end(W, H, 0, f.fbo);
        std::vector<float> px(static_cast<std::size_t>(W) * H * 4);
        plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
        plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
        plgl::glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, px.data());
        dropFbo(f);
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
            R.end(W, H, 0, f.fbo);
            plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
            plgl::glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, (pass == 0 ? a : b).data());
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
    // 2b. 0.55.1, the disc: the card against the double-precision table lookup (DiscTable in double: lab.h discPixel) at every pixel, and against the reference
    //     (Binet by Dormand-Prince: disc.h discRay) at every 16th pixel in each direction; two distances, two inclinations (a disc of 6 to 30 M, the feeding preset)
    for (double dist : {27.0, 4.0})
        for (double incl : {10.0, 60.0}) {
            if (onlySpin) break;
            Settings s = settingsDefaults();
            applyPreset(s, kFeedingKey);
            setDistance(s, dist);
            s.pitch = (90 - incl) * kPi / 180;
            if (dist < 6) setFov(s, 150);
            view.ensureTable(dist);
            view.ensureDisc(dist);
            HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
            view.st_.look = discLook(*view.discTable(), s, v, W, H);
            render::Renderer2D::LensSky L = view.lensFor(s, v);
            L.debugDisc = true;
            Fbo f = makeFbo(W, H, true);
            R.begin(W, H);
            R.lensSky(L);
            R.end(W, H, 0, f.fbo);
            std::vector<float> px(static_cast<std::size_t>(W) * H * 4);
            plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
            plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
            plgl::glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, px.data());
            dropFbo(f);
            const DiscTable& T = *view.discTable();
            const DiscGeom g = discGeom(s);
            const Vec3 N = discAxisView(v);
            std::vector<double> rr(static_cast<std::size_t>(W) * H, -1);
            std::vector<int> kk(static_cast<std::size_t>(W) * H, -1);
            double dr = 0, dg = 0, refR = 0, refG = 0, edgeWorst = 0;
            int disc = 0, wrong = 0, refN = 0, refWrong = 0;
            for (int j = 0; j < H; j++)
                for (int i = 0; i < W; i++) {
                    const double X = i + 0.5, Y = H - 1 - j + 0.5;
                    const DiscPixel q = discPixel(T, s, v, X, Y);
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
                        const DiscPixel q = discPixel(T, s, v, (nb % W) + 0.5, H - 1 - static_cast<double>(nb / W) + 0.5);
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
                    render::Renderer2D::LensSky L = view.lensFor(s, v);
                    if (mode == 0) L.debug = true; else L.debugDisc = true;
                    Fbo f = makeFbo(W, H, true);
                    R.begin(W, H);
                    R.lensSky(L);
                    R.end(W, H, 0, f.fbo);
                    px[mode].resize(static_cast<std::size_t>(W) * H * 4);
                    plgl::glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
                    plgl::glPixelStorei(GL_PACK_ALIGNMENT, 4);
                    plgl::glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, px[mode].data());
                    dropFbo(f);
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
                        const SpinDisc sd = spinDisc(h, s.spin, g.rIn, g.rOut);
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
                            const SpinDisc rd = spinDisc(ref, s.spin, g.rIn, g.rOut), td = spinDisc(h, s.spin, g.rIn, g.rOut);
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
            R.end(sz[0], sz[1], 0, f.fbo);
            plgl::glFinish();
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
            R.end(sz[0], sz[1], 0, f2.fbo);
            plgl::glFinish();
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
                R.end(sz[0], sz[1], 0, f.fbo);
                plgl::glFinish();
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
