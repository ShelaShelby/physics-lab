// 0.78.1 research (native/PLAN_DISC.md): prototype pictures of a realistic gas disc for the Black Hole Lab. Processor only, double precision, the lab's own
// ray paths (geodesic.h KerrBL and trace, J15 (A.15), every accepted state kept). No change to the app. Each picture: today's disc beside the proposed one at
// the same view and the same exposure. Usage:
//   discproto real <out.png>   the feeding preset (10^8 Suns, 0.1 L_Edd, spin 0), Real: today (thin disc + 0.72's patches and clump) | proposed (true
//                              thickness, fading inner edge, streaks)
//   discproto sgra <out.png>   Sagittarius A*, Real: today (the thin disc one click away, 0.1 L_Edd) | the hot flow (BL06's RIAF) at the same exposure |
//                              the hot flow at its own exposure (the factor printed)
//   discproto film <out.png>   Film (J15's disc): today | proposed (soft edges, streaks, partly see-through; the lab's own, labelled)
//   discproto real079 <out.png> 0.79.0: today | the app's new disc (gas.h streaks, disc.h thickness table, the clump), each at its own automatic exposure
//   discproto count            steps per ray and samples per ray for the GPU estimate (PLAN_DISC.md section 6)
// Every number is from a named source (PLAN_DISC.md sections 2-5) or is marked CHOICE (the prototype's own, labelled in the plan).
//
// THE VIEW: the lab's starting view (lab.h): camera hovering at 50 M, 10 degrees from the disc's plane, 45 degrees across the picture's height, the hole in
//   the middle; the equidistant projection of the lab (schwarzschild.h HoleView: the angle from the middle is the distance from it). The camera sits at
//   theta = 100 degrees (lab.h kTiltDefaultDeg), phi = 0, screen up = e_theta (towards -z), so the disc is below the hole as in the lab.
//   The gas turns anticlockwise seen from +z (lab.h): the side coming towards the camera is on the right.
// THE EXPOSURE: Real: the lab's camera curve (tone.h, Reinhard et al. 2002) made from TODAY's disc pixels and used for both panels. Film: the screen's white
//   at the film disc's own colour (its largest channel), then the lab's own glow (looks.h glowReference) on both panels. No sky (the lab's black background).
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "app/shot.h"
#include "blackhole/disc.h"
#include "blackhole/gas.h"
#include "blackhole/geodesic.h"
#include "blackhole/kerr.h"
#include "blackhole/lab.h"
#include "blackhole/looks.h"
#include "blackhole/presets.h"
#include "blackhole/spin.h"
#include "blackhole/tone.h"
#include "render/cie1931.h"
#include "patches078.h"

using namespace blackhole;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kW = 480, kH = 300;           // one panel (CHOICE: the processor's time; 2 x 2 rays a pixel)
constexpr int kSub = 2;

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double encS(double l) { l = std::clamp(l, 0.0, 1.0); return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055; }   // IEC 61966-2-1

// ---------------- the view ----------------
struct View { double rC = kDistDefault, thC = kTiltDefaultDeg * kPi / 180, fovDeg = kFovDefault; };
// the look direction (N_r, N_theta, N_phi) on the FIDO's frame for picture point (X, Y) (pixels, y down)
Vec3 lookOf(const View& v, double X, double Y) {
    const double K = (kH / 2.0) / (v.fovDeg / 2 * kPi / 180);
    const double dx = X - kW / 2.0, dy = kH / 2.0 - Y, d = std::hypot(dx, dy), t = d / K;
    if (d == 0) return Vec3{-1, 0, 0};
    const double s = std::sin(t);
    return Vec3{-std::cos(t), s * dy / d, -s * dx / d};   // forward = -e_r, up = e_theta (-z), right = -e_phi
}
double pixelSolidAngle(const View& v, double X, double Y) {   // sr (light.h: sin(th) / (th K^2) per square pixel of the equidistant map)
    const double K = (kH / 2.0) / (v.fovDeg / 2 * kPi / 180);
    const double d = std::hypot(X - kW / 2.0, kH / 2.0 - Y), t = d / K;
    return t > 1e-12 ? std::sin(t) / (t * K * K) : 1 / (K * K);
}

// one ray: its whole path (5 numbers a state), b, and the crossings of the equator
struct RayPath { std::vector<double> path, cross; double b = 0, q = 0; RayEnd end; };
RayPath traceRay(const KerrBL& ch, const View& v, double X, double Y) {
    RayPath p;
    Camera cam; cam.r = v.rC; cam.theta = v.thC; cam.phi = 0;
    const Vec3 look = lookOf(v, X, Y);
    Ray ray;
    ch.launch(cam, look, ray);
    p.b = ray.c[0]; p.q = ray.c[1];
    TraceOptions o; o.maxCross = 4; o.cross = &p.cross;
    p.end = trace(ch, cam, look, o, &p.path);
    return p;
}

// ---------------- the structure: a Gaussian random field (Lee & Gammie 2021) ----------------
// Lee & Gammie 2021 (ApJ 906, 39; arXiv:2011.07151) (13): P(k) proportional to 1 / (1 + k.Lambda k)^2 (the Matern field of their (3) with their sect. 4.1's
// choice), Lambda = lambda1^2 u1 u1 + lambda2^2 u2 u2 (6), lambda1 proportional to r (sect. 4.2), surface brightness mu = g exp(f / n) (15).
// Here in (x = ln R, y = phi), where lambda proportional to R makes the field homogeneous. u1 at kTilt from the azimuth, trailing.
//   kTilt = 15 deg: Guan, Gammie, Simon & Johnson 2009 (ApJ 694, 1010; arXiv:0901.0273) Table 2, theta_B,tilt of the converged net-azimuthal runs (15-17 deg);
//   lambda1 / R = 25 deg: Schnittman, Krolik & Hawley 2006 section 5, the patches' azimuthal size (the knee of the spectrum: CHOICE of which length);
//   lambda2 / lambda1 = 0.049 / 0.27: Guan et al. 2009 Table 2, y256a (lambda_B,min / lambda_B,maj);
//   amplitude: S = exp(c f - c^2 / 2), f of unit variance (mean of S exactly 1: light moved, not added); c = kAmp: CHOICE for the picture (the build job
//   calibrates it to SKH06 Table 2's 43.8 % rms of the 6-10 M ring with the field moving).
constexpr double kTilt = 15 * kPi / 180, kLam1 = 25 * kPi / 180, kLamRatio = 0.049 / 0.27, kAmp = 0.6;
struct Field {
    int nx = 1024, ny = 2048;
    double x0 = std::log(1.5), x1 = std::log(4000.0);
    std::vector<double> f;
    void fft(std::vector<std::complex<double>>& a, int n, int stride, int off, bool inv) {   // radix 2, in place, one line
        std::vector<std::complex<double>> t(n);
        for (int i = 0; i < n; i++) t[i] = a[off + i * stride];
        for (int i = 1, j = 0; i < n; i++) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(t[i], t[j]);
        }
        for (int len = 2; len <= n; len <<= 1) {
            const double ang = 2 * kPi / len * (inv ? 1 : -1);
            const std::complex<double> wl(std::cos(ang), std::sin(ang));
            for (int i = 0; i < n; i += len) {
                std::complex<double> w(1);
                for (int k = 0; k < len / 2; k++) {
                    const std::complex<double> u = t[i + k], vv = t[i + k + len / 2] * w;
                    t[i + k] = u + vv; t[i + k + len / 2] = u - vv; w *= wl;
                }
            }
        }
        for (int i = 0; i < n; i++) a[off + i * stride] = t[i];
    }
    void make(unsigned seed) {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> N01(0, 1);
        std::vector<std::complex<double>> a(static_cast<std::size_t>(nx) * ny);
        for (auto& z : a) z = N01(rng);
        for (int y = 0; y < ny; y++) fft(a, nx, 1, y * nx, false);
        for (int x = 0; x < nx; x++) fft(a, ny, nx, x, false);
        const double dx = (x1 - x0) / nx, dy = 2 * kPi / ny;
        const double u1x = -std::sin(kTilt), u1y = std::cos(kTilt), u2x = u1y, u2y = -u1x;
        const double l1 = kLam1, l2 = kLam1 * kLamRatio;
        for (int y = 0; y < ny; y++)
            for (int x = 0; x < nx; x++) {
                const double kx = 2 * kPi * (x <= nx / 2 ? x : x - nx) / (nx * dx), ky = 2 * kPi * (y <= ny / 2 ? y : y - ny) / (ny * dy);
                const double k1 = kx * u1x + ky * u1y, k2 = kx * u2x + ky * u2y;
                const double q = 1 + l1 * l1 * k1 * k1 + l2 * l2 * k2 * k2;
                a[static_cast<std::size_t>(y) * nx + x] *= 1 / q;          // sqrt of P = 1 / q^2
            }
        for (int x = 0; x < nx; x++) fft(a, ny, nx, x, true);
        for (int y = 0; y < ny; y++) fft(a, nx, 1, y * nx, true);
        f.resize(a.size());
        double m = 0, s2 = 0;
        for (std::size_t i = 0; i < a.size(); i++) { f[i] = a[i].real(); m += f[i]; }
        m /= static_cast<double>(f.size());
        for (double& z : f) { z -= m; s2 += z * z; }
        const double sd = std::sqrt(s2 / static_cast<double>(f.size()));
        for (double& z : f) z /= sd;
    }
    double at(double R, double phi) const {   // bilinear; x clamped (padding), phi periodic
        const double fx = (std::log(R) - x0) / (x1 - x0) * nx - 0.5;
        double py = std::fmod(phi, 2 * kPi); if (py < 0) py += 2 * kPi;
        const double fy = py / (2 * kPi) * ny - 0.5;
        const int ix = std::clamp(static_cast<int>(std::floor(fx)), 0, nx - 2);
        const double ax = std::clamp(fx - ix, 0.0, 1.0);
        const int iy0 = static_cast<int>(std::floor(fy));
        const double ay = fy - iy0;
        const int iy = (iy0 % ny + ny) % ny, iy1 = (iy + 1) % ny;
        const auto F = [&](int xx, int yy) { return f[static_cast<std::size_t>(yy) * nx + xx]; };
        return (1 - ay) * ((1 - ax) * F(ix, iy) + ax * F(ix + 1, iy)) + ay * ((1 - ax) * F(ix, iy1) + ax * F(ix + 1, iy1));
    }
    double S(double R, double phi) const { const double z = at(R, phi); return std::exp(kAmp * z - kAmp * kAmp / 2); }
};

// ---------------- the thin disc's true thickness ----------------
// Zhu, Davis, Narayan, Kulkarni, Penna & McClintock 2012 (MNRAS 424, 2504; arXiv:1202.1530) App. B, one zone: (B1a) P_tot / h = rho Q h, (B1b) P_rad = F_tot kappa
// Sigma / (4 c), (B1f) Sigma = 2 h rho. Where radiation pressure dominates (P_tot = P_rad): h = kappa F / (2 c Q) (computed here). Q = Omega_k^2 R_z, (5)-(7),
// R_z = (u_phi^2 + a^2 (...)) / r (6); at a = 0 Q = u_phi^2 / r^4 (M = 1), u_phi = L / mu = r / (r - 3)^(1/2) (BPT (2.13), a = 0). F = Mdot c^2 / r_g^2 x
// discFlux (disc.h, PT). With L_Edd = 4 pi G M c / kappa (YN footnote 1) and L = eta Mdot c^2 = l L_Edd (disc.h DiscPhys::mdot): h / M = 2 pi (l / eta) discFlux / Q.
double hRad(double R, double l, double eta) {
    if (R <= discInner(0)) return 0;
    const double uphi2 = R * R / (R - 3), Q = uphi2 / (R * R * R * R);
    return 2 * kPi * (l / eta) * discFlux(R, 0) / Q;
}

// ---------------- Sagittarius A*'s hot flow (BL06) ----------------
// A. E. Broderick, A. Loeb, MNRAS 367, 905 (2006), arXiv:astro-ph/0509237 ("BL06") section 2.2 (4) and Table 1 (a = 0): n_e = n0 (rho/M)^-1.1 exp(-z^2/2rho^2),
//   T_e = T0 (r/M)^-0.84, n_nth = n0nth (rho/M)^pnth exp(-z^2/2rho^2); n0 = 3e7 cm^-3, T0 = 1.7e11 K, n0nth = 8e4 cm^-3, pnth = -2.9; the non-thermal electrons'
//   spectral index 1.25 (p = 2 x 1.25 + 1 = 3.5), gamma_min = 100; B at 30 % of equipartition with the protons; toroidal; Keplerian outside the ISCO, free fall
//   inside. The equipartition energy as written by Broderick, Fish, Doeleman & Loeb 2011 (ApJ 735, 110; arXiv:1011.2770) (4): B^2 / 8 pi = beta^-1 n m_p c^2
//   r_S / (12 r), here with beta^-1 = 0.3 (BL06's 30 %: the build job confirms this reading against BL06 Fig. 1's spectrum).
// Emissivities: Pandya, Zhang, Chandra & Gammie 2016 (ApJ 822, 34; arXiv:1602.08749): thermal (24) (Leung et al. 2011's fit), power law (26), its absorptivity (27).
constexpr double kN0 = 3e7, kT0 = 1.7e11, kN0nth = 8e4, kPnth = -2.9, kP = 3.5, kGmin = 100, kBeta1 = 0.3;
// cgs constants (CODATA 2018 exact h, k, c, e; m_e, m_p: CODATA 2018)
constexpr double cE = 4.80320471e-10, cMe = 9.1093837015e-28, cMp = 1.67262192369e-24, cC = 2.99792458e10, cK = 1.380649e-16, cH = 6.62607015e-27;
struct Hot { double nu[2] = {cC / 550e-7, cC / 2.2e-4}; };   // 550 nm (the eye's middle), 2.2 um (GRAVITY 2020's K band)
// the gas's 4-velocity at (r, theta), a = 0: u^t, u^r, u^phi (contravariant)
void hotVelocity(double r, double th, double& ut, double& ur, double& uph) {
    const double R = r * std::sin(th);
    if (R >= 6) {
        const double Om = 1 / (R * std::sqrt(R));
        const double d = 1 - 2 / r - r * r * std::sin(th) * std::sin(th) * Om * Om;
        ut = 1 / std::sqrt(std::max(d, 1e-12)); ur = 0; uph = Om * ut;
        return;
    }
    // inside: the ISCO's E and L kept (free fall, BL06), dr/dt and dphi/dt of that equatorial plunge at the same r, then normalised at (r, theta)
    const double E = orbitEnergy(6, 0), L = orbitAngMom(6, 0), f = 1 - 2 / r;
    const double ut0 = E / f, ur0 = -std::sqrt(std::max(0.0, E * E - f * (1 + L * L / (r * r)))), up0 = L / (r * r);
    const double vr = ur0 / ut0, Om = up0 / ut0, s2 = std::sin(th) * std::sin(th);
    ut = 1 / std::sqrt(std::max(1e-12, f - vr * vr / f - r * r * s2 * Om * Om));
    ur = vr * ut; uph = Om * ut;
}
// j_nu (erg s^-1 cm^-3 Hz^-1 sr^-1) at frequency nu in the gas's frame, sin of the angle to B: thermal and power law
double jThermal(double ne, double Te, double B, double nu, double sth) {
    const double Th = cK * Te / (cMe * cC * cC), nuc = cE * B / (2 * kPi * cMe * cC), nus = 2.0 / 9 * nuc * Th * Th * sth;
    if (!(nus > 0)) return 0;
    const double X = nu / nus, k2 = std::cyl_bessel_k(2.0, 1 / Th);
    const double t = std::sqrt(X) + std::pow(2.0, 11.0 / 12) * std::cbrt(std::sqrt(X));
    return ne * std::sqrt(2.0) * kPi * cE * cE * nus / (3 * k2 * cC) * t * t * std::exp(-std::cbrt(X));
}
double jPower(double nn, double B, double nu, double sth) {
    const double nuc = cE * B / (2 * kPi * cMe * cC), p = kP;
    return nn * cE * cE * nuc / cC * std::pow(3.0, p / 2) * (p - 1) * sth / (2 * (p + 1) * std::pow(kGmin, 1 - p)) * std::tgamma((3 * p - 1) / 12) *
           std::tgamma((3 * p + 19) / 12) * std::pow(nu / (nuc * sth), -(p - 1) / 2);
}
double aPower(double nn, double B, double nu, double sth) {
    const double nuc = cE * B / (2 * kPi * cMe * cC), p = kP;
    return nn * cE * cE / (nu * cMe * cC) * std::pow(3.0, (p + 1) / 2) * (p - 1) / (4 * std::pow(kGmin, 1 - p)) * std::tgamma((3 * p + 12) / 12) *
           std::tgamma((3 * p + 22) / 12) * std::pow(nu / (nuc * sth), -(p + 2) / 2);
}

struct Panel { std::vector<std::array<double, 3>> rgb; std::vector<double> Y; std::vector<unsigned char> on; };
Panel newPanel() { Panel p; p.rgb.assign(kW * kH, {0, 0, 0}); p.Y.assign(kW * kH, 0); p.on.assign(kW * kH, 0); return p; }

template <class F> void forRows(F f) {
    std::atomic<int> next{0};
    const int nT = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> th;
    for (int i = 0; i < nT; i++) th.emplace_back([&] { for (int y; (y = next++) < kH;) f(y); });
    for (auto& t : th) t.join();
}

// interpolated state between two path states (linear; the steps are short where the gas is: the counts are printed)
void lerp5(const double* a, const double* b, double t, double* o) { for (int i = 0; i < 5; i++) o[i] = a[i] + t * (b[i] - a[i]); }

// today's light at a crossing: the lab's rule (lab.cpp discLight) times 0.72's moving gas (the pattern's factor and the clump, at one moment, no travel time)
std::array<double, 3> todayLight(const Settings& s, double r, double phi, double g, const patches078::GasPattern& gp, const Clump& cl) {
    const double fac = gp.factor(r, phi, 0) * (1 + cl.A * cl.density(r, phi, 0));
    DiscPixel q;
    discLight(s, r, g, q);
    if (s.look == Look::Film) { for (double& c : q.rgb) c *= fac; return q.rgb; }
    const double T = discPhys(s).temperature(r) * std::pow(fac, 0.25);   // flux x fac: T x fac^(1/4) (render/shaders.h kGasPart's Real rule)
    return blackBodyLinearFast(g * T);
}

void writePng(const std::string& path, const std::vector<std::array<double, 3>>& img, int W, int H) {
    std::vector<uint8_t> rgb(static_cast<std::size_t>(W) * H * 3);
    for (std::size_t i = 0; i < img.size(); i++)
        for (int c = 0; c < 3; c++) rgb[3 * i + c] = static_cast<uint8_t>(std::lround(255 * encS(img[i][c])));
    const std::vector<uint8_t> png = app::encodePng(W, H, rgb.data());
    FILE* fo = std::fopen(path.c_str(), "wb");
    if (!fo) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
    std::fwrite(png.data(), 1, png.size(), fo);
    std::fclose(fo);
}
// panels side by side with a 4 px grey gap
std::vector<std::array<double, 3>> joinPanels(const std::vector<std::vector<std::array<double, 3>>>& ps, int& W) {
    const int n = static_cast<int>(ps.size()), gap = 4;
    W = n * kW + (n - 1) * gap;
    std::vector<std::array<double, 3>> o(static_cast<std::size_t>(W) * kH, {0.2, 0.2, 0.2});
    for (int k = 0; k < n; k++)
        for (int y = 0; y < kH; y++)
            for (int x = 0; x < kW; x++) o[static_cast<std::size_t>(y) * W + k * (kW + gap) + x] = ps[k][static_cast<std::size_t>(y) * kW + x];
    return o;
}

// ---------------- the three pictures ----------------
std::atomic<long> g_steps{0}, g_samples{0}, g_rays{0};

int realPicture(const std::string& out, bool app079 = false) {
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    const DiscPhys P = discPhys(s);
    const double eta = discEfficiency(0), l = s.feed, rIn = discInner(0), rOut = s.outer;
    const View v;
    const KerrBL ch(0);
    Field fld; fld.make(78101);
    patches078::GasPattern gp = patches078::GasPattern::reduced(); gp.a = 0; gp.rIn = rIn; gp.rOut = rOut;
    Clump cl; cl.a = 0; cl.rc = gasClumpRadius(s); cl.init();
    Panel A = newPanel(), B = newPanel();
    double hMax = 0, hrMax = 0, hrAt = 0;
    // 0.79.0 (app079): the app's own model on the right: the thickness table with the gas's pressure (disc.h heightTable, Zhu App. B), the streaks (gas.h
    // StreakField) and the clump at the same moment as the left's (t = 0, no travel time)
    const HeightTable ht = heightTable(P);
    const auto hOf = [&](double R) { return app079 ? ht.at(R) : hRad(R, l, eta); };
    for (double R = rIn; R < rOut; R *= 1.001) { const double h = hOf(R); if (R < 200 && h / R > hrMax) { hrMax = h / R; hrAt = R; } hMax = std::max(hMax, h); }
    const double t0 = now();
    forRows([&](int y) {
        for (int x = 0; x < kW; x++) {
            std::array<double, 3> ca{0, 0, 0}, cb{0, 0, 0};
            int na = 0, nb = 0;
            for (int k = 0; k < kSub * kSub; k++) {
                const double X = x + (0.5 + (k % kSub)) / kSub, Y = y + (0.5 + (k / kSub)) / kSub;
                const RayPath p = traceRay(ch, v, X, Y);
                g_rays++; g_steps += p.end.steps;
                // today: the first crossing on the disc
                for (std::size_t c = 0; c + 5 <= p.cross.size(); c += 5) {
                    const double r = p.cross[c];
                    if (r >= rIn && r <= rOut) {
                        const auto L = todayLight(s, r, p.cross[c + 2], discShiftKerr(r, p.b, 0), gp, cl);
                        for (int q = 0; q < 3; q++) ca[q] += L[q];
                        na++;
                        break;
                    }
                }
                // proposed: the first point of the path inside |z| <= h(R) (the disc's surface, opaque), found by sub-steps and bisection
                const std::vector<double>& pa = p.path;
                bool hit = false;
                for (std::size_t i = 5; i + 5 <= pa.size() && !hit; i += 5) {
                    const double* a = &pa[i - 5]; const double* b = &pa[i];
                    const double za = a[0] * std::cos(a[1]), zb = b[0] * std::cos(b[1]);
                    const double Ra = a[0] * std::sin(a[1]), Rb = b[0] * std::sin(b[1]);
                    if (std::min(Ra, Rb) > rOut * 1.01 || (std::min(std::fabs(za), std::fabs(zb)) > hMax * 1.01 && za * zb > 0)) continue;
                    const double span = std::max({std::fabs(zb - za), std::fabs(Rb - Ra) * 0.05, 1e-6});
                    const int n = std::clamp(static_cast<int>(std::ceil(span / 0.02)), 1, 4000);
                    double prev[5]; std::copy(a, a + 5, prev);
                    for (int j = 1; j <= n; j++) {
                        double cur[5]; lerp5(a, b, static_cast<double>(j) / n, cur);
                        g_samples++;
                        double R = cur[0] * std::sin(cur[1]), z = cur[0] * std::cos(cur[1]);
                        const double zp = prev[0] * std::cos(prev[1]);
                        if (zp * z < 0) {                                   // the plane crossed between two sub-steps: the ray went through the slab
                            double lo = 0, hi = 1, st[5];                  // (where h is below the sub-step, as at the inner edge): end the sub-step at the plane
                            for (int it = 0; it < 60; it++) {
                                const double m = (lo + hi) / 2;
                                lerp5(prev, cur, m, st);
                                if ((st[0] * std::cos(st[1])) * zp > 0) lo = m; else hi = m;
                            }
                            lerp5(prev, cur, hi, cur);
                            R = cur[0] * std::sin(cur[1]); z = 0;
                        }
                        if (R >= rIn && R <= rOut && std::fabs(z) <= hOf(R)) {
                            double lo = 0, hi = 1, st[5];                  // bisection between prev (outside) and cur (inside)
                            for (int it = 0; it < 40; it++) {
                                const double m = (lo + hi) / 2;
                                lerp5(prev, cur, m, st);
                                const double Rm = st[0] * std::sin(st[1]), zm = st[0] * std::cos(st[1]);
                                if (Rm >= rIn && Rm <= rOut && std::fabs(zm) <= hOf(Rm)) hi = m; else lo = m;
                            }
                            lerp5(prev, cur, hi, st);
                            const double Rh = st[0] * std::sin(st[1]);
                            const double g = discShiftKerr(Rh, p.b, 0);   // (the gas at R on its equatorial orbit: h/R < 0.07, PLAN_DISC.md section 2)
                            const double Sg = app079 ? streakField().factor(Rh, st[2], 0, 0) * (1 + cl.A * cl.density(Rh, st[2], 0)) : fld.S(Rh, st[2]);
                            const double T = P.temperature(Rh) * std::pow(Sg, 0.25);
                            const auto L = blackBodyLinearFast(g * T);
                            for (int q = 0; q < 3; q++) cb[q] += L[q];
                            nb++;
                            hit = true;
                            break;
                        }
                        std::copy(cur, cur + 5, prev);
                    }
                }
            }
            const std::size_t i = static_cast<std::size_t>(y) * kW + x;
            for (int q = 0; q < 3; q++) { A.rgb[i][q] = ca[q] / (kSub * kSub); B.rgb[i][q] = cb[q] / (kSub * kSub); }
            A.on[i] = na > 0; B.on[i] = nb > 0;
            A.Y[i] = lumaSrgb(A.rgb[i]); B.Y[i] = lumaSrgb(B.rgb[i]);
        }
    });
    const double secs = now() - t0;
    std::vector<double> ys;
    for (std::size_t i = 0; i < A.Y.size(); i++) if (A.on[i]) ys.push_back(A.Y[i]);
    std::sort(ys.begin(), ys.end(), std::greater<double>());
    const ToneExposure e = toneFromSamples(ys, 0);
    std::vector<std::array<double, 3>> a(A.rgb.size()), b(B.rgb.size());
    for (std::size_t i = 0; i < a.size(); i++) { a[i] = toneRGB(A.rgb[i], e); b[i] = toneRGB(B.rgb[i], e); }
    if (app079) {                                                       // each panel at its own automatic exposure (the app's rule, tone.h), and its numbers
        std::vector<double> yb;
        for (std::size_t i = 0; i < B.Y.size(); i++) if (B.on[i]) yb.push_back(B.Y[i]);
        std::sort(yb.begin(), yb.end(), std::greater<double>());
        const ToneExposure eb = toneFromSamples(yb, 0);
        for (std::size_t i = 0; i < b.size(); i++) b[i] = toneRGB(B.rgb[i], eb);
        const auto stats = [&](const Panel& Pn, const std::vector<std::array<double, 3>>& im, const ToneExposure& ex, const char* name) {
            std::vector<double> luma, lnY;
            long n = 0, white = 0, clip = 0;
            double sat = 0;
            for (std::size_t i = 0; i < im.size(); i++) {
                if (!Pn.on[i]) continue;
                n++;
                const double r = encS(im[i][0]), g = encS(im[i][1]), bl = encS(im[i][2]), mx = std::max({r, g, bl}), mn = std::min({r, g, bl});
                if (r >= 254.5 / 255 && g >= 254.5 / 255 && bl >= 254.5 / 255) white++;
                if (mx >= 254.5 / 255) clip++;
                sat += mx > 0 ? (mx - mn) / mx : 0;
                luma.push_back(0.2126 * r + 0.7152 * g + 0.0722 * bl);
                if (Pn.Y[i] > 0) lnY.push_back(std::log(Pn.Y[i]));
            }
            std::sort(luma.begin(), luma.end());
            double m = 0, v = 0;
            for (double q : lnY) m += q;
            m /= static_cast<double>(std::max<std::size_t>(lnY.size(), 1));
            for (double q : lnY) v += (q - m) * (q - m);
            const auto pc = [&](double f) { return luma.empty() ? 0.0 : luma[static_cast<std::size_t>(f * static_cast<double>(luma.size() - 1))]; };
            std::printf("  %s at its own exposure (k %.4g, log-average %.4g, brightest Y %.4g): %ld disc pixels, %.2f %% full white, %.2f %% with a channel at 255; "
                        "screen luma (sRGB 0-1) 5/50/95 %%: %.3f %.3f %.3f; spread of ln Y (scene) %.3f; mean saturation %.3f\n",
                        name, ex.k, ex.logAvg, ex.yMax, n, 100.0 * white / std::max(n, 1L), 100.0 * clip / std::max(n, 1L), pc(0.05), pc(0.5), pc(0.95),
                        std::sqrt(v / static_cast<double>(std::max<std::size_t>(lnY.size(), 1))), sat / std::max(n, 1L));
        };
        stats(A, a, e, "today (0.78.1: thin, patches, clump)");
        stats(B, b, eb, "0.79.0 (surface, streaks, clump)");
    }
    int W = 0;
    const auto joined = joinPanels({a, b}, W);
    writePng(out, joined, W, kH);
    std::printf("real: %s %d x %d (two panels of %d x %d, %d rays a pixel), %.1f s on %u threads\n", out.c_str(), W, kH, kW, kH, kSub * kSub, secs,
                std::thread::hardware_concurrency());
    std::printf("  feeding preset: %.0e Suns, L = %.2f L_Edd, spin 0, eta = %.5f, r_ms = %.4f M, outer %.0f M, hottest %.0f K at %.2f M\n", s.massSun, l, eta, rIn,
                rOut, P.hottest(), [&] { double w; P.hottest(&w); return w; }());
    std::printf("  thickness h (Zhu et al. 2012 App. B, radiation pressure): largest h/R %.4f at R = %.2f M; h at 10, 20, 50, 200, 2000 M: %.3f %.3f %.3f %.3f %.3f M\n",
                hrMax, hrAt, hRad(10, l, eta), hRad(20, l, eta), hRad(50, l, eta), hRad(200, l, eta), hRad(2000, l, eta));
    std::printf("  exposure (tone.h from today's %zu disc samples): k %.4g, log-average %.4g, brightest Y %.4g (Sun's surface = 1)\n", ys.size(), e.k, e.logAvg, e.yMax);
    std::printf("  rays %ld, integrator steps %.1f a ray, sub-samples near the disc %.1f a ray\n", g_rays.load(), double(g_steps) / g_rays, double(g_samples) / g_rays);
    return 0;
}

int sgraPicture(const std::string& out) {
    Settings s = settingsDefaults();                                    // Sagittarius A* (the lab's default preset)
    setDisc(s, true);                                                   // today: the thin disc one click away (0.1 L_Edd, "what if")
    const Preset* pr = preset("sgra");
    const double rg = gravRadiusM(pr->massSun) * 100;                   // cm
    const double D = pr->distPc * parsecM();                            // m
    const View v;
    const KerrBL ch(0);
    patches078::GasPattern gp = patches078::GasPattern::reduced(); gp.a = 0; gp.rIn = discGeom(s).rIn; gp.rOut = discGeom(s).rOut;
    Clump cl; cl.a = 0; cl.rc = gasClumpRadius(s); cl.init();
    const DiscGeom dg = discGeom(s);
    Panel A = newPanel();
    std::vector<double> IV(kW * kH, 0), IK(kW * kH, 0), ITh(kW * kH, 0);   // W m^-2 Hz^-1 sr^-1 at 550 nm (power law), 2.2 um (both), 550 nm thermal
    double tauMax = 0;
    std::vector<double> tauRow(kH, 0);
    const double t0 = now();
    forRows([&](int y) {
        for (int x = 0; x < kW; x++) {
            std::array<double, 3> ca{0, 0, 0};
            int na = 0;
            double iv = 0, ik = 0, ith = 0;
            for (int k = 0; k < kSub * kSub; k++) {
                const double X = x + (0.5 + (k % kSub)) / kSub, Y = y + (0.5 + (k / kSub)) / kSub;
                const RayPath p = traceRay(ch, v, X, Y);
                g_rays++; g_steps += p.end.steps;
                for (std::size_t c = 0; c + 5 <= p.cross.size(); c += 5) {
                    const double r = p.cross[c];
                    if (r >= dg.rIn && r <= dg.rOut) {
                        const auto L = todayLight(s, r, p.cross[c + 2], discShiftKerr(r, p.b, 0), gp, cl);
                        for (int q = 0; q < 3; q++) ca[q] += L[q];
                        na++;
                        break;
                    }
                }
                // the hot flow: I_nu,obs = Int g^2 j_nu(nu / g) dlambda (the invariant I / nu^3; dlambda: the affine length of p with p_t = -1, in M, times r_g)
                const std::vector<double>& pa = p.path;
                double sv = 0, sk = 0, sth = 0, tau = 0;
                for (std::size_t i = 5; i + 5 <= pa.size(); i += 5) {
                    const double* a = &pa[i - 5]; const double* b = &pa[i];
                    if (std::min(a[0], b[0]) > 100) continue;           // CHOICE: the flow out to 100 M (its light falls steeply outward: the counts printed)
                    double dyA[kMaxState];                              // the affine length of the step: |dx| / |dx/dlambda| (the step is short: straight)
                    Ray rr; rr.n = 5; rr.c[0] = p.b; rr.c[1] = p.q;
                    ch.rhs(rr, a, dyA);
                    const double ds = std::hypot(b[0] - a[0], a[0] * (b[1] - a[1])) / std::max(1e-300, std::hypot(dyA[0], a[0] * dyA[1]));
                    const int n = std::clamp(static_cast<int>(std::ceil(std::fabs(b[0] - a[0]) / (0.02 * std::min(a[0], b[0])) + std::fabs(b[1] - a[1]) / 0.01)), 1, 2000);
                    for (int j = 0; j < n; j++) {
                        double st[5]; lerp5(a, b, (j + 0.5) / n, st);
                        g_samples++;
                        const double r = st[0], th = st[1], R = r * std::sin(th), z = r * std::cos(th);
                        if (r < 2.05 || R < 1e-6) continue;
                        const double ex = std::exp(-z * z / (2 * R * R));
                        const double ne = kN0 * std::pow(R, -1.1) * ex, nn = kN0nth * std::pow(R, kPnth) * ex, Te = kT0 * std::pow(r, -0.84);
                        const double B = std::sqrt(8 * kPi * kBeta1 * ne * cMp * cC * cC / (6 * r));   // r_S / (12 r) = 2 / (12 r), M = 1
                        double ut, ur, uph; hotVelocity(r, th, ut, ur, uph);
                        // p_t = -1, p_phi = b, p_r = st[3]; -p.u = u^t - b u^phi - p_r u^r
                        const double E = ut - p.b * uph - st[3] * ur;
                        if (!(E > 0)) continue;
                        const double g = 1 / E;
                        // the angle to the toroidal field in the gas's frame (a = 0; the gas's phi unit vector boosted from the static frame, its radial motion left out)
                        const double fs = std::sqrt(1 - 2 / r), Es = 1 / fs, pphi = p.b / (r * std::sin(th));
                        const double vphi = r * std::sin(th) * uph / (ut * fs), gam = 1 / std::sqrt(std::max(1e-12, 1 - vphi * vphi));
                        const double cosB = std::clamp(gam * (pphi - vphi * Es) * g, -1.0, 1.0), sB = std::max(1e-3, std::sqrt(1 - cosB * cosB));
                        const double dl = ds / n * rg;                  // cm
                        const double nuV = cC / 550e-7, nuK = cC / 2.2e-4;
                        sv += g * g * jPower(nn, B, nuV / g, sB) * dl;
                        sk += g * g * (jPower(nn, B, nuK / g, sB) + jThermal(ne, Te, B, nuK / g, sB)) * dl;
                        sth += g * g * jThermal(ne, Te, B, nuV / g, sB) * dl;
                        tau += aPower(nn, B, nuV / g, sB) * dl / g;     // (alpha nu is invariant: the optical depth along the ray, an upper bound check)
                    }
                }
                iv += sv * 1e-3; ik += sk * 1e-3; ith += sth * 1e-3;   // erg s^-1 cm^-2 Hz^-1 sr^-1 -> W m^-2 Hz^-1 sr^-1
                tauRow[y] = std::max(tauRow[y], tau);
            }
            const std::size_t i = static_cast<std::size_t>(y) * kW + x;
            for (int q = 0; q < 3; q++) A.rgb[i][q] = ca[q] / (kSub * kSub);
            A.on[i] = na > 0; A.Y[i] = lumaSrgb(A.rgb[i]);
            IV[i] = iv / (kSub * kSub); IK[i] = ik / (kSub * kSub); ITh[i] = ith / (kSub * kSub);
        }
    });
    for (double t : tauRow) tauMax = std::max(tauMax, t);
    const double secs = now() - t0;
    // the flow's colour and luminance: the observed spectrum is the power law I_nu = I_V (nu / nu_V)^-1.25 at every pixel (the shift keeps a power law);
    // Y = Sum ybar(lambda) I_lambda dlambda, I_lambda = I_nu c / lambda^2 (the CIE 1931 table of disc.h), against the Sun's surface (disc.h blackBodyXYZ)
    const double alpha = (kP - 1) / 2, nuV = cC / 550e-7;
    std::array<double, 3> xyz{0, 0, 0};
    for (int k = 0; k < cie::kCmfN; k++) {
        const double lam = (cie::kCmfNm0 + k * cie::kCmfStepNm) * 1e-9, nu = 2.99792458e8 / lam;
        const double Il = std::pow(nu / nuV, -alpha) * 2.99792458e8 / (lam * lam);
        for (int c = 0; c < 3; c++) xyz[c] += Il * cie::kCmf[k][c] * cie::kCmfStepNm * 1e-9;
    }
    const double ySun = blackBodyXYZ(sunTeff())[1];
    const cie::V3 lin = cie::linearSrgb(cie::V3{xyz[0], xyz[1], xyz[2]});
    std::array<double, 3> unit{std::max(0.0, lin[0]) / ySun, std::max(0.0, lin[1]) / ySun, std::max(0.0, lin[2]) / ySun};   // per unit I_V
    const double yPer = xyz[1] / ySun;
    Panel B = newPanel();
    double fluxK = 0, fluxV = 0, ivMax = 0, thRatio = 0;
    for (int y = 0; y < kH; y++)
        for (int x = 0; x < kW; x++) {
            const std::size_t i = static_cast<std::size_t>(y) * kW + x;
            const double dO = pixelSolidAngle(v, x + 0.5, y + 0.5);
            fluxK += IK[i] * dO; fluxV += IV[i] * dO; ivMax = std::max(ivMax, IV[i]);
            if (IV[i] > 1e-3 * ivMax) thRatio = std::max(thRatio, ITh[i] / IV[i]);
            for (int q = 0; q < 3; q++) B.rgb[i][q] = IV[i] * unit[q];
            B.Y[i] = IV[i] * yPer; B.on[i] = IV[i] > 0;
        }
    // at Earth: the camera's flux times (r_cam / D)^2 (the camera at 50 M sees the flow from close; only an order-of-magnitude check: the picture's field only)
    const double toEarth = std::pow(v.rC * rg / 100 / D, 2);
    std::vector<double> ys;
    for (std::size_t i = 0; i < A.Y.size(); i++) if (A.on[i]) ys.push_back(A.Y[i]);
    std::sort(ys.begin(), ys.end(), std::greater<double>());
    const ToneExposure e = toneFromSamples(ys, 0);
    // the third panel: the flow's own exposure, the screen's white at its brightest pixel's largest channel (the lab's Film rule: linear, nothing burnt out)
    double wB = 0;
    for (const auto& c : B.rgb) wB = std::max({wB, c[0], c[1], c[2]});
    std::vector<std::array<double, 3>> a(A.rgb.size()), b1(A.rgb.size()), b2(A.rgb.size());
    for (std::size_t i = 0; i < a.size(); i++) {
        a[i] = toneRGB(A.rgb[i], e); b1[i] = toneRGB(B.rgb[i], e);
        for (int q = 0; q < 3; q++) b2[i][q] = B.rgb[i][q] / wB;
    }
    double yMaxB = 0;
    for (double yv : B.Y) yMaxB = std::max(yMaxB, yv);
    int W = 0;
    const auto joined = joinPanels({a, b1, b2}, W);
    writePng(out, joined, W, kH);
    std::printf("sgra: %s %d x %d (three panels of %d x %d, %d rays a pixel), %.1f s\n", out.c_str(), W, kH, kW, kH, kSub * kSub, secs);
    std::printf("  today's thin disc (what if, 0.1 L_Edd): exposure k %.4g, brightest Y %.4g x the Sun's surface\n", e.k, e.yMax);
    std::printf("  hot flow (BL06 a = 0): brightest Y %.4g x the Sun's surface (550 nm I_nu max %.4g W m^-2 Hz^-1 sr^-1); right panel: white = %.4g x the Sun's surface\n",
                yMaxB, ivMax, wB);
    std::printf("  middle panel at the left's exposure: the flow's brightest pixel reaches %.3g of the screen's white (one 8-bit step: %.3g)\n", toneRGB(B.rgb[0], e)[0] * 0 +
                [&] { double m = 0; for (const auto& c : b1) m = std::max({m, c[0], c[1], c[2]}); return m; }(), 0.5 / (255 * 12.92));
    std::printf("  thermal electrons at 550 nm: at most %.3g of the power-law light where the flow is drawn (not drawn); largest optical depth at 550 nm %.3g\n", thRatio, tauMax);
    std::printf("  colour of the power law I_nu ~ nu^-%.2f: x %.4f y %.4f\n", alpha, xyz[0] / (xyz[0] + xyz[1] + xyz[2]), xyz[1] / (xyz[0] + xyz[1] + xyz[2]));
    std::printf("  flux in the picture at 2.2 um, moved to Earth by (r_cam / D)^2: %.3g mJy (GRAVITY 2020: median 1.1 +- 0.3 mJy); at 550 nm %.3g mJy\n",
                fluxK * toEarth / 1e-29, fluxV * toEarth / 1e-29);
    std::printf("  rays %ld, integrator steps %.1f a ray, samples %.1f a ray\n", g_rays.load(), double(g_steps) / g_rays, double(g_samples) / g_rays);
    return 0;
}

int filmPicture(const std::string& out) {
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    setLook(s, Look::Film);
    s = shown(s);                                                       // spin 0.6, J15's edges 9.26 and 18.70 M, one colour
    const DiscGeom dg = discGeom(s);
    const View v;
    const KerrBL ch(s.spin);
    Field fld; fld.make(78102);
    patches078::GasPattern gp = patches078::GasPattern::reduced(); gp.a = s.spin; gp.rIn = dg.rIn; gp.rOut = dg.rOut;
    Clump cl; cl.a = s.spin; cl.rc = gasClumpRadius(s); cl.init();
    const auto film = filmDiscLinear();
    // CHOICES (the lab's own, labelled in the plan): the edges fade over kFade in ln r inside J15's edges; the disc is "marginally optically thick" (J15 4.1.1):
    // each crossing covers 1 - exp(-tau), tau = kTau x the texture x the edge; the crossings are laid front to back
    constexpr double kFade = 0.12, kTau = 1.5;
    Panel A = newPanel(), B = newPanel();
    const double t0 = now();
    forRows([&](int y) {
        for (int x = 0; x < kW; x++) {
            std::array<double, 3> ca{0, 0, 0}, cb{0, 0, 0};
            int na = 0;
            for (int k = 0; k < kSub * kSub; k++) {
                const double X = x + (0.5 + (k % kSub)) / kSub, Y = y + (0.5 + (k / kSub)) / kSub;
                const RayPath p = traceRay(ch, v, X, Y);
                g_rays++; g_steps += p.end.steps;
                bool done = false;
                double left = 1;
                for (std::size_t c = 0; c + 5 <= p.cross.size(); c += 5) {
                    const double r = p.cross[c];
                    if (r < dg.rIn || r > dg.rOut) continue;
                    if (!done) {
                        const auto L = todayLight(s, r, p.cross[c + 2], 1, gp, cl);
                        for (int q = 0; q < 3; q++) ca[q] += L[q];
                        na++; done = true;
                    }
                    const double u = std::log(r);
                    const double e1 = std::clamp((u - std::log(dg.rIn)) / kFade, 0.0, 1.0), e2 = std::clamp((std::log(dg.rOut) - u) / kFade, 0.0, 1.0);
                    const double edge = e1 * e1 * (3 - 2 * e1) * e2 * e2 * (3 - 2 * e2);
                    const double S = fld.S(r, p.cross[c + 2]);
                    const double cover = 1 - std::exp(-kTau * S * edge);
                    for (int q = 0; q < 3; q++) cb[q] += left * cover * film[q] * S;
                    left *= 1 - cover;
                }
            }
            const std::size_t i = static_cast<std::size_t>(y) * kW + x;
            for (int q = 0; q < 3; q++) { A.rgb[i][q] = ca[q] / (kSub * kSub); B.rgb[i][q] = cb[q] / (kSub * kSub); }
            A.on[i] = na > 0;
        }
    });
    const double secs = now() - t0;
    const double white = std::max({film[0], film[1], film[2]});   // the screen's white: the film's disc colour (its largest channel), both panels
    std::vector<std::vector<std::array<double, 3>>> outP;
    for (Panel* P : {&A, &B}) {
        std::vector<double> lin(static_cast<std::size_t>(kW) * kH * 3);
        for (std::size_t i = 0; i < P->rgb.size(); i++) for (int q = 0; q < 3; q++) lin[3 * i + q] = std::clamp(P->rgb[i][q] / white, 0.0, 1.0);
        const std::vector<double> gl = glowReference(lin, kW, kH);
        std::vector<std::array<double, 3>> o(P->rgb.size());
        for (std::size_t i = 0; i < o.size(); i++) for (int q = 0; q < 3; q++) o[i][q] = lin[3 * i + q] + kGlowStrength * gl[3 * i + q];
        outP.push_back(o);
    }
    int W = 0;
    const auto joined = joinPanels(outP, W);
    writePng(out, joined, W, kH);
    std::printf("film: %s %d x %d (two panels), %.1f s; spin %.2f, edges %.2f-%.2f M, white = the film disc's colour (%.4g x the Sun's surface, largest channel)\n", out.c_str(), W, kH, secs,
                s.spin, dg.rIn, dg.rOut, white);
    std::printf("  rays %ld, integrator steps %.1f a ray\n", g_rays.load(), double(g_steps) / g_rays);
    return 0;
}

// the counts for the GPU estimate: a row through the middle of the feeding preset's view and of Sagittarius A*'s
int counts() {
    const View v;
    const KerrBL ch(0);
    long rays = 0, steps = 0, hitRays = 0;
    double ms = 0;
    for (int y = 0; y < kH; y += 10)
        for (int x = 0; x < kW; x += 10) {
            const double t0 = now();
            const RayPath p = traceRay(ch, v, x + 0.5, y + 0.5);
            ms += (now() - t0) * 1e3;
            rays++; steps += p.end.steps;
            for (std::size_t i = 0; i < p.path.size(); i += 5) if (p.path[i] < 100) { hitRays++; break; }
        }
    std::printf("count: %ld rays of the lab's starting view: %.1f integrator steps a ray, %.3f ms a ray on one core (double, rtol 1e-11); %ld of them come within 100 M\n",
                rays, double(steps) / rays, ms / rays, hitRays);
    // one hot-flow sample's arithmetic on the processor (the emissivity, the shift, the angle): time per sample
    volatile double acc = 0;
    const double t0 = now();
    const int N = 2000000;
    for (int i = 0; i < N; i++) {
        const double r = 3 + 50.0 * i / N, th = 1.2, R = r * std::sin(th), z = r * std::cos(th);
        const double ex = std::exp(-z * z / (2 * R * R));
        const double nn = kN0nth * std::pow(R, kPnth) * ex, ne = kN0 * std::pow(R, -1.1) * ex;
        const double B = std::sqrt(8 * kPi * kBeta1 * ne * cMp * cC * cC / (6 * r));
        acc = acc + jPower(nn, B, 5e14, 0.7);
    }
    std::printf("count: one emissivity sample (power law, double) %.1f ns on one core\n", (now() - t0) * 1e9 / N);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string m = argc > 1 ? argv[1] : "";
    const std::string out = argc > 2 ? argv[2] : m + ".png";
    if (m == "real") return realPicture(out);
    if (m == "real079") return realPicture(out, true);   // 0.79.0: the app's model on the right, each panel at its own exposure
    if (m == "sgra") return sgraPicture(out);
    if (m == "film") return filmPicture(out);
    if (m == "count") return counts();
    std::fprintf(stderr, "usage: discproto real|real079|sgra|film <out.png> | count\n");
    return 1;
}
