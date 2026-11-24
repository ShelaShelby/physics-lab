// Black Hole Lab (0.80.0): Sagittarius A*'s hot flow. The sources and the rules are in hot.h.
#include "blackhole/hot.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "blackhole/disc.h"
#include "blackhole/low_priority.h"
#include "blackhole/presets.h"
#include "blackhole/schwarzschild.h"
#include "render/cie1931.h"

namespace blackhole {
namespace hot {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

bool modelFor(double spin, Model& m) {
    // BL06 Table 1 and sect. 2.2's ISCO radii (6M, 4.233M, 1.237M)
    struct Row { double a, n0, T0, n0nth, pnth, rIsco; };
    static const Row rows[3] = {{0, 3e7, 1.7e11, 8e4, -2.9, 6}, {0.5, 3e7, 1.4e11, 5e4, -2.8, 4.233}, {0.998, 1e7, 1.5e11, 1e5, -2.8, 1.237}};
    for (const Row& r : rows)
        if (std::fabs(spin - r.a) < 1e-9) { m = Model{r.a, r.n0, r.T0, r.n0nth, r.pnth, r.rIsco}; return true; }
    return false;
}

double bEquipartition(double ne, double r) { return std::sqrt(8 * kPi * ne * cMp * cC * cC * 2 / (12 * r)); }   // r_S / (12 r) = 2M / (12 r)

Plasma plasma(const Model& m, double r, double theta, BReading rd) {
    Plasma p;
    const double s = std::sin(theta), c = std::cos(theta), R = r * s / m.unit, rr = r / m.unit;
    if (!(R > 1e-12)) return p;
    const double ex = std::exp(-c * c / (2 * s * s));          // exp(-z^2 / 2 rho^2), z / rho = cot(theta)
    p.ne = m.n0 * std::pow(R, -1.1) * ex;
    p.nnth = m.n0nth * std::pow(R, m.pnth) * ex;
    p.Te = m.T0 * std::pow(rr, -0.84);
    const double beq = bEquipartition(p.ne, r);
    p.B = rd == BReading::Energy ? std::sqrt(kEquipShare) * beq : kEquipShare * beq;
    return p;
}

double planckNu(double nu, double T) { return 2 * cH * nu * nu * nu / (cC * cC) / std::expm1(cH * nu / (cK * T)); }

double jThermal(double ne, double Te, double B, double nu, double sinB) {
    const double Th = cK * Te / (cMe * cC * cC), nuc = cE * B / (2 * kPi * cMe * cC), nus = 2.0 / 9 * nuc * Th * Th * sinB;
    if (!(nus > 0) || !(ne > 0)) return 0;
    const double X = nu / nus, k2 = std::cyl_bessel_k(2.0, 1 / Th);
    if (!(k2 > 0) || !std::isfinite(k2)) return 0;
    const double cx = std::cbrt(X), t = std::sqrt(X) + std::pow(2.0, 11.0 / 12) * std::sqrt(cx);   // X^1/6 = (X^1/3)^1/2
    return ne * std::sqrt(2.0) * kPi * cE * cE * nus / (3 * k2 * cC) * t * t * std::exp(-cx);
}
double aThermal(double ne, double Te, double B, double nu, double sinB) {
    const double j = jThermal(ne, Te, B, nu, sinB);
    return j > 0 ? j / planckNu(nu, Te) : 0;
}

double jPower(double nnth, double B, double nu, double sinB) {
    if (!(nnth > 0) || !(sinB > 0) || !(B > 0)) return 0;
    const double nuc = cE * B / (2 * kPi * cMe * cC), p = kP;
    return nnth * cE * cE * nuc / cC * std::pow(3.0, p / 2) * (p - 1) * sinB / (2 * (p + 1) * std::pow(kGammaMin, 1 - p)) *
           std::tgamma((3 * p - 1) / 12) * std::tgamma((3 * p + 19) / 12) * std::pow(nu / (nuc * sinB), -(p - 1) / 2);
}
double aPower(double nnth, double B, double nu, double sinB) {
    if (!(nnth > 0) || !(sinB > 0) || !(B > 0)) return 0;
    const double nuc = cE * B / (2 * kPi * cMe * cC), p = kP;
    // (27) with 3^((p+1)/2) and Gamma((3p+2)/12) in place of the printed 3^(p/2) and Gamma((3p+12)/12): the form that equals the direct integral
    // (aPowerIntegral, Rybicki & Lightman (6.50)) to 1e-6; the printed one is 0.563 of it (tests/test_blackhole.cpp testHotEmissivity; REPORT 0.80.0)
    return nnth * cE * cE / (nu * cMe * cC) * std::pow(3.0, (p + 1) / 2) * (p - 1) / (4 * std::pow(kGammaMin, 1 - p)) * std::tgamma((3 * p + 2) / 12) *
           std::tgamma((3 * p + 22) / 12) * std::pow(nu / (nuc * sinB), -(p + 2) / 2);
}

// ---- the tests' independent reference: G. B. Rybicki, A. P. Lightman, "Radiative Processes in Astrophysics" (1979), the one-electron spectrum (6.17c),
// (6.18): P(nu) = sqrt(3) e^3 B sin(a) / (m c^2) F(nu / nu_cr), nu_cr = (3/2) gamma^2 nu_c sin(a), F(x) = x Int_x^inf K_5/3(t) dt; an isotropic
// distribution N(gamma): j = (1/4 pi) Int N P dgamma, and (6.50) alpha = -(c^2 / 8 pi nu^2) Int P E^2 d/dE (N / E^2) dE, which for N ~ gamma^-p is
// (p + 2) / (8 pi m nu^2) Int P N / gamma dgamma.
namespace {
struct FTable {
    std::vector<double> lx, lf;   // ln x, ln F(x)
    FTable() {
        const int n = 6000;
        const double l0 = std::log(1e-9), l1 = std::log(80.0);
        std::vector<double> t(n), k(n);
        for (int i = 0; i < n; i++) { t[i] = std::exp(l0 + (l1 - l0) * i / (n - 1)); k[i] = std::cyl_bessel_k(5.0 / 3, t[i]) * t[i]; }   // K dt = K t dln t
        std::vector<double> cum(n, 0);
        const double h = (l1 - l0) / (n - 1);
        for (int i = n - 2; i >= 0; i--) {   // Simpson on each interval with its middle
            const double tm = std::exp(l0 + h * (i + 0.5)), km = std::cyl_bessel_k(5.0 / 3, tm) * tm;
            cum[i] = cum[i + 1] + h / 6 * (k[i] + 4 * km + k[i + 1]);
        }
        for (int i = 0; i < n; i++) if (cum[i] > 0) { lx.push_back(std::log(t[i])); lf.push_back(std::log(t[i] * cum[i])); }
    }
    double F(double x) const {
        const double l = std::log(x);
        if (l <= lx.front() || l >= lx.back()) return 0;
        const double q = (l - lx.front()) / (lx.back() - lx.front()) * (lx.size() - 1);
        const std::size_t i = std::min(static_cast<std::size_t>(q), lx.size() - 2);
        const double f = q - i;
        return std::exp(lf[i] + f * (lf[i + 1] - lf[i]));
    }
};
const FTable& fTable() { static const FTable t; return t; }
double powerIntegral(double nnth, double B, double nu, double sinB, double extraPow) {
    const FTable& F = fTable();
    const double nuc = cE * B / (2 * kPi * cMe * cC), p = kP;
    const double l0 = std::log(kGammaMin), l1 = std::log(kGammaMin * 1e7);
    const int n = 28000;
    double s = 0;
    for (int i = 0; i <= n; i++) {
        const double lg = l0 + (l1 - l0) * i / n, g = std::exp(lg);
        const double N = nnth * (p - 1) * std::pow(kGammaMin, p - 1) * std::pow(g, -p);
        const double P = std::sqrt(3.0) * cE * cE * cE * B * sinB / (cMe * cC * cC) * F.F(nu / (1.5 * g * g * nuc * sinB));
        const double w = (i == 0 || i == n) ? 0.5 : 1;
        s += w * N * P * std::pow(g, extraPow) * g;            // dgamma = gamma dln gamma
    }
    return s * (l1 - l0) / n;
}
}  // namespace
double jPowerIntegral(double nnth, double B, double nu, double sinB) { return powerIntegral(nnth, B, nu, sinB, 0) / (4 * kPi); }
namespace {
// j / (n e^2 nu_c / c) and alpha / (n e^2 / (nu m c)) depend on X = nu / (nu_c sin) and sin only through X and one factor sin (j) or none (alpha):
// tabulated at sin = 1 in ln X from 1 to 1e14 (ratio to the far limit (26), (27), which is reached far above gamma_min^2)
struct CutTable {
    std::vector<double> lx, rj, ra;
    CutTable() {
        const int n = 281;
        for (int i = 0; i < n; i++) {
            const double l = std::log(1.0) + (std::log(1e14) - std::log(1.0)) * i / (n - 1), X = std::exp(l);
            const double nuc = cE * 1.0 / (2 * kPi * cMe * cC), nu = X * nuc;   // B = 1 G, sin = 1
            lx.push_back(l);
            rj.push_back(jPowerIntegral(1, 1, nu, 1) / jPower(1, 1, nu, 1));
            ra.push_back(aPowerIntegral(1, 1, nu, 1) / aPower(1, 1, nu, 1));
        }
    }
    double at(const std::vector<double>& v, double X) const {
        const double l = std::log(X);
        if (l >= lx.back()) return v.back();
        const double q = std::max(0.0, (l - lx.front()) / (lx.back() - lx.front()) * (lx.size() - 1));
        const std::size_t i = std::min(static_cast<std::size_t>(q), lx.size() - 2);
        const double f = q - i;
        return v[i] + f * (v[i + 1] - v[i]);
    }
};
const CutTable& cutTable() { static const CutTable t; return t; }
}  // namespace
double jPowerCut(double nnth, double B, double nu, double sinB) {
    if (!(nnth > 0) || !(sinB > 0) || !(B > 0)) return 0;
    const double X = nu / (cE * B / (2 * kPi * cMe * cC) * sinB);
    return jPower(nnth, B, nu, sinB) * cutTable().at(cutTable().rj, std::max(X, 1.0));
}
double aPowerCut(double nnth, double B, double nu, double sinB) {
    if (!(nnth > 0) || !(sinB > 0) || !(B > 0)) return 0;
    const double X = nu / (cE * B / (2 * kPi * cMe * cC) * sinB);
    return aPower(nnth, B, nu, sinB) * cutTable().at(cutTable().ra, std::max(X, 1.0));
}
double aPowerIntegral(double nnth, double B, double nu, double sinB) {
    return (kP + 2) / (8 * kPi * cMe * nu * nu) * powerIntegral(nnth, B, nu, sinB, -1);
}

void velocity0(double r, double th, double& ut, double& ur, double& uph) {
    const double s = std::sin(th), R = r * s, f = 1 - 2 / r;
    if (R >= 6) {
        const double Om = 1 / (R * std::sqrt(R));
        ut = 1 / std::sqrt(std::max(f - r * r * s * s * Om * Om, 1e-12)); ur = 0; uph = Om * ut;
        return;
    }
    const double E = orbitEnergy(6, 0), L = orbitAngMom(6, 0);
    const double ut0 = E / f, ur0 = -std::sqrt(std::max(0.0, E * E - f * (1 + L * L / (r * r)))), up0 = L / (r * r);
    const double vr = ur0 / ut0, Om = up0 / ut0;
    ut = 1 / std::sqrt(std::max(1e-12, f - vr * vr / f - r * r * s * s * Om * Om));
    ur = vr * ut; uph = Om * ut;
}

Shift shift0(double r, double th, double pr, double Lz) {
    Shift o;
    double ut, ur, uph;
    velocity0(r, th, ut, ur, uph);
    const double E = ut - Lz * uph - pr * ur;                  // -p.u with p_t = -1, p_phi = Lz
    if (!(E > 0)) return o;
    o.g = 1 / E;
    const double s = std::sin(th), gtt = -(1 - 2 / r), gpp = r * r * s * s, grr = 1 / (1 - 2 / r);
    const double bt = gpp * uph, bp = -gtt * ut;                // the toroidal field's direction, orthogonal to u
    const double nb2 = gtt * gpp * (gpp * uph * uph + gtt * ut * ut);
    const double n2 = nb2 > 0 ? nb2 : gtt * gpp * (-1 - grr * ur * ur);
    const double kb = -bt + Lz * bp;
    const double c = std::clamp(kb / (std::sqrt(std::max(n2, 1e-300)) * E), -1.0, 1.0);
    o.sinB = std::sqrt(1 - c * c);
    return o;
}

void addLight(double g, double j, double a, double dlam, double& I, double& tau) {
    if (!(g > 0) || !(j > 0)) return;
    const double dt = a * dlam / g;
    const double att = std::exp(-tau);
    if (dt > 1e-12) I += g * g * g * (j / a) * (-std::expm1(-dt)) * att;
    else I += g * g * j * dlam * att;
    tau += dt;
}

namespace {
// one step's light: the point (r, cos theta), the light's p_r and Lz, the step's affine length dlam (cm); adds to I and tau
inline void addStep(const Model& m, BReading rd, double nu, const Parts& parts, double r, double cth, double pr, double Lz, double dlam,
                    double& I, double& tau) {
    if (!(r > 2) || r > kRMax) return;
    cth = std::clamp(cth, -1.0, 1.0);
    const double th = std::acos(cth);
    const Plasma q = plasma(m, r, th, rd);
    if (!(q.ne > 0) && !(q.nnth > 0)) return;
    const Shift sh = shift0(r, th, pr, Lz);
    if (!(sh.g > 0) || !(sh.sinB > 1e-9)) return;
    const double nuG = nu / sh.g;
    double j = 0, a = 0;
    if (parts.thermal) { const double jt = jThermal(q.ne, q.Te, q.B, nuG, sh.sinB); j += jt; if (parts.absorb && jt > 0) a += jt / planckNu(nuG, q.Te); }
    if (parts.power) {
        j += parts.cut ? jPowerCut(q.nnth, q.B, nuG, sh.sinB) : jPower(q.nnth, q.B, nuG, sh.sinB);
        if (parts.absorb) a += parts.cut ? aPowerCut(q.nnth, q.B, nuG, sh.sinB) : aPower(q.nnth, q.B, nuG, sh.sinB);
    }
    addLight(sh.g, j, a, dlam, I, tau);
}
}  // namespace

Light rayLight(const Model& m, double massSun, double nu, const RayIn& in, double eps, Parts parts, BReading rd) {
    Light L;
    const double rg = gravRadiusM(massSun) * 100;              // cm
    const double psi = std::clamp(in.psi, 1e-12, kPi - 1e-12);
    const double b = cameraImpact(in.rO, psi);
    double u = 1 / in.rO, w = -u * std::sqrt(1 - 2 * u) * std::cos(psi) / std::sin(psi), phi = 0;
    const double uMin = 1 / kRMax;
    const auto f = [](double uu) { return 3 * uu * uu - uu; };
    for (int it = 0; it < 4000000; it++) {
        if (!(u > 0) || u >= 0.5) break;
        if (w < 0 && u < uMin) break;                          // going out beyond the flow: it never comes back (u'' < 0 there)
        const double h = std::min(0.02, eps * std::min(u, 0.5 - u + 1e-3) / std::max(std::fabs(w), 1e-300));
        // RK4 on (u, w)
        const double k1u = w, k1w = f(u);
        const double k2u = w + h / 2 * k1w, k2w = f(u + h / 2 * k1u);
        const double k3u = w + h / 2 * k2w, k3w = f(u + h / 2 * k2u);
        const double k4u = w + h * k3w, k4w = f(u + h * k3u);
        const double u1 = u + h / 6 * (k1u + 2 * k2u + 2 * k3u + k4u), w1 = w + h / 6 * (k1w + 2 * k2w + 2 * k3w + k4w);
        const double um = u + h / 2 * k1u + h * h / 8 * k1w;   // the step's middle (second order)
        const double wm = (w + w1) / 2, pm = phi + h / 2;
        if (um > uMin && um < 0.5) {
            const double r = 1 / um, cth = in.A * std::cos(pm) + in.B * std::sin(pm);
            const double pr = b * wm / (1 - 2 * um);
            addStep(m, rd, nu, parts, r, cth, pr, in.Lz, h / (b * um * um) * rg, L.I, L.tau);
        }
        u = u1; w = w1; phi += h;
    }
    return L;
}

void viewGeometry(double tiltDeg, double chi, double& A, double& B, double& LzOverB) {
    const double t = tiltDeg * kPi / 180, sb = std::sin(t), cb = std::cos(t);
    const double nx = std::cos(chi), ny = std::sin(chi);
    A = cb; B = -ny * sb; LzOverB = nx * sb;                    // N = (0, -sin tilt, cos tilt) on (right, up, towards the camera): lab.h discAxisView
}

double farFluxJy(const Model& m, double massSun, double distPc, double nu, double inclDeg, BReading rd, Parts parts, int nb, int nchi, double bMax,
                 double eps) {
    const double rO = 1e5, rg = gravRadiusM(massSun) * 100, D = distPc * parsecM() * 100;
    std::vector<double> rowSum(static_cast<std::size_t>(nb), 0);
    parallelFor(nb, [&](int k) {
        const double b = (k + 0.5) * bMax / nb;
        double s = 0;
        for (int c = 0; c < nchi; c++) {
            const double chi = (c + 0.5) * kPi / nchi;           // [0, pi]; -chi gives the same light (the flow's mirror symmetry in its plane)
            RayIn in;
            in.rO = rO; in.psi = kPi - std::asin(b * std::sqrt(1 - 2 / rO) / rO);
            double lz;
            viewGeometry(inclDeg, chi, in.A, in.B, lz);
            in.Lz = b * lz;
            s += rayLight(m, massSun, nu, in, eps, parts, rd).I;
        }
        rowSum[static_cast<std::size_t>(k)] = s * 2 * (kPi / nchi) * b * (bMax / nb);
    });
    double F = 0;
    for (double v : rowSum) F += v;
    return F * rg * rg / (D * D) / 1e-23;
}

void powerLawColour(double alpha, double& x, double& y) {
    double X = 0, Y = 0, Z = 0;
    for (int k = 0; k < cie::kCmfN; k++) {
        const double lam = (cie::kCmfNm0 + k * cie::kCmfStepNm) * 1e-9, nu = 2.99792458e8 / lam;
        const double Il = std::pow(nu, -alpha) / (lam * lam);
        X += Il * cie::kCmf[k][0]; Y += Il * cie::kCmf[k][1]; Z += Il * cie::kCmf[k][2];
    }
    x = X / (X + Y + Z); y = Y / (X + Y + Z);
}
}  // namespace hot
}  // namespace blackhole
