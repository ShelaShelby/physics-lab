#include "outline.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>

#include "atom_core.h"

namespace atom {

namespace {

// Marching squares on a grid of values g (nx × ny, g[j * nx + i] at x = x0 + i h, y = y0 + j h): the segments of the line g = c, with the crossings
// found by linear interpolation along the cell edges.
template <class Emit>
void marching(const float* g, int nx, int ny, double c, double x0, double y0, double h, Emit emit) {
    for (int j = 0; j + 1 < ny; j++)
        for (int i = 0; i + 1 < nx; i++) {
            const double v[4] = {g[j * nx + i], g[j * nx + i + 1], g[(j + 1) * nx + i + 1], g[(j + 1) * nx + i]};   // corners a b c d, going round
            const int idx = (v[0] >= c) | ((v[1] >= c) << 1) | ((v[2] >= c) << 2) | ((v[3] >= c) << 3);
            if (idx == 0 || idx == 15) continue;
            const double px[4] = {x0 + i * h, x0 + (i + 1) * h, x0 + (i + 1) * h, x0 + i * h}, py[4] = {y0 + j * h, y0 + j * h, y0 + (j + 1) * h, y0 + (j + 1) * h};
            // edge k joins corner k and corner k+1: its crossing point
            const auto cross = [&](int k, double& x, double& y) {
                const int a = k, b = (k + 1) & 3;
                const double d = v[b] - v[a], t = d != 0 ? (c - v[a]) / d : 0.5;
                x = px[a] + (px[b] - px[a]) * t; y = py[a] + (py[b] - py[a]) * t;
            };
            // which edges the line crosses, paired up (two pairs for the saddle cases 5 and 10, split by the middle value)
            static const int tab[16][4] = {{-1, -1, -1, -1}, {3, 0, -1, -1}, {0, 1, -1, -1}, {3, 1, -1, -1}, {1, 2, -1, -1}, {-2, -2, -2, -2}, {0, 2, -1, -1}, {3, 2, -1, -1},
                                           {2, 3, -1, -1}, {2, 0, -1, -1}, {-3, -3, -3, -3}, {2, 1, -1, -1}, {1, 3, -1, -1}, {1, 0, -1, -1}, {0, 3, -1, -1}, {-1, -1, -1, -1}};
            int e[4] = {tab[idx][0], tab[idx][1], tab[idx][2], tab[idx][3]};
            if (e[0] == -2 || e[0] == -3) {
                const bool centreIn = 0.25 * (v[0] + v[1] + v[2] + v[3]) >= c;
                if (e[0] == -2) { if (centreIn) { e[0] = 3; e[1] = 2; e[2] = 1; e[3] = 0; } else { e[0] = 3; e[1] = 0; e[2] = 1; e[3] = 2; } }
                else { if (centreIn) { e[0] = 3; e[1] = 0; e[2] = 1; e[3] = 2; } else { e[0] = 3; e[1] = 2; e[2] = 1; e[3] = 0; } }
            }
            for (int s = 0; s < 4; s += 2) {
                if (e[s] < 0) break;
                double ax, ay, bx, by;
                cross(e[s], ax, ay); cross(e[s + 1], bx, by);
                emit(ax, ay, bx, by);
            }
        }
}

std::map<int, std::unique_ptr<Outline>>& cache() { static std::map<int, std::unique_ptr<Outline>> m; return m; }

}  // namespace

// The EXACT normalisation integral of the lab's unnormalised psi at Z = 1 (0.13 stage 3; before, a numerical sum that left the amplitude 3e-6 (2p) to 1.6e-5 (3d z2) too big).
// psi = R~(r) P(x, y, z) with R~ = L_(n-l-1)^(2l+1)(2r/n) e^(-r/n) and P the angular polynomial (it holds the r^l), so
//   integral |psi|^2 = [ integral R~^2 r^(2l+2) dr ] x [ integral |P|^2 over the unit sphere ].
// Radial part, with x = 2r/n, a = 2l+1, k = n-l-1: integral x^(a+1) e^(-x) [L_k^a(x)]^2 dx = (2k+a+1) (k+a)!/k! (the Laguerre orthogonality integral of x^a e^(-x) L_k^2 = (k+a)!/k!, and the three-term
// recurrence x L_k = (2k+a+1) L_k - (k+1) L_(k+1) - (k+a) L_(k-1)): the NIST Digital Library of Mathematical Functions, chapter 18, and Abramowitz and Stegun, chapter 22 (the standard formulas, written
// from knowledge: the section numbers were not opened in this session, so none is quoted; the result is checked by hand and by an independent quadrature in tests/test_outline_norm.inc). Here 2k+a+1 = 2n and k+a = n+l, and r^(2l+2) dr = (n/2)^(2l+3) x^(a+1) dx, so
//   radial = (n/2)^(2l+3) 2n (n+l)!/(n-l-1)!      (checked by hand for 1s: 1/4; 2p: 24 ...)
// Angular part: the sphere integral of the squared polynomial, from the moments of x, y, z over the sphere (4 pi (2a-1)!!(2b-1)!!(2c-1)!!/(2(a+b+c)+1)!! for x^2a y^2b z^2c): s 4 pi; x, y, z 4 pi/3; z2 = 3z^2-r^2 16 pi/5;
// xz, yz, xy 4 pi/15; x2-y2 16 pi/15; z(5z^2-3r^2) 16 pi/7; x(5z^2-r^2), y(5z^2-r^2) 32 pi/21; xyz 4 pi/105; z(x^2-y^2) 16 pi/105; x(x^2-3y^2), y(3x^2-y^2) 32 pi/35 (they are the cubic harmonics,
// combinations of the Y_l^m with the normalisation of Griffiths ch. 4). A complex state (Re + i c Im)/sqrt(2) has the norm of its real partner: (N_Re + c^2 N_Im)/2 is the same number (tested).
double angularNormSq(int orb) {
    constexpr double pi = 3.14159265358979323846;
    static const double real[16] = {4 * pi, 4 * pi / 3, 4 * pi / 3, 4 * pi / 3, 16 * pi / 5, 4 * pi / 15, 4 * pi / 15, 4 * pi / 15, 16 * pi / 15,
                                    16 * pi / 7, 32 * pi / 21, 32 * pi / 21, 4 * pi / 105, 16 * pi / 105, 32 * pi / 35, 32 * pi / 35};
    const Orbital& o = orbitals()[stateOrbital(orb)];
    if (!isComplexState(orb)) return real[o.ang];
    const int l = o.l, m = std::abs(stateM(orb));
    if (l == 1) return real[1];                                               // (x + i y) / sqrt 2
    if (l == 2) return m == 1 ? real[5] : real[8];                            // xz, yz;  x2-y2, xy
    return m == 1 ? real[10] : m == 2 ? real[13] : real[14];                  // xz2, yz2;  z(x2-y2), xyz;  x(x2-3y2), y(3x2-y2)
}

double orbitalNorm(int orb) {
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const int n = o.n, l = o.l;
    double fup = 1, fdown = 1;                                               // (n+l)! and (n-l-1)!
    for (int i = 2; i <= n + l; i++) fup *= i;
    for (int i = 2; i <= n - l - 1; i++) fdown *= i;
    return std::pow(n / 2.0, 2 * l + 3) * 2 * n * fup / fdown * angularNormSq(orb);
}

// ---- the pieces of the outline (public, 0.13 stage 3): the CPU reference of what the GPU does, and the part both paths share ----
render::OutlineDensity outlineDensityOf(int orb) {
    const Orbital& o = orbitals()[stateOrbital(orb)];
    render::OutlineDensity d;
    d.n = float(o.n);
    for (int i = 0; i < 4; i++) d.lag[i] = i < int(o.lag.size()) ? float(o.lag[std::size_t(i)]) : 0.f;       // the coefficients of L_(n-l-1)^(2l+1) (n <= 4: at most 4 of them)
    if (isComplexState(orb)) {
        int re = 0, im = 0; double c = 1;
        complexPairOf(o.l, std::abs(stateM(orb)), re, im, c);
        d.ang = re; d.angIm = im; d.cIm = float(c); d.weight = 0.5f;                                            // |(Re + i c Im) / sqrt 2|^2 = (Re^2 + c^2 Im^2) / 2
    } else {
        d.ang = o.ang; d.angIm = 0; d.cIm = 0.f; d.weight = 1.f;
    }
    return d;
}

std::vector<float> outlineGridCpu(int orb) {
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const int N = kOutlineGrid;
    const double half = 1.15 * kExt[o.n], h = 2 * half / N;
    const auto at = [&](int i) { return -half + (i + 0.5) * h; };
    std::vector<float> f(std::size_t(N) * N * N);
    for (int k = 0; k < N; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++) f[(std::size_t(k) * N + j) * N + i] = float(density(orb, 1, at(i), at(j), at(k)));
    return f;
}

float outlineLevelCpu(const std::vector<float>& grid, double dV, double norm) {
    // the level: add the densest voxels first until they hold 90 % of the probability
    std::vector<float> sorted(grid);
    std::sort(sorted.begin(), sorted.end(), std::greater<float>());
    double acc = 0;
    float c = sorted.back();
    for (float v : sorted) { acc += double(v) * dV / norm; if (acc >= kOutlineProb) { c = v; break; } }
    return c;
}

std::vector<float> outlinePlaneCpu(int orb, double half, bool horizontal, double offset, int n) {
    const double h = 2 * half / n;
    std::vector<float> g(std::size_t(n) * n);
    for (int b = 0; b < n; b++)
        for (int a = 0; a < n; a++) {
            const double u = -half + (a + 0.5) * h, v = -half + (b + 0.5) * h;
            g[std::size_t(b) * n + a] = float(horizontal ? density(orb, 1, u, v, offset) : density(orb, 1, u, offset, v));   // horizontal: plane z = offset (x, y); vertical: plane y = offset (x, z)
        }
    return g;
}

Outline outlineFromGrid(int orb, const std::vector<float>& f, float level) {
    Outline O;
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const int N = kOutlineGrid;
    O.orb = orb;
    O.norm = orbitalNorm(orb);
    O.half = 1.15 * kExt[o.n];
    const double h = 2 * O.half / N, dV = h * h * h;
    const auto at = [&](int i) { return -O.half + (i + 0.5) * h; };
    O.c = level;
    O.mass = 0;
    double ax[3] = {0, 0, 0};                                  // how far the region reaches along x, y, z
    for (int k = 0; k < N; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++)
                if (f[(std::size_t(k) * N + j) * N + i] >= O.c) {
                    O.mass += f[(std::size_t(k) * N + j) * N + i] * dV / O.norm;
                    ax[0] = std::max(ax[0], std::fabs(at(i))); ax[1] = std::max(ax[1], std::fabs(at(j))); ax[2] = std::max(ax[2], std::fabs(at(k)));
                    O.rmax = std::max(O.rmax, std::sqrt(at(i) * at(i) + at(j) * at(j) + at(k) * at(k)));
                }
    // the side view: the outline of the region projected along y
    {
        std::vector<float> proj(std::size_t(N) * N, 0.f);       // [k (z)][i (x)]
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                for (int i = 0; i < N; i++) proj[std::size_t(k) * N + i] = std::max(proj[std::size_t(k) * N + i], f[(std::size_t(k) * N + j) * N + i]);
        marching(proj.data(), N, N, O.c, at(0), at(0), h, [&](double x0, double z0, double x1, double z1) { O.side.push_back({float(x0), float(z0), float(x1), float(z1)}); });
    }
    // the wireframe: contour lines in 5 planes across the region for each of the three axes
    {
        std::vector<float> g(std::size_t(N) * N);
        for (int axis = 0; axis < 3; axis++)
            for (int p = -2; p <= 2; p++) {
                const double pos = ax[axis] * p / 3.0;
                const int s = std::max(0, std::min(N - 1, int(std::floor((pos + O.half) / h))));
                for (int b = 0; b < N; b++)
                    for (int a = 0; a < N; a++)
                        g[std::size_t(b) * N + a] = axis == 0 ? f[(std::size_t(b) * N + a) * N + s]        // plane x: axes y (a), z (b)
                                                  : axis == 1 ? f[(std::size_t(b) * N + s) * N + a]        // plane y: axes x (a), z (b)
                                                              : f[(std::size_t(s) * N + b) * N + a];       // plane z: axes x (a), y (b)
                const double plane = at(s);
                marching(g.data(), N, N, O.c, at(0), at(0), h, [&](double u0, double v0, double u1, double v1) {
                    if (axis == 0) O.wire.push_back({float(plane), float(u0), float(v0), float(plane), float(u1), float(v1)});
                    else if (axis == 1) O.wire.push_back({float(u0), float(plane), float(v0), float(u1), float(plane), float(v1)});
                    else O.wire.push_back({float(u0), float(v0), float(plane), float(u1), float(v1), float(plane)});
                });
            }
    }
    return O;
}

const Outline& outlineOf(int orb) {
    auto& m = cache();
    auto it = m.find(orb);
    if (it != m.end()) return *it->second;
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const double half = 1.15 * kExt[o.n], h = 2 * half / kOutlineGrid, dV = h * h * h;
    const std::vector<float> f = outlineGridCpu(orb);
    const float c = outlineLevelCpu(f, dV, orbitalNorm(orb));
    return *(m[orb] = std::make_unique<Outline>(outlineFromGrid(orb, f, c)));
}

// ---- the GPU path, without waiting ----
namespace {
render::IOutlineCompute* g_gpu = nullptr;
struct Pending { render::IOutlineCompute* gc; int job; };
std::map<int, Pending>& pendingJobs() { static std::map<int, Pending> m; return m; }
}  // namespace

void setOutlineCompute(render::IOutlineCompute* gc) {
    if (gc != g_gpu) pendingJobs().clear();                      // the old object may be gone: forget its jobs (it frees its own buffers when it goes)
    g_gpu = gc;
}
render::IOutlineCompute* outlineCompute() { return g_gpu; }

const Outline* outlineReady(int orb, bool useGpu) {
    auto& m = cache();
    const auto it = m.find(orb);
    if (it != m.end()) return it->second.get();
    if (useGpu && g_gpu && g_gpu->ok()) {
        auto& pend = pendingJobs();
        auto pi = pend.find(orb);
        if (pi == pend.end()) {                                  // first request: start the job (the whole 64^3 grid and its 90 % level on the GPU) and carry on
            const Orbital& o = orbitals()[stateOrbital(orb)];
            const double half = 1.15 * kExt[o.n], h = 2 * half / kOutlineGrid, dV = h * h * h;
            const int job = g_gpu->beginGrid(outlineDensityOf(orb), kOutlineGrid, float(half), float(dV / orbitalNorm(orb)), float(kOutlineProb));
            if (job > 0) { pend[orb] = {g_gpu, job}; return nullptr; }
        } else {                                                 // asked before: has the GPU finished? (never waits)
            std::vector<float> grid; float level = 0;
            const render::JobState st = g_gpu->pollGrid(pi->second.job, grid, level);
            if (st == render::JobState::Pending) return nullptr;
            pend.erase(pi);
            if (st == render::JobState::Done) return (m[orb] = std::make_unique<Outline>(outlineFromGrid(orb, grid, level))).get();
            // Failed: the CPU makes it, below
        }
    }
    return &outlineOf(orb);
}

void clearOutlineCache() {
    for (auto& p : pendingJobs()) if (p.second.gc == g_gpu && g_gpu) g_gpu->cancel(p.second.job);   // a job in flight is dropped
    pendingJobs().clear();
    cache().clear();
}
std::size_t outlineCacheSize() { return cache().size(); }

std::vector<OutSeg2> outlineSliceFromValues(const Outline& O, const std::vector<float>& values, int n) {
    std::vector<OutSeg2> out;
    const double h = 2 * O.half / n;
    marching(values.data(), n, n, O.c, -O.half + 0.5 * h, -O.half + 0.5 * h, h, [&](double x0, double y0, double x1, double y1) { out.push_back({float(x0), float(y0), float(x1), float(y1)}); });
    return out;
}

void SliceContour::update(const Outline& O, int orb, bool hz, double off0, bool gpu) {
    render::IOutlineCompute* oc = gpu ? g_gpu : nullptr;
    if (job_ && !(oc && oc->ok())) { if (g_gpu) g_gpu->cancel(job_); job_ = 0; }          // the switch was turned off (or the object is gone): forget the job
    const auto stale = [&] { return orb_ != orb || hz_ != hz || off_ != off0; };
    bool cpuNow = !(oc && oc->ok());
    if (!cpuNow && job_) {
        std::vector<float> values;
        const render::JobState st = oc->pollPlane(job_, values);
        if (st != render::JobState::Pending) {
            if (st == render::JobState::Done && jobOrb_ == orb) { segs_ = outlineSliceFromValues(O, values); orb_ = jobOrb_; hz_ = jobHz_; off_ = jobOff_; }
            job_ = 0;
            if (st == render::JobState::Failed) cpuNow = true;
        }
    }
    if (!cpuNow && !job_ && stale()) {
        job_ = oc->beginPlane(outlineDensityOf(orb), 200, float(O.half), hz, float(off0));
        if (job_) { jobOrb_ = orb; jobHz_ = hz; jobOff_ = off0; } else cpuNow = true;
    }
    if (cpuNow && stale()) { segs_ = outlineSlice(orb, hz, off0); orb_ = orb; hz_ = hz; off_ = off0; }
}

std::vector<OutSeg2> outlineSlice(int orb, bool horizontal, double offset, int n) {
    const Outline& O = outlineOf(orb);
    return outlineSliceFromValues(O, outlinePlaneCpu(orb, O.half, horizontal, offset, n), n);
}

}  // namespace atom
