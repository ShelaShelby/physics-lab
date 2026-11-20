#include "molecule/surface_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "molecule/molecule.h"
#include "molecule/vibration.h"

namespace molecule {

namespace {
void turn(double yaw, double pitch, double R[9]) {   // rows: the picture's right, up and towards the viewer
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const double r[9] = {cy, 0, sy, sy * sp, cp, -cy * sp, -sy * cp, sp, cy * cp};
    for (int i = 0; i < 9; i++) R[i] = r[i];
}
double radiusOf(const dens::Mesh& m) {
    double r = 0;
    for (std::size_t v = 0; v + 2 < m.p.size(); v += 3) r = std::max(r, std::sqrt(double(m.p[v]) * m.p[v] + double(m.p[v + 1]) * m.p[v + 1] + double(m.p[v + 2]) * m.p[v + 2]));
    return r;
}
const render::Color kPlain{0.80f, 0.84f, 0.90f, 1};
}  // namespace

std::vector<SurfaceCase> surfaceCheckCases() {
    std::vector<SurfaceCase> v;
    const double iso = kRealIso[2];
    const auto add = [&](const char* name, const char* id, int mode, double phase, double yaw, double pitch) {
        const dens::Data* d = dens::get(id);
        if (!d) return;
        SurfaceCase c;
        c.name = name;
        dens::Mesh still;
        dens::densitySurface(*d, iso, 2, still);
        c.potRange = std::max(1e-6, dens::potentialRange(still));
        if (mode >= 0) {
            const dens::VibDensityMode* vm = dens::vibDensity(id, mode);
            if (!vm) return;
            dens::Data at;
            dens::blendVib(*d, *vm, phase, at, false);
            dens::densitySurface(at, iso, 2, c.mesh);
        } else c.mesh = still;
        if (c.mesh.tri.empty()) return;
        turn(yaw, pitch, c.R);
        c.px = 0.3; c.py = -0.2;
        c.radius = radiusOf(c.mesh);
        v.push_back(std::move(c));
    };
    add("benzene (C6H6)", "C6H6", -1, 0, 0.6, 0.45);
    add("water (H2O)", "H2O", -1, 0, -0.9, 0.3);
    int mode = 0;
    while (mode < int(vib::modes("H2O").size()) && !dens::vibDensity("H2O", mode)) mode++;
    add("water vibrating (its first mode with a density, at 0.7 of the swing)", "H2O", mode, 0.7, 0.4, -0.5);
    return v;
}

render::SurfaceView surfaceView(const double R[9], double px, double py, double cx, double cy, double s, bool pot, double potRange, render::Color plain, float aFront, float aBack) {
    render::SurfaceView v;
    for (int i = 0; i < 9; i++) v.R[i] = static_cast<float>(R[i]);
    v.px = static_cast<float>(px); v.py = static_cast<float>(py); v.cx = static_cast<float>(cx); v.cy = static_cast<float>(cy); v.s = static_cast<float>(s);
    v.pot = pot ? 1 : 0; v.potRange = static_cast<float>(potRange);
    v.plain[0] = plain.r; v.plain[1] = plain.g; v.plain[2] = plain.b; v.plain[3] = plain.a;
    v.aFront = aFront; v.aBack = aBack;
    return v;
}

render::SurfaceDraw surfaceDrawOf(const SurfaceCase& c) {
    render::SurfaceDraw d;
    d.mesh.rev = 0; d.mesh.p = c.mesh.p.data(); d.mesh.nrm = c.mesh.n.data(); d.mesh.value = c.mesh.value.data(); d.mesh.tri = c.mesh.tri.data();
    d.mesh.nv = static_cast<int>(c.mesh.p.size() / 3); d.mesh.nt = static_cast<int>(c.mesh.tri.size() / 3);
    d.view = surfaceView(c.R, c.px, c.py, c.cx, c.cy, c.s, true, c.potRange, kPlain, 0.6f, 0.15f);
    return d;
}

SurfaceCompare compareSurface(const SurfaceCase& c, const std::vector<float>& verts, const std::vector<std::uint32_t>& order) {
    SurfaceCompare k;
    const std::size_t nv = c.mesh.p.size() / 3, nt = c.mesh.tri.size() / 3;
    if (verts.size() != 8 * nv || order.size() != nt || nt == 0) return k;
    SurfaceCpu o;
    surfacePlace(c.mesh, c.R, c.px, c.py, c.cx, c.cy, c.s, o);
    surfaceColours(c.mesh, c.R, true, c.potRange, kPlain, o);
    k.posLimit = 2e-6 * (c.s * (c.radius + std::hypot(c.px, c.py)) + std::fabs(c.cx) + std::fabs(c.cy));
    for (std::size_t v = 0; v < nv; v++) {
        const float* q = &verts[8 * v];
        const double e = std::max(std::fabs(double(q[0]) - o.sx[v]), std::fabs(double(q[1]) - o.sy[v]));
        k.posWorst = std::max(k.posWorst, e);
        if (!(e <= k.posLimit)) k.posOver++;
        const double ec = std::max({std::fabs(double(q[4]) - o.vc[v].r), std::fabs(double(q[5]) - o.vc[v].g), std::fabs(double(q[6]) - o.vc[v].b), std::fabs(double(q[7]) - o.vc[v].a)});
        k.colourWorst = std::max(k.colourWorst, ec);
        if (!(ec <= 1e-4)) k.colourOver++;
        k.vertices++;
    }
    k.dropLimit = 2 * 6e-6 * c.radius;
    std::vector<char> seen(nt, 0);
    double prev = -1e300;
    for (std::size_t i = 0; i < nt; i++) {
        const std::uint32_t t = order[i];
        if (t >= nt || seen[t]) { k.orderBad++; continue; }
        seen[t] = 1;
        const double z = o.tz[t];
        if (prev > z) { k.dropWorst = std::max(k.dropWorst, prev - z); if (prev - z > k.dropLimit) k.orderBad++; }
        prev = std::max(prev, z);
        k.triangles++;
    }
    return k;
}

std::string surfaceCompareText(const SurfaceCompare& k) {
    char b[260];
    std::snprintf(b, sizeof b, "%ld vertices, %ld triangles: %ld places over %.1e point (the largest %.1e), %ld colours over 1e-4 (the largest %.1e), %ld out of order (the largest fall %.1e bohr, limit %.1e)",
                  k.vertices, k.triangles, k.posOver, k.posLimit, k.posWorst, k.colourOver, k.colourWorst, k.orderBad, k.dropWorst, k.dropLimit);
    return b;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path {
    int state = 0;                 // 0 waiting for the surface on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false;
    int jobs[3] = {0, 0, 0};
    std::vector<float> verts[3];
    std::vector<std::uint32_t> order[3];
    std::string why, check;
    bool asked = false;            // (0.68.0: asked this frame)
};
Path g_p;
const char* kPipe = "the surface's pipelines";
const char* kCheck = "the surface's check";
const char* kFirst = "the surface's first full use";

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLSURFACECARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkSurfaceGuard();
    render::SurfaceCard* C = R.surfaceCard();
    if (!C) { off("no card programs on this start"); return; }
    const double now = nowS();
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why) || !R.surfaceProgram(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipelines could not be made: " + why); off("its pipelines could not be made: " + why); return; }
        char b[120];
        std::snprintf(b, sizeof b, "kSurfaceCS and kSurfaceVS as SPIR-V, made after %.0f ms", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    static std::vector<SurfaceCase> cases;
    if (cases.empty()) cases = surfaceCheckCases();
    if (cases.size() != 3) { G.begin(kCheck); G.end(kCheck, "its cases could not be made"); G.finished(false, "the surface's check cases could not be made"); off("the surface's check cases could not be made (the density files)"); return; }
    if (g_p.step == 1) {
        G.begin(kCheck);
        for (int i = 0; i < 3; i++) {
            g_p.jobs[i] = C->beginCheck(surfaceDrawOf(cases[std::size_t(i)]));
            if (!g_p.jobs[i]) { for (int k = 0; k < i; k++) C->cancel(g_p.jobs[k]); G.end(kCheck, "could not be started"); G.finished(false, "the surface's check could not be started"); off("the surface's check could not be started"); return; }
        }
        g_p.stepAt = now; g_p.step = 2;
        return;
    }
    if (g_p.step == 2) {
        bool pending = false;
        for (int i = 0; i < 3; i++) {
            if (!g_p.jobs[i]) continue;
            const render::JobState st = C->pollCheck(g_p.jobs[i], g_p.verts[i], g_p.order[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.jobs[i] = 0;
            if (st == render::JobState::Failed) { g_p.verts[i].clear(); g_p.order[i].clear(); }
        }
        if (pending) {
            if (now - g_p.stepAt > 10) { for (int& j : g_p.jobs) if (j) { C->cancel(j); j = 0; } G.timedOut(kCheck); off("a step of the surface on the card did not finish in time (the check)"); }
            return;
        }
        bool ok = true;
        std::string text;
        for (int i = 0; i < 3; i++) {
            const SurfaceCase& c = cases[std::size_t(i)];
            const SurfaceCompare k = compareSurface(c, g_p.verts[i], g_p.order[i]);
            ok = ok && k.ok();
            const std::string t = c.name + ": " + surfaceCompareText(k);
            text += (i ? "; " : "") + t;
            std::printf("PLSURFACECASE %d %s\n", i, t.c_str());
        }
        for (int i = 0; i < 3; i++) { g_p.verts[i].clear(); g_p.verts[i].shrink_to_fit(); g_p.order[i].clear(); g_p.order[i].shrink_to_fit(); }
        static const bool forced = std::getenv("PL_SURFACE_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) ok = false;
        char t[96];
        std::snprintf(t, sizeof t, "%s after %.0f ms: ", ok ? "passed" : "FAILED", 1000 * (now - g_p.stepAt));
        G.end(kCheck, t + text);
        g_p.check = text;
        if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_SURFACE_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
        g_p.state = 1;
        G.rest();
        std::printf("PLSURFACECARD state=1 check=%s\n", text.c_str());
        std::fflush(stdout);
    }
}
}  // namespace

bool surfaceOnCard(render::Renderer2D& R) {
    g_p.asked = true;
    if (!R.vulkan()) return false;                   // (OpenGL: the processor's triangles; no card program for them there)
    static const bool never = std::getenv("PL_NO_SURFACE_CARD") != nullptr;   // (tests: the processor's triangles on Vulkan, to compare)
    if (never) return false;
    app::GpuGuard& G = app::gpuVkSurfaceGuard();
    if (g_p.gen != G.generation()) {                 // the first frame, and "Try again" (Settings, Performance)
        g_p.gen = G.generation();
        if (g_p.state == -1) { g_p = Path(); g_p.gen = G.generation(); }
    }
    if (g_p.state == 0) {
        if (!R.computeShaders()) off("this Vulkan start has no compute programs on the card (or they are switched off)");
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
    render::SurfaceCard* C = R.surfaceCard();
    if (C) C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    if (g_p.state != 1 || !C || !C->ready) return false;
    if (!g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    return true;
}

int surfaceCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

std::string surfaceCardPathText(bool vulkan) {
    if (!vulkan) return "the surface: the processor (no card program for it on OpenGL)";
    if (std::getenv("PL_NO_SURFACE_CARD")) return "the surface: the processor (PL_NO_SURFACE_CARD)";
    if (g_p.state == 1) return "the surface: the graphics card (Vulkan; turned, coloured, sorted and drawn there, sent once for each new surface; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the surface: the processor, because " + g_p.why;
    return "the surface: the graphics card once its start-up check has passed (made when the surface is first shown)";
}


// 0.68.0: once a frame (main.cpp): a frame that did not ask (its lab not shown) still runs the check's steps, and a first full use left unfinished is
// ended and begun again when it is shown, so its guard never keeps the turn while another lab's card programs wait for it
void surfaceFrameEnd(render::Renderer2D& R) {
    const bool asked = g_p.asked;
    g_p.asked = false;
    if (asked || !R.vulkan()) return;
    if (g_p.state == 2) { advance(R); return; }
    if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone) {
        app::GpuGuard& G = app::gpuVkSurfaceGuard();
        G.end(kFirst, "not finished: its lab was left before it was drawn 3 times (begun again when it is shown)");
        G.rest();
        g_p.firstUse = false; g_p.usedFrames = 0;
        if (auto* C = R.surfaceCard()) C->ready = false;
    }
}

}  // namespace molecule
