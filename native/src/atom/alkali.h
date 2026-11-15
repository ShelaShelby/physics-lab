#pragma once
// 0.46 (B3): photon jumps of the atoms with ONE valence electron (Li, Na, K, Rb, Cs) with every electron shown.
//
// The model (and what it is not):
//  - The outer electron jumps between orbitals; the core (every other electron) is kept FIXED: each valence orbital is an eigenfunction of the closed-shell ion's Dirac-Fock operator
//    (frozen-core, V^(N-1) Dirac-Hartree-Fock: tools/dhf/valence.py), the starting point of M. S. Safronova, W. R. Johnson and A. Derevianko, PRA 60, 4476 (1999), whose DHF removal
//    energies of K 4s, Rb 5s and Cs 6s (Table I) it reproduces to 1 cm-1 (data/dhf/valence_summary.tsv). No core relaxation, no electron correlation (the DHF energies are 3-12 % too small).
//  - What is drawn: the neutral atom as before (relaxed ground-state Dirac-Hartree-Fock, hf.h), with its outer s electron replaced by the frozen-core orbital of the excited level.
//  - Energies and wavelengths are only MEASURED values: NIST ASD levels and lines (data/asd/alkali, tools/make_alkali_data.py). Jumps only along electric-dipole lines that ASD lists
//    (l changes by 1); a line's transition probability Aki is ASD's where it gives one.
#include <string>
#include <vector>

#include "atom/hf.h"

namespace atom {

struct AlkLevel { int n, l, j2; double cm, unc; std::string cmText; };                 // a NIST level of the valence electron in n l (one J), cm-1 above the ground level
struct AlkLine { int nLo, lLo, j2Lo, nHi, lHi, j2Hi; double obsNm, ritzNm, aki; std::string acc; double nm() const; };   // vacuum nm (observed where ASD gives it, else Ritz); aki NaN: none
bool alkaliJumps(int Z);                                     // Li, Na, K, Rb, Cs, Fr (0.47.2)
std::vector<AlkLevel> alkaliLevels(int Z);
std::vector<AlkLine> alkaliLines(int Z);
struct AlkOrbital { int n = 0, l = 0; };
std::vector<AlkOrbital> alkaliOrbitals(int Z);               // the valence orbitals that have levels, ordered by energy (the ground ns first)
double alkaliTermCm(int Z, int n, int l);                    // the (2J+1)-weighted mean of the term's NIST levels, cm-1 (NaN: none)
std::vector<AlkLine> alkaliLinesBetween(int Z, int n1, int l1, int n2, int l2);   // the fine-structure components of n1 l1 <-> n2 l2 (either order)
// One component of a jump from the term (nFrom, lFrom) to (nTo, lTo), chosen with odds g_upper x Aki (the relative intensities of a term whose J levels are filled in proportion to 2J + 1);
// equal odds per upper g where ASD gives no Aki for any component. u in [0, 1). False when there is no line. 0.47.2: j2From > 0: only the components whose level in (nFrom, lFrom)
// has that J (when there is one; else all): the electron is in one level of the term.
bool alkaliPickLine(int Z, int nFrom, int lFrom, int nTo, int lTo, double u, AlkLine& out, int j2From = 0);
const hf::Subshell* valenceOrbital(int Z, int n, int l, bool relativistic);   // frozen-core orbital (occupation 1, j parts statistical); nullptr when not computed
// The neutral atom with its valence electron in n l: the ground atom of hf::find with its outer s subshell replaced (nullptr when not available). The pointer is stable (cached), and the
// ground state returns hf::find's own atom.
const hf::Atom* alkaliAtom(int Z, int n, int l, bool relativistic);
// 0.47.2: relativistic, with GRASP clouds: the level (n, l, J) (j2 < 0: graspDefaultJ2), ground included; non-relativistic: as above (frozen core, J not used)
const hf::Atom* alkaliAtom(int Z, int n, int l, bool relativistic, int j2);
double valenceEnergyHartree(int Z, int n, int l, int j2, bool relativistic);   // frozen-core orbital energy (for tests and notes only, never shown as a measurement)
std::string alkaliModelNote();                               // one line for the readout (the non-relativistic model: frozen core)
std::string graspModelNote();                                // 0.47.2: one line for the readout (the GRASP2018 clouds)

// ---- 0.47.2: the GRASP2018 clouds (tools/grasp; PLAN_JUMPS.md part 1) ----
// Every level the app uses (and francium's) computed ahead with GRASP2018 (C. Froese Fischer, G. Gaigalas, P. Joensson, J. Bieron, Comput. Phys. Commun. 237, 184 (2019)):
// core-valence MCDHF (single and double excitations, at most one hole in the outer p shell of the core, 1s for Li; three layers of correlation orbitals s..g) and RCI with
// the Breit interaction and QED. What is drawn: (1) every electron, as the level's natural orbitals with their occupations (not whole numbers once configurations mix;
// they add up to the electron count); (2) the shape of the aligned state M_J = J: D(r, theta) = (1/4 pi) sum_k D_k(r) P_k(cos theta), k even, k <= 2J (D_0 = the natural
// orbitals' sum). A J = 1/2 level is round in every M_J state; any level averaged over M_J (a free atom) is round. The energies shown stay NIST's; the calculation's own
// excitation energy is the cloud's quality figure. Data: data/jumps/grasp/<X>/ (levels.tsv: energies and checks; clouds.bin: the packed orbitals).
struct GraspLevel {
    int n = 0, l = 0, j2 = 0;
    std::string conf, term;                                  // 0.47.3 (level atoms, levels.h): NIST's configuration and term of the level ("3s.3p", "3P*"); empty for the alkalis
    bool testOnly = false;                                   // a one-configuration level kept only for the tests (Na 3p 2P3/2 before correlation)
    double computedCm = 0, nistCm = 0;                       // the excitation energy from the ground level: GRASP's (cm-1) and NIST's
    hf::Atom atom;                                           // the natural orbitals (Subshell::focc, lumped)
    double r0 = 0, h = 0;                                    // the grid of the multipoles: r = r0 exp(i h)
    std::vector<int> ks;                                     // 2, 4, ... (none for J = 1/2)
    std::vector<std::vector<float>> Dk;                      // D_k(r), electrons per bohr
    double qualityPercent() const;                           // (computed - NIST) / NIST x 100 (NaN for the ground level)
};
bool graspHas(int Z);                                        // the atom has GRASP clouds (Li, Na, K, Rb, Cs, Fr; 0.47.3: the level atoms of levels.h too)
const std::vector<GraspLevel>& graspAll(int Z);              // 0.47.3: every stored cloud of the atom
const GraspLevel* graspLevel(int Z, int n, int l, int j2);   // nullptr when not computed
const GraspLevel* graspTestLevel(int Z);
const GraspLevel* graspOfAtom(const hf::Atom* a);           // the level whose natural orbitals a is (nullptr: not a GRASP cloud)                     // the one-configuration test level (nullptr: none)
int graspDefaultJ2(int Z, int n, int l);                     // the J a placed electron takes: the highest J of the term that has a cloud (s: 1)
double graspDk(const GraspLevel& g, std::size_t i, double r);   // D_{ks[i]}(r)
// The aligned state's direction weight at radius r: w = 1 + sum_k a_k(r) P_k(cos theta), a_k = D_k / D_0 (>= 0 for a physical density); bound >= max over theta of w.
double graspShape(const GraspLevel& g, double r, double cz, double& bound);
// A direction from the aligned state at radius r (cos theta uniform, kept with odds w / bound; azimuth uniform). The z axis is the alignment axis.
void graspDirection(const GraspLevel& g, double r, orbit::Mulberry32& rng, double& ux, double& uy, double& uz);
// One dot of the whole atom in the aligned state (exact): a natural orbital by its occupation, its radius, graspDirection. shellOut: the subshell's index in g.atom.sub.
void graspSample(const GraspLevel& g, orbit::Mulberry32& rng, double& x, double& y, double& z, int& shellOut);
double partDensity(const hf::Part& p, int l, double r);     // P^2 + Q^2 of one stored part (not normalised: the tests check the norm)
}  // namespace atom
