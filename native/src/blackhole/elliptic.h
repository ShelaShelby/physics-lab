// Black Hole Lab (0.54.3): Legendre's elliptic integrals of the first kind in double precision, for the closed-form light paths (blackhole/schwarzschild.h).
// No physics in here. Written by hand (not std::ellint_1: the C++17 special functions are missing from some standard libraries), with Carlson's symmetric
// integral R_F computed by the duplication method; tests/test_blackhole.cpp checks it against identities that hold exactly:
//   F(phi, 0) = phi,  F(phi, 1) = atanh(sin phi),  F(pi/2, k) = K(k),  K(0) = pi/2,  F(phi + pi, k) = F(phi, k) + 2 K(k),  and a direct quadrature.
// Convention: k is the MODULUS (Luminet 1979 writes F(zeta, k) with modulus k). Gralla & Lupsasca 2020 and Berens et al. 2026 write F(phi | m) with the
// PARAMETER m = k^2 under the root; callers convert (ellF(phi, std::sqrt(m))).
#pragma once

namespace blackhole {

double carlsonRF(double x, double y, double z);       // R_F(x, y, z) = 1/2 Int_0^inf dt / sqrt((t + x)(t + y)(t + z)), x, y, z >= 0, at most one zero
double ellF(double phi, double k);                     // F(phi, k) = Int_0^phi dt / sqrt(1 - k^2 sin^2 t), any real phi, 0 <= k <= 1 (k = 1 only for |phi| < pi/2)
double ellK(double k);                                 // K(k) = F(pi/2, k), 0 <= k < 1
// The same with the complementary parameter kc2 = 1 - k^2 given directly (no rounding when k is close to 1):
double ellFc(double phi, double kc2);                  // F(phi, k), any real phi
double ellKc(double kc2);                              // K(k)
double ellFsc(double s, double c, double kc2);         // F(zeta, k) for 0 <= zeta <= pi/2 given by s = sin zeta, c = cos zeta (no rounding near pi/2)

// ---- 0.55.9: what the spinning hole's closed forms (blackhole/kerr_closed.h) need besides F. Below, m is the PARAMETER (m = k^2) and kc2 = 1 - m is given
// directly; m may be negative (kc2 > 1), as Gralla & Lupsasca's polar integrals have it. Every function has a fixed bound on its loops and no recursion.
// Carlson's symmetric integrals by the duplication method (B. C. Carlson, Numer. Algorithms 10, 13 (1995)): each step replaces the arguments by (x + l) / 4 ...,
// l = sqrt(xy) + sqrt(yz) + sqrt(zx), which keeps the integral; once they are within a relative 1e-3 of their mean, a series in the differences ends it.
//   R_C(x, y) = 1/2 Int_0^inf dt / ((t + y) sqrt(t + x)),  R_J(x, y, z, p) = 3/2 Int_0^inf dt / ((t + p) sqrt((t + x)(t + y)(t + z)))   (x, y, z >= 0, p > 0)
// Legendre's third kind Pi(n; phi | m) = Int_0^phi dt / ((1 - n sin^2 t) sqrt(1 - m sin^2 t)) (the convention of Gralla & Lupsasca 2020 and Berens et al. 2026,
// Berens (32)), for |phi| <= pi/2 by Carlson's reduction  Pi = s R_F(c^2, d^2, 1) + (n/3) s^3 R_J(c^2, d^2, 1, 1 - n s^2),  s = sin phi, c = cos phi,
// d^2 = 1 - m s^2 (it follows from the substitution t' = cot^2 t in the integral; tested against a direct quadrature).
// n > 1 with 1 - n s^2 <= 0 (beyond the integrand's pole at sin^2 t = 1/n): the antiderivative continued past the pole (derived here): with n' = m / n < 1 and
//   P = ((n - 1)(1 - m / n))^(1/2),   G(phi) = F(phi | m) - Pi(n'; phi | m) + (1 / (2P)) ln| (d c + P s) / (d c - P s) |
//   differentiating: dG/dphi = 1/d - 1/((1 - n' s^2) d) + (1 - m s^4) / (d (c^2 d^2 - P^2 s^2)), and c^2 d^2 - P^2 s^2 = (1 - n s^2)(1 - n' s^2) for exactly this P,
//   so dG/dphi = n (1 - m s^4) / (n (1 - n s^2)(1 - n' s^2) d) ... = 1 / ((1 - n s^2) d): G is an antiderivative on each side of the pole, G(0) = 0, and G = Pi
//   below the pole. A definite integral between two amplitudes on the same side of the pole is the difference of G (that is how the closed forms use it).
//   (d c - P s is computed as (1 - n s^2)(1 - n' s^2) / (d c + P s), no cancellation near the pole.)
// Both kinds for any amplitude: F(phi + pi) = F(phi) + 2K, Pi(phi + pi) = Pi(phi) + 2 Pi(n | m) and the reflection f(pi - phi) = 2 f(pi/2) - f(phi) (the
// integrands are even about pi/2); G (n > 1) keeps the reflection with G(pi/2) = K - Pi(n' | m) (its log term changes sign with c).
// The Jacobi amplitude am(u | m) (F(am(u) | m) = u): for 0 <= m < 1 by the arithmetic-geometric mean and its descending recursion (M. Abramowitz, I. A. Stegun,
// Handbook of Mathematical Functions (1964), 16.4); for m < 0 (derived here): with mu = -m / (1 - m) in (0, 1) and t = pi/2 - t', 1 - m sin^2 t =
// (1 - m)(1 - mu sin^2 t'), so F(phi | m) = (K(mu) - F(pi/2 - phi | mu)) / (1 - m)^(1/2) and am(u | m) = pi/2 - am(K(mu) - u (1 - m)^(1/2) | mu); its rate
// (1 - m)^(1/2) dn(K(mu) - u (1-m)^(1/2) | mu) = (1 - m sin^2 am)^(1/2) (checked in the derivation, and tested: F(am(u)) = u).
double carlsonRC(double x, double y);                  // y > 0 (in closed form, elliptic.cpp)
double carlsonRCdup(double x, double y);               // the same by the duplication method (the tests' reference)
double carlsonRJ(double x, double y, double z, double p);   // p > 0
double ellFang(double s, double c, double kc2);        // F(phi | m) for any phi in (-pi, pi] given by s = sin phi, c = cos phi
double ellPiang(double n, double s, double c, double kc2);   // Pi(n; phi | m), any phi in (-pi, pi] (n > 1: G above), n != 1
double ellPic(double n, double kc2);                   // Pi(n | m) = Pi(n; pi/2 | m), n < 1
double jacobiAm(double u, double kc2);                 // am(u | m), any real u, m < 1

}  // namespace blackhole
