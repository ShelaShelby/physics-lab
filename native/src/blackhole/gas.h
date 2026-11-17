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
// THE PATTERN (a labelled model). J. D. Schnittman, J. H. Krolik, J. F. Hawley, ApJ 651, 1031 (2006), arXiv:astro-ph/0606615 ("SKH06"): in their simulated (MHD,
//   Schwarzschild) disc the bright patches are self-similar, "delta phi = 25 deg and delta r/r = 0.3" (section 5; lambda_eta = 0.26), and live "T_life ~ 0.3 T_orb"
//   ((36)). Here: patches born at places and times drawn from a 32-bit integer hash (so the graphics card makes the same ones), each a Gaussian in (ln r, phi) of
//   full width at half maximum 0.26 and 25 degrees at birth, bright or dark (the hash's top bit), its strength x e^(1 - x), x = (t - t_birth) / T_life(r_birth);
//   each piece carried round at its own radius's Omega(r): phi = phi_birth + Omega(r) (t - t_birth) (it shears into a trailing spiral at the true speeds).
//   Cells: eta = ln r in bins of 0.13 (half the width), phi in bins of 12.5 degrees at birth, birth slots of T_life of the bin; a point sees its eta bin and two
//   on each side (5), the 9 slots up to its own (x e^(1 - x) < 2.5e-3 after 8 lifetimes) and the 28 phi cells. Brightness factor exp(c S) / norm, norm = the mean
//   of exp(c S) weighted by the disc's light, over the disc and over time (the pattern moves light around without adding any; tested). c = 0.65 makes the face-on 6-10 M ring's light vary by
//   about SKH06 Table 2's 43.8 % rms (thermal model, power above 10 Hz; tested within the plan's 20 %).
//   (For the card, step 2: per eta bin and slot only the phi cells whose patch can reach the point need a look; the birth time inside a slot is spread over 0.3
//   of an orbit (108 degrees of turn), so more cells than the plan's estimate of 3 can reach it: step 2 counts them.)
#pragma once

#include <cstdint>

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

// ---- the pattern (SKH06) ----
std::uint32_t gasHash(std::uint32_t a, std::uint32_t b, std::uint32_t c);   // a small integer hash (the card does the same)
double gasU01(std::uint32_t h);                                              // its low 24 bits as a number in [0, 1) (exact in single precision)
struct GasPatch { double eta = 0, phi = 0, tBirth = 0, life = 1, sign = 1; };
struct GasPattern {
    double a = 0, rIn = 6, rOut = 20, contrast = 0.65, norm = 1, normErr = 0;   // normErr: the norm's relative standard error (normalise)
    static constexpr double kWidthEta = 0.26, kWidthPhi = 25 * 3.14159265358979323846 / 180;   // SKH06 section 5 (full widths)
    static constexpr double kLife = 0.3;                                                         // T_life / T_orb, SKH06 (36)
    static constexpr int kEtaSide = 2, kSlots = 9;                                                // eta bins on each side, birth slots looked at
    // one patch's strength at (r, phi, t): its Gaussian at where this piece of gas was when the patch was born, times x e^(1 - x) (fade) or 1 (no fade, x > 0)
    static double patch(const GasPatch& p, double r, double phi, double t, double a, bool fade = true);
    double sum(double r, double phi, double t) const;                       // S: the sum of the patches that reach (r, phi) at t
    double factor(double r, double phi, double t) const;                    // exp(c S) / norm
    void normalise(int samples = 20000);                                    // norm = the mean of exp(c S) weighted by the disc's light, at independent samples
};

}  // namespace blackhole
