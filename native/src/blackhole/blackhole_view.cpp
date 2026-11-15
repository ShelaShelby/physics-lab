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
    return W == o.W && H == o.H && cx == o.cx && cy == o.cy && dist == o.dist && fov == o.fov && yaw == o.yaw && pitch == o.pitch && bend == o.bend && sky == o.sky;
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
    return L;
}

void BlackHoleView::draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy) {
    R.rect(0, 0, W, H, render::Color{0, 0, 0, 1});                          // space: black, whatever the theme's page
    ensureTable(s.dist);
    const HoleView v = holeView(s, W, H, cx, cy);
    st_.shadowPx = s.bend ? (kPi - table_->edge()) * v.K : 0;
    st_.cx = cx; st_.cy = cy;
    if (!s.skyOurs) { dots_.clear(); key_ = Key{}; return; }               // Sky: Black. (No gas yet, so nothing is lit: the picture is black.)
    R.lensSky(lensFor(s, v));
    const Key k{W, H, cx, cy, s.dist, s.fovDeg, s.yaw, s.pitch, s.bend, s.skyOurs};
    if (!(k == key_)) {                                                     // the stars' images only when the view changed
        key_ = k;
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
        st_.stars = stars; st_.images = images; st_.extraImages = extra; st_.capped = capped;
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
    }
    R.begin(W, H);                                                          // (nothing of this is left for the first frame)
}

}  // namespace blackhole
