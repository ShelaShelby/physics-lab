#pragma once
// The Wigner 3j symbol, exact (0.50.2, in the calculator until 0.54.0; moved here so the calculator's cg/wigner3j and the Zeeman effect (atom/zeeman.h) share one copy).
// All arguments doubled (2j, 2m), so half-integers are exact. 0 when a selection rule fails (m1 + m2 + m3 != 0, the triangle rule, |m| > j).
// Racah's formula: A. R. Edmonds, "Angular Momentum in Quantum Mechanics" (Princeton 1957), eq. (3.7.3); DLMF 34.2.4.
namespace atom {
double threeJ(int j1, int j2, int j3, int m1, int m2, int m3);
// The Clebsch-Gordan coefficient <j1 m1 j2 m2 | J M>, arguments doubled: (-1)^(j1 - j2 + M) sqrt(2J + 1) (j1 j2 J; m1 m2 -M) (Edmonds eq. (3.7.1)), Condon-Shortley phase
double clebschGordan(int j1, int m1, int j2, int m2, int J, int M);
}  // namespace atom
