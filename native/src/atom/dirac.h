// Relativity as an option in the Atom Lab (0.10.8 stage 1): the energy levels of a one-electron atom from the Dirac equation for a POINT nucleus, so a level depends on n and on j (the total angular
// momentum), not on n alone, and the lines of the spectrum split. No drawing here, so it can be tested. The setting is off by default (AtomParams::dirac); nothing in the lab changes while it is off.
//
// What this is and is not (the app says the same, in plain words):
//   - It changes the ENERGIES and LINES of the two spectrum cards. The clouds, orbitals, outlines and the radial graph are still the non-relativistic wave functions; the photon of a jump and
//     the quantum beats keep the non-relativistic level (the lab's orbitals have no spin and so no j to give them). (Since 0.46 the energy over the picture is the Dirac level from kRelativityZ on, whatever this setting.)
//   - The Dirac energy alone has no Lamb shift and no finite nuclear size: in Dirac theory 2s(1/2) and 2p(1/2) have exactly the same energy, in reality they do not. Since 0.49.3 the lab adds the Lamb
//     shift of 1s, 2s, 2p1/2 and 2p3/2 for Z = 1-110 (lambShiftEV below: QED with the finite nuclear size, Yerokhin & Shabaev 2015, row LAMB); levels with n >= 3 have none (no source). 0.86.0: this line
//     said "No Lamb shift, no other QED effect" until then (PLAN_REVIEW.md R8).
//   - The nucleus's motion: see diracCorrectionEV below. The main energy has the reduced mass, as always in the lab; the relativistic correction is that of an infinitely heavy nucleus. Recoil terms beyond the
//     reduced mass are not included (no source for them could be read).
#pragma once

#include <string>
#include <vector>

#include "atom_core.h"

namespace atom {

// The exact Dirac energy of a one-electron atom, point nucleus of infinite mass, level (n, j = twoJ / 2), WITHOUT the electron's rest energy (so a bound level is negative), eV:
//   E = m c^2 [ (1 + (Z alpha)^2 / (n - delta)^2)^(-1/2) - 1 ],   delta = (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2).
// The Sommerfeld fine-structure formula, the exact solution of the Dirac equation for a Coulomb potential. 0.86.0 (PLAN_REVIEW.md R7): the source is V. A. Yerokhin and V. M. Shabaev, J. Phys. Chem.
// Ref. Data 44, 033103 (2015), arXiv:1506.01885, Eqs. (2)-(4): E_D = m c^2 [ sqrt(1 - (Z alpha)^2 / N^2) - 1 ], N = sqrt((n_r + gamma)^2 + (Z alpha)^2), gamma = sqrt(kappa^2 - (Z alpha)^2), n_r = n - |kappa|
// (extract: data/lamb/yerokhin_shabaev_2015_dirac.extract.txt). The same formula: |kappa| = j + 1/2, so n_r + gamma = n - delta and sqrt(1 - (Z alpha)^2 / N^2) = (n - delta) / N
// = (1 + (Z alpha)^2 / (n - delta)^2)^(-1/2). Tested against that paper's DIRAC rows for every Z = 1-110 (1s, 2s-1s, 2p3/2-2p1/2; tests/test_atom.cpp testDiracRows086), and against hand
// arithmetic, its expansion and the measured NIST lines. (Until 0.85: "written from knowledge, NOT looked up".) Constants: alpha and m c^2 from CODATA 2022 (atom_core.h). Written with no subtraction of nearly equal numbers (0.10.8 stage 2): delta = (Z alpha)^2 / ((j + 1/2) + sqrt(...)) and
// E = -m c^2 t / (s (1 + s)), t = (Z alpha)^2 / (n - delta)^2, s = sqrt(1 + t), so only +, *, / and sqrt are used and the result is good to a few units of the last digit (the naive form loses up to 10 digits).
// Valid for Z alpha < j + 1/2, i.e. every Z up to 118 (Z alpha = 0.86).
double diracInfiniteEV(int Z, int n, int twoJ);
// 0.86.0: the same energy in units of h c R_inf = alpha^2 m c^2 / 2 (Yerokhin & Shabaev 2015's table unit, "in Rydbergs"), for a given alpha (their table used CODATA 2010's)
double diracInfiniteRy(int Z, int n, int twoJ, double alpha = kAlpha);
// 0.86.0: Yerokhin & Shabaev 2015's DIRAC row for Z (1-110) as printed: [0] 1s, [1] 2s-1s, [2] 2p3/2-2p1/2 in h c R_inf, each with its printed decimals and its bracketed
// uncertainty in units of the last digit (0: none); nullptr outside 1-110 (lamb_data.inc, tools/make_lamb_data.py)
struct DiracPrinted { double v; int decimals, unc; };
const DiracPrinted* diracPrinted(int Z);
// The defect delta = (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2) used inside it (it depends on Z and j only): (Z alpha)^2 / ((j + 1/2) + sqrt(...)), so it keeps its digits where the naive difference loses them.
double diracDelta(int Z, int twoJ);
// The relativistic correction alone: the Dirac energy minus the non-relativistic one with the SAME constants, -(1/2) m c^2 (Z alpha)^2 / n^2. It is of the order (Z alpha)^2 of the level, negative (the level lies deeper). Worked out directly (see dirac.cpp), NOT as the difference of the two energies, which would lose about 5 digits for hydrogen.
double diracCorrectionEV(int Z, int n, int twoJ);
// 0.91.0: the leading recoil correction E_M of Yerokhin & Shabaev 2015, Eq. (3) (data/lamb/yerokhin_shabaev_2015_recoil.extract.txt), in h c R_inf, for a nucleus of
// m / M = x: E_M = m c^2 x (Z alpha)^2 / (2 N^2) - m_r c^2 x^2 (Z alpha)^2 / (2 n^2), N = sqrt((n_r + gamma)^2 + (Z alpha)^2). It depends on n and j only.
// Its first part is the first-order recoil from the Breit equation (Shabaev 1985), the second the reduced mass beyond first order. The tests check it against
// the table's MASS rows (H, He+).
double diracMassRy(int Z, int n, int twoJ, double x, double alpha = kAlpha);
// 0.91.0: what the recoil adds beyond the reduced mass: E_D + E_M - (levelEV + diracCorrectionEV) = m c^2 x (Z alpha)^4 n_r / (n^2 N^2 (|kappa| + gamma)), eV
// (worked out in dirac.cpp: no difference of near-equal numbers). 0 for n_r = 0 (1s, 2p3/2, 3d5/2 ...); hydrogen 2p1/2: 2.5e-14 eV, 0.054 % of the 2p fine structure.
double diracRecoilEV(int Z, int N, int n, int twoJ);
// The level the lab shows with the setting on: the Dirac energy of an infinitely heavy nucleus plus the leading recoil correction E_M above (0.91.0; Yerokhin & Shabaev's
// E_D + E_M). Worked out as the lab's own non-relativistic level with the nucleus's real mass (levelEV, the reduced-mass Rydberg energy), plus the relativistic correction
// (diracCorrectionEV), plus diracRecoilEV. Up to 0.90.0 the recoil beyond the reduced mass was left out (hydrogen's 2p fine structure 0.054 % too wide). The recoil terms
// of higher order in (Z alpha) and m / M are in the Lamb shift below where the source has it (n = 1, 2), and are left out for n >= 3.
double diracEV(int Z, int N, int n, int twoJ);

// 0.49.3 (outside review): the Lamb shift of the levels 1s, 2s, 2p1/2 and 2p3/2 (QED: the self-energy, vacuum polarisation and the higher-order terms, with the finite nuclear size and its
// recoil parts as the source includes them), for Z = 1..110: V. A. Yerokhin and V. M. Shabaev, J. Phys. Chem. Ref. Data 44, 033103 (2015), arXiv:1506.01885, table "Ionization energy ... in Rydbergs",
// row LAMB (data/lamb, tools/make_lamb_data.py, lamb_data.inc). The unit there is h c R_inf; here eV (x kRydInf). Each Z is computed there for one isotope (lambIsotope); another isotope of the same Z
// differs only in the finite-size part, which is not redone here. No values for n >= 3 or Z > 110: there the shift is 0 and lambKnown is false (the app says so).
bool lambKnown(int Z, int n);
double lambShiftEV(int Z, int n, int l, int twoJ);        // 0 where lambKnown is false; also 0 for a (l, j) that does not exist
int lambIsotopeA(int Z);                                  // the mass number of the isotope the source used (0: none)
double lambRadiusFm(int Z);                               // the rms charge radius the source used, fm (NaN: none)
// The level the lab shows with the setting on (0.49.3): diracEV plus the Lamb shift of (n, l, j). For n >= 3 it IS diracEV.
double diracQedEV(int Z, int N, int n, int l, int twoJ);

// The electric-dipole rule for a one-electron atom: l changes by 1 and j by 0 or 1 (|2j - 2j'| <= 2); both (l, j) must exist (j = l +- 1/2, j > 0).
bool diracAllowed(int lLo, int jLo2, int lHi, int jHi2);

// One line of the spectrum with the Dirac levels: the jump from (hi, jHi2) down to (lo, jLo2). Lines of equal energy are ONE line: in Dirac theory 3s(1/2) -> 2p(1/2) and 3p(1/2) -> 2s(1/2) coincide, and
// lLo, lHi name the first pair (the smallest l) that gives it. hi = 0 is a series limit of the level (lo, jLo2): lHi = -1, jHi2 = 0. Wavelengths in nm, vacuum, like specLines.
struct DiracLine { int lo, hi, lLo, lHi, jLo2, jHi2; double lam; };
// Same set of lines as specLines (lower level 1 to 5, upper level up to 20 above it, a limit for each lower level), but split by j: each (lo, hi) group is sorted by wavelength, a limit group is sorted
// by j (the deepest level, the shortest wavelength, first). Every line is a jump the rule above allows.
std::vector<DiracLine> diracLines(int Z, int N);

// What the spectrum card draws: specLines (Off) or the same list from the Dirac lines (On; the DiracLine's lo, hi and lam). With the setting off these ARE specLines and visibleLines, bit for bit.
std::vector<SpecLine> specLinesFor(int Z, int N, bool dirac);
std::vector<SpecLine> visibleLinesFor(int Z, int N, bool dirac);

// The levels in words, one row each, for n = 1 to 3 (what the card shows under the strip): energy of every (n, j) and the fine-structure gap of each n.
std::vector<std::string> diracLevelRows(int Z, int N);

// The words of the setting (shown in the panel, checked by the tests).
extern const char* const kDiracLabel;           // the label of the setting
std::string diracExplain();                     // one line: what it changes and what it does not
std::string diracOffNote();                     // under the setting while it is off
std::string diracOnNote(int Z);                 // under the setting while it is on: what is Dirac, what is not, why NIST still differs

}  // namespace atom
