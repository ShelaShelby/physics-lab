// 0.57.1: the Molecule Lab's Density cloud dots, the processor's side: each dot's place, size and depth weight (the formulas drawRealistic used, in double,
// unchanged: the reference the card's kMolDotsCS is checked against), the float limits of that check, the dots near the nuclei (the exposure's cap there,
// atom::glowCapExposure) and the start-up check's cases. No graphics here.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "atom/cloud_dots.h"

namespace molecule {

struct CloudCam {                                  // the picture's camera for the dots (angstrom in, points out)
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};    // the view's rotation (rows)
    double px = 0, py = 0;                         // pan (angstrom)
    double cx = 0, cy = 0, s = 1;                  // the molecule's centre on the picture (points) and points per angstrom
    double ext = 1;                                // the depth scale: the molecule's radius (angstrom, at least 0.5)
    double w = 1, h = 1;                           // the picture (points): dots outside are not drawn
};

// one dot as the processor draws it: false = off the picture (not drawn). vz: its depth (angstrom, bigger = nearer), for the limits.
inline bool cloudDot(const CloudCam& c, const float* p, atom::GlowDot& g, double* vzOut = nullptr) {
    const double x = p[0], y = p[1], z = p[2];
    const double vx = c.R[0] * x + c.R[1] * y + c.R[2] * z + c.px, vy = c.R[3] * x + c.R[4] * y + c.R[5] * z + c.py, vz = c.R[6] * x + c.R[7] * y + c.R[8] * z;
    if (vzOut) *vzOut = vz;
    const double X = c.cx + vx * c.s, Y = c.cy - vy * c.s;
    if (X < 0 || X > c.w || Y < 0 || Y > c.h) { g = {static_cast<float>(X), static_cast<float>(Y), 0, 0, 0}; return false; }
    const float t = static_cast<float>(std::clamp(-vz / c.ext, -1.0, 1.0));   // -1 nearest, +1 farthest (as the Atom Lab's depth over the cloud's radius)
    const float near = 0.5f - 0.5f * t;
    g = {static_cast<float>(X), static_cast<float>(Y), 4.6f * (0.8f + 0.4f * near), 0.3f + 0.9f * near, 0};
    return true;
}

// The limits of the card's single-precision dot against the processor's (derived from the operations, before any card's result was seen).
// u = 2^-24, the unit roundoff of a float; GLSL 4.30 (4.7.1) rounds +, -, * correctly and a division within 2.5 ULP (<= 5 u relative); a fused a b + c
// rounds once instead of twice, which only lowers the bounds. The card gets R, pan, cx, cy, s, ext rounded to float (u relative each); the dot's place is
// a float on both sides. |R_ij| <= 1 (a rotation).
//   vx = R0 x + R1 y + R2 z + px: R rounded (u per term), px rounded (u), 3 products and 3 sums (at most 3 u of the running sum each, first order):
//        |dvx| <= 4 u S, S = |x| + |y| + |z| + |px|.
//   X = cx + vx s: |dvx| s, s rounded and the product (2 u |vx s| <= 2 u S s), cx rounded (u |cx|), the sum (u |X|); the processor rounds its double X to
//        float once more (u |X|): |dX| <= u (6 S s + |cx| + 2 |X|). Y the same with py.
//   t = clamp(-vz / ext): vz as vx without pan (4 u S3, S3 = |x| + |y| + |z|), ext rounded (u |t|), the division (5 u |t|), the processor's float of its
//        double t (u |t|); clamp does not widen it: |dt| <= u (4 S3 / ext + 7).
//   near = 0.5 - 0.5 t: 0.5 |dt| and each side's own rounding of the difference (u each, near <= 1): |dnear| <= 0.5 |dt| + 2 u.
//   r = 4.6 (0.8 + 0.4 near): 1.84 |dnear| and each side's three roundings (0.4 near <= 0.4, the sum <= 1.2, the product <= 5.52: 7.12 u a side).
//   wt = 0.3 + 0.9 near: 0.9 |dnear| and each side's two roundings (<= 0.9 and <= 1.2: 2.1 u a side).
// A dot drawn by one side and not the other is explained when its X or Y lies within that limit of the picture's edge.
struct CloudLimit { double X = 0, Y = 0, r = 0, wt = 0; };
inline CloudLimit cloudLimit(const CloudCam& c, const float* p, double X, double Y) {
    constexpr double u = 1.0 / 16777216.0;
    const double ax = std::fabs(double(p[0])), ay = std::fabs(double(p[1])), az = std::fabs(double(p[2]));
    const double S3 = ax + ay + az, Sx = S3 + std::fabs(c.px), Sy = S3 + std::fabs(c.py);
    CloudLimit L;
    L.X = u * (6 * Sx * c.s + std::fabs(c.cx) + 2 * std::fabs(X));
    L.Y = u * (6 * Sy * c.s + std::fabs(c.cy) + 2 * std::fabs(Y));
    const double dt = u * (4 * S3 / c.ext + 7), dn = 0.5 * dt + 2 * u;
    L.r = 1.84 * dn + 2 * 7.12 * u;
    L.wt = 0.9 * dn + 2 * 2.1 * u;
    return L;
}

// The card's dots (4 floats each: X, Y, r, wt; r = 0 not drawn) against the processor's for the same camera
struct CloudCompare {
    long dots = 0, bothDrawn = 0, flagDiff = 0, flagExplained = 0, pointBad = 0;
    double worstRatio = 0;                          // the largest difference over its limit (X, Y, r, wt), over the dots both drew
    bool ok() const { return dots > 0 && pointBad == 0 && flagDiff == flagExplained; }
};
inline CloudCompare compareCloud(const CloudCam& c, const float* pts, long n, const std::vector<float>& card) {
    CloudCompare k;
    if (long(card.size()) < 4 * n) { k.dots = n; k.pointBad = n; return k; }
    for (long i = 0; i < n; i++) {
        atom::GlowDot g{};
        const bool cpu = cloudDot(c, pts + 3 * i, g);
        const float* q = &card[std::size_t(4 * i)];
        const bool gpu = q[2] > 0;
        const double X = c.cx + (c.R[0] * pts[3 * i] + c.R[1] * pts[3 * i + 1] + c.R[2] * pts[3 * i + 2] + c.px) * c.s;
        const double Y = c.cy - (c.R[3] * pts[3 * i] + c.R[4] * pts[3 * i + 1] + c.R[5] * pts[3 * i + 2] + c.py) * c.s;
        const CloudLimit L = cloudLimit(c, pts + 3 * i, X, Y);
        k.dots++;
        if (!std::isfinite(q[0]) || !std::isfinite(q[1])) { k.pointBad++; continue; }
        if (cpu != gpu) {
            k.flagDiff++;
            if (std::fabs(X) <= L.X || std::fabs(X - c.w) <= L.X || std::fabs(Y) <= L.Y || std::fabs(Y - c.h) <= L.Y) k.flagExplained++;
            continue;
        }
        const double rx = std::fabs(double(q[0]) - g.x) / L.X, ry = std::fabs(double(q[1]) - g.y) / L.Y;   // (the place: also for dots neither draws)
        double worst = std::max(rx, ry);
        if (cpu) { k.bothDrawn++; worst = std::max({worst, std::fabs(double(q[2]) - g.r) / L.r, std::fabs(double(q[3]) - g.wt) / L.wt}); }
        if (!(worst <= 1)) k.pointBad++;
        k.worstRatio = std::max(k.worstRatio, worst);
    }
    return k;
}
inline std::string cloudCompareText(const CloudCompare& k) {
    char b[240];
    std::snprintf(b, sizeof b, "%ld dots: %ld drawn by both, %ld drawn by one side only (%ld within the rounding at the picture's edge); %ld beyond their limit, the largest difference %.2g of its limit",
                  k.dots, k.bothDrawn, k.flagDiff, k.flagExplained, k.pointBad, k.worstRatio);
    return b;
}

// For each point of `at` (the nuclei on the picture), the dots whose disc can reach it, in dot order, as the processor's path makes them (cloudDot): what
// atom::glowCapExposure sums there (a dot farther than its radius adds nothing, so each list holds every dot that counts at its point, in the same order:
// cloudCapExposure below gives glowCapExposure's exposure bit for bit). A quick pass in single precision first leaves out only dots more than a point beyond
// the largest disc (4.6 x 1.2 points) from the point (its rounding is far below a point; the points are found through a coarse grid of cells); the dots it
// keeps are then made exactly. Returns the dots on the picture (counted in the quick pass).
inline long cloudNear(const float* pts, long n, const CloudCam& c, const std::vector<std::array<double, 2>>& at, std::vector<std::vector<atom::GlowDot>>& out) {
    out.resize(at.size());
    for (auto& v : out) v.clear();
    const float m = 4.6f * 1.2f + 1.f, cell = 2 * m;
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
    for (const auto& a : at) { x0 = std::min(x0, a[0]); x1 = std::max(x1, a[0]); y0 = std::min(y0, a[1]); y1 = std::max(y1, a[1]); }
    const float bx0 = float(x0) - m, bx1 = float(x1) + m, by0 = float(y0) - m, by1 = float(y1) + m;
    const int gw = at.empty() ? 0 : int((bx1 - bx0) / cell) + 1, gh = at.empty() ? 0 : int((by1 - by0) / cell) + 1;
    static std::vector<std::vector<int>> cells;                // each cell's points whose reach (a square of half side m) overlaps it
    cells.assign(std::size_t(std::max(0, gw * gh)), {});
    std::vector<float> ax(at.size()), ay(at.size());
    for (std::size_t k = 0; k < at.size(); k++) {
        ax[k] = float(at[k][0]); ay[k] = float(at[k][1]);
        const int i0 = std::max(0, int((ax[k] - m - bx0) / cell)), i1 = std::min(gw - 1, int((ax[k] + m - bx0) / cell));
        const int j0 = std::max(0, int((ay[k] - m - by0) / cell)), j1 = std::min(gh - 1, int((ay[k] + m - by0) / cell));
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) cells[std::size_t(j * gw + i)].push_back(int(k));
    }
    float Rf[9];
    for (int k = 0; k < 9; k++) Rf[k] = float(c.R[k]);
    const float pxf = float(c.px), pyf = float(c.py), cxf = float(c.cx), cyf = float(c.cy), sf = float(c.s), wf = float(c.w), hf = float(c.h);
    long onPicture = 0;
    constexpr long kBlock = 1024;                                // (the quick pass in blocks, so the compiler can run it several dots at a time)
    float bX[kBlock], bY[kBlock];
    int inBox[kBlock], cand[kBlock];
    for (long b0 = 0; b0 < n; b0 += kBlock) {
        const long nb = std::min(kBlock, n - b0);
        const float* q = pts + 3 * b0;
        long on = 0;
        for (long j = 0; j < nb; j++) {
            const float x = q[3 * j], y = q[3 * j + 1], z = q[3 * j + 2];
            const float X = cxf + (Rf[0] * x + Rf[1] * y + Rf[2] * z + pxf) * sf, Y = cyf - (Rf[3] * x + Rf[4] * y + Rf[5] * z + pyf) * sf;
            bX[j] = X; bY[j] = Y;
            on += (X >= 0) & (X <= wf) & (Y >= 0) & (Y <= hf);
            inBox[j] = (X >= bx0) & (X <= bx1) & (Y >= by0) & (Y <= by1);
        }
        onPicture += on;
        if (at.empty()) continue;
        int nc = 0;
        for (long j = 0; j < nb; j++) { cand[nc] = int(j); nc += inBox[j]; }   // (without a branch per dot)
        for (int t = 0; t < nc; t++) {
            const long j = cand[t];
            const float X = bX[j], Y = bY[j];
            const int ci = std::min(gw - 1, int((X - bx0) / cell)), cj = std::min(gh - 1, int((Y - by0) / cell));
            bool made = false, drawn = false;
            atom::GlowDot g{};
            for (const int k : cells[std::size_t(cj * gw + ci)]) {
                const float dx = X - ax[std::size_t(k)], dy = Y - ay[std::size_t(k)];
                if (!(dx * dx + dy * dy < m * m)) continue;
                if (!made) { drawn = cloudDot(c, q + 3 * j, g); made = true; }
                if (drawn) out[std::size_t(k)].push_back(g);
            }
        }
    }
    return onPicture;
}
// atom::glowCapExposure (base 1) with each point's own list from cloudNear: the same sums, so the same exposure
inline void cloudCapExposure(const std::vector<std::vector<atom::GlowDot>>& near, long cap, const std::vector<std::array<double, 2>>& at, double& expo) {
    for (int it = 0; it < 4 && expo > 0; it++) {
        double top = 0;
        for (std::size_t k = 0; k < at.size() && k < near.size(); k++) top = std::max(top, atom::glowSumAt(near[k], cap, expo, 1.0, at[k][0], at[k][1]));
        if (top <= 1) return;
        expo /= top;
    }
}

// The start-up check's cameras (deterministic; built from angles so the rotation is orthonormal to double rounding): small and large pictures, near and
// far zooms, pans that put the picture's edges through the cloud
inline std::vector<CloudCam> cloudCheckCams() {
    std::vector<CloudCam> v;
    const double kA[6][3] = {{0, 0, 0}, {0.7, -0.4, 0.2}, {2.1, 0.9, -1.3}, {-1.1, 1.4, 2.8}, {0.33, -2.6, 0.05}, {3.0, 0.2, 1.7}};
    const double kS[6] = {60, 145.5, 410, 33.3, 900, 222};
    const double kW[6][2] = {{880, 560}, {1280, 800}, {2560, 1369}, {880, 560}, {2560, 1369}, {1280, 800}};
    const double kPan[6][2] = {{0, 0}, {0.8, -0.3}, {-2.4, 1.1}, {0.05, 0.02}, {1.6, 0.9}, {-0.7, -1.9}};
    const double kExt[6] = {1.0, 1.4, 2.6, 0.5, 3.1, 1.9};
    for (int k = 0; k < 6; k++) {
        CloudCam c;
        const double a = kA[k][0], b = kA[k][1], g = kA[k][2];
        const double ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b), cg = std::cos(g), sg = std::sin(g);
        const double Rz[9] = {ca, -sa, 0, sa, ca, 0, 0, 0, 1}, Rx[9] = {1, 0, 0, 0, cb, -sb, 0, sb, cb}, Ry[9] = {cg, 0, sg, 0, 1, 0, -sg, 0, cg};
        double T[9], R[9];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[3 * i + j] = 0; for (int q = 0; q < 3; q++) T[3 * i + j] += Rx[3 * i + q] * Rz[3 * q + j]; }
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { R[3 * i + j] = 0; for (int q = 0; q < 3; q++) R[3 * i + j] += Ry[3 * i + q] * T[3 * q + j]; }
        for (int q = 0; q < 9; q++) c.R[q] = R[q];
        c.w = kW[k][0]; c.h = kW[k][1]; c.s = kS[k]; c.px = kPan[k][0]; c.py = kPan[k][1]; c.ext = kExt[k];
        c.cx = c.w * (0.5 + 0.03 * k); c.cy = c.h * (0.5 - 0.02 * k);
        v.push_back(c);
    }
    return v;
}
// dots for the check besides the lab's own: a box of +-9 angstrom (beyond every molecule's surface), splitmix64 from a fixed seed
inline void cloudCheckDots(long n, std::vector<float>& out) {
    out.resize(std::size_t(3 * n));
    std::uint64_t s = 20261117;
    for (long i = 0; i < 3 * n; i++) {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; z ^= z >> 31;
        out[std::size_t(i)] = float((double(z >> 11) / 9007199254740992.0 - 0.5) * 18.0);
    }
}

}  // namespace molecule
