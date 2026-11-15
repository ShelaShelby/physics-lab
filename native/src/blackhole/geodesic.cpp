#include "blackhole/geodesic.h"

#include <algorithm>
#include <cmath>

#include "blackhole/kerr.h"
#include "blackhole/spin.h"

namespace blackhole {

bool KerrBL::launch(const Camera& cam, const Vec3& look, Ray& ray) const {
    if (!(cam.r > horizon(a_))) return false;
    const KerrMetric m = kerrMetric(a_, cam.r, cam.theta);
    const Vec3 nF = cam.beta > 0 ? aberrate(look, cam.beta, cam.B) : look * -1.0;   // n_F = -N for a hovering camera ((A.9), beta = 0)
    const double nr = nF.x, nth = nF.y, nph = nF.z;
    const double EF = 1 / (m.alpha + m.omega * m.varpi * nph);             // (A.11)
    const double rho = std::sqrt(m.rho2);
    ray.n = 5;
    ray.y[0] = cam.r; ray.y[1] = cam.theta; ray.y[2] = cam.phi;
    ray.y[3] = EF * rho / std::sqrt(m.Delta) * nr;
    ray.y[4] = EF * rho * nth;
    const double b = EF * m.varpi * nph;                                   // (A.12)
    ray.c[0] = b;
    ray.c[1] = carterQ(b, ray.y);
    ray.c[2] = std::max(rFar_, 1e4 * cam.r);
    return true;
}

double KerrBL::plane(const double* y) const { return std::cos(y[1]); }
double KerrBL::planeRate(const double* y, const double* dy) const { return -std::sin(y[1]) * dy[1]; }

double KerrBL::carterQ(double b, const double* y) const {
    const double c = std::cos(y[1]), s = std::sin(y[1]);
    return y[4] * y[4] + c * c * (b * b / (s * s) - a_ * a_);              // (A.12)
}

double KerrBL::hamiltonian(double b, double q, const double* y) const {
    const double r = y[0], c = std::cos(y[1]), s = std::sin(y[1]), a = a_;
    const double rho2 = r * r + a * a * c * c, D = r * r - 2 * r + a * a;
    const double P = r * r + a * a - a * b, R = P * P - D * ((b - a) * (b - a) + q);   // (A.4)
    const double Th = q - c * c * (b * b / (s * s) - a * a);
    return (-D * y[3] * y[3] - y[4] * y[4] + R / D + Th) / (2 * rho2);    // the bracket of (A.15)
}

double KerrBL::nullCondition(const Ray& ray, const double* y) const {
    const double b = ray.c[0], q = ray.c[1], r = y[0], c = std::cos(y[1]), s = std::sin(y[1]), a = a_;
    const double D = r * r - 2 * r + a * a, P = r * r + a * a - a * b, R = P * P - D * ((b - a) * (b - a) + q);
    const double Th = q - c * c * (b * b / (s * s) - a * a);
    const double t1 = D * y[3] * y[3], t2 = y[4] * y[4], t3 = R / D;
    // relative to the size of the terms before they cancel: R / Delta = P^2 / Delta - ((b - a)^2 + q)
    const double scale = std::fabs(t1) + std::fabs(t2) + P * P / std::fabs(D) + std::fabs((b - a) * (b - a) + q) + std::fabs(Th);
    return (-t1 - t2 + t3 + Th) / scale;
}

void KerrBL::rhs(const Ray& ray, const double* y, double* dy) const {
    const double b = ray.c[0], q = ray.c[1], a = a_;
    const double r = y[0], c = std::cos(y[1]), s = std::sin(y[1]), pr = y[3], pt = y[4];
    const double rho2 = r * r + a * a * c * c, D = r * r - 2 * r + a * a, dD = 2 * r - 2;
    const double P = r * r + a * a - a * b, Kc = (b - a) * (b - a) + q, R = P * P - D * Kc, dR = 4 * r * P - dD * Kc;
    const double Th = q - c * c * (b * b / (s * s) - a * a), dTh = 2 * c * (b * b / (s * s * s) - a * a * s);
    const double N = -D * pr * pr - pt * pt + R / D + Th;                  // H = N / (2 rho^2)
    const double dNr = -dD * pr * pr + (dR * D - R * dD) / (D * D), drho2t = -2 * a * a * c * s;
    dy[0] = D * pr / rho2;
    dy[1] = pt / rho2;
    dy[2] = (a * P / D + (b - a) + b * c * c / (s * s)) / rho2;           // -d/db [(R + Delta Theta) / (2 Delta rho^2)]
    dy[3] = dNr / (2 * rho2) - N * 2 * r / (2 * rho2 * rho2);
    dy[4] = dTh / (2 * rho2) - N * drho2t / (2 * rho2 * rho2);
}

int KerrBL::ended(const Ray& ray, const double* y, const double* dy, double zs) const {
    const double drdz = dy[0] * zs;
    if (y[0] >= ray.c[2] && drdz > 0) return 1;
    if (y[0] < horizon(a_) + 1e-3 && drdz < 0) return 2;
    return 0;
}

Vec3 KerrBL::farDirection(const Ray&, const double* y, const double* dy, double zs) const {
    const double r = y[0], ct = std::cos(y[1]), st = std::sin(y[1]), cp = std::cos(y[2]), sp = std::sin(y[2]);
    const Vec3 er{st * cp, st * sp, ct}, et{ct * cp, ct * sp, -st}, ep{-sp, cp, 0};
    const Vec3 v = er * dy[0] + et * (r * dy[1]) + ep * (r * st * dy[2]);
    const double L = length(v);
    return v * (zs / L);                       // the direction the light came from = the direction of its past motion
}

// Dormand & Prince (1980) RK5(4)7M, the embedded pair used by most adaptive integrators (its coefficients are the published tableau)
namespace {
constexpr double a21 = 1.0 / 5;
constexpr double a31 = 3.0 / 40, a32 = 9.0 / 40;
constexpr double a41 = 44.0 / 45, a42 = -56.0 / 15, a43 = 32.0 / 9;
constexpr double a51 = 19372.0 / 6561, a52 = -25360.0 / 2187, a53 = 64448.0 / 6561, a54 = -212.0 / 729;
constexpr double a61 = 9017.0 / 3168, a62 = -355.0 / 33, a63 = 46732.0 / 5247, a64 = 49.0 / 176, a65 = -5103.0 / 18656;
constexpr double b1 = 35.0 / 384, b3 = 500.0 / 1113, b4 = 125.0 / 192, b5 = -2187.0 / 6784, b6 = 11.0 / 84;
constexpr double e1 = 71.0 / 57600, e3 = -71.0 / 16695, e4 = 71.0 / 1920, e5 = -17253.0 / 339200, e6 = 22.0 / 525, e7 = -1.0 / 40;

// one trial step: y -> yn, with the error estimate; k1 = f(y) given, k7 = f(yn) returned (first same as last)
double dpStep(const Chart& ch, const Ray& ray, const double* y, const double* k1, double h, double* yn, double* k7, const TraceOptions& o) {
    const int n = ray.n;
    double k2[kMaxState] = {}, k3[kMaxState] = {}, k4[kMaxState] = {}, k5[kMaxState] = {}, k6[kMaxState] = {}, t[kMaxState] = {};
    for (int i = 0; i < n; i++) t[i] = y[i] + h * a21 * k1[i];
    ch.rhs(ray, t, k2);
    for (int i = 0; i < n; i++) t[i] = y[i] + h * (a31 * k1[i] + a32 * k2[i]);
    ch.rhs(ray, t, k3);
    for (int i = 0; i < n; i++) t[i] = y[i] + h * (a41 * k1[i] + a42 * k2[i] + a43 * k3[i]);
    ch.rhs(ray, t, k4);
    for (int i = 0; i < n; i++) t[i] = y[i] + h * (a51 * k1[i] + a52 * k2[i] + a53 * k3[i] + a54 * k4[i]);
    ch.rhs(ray, t, k5);
    for (int i = 0; i < n; i++) t[i] = y[i] + h * (a61 * k1[i] + a62 * k2[i] + a63 * k3[i] + a64 * k4[i] + a65 * k5[i]);
    ch.rhs(ray, t, k6);
    for (int i = 0; i < n; i++) yn[i] = y[i] + h * (b1 * k1[i] + b3 * k3[i] + b4 * k4[i] + b5 * k5[i] + b6 * k6[i]);
    ch.rhs(ray, yn, k7);
    double err = 0;
    for (int i = 0; i < n; i++) {
        const double e = h * (e1 * k1[i] + e3 * k3[i] + e4 * k4[i] + e5 * k5[i] + e6 * k6[i] + e7 * k7[i]);
        const double sc = o.atol + o.rtol * std::max(std::fabs(y[i]), std::fabs(yn[i]));
        err = std::max(err, std::fabs(e) / sc);
    }
    return std::isfinite(err) ? err : 1e30;
}
double grow(double err) { return err <= 0 ? 5.0 : std::clamp(0.9 * std::pow(err, -0.2), 0.2, 5.0); }
}  // namespace

RayEnd trace(const Chart& ch, const Camera& cam, const Vec3& look, const TraceOptions& o, std::vector<double>* path) {
    RayEnd out;
    Ray ray;
    if (!ch.launch(cam, look, ray)) return out;
    const double zs = -1;                                      // backward in time
    double y[kMaxState], k1[kMaxState], yn[kMaxState], k7[kMaxState];
    std::copy(ray.y, ray.y + ray.n, y);
    ch.rhs(ray, y, k1);
    double h = zs * 0.01 * cam.r;
    int found = 0;
    if (path) path->insert(path->end(), y, y + ray.n);
    for (int s = 0; s < o.maxSteps; s++) {
        const double err = dpStep(ch, ray, y, k1, h, yn, k7, o);
        if (err > 1) { h *= std::max(0.2, 0.9 * std::pow(err, -0.2)); continue; }
        if (o.cross && found < o.maxCross && (ch.plane(y) > 0) != (ch.plane(yn) > 0)) {
            // the plane is crossed inside this step: Newton on the step length from y (each trial one full step of the same pair), from the secant guess
            const double p0 = ch.plane(y), p1 = ch.plane(yn);
            double hc = h * p0 / (p0 - p1), yc[kMaxState], kc[kMaxState];
            for (int it = 0; it < 8; it++) {
                dpStep(ch, ray, y, k1, hc, yc, kc, o);
                const double pc = ch.plane(yc), rate = ch.planeRate(yc, kc);
                if (!(rate != 0)) break;
                const double dh = -pc / rate;
                hc += dh;
                if (std::fabs(dh) <= 1e-14 * std::fabs(h)) break;
            }
            dpStep(ch, ray, y, k1, hc, yc, kc, o);
            o.cross->insert(o.cross->end(), yc, yc + ray.n);
            found++;
        }
        std::copy(yn, yn + ray.n, y);
        std::copy(k7, k7 + ray.n, k1);
        out.steps++;
        if (path) path->insert(path->end(), y, y + ray.n);
        const int e = ch.ended(ray, y, k1, zs);
        if (e) { std::copy(y, y + ray.n, out.y); std::copy(k1, k1 + ray.n, out.dy); }
        if (e == 1) { out.kind = RayEnd::Sky; out.dir = ch.farDirection(ray, y, k1, zs); return out; }
        if (e == 2) { out.kind = RayEnd::Horizon; return out; }
        h *= grow(err);
    }
    return out;
}

int integrateSpan(const Chart& ch, Ray& ray, double span, const TraceOptions& o) {
    double y[kMaxState], k1[kMaxState], yn[kMaxState], k7[kMaxState];
    std::copy(ray.y, ray.y + ray.n, y);
    ch.rhs(ray, y, k1);
    double done = 0, h = span / 1000;
    int steps = 0;
    while (std::fabs(done) < std::fabs(span) && steps < o.maxSteps) {
        if (std::fabs(done + h) > std::fabs(span)) h = span - done;
        const double err = dpStep(ch, ray, y, k1, h, yn, k7, o);
        if (err > 1) { h *= std::max(0.2, 0.9 * std::pow(err, -0.2)); continue; }
        std::copy(yn, yn + ray.n, y);
        std::copy(k7, k7 + ray.n, k1);
        done += h;
        steps++;
        h *= grow(err);
    }
    std::copy(y, y + ray.n, ray.y);
    return steps;
}

}  // namespace blackhole
