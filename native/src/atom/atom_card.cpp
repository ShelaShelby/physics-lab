#include "atom/atom_card.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#include "atom/atom_core.h"
#include "atom/dirac_cloud.h"
#include "atom/stark.h"
#include "orbit/belts.h"

namespace atom {

render::SliceDotsJob sliceJobOf(const PlaneSampler& ps, std::uint32_t seed, int tries, int stride, int first) {
    render::SliceDotsJob j;
    j.seed = seed; j.tries = tries; j.axis = ps.axis; j.stride = stride; j.first = first;
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
SliceCompare compareSlice(const PlaneSampler& ps, std::uint32_t seed, const std::vector<float>& res, int tries, int stride, int first) {
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
        orbit::Mulberry32 r(mulberryAfter(seed, std::uint64_t(stride) * std::uint64_t(t) + std::uint64_t(first)));
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
            {1, orbIndex("6s"), 1, 25.0, 0x00C0FFEEu, 4096},
            // 0.64.0: the benchmark's superposition (atom.slice_super_drag: mendelevium, 2p z + 5f z3) on the plane of its cut at -0.5 (the view's size is the
            // larger state's: kExt[5] / Z), each state with a superposition's numbering (the number 4t chooses the state, 4t + 1 .. 4t + 3 are the attempt's)
            {101, orbIndex("2p|z"), 1, -0.5 * 0.5 * kExt[5] / 101.0, 0x13579BDFu, 4096, 4, 1},
            {101, orbIndex("5f|z3"), 1, -0.5 * 0.5 * kExt[5] / 101.0, 0x13579BDFu, 4096, 4, 1}};
}

// ---- 0.91.0: the Slice of a Dirac state on the card ----
render::DiracSliceJob diracJobOf(const DiracCase& c) {
    render::DiracSliceJob j;
    const int kappa = diracKappa(c.l, c.j2);
    const DiracPlaneTable t = diracPlaneTable(c.Z, c.n, kappa);
    const double a = std::fabs(c.off), c0 = diracPlaneCdf(c.Z, c.n, kappa, a);
    if (!(c0 < 1)) return j;                                               // (tries 0: nothing on this plane)
    j.seed = c.seed; j.tries = c.tries; j.axis = c.axis; j.off = float(c.off); j.a = float(a); j.tail0 = float(1 - c0);
    j.L = (c.j2 - 1) / 2; j.keep = !(c.round || j.L == 0) ? 1 : 0; j.stride = j.keep ? 3 : 2;
    j.n = int(t.r->size());
    j.tab.resize(std::size_t(4 * j.n));
    for (int i = 0; i < j.n; i++) {
        const std::size_t k = std::size_t(i);
        j.tab[4 * k] = float((*t.r)[k]); j.tab[4 * k + 1] = float(1 - (*t.pc)[k]); j.tab[4 * k + 2] = float(-(*t.pd)[k]); j.tab[4 * k + 3] = 0.f;
    }
    return j;
}

// THE LIMITS of a Dirac attempt (written before any card's result was looked at; eps = 2^-24, the processor's own unit 2^-53):
//  1. the target tail T = (1 - c0)(1 - u): 1 - c0 and 1 - u rounded once each, the product once: 3 eps T. The processor's own U = c0 + u (1 - c0) and its
//     Hermite values err by about 8 2^-53 (2 + |d0 h| + |d1 h|) in absolute terms (its distribution function is near 1 there): dT = 3 eps T + that.
//  2. the cubic in the tail at the card: each of the four stored values rounded once (eps), the weights from s = (m - x0) / h (the nodes x0, h rounded:
//     a shift of the whole cubic by about 4 eps r), about 10 operations: dH = 14 eps S, S = |T0| + |T1| + |D0 h| + |D1 h| of the segment.
//  3. the root: the cubic falls with slope |H'(r)| (its derivative at the processor's root, from the processor's table): dr = 2 (dT + dH) / |H'| + 6 eps r
//     + 2^-22 r (the bisection stops within 2 ulp; the factor 2 for the slope's change, as compareSlice's).
//  4. rho^2 = rk^2 - a^2 (a = |off| rounded once): dD = 2 rk dr + dr^2 + 4 eps (rk^2 + a^2); rho = sqrt: drho = min(sqrt dD, dD / rho) + 10 eps rho.
//  5. cos and sin of the turn as compareSlice step 5 (dcs); a coordinate rho cos: dq = drho + rho dcs + 2 eps rho; the plane's coordinate: eps |off|.
//  6. kept or not: the card compares K = k3 2^-32 (rounded once) with w = (1 - cz^2)^L, cz = z / rk: dcz = (dq + |cz| dr) / rk + 3 eps; dw = 2 L |cz| dcz
//     (1 - cz^2)^(L - 1) + (2 L + 2) eps. A differing answer is explained when |K - w| <= dw + 2 eps.
SliceCompare compareDirac(const DiracCase& c, const std::vector<float>& res) {
    constexpr double pi = 3.14159265358979323846;
    SliceCompare out;
    const render::DiracSliceJob j = diracJobOf(c);
    if (j.tries <= 0 || int(res.size()) < 4 * j.tries) return out;
    const int kappa = diracKappa(c.l, c.j2);
    const DiracPlaneTable t = diracPlaneTable(c.Z, c.n, kappa);
    const std::vector<double>& gx = *t.r; const std::vector<double>& gc = *t.pc; const std::vector<double>& gd = *t.pd;
    const double a = std::fabs(c.off), tail0 = 1 - diracPlaneCdf(c.Z, c.n, kappa, a);
    const double dcs = 4 * pi * kEps + 2.6e-8 + 6 * kEps, u53 = std::ldexp(1.0, -53);
    for (int k = 0; k < j.tries; k++) {
        orbit::Mulberry32 r(mulberryAfter(c.seed, std::uint64_t(j.stride) * std::uint64_t(k)));
        orbit::Mulberry32 r2 = r;
        const double u = r2.next(); r2.next(); const double K = j.keep ? r2.next() : 0.0;
        double x = 0, y = 0, z = 0;
        const bool kept = tryDiracOnPlane(c.Z, c.n, c.l, c.j2, c.round, c.axis, c.off, r, x, y, z);
        const double rk = std::sqrt(x * x + y * y + z * z), rho = std::sqrt(std::max(0.0, rk * rk - a * a));
        const float* g = &res[std::size_t(4 * k)];
        const int gw = g[3] > 0.5f ? 1 : g[3] < -0.5f ? -1 : 0, cw = kept ? 1 : 0;
        out.tries++;
        out.kept += cw;
        // 1-3: the radius
        const double T = tail0 * (1 - u);
        std::size_t hi = std::size_t(std::upper_bound(gc.begin(), gc.end(), 1 - T) - gc.begin());
        hi = std::max<std::size_t>(1, std::min(hi, gx.size() - 1));
        const std::size_t lo = hi - 1;
        const double h = gx[hi] - gx[lo], T0 = 1 - gc[lo], T1 = 1 - gc[hi], D0 = -gd[lo], D1 = -gd[hi], s = std::min(1.0, std::max(0.0, (rk - gx[lo]) / h));
        const double S = std::fabs(T0) + std::fabs(T1) + std::fabs(D0 * h) + std::fabs(D1 * h);
        const double slope = std::fabs(((6 * s * s - 6 * s) * T0 + (3 * s * s - 4 * s + 1) * D0 * h + (-6 * s * s + 6 * s) * T1 + (3 * s * s - 2 * s) * D1 * h) / h);
        const double dT = 3 * kEps * T + 8 * u53 * (2 + std::fabs(D0 * h) + std::fabs(D1 * h)), dH = 14 * kEps * S;
        const double dr = (slope > 0 ? 2 * (dT + dH) / slope : 1e30) + 6 * kEps * rk + std::ldexp(1.0, -22) * rk;
        // 4-5: the point
        const double dD = 2 * rk * dr + dr * dr + 4 * kEps * (rk * rk + a * a);
        const double drho = std::min(std::sqrt(dD), rho > 0 ? dD / rho : 1e30) + 10 * kEps * rho;
        const double lim = drho + rho * dcs + 2 * kEps * rho + kEps * a;
        const double e = std::max({std::fabs(double(g[0]) - x), std::fabs(double(g[1]) - y), std::fabs(double(g[2]) - z)});
        if (!(e <= lim)) out.pointBad++;
        out.worstRatio = std::max(out.worstRatio, lim > 0 ? e / lim : (e > 0 ? 1e30 : 0));
        if (gw == 1 && cw == 1) { out.bothKept++; out.worstAbs = std::max(out.worstAbs, e); }
        if (gw != cw) {
            out.flagDiff++;
            if (gw >= 0 && j.keep && rk > 0) {
                const double cz = z / rk, w0 = std::max(0.0, 1 - cz * cz), w = std::pow(w0, j.L);
                const double dcz = (lim + std::fabs(cz) * dr) / rk + 3 * kEps;
                const double dw = 2 * j.L * std::fabs(cz) * dcz * (j.L > 1 ? std::pow(w0, j.L - 1) : 1.0) + (2 * j.L + 2) * kEps;
                if (std::fabs(K - w) <= dw + 2 * kEps) out.flagExplained++;
            }
        }
    }
    return out;
}

std::vector<DiracCase> diracCheckCases() {
    // the benchmark's state (atom.slice_dirac_drag: uranium 4f, j = 7/2 lined up) on the cut at -0.9, 0 and +0.4 of the view (off = cut x kExt[4] / 2 / Z, as
    // sliceCheckCases), hydrogen 3d5/2 on a plane far out (its tail), uranium 1s round near the nucleus, and 2p1/2 (j = 1/2: no keep test)
    const double half4 = 0.5 * kExt[4] / 92.0;
    return {{92, 4, 3, 7, false, 1, -0.9 * half4, 0x9E3779B9u, 4096}, {92, 4, 3, 7, false, 1, 0.0, 0x01234567u, 4096},
            {92, 4, 3, 7, false, 2, 0.4 * half4, 0xDEADBEEFu, 4096}, {1, 3, 2, 5, false, 1, 30.0, 0x00C0FFEEu, 4096},
            {92, 1, 0, 1, true, 2, 0.004, 0x5A5A5A5Au, 4096}, {79, 2, 1, 1, false, 1, 0.02, 0x13579BDFu, 4096}};
}

// ---- 0.93.0: a parabolic state's cloud ----
namespace {
double hornerD(const std::vector<double>& q, double x) { double s = 0; for (std::size_t i = q.size(); i-- > 0;) s = s * x + q[i]; return s; }
// where e^-x Q(x) / Q(0) first falls below 2^-40 (doubling from 1): the card's bisection top, above every root it is asked for (1 - u >= 2^-32)
double tailTop(const std::vector<double>& q) {
    double x = 1;
    while (x < 1e4 && std::exp(-x) * hornerD(q, x) / q[0] > std::ldexp(1.0, -40)) x *= 2;
    return x;
}
const std::vector<double>* tailsOf(const stark::ParSampler& ps, int w) { return w == 0 ? &ps.qA1 : w == 1 ? &ps.qA2 : w == 2 ? &ps.qB1 : &ps.qB2; }
const std::vector<double>* densOf(const stark::ParSampler& ps, int w) { return w == 0 ? &ps.pA1 : w == 1 ? &ps.pA2 : w == 2 ? &ps.pB1 : &ps.pB2; }
}  // namespace

render::StarkJob starkJobOf(const StarkCase& c) {
    render::StarkJob j;
    const stark::Par s{c.n, c.n1, c.n2, c.m};
    if (!stark::valid(s)) return j;
    const stark::ParSampler ps = stark::parSampler(s);
    j.seed = c.seed; j.tries = c.tries; j.n = float(c.n);
    const double t = std::ceil(ps.share() * 4294967296.0);           // exact: share 2^32 is a double times a power of two
    j.shareK = t >= 4294967295.0 ? 0xFFFFFFFFu : std::uint32_t(t);
    for (int w = 0; w < 4; w++) {
        const std::vector<double>& q = *tailsOf(ps, w);
        if (q.empty() || q.size() > 16 || !(q[0] > 0)) { j.tries = 0; return j; }
        j.nq[w] = int(q.size());
        for (std::size_t i = 0; i < q.size(); i++) j.q[std::size_t(16 * w) + i] = float(q[i] / q[0]);
        j.xmax[w] = float(tailTop(q));
    }
    return j;
}

// THE LIMITS (eps = 2^-24; the card's steps in kStarkCS, first order in eps, each bound from the processor's own point):
//  1. the part: k0 < ceil(share 2^32) is the processor's u0 < share exactly (k0 an integer). A differing part is a failure.
//  2. a tail's logarithm g(x) = ln(Q^(x)) - x, Q^ = Q / Q(0) with d + 1 coefficients rounded once: relative error of Q^ (2d + 2) eps K (K = sum |q_i| x^i / Q(x),
//     Horner's condition number), then log (GLSL 4.30 section 4.7.1: 2^-21 absolute in [0.5, 2], 3 ulp outside; both taken: 2^-21 + 6 eps |ln Q^|), then the
//     subtraction eps (|ln Q^| + x). The target ln T: T = (2^32 - k) 2^-32 rounded once (eps), its log 2^-21 + 6 eps |ln T|. dg is their sum.
//  3. the root: g falls with slope p(x) / Q(x) (the density over its tail): dX = 2 dg Q / p + 4 eps X (the factor 2 for the slope's change over the
//     interval, the bisection stopping within 2 ulp). Where p = 0 (a node of the density) there is no limit (none of the check's points lands there).
//  4. xi = n X, eta = n Y: d xi = n dX + eps xi. z = (xi - eta) / 2: dz = (d xi + d eta) / 2 + eps |z|. D = xi eta: dD = xi d eta + eta d xi + d xi d eta
//     + 2 eps xi eta; rho = sqrt D: drho = min(sqrt dD, dD / rho) + 10 eps rho (as compareSlice).
//  5. cos and sin of the turn as compareSlice step 5 (dcs); x = rho cos: dq = drho + rho dcs + 2 eps rho.
SliceCompare compareStark(const StarkCase& c, const std::vector<float>& res) {
    constexpr double pi = 3.14159265358979323846;
    SliceCompare out;
    const render::StarkJob j = starkJobOf(c);
    if (j.tries <= 0 || int(res.size()) < 4 * j.tries) return out;
    const stark::ParSampler ps = stark::parSampler(stark::Par{c.n, c.n1, c.n2, c.m});
    const double dcs = 4 * pi * kEps + 2.6e-8 + 6 * kEps, n = c.n;
    const auto dRoot = [&](int w, double X, double u) {
        const std::vector<double>& q = *tailsOf(ps, w);
        const std::vector<double>& p = *densOf(ps, w);
        if (!(u > 0)) return 4 * kEps * X;                               // (1 - u = 1: x = 0 on both)
        double sa = 0, xp = 1;
        for (double qi : q) { sa += std::fabs(qi) * xp; xp *= X; }
        const double Q = hornerD(q, X), P = hornerD(p, X), lnQ = std::log(Q / q[0]), T = 1 - u;
        if (!(P > 0) || !(Q > 0)) return 1e30;
        const double d = double(q.size()) - 1;
        const double dg = (2 * d + 2) * kEps * sa / Q + std::ldexp(1.0, -21) + 6 * kEps * std::fabs(lnQ) + kEps * (std::fabs(lnQ) + X)
                        + kEps + std::ldexp(1.0, -21) + 6 * kEps * std::fabs(std::log(T));
        return 2 * dg * Q / P + 4 * kEps * X;
    };
    for (int k = 0; k < j.tries; k++) {
        orbit::Mulberry32 r(mulberryAfter(c.seed, 4ull * std::uint64_t(k)));
        orbit::Mulberry32 r2 = r;
        const double u0 = r2.next(), ux = r2.next(), uy = r2.next();
        double x = 0, y = 0, z = 0;
        stark::samplePar(ps, r, x, y, z);
        const bool one = u0 < ps.share();
        const int wx = one ? 0 : 2, wy = one ? 1 : 3;
        const double X = stark::invertTail(*tailsOf(ps, wx), 1 - ux), Y = stark::invertTail(*tailsOf(ps, wy), 1 - uy);
        const double xi = n * X, eta = n * Y, rho = std::sqrt(xi * eta);
        const double dxi = n * dRoot(wx, X, ux) + kEps * xi, deta = n * dRoot(wy, Y, uy) + kEps * eta;
        const double dz = 0.5 * (dxi + deta) + kEps * std::fabs(z);
        const double dD = xi * deta + eta * dxi + dxi * deta + 2 * kEps * xi * eta;
        const double drho = std::min(std::sqrt(dD), rho > 0 ? dD / rho : 1e30) + 10 * kEps * rho;
        const double dq = drho + rho * dcs + 2 * kEps * rho;
        const float* g = &res[std::size_t(4 * k)];
        out.tries++;
        out.kept++;
        const bool gOne = g[3] > 0.5f && g[3] < 1.5f;
        if (gOne != one || !(g[3] > 0.5f)) { out.flagDiff++; continue; }   // (never explained: the part is exact)
        out.bothKept++;
        const double ex = std::fabs(double(g[0]) - x), ey = std::fabs(double(g[1]) - y), ez = std::fabs(double(g[2]) - z);
        if (!(ex <= dq && ey <= dq && ez <= dz)) out.pointBad++;
        const double ratio = std::max({dq > 0 ? ex / dq : 0.0, dq > 0 ? ey / dq : 0.0, dz > 0 ? ez / dz : 0.0});
        out.worstRatio = std::max(out.worstRatio, ratio);
        out.worstAbs = std::max({out.worstAbs, ex, ey, ez});
    }
    return out;
}

std::vector<StarkCase> starkCheckCases() {
    return {{5, 4, 0, 0, 0x2545F491u, 4096}, {5, 1, 2, -1, 0x9E3779B9u, 4096}, {4, 0, 0, 3, 0x0BADC0DEu, 4096}};
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
