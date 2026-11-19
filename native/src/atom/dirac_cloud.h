// 0.49.3 (outside review): the CLOUD of a one-electron atom from the Dirac equation (the levels and lines are dirac.h). No drawing here, so it can be tested.
//
// What it is:
//   - A stationary state of the Dirac equation for one electron round a point nucleus of charge Z: (n, l, j), with its large component P = r g and small component Q = r f (Grant's
//     convention, as GRASP and the lab's many-electron clouds: P' + (kappa / r) P - ((E - V + 2 c^2) / c) Q = 0, Q' - (kappa / r) Q + ((E - V) / c) P = 0, atomic units, V = -Z / r,
//     kappa = -(l + 1) for j = l + 1/2 and l for j = l - 1/2). The radial functions are the closed form (Berestetskii, Lifshitz & Pitaevskii, Quantum Electrodynamics, section 36; Bethe & Salpeter
//     section 14), with n_r = n - |kappa|, gamma = sqrt(kappa^2 - (Z alpha)^2), eps = E_total / m c^2, N = sqrt(n_r^2 + 2 n_r gamma + kappa^2), rho = 2 Z r / N:
//         P = C sqrt(1 + eps) e^(-rho/2) rho^gamma [ n_r M(1 - n_r, 2 gamma + 1, rho) - (N - kappa) M(-n_r, 2 gamma + 1, rho) ]
//         Q = C sqrt(1 - eps) e^(-rho/2) rho^gamma [ n_r M(1 - n_r, 2 gamma + 1, rho) + (N - kappa) M(-n_r, 2 gamma + 1, rho) ]
//     (M = Kummer's function, a polynomial here). Written from knowledge; the signs were fixed and the whole form checked against the radial equations themselves (the tests repeat that check),
//     C by integrating P^2 + Q^2 to 1. The energy in the equations is dirac.h's diracInfiniteEV, an independent formula.
//   - Its density: (P^2 + Q^2) / r^2 times |Omega_kappa,m|^2, the same angular factor for both components (Omega_-kappa,m = -(sigma . r_hat) Omega_kappa,m, and sigma . r_hat is unitary at
//     every point). Lined up, m = j: |Omega|^2 = |Y_L,L|^2 with L = j - 1/2, proportional to sin^(2L) theta (j = 1/2: round; 2p3/2: a ring round z; 3d5/2: a flatter ring). Round: the average over m.
//   - Lengths: the radial functions are for an infinitely heavy nucleus; the lab draws them in its reduced-mass Bohr radius (atom_sim.h aUnit), as the non-relativistic clouds.
//   - Not included: the Lamb shift and the finite size of the nucleus in the WAVE (they are in the energies, dirac.h), hyperfine structure.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "orbit/belts.h"

namespace atom {

int diracKappa(int l, int j2);                                     // -(l + 1) for j = l + 1/2, l for j = l - 1/2
double diracGamma(int Z, int kappa);                               // sqrt(kappa^2 - (Z alpha)^2)
// the normalised radial functions P = r g and Q = r f at r (Bohr radii, infinite nuclear mass), and P^2 + Q^2 (the radial probability density: its integral over r is 1)
void diracPQ(int Z, int n, int kappa, double r, double& P, double& Q);
double diracRadialDensity(int Z, int n, int kappa, double r);
double diracMeanRadius(int Z, int n, int kappa);                   // <r>, Bohr radii
double diracCdf(int Z, int n, int kappa, double r);                // the share of the probability inside r
double diracSmallShare(int Z, int n, int kappa);                   // the integral of Q^2: the small component's share of the electron

// sampling (Bohr radii, infinite mass): the radius by inverting the distribution function (a table of 4,001 values on r = 1e-7 .. 250, cubic Hermite); the direction lined up (m = j:
// sin^(2L) theta by rejection, L = j - 1/2) or round (uniform)
double sampleDiracRadius(int Z, int n, int kappa, orbit::Mulberry32& r);
void sampleDiracDirection(int j2, bool round, orbit::Mulberry32& r, double& ux, double& uy, double& uz);
void sampleDirac(int Z, int n, int l, int j2, bool round, orbit::Mulberry32& r, double& x, double& y, double& z);
// The Slice view: a point ON the plane {x[axis] = off} (axis 1: y, 2: z), with exactly the density restricted to the plane; false: this attempt was rejected (try again)
bool tryDiracOnPlane(int Z, int n, int l, int j2, bool round, int axis, double off, orbit::Mulberry32& r, double& x, double& y, double& z);
// 0.65.0: the Slice's dots of a Dirac state in a row: attempts of tryDiracOnPlane until `want` dots are kept or maxTries attempts are made, each kept dot
// given to `record`; the same dots and the same random numbers as calling it attempt by attempt (the radii of four attempts worked out side by side).
// Returns the dots kept.
long diracPlaneDots(int Z, int n, int l, int j2, bool round, int axis, double off, orbit::Mulberry32& r, long want, long maxTries,
                    const std::function<void(double, double, double)>& record);

// The radial graph's curve for a Dirac state (radial_graph.h's convention: P(r) = the radial probability density, area 1)
struct DiracGraph { std::vector<double> r, D; double rEnd = 0, peak = 0, mean = 0; std::vector<double> maxima; };
DiracGraph diracGraph(int Z, int n, int kappa);

// The odds of the electric-dipole jumps from (l, j) to the two j' of l' (one electron, LS coupling): the relative line strength (2j + 1)(2j' + 1) {l j 1/2; j' l' 1}^2 (Wigner 6j symbol,
// Racah's formula). For one upper level they are also the odds of its decays (the frequencies of the two lines differ by the fine structure only; the lab neglects that, below 1e-3 for Z < 30).
double wigner6j(double j1, double j2, double j3, double j4, double j5, double j6);
double lineStrength(int l, int j2, int lp, int jp2);              // 0 when the jump is not allowed

std::string diracStateText(int n, int l, int j2);                 // "2p₃/₂"

}  // namespace atom
