#pragma once
// 0.54.0: atoms in a magnetic field: the Zeeman effect from the weak field (Lande g) through the region between to the Paschen-Back limit, and the split lines
// with their polarisations. No drawing here, so it can be tested (tests/test_zeeman.inc, ctest atom_physics).
//
// The model (the app says what it leaves out: limitsText):
//  - Inside ONE LS term (L and S fixed) with all its J levels at their measured energies E_J (NIST ASD; the one-electron atom: the lab's Dirac levels with the
//    Lamb shift, dirac.h), the field B along z adds mu_B B (g_L L_z + g_s S_z), g_s = |g_e| (CODATA 2022, atom_core.h kGe). In the |J M> basis of the term this is
//    H = E_J delta_JJ' + mu_B B [g_L M delta_JJ' + (g_s - g_L) <J M| S_z |J' M>], since g_L L_z + g_s S_z = g_L J_z + (g_s - g_L) S_z (0.96.0: g_L = 1 - m_e/m_nuc, the
//    nucleus's motion, Steck / Bethe & Salpeter; until 0.95.0 g_L = 1). M is kept, so H splits into one small block
//    per M, diagonalised exactly (no switch between weak- and strong-field formulas). <J M| S_z |J' M> = sum over M_S of <L M-M_S S M_S|J M> <L M-M_S S M_S|J' M> M_S,
//    with the exact Clebsch-Gordan coefficients of threej.h. Textbooks: H. A. Bethe & E. E. Salpeter, "Quantum Mechanics of One- and Two-Electron Atoms" (1957),
//    the Zeeman-effect sections (weak, strong and intermediate fields); E. U. Condon & G. H. Shortley, "The Theory of Atomic Spectra" (1935), the Zeeman-effect chapter.
//  - Weak field: E = E_J + g_J mu_B B M with Lande's g_J = 1 + (g_s - 1) [J(J+1) + S(S+1) - L(L+1)] / (2 J(J+1)) (Bethe & Salpeter, with g_s in place of 2);
//    strong field: E -> mu_B B (M_L + g_s M_S) plus the fine structure as a small shift (Paschen-Back).
//  - A term whose levels the lab does not all have (a J missing), or with one J only, gets no matrix: each level splits linearly with its g, NIST's measured one
//    where ASD lists it (data/jumps/asd_g, lande_data.inc), else the LS value; the text says which. A level whose NIST term is not an LS term (jK coupling,
//    "2[3/2]") gets no calculated splitting.
//  - Lines (E1): every component M_u -> M_l with q = M_u - M_l = 0 (pi), +1 (sigma+), -1 (sigma-). Its strength is the squared dipole matrix element between
//    the two field states. For a line inside one LS multiplet (both levels in LS terms of the same S) the element is built from the orbital part,
//    <L_u M_Lu| d_q |L_l M_Ll> = (-1)^(L_u - M_Lu) (L_u 1 L_l; -M_Lu q M_Ll) <L_u||d||L_l> (the Wigner-Eckart theorem, Edmonds chapter 5), with the spin a spectator; the
//    reduced element cancels in the relative strengths. Otherwise (an intercombination line, or a term without a matrix) the line's own J pair is used:
//    <J_u M_u| d_q |J_l M_l> = (-1)^(J_u - M_u) (J_u 1 J_l; -M_u q M_l) <J_u||d||J_l>. In weak fields both give the 3j-squared pattern.
//  - Seen at the angle theta to the field: pi ~ sin^2 theta, sigma ~ (1 + cos^2 theta) / 2 (the dipole patterns; across: pi and sigma, linearly polarised;
//    along: sigma only, circular). Strengths are relative to the line's field-free total.
// Left out (said in the app): mixing with other terms (one LS term only), the diamagnetic B^2 term, hyperfine structure, the nucleus's own moment.
#include <string>
#include <vector>

namespace atom::zeeman {

double lorentzCmPerT();                 // the Lorentz unit mu_B B / (h c) per tesla, cm^-1/T, from CODATA 2022 mu_B, h and c (atom_core.h)
double gsCodata();                      // |g_e| = 2 (1 + a_e), CODATA 2022
// Lande g_J of an LS level (J2 = 2J > 0; J = 0: 0), with the orbital g-factor gL (0.96.0):
//   g_J = gL [J(J+1) - S(S+1) + L(L+1)] / (2J(J+1)) + gs [J(J+1) + S(S+1) - L(L+1)] / (2J(J+1))
// (D. A. Steck, "Sodium D Line Data", rev. 2.3.4 (2025), Eq. (21), citing Bethe & Salpeter 1957; extract native/data/jumps/steck/steck_sodium_d_line_2.3.4.extract.txt).
// gL = 1 is the form used until 0.95.0, 1 + (gs - 1) [J(J+1) + S(S+1) - L(L+1)] / (2J(J+1)).
double landeLS(int L, int S2, int J2, double gs, double gL = 1.0);
// 0.96.0: the electron's orbital g-factor with the nucleus's motion, gL = 1 - m_e / m_nuc (Steck Eq. (19), "correct to lowest order in m_e/m_nuc"), m_nuc of the nucleus
// with Z protons and N neutrons (atom_core.h nuclearMassMe: AME2020 binding). Hydrogen: 1 - 5.4e-4; sodium-23: 0.99997613 (Steck's Table 6, tests/test_zeeman.inc).
double orbitalG(int Z, int N);

enum class Mode { Matrix, Linear, None };
struct Term {
    int L = 0, S2 = 0;                  // S2 = 2S
    double gL = 1;                      // 0.96.0: the orbital g-factor, 1 - m_e / m_nuc of the lab's isotope (orbitalG); the matrix is mu_B B (gL L_z + gs S_z)
    std::vector<int> J2;                // the term's J levels in the lab, lowest J first
    std::vector<double> cm;             // their field-free energies, cm^-1
    std::vector<double> gLin;           // Linear mode: the g each level splits with
    std::vector<bool> gMeasured;        // ... NIST's measured one (true) or the LS value
    Mode mode = Mode::None;
    std::string name;                   // "3s3p ³P°", "3p ²P°"
    std::string why;                    // Linear / None: one sentence why there is no matrix
};
// The terms from the lab's data. measured g looked up in lande_data.inc.
Term termOneElectron(int Z, int N, int n, int l);       // the lab's Dirac levels with the Lamb shift (dirac.h diracQedEV)
Term termAlkali(int Z, int n, int l);                   // NIST levels (alkali.h)
Term termLevel(int Z, int level);                       // the term of a level atom's level (levels.h), all its J levels from NIST
bool parseLS(const std::string& asdTerm, int& L, int& S2);   // "3P*" -> L 1, S2 2; false for a term that is not LS ("2[3/2]*")
double gapCm(const Term& t);                            // the smallest fine-structure gap between neighbouring J levels (NaN: no matrix or one level)

// measured Lande g from NIST ASD (NaN: none). Alkali levels by n, l, J; level atoms by ASD's configuration, term and J text
double measuredGAlkali(int Z, int n, int l, int J2);
double measuredGLevel(int Z, const std::string& conf, const std::string& term, int J2);
std::string measuredGRef(int Z, int n, int l, int J2, const std::string& conf, const std::string& term);   // ASD's reference code ("L5770"; "" none)

struct Sub {
    int J2 = 0, M2 = 0;                 // the level it joins at zero field, and 2M
    double cm = 0;                      // energy, cm^-1
    double rel = 0, mean = 0;           // the same as rel + mean: mean = the term's mean level energy, rel the rest (kept apart so small shifts keep their digits)
    std::vector<double> c;              // amplitudes on the term's J levels (Term::J2 order)
    std::vector<double> u;              // amplitudes on the uncoupled |M_L, M_S> states of this M (M_S = -S..S, index (M_S2 + S2) / 2)
};
std::vector<Sub> sublevels(const Term& t, double B, double gs);   // every M sublevel, by J then M

struct Line {
    Term up, lo;
    int upJ2 = 0, loJ2 = 0;             // the line's own J levels
    double cm0 = 0;                     // its field-free wavenumber, cm^-1 (NIST's line; one-electron: the lab's Dirac levels)
    double nm0 = 0;                     // its vacuum wavelength, nm
    std::string name;
};
bool lsRoute(const Line& L);            // strengths from the LS multiplet's orbital element (true) or the line's J pair (false)
struct Component {
    int M2u = 0, M2l = 0, q = 0;        // q = M_u - M_l: 0 pi, +1 sigma+, -1 sigma-
    double shiftCm = 0;                 // from the field-free line, cm^-1
    double strength = 0;                // relative to the line's field-free total (the components sum to 1 in weak fields)
    double raw = 0;                     // |<u| d_q |l>|^2 in units of the reduced element squared
};
std::vector<Component> components(const Line& L, double B, double gs);   // by shift
double fieldFreeRaw(const Line& L);     // the line's total raw strength at B = 0
// every pair of states labelled by the line's two J levels, with raw strengths only (also for a J pair with no line at zero field, such as 2D5/2 - 2P1/2:
// in a strong field such pairs get strength from the mixing; the app's lines leave them out and say so)
std::vector<Component> rawComponents(const Line& L, double B, double gs);
double polWeight(int q, double thetaRad);   // pi: sin^2 theta; sigma: (1 + cos^2 theta) / 2
double seenStrength(const Component& c, bool along);   // the strength seen across (theta 90 deg) or along (0 deg) the field
double shiftNm(const Line& L, double shiftCm);          // the component's wavelength minus the line's, nm (vacuum)

// The lines the lab offers for an atom: kind 0 the one-electron atom (n_u <= 5), 1 an alkali, 2 a level atom. The default: the strongest line of the level now.
struct LineRef { Line line; int a = 0, b = 0; };   // a, b: the lab's own indices (kind 2: upper and lower level; kind 1: index into alkaliLines; kind 0: n_u, n_l)
const std::vector<LineRef>& labLines(int Z, int N, int kind);
int defaultLine(int Z, int N, int kind, int levelNow, int nNow, int lNow);   // index into labLines

enum class Regime { NoFine, Weak, Between, Strong };
Regime regime(const Line& L, double B, double& muBcm, double& gap);   // compares mu_B B / hc with the smaller fine-structure gap of the two terms (weak < 0.1 x, strong > 10 x: the app's rule)
std::string regimeText(const Line& L, double B);   // one sentence with both numbers
std::string usedText(const Line& L);               // which model each level used (matrix, measured g, LS g) and why
std::string limitsText(int Z, int N, int kind);    // the limits in plain words, with the lab's hyperfine number where it has one
double hyperfineFieldT(int Z, int N, int kind);    // the field where mu_B B equals the ground level's hyperfine splitting in the lab (NaN: the lab has none)
std::string mText(int M2);                          // "3/2", "−1/2", "0"
std::string polName(int q);                         // "π", "σ+", "σ−"

}  // namespace atom::zeeman
