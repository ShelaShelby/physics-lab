#include "orbit/ring_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "orbit/camera.h"
#include "orbit/saturn_profile.h"
#include "orbit/saturn_rings.h"
#include "render/shading.h"

namespace orbit {

namespace {
void blend(float* c, const float* col, float a) { for (int i = 0; i < 3; i++) c[i] = c[i] * (1 - a) + col[i] * a; }
}  // namespace

void ringPictureCpu(const RingFrame& f, const std::vector<RingBand>& bands, int w, int h, std::vector<float>& rgb) {
    rgb.assign(std::size_t(3) * std::size_t(w) * std::size_t(h), 0.f);
    for (int half = 0; half < 2; half++)
        for (const RingBand& b : bands) {
            float lit[3] = {float(render::shadeEncoded(kRingTint[0], b.k)), float(render::shadeEncoded(kRingTint[1], b.k)), float(render::shadeEncoded(kRingTint[2], b.k))};
            const float dark[3] = {0, 0, 0};
            ringTriangles(f, b, half == 0, w, h, [&](double x0, double y0, double x1, double y1, double x2, double y2, bool isLit, const double*, const double*, const double*) {
                const double area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
                if (area == 0) return;
                const double s = area > 0 ? 1 : -1;
                const int ix0 = std::max(0, int(std::floor(std::min({x0, x1, x2}) - 0.5))), ix1 = std::min(w - 1, int(std::ceil(std::max({x0, x1, x2}) - 0.5)));
                const int iy0 = std::max(0, int(std::floor(std::min({y0, y1, y2}) - 0.5))), iy1 = std::min(h - 1, int(std::ceil(std::max({y0, y1, y2}) - 0.5)));
                for (int y = iy0; y <= iy1; y++)
                    for (int x = ix0; x <= ix1; x++) {
                        const double px = x + 0.5, py = y + 0.5;
                        const double e0 = s * ((x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)), e1 = s * ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)), e2 = s * ((x0 - x2) * (py - y2) - (y0 - y2) * (px - x2));
                        if (!(e0 >= 0 && e1 >= 0 && e2 >= 0)) continue;
                        float alpha = b.alpha;
                        const float* col = isLit ? lit : dark;
                        float own[3];
                        if (b.i0 >= 0) {   // 0.75.1: a profile band: its opacity and light at this pixel's own slant (the pixel's line of sight meets the plane at q)
                            double q[3];
                            if (!ringPointAtPixel(f, px, py, q)) continue;
                            const RingValue v = ringBandValue(f, b, ringMu(f, q));
                            alpha = v.alpha;
                            for (int k = 0; k < 3; k++) own[k] = isLit ? float(render::shadeEncoded(kRingTint[k], v.k)) : 0.f;
                            col = own;
                        }
                        blend(&rgb[std::size_t(3) * (std::size_t(y) * std::size_t(w) + std::size_t(x))], col, alpha);
                    }
            });
        }
}

void ringPictureCard(const std::vector<float>& back, const std::vector<float>& front, int w, int h, std::vector<float>& rgb) {
    const std::size_t n = std::size_t(w) * std::size_t(h);
    rgb.assign(3 * n, 0.f);
    if (back.size() < 4 * n || front.size() < 4 * n) return;
    for (std::size_t i = 0; i < n; i++) {
        blend(&rgb[3 * i], &back[4 * i], back[4 * i + 3]);
        blend(&rgb[3 * i], &front[4 * i], front[4 * i + 3]);
    }
}

RingCompare compareRingPictures(const std::vector<float>& cpu, const std::vector<float>& card, int w, int h) {
    RingCompare c;
    const std::size_t n = std::size_t(w) * std::size_t(h);
    if (cpu.size() < 3 * n || card.size() < 3 * n) return c;
    const auto level = [](float v) { return int(std::lround(std::clamp(v, 0.f, 1.f) * 255)); };
    for (std::size_t i = 0; i < n; i++) {
        int d = 0; bool any = false;
        for (int k = 0; k < 3; k++) { const int a = level(cpu[3 * i + k]), b = level(card[3 * i + k]); d = std::max(d, std::abs(a - b)); any = any || a || b; }
        if (!any) continue;
        c.ringPixels++;
        c.over8 += d > 8;
        c.worst = std::max(c.worst, d);
    }
    return c;
}

std::vector<RingCheckView> ringCheckViews(int w, int h) {
    // Saturn's shape (orbit/shapes.h: equatorial / polar radius 60,268 / 54,364 km) in a frame turned so its pole is the pole given; the Sun's direction
    // on the screen (right, up, towards you) and the pole (right, up, towards you)
    const auto view = [&](const char* name, double cx, double cy, double r, double px, double py, double pz, double Lx, double Ly, double Lz, double ppx = 0, double ppy = 0, double zr = 0, double wn = 0) {
        const double pn = std::sqrt(px * px + py * py + pz * pz), ln = std::sqrt(Lx * Lx + Ly * Ly + Lz * Lz);
        px /= pn; py /= pn; pz /= pn; Lx /= ln; Ly /= ln; Lz /= ln;
        // K: rows = the planet's axes A, B (in the equator) and P (the pole) in the screen frame, as orbit_view.cpp spinCtx
        double ax = py, ay = -px, az = 0; { const double n = std::hypot(ax, ay); if (n < 1e-6) { ax = 1; ay = 0; } else { ax /= n; ay /= n; } }
        const double bx = py * az - pz * ay, by = pz * ax - px * az, bz = px * ay - py * ax;
        const float K[9] = {float(ax), float(ay), float(az), float(bx), float(by), float(bz), float(px), float(py), float(pz)};
        RingCheckView v;
        v.name = name;
        v.f = ringFrame(cx, cy, r, px, py, pz, K, 60268.0 / 54364.0, Lx, Ly, Lz);
        const double sunElev = Lx * px + Ly * py + Lz * pz;
        const double mu0 = std::max(std::fabs(sunElev), 1e-3);
        v.f.ppx = ppx; v.f.ppy = ppy; v.f.zr = zr; v.f.wn = wn;      // (the pinhole: zr 0 is the flat map)
        v.f.mu0 = mu0; v.f.sunlit = ringCameraOnPoleSide(v.f) == (sunElev >= 0);
        v.bands = saturnDrawnBands(rings::kSaturnEqRadiusKm, r);
        return v;
    };
    std::vector<RingCheckView> v = {view("the orbit.saturn set-up's kind of view (the rings across the picture, lit from the front)", 0.30 * w, 0.62 * h, 0.22 * w, 0.25, 0.85, 0.46, 0.55, 0.35, 0.76),
            view("the rings almost edge-on", 0.5 * w, 0.5 * h, 0.2 * w, 0.06, 0.995, 0.08, 0.6, 0.3, 0.74),
            view("the planet's shadow across the rings (the Sun behind the planet)", 0.5 * w, 0.5 * h, 0.17 * w, 0.0, 0.85, 0.53, -0.35, 0.25, -0.90)};
    // 0.75.0: the 3D pinhole camera close by: 1.5 equatorial radii from the middle (the closest zoom, orbit/camera.h), 4 degrees above the ring plane, so the
    // rings' near part (out to 2.3 radii) passes beside and behind the camera and is cut at the near plane (orbit/ring_draw.h RingFrame zr, wn). The radius on the
    // screen is focal x R / Zc for the picture's focal length (45 degrees across its height); the middle a little off the principal point, as a slide puts it.
    {
        const double sp = std::sin(4 * 3.14159265358979323846 / 180), cp = std::cos(4 * 3.14159265358979323846 / 180), F = focalFor(h), Zc = 1.5;
        // (zr = 1 / Zc, wn = near / Zc: the camera's distance to the look-at plane is Zc for a followed Saturn)
        v.push_back(view("a close view from 1.5 radii, 4 degrees above the ring plane (the pinhole; the rings pass behind the camera)", 0.55 * w, 0.45 * h, F / Zc, 0.12, cp, sp, 0.5, 0.45, 0.74,
                         0.5 * w, 0.5 * h, 1 / Zc, kNearFraction));
        // 0.75.1: the same distance 30 degrees below the plane (the camera on the other side: the unlit face when the Sun is above, the dark-face formula at slants
        // near the Sun's, which single precision must not lose), Saturn off the middle: every part of the rings at a different slant
        const double sp2 = std::sin(30 * 3.14159265358979323846 / 180), cp2 = std::cos(30 * 3.14159265358979323846 / 180);
        v.push_back(view("a close view from 1.5 radii, 30 degrees below the ring plane (the pinhole; the face the Sun does not light)", 0.42 * w, 0.58 * h, F / Zc, 0.15, cp2, -sp2, 0.5, 0.45, 0.74,
                         0.5 * w, 0.5 * h, 1 / Zc, kNearFraction));
    }
    return v;
}

namespace {
// the profile's optical depths for the card: floats, padded to whole vec4s (render/ring_card.h kRingCardTauVec4); made once
const float* profileTauForCard() {
    static std::vector<float> tau;
    if (tau.empty()) {
        tau.assign(std::size_t(4) * render::kRingCardTauVec4, 0.f);
        const int n = std::min(rings::saturnProfileCount(), 4 * render::kRingCardTauVec4);
        for (int i = 0; i < n; i++) tau[std::size_t(i)] = float(rings::saturnProfileTau(i));
    }
    return tau.data();
}
}  // namespace

render::RingsDraw ringsDraw(const RingFrame& f, const std::vector<RingBand>& bands, bool back, double W, double H) {
    render::RingsDraw d;
    d.cx = float(f.x); d.cy = float(f.y); d.r = float(f.r);
    for (int i = 0; i < 3; i++) { d.e1[i] = float(f.e1[i]); d.e2[i] = float(f.e2[i]); d.Lb[i] = float(f.Lb[i]); d.tint[i] = float(kRingTint[i]); }
    for (int i = 0; i < 9; i++) d.K[i] = float(f.K[i]);
    d.sz = float(f.sz);
    d.back = back ? 1 : 0;
    d.bands.reserve(4 * bands.size());
    double amax = 0;
    // 0.75.1: a band is (a0, a1 in planet radii, i0 * 1024 + n: its measurements, cover); the means over them are taken at each pixel's own slant
    for (const RingBand& b : bands) { d.bands.push_back(float(b.a0)); d.bands.push_back(float(b.a1)); d.bands.push_back(float(b.i0 * 1024 + b.n)); d.bands.push_back(float(b.cover)); amax = std::max(amax, b.a1); }
    for (int i = 0; i < 3; i++) d.pole[i] = float(f.pole[i]);
    d.mu0 = float(f.mu0); d.sunlit = f.sunlit ? 1 : 0;
    d.tau = profileTauForCard();
    { int bnd[4]; double alb[5]; rings::saturnAlbedoRegions(bnd, alb); for (int k = 0; k < 4; k++) { d.albIdx[k] = bnd[k]; d.albVal[k] = float(alb[k]); } d.albF = float(alb[4]); }
    d.pp[0] = float(f.ppx); d.pp[1] = float(f.ppy); d.zr = float(f.zr); d.wn = float(f.wn);
    if (f.zr == 0) {
        const double ex = f.r * amax * (std::fabs(f.e1[0]) + std::fabs(f.e2[0])) + 2, ey = f.r * amax * (std::fabs(f.e1[1]) + std::fabs(f.e2[1])) + 2;
        d.x0 = float(std::max(-2.0, f.x - ex)); d.x1 = float(std::min(W + 2, f.x + ex));
        d.y0 = float(std::max(-2.0, f.y - ey)); d.y1 = float(std::min(H + 2, f.y + ey));
        return d;
    }
    // 0.75.0, the pinhole: the box of the outer edge's corners on the screen (the chords lie inside it); any corner nearer than the near plane: the whole picture
    double mnx = 1e300, mxx = -1e300, mny = 1e300, mxy = -1e300;
    bool all = true;
    for (int i = 0; i < kRingSegments; i++) {
        const double t = 2 * 3.14159265358979323846 * i / kRingSegments, c = std::cos(t), s = std::sin(t);
        const double q[3] = {amax * (c * f.e1[0] + s * f.e2[0]), amax * (c * f.e1[1] + s * f.e2[1]), amax * (c * f.e1[2] + s * f.e2[2])};
        double X = 0, Y = 0;
        if (!ringOnScreen(f, q, X, Y)) { all = false; break; }
        mnx = std::min(mnx, X); mxx = std::max(mxx, X); mny = std::min(mny, Y); mxy = std::max(mxy, Y);
    }
    if (!all) { mnx = -2; mxx = W + 2; mny = -2; mxy = H + 2; }
    d.x0 = float(std::max(-2.0, mnx - 2)); d.x1 = float(std::min(W + 2, mxx + 2));
    d.y0 = float(std::max(-2.0, mny - 2)); d.y1 = float(std::min(H + 2, mxy + 2));
    return d;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
constexpr int kCheckW = 400, kCheckH = 250, kViews = 5;   // 0.75.1: five views (the second close one, below the plane)
struct Path {
    int state = 0;                 // 0 waiting for the rings on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false;
    int jobs[2 * kViews] = {};                // a job a half of each view
    std::vector<float> res[2 * kViews];
    std::string why, check;
};
Path g_p;
const char* kPipe = "the rings' pipelines";
const char* kCheck = "the rings' check";
const char* kFirst = "the rings' first full use";

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLRINGCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

// one step a frame
void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkRingsGuard();
    render::RingCard* C = R.ringCard();
    if (!C) { off("no card programs on this start"); return; }
    const double now = nowS();
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipelines could not be made: " + why); off("its pipelines could not be made: " + why); return; }
        char b[160];
        std::snprintf(b, sizeof b, "kVertexShader + kRingsFS and kRingsProbeCS as SPIR-V, made after %.0f ms", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    if (g_p.step == 1) {                                             // the check: both halves of the views asked for now, compared when all are back
        G.begin(kCheck);
        std::vector<float> xy;
        xy.reserve(std::size_t(2) * kCheckW * kCheckH);
        for (int y = 0; y < kCheckH; y++) for (int x = 0; x < kCheckW; x++) { xy.push_back(x + 0.5f); xy.push_back(y + 0.5f); }
        const std::vector<RingCheckView> views = ringCheckViews(kCheckW, kCheckH);
        for (int i = 0; i < 2 * kViews; i++) {
            const RingCheckView& v = views[std::size_t(i / 2)];
            g_p.jobs[i] = C->beginProbe(ringsDraw(v.f, v.bands, i % 2 == 0, kCheckW, kCheckH), xy);
            if (!g_p.jobs[i]) { for (int k = 0; k < i; k++) C->cancel(g_p.jobs[k]); G.end(kCheck, "could not be started"); G.finished(false, "the rings' check could not be started"); off("the rings' check could not be started"); return; }
        }
        g_p.stepAt = now; g_p.step = 2;
        return;
    }
    if (g_p.step == 2) {
        bool pending = false;
        for (int i = 0; i < 2 * kViews; i++) {
            if (!g_p.jobs[i]) continue;
            const render::JobState st = C->pollProbe(g_p.jobs[i], g_p.res[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.jobs[i] = 0;
            if (st == render::JobState::Failed) g_p.res[i].clear();
        }
        if (pending) {
            if (now - g_p.stepAt > 10) { for (int& j : g_p.jobs) if (j) { C->cancel(j); j = 0; } G.timedOut(kCheck); off("a step of the rings on the card did not finish in time (the check)"); }
            return;
        }
        const std::vector<RingCheckView> views = ringCheckViews(kCheckW, kCheckH);
        bool ok = true;
        std::string text;
        for (int v = 0; v < kViews; v++) {
            std::vector<float> cpu, card;
            ringPictureCpu(views[std::size_t(v)].f, views[std::size_t(v)].bands, kCheckW, kCheckH, cpu);
            ringPictureCard(g_p.res[2 * v], g_p.res[2 * v + 1], kCheckW, kCheckH, card);
            const RingCompare c = compareRingPictures(cpu, card, kCheckW, kCheckH);
            if (const char* dbg = std::getenv("PL_RINGS_DEBUG")) {   // (for looking at a failed view: both pictures as PPM files)
                for (int k = 0; k < 2; k++) {
                    char fn[300]; std::snprintf(fn, sizeof fn, "%s/ringview%d_%s.ppm", dbg, v, k ? "card" : "cpu");
                    if (FILE* fp = std::fopen(fn, "wb")) {
                        std::fprintf(fp, "P6 %d %d 255\n", kCheckW, kCheckH);
                        const std::vector<float>& src = k ? card : cpu;
                        for (std::size_t q = 0; q < src.size(); q++) std::fputc(int(std::clamp(src[q], 0.0f, 1.0f) * 255 + 0.5f), fp);
                        std::fclose(fp);
                    }
                }
            }
            ok = ok && c.ok();
            char b[200];
            std::snprintf(b, sizeof b, "%s%ld of %ld ring pixels over 8 levels (%.2f %%, limit 1 %%), the largest %d", v ? "; " : "", c.over8, c.ringPixels, c.ringPixels ? 100.0 * double(c.over8) / double(c.ringPixels) : 0.0, c.worst);
            text += b;
            std::printf("PLRINGCARDCASE %d %s: %s\n", v, views[std::size_t(v)].name, b + (v ? 2 : 0));
        }
        for (std::vector<float>& r : g_p.res) { r.clear(); r.shrink_to_fit(); }
        static const bool forced = std::getenv("PL_RINGS_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) ok = false;
        char t[96];
        std::snprintf(t, sizeof t, "%s after %.0f ms: %d views of %d x %d, ", ok ? "passed" : "FAILED", 1000 * (now - g_p.stepAt), kViews, kCheckW, kCheckH);
        G.end(kCheck, t + text);
        g_p.check = std::to_string(kViews) + " views: " + text;
        if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_RINGS_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
        g_p.state = 1;
        G.rest();                                                    // (the first full use comes when the rings are next drawn)
        std::printf("PLRINGCARD state=1 check=%s\n", text.c_str());
        std::fflush(stdout);
    }
}
}  // namespace

bool drawRingsOnCard(render::Renderer2D& R, const RingFrame& f, const std::vector<RingBand>& bands, bool back, double W, double H) {
    if (!R.vulkan()) return false;                                   // (OpenGL: the processor's triangles; no card program for the rings there)
    app::GpuGuard& G = app::gpuVkRingsGuard();
    if (back) {                                                      // (once a frame: the back half comes first)
        if (g_p.gen != G.generation()) {                             // the first frame, and "Try again" (Settings, Performance)
            g_p.gen = G.generation();
            if (g_p.state == -1) { g_p = Path(); g_p.gen = G.generation(); }
        }
        if (g_p.state == 0) {
            if (!R.computeShaders()) { off("this Vulkan start has no compute programs on the card (or they are switched off)"); }
            else if (G.started() && !G.allowed()) off(G.offText().empty() ? std::string("its guard switched it off") : G.offText());
            else if (++g_p.frames >= 3 && G.hold()) { g_p.state = 2; g_p.step = 0; }
        }
        if (g_p.state == 2) advance(R);
        else if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone && ++g_p.usedFrames >= 4) {   // (3 frames drawn by the card, their batches long done)
            char b[96];
            std::snprintf(b, sizeof b, "drawn in %d frames after %.0f ms", g_p.usedFrames - 1, 1000 * (nowS() - g_p.stepAt));
            G.end(kFirst, b);
            G.finished(true);
            g_p.firstDone = true;
        }
        if (render::RingCard* C = R.ringCard()) C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    }
    render::RingCard* C = R.ringCard();
    if (g_p.state != 1 || !C || !C->ready || bands.empty()) return false;
    if (back && !g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    const render::RingsDraw d = ringsDraw(f, bands, back, W, H);
    if (!(d.x1 > d.x0) || !(d.y1 > d.y0)) return true;               // (wholly beyond the picture: nothing to draw either way)
    return R.ringsCard(d);
}

int ringCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

std::string ringCardPathText(bool vulkan) {
    if (!vulkan) return "Saturn's rings: the processor (no card program for the rings on OpenGL)";
    if (g_p.state == 1) return "Saturn's rings: the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "Saturn's rings: the processor, because " + g_p.why;
    return "Saturn's rings: the graphics card once its start-up check has passed (made when Saturn's rings are first shown)";
}

}  // namespace orbit
