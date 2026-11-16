#include "blackhole/mino.h"

#include <algorithm>
#include <cmath>

#include "blackhole/kerr.h"

namespace blackhole {

namespace {
// the ray's constants, worked out once (mino.h)
struct Eq {
    double a, b, q, A, K, a2, c3;
    Eq(double a_, double b_, double q_) : a(a_), b(b_), q(q_), A(a_ * a_ - a_ * b_), K((b_ - a_) * (b_ - a_) + q_), a2(a_ * a_), c3(q_ + b_ * b_ - a_ * a_) {}
    void rhs(const double* y, double* dy) const {
        const double u = y[0], m = y[2], u2 = u * u, Au2 = 1 + A * u2;
        dy[0] = y[1];
        dy[1] = 2 * A * u * Au2 - K * u * (1 - 3 * u + 2 * a2 * u2);
        dy[2] = y[3];
        dy[3] = -m * (c3 + 2 * a2 * m * m);
        dy[4] = -(a * Au2 / (1 - 2 * u + a2 * u2) - a + b / (1 - m * m));
    }
};

// Dormand & Prince (1980) RK5(4)7M: the tableau as in geodesic.cpp (copied from there)
constexpr double a21 = 1.0 / 5;
constexpr double a31 = 3.0 / 40, a32 = 9.0 / 40;
constexpr double a41 = 44.0 / 45, a42 = -56.0 / 15, a43 = 32.0 / 9;
constexpr double a51 = 19372.0 / 6561, a52 = -25360.0 / 2187, a53 = 64448.0 / 6561, a54 = -212.0 / 729;
constexpr double a61 = 9017.0 / 3168, a62 = -355.0 / 33, a63 = 46732.0 / 5247, a64 = 49.0 / 176, a65 = -5103.0 / 18656;
constexpr double b1 = 35.0 / 384, b3 = 500.0 / 1113, b4 = 125.0 / 192, b5 = -2187.0 / 6784, b6 = 11.0 / 84;
constexpr double e1 = 71.0 / 57600, e3 = -71.0 / 16695, e4 = 71.0 / 1920, e5 = -17253.0 / 339200, e6 = 22.0 / 525, e7 = -1.0 / 40;
constexpr int N = 5;

double step(const Eq& f, const double* y, const double* k1, double h, double* yn, double* k7, double rtol, double atol) {
    double k2[N], k3[N], k4[N], k5[N], k6[N], t[N];
    for (int i = 0; i < N; i++) t[i] = y[i] + h * a21 * k1[i];
    f.rhs(t, k2);
    for (int i = 0; i < N; i++) t[i] = y[i] + h * (a31 * k1[i] + a32 * k2[i]);
    f.rhs(t, k3);
    for (int i = 0; i < N; i++) t[i] = y[i] + h * (a41 * k1[i] + a42 * k2[i] + a43 * k3[i]);
    f.rhs(t, k4);
    for (int i = 0; i < N; i++) t[i] = y[i] + h * (a51 * k1[i] + a52 * k2[i] + a53 * k3[i] + a54 * k4[i]);
    f.rhs(t, k5);
    for (int i = 0; i < N; i++) t[i] = y[i] + h * (a61 * k1[i] + a62 * k2[i] + a63 * k3[i] + a64 * k4[i] + a65 * k5[i]);
    f.rhs(t, k6);
    for (int i = 0; i < N; i++) yn[i] = y[i] + h * (b1 * k1[i] + b3 * k3[i] + b4 * k4[i] + b5 * k5[i] + b6 * k6[i]);
    f.rhs(yn, k7);
    double err = 0;
    for (int i = 0; i < N; i++) {
        const double e = h * (e1 * k1[i] + e3 * k3[i] + e4 * k4[i] + e5 * k5[i] + e6 * k6[i] + e7 * k7[i]);
        const double sc = atol + rtol * std::max(std::fabs(y[i]), std::fabs(yn[i]));
        err = std::max(err, std::fabs(e) / sc);
    }
    return std::isfinite(err) ? err : 1e30;
}

// the step from y whose end has component c at 0 (Newton on the step's length from the secant guess, each trial one full step; the rate is component c + 1)
void zeroOf(const Eq& f, int c, const double* y, const double* k1, double h, double y1c, double* yc, double* kc, double rtol, double atol) {
    double hc = h * y[c] / (y[c] - y1c);
    for (int it = 0; it < 8; it++) {
        step(f, y, k1, hc, yc, kc, rtol, atol);
        const double rate = yc[c + 1];
        if (!(rate != 0)) break;
        const double dh = -yc[c] / rate;
        hc += dh;
        if (std::fabs(dh) <= 1e-14 * std::fabs(h)) break;
    }
    step(f, y, k1, hc, yc, kc, rtol, atol);
}
}  // namespace

void minoRhs(double a, double b, double q, const double* y, double* dy) { Eq(a, b, q).rhs(y, dy); }

void minoNull(double a, double b, double q, const double* y, double& eu, double& emu) {
    const double u = y[0], m = y[2], A = a * a - a * b, K = (b - a) * (b - a) + q, c3 = q + b * b - a * a;
    const double t1 = (1 + A * u * u) * (1 + A * u * u), t2 = K * u * u * std::fabs(1 - 2 * u + a * a * u * u);
    eu = (y[1] * y[1] - (t1 - K * u * u * (1 - 2 * u + a * a * u * u))) / (y[1] * y[1] + t1 + t2);
    const double s1 = std::fabs(q) + std::fabs(c3) * m * m + a * a * m * m * m * m;
    emu = (y[3] * y[3] - (q - c3 * m * m - a * a * m * m * m * m)) / (y[3] * y[3] + s1 + 1e-300);
}

MinoEnd traceMino(double a, const Camera& cam, const Vec3& look, const TraceOptions& o, std::vector<double>* uCross) {
    MinoEnd out;
    const KerrBL ch(a);
    Ray ray;
    if (!ch.launch(cam, look, ray)) return out;
    const double r = ray.y[0], th = ray.y[1], D = r * r - 2 * r + a * a;
    out.b = ray.c[0]; out.q = ray.c[1];
    const Eq f(a, out.b, out.q);
    {   // the smallest sin^2 theta on the ray: Mu(mu) = 0 at mu^2 = 2 q / (c3 + (c3^2 + 4 a^2 q)^(1/2))
        const double m2 = f.q > 0 ? 2 * f.q / (f.c3 + std::sqrt(f.c3 * f.c3 + 4 * f.a2 * f.q)) : 0;
        if (1 - m2 < kMinoPole) { out.nearPole = true; return out; }
    }
    double y[N] = {1 / r, D * ray.y[3] / (r * r), std::cos(th), std::sin(th) * ray.y[4], ray.y[2]}, k1[N], yn[N], k7[N], yc[N], kc[N];
    f.rhs(y, k1);
    const double uH = 1 / (horizon(a) + 1e-3);
    double h = 0.01 * y[0];
    int found = 0;
    const auto finish = [&](RayEnd::Kind k, const double* s, const double* ds) {
        out.kind = k;
        std::copy(s, s + N, out.y); std::copy(ds, ds + N, out.dy);
    };
    for (int s = 0; s < o.maxSteps; s++) {
        const double err = step(f, y, k1, h, yn, k7, o.rtol, o.atol);
        if (err > 1) { out.rejected++; h *= std::max(0.2, 0.9 * std::pow(err, -0.2)); continue; }
        if (uCross && found < o.maxCross && (y[2] > 0) != (yn[2] > 0)) {
            zeroOf(f, 2, y, k1, h, yn[2], yc, kc, o.rtol, o.atol);
            if (yc[0] > 0) { uCross->push_back(yc[0]); found++; }   // (a crossing beyond u = 0 would lie past infinity: not one)
        }
        out.steps++;
        if (yn[0] <= 0 && yn[1] < 0) {                                  // out at infinity inside this step: the place where u = 0
            zeroOf(f, 0, y, k1, h, yn[0], yc, kc, o.rtol, o.atol);
            const double m = std::clamp(yc[2], -1.0, 1.0), sn = std::sqrt(1 - m * m);
            out.dir = Vec3{sn * std::cos(yc[4]), sn * std::sin(yc[4]), m};
            out.pz = yc[1] != 0 ? yc[3] / yc[1] : 0;
            finish(RayEnd::Sky, yc, kc);
            return out;
        }
        std::copy(yn, yn + N, y);
        std::copy(k7, k7 + N, k1);
        if (y[0] > uH && y[1] > 0) { finish(RayEnd::Horizon, y, k1); return out; }
        h *= err <= 0 ? 5.0 : std::clamp(0.9 * std::pow(err, -0.2), 0.2, 5.0);
    }
    return out;
}

}  // namespace blackhole
