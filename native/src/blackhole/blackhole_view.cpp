#include "blackhole/blackhole_view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "orbit/sky.h"
#include "render/gl_loader.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
double msSince(std::chrono::steady_clock::time_point t0) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
// the brightest catalogue star seen without the hole: the cap of a drawn image (blackhole_view.h)
float brightestMag() {
    static const float m = [] { float b = 99; for (const orbit::SkyStar& s : orbit::skyData().stars) b = std::min(b, s.mag); return b; }();
    return m;
}
}  // namespace

bool BlackHoleView::Key::operator==(const Key& o) const {
    return W == o.W && H == o.H && cx == o.cx && cy == o.cy && dist == o.dist && fov == o.fov && yaw == o.yaw && pitch == o.pitch && bend == o.bend && sky == o.sky &&
           disc == o.disc && outer == o.outer && feed == o.feed && mass == o.mass && exp == o.exp && expAuto == o.expAuto;
}

void BlackHoleView::ensureDisc(double dist) {
    if (disc_ && discDist_ == dist) return;
    const auto t0 = std::chrono::steady_clock::now();
    disc_ = std::make_unique<DiscTable>(dist);
    discDist_ = dist;
    discVer_++;
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
    tableVer_++;
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
    L.sky = s.skyOurs;
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
    }
    return L;
}

void BlackHoleView::draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    R.rect(0, 0, W, H, render::Color{0, 0, 0, 1});                          // space: black, whatever the theme's page
    ensureTable(s.dist);
    const HoleView v = holeView(s, W, H, cx, cy);
    st_.shadowPx = s.bend ? (kPi - table_->edge()) * v.K : 0;
    st_.cx = cx; st_.cy = cy;
    const bool disc = s.disc && s.bend;
    if (!s.skyOurs && !disc) { dots_.clear(); key_ = Key{}; return; }      // Sky: Black and no disc: nothing is lit, the picture is black
    if (disc) ensureDisc(s.dist);
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs, disc, s.outer, s.feed, s.massSun, s.expLog, s.expAuto};
    const bool changed = !(k == key_);
    if (changed) st_.look = disc ? discLook(*disc_, s, v, W, H) : DiscLook{};   // the exposure first: the shader needs the white
    R.lensSky(lensFor(s, v));
    if (!s.skyOurs) { dots_.clear(); key_ = k; return; }
    if (changed) {                                                          // the stars' images only when the view changed
        key_ = k;
        int hidden = 0;
        const auto t0 = std::chrono::steady_clock::now();
        dots_.clear();
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
                const orbit::StarDraw d = orbit::starDraw(mag);
                const auto& c = S.colours[st.bucket];
                dots_.push_back({static_cast<float>(X), static_cast<float>(Y), static_cast<float>(d.radius), render::rgba(c[0], c[1], c[2], static_cast<float>(d.alpha))});
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

void compareHook(render::Renderer2D& R, const char* spec) {
    (void)spec;
    const int W = 1280, H = 800;
    BlackHoleView view;
    // 1. the card against the double-precision reference: one far camera, one at 27 M, one inside the photon orbit
    for (double dist : {1000.0, 27.0, 2.5}) {
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
    {
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
