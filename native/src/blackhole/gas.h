// Black Hole Lab (0.56.4): moving gas, step 1 (PLAN_MOVINGGAS.md section 7): the physics on the processor in double precision, the reference the graphics card
// will be checked against (step 2). Moved from tools/movinggas (0.56.3), which now calls this file. No graphics. G = c = 1, lengths and times in M
// (one M of time = GM/c^3). The owner's choice of 17 Nov 2026 (DECISIONS.md): Real and Film both get the moving pattern and the clump.
//
// THE CLOCKS. Far away: Boyer-Lindquist t (the hole's time). Gas on a circular equatorial orbit, Omega = 1 / (r^(3/2) + a) (BPT (2.16), disc.h orbitOmega), moves
//   at V = (varpi / alpha)(Omega - omega) in the locally non-rotating frame (C. T. Cunningham, ApJ 202, 788 (1975) ("C75") (A7c) = J15 (A.7)); its own clock
//   runs at dtau/dt = 1 / u^t = alpha (1 - V^2)^(1/2) (C75 (A7a)); tested against the normalisation u.u = -1 with J15 (A.1)'s metric, and -u_t against BPT
//   (2.12)'s E. The hovering camera (a FIDO, J15 (A.3)): dtau/dt = alpha (J15 (A.1)-(A.3); kerr.h hoverShift(b = 0) = 1 / alpha).
// TIME ALONG A RAY (the reference integrator). Per unit of the app's affine parameter zeta (geodesic.h KerrBL), with p_t = -1 and b = p_phi:
//   dt/dzeta = [ (r^2 + a^2) P / Delta + a (b - a sin^2 theta) ] / rho^2,   P = r^2 + a^2 - a b
//   (-dH/dp_t of J15 (A.15)'s Hamiltonian; the integrands of S. E. Gralla, A. Lupsasca, PRD 101, 044032 (2020) ("GLa") (12), (13c), (13f)), and the Mino time
//   dtau/dzeta = 1 / rho^2 (GLa (9): dr/dtau = +-R(r)^(1/2), against J15 (A.15)'s rho^2 dr/dzeta = Delta p_r = +-R^(1/2)); the closed forms (kerr_closed.h) are
//   checked against this integrator.
// THE CLUMP (a labelled model). A. E. Broderick, A. Loeb, MNRAS 367, 905 (2006), arXiv:astro-ph/0509237 ("BL06") section 2.3 (5): a spot of extra emission,
//   Gaussian in the distance measured in its own rest frame, n = exp(-[dx.dx + (u.dx)^2] / (2 R_S^2)), centred on a point on a circular (Keplerian) orbit,
//   R_S = 1.5 M (BL06 section 2.3: "we used RS = 1.5M"); it keeps its shape (BL06's spot does not shear; real gas would). On the thin disc's face dx = (0, dr, 0,
//   dphi), so dx.dx + (u.dx)^2 = g_rr dr^2 + (g_phph + u_phi^2) dphi^2, u_phi = L / mu (BPT (2.13)). Brightness: the disc's flux times (1 + A n).
//   CHOICES (plan section 4, said in the app): A = 4 (its centre 5 times the gas around it), its radius (6 M), its phase (at phi = 0 at t = 0).
// THE STREAKS (0.79.0, a labelled model; PLAN_DISC.md section 4; they replace 0.72's round patches, the owner's choice of 24 Nov 2026). Simulated discs are
//   streaked: magnetic turbulence makes long thin filaments, tilted from the orbit and wound into trailing spirals. Here the disc's light is multiplied by
//   S = exp(c f - c^2 s^2 / 2), f a Gaussian random field (s^2 its variance where it is read; the mean of S is exactly 1 for a Gaussian f: light moved, not
//   added). f lives on (x = ln R, y = phi), where correlation lengths proportional to R make it homogeneous (D. Lee, C. F. Gammie, ApJ 906, 39 (2021),
//   arXiv:2011.07151 ("LG21") sect. 4.2: "lambda1 proportional to r and lambda2 / lambda1 = const"). Its spectrum LG21 (13), P(k) ~ 1 / (1 + k.Lambda k)^2, with
//   Lambda = lambda1^2 u1 u1 + lambda2^2 u2 u2 (LG21 (6)); here a 2-D field (x, y): its covariance is s K_1(s), s^2 = dx.Lambda^-1 dx (LG21 (7) with nu = 1).
//   u1 at 15 degrees from the orbit, trailing: X. Guan, C. F. Gammie, J. B. Simon, B. M. Johnson, ApJ 694, 1010 (2009), arXiv:0901.0273 ("G09") Table 2,
//   theta_B,tilt of the converged runs y128a-y256b (15-17 degrees). lambda2 / lambda1 = 0.049 / 0.27 (G09 Table 2, y256a: lambda_B,min / lambda_B,maj).
//   lambda1 = 25 degrees x R: J. D. Schnittman, J. H. Krolik, J. F. Hawley, ApJ 651, 1031 (2006), arXiv:astro-ph/0606615 ("SKH06") section 5, "delta phi = 25 deg"
//   (CHOICE: which length is the spectrum's knee). Made once (StreakField::make: white noise from gasHash, Box-Muller, the spectrum by FFT; then each value
//   replaced by the normal quantile of its rank, so the field's values are exactly standard normal; mip levels by 2 x 2 means, the variance s^2 and the
//   covariance with the next level measured at 2^18 points).
//   TIME: the field is carried round at each radius's own speed Omega(R) (BPT (2.16)) and renewed. Clocks run in bins of ln R (kStreakBinW wide; bin k
//   centred at R_k = exp((k + 1/2) kStreakBinW)) with T_life,k = 0.3 T_orb(R_k) (SKH06 (36): "T_life ~ 0.3 T_orb"). In each bin two copies j = 0, 1 with clocks
//   s = t / T_life,k + j/2; the copy born at n = floor(s) carries the field from where its gas was at birth, phi - Omega(R) (s - n) T_life,k, at a random
//   offset in the field drawn from gasHash(n, bin, j) (a new pattern each life), weighted by sin(pi (s - n)): the two weights' squares sum to 1 (sin^2 + cos^2),
//   and the copy is reborn where its weight is 0 (no jump). A point mixes its two nearest bins with weights cos and sin of (pi/2) x its place between their
//   centres: again squares summing to 1, so f keeps unit variance. The winding never runs away: a copy lives T_life. The card gets each bin's E / T_life,k
//   = K + f (E: the epoch, gasCard) from the processor in double, so the clocks stay exact however long the film runs (as 0.72's patches).
//   STRENGTH: c = kStreakC makes the face-on 6-10 M ring's light vary by SKH06 Table 2's 43.8 % rms (thermal model, power above 10 Hz; tested within the
//   plan's 20 %, as 0.72's patches were).
//   THE PIXEL'S FOOTPRINT: the field read at the mip level of the footprint (its sigma fe in ln R, fp in phi against the texel), blended linearly between two
//   levels. s^2 where f is read: between texels the bilinear mix has a smaller variance than a texel, by an amount that depends on where (varAt: exact from
//   the texel covariances); a blend's (1 - w)^2 s_L^2 + w^2 s_L+1^2 + 2 w (1 - w) cov_L (cov_L: the mean over points); the copies' variances add, each times
//   its weight squared. So the mean of S stays 1 wherever and whenever it is read.
#pragma once

#include <cstdint>
#include <vector>

#include "blackhole/geodesic.h"

namespace blackhole {

// ---- the clocks and speeds (circular equatorial orbits with the spin, a >= 0) ----
double gasOmega(double r, double a);              // BPT (2.16)
double gasPeriod(double r, double a);             // 2 pi / Omega, far clock (M)
double gasSpeed(double r, double a);              // V, C75 (A7c)
double gasClockRate(double r, double a);          // dtau/dt on the gas = alpha (1 - V^2)^(1/2), C75 (A7a)
double cameraClockRate(double a, double r, double theta);   // dtau/dt of the hovering camera (FIDO) = alpha
double massTimeSeconds(double massSun);           // one M of time, GM/c^3, in seconds (orbit/units.h: the Sun's GM, c)

// ---- the reference integrator: KerrBL with the far time t as y[5] and the Mino time as y[6] (both 0 at the camera, decreasing into the ray's past) ----
constexpr int kTState = 7;
class KerrBLt : public KerrBL {
public:
    explicit KerrBLt(double a, double rFar = 1e5) : KerrBL(a, rFar), a_(a) {}
    bool launch(const Camera& cam, const Vec3& look, Ray& ray) const override;
    void rhs(const Ray& ray, const double* y, double* dy) const override;
private:
    double a_;
};

// ---- the clump (BL06 (5)) ----
struct Clump {
    double a = 0, rc = 6, RS = 1.5, A = 4, phi0 = 0;   // spin, orbit radius, size (M), strength, its phi at t = 0
    double grr = 1, gpp = 1, omega = 0;                // set by init()
    void init();
    double density(double r, double phi, double tEmit) const;   // n of (5) at a point of the disc's face at the time the light left it (0..1)
    double period() const;                                       // 2 pi / Omega(rc)
};

std::uint32_t gasHash(std::uint32_t a, std::uint32_t b, std::uint32_t c);   // a small integer hash (the card does the same)
double gasU01(std::uint32_t h);                                              // its low 24 bits as a number in [0, 1) (exact in single precision)

// ---- 0.79.0: the streaks (above) ----
constexpr double kStreakC = 1.3;                    // c (tested: the 6-10 M ring against SKH06 Table 2)
constexpr double kStreakBinW = 0.5;                 // the clocks' bins in ln R (above)
constexpr int kStreakBins = 17;                     // bins 0..16: centres from R = 1.28 to 4024 M (the field's period in ln R is 8: R from 1 to 2981)
struct StreakField {
    static constexpr int kNx = 512, kNy = 1024, kLevels = 8;                  // x (ln R) and y (phi) texels at level 0; mip levels
    static constexpr double kLnSpan = 8;                                     // ln R from 0 to 8 is one period of x (R from 1 to 2981 M)
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kTilt = 15 * kPi / 180, kLam1 = 25 * kPi / 180, kRatio = 0.049 / 0.27, kLife = 0.3;   // G09, SKH06 (above)
    std::vector<float> lev[kLevels];                // level L: (kNx >> L) x (kNy >> L), row y, column x
    double var[kLevels] = {}, cov[kLevels] = {};    // s^2 of level L read between texels (the mean over points), its covariance with level L + 1 (measured)
    double c00[kLevels] = {}, cx[kLevels] = {}, cy[kLevels] = {}, cd[kLevels] = {};   // texel covariances at (0, 0), (1, 0), (0, 1), (1, 1) + (1, -1)
    double makeMs = 0;                              // the time make() took
    void make(std::uint32_t seed = 79);
    int nx(int L) const { return kNx >> L; }
    int ny(int L) const { return kNy >> L; }
    double texel(int L, int i, int j) const;        // periodic
    double sample(int L, double X, double Y, double* v = nullptr) const;   // bilinear, X and Y in periods (any real number), as the card reads it; v: its variance
    double varAt(int L, double ax, double ay) const; // the variance of the bilinear mix at fractions (ax, ay) between texels
    double lodOf(double fe, double fp) const;       // the footprint's level (0 .. kLevels - 1)
    // f (and its variance s^2) at (R, phi) at the far time T (the light's leaving, M), spin a, the footprint fe, fp (0: level 0). The clocks from the epoch
    // gasEpoch(T) as the card has them (gasCard)
    double field(double R, double phi, double T, double a, double fe, double fp, double* s2) const;
    double factor(double R, double phi, double T, double a, double fe = 0, double fp = 0, double c = kStreakC) const;   // S
    std::vector<float> atlas(int& w, int& h) const;  // every level in one texture for the card: level 0 at (0, 0), level L >= 1 at (kNx, kNy - (2 kNy >> L))
    static int atlasY(int L) { return L == 0 ? 0 : kNy - ((2 * kNy) >> L); }
    static int atlasX(int L) { return L == 0 ? 0 : kNx; }
};
const StreakField& streakField();                   // made once, the first time it is asked for (thread safe; waits for a worker making it)
// 0.79.0: the app's way (about half a second on one core of the build machine, StreakField::makeMs): the first call starts making it on a worker and every
// call returns null until it is made (the picture meanwhile has no streaks, and the benchmark's "response.blackhole_streaks" waits for it). Remake (the
// benchmark only): made again from nothing on a worker (the earlier one is kept: references to it stay good).
const StreakField* streakFieldReady();
void streakFieldRemake();
double streakLife(int k, double a);                 // T_life of clock bin k

// ---- the numbers the graphics card needs (render/shaders.h kGasPart; renderer2d.h LensSky::gas) ----
// The far time T runs without bound; the card works in single precision from an epoch E = kGasEpochM floor(T / kGasEpochM) kept here in double: tau = T - E
// (below kGasEpochM; 0.79.0: 64 M, 0.72-0.78: 1024 M: the streaks' clock phase te / T_life reaches te / 5 M near a fast-spinning hole, and te's float rounding
// at 1024 M moved the field by up to 1.3e-3 of S against the double reference (REPORT 0.79.0)), the clump's phase at E (phi0 + Omega_c E, mod 2 pi) and, for the streaks, each clock bin's E / T_life,k = K + f as (f, K div 65536,
// K mod 65536, 0) (K mod 2^32: the hash's uint32). V: per level (c00, cx, cy, cd) and W: per level cov (the streaks' variances, StreakField).
struct GasCard {
    float T[4] = {0, 0, 0, 0}, C[4] = {}, G[4] = {}, V[4 * StreakField::kLevels] = {}, W[StreakField::kLevels] = {}, B[4 * kStreakBins] = {};
    float tau = 0; double epoch = 0;
};
void streakClock(double epoch, double life, double& f, std::uint32_t& K);   // E / T_life = K + f (K mod 2^32)
constexpr double kGasEpochM = 64;
double gasEpoch(double T);
// streaks: the field's factor on (c = kStreakC); clump: the clump or null
void gasCard(bool streaks, double a, const Clump* clump, double T, GasCard& out);

}  // namespace blackhole
