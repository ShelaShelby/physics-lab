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

render::ClassicDraw sideDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, double cx, double cy, double pxm, double w, double h,
                             const DotColours& dc, double weight, bool glow) {
    render::ClassicDraw d = classicDraw(pts, cnt, cap, epoch, k, grp, CloudCamera{cx, cy, pxm, 1, 0, 1, 0, 1}, w, h, dc, weight);
    d.side = true; d.soft = glow; d.half = glow ? 4.4f : 1.f;
    return d;
}

render::ClassicDraw sliceDraw(const float* pts, long cnt, long cap, std::uint64_t epoch, long k, const unsigned char* grp, double cx, double cy, double pxm, double w, double h,
                              const DotColours& dc, double weight, bool glow, bool hz, double off, double th) {
    render::ClassicDraw d = classicDraw(pts, cnt, cap, epoch, k, grp, CloudCamera{cx, cy, pxm, 1, 0, 1, 0, 1}, w, h, dc, weight);
    d.slice = hz ? 2 : 1; d.soft = glow; d.half = glow ? 4.2f : 1.25f; d.sliceOff = float(off); d.sliceTh = float(th);
    return d;
}

void sideGlowGrid(const std::vector<float>& counts, std::vector<float>& grid) { glowGridOfCounts(counts, 4.4f, grid); }

void glowGridOfCounts(const std::vector<float>& counts, float r, std::vector<float>& grid) {
    const float wt = 1.f;
    const float v = wt * r * r * 0.785f;                                   // (glowBlobs: g += d.wt * d.r * d.r * 0.785f, in float)
    grid.assign(counts.size(), 0.f);
    for (std::size_t c = 0; c < counts.size(); c++) {
        float g = 0.f;
        for (long n = long(counts[c]); n > 0; n--) g += v;
        grid[c] = g;
    }
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

ClassicCase classicSideCase(bool glow) {
    ClassicCase c = classicCheckCase();
    c.side = true; c.soft = glow;
    c.cam = CloudCamera{150, 170, 160, 1, 0, 1, 0, 1};                    // (x across, z up: a part of the cloud beyond the picture's left and lower edges)
    if (glow) c.weight = 0.37;                                              // (the dots' light, as the exposure gives it)
    return c;
}

ClassicCase classicSliceCase(bool glow, bool hz) {
    ClassicCase c = classicCheckCase();
    c.slice = hz ? 2 : 1; c.soft = glow;
    c.cam = CloudCamera{150, 170, 160, 1, 0, 1, 0, 1};
    c.off = 0.2; c.th = 0.9;                                                // (the slab 0.2 +- 0.9 of the cloud's radius 1.1: some dots beyond it)
    if (glow) c.weight = 0.37; else c.weight = 0.6 * 1.3;
    return c;
}

render::ClassicDraw classicCaseDraw(const ClassicCase& c) {
    const long n = long(c.grp.size());
    if (c.slice) return sliceDraw(c.pts.data(), n, n, 1, n, c.grp.data(), c.cam.cx, c.cam.cy, c.cam.pxm, c.w, c.h, c.dc, c.weight, c.soft, c.slice == 2, c.off, c.th);
    if (c.side) return sideDraw(c.pts.data(), n, n, 1, n, c.grp.data(), c.cam.cx, c.cam.cy, c.cam.pxm, c.w, c.h, c.dc, c.weight, c.soft);
    return classicDraw(c.pts.data(), n, n, 1, n, c.grp.data(), c.cam, c.w, c.h, c.dc, c.weight);
}

// 0.65.0: the Slice: every dot's own place in the card's list (dot order), against atom_view.cpp's Slice loop in double: shown or not (a dot within the
// float rounding of the slab's edge or within 2e-3 points of the picture's edge may differ: counted apart), its place within 2e-3 points, size, kind,
// group and opacity; the Glow's cells as the side view's
static ClassicCompare compareSlice(const ClassicCase& cs, const std::vector<float>& sq, const std::vector<std::uint32_t>& cells) {
    ClassicCompare r;
    const long cnt = long(cs.grp.size());
    if (long(sq.size()) < 20 * cnt) return r;
    const int ng = std::min(cs.dc.n, 24);
    const bool hz = cs.slice == 2;
    const int gw = int(cs.w / 8) + 2, gh = int(cs.h / 8) + 2;
    std::vector<long> want(std::size_t(gw * gh), 0), slack(want.size(), 0);
    const float half = cs.soft ? 4.2f : 1.25f, kind = cs.soft ? 6.f : 5.f;
    for (long i = 0; i < cnt; i++) {
        r.dots++;
        const double x = cs.pts[std::size_t(3 * i)], u = cs.pts[std::size_t(3 * i + (hz ? 1 : 2))], dd = cs.pts[std::size_t(3 * i + (hz ? 2 : 1))];
        const double dist = std::fabs(dd - cs.off), X = cs.cam.cx + x * cs.cam.pxm, Y = cs.cam.cy - u * cs.cam.pxm;
        const int g = cs.grp[std::size_t(i)];
        const bool cpuShown = !(dist > cs.th) && g != 255 && g < ng && !(X < -2 || X > cs.w || Y < -2 || Y > cs.h);
        const float* b = &sq[std::size_t(20 * i)];
        const bool cardShown = b[2] > 0;
        const bool edge = std::fabs(dist - cs.th) <= 1e-6 * (std::fabs(dd) + std::fabs(cs.off) + cs.th) || std::fabs(X + 2) < 2e-3 || std::fabs(X - cs.w) < 2e-3 ||
                          std::fabs(Y + 2) < 2e-3 || std::fabs(Y - cs.h) < 2e-3;
        if (cpuShown != cardShown) { if (edge) r.bandFlip++; else r.bandBad++; continue; }
        if (!cpuShown) { if (b[2] != 0.f || b[11] != 0.f) r.squareBad++; continue; }
        r.shown++;
        const double ex = cs.soft ? double(float(X)) : X - 1 + 1.25, ey = cs.soft ? double(float(Y)) : Y - 1 + 1.25;
        const double e = std::max(std::fabs(double(b[0]) - ex), std::fabs(double(b[1]) - ey));
        r.worstPlace = std::max(r.worstPlace, e);
        if (!(e <= 2e-3)) r.placeBad++;
        if (b[4] != float(g)) r.groupBad++;
        const float alpha = float(std::max(0.0, std::min(1.0, cs.weight)) * cs.dc.c[g].a);
        if (b[2] != half || b[3] != kind || !(std::fabs(b[11] - alpha) <= alpha * 2.4e-7f)) r.squareBad++;
        if (cs.soft) {
            const float fx = float(X), fy = float(Y);
            const int cx = std::max(0, std::min(gw - 1, int(fx / 8))), cy = std::max(0, std::min(gh - 1, int(fy / 8)));
            want[std::size_t(cy * gw + cx)]++;
            const double qx = fx / 8.0 - std::floor(fx / 8.0), qy = fy / 8.0 - std::floor(fy / 8.0);
            if (qx < 2.5e-4 || qx > 1 - 2.5e-4 || qy < 2.5e-4 || qy > 1 - 2.5e-4)
                for (int ax = -1; ax <= 1; ax++) for (int ay = -1; ay <= 1; ay++) {
                    const int px = cx + ax, py = cy + ay;
                    if (px >= 0 && px < gw && py >= 0 && py < gh) slack[std::size_t(py * gw + px)]++;
                }
        }
    }
    if (cs.soft) {
        r.cells = long(want.size());
        if (cells.size() != want.size()) r.cellBad = r.cells;
        else for (std::size_t c = 0; c < want.size(); c++) {
            const long d = std::labs(long(cells[c]) - want[c]);
            if (d > slack[c]) r.cellBad++; else if (d > 0) r.cellEdge++;
        }
    }
    return r;
}

ClassicCompare compareClassic(const ClassicCase& cs, const std::vector<std::uint32_t>& keys, const std::vector<float>& sq, long total, const std::vector<std::uint32_t>& cells) {
    ClassicCompare r;
    if (cs.slice) return compareSlice(cs, sq, cells);
    const long cnt = long(cs.grp.size());
    if (long(keys.size()) < 3 * cnt || long(sq.size()) < 20 * cnt) return r;
    std::vector<float> PX, PY, PD;
    if (!cs.side) projectCloud(cs.pts.data(), cnt, cs.cam, cs.w, cs.h, PX, PY, PD);
    else {                                                                  // 0.65.0: sideDots' places (in double), in view or not (NaN), one band
        PX.resize(std::size_t(cnt)); PY.resize(std::size_t(cnt)); PD.resize(std::size_t(cnt));
        for (long i = 0; i < cnt; i++) {
            const double X = cs.cam.cx + cs.pts[std::size_t(3 * i)] * cs.cam.pxm - 1, Y = cs.cam.cy - cs.pts[std::size_t(3 * i + 2)] * cs.cam.pxm - 1;
            PX[std::size_t(i)] = float(X + 1); PY[std::size_t(i)] = float(Y + 1);
            PD[std::size_t(i)] = X > -2 && X < cs.w && Y > -2 && Y < cs.h ? 0.f : NAN;
        }
    }
    const double lo = cs.side ? -1 : 0, hiX = cs.side ? cs.w + 1 : cs.w, hiY = cs.side ? cs.h + 1 : cs.h;   // (the picture's edges for the dots' final places)
    const auto bandOf = [&](long i) { return cs.side ? (std::isnan(PD[std::size_t(i)]) ? kNoBand : (unsigned char)0) : depthBandOf(PD[std::size_t(i)]); };
    const int ng = std::min(cs.dc.n, 24);
    const auto fl = [](std::uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; };
    std::map<std::uint32_t, std::vector<std::pair<float, float>>> byKey;     // the card's own dots of each key
    for (long i = 0; i < cnt; i++) {
        r.dots++;
        const std::uint32_t k = keys[std::size_t(3 * i)];
        const float X = fl(keys[std::size_t(3 * i + 1)]), Y = fl(keys[std::size_t(3 * i + 2)]);
        const unsigned char band = bandOf(i);
        const bool cpuShown = band != kNoBand && cs.grp[std::size_t(i)] < ng, cardShown = k != 0xFFFFFFFFu;
        if (cardShown) { r.shown++; byKey[k].push_back({X, Y}); }
        const bool nearEdge = std::fabs(PX[std::size_t(i)] - lo) < 2e-3 || std::fabs(PX[std::size_t(i)] - hiX) < 2e-3 || std::fabs(PY[std::size_t(i)] - lo) < 2e-3 || std::fabs(PY[std::size_t(i)] - hiY) < 2e-3;
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
        const DepthBand db = cs.side ? DepthBand{1.0, 2.0 * (cs.soft ? 4.4f : 1.f)} : kDepthBand[band];
        const float half = float(db.size / 2), alpha = float(std::max(0.0, std::min(1.0, db.alpha * cs.weight)) * cs.dc.c[g].a), kind = cs.soft ? 6.f : 5.f;
        std::vector<std::pair<float, float>> got;
        for (std::size_t j = 0; j < pl.size(); j++, at++) {
            if (at >= cnt) { r.orderBad++; break; }
            const float* b = &sq[std::size_t(20 * at)];
            got.push_back({b[0], b[1]});
            if (b[2] != half || b[3] != kind || b[4] != float(g) || !(std::fabs(b[11] - alpha) <= alpha * 2.4e-7f)) r.squareBad++;
        }
        std::sort(pl.begin(), pl.end()); std::sort(got.begin(), got.end());
        if (got != pl) r.orderBad++;
    }
    if (total != at) r.orderBad++;
    for (long j = at; j < cnt; j++) if (sq[std::size_t(20 * j + 11)] != 0.f) r.squareBad++;
    if (cs.soft) {                                                          // 0.65.0: the Glow's cells (glowBlobs: int(x / 8), int(y / 8), clamped) against the processor's
        const int gw = int(cs.w / 8) + 2, gh = int(cs.h / 8) + 2;           // count; a dot within 2e-3 points of a cell's edge may be counted in the next cell
        std::vector<long> want(std::size_t(gw * gh), 0), slack(want.size(), 0);
        for (long i = 0; i < cnt; i++) {
            if (bandOf(i) == kNoBand || cs.grp[std::size_t(i)] >= ng) continue;
            const float x = PX[std::size_t(i)], y = PY[std::size_t(i)];
            const int cx = std::max(0, std::min(gw - 1, int(x / 8))), cy = std::max(0, std::min(gh - 1, int(y / 8)));
            want[std::size_t(cy * gw + cx)]++;
            const double fx = x / 8.0 - std::floor(x / 8.0), fy = y / 8.0 - std::floor(y / 8.0);
            if (fx < 2.5e-4 || fx > 1 - 2.5e-4 || fy < 2.5e-4 || fy > 1 - 2.5e-4)
                for (int ax = -1; ax <= 1; ax++) for (int ay = -1; ay <= 1; ay++) {
                    const int qx = cx + ax, qy = cy + ay;
                    if (qx >= 0 && qx < gw && qy >= 0 && qy < gh) slack[std::size_t(qy * gw + qx)]++;
                }
        }
        r.cells = long(want.size());
        if (cells.size() != want.size()) r.cellBad = r.cells;
        else for (std::size_t c = 0; c < want.size(); c++) {
            const long d = std::labs(long(cells[c]) - want[c]);
            if (d > slack[c]) r.cellBad++; else if (d > 0) r.cellEdge++;
        }
    }
    return r;
}

std::string classicCompareText(const ClassicCompare& c) {
    char b[400];
    std::snprintf(b, sizeof b, "%ld dots, %ld shown: %ld in another group, %ld in another band (%ld at a band's edge within the rounding), %ld places beyond 2e-3 points "
                               "(the largest %.2g), %ld keys out of order, %ld squares wrong", c.dots, c.shown, c.groupBad, c.bandBad + c.bandFlip, c.bandFlip, c.placeBad, c.worstPlace, c.orderBad, c.squareBad);
    std::string t = b;
    if (c.cells > 0) { std::snprintf(b, sizeof b, "; %ld cells, %ld beyond their limit (%ld differ by a dot at a cell's edge)", c.cells, c.cellBad, c.cellEdge); t += b; }
    return t;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path { int state = 0, step = 0, frames = 0, gen = -1, usedFrames = 0, job[5] = {0, 0, 0, 0, 0}; double stepAt = 0; bool firstUse = false, firstDone = false; std::string why, check; };
Path g_p;
const char* kPipe = "the Classic dots' pipeline";
const char* kCheck = "the Classic dots' check";
const char* kFirst = "the Classic dots' first full use";
// the check's cases (0.65.0: the side view's Classic squares and Glow dots with their cells, the Slice's Classic squares (a vertical cut) and Glow dots (a
// horizontal one) too)
constexpr int kCases = 5;
const char* const kCaseName[kCases] = {"Cloud 3D", "the side view, Classic", "the side view, Glow", "the Slice, Classic", "the Slice, Glow"};
const ClassicCase& checkCase(int k) {
    static const ClassicCase c[kCases] = {classicCheckCase(), classicSideCase(false), classicSideCase(true), classicSliceCase(false, false), classicSliceCase(true, true)};
    return c[k];
}

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
        bool started = true;
        for (int k = 0; k < kCases; k++) started = (g_p.job[k] = C->beginCheck(classicCaseDraw(checkCase(k)))) != 0 && started;
        if (!started) { G.end(kCheck, "could not be started"); G.finished(false, "the Classic dots' check could not be started"); off("the Classic dots' check could not be started"); return; }
        g_p.stepAt = nowS(); g_p.step = 2;
        return;
    }
    // (the jobs come back in order: the first still pending means all are)
    static ClassicCompare done[kCases];
    static int doneN = 0;
    static bool batchOk = true;
    if (doneN == 0) batchOk = true;
    while (doneN < kCases) {
        std::vector<std::uint32_t> keys;
        std::vector<float> sq;
        long total = 0;
        const render::JobState st = C->pollCheck(g_p.job[doneN], keys, sq, total);
        if (st == render::JobState::Pending) { if (nowS() - g_p.stepAt > 10) { doneN = 0; G.timedOut(kCheck); off("a step of the Classic dots on the card did not finish in time (the check)"); } return; }
        done[doneN] = st == render::JobState::Done ? compareClassic(checkCase(doneN), keys, sq, total, C->checkCells()) : ClassicCompare();
        batchOk = batchOk && st == render::JobState::Done;
        doneN++;
    }
    doneN = 0;
    bool ok = batchOk;
    for (int k = 0; k < kCases; k++) ok = ok && done[k].ok();
    static const bool forced = std::getenv("PL_CLASSIC_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
    if (forced) ok = false;
    std::string text = batchOk ? std::string() : std::string("the batch failed on the card");
    for (int k = 0; k < kCases && batchOk; k++) text += std::string(k ? " | " : "") + kCaseName[k] + " " + classicCompareText(done[k]);
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

std::string classicCardPathText(bool vulkan, const std::string& what) {
    if (!vulkan) return what + ": the processor (no card program for them on OpenGL)";
    if (g_p.state == 1) return what + ": the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return what + ": the processor, because " + g_p.why;
    return what + ": the graphics card once its start-up check has passed (made when the Classic Cloud 3D or the side view with every electron is first shown)";
}

}  // namespace atom
