#include "blackhole/elliptic.h"

#include <algorithm>
#include <cmath>

namespace blackhole {

// Carlson's duplication: each step replaces (x, y, z) by ((x + l) / 4, ...), l = sqrt(xy) + sqrt(yz) + sqrt(zx), which keeps R_F; once the three are within
// a relative 8e-4 of their mean A, the fifth-order series in the differences ends it (relative error below about 1e-17).
double carlsonRF(double x, double y, double z) {
    for (int it = 0; it < 200; it++) {
        const double A = (x + y + z) / 3, dx = (A - x) / A, dy = (A - y) / A, dz = (A - z) / A;
        if (std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz)}) < 8e-4) {
            const double e2 = dx * dy - dz * dz, e3 = dx * dy * dz;
            return (1 + (e2 / 24 - 0.1 - 3.0 / 44 * e3) * e2 + e3 / 14) / std::sqrt(A);
        }
        const double sx = std::sqrt(x), sy = std::sqrt(y), sz = std::sqrt(z), l = sx * sy + sy * sz + sz * sx;
        x = (x + l) / 4; y = (y + l) / 4; z = (z + l) / 4;
    }
    return std::nan("");
}

double ellKc(double kc2) { return carlsonRF(0, kc2, 1); }
double ellK(double k) { return ellKc(1 - k * k); }
double ellFsc(double s, double c, double kc2) { return s * carlsonRF(c * c, c * c + kc2 * s * s, 1); }   // 1 - k^2 s^2 = c^2 + kc2 s^2

double ellFc(double phi, double kc2) {
    const double kPi = 3.14159265358979323846;
    const double n = std::floor(phi / kPi + 0.5), p = phi - n * kPi;   // p in [-pi/2, pi/2]; F(p + n pi) = F(p) + 2 n K
    const double f = ellFsc(std::sin(p), std::cos(p), kc2);
    return n == 0 ? f : f + 2 * n * ellKc(kc2);
}
double ellF(double phi, double k) { return ellFc(phi, 1 - k * k); }

// ---- 0.55.9 (elliptic.h) ----
// R_C in closed form (Carlson 1995 (2.4)-(2.6) give it as the inverse circular / hyperbolic functions; written here from its integral, x >= 0, y > 0): with
// e = 1 - x / y, R_C = atan(sqrt(e / (1 - e))) / sqrt(y e) for 0 < e <= 1 (x < y), atanh(sqrt(-e / (1 - e))) / sqrt(-y e) for e < 0 (x > y); near e = 0 the series
// of both, R_C = (1 + e/6 + 3 e^2/40 + 5 e^3/112 + 35 e^4/1152) / sqrt(y) (|e| < 1e-3: the next term is below 1e-16). Tested against the duplication form.
double carlsonRC(double x, double y) {
    const double e = (y - x) / y;
    if (std::fabs(e) < 1e-3) return (1 + e * (1.0 / 6 + e * (3.0 / 40 + e * (5.0 / 112 + e * 35.0 / 1152)))) / std::sqrt(y);
    if (x < y) return x > 0 ? std::atan(std::sqrt((y - x) / x)) / std::sqrt(y - x) : 1.5707963267948966 / std::sqrt(y);
    const double z = std::sqrt((x - y) / x);                // atanh z = ln((1 + z) / (1 - z)) / 2, 1 - z = (y / x) / (1 + z): no cancellation
    return 0.5 * std::log((1 + z) * (1 + z) * x / y) / std::sqrt(x - y);
}

// the duplication form (the tests' reference for the closed form above)
double carlsonRCdup(double x, double y) {
    for (int it = 0; it < 60; it++) {
        const double A = (x + y + y) / 3, sd = (y - A) / A;
        if (std::fabs(sd) < 1e-3) return (1 + sd * sd * (0.3 + sd * (1.0 / 7 + sd * (0.375 + sd * 9.0 / 22)))) / std::sqrt(A);
        const double l = 2 * std::sqrt(x) * std::sqrt(y) + y;
        x = (x + l) / 4; y = (y + l) / 4;
    }
    return std::nan("");
}

double carlsonRJ(double x, double y, double z, double p) {
    double sum = 0, fac = 1;
    for (int it = 0; it < 60; it++) {
        const double A = (x + y + z + p + p) / 5, dx = (A - x) / A, dy = (A - y) / A, dz = (A - z) / A, dp = (A - p) / A;
        if (std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz), std::fabs(dp)}) < 1e-3) {
            const double C1 = 3.0 / 14, C2 = 1.0 / 3, C3 = 3.0 / 22, C4 = 3.0 / 26, C5 = 0.75 * C3, C6 = 1.5 * C4, C7 = 0.5 * C2, C8 = C3 + C3;
            const double ea = dx * (dy + dz) + dy * dz, eb = dx * dy * dz, ec = dp * dp, ed = ea - 3 * ec, ee = eb + 2 * dp * (ea - ec);
            return 3 * sum + fac * (1 + ed * (-C1 + C5 * ed - C6 * ee) + eb * (C7 + dp * (-C8 + dp * C4)) + dp * ea * (C2 - dp * C3) - C2 * dp * ec) / (A * std::sqrt(A));
        }
        const double sx = std::sqrt(x), sy = std::sqrt(y), sz = std::sqrt(z), l = sx * sy + sy * sz + sz * sx;
        const double al = p * (sx + sy + sz) + sx * sy * sz, be = p * (p + l) * (p + l);
        sum += fac * carlsonRC(al * al, be);
        fac *= 0.25;
        x = (x + l) / 4; y = (y + l) / 4; z = (z + l) / 4; p = (p + l) / 4;
    }
    return std::nan("");
}

namespace {
// Pi(n; phi | m) for 0 <= phi <= pi/2 (s >= 0, c >= 0), n < 1 or 1 - n s^2 > 0: Carlson's reduction
double piCarlson(double n, double s, double c, double kc2) {
    const double c2 = c * c, d2 = c2 + kc2 * s * s;
    return s * carlsonRF(c2, d2, 1) + n / 3 * s * s * s * carlsonRJ(c2, d2, 1, 1 - n * s * s);
}
}  // namespace

double ellPic(double n, double kc2) { return piCarlson(n, 1, 0, kc2); }

double ellFang(double s, double c, double kc2) {
    const double sg = s < 0 ? -1 : 1, f = ellFsc(std::fabs(s), std::fabs(c), kc2);
    return sg * (c >= 0 ? f : 2 * ellKc(kc2) - f);
}

double ellPiang(double n, double s, double c, double kc2) {
    const double sg = s < 0 ? -1 : 1, as = std::fabs(s), ac = std::fabs(c);
    if (n < 1) {
        const double p = piCarlson(n, as, ac, kc2);
        return sg * (c >= 0 ? p : 2 * ellPic(n, kc2) - p);
    }
    const double m = 1 - kc2, np = m / n, P = std::sqrt((n - 1) * (1 - np)), d = std::sqrt(c * c + kc2 * s * s);
    const double pole = (1 - n * s * s) * (1 - np * s * s);   // (d c)^2 - (P s)^2
    if (c >= 0 && pole > 0) return sg * piCarlson(n, as, ac, kc2);
    // G (elliptic.h) at |phi| or pi - |phi|: the log term with the signed c; the larger of d c +- P s computed directly, the other as pole / it
    const double plus = d * c + P * as, minus = d * c - P * as;
    const double L = std::fabs(plus) >= std::fabs(minus) ? std::log(std::fabs(plus * plus / pole)) : std::log(std::fabs(pole / (minus * minus)));
    const double base = ellFsc(as, ac, kc2) - piCarlson(np, as, ac, kc2);
    const double g = c >= 0 ? base : 2 * (ellKc(kc2) - ellPic(np, kc2)) - base;
    return sg * (g + L / (2 * P));
}

namespace {
// 0 <= m < 1. A&S 16.4: a0 = 1, b0 = (1 - m)^(1/2), c0 = m^(1/2); a_n = (a + b)/2, b_n = (a b)^(1/2), c_n = (a - b)/2 until c_N is negligible;
// phi_N = 2^N a_N u, phi_(n-1) = (phi_n + arcsin(c_n sin(phi_n) / a_n)) / 2
double amAGM(double u, double kc2) {
    if (kc2 >= 1) return u;
    double a[16], c[16], b = std::sqrt(kc2);
    a[0] = 1; c[0] = std::sqrt(1 - kc2);
    int N = 0;
    while (N < 15 && std::fabs(c[N]) > 1e-17 * a[N]) {
        a[N + 1] = (a[N] + b) / 2; c[N + 1] = (a[N] - b) / 2; b = std::sqrt(a[N] * b); N++;
    }
    double phi = std::ldexp(a[N] * u, N);
    for (int j = N; j > 0; j--) phi = (phi + std::asin(c[j] * std::sin(phi) / a[j])) / 2;
    return phi;
}
}  // namespace

double jacobiAm(double u, double kc2) {
    if (kc2 <= 1) return amAGM(u, kc2);
    // m < 0 (elliptic.h): am(u | m) = pi/2 - am(K(mu) - u (1 - m)^(1/2) | mu), mu = -m / (1 - m), 1 - mu = 1 / (1 - m) = 1 / kc2
    return 1.5707963267948966 - amAGM(ellKc(1 / kc2) - u * std::sqrt(kc2), 1 / kc2);
}

}  // namespace blackhole
