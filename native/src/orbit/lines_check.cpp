#include "orbit/lines_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "orbit/units.h"

namespace orbit {

using namespace units;

namespace {
// a body on an ellipse (a in AU, e, the tilt, the node and the perihelion's angle in degrees), at its perihelion
Body ellipseBody(int id, double aAU, double e, double incDeg, double nodeDeg, double periDeg) {
    Body b;
    b.id = id;
    const double a = aAU * kAU, r = a * (1 - e), v = std::sqrt(kGMSun * (1 + e) / r), d = kPi / 180;
    const double cw = std::cos(periDeg * d), sw = std::sin(periDeg * d), ci = std::cos(incDeg * d), si = std::sin(incDeg * d), cO = std::cos(nodeDeg * d), sO = std::sin(nodeDeg * d);
    const auto turn = [&](double x, double y) {       // the plane's x, y into space: about z by the perihelion's angle, about x by the tilt, about z by the node
        const double x1 = x * cw - y * sw, y1 = x * sw + y * cw;
        const double y2 = y1 * ci, z2 = y1 * si;
        return Vec3{x1 * cO - y2 * sO, x1 * sO + y2 * cO, z2};
    };
    b.pos = turn(r, 0);
    b.vel = turn(0, v);
    return b;
}
LineCam camera(double W, double H, double zoom, bool d3, double yaw, double pitch, const Vec3& bc) {
    LineCam c;
    c.W = W; c.H = H; c.bc = bc; c.d3 = d3;
    const double yawA = d3 ? yaw + kPi : 0, pitchA = d3 ? -pitch : 0;   // (orbit_view.cpp)
    c.ca = std::cos(yawA); c.sa = std::sin(yawA); c.cb = std::cos(pitchA); c.sb = std::sin(pitchA);
    c.px = std::min(W, H) / 34 * std::pow(10.0, zoom) / kSimLength;
    c.D = std::max(90.0, 3 * 34 / std::pow(10.0, zoom)) * kSimLength;
    return c;
}
void follow(LineCam& c, const Vec3& t) {              // orbit_view.cpp's follow: the target in the middle, at the view's middle depth
    c.cd = 0; c.ox = 0; c.oy = 0;
    c.cd = c.base(t.x, t.y, t.z).dep;
    const Q q = c.base(t.x, t.y, t.z);
    c.ox = c.W / 2 + c.fx - q.first; c.oy = c.H / 2 - q.second;
}
const render::Color kCols[4] = {{0.3f, 0.6f, 1.0f, 0.15f}, {0.3f, 0.6f, 1.0f, 0.35f}, {0.3f, 0.6f, 1.0f, 0.55f}, {0.3f, 0.6f, 1.0f, 0.75f}};
const render::Color kOrbitCol{0.9f, 0.7f, 0.3f, 0.32f}, kRingCol{0.6f, 0.65f, 0.74f, 0.22f};
}  // namespace

std::vector<LinesCase> linesCheckCases() {
    LinesCase base;
    base.bodies = {ellipseBody(1, 1.0, 0.0167, 0.0, 0.0, 102.9), ellipseBody(2, 1.524, 0.0934, 1.85, 49.6, 286.5), ellipseBody(3, 5.203, 0.0484, 1.30, 100.5, 273.9),
                   ellipseBody(4, 39.48, 0.2488, 17.16, 110.3, 113.8)};
    std::vector<Vec3> pts;
    for (Body& b : base.bodies) {                     // a trail of 150 points on its own orbit, 2.5 radians of E back from the body
        orbitPath(b, -2.5, 0.0, 150, pts);
        std::deque<Vec3> tr(pts.begin(), pts.end() - 1);
        base.trails.push_back(tr);
        b.pos = pts.back();
    }
    base.rings = {1 * kAU, 2 * kAU, 5 * kAU, 10 * kAU, 20 * kAU, 50 * kAU};
    const Vec3 bc{1.1e9, -0.7e9, 2.0e7};               // (the Sun's wobble: the barycentre off the origin)
    std::vector<LinesCase> v;
    LinesCase a = base;
    a.name = "the heavy view's camera (3D, zoom x1, 1280 x 800)";
    a.cam = camera(1280, 800, 0.0, true, 0.7, 0.5, bc);
    v.push_back(a);
    LinesCase b = base;
    b.name = "the closest zoom in 2D, following the 1 AU body";
    b.cam = camera(1280, 800, 5.5, false, 0, 0, bc);
    follow(b.cam, base.bodies[0].pos);
    v.push_back(b);
    LinesCase c = base;
    c.name = "a 3D close-up following the 5.2 AU body (zoom 3)";
    c.cam = camera(1280, 800, 3.0, true, 2.0, 0.9, bc);
    follow(c.cam, base.bodies[2].pos);
    v.push_back(c);
    return v;
}

void linesCpu(const LinesCase& c, std::vector<std::vector<double>>& pens) {
    pens.clear();
    static render::Renderer2D none;                  // (never drawn into: the pens keep their points)
    std::vector<double> xyz;
    for (double Rm : c.rings) { ringPointsXYZ(Rm, xyz); drawRingCpu(none, c.cam, xyz, kRingCol, &pens); }
    for (std::size_t i = 0; i < c.trails.size(); i++) drawTrailCpu(none, c.cam, c.trails[i], c.bodies[i].pos, 0, 1.5, kCols, &pens);
    std::vector<Vec3> cpts;
    for (const Body& b : c.bodies) drawOrbitCpu(none, c.cam, b, 1.0, kOrbitCol, cpts, &pens);
}

render::LinesDraw linesDrawOf(const LinesCase& c) {
    render::LinesDraw d;
    d.cam = linesCamera(c.cam);
    LinesPoints pts;
    pts.check = &d;
    std::vector<double> xyz;
    std::uint64_t key = 1;
    for (double Rm : c.rings) { ringPointsXYZ(Rm, xyz); addRingRuns(pts, d, key++, xyz, kRingCol); }
    for (std::size_t i = 0; i < c.trails.size(); i++) addTrailRuns(pts, d, key++, c.trails[i], c.bodies[i].pos, 0, 1.5, kCols);
    std::vector<Vec3> cpts;
    for (const Body& b : c.bodies) addOrbitRuns(pts, d, key++, b, 1.0, kOrbitCol, cpts);
    return d;
}

LinesCompare compareLines(const LinesCase& c, const render::LinesDraw& d, const std::vector<std::vector<double>>& pens, const std::vector<float>& ends,
                          const std::vector<std::uint32_t>& offsets, const std::vector<float>& points) {
    LinesCompare k;
    if (offsets.size() != std::size_t(d.pieces) + 1 || ends.size() != 4 * std::size_t(d.pieces)) return k;
    const double Cx = double(d.cam.cHi[0]) + double(d.cam.cLo[0]), Cy = double(d.cam.cHi[1]) + double(d.cam.cLo[1]), Cz = double(d.cam.cHi[2]) + double(d.cam.cLo[2]);
    std::size_t r = 0;
    for (const std::vector<double>& pen : pens) {
        const std::size_t n = pen.size() / 5;
        if (n < 2) continue;                          // (a pen with its first point only draws nothing: no run on the card)
        k.runs++;
        if (r >= d.runs.size()) { k.runsBad++; continue; }
        const render::LinesRun& run = d.runs[r++];
        const int first = run.first, count = run.to - run.from + 1;
        std::vector<double> card = {ends[4 * std::size_t(first)], ends[4 * std::size_t(first) + 1]};
        for (int g = first; g < first + count; g++)
            for (std::uint32_t j = offsets[std::size_t(g)]; j < offsets[std::size_t(g) + 1] && 2 * std::size_t(j) + 1 < points.size(); j++) { card.push_back(points[2 * std::size_t(j)]); card.push_back(points[2 * std::size_t(j) + 1]); }
        if (card.size() != 2 * n) { k.runsBad++; continue; }
        // the length scale of the run's curve steps: an orbit's P a and Q b times the step of E; a trail's longest piece (times 2: the curve's neighbours)
        double scale = 0;
        if (run.kind == 2) scale = 1e-6 * (std::sqrt(double(run.pa[0]) * run.pa[0] + double(run.pa[1]) * run.pa[1] + double(run.pa[2]) * run.pa[2]) +
                                           std::sqrt(double(run.qb[0]) * run.qb[0] + double(run.qb[1]) * run.qb[1] + double(run.qb[2]) * run.qb[2])) * double(run.qb[3]);
        else if (run.kind == 1) {
            std::size_t at = 0;
            double L = 0;
            for (int g = first; g < first + count; g++) {
                const std::size_t next = at + (offsets[std::size_t(g) + 1] - offsets[std::size_t(g)]);
                if (next >= n) break;
                const double* a = &pen[5 * at + 2];
                const double* b = &pen[5 * next + 2];
                L = std::max(L, std::sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2])));
                at = next;
            }
            scale = 5e-6 * 2 * L;
        }
        for (std::size_t i = 0; i < n; i++) {
            const double* p = &pen[5 * i];
            const double f = c.cam.base(p[2], p[3], p[4]).f, s = c.cam.px * std::max(1.0, f);
            const double dist = std::sqrt((p[2] - Cx) * (p[2] - Cx) + (p[3] - Cy) * (p[3] - Cy) + (p[4] - Cz) * (p[4] - Cz));
            const double lim = 0.01 + 4e-6 * s * dist + scale * s;
            const double e = std::hypot(card[2 * i] - p[0], card[2 * i + 1] - p[1]);
            k.points++;
            k.worstPt = std::max(k.worstPt, e);
            k.worst = std::max(k.worst, e / lim);
            if (!(e <= lim)) { k.over++; if (std::getenv("PL_LINES_DEBUG") && k.over < 12) std::printf("PLLINESDBG run %zu kind %d pt %zu/%zu cpu %.3f %.3f card %.3f %.3f lim %.3g dist %.4g f %.6g\n", r - 1, run.kind, i, n, p[0], p[1], card[2 * i], card[2 * i + 1], lim, dist, f); }
        }
    }
    if (r != d.runs.size()) k.runsBad++;
    return k;
}

std::string linesCompareText(const LinesCompare& k) {
    char b[200];
    std::snprintf(b, sizeof b, "%ld runs, %ld points: %ld over the limit, %ld runs with another number of points; the largest difference %.2g of its limit (%.2g point)",
                  k.runs, k.points, k.over, k.runsBad, k.worst, k.worstPt);
    return b;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path {
    int state = 0;                 // 0 waiting for the lines on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false;
    int jobs[3] = {0, 0, 0};
    std::vector<float> ends[3], pts[3];
    std::vector<std::uint32_t> offs[3];
    std::string why, check;
};
Path g_p;
const char* kPipe = "the lines' pipelines";
const char* kCheck = "the lines' check";
const char* kFirst = "the lines' first full use";

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLLINESCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkLinesGuard();
    render::LinesCard* C = R.linesCard();
    if (!C) { off("no card programs on this start"); return; }
    const double now = nowS();
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why) || !R.linesProgram(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipelines could not be made: " + why); off("its pipelines could not be made: " + why); return; }
        char b[120];
        std::snprintf(b, sizeof b, "kLinesCS and kLinesVS as SPIR-V, made after %.0f ms", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    const std::vector<LinesCase> cases = linesCheckCases();
    if (g_p.step == 1) {
        G.begin(kCheck);
        for (int i = 0; i < 3; i++) {
            render::LinesDraw d = linesDrawOf(cases[std::size_t(i)]);
            std::vector<std::vector<double>> pens;
            linesCpu(cases[std::size_t(i)], pens);
            long total = 0;
            for (const auto& p : pens) total += long(p.size() / 5);
            d.checkRoom = int(2 * total + 4096);
            g_p.jobs[i] = C->beginCheck(d);
            if (!g_p.jobs[i]) { for (int k = 0; k < i; k++) C->cancel(g_p.jobs[k]); G.end(kCheck, "could not be started"); G.finished(false, "the lines' check could not be started"); off("the lines' check could not be started"); return; }
        }
        g_p.stepAt = now; g_p.step = 2;
        return;
    }
    if (g_p.step == 2) {
        bool pending = false;
        for (int i = 0; i < 3; i++) {
            if (!g_p.jobs[i]) continue;
            const render::JobState st = C->pollCheck(g_p.jobs[i], g_p.ends[i], g_p.offs[i], g_p.pts[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.jobs[i] = 0;
            if (st == render::JobState::Failed) { g_p.ends[i].clear(); g_p.offs[i].clear(); g_p.pts[i].clear(); }
        }
        if (pending) {
            if (now - g_p.stepAt > 10) { for (int& j : g_p.jobs) if (j) { C->cancel(j); j = 0; } G.timedOut(kCheck); off("a step of the lines on the card did not finish in time (the check)"); }
            return;
        }
        bool ok = true;
        std::string text;
        for (int i = 0; i < 3; i++) {
            const LinesCase& c = cases[std::size_t(i)];
            std::vector<std::vector<double>> pens;
            linesCpu(c, pens);
            const LinesCompare k = compareLines(c, linesDrawOf(c), pens, g_p.ends[i], g_p.offs[i], g_p.pts[i]);
            ok = ok && k.ok();
            const std::string t = std::string(c.name) + ": " + linesCompareText(k);
            text += (i ? "; " : "") + t;
            std::printf("PLLINESCASE %d %s\n", i, t.c_str());
        }
        for (int i = 0; i < 3; i++) { g_p.ends[i].clear(); g_p.offs[i].clear(); g_p.pts[i].clear(); g_p.pts[i].shrink_to_fit(); }
        static const bool forced = std::getenv("PL_LINES_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) ok = false;
        char t[96];
        std::snprintf(t, sizeof t, "%s after %.0f ms: ", ok ? "passed" : "FAILED", 1000 * (now - g_p.stepAt));
        G.end(kCheck, t + text);
        g_p.check = text;
        if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_LINES_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
        g_p.state = 1;
        G.rest();
        std::printf("PLLINESCARD state=1 check=%s\n", text.c_str());
        std::fflush(stdout);
    }
}
}  // namespace

bool linesOnCard(render::Renderer2D& R) {
    if (!R.vulkan()) return false;                   // (OpenGL: the processor's lines; no card program for them there)
    static const bool never = std::getenv("PL_NO_LINES_CARD") != nullptr;   // (tests: the processor's lines on Vulkan, to compare)
    if (never) return false;
    app::GpuGuard& G = app::gpuVkLinesGuard();
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
    render::LinesCard* C = R.linesCard();
    if (C) C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    if (g_p.state != 1 || !C || !C->ready) return false;
    if (!g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    return true;
}

int linesCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

bool linesCardSettled(bool vulkan) {
    static const bool never = std::getenv("PL_NO_LINES_CARD") != nullptr;
    return !vulkan || never || g_p.state == -1 || (g_p.state == 1 && g_p.firstDone);
}

std::string linesCardPathText(bool vulkan) {
    if (!vulkan) return "the trails' and orbits' lines: the processor (no card program for them on OpenGL)";
    if (std::getenv("PL_NO_LINES_CARD")) return "the trails' and orbits' lines: the processor (PL_NO_LINES_CARD)";
    if (g_p.state == 1) return "the trails' and orbits' lines: the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the trails' and orbits' lines: the processor, because " + g_p.why;
    return "the trails' and orbits' lines: the graphics card once its start-up check has passed (made when the Orbit Lab is first shown)";
}

}  // namespace orbit
