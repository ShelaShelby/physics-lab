#include "atom/atom_card.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#include "atom/atom_core.h"

namespace atom {

render::SliceDotsJob sliceJobOf(const PlaneSampler& ps, std::uint32_t seed, int tries) {
    render::SliceDotsJob j;
    j.seed = seed; j.tries = tries; j.axis = ps.axis;
    j.off = float(ps.off); j.x0 = float(ps.x0);
    j.lnG0 = float(ps.g0 > 0 ? std::log(ps.g0) : -1e30);
    j.scale = float(ps.n / (2.0 * ps.Z));
    j.bound = float(ps.bound);
    if (ps.complex) {
        int re = 0, im = 0; double c = 1;
        complexPairOf(ps.l, std::abs(ps.m), re, im, c);
        j.ang = re; j.angIm = im; j.cIm = float(c);
    } else {
        j.ang = ps.ang; j.angIm = -1; j.cIm = 0;
    }
    const std::vector<double>& h = ps.tailPoly();
    const std::vector<double>& c = ps.densPoly();
    j.nh = int(std::min<std::size_t>(16, h.size()));
    for (int i = 0; i < j.nh; i++) { j.h[i] = float(h[std::size_t(i)]); j.c[i] = i < int(c.size()) ? float(c[std::size_t(i)]) : 0.f; }
    return j;
}

namespace {
constexpr double kEps = 5.9604644775390625e-8;      // 2^-24: the unit roundoff of single precision (one correctly rounded operation)
double poly(const std::vector<double>& a, double x) { double v = 0; for (std::size_t i = a.size(); i-- > 0;) v = v * x + a[i]; return v; }
double polyAbs(const std::vector<double>& a, double x) { double v = 0; for (std::size_t i = a.size(); i-- > 0;) v = v * x + std::fabs(a[i]); return v; }
}  // namespace

// THE LIMITS (written before any card's result was looked at; eps = 2^-24):
//  1. the target L = ln u + ln G(x0): u = (~k1 + 1) 2^-32 is rounded twice (2 eps relative); GLSL 4.30 4.7.1 allows log an absolute error of 2^-21 inside
//     [0.5, 2] and 3 ulp (6 eps relative) outside; ln G(x0) is rounded once; the sum once:   dL = 2^-21 + 6 eps |ln u| + 2 eps + eps |ln G0| + eps |L|.
//  2. g(x) = ln H(x) - x - L at the processor's root x: Horner over nh coefficients (each rounded once) errs at most (2 nh + 1) eps sum|h_j| x^j, relative
//     to H that is (2 nh + 1) eps cond(x), cond = sum|h_j| x^j / H(x); then the log (2^-21 + 6 eps |ln H|) and the two subtractions (2 eps (|ln H| + x + |L|)):
//     dg = dL + (2 nh + 1) eps cond + 2^-21 + 6 eps |ln H| + 2 eps (|ln H| + x + |L|).
//  3. the root: g falls with slope f(x) / H(x) (G' = -f e^(-x)); Newton stops at a step of 2^-20 (1 + x): dx = 2 dg / (f / H) + 2 2^-20 (1 + x)
//     (the factor 2 for the slope's change over the last step); the processor's own root is good to 1e-13.
//  4. s = x n / 2Z (scale rounded, one product): ds = dx scale + 2 eps s; rho^2 = s^2 - off^2 (three roundings and off rounded):
//     dD = 2 s ds + ds^2 + 4 eps (s^2 + off^2); rho = sqrt (GLSL: from inversesqrt and division, 2 + 2.5 ulp): drho = min(sqrt dD, dD / rho) + 10 eps rho.
//  5. the turn t = k2 2^-32 (eps t), the angle 2 pi (t - q/4) (2 eps |a| + 2 pi eps), the Taylor series' remainder on [-pi/4, pi/4] (below 2.6e-8) and its
//     own rounding (6 eps): dcs = 2 pi eps + 2 pi eps + 2.6e-8 + 6 eps; a coordinate rho cos (one product): dq = drho + rho dcs + 2 eps rho; the plane's
//     own coordinate off is rounded once: eps |off|.
//  6. kept or not: the card compares r3 M with |P(u)|^2 at ITS point; P has at most 4 monomials of degree <= 3 with coefficients whose absolute sum is at
//     most 16 (atom_core.cpp angular), |u| <= 1, so its value errs at most 10 eps 16 = 160 eps (plus the unit vector's division, 3 eps a component, 3 x 16
//     eps); |P|^2: dA = 2 sqrt(M) 208 eps + 2 eps M (a complex state: two such parts and c <= 2: four times that); r3 M: 2 eps M. A differing answer is
//     explained when r3 M lies between the processor's |P|^2 and |P|^2 at the card's point (in double), widened by dA + 2 eps M on both sides.
SliceCompare compareSlice(const PlaneSampler& ps, std::uint32_t seed, const std::vector<float>& res, int tries) {
    constexpr double pi = 3.14159265358979323846;
    SliceCompare c;
    if (int(res.size()) < 4 * tries || !(ps.g0 > 0)) return c;
    const std::vector<double>& h = ps.tailPoly();
    const std::vector<double>& f = ps.densPoly();
    const double scale = ps.n / (2.0 * ps.Z), lnG0 = std::log(ps.g0), nh = double(h.size());
    const double dcs = 4 * pi * kEps + 2.6e-8 + 6 * kEps;
    const double dA1 = 2 * std::sqrt(ps.bound) * 208 * kEps + 2 * kEps * ps.bound, dA = ps.complex ? 4 * dA1 : dA1;
    const auto a2At = [&](double x, double y, double z) {
        const double s = std::sqrt(x * x + y * y + z * z);
        if (!(s > 0)) return 0.0;
        const double ux = x / s, uy = y / s, uz = z / s;
        if (ps.complex) return std::norm(angularComplex(ps.l, ps.m, ux, uy, uz));
        const double a = angular(ps.ang, ux, uy, uz);
        return a * a;
    };
    for (int t = 0; t < tries; t++) {
        orbit::Mulberry32 r(mulberryAfter(seed, 3ull * std::uint64_t(t)));
        const PlaneSampler::Detail d = ps.tryDetail(r);
        const float* g = &res[std::size_t(4 * t)];
        const int gw = g[3] > 0.5f ? 1 : g[3] < -0.5f ? -1 : 0, cw = d.radiusZero ? -1 : d.kept ? 1 : 0;
        c.tries++;
        c.kept += cw == 1;
        // the point's limit
        const double L = std::log(d.u) + lnG0, H = poly(h, d.x), lnH = std::log(H), cond = polyAbs(h, d.x) / H, hz = poly(f, d.x) / H;
        const double dL = std::ldexp(1.0, -21) + 6 * kEps * std::fabs(std::log(d.u)) + 2 * kEps + kEps * std::fabs(lnG0) + kEps * std::fabs(L);
        const double dg = dL + (2 * nh + 1) * kEps * cond + std::ldexp(1.0, -21) + 6 * kEps * std::fabs(lnH) + 2 * kEps * (std::fabs(lnH) + d.x + std::fabs(L));
        const double dx = (hz > 0 ? 2 * dg / hz : 1e30) + 2 * std::ldexp(1.0, -20) * (1 + d.x);
        const double ds = dx * scale + 2 * kEps * d.s;
        const double dD = 2 * d.s * ds + ds * ds + 4 * kEps * (d.s * d.s + ps.off * ps.off);
        const double drho = std::min(std::sqrt(dD), d.rho > 0 ? dD / d.rho : 1e30) + 10 * kEps * d.rho;
        const double lim = drho + d.rho * dcs + 2 * kEps * d.rho + kEps * std::fabs(ps.off);
        const double e = std::max({std::fabs(double(g[0]) - d.q.x), std::fabs(double(g[1]) - d.q.y), std::fabs(double(g[2]) - d.q.z)});
        if (!(e <= lim)) c.pointBad++;
        c.worstRatio = std::max(c.worstRatio, lim > 0 ? e / lim : (e > 0 ? 1e30 : 0));
        if (gw == 1 && cw == 1) { c.bothKept++; c.worstAbs = std::max(c.worstAbs, e); }
        if (gw != cw) {
            c.flagDiff++;
            if (gw >= 0 && cw >= 0) {
                const double mine = d.a2, theirs = a2At(g[0], g[1], g[2]), thr = d.r3 * ps.bound, w = dA + 2 * kEps * ps.bound;
                if (thr >= std::min(mine, theirs) - w && thr <= std::max(mine, theirs) + w) c.flagExplained++;
            } else if (d.s <= ds) c.flagExplained++;                 // a radius of 0 on one side only: the radius within its limit of 0
        }
    }
    return c;
}

std::string sliceCompareText(const SliceCompare& c) {
    char b[320];
    std::snprintf(b, sizeof b, "%ld attempts, %ld kept by the processor, %ld by both; %ld answered differently (%ld within the rounding there); "
                               "points %ld beyond their limit, the largest difference %.2g of its limit (%.2g Bohr radii in a kept dot)",
                  c.tries, c.kept, c.bothKept, c.flagDiff, c.flagExplained, c.pointBad, c.worstRatio, c.worstAbs);
    return b;
}

std::vector<SliceCheckCase> sliceCheckCases() {
    // the benchmark's own state and planes (uranium 4f z3, the cut at -0.9, 0 and +0.4 of the view: off = cut x kExt[4] / 2 / Z), a complex state on a
    // horizontal plane, an s state far out on the plane's tail
    const double half4 = 0.5 * kExt[4] / 92.0;
    return {{92, orbIndex("4f|z3"), 1, -0.9 * half4, 0x9E3779B9u, 4096},
            {92, orbIndex("4f|z3"), 1, 0.0, 0x01234567u, 4096},
            {92, orbIndex("4f|z3"), 1, 0.4 * half4, 0xDEADBEEFu, 4096},
            {26, complexState(orbIndex("3d|z2"), -1), 2, 0.3, 0x5A5A5A5Au, 4096},
            {1, orbIndex("6s"), 1, 25.0, 0x00C0FFEEu, 4096}};
}

std::vector<JumpCheckCase> jumpCheckCases() { return {{20000, 0.5}, {20000, 0.0137}, {7, 0.9}}; }

void jumpCheckFixed(long jn, std::vector<unsigned char>& fixedGroups) {
    fixedGroups.resize(std::size_t(2 * jn));
    for (long k = 0; k < 2 * jn; k++) fixedGroups[std::size_t(k)] = k % 37 == 5 ? 255 : (unsigned char)(k % 24);
}

void jumpCheckExpected(long jn, double shareNew, const std::vector<unsigned char>& fixedGroups, std::vector<unsigned char>& expected) {
    expected.resize(std::size_t(2 * jn));
    for (int pop = 0; pop < 2; pop++) {
        const double f = pop ? shareNew : 1 - shareNew;
        for (long i = 0; i < jn; i++) {
            const unsigned char g = fixedGroups[std::size_t(pop * jn + i)];
            expected[std::size_t(pop * jn + i)] = g != 255 && jumpUniformOf(pop, i) < f ? g : 255;
        }
    }
}

}  // namespace atom
