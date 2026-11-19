#include "atom/classic_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>

#include "app/gpu_guard.h"

namespace atom {

render::ClassicDraw classicDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, const CloudCamera& cam, double w, double h,
                                const DotColours& dc, double weight) {
    render::ClassicDraw d;
    d.pts = pts; d.cnt = cnt; d.cap = cap; d.epoch = epoch; d.k = k; d.grp = grp;
    d.cx = float(cam.cx); d.cy = float(cam.cy); d.pxm = float(cam.pxm); d.ext = float(cam.ext); d.ca = float(cam.ca); d.sa = float(cam.sa); d.cb = float(cam.cb); d.sb = float(cam.sb);
    d.w = float(w); d.h = float(h);
    d.weight = float(weight);
    d.ng = std::min(dc.n, 24);
    for (int g = 0; g < d.ng; g++) d.groupA[g] = dc.c[g].a;
    const render::Renderer2D::PhasePalette p = dc.palette();
    d.wheel = p.wheel; d.light = p.light;
    const render::Color& a = p.sign[0]; const render::Color& b = p.sign[1];
    d.plus[0] = a.r; d.plus[1] = a.g; d.plus[2] = a.b; d.plus[3] = a.a;
    d.minus[0] = b.r; d.minus[1] = b.g; d.minus[2] = b.b; d.minus[3] = b.a;
    return d;
}

ClassicCase classicCheckCase() {
    ClassicCase c;
    std::uint32_t s = 0x2545F491u;
    const auto next = [&] { s = s * 1664525u + 1013904223u; return double(s >> 8) / 16777216.0; };
    const long n = 6000;
    for (long i = 0; i < n; i++) {
        double x, y, z;
        do { x = 2 * next() - 1; y = 2 * next() - 1; z = 2 * next() - 1; } while (x * x + y * y + z * z > 1);
        c.pts.push_back(float(1.1 * x)); c.pts.push_back(float(1.1 * y)); c.pts.push_back(float(1.1 * z));
        c.grp.push_back(i % 37 == 5 ? 255 : (unsigned char)(i % 6));
    }
    c.cam = CloudCamera{200, 150, 120, std::cos(0.7), std::sin(0.7), std::cos(0.4), std::sin(0.4), 1.2};
    c.w = 400; c.h = 300;
    c.dc = makeDotColours(true, true, false, render::Color{0.3f, 0.6f, 1.0f, 1.0f}, render::Color{1.0f, 0.55f, 0.2f, 1.0f});
    c.weight = 1.3;
    return c;
}

ClassicCompare compareClassic(const ClassicCase& cs, const std::vector<std::uint32_t>& keys, const std::vector<float>& sq, long total) {
    ClassicCompare r;
    const long cnt = long(cs.grp.size());
    if (long(keys.size()) < 3 * cnt || long(sq.size()) < 20 * cnt) return r;
    std::vector<float> PX, PY, PD;
    projectCloud(cs.pts.data(), cnt, cs.cam, cs.w, cs.h, PX, PY, PD);
    const int ng = std::min(cs.dc.n, 24);
    const auto fl = [](std::uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; };
    std::map<std::uint32_t, std::vector<std::pair<float, float>>> byKey;     // the card's own dots of each key
    for (long i = 0; i < cnt; i++) {
        r.dots++;
        const std::uint32_t k = keys[std::size_t(3 * i)];
        const float X = fl(keys[std::size_t(3 * i + 1)]), Y = fl(keys[std::size_t(3 * i + 2)]);
        const unsigned char band = depthBandOf(PD[std::size_t(i)]);
        const bool cpuShown = band != kNoBand && cs.grp[std::size_t(i)] < ng, cardShown = k != 0xFFFFFFFFu;
        if (cardShown) { r.shown++; byKey[k].push_back({X, Y}); }
        const bool nearEdge = std::fabs(PX[std::size_t(i)]) < 2e-3 || std::fabs(PX[std::size_t(i)] - float(cs.w)) < 2e-3 || std::fabs(PY[std::size_t(i)]) < 2e-3 || std::fabs(PY[std::size_t(i)] - float(cs.h)) < 2e-3;
        if (cpuShown != cardShown) { if (nearEdge) r.bandFlip++; else r.bandBad++; continue; }
        if (!cpuShown) continue;
        const double e = std::max(std::fabs(double(X) - PX[std::size_t(i)]), std::fabs(double(Y) - PY[std::size_t(i)]));
        r.worstPlace = std::max(r.worstPlace, e);
        if (!(e <= 2e-3)) r.placeBad++;
        if (int(k % std::uint32_t(ng)) != cs.grp[std::size_t(i)]) r.groupBad++;
        if (int(k / std::uint32_t(ng)) != band) {
            const float v = PD[std::size_t(i)];
            if (std::fabs(v - 0.35f) < 2e-5f || std::fabs(v + 0.35f) < 2e-5f) r.bandFlip++; else r.bandBad++;
        }
    }
    // the squares: key by key in the processor's order (band, then group), each key's squares the card's own dots of that key
    long at = 0;
    for (auto& [k, pl] : byKey) {
        const int band = int(k / std::uint32_t(ng)), g = int(k % std::uint32_t(ng));
        const DepthBand& db = kDepthBand[band];
        const float half = float(db.size / 2), alpha = float(std::max(0.0, std::min(1.0, db.alpha * cs.weight)) * cs.dc.c[g].a);
        std::vector<std::pair<float, float>> got;
        for (std::size_t j = 0; j < pl.size(); j++, at++) {
            if (at >= cnt) { r.orderBad++; break; }
            const float* b = &sq[std::size_t(20 * at)];
            got.push_back({b[0], b[1]});
            if (b[2] != half || b[3] != 5.f || b[4] != float(g) || !(std::fabs(b[11] - alpha) <= alpha * 2.4e-7f)) r.squareBad++;
        }
        std::sort(pl.begin(), pl.end()); std::sort(got.begin(), got.end());
        if (got != pl) r.orderBad++;
    }
    if (total != at) r.orderBad++;
    for (long j = at; j < cnt; j++) if (sq[std::size_t(20 * j + 11)] != 0.f) r.squareBad++;
    return r;
}

std::string classicCompareText(const ClassicCompare& c) {
    char b[300];
    std::snprintf(b, sizeof b, "%ld dots, %ld shown: %ld in another group, %ld in another band (%ld at a band's edge within the rounding), %ld places beyond 2e-3 points "
                               "(the largest %.2g), %ld keys out of order, %ld squares wrong", c.dots, c.shown, c.groupBad, c.bandBad + c.bandFlip, c.bandFlip, c.placeBad, c.worstPlace, c.orderBad, c.squareBad);
    return b;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path { int state = 0, step = 0, frames = 0, gen = -1, usedFrames = 0, job = 0; double stepAt = 0; bool firstUse = false, firstDone = false; std::string why, check; };
Path g_p;
const char* kPipe = "the Classic dots' pipeline";
const char* kCheck = "the Classic dots' check";
const char* kFirst = "the Classic dots' first full use";
ClassicCase& checkCase() { static ClassicCase c = classicCheckCase(); return c; }

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLCLASSICCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkClassicGuard();
    render::ClassicCard* C = R.classicCard();
    if (!C) { off("no card programs on this start"); return; }
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipeline could not be made: " + why); off("its pipeline could not be made: " + why); return; }
        char b[120];
        std::snprintf(b, sizeof b, "kClassicCS as SPIR-V (the squares drawn with kBlobVS + kBlobFS), made after %.0f ms", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    if (g_p.step == 1) {
        G.begin(kCheck);
        const ClassicCase& c = checkCase();
        const long n = long(c.grp.size());
        g_p.job = C->beginCheck(classicDraw(c.pts.data(), n, n, 1, n, c.grp.data(), c.cam, c.w, c.h, c.dc, c.weight));
        if (!g_p.job) { G.end(kCheck, "could not be started"); G.finished(false, "the Classic dots' check could not be started"); off("the Classic dots' check could not be started"); return; }
        g_p.stepAt = nowS(); g_p.step = 2;
        return;
    }
    std::vector<std::uint32_t> keys;
    std::vector<float> sq;
    long total = 0;
    const render::JobState st = C->pollCheck(g_p.job, keys, sq, total);
    if (st == render::JobState::Pending) { if (nowS() - g_p.stepAt > 10) { G.timedOut(kCheck); off("a step of the Classic dots on the card did not finish in time (the check)"); } return; }
    ClassicCompare cmp;
    if (st == render::JobState::Done) cmp = compareClassic(checkCase(), keys, sq, total);
    bool ok = st == render::JobState::Done && cmp.ok();
    static const bool forced = std::getenv("PL_CLASSIC_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
    if (forced) ok = false;
    const std::string text = st == render::JobState::Done ? classicCompareText(cmp) : std::string("the batch failed on the card");
    char t[80];
    std::snprintf(t, sizeof t, "%s after %.0f ms: ", ok ? "passed" : "FAILED", 1000 * (nowS() - g_p.stepAt));
    G.end(kCheck, t + text);
    g_p.check = text;
    std::printf("PLCLASSICCARDCASE %s\n", text.c_str());
    if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_CLASSIC_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
    g_p.state = 1;
    G.rest();
    std::printf("PLCLASSICCARD state=1 check=%s\n", text.c_str());
    std::fflush(stdout);
}
}  // namespace

bool drawClassicOnCard(render::Renderer2D& R, const render::ClassicDraw& d) {
    if (!R.vulkan()) return false;                                   // (OpenGL: the processor's squares)
    app::GpuGuard& G = app::gpuVkClassicGuard();
    if (g_p.gen != G.generation()) {
        g_p.gen = G.generation();
        if (g_p.state == -1) { g_p = Path(); g_p.gen = G.generation(); }
    }
    if (g_p.state == 0) {
        if (!R.computeShaders()) off("this Vulkan start has no compute programs on the card (or they are switched off)");
        else if (G.started() && !G.allowed()) off(G.offText().empty() ? std::string("its guard switched it off") : G.offText());
        else if (++g_p.frames >= 3 && G.hold()) { g_p.state = 2; g_p.step = 0; }
    }
    if (g_p.state == 2) advance(R);
    else if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone && ++g_p.usedFrames >= 4) {
        char b[96];
        std::snprintf(b, sizeof b, "drawn in %d frames after %.0f ms", g_p.usedFrames - 1, 1000 * (nowS() - g_p.stepAt));
        G.end(kFirst, b);
        G.finished(true);
        g_p.firstDone = true;
    }
    render::ClassicCard* C = R.classicCard();
    if (!C) return false;
    C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    if (!C->ready) return false;
    if (!g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    return R.cloudClassic(d);
}

int classicCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

std::string classicCardPathText(bool vulkan) {
    if (!vulkan) return "the Classic dots: the processor (no card program for them on OpenGL)";
    if (g_p.state == 1) return "the Classic dots: the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the Classic dots: the processor, because " + g_p.why;
    return "the Classic dots: the graphics card once its start-up check has passed (made when the Classic Cloud 3D is first shown)";
}

}  // namespace atom
