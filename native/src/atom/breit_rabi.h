#pragma once
// 0.93.0: the ground state's hyperfine levels in a magnetic field (the Breit-Rabi formula). No drawing here, so it can be tested (tests/test_breit_rabi.inc).
//
// The model (the app says what it leaves out: limitsText):
//  - One level with J = 1/2 (an alkali's or hydrogen's ground state ns 2S1/2) and a nucleus of spin I, in a uniform field B along z:
//    H = A_hfs I.J + mu_B (g_J J_z + g_I I_z) B (Steck, "Sodium D Line Data" 2.3.4, eq. 15 and 20; the same in his Rb and Cs notes). A_hfs > 0 for all five
//    atoms here. g_I is in Steck's sign convention: g_I = -mu_I / (mu_B I), negative for a positive nuclear moment.
//  - The energies by the Breit-Rabi formula (Steck eq. 26 to 28), and, as the independent check, by diagonalising H in the |m_J m_I> basis (each m_F block
//    exactly, and the whole matrix by Jacobi rotations). The low-field g_F is Steck eq. 23, the clock transition's small-field shift Steck eq. 29.
//  - Left out: the mixing with excited levels, the diamagnetic term (second order in B), and the nuclear quadrupole (zero for J = 1/2).
#include <string>
#include <vector>

namespace atom::breitrabi {

struct GroundState {
    int Z = 1, N = 0;
    const char* name = "";       // "hydrogen-1 1s 2S1/2"
    int twoI = 1;                // 2 I
    double aHz = 0;              // A_hfs / h, Hz
    double gJ = 2, gI = 0;       // Steck's convention (g_I negative for a positive moment)
    const char* source = "";     // where the numbers come from, in plain words
};
const std::vector<GroundState>& groundStates();   // H-1 1s, Na-23 3s, Rb-85 5s, Rb-87 5s, Cs-133 6s
const GroundState* find(int Z, int N);           // nullptr: not one of them

double muBHzPerT();                              // mu_B / h, Hz/T (CODATA 2022 mu_B and the exact h, atom_core.h)
double gJHydrogen();                             // g_J of hydrogen's 1s: |g_D| + (|g_e| - 2)(1 + alpha^2 / 6) (CODATA 2022 eq. 114, 116, 117)
double splittingHz(const GroundState& g);        // Delta E_hfs / h = A_hfs (I + 1/2)
double xOf(const GroundState& g, double B);      // x = (g_J - g_I) mu_B B / Delta E_hfs (Steck eq. 27)
double bOfX(const GroundState& g, double x);     // the field (T) where x has this value

// one sublevel: F (twice) and m_F (twice) by its low-field name; F = I + 1/2 or I - 1/2. Energies E / h in Hz, zero at the centre of gravity at B = 0.
struct Sub { int twoF = 0, twoM = 0; double eHz = 0; };
// the Breit-Rabi formula (Steck eq. 26, eq. 28 for m_F = +-(I + 1/2)); twoF must be 2I + 1 or 2I - 1, |twoM| <= twoF
double formulaHz(const GroundState& g, int twoF, int twoM, double B);
// every sublevel (2 (2I + 1) of them) by the formula, by F (upper first) then m_F
std::vector<Sub> formula(const GroundState& g, double B);
// every sublevel by diagonalising H in the |m_J m_I> basis: the same order as formula(). Labels: m_F = m_I + m_J is kept by H; inside one m_F the upper root
// is F = I + 1/2 (A_hfs > 0: the two never cross)
std::vector<Sub> diagonalise(const GroundState& g, double B);
std::vector<double> fullMatrix(const GroundState& g, double B);   // every eigenvalue of the whole matrix (Jacobi rotations), lowest first, Hz
double gF(const GroundState& g, int twoF);       // the low-field g_F (Steck eq. 23)
double clockShiftHzPerT2(const GroundState& g);  // the m_F = 0 to m_F = 0 transition's shift over B^2 (Steck eq. 29), Hz/T^2

std::string limitsText();                        // the limits in plain words
}  // namespace atom::breitrabi
