#include "blackhole/spin.h"

#include <algorithm>
#include <cmath>

#include "blackhole/elliptic.h"
#include "blackhole/kerr.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// trapped orbits, J15 (A.5), (A.6)
double trappedRadius1(double a) { return 2 * (1 + std::cos(2.0 / 3.0 * std::acos(-a))); }
double trappedRadius2(double a) { return 2 * (1 + std::cos(2.0 / 3.0 * std::acos(a))); }
double trappedB(double a, double r) { return -(r * r * (r - 3) + a * a * (r + 1)) / (a * (r - 1)); }
double trappedQ(double a, double r) { const double d = r - 3; return -r * r * r * (r * d * d - 4 * a * a) / (a * a * (r - 1) * (r - 1)); }
double trappedRadiusOfB(double a, double b) {
    // b_o falls from b2 at r1 to b1 at r2; F(r) = r^2 (r - 3) + a^2 (r + 1) + a b (r - 1) is zero where b_o(r) = b (from (A.5)) and rises with r there
    double lo = trappedRadius1(a), hi = trappedRadius2(a);
    if (lo > hi) std::swap(lo, hi);
    const auto F = [&](double r) { return r * r * (r - 3) + a * a * (r + 1) + a * b * (r - 1); };
    const double flo = F(lo);
    for (int it = 0; it < 200 && hi - lo > 1e-15 * hi; it++) {
        const double mid = 0.5 * (lo + hi);
        if ((F(mid) > 0) == (flo > 0)) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}
double radialR(double a, double b, double q, double r) {
    const double D = r * r - 2 * r + a * a, P = r * r + a * a - a * b;
    return P * P - D * ((b - a) * (b - a) + q);                            // (A.4)
}

namespace {
// the real roots of t^3 + p t + c = 0 (trigonometric form for three real roots, Cardano's for one)
int depressedCubic(double p, double c, double t[3]) {
    if (p == 0) { t[0] = std::cbrt(-c); return 1; }
    const double disc = c * c / 4 + p * p * p / 27;
    if (disc > 0) {
        const double s = std::sqrt(disc);
        t[0] = std::cbrt(-c / 2 + s) + std::cbrt(-c / 2 - s);
        return 1;
    }
    const double m = 2 * std::sqrt(-p / 3), th = std::acos(std::clamp(3 * c / (p * m), -1.0, 1.0)) / 3;
    for (int k = 0; k < 3; k++) t[k] = m * std::cos(th - 2 * kPi * k / 3);
    return 3;
}
bool fromHorizonPos(double a, double rc, double b, double q, double pr) {
    const double r1 = trappedRadius1(a), r2 = trappedRadius2(a);
    const double b1 = trappedB(a, r2), b2 = trappedB(a, r1);
    if (b1 < b && b < b2 && q < trappedQ(a, trappedRadiusOfB(a, b))) return pr > 0;   // (v)(a), (b): no turning point
    // (c): R'(r) = 4 r^3 + 2 (a^2 - b^2 - q) r + 2 ((b - a)^2 + q) = 0  <=>  r^3 + p r + c = 0
    const double K = (b - a) * (b - a) + q, p = (a * a - b * b - q) / 2, c = K / 2;
    double t[3];
    const int n = depressedCubic(p, c, t);
    for (int i = 0; i < n; i++)
        if (t[i] > rc && radialR(a, b, q, t[i]) < 0) return true;           // a turning point above the camera: it is below the barrier
    return false;
}
}  // namespace

bool fromHorizon(double a, double rc, double b, double q, double pr) {
    if (a < 0) return fromHorizonPos(-a, rc, -b, q, pr);
    return fromHorizonPos(a, rc, b, q, pr);
}

bool lookFromHorizon(double a, const Camera& cam, const Vec3& look) {
    const KerrBL ch(a);
    Ray ray;
    if (!ch.launch(cam, look, ray)) return true;
    return fromHorizon(a, cam.r, ray.c[0], ray.c[1], ray.y[3]);
}

Vec3 lookAt(double psi, double chi) {
    const double s = std::sin(psi);
    return {-std::cos(psi), s * std::cos(chi), s * std::sin(chi)};
}

double shadowEdge(double a, const Camera& cam, double chi) {
    double lo = 0, hi = kPi;                                               // lo: horizon, hi: sky
    while (hi - lo > 1e-14) {
        const double mid = 0.5 * (lo + hi);
        if (lookFromHorizon(a, cam, lookAt(mid, chi))) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// GLb
double criticalLambda(double a, double rt) { const double D = rt * rt - 2 * rt + a * a; return a + rt / a * (rt - 2 * D / (rt - 1)); }   // (38)
double criticalEta(double a, double rt) { const double D = rt * rt - 2 * rt + a * a; return rt * rt * rt / (a * a) * (4 * D / ((rt - 1) * (rt - 1)) - rt); }   // (39)
double criticalRadiusMinus(double a) { return 2 * (1 + std::cos(2.0 / 3.0 * std::acos(-a))); }   // (40) with the lower sign: r~- uses arccos(-a/M)
double criticalRadiusPlus(double a) { return 2 * (1 + std::cos(2.0 / 3.0 * std::acos(a))); }
bool criticalScreen(double a, double rt, double thO, double& alpha, double& beta2) {
    const double lam = criticalLambda(a, rt), eta = criticalEta(a, rt), s = std::sin(thO), c = std::cos(thO);
    alpha = -lam / s;                                                      // (55)
    beta2 = eta + a * a * c * c - lam * lam * c * c / (s * s);
    return beta2 >= 0;
}
double lyapunov(double a, double rt) {
    // the constants from J15 (A.5) (= GLb (38), (39), tested; the factored form keeps its digits at small spin)
    const double lam = trappedB(a, rt), eta = trappedQ(a, rt), D = rt * rt - 2 * rt + a * a;
    const double dth = 0.5 * (1 - (eta + lam * lam) / (a * a)), root = std::sqrt(dth * dth + eta / (a * a));   // (11)
    const double um = dth - root, up = -eta / (a * a * um);               // u+ = dth + root written as -(eta / a^2) / u- (u+ u- = -eta / a^2): no cancellation
    const double chi = 1 - D / (rt * (rt - 1) * (rt - 1));                 // (44), M = 1
    const double K = ellKc(1 - up / um);                                   // K(m) with the parameter m = u+/u- (< 0): kc2 = 1 - m
    return 4 * rt * std::sqrt(chi) * K / (a * std::sqrt(-um));             // (43)
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the gas, C75 (A7), (A9)
double discShiftKerr(double r, double b, double a) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    const double Om = 1 / (r * std::sqrt(r) + a), V = (Om - m.omega) * m.varpi / m.alpha;
    return m.alpha * std::sqrt(1 - V * V) / (1 - Om * b);
}

double c75EquatorialLambda(double r, double a) {
    // q = 0 rays from the ring at r with Int dr / R^(1/2) (Mino time, from r to infinity, through a turning point if the ray first goes in) = pi / (lambda^2 - a^2)^(1/2)
    const auto R = [&](double rr, double lam) { return radialR(a, lam, 0, rr); };
    const auto turning = [&](double lam, double& P) -> bool {              // the largest root of R (q = 0) below r, if R < 0 somewhere between the horizon and r
        double lo = horizon(a) + 1e-9, hi = r;
        if (R(lo, lam) >= 0) {                                             // R > 0 at the horizon: a barrier, if any, lies between
            // scan for a sign change (R is a quartic: few)
            const int n = 4000;
            double prev = R(hi, lam), rp = hi;
            for (int i = 1; i <= n; i++) {
                const double rr = hi - (hi - lo) * i / n, v = R(rr, lam);
                if ((v < 0) != (prev < 0)) { double l = rr, h = rp; for (int k = 0; k < 100; k++) { const double mm = 0.5 * (l + h); if (R(mm, lam) < 0) l = mm; else h = mm; } P = h; return true; }
                prev = v; rp = rr;
            }
            return false;
        }
        for (int k = 0; k < 200; k++) { const double mm = 0.5 * (lo + hi); if (R(mm, lam) < 0) lo = mm; else hi = mm; }
        P = hi;
        return true;
    };
    // Int_x^y dr / sqrt(R) with r = 1/u, near a turning point u = uP - s^2 (Gauss-Legendre on many panels)
    const auto mino = [&](double lam, bool inward, double P) {
        static const double xg[8] = {-0.9602898564975363, -0.7966664774136267, -0.5255324099163290, -0.1834346424956498, 0.1834346424956498, 0.5255324099163290, 0.7966664774136267, 0.9602898564975363};
        static const double wg[8] = {0.1012285362903763, 0.2223810344533745, 0.3137066243625401, 0.3626837833783620, 0.3626837833783620, 0.3137066243625401, 0.2223810344533745, 0.1012285362903763};
        const auto f = [&](double u) { return 1 / std::sqrt(std::max(R(1 / u, lam) * u * u * u * u, 1e-300)); };   // dr / sqrt(R) = du / sqrt(R u^4)
        const auto integ = [&](double a0, double b0, const auto& g) {
            const int np = 400; double s = 0;
            for (int i = 0; i < np; i++) { const double x0 = a0 + (b0 - a0) * i / np, x1 = a0 + (b0 - a0) * (i + 1) / np; for (int k = 0; k < 8; k++) s += wg[k] * 0.5 * (x1 - x0) * g(0.5 * (x0 + x1) + 0.5 * (x1 - x0) * xg[k]); }
            return s;
        };
        if (!inward) return integ(0.0, 1 / r, f);
        const double uP = 1 / P;
        const auto g = [&](double s) { return 2 * s * f(uP - s * s); };
        return integ(0.0, std::sqrt(uP - 1 / r), g) + integ(0.0, std::sqrt(uP), g);
    };
    double best = 0;
    const int n = 3000;
    for (int pass = 0; pass < 2; pass++) {
        const bool inward = pass == 1;
        double prevL = 0, prevH = 0;
        bool have = false;
        for (int i = 1; i <= n; i++) {
            const double lam = a + (30.0 - a) * i / n;
            double P = 0;
            const bool tp = turning(lam, P);
            const bool ok = R(r, lam) >= 0 && (inward ? (tp && P < r) : !(tp && P > r));
            if (!ok) { have = false; continue; }
            const double h = mino(lam, inward, P) - kPi / std::sqrt(lam * lam - a * a);
            if (have && (h > 0) != (prevH > 0)) {
                double lo = prevL, hi = lam, hlo = prevH;
                for (int k = 0; k < 60; k++) {
                    const double mm = 0.5 * (lo + hi);
                    double Pm = 0; turning(mm, Pm);
                    const double hm = mino(mm, inward, Pm) - kPi / std::sqrt(mm * mm - a * a);
                    if ((hm > 0) == (hlo > 0)) { lo = mm; hlo = hm; } else hi = mm;
                }
                best = std::max(best, 0.5 * (lo + hi));
            }
            prevL = lam; prevH = h; have = true;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the moving camera, J15 (A.9), (A.10), (A.16)
Vec3 aberrate(const Vec3& N, double beta, const Vec3& B) {
    // the light moves along d' = -N on the camera's axes; boosted to the FIDO along B with speed beta (camera moving at +beta B relative to the FIDO)
    const double Ny = dot(N, B), g = std::sqrt(1 - beta * beta), den = 1 - beta * Ny;
    const Vec3 perp = N - B * Ny;
    return B * ((-Ny + beta) / den) + perp * (-g / den);
}
double cameraShift(double a, const Camera& cam, const Vec3& N, double b) {
    const KerrMetric m = kerrMetric(a, cam.r, cam.theta);
    const double Ny = dot(N, cam.B);
    return std::sqrt(1 - cam.beta * cam.beta) / (1 - cam.beta * Ny) * (1 - b * m.omega) / m.alpha;
}

}  // namespace blackhole
