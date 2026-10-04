#include "outline.h"

#include <algorithm>
#include <cmath>
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

double angularMeanSq(int orb) {        // the average of |A|^2 over the sphere, A = the angular part of psi on the unit sphere: midpoint rule in cos(theta) (400) and phi (64)
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const int nu = 400, nphi = 64;
    double s = 0;
    for (int i = 0; i < nu; i++) {
        const double z = -1 + 2 * (i + 0.5) / nu, r = std::sqrt(std::max(0.0, 1 - z * z));
        for (int k = 0; k < nphi; k++) {
            const double a = 2 * kPi * (k + 0.5) / nphi;
            if (isComplexState(orb)) { s += std::norm(angularComplex(o.l, stateM(orb), r * std::cos(a), r * std::sin(a), z)); continue; }
            const double A = angular(o.ang, r * std::cos(a), r * std::sin(a), z);
            s += A * A;
        }
    }
    return s / (double(nu) * nphi);
}

std::map<int, std::unique_ptr<Outline>>& cache() { static std::map<int, std::unique_ptr<Outline>> m; return m; }

}  // namespace

double orbitalNorm(int orb) {
    const Orbital& o = orbitals()[stateOrbital(orb)];
    // |psi|^2 = radial(r)^2 × angular(x, y, z)^2, and angular is a polynomial of degree l in x, y, z: angular = r^l × A(unit vector)
    const double top = 2.5 * kExt[o.n] + 20, dr = 0.01;
    double s = 0;
    for (double r = 0.5 * dr; r < top; r += dr) { const double R = radial(o, r); s += R * R * std::pow(r, 2 * o.l + 2) * dr; }
    return s * 4 * kPi * angularMeanSq(orb);
}

void clearOutlineCache() { cache().clear(); }
std::size_t outlineCacheSize() { return cache().size(); }

const Outline& outlineOf(int orb) {
    auto& m = cache();
    auto it = m.find(orb);
    if (it != m.end()) return *it->second;
    auto out = std::make_unique<Outline>();
    Outline& O = *out;
    const Orbital& o = orbitals()[stateOrbital(orb)];
    const int N = kOutlineGrid;
    O.orb = orb;
    O.norm = orbitalNorm(orb);
    O.half = 1.15 * kExt[o.n];
    const double h = 2 * O.half / N, dV = h * h * h;
    const auto at = [&](int i) { return -O.half + (i + 0.5) * h; };
    std::vector<float> f(std::size_t(N) * N * N);
    for (int k = 0; k < N; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++) f[(std::size_t(k) * N + j) * N + i] = float(density(orb, 1, at(i), at(j), at(k)));
    // the level: add the densest voxels first until they hold 90 % of the probability
    std::vector<float> sorted(f);
    std::sort(sorted.begin(), sorted.end(), std::greater<float>());
    double acc = 0;
    O.c = sorted.back();
    for (float v : sorted) { acc += double(v) * dV / O.norm; if (acc >= kOutlineProb) { O.c = v; break; } }
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
    return *(m[orb] = std::move(out));
}

std::vector<OutSeg2> outlineSlice(int orb, bool horizontal, double offset, int n) {
    const Outline& O = outlineOf(orb);
    std::vector<OutSeg2> out;
    const double h = 2 * O.half / n;
    std::vector<float> g(std::size_t(n) * n);
    for (int b = 0; b < n; b++)
        for (int a = 0; a < n; a++) {
            const double u = -O.half + (a + 0.5) * h, v = -O.half + (b + 0.5) * h;
            g[std::size_t(b) * n + a] = float(horizontal ? density(orb, 1, u, v, offset) : density(orb, 1, u, offset, v));   // horizontal: plane z = offset (x, y); vertical: plane y = offset (x, z)
        }
    marching(g.data(), n, n, O.c, -O.half + 0.5 * h, -O.half + 0.5 * h, h, [&](double x0, double y0, double x1, double y1) { out.push_back({float(x0), float(y0), float(x1), float(y1)}); });
    return out;
}

}  // namespace atom
