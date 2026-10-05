// 0.13 stage 4, step 1 of the brief: WHERE the CPU time goes in the benchmark scenes 1 and 6 (Orbit Lab rocks), 19 to 22 (the chart of nuclides and the landscape) and in scene 10 against scene 8
// (Cloud 2D against Cloud 3D, Wheel). A relative guide only: this machine's CPU, one core, no graphics. The pieces replay what orbit_view.cpp and atom_view.cpp do, with the real functions:
//   rocks:   position (beltPoint for every rock), project (the camera of scene 1 for every rock and the on-screen test), copy (a stand-in for the upload: the on-screen centres copied twice)
//   chart:   build (ChartScene::build, only when the view changes), draw (the drawing calls: the squares/blocks, notes, legend; the text is a dummy that measures but draws nothing, Dear ImGui's
//            text is not part of this tool), vertex bytes (what the renderer sends to the GPU every frame)
//   cloud:   step, groups, project (3D) or sideDots (2D), build (blobs), the counted dots
// Built by -DPL_TIMING=ON (target stage4_timing); not a test (it prints numbers and always exits 0).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "atom/atom_bench.h"
#include "atom/chart_draw.h"
#include "atom/chart_view.h"
#include "atom/cloud_dots.h"
#include "atom/cloud_light.h"
#include "orbit/solar_system.h"
#include "render/renderer2d.h"

namespace plgl {   // the GL function pointers are never called here (nothing is drawn); they only have to exist
#define PL_GL_DEFINE(ret, name, args) PFN_##name name = nullptr;
PL_GL_FUNCTIONS(PL_GL_DEFINE)
PL_GL43_FUNCTIONS(PL_GL_DEFINE)
#undef PL_GL_DEFINE
bool loadGL43(const char**) { return false; }
}  // namespace plgl

using namespace atom;
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

namespace {
struct DummyText : ChartText {
    double width(const std::string& t, double size, bool) const override { return 0.55 * size * double(t.size()); }
    void draw(const std::string&, double, double, double, render::Color, double, bool) const override {}
};

void rocks(int scale) {
    orbit::SolarSystem sys;
    sys.loadAt(1767268800000LL);                                           // 2026-01-01 12:00 UTC
    if (scale != 1) sys.belts = orbit::makeBelts(sys.epochMs, scale);
    const double W = 1040, H = 713, zoom = 0.0;
    const double px = std::min(W, H) / 34 * std::pow(10.0, zoom) / units::kSimLength;
    const double ca = std::cos(0.6 + units::kPi), sa = std::sin(0.6 + units::kPi), cb = std::cos(-1.0), sb = std::sin(-1.0);
    const double D = std::max(90.0, 3 * 34 / std::pow(10.0, zoom)) * units::kSimLength;
    long total = 0;
    for (const auto& g : sys.belts) total += g.n;
    std::vector<std::vector<orbit::Vec3>> pos(sys.belts.size());
    std::vector<float> scr, up1, up2;
    double tStep = 0, tPos = 0, tProj = 0, tCopy = 0, worst = 0;
    int counted = 0;
    long shown = 0;
    for (int f = 0; f < 240; f++) {
        const auto t0 = Clock::now();
        sys.step(units::kPhysicsStep, units::kTspeedMax);
        const auto t1 = Clock::now();
        for (std::size_t gi = 0; gi < sys.belts.size(); gi++) { pos[gi].resize(std::size_t(sys.belts[gi].n)); for (int k = 0; k < sys.belts[gi].n; k++) pos[gi][std::size_t(k)] = orbit::beltPoint(sys.belts[gi], k, sys.t); }
        const auto t2 = Clock::now();
        shown = 0;
        scr.clear();
        for (const auto& pg : pos) for (const orbit::Vec3& p : pg) {
            const double x1 = p.x * ca - p.y * sa, y1 = p.x * sa + p.y * ca, y2 = y1 * cb - p.z * sb, dep = y1 * sb + p.z * cb;
            const double fpr = D / std::max(D - dep, 0.05 * D);
            const double X = W / 2 + x1 * px * fpr, Y = H / 2 - y2 * px * fpr;
            if (X > -2 && X < W + 2 && Y > -2 && Y < H + 2) { scr.push_back(float(X)); scr.push_back(float(Y)); shown++; }
        }
        const auto t3 = Clock::now();
        up1 = scr; up2 = up1;
        const auto t4 = Clock::now();
        if (f >= 40) { tStep += ms(t0, t1); tPos += ms(t1, t2); tProj += ms(t2, t3); tCopy += ms(t3, t4); worst = std::max(worst, ms(t1, t4)); counted++; }
    }
    std::printf("rocks x%-2d %6ld rocks, %6ld on screen   step %6.3f  position %6.3f  project %6.3f  copy* %6.3f   rocks total %6.3f  worst %6.3f ms\n", scale, total, shown, tStep / counted, tPos / counted, tProj / counted, tCopy / counted,
                (tPos + tProj + tCopy) / counted, worst);
}

void chart(int which, const char* name) {
    AtomLab L;
    benchSetup(L, which, 1040, 713);
    render::Renderer2D R;
    const DummyText T;
    const ChartColours C{render::Color{0.9f, 0.9f, 0.9f, 1}, render::Color{0.6f, 0.6f, 0.65f, 1}, render::Color{0.4f, 0.6f, 1, 1}, render::Color{0.1f, 0.11f, 0.14f, 1}, render::Color{0.3f, 0.3f, 0.35f, 1}};
    const bool flat = L.s.chartView.flat();
    const ChainOverlay chainOv{L.s.showChain ? &L.decayGraph() : nullptr, &L.s.walk};
    double tBuild = 0, tDraw = 0, worst = 0;
    int counted = 0;
    std::size_t bytes = 0;
    {   // the build when the view changes (once here)
        const auto t0 = Clock::now();
        L.viewW = 1040; L.viewH = 713;
        if (!flat) (void)L.chartScene();
        tBuild = ms(t0, Clock::now());
    }
    for (int f = 0; f < 80; f++) {
        const auto t0 = Clock::now();
        R.begin(1040, 713);
        R.rect(0, 0, 1040, 713, C.card);
        if (flat) drawChart(R, T, C, 1040, 713, L.p.Z, L.p.N, 0, 0, L.s.showChain ? &chainOv : nullptr);
        else drawChartScene(R, T, C, L.chartScene(), 1040, 713, L.p.Z, L.p.N, 0, 0, L.s.showChain ? &chainOv : nullptr);
        const auto t1 = Clock::now();
        bytes = R.pendingVertexBytes();
        if (f >= 20) { tDraw += ms(t0, t1); worst = std::max(worst, ms(t0, t1)); counted++; }
    }
    std::printf("scene %-2d %-30s build (view change only) %6.2f   draw %6.3f (worst %6.3f)   vertex bytes sent each frame %8.2f MB\n", which + 3, name, tBuild, tDraw / counted, worst, bytes / 1048576.0);
}

void cloud(int which, const char* name) {
    AtomLab L;
    benchSetup(L, which, 1040, 713);
    const bool d3 = L.p.view == "Cloud 3D";
    std::vector<float> PX, PY, PD;
    std::vector<unsigned char> grp;
    std::vector<render::Renderer2D::Blob> B;
    std::vector<SideDot> side;
    double t[4] = {0, 0, 0, 0};
    int counted = 0;
    long dots = 0, drawn = 0;
    for (int f = 0; f < 140; f++) {
        benchFrame(L, which, f, f < 60 ? -1 : f - 60);
        const auto t0 = Clock::now();
        L.tick(1.0 / 240); L.step(1.0 / 240);
        const auto t1 = Clock::now();
        const long cap = std::min<long>(L.cloudCap(), long(L.s.sg.size()));
        const long cnt = std::min<long>(L.s.k, cap);
        grp.resize(std::size_t(cnt));
        plainGroups(L.usesWheel(), L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
        const auto t2 = Clock::now();
        const double m = std::min(1040.0, 713.0), pxm = L.pxPerM(m, L.s.szCur) * std::pow(10, L.p.zoom), ext = kExt[L.sizeN()] / L.p.Z * kA0;
        const DotColours dc = makeDotColours(L.gpuColours, L.usesWheel(), false, render::Color{0.42f, 0.61f, 1.f, 1.f}, render::Color{0.94f, 0.54f, 0.24f, 1.f});
        B.clear();
        if (d3) {
            projectCloud(L.s.pts.data(), cnt, CloudCamera{520, 356.5, pxm, std::cos(L.s.yaw), std::sin(L.s.yaw), std::cos(L.s.pitch), std::sin(L.s.pitch), ext}, 1040, 713, PX, PY, PD);
            const auto t3 = Clock::now();
            buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(cap), B);
            const auto t4 = Clock::now();
            if (f >= 60) { t[0] += ms(t0, t1); t[1] += ms(t1, t2); t[2] += ms(t2, t3); t[3] += ms(t3, t4); counted++; }
        } else {
            const auto t2b = Clock::now();
            sideDots(L.s.pts.data(), cnt, grp.data(), dc.n, 520, 356.5, pxm, 1040, 713, L.gpuColours, side);
            const auto t3 = Clock::now();
            for (const SideDot& d : side) B.push_back(squareDot(dc, d.x, d.y, 1, d.g, 0.3 * cloudDotWeight(cap)));
            const auto t4 = Clock::now();
            if (f >= 60) { t[0] += ms(t0, t1); t[1] += ms(t1, t2); t[2] += ms(t2b, t3); t[3] += ms(t3, t4); counted++; }
        }
        dots = cnt; drawn = long(B.size());
    }
    std::printf("scene %-2d %-34s step %6.3f  groups %6.3f  %s %6.3f  build %6.3f  total %6.3f ms   dots %ld, drawn %ld\n", which + 3, name, t[0] / counted, t[1] / counted, d3 ? "project" : "sideDots", t[2] / counted, t[3] / counted,
                (t[0] + t[1] + t[2] + t[3]) / counted, dots, drawn);
}
}  // namespace

int main() {
    std::printf("== Orbit Lab rocks (scenes 1 and 6; * the copy is a stand-in for the upload, 8 bytes per rock on screen)\n");
    rocks(1);
    rocks(8);
    std::printf("\n== chart of nuclides (scenes 19 to 22), vertex bytes = what the renderer uploads every frame\n");
    chart(16, "chart flat");
    chart(17, "landscape");
    chart(18, "landscape, binding height");
    chart(19, "landscape with the chain");
    std::printf("\n== Cloud 3D against Cloud 2D, Wheel, 48,000 dots (scenes 8 and 10)\n");
    cloud(5, "Cloud 3D Wheel");
    cloud(7, "Cloud 2D Wheel");
    cloud(20, "Cloud 3D Wheel, GPU colours Off");
    cloud(21, "Cloud 2D Wheel, GPU colours Off");
    return 0;
}
