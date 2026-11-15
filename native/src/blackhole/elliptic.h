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

}  // namespace blackhole
