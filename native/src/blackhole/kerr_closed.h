// Black Hole Lab (0.55.9): every light ray of the spin table in closed form (no step-by-step integration). Physics only, no graphics, double precision.
// G = c = 1, lengths in M. The integrator (geodesic.h KerrBL; mino.h) stays as the reference in the tests.
//
// SOURCE: S. E. Gralla, A. Lupsasca, "Null geodesics of the Kerr exterior", Phys. Rev. D 101, 044032 (2020), arXiv:1910.12881v3 ("GLa"; saved in the private
// sources repo, data/blackhole/arxiv/1910.12881_gralla_lupsasca2020a.txt.gz; every equation below was read there; equation numbers are GLa's; lambda = J15's b,
// eta = J15's q). R. Berens, D. O. Chang, T. Gravely, A. Lupsasca, PASP 138, 114501 (2026), arXiv:2603.05810 ("B26") uses the same forms per pixel for a camera
// far away: only the polar motion of type A and the radial case (2) with a turning point at r4 (its Sec. IV: "We thus restrict to case (2)"; the rest is its
// shadow, drawn black, nothing computed there). The lab needs more (rays seen inside the shadow cross the disc before the horizon; a hovering camera close in
// sees rays of every case), so all of GLa's cases are built here.
//
// THE RAY IN MINO TIME (GLa (9)-(14)): with tau the Mino time, (dr/dtau)^2 = R(r) (7) and (dtheta/dtau)^2 = Theta(theta) (8); the path integrals I_r = G_theta = tau
// (14), phi_o - phi_s = I_phi + lambda G_phi (11), with I_phi = Int a (2 r - a lambda) / (Delta sqrt R) dr (13b) and G_phi = Int csc^2 theta / sqrt(Theta) dtheta
// (13e), all taken along the path, increasing. The lab follows a ray from the camera into its past (mino.h: s = -lambda_Mino): that is a path of the same
// equations with tau = s, starting at the camera ("source" s = the camera in GLa's notation) with the signs nu_r = sign(dr/ds) = -sign(p_r) and
// nu_theta = sign(dtheta/ds) = -sign(p_theta) (J15 (A.11), (A.15): dr/dlambda = Delta p_r, dtheta/dlambda = p_theta); phi runs backward, so
//   phi(s) = phi_camera - I_phi(s) - lambda G_phi(s).
// THE POLAR MOTION (GLa Sec. III). u+- (19) are the roots of (1 - u) Theta(u) = eta + (a^2 - eta - lambda^2) u - a^2 u^2 (18), u = cos^2 theta; written here as
//   -u- a^2 = (-A + (A^2 + 4 a^2 eta)^(1/2)) / 2 (A = a^2 - eta - lambda^2) and u+ = eta / (-u- a^2) (their product is -eta / a^2): no division by a^2.
//   Type A (eta > 0, "ordinary", GLa III.A): cos theta = -nu_theta u+^(1/2) sin Psi (49), Psi = am(X | u+/u-) (46) with X = (-u- a^2)^(1/2) tau + F(Psi_0 | u+/u-),
//   Psi_0 = -nu_theta arcsin(cos theta_s / u+^(1/2)) (from (29), (36)); G_phi = (Pi(u+; Psi | u+/u-) - Pi(u+; Psi_0 | u+/u-)) / (-u- a^2)^(1/2) ((47) with (30); Pi is
//   odd). The equator (cos theta = 0) is crossed where Psi = j pi: tau_j = (2 j K(u+/u-) - F(Psi_0)) / (-u- a^2)^(1/2), the first j with tau_j > 0.
//   Type B (eta < 0, "vortical", GLa III.B, only |lambda| < a): u- a^2 = -2 a^2 eta / (A + (A^2 + 4 a^2 eta)^(1/2)) > 0, h = sign(cos theta_s) (54);
//   cos theta = h (u- + (u+ - u-) sin^2 Upsilon)^(1/2) (69), Upsilon = am(X | 1 - u+/u-) (66), X = (u- a^2)^(1/2) tau + F(Upsilon_0), Upsilon_0 = -h nu_theta
//   arcsin(((cos^2 theta_s - u-) / (u+ - u-))^(1/2)) ((59), (65)); G_phi = (Pi(n; Upsilon) - Pi(n; Upsilon_0)) / ((1 - u-)(u- a^2)^(1/2)), n = (u+ - u-) / (1 - u-)
//   ((67) with (57)). It never crosses the equator.
// THE RADIAL MOTION (GLa Sec. IV, App. B). Roots: R(r) = r^4 + A r^2 + B r + C (78), A as above, B = 2 (eta + (lambda - a)^2) (80), C = -a^2 eta (81); the
//   resolvent cubic's largest root xi0 by Cardano (84)-(91) (the trigonometric form where its discriminant is positive), polished by two Newton steps on (83);
//   z = (xi0 / 2)^(1/2) (94); r1, r2 = -z -+ (-A/2 - z^2 + B/(4z))^(1/2), r3, r4 = z -+ (-A/2 - z^2 - B/(4z))^(1/2) (95). Cases (GLa Sec. VI's recipe):
//   r2 complex: case IV, substitution (4); else r4 complex: case III, (3); else r4 > r+: case I, (1) if the camera is below r3, (2) if above r4; else case II, (2).
//   Per case, with rij = ri - rj and the antiderivatives that vanish at the turning point or start of the substitution:
//   (1) r2 < r <= r3 (App. B.1): I_0 = F^(1) (B16), (B20); r(tau) (B27). Only rays that fall in (the camera is inside r3): no I_+-.
//   (2) r >= r4 (App. B.2): I_0 = F^(2) (B36), (B40); I_+- = -Pi_+-^(2) - F^(2) / r_+-3 (B39), (B43); r(tau) (B46).
//   (3) r1 < r2 < r- (App. B.3): I_0 = F^(3) (B67), (B71); I_+- (B70) with R1 (B62), f1 (B65), alpha_+- (B66); A, B (B56), (B57), k3 (B59); r(tau) (B75).
//   (4) all complex (App. B.4): I_0 = F^(4) (B97), (B101); I_+- (B100) with S1 (B92), f2 (B95), g0 (B88), g_+- (B96); C, D (B85), k4 (B87); r(tau) (B109),
//     written as r = a2 tan(am(X4) - arctan g0) - b1 (the same by the tangent's subtraction formula; no sign ambiguity: x4 > 0).
//   I_phi = (2 a / (r+ - r-)) [(r+ - a lambda / 2) I_+ - (r- - a lambda / 2) I_-] (B2), with I_+- the path integrals of 1 / ((r - r+-) sqrt R) (B4).
//   Each path integral = the antiderivative's difference along the path (B7): outward from the camera, I(far end) - I(camera); inward to a turning point and
//   out again (case (2), r4 > r+), I(camera) + I(infinity) (the antiderivatives vanish at r4: x2(r4) = 0); inward to the horizon, I(camera) - I(end);
//   case (1) outward, up to r3 and back in: 2 I(r3) - I(camera) - I(end).
// FROM A FINITE RADIUS: GLa's antiderivatives are functions of r for any r in the range of the motion (the "source" and "observer" of (B7) are any two points),
//   so a path between two finite radii and a path from a finite radius to infinity are both the difference of the same antiderivative; infinity is its limit
//   (x2 -> (r31/r41)^(1/2), x3 -> (A - B)/(A + B), x4 -> infinity). The paper needs nothing more for the lab's hovering camera.
// WHERE THE RAY ENDS: the sky at tau_inf (r = infinity; its direction there (sin theta cos phi, sin theta sin phi, cos theta), as mino.h); the horizon: the lab
//   (as geodesic.h and mino.h) ends a falling ray at r+ + 1e-3; its theta and dtheta/dr there are given (spin_sky.cpp continues the crossings from them).
// THE THIRD KIND beyond its pole (cases (2) case II, (3), (4) have n s^2 > 1 on the path): elliptic.h's ellPiang continues the antiderivative past the pole, so
//   a difference between two points on the path (which never meets r+-) is exact; tested against a direct quadrature of (B4) (test_blackhole.cpp testClosed).
//
// TIME AND AZIMUTH AT A CROSSING (0.56.4, PLAN_MOVINGGAS section 3): phi = phi_camera - I_phi - lambda G_phi (11), (B2), and the time the light took from the
//   crossing to the camera, I_t + a^2 G_t (12), (B3), with (13c), (13f): I1, I2 of every case ((B17)-(B18), (B37)-(B38), (B68)-(B69) with R2 (B64), (B98)-(B99)
//   with S2 (B94)), I+- of case (1) ((B19), (B23)), G_t of type A ((31), (48)) and B ((58), (68)), written per amplitude of the substitution so that turning
//   points need no case of their own (kerr_closed.cpp Radial::timeInts, Polar::Gt). E(phi | m) by Carlson's R_D. Only differences between the camera and the
//   crossing (the camera is at a finite radius; I2 grows without bound at infinity). Tested against the integrator with t added (gas.h KerrBLt).
// FOR A SHADER LATER: no recursion; every loop has a fixed bound (Carlson's R_F, R_J, R_C: 60 at most; the AGM: 15). Elementary functions used: sqrt, sin, cos,
// asin, acos, atan, log, cbrt, fabs, floor, ldexp (a power of two).
#pragma once

#include "blackhole/geodesic.h"

namespace blackhole {

constexpr int kClosedCross = 8;
struct ClosedEnd {
    RayEnd::Kind kind = RayEnd::Lost;   // Lost: no case applies (counted by the callers; spin_sky falls back to mino.h for it)
    Vec3 dir;                    // Sky: the direction at infinity (chart axes)
    double b = 0, q = 0;
    int nCross = 0;
    double u[kClosedCross] = {};  // 1 / r at the first crossings of the equator, in the order the ray meets them going into its past
    double phi[kClosedCross] = {}; // 0.56.4: the azimuth (Boyer-Lindquist phi) at those crossings, and the time the light took from there to the camera
    double t[kClosedCross] = {};   //   (far clock, M, > 0): GLa (11), (12) (kerr_closed.h "TIME AND AZIMUTH")
    double pz = 0;               // Sky: the z-part of the asymptote's offset (M) = (dmu/ds) / (du/ds) at infinity (mino.h)
    double rEnd = 0, thEnd = 0, dthdr = 0;   // Horizon: where the ray is ended (r+ + 1e-3), its theta and dtheta/dr there
    int radial = 0;              // the substitution used: 1..4 (GLa App. B); 5: case II (substitution (2), all roots inside the horizon)
    bool vortical = false;       // polar type B
};

// the ray the camera sees in direction look (KerrBL's launch), into its past; at most maxCross crossings (<= kClosedCross)
ClosedEnd traceClosed(double a, const Camera& cam, const Vec3& look, int maxCross);

// for the tests: the roots (95) and the case, and the path integrals of App. B evaluated from the camera's radius rs to r (no turning point between; r may be
// infinite): I0 = Int dr / sqrt R, Ip and Im = Int dr / ((r - r+-) sqrt R)
struct ClosedRadial { double r1, r2, r3, r4; double im1, im2, im3, im4; int sub; };
ClosedRadial closedRoots(double a, double lambda, double eta);
bool closedRadialIntegrals(double a, double lambda, double eta, double rs, double r, double& I0, double& Ip, double& Im);
// 0.56.4, for the tests: the ray's point at Mino time tau from the camera (no crossing needed; tau below the ray's end): r, cos theta, phi, and the time the light
// took from there to the camera (t_camera - t, M)
struct ClosedState { double r = 0, mu = 0, phi = 0, t = 0; int sub = 0; bool vortical = false; };
bool closedStateAt(double a, const Camera& cam, const Vec3& look, double tau, ClosedState& st);

}  // namespace blackhole
