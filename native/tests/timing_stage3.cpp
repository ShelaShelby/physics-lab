// 0.13 stage 3, step A1 of the brief: WHERE the CPU time of the 90 % outline goes (scene 16, "first use": the 64 x 64 x 64 pass, run once per orbital; scene 17: the Slice's contour,
// redone every frame while the cut moves). A relative guide only: this machine's CPU, one core, no graphics. Pieces replay outline.cpp's outlineOf with the real functions:
//   norm      orbitalNorm (the radial sum and the angular average)
//   grid      262,144 density values, float(density(...))
//   sort      the densest-first sort of the whole grid, to find the 90 % level
//   level     the running sum down the sorted list
//   rest      everything else of outlineOf (the mass and reach loops, the side projection, 15 planes of marching squares): total minus the four above
//   finish    outlineFromGrid alone: the CPU work that is LEFT on the GPU path (the grid and the level come from the GPU; the lines are built on the CPU with this)
// and for the Slice: the 200 x 200 density values, and the whole outlineSlice call (the marching squares is the difference).
// Built by -DPL_TIMING=ON (target stage3_timing); not a test (it prints numbers and always exits 0).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "atom/outline.h"

using namespace atom;
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
template <class F> static double best(int reps, F f) { double b = 1e30; for (int i = 0; i < reps; i++) { const auto t0 = Clock::now(); f(); b = std::min(b, ms(t0, Clock::now())); } return b; }

int main() {
    const char* keys[] = {"4f|z3", "3d|z2", "2p|z", "1s|"};
    std::printf("%-8s %8s %8s %8s %8s %8s %8s %8s\n", "outline", "norm", "grid", "sort", "level", "rest", "total", "finish");
    for (const char* key : keys) {
        const int orb = orbIndex(key[3] == '|' && key[4] == 0 ? "1s" : key);
        const Orbital& o = orbitals()[orb];
        const double tNorm = best(3, [&] { volatile double v = orbitalNorm(orb); (void)v; });
        const int N = kOutlineGrid; const double half = 1.15 * kExt[o.n], h = 2 * half / N;
        std::vector<float> f(std::size_t(N) * N * N);
        const double tGrid = best(3, [&] {
            for (int k = 0; k < N; k++) for (int j = 0; j < N; j++) for (int i = 0; i < N; i++)
                f[(std::size_t(k) * N + j) * N + i] = float(density(orb, 1, -half + (i + 0.5) * h, -half + (j + 0.5) * h, -half + (k + 0.5) * h));
        });
        std::vector<float> sorted;
        const double tSort = best(3, [&] { sorted = f; std::sort(sorted.begin(), sorted.end(), std::greater<float>()); });
        const double norm = orbitalNorm(orb), dV = h * h * h;
        const double tLevel = best(3, [&] { double acc = 0; float c = sorted.back(); for (float v : sorted) { acc += double(v) * dV / norm; if (acc >= kOutlineProb) { c = v; break; } } volatile float s = c; (void)s; });
        const double tTotal = best(3, [&] { clearOutlineCache(); (void)outlineOf(orb); });
        const float lvl = outlineLevelCpu(f, dV, norm);
        const double tFinish = best(3, [&] { (void)outlineFromGrid(orb, f, lvl); });
        std::printf("%-8s %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n", o.key.c_str(), tNorm, tGrid, tSort, tLevel, tTotal - tNorm - tGrid - tSort - tLevel, tTotal, tFinish);
    }
    std::printf("\nSlice (scene 17), 4f, one frame's contour: n = 200\n");
    const int orb = orbIndex("4f|z3");
    (void)outlineOf(orb);
    const Orbital& o = orbitals()[orb];
    const double half = 1.15 * kExt[o.n], h = 2 * half / 200;
    std::vector<float> g(40000);
    const double tVals = best(5, [&] { for (int b = 0; b < 200; b++) for (int a = 0; a < 200; a++) g[std::size_t(b) * 200 + a] = float(density(orb, 1, -half + (a + 0.5) * h, 0.3, -half + (b + 0.5) * h)); });
    const double tAll = best(5, [&] { (void)outlineSlice(orb, false, 0.3, 200); });
    std::printf("density values %.2f ms, whole outlineSlice %.2f ms (marching squares and the segment list %.2f ms)\n", tVals, tAll, tAll - tVals);
    return 0;
}
