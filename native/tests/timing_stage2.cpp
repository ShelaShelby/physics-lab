// 0.13 stage 2, step 2 of the brief: WHERE the CPU time of one frame goes in the benchmark scenes 11 to 15 (the cloud jump and the three superpositions).
// A relative guide only: this machine's CPU, one core, no graphics. It runs the same calls atom_view.cpp makes for a Classic Cloud 3D frame and times each part:
//   step      AtomLab::tick + step (what the lab does per frame: the beat clock, the jump's building, the walker that samples new dots)
//   groups    the colour group (thinning decision + phase bin) of every dot: lab.jumpGroups, or lab.superGroup in a loop
//   copy      the jump only: the two populations of dots are concatenated into one array every frame
//   project   the 3D projection of every dot
//   build     depth band, counting sort and one blob per dot (cloud_dots.cpp)
//   upload    a stand-in: copying the blobs twice (renderer's vector, then the driver's copy), 80 bytes a dot; the real glBufferData is not measured here
// Built by -DPL_TIMING=ON (target stage2_timing); not a test (it prints numbers and always exits 0).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "atom/atom_bench.h"
#include "atom/cloud_dots.h"
#include "atom/cloud_light.h"

using namespace atom;
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

int main(int argc, char** argv) {
    const int frames = argc > 1 ? std::atoi(argv[1]) : 120;   // (scene 12 needs about 1,500 to see several jump starts)
    struct Sc { int which; const char* name; };
    const Sc list[] = {{8, "11 jump held half way"}, {9, "12 jumps one after another"}, {10, "13 2p z + 2p x"}, {11, "14 2p x + 2p m+1 (overlap)"}, {12, "15 2p z + 3d z2 (beats)"}, {5, "8 plain orbital, Wheel"}, {6, "9 plain orbital, Sign"}};
    std::printf("%-30s %7s %7s %7s %7s %7s %7s %8s %8s\n", "scene (ms per frame, mean)", "step", "groups", "copy", "project", "build", "upload*", "total", "worst");
    for (const Sc& sc : list) {
        AtomLab L;
        benchSetup(L, sc.which, 1040, 713);
        std::vector<float> allPts, PX, PY, PD;
        std::vector<unsigned char> grp;
        std::vector<render::Renderer2D::Blob> B, up1, up2;
        double t[6] = {0}, worst = 0, stepMax = 0;
        int counted = 0;
        for (int f = 0; f < 60 + frames; f++) {
            benchFrame(L, sc.which, f, f < 60 ? -1 : f - 60);
            const auto t0 = Clock::now();
            L.tick(1.0 / 240); L.step(1.0 / 240);
            const auto t1 = Clock::now();
            const long cap = std::min<long>(L.cloudCap(), long(L.s.sg.size()));
            long cnt = std::min<long>(L.s.k, cap);
            const bool jump = L.jumpDrawn();
            const float* PTS = L.s.pts.data();
            const auto t2a = Clock::now();
            if (jump) {
                const long jn = L.jumpDots();
                allPts.assign(L.s.pts.begin(), L.s.pts.begin() + 3 * jn); allPts.insert(allPts.end(), L.s.jp.begin(), L.s.jp.begin() + 3 * jn);
                PTS = allPts.data(); cnt = 2 * jn;
            }
            const auto t2 = Clock::now();
            grp.resize(std::size_t(cnt));
            if (jump) L.jumpGroups(grp.data());
            else if (L.superOn()) { const double th = L.beatTheta(); for (long i = 0; i < cnt; i++) grp[std::size_t(i)] = (unsigned char)L.superGroup(i, th); }
            else plainGroups(L.usesWheel(), L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
            const auto t3 = Clock::now();
            const double m = std::min(1040.0, 713.0), pxm = L.pxPerM(m, L.s.szCur) * std::pow(10, L.p.zoom), ext = kExt[L.sizeN()] / L.p.Z * kA0;
            projectCloud(PTS, cnt, CloudCamera{520, 356.5, pxm, std::cos(L.s.yaw), std::sin(L.s.yaw), std::cos(L.s.pitch), std::sin(L.s.pitch), ext}, 1040, 713, PX, PY, PD);
            const auto t4 = Clock::now();
            const DotColours dc = makeDotColours(true, L.usesWheel(), false, render::Color{0.42f, 0.61f, 1.f, 1.f}, render::Color{0.94f, 0.54f, 0.24f, 1.f});
            buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(cap), B);
            const auto t5 = Clock::now();
            up1 = B; up2 = up1;
            const auto t6 = Clock::now();
            if (f >= 60) {
                t[0] += ms(t0, t1); t[1] += ms(t2, t3); t[2] += ms(t2a, t2); t[3] += ms(t3, t4); t[4] += ms(t4, t5); t[5] += ms(t5, t6);
                worst = std::max(worst, ms(t0, t6)); stepMax = std::max(stepMax, ms(t0, t1)); counted++;
            }
        }
        const double tot = t[0] + t[1] + t[2] + t[3] + t[4] + t[5];
        std::printf("%-30s %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %8.2f %8.2f\n", sc.name, t[0] / counted, t[1] / counted, t[2] / counted, t[3] / counted, t[4] / counted, t[5] / counted, tot / counted, worst);
        std::printf("%-30s   slowest step() of the run: %.2f ms\n", "", stepMax);
    }
    std::printf("* upload is a stand-in (two copies of the blobs), not a glBufferData\n");
    return 0;
}
