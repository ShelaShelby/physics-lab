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
// THE DISC'S SURFACE (0.95.0; the spin-0 rule of disc.h surfaceStep, in Mino time). The disc's surface is |z| = h(R) with z = r cos theta and
//   R = r sin theta (Boyer-Lindquist r, theta: the height above the equator and the distance from the spin axis; lab.h keepOutsideDisc and the spin-0 lens
//   code use the same z and R). Near a crossing at Mino time tau_k the ray is followed back towards the camera: tau = tau_k - dtau. The polar motion of type A
//   has Psi = j pi at the crossing (cos theta = -nu u+^(1/2) sin Psi, (49)), so am(X - sq dtau) = j pi - am(sq dtau) and |cos theta| = u+^(1/2) sin(phi) with
//   phi = am(sq dtau | u+/u-) exactly, sq = (-u- a^2)^(1/2): phi plays the swept angle's part of spin 0 (at a = 0 it is the swept angle) and C = u+^(1/2)
//   the part of (A^2 + B^2)^(1/2). The radius on the line through the crossing: r = r_k - (dr/dPsi) phi, dr/dPsi = (dr/dtau) / sq, (dr/dtau)^2 = R(r_k) (7)
//   with the ray's own sign there (nu_r, turned at r4 in case (2) and at r3 in case (1)). surfaceStep's kSurfaceSteps fixed-point steps on that line, then
//   kSurfaceExactSpin steps on the closed forms: dtau = F(phi | u+/u-) / sq (F inverts am), r = r(tau_k - dtau) ((B27), (B46), (B75), (B109)), |z| = r u+^(1/2)
//   sin phi, phi = asin(h(R) / (r u+^(1/2))) (kept below kSurfaceMaxSpin); the hit at the last phi. Only the first kSurfaceOrders crossings (the direct and the
//   underside's images; the thin rings beyond stay at the plane). A hit before the camera (dtau >= tau_k; the camera rule keeps it outside the slab) keeps
//   the crossing. The moving gas's phi and t stay those of the crossing (the slab's height, a few GM/c^2 of travel, is left out of them).
//   Tested against the integrator followed until |z| = h(R) (tests/test_blackhole.cpp testSpinSurface095).
// FOR A SHADER LATER: no recursion; every loop has a fixed bound (Carlson's R_F, R_J, R_C: 60 at most; the AGM: 15). Elementary functions used: sqrt, sin, cos,
// asin, acos, atan, log, cbrt, fabs, floor, ldexp (a power of two).
#pragma once

#include "blackhole/disc.h"
#include "blackhole/geodesic.h"

namespace blackhole {

constexpr int kClosedCross = 8;
// 0.95.0: the largest phi of the surface's exact steps. It stays below pi/2, where the ray turns in theta (|cos theta| at most u+^(1/2)). Spin 0's 0.5 rad
// (disc.h kSurfaceMax) failed testSpinSurface095's grazing view (spin 0.99, the camera at the camera rule's limit 20 GM/c^2 out: the rays level with the
// camera skim the slab and meet it far from their crossing; 98.6 % of the pixels within 0.5 px); 1.2 passes.
constexpr double kSurfaceMaxSpin = 1.2;
// 0.95.0: one exact step at spin, then the hit (spin 0's kSurfaceExact is 2). Each step costs the sky shader an F and a Jacobi amplitude per crossing; with
// two the sky pass took 588 ms instead of 168 on the build machine (REPORT 0.95.0). testSpinSurface095 passes with one (and with two).
constexpr int kSurfaceExactSpin = 1;
struct ClosedEnd {
    RayEnd::Kind kind = RayEnd::Lost;   // Lost: no case applies (counted by the callers; spin_sky falls back to mino.h for it)
    Vec3 dir;                    // Sky: the direction at infinity (chart axes)
    double b = 0, q = 0;
    int nCross = 0;
    double u[kClosedCross] = {};  // 1 / r at the first crossings of the equator, in the order the ray meets them going into its past
    bool timed = false;            // 0.57.2: phi and t below were asked for (traceClosed's timed); else they are 0
    double phi[kClosedCross] = {}; // 0.56.4: the azimuth (Boyer-Lindquist phi) at those crossings, and the time the light took from there to the camera
    double t[kClosedCross] = {};   //   (far clock, M, > 0): GLa (11), (12) (kerr_closed.h "TIME AND AZIMUTH")
    double pz = 0;               // Sky: the z-part of the asymptote's offset (M) = (dmu/ds) / (du/ds) at infinity (mino.h)
    double rEnd = 0, thEnd = 0, dthdr = 0;   // Horizon: where the ray is ended (r+ + 1e-3), its theta and dtheta/dr there
    int radial = 0;              // the substitution used: 1..4 (GLa App. B); 5: case II (substitution (2), all roots inside the horizon)
    bool vortical = false;       // polar type B
    // 0.95.0 (THE DISC'S SURFACE above): cn = u+^(1/2) (type A; 0 otherwise); per crossing dr/dPsi there (the spin table's slope); with a surface asked for,
    // per crossing k < kSurfaceOrders the hit: 1 / R there (0: no surface hit, the crossing stands), its |z| and its Mino time from the camera (tauS: every
    // crossing's own Mino time where it has no surface hit)
    double cn = 0;
    double slope[kClosedCross] = {};
    double uS[kClosedCross] = {}, zS[kClosedCross] = {}, tauS[kClosedCross] = {};
};

// the ray the camera sees in direction look (KerrBL's launch), into its past; at most maxCross crossings (<= kClosedCross). timed (0.57.2): also phi and t at
// the crossings (the moving gas; about five times the cost of the rest, so only where asked)
// 0.95.0: surf (may be null, or without a height table): the disc's surface hits (cn, slope are always given)
ClosedEnd traceClosed(double a, const Camera& cam, const Vec3& look, int maxCross, bool timed = false, const DiscGeom* surf = nullptr);
// 0.95.0: the surface hit of a crossing at radius rk, Mino time tauK, slope dr/dPsi (drk), C = u+^(1/2), sq, the polar kc2 = 1 - u+/u-; r(tau) given (the
// closed forms' rOf, or anything for the tests); returns dtau (0: none) and the hit's r and |z|
double surfaceHitMino(double rk, double tauK, double drk, double C, double sq, double kc2, const HeightTable& h, const DiscGeom& d,
                      double (*rOf)(double tau, const void* ctx), const void* ctx, double& r, double& z);

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
