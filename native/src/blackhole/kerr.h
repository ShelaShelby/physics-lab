// Black Hole Lab (0.54.3): the spinning (Kerr) hole's closed forms that every later step needs. Physics only, no graphics, double precision.
// Units: G = c = 1, lengths in M (the hole's mass; M = GM/c^2 = "r_g"), spin a in M, 0 <= a <= 1. Upper sign of the papers = orbiting WITH the spin (prograde,
// pro = true), lower sign = against it (retrograde).
//
// Special radii, J. M. Bardeen, W. H. Press, S. A. Teukolsky, ApJ 178, 347 (1972) (data/blackhole/scans, read from the page images):
//   horizon        r+ = M + (M^2 - a^2)^(1/2)                                              (2.6)   "Over the range 0 <= a <= M, r+ varies from 2M to M"
//   static limit   r0 = M + (M^2 - a^2 cos^2 theta)^(1/2)                                  (2.7)
//   photon orbit   r_ph = 2M {1 + cos[2/3 arccos(-+a/M)]}                                 (2.18)  "For a = 0, r_ph = 3M, while for a = M, r_ph = M (direct) or 4M (retrograde)"
//                  (its root condition r^(3/2) - 3M r^(1/2) +- 2a M^(1/2) = 0 is (2.17) with equality)
//   marginally bound r_mb = 2M -+ a + 2M^(1/2) (M -+ a)^(1/2)                             (2.19)  "For a = 0, r_mb = 4M; for a = M, r_mb = M (direct) or 5.83M (retrograde)"
//   innermost stable r_ms = M{3 + Z2 -+ [(3 - Z1)(3 + Z1 + 2 Z2)]^(1/2)},
//                  Z1 = 1 + (1 - a^2/M^2)^(1/3) [(1 + a/M)^(1/3) + (1 - a/M)^(1/3)],  Z2 = (3 a^2/M^2 + Z1^2)^(1/2)   (2.21)
//                  "For a = 0, r_ms = 6M; for a = M, r_ms = M (direct) or 9M (retrograde)"; it is the root of r^2 - 6Mr +- 8a M^(1/2) r^(1/2) - 3a^2 = 0 (p. 351, above (2.20)).
//   The same radius as D. N. Page, K. S. Thorne, ApJ 191, 499 (1974), (15k)-(15m): x0 = (r_ms/M)^(1/2) = {3 + Z2 - sgn(a*)[(3 - Z1)(3 + Z1 + 2Z2)]^(1/2)}^(1/2)
//   with a signed spin a* (negative = the gas orbits against the spin): iscoPageThorne below, tested to agree.
//
// The metric (Boyer-Lindquist chart), O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, Class. Quantum Grav. 32, 065001 (2015) (arXiv:1502.03808,
// data/blackhole/arxiv, read from the PDF), M = 1:
//   ds^2 = -alpha^2 dt^2 + (rho^2/Delta) dr^2 + rho^2 dtheta^2 + varpi^2 (dphi - omega dt)^2                         (A.1)
//   rho = (r^2 + a^2 cos^2 theta)^(1/2),  Delta = r^2 - 2r + a^2,  Sigma = ((r^2 + a^2)^2 - a^2 Delta sin^2 theta)^(1/2)
//   alpha = rho Delta^(1/2) / Sigma,  omega = 2 a r / Sigma^2,  varpi = Sigma sin theta / rho                             (A.2)
#pragma once

namespace blackhole {

double horizon(double a);                                   // r+ (2.6)
double staticLimit(double a, double theta);                 // r0 (2.7)
double photonOrbit(double a, bool pro);                     // r_ph (2.18)
double marginallyBound(double a, bool pro);                 // r_mb (2.19)
double innermostStable(double a, bool pro);                 // r_ms (2.21)
double iscoPageThorne(double aSigned);                      // Page & Thorne (15k)-(15m): r_ms = x0^2 for a signed spin
double iscoCondition(double r, double a, bool pro);         // r^2 - 6r +- 8a r^(1/2) - 3a^2 (BPT p. 351): zero at r_ms, positive outside it
double photonCondition(double r, double a, bool pro);       // r^(3/2) - 3 r^(1/2) +- 2a (BPT (2.17)): zero at r_ph

struct KerrMetric { double rho2, Delta, Sigma2, alpha, omega, varpi; };
KerrMetric kerrMetric(double a, double r, double theta);    // J15 (A.1), (A.2)

// The light seen by a camera that hovers (a "FIDO", at rest in the frame of J15 (A.3)) at (r, theta), for a ray with axial angular momentum b
// (p_t = -1): the frequency it sees over the frequency emitted at rest far away, f_c / f' = (1 - b omega) / alpha (J15 (A.16) with beta = 0).
// a = 0: 1 / (1 - 2M/r)^(1/2), the same for every ray (A. Riazuelo, IJMPD 28, 1950042 (2019), (B18), (B19): z_grav = -1 + (1 - 2M/r)^(1/2)).
double hoverShift(double a, double r, double theta, double b);

}  // namespace blackhole
