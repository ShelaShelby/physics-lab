#include "atom/stark.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "atom/atom_core.h"
#include "atom/threej.h"
#include "orbit/belts.h"

namespace atom::stark {

namespace {
constexpr double kTwoPi = 2 * kPi;

double factorial(int k) { double f = 1; for (int i = 2; i <= k; i++) f *= i; return f; }
double binom(int a, int b) { return b < 0 || b > a ? 0 : factorial(a) / (factorial(b) * factorial(a - b)); }

// the associated Laguerre polynomial L^a_k(x) by its three-term recurrence (DLMF 18.9.13), and its derivative d/dx L^a_k = -L^{a+1}_{k-1} (DLMF 18.9.23)
double laguerre(int k, double a, double x) {
    if (k < 0) return 0;
    double l0 = 1, l1 = 1 + a - x;
    if (k == 0) return l0;
    for (int i = 1; i < k; i++) { const double l2 = ((2 * i + 1 + a - x) * l1 - (i + a) * l0) / (i + 1); l0 = l1; l1 = l2; }
    return l1;
}
double laguerreD(int k, double a, double x) { return k <= 0 ? 0 : -laguerre(k - 1, a + 1, x); }
// the coefficients of L^m_k(x) = sum_j (-1)^j C(k + m, k - j) x^j / j! (DLMF 18.5.12)
std::vector<double> laguerreCoef(int k, int m) {
    std::vector<double> c(static_cast<std::size_t>(k + 1));
    for (int j = 0; j <= k; j++) c[std::size_t(j)] = (j % 2 ? -1 : 1) * binom(k + m, k - j) / factorial(j);
    return c;
}
std::vector<double> mul(const std::vector<double>& a, const std::vector<double>& b) {
    std::vector<double> c(a.size() + b.size() - 1, 0.0);
    for (std::size_t i = 0; i < a.size(); i++) for (std::size_t j = 0; j < b.size(); j++) c[i + j] += a[i] * b[j];
    return c;
}
std::vector<double> shiftUp(const std::vector<double>& a, int by) { std::vector<double> c(static_cast<std::size_t>(by), 0.0); c.insert(c.end(), a.begin(), a.end()); return c; }
double polyAt(const std::vector<double>& a, double x) { double v = 0; for (std::size_t i = a.size(); i-- > 0;) v = v * x + a[i]; return v; }
double moment(const std::vector<double>& p) { double s = 0; for (std::size_t j = 0; j < p.size(); j++) s += p[j] * factorial(int(j)); return s; }   // int_0^inf p(x) e^{-x} dx
// the tail polynomial Q = p + p' + p'' + ...: int_x^inf p(t) e^{-t} dt = e^{-x} Q(x)
std::vector<double> tailOf(const std::vector<double>& p) {
    std::vector<double> q(p.size(), 0.0);
    std::vector<double> d = p;
    for (std::size_t it = 0; it < p.size(); it++) {
        for (std::size_t j = 0; j < d.size(); j++) q[j] += d[j];
        std::vector<double> e(d.size() > 1 ? d.size() - 1 : 1, 0.0);
        for (std::size_t j = 1; j < d.size(); j++) e[j - 1] = d[j] * double(j);
        d = e;
    }
    return q;
}

// eigenvalues (and, if v, eigenvectors as columns) of a real symmetric matrix by cyclic Jacobi rotations (Golub & Van Loan, "Matrix Computations", the
// cyclic Jacobi method; the same method as atom/zeeman.cpp)
void jacobi(std::vector<std::vector<double>>& a, std::vector<double>& w, std::vector<std::vector<double>>* v) {
    const std::size_t n = a.size();
    if (v) { v->assign(n, std::vector<double>(n, 0)); for (std::size_t i = 0; i < n; i++) (*v)[i][i] = 1; }
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0, all = 0;
        for (std::size_t i = 0; i < n; i++) for (std::size_t j = 0; j < n; j++) { all += a[i][j] * a[i][j]; if (i != j) off += a[i][j] * a[i][j]; }
        if (off <= 1e-34 * all || off == 0) break;
        for (std::size_t p = 0; p < n; p++)
            for (std::size_t q = p + 1; q < n; q++) {
                if (a[p][q] == 0) continue;
                const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const double t = (th >= 0 ? 1.0 : -1.0) / (std::fabs(th) + std::sqrt(th * th + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (std::size_t k = 0; k < n; k++) { const double akp = a[k][p], akq = a[k][q]; a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq; }
                for (std::size_t k = 0; k < n; k++) { const double apk = a[p][k], aqk = a[q][k]; a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk; }
                if (v) for (std::size_t k = 0; k < n; k++) { const double vkp = (*v)[k][p], vkq = (*v)[k][q]; (*v)[k][p] = c * vkp - s * vkq; (*v)[k][q] = s * vkp + c * vkq; }
            }
    }
    w.resize(n);
    for (std::size_t i = 0; i < n; i++) w[i] = a[i][i];
}

// Gauss-Laguerre nodes and weights for the weight e^{-x} on [0, inf), N points: exact for polynomials of degree 2N - 1. The nodes start from the
// eigenvalues of the Jacobi matrix (diagonal 2i + 1, off-diagonal i: Golub & Welsch, Math. Comp. 23, 221 (1969)) and are polished by Newton's method on
// L_N; the weights w = x / ((N + 1)^2 L_{N+1}(x)^2) (DLMF 3.5.27 with 18.9.13).
struct GL { std::vector<double> x, w; };
const GL& gaussLaguerre(int N) {
    static std::vector<GL> cache(96);
    GL& g = cache[std::size_t(std::min(N, 95))];
    if (!g.x.empty()) return g;
    std::vector<std::vector<double>> J(static_cast<std::size_t>(N), std::vector<double>(static_cast<std::size_t>(N), 0.0));
    for (int i = 0; i < N; i++) { J[std::size_t(i)][std::size_t(i)] = 2 * i + 1; if (i + 1 < N) J[std::size_t(i)][std::size_t(i + 1)] = J[std::size_t(i + 1)][std::size_t(i)] = i + 1; }
    std::vector<double> ev;
    jacobi(J, ev, nullptr);
    std::sort(ev.begin(), ev.end());
    g.x.resize(std::size_t(N)); g.w.resize(std::size_t(N));
    for (int i = 0; i < N; i++) {
        double x = ev[std::size_t(i)];
        for (int it = 0; it < 8; it++) { const double d = laguerreD(N, 0, x); if (d == 0) break; const double dx = laguerre(N, 0, x) / d; x -= dx; if (std::fabs(dx) <= 1e-16 * x) break; }
        const double l1 = laguerre(N + 1, 0, x);
        g.x[std::size_t(i)] = x;
        g.w[std::size_t(i)] = x / ((N + 1.0) * (N + 1.0) * l1 * l1);
    }
    return g;
}
double angZ(int l, int lp, int m) {          // <l m| cos theta |l' m>, l' = l +- 1
    const int L = std::max(l, lp);
    if (std::abs(l - lp) != 1 || L < 1) return 0;
    const double v = (double(L) * L - double(m) * m) / ((2.0 * L + 1) * (2.0 * L - 1));
    return v > 0 ? std::sqrt(v) : 0;
}
double angC(int l, int m, int lp, int mp, int q) {   // <l m| C^1_q |l' m'> (Edmonds eq. 5.4.5 with 4.6.3)
    const double s = ((m % 2) + 2) % 2 ? -1.0 : 1.0;
    return s * std::sqrt((2.0 * l + 1) * (2.0 * lp + 1)) * threeJ(2 * l, 2, 2 * lp, -2 * m, 2 * q, 2 * mp) * threeJ(2 * l, 2, 2 * lp, 0, 0, 0);
}
}  // namespace

double fieldUnitVm() { return kHartreeEV / kA0; }          // E_h / (e a0): E_h in eV is E_h / e in volts
double reducedMassH() { return 1.0 / (1.0 + 1.0 / kMpOverMe); }

bool valid(const Par& s) { return s.n >= 1 && s.n1 >= 0 && s.n2 >= 0 && s.n1 + s.n2 + std::abs(s.m) + 1 == s.n; }
std::vector<Par> manifold(int n) {
    std::vector<Par> v;
    for (int m = -(n - 1); m <= n - 1; m++) {
        const int d = n - std::abs(m) - 1;
        for (int n1 = 0; n1 <= d; n1++) v.push_back(Par{n, n1, d - n1, m});   // n1 - n2 = 2 n1 - d: lowest first
    }
    return v;
}
std::string name(const Par& s) { char b[64]; std::snprintf(b, sizeof b, "(n1, n2, m) = (%d, %d, %d)", s.n1, s.n2, s.m); return b; }
double linearAu(const Par& s) { return 1.5 * s.n * (s.n1 - s.n2); }
double secondAu(const Par& s) {
    const double n = s.n, k = s.n1 - s.n2, m = s.m;
    return -(n * n * n * n) / 16 * (17 * n * n - 3 * k * k - 9 * m * m + 19);
}
double energyAu(const Par& s, double F, double mu) {
    const double f = F / (mu * mu);                          // the field in the reduced mass's own unit
    return mu * (-1.0 / (2.0 * s.n * s.n) + linearAu(s) * f + secondAu(s) * f * f);
}
double saddleFieldAu(int n, double mu) { const double W = 1.0 / (2.0 * n * n); return mu * mu * W * W / 4; }

double hydrogenR(int n, int l, double r) {
    const double x = 2 * r / n;
    const double N = std::sqrt(std::pow(2.0 / n, 3) * factorial(n - l - 1) / (2.0 * n * factorial(n + l)));
    return N * std::exp(-r / n) * std::pow(x, l) * laguerre(n - l - 1, 2 * l + 1, x);
}
double radialR(int n, int l, int n2, int l2) {
    // int R R' r^3 dr with r = x / s, s = 1/n + 1/n2: e^{-x} times a polynomial of degree (n - 1) + (n2 - 1) + 3 in x
    const double s = 1.0 / n + 1.0 / n2;
    const GL& g = gaussLaguerre(n + n2 + 4);
    double sum = 0;
    for (std::size_t i = 0; i < g.x.size(); i++) {
        const double r = g.x[i] / s;
        sum += g.w[i] * std::exp(g.x[i]) * hydrogenR(n, l, r) * hydrogenR(n2, l2, r) * r * r * r;
    }
    return sum / s;
}

Block withinN(int n, int m) {
    Block b;
    b.n = n; b.m = m;
    const int am = std::abs(m), dim = n - am;
    if (dim <= 0) return b;
    std::vector<std::vector<double>> M(static_cast<std::size_t>(dim), std::vector<double>(static_cast<std::size_t>(dim), 0.0));
    for (int i = 0; i + 1 < dim; i++) {
        const int l = am + i;
        M[std::size_t(i)][std::size_t(i + 1)] = M[std::size_t(i + 1)][std::size_t(i)] = radialR(n, l, n, l + 1) * angZ(l, l + 1, am);
    }
    std::vector<double> w;
    std::vector<std::vector<double>> V;
    jacobi(M, w, &V);
    std::vector<int> order(static_cast<std::size_t>(dim));
    for (int i = 0; i < dim; i++) order[std::size_t(i)] = i;
    std::sort(order.begin(), order.end(), [&](int a, int c) { return w[std::size_t(a)] < w[std::size_t(c)]; });
    for (int i : order) {
        b.z.push_back(w[std::size_t(i)]);
        b.k.push_back(int(std::lround(w[std::size_t(i)] / (1.5 * n))));
        std::vector<double> v(static_cast<std::size_t>(dim));
        for (int l = 0; l < dim; l++) v[std::size_t(l)] = V[std::size_t(l)][std::size_t(i)];
        b.vec.push_back(v);
    }
    return b;
}

std::vector<double> sturmianLevels(int nk, int m, double F, int lmax, int K) {
    const int am = std::abs(m);
    if (lmax < am || K < 0) return {};
    const int nl = lmax - am + 1, D = nl * (K + 1);
    const GL& g = gaussLaguerre(lmax + K + 5);
    const std::size_t N = g.x.size();
    // each basis function without its exponential, f = r^l L^{2l+1}_k(x), and its r-derivative with the exponential's part, gfn (chi' = e^{-r/nk} gfn), at the nodes
    std::vector<std::vector<double>> f(static_cast<std::size_t>(D), std::vector<double>(N)), gd(static_cast<std::size_t>(D), std::vector<double>(N));
    std::vector<int> lOf(static_cast<std::size_t>(D));
    for (int il = 0; il < nl; il++)
        for (int k = 0; k <= K; k++) {
            const int a = il * (K + 1) + k, l = am + il;
            lOf[std::size_t(a)] = l;
            for (std::size_t j = 0; j < N; j++) {
                const double x = g.x[j], r = nk * x / 2, L = laguerre(k, 2 * l + 1, x), Ld = laguerreD(k, 2 * l + 1, x);
                const double rl = std::pow(r, l);
                f[std::size_t(a)][j] = rl * L;
                gd[std::size_t(a)][j] = (l > 0 ? l * std::pow(r, l - 1) * L : 0.0) - rl * L / nk + rl * Ld * 2.0 / nk;
            }
        }
    // integrals int F(r) e^{-2r/nk} dr = (nk / 2) sum w F(r_j)
    const double h = nk / 2.0;
    std::vector<std::vector<double>> S(static_cast<std::size_t>(D), std::vector<double>(static_cast<std::size_t>(D), 0.0)), H = S;
    for (int a = 0; a < D; a++)
        for (int b = a; b < D; b++) {
            const int la = lOf[std::size_t(a)], lb = lOf[std::size_t(b)];
            double s = 0, hh = 0;
            if (la == lb) {
                double t = 0, v = 0;
                for (std::size_t j = 0; j < N; j++) {
                    const double r = nk * g.x[j] / 2, ff = f[std::size_t(a)][j] * f[std::size_t(b)][j];
                    s += g.w[j] * ff * r * r;
                    v -= g.w[j] * ff * r;
                    t += g.w[j] * (gd[std::size_t(a)][j] * gd[std::size_t(b)][j] * r * r + la * (la + 1.0) * ff);
                }
                s *= h; hh = h * (0.5 * t + v);
            } else if (std::abs(la - lb) == 1 && F != 0) {
                double zz = 0;
                for (std::size_t j = 0; j < N; j++) { const double r = nk * g.x[j] / 2; zz += g.w[j] * f[std::size_t(a)][j] * f[std::size_t(b)][j] * r * r * r; }
                hh = F * h * zz * angZ(la, lb, am);
            }
            S[std::size_t(a)][std::size_t(b)] = S[std::size_t(b)][std::size_t(a)] = s;
            H[std::size_t(a)][std::size_t(b)] = H[std::size_t(b)][std::size_t(a)] = hh;
        }
    // scale each function to unit norm, then Cholesky S = L L^T and A = L^-1 H L^-T
    std::vector<double> sc(static_cast<std::size_t>(D));
    for (int a = 0; a < D; a++) sc[std::size_t(a)] = 1 / std::sqrt(S[std::size_t(a)][std::size_t(a)]);
    for (int a = 0; a < D; a++) for (int b = 0; b < D; b++) { S[std::size_t(a)][std::size_t(b)] *= sc[std::size_t(a)] * sc[std::size_t(b)]; H[std::size_t(a)][std::size_t(b)] *= sc[std::size_t(a)] * sc[std::size_t(b)]; }
    std::vector<std::vector<double>> L(static_cast<std::size_t>(D), std::vector<double>(static_cast<std::size_t>(D), 0.0));
    for (int i = 0; i < D; i++)
        for (int j = 0; j <= i; j++) {
            double s = S[std::size_t(i)][std::size_t(j)];
            for (int k = 0; k < j; k++) s -= L[std::size_t(i)][std::size_t(k)] * L[std::size_t(j)][std::size_t(k)];
            if (i == j) { if (!(s > 0)) return {}; L[std::size_t(i)][std::size_t(i)] = std::sqrt(s); }
            else L[std::size_t(i)][std::size_t(j)] = s / L[std::size_t(j)][std::size_t(j)];
        }
    // Y = L^-1 H (forward substitution per column), A = L^-1 Y^T
    std::vector<std::vector<double>> Y(static_cast<std::size_t>(D), std::vector<double>(static_cast<std::size_t>(D), 0.0)), A = Y;
    for (int c = 0; c < D; c++)
        for (int i = 0; i < D; i++) {
            double s = H[std::size_t(i)][std::size_t(c)];
            for (int k = 0; k < i; k++) s -= L[std::size_t(i)][std::size_t(k)] * Y[std::size_t(k)][std::size_t(c)];
            Y[std::size_t(i)][std::size_t(c)] = s / L[std::size_t(i)][std::size_t(i)];
        }
    for (int c = 0; c < D; c++)
        for (int i = 0; i < D; i++) {
            double s = Y[std::size_t(c)][std::size_t(i)];
            for (int k = 0; k < i; k++) s -= L[std::size_t(i)][std::size_t(k)] * A[std::size_t(k)][std::size_t(c)];
            A[std::size_t(i)][std::size_t(c)] = s / L[std::size_t(i)][std::size_t(i)];
        }
    for (int i = 0; i < D; i++) for (int j = i + 1; j < D; j++) { const double s = 0.5 * (A[std::size_t(i)][std::size_t(j)] + A[std::size_t(j)][std::size_t(i)]); A[std::size_t(i)][std::size_t(j)] = A[std::size_t(j)][std::size_t(i)] = s; }
    std::vector<double> w;
    jacobi(A, w, nullptr);
    std::sort(w.begin(), w.end());
    return w;
}
std::vector<double> sturmianManifold(int n, int m, double F, int lmax, int K) {
    const int dim = n - std::abs(m);
    if (dim <= 0) return {};
    std::vector<double> w = sturmianLevels(n, m, F, lmax, K);
    if (int(w.size()) < dim) return {};
    const double e0 = -1.0 / (2.0 * n * n);
    std::sort(w.begin(), w.end(), [&](double a, double b) { return std::fabs(a - e0) < std::fabs(b - e0); });
    w.resize(std::size_t(dim));
    std::sort(w.begin(), w.end());
    return w;
}

ParSampler parSampler(const Par& s) {
    ParSampler ps;
    ps.s = s;
    const int am = std::abs(s.m);
    const std::vector<double> c1 = laguerreCoef(s.n1, am), c2 = laguerreCoef(s.n2, am);
    const std::vector<double> f1 = shiftUp(mul(c1, c1), am), f2 = shiftUp(mul(c2, c2), am);   // x^|m| L^2
    ps.pA1 = shiftUp(f1, 1); ps.pA2 = f2; ps.pB1 = f1; ps.pB2 = shiftUp(f2, 1);
    ps.qA1 = tailOf(ps.pA1); ps.qA2 = tailOf(ps.pA2); ps.qB1 = tailOf(ps.pB1); ps.qB2 = tailOf(ps.pB2);
    ps.w1 = moment(ps.pA1) * moment(ps.pA2);
    ps.w2 = moment(ps.pB1) * moment(ps.pB2);
    const double n = s.n;
    ps.A = std::sqrt(4 / (kTwoPi * n * n * n * (ps.w1 + ps.w2)));   // int |psi|^2 dV = A^2 2 pi (n^3 / 4) (w1 + w2)
    return ps;
}
double normalisation(const ParSampler& ps) { const double n = ps.s.n; return ps.A * ps.A * kTwoPi * n * n * n / 4 * (ps.w1 + ps.w2); }
double meanZ(const ParSampler& ps) {
    // <z> = A^2 2 pi (n^4 / 8) [int x^2 f1 int f2 - int f1 int y^2 f2]
    const double n = ps.s.n;
    const double a = moment(shiftUp(ps.pA1, 1)) * moment(ps.pA2), b = moment(ps.pB1) * moment(shiftUp(ps.pB2, 1));
    return ps.A * ps.A * kTwoPi * n * n * n * n / 8 * (a - b);
}
double meanR(const ParSampler& ps) {
    // <r> = <(xi + eta) / 2> = A^2 2 pi (n^4 / 8) [int x^2 f1 int f2 + 2 int x f1 int y f2 + int f1 int y^2 f2] ((xi + eta)^2 / 8 with dV)
    const double n = ps.s.n;
    const double a = moment(shiftUp(ps.pA1, 1)) * moment(ps.pA2), b = moment(ps.pB1) * moment(shiftUp(ps.pB2, 1)), c = moment(ps.pA1) * moment(ps.pB2);
    return ps.A * ps.A * kTwoPi * n * n * n * n / 8 * (a + 2 * c + b);
}
double invertTail(const std::vector<double>& q, double T) {
    if (q.empty()) return 0;
    const double Q0 = q[0];
    if (!(T < 1)) return 0;
    if (!(T > 0)) T = 1e-300;
    const auto G = [&](double x) { return std::exp(-x) * polyAt(q, x); };
    double lo = 0, hi = 1;
    while (G(hi) > T * Q0 && hi < 1e4) { lo = hi; hi *= 2; }
    for (int i = 0; i < 64; i++) {
        const double mid = 0.5 * (lo + hi);
        if (mid == lo || mid == hi) break;
        if (G(mid) > T * Q0) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}
void samplePar(const ParSampler& ps, orbit::Mulberry32& r, double& x, double& y, double& z) {
    const double u0 = r.next(), ux = r.next(), uy = r.next(), uf = r.next();
    const bool a = u0 < ps.share();
    const double X = invertTail(a ? ps.qA1 : ps.qB1, 1 - ux), Y = invertTail(a ? ps.qA2 : ps.qB2, 1 - uy);
    const double n = ps.s.n, xi = n * X, eta = n * Y, rho = std::sqrt(xi * eta), ph = kTwoPi * uf;
    x = rho * std::cos(ph); y = rho * std::sin(ph); z = (xi - eta) / 2;
}
std::complex<double> psiPar(const ParSampler& ps, double x, double y, double z) {
    const double r = std::sqrt(x * x + y * y + z * z), n = ps.s.n, am = std::abs(ps.s.m);
    const double X = (r + z) / n, Y = std::max(0.0, r - z) / n;
    const auto u = [&](int k, double t) { return std::pow(t, am / 2) * std::exp(-t / 2) * laguerre(k, am, t); };
    const double v = ps.A * u(ps.s.n1, X) * u(ps.s.n2, Y);
    const double ph = ps.s.m * std::atan2(y, x);
    return std::complex<double>(v * std::cos(ph), v * std::sin(ph));
}

std::vector<Component> lineComponents(int nu, int nl, double F, double mu) {
    std::vector<Component> out;
    if (!(nu > nl) || nl < 1) return out;
    std::vector<Block> bu, bl;
    for (int m = -(nu - 1); m <= nu - 1; m++) bu.push_back(withinN(nu, m));
    for (int m = -(nl - 1); m <= nl - 1; m++) bl.push_back(withinN(nl, m));
    // the radial integrals once
    std::vector<std::vector<double>> R(static_cast<std::size_t>(nu), std::vector<double>(static_cast<std::size_t>(nl), 0.0));
    for (int l = 0; l < nu; l++) for (int lp = 0; lp < nl; lp++) if (std::abs(l - lp) == 1) R[std::size_t(l)][std::size_t(lp)] = radialR(nu, l, nl, lp);
    const double e0 = mu * (-1.0 / (2.0 * nu * nu) + 1.0 / (2.0 * nl * nl));
    for (const Block& U : bu)
        for (const Block& D : bl) {
            const int q = U.m - D.m;
            if (std::abs(q) > 1) continue;
            const int au = std::abs(U.m), ad = std::abs(D.m);
            for (std::size_t i = 0; i < U.z.size(); i++)
                for (std::size_t j = 0; j < D.z.size(); j++) {
                    double amp = 0;
                    for (std::size_t a = 0; a < U.vec[i].size(); a++)
                        for (std::size_t b = 0; b < D.vec[j].size(); b++) {
                            const int l = au + int(a), lp = ad + int(b);
                            if (std::abs(l - lp) != 1) continue;
                            amp += U.vec[i][a] * D.vec[j][b] * R[std::size_t(l)][std::size_t(lp)] * angC(l, U.m, lp, D.m, q);
                        }
                    Component c;
                    const int ku = U.k[i], kd = D.k[j];
                    c.up = Par{nu, (nu - au - 1 + ku) / 2, (nu - au - 1 - ku) / 2, U.m};
                    c.lo = Par{nl, (nl - ad - 1 + kd) / 2, (nl - ad - 1 - kd) / 2, D.m};
                    c.q = q;
                    c.strength = amp * amp;
                    c.shiftAu = energyAu(c.up, F, mu) - energyAu(c.lo, F, mu) - e0;
                    out.push_back(c);
                }
        }
    std::sort(out.begin(), out.end(), [](const Component& a, const Component& b) { return a.shiftAu < b.shiftAu; });
    return out;
}
double fieldFreeStrength(int nu, int nl) {
    double s = 0;
    for (int l = 0; l < nu; l++)
        for (int lp = 0; lp < nl; lp++) {
            if (std::abs(l - lp) != 1) continue;
            const double R = radialR(nu, l, nl, lp);
            for (int m = -l; m <= l; m++)
                for (int mp = -lp; mp <= lp; mp++) {
                    const int q = m - mp;
                    if (std::abs(q) > 1) continue;
                    const double a = R * angC(l, m, lp, mp, q);
                    s += a * a;
                }
        }
    return s;
}

double diracFineSplitAu(int n) {
    if (n < 2) return 0;
    const auto e = [](int nn, double j) { const double k = j + 0.5, d = nn - k + std::sqrt(k * k - kAlpha * kAlpha); return (1 / std::sqrt(1 + kAlpha * kAlpha / (d * d)) - 1) / (kAlpha * kAlpha); };
    return e(n, n - 0.5) - e(n, 0.5);
}
double fineCrossFieldAu(int n) { return n >= 2 ? diracFineSplitAu(n) / (1.5 * n * (n - 1)) : 0; }

std::string limitsText() {
    char fs[400];
    const double u = fieldUnitVm() / 1e5;   // kV/cm per atomic unit
    std::snprintf(fs, sizeof fs, " With fine structure, in fields where the Stark shift is smaller than the fine-structure splitting, the levels shift with the square "
                  "of the field. The lab leaves this out. The outermost levels of n = 2 reach the splitting at %.2g kV/cm, n = 3 at %.2g, n = 4 at %.2g and n = 5 at %.2g "
                  "(Dirac): below that field the picture of that n is not right.", fineCrossFieldAu(2) * u, fineCrossFieldAu(3) * u, fineCrossFieldAu(4) * u, fineCrossFieldAu(5) * u);
    return "Hydrogen only, without relativity: no fine structure, no spin, no Lamb shift. The levels are worked out to second order in the field: the linear "
           "shift inside each n (exact there) and the second-order shift from the mixing between different n. Below the saddle-point field F_c = W²/4 of the "
           "highest n shown (W: that level's binding energy) the electron stays bound; above it, it can leave over the saddle of the potential. The lab draws "
           "no ionisation and no level widths, so the field stops at F_c. The nucleus is a point of hydrogen's mass (the reduced mass is kept in the energies). "
           "A parabolic state's cloud is the state the field picks at first order (its shape does not change with the field's size); its lengths include the reduced "
           "mass, as the lab's other hydrogen clouds do (0.05 % longer than for an infinitely heavy nucleus)." + std::string(fs);
}

}  // namespace atom::stark
