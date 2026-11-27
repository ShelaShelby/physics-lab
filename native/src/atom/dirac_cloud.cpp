#include "dirac_cloud.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <tuple>

#include "atom_core.h"
#include "dirac.h"

namespace atom {

int diracKappa(int l, int j2) { return j2 == 2 * l + 1 ? -(l + 1) : l; }
double diracGamma(int Z, int kappa) { const double x = Z * kAlpha; return std::sqrt(double(kappa) * kappa - x * x); }

namespace {
// Kummer's M(a, b, x) for a = 0, -1, -2, ...: a polynomial of degree -a
double kummer(int a, double b, double x) {
    double s = 1, t = 1;
    for (int k = 0; a + k < 0; k++) { t *= (a + k) / (b + k) * x / (k + 1); s += t; }
    return s;
}
// P and Q before normalisation (the closed form in the header, C = 1)
void rawPQ(int Z, int n, int kappa, double r, double& P, double& Q) {
    const int nr = n - std::abs(kappa);
    const double g = diracGamma(Z, kappa), x = Z * kAlpha;
    const double eps = 1 / std::sqrt(1 + x * x / ((nr + g) * (nr + g)));
    const double N = std::sqrt(double(nr) * nr + 2 * nr * g + double(kappa) * kappa);
    const double rho = 2 * Z * r / N;
    if (!(rho > 0)) { P = 0; Q = 0; return; }
    const double common = std::exp(-rho / 2 + g * std::log(rho));
    const double a = nr > 0 ? nr * kummer(1 - nr, 2 * g + 1, rho) : 0.0, b = (N - kappa) * kummer(-nr, 2 * g + 1, rho);
    P = std::sqrt(1 + eps) * common * (a - b);
    Q = std::sqrt(1 - eps) * common * (a + b);
}

// tables on r = e^t, t from ln 1e-7 to ln 250 (4,001 points), as hf.cpp: the density D = P^2 + Q^2 and its distribution function, the plane density D / r and its, <r>, the small share
struct Prep {
    double norm = 1, mean = 0, small = 0;
    std::vector<double> gx, gc, gd, pc, pd;
};
std::vector<double> cumulate(const std::vector<double>& t, const std::vector<double>& g) {   // trapezoid with the Euler-Maclaurin end correction per interval (hf.cpp)
    const std::size_t N = t.size();
    std::vector<double> c(N, 0.0), d(N, 0.0);
    const double h = t[1] - t[0];
    for (std::size_t i = 0; i < N; i++) d[i] = (i == 0 ? g[1] - g[0] : i == N - 1 ? g[N - 1] - g[N - 2] : 0.5 * (g[i + 1] - g[i - 1])) / h;
    for (std::size_t i = 1; i < N; i++) c[i] = c[i - 1] + 0.5 * h * (g[i - 1] + g[i]) - h * h / 12 * (d[i] - d[i - 1]);
    return c;
}
const Prep& prep(int Z, int n, int kappa) {
    static std::map<std::tuple<int, int, int>, std::unique_ptr<Prep>> cache;
    const auto key = std::make_tuple(Z, n, kappa);
    auto it = cache.find(key);
    if (it != cache.end()) return *it->second;
    auto p = std::make_unique<Prep>();
    const int N = 4001;
    const double t0 = std::log(1e-7), t1 = std::log(250.0);
    std::vector<double> t(N), g(N), gp(N), gm(N), gq(N);
    p->gx.resize(N); p->gd.resize(N); p->pd.resize(N);
    for (int i = 0; i < N; i++) {
        const std::size_t k = std::size_t(i);
        t[k] = t0 + (t1 - t0) * i / (N - 1);
        const double r = std::exp(t[k]);
        double P, Q; rawPQ(Z, n, kappa, r, P, Q);
        const double d = P * P + Q * Q;
        p->gx[k] = r; p->gd[k] = d; p->pd[k] = d / r;
        g[k] = d * r; gp[k] = d; gm[k] = d * r * r; gq[k] = Q * Q * r;    // integrands in t: D dr = D r dt, (D / r) dr = D dt, r D dr = D r^2 dt, Q^2 dr
    }
    const std::vector<double> c = cumulate(t, g), cp = cumulate(t, gp), cm = cumulate(t, gm), cq = cumulate(t, gq);
    p->norm = c.back(); p->mean = cm.back() / c.back(); p->small = cq.back() / c.back();
    p->gc.resize(N); p->pc.resize(N);
    for (int i = 0; i < N; i++) {
        const std::size_t k = std::size_t(i);
        p->gc[k] = std::min(1.0, std::max(0.0, c[k] / c.back())); p->gd[k] /= c.back();
        p->pc[k] = std::min(1.0, std::max(0.0, cp[k] / cp.back())); p->pd[k] /= cp.back();
    }
    p->gc.front() = 0; p->gc.back() = 1; p->pc.front() = 0; p->pc.back() = 1;
    const Prep& ref = *p;
    cache.emplace(key, std::move(p));
    return ref;
}
double interpCdf(const std::vector<double>& gx, const std::vector<double>& gc, const std::vector<double>& gd, double r) {   // cubic Hermite in r (hf.cpp)
    if (r <= gx.front()) return 0;
    if (r >= gx.back()) return 1;
    const std::size_t hi = std::size_t(std::upper_bound(gx.begin(), gx.end(), r) - gx.begin()), lo = hi - 1;
    const double h = gx[hi] - gx[lo], u = (r - gx[lo]) / h, u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * gc[lo] + (u3 - 2 * u2 + u) * gd[lo] * h + (-2 * u3 + 3 * u2) * gc[hi] + (u3 - u2) * gd[hi] * h;
}
double invertTable(const std::vector<double>& gx, const std::vector<double>& gc, const std::vector<double>& gd, double u) {   // the r with interpCdf(r) = u (bisection on the bracket, then on the cubic)
    if (u <= 0) return gx.front();
    if (u >= 1) return gx.back();
    const std::size_t hi = std::size_t(std::upper_bound(gc.begin(), gc.end(), u) - gc.begin()), lo = hi - 1;
    double a = gx[lo], b = gx[std::min(hi, gx.size() - 1)];
    // 0.65.0: every midpoint lies in [gx[lo], gx[hi]], so interpCdf's own search would find this segment again (at its right end the next segment's left end
    // gives gc[hi] too): the cubic of this segment evaluated directly, the same numbers bit for bit without a search of the table at each of the 60 steps
    if (hi >= gx.size() || lo + 1 != hi) { for (int it = 0; it < 60; it++) { const double m = 0.5 * (a + b); (interpCdf(gx, gc, gd, m) < u ? a : b) = m; } return 0.5 * (a + b); }
    const double x0 = gx[lo], x1 = gx[hi], h = x1 - x0, c0 = gc[lo], c1 = gc[hi], d0 = gd[lo], d1 = gd[hi], front = gx.front(), back = gx.back();
    const auto cdf = [&](double r) {                                    // (interpCdf on [gx[lo], gx[hi]], term by term as there)
        if (r <= front) return 0.0;
        if (r >= back) return 1.0;
        const double t = (r - x0) / h, t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * c0 + (t3 - 2 * t2 + t) * d0 * h + (-2 * t3 + 3 * t2) * c1 + (t3 - t2) * d1 * h;
    };
    // 0.66.0: a step whose midpoint is a or b itself leaves [a, b] where no later step changes it (the next midpoint and its test are the same, or a == b):
    // the loop stops there, the same number bit for bit as all 60 steps (about 45 steps here: the segment is 2^-7.5 of r wide)
    for (int it = 0; it < 60; it++) { const double m = 0.5 * (a + b); const bool end = m == a || m == b; (cdf(m) < u ? a : b) = m; if (end) break; }
    return 0.5 * (a + b);
}
double factorial(int k) { double f = 1; for (int i = 2; i <= k; i++) f *= i; return f; }
// the triangle coefficient of three doubled angular momenta: sqrt((a+b-c)! (a-b+c)! (-a+b+c)! / (a+b+c+1)!), 0 when they do not form a triangle
double triangle(int a2, int b2, int c2) {
    const int x = a2 + b2 - c2, y = a2 - b2 + c2, z = -a2 + b2 + c2, s = a2 + b2 + c2;
    if (x < 0 || y < 0 || z < 0 || x % 2 || y % 2 || z % 2) return 0;
    return std::sqrt(factorial(x / 2) * factorial(y / 2) * factorial(z / 2) / factorial(s / 2 + 1));
}
}  // namespace

void diracPQ(int Z, int n, int kappa, double r, double& P, double& Q) {
    rawPQ(Z, n, kappa, r, P, Q);
    const double s = 1 / std::sqrt(prep(Z, n, kappa).norm);
    P *= s; Q *= s;
}
double diracRadialDensity(int Z, int n, int kappa, double r) { double P, Q; diracPQ(Z, n, kappa, r, P, Q); return P * P + Q * Q; }
double diracMeanRadius(int Z, int n, int kappa) { return prep(Z, n, kappa).mean; }
double diracCdf(int Z, int n, int kappa, double r) { const Prep& p = prep(Z, n, kappa); return interpCdf(p.gx, p.gc, p.gd, r); }
double diracSmallShare(int Z, int n, int kappa) { return prep(Z, n, kappa).small; }

DiracPlaneTable diracPlaneTable(int Z, int n, int kappa) { const Prep& p = prep(Z, n, kappa); return DiracPlaneTable{&p.gx, &p.pc, &p.pd}; }
double diracPlaneCdf(int Z, int n, int kappa, double r) { const Prep& p = prep(Z, n, kappa); return interpCdf(p.gx, p.pc, p.pd, r); }
double sampleDiracRadius(int Z, int n, int kappa, orbit::Mulberry32& r) { const Prep& p = prep(Z, n, kappa); return invertTable(p.gx, p.gc, p.gd, r.next()); }
void sampleDiracDirection(int j2, bool round, orbit::Mulberry32& r, double& ux, double& uy, double& uz) {
    const int L = (j2 - 1) / 2;
    double cz;
    for (;;) {
        cz = 2 * r.next() - 1;
        if (round || L == 0) break;
        if (r.next() < std::pow(1 - cz * cz, L)) break;                // |Y_L,L|^2 ~ sin^(2L) theta, at most 1 (at the equator)
    }
    const double ph = 2 * kPi * r.next(), sz = std::sqrt(std::max(0.0, 1 - cz * cz));
    ux = sz * std::cos(ph); uy = sz * std::sin(ph); uz = cz;
}
void sampleDirac(int Z, int n, int l, int j2, bool round, orbit::Mulberry32& r, double& x, double& y, double& z) {
    const double rad = sampleDiracRadius(Z, n, diracKappa(l, j2), r);
    double ux, uy, uz; sampleDiracDirection(j2, round, r, ux, uy, uz);
    x = rad * ux; y = rad * uy; z = rad * uz;
}
bool tryDiracOnPlane(int Z, int n, int l, int j2, bool round, int axis, double off, orbit::Mulberry32& r, double& x, double& y, double& z) {
    // A density f(r) A(theta) on the plane: the ring of plane radius s (s^2 = r^2 - off^2, s ds = r dr) holds f(r) 2 pi r dr, f = D / (4 pi r^2), so r has the density D(r) / r from |off| on
    // (drawn from its table, conditioned on r >= |off|); the azimuth in the plane is uniform; the lined-up factor sin^(2L) theta (theta from z) is applied by rejection (at most 1).
    const Prep& p = prep(Z, n, diracKappa(l, j2));
    const double a = std::fabs(off), c0 = interpCdf(p.gx, p.pc, p.pd, a);
    if (!(c0 < 1)) return false;
    const double rad = std::max(a, invertTable(p.gx, p.pc, p.pd, c0 + r.next() * (1 - c0)));
    const double s = std::sqrt(std::max(0.0, rad * rad - a * a)), ph = 2 * kPi * r.next(), u = s * std::cos(ph), v = s * std::sin(ph);
    if (axis == 2) { x = u; y = v; z = off; } else { x = u; z = v; y = off; }
    const int L = (j2 - 1) / 2;
    if (round || L == 0 || !(rad > 0)) return true;
    const double cz = z / rad;
    return r.next() < std::pow(std::max(0.0, 1 - cz * cz), L);
}

namespace {
// four of invertTable side by side (the same numbers bit for bit: each lane's steps are invertTable's own, on its own segment); a lane whose segment
// cannot be fixed is left to invertTable
void invertTable4(const std::vector<double>& gx, const std::vector<double>& gc, const std::vector<double>& gd, const double* u, int m, double* out) {
    double a[4] = {0, 0, 0, 0}, b[4] = {0, 0, 0, 0}, x0[4] = {0, 0, 0, 0}, h[4] = {1, 1, 1, 1}, c0[4] = {0, 0, 0, 0}, c1[4] = {0, 0, 0, 0}, d0[4] = {0, 0, 0, 0}, d1[4] = {0, 0, 0, 0}, uu[4] = {0, 0, 0, 0};
    bool lane[4] = {false, false, false, false};
    const double front = gx.front(), back = gx.back();
    for (int k = 0; k < m; k++) {
        if (u[k] <= 0 || u[k] >= 1) { out[k] = invertTable(gx, gc, gd, u[k]); continue; }
        const std::size_t hi = std::size_t(std::upper_bound(gc.begin(), gc.end(), u[k]) - gc.begin()), lo = hi - 1;
        if (hi >= gx.size()) { out[k] = invertTable(gx, gc, gd, u[k]); continue; }
        lane[k] = true; uu[k] = u[k];
        a[k] = gx[lo]; b[k] = gx[hi]; x0[k] = gx[lo]; h[k] = gx[hi] - gx[lo]; c0[k] = gc[lo]; c1[k] = gc[hi]; d0[k] = gd[lo]; d1[k] = gd[hi];
    }
    bool run[4] = {lane[0], lane[1], lane[2], lane[3]};
    for (int it = 0; it < 60 && (run[0] || run[1] || run[2] || run[3]); it++)   // 0.66.0: a lane stops where invertTable's loop stops (the same numbers)
        for (int k = 0; k < 4; k++) {
            if (!run[k]) continue;
            const double mid = 0.5 * (a[k] + b[k]);
            if (mid == a[k] || mid == b[k]) run[k] = false;
            double c;
            if (mid <= front) c = 0.0;
            else if (mid >= back) c = 1.0;
            else { const double t = (mid - x0[k]) / h[k], t2 = t * t, t3 = t2 * t; c = (2 * t3 - 3 * t2 + 1) * c0[k] + (t3 - 2 * t2 + t) * d0[k] * h[k] + (-2 * t3 + 3 * t2) * c1[k] + (t3 - t2) * d1[k] * h[k]; }
            (c < uu[k] ? a[k] : b[k]) = mid;
        }
    for (int k = 0; k < m; k++) if (lane[k]) out[k] = 0.5 * (a[k] + b[k]);
}
}  // namespace

long diracPlaneDots(int Z, int n, int l, int j2, bool round, int axis, double off, orbit::Mulberry32& r, long want, long maxTries,
                    const std::function<void(double, double, double)>& record) {
    const Prep& p = prep(Z, n, diracKappa(l, j2));
    const double a = std::fabs(off), c0 = interpCdf(p.gx, p.pc, p.pd, a);
    if (!(c0 < 1)) return 0;                                            // (every attempt kept nothing and took no number)
    const int L = (j2 - 1) / 2;
    const bool keepTest = !(round || L == 0);
    long got = 0, tries = 0;
    while (got < want && tries < maxTries) {
        const int m = int(std::min<long>(4, maxTries - tries));
        orbit::Mulberry32 q = r;                                        // (the next attempts' numbers, read ahead: 3 an attempt with the keep test, else 2)
        double U[4] = {0, 0, 0, 0}, PH[4] = {0, 0, 0, 0}, K[4] = {0, 0, 0, 0}, rad[4] = {0, 0, 0, 0};
        for (int k = 0; k < m; k++) { U[k] = c0 + q.next() * (1 - c0); PH[k] = q.next(); if (keepTest) K[k] = q.next(); }
        invertTable4(p.gx, p.pc, p.pd, U, m, rad);
        for (int k = 0; k < m && got < want; k++) {                     // attempt by attempt, as tryDiracOnPlane
            tries++;
            const double rk = std::max(a, rad[k]);
            r.next(); r.next();
            const double s = std::sqrt(std::max(0.0, rk * rk - a * a)), ph = 2 * kPi * PH[k], uu = s * std::cos(ph), vv = s * std::sin(ph);
            double x, y, z;
            if (axis == 2) { x = uu; y = vv; z = off; } else { x = uu; z = vv; y = off; }
            if (!keepTest || !(rk > 0)) {
                record(x, y, z); got++;
                if (keepTest) break;                                    // (no keep number taken: the numbers read ahead for the next attempts are not theirs)
                continue;
            }
            r.next();
            const double cz = z / rk;
            if (K[k] < std::pow(std::max(0.0, 1 - cz * cz), L)) { record(x, y, z); got++; }
        }
    }
    return got;
}

DiracGraph diracGraph(int Z, int n, int kappa) {
    const Prep& p = prep(Z, n, kappa);
    DiracGraph g;
    g.rEnd = invertTable(p.gx, p.gc, p.gd, 0.999);                     // radial_graph.h kRadialCover
    g.mean = p.mean;
    const int K = 400;
    double best = -1;
    for (int i = 0; i <= K; i++) {
        const double r = g.rEnd * i / K, d = diracRadialDensity(Z, n, kappa, r);
        g.r.push_back(r); g.D.push_back(d);
        if (d > best) { best = d; g.peak = r; }
    }
    for (int i = 1; i < K; i++) if (g.D[std::size_t(i)] > g.D[std::size_t(i - 1)] && g.D[std::size_t(i)] >= g.D[std::size_t(i + 1)]) g.maxima.push_back(g.r[std::size_t(i)]);
    return g;
}

double wigner6j(double j1, double j2, double j3, double j4, double j5, double j6) {
    // Racah's formula: {j1 j2 j3; j4 j5 j6} = D(j1 j2 j3) D(j1 j5 j6) D(j4 j2 j6) D(j4 j5 j3) sum_t (-1)^t (t + 1)! / [(t - a1)!(t - a2)!(t - a3)!(t - a4)! (b1 - t)!(b2 - t)!(b3 - t)!]
    const int a = int(std::lround(2 * j1)), b = int(std::lround(2 * j2)), c = int(std::lround(2 * j3)), d = int(std::lround(2 * j4)), e = int(std::lround(2 * j5)), f = int(std::lround(2 * j6));
    const double pre = triangle(a, b, c) * triangle(a, e, f) * triangle(d, b, f) * triangle(d, e, c);
    if (pre == 0) return 0;
    const int a1 = (a + b + c) / 2, a2 = (a + e + f) / 2, a3 = (d + b + f) / 2, a4 = (d + e + c) / 2, b1 = (a + b + d + e) / 2, b2 = (b + c + e + f) / 2, b3 = (a + c + d + f) / 2;
    double sum = 0;
    for (int t = std::max({a1, a2, a3, a4}); t <= std::min({b1, b2, b3}); t++)
        sum += (t % 2 ? -1.0 : 1.0) * factorial(t + 1) / (factorial(t - a1) * factorial(t - a2) * factorial(t - a3) * factorial(t - a4) * factorial(b1 - t) * factorial(b2 - t) * factorial(b3 - t));
    return pre * sum;
}
double lineStrength(int l, int j2, int lp, int jp2) {
    if (!diracAllowed(l, j2, lp, jp2)) return 0;
    const double w = wigner6j(l, j2 / 2.0, 0.5, jp2 / 2.0, lp, 1);
    return (j2 + 1) * (jp2 + 1) * w * w;
}

std::string diracStateText(int n, int l, int j2) {
    static const char* const sub[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
    return std::to_string(n) + "spdfghi"[l] + sub[j2 % 10] + "/₂";
}

}  // namespace atom
