// Atom Lab physics core: one electron around any nucleus from hydrogen to oganesson.
// Ported from the web version's Atom Lab (index.html). No drawing here.
//
// Units: everything stored is SI (metres, seconds, m/s, joules). The web worked in atomic units (length = Bohr radius, energy = Hartree, time = atomic unit of time) with ROUNDED
// constants (0.529 Å, 27.211 eV, 24.19 as). Since 0.10.6 stage 2 the lab uses the CODATA 2022 values below, so the picture, the readouts and the Learn guides share one set.
// Two things stay dimensionless on purpose, as in the web: the Metropolis walker that samples the cloud (in Bohr radii, so it draws exactly the same dots from the same random
// numbers) and the nucleon placement (in fm while it is built). Both are turned into metres when stored.
#pragma once

#include <cmath>
#include <cstdint>
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
constexpr double kBoltzmann = 1.380649e-23;           // J/K, exact. page Value?k
constexpr double kFm = 1e-15;                         // one femtometre, m (exact)
constexpr double kFmLab = kFm;                        // metres per fm: now just the exact value (the web needed its own conversion, 0.03 % off)
constexpr double kFmPerA0 = kA0 / kFm;                // femtometres per Bohr radius (52 917.721 054 4)
constexpr double kNucleonR = 0.84;                    // proton charge radius, fm, ROUNDED: CODATA 2022 gives 0.84075(64) fm (page Value?rp, read 2026-10-04); the web's 0.84 is kept
// The relativity line (0.10.6 stage 5). The lab is non-relativistic at every Z. From kRelativityZ on the text WARNS that the error is large (readout, hint and spectrum note all use this one number).
// Why 27: for a point nucleus the Dirac 1s energy mc²(1 − √(1 − (Zα)²)) is above the non-relativistic ½mc²(Zα)² by 0.917 % at Z = 26 and 0.990 % at Z = 27 (about 1 %); there (Zα)² = 0.0388
// (3.9 %) and the 1s electron moves at Zα = 20 % of the speed of light. Z = 20 is 0.54 % (Zα)² = 0.021. The percentages are the lab's own Dirac formula (the readout compares with it); no source was read.
constexpr int kRelativityZ = 27;
// Visible light: ONE range for the whole Atom Lab (the spectrum card, the line colours, the (UV) and (IR) tags, the strip under the picture). 380-780 nm is a CONVENTION this app uses: it is the range of the
// web version's colour code (lamColor); no source for the range was read (it was 380-750 nm in the spectrum card before 0.10.6 stage 5). Inside it a line is drawn in its real colour, outside in a stand-in.
constexpr double kVisibleLoNm = 380, kVisibleHiNm = 780;
constexpr double kYearS = 365.25 * 86400;     // one year, s (web: YEAR_S)
constexpr double kTRoom = 293.15, kTMax = 6000;   // room temperature and the top of the temperature slider, K
constexpr int kCap = 12000;                   // cloud dots kept (web: CAP)
constexpr double kExt[5] = {0, 4.5, 12, 24, 40};  // view radius per n, Bohr radii (web: EXT)

// ---- random numbers: Math.random in the web. One generator for the whole lab, so a test can
// seed it and the web's Math.random the same way and compare the results number for number. ----
double random();                              // [0, 1)
void seedRandom(uint32_t seed);
uint32_t randomState();                       // the generator's state now (Mulberry32: one 32-bit number)
void skipRandom(uint64_t n);                  // as if random() had been called n times, at once (the state moves by the same step every call: exact, in constant time)

// ---- element and nuclide data (generated from index.html) ----
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
int categoryHue(int Z);
int categoryIndex(int Z);
struct FamousIsotope { int Z, N; const char* why; };
const std::vector<FamousIsotope>& famous();
std::pair<int, int> ptPos(int Z);             // [row, column] in the periodic table

// ---- how numbers are written (as the web's helpers of the same name) ----
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
std::string jsNum(double x, int digits);      // +x.toPrecision(digits) written as JavaScript would

// ---- orbitals: every hydrogen-like orbital up to n = 4 in its real orientations ----
struct Orbital {
    int n = 0, l = 0;
    std::string shell, ori, key;              // "2p", "x", "2p|x"
    std::string lab, txt;                     // "2p<sub>x</sub>", "2px"
    std::vector<double> lag;                  // Laguerre coefficients of the radial part
    int sgn = 1;                              // overall sign (the outermost part is +)
    int ang = 0;                              // which angular polynomial
};
const std::vector<Orbital>& orbitals();       // in the web's order (n, then l, then orientation)
int orbIndex(const std::string& key);         // "2p|x" -> index, or a shell "2p" -> its first orientation; -1 if unknown
std::vector<double> lagCoef(int k, double a);
double radial(const Orbital& o, double q);    // q = Z·r in Bohr radii
double angular(int ang, double x, double y, double z);
double psi(const std::string& key, int Z, double x, double y, double z);   // x, y, z in Bohr radii
double psi(int orb, int Z, double x, double y, double z);
bool dipoleOK(int a, int b);                  // can light connect these two orbitals?

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
// ---- smooth photon jumps (0.10.2, stage 3) ----
// A cloud jump shows psi = cos(theta) A + sin(theta) B e^(-i phi) (theta from 0 to pi/2). The picture is built from TWO fixed populations of dots: the cloud that was on screen (dots
// sampled from |A|^2) and a new one of the target orbital (from |B|^2), N dots each. A dot of the first is shown with probability fA, of the second with fB, where
//   |A|^2 fA + |B|^2 fB = |psi|^2   (so the shown dots are exact samples of N |psi|^2, at every moment), and 0 <= f <= 1 because |psi|^2 <= (cos|A| + sin|B|)^2 <= |A|^2 + |B|^2.
// The fractions blend "the old dots fill the density first" (weight cos^2 theta) with "the new dots fill it first" (weight sin^2 theta): at theta = 0 only the old dots are shown,
// at pi/2 only the new ones, so the cloud on screen never reshuffles at the start or the end.
void jumpFractions(std::complex<double> A, std::complex<double> B, double theta, double phi, double& fA, double& fB);
// The same, with the terms that do not depend on the dot worked out once per frame (0.10.6 stage 8): the very same numbers, bit for bit (tested), without a cos, a sin and a complex exp per dot.
struct JumpTerms { double cth, sth, lam; std::complex<double> em, ep; };   // cos theta, sin theta, cos^2 theta, exp(-i phi), exp(i phi u)
JumpTerms jumpTerms(double theta, double phi, double u);
void jumpFractions(std::complex<double> A, std::complex<double> B, const JumpTerms& T, double& fA, double& fB);
// The phase of a dot during a jump (the gauge ends on the new orbital's own phase): the same expression as AtomLab::jumpPhase, with the terms given.
inline double jumpPhaseOf(std::complex<double> A, std::complex<double> B, const JumpTerms& T) { return std::arg(T.ep * (T.cth * A + T.sth * B * T.em)); }
bool dipoleAxes(const Superposition& s, double out[3]);   // can light couple A and B? out = size of the angular dipole integral along x, y, z (relative); false when none

// ---- light: the reduced-mass Rydberg energy and the spectral lines ----
double nuclearMassMe(int Z, int N);              // the nucleus's mass in electron masses: Z m_p + N m_n - B/c² (B from the binding table, AME2020) where the table has it, else A u - Z m_e
double rydM(int Z, int N);                    // eV
struct SpecLine { int lo; int hi; double lam; };   // hi = 0 for a series limit; lam in nm
std::vector<SpecLine> specLines(int Z, int N);
extern const char* const kSeries[6];          // "", "Lyman", "Balmer", ...
struct Rgb { int r, g, b; };
Rgb lamColor(double nm);
std::string lamColorCss(double nm);           // "rgb(r,g,b)" or the web's hex stand-ins
inline bool isVisibleNm(double nm) { return nm >= kVisibleLoNm && nm <= kVisibleHiNm; }   // inside the visible range (both ends included)
const char* bandTag(double nm);               // " (X-ray)" below 10 nm, " (UV)" below the visible range, " (IR)" above it, "" inside it
std::vector<SpecLine> visibleLines(int Z, int N);   // the lines (not the series limits) that fall in visible light: what the spectrum card marks in its visible strip
double nuclearR(int A);                       // nuclear radius, fm

// ---- the nucleus ----
double nucBeta(int Z, int N);                 // quadrupole deformation β₂
bool nucMeasured(int Z, int N);
bool nucBE2Known(int Z, int N);              // the B(E2) table has it (0.10.9 stage 2: nucMeasured is also true for doubly magic nuclei, which are drawn round by expectation)
const char* nucShapeWord(double b);
struct Nucleon { double x, y, z; bool isP; double ph; };   // centre (m), proton?, wobble phase
std::vector<Nucleon> buildNucleus(int Z, int N);        // sized by the measured charge radius where there is one (radii.h), else by the rule
std::vector<Nucleon> buildNucleusAxial(int Z, int N);   // buildNucleus without the final random tilt: the symmetry axis along z (for tests of the shape)
std::vector<Nucleon> buildNucleusByRule(int Z, int N);  // the rule only: the web version's layout (the saved web positions test this one)

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

// ---- the element card and the facts (web: elemCardHTML, factsHTML) ----
std::string everyday(int Z);                  // "Where you meet it" (units follow the metric/imperial setting)
int periodOf(int Z);
std::vector<std::string> elementChips(int Z); // category, period · group, state at room temperature, radioactive
std::string factsLinesHtml(int Z);            // the melting / boiling / room-temperature lines (<div> each, <b> bold)
std::string factsNotesHtml(int Z);            // the notes under the bar ("" if none)
extern const char* const kFactsFootnote;      // "Values are for a lump of the element ..."

}  // namespace atom
