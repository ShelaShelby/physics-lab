// The calculator (0.49): a units-aware physics calculator. No graphics: ui/calc_panel.cpp draws it, tests/test_calc.cpp checks it.
//
// What it does: it reads a line such as "384400 km / c" or "5 km to mi", works out the value in SI units and keeps track of the units (dimensions) through
// every step, so it can refuse what makes no sense ("5 km + 3 s") instead of giving a wrong number. Every unit and constant it knows is in symbols(), each
// with what it means and where its value comes from (CODATA 2022, IAU, NASA/JPL, the SI definitions).
//
// The rules (the panel's help says the same):
//   numbers   123  1.5  .5  2e-19  6.02E23   (no commas: "384,400" is refused, so a decimal comma can never be misread)
//   + - * /   and × · ÷ − also work;  ^ or ** for powers, and ² ³ ⁻¹ ... after a number or a unit
//   juxtaposition   a number or unit written next to a unit or a bracket multiplies it, and binds TIGHTER than * and /:
//             "100 km / 2 hr" is (100 km) / (2 hr) = 50 km/hr, "J/kg K" is J / (kg K). Two plain numbers next to each other ("384 400") are refused.
//             After / that is only done for a quantity (units, or a number and units); "4/3 π r^3", "1/2 km" or "1/(2 π) hP" are refused with both
//             readings, because calculators disagree about them: write the brackets, or *.
//   powers    bind tightest and go right to left: 2^3^2 = 2^9; -2^2 = -4; 2^-1 = 0.5
//   to        "expr to unit" shows the answer in that unit ("5 km to mi"); also "->" and "→". After "to" only units and constants, no plain numbers.
//             0.50.1: after "to" an energy, a wavenumber (1/m), a frequency and a temperature convert into each other, and a mass into an energy, through one exact
//             factor each (E = h c ν̃, E = h f with f cyclic, E = kB T, E = m c²), named in a note: "1 eV to cm^-1", "1 u to MeV". + still refuses to mix them.
//             0.50.2: in Hz a frequency is cyclic (E = h f), in rad/s angular (E = ħω): "1 eV to rad/s" = e/ħ, "1e15 rad/s to eV", "1 GHz to rad/s" (× 2π) with a
//             note; a photon's vacuum wavelength through E = h c / λ: "1 eV to nm", "500 nm to eV" (not temperature or mass to a length).
//   ħ → ω     0.50.1, rule 0.50.2: the net powers of ħ (hbar) and h (hP) typed in the line are carried like a dimension. A frequency is angular (ω, rad/s) when the
//             power of ħ is not 0 and the power of h is not below 0 (E/ħ, μB B/ħ, h f/ħ); dividing by h makes it cyclic (E/h, gp e ħ/(2 mp) B/(2h), ħ 2π f/h).
//             An angular answer after "to" a hertz unit gets a note with f = ω/2π; with no "to" the note first says that a rate (a decay rate) has no 2π, and the
//             answer stays undivided. No note after "to 1/s" (a rate) or "to rad/s"; converting an angular ħ line to an energy is refused (that needs ħω).
//   complex   0.50.2: i is the imaginary unit (3 + 4i), with re, im, conj, abs, arg and exp of a complex number; sqrt or a fractional power of a negative number is
//             imaginary only in a line that uses i (else refused as before). A part below 1e-15 of the whole is dropped (exp(iπ) = -1, not -1 + 1.2e-16 i).
//             Other functions and the conversions through E = hf ... work on real numbers only. Units belong to the whole number: (2 + 3i) m.
//   variables 0.50.2: "x = 2 m" names a value (the panel keeps it with the line); the names of units, constants, functions, i and ans can't be used.
//   0 values  0.50.2: binom(n, k) with k < 0 or k > n, assoclegendre with |m| > l, cg and wigner3j with |m| > j, M ≠ m1 + m2 or the triangle rule broken are 0
//             with a note (as sympy and Mathematica), not refused; non-whole or non-half-whole arguments are still refused.
//   underflow 0.50.2: a value below the smallest normal double (2.2e-308) is 0 with a note, and + and sum() go on with it (1 + exp(-800) = 1); an overflow
//             (over 1e308) is refused, and so is a number typed below 1e-307.
//   functions sqrt cbrt abs sin cos tan asin acos atan sinh cosh tanh exp ln log10, always with brackets: sqrt(2). Angles in radians, or write deg: sin(30 deg)
//             0.50.1, on plain numbers: fact(n) and n!, gamma, lngamma, erf, erfc, besselj(n, x), sphbessel(l, x), legendre(l, x), assoclegendre(l, m, x) (WITH the
//             Condon-Shortley phase), laguerre(n, x), assoclaguerre(n, k, x) (L_n^(k), no n! factor), hermite(n, x) (physicists'), binom(n, k),
//             cg(j1, m1, j2, m2, J, M), wigner3j(j1, j2, j3, m1, m2, m3) (half-integers as 0.5 or 1/2; 0.50.2: exact sums, every j up to 200),
//             sum(expr, k, from, to), solve(expr, x, a, b) (bisection).
//   commas    only between a function's arguments, inside its brackets; anywhere else a comma is refused (no thousands separators, no decimal commas)
//   1/(4 π ε0) 0.50.1: "1/(4 π ε0) e^2 / r" (1 over a bracket holding ε0, then a quantity) is read as (1/(4 π ε0)) × quantity, with a note
//   matrices  0.54.2: [[a, b], [c, d]], each row in its own brackets, from 1x1 to 4x4 ([[1, 2]] is a row, [[1], [2]] a column; one pair of brackets, [1, 2], is a
//             row too). The entries are any expressions, complex allowed. One unit belongs to the whole matrix, as for a complex number: [[1, 2], [3, 4]] eV;
//             entries of the same kind in different units mix ([[1 eV, 500 meV]], all SI inside), entries of different kinds are refused (also a bare 0 next to
//             an energy: take the unit outside). + and − (same size, same kind), * and juxtaposition (matrix × matrix by the size rule, number or quantity ×
//             matrix), matrix / number, unary −, matrix ^ whole number ≥ 0 (square). transpose(M), dagger(M) (conjugate transpose), conj(M), det(M), tr(M),
//             identity(n), kron(A, B) (result at most 4x4), eig(M): the eigenvalues of a Hermitian matrix (equal to its dagger within 1e-12 of its largest
//             entry), real, smallest first, with the matrix's unit, as a column that is shown as a list. Built in: the Pauli matrices sigmax, sigmay, sigmaz,
//             also typed σx, σy, σz (sigma and σ alone stay the Stefan-Boltzmann constant). Variables and ans hold a matrix like any value.
//             ħ and h: their powers are carried for the WHOLE matrix, as for a number (entries with different powers: unknown, no note); a matrix or eig list of
//             frequencies from ħ gets the same angular-frequency note, without a single f. After "to" only a unit of the same kind (the E = hf ... conversions
//             work on single numbers); the Hz ↔ rad/s factor 2π applies as for a number.
//             Refused, each saying what to write: sizes that do not fit, more than 4 rows or columns, matrix + number (write M + c identity(n)), det, tr, eig
//             or a power of a non-square matrix, eig of a non-Hermitian one (with how far off it is), 1/M and M^-1 (no inverse), sqrt, sin, exp ... of a matrix.
//             Limits: at most 4x4; eigenvalues only (no eigenvectors), no inverse, no matrix exponential; ordinary double arithmetic: eigenvalues are right to
//             about 13 digits of the largest one (the tests: 60 seeded matrices with known eigenvalues), a part below that of the largest entry is rounding.
//   ans       the last answer
// Refused with a message that says what to write instead: adding different kinds of quantity, sin of a length, sqrt of a negative number (in a line without i),
// dividing by zero, results too big for the computer's numbers, "h" (hours or Planck's constant? write hr or hP), "log" (write ln or log10), "in" (write to),
// °C and °F (kelvin only), unknown names (with a "did you mean").
#pragma once

#include <array>
#include <complex>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace calc {

// A fraction n/d (d > 0, lowest terms): the power of one base unit. Fractions appear after sqrt (m^(1/2)).
struct Rat {
    long long n = 0, d = 1;
    bool zero() const { return n == 0; }
    bool operator==(const Rat& o) const { return n == o.n && d == o.d; }
    bool operator!=(const Rat& o) const { return !(*this == o); }
};

constexpr int kBaseCount = 6;                                  // kg, m, s, A, K, mol (in that order, also the order the units are written in)
extern const char* const kBaseSymbol[kBaseCount];
struct Dim {
    std::array<Rat, kBaseCount> e{};
    bool none() const { for (const Rat& r : e) if (!r.zero()) return false; return true; }   // a plain number
    bool operator==(const Dim& o) const { return e == o.e; }
    bool operator!=(const Dim& o) const { return !(*this == o); }
};
Dim dimOf(int kg, int m, int s, int A = 0, int K = 0, int mol = 0);

// The value in SI units. 0.50.2: also the net powers of ħ (hbar) and h (hP) the line typed, carried like a dimension (NaN when a sum mixes different powers): they
// decide whether a frequency is angular (see "ħ → ω" above).
// 0.54.2: a small matrix (at most 4x4), its entries in SI, row after row. list: it is eig's answer (a column of eigenvalues, shown as a list).
constexpr int kMatMax = 4;
struct Matrix {
    int rows = 0, cols = 0;
    bool list = false;
    std::array<std::complex<double>, kMatMax * kMatMax> a{};
    std::complex<double>& at(int r, int c) { return a[static_cast<std::size_t>(r * cols + c)]; }
    const std::complex<double>& at(int r, int c) const { return a[static_cast<std::size_t>(r * cols + c)]; }
};
// im: 0.50.2, the imaginary part (in SI, like v). mat: 0.54.2, set when the value is a matrix (then v and im are 0; dim and the Planck powers are the whole matrix's)
struct Quantity {
    double v = 0; Dim dim; double hbarPow = 0, hPow = 0; double im = 0;
    std::shared_ptr<const Matrix> mat;
    Quantity() = default;
    Quantity(double value, const Dim& d = Dim{}, double hbar = 0, double h = 0, double imag = 0) : v(value), dim(d), hbarPow(hbar), hPow(h), im(imag) {}   // (as it was written before 0.54.2: {v, dim, ...})
};

// ---- the symbols ----
enum class SymKind { Unit, Constant };
struct Symbol {
    std::string name;            // as typed, e.g. "km", "c", "μ0"
    SymKind kind = SymKind::Unit;
    Quantity q;
    std::string meaning;         // "kilometre", "speed of light in vacuum"
    std::string source;          // where the value comes from; "" for units defined from other units (the meaning says how)
    std::string published;       // a constant's value as its source prints it, e.g. "6.674 30(15) × 10^-11" ("" when computed); the tests check q.v against it
    bool listed = true;          // shown in the panel's list (the prefixed units and the second spellings are not)
};
const std::vector<Symbol>& symbols();
const Symbol* findSymbol(const std::string& name);
const std::vector<std::string>& functionNames();

// ---- evaluation ----
struct Result {
    bool empty = true;           // nothing typed (only spaces): no answer and no error
    bool ok = false;
    std::string error;           // what is wrong, in plain words, and what to write instead
    int errPos = -1, errLen = 0; // where in the input (bytes), -1 when it is the whole line
    Quantity q;                  // the answer in SI (also when "to" was used: this is what ans becomes)
    std::string number;          // the answer as shown: "1.282282624", "6.02214076×10²³"
    double inUnit = 0;           // 0.50.1: the answer in the unit shown, before it is rounded to 10 digits (after "to" with a spectroscopy conversion it is not q / unit)
    std::string unit;            // its unit as shown: "s", "km/hr", "J/K", "" for a plain number
    std::string kindWord;        // what kind of quantity it is: "time", "speed", ... ("" when it has no common name)
    std::string readAs;          // how the line was understood, with every implicit bracket written out: "(100 km) / (2 hr)"
    std::vector<std::string> used;   // one line per unit or constant used: "km = kilometre = 1000 m"
    std::vector<std::string> also;   // the same answer in a few other handy units: "= 0.02137 min"
    std::vector<std::string> notes;  // worth knowing: "sin, cos and tan work in radians ..."
    std::string assigned;            // 0.50.2: "x" for a line "x = 2 m": the panel keeps q under that name when the line is kept
    // 0.54.2: a matrix answer: its size and its entries in the unit shown, row after row (number is then the one-line text "[[0, 1], [1, 0]]"); list: eig's
    // answer, a column shown as a list ("-1, 1"). rows = 0 for a number.
    int rows = 0, cols = 0;
    bool list = false;
    std::vector<std::complex<double>> cells;
};
// 0.50.2: the named variables kept so far (x = 2 m on one line, x on the next), each as its value in SI
using Vars = std::map<std::string, Quantity>;
Result evaluate(const std::string& line, const Quantity* ans = nullptr, const Vars* vars = nullptr);

// ---- writing numbers and units ----
std::string formatNumber(double v, int digits = 10);            // "%.10g", with ×10ⁿ for the exponent; -0 is "0"
std::string exactNumber(double v);                              // plain ASCII that reads back as the same double ("%.17g"), for putting a value into the line
std::string complexText(double re, double im, bool unitFollows = false, int digits = 10);   // 0.50.2: "3 + 4i", "0.5 − 0.5i", "-i"; "(3 + 2i)" when a unit follows
// 0.54.2: a matrix on one line, "[[0, 1], [1, 0]]", or eig's list, "-1, 1"; and a variable's or ans's value as the panel writes it (a number or a matrix, with its unit)
std::string matrixText(const std::vector<std::complex<double>>& cells, int rows, int cols, bool list, int digits = 10);
std::string valueText(const Quantity& q, int matrixDigits = 6);
bool isPauliName(const std::string& name);                      // sigmax, sigmay, sigmaz, σx, σy, σz
std::string unitText(const Dim& d);                             // "kg·m²/s²" → shown as "J", "J/K", "m³/(kg·s²)"; "" for a plain number
std::string kindWord(const Dim& d);                             // "length", "speed", ... or ""

}  // namespace calc
