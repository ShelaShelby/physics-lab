// Hyperfine structure (0.31): the magnetic-dipole splitting of a one-electron atom's levels by the nucleus's magnetic moment. No drawing here, so it can be tested.
//
// The model, and what it leaves out (the app says so where it shows the numbers):
//   - The nucleus: its ground-state spin I and magnetic dipole moment mu (nuclear magnetons) from the IAEA table of recommended moments (data/moments/, made into
//     hfs_data.inc by tools/make_hfs_data.py). A nucleus with I = 0 (every even-even one) has no moment: no hyperfine structure.
//   - The electron: the lab's non-relativistic hydrogen-like level (n, l, j). The magnetic dipole constant
//         A = alpha^4 m_e c^2 g_I (m_e / m_p) Z^3 / (n^3 j (j + 1) (2l + 1)) (m_r / m_e)^3,   g_I = mu / I,
//     (the Fermi contact term for s levels; the orbital and spin-dipole terms for l > 0; textbook: H. A. Bethe & E. E. Salpeter, "Quantum Mechanics of One- and
//     Two-Electron Atoms" (1957) section 22; M. I. Eides, H. Grotch, V. A. Shelyuto, Phys. Rep. 342 (2001) eq. (271) for the s levels, with the reduced mass m_r).
//     For s levels the electron's spin moment is larger than Dirac's by its anomaly a_e (CODATA 2022), so A is multiplied by (1 + a_e); for l > 0 it is left out
//     (a 0.1 % effect on part of A). The levels: E_F = (A / 2) [F (F + 1) - I (I + 1) - j (j + 1)], F = |I - j| ... I + j.
//   - 0.33: s levels are multiplied by Breit's relativistic factor (relativisticFactorS below): for 1s 1 / (gamma (2 gamma - 1)), gamma = sqrt(1 - (Z alpha)^2):
//     +0.008 % at Z = 1, +24 % at Z = 50, x 2.1 at Z = 83. (0.31's comment here gave 1 / (gamma (2 gamma^2 - 1)), +0.013 % and +46 %: that was wrong.) For l > 0
//     the levels stay non-relativistic (the Dirac p and d factors are not in).
//   - Left out: the QED corrections beyond a_e,
//     the nucleus's size and the spread of its magnetism (Zemach, Bohr-Weisskopf), its recoil beyond m_r, and the electric quadrupole term (I >= 1, j >= 3/2).
//     Hydrogen's 21 cm line comes out 0.014 % above the measured one (Eides et al.'s left-out terms close it to 1e-5), deuterium's 0.004 % below; hydrogen-like
//     holmium-165 about 7 % above, mostly the nucleus's size (Shabaev et al. 1997; tests/test_hfs.inc).
#pragma once

#include <string>
#include <vector>

namespace atom::hfs {

constexpr double kAe = 1.15965218046e-3;   // electron magnetic moment anomaly, CODATA 2022: 1.159 652 180 46(18)e-3 (NIST, physics.nist.gov/cgi-bin/cuu/Value?ae)

struct Nucleus { int Z, A, twoI; bool spinTentative; double mu, muUnc; int sign; };   // sign: 1 measured, 2 assumed by the table, 0 unknown (mu is then its size)
const Nucleus* find(int Z, int A);         // the IAEA ground state, or nullptr (not in the table: no measured moment)
int tableSize();

// A (eV) of level (n, l, j = twoJ / 2) of the one-electron ion with Z protons and N neutrons (0 when the nucleus has no moment or is not in the table)
double aConstEV(int Z, int N, int n, int l, int twoJ);
// 0.33: Breit's relativistic factor of an nS level's splitting over the non-relativistic one (point nucleus): Eides, Grotch & Shelyuto, Phys. Rep. 342 (2001)
// eq. (274), n^3 [1 + 2 sqrt(1 - (Z alpha)^2 / N^2)] / (N^3 gamma (4 gamma^2 - 1)), N = sqrt(n^2 - 2 (Z alpha)^2 (n - 1) / (1 + gamma)), gamma = sqrt(1 - (Z alpha)^2);
// for n = 1 it is 1 / (gamma (2 gamma - 1)), Shabaev et al., Phys. Rev. A 56, 252 (1997) eq. (2). Z alpha < 1.
double relativisticFactorS(int Z, int n);
struct Level { int twoF; double eEV; };
std::vector<Level> levels(int Z, int N, int n, int l, int twoJ);   // the F levels, lowest first (empty: no hyperfine structure)
double splittingEV(int Z, int N, int n, int l, int twoJ);         // highest minus lowest F level (0: none)
constexpr double kEVtoMHz = 1.602176634e-19 / 6.62607015e-34 / 1e6;   // E / h, exact
// The Spectrum tab's paragraph for the ground state 1s of this ion: the nucleus's spin and moment, the F levels and the splitting as an energy, a frequency and a
// wavelength, with the model's limits; or why there is none (spin 0: every nucleus with even Z and even N; or no measured moment in the table).
std::string groundText(int Z, int N);

}  // namespace atom::hfs
