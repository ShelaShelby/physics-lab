#include "blackhole/gas.h"

#include <algorithm>
#include <cmath>

#include "blackhole/disc.h"
#include "blackhole/kerr.h"
#include "orbit/units.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

double gasOmega(double r, double a) { return orbitOmega(r, a); }
double gasPeriod(double r, double a) { return 2 * kPi / gasOmega(r, a); }
double gasSpeed(double r, double a) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    return m.varpi / m.alpha * (gasOmega(r, a) - m.omega);
}
double gasClockRate(double r, double a) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    const double V = gasSpeed(r, a);
    return m.alpha * std::sqrt(1 - V * V);
}
double cameraClockRate(double a, double r, double theta) { return kerrMetric(a, r, theta).alpha; }
double massTimeSeconds(double massSun) { return massSun * units::kGMSun / std::pow(units::kLightSpeed, 3); }

bool KerrBLt::launch(const Camera& cam, const Vec3& look, Ray& ray) const {
    if (!KerrBL::launch(cam, look, ray)) return false;
    ray.n = kTState;
    ray.y[5] = 0;
    ray.y[6] = 0;
    return true;
}
void KerrBLt::rhs(const Ray& ray, const double* y, double* dy) const {
    KerrBL::rhs(ray, y, dy);
    const double r = y[0], s = std::sin(y[1]), c = std::cos(y[1]), a = a_, b = ray.c[0];
    const double rho2 = r * r + a * a * c * c, D = r * r - 2 * r + a * a, P = r * r + a * a - a * b;
    dy[5] = ((r * r + a * a) * P / D + a * (b - a * s * s)) / rho2;
    dy[6] = 1 / rho2;
}

void Clump::init() {
    const KerrMetric m = kerrMetric(a, rc, kPi / 2);
    const double uph = orbitAngMom(rc, a);
    grr = rc * rc / m.Delta;                       // g_rr = rho^2 / Delta at the equator
    gpp = m.varpi * m.varpi + uph * uph;           // g_phph + u_phi^2
    omega = gasOmega(rc, a);
}
double Clump::density(double r, double phi, double tEmit) const {
    const double dr = r - rc, dp = std::remainder(phi - phi0 - omega * tEmit, 2 * kPi);
    return std::exp(-(grr * dr * dr + gpp * dp * dp) / (2 * RS * RS));
}
double Clump::period() const { return 2 * kPi / omega; }

std::uint32_t gasHash(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
double gasU01(std::uint32_t h) { return (h & 0xFFFFFF) / 16777216.0; }

double GasPattern::patch(const GasPatch& p, double r, double phi, double t, double a, bool fade) {
    const double se = kWidthEta / 2.3548, sp = kWidthPhi / 2.3548;          // Gaussian sigma from the full width at half maximum
    const double x = (t - p.tBirth) / p.life;
    if (x <= 0) return 0;
    const double env = fade ? x * std::exp(1 - x) : 1.0;
    const double dE = std::log(r) - p.eta;
    double dP = phi - gasOmega(r, a) * (t - p.tBirth) - p.phi;               // where this piece of gas was when the patch was born
    dP = std::remainder(dP, 2 * kPi);
    return p.sign * env * std::exp(-0.5 * (dE * dE / (se * se) + dP * dP / (sp * sp)));
}

double GasPattern::sum(double r, double phi, double t) const {
    const double eta = std::log(r), de = kWidthEta / 2, dp = kWidthPhi / 2;
    const double se = kWidthEta / 2.3548;
    const int ie = static_cast<int>(std::floor(eta / de));
    const int nphi = static_cast<int>(std::floor(2 * kPi / dp));
    double s = 0;
    for (int je = ie - kEtaSide; je <= ie + kEtaSide; je++) {
        const double rb = std::exp((je + 0.5) * de);
        if (rb < rIn * 0.8 || rb > rOut * 1.3) continue;
        const double life = kLife * gasPeriod(std::max(rb, rIn), a);
        const int k0 = static_cast<int>(std::floor(t / life));
        for (int k = k0 - (kSlots - 1); k <= k0; k++) {   // patches born up to 8 lifetimes ago
            for (int jp = 0; jp < nphi; jp++) {
                const std::uint32_t h = gasHash(static_cast<std::uint32_t>(je + 1000), static_cast<std::uint32_t>(k + 100000), static_cast<std::uint32_t>(jp));
                GasPatch p;
                p.life = life;
                p.tBirth = (k + gasU01(h)) * life;
                const double x = (t - p.tBirth) / life;
                if (x <= 0) continue;
                if (x * std::exp(1 - x) < 2e-3) continue;
                p.eta = (je + gasU01(h * 747796405u + 1u)) * de;
                if (std::fabs(eta - p.eta) > 3.5 * se) continue;
                p.phi = (jp + gasU01(h * 2891336453u + 7u)) * dp;
                p.sign = (h >> 31) ? 1.0 : -1.0;                // bright or dark patches
                s += patch(p, r, phi, t, a, true);
            }
        }
    }
    return s;
}

double GasPattern::factor(double r, double phi, double t) const { return std::exp(contrast * sum(r, phi, t)) / norm; }

void GasPattern::normalise(int samples) {
    // 0.56.4: the mean weighted by the disc's light (r F(r): the disc's total light), r stratified over [rIn, rOut], phi from the hash, and times 10 lifetimes
    // of the outermost bins apart, so that no two samples at one place share a patch (a point sees the patches born in the 9 slots before it): independent
    // samples. (0.56.3's tool took 20000 points in one window of 2000 M, a few lifetimes far out: its mean was about 20 % high, found by the conservation test.)
    norm = 1;
    const double gap = 10 * kLife * gasPeriod(1.3 * rOut * std::exp(kWidthEta), a);
    double sw = 0, sx = 0, sx2 = 0, sw2 = 0;
    for (int i = 0; i < samples; i++) {
        const std::uint32_t h = gasHash(7u, 11u, static_cast<std::uint32_t>(i));
        const double r = rIn + (rOut - rIn) * (i + 0.5) / samples, w = r * discFlux(r, a);
        const double x = factor(r, 2 * kPi * gasU01(h), 1e4 + gap * (i + gasU01(h * 747796405u + 1u)));
        sw += w; sx += w * x; sx2 += w * x * x; sw2 += w * w;
    }
    const double m = sx / sw, var = sx2 / sw - m * m;
    norm = m;
    normErr = std::sqrt(std::max(var, 0.0) * sw2) / sw / m;
}

}  // namespace blackhole
