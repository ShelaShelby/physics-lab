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
    c.ppx = W / 2; c.ppy = H / 2;                       // (0.75.0: the pinhole's principal point: the middle, nothing held off it)
    c.finish();
    return c;
}
void follow(LineCam& c, const Vec3& t) {              // orbit_view.cpp's follow: the target in the middle, at the look-at plane (0.75.0: the camera looks at it)
    c.cd = c.depth(t.x, t.y, t.z);
    double sx = 0, sy = 0;
    c.plane(t.x, t.y, t.z, sx, sy);
    c.ox = c.W / 2 + c.fx - sx; c.oy = c.H / 2 - sy;
    c.finish();
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
    // 0.73.0: the Alpha Centauri scene's ellipses (numbers of the size the scene draws: orbit/alphacen.h), far from the origin, solid and dashed
    LinesCase ac;
    ac.name = "the Alpha Centauri scene's ellipses (2D, zoom 2.5, following a planet 12,000 au out)";
    const double d = kPi / 180;
    const auto unit = [&](double incDeg, double nodeDeg, double angDeg) {   // a direction in a tilted plane
        const double ci = std::cos(incDeg * d), si = std::sin(incDeg * d), cO = std::cos(nodeDeg * d), sO = std::sin(nodeDeg * d), ca = std::cos(angDeg * d), sa = std::sin(angDeg * d);
        return Vec3{ca * cO - sa * ci * sO, ca * sO + sa * ci * cO, sa * si};
    };
    const Vec3 prox{5322 * kAU, -8740 * kAU, 6367 * kAU};
    LineEllipse eA{{0, 0, 0}, unit(79.2, 205.1, 231.5), unit(79.2, 205.1, 321.5), 10.66 * kAU, 0.51947, 10.66 * kAU * 0.8545, false};
    LineEllipse eP{{-120 * kAU, 230 * kAU, 150 * kAU}, unit(124.9, 165, 151), unit(124.9, 165, 241), 8200 * kAU, 0.497, 8200 * kAU * 0.8678, false};
    LineEllipse eb{prox, unit(47, 90, 0), unit(47, 90, 90), 0.04848 * kAU, 0, 0.04848 * kAU, false};
    LineEllipse ec{prox, unit(47, 90, 0), unit(47, 90, 90), 0.06 * kAU, 0, 0.06 * kAU, true};
    ac.ellipses = {eA, eP, eb, ec};
    ac.cam = camera(1280, 800, 2.5, false, 0, 0, Vec3{});
    follow(ac.cam, prox + eb.P * eb.a);
    v.push_back(ac);
    // 0.75.0: two views from a camera 4,200 km from the 1 AU body (3D, zoom 5.5), 1.2 degrees above its orbit's plane and looking along the orbit, so the orbit
    // (and in the second, the trail) passes about 90 km from the camera and goes on behind it: pieces cut at the near plane, points behind the camera.
    // (Its tangent at perihelion is 102.9 + 90 degrees from x: yaw + pi = atan2(0.9748, 0.2233) puts the view's depth axis along it, pitch 1.55 tips it 1.2 degrees.)
    LinesCase e = base;
    e.name = "3D at zoom 5.5 following the 1 AU body, looking back along its orbit (the orbit passes the camera and goes behind it)";
    e.cam = camera(1280, 800, 5.5, true, std::atan2(0.9748, 0.2233) - kPi, 1.55, bc);
    follow(e.cam, base.bodies[0].pos);
    v.push_back(e);
    LinesCase f = base;
    f.name = "3D at zoom 5.5 following the 1 AU body, looking ahead along its orbit (its trail passes the camera and goes behind it)";
    f.cam = camera(1280, 800, 5.5, true, std::atan2(0.9748, 0.2233), 1.55, bc);
    follow(f.cam, base.bodies[0].pos);
    v.push_back(f);
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
    for (const LineEllipse& el : c.ellipses) drawEllipseCpu(none, c.cam, el, 1.0, kOrbitCol, cpts, &pens);
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
    for (const LineEllipse& el : c.ellipses) addEllipseRuns(pts, d, key++, el, 1.0, kOrbitCol, cpts);
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
        // 0.75.0: the card's steps as segments (each cut at the near plane: 4 floats, or -1e9 when none of it is in front), the pen's as its points joined
        std::vector<double> card;
        for (int g = first; g < first + count; g++)
            for (std::uint32_t j = offsets[std::size_t(g)]; j < offsets[std::size_t(g) + 1] && 4 * std::size_t(j) + 3 < points.size(); j++)
                for (int q = 0; q < 4; q++) card.push_back(points[4 * std::size_t(j) + std::size_t(q)]);
        if (card.size() != 4 * (n - 1)) { k.runsBad++; continue; }
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
        // an end of a segment: the processor's (cut at the near plane where it crosses: its place in space by the same fraction) against the card's
        const auto endCheck = [&](const Q& q, const double* pa, const double* pb, double t, double cx, double cy) {   // t: where along a-b in space it is
            const double X = pa[0] + t * (pb[0] - pa[0]), Y = pa[1] + t * (pb[1] - pa[1]), Z = pa[2] + t * (pb[2] - pa[2]);
            const double s = c.cam.px * std::max(1.0, q.f);
            const double dist = std::sqrt((X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy) + (Z - Cz) * (Z - Cz));
            const double lim = 0.01 + 4e-6 * s * dist + scale * s;
            const double e = std::hypot(cx - q.first, cy - q.second);
            k.points++;
            k.worstPt = std::max(k.worstPt, e);
            k.worst = std::max(k.worst, e / lim);
            if (!(e <= lim)) { k.over++; if (std::getenv("PL_LINES_DEBUG") && k.over < 12) std::printf("PLLINESDBG run %zu kind %d cpu %.3f %.3f card %.3f %.3f lim %.3g dist %.4g f %.6g\n", r - 1, run.kind, q.first, q.second, cx, cy, lim, dist, q.f); }
        };
        // 0.75.0, 3D: a raw end is compared where it lies within 8 focal lengths of the camera's axis on the picture (|x - ppx|, |y - ppy| <= 8 F); farther out the
        // card's single-precision depth (rounded to about 2^-24 of the point's distance from C) is amplified by x / z, and such a point is never drawn: there the
        // drawn part of the segment (cut to the picture with its 20-point margin, as LinePen and kLinesVS draw it) is compared instead, with the same limit at
        // the cut end's own place. 2D: every end, as before.
        const double win = 8 * c.cam.F;
        const auto inWin = [&](const Q& q) { return !c.cam.d3 || (std::fabs(q.first - c.cam.ppx) <= win && std::fabs(q.second - c.cam.ppy) <= win); };
        for (std::size_t i = 1; i < n; i++) {
            const double* pa = &pen[5 * (i - 1) + 2];
            const double* pb = &pen[5 * i + 2];
            const Q qa = c.cam.proj(pa[0], pa[1], pa[2]), qb = c.cam.proj(pb[0], pb[1], pb[2]);
            Q a2{0, 0, 0, 0}, b2{0, 0, 0, 0};
            const bool cpuShown = clipNear(c.cam, qa, qb, a2, b2);
            const double* cs = &card[4 * (i - 1)];
            const bool cardShown = cs[0] > -1e37;           // (the card's -1e38: none of it in front)
            if (cpuShown != cardShown) {               // one shows the segment and the other does not: over its limit
                k.points++; k.over++; k.worst = std::max(k.worst, 1e9);
                if (std::getenv("PL_LINES_DEBUG")) std::printf("PLLINESDBG run %zu seg %zu shown cpu %d card %d (Z %.6g %.6g near %.6g)\n", r - 1, i, int(cpuShown), int(cardShown), qa.Z, qb.Z, c.cam.zNear);
                continue;
            }
            if (!cpuShown) { k.behind++; continue; }
            if (c.cam.d3 && (qa.Z < c.cam.zNear || qb.Z < c.cam.zNear)) k.cut++;
            const double tc = c.cam.d3 && qb.Z != qa.Z ? (c.cam.zNear - qa.Z) / (qb.Z - qa.Z) : 0;   // (where it crosses the near plane)
            const double ta = c.cam.d3 && qa.Z < c.cam.zNear ? tc : 0.0, tb = c.cam.d3 && qb.Z < c.cam.zNear ? tc : 1.0;
            if (inWin(a2)) endCheck(a2, pa, pb, ta, cs[0], cs[1]);
            if (inWin(b2)) endCheck(b2, pa, pb, tb, cs[2], cs[3]);
            if (inWin(a2) && inWin(b2)) continue;
            // the drawn part: both cut to the picture; an end on the picture at screen fraction u of a2-b2 is at 3D fraction (1 / Z linear in u) of it
            double cc[4], kc[4];
            const bool cpuIn = clipSeg(a2.first, a2.second, b2.first, b2.second, -20, -20, c.cam.W + 20, c.cam.H + 20, cc);
            const bool cardIn = clipSeg(cs[0], cs[1], cs[2], cs[3], -20, -20, c.cam.W + 20, c.cam.H + 20, kc);
            if (cpuIn != cardIn) {                     // drawn by one only: over its limit
                k.points++; k.over++; k.worst = std::max(k.worst, 1e9);
                if (std::getenv("PL_LINES_DEBUG")) std::printf("PLLINESDBG run %zu seg %zu drawn cpu %d card %d\n", r - 1, i, int(cpuIn), int(cardIn));
                continue;
            }
            if (!cpuIn) continue;
            const double L = std::hypot(b2.first - a2.first, b2.second - a2.second);
            for (int e = 0; e < 2; e++) {
                const double u = L > 0 ? std::hypot(cc[2 * e] - a2.first, cc[2 * e + 1] - a2.second) / L : 0;
                const double s3 = c.cam.d3 ? (1 / ((1 - u) / a2.Z + u / b2.Z) - a2.Z) / (b2.Z - a2.Z == 0 ? 1 : b2.Z - a2.Z) : u;   // its 3D fraction of a2-b2
                const double t = ta + (b2.Z - a2.Z == 0 ? u : s3) * (tb - ta);
                Q q = c.cam.d3 ? c.cam.fromCam(a2.X + (t - ta) / (tb - ta) * (b2.X - a2.X), 0, a2.Z + (t - ta) / (tb - ta) * (b2.Z - a2.Z), 0) : Q{0, 0, 0, 1};
                q.first = cc[2 * e]; q.second = cc[2 * e + 1];
                endCheck(q, pa, pb, t, kc[2 * e], kc[2 * e + 1]);
            }
        }
    }
    if (r != d.runs.size()) k.runsBad++;
    return k;
}

std::string linesCompareText(const LinesCompare& k) {
    char b[260];
    std::snprintf(b, sizeof b, "%ld runs, %ld points: %ld over the limit, %ld runs with another number of points; the largest difference %.2g of its limit (%.2g point); %ld pieces cut at the near plane, %ld behind it",
                  k.runs, k.points, k.over, k.runsBad, k.worst, k.worstPt, k.cut, k.behind);
    return b;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
constexpr int kLinesCases = 6;
struct Path {
    int state = 0;                 // 0 waiting for the lines on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false;
    int jobs[kLinesCases] = {0, 0, 0, 0, 0, 0};    // 0.73.0: four cases (the Alpha Centauri scene's ellipses); 0.75.0: six (the camera by a line)
    std::vector<float> ends[kLinesCases], pts[kLinesCases];
    std::vector<std::uint32_t> offs[kLinesCases];
    std::string why, check;
    bool asked = false;            // (0.68.0: asked this frame)
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
        for (int i = 0; i < kLinesCases; i++) {
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
        for (int i = 0; i < kLinesCases; i++) {
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
        for (int i = 0; i < kLinesCases; i++) {
            const LinesCase& c = cases[std::size_t(i)];
            std::vector<std::vector<double>> pens;
            linesCpu(c, pens);
            const LinesCompare k = compareLines(c, linesDrawOf(c), pens, g_p.ends[i], g_p.offs[i], g_p.pts[i]);
            ok = ok && k.ok();
            const std::string t = std::string(c.name) + ": " + linesCompareText(k);
            text += (i ? "; " : "") + t;
            std::printf("PLLINESCASE %d %s\n", i, t.c_str());
        }
        for (int i = 0; i < kLinesCases; i++) { g_p.ends[i].clear(); g_p.offs[i].clear(); g_p.pts[i].clear(); g_p.pts[i].shrink_to_fit(); }
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
    g_p.asked = true;
    if (!R.vulkan()) return false;                   // (no Vulkan renderer, as in the unit tests: the CPU's lines)
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
    if (!vulkan) return "the trails' and orbits' lines: the processor (no Vulkan renderer)";
    if (std::getenv("PL_NO_LINES_CARD")) return "the trails' and orbits' lines: the processor (PL_NO_LINES_CARD)";
    if (g_p.state == 1) return "the trails' and orbits' lines: the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the trails' and orbits' lines: the processor, because " + g_p.why;
    return "the trails' and orbits' lines: the graphics card once its start-up check has passed (made when the Orbit Lab is first shown)";
}


// 0.68.0: once a frame (main.cpp): a frame that did not ask (its lab not shown) still runs the check's steps, and a first full use left unfinished is
// ended and begun again when it is shown, so its guard never keeps the turn while another lab's card programs wait for it
void linesFrameEnd(render::Renderer2D& R) {
    const bool asked = g_p.asked;
    g_p.asked = false;
    if (asked || !R.vulkan()) return;
    if (g_p.state == 2) { advance(R); return; }
    if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone) {
        app::GpuGuard& G = app::gpuVkLinesGuard();
        G.end(kFirst, "not finished: its lab was left before it was drawn 3 times (begun again when it is shown)");
        G.rest();
        g_p.firstUse = false; g_p.usedFrames = 0;
        if (auto* C = R.linesCard()) C->ready = false;
    }
}

}  // namespace orbit
