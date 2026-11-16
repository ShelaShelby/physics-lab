// Black Hole Lab (0.55.8): a faster form of the same ray equations, for the spin table (spin_sky.h). Physics only, no graphics. G = c = 1, lengths in M.
//
// WHY: the table's rays are followed by geodesic.h's KerrBL in the affine parameter zeta: every right-hand side needs a sine, a cosine and about eight
// divisions, and an escaping ray is followed out to r = 1e4 times the camera's radius. Measured (REPORT 0.55.8) nearly all of the table's time is in these rays.
//
// THE FORM (derived here from J15 (A.4), (A.12), (A.15), the equations geodesic.h already uses; J15 = James et al. 2015, saved in data/blackhole):
//   the parameter lambda with d lambda = d zeta / rho^2 (usually called Mino time; nothing here is taken from elsewhere). (A.15) times rho^2 gives  dr/dlambda = Delta p_r,  dtheta/dlambda = p_theta,
//   dphi/dlambda = a P / Delta + (b - a) + b cos^2 theta / sin^2 theta = a P / Delta - a + b / sin^2 theta, with P, R, Theta of (A.4).
//   On a light ray the Hamiltonian of (A.15) is 0 and (A.12) holds, so p_theta^2 = Theta(theta) and Delta p_r^2 = R / Delta: the two motions separate,
//     (dr/dlambda)^2 = R(r),  (dtheta/dlambda)^2 = Theta(theta).
//   With u = 1/r and mu = cos theta (polynomials, no square roots and no sines):
//     (du/dlambda)^2 = u^4 R(1/u) = U(u) = (1 + A u^2)^2 - K (u^2 - 2 u^3 + a^2 u^4),   A = a^2 - a b,  K = (b - a)^2 + q,
//     (dmu/dlambda)^2 = sin^2 theta Theta = Mu(mu) = q - (q + b^2 - a^2) mu^2 - a^2 mu^4,
//   and differentiating once (the turning points need no sign):  u'' = U'(u) / 2 = 2 A u (1 + A u^2) - K (u - 3 u^2 + 2 a^2 u^3),
//     mu'' = Mu'(mu) / 2 = -(q + b^2 - a^2) mu - 2 a^2 mu^3,   phi' = a (1 + A u^2) / (1 - 2 u + a^2 u^2) - a + b / (1 - mu^2)   (P / Delta in u).
//   The state y = (u, du/ds, mu, dmu/ds, phi) in s = -lambda (into the light's past, as KerrBL's zeta decreasing): u'' and mu'' keep their sign, phi' changes it.
//   From KerrBL's launch: du/ds = u^2 Delta p_r, dmu/ds = sin theta p_theta.
// THE FAR SKY: U(0) = 1, so u reaches 0 (r = infinity) at a finite s with du/ds = -1, smoothly: the ray is followed to u = 0 itself (Newton on the step's length,
//   as the plane's crossings) and its direction there is the direction of its place at infinity, (sin theta cos phi, sin theta sin phi, cos theta): no rest of
//   the bending is left out (KerrBL stops at a large radius instead). The asymptote's offset from the centre, p = d n / du at u = 0 (n the unit vector to the
//   ray's place: n = d + u p + ...), has the z-part p_z = (dmu/ds) / (du/ds) there (spin_sky.cpp's continued crossing uses it).
// THE HORIZON: as KerrBL, inside r+ + 1e-3 going inward (only phi' has a pole there, at Delta = 0).
// THE PLANE: mu = 0, its rate dmu/ds (the disc's crossings, found as KerrBL's: Newton on the step's length, each trial one full step).
// THE STEP: the same Dormand-Prince 5(4) pair and step control as geodesic.cpp, the error measured on this state.
// Tests (test_blackhole.cpp testMino): the null conditions (du/ds)^2 = U(u) and (dmu/ds)^2 = Mu(mu) along rays; the far direction, b and the crossings against
// KerrBL's trace at its full tolerance at spin 0.01, 0.6, 0.9, 0.99, near and far from the shadow's edge.
#pragma once

#include <vector>

#include "blackhole/geodesic.h"

namespace blackhole {

// rays that come within sin^2 theta < kMinoPole of the spin axis are not followed by this form (nearPole): there phi turns by about pi within a stretch of s
// shorter than the steps (phi' = b / (1 - mu^2) is a narrow spike while mu itself passes smoothly), and a step can pass over it unseen
constexpr double kMinoPole = 1e-3;

struct MinoEnd {
    RayEnd::Kind kind = RayEnd::Lost;
    Vec3 dir;                    // Sky: the direction at infinity (chart axes)
    double b = 0, q = 0;
    double y[5] = {}, dy[5] = {};   // the end state (u, du/ds, mu, dmu/ds, phi) and its rate
    double pz = 0;               // Sky: the z-part of the asymptote's offset (M)
    int steps = 0, rejected = 0;
    bool nearPole = false;       // not followed: its sin^2 theta comes below kMinoPole (mino.h)
};

// the ray seen in direction look by the camera (KerrBL's launch), into its past until it ends; the first o.maxCross crossings of the equator appended to
// *uCross as u = 1/r (if given)
MinoEnd traceMino(double a, const Camera& cam, const Vec3& look, const TraceOptions& o, std::vector<double>* uCross = nullptr);
// the rate of the state (for the tests)
void minoRhs(double a, double b, double q, const double* y, double* dy);
// the two null conditions, relative to the size of their terms: ((du/ds)^2 - U) and ((dmu/ds)^2 - Mu)
void minoNull(double a, double b, double q, const double* y, double& eu, double& emu);

}  // namespace blackhole
