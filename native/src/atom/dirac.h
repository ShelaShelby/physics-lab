// Relativity as an option in the Atom Lab (0.10.8 stage 1): the energy levels of a one-electron atom from the Dirac equation for a POINT nucleus, so a level depends on n and on j (the total angular
// momentum), not on n alone, and the lines of the spectrum split. No drawing here, so it can be tested. The setting is off by default (AtomParams::dirac); nothing in the lab changes while it is off.
//
// What this is and is not (the app says the same, in plain words):
//   - It changes the ENERGIES and LINES of the two spectrum cards. The clouds, orbitals, outlines and the radial graph are still the non-relativistic wave functions; the readouts, the photon of a jump and
//     the quantum beats keep the non-relativistic level (the lab's orbitals have no spin and so no j to give them).
//   - No Lamb shift, no other QED effect and no finite nuclear size: in Dirac theory 2s(1/2) and 2p(1/2) have exactly the same energy, in reality they do not. The measured NIST lines therefore still differ from
//     these a little (tested: NIST lies between the Dirac line and the non-relativistic one for Lyman-alpha of every ion).
//   - The nucleus's motion: see diracCorrectionEV below. The main energy has the reduced mass, as always in the lab; the relativistic correction is that of an infinitely heavy nucleus. Recoil terms beyond the
//     reduced mass are not included (no source for them could be read).
#pragma once

#include <string>
#include <vector>

#include "atom_core.h"

namespace atom {

// The exact Dirac energy of a one-electron atom, point nucleus of infinite mass, level (n, j = twoJ / 2), WITHOUT the electron's rest energy (so a bound level is negative), eV:
//   E = m c^2 [ (1 + (Z alpha)^2 / (n - delta)^2)^(-1/2) - 1 ],   delta = (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2).
// The Sommerfeld fine-structure formula, the exact solution of the Dirac equation for a Coulomb potential (standard textbook material: Bethe & Salpeter, Quantum Mechanics of One- and Two-Electron Atoms;
// Griffiths ch. 6 gives its expansion). Written from knowledge, NOT looked up in the session that wrote it (only physics.nist.gov was reachable): the tests check it against hand arithmetic, its expansion and the
// measured NIST lines. Constants: alpha and m c^2 from CODATA 2022 (atom_core.h). Written with expm1 / log1p and delta = (Z alpha)^2 / ((j + 1/2) + sqrt(...)), the same function, so the small relativistic
// correction keeps its digits. Valid for Z alpha < j + 1/2, i.e. every Z up to 118 (Z alpha = 0.86).
double diracInfiniteEV(int Z, int n, int twoJ);
// The relativistic correction alone: the Dirac energy minus the non-relativistic one with the SAME constants, -(1/2) m c^2 (Z alpha)^2 / n^2. It is of the order (Z alpha)^2 of the level, negative (the level lies deeper).
double diracCorrectionEV(int Z, int n, int twoJ);
// The level the lab shows with the setting on: the lab's own non-relativistic level, with the nucleus's real mass (levelEV, the reduced-mass Rydberg energy), plus the correction above. In words: the main
// energy has the reduced mass, as it always had; the relativistic correction is worked out for an infinitely heavy nucleus. Putting the reduced mass into the whole Dirac formula instead would scale the correction
// by mu / m as well; the two differ by the correction times (1 - mu / m): 9e-9 of the Lyman-alpha energy for hydrogen (measured by the tests). Neither is the full treatment of recoil, which has more terms
// (that literature was not available here), so the level is not claimed exact at that size (of the order 1e-8 of a hydrogen line), which is far below what the Lamb shift and the rest of QED leave (3.3e-6 of the hydrogen Lyman-alpha line, measured against NIST).
double diracEV(int Z, int N, int n, int twoJ);

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
