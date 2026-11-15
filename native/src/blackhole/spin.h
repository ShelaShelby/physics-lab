// Black Hole Lab (0.55.2): the spinning hole's closed forms for the picture (plan step 3). Physics only, no graphics, double precision. G = c = 1, lengths in M.
// Every formula below was read from the paper (J15 from its PDF, data/blackhole/arxiv/1502.03808_james2015.pdf; GLb and PeT from their arXiv text, checked
// line by line; C75 and BPT from the page images of the scans; all in the private sources repo, native/data/SOURCES.md). Equation numbers are the papers'.
//
// WHICH RAYS COME FROM THE HORIZON (no integration). O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, CQG 32, 065001 (2015) ("J15"), App. A.1:
//   the trapped (spherical) photon orbits at radius r_o have the constants
//     b_o = -(r_o^3 - 3 r_o^2 + a^2 r_o + a^2) / (a (r_o - 1)),   q_o = -r_o^3 (r_o^3 - 6 r_o^2 + 9 r_o - 4 a^2) / (a^2 (r_o - 1)^2)            (A.5)
//   for r1 <= r_o <= r2, r1 = 2 {1 + cos[(2/3) arccos(-a)]}, r2 = 2 {1 + cos[(2/3) arccos(+a)]}                                                 (A.6)
//   (written here as -(r^2 (r - 3) + a^2 (r + 1)) / (a (r - 1)) and -r^3 (r (r - 3)^2 - 4 a^2) / (a^2 (r - 1)^2): the same polynomials, factored so that they
//   keep their digits at small spin, where r_o is close to 3 and both numerators cancel).
//   Step (v): (a) are b1 < b < b2 (b1 = b_o(r2), b2 = b_o(r1)) (A.13) and q < q_o(b) (A.14)? (b) If yes there is no radial turning point: the ray comes from
//   the horizon if p_r > 0 at the camera, from the sky if p_r < 0. (c) If no, there are two turning points: from the sky if the camera's r_c >= r_up, the
//   largest real root of R(r) = 0 (A.4), else from the horizon.
//   The code decides (c) without solving the quartic: the camera is in an allowed region (R(r_c) >= 0), and r_c < r_up exactly when R < 0 somewhere above r_c;
//   R's minima are at the roots of R'(r) = 4 r^3 + 2 (a^2 - b^2 - q) r + 2 ((b - a)^2 + q), a cubic with no r^2 term, solved in closed form (trigonometric).
//   R(r) = r^4 + (a^2 - b^2 - q) r^2 + 2 ((b - a)^2 + q) r - a^2 q is J15's R = P^2 - Delta [(b - a)^2 + q] (A.4) multiplied out (tested against (A.4)).
//   Negative spin: (A.4) is unchanged under (a, b) -> (-a, -b), so the test for -a is the test for a with -b.
//
// THE FAR-AWAY EDGE (the critical curve). S. E. Gralla, A. Lupsasca, PRD 101, 044031 (2020), arXiv:1910.12873 ("GLb"):
//   lambda~ = a + (r~/a) [r~ - 2 Delta~ / (r~ - M)]  (38),   eta~ = (r~^3 / a^2) [4 M Delta~ / (r~ - M)^2 - r~]  (39),   r~ in [r~-, r~+],
//   r~(+-) = 2M {1 + cos[(2/3) arccos(+-a/M)]}  (40);   on the screen of a far observer at inclination theta_o:
//   alpha = -lambda / sin theta_o,   beta = +-(eta + a^2 cos^2 theta_o - lambda^2 cot^2 theta_o)^(1/2)   (55)  (lambda = J15's b, eta = J15's q).
//   The Lyapunov exponent of the bound orbit at r~ (each further image thinner by e^-gamma):
//   gamma = 4 r~ chi~^(1/2) K~ / (a (-u~-)^(1/2))  (43),  chi~ = 1 - M Delta(r~) / (r~ (r~ - M)^2)  (44),  K~ = K(u~+ / u~-) (parameter, (17)),
//   u(+-) = Delta_theta +- (Delta_theta^2 + eta / a^2)^(1/2),  Delta_theta = (1/2)(1 - (eta + lambda^2) / a^2)  (11).  At a = 0: gamma = pi (72).
//   V. Perlick, O. Yu. Tsupko, Phys. Rep. 947, 1 (2022), arXiv:2105.07101 ("PeT") Table III: the far shadow's diameters (in M): 6 sqrt(3) for a = 0 at any
//   inclination; 9 across and 6 sqrt(3) up-down for a = M seen from the equator; 4 (1 + sqrt(2)) both ways for a = M seen along the axis; 6 sqrt(3) up-down
//   for any spin seen from the equator; and (their Table II) the a = M shadow from the equator spans alpha from -2M to 7M.
//
// THE GAS'S SHIFT AT ANY SPIN. C. T. Cunningham, ApJ 202, 788 (1975) ("C75"), App.: gas on circular equatorial orbits, u = e^-nu (1 - V^2)^(-1/2) (d/dt + Omega d/dphi)
//   (A7a), Omega = M^(1/2) / (r^(3/2) + a M^(1/2)) (A7b), V = (Omega - omega) e^(psi - nu) its speed in the locally non-rotating frame (A7c), and for a photon of
//   axial angular momentum lambda (= b) the shift g = E_obs / E_em = e^nu (1 - V^2)^(1/2) (1 - Omega lambda)^-1 (A9), with C75's e^nu, e^psi, omega (A1, A2) =
//   J15's alpha, varpi, omega (A.1, A.2). (J15 (A.7) gives the same speed, beta = (varpi / alpha)(Omega - omega), for a camera on that orbit.) At a = 0 it is
//   L79 (16), (19)'s (1 - 3M/r)^(1/2) / (1 - Omega L_z) (tested).
//   C75's "equatorial observer" (the 1.7 and 15 of p. 792; REPORT 0.55.2): C75 finds the rays from a ring to the observer by solving (A5),
//   Int R^(-1/2) dr = Int Theta^(-1/2) dtheta, for (lambda, q); for an observer exactly in the disc's plane (theta_o = pi/2) the rays it can find are those
//   with q -> 0+ whose theta-motion makes whole half-oscillations, i.e. Int R^(-1/2) dr = n pi / (lambda^2 - a^2)^(1/2) (the small-q limit of Theta's
//   integral), not the rays that run along the plane (q = 0 exactly: in C75's footnote 2 the plane itself is in the disc's shadow). c75EquatorialG finds the
//   n = 1 ray from a ring at r and returns its g; tests/test_blackhole.cpp reproduces C75's two ratios with it.
#pragma once

#include "blackhole/geodesic.h"

namespace blackhole {

// ---- trapped orbits and the closed shadow test (J15) ----
double trappedRadius1(double a);                      // r1 (A.6): the prograde equatorial photon orbit
double trappedRadius2(double a);                      // r2 (A.6): the retrograde one
double trappedB(double a, double r);                  // b_o (A.5)
double trappedQ(double a, double r);                  // q_o (A.5)
double trappedRadiusOfB(double a, double b);          // the r_o in [r1, r2] with b_o(r_o) = b (bisection; b must lie in [b1, b2])
double radialR(double a, double b, double q, double r);    // R (A.4)
// (v): true if the ray with constants (b, q) and covariant p_r at a camera at rc comes from the horizon
bool fromHorizon(double a, double rc, double b, double q, double pr);
// the same for the ray the camera sees in direction look (its constants by KerrBL::launch, J15 (A.8)-(A.12)), no integration
bool lookFromHorizon(double a, const Camera& cam, const Vec3& look);

// ---- the camera's sky round the hole ----
// The look direction (N_r, N_theta, N_phi) on the camera's frame at angle psi from the hole's direction (-e_r) and azimuth chi round it, measured from e_theta
// towards e_phi: N = cos psi (-e_r) + sin psi (cos chi e_theta + sin chi e_phi).
Vec3 lookAt(double psi, double chi);
// The shadow's edge along azimuth chi: the psi where the closed test changes (bisection to 1e-14 rad; the hole's direction psi = 0 always comes from the
// horizon and psi = pi from the sky: J15 (v) with b = 0). Tested to be the only change along each azimuth (tests/test_blackhole.cpp testSpinShadow).
double shadowEdge(double a, const Camera& cam, double chi);

// ---- the far-away edge (GLb) ----
double criticalLambda(double a, double rt);           // (38)
double criticalEta(double a, double rt);              // (39)
double criticalRadiusMinus(double a);                 // r~- (40), the prograde end
double criticalRadiusPlus(double a);                  // r~+ (40)
// the screen point (55) of the critical ray at r~ for an observer at theta_o (0 < theta_o < pi); false where beta^2 < 0 (r~ outside this observer's range)
bool criticalScreen(double a, double rt, double thetaO, double& alpha, double& beta2);
double lyapunov(double a, double rt);                 // gamma (43); pi at a -> 0 (72)

// ---- the gas ----
double discShiftKerr(double r, double b, double a);   // C75 (A9): g = alpha (1 - V^2)^(1/2) / (1 - Omega b)
// C75's equatorial-observer ray from the ring at r (see above): its lambda (the largest of the n = 1 solutions, prograde), 0 if none
double c75EquatorialLambda(double r, double a);

// ---- a camera that moves relative to the FIDO (J15 (A.9), (A.10), (A.16); the lab's camera hovers: beta = 0) ----
// n_F, the light's direction of motion measured by the FIDO, for a camera moving at beta along B (unit, on e_r, e_theta, e_phi) that sees the light come from N
// (unit, on its own axes, the FIDO's axes boosted along B): special-relativity aberration, (A.9) with (A.10).
Vec3 aberrate(const Vec3& N, double beta, const Vec3& B);
// J15 (A.16): the frequency the camera sees over the frequency emitted at rest far away
double cameraShift(double a, const Camera& cam, const Vec3& N, double b);

}  // namespace blackhole
