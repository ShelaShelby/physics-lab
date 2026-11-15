// Molecule Lab (0.53): rotation of the linear molecules. No graphics in here (molecule_view.cpp turns the drawn atoms, ui/shell_molecule.cpp has the panel and the
// card), so tests/test_rotation.cpp checks it without a window.
//
// The physics (every constant from a saved page; the sources are named next to each value in rotation.cpp):
//   - Levels. Diatomics H2, N2, O2, HCl: F_v(J) = B_v J(J+1) - D_v [J(J+1)]^2, B_v = Be - alpha_e (v + 1/2), D_v = D, with Be, alpha_e and D from Huber & Herzberg (1979)
//     via the NIST Chemistry WebBook (data/molecules/vib/webbook_diatomic/). O2's table gives B0 and D0 themselves (in brackets), so O2's B_v = B0 - alpha_e v.
//     HCl (0.54.1) also has the table's higher terms (footnotes 29 and 30): B_v = Be - alpha_e x + 0.0017724 x^2 - 0.0001201 x^3 and D_v = De - 7.510E-6 x + 4.00E-7 x^2,
//     x = v + 1/2 (the x^2 term of B_v is what the table calls gamma_e; D_v's x term its beta_e). HCN, CO2, C2H2: the
//     measured ground-state B the lab already has (NIST CCCBDB), a rigid rotor (no centrifugal term). The eight non-linear molecules: not shown yet.
//   - Populations at temperature T: N_J / N = g(J) (2J+1) exp(-hc F(J) / kT) / Q, Q the sum over J until a term is below 1e-12 of the sum. The one-term centrifugal
//     formula stops rising once (J+1)^2 passes B / 2D (H2: after J = 25; HCl: J = 99); the sum stops there too, and `Partition::cut` says when the levels above would still hold a millionth or more (H2 above about 1,800 K).
//   - Nuclear-spin weights g(J): worked out from the nuclear spins (NUBASE2020, data/nubase) and the symmetry of the ground electronic state, by the rule for two
//     identical nuclei of spin I (G. Herzberg, Molecular Spectra and Molecular Structure I: Spectra of Diatomic Molecules, 2nd ed. (1950), chapter III, the influence
//     of nuclear spin; the book was not opened in this session): of the (2I+1)^2 spin states, (I+1)(2I+1) are symmetric and I(2I+1) antisymmetric under the exchange
//     of the two nuclei; the whole state must be symmetric for bosons (integer I) and antisymmetric for fermions (half-integer I); in a Sigma_g+ state the even-J levels
//     are symmetric, in O2's Sigma_g- state the odd-N ones. H2 even 1, odd 3; 14N2 6 and 3; 16O2 only odd N; 12C16O2 only even J; C2H2 1 and 3 (the H pair; 12C has
//     spin 0); HCl and HCN 1 (no exchange). Constant factors (the spins of unexchanged nuclei) are left out: they cancel in every share.
//   - O2's ground state is a triplet: N is the rotation quantum number, and each N level is split into three (J = N - 1, N, N + 1) by about 2 cm^-1 (not shown).
//   - Lines. Pure rotation J -> J + 1 (only a molecule with a permanent dipole: HCl, HCN): nu = F(J+1) - F(J) in v = 0. HCl's fundamental band v = 0 -> 1:
//     R(J) = nu0 + F_1(J+1) - F_0(J), P(J) = nu0 + F_1(J-1) - F_0(J), nu0 = G(1) - G(0) from the lab's measured ladder (vibration.h). A line's relative strength:
//     the lower level's share times the Honl-London factor over 2J+1 (R and pure rotation: (J+1)/(2J+1), P: J/(2J+1)), scaled so the strongest line of the shown
//     spectrum is 1. That is the transition's own strength, not the absorbance (which also has the factor nu (1 - exp(-hc nu / kT))); sticks, no line widths, no
//     Herman-Wallis correction (the strengths' change with m), no hot bands (v = 1 -> 2), no terms of the series beyond footnotes 29 and 30 (the page names more
//     in Rank, Rao et al. 1965, not saved). Against Rank et al. 1962's 31 measured H35Cl lines the worst gap is in REPORT 0.54.1 and tests/test_rotation.cpp.
//   - Isotopes of HCl: H35Cl measured; H37Cl, D35Cl, D37Cl scaled from it by the Born-Oppenheimer mass rules of Dunham's Y_kl ~ mu^-(k/2 + l): we ~ mu^-1/2,
//     wexe ~ mu^-1, weye ~ mu^-3/2, weze ~ mu^-2, Be ~ mu^-1, alpha_e ~ mu^-3/2, De and gamma_e ~ mu^-2, B_v's x^3 term and beta_e ~ mu^-5/2, D_v's x^2 term
//     ~ mu^-3 (none of these measured for them here: H&H's HCl page lists H35Cl only; the DCl page's measured we, wexe, Be, alpha_e, gamma_e, De are used only by
//     the tests, to check the rules), with the reduced mass from NIST's atomic masses
//     (data/molecules/nist_compositions_*.html). The natural mix: H35Cl and H37Cl weighted by NIST's isotopic compositions of chlorine (0.7576, 0.2424); the
//     hydrogen is taken as 1H (natural deuterium, 0.0115 %, is left out).
//   - The picture: a classical rotor with the energy of level J turns at omega = sqrt(J(J+1)) hbar / I (I = h / (8 pi^2 c B)), slowed down kSlowDown times. A quantum
//     rotor in level J has no turning picture (its orientation is spread over every direction); the lab says so.
// Constants: CODATA 2022 (h, c, k, u) as in atom_core.h.
#pragma once

#include <string>
#include <vector>

#include "molecule/molecule.h"

namespace molecule::rot {

// HCl's isotope choice (the other molecules have one)
enum Iso { kH35Cl = 0, kH37Cl, kD35Cl, kD37Cl, kNatural, kIsoCount };
const char* isoName(int iso);                      // "H³⁵Cl" ... "Natural mix"
const char* isoShort(int iso);                     // for the segmented control: "H³⁵Cl", "H³⁷Cl", "D³⁵Cl", "D³⁷Cl", "Mix"

// A diatomic's measured rotation constants (cm^-1), as the saved page lists them
struct Measured {
    const char* id;
    double Be;         // Be, or B0 where the table gives only B0 (b0 true)
    double alphaE;
    double De;         // De (N2 and O2: D0, in brackets in the table)
    bool b0;           // Be holds B0 (O2)
    const char* note;  // what the table says about the values
    // HCl only (0.54.1, Huber & Herzberg's footnotes 29 and 30); 0 for the others: B_v = Be - alphaE x + bv2 x^2 + bv3 x^3, D_v = De + dv1 x + dv2 x^2, x = v + 1/2
    double bv2 = 0, bv3 = 0, dv1 = 0, dv2 = 0;
};
const Measured* measured(const std::string& id);   // H2, N2, O2, HCl; nullptr otherwise

// The nuclear-spin weight rule: two identical nuclei of spin twoI / 2 exchanged by a rotation of 180 degrees; sigmaMinus: the electronic state is Sigma_g- (the
// even levels are then antisymmetric). Gives the weights of the even and the odd levels.
void spinWeights(int twoI, bool sigmaMinus, int& gEven, int& gOdd);

struct Rotor {
    std::string id;            // the molecule
    int iso = 0;               // the isotope (HCl only; else 0)
    std::string label;         // "H³⁵Cl", "H₂"
    double B[2] = {0, 0};      // B_v for v = 0 and v = 1 (cm^-1); B[1] = 0: no v = 1 constants
    double D[2] = {0, 0};      // the centrifugal constant D_v for v = 0 and v = 1 (cm^-1); 0: rigid (only HCl's changes with v)
    double nu0 = 0;            // the band origin G(1) - G(0), cm^-1 (0: not shown)
    int gEven = 1, gOdd = 1;   // nuclear-spin weights
    bool nLevels = false;      // O2: the levels are N, each split in three (not shown)
    bool dipole = false;       // a permanent dipole: pure-rotation lines
    int sigma = 1;             // symmetry number (2 for a molecule with a centre of symmetry)
    double muU = 0;            // diatomic reduced mass (u); 0 for the polyatomics
    double abundance = 1;      // in the natural mix
    bool scaled = false;       // constants scaled from H35Cl, not measured
    const char* source = "";   // where B comes from
};
bool hasRotation(const Molecule& m);               // the linear ones (H2, N2, O2, HCl, CO2, HCN, C2H2)
Rotor rotor(const std::string& id, int iso = kH35Cl);   // iso kNatural gives H35Cl (the natural mix's main part)
double term(const Rotor& r, int v, int J);         // F_v(J), cm^-1
double termG(const Rotor& r, int v);               // HCl's G(v) for this isotope (scaled), cm^-1, from vib::anharmonic
int weight(const Rotor& r, int J);                 // g(J)
int risingTop(const Rotor& r, int v = 0);          // the highest J at which F_v(J) still rises (rigid: a large number)
struct Partition { double Q = 0; int top = 0; bool cut = false; double lastRel = 0; };   // top: the last J summed; lastRel: its term over the sum when the sum stopped at risingTop (else 0); cut: lastRel >= 1e-6 (the levels beyond would still show)
Partition partition(const Rotor& r, double T);     // T in K, energies from F(0) = 0 (the textbook's); T <= 0: Q = 0 (only the lowest allowed level is held)
double share(const Rotor& r, int J, double T);     // N_J / N
int lowestJ(const Rotor& r);                       // the lowest allowed level (O2: N = 1)
int mostPopulated(const Rotor& r, double T);

struct Line {
    char branch = 'J';         // 'R', 'P' (the fundamental band) or 'J' (pure rotation)
    int J = 0;                 // the lower level's J
    int m = 0;                 // R: J + 1, P: -J, pure rotation: J + 1
    int vLo = 0, vHi = 0, jLo = 0, jHi = 0;
    double nu = 0;             // cm^-1
    double strength = 0;       // relative (the strongest line of the list is 1)
    int iso = 0;               // which isotope (natural mix: both chlorines)
    std::string name() const;  // "R(3)", "P(2)", "J 2 → 3"
};
enum class Spectrum { PureRotation = 0, Fundamental = 1 };
bool hasSpectrum(const Rotor& r, Spectrum s);      // pure rotation: a dipole; fundamental: HCl only
// The lines of one molecule (iso: kNatural gives both chlorines, each weighted by its abundance), sorted by wavenumber; lines below minRel of the strongest are left out
std::vector<Line> lines(const std::string& id, int iso, Spectrum s, double T, double minRel = 1e-3);

// H35Cl's measured fundamental lines (Rank, Eastman, Rao & Wiggins, J. Opt. Soc. Am. 52, 1 (1962), Table III, observed): R(0..15), P(1..15)
struct MeasuredLine { char branch; int J; double nu; };
std::vector<MeasuredLine> measuredHClLines();
double measuredHClLine(char branch, int J);        // NaN: not in the table

// The classical turning picture
double inertiaKgM2(const Rotor& r);                // I = h / (8 pi^2 c B0)
double omegaRadS(const Rotor& r, int J);           // sqrt(J(J+1)) hbar / I
constexpr double kSlowDown = 5e12;                 // the picture's slow-down (the vibrations use their own, 2 s for a 1000 cm^-1 swing)
double screenTurnS(const Rotor& r, int J);         // seconds per turn on screen (0: J = 0, no turning)

// The masses and abundances used (NIST "Atomic Weights and Isotopic Compositions", saved pages)
double massU(int z, int a);                        // 1H, 2H, 35Cl, 37Cl; NaN for others
double abundance(int z, int a);                    // the isotopic composition (2H: 0.000115)

}  // namespace molecule::rot
