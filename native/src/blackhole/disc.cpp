#include "blackhole/disc.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include "blackhole/kerr.h"
#include "blackhole/presets.h"
#include "blackhole/schwarzschild.h"
#include "orbit/sun_facts.h"
#include "orbit/units.h"
#include "render/cie1931.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
// The SI defining constants (exact since 2019; CODATA 2018 and 2022 list them as exact): the same as tools/star_colour.py
constexpr double kH = 6.62607015e-34, kK = 1.380649e-23, kC = 299792458.0;
}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the gas
double orbitEnergy(double r, double a) {
    const double s = std::sqrt(r);
    return (r * s - 2 * s + a) / (std::pow(r, 0.75) * std::sqrt(r * s - 3 * s + 2 * a));       // BPT (2.12), M = 1, signed a = the upper sign
}
double orbitAngMom(double r, double a) {
    const double s = std::sqrt(r);
    return (r * r - 2 * a * s + a * a) / (std::pow(r, 0.75) * std::sqrt(r * s - 3 * s + 2 * a));   // BPT (2.13)
}
double orbitOmega(double r, double a) { return 1 / (r * std::sqrt(r) + a); }                    // BPT (2.16)
double discInner(double a) { return iscoPageThorne(a); }
double discEfficiency(double a) { return 1 - orbitEnergy(discInner(a), a); }

double ptF(double r, double a) {
    const double x0 = std::sqrt(discInner(a));
    const double x = std::sqrt(r);
    if (!(x > x0)) return 0;
    const double th = std::acos(std::clamp(a, -1.0, 1.0)) / 3;
    const double xs[3] = {2 * std::cos(th - kPi / 3), 2 * std::cos(th + kPi / 3), -2 * std::cos(th)};   // PT p. 502
    double br = x - x0 - 1.5 * a * std::log(x / x0);
    for (int i = 0; i < 3; i++) {
        const double xi = xs[i], xj = xs[(i + 1) % 3], xk = xs[(i + 2) % 3];
        if (xi == 0) continue;                                                     // (the limit: see the header)
        br -= 3 * (xi - a) * (xi - a) / (xi * (xi - xj) * (xi - xk)) * std::log((x - xi) / (x0 - xi));
    }
    return 1.5 / (x * x * (x * x * x - 3 * x + 2 * a)) * br;                       // PT (15n)
}
double discFlux(double r, double a) { return ptF(r, a) / (4 * kPi * r); }          // PT (11b), (15d)

double luminetFlux(double r, double c) {
    if (!(r > 6)) return 0;
    const double s = std::sqrt(r), s3 = std::sqrt(3.0), s6 = std::sqrt(6.0);
    const double lg = std::log((s + s3) * (s6 - s3) / ((s - s3) * (s6 + s3)));
    return 3 / (8 * kPi) / ((r - 3) * std::pow(r, 2.5)) * (s - s6 + c * lg);      // L79 (15)
}

double discLuminosity(double a, double rOut) {
    // Int 4 pi r F E dr = Int f E dr from r_ms to rOut; with r = r_ms / w^2 (w from (r_ms / rOut)^(1/2) to 1, x = x0 / w) the integrand
    // 2 r_ms f E / w^3 is smooth and finite at w -> 0 (f ~ 3 / (2 r^2) far out)
    const double r0 = discInner(a), w0 = rOut < 1e300 ? std::sqrt(r0 / rOut) : 0;
    static const double gx[5] = {-0.9061798459386640, -0.5384693101056831, 0, 0.5384693101056831, 0.9061798459386640};
    static const double gw[5] = {0.2369268850561891, 0.4786286704993665, 0.5688888888888889, 0.4786286704993665, 0.2369268850561891};
    const int n = 2000;
    double sum = 0;
    for (int p = 0; p < n; p++) {
        const double lo = w0 + (1 - w0) * p / n, hi = w0 + (1 - w0) * (p + 1) / n, mid = (lo + hi) / 2, half = (hi - lo) / 2;
        for (int q = 0; q < 5; q++) {
            const double w = mid + half * gx[q];
            const double r = r0 / (w * w);
            sum += gw[q] * half * ptF(r, a) * orbitEnergy(r, a) * 2 * r0 / (w * w * w);
        }
    }
    return sum;
}

double fluxMaxRadius(double a) {
    double lo = discInner(a), hi = 60;
    const double gr = (std::sqrt(5.0) - 1) / 2;
    double c = hi - gr * (hi - lo), d = lo + gr * (hi - lo);
    while (hi - lo > 1e-10) {
        if (discFlux(c, a) > discFlux(d, a)) hi = d; else lo = c;
        c = hi - gr * (hi - lo); d = lo + gr * (hi - lo);
    }
    return (lo + hi) / 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// in numbers
double stefanBoltzmann() { return 2 * std::pow(kPi, 5) * std::pow(kK, 4) / (15 * kH * kH * kH * kC * kC); }
double eddingtonLuminosity(double m) { return 4 * kPi * units::kGMSun * m * kC / kKappaEs; }      // YN footnote 1
double eddingtonRate(double m) { return 10 * eddingtonLuminosity(m) / (kC * kC); }
double DiscPhys::mdot() const { return lEdd * eddingtonLuminosity(massSun) / (discEfficiency(aSigned) * kC * kC); }
double DiscPhys::fluxScale() const { const double rg = gravRadiusM(massSun); return mdot() * kC * kC / (rg * rg); }
double DiscPhys::flux(double r) const { return fluxScale() * discFlux(r, aSigned); }
double DiscPhys::temperature(double r) const { return std::pow(std::max(0.0, flux(r)) / stefanBoltzmann(), 0.25); }
double DiscPhys::hottest(double* where) const {
    const double r = fluxMaxRadius(aSigned);
    if (where) *where = r;
    return temperature(r);
}
double afFluxCoefficient() {
    const double gm = units::kGMSun, c4 = kC * kC * kC * kC;
    return 3 * eddingtonLuminosity(1) * c4 / (8 * kPi * gm * gm) * 1e3;           // W/m^2 -> erg cm^-2 s^-1
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the light
std::array<double, 3> blackBodyXYZ(double T) {
    std::array<double, 3> s{0, 0, 0};
    if (!(T > 0)) return s;
    for (int i = 0; i < cie::kCmfN; i++) {
        const double l = (cie::kCmfNm0 + i * cie::kCmfStepNm) * 1e-9;
        const double p = 2 * kH * kC * kC / std::pow(l, 5) / std::expm1(kH * kC / (l * kK * T));   // Planck, W m^-2 sr^-1 m^-1
        for (int k = 0; k < 3; k++) s[k] += p * cie::kCmf[i][k] * cie::kCmfStepNm * 1e-9;
    }
    return s;
}
double sunTeff() { return sunfacts::kSunTeffK; }
static double sunY() { static const double y = blackBodyXYZ(sunfacts::kSunTeffK)[1]; return y; }
double visibleVsSun(double T) { return blackBodyXYZ(T)[1] / sunY(); }
std::array<double, 3> blackBodyLinear(double T) {
    const std::array<double, 3> X = blackBodyXYZ(T);
    const cie::V3 l = cie::linearSrgb(cie::V3{X[0], X[1], X[2]});
    const double ys = sunY();
    return {std::max(0.0, l[0]) / ys, std::max(0.0, l[1]) / ys, std::max(0.0, l[2]) / ys};      // clipped at 0 as tools/star_colour.py
}

namespace {
struct LightTable { static constexpr int N = 4096; double l0, dl; std::vector<double> lnY, c; };
const LightTable& lightTable() {
    static const LightTable L = [] {
        LightTable t;
        t.l0 = std::log10(kTmin); t.dl = (std::log10(kTmax) - t.l0) / (LightTable::N - 1);
        t.lnY.resize(LightTable::N); t.c.resize(3 * LightTable::N);
        for (int i = 0; i < LightTable::N; i++) {
            const double T = std::pow(10.0, t.l0 + i * t.dl);
            const double Y = std::max(visibleVsSun(T), 1e-300);
            t.lnY[i] = std::log(Y);
            const auto l = blackBodyLinear(T);
            for (int k = 0; k < 3; k++) t.c[3 * i + k] = l[k] / Y;            // (the channels over Y change slowly with T: interpolated linearly)
        }
        return t;
    }();
    return L;
}
inline bool lightAt(double T, int& i, double& f) {
    const LightTable& L = lightTable();
    if (!(T >= kTmin)) return false;
    const double q = std::min((std::log10(T) - L.l0) / L.dl, LightTable::N - 1.000001);
    i = static_cast<int>(q); f = q - i;
    return true;
}
}  // namespace
double visibleVsSunFast(double T) {
    int i; double f;
    if (!lightAt(T, i, f)) return 0;
    const LightTable& L = lightTable();
    return std::exp(L.lnY[i] + f * (L.lnY[i + 1] - L.lnY[i]));
}
std::array<double, 3> blackBodyLinearFast(double T) {
    int i; double f;
    if (!lightAt(T, i, f)) return {0, 0, 0};
    const LightTable& L = lightTable();
    std::array<double, 3> r;
    const double Y = std::exp(L.lnY[i] + f * (L.lnY[i + 1] - L.lnY[i]));
    for (int k = 0; k < 3; k++) r[k] = Y * (L.c[3 * i + k] + f * (L.c[3 * i + 3 + k] - L.c[3 * i + k]));
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// rays: Binet's equation u'' = 3u^2 - u (L79 (3)) by Dormand-Prince 5(4)
namespace {
struct Y2 { double u, w; };
inline Y2 rhs(const Y2& y) { return {y.w, 3 * y.u * y.u - y.u}; }
// one step of size h; err: the embedded error estimate, scaled
Y2 dpStep(const Y2& y, double h, double* err) {
    const Y2 k1 = rhs(y);
    const Y2 k2 = rhs({y.u + h * (k1.u / 5), y.w + h * (k1.w / 5)});
    const Y2 k3 = rhs({y.u + h * (3 * k1.u / 40 + 9 * k2.u / 40), y.w + h * (3 * k1.w / 40 + 9 * k2.w / 40)});
    const Y2 k4 = rhs({y.u + h * (44 * k1.u / 45 - 56 * k2.u / 15 + 32 * k3.u / 9), y.w + h * (44 * k1.w / 45 - 56 * k2.w / 15 + 32 * k3.w / 9)});
    const Y2 k5 = rhs({y.u + h * (19372 * k1.u / 6561 - 25360 * k2.u / 2187 + 64448 * k3.u / 6561 - 212 * k4.u / 729),
                       y.w + h * (19372 * k1.w / 6561 - 25360 * k2.w / 2187 + 64448 * k3.w / 6561 - 212 * k4.w / 729)});
    const Y2 k6 = rhs({y.u + h * (9017 * k1.u / 3168 - 355 * k2.u / 33 + 46732 * k3.u / 5247 + 49 * k4.u / 176 - 5103 * k5.u / 18656),
                       y.w + h * (9017 * k1.w / 3168 - 355 * k2.w / 33 + 46732 * k3.w / 5247 + 49 * k4.w / 176 - 5103 * k5.w / 18656)});
    const Y2 n{y.u + h * (35 * k1.u / 384 + 500 * k3.u / 1113 + 125 * k4.u / 192 - 2187 * k5.u / 6784 + 11 * k6.u / 84),
               y.w + h * (35 * k1.w / 384 + 500 * k3.w / 1113 + 125 * k4.w / 192 - 2187 * k5.w / 6784 + 11 * k6.w / 84)};
    if (err) {
        const Y2 k7 = rhs(n);
        const double eu = h * (71 * k1.u / 57600 - 71 * k3.u / 16695 + 71 * k4.u / 1920 - 17253 * k5.u / 339200 + 22 * k6.u / 525 - k7.u / 40);
        const double ew = h * (71 * k1.w / 57600 - 71 * k3.w / 16695 + 71 * k4.w / 1920 - 17253 * k5.w / 339200 + 22 * k6.w / 525 - k7.w / 40);
        const double rt = 1e-13, at = 1e-16;
        const double su = at + rt * std::max(std::fabs(y.u), std::fabs(n.u)), sw = at + rt * std::max(std::fabs(y.w), std::fabs(n.w));
        *err = std::max(std::fabs(eu) / su, std::fabs(ew) / sw);
    }
    return n;
}
Y2 start(double rO, double psi) {
    const double u0 = 1 / rO;
    return {u0, -u0 * std::sqrt(1 - 2 * u0) * std::cos(psi) / std::sin(psi)};
}
// Integrates from phi = 0. Visits each target (increasing) and stops at the ray's end (u = 0 or 1/2). Returns the end angle; captured set.
// targets may be null. At most maxSteps accepted steps.
double integrate(double rO, double psi, const double* tg, int nt, double* out, bool& captured) {
    captured = psi > kPi / 2;
    if (!(std::sin(psi) > 0)) { for (int i = 0; i < nt; i++) out[i] = 1 / rO; return 0; }
    Y2 y = start(rO, psi);
    double phi = 0;
    double h = std::min(0.01, 0.01 * y.u / (std::fabs(y.w) + 1e-300));
    int ti = 0;
    for (int steps = 0; steps < 2000000; steps++) {
        double err = 0;
        bool toTarget = false;
        double hs = h;
        if (ti < nt && phi + hs >= tg[ti]) { hs = tg[ti] - phi; toTarget = true; }
        if (hs <= 0) { out[ti++] = y.u; continue; }
        const Y2 n = dpStep(y, hs, &err);
        if (err > 1) { h = hs * std::max(0.2, 0.9 * std::pow(err, -0.2)); continue; }
        // the end inside this step?
        const bool esc = n.u <= 0, hor = n.u >= 0.5;
        if (esc || hor) {
            const double target = esc ? 0 : 0.5;
            double hh = hs * (y.u - target) / (y.u - n.u);                          // a first guess, then Newton on single steps from the step's start
            for (int it = 0; it < 30; it++) {
                const Y2 m = dpStep(y, hh, nullptr);
                const double d = (m.u - target) / m.w;
                hh -= d;
                if (std::fabs(d) < 1e-16 * (1 + phi)) break;
            }
            captured = hor;
            // targets beyond the end are not filled; a target within the step and before the end is
            while (ti < nt && tg[ti] <= phi + hh) { out[ti] = dpStep(y, tg[ti] - phi, nullptr).u; ti++; }
            for (; ti < nt; ti++) out[ti] = target;
            return phi + hh;
        }
        phi += hs; y = n;
        if (toTarget) out[ti++] = y.u;
        if (!toTarget || err > 0) h = hs * std::min(5.0, std::max(0.2, 0.9 * std::pow(std::max(err, 1e-30), -0.2)));
        while (ti < nt && tg[ti] <= phi) out[ti++] = y.u;                         // (equal targets)
    }
    return phi;
}
}  // namespace

RayEnd2 rayEndAngle(double rO, double psi) {
    RayEnd2 e;
    e.phiEnd = integrate(rO, psi, nullptr, 0, nullptr, e.captured);
    return e;
}
void rayU(double rO, double psi, const double* phis, int n, double* u) { bool c; integrate(rO, psi, phis, n, u, c); }
double rayRadius(double rO, double psi, double phi) { double u; rayU(rO, psi, &phi, 1, &u); return 1 / u; }

double firstCrossing(double A, double B) {
    double p = std::atan2(-A, B);
    if (p <= 0) p += kPi;
    return p;
}
double discShift(double r, double Lz) {
    const double ut = 1 / std::sqrt(1 - 3 / r), om = 1 / (r * std::sqrt(r));   // L79 p. 233 (a = 0): u^t, Omega = (M/r^3)^(1/2)
    return 1 / (ut * (1 - om * Lz));                                           // L79 (16)
}

DiscHit discRay(double rO, double psi, double A, double B, double LzOverB, const DiscGeom& d) {
    DiscHit h;
    h.b = cameraImpact(rO, psi);
    double tg[kDiscOrders], u[kDiscOrders];
    const double p0 = firstCrossing(A, B);
    for (int k = 0; k < kDiscOrders; k++) tg[k] = p0 + k * kPi;
    bool cap;
    const double end = integrate(rO, psi, tg, kDiscOrders, u, cap);
    h.captured = cap;
    for (int k = 0; k < kDiscOrders; k++) {
        if (tg[k] >= end) break;
        const double r = 1 / u[k];
        if (r >= d.rIn && r <= d.rOut) {
            h.order = k; h.r = r; h.Lz = h.b * LzOverB; h.g = discShift(r, h.Lz);
            return h;
        }
    }
    return h;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the card's table
DiscTable::DiscTable(double rO, int nS, int nT) : rO_(rO), alpha_(shadowAngle(rO)), nS_(nS), nT_(nT) {
    const auto t0 = std::chrono::steady_clock::now();
    const double xmax[2] = {kPi - alpha_, alpha_};
    u_.assign(static_cast<std::size_t>(2) * nS * nT, 0);
    p_.assign(static_cast<std::size_t>(2) * nS, 0);
    for (int side = 0; side < 2; side++) {
        s0_[side] = sOf(1e-9);
        ds_[side] = (sOf(xmax[side]) - s0_[side]) / (nS - 1);
    }
    // every row on its own (all cores; each row writes only its own values)
    const auto rowJob = [&](int side, int i, std::vector<double>& tg, std::vector<double>& out) {
        const double s = s0_[side] + i * ds_[side];
        double x = std::min(std::exp(s), xmax[side]);                         // invert s = ln x + x / xc by Newton
        for (int it = 0; it < 60; it++) { const double d = (sOf(x) - s) / (1 / x + 1 / kDiscXc); x = std::max(x - d, x / 4); if (std::fabs(d) < 1e-16 * x) break; }
        if (i == nS - 1) x = xmax[side];
        const double psi = side == 0 ? (kPi - alpha_) - x : (kPi - alpha_) + x;
        double end;
        bool cap;
        if (side == 0) end = bendFromCameraEdge(rO, x).dphi;                  // the closed form (schwarzschild.h): the ray to the far sky
        else end = integrate(rO, psi, nullptr, 0, nullptr, cap);
        p_[static_cast<std::size_t>(side) * nS + i] = end;
        for (int j = 0; j < nT; j++) tg[j] = end * j / (nT - 1);
        integrate(rO, psi, tg.data(), nT, out.data(), cap);
        out[nT - 1] = side == 0 ? 0 : 0.5;                                    // (the end itself, exactly)
        for (int j = 0; j < nT; j++) u_[(static_cast<std::size_t>(side) * nS + i) * nT + j] = out[j];
    };
    const int nth = std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, 16);
    std::atomic<int> next{0};
    const auto worker = [&] {
        std::vector<double> tg(nT), out(nT);
        for (int k; (k = next++) < 2 * nS;) rowJob(k / nS, k % nS, tg, out);
    };
    std::vector<std::thread> pool;
    for (int w = 1; w < nth; w++) pool.emplace_back(worker);
    worker();
    for (std::thread& th : pool) th.join();
    uf_.assign(u_.begin(), u_.end());
    pf_.assign(p_.begin(), p_.end());
    ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

namespace {
// LensTable's 4-point cubic: weights at u in [0, 1] (between nodes i and i+1) for nodes i-1 .. i+2
inline void cubicW(double u, double w[4]) {
    w[0] = -u * (u - 1) * (u - 2) / 6;
    w[1] = (u + 1) * (u - 1) * (u - 2) / 2;
    w[2] = -(u + 1) * u * (u - 2) / 2;
    w[3] = (u + 1) * u * (u - 1) / 6;
}
// node index (lowest of the four) and weights for a position q on n nodes, clamped at the ends
inline int stencil(double q, int n, double w[4]) {
    q = std::clamp(q, 0.0, static_cast<double>(n - 1));
    int i = std::clamp(static_cast<int>(q), 1, n - 3);
    cubicW(q - i, w);
    return i - 1;
}
}  // namespace

double DiscTable::phiEnd(double x) const {
    const int side = x > 0 ? 0 : 1;
    const double q = (sOf(std::fabs(x)) - s0_[side]) / ds_[side];
    double w[4];
    const int i0 = stencil(q, nS_, w);
    double v = 0;
    for (int a = 0; a < 4; a++) v += w[a] * p_[static_cast<std::size_t>(side) * nS_ + i0 + a];
    return v;
}

bool DiscTable::lookup(double x, double phi, double& u) const {
    const int side = x > 0 ? 0 : 1;
    const double P = phiEnd(x);
    if (!(phi <= P)) return false;
    const double q = (sOf(std::fabs(x)) - s0_[side]) / ds_[side];
    double wr[4], wc[4];
    const int i0 = stencil(q, nS_, wr);
    const int j0 = stencil(phi / P * (nT_ - 1), nT_, wc);
    double v = 0;
    for (int a = 0; a < 4; a++) {
        const double* row = &u_[(static_cast<std::size_t>(side) * nS_ + i0 + a) * nT_ + j0];
        v += wr[a] * (wc[0] * row[0] + wc[1] * row[1] + wc[2] * row[2] + wc[3] * row[3]);
    }
    u = v;
    return true;
}

DiscHit discRayTable(const DiscTable& t, double x, double A, double B, double LzOverB, const DiscGeom& d) {
    DiscHit h;
    const double psi = (kPi - t.alphaSh()) - x;
    h.b = cameraImpact(t.rO(), psi);
    h.captured = x < 0;
    const double p0 = firstCrossing(A, B);
    for (int k = 0; k < kDiscOrders; k++) {
        double u;
        if (!t.lookup(x, p0 + k * kPi, u)) break;
        const double r = 1 / u;
        if (u > 0 && r >= d.rIn && r <= d.rOut) {
            h.order = k; h.r = r; h.Lz = h.b * LzOverB; h.g = discShift(r, h.Lz);
            return h;
        }
    }
    return h;
}

}  // namespace blackhole
