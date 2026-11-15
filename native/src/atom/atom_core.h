// Atom Lab physics core: one electron around any nucleus from hydrogen to oganesson.
// No drawing here.
//
// Units: everything stored is SI (metres, seconds, m/s, joules).
// constants (0.529 Å, 27.211 eV, 24.19 as). Since 0.10.6 stage 2 the lab uses the CODATA 2022 values below, so the picture, the readouts and the Learn guides share one set.
// numbers) and the nucleon placement (in fm while it is built). Both are turned into metres when stored.
#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <complex>
#include <string>
#include <utility>
#include <vector>

namespace atom {

// ---- constants: CODATA 2022 (NIST), each read from its page https://physics.nist.gov/cgi-bin/cuu/Value?<key> in the session of 2026-10-04; every page says
// "Source: 2022 CODATA recommended values". Derived constants are worked out from these, never typed in again. ----
constexpr double kPi = 3.14159265358979323846;
constexpr double kLn2 = 0.69314718055994530942;
constexpr double kC = 299792458.0;                    // speed of light, m/s, exact. page Value?c
constexpr double kEV = 1.602176634e-19;               // elementary charge = one electronvolt in J, exact. page Value?e (1.602 176 634e-19 C)
constexpr double kPlanckJs = 6.62607015e-34;          // Planck constant, J s, exact. page Value?h
constexpr double kPlanckEVs = kPlanckJs / kEV;        // h in eV·s = 4.135 667 696...e-15 (page Value?hev says the same)
constexpr double kHcEVnm = kPlanckJs * kC / kEV * 1e9;   // h·c in eV·nm = 1239.841 984 33, exact; worked out from h, c and e (NIST has no page for it)
constexpr double kA0 = 5.29177210544e-11;             // Bohr radius, m: 5.291 772 105 44(82)e-11 m. page Value?bohrrada0
constexpr double kA0Ang = kA0 * 1e10;                 // Bohr radius, Å (0.529 177 210 544)
constexpr double kHartreeEV = 27.211386245981;        // Hartree energy, eV: 27.211 386 245 981(30). page Value?hrev
constexpr double kAuTime = 2.4188843265864e-17;       // atomic unit of time, s: 2.418 884 326 5864(26)e-17 s. page Value?aut
constexpr double kAuTimeAs = kAuTime * 1e18;          // atomic unit of time, attoseconds (24.188 843 265 864)
constexpr double kAuSpeed = kA0 / kAuTime;            // atomic unit of speed, m/s (= alpha·c to 6e-12, checked in the tests)
constexpr double kAlpha = 7.2973525643e-3;            // fine-structure constant: 7.297 352 5643(11)e-3. page Value?alph
constexpr double kHartree = kHartreeEV * kEV;         // Hartree energy, J
constexpr double kRydInf = 13.605693122990;           // Rydberg energy h·c·R_inf for an infinitely heavy nucleus, eV: 13.605 693 122 990(15). page Value?rydhcev
constexpr double kMecEV = 510998.95069;               // electron mass energy m_e c², eV: 0.510 998 950 69(16) MeV. page Value?mec2mev
constexpr double kMeInU = 5.485799090441e-4;          // electron mass in u: 5.485 799 090 441(97)e-4. page Value?meu
constexpr double kMeU = 1.0 / kMeInU;                 // one atomic mass unit in electron masses (1822.888 486 28)
constexpr double kMpOverMe = 1836.152673426;          // proton-electron mass ratio: 1836.152 673 426(32). page Value?mpsme
constexpr double kUKg = 1.66053906892e-27;            // unified atomic mass unit, kg: 1.660 539 068 92(52)e-27. page Value?ukg
constexpr double kUMeV = 931.49410372;                // u c² in MeV: 931.494 103 72(29). page Value?muc2mev
constexpr double kMeKg = 9.1093837139e-31;            // electron mass, kg: 9.109 383 7139(28)e-31. page Value?me
constexpr double kMpKg = 1.67262192595e-27;           // proton mass, kg: 1.672 621 925 95(52)e-27. page Value?mp
constexpr double kMnKg = 1.67492750056e-27;           // neutron mass, kg: 1.674 927 500 56(85)e-27. page Value?mn
constexpr double kMpU = 1.0072764665789;              // proton mass in u: 1.007 276 466 5789(83). page Value?mpu
constexpr double kMnU = 1.00866491606;                // neutron mass in u: 1.008 664 916 06(40). page Value?mnu
// 0.54.0 (the Zeeman effect, atom/zeeman.h; the calculator's muB and ge read these, so there is one copy): CODATA 2022, Table XXXIII of Mohr et al. (arXiv:2409.03787,
// data/calc/codata2022_mohr2024_arxiv2409.03787.pdf, read with pdftotext -layout)
constexpr double kMuBJT = 9.2740100657e-24;           // Bohr magneton e hbar / 2 m_e, J/T: 9.274 010 0657(29)e-24
constexpr double kGe = -2.00231930436092;             // electron g-factor -2 (1 + a_e): -2.002 319 304 360 92(36) (negative: CODATA's sign)
constexpr double kBoltzmann = 1.380649e-23;           // J/K, exact. page Value?k
constexpr double kFm = 1e-15;                         // one femtometre, m (exact)
constexpr double kFmLab = kFm;                        // metres per fm: now just the exact value
constexpr double kFmPerA0 = kA0 / kFm;                // femtometres per Bohr radius (52 917.721 054 4)
// 0.42: the nucleon (sources saved in data/qcd/nucleon; About lists them)
constexpr double kProtonRmsFm = 0.84075;              // proton rms charge radius, fm: CODATA 2022 0.840 75(64) fm (page Value?rp); PDG 2024 average 0.8409(4) fm agrees. Older e-p scattering gave about 0.88 (PDG listing)
constexpr double kNucleonR = kProtonRmsFm;            // the drawn size of a nucleon (fm): the proton's charge radius (was 0.84, rounded, until 0.41)
constexpr double kFluxTubeRadiusFm = 0.38;            // 0.45: radius of the gluon flux tube between static quarks, lattice QCD: Bissey et al., PRD 76, 114512 (2007), 0.38 +- 0.03 fm (data/qcd/quarks/bissey_2007); the Simple style draws its tubes this wide
constexpr double kNeutronMsFm2 = -0.1155;             // neutron mean-square charge radius, fm^2: PDG 2024 -0.1155(17) (n-e scattering); chiral EFT from the deuteron: -0.106(+7,-5) (Filin et al., PRL 124, 082501 (2020))
constexpr double kHbarCMeVFm = 197.3269804;           // hbar c, MeV fm (CODATA 2022, exact to the digits given)
constexpr double kMpMeV = 938.27208943;               // proton mass, MeV/c^2: CODATA 2022 938.272 089 43(29). page Value?mpc2mev
constexpr double kMnMeV = 939.56542194;               // neutron mass, MeV/c^2: CODATA 2022 939.565 421 94(48). page Value?mnc2mev
// 0.52.1: the quark mass bar of the Quarks zoom names the nucleon shown (until 0.52.0 it said "proton" on a neutron too). Its orange part: the three valence
// quarks' own masses over the nucleon's, u 2.16 and d 4.70 MeV/c^2 (PDG 2024, Navas et al., Phys. Rev. D 110, 030001; MS-bar at 2 GeV, +- 0.07 each, as atom_learn.cpp)
constexpr double kUpQuarkMeV = 2.16, kDownQuarkMeV = 4.70;
inline double valenceQuarkShare(bool proton) { return proton ? (2 * kUpQuarkMeV + kDownQuarkMeV) / kMpMeV : (kUpQuarkMeV + 2 * kDownQuarkMeV) / kMnMeV; }   // 0.96 % (uud), 1.23 % (udd)
inline const char* nucleonMassBarText(bool proton) {
    return proton ? "mass of a proton: its three quarks' own masses are about 1%, the rest mostly strong-force energy"
                  : "mass of a neutron: its three quarks' own masses are about 1%, the rest mostly strong-force energy";
}
constexpr double kDarwinFoldyFm2 = 0.75 * (kHbarCMeVFm / kMpMeV) * (kHbarCMeVFm / kMpMeV);   // 3/(4 M^2), the Darwin-Foldy term of a charge radius: 0.0332 fm^2 (Ong, Berengut, Flambaum, PRC 82, 014320 (2010), eq. 8: 0.033)
constexpr double kMuP = 2.79284734463;                // proton magnetic moment, nuclear magnetons: CODATA 2022 2.792 847 344 63(82). page Value?mupsmun
constexpr double kMuN = -1.91304276;                  // neutron magnetic moment, nuclear magnetons: CODATA 2022 -1.913 042 76(45). page Value?munsmun
// The relativity line (0.10.6 stage 5). The lab is non-relativistic at every Z. From kRelativityZ on the text WARNS that the error is large (readout, hint and spectrum note all use this one number).
// Why 27: for a point nucleus the Dirac 1s energy mc²(1 − √(1 − (Zα)²)) is above the non-relativistic ½mc²(Zα)² by 0.917 % at Z = 26 and 0.990 % at Z = 27 (about 1 %); there (Zα)² = 0.0388
// (3.9 %) and the 1s electron moves at Zα = 20 % of the speed of light. Z = 20 is 0.54 % (Zα)² = 0.021. The percentages are the lab's own Dirac formula (the readout compares with it); no source was read.
constexpr int kRelativityZ = 27;
// Visible light: ONE range for the whole Atom Lab (the spectrum card, the line colours, the (UV) and (IR) tags, the strip under the picture). 380-780 nm is a CONVENTION this app uses: it is the range of the
// Inside it a line is drawn in its real colour, outside in a stand-in.
constexpr double kVisibleLoNm = 380, kVisibleHiNm = 780;
constexpr double kYearS = 365.25 * 86400;     // one year, s
constexpr double kTRoom = 293.15, kTMax = 6000;   // room temperature and the top of the temperature slider, K
constexpr int kCap = 12000;                   // cloud dots kept
constexpr double kExt[7] = {0, 4.5, 12, 24, 40, 62, 90};  // view radius per n, Bohr radii (a drawing choice; 0.46: n = 5, 6 added at about 2.5 n^2, as n = 4: their 90 % outlines reach about 52 and 76 a0)

// ---- random numbers: One generator for the whole lab, so a test can
double random();                              // [0, 1)
void seedRandom(uint32_t seed);
uint32_t randomState();                       // the generator's state now (Mulberry32: one 32-bit number)
void skipRandom(uint64_t n);                  // as if random() had been called n times, at once (the state moves by the same step every call: exact, in constant time)

// ---- element and nuclide data ----
const char* name(int Z);                      // "Hydrogen"
const char* symbol(int Z);                    // "H"
int defaultN(int Z);                          // the usual isotope's neutron count
std::pair<int, int> nRange(int Z);            // the neutron slider's range
const std::map<int, std::string>& nuclides(int Z);   // neutrons -> code (S, U, ?, half-life s, ~approx)
const std::string* nucCode(int Z, int N);     // nullptr when nobody has seen it
bool hasStable(int Z);
struct Temp { double melt, boil; char flag; double density; };   // K, K, s/p/e/a or 0, g/cm³ (NaN = unknown)
const Temp& tempData(int Z);
const char* categoryName(int Z);
std::string categoryShown(int Z);   // 0.48: the family as shown; Rf to Og get "(expected)": their place in the table, not measured chemistry
std::string familyNote(int Z);      // 0.48: a sentence for Rf to Og saying so (empty for the others)
int categoryHue(int Z);
struct FamousIsotope { int Z, N; const char* why; };
const std::vector<FamousIsotope>& famous();
std::pair<int, int> ptPos(int Z);             // [row, column] in the periodic table

// ---- how numbers are written ----
std::string supDigits(long n);                // 12 -> ¹²
std::string sci(double x);                    // 5.4×10⁻⁵
std::string metres(double x);                 // 5×10⁻¹⁵ m
std::string fmtLam(double nm);                // wavelength: "656.5 nm", "30.4 pm"
std::string fmtLen(double ang);               // length in Å: "0.53 Å", "26.5 pm"
std::string fmtT(double as);                  // time in attoseconds: "152 as", "380 zs"
std::string fmtEV(double eV);                 // "13.60 eV", "9.2 keV"
std::string fmtHalf(double s, int sf = 3);    // "2.4 s", "5,700 years", "4.47 billion years"
std::string fmtLife(double s);
std::string ionLabel(int Z);                  // H, He⁺, Li²⁺
std::string nuclideLabel(int Z, int N);       // Helium-4 (⁴He)
std::string lrnSci(double x);
std::string lrnSci3(double x, int d = 3);
std::string lrnLen(double m);                 // follows the metric/imperial setting
std::string numText(double x, int digits);      // x to that many significant digits, in the shortest form

// ---- orbitals: every hydrogen-like orbital up to n = 4 in its real orientations ----
struct Orbital {
    int n = 0, l = 0;
    std::string shell, ori, key;              // "2p", "x", "2p|x"
    std::string lab, txt;                     // "2p<sub>x</sub>", "2px"
    std::vector<double> lag;                  // Laguerre coefficients of the radial part
    int sgn = 1;                              // overall sign (the outermost part is +)
    int ang = 0;                              // which angular polynomial
};
const std::vector<Orbital>& orbitals();
int orbIndex(const std::string& key);         // "2p|x" -> index, or a shell "2p" -> its first orientation; -1 if unknown
std::vector<double> lagCoef(int k, double a);
double radial(const Orbital& o, double q);    // q = Z·r in Bohr radii
double angular(int ang, double x, double y, double z);
double psi(const std::string& key, int Z, double x, double y, double z);   // x, y, z in Bohr radii
double psi(int orb, int Z, double x, double y, double z);
bool dipoleOK(int a, int b);                  // can light connect these two orbitals?
double dipoleAngularSq(int a, int b);         // 0.46.1: sum over x, y, z of <A|r_c/r|B>^2 (normalised angular parts)

// ---- complex orbitals (0.10.2): the states with a magnetic quantum number m = +-1 .. +-l, from the real angular parts above ----
// Condon-Shortley phase: Y_l,+m ~ (-1)^m (Re + i c Im) / sqrt 2 and Y_l,-m ~ (Re - i c Im) / sqrt 2, where (Re, Im, c) is the real pair of the same |m| (p: x, y; d: xz, yz and
// x2-y2, xy with c = 2; f: xz2, yz2 and z(x2-y2), xyz with c = 2 and x(x2-3y2), y(3x2-y2); c is 2 where the two real polynomials are Re and Im of (x + iy)^2 divided by 2).
// A complex state is not one of the 30 real orbitals: it has its own id (>= kComplexBase) and shares its shell's radial part. The real orbitals and their indices are untouched.
// Like psi(), the values are exact up to a constant that depends only on (l, |m|) (the app only ever uses ratios and |psi|^2). One electron, non-relativistic,
// no fine structure: all l of one n have one energy, and so do all m of one l.
constexpr int kComplexBase = 1000;
int complexState(int shellOrb, int m);                 // the id of (the shell of real orbital shellOrb, m); -1 when m = 0 or |m| > l
bool isComplexState(int id);
int stateOrbital(int id);                              // the real orbital whose radial part, n and l the state shares (the id itself for a real orbital)
int stateM(int id);                                    // m of a complex state, 0 for a real orbital
bool oriIsComplex(const std::string& ori, int& m);     // "m+2" -> true, m = 2 (the Orientation setting's value)
std::string oriKey(int m);                             // 2 -> "m+2", -1 -> "m-1"
std::complex<double> angularComplex(int l, int m, double x, double y, double z);   // the angular part (with r^l), l = 1..3, m = +-1..+-l
// The real angular polynomials a complex state is made of: (angular(re) + i sign c angular(im)) / sqrt(2), with the Condon-Shortley sign (-1)^m for m > 0 (atom_core.cpp, kPairs). Returns false for l = 0 or |m| > l.
bool complexPairOf(int l, int absM, int& re, int& im, double& c);
std::complex<double> psiComplex(int id, int Z, double x, double y, double z);      // psi of a complex state; x, y, z in Bohr radii
double density(int id, int Z, double x, double y, double z);                       // |psi|^2 of a real orbital or a complex state
std::string stateText(int id);                         // "2pz" or "2p (m = +1)"

// ---- quantum beats (0.10.2, stage 2): a superposition of two orbitals ----
// psi(t) = a psiA e^(-i EA t/hbar) + b psiB e^(-i EB t/hbar), a^2 + b^2 = 1, with the lab's hydrogen-like energies E_n = -rydM(Z, N) Z^2 / n^2 (reduced mass, rydM above).
// The states are NORMALISED here (normFactor), so a^2 and b^2 are the shares. The density changes with the beat phase theta = (EB - EA) t / hbar. A wave's overall phase has no
// physical meaning, so psiSuper() is given relative to state A (multiplied by e^(+i EA t/hbar)): the colours then turn only at the beat frequency, not at the huge energy frequency.
// Both ids may be real orbitals or complex states. In this model all l of one n share one energy: 2s + 2p do not beat (no fine structure, no Lamb shift).
struct Superposition { int idA = 0, idB = 0; double a = 1, b = 0; };
double levelEV(int Z, int N, int n);                   // E_n = -rydM(Z, N) Z^2 / n^2, eV
double beatEV(int Z, int N, const Superposition& s);   // E_B - E_A, eV (signed)
double beatPeriodS(int Z, int N, const Superposition& s);   // h / |E_B - E_A|, s (infinity when the energies are equal)
double normFactor(int id, int Z);                      // 1 / sqrt(integral of |psi|^2), so that normFactor * psi is normalised (x, y, z in Bohr radii)
std::complex<double> overlapAB(int idA, int idB);      // <A|B> of the two NORMALISED states: 1 for the same state, exactly 0 when they are orthogonal (different n or l, or the angular parts are), else the angular overlap
double superNorm2(const Superposition& s, double theta);   // integral of |a A + b B e^(-i theta)|^2 over all space = a^2 + b^2 + 2 a b Re(<A|B> e^(-i theta)); exactly a^2 + b^2 (= 1) for orthogonal states
std::complex<double> psiSuper(const Superposition& s, int Z, double theta, double x, double y, double z);   // (a A + b B e^(-i theta)) / sqrt(superNorm2), A and B normalised, relative to A: normalised for ANY pair
std::complex<double> psiSuperT(const Superposition& s, int Z, int N, double tSeconds, double x, double y, double z);   // the same at real time t: theta = (EB - EA) t / hbar
// ---- photon jumps ----
// 0.49.3 (outside review): the coherent jump of 0.10.2 (psi = cos(theta) A + sin(theta) B e^(-i phi), jumpFractions, jumpTerms, jumpPhaseOf) is gone: one photon leaves the atom in a MIXTURE
// of the two levels (AtomLab::jumpShareTo, atom_sim.h), drawn from the same two populations of dots, each shown with its level's share.
bool dipoleAxes(const Superposition& s, double out[3]);   // can light couple A and B? out = size of the angular dipole integral along x, y, z (relative); false when none

// ---- light: the reduced-mass Rydberg energy and the spectral lines ----
double nuclearMassMe(int Z, int N);              // the nucleus's mass in electron masses: Z m_p + N m_n - B/c² (B from the binding table, AME2020) where the table has it, else A u - Z m_e
double rydM(int Z, int N);                    // eV
struct SpecLine { int lo; int hi; double lam; };   // hi = 0 for a series limit; lam in nm
std::vector<SpecLine> specLines(int Z, int N);
extern const char* const kSeries[6];          // "", "Lyman", "Balmer", ...
struct Rgb { int r, g, b; };
Rgb lamColor(double nm);
std::string lamColorCss(double nm);
// 0.52.1: the spectrum card's one sentence on how the lines are coloured (lamColor: CIE 1931 -> sRGB, white mixed in)
inline const char* spectralColourText() {
    return "A screen cannot show the pure colour of a single wavelength, so each line is drawn in its CIE 1931 colour with just enough white mixed in to fit "
           "the screen's colours, at full brightness: the same hue, paler than the real line.";
}
inline bool isVisibleNm(double nm) { return nm >= kVisibleLoNm && nm <= kVisibleHiNm; }   // inside the visible range (both ends included)
const char* bandTag(double nm);               // " (X-ray)" below 10 nm, " (UV)" below the visible range, " (IR)" above it, "" inside it
std::string vacuumBand(double nm);           // 0.46.1: "vacuum", "vacuum, UV" (for one bracket: "330.393 nm (vacuum, UV; ...)")
std::vector<SpecLine> visibleLines(int Z, int N);   // the lines (not the series limits) that fall in visible light: what the spectrum card marks in its visible strip
double nuclearR(int A);                       // nuclear radius, fm

// ---- the nucleus ----
double nucBeta(int Z, int N);
// 0.22: the nucleus as a smooth CHARGE density, for the picture's Density look: `count` points (x, y, z in fm, one after the other) drawn at random from a two-parameter Fermi
// (Woods–Saxon) charge distribution, a Gaussian for A <= 16, with the lab's shape (quadrupole.h) about the axis (ax, ay, az) and scaled to the measured rms charge radius (IAEA) where there is one.
// 0.46.5: deuteronSpin: 0 unpolarised (the three spin states mixed: round), 1 the M = ±1 state (a dumbbell along the spin axis), 2 the M = 0 state (a doughnut round it); the spin axis is (ax, ay, az).
std::vector<double> nucleusChargeDots(int Z, int N, int count, double ax, double ay, double az, unsigned seed, int deuteronSpin = 0);
// 0.54.1: the same charge density as a function (no random points), for the Density look's exact projection: the model nucleusChargeDots draws from (Fermi, Gaussian, HO,
// the deuteron's CD-Bonn pair folded with the proton's exponential charge, the proton alone), the same shape about the same axis, scaled the same way (rms = the measured
// charge radius; a deformed nucleus also to its shape's <3z^2 - r^2> / <r^2>), here with the scale factors from the density's own moments (by quadrature), not from a sample,
// and normalised so that its volume integral is Z.
struct NucleusChargeDensity {
    int Z = 0, kind = -1;          // -1 none (a lone neutron), 0 the proton (exponential), 1 the deuteron, 2 HO (C-12, O-16), 3 Gaussian (other A <= 16), 4 Fermi
    double n[3] = {0, 0, 1};       // the symmetry axis (the deuteron: its spin axis), unit
    double beta = 0;               // the shape (quadrupole.h); 0 round
    double fx = 1, fz = 1;         // the scale factors put on the base density across and along the axis
    double norm = 0;               // Z / (the base density's integral x fx^2 fz)
    double b = 0, c = 0, a = 0, rmax = 0, hoA = 0, hoAl = 0, kk = 0;   // the base density's parameters (as nucleusChargeDots uses them); kk: the deuteron's P2 weight (0, +1, -2)
    double rEff = 0;               // beyond this radius (fm) the density is taken as 0 (the base's own cut, or where it is below about 1e-13 of the middle)
    double ext997 = 0;             // the radius (fm) holding 99.7 % of the charge (the Density look's grid reaches it)
    bool radial = false;           // round, or the deuteron (Legendre L <= 2): projected through radial tables; else ray by ray through a table over (across, along)
    double atBody(double rp, double z) const;              // e/fm^3, body frame: rp across the axis, z along it (straight from the model)
    double at(double x, double y, double z) const;         // e/fm^3 at a point (fm) in the lab frame
    int nT = 0; double hT = 0, hU = 0, rCut = 0; std::vector<float> T;   // (deformed) the density on a grid over (rp^2, |z|): nT x nT nodes, hU fm^2 and hT fm; rCut: the rays' reach
    int nR = 0; double hR = 0; std::vector<double> I0, I2, K0, K2;   // (radial) the line-of-sight integrals against the distance R from the middle (see projectChargeDensity)
};
NucleusChargeDensity nucleusChargeDensity(int Z, int N, double ax, double ay, double az, int deuteronSpin = 0);
// The exact line-of-sight integral of that density (e/fm^2) on a G x G grid of square cells `cellFm` across, centred on the nucleus: e1 the picture's x direction, e2 its y (up),
// e3 the line of sight (unit, square to each other); out[row * G + col], row 0 at the top (y = +G cellFm / 2), col 0 at the left; each cell holds the value at its centre.
// The deformed ones are integrated ray by ray (trapezoids of one surface thickness a) on a quarter grid of qn x qn rays along and across the projected axis
// (the picture is symmetric both ways) and read at the cells by interpolation.
void projectChargeDensity(const NucleusChargeDensity& d, const double e1[3], const double e2[3], const double e3[3], int G, double cellFm, std::vector<float>& out, int qn = 64);
// ---- 0.46.5: the deuteron's wave function, CD-Bonn (Machleidt, PRC 63, 024001 (2001), arXiv:nucl-th/0006014, appendix D, eqs. D19-D25 and Table XX; data/qcd/nucleus) ----
double deuteronU(double r);                 // the S wave u(r), fm^-1/2 (r: the neutron-proton distance, fm); the integral of u^2 + w^2 is 1
double deuteronW(double r);                 // the D wave w(r), fm^-1/2
// one neutron-proton separation (fm, x y z; z = the spin axis) drawn from |psi|^2 of the spin state (0, 1, 2 as above); uni() gives uniform numbers in [0, 1)
void deuteronSeparation(int spin, const std::function<double()>& uni, double& x, double& y, double& z);
double deuteronFarShare(double r);          // the chance that the neutron and the proton are more than r fm apart
// 0.46.5: the harmonic-oscillator charge density rho ~ (1 + alpha r^2/a^2) exp(-r^2/a^2) measured for C-12 and O-16 (de Vries et al. 1987, via Shukla arXiv:nucl-th/0302030 Table I
// and Loizides et al. arXiv:1408.2549 Table I); false for every other nucleus
bool nucleusHoCharge(int Z, int N, double& a, double& alpha);
// 0.46.5: how thick the drawn charge density's edge is, fm: from 90 % to 10 % of its highest value, going outwards (round shape; NaN for the deuteron, which has no edge but a long halo)
double nucEdgeFm(int Z, int N);
inline int deuteronSpinIndex(const std::string& v) { return v == "M = ±1" ? 1 : v == "M = 0" ? 2 : 0; }   // AtomParams::dspin -> deuteronSpin
bool nucMeasured(int Z, int N);             // 0.46.3: the shape is measured (a B(E2) from ENSDF); doubly magic nuclei are no longer "measured" by expectation
bool nucFrdmKnown(int Z, int N);            // 0.46.3: FRDM(2012) (Moller et al., ADNDT 109-110 (2016) 1) has this nucleus
double nucFrdmBeta(int Z, int N);           // its calculated ground-state beta2, NaN when it has none
double nucBE2Q0Fm2(int Z, int N);          // 0.46.4: the intrinsic moment Q0 = sqrt(16 pi B(E2) / 5) of a rigid rotor, with the sign, e fm^2 (NaN without a B(E2))
double nucBE2ShapeBeta(int Z, int N);      // 0.46.4: the beta2 of the lab's drawn body with that Q0 and the measured size (NaN: none, or |beta2| > 0.6); what nucBeta draws
double nucBE2Beta(int Z, int N);           // 0.46.3 (Raman's first-order convention, shown on the card, not drawn since 0.46.4): beta2 from the measured B(E2) with its sign (NaN without one)
int nucBE2SignSource(int Z, int N);         // 1 the measured 2+ moment, 2 FRDM(2012)'s sign, 3 none (drawn prolate), 0 no B(E2)
double nucBE2Up(int Z, int N);              // 0.28: the table's B(E2; 0+ -> 2+) in e² b² (ENSDF via the IAEA API, tools/make_be2_data.py), NaN when it has none
bool nucBE2Known(int Z, int N);              // the B(E2) table has it
const char* nucShapeWord(double b);
std::string nucleonBallNote();   // 0.46 (review A10): the drawn balls overlap (radius 0.84 fm, centres at least 1.0 fm apart): a drawing choice, said where the nucleons are seen
struct Nucleon { double x, y, z; bool isP; double ph; };   // centre (m), proton?, wobble phase
std::vector<Nucleon> buildNucleus(int Z, int N, double* axis = nullptr, int deuteronSpin = 0);   // (0.46.5: deuteronSpin as for nucleusChargeDots) sized by the measured charge radius where there is one (radii.h), else by the rule; axis: where its symmetry axis points (3 numbers)
std::vector<Nucleon> buildNucleusAxial(int Z, int N);   // buildNucleus without the final random tilt: the symmetry axis along z (for tests of the shape)
std::vector<Nucleon> buildNucleusByRule(int Z, int N);

// ---- radioactive decay (NUBASE2020) ----
struct DecayInfo { std::optional<double> T; bool unk = false; std::vector<std::pair<std::string, double>> br; };
std::optional<DecayInfo> decInfo(int Z, int N);
std::optional<std::pair<int, int>> decMove(const std::string& m);   // [ΔZ, ΔN], none for fission
std::string decWord(const std::string& m);
struct Shot { std::string label; std::string col; double size; };
std::vector<Shot> decShots(const std::string& m);
std::string decShort(const std::string& m);
std::string isoLine(int Z, int N);
std::string isoInfo(int Z, int N);

// ---- mass and temperature ----
struct Binding { double B; bool real; };      // total binding energy, MeV
Binding massBinding(int Z, int N);
std::optional<std::string> tempState(int Z, double T);   // "solid" / "liquid" / "gas", none if never measured
std::string tempLine(int Z);                  // follows the metric/imperial setting
std::string fK(double k), fC(double k), fF(double k);
std::string tAll(double k);                   // "<b>1,811 K</b> · 1,538 °C · 2,800 °F"
const char* stateIcon(const std::string& st); // 🧊 💧 ☁️

// ---- the element card and the facts ----
std::string everyday(int Z);                  // "Where you meet it" (units follow the metric/imperial setting)
int periodOf(int Z);
std::vector<std::string> elementChips(int Z); // category, period · group, state at room temperature, radioactive
std::string factsLinesHtml(int Z);            // the melting / boiling / room-temperature lines (<div> each, <b> bold)
std::string factsNotesHtml(int Z);            // the notes under the bar ("" if none)
extern const char* const kFactsFootnote;      // "Values are for a lump of the element ..."

}  // namespace atom
