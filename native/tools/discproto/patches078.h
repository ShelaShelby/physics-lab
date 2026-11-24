// 0.79.0: 0.72-0.78's patches (SKH06), kept here only so that discproto can draw "today" as the owner saw it in disc078_*.png: the app's own code of 0.78.1
// (native/src/blackhole/gas.h, gas.cpp at commit "native 0.78.1"), moved into namespace patches078 unchanged. Not part of the app.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "blackhole/disc.h"
#include "blackhole/gas.h"
namespace patches078 {
using namespace blackhole;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCutPhi = 3;
inline double gasFade(double x, double end = 8);
inline double gasShear(double r, double a);
struct GasPatch { double eta = 0, phi = 0, tBirth = 0, life = 1, sign = 1; };
// 0.72.0: the lab's norm, one number: the pattern is self-similar (its patches' sizes in ln r and phi and their lives in orbits do not depend on r), so the mean
// of exp(c S) is the same over any disc but for its inner edge. normalise(160000) here (c = 0.8): spin 0, outer edge 30 M: 3.137 (standard error 0.023); spin 0,
// 2000 M: 3.124 (0.117); spin 0.9, 30 M: 3.200 (0.028); spin 0.9, 2000 M: 3.158 (0.186); their mean weighted by 1 / error^2: 3.16 (tested: testGas)
constexpr double kGasNorm = 3.16;
// 0.72.0: the same for the pattern the lab draws (reduced(), c = 1.1), normalise(160000): spin 0, 30 M: 2.573 (0.015); spin 0, 2000 M: 2.686 (0.076); spin 0.9,
// 30 M: 2.557 (0.018); spin 0.9, 2000 M: 2.696 (0.126); weighted 2.57 (tested: testGasCard)
constexpr double kGasNormCard = 2.57;
struct GasPattern {
    double a = 0, rIn = 6, rOut = 20, contrast = 0.8, norm = kGasNorm, normErr = 0;   // normErr: the norm's relative standard error (normalise)
    int cells = 28, slots = 9;                                                            // 0.72.0: phi cells per bin and slot, birth slots looked at, and
    double fadeEnd = 8, cutPhi = kCutPhi;                                                 // where the fade ends (lifetimes), the reach in phi (sigmas): the full
                                                                                          // pattern (below: reduced())
    static constexpr double kWidthEta = 0.26, kWidthPhi = 25 * 3.14159265358979323846 / 180;   // SKH06 section 5 (full widths)
    static constexpr double kLife = 0.3;                                                         // T_life / T_orb, SKH06 (36)
    static constexpr int kEtaSide = 2, kSlots = 9;                                                // eta bins on each side, birth slots looked at
    static constexpr int kCells = 28;                                                             // 0.72.0: phi cells of 2 pi / 28 (0.56.4: 12.5 degrees, a 10 degree gap)
    static constexpr double kCutEta = kWidthEta, kCutPhi = 3, kFadeEnd = 8;                       // 0.72.0: the tapers' reach (2 bins; 3 sigma; 8 lifetimes)
    // one patch's strength at (r, phi, t): its Gaussian at where this piece of gas was when the patch was born, times x e^(1 - x) (fade) or 1 (no fade, x > 0)
    static double patch(const GasPatch& p, double r, double phi, double t, double a, bool fade = true);
    double sum(double r, double phi, double t) const;                       // S: the sum of the patches that reach (r, phi) at t
    // 0.72.0: S widened by the pixel's footprint (its sigma fe in ln r, fp in phi; 0: none) and the number of cells looked at (the card does the same);
    // all (tests): every cell of every slot looked at
    double sumAt(double r, double phi, double t, double fe, double fp, int* looked, bool all = false, double* var = nullptr) const;
    double factorAt(double r, double phi, double t, double fe, double fp) const;   // the footprint's mean of the factor (gas.cpp)
    int binLo() const;                                                      // the bins with patches: centre exp((je + 1/2) d_eta) in [0.8 rIn, 1.3 rOut]
    int binHi() const;
    double binLife(int je) const;                                           // T_life of bin je: kLife T_orb(max(r_bin, rIn))
    double binOmega(int je) const;                                          // Omega at the bin's centre
    double factor(double r, double phi, double t) const;                    // exp(c S) / norm
    void normalise(int samples = 20000);                                    // norm = the mean of exp(c S) weighted by the disc's light, at independent samples
    // 0.72.0, the pattern the lab draws (the job's stop rule: the full pattern per pixel took 7.7 times 0.71.0's sky pass on this box; this one 4.0 with the
    // light's travel time and the clump, REPORT 0.72.0): half the patches (14 phi cells of 25.7 degrees per bin and slot, each patch still 25 degrees wide),
    // their fade ending at 3.5 lifetimes (4 slots), their reach in phi 2.5 sigma, c = 1.1 so that the face-on 6-10 M ring still varies by SKH06 Table 2's
    // 43.8 % rms (43.9 % here; tested), norm kGasNormCard. The full pattern above stays the processor's reference of the model (tests).
    static GasPattern reduced();
};


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

inline double GasPattern::patch(const GasPatch& p, double r, double phi, double t, double a, bool fade) {
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
inline double gasFade(double x, double end) {
    if (x <= 0 || x >= end) return 0;
    const double q = x / end, q2 = q * q, q4 = q2 * q2, w = 1 - q4 * q4;
    return x * std::exp(1 - x) * w * w;
}

inline double gasShear(double r, double a) {   // -r dOmega/dr for Omega = 1 / (r^(3/2) + a) (BPT (2.16))
    const double w = 1 / (r * std::sqrt(r) + a);
    return 1.5 * r * std::sqrt(r) * w * w;
}

inline int GasPattern::binLo() const { return static_cast<int>(std::ceil(std::log(rIn * 0.8) / (kWidthEta / 2) - 0.5)); }
inline int GasPattern::binHi() const { return static_cast<int>(std::floor(std::log(rOut * 1.3) / (kWidthEta / 2) - 0.5)); }
inline double GasPattern::binLife(int je) const { return kLife * gasPeriod(std::max(std::exp((je + 0.5) * kWidthEta / 2), rIn), a); }
inline double GasPattern::binOmega(int je) const { return gasOmega(std::exp((je + 0.5) * kWidthEta / 2), a); }

// 0.72.0 (the card's form, render/shaders.h kGasPart does the same in single precision): the patches of bin je, slot k, cell jp are born at
//   t_birth = (k + u1) T_life, eta = (je + u2) d_eta, phi_birth = psi + Omega(r_bin) u1 T_life with psi = (jp + u3) d_phi (u1, u2, u3 from the hash):
// psi is the patch's place in the frame turning with its bin from the slot's start, so its birth places are as even in phi as before (0.56.4: psi itself).
// Then where the piece of gas at the point was at birth, phi - Omega(r) (t - t_birth) - phi_birth = c0 + u1 delta - psi with c0 = phi - Omega(r) (t - k T_life)
// and delta = T_life (Omega(r) - Omega(r_bin)): the cells that can reach the point are those whose psi lies within kCutPhi sigma of [c0 + min(0, delta),
// c0 + max(0, delta)] (cellsLooked counts them: feasibility, REPORT 0.72.0). 0.56.4 looked at all 28 cells: the same patches reach (the taper), fewer looks.
inline double GasPattern::sumAt(double r, double phi, double t, double fe, double fp, int* looked, bool all, double* var) const {
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

inline double GasPattern::sum(double r, double phi, double t) const { return sumAt(r, phi, t, 0, 0, nullptr, false); }

inline double GasPattern::factor(double r, double phi, double t) const { return std::exp(contrast * sum(r, phi, t)) / norm; }
// 0.72.0: the footprint's mean of exp(c S), S taken as Gaussian over the footprint with the patches' spread V (each patch's footprint mean of its square less
// the square of its mean; the patches as independent): exp(c S + c^2 V / 2) / norm. With no footprint V = 0: factor().
inline double GasPattern::factorAt(double r, double phi, double t, double fe, double fp) const {
    double v = 0;
    const double S = sumAt(r, phi, t, fe, fp, nullptr, false, &v);
    return std::exp(contrast * S + 0.5 * contrast * contrast * v) / norm;
}

inline void GasPattern::normalise(int samples) {
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

inline GasPattern GasPattern::reduced() {
    GasPattern p;
    p.cells = 14; p.slots = 4; p.fadeEnd = 3.5; p.cutPhi = 2.5; p.contrast = 1.1; p.norm = kGasNormCard;
    return p;
}

}  // namespace patches078
