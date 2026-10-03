// Atom Lab physics core: one electron around any nucleus from hydrogen to oganesson.
// Ported from the web version's Atom Lab (index.html). No drawing here.
//
// Units: everything stored is SI (metres, seconds, m/s, joules). The web works in atomic units
// (length = Bohr radius, energy = Hartree, time = 24.19 as) with rounded constants for what it
// shows; the same rounded constants are used here to convert, so every number on screen matches.
// Two things stay dimensionless on purpose, as in the web: the Metropolis walker that samples the
// cloud (in Bohr radii, so it draws exactly the same dots from the same random numbers) and the
// nucleon placement (in fm while it is built). Both are turned into metres when stored.
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

// ---- constants (the web's own values) ----
constexpr double kPi = 3.14159265358979323846;
constexpr double kLn2 = 0.69314718055994530942;
constexpr double kA0Ang = 0.529;              // Bohr radius, Å (web: A0)
constexpr double kHartreeEV = 27.211;         // Hartree energy, eV (web: HARTREE)
constexpr double kAuTimeAs = 24.19;           // atomic unit of time, attoseconds (web: AU_T)
constexpr double kA0 = kA0Ang * 1e-10;        // Bohr radius, m
constexpr double kAuTime = kAuTimeAs * 1e-18; // atomic unit of time, s
constexpr double kAuSpeed = kA0 / kAuTime;    // atomic unit of speed, m/s
constexpr double kEV = 1.602176634e-19;       // one electronvolt, J
constexpr double kHartree = kHartreeEV * kEV; // Hartree energy, J
constexpr double kFm = 1e-15;                 // one femtometre, m
constexpr double kFmPerA0 = 52917.7;          // femtometres per Bohr radius (web: FM_AU)
constexpr double kNucleonR = 0.84;            // proton charge radius, fm (web: NUCLEON_R)
// The web measures the nucleus in fm and converts with FM_AU = 52,917.7 fm per Bohr radius, while
// its Bohr radius on screen is 0.529 Å (52,900 fm). To keep every picture and ratio as on the web,
// nuclear lengths go fm -> Bohr radii -> metres with the web's own two numbers (0.03 % from 1e-15).
constexpr double kFmLab = kA0 / kFmPerA0;     // metres per fm, as the lab uses it
constexpr double kRydInf = 13.605693123;      // Rydberg energy for an infinitely heavy nucleus, eV
constexpr double kHcEVnm = 1239.841984;       // h·c, eV·nm
constexpr double kMeU = 1822.888486;          // one atomic mass unit in electron masses
constexpr double kYearS = 365.25 * 86400;     // one year, s (web: YEAR_S)
constexpr double kTRoom = 293.15, kTMax = 6000;   // room temperature and the top of the temperature slider, K
constexpr int kCap = 12000;                   // cloud dots kept (web: CAP)
constexpr double kExt[5] = {0, 4.5, 12, 24, 40};  // view radius per n, Bohr radii (web: EXT)

// ---- random numbers: Math.random in the web. One generator for the whole lab, so a test can
// seed it and the web's Math.random the same way and compare the results number for number. ----
double random();                              // [0, 1)
void seedRandom(uint32_t seed);

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

// ---- light: the reduced-mass Rydberg energy and the spectral lines ----
double rydM(int Z, int N);                    // eV
struct SpecLine { int lo; int hi; double lam; };   // hi = 0 for a series limit; lam in nm
std::vector<SpecLine> specLines(int Z, int N);
extern const char* const kSeries[6];          // "", "Lyman", "Balmer", ...
struct Rgb { int r, g, b; };
Rgb lamColor(double nm);
std::string lamColorCss(double nm);           // "rgb(r,g,b)" or the web's hex stand-ins
double nuclearR(int A);                       // nuclear radius, fm

// ---- the nucleus ----
double nucBeta(int Z, int N);                 // quadrupole deformation β₂
bool nucMeasured(int Z, int N);
const char* nucShapeWord(double b);
struct Nucleon { double x, y, z; bool isP; double ph; };   // centre (m), proton?, wobble phase
std::vector<Nucleon> buildNucleus(int Z, int N);

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
