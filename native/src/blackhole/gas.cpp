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

// 0.72.0: the patch's Gaussian in (ln r, phi at birth) is tapered to 0 at kCutEta = 2 bins (so the 5 bins a point looks at hold every patch that reaches
// it: no seams where the point crosses a bin's edge; 0.56.4's cut-off left patches 2.35 to 3.5 sigma away out, a jump of up to 6 % of a patch at every
// bin's edge) and at kCutPhi = 3 sigma in phi, by (1 - (d / cut)^4)^2. Widened by the pixel's footprint (fe, fp: the footprint's sigma in ln r and phi):
// the patch sheared by the rotation: its offset at birth dP = Y + S X in the point's (X, Y) = (ln r, phi) offsets, S = -r Omega'(r) (t - t_birth), so it is
// a Gaussian of covariance [[se^2, -S se^2], [-S se^2, S^2 se^2 + sp^2]] in (X, Y); blurred by the footprint's (diagonal fe^2, fp^2) it is the Gaussian of the summed covariance, its height
// scaled by (det before / det after)^(1/2) (the patch's light kept). fe = fp = 0: the patch itself.
namespace {
inline double taper(double d, double cut) { const double q = d / cut, q2 = q * q; return q >= 1 ? 0.0 : (1 - q2 * q2) * (1 - q2 * q2); }
// the patch's shape at (dE, dP) and, for the footprint's spread, the footprint's mean of its square (g^2 is the Gaussian of half the covariance)
inline void shapeAt(double dE, double dP, double S, double fe, double fp, double& g, double& g2, double cutPhi = GasPattern::kCutPhi) {
    const double se = GasPattern::kWidthEta / 2.3548, sp = GasPattern::kWidthPhi / 2.3548;
    g = g2 = 0;
    const double tp = taper(std::fabs(dE), GasPattern::kCutEta) * taper(std::fabs(dP), cutPhi * sp);
    if (tp <= 0) return;
    const double se2 = se * se, sp2 = sp * sp, fe2 = fe * fe, fp2 = fp * fp;
    const double x = dE, y = dP - S * dE;
    for (int k = 0; k < 2; k++) {
        const double h = k == 0 ? 1.0 : 0.5;                                    // the covariance h Sigma, blurred by the footprint's
        const double c11 = h * se2 + fe2, c12 = -h * S * se2, c22 = h * (S * S * se2 + sp2) + fp2;
        const double det0 = h * h * se2 * sp2, det = c11 * c22 - c12 * c12;
        const double q = (c22 * x * x - 2 * c12 * x * y + c11 * y * y) / det;
        const double v = std::sqrt(det0 / det) * std::exp(-0.5 * q);
        if (k == 0) g = tp * v; else g2 = tp * tp * v;
    }
}
}  // namespace

double GasPattern::patch(const GasPatch& p, double r, double phi, double t, double a, bool fade) {
    const double x = (t - p.tBirth) / p.life;
    if (x <= 0) return 0;
    const double env = fade ? gasFade(x) : 1.0;
    const double dE = std::log(r) - p.eta;
    double dP = phi - gasOmega(r, a) * (t - p.tBirth) - p.phi;               // where this piece of gas was when the patch was born
    dP = std::remainder(dP, 2 * kPi);
    double g, g2;
    shapeAt(dE, dP, 0, 0, 0, g, g2);
    return p.sign * env * g;
}

// 0.72.0: SKH06's light x e^(1 - x), tapered to 0 at x = kFadeEnd by (1 - (x / kFadeEnd)^8)^2 (0.99 at x = 4, 0.43 at 7): every patch with x up to 8 is
// in the 9 slots looked at (the oldest slot reaches x = 8 + its fraction), so the sum has no jump when the slots move on (0.56.4: a cut at 2e-3)
double gasFade(double x, double end) {
    if (x <= 0 || x >= end) return 0;
    const double q = x / end, q2 = q * q, q4 = q2 * q2, w = 1 - q4 * q4;
    return x * std::exp(1 - x) * w * w;
}

double gasShear(double r, double a) {   // -r dOmega/dr for Omega = 1 / (r^(3/2) + a) (BPT (2.16))
    const double w = 1 / (r * std::sqrt(r) + a);
    return 1.5 * r * std::sqrt(r) * w * w;
}

int GasPattern::binLo() const { return static_cast<int>(std::ceil(std::log(rIn * 0.8) / (kWidthEta / 2) - 0.5)); }
int GasPattern::binHi() const { return static_cast<int>(std::floor(std::log(rOut * 1.3) / (kWidthEta / 2) - 0.5)); }
double GasPattern::binLife(int je) const { return kLife * gasPeriod(std::max(std::exp((je + 0.5) * kWidthEta / 2), rIn), a); }
double GasPattern::binOmega(int je) const { return gasOmega(std::exp((je + 0.5) * kWidthEta / 2), a); }

// 0.72.0 (the card's form, render/shaders.h kGasPart does the same in single precision): the patches of bin je, slot k, cell jp are born at
//   t_birth = (k + u1) T_life, eta = (je + u2) d_eta, phi_birth = psi + Omega(r_bin) u1 T_life with psi = (jp + u3) d_phi (u1, u2, u3 from the hash):
// psi is the patch's place in the frame turning with its bin from the slot's start, so its birth places are as even in phi as before (0.56.4: psi itself).
// Then where the piece of gas at the point was at birth, phi - Omega(r) (t - t_birth) - phi_birth = c0 + u1 delta - psi with c0 = phi - Omega(r) (t - k T_life)
// and delta = T_life (Omega(r) - Omega(r_bin)): the cells that can reach the point are those whose psi lies within kCutPhi sigma of [c0 + min(0, delta),
// c0 + max(0, delta)] (cellsLooked counts them: feasibility, REPORT 0.72.0). 0.56.4 looked at all 28 cells: the same patches reach (the taper), fewer looks.
double GasPattern::sumAt(double r, double phi, double t, double fe, double fp, int* looked, bool all, double* var) const {
    const double eta = std::log(r), de = kWidthEta / 2, dp = 2 * kPi / cells, sp = kWidthPhi / 2.3548, w = cutPhi * sp;
    const int ie = static_cast<int>(std::floor(eta / de)), lo = binLo(), hi = binHi();
    const double om = gasOmega(r, a), shr = gasShear(r, a);
    double s = 0, vs = 0;
    int n = 0;
    for (int je = ie - kEtaSide; je <= ie + kEtaSide; je++) {
        if (je < lo || je > hi) continue;
        const double life = binLife(je), xs = t / life, del = life * (om - binOmega(je));
        const double k0 = std::floor(xs);
        for (int sl = 0; sl < slots; sl++) {
            const double kd = k0 - sl, xm = xs - kd;                          // the slot's patches: x in (xm - 1, xm]
            const double c0 = std::remainder(phi - om * life * xm, 2 * kPi);
            const int j0 = static_cast<int>(std::floor((c0 + std::min(0.0, del) - w) / dp));
            const int nc = all ? cells : std::min(static_cast<int>(std::floor((c0 + std::max(0.0, del) + w) / dp)) - j0 + 1, cells);
            const std::uint32_t kk = static_cast<std::uint32_t>(static_cast<std::int64_t>(kd) + 100000);
            for (int i = 0; i < nc; i++) {
                const int jp = ((j0 + i) % cells + cells) % cells;
                n++;
                const std::uint32_t h = gasHash(static_cast<std::uint32_t>(je + 1000), kk, static_cast<std::uint32_t>(jp));
                const double u1 = gasU01(h), x = xm - u1;
                const double env = gasFade(x, fadeEnd);
                if (env <= 0) continue;
                const double dE = eta - (je + gasU01(h * 747796405u + 1u)) * de;
                if (std::fabs(dE) >= kCutEta) continue;
                const double psi = (jp + gasU01(h * 2891336453u + 7u)) * dp;
                const double dP = std::remainder(c0 + u1 * del - psi, 2 * kPi);
                if (std::fabs(dP) >= w) continue;
                double g, g2;
                shapeAt(dE, dP, shr * life * x, fe, fp, g, g2, cutPhi);                  // S = -r Omega' (t - t_birth)
                s += ((h >> 31) ? 1.0 : -1.0) * env * g;
                vs += env * env * std::max(g2 - g * g, 0.0);
            }
        }
    }
    if (looked) *looked = n;
    if (var) *var = vs;
    return s;
}

double GasPattern::sum(double r, double phi, double t) const { return sumAt(r, phi, t, 0, 0, nullptr, false); }

double GasPattern::factor(double r, double phi, double t) const { return std::exp(contrast * sum(r, phi, t)) / norm; }
// 0.72.0: the footprint's mean of exp(c S), S taken as Gaussian over the footprint with the patches' spread V (each patch's footprint mean of its square less
// the square of its mean; the patches as independent): exp(c S + c^2 V / 2) / norm. With no footprint V = 0: factor().
double GasPattern::factorAt(double r, double phi, double t, double fe, double fp) const {
    double v = 0;
    const double S = sumAt(r, phi, t, fe, fp, nullptr, false, &v);
    return std::exp(contrast * S + 0.5 * contrast * contrast * v) / norm;
}

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

GasPattern GasPattern::reduced() {
    GasPattern p;
    p.cells = 14; p.slots = 4; p.fadeEnd = 3.5; p.cutPhi = 2.5; p.contrast = 1.1; p.norm = kGasNormCard;
    return p;
}

double gasEpoch(double T) { return 1024.0 * std::floor(T / 1024.0); }

void gasCard(const GasPattern* p, const Clump* c, double T, GasCard& o) {
    o = GasCard{};
    o.epoch = gasEpoch(T);
    o.tau = static_cast<float>(T - o.epoch);
    if (c) {
        o.T[3] = static_cast<float>(c->A);
        o.C[0] = static_cast<float>(c->rc);
        o.C[1] = static_cast<float>(std::remainder(c->phi0 + c->omega * o.epoch, 2 * kPi));
        o.C[2] = static_cast<float>(c->omega);
        o.C[3] = static_cast<float>(1 / (2 * c->RS * c->RS));
        o.G[0] = static_cast<float>(c->grr);
        o.G[1] = static_cast<float>(c->gpp);
    }
    if (p) {
        o.T[0] = static_cast<float>(p->fadeEnd);
        o.cutPhi = static_cast<float>(p->cutPhi);
        o.N[3] = p->cells;
        o.T[1] = static_cast<float>(p->contrast);
        o.T[2] = static_cast<float>(1 / p->norm);
        o.G[2] = static_cast<float>(p->a);
        o.G[3] = static_cast<float>(p->rIn);
        const int lo = p->binLo(), n = std::clamp(p->binHi() - lo + 1, 0, kGasCardBins);
        o.N[0] = lo; o.N[1] = n; o.N[2] = 1;
        for (int i = 0; i < n; i++) {
            const double q = o.epoch / p->binLife(lo + i), K = std::floor(q);
            const std::uint32_t k = static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(K)));
            o.bin[4 * i] = static_cast<float>(q - K);
            o.bin[4 * i + 1] = static_cast<float>(k >> 16);
            o.bin[4 * i + 2] = static_cast<float>(k & 0xFFFFu);
        }
    } else if (c) o.G[2] = static_cast<float>(c->a);
}

}  // namespace blackhole
