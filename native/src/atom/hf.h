#pragma once
// MANY-ELECTRON atoms in the Atom Lab (0.39: Hartree-Fock; 0.41: Dirac-Hartree-Fock, every element H to Og).
//
// The model (and what it is not):
//  - 0.41: each electron is in a relativistic orbital (a 4-component Dirac spinor) of the Dirac-Hartree-Fock mean field with the Dirac-Coulomb Hamiltonian, computed for this app by
//    tools/dhf (radial, average-of-configuration Dirac-Fock with a finite Gaussian nucleus; data/dhf/README.txt). Configurations: NIST ASD 5.12 ground shells (H to Hs) and
//    Smits et al., Phys. Rep. 1035 (2023), Table 2 (Mt to Og). In the open shells a small configuration interaction finds the ground level J; the orbitals are then made for that
//    level's mix of relativistic configurations (e.g. thallium's 6p electron is all 6p1/2, lead's two 93 % 6p1/2), and the density is its average over M_J, which is spherical.
//  - Checked against published Dirac-Fock tables (L. Visscher and K. G. Dyall, At. Data Nucl. Data Tables 67, 207 (1997)): every orbital's <r> for Z = 1..109 (tools/dhf, vis mode: within 1.5e-5, except actinium's test configuration 7s2 5f1: data/dhf/README.txt);
//    the tests repeat this for the closed-shell atoms from the app's own data.
//  - NOT included: electron correlation (the largest missing effect: the summed <r^2> of neon's electrons is 2.0 % larger with it, argon's 0.4 % smaller; M. Lesiuk and
//    B. Jeziorski, arXiv:2311.11357 (2023), Table IX), the Breit interaction and QED (much smaller: Breit about 4e-5 of those <r^2> (ibid.); QED makes gold's 6s 0.15 % larger: K. Sunaga, M. Salman and T. Saue, J. Chem. Phys. 157, 164101 (2022)). The app says so.
//  - The nonrelativistic limit of the same calculation (same configuration, same code, c -> infinity) is kept for comparison (Electrons All, Model: Non-relativistic).
//  - The whole atom's density is the spherically averaged sum over subshells (Unsold's theorem). A nonrelativistic subshell (n, l) here holds both j = l - 1/2 and l + 1/2: its radial
//    density is their occupation-weighted sum. One chosen orbital is drawn with that radial density and the real angular part of the one-electron lab.
//  - Energies: only MEASURED values (NIST ASD) are shown as fact; the orbital energies here are for notes and tests.

#include <string>
#include <vector>

#include "orbit/belts.h"

namespace atom::hf {

// One relativistic subshell (n, l, j): P = r R_large = sum dP_i r^(l+1) e^(-alpha_i r^2), Q = r R_small = sum dQ_i ((l+1+kappa)/r - 2 alpha_i r) r^(l+1) e^(-alpha_i r^2)
// (atomic units; alpha_i = amin ratio^i; Q already holds the 1/2c of kinetic balance). Nonrelativistic data have dQ = 0.
struct Part {
    int j2 = 0; double occ = 0, eps = 0, amin = 0, ratio = 0; std::vector<double> dP, dQ;
    // 0.47.2: or P and Q tabulated (GRASP2018 natural orbitals, alkali.h): on r = tabR0 exp(i tabH), i = 0 .. size - 1; 4-point Lagrange in ln r; below tabR0 P(tabR0) (r / tabR0)^(l+1)
    // (Q alike); 0 from the last point on. Used when tabP is not empty.
    std::vector<float> tabP, tabQ; double tabR0 = 0, tabH = 0;
};
struct Subshell {
    int n = 0, l = 0, occ = 0;                             // occ: electrons in the whole (n, l) subshell (an integer: the configuration's)
    double eps = 0;                                        // orbital energy (hartree), occupation-weighted over j (not shown as a measured value)
    std::vector<Part> parts;                               // j = l - 1/2 (if l > 0) and j = l + 1/2; a part's occ may be fractional (the ground level's mix)
    double focc = -1;                                      // 0.47.2: the electrons when not a whole number (a natural orbital's occupation; occ is then this rounded); < 0: occ
    bool lumped = false;                                   // 0.47.2: the weakly occupied natural orbitals of one l drawn together (alkali.h)
};
double electronsIn(const Subshell& s);                     // focc when set, else occ
double tabValue(const std::vector<float>& f, double r0, double h, int l, double r);   // 0.47.2: a tabulated P or Q at r (Part::tabP)
struct Atom {
    int Z = 0, charge = 0, A = 0;                          // A: the mass number of the nucleus used (its Gaussian charge radius)
    bool relativistic = true;
    int J2 = -1;                                           // 2J of the ground level from the configuration interaction (-1: not computed)
    std::string shells, level, source;                    // the ground shells and level as the source writes them, and the source ("NIST ASD 5.12", "Smits et al. 2023")
    double energy = 0;                                     // total energy (hartree) of the calculation
    std::vector<Subshell> sub;                             // occupied subshells, ordered by n then l
    int electrons() const;
};

// The atom (neutral only: charge 0), relativistic (Dirac-Hartree-Fock) or its nonrelativistic limit; nullptr for H (one electron: the one-electron lab is exact), Z > 118, a charge.
const Atom* find(int Z, int charge, bool relativistic = true);
bool hasNeutral(int Z);

double radialR(const Subshell& s, double r);              // sign of the larger j part's large component x sqrt(r^2 R^2 summed over the parts) / r: |R|^2 r^2 is the density below
double radialDensity(const Subshell& s, double r);        // D(r) = r^2 (|R_large|^2 + |R_small|^2), occupation-weighted over j, normalised: integral of D dr = 1; r in Bohr radii
double meanRadius(const Subshell& s);                      // <r> = integral of r D dr
double radialCdf(const Subshell& s, double r);             // integral from 0 to r of D
double totalRadial(const Atom& a, double r);               // sum of occ D: integrates to the number of electrons
double partMeanRadius(const Part& p, int l);              // <r> of one j part alone (normalised)

// Independent samples (Bohr radii): the radius by inverting the distribution function (a table of 4,001 values, cubic Hermite, within 1e-6 in probability).
double sampleRadius(const Subshell& s, orbit::Mulberry32& r);
// One dot of the whole atom: a subshell with probability occ / electrons, its radius, a uniform direction. shellOut = the subshell's index in a.sub.
void sampleAtom(const Atom& a, orbit::Mulberry32& r, double& x, double& y, double& z, int& shellOut);
int subshellIndex(const Atom& a, int n, int l);            // -1 when the atom has no such orbital

// The Slice view: dots ON the plane at distance `off` (Bohr radii) from the nucleus. A spherical density f(r) gives a ring of the plane (polar rho, phi; r^2 = rho^2 + off^2,
// rho drho = r dr) the share f(r) 2 pi r dr, so on the plane a subshell's radius r has the density D(r) / r from |off| on, and its mass there is occ / 2 * integral_|off|^inf D / r dr
// (with the angle average 1 / 4 pi of a filled subshell). planeTail is that integral; planeOf works out every subshell's mass on the plane once; samplePlaneRadius draws r from the
// plane's own table; samplePlane picks a subshell by its mass on the plane (one < 0) or takes `one`.
double planeTail(const Subshell& s, double off);
struct Plane { double off = 0, total = 0; std::vector<double> mass, start; };   // mass: per subshell; start: the plane radius's distribution function at |off|
Plane planeOf(const Atom& a, double off);
double samplePlaneRadius(const Subshell& s, double start, orbit::Mulberry32& r);
bool samplePlane(const Atom& a, const Plane& pl, int one, orbit::Mulberry32& r, double& radius, int& shellOut);   // false: the plane holds (to double precision) none of it

// Text: "1s2 2s2 2p6 3s2 3p6 3d6 4s2" (ordered by n then l); "6p1/2 0.98 · 6p3/2 0.02" (a subshell's electrons by j); the letter of a shell ("K", "L", ...)
std::string configText(const Atom& a);
std::string jText(const Subshell& s);
const char* shellLetter(int n);

// The published nonrelativistic Hartree-Fock orbitals of 0.39 (Koga et al. 1999 and 2000, He..Lr), kept as an independent reference for the tests: <r> of an orbital in closed form,
// or a negative number when the files have none.
double kogaMeanRadius(int Z, int n, int l);

// NIST ASD ver. 5.12 for the neutral atom and the 1+ ion (H to Ds): the ground shells and level as NIST writes them, the ionization energy with NIST's marks:
// prefix "" = from evaluated experimental data (measured), "[" = semi-empirical, "(" = theoretical (NIST's own definitions, help page of ie.pl).
struct NistIe { int Z = 0, charge = 0; std::string sp, shells, level, prefix, ie, unc, refs; };
const NistIe* nistIe(int Z, int charge);
std::string nistKind(const NistIe& n);                    // "measured", "semi-empirical" or "theoretical"
std::string shellsText(const std::string& nistShells);    // "[Ar].3d6.4s2" -> "[Ar] 3d6 4s2"
std::string levelText(const std::string& nistLevel);      // NIST's "5D<4>" / "1S0" / "2P*<1/2>" -> the term symbol with a superscript multiplicity, a subscript J and ° for odd parity
double levelJ(const std::string& nistLevel);              // the J of NIST's level text (-1 if none)
bool configMatchesNist(const Atom& a, const NistIe& n);    // the atom's occupations equal NIST's ground shells

const unsigned char* embeddedDhf(std::size_t& size);       // data/dhf/dhf.bin, built into the program (cmake/embed_dhf.cmake)

}  // namespace atom::hf
