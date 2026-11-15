#include "atom_core.h"
#include "decay_chain.h"
#include "outline.h"
#include "radii.h"
#include "quadrupole.h"
#include "render/cie1931.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <random>
#include <cstdlib>
#include <cstring>

#include "orbit/belts.h"
#include "orbit/format.h"

namespace atom {

#include "atom_data.inc"

// ---------------------------------------------------------------- random numbers
static orbit::Mulberry32 g_rng(0x9e3779b9u);
double random() { return g_rng.next(); }
void seedRandom(uint32_t seed) { g_rng = orbit::Mulberry32(seed); }
uint32_t randomState() { return g_rng.seed; }
void skipRandom(uint64_t n) { g_rng.seed += uint32_t(n * 0x6D2B79F5ull); }   // Mulberry32::next() adds 0x6D2B79F5 to the seed (mod 2^32) before it mixes it, so n calls move it by n times that

// ---------------------------------------------------------------- small number helpers
// parseFloat: the number at the start of the text, NaN when there is none
static double parseFloat(const std::string& s) {
    const char* b = s.c_str();
    char* e = nullptr;
    const double v = std::strtod(b, &e);
    return e == b ? NAN : v;
}
// Math.round: halves go up, and a small negative rounds to −0
static double roundHalfUp(double x) {
    const double r = std::floor(x + 0.5);
    return r == 0 && x < 0 ? -0.0 : r;
}
static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = s.find(sep, a);
        out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) return out;
        a = b + 1;
    }
}
std::string numText(double x, int digits) { return fmt::shortestNumber(std::strtod(fmt::toPrecision(x, digits).c_str(), nullptr)); }

// ---------------------------------------------------------------- element and nuclide data
const char* name(int Z) { return Z >= 0 && Z <= 118 ? kName[Z] : ""; }
const char* symbol(int Z) { return Z >= 0 && Z <= 118 ? kSymbol[Z] : ""; }
int defaultN(int Z) { return Z >= 0 && Z <= 118 ? kDefaultN[Z] : 0; }   // 0.50.3: guarded as name() and symbol()

static const std::vector<std::map<int, std::string>>& nucTable() {
    static std::vector<std::map<int, std::string>> t = [] {
        std::vector<std::map<int, std::string>> v(119);
        for (const char* line : kNucLines) {
            const std::string l = line;
            const std::size_t bar = l.find('|');
            const int z = std::atoi(l.substr(0, bar).c_str());
            for (const std::string& e : split(l.substr(bar + 1), ',')) {
                const std::size_t c = e.find(':');
                v[z][std::atoi(e.substr(0, c).c_str())] = e.substr(c + 1);
            }
        }
        return v;
    }();
    return t;
}
const std::map<int, std::string>& nuclides(int Z) { return nucTable()[Z]; }
const std::string* nucCode(int Z, int N) {
    if (Z < 1 || Z > 118) return nullptr;
    const auto& m = nucTable()[Z];
    const auto it = m.find(N);
    return it == m.end() ? nullptr : &it->second;
}
// the neutron slider: from 3 below the lightest known isotope to 3 above the heaviest (0 to 180)
std::pair<int, int> nRange(int Z) {
    const auto& m = nuclides(Z);
    if (m.empty()) return {0, 0};                                // 0.50.3: no begin() of an empty table
    return {std::max(0, m.begin()->first - 3), std::min(180, m.rbegin()->first + 3)};
}
// stable, or a half-life of over 10¹⁵ years
bool hasStable(int Z) {
    for (const auto& [n, c] : nuclides(Z))
        if (c == "S" || (c[0] != '~' && parseFloat(c) >= 1e15 * kYearS)) return true;
    return false;
}
const Temp& tempData(int Z) {
    static_assert(sizeof(Temp) == sizeof(TempData), "same layout");
    return reinterpret_cast<const Temp&>(kTempD[Z]);
}
const char* categoryName(int Z) { return kCats[kCatOf[Z]].name; }
int categoryHue(int Z) { return kCats[kCatOf[Z]].hue; }
// 0.48 (review): Rf to Og have a family only by their column of the table; their chemistry is known from a few one-atom-at-a-time experiments or not at all.
// Cn and Og: Smits et al., Phys. Rep. 1035 (2023), section 6.6 (arXiv:2301.02553; excerpt saved as data/superheavy/smits2023_section6.txt).
std::string categoryShown(int Z) { return std::string(categoryName(Z)) + (Z >= 104 ? " (expected)" : ""); }
std::string familyNote(int Z) {
    if (Z < 104) return "";
    std::string s = "Its family is the one expected from its column of the table: its chemistry has been tested only with single atoms, or not at all.";
    if (Z == 112) s += " Calculations expect copernicium to behave like a noble gas rather than a metal (Smits et al. 2023).";
    if (Z == 118) s += " Calculations predict that oganesson, unlike the other noble gases, may be a solid semiconductor at room temperature (Smits et al. 2023).";
    return s;
}
const std::vector<FamousIsotope>& famous() {
    static std::vector<FamousIsotope> v = [] {
        std::vector<FamousIsotope> out;
        for (const Famous& f : kFamous) out.push_back({f.Z, f.N, f.why});
        return out;
    }();
    return v;
}
std::pair<int, int> ptPos(int Z) {   // lanthanides and actinides sit in two rows below
    if (Z <= 2) return {1, Z == 1 ? 1 : 18};
    if (Z <= 10) return {2, Z <= 4 ? Z - 2 : Z + 8};
    if (Z <= 18) return {3, Z <= 12 ? Z - 10 : Z};
    if (Z <= 36) return {4, Z - 18};
    if (Z <= 54) return {5, Z - 36};
    if (Z <= 56) return {6, Z - 54};
    if (Z <= 71) return {9, Z - 54};
    if (Z <= 86) return {6, Z - 68};
    if (Z <= 88) return {7, Z - 86};
    if (Z <= 103) return {10, Z - 86};
    return {7, Z - 100};
}

// ---------------------------------------------------------------- how numbers are written
static const char* const kSup[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
std::string supDigits(long n) {
    std::string out;
    for (char c : std::to_string(n)) out += c == '-' ? "⁻" : kSup[c - '0'];
    return out;
}
std::string sci(double x) {
    if (!std::isfinite(x) || x <= 0) return "0";                // 0.50.3 (outside review): no logarithm of 0
    int e = static_cast<int>(std::floor(std::log10(x)));
    double m = std::round(x / std::pow(10.0, e) * 10) / 10;     // 0.50.3: round first, then fix the exponent (9.97 was "10.0")
    if (m >= 10) { m /= 10; e++; }
    return fmt::fixed(m, 1) + "×10" + supDigits(e);
}
std::string metres(double x) {
    int e = static_cast<int>(std::floor(std::log10(x) + 1e-9));
    double m = roundHalfUp(x / std::pow(10.0, e) * 10) / 10;
    if (m >= 10) { m /= 10; e++; }
    return (m == 1 ? std::string("10") : fmt::shortestNumber(m) + "×10") + supDigits(e) + " m";
}
std::string fmtLam(double l) { return l < 0.1 ? fmt::fixed(l * 1000, 1) + " pm" : fmt::fixed(l, l < 100 ? 1 : 0) + " nm"; }
std::string fmtLen(double a) { return a < 0.1 ? fmt::fixed(a * 100, a < 0.01 ? 2 : 1) + " pm" : fmt::fixed(a, 2) + " Å"; }
std::string fmtT(double as) { return as < 1 ? fmt::fixed(as * 1000, as < 0.01 ? 1 : 0) + " zs" : fmt::fixed(as, 0) + " as"; }
std::string fmtEV(double e) {
    return e >= 1e5 ? fmt::fixed(e / 1e3, 0) + " keV" : e >= 1e3 ? fmt::fixed(e / 1e3, 1) + " keV" : fmt::fixed(e, 2) + " eV";
}
std::string fmtHalf(double t, int sf) {
    auto num = [&](double x, const char* u, bool pl) {
        const double v = std::strtod(fmt::toPrecision(x, sf).c_str(), nullptr);
        return fmt::localeNumber(v) + " " + u + (pl && v != 1 ? "s" : "");
    };
    if (t < 1e-6) return num(t * 1e9, "ns", false);
    if (t < 1e-3) return num(t * 1e6, "µs", false);
    if (t < 1) return num(t * 1e3, "ms", false);
    if (t < 60) return num(t, "s", false);
    if (t < 3600) return num(t / 60, "minute", true);
    if (t < 86400) return num(t / 3600, "hour", true);
    if (t < 200 * 86400) return num(t / 86400, "day", true);
    const double y = t / kYearS;
    return y < 1e6 ? num(y, "year", true) : y < 1e9 ? num(y / 1e6, "million year", true)
         : y < 1e12 ? num(y / 1e9, "billion year", true) : sci(y) + " years";
}
std::string fmtLife(double t) { return !(t > 0) ? "an instant" : t < 1e-6 ? sci(t) + " s" : fmtHalf(t); }
// the one-electron ion: H, He⁺, Li²⁺ ... Fe²⁵⁺
std::string ionLabel(int Z) { return Z < 2 ? symbol(Z) : std::string(symbol(Z)) + (Z > 2 ? supDigits(Z - 1) : "") + "⁺"; }
std::string nuclideLabel(int Z, int N) {
    const int A = Z + N;
    const char* extra = Z == 1 && N == 1 ? "deuterium" : Z == 1 && N == 2 ? "tritium" : nullptr;
    return std::string(name(Z)) + "-" + std::to_string(A) + " (" + supDigits(A) + symbol(Z) + ")" + (extra ? std::string("  ·  ") + extra : "");
}
static std::string supAbs(int e) { return (e < 0 ? "⁻" : "") + supDigits(std::abs(e)); }
std::string lrnSci(double x) {
    if (!std::isfinite(x) || x <= 0) return "0";
    if (x >= 0.1 && x < 10000) return x < 10 ? fmt::fixed(x, 1) : fmt::localeNumber(roundHalfUp(x));
    int e = static_cast<int>(std::floor(std::log10(x)));
    double m = std::round(x / std::pow(10.0, e) * 10) / 10;     // 0.50.3: round first, then fix the exponent
    if (m >= 10) { m /= 10; e++; }
    return fmt::fixed(m, 1) + "×10" + supAbs(e);
}
std::string lrnSci3(double x, int d) {
    if (!std::isfinite(x) || x <= 0) return "0";
    d = std::max(d, 1);                                         // 0.50.3 (outside review): d = 0 asked fixed() for -1 decimals
    int e = static_cast<int>(std::floor(std::log10(x)));
    {   const double p = std::pow(10.0, d - 1), m = std::round(x / std::pow(10.0, e) * p) / p;   // 0.50.3: 9.996 to 3 digits is 1.00×10^(e+1), not 10.0×10^e
        if (m >= 10) e++; }
    if (e >= -2 && e < 4) {
        std::string s = fmt::toPrecision(x, d);
        if (s.find('.') != std::string::npos) {   // drop trailing zeros, then a trailing point
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        return s.find('e') != std::string::npos ? s : fmt::localeNumber(std::strtod(s.c_str(), nullptr), 6);
    }
    return fmt::fixed(x / std::pow(10.0, e), d - 1) + "×10" + supAbs(e);
}
std::string lrnLen(double m) {
    if (fmt::imperial())
        return m >= 1609.344 ? lrnSci(m / 1609.344) + " mi" : m >= 0.3048 ? lrnSci(m * 3.28084) + " ft" : lrnSci(m * 39.3701) + " in";
    return m >= 1000 ? lrnSci(m / 1000) + " km" : m >= 1 ? lrnSci(m) + " m" : m >= 0.01 ? lrnSci(m * 100) + " cm" : lrnSci(m * 1000) + " mm";
}

// ---------------------------------------------------------------- orbitals
// ψ = (radial part) × (angular polynomial), exact up to a constant (the sampling only uses ratios of |ψ|²).
// Radial part: e^(−ρ/n) · L(2ρ/n), L = generalised Laguerre polynomial L_(n−l−1)^(2l+1), ρ = Z·r.
// The angular polynomial holds the r^l factor (the real "cubic harmonics": pz = z, dxy = xy, fxyz = xyz ...).
struct AngDef { char letter; const char* ori; const char* lab; };
static const AngDef kAng[16] = {
    {'s', "", ""},
    {'p', "x", "x"}, {'p', "y", "y"}, {'p', "z", "z"},
    {'d', "z2", "z²"}, {'d', "xz", "xz"}, {'d', "yz", "yz"}, {'d', "xy", "xy"}, {'d', "x2y2", "x²−y²"},
    {'f', "z3", "z³"}, {'f', "xz2", "xz²"}, {'f', "yz2", "yz²"}, {'f', "xyz", "xyz"}, {'f', "zx2y2", "z(x²−y²)"},
    {'f', "xx23y2", "x(x²−3y²)"}, {'f', "y3x2y2", "y(3x²−y²)"},
};
double angular(int a, double x, double y, double z) {
    const double r2 = x * x + y * y + z * z;
    switch (a) {
        case 0: return 1;
        case 1: return x;
        case 2: return y;
        case 3: return z;
        case 4: return 3 * z * z - r2;
        case 5: return x * z;
        case 6: return y * z;
        case 7: return x * y;
        case 8: return x * x - y * y;
        case 9: return z * (5 * z * z - 3 * r2);
        case 10: return x * (5 * z * z - r2);
        case 11: return y * (5 * z * z - r2);
        case 12: return x * y * z;
        case 13: return z * (x * x - y * y);
        case 14: return x * (x * x - 3 * y * y);
        default: return y * (3 * x * x - y * y);
    }
}
// L_k^a(x) = Σ (−1)^i C(k+a, k−i) x^i / i!
std::vector<double> lagCoef(int k, double a) {
    std::vector<double> c;
    for (int i = 0; i <= k; i++) {
        double bin = 1;
        for (int j = 1; j <= k - i; j++) bin *= (a + i + j) / j;
        double f = 1;
        for (int j = 2; j <= i; j++) f *= j;
        c.push_back((i % 2 ? -1 : 1) * bin / f);
    }
    return c;
}
const std::vector<Orbital>& orbitals() {
    static std::vector<Orbital> v = [] {
        std::vector<Orbital> out;
        const char* letters = "spdf";
        for (int n = 1; n <= 6; n++)                       // 0.46 (B1): to n = 6, for the jumps of H-gamma and H-delta; l up to 3 (s p d f): no g or h orbitals (no line the lab draws needs them)
            for (int l = 0; l < std::min(n, 4); l++)
                for (int a = 0; a < 16; a++) {
                    if (kAng[a].letter != letters[l]) continue;
                    Orbital o;
                    o.n = n; o.l = l;
                    o.shell = std::to_string(n) + letters[l];
                    o.ori = kAng[a].ori;
                    o.key = o.shell + "|" + o.ori;
                    o.lab = o.shell + (*kAng[a].lab ? std::string("<sub>") + kAng[a].lab + "</sub>" : "");
                    o.txt = o.shell + kAng[a].lab;
                    o.lag = lagCoef(n - l - 1, 2 * l + 1);
                    o.sgn = (n - l - 1) % 2 ? -1 : 1;   // the sign is a free choice: the outermost part of every orbital is +
                    o.ang = a;
                    out.push_back(o);
                }
        return out;
    }();
    return v;
}
int orbIndex(const std::string& key) {
    const auto& v = orbitals();
    for (std::size_t i = 0; i < v.size(); i++) if (v[i].key == key) return static_cast<int>(i);
    for (std::size_t i = 0; i < v.size(); i++) if (v[i].shell == key) return static_cast<int>(i);   // a shell: its first orientation
    return -1;
}
double radial(const Orbital& o, double q) {
    const double x = 2 * q / o.n;
    double L = 0;
    for (int i = static_cast<int>(o.lag.size()) - 1; i >= 0; i--) L = L * x + o.lag[i];
    return o.sgn * L * std::exp(-q / o.n);
}
double psi(int orb, int Z, double x, double y, double z) {
    const Orbital& o = orbitals()[orb < 0 ? 0 : orb];
    return radial(o, Z * std::hypot(x, y, z)) * angular(o.ang, x, y, z) * std::pow(static_cast<double>(Z), o.l);
}
double psi(const std::string& key, int Z, double x, double y, double z) { return psi(orbIndex(key), Z, x, y, z); }

// ---- complex orbitals (see atom_core.h) ----
// For each l and |m|: which real angular polynomials are Re and Im (indices into angular()), and c.
struct ComplexPair { int l, m, re, im; double c; };
static const ComplexPair kPairs[] = {
    {1, 1, 1, 2, 1},                       // x, y
    {2, 1, 5, 6, 1}, {2, 2, 8, 7, 2},      // xz, yz;  x2-y2, xy
    {3, 1, 10, 11, 1}, {3, 2, 13, 12, 2}, {3, 3, 14, 15, 1},   // xz2, yz2;  z(x2-y2), xyz;  x(x2-3y2), y(3x2-y2)
};
bool complexPairOf(int l, int absM, int& re, int& im, double& c) {
    for (const ComplexPair& p : kPairs) if (p.l == l && p.m == absM) { re = p.re; im = p.im; c = p.c; return true; }
    return false;
}
std::complex<double> angularComplex(int l, int m, double x, double y, double z) {
    const int am = m < 0 ? -m : m;
    for (const ComplexPair& p : kPairs) {
        if (p.l != l || p.m != am) continue;
        const std::complex<double> v(angular(p.re, x, y, z), (m < 0 ? -1.0 : 1.0) * p.c * angular(p.im, x, y, z));
        return v * ((m > 0 && (am % 2)) ? -1.0 : 1.0) / std::sqrt(2.0);
    }
    return 0;
}
int complexState(int shellOrb, int m) {
    if (shellOrb < 0 || shellOrb >= static_cast<int>(orbitals().size()) || m == 0) return -1;
    const int l = orbitals()[shellOrb].l;
    if (std::abs(m) > l) return -1;
    return kComplexBase + shellOrb * 16 + (m + 8);
}
bool isComplexState(int id) { return id >= kComplexBase; }
int stateOrbital(int id) { return isComplexState(id) ? (id - kComplexBase) / 16 : id; }
int stateM(int id) { return isComplexState(id) ? (id - kComplexBase) % 16 - 8 : 0; }
bool oriIsComplex(const std::string& ori, int& m) {
    if (ori.size() != 3 || ori[0] != 'm' || (ori[1] != '+' && ori[1] != '-') || ori[2] < '1' || ori[2] > '3') return false;
    m = (ori[1] == '-' ? -1 : 1) * (ori[2] - '0');
    return true;
}
std::string oriKey(int m) { return std::string("m") + (m < 0 ? "-" : "+") + std::to_string(std::abs(m)); }
std::complex<double> psiComplex(int id, int Z, double x, double y, double z) {
    const Orbital& o = orbitals()[stateOrbital(id)];
    return radial(o, Z * std::hypot(x, y, z)) * angularComplex(o.l, stateM(id), x, y, z) * std::pow(static_cast<double>(Z), o.l);
}
double density(int id, int Z, double x, double y, double z) {
    if (!isComplexState(id)) { const double p = psi(id, Z, x, y, z); return p * p; }
    return std::norm(psiComplex(id, Z, x, y, z));
}
// ---- quantum beats (see atom_core.h) ----
double levelEV(int Z, int N, int n) { return -rydM(Z, N) * Z * Z / (double(n) * n); }
double beatEV(int Z, int N, const Superposition& s) { return levelEV(Z, N, orbitals()[stateOrbital(s.idB)].n) - levelEV(Z, N, orbitals()[stateOrbital(s.idA)].n); }
double beatPeriodS(int Z, int N, const Superposition& s) { const double d = std::fabs(beatEV(Z, N, s)); return d == 0 ? INFINITY : kPlanckEVs / d; }
double normFactor(int id, int Z) {
    static std::map<int, double> cache;                                    // at Z = 1; psi(Z) is psi(1) with lengths divided by Z, so its integral is the Z = 1 one over Z^3
    auto it = cache.find(id);
    if (it == cache.end()) it = cache.emplace(id, 1 / std::sqrt(orbitalNorm(id))).first;
    return it->second * std::pow(double(Z), 1.5);
}
// <A|B> for two normalised states. Every state is (radial part of its shell) x (angular part), so <A|B> is zero for different n or l (the radial parts, or the spherical harmonics, are orthogonal:
// textbook, Griffiths ch. 4 / Bethe & Salpeter, the sources already in About) and, within one shell (same n and l), the overlap of the normalised angular parts over the sphere. That integral is done
// with 16 Gauss-Legendre points in cos(theta) and 32 in phi (exact for the polynomials of degree <= 6 in x, y, z met here). Values below 1e-9 are rounding of an exact 0 and are returned as exactly 0, so
// orthogonal pairs take exactly the same numbers as before this function existed.
std::complex<double> overlapAB(int idA, int idB) {
    if (idA == idB) return 1.0;
    const Orbital &oa = orbitals()[stateOrbital(idA)], &ob = orbitals()[stateOrbital(idB)];
    if (oa.n != ob.n || oa.l != ob.l) return 0.0;
    static std::map<std::pair<int, int>, std::complex<double>> cache;
    const auto it = cache.find({idA, idB});
    if (it != cache.end()) return it->second;
    static std::vector<double> zs, ws;
    if (zs.empty()) {                                                      // Gauss-Legendre nodes by Newton's method on P_16
        const int nu = 16;
        for (int i = 0; i < nu; i++) {
            double x = std::cos(kPi * (i + 0.75) / (nu + 0.5)), dp = 1;
            for (int k = 0; k < 100; k++) {
                double p0 = 1, p1 = x;
                for (int j = 2; j <= nu; j++) { const double p2 = ((2 * j - 1) * x * p1 - (j - 1) * p0) / j; p0 = p1; p1 = p2; }
                dp = nu * (x * p1 - p0) / (x * x - 1);
                const double dx = p1 / dp; x -= dx; if (std::fabs(dx) < 1e-15) break;
            }
            zs.push_back(x); ws.push_back(2 / ((1 - x * x) * dp * dp));
        }
    }
    const auto ang = [](int id, double x, double y, double z) {
        const Orbital& o = orbitals()[stateOrbital(id)];
        return isComplexState(id) ? angularComplex(o.l, stateM(id), x, y, z) : std::complex<double>(angular(o.ang, x, y, z), 0);
    };
    std::complex<double> ab = 0; double aa = 0, bb = 0;
    const int nphi = 32;
    for (std::size_t i = 0; i < zs.size(); i++) {
        const double z = zs[i], r = std::sqrt(std::max(0.0, 1 - z * z));
        for (int k = 0; k < nphi; k++) {
            const double f = 2 * kPi * (k + 0.5) / nphi, x = r * std::cos(f), y = r * std::sin(f);
            const std::complex<double> A = ang(idA, x, y, z), B = ang(idB, x, y, z);
            ab += std::conj(A) * B * ws[i]; aa += std::norm(A) * ws[i]; bb += std::norm(B) * ws[i];
        }
    }
    std::complex<double> v = ab / std::sqrt(aa * bb);
    if (std::abs(v) < 1e-9) v = 0.0;
    cache[{idA, idB}] = v;
    return v;
}
double superNorm2(const Superposition& s, double theta) {
    const std::complex<double> S = overlapAB(s.idA, s.idB);
    if (S == 0.0) return s.a * s.a + s.b * s.b;
    return s.a * s.a + s.b * s.b + 2 * s.a * s.b * (S * std::exp(std::complex<double>(0, -theta))).real();
}
static std::complex<double> amp(int id, int Z, double x, double y, double z) {
    return normFactor(id, Z) * (isComplexState(id) ? psiComplex(id, Z, x, y, z) : std::complex<double>(psi(id, Z, x, y, z), 0));
}
std::complex<double> psiSuper(const Superposition& s, int Z, double theta, double x, double y, double z) {
    const std::complex<double> v = s.a * amp(s.idA, Z, x, y, z) + s.b * amp(s.idB, Z, x, y, z) * std::exp(std::complex<double>(0, -theta));
    if (overlapAB(s.idA, s.idB) == 0.0) return v;                          // orthogonal states (the usual case): a^2 + b^2 = 1 already, the numbers are exactly the old ones
    return v / std::sqrt(std::max(1e-300, superNorm2(s, theta)));          // overlapping states (same state twice, a real orbital with an m state of its shell): renormalised
}
std::complex<double> psiSuperT(const Superposition& s, int Z, int N, double tSeconds, double x, double y, double z) {
    return psiSuper(s, Z, beatEV(Z, N, s) * tSeconds / (kPlanckEVs / (2 * kPi)), x, y, z);
}
bool dipoleAxes(const Superposition& s, double out[3]) {              // the angular integral of conj(A) (x, y, z) B over the sphere (1,500 evenly spread points, as dipoleOK)
    static std::vector<std::array<double, 3>> sphere = [] {
        std::vector<std::array<double, 3>> P;
        const int N = 1500;
        const double g = kPi * (3 - std::sqrt(5.0));
        for (int i = 0; i < N; i++) { const double y = 1 - 2 * (i + 0.5) / N, r = std::sqrt(1 - y * y), a = g * i; P.push_back({r * std::cos(a), y, r * std::sin(a)}); }
        return P;
    }();
    const auto ang = [](int id, double x, double y, double z) {
        const Orbital& o = orbitals()[stateOrbital(id)];
        return isComplexState(id) ? angularComplex(o.l, stateM(id), x, y, z) : std::complex<double>(angular(o.ang, x, y, z), 0);
    };
    std::complex<double> acc[3]; double na = 0, nb = 0;
    for (const auto& p : sphere) {                                    // (the sphere's own axes: its y is the z axis here, so map back: x, y, z = p[0], p[2], p[1])
        const double x = p[0], y = p[2], z = p[1];
        const std::complex<double> A = ang(s.idA, x, y, z), B = ang(s.idB, x, y, z);
        acc[0] += std::conj(A) * x * B; acc[1] += std::conj(A) * y * B; acc[2] += std::conj(A) * z * B;
        na += std::norm(A); nb += std::norm(B);
    }
    const double scale = std::sqrt(na * nb + 1e-30);
    double best = 0;
    for (int i = 0; i < 3; i++) { out[i] = std::abs(acc[i]) / scale; best = std::max(best, out[i]); }
    return best > 1e-3;
}
std::string stateText(int id) {
    const Orbital& o = orbitals()[stateOrbital(id)];
    if (!isComplexState(id)) return o.txt;
    const int m = stateM(id);
    return o.shell + " (m = " + (m < 0 ? "−" : "+") + std::to_string(std::abs(m)) + ")";
}

// The dipole rule: light connects two orbitals only if the angular integral of
// (orbital A) × (x, y or z) × (orbital B) over a sphere is not zero. Worked out numerically on
// 0.46.1: the squared angular dipole element between two real orbitals, sum over x, y, z of <A|r_c/r|B>^2 (normalised angular parts). For one A, the sum over every orientation B of a shell
// l' is l_> / (2 l_A + 1) (l_> the larger of l_A, l'), the textbook sum rule the tests check. The odds of a decay A -> B are then A(n l -> n' l') x this x (2 l_A + 1) / l_>.
// Worked out on a finer Fibonacci sphere than dipoleOK (20,000 points), cached.
double dipoleAngularSq(int a, int b) {
    static const std::vector<std::array<double, 3>> sphere = [] {
        std::vector<std::array<double, 3>> P;
        const int N = 20000;
        const double g = kPi * (3 - std::sqrt(5.0));
        for (int i = 0; i < N; i++) {
            const double z = 1 - 2 * (i + 0.5) / N, r = std::sqrt(1 - z * z);
            P.push_back({r * std::cos(g * i), r * std::sin(g * i), z});
        }
        return P;
    }();
    static std::map<std::pair<int, int>, double> cache;
    if (const auto it = cache.find({a, b}); it != cache.end()) return it->second;
    const int A = orbitals()[std::size_t(a)].ang, B = orbitals()[std::size_t(b)].ang;
    double c3[3] = {0, 0, 0}, na = 0, nb = 0;
    for (const auto& v : sphere) {
        const double fa = angular(A, v[0], v[1], v[2]), fb = angular(B, v[0], v[1], v[2]);
        for (int c = 0; c < 3; c++) c3[c] += fa * v[std::size_t(c)] * fb;
        na += fa * fa; nb += fb * fb;
    }
    const double r = (c3[0] * c3[0] + c3[1] * c3[1] + c3[2] * c3[2]) / (na * nb + 1e-300);
    return cache[{a, b}] = cache[{b, a}] = r;
}

bool dipoleOK(int a, int b) {
    static std::vector<std::array<double, 3>> sphere = [] {
        std::vector<std::array<double, 3>> P;
        const int N = 1500;
        const double g = kPi * (3 - std::sqrt(5.0));
        for (int i = 0; i < N; i++) {
            const double z = 1 - 2 * (i + 0.5) / N, r = std::sqrt(1 - z * z);
            P.push_back({r * std::cos(g * i), r * std::sin(g * i), z});
        }
        return P;
    }();
    static std::map<std::pair<int, int>, bool> cache;
    const auto it = cache.find({a, b});
    if (it != cache.end()) return it->second;
    const int A = orbitals()[a].ang, B = orbitals()[b].ang;
    double best = 0, na = 0, nb = 0;
    for (int c = 0; c < 3; c++) {
        double sum = 0;
        for (const auto& v : sphere) sum += angular(A, v[0], v[1], v[2]) * v[c] * angular(B, v[0], v[1], v[2]);
        best = std::max(best, std::fabs(sum));
    }
    for (const auto& v : sphere) {
        na += std::pow(angular(A, v[0], v[1], v[2]), 2);
        nb += std::pow(angular(B, v[0], v[1], v[2]), 2);
    }
    const bool ok = best / std::sqrt(na * nb + 1e-30) > 1e-3;
    cache[{a, b}] = cache[{b, a}] = ok;
    return ok;
}

// ---------------------------------------------------------------- light
// The electron and the nucleus both circle their common centre: the Rydberg energy with the reduced mass, M / (M + 1) times the infinite-mass value, M = the nucleus's mass in electron masses.
// M = Z m_p + N m_n - B / c² (0.10.6 stage 6; it was A u - Z m_e, which ignores that a nucleus weighs less than its parts by B/c² AND that A u is not the mass of A nucleons: for H-2 it was 0.70 % low, for
// H-3 0.53 %, He-4 0.065 %, Fe-56 0.12 % high; the old comment "its binding energy changes this by under 10⁻⁶" was wrong: the effect on the Rydberg energy is 1.9e-6 for H-2, i.e. 1.3 pm on D-alpha).
//  m_p and m_n: the CODATA 2022 masses in atom_core.h. B: the lab's binding table (massBinding), the total nuclear binding energy from the measured ATOMIC masses, in whole keV per nucleon (AME2020: all 3,557
//  nuclides of the lab's table agree with the AME2020 atomic masses, https://www-nds.iaea.org/amdc/ame2020/mass.mas20.txt, read 2026-10-04: M within 5.4e-7 of AME's in the worst case, 2.6e-7 typically; 1,008 of
//  them are AME's own extrapolated values, marked # there, which are as sourced as the rest). B comes from atomic masses, so it includes the electrons' binding energies, which are not taken out here:
//  that and the rounding to 1 keV per nucleon are the 5e-7 (and the Rydberg energy moves by that over M, below 1e-9). A nuclide the table does not hold (the neutron slider reaches 3 beyond the known
//  ones) keeps the old form A u - Z m_e, because no mass is known for it: the error of that form is the 1e-3-ish above for light nuclei, 1e-6 or less for the Rydberg energy.
double nuclearMassMe(int Z, int N) {
    const Binding b = massBinding(Z, N);
    if (!b.real) return (Z + N) * kMeU - Z;
    return Z * kMpOverMe + N * (kMnU / kMeInU) - b.B * 1e6 / kMecEV;       // B in MeV -> eV; m_n in electron masses from the neutron mass in u and the electron mass in u
}
double rydM(int Z, int N) {
    const double M = nuclearMassMe(Z, N);
    return kRydInf * M / (M + 1);
}
const char* const kSeries[6] = {"", "Lyman", "Balmer", "Paschen", "Brackett", "Pfund"};
// every line with the lower level 1 to 5 and the upper level up to 20 above it, plus each series limit (nm, vacuum)
std::vector<SpecLine> specLines(int Z, int N) {
    const double R = rydM(Z, N) * Z * Z;
    std::vector<SpecLine> out;
    for (int lo = 1; lo <= 5; lo++) {
        for (int hi = lo + 1; hi <= lo + 20; hi++) out.push_back({lo, hi, kHcEVnm / (R * (1.0 / (lo * lo) - 1.0 / (hi * hi)))});
        out.push_back({lo, 0, kHcEVnm / (R / (lo * lo))});
    }
    return out;
}
// wavelength (nm) -> colour; UV and IR, which the eye cannot see, are one grey
// 0.52.1: colorimetric: the CIE 1931 2-degree observer -> XYZ -> linear sRGB (IEC 61966-2-1) -> sRGB, the app's one CIE table (render/cie1931.h).
// A single wavelength lies outside what a screen can show: D65 white is mixed in until no channel is negative (the dominant wavelength stays the
// line's), then the colour is scaled to its brightest displayable value (largest channel 255). Until 0.52.0: Dan Bruton's piecewise-linear approximation.
Rgb lamColor(double l) {
    if (l < kVisibleLoNm || l > kVisibleHiNm) return {0x9a, 0x9a, 0xa3};
    const cie::V3 v = cie::spectralLinear(l);
    auto c = [&](double x) { return static_cast<int>(roundHalfUp(255 * cie::srgbEncode(x))); };
    return {c(v[0]), c(v[1]), c(v[2])};
}
std::string lamColorCss(double l) {
    if (l < kVisibleLoNm || l > kVisibleHiNm) return "#9a9aa3";
    const Rgb c = lamColor(l);
    return "rgb(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")";
}
std::string vacuumBand(double nm) { const std::string t = bandTag(nm); return t.empty() ? std::string("vacuum") : "vacuum, " + t.substr(2, t.size() - 3); }   // 0.46.1: "vacuum, UV"
const char* bandTag(double nm) { return nm < 10 ? " (X-ray)" : nm < kVisibleLoNm ? " (UV)" : nm > kVisibleHiNm ? " (IR)" : ""; }
std::vector<SpecLine> visibleLines(int Z, int N) { std::vector<SpecLine> v; for (const SpecLine& l : specLines(Z, N)) if (l.hi != 0 && isVisibleNm(l.lam)) v.push_back(l); return v; }
double nuclearR(int A) { return A == 1 ? kNucleonR : 1.2 * std::cbrt(static_cast<double>(A)); }   // r = 1.2 A^(1/3) fm

// ---------------------------------------------------------------- the nucleus
static const BetaKnown* betaKnown(int Z, int A) {
    for (const BetaKnown& b : kBetaKnown) if (b.Z == Z && b.A == A) return &b;
    return nullptr;
}
// 0.46.3 (outside review: 88 % of the shapes came from the lab's own unpublished curve): FRDM(2012)'s calculated ground-state beta2 (frdm_data.inc, tools/make_frdm_data.py)
struct FrdmRow { short Z, A; float beta2; };
static const FrdmRow kFrdm[] = {
#include "frdm_data.inc"
};
static const FrdmRow* frdmRow(int Z, int A) {
    const FrdmRow* e = std::end(kFrdm);
    const FrdmRow* p = std::lower_bound(std::begin(kFrdm), e, std::make_pair(Z, A), [](const FrdmRow& r, const std::pair<int, int>& k) { return r.Z != k.first ? r.Z < k.first : r.A < k.second; });
    return p != e && p->Z == Z && p->A == A ? p : nullptr;
}
bool nucFrdmKnown(int Z, int N) { return frdmRow(Z, Z + N) != nullptr; }
double nucFrdmBeta(int Z, int N) { const FrdmRow* r = frdmRow(Z, Z + N); return r ? r->beta2 : NAN; }
double nucBE2Beta(int Z, int N);
// quadrupole deformation β₂: + stretched (rugby ball), − flattened (pancake). 0.46.3: the measured B(E2) where ENSDF has one, else FRDM(2012), else round (no published value)
double nucBeta(int Z, int N) {
    const int A = Z + N;
    const FrdmRow* r = frdmRow(Z, A);
    if (const BetaKnown* k = betaKnown(Z, A)) {
        // B(E2) gives the size of the deformation, static and vibrating together. It is drawn as a fixed shape only where FRDM(2012) calculates a deformed ground state
        // (|beta2| >= 0.10, quadrupole.h kDeformedCoreBeta: the lab's own threshold) or has no value; where FRDM calls the nucleus round (Pb-208, Ca-40 ...) the B(E2) is a vibration
        // about a round shape, and the nucleus is drawn round (the card gives the measured value). 0.46.3.
        if (r && std::fabs(r->beta2) < 0.10) return 0;
        const double b = nucBE2ShapeBeta(Z, A - Z);
        if (std::isfinite(b)) return b;
        return r ? r->beta2 : 0;                        // the moment would need |beta2| > 0.6 (quadrupole.h kShapeBetaLimit): FRDM's, else round (the card says so)
    }
    if (r) return r->beta2;
    return 0;
}
double nucBE2Beta(int Z, int N) {   // β₂ = ±4π / (3 Z R0²) · √B(E2↑), R0 = 1.2 A^(1/3) fm (the usual relation, e.g. Raman, Nestor, Tikkanen, ADNDT 78 (2001) 1); NaN without a B(E2)
    const BetaKnown* k = betaKnown(Z, Z + N);
    if (!k) return NAN;
    const double R0 = 1.2 * std::cbrt(static_cast<double>(Z + N));
    return k->sign * 4 * kPi / (3 * Z * R0 * R0) * std::sqrt(k->be2 * 1e4);
}
// 0.46.4 (outside review: Raman's first-order beta2, which assumes a sharp sphere of radius 1.2 A^(1/3) fm, was put into the lab's exact volume-keeping body sized by the measured
// radius, so the drawn intrinsic moment came out 7 % (Th-232) to 30 % (Ne-20) larger than the B(E2) gives, and Ne-20 was drawn at 0.71, past the 0.6 limit): the B(E2) is turned into the
// intrinsic moment of a rigid rotor, Q0 = sqrt(16 pi B(E2; 0+ -> 2+) / 5) / e (K = 0; the relation Raman, Nestor and Tikkanen use, ADNDT 78 (2001) 1), and Q0 into beta2 exactly as a measured
// moment is (quadrupole.h betaFromRatio with the protons' point mean square radius). NaN without a B(E2), or when it would need |beta2| > kShapeBetaLimit.
double nucBE2Q0Fm2(int Z, int N) { const BetaKnown* k = betaKnown(Z, Z + N); return k ? k->sign * std::sqrt(16 * kPi / 5 * k->be2) * 100 : NAN; }   // e b^2 -> e fm^2: x 100
double nucBE2ShapeBeta(int Z, int N) {
    const double q0 = nucBE2Q0Fm2(Z, N);
    if (!std::isfinite(q0) || Z < 1) return NAN;
    bool ok = false;
    const double b = betaFromRatio(q0 / (Z * pointProtonMs(Z, N)), ok);
    return ok ? b : NAN;
}
bool nucBE2Known(int Z, int N) { return betaKnown(Z, Z + N) != nullptr; }   // the lab's own table of measured B(E2; 0+ -> 2+) values has this isotope
double nucBE2Up(int Z, int N) { const BetaKnown* k = betaKnown(Z, Z + N); return k ? k->be2 : NAN; }   // e² b²; NaN when the table has none
int nucBE2SignSource(int Z, int N) { const BetaKnown* k = betaKnown(Z, Z + N); return k ? k->signSrc : 0; }
bool nucMeasured(int Z, int N) { return betaKnown(Z, Z + N) != nullptr; }
std::string nucleonBallNote() {
    return "each nucleon is drawn as a ball of the proton's charge radius, 0.84 fm, with the centres kept at least 1.0 fm apart, so the balls overlap: a drawing choice (heavy-ion Glauber codes "
           "keep 0.4 fm; real nucleons are not hard balls)";
}
const char* nucShapeWord(double b) {
    return b >= 0.15 ? "stretched like a rugby ball" : b <= -0.15 ? "flattened like a pancake"
         : b >= 0.07 || b <= -0.07 ? "slightly deformed" : "nearly round";
}

// ---------------------------------------------------------------- 0.46.5: the deuteron's wave function
// CD-Bonn (Machleidt, PRC 63, 024001 (2001), arXiv:nucl-th/0006014, saved in data/qcd/nucleus), appendix D: u(r) = sum C_j exp(-m_j r), w(r) = sum D_j exp(-m_j r) (1 + 3/(m_j r) + 3/(m_j r)^2),
// m_j = gamma + (j - 1) m0, gamma = 0.2315380 fm^-1 (eq. D6), m0 = 0.9 fm^-1, n = 11 (eqs. D19-D20, D25); C_1..C_10 and D_1..D_8 from Table XX, C_11 and D_9..D_11 from the conditions
// u ~ r, w ~ r^3 at r -> 0 (eqs. D23-D24, worked out in double precision as the paper asks). It reproduces the paper's Table XIX to about 1e-4 and Table XV's r_d 1.966 fm, Q_d 0.270 fm^2
// (without meson currents; measured 0.2859 fm^2) and P_D 4.85 % (tests/test_atom0465.inc). A realistic NN potential: its D wave (the tensor force) makes the shapes below.
namespace {
struct CdBonn {
    double m[11], C[11], D[11];
    CdBonn() {
        const double g = 0.2315380, m0 = 0.9;
        for (int j = 0; j < 11; j++) m[j] = g + j * m0;
        const double c[10] = {0.88472985, -0.26408759, -0.44114404e-1, -0.14397512e2, 0.85591256e2, -0.31876761e3, 0.70336701e3, -0.90049586e3, 0.66145441e3, -0.25958894e3};
        const double d[8] = {0.22623762e-1, -0.50471056, 0.56278897, -0.16079764e2, 0.11126803e3, -0.44667490e3, 0.10985907e4, -0.16114995e4};
        double cs = 0;
        for (int j = 0; j < 10; j++) { C[j] = c[j]; cs += c[j]; }
        C[10] = -cs;                                                     // eq. D23
        double s1 = 0, s2 = 0, s3 = 0;
        for (int j = 0; j < 8; j++) { D[j] = d[j]; s1 += d[j] / (m[j] * m[j]); s2 += d[j]; s3 += d[j] * m[j] * m[j]; }
        const auto Dk = [&](int k, int a, int b) {                       // eq. D24 and its two circular permutations
            const double mk = m[k] * m[k], ma = m[a] * m[a], mb = m[b] * m[b];
            return mk / ((ma - mk) * (mb - mk)) * (-ma * mb * s1 + (ma + mb) * s2 - s3);
        };
        D[8] = Dk(8, 9, 10); D[9] = Dk(9, 10, 8); D[10] = Dk(10, 8, 9);
    }
};
const CdBonn& cdBonn() { static const CdBonn c; return c; }
// the radial chances: u^2 + w^2 on a grid of 0.005 fm out to 40 fm (1.2e-8 of the pair lies beyond), and its running sum
struct DeuteronTable {
    static constexpr double kStep = 0.005, kMax = 40;
    std::vector<double> cdf, g;                                          // g: the angular term's size, (sqrt(2) u w - w^2/2) / (u^2 + w^2)
    double ms = 0;                                                       // <s^2>, fm^2 (4 r_d^2)
    DeuteronTable() {
        const int n = int(kMax / kStep);
        cdf.assign(n + 1, 0.0); g.assign(n + 1, 0.0);
        double sum = 0, m2 = 0;
        for (int i = 1; i <= n; i++) {
            const double r = (i - 0.5) * kStep, u = deuteronU(r), w = deuteronW(r), p = u * u + w * w;
            sum += p * kStep; m2 += r * r * p * kStep; cdf[i] = sum;
            const double re = i * kStep, ue = deuteronU(re), we = deuteronW(re), pe = ue * ue + we * we;
            g[i] = pe > 0 ? (std::sqrt(2.0) * ue * we - 0.5 * we * we) / pe : 0;
        }
        for (double& c : cdf) c /= sum;
        ms = m2 / sum;
    }
};
const DeuteronTable& deuteronTable() { static const DeuteronTable t; return t; }
}  // namespace
double deuteronU(double r) {
    const CdBonn& c = cdBonn();
    double s = 0;
    for (int j = 0; j < 11; j++) s += c.C[j] * std::exp(-c.m[j] * r);
    return s;
}
double deuteronW(double r) {
    const CdBonn& c = cdBonn();
    if (r < 1e-3) r = 1e-3;                                              // w ~ r^3: below 0.001 fm the cancelling terms lose their digits (and w is ~1e-9 there)
    double s = 0;
    for (int j = 0; j < 11; j++) { const double x = c.m[j] * r; s += c.D[j] * std::exp(-x) * (1 + 3 / x + 3 / (x * x)); }
    return s;
}
double deuteronFarShare(double r) {
    const DeuteronTable& t = deuteronTable();
    const double i = r / t.kStep;
    if (i <= 0) return 1;
    if (i >= double(t.cdf.size() - 1)) return 0;
    const int k = int(i);
    return 1 - (t.cdf[k] + (t.cdf[k + 1] - t.cdf[k]) * (i - k));
}
// |psi_M|^2 summed over the two spins: (u^2 + w^2 + k P2(cos theta) (sqrt(2) u w - w^2/2)) / (4 pi r^2), k = 0 unpolarised (the three M mixed), +1 for M = +-1, -2 for M = 0
// (from <S12> = 2 P2 (M = +-1) and -4 P2 (M = 0) in the spin-1 states; the same as Forest et al., PRC 54, 646 (1996), eqs. 3.3-3.6). So r follows u^2 + w^2 in every state and the
// angle, about the spin axis z, follows 1 + k P2 g(r): M = +-1 piles the pair up along the axis (a dumbbell), M = 0 round it (a doughnut).
void deuteronSeparation(int spin, const std::function<double()>& uni, double& x, double& y, double& z) {
    const DeuteronTable& t = deuteronTable();
    const double v = uni();
    const auto it = std::lower_bound(t.cdf.begin(), t.cdf.end(), v);
    const int k = std::max(1, int(it - t.cdf.begin()));
    const double c0 = t.cdf[k - 1], c1 = t.cdf[k], f = c1 > c0 ? (v - c0) / (c1 - c0) : 0.5;
    const double r = (k - 1 + f) * t.kStep;
    const double kk = spin == 1 ? 1.0 : spin == 2 ? -2.0 : 0.0, gr = t.g[k - 1] + (t.g[k] - t.g[k - 1]) * f, bound = 1 + std::fabs(kk * gr);
    double c = 0;
    for (;;) {                                                           // the angle by rejection (1 + k P2 g >= 0 for every r: the density is never negative)
        c = 2 * uni() - 1;
        if (uni() * bound <= 1 + kk * 0.5 * (3 * c * c - 1) * gr) break;
    }
    const double ph = 2 * kPi * uni(), sn = std::sqrt(std::max(0.0, 1 - c * c));
    x = r * sn * std::cos(ph); y = r * sn * std::sin(ph); z = r * c;
}
bool nucleusHoCharge(int Z, int N, double& a, double& alpha) {
    // rho = rho0 (1 + alpha r^2/a^2) exp(-r^2/a^2): de Vries, de Jager, de Vries, ADNDT 36, 495 (1987), not reached; quoted by Shukla (arXiv:nucl-th/0302030, Table I: C-12 1.082 / 1.692 fm,
    // O-16 1.544 / 1.833 fm) and, for O-16, by Loizides et al. (arXiv:1408.2549, Table I "Oho"): both saved in data/qcd/nucleus
    if (Z == 6 && N == 6) { a = 1.692; alpha = 1.082; return true; }
    if (Z == 8 && N == 8) { a = 1.833; alpha = 1.544; return true; }
    return false;
}
// the HO shape's radius drawn: r^2 exp(-r^2/a^2) (3 Gaussians' length) or r^4 exp(-r^2/a^2) (5 Gaussians' length), mixed 1 : 1.5 alpha (their integrals)
static double hoRadius(double a, double alpha, const std::function<double()>& gs, const std::function<double()>& uni) {
    const int k = uni() * (1 + 1.5 * alpha) < 1 ? 3 : 5;
    double s = 0;
    for (int i = 0; i < k; i++) { const double v = gs(); s += v * v; }
    return a / std::sqrt(2.0) * std::sqrt(s);
}
// the measured rms charge radius the Density look is scaled to (IAEA; CODATA for the deuteron and the proton), else the rule ball plus the nucleons' own size
static double chargeRmsWant(int Z, int N) {
    const RadiusRow* row = chargeRadius(Z, N);
    return row && std::isfinite(row->rms) && row->rms > 0 ? row->rms : std::sqrt(pointProtonMs(Z, N) + nucleonSizeMs(Z, N));
}
double nucEdgeFm(int Z, int N) {
    const int A = Z + N;
    if (Z == 0 || A == 2) return NAN;                                    // a lone neutron draws no charge; the deuteron has a long halo, no edge
    const double rms = chargeRmsWant(Z, N), a = kWsDiffuseFm;
    double ha = 0, hal = 0;
    const bool ho = nucleusHoCharge(Z, N, ha, hal);
    if (ho) ha *= rms / (ha * std::sqrt((std::tgamma(2.5) + hal * std::tgamma(3.5)) / (std::tgamma(1.5) + hal * std::tgamma(2.5))));   // scaled to the measured rms, as the dots are
    const double c = A > 16 ? std::sqrt(std::max(1.0, 5.0 / 3.0 * (rms * rms - 7.0 / 5.0 * kPi * kPi * a * a))) : 0;
    const auto f = [&](double r) {
        if (A == 1) return std::exp(-r / (rms / std::sqrt(12.0)));
        if (ho) return (1 + hal * r * r / (ha * ha)) * std::exp(-r * r / (ha * ha));
        if (A <= 16) return std::exp(-r * r * 3 / (2 * rms * rms));
        return 1 / (1 + std::exp((r - c) / a));
    };
    const double dr = 1e-4;
    double top = 0, rTop = 0;
    for (double r = 0; r < 4 * rms + 10 * a; r += 1e-3) if (f(r) > top) { top = f(r); rTop = r; }
    double r90 = NAN, r10 = NAN, prev = f(rTop);
    for (double r = rTop + dr; r < 4 * rms + 20 * a && std::isnan(r10); r += dr) {
        const double v = f(r);
        if (std::isnan(r90) && v <= 0.9 * top) r90 = r - dr * (0.9 * top - v) / (prev - v);
        if (v <= 0.1 * top) r10 = r - dr * (0.1 * top - v) / (prev - v);
        prev = v;
    }
    return r10 - r90;
}

// Nucleon positions are sampled from the real density profile (Woods–Saxon, as in heavy-ion
// Glauber Monte Carlo codes; a Gaussian for light nuclei), with a hard core so nucleons do not
// overlap, a neutron skin and the measured quadrupole deformation. One possible snapshot: really,
// every nucleon is a wave spread through the nucleus. Worked out in fm, stored in metres (kFmLab).
static std::vector<Nucleon> buildNucleusImpl(int Z, int N, bool measured, bool newShape, bool tilt = true, double* axisOut = nullptr, int dspin = 0) {
    const int A = Z + N;
    std::vector<Nucleon> out;
    auto gs = [] { double u = 0; while (!u) u = random(); const double v = random(); return std::sqrt(-2 * std::log(u)) * std::cos(2 * kPi * v); };
    auto toMetres = [&] { for (Nucleon& q : out) { q.x *= kFmLab; q.y *= kFmLab; q.z *= kFmLab; } return out; };
    if (A == 1) { out.push_back({0, 0, 0, Z != 0, 0}); return out; }
    std::vector<int> types;
    for (int i = 0; i < Z; i++) types.push_back(1);
    for (int i = 0; i < N; i++) types.push_back(0);
    for (int i = static_cast<int>(types.size()) - 1; i > 0; i--) {   // shuffle
        const int j = static_cast<int>(std::floor(random() * (i + 1)));
        std::swap(types[i], types[j]);
    }
    double mx = 0, my = 0, mz = 0;
    if (A == 2) {                                  // the deuteron is a loose pair
        // 0.46.5: the pair's separation is drawn from the deuteron's wave function (CD-Bonn, deuteronSeparation above) in the chosen spin state, the spin axis along z before the tilt
        // (so along the axis the Density look uses). Until 0.46.4 the pair was always exactly the rms distance apart, in the x-y plane. Measured: the drawn distances are scaled so that
        // on average (d/2)^2 is the measured point-proton mean square radius, (d/2)^2 = r_d^2 - r_p^2 - r_n^2 - 3/(4 M^2) (Filin et al., PRL 124, 082501 (2020), eq. 2; CODATA 2022
        // r_d = 2.127 78(27) fm): a factor 1.005 on CD-Bonn's own 1.966 fm. By the rule: CD-Bonn as it is.
        double x, y, z;
        deuteronSeparation(dspin, [] { return random(); }, x, y, z);
        const double sc = measured ? std::sqrt(std::max(0.5, pointProtonMsFromCharge(Z, N, chargeRadius(Z, N)->rms)) / (deuteronTable().ms / 4)) : 1.0;
        x *= sc / 2; y *= sc / 2; z *= sc / 2;
        out.push_back({x, y, z, types[0] != 0, 0});
        out.push_back({-x, -y, -z, types[1] != 0, 1.7});
    } else {
    // The shape (0.10.9 stage 2). newShape: the shape of nucShape (a measured quadrupole moment where it can be used, else the lab's B(E2) value or estimate), drawn as the surface r = R f(cos theta) of quadrupole.h
    const Shape shp = newShape ? nucShape(Z, N) : Shape{};
    const double beta = newShape ? shp.beta : nucBeta(Z, N), cA = std::cbrt(static_cast<double>(A)), a = kWsPointDiffuseFm;   // 0.46 (review A8): the CENTRES' edge (point nucleons; each is drawn with its own size on top)
    const double skinRms = neutronSkinFm(Z, N);    // 0.42: r_n - r_p of the rms radii, the antiprotonic-atom fit (radii.h); until 0.41 an unsourced 1.7 (N - Z) / A as a shift of the half-density radius
    const bool gauss = A <= 16;
    // 0.46.5: C-12 and O-16 have a measured charge density of the HO form (nucleusHoCharge). It is the nucleon centres' density folded with the nucleons' own charge; that fold is taken
    // out here, treating the nucleons' charge as a Gaussian of the same mean square radius (nucleonSizeMs; a choice: the proton's is closer to exponential): a Gaussian fold keeps the
    // HO form, so the centres have a_p^2 = a^2 - a_f^2 and alpha_p = alpha a^2 / (a_p^2 - 1.5 alpha a_f^2) (worked out from the moments), a_f^2 = (2/3) nucleonSizeMs. The size is then made exact below.
    double hoA = 0, hoAl = 0;
    const bool ho = gauss && nucleusHoCharge(Z, N, hoA, hoAl);
    if (ho) {
        const double af2 = 2.0 / 3.0 * std::max(0.0, nucleonSizeMs(Z, N)), ac2 = std::max(0.5, hoA * hoA - af2);
        hoAl = hoAl * hoA * hoA / std::max(0.1, ac2 - 1.5 * hoAl * af2);
        hoA = std::sqrt(ac2);
    }
    // The size. By the rule: R0 = 1.12 A^(1/3) - 0.86 / A^(1/3), and for A <= 16 a Gaussian whose point protons have the rule ball's size (0.46, review A7: sqrt(pointProtonMs), the ball of
    // r = 1.2 A^(1/3) fm that the card names, as the Density look already used; until 0.45 an unsourced rms of 1.76 (A = 3) or 1.68 + 0.085 (A - 4) fm). Measured (0.10.9 stage 1, radii.h): the sampling starts from the measured
    // size (the protons' point mean square radius: radii.h pointProtonMsFromCharge, 0.42; the neutrons are wider by the skin, radii.h neutronSkinFm), and after the nucleons are placed and centred the whole snapshot is scaled about the
    // centre of mass so that its PROTONS' rms charge radius is the measured one EXACTLY. The scale absorbs what the sampling adds on its own: the hard core between nucleons spreads them out, re-centring on the
    // centroid pulls them in (by (A - 1) / A), and a deformed nucleus' rms grows with beta2^2. Each picture is one random snapshot (as before) with the measured size; the rule's own layout is not scaled.
    double R0 = 1.12 * cA - 0.86 / cA;
    double sg = std::sqrt(std::max(0.5, pointProtonMs(Z, N))) / std::sqrt(3.0);   // (the rule's light-nucleus size; a measured one replaces it below)
    // The snapshot is also made EXACT in its protons' quadrupole when the size is exact and the shape is not round (below): the protons' <x^2> = <y^2> and <z^2> are scaled to the values of the uniformly charged body of
    // quadrupole.h (<3 z^2 - r^2> / <r^2> = shapeMomentRatio(beta2)); the trace, so the rms radius, is unchanged. The size is exact for a measured radius, and for a shape from a measured moment (then at the rule's rms).
    const bool exact = measured || (newShape && shp.src == ShapeSource::MeasuredMoment);
    double chargeTarget = exact ? pointProtonMs(Z, N) : 0;                   // the protons' mean square radius about the centre of mass the picture must have, fm^2 (the measured rms^2 - r_p^2, or the rule ball's)
    if (measured) {
        const double m = chargeRadius(Z, N)->rms;
        chargeTarget = pointProtonMsFromCharge(Z, N, m);
        if (gauss) sg = std::sqrt(std::max(0.5, chargeTarget)) / std::sqrt(3.0);
        else R0 = wsHalfDensityRadiusForPointMs(chargeTarget);              // the protons' half-density radius
    }
    // the neutrons' half-density radius: the one whose rms is the protons' rms + skinRms (same diffuseness; <r^2> = 3/5 R^2 + 7/5 pi^2 a^2, Fermi distribution). For A <= 16 (Gaussian) no skin is drawn,
    // as until 0.41: the antiprotonic-atom rule was fitted to 26 nuclei from Ca-40 to U-238 (Trzcinska et al. 2001), and light nuclei's neutron halos (Li-11, Be-11) are not modelled.
    const double pi2a2 = 1.4 * kPi * kPi * a * a, rP = std::sqrt(0.6 * R0 * R0 + pi2a2), rN = std::max(0.5, rP + skinRms);
    const double Rn = std::sqrt(std::max(0.25, (rN * rN - pi2a2) / 0.6));
    double dmin = 1.0;   // the hard core between nucleon centres (fm): a DRAWING choice so the balls do not sit inside each other; Glauber Monte Carlo codes use 0.4 fm (Loizides, Nagle and Steinberg, arXiv:1408.2549)
    for (int i = 0; i < A; i++) {
        const bool isP = types[i] != 0;
        const double Ri = isP ? R0 : Rn, sgi = sg;
        bool found = false;
        double px = 0, py = 0, pz = 0;
        for (int tries = 0; !found; tries++) {
            double x, y, z;
            if (gauss && ho) {                       // 0.46.5: C-12, O-16: the centres from the measured HO charge density with the nucleons' own size taken out (hoA, hoAl below)
                const double r = hoRadius(hoA, hoAl, gs, [] { return random(); }), u = 2 * random() - 1, f = 2 * kPi * random(), sn = std::sqrt(1 - u * u);
                x = r * sn * std::cos(f); y = r * sn * std::sin(f); z = r * u;
                if (beta) { const double fr = shapeRadiusFactor(beta, u); x *= fr; y *= fr; z *= fr; }
            } else if (gauss) {
                x = sgi * gs(); y = sgi * gs(); z = sgi * gs();
                if (beta && !newShape) z *= 1 + beta;
                else if (beta) { const double r = std::sqrt(x * x + y * y + z * z), f = r > 0 ? shapeRadiusFactor(beta, z / r) : 1; x *= f; y *= f; z *= f; }   // the same volume-keeping surface as the heavy nuclei
            } else {
                const double Rmax = R0 + 9 * a + 1, u = random() * 2 - 1, f = random() * 6.2832, sn = std::sqrt(1 - u * u);
                const double Rth = newShape ? Ri * shapeRadiusFactor(beta, u) : Ri * (1 + beta * 0.3154 * (3 * u * u - 1));
                const double r = Rmax * std::cbrt(random());
                if (random() > 1 / (1 + std::exp((r - Rth) / a))) continue;   // Woods–Saxon: keep it with the density's odds
                x = r * sn * std::cos(f); y = r * sn * std::sin(f); z = r * u;
            }
            bool ok = true;
            for (const Nucleon& q : out) {
                const double dx = q.x - x, dy = q.y - y, dz = q.z - z;
                if (dx * dx + dy * dy + dz * dz < dmin * dmin) { ok = false; break; }
            }
            if (ok) { found = true; px = x; py = y; pz = z; }
            else if (tries > 60) { dmin = std::max(0.55, dmin * 0.93); tries = 0; }   // crowded: let them sit a little closer
        }
        out.push_back({px, py, pz, isP, i * 1.7});
    }
    mx = my = mz = 0;
    for (const Nucleon& q : out) { mx += q.x; my += q.y; mz += q.z; }
    mx /= A; my /= A; mz /= A;
    // exact: the scales that give the protons exactly the wanted mean square radius about the centre of mass (and, for a deformed nucleus, the wanted quadrupole); they move EVERY nucleon, as until 0.41.
    // 0.42: then, for A > 16, the neutrons alone get one round factor so that their rms is the protons' + the skin (radii.h neutronSkinFm) exactly. (Scaling the neutrons with the protons' factor alone
    // drew the skin too thick on average: +0.017 fm for Pb-208 and +0.04 fm for Ca-48 over 10,000 snapshots, because a proton set that comes out small by chance inflates the neutrons too.) That moves the
    // centre of mass a little, so the two steps are repeated about the new centre until they settle (12 rounds reach rounding level). The neutrons' own factor (about 1 +- 3 %) moves a surface neutron up to
    // ~0.4 fm against its proton neighbours, so pairs pushed closer than the sampling's hard core (times the first scale) are then pushed apart along their line and the scales fitted again (a few passes).
    // Light nuclei (A <= 16) keep one set of factors and no skin (above).
    if (exact) {
        const double rP = std::sqrt(std::max(0.0, chargeTarget)), msN = (rP + skinRms) * (rP + skinRms);
        const bool ownN = !(gauss || N < 2);
        double firstScale = 0;
        const auto fit = [&] {
            for (int round = 0; round < (ownN ? 12 : 1); round++) {
                mx = my = mz = 0;
                for (const Nucleon& q : out) { mx += q.x; my += q.y; mz += q.z; }
                mx /= A; my /= A; mz /= A;
                double fx = 1, fy = 1, fz = 1, sxx = 0, syy = 0, szz = 0; int np = 0;
                for (const Nucleon& q : out) if (q.isP) { sxx += (q.x - mx) * (q.x - mx); syy += (q.y - my) * (q.y - my); szz += (q.z - mz) * (q.z - mz); np++; }
                if (np > 0 && sxx + syy + szz > 0) {
                    fx = fy = fz = std::sqrt(chargeTarget * np / (sxx + syy + szz));
                    if (newShape && beta != 0 && np >= 2 && sxx > 0 && syy > 0 && szz > 0) {
                        const double ratio = shapeMomentRatio(beta), tx = chargeTarget * (1 - ratio / 2) / 3, tz2 = chargeTarget * (1 + ratio) / 3;   // <x^2> = <y^2>, <z^2> of the body; 2 tx + tz2 = chargeTarget
                        fx = std::sqrt(tx * np / sxx); fy = std::sqrt(tx * np / syy); fz = std::sqrt(tz2 * np / szz);
                    }
                }
                if (firstScale == 0) firstScale = std::min(fx, std::min(fy, fz));
                for (Nucleon& q : out) { q.x = mx + (q.x - mx) * fx; q.y = my + (q.y - my) * fy; q.z = mz + (q.z - mz) * fz; }
                if (!ownN) break;
                double sn = 0; int nn = 0;
                for (const Nucleon& q : out) if (!q.isP) { sn += (q.x - mx) * (q.x - mx) + (q.y - my) * (q.y - my) + (q.z - mz) * (q.z - mz); nn++; }
                const double g = sn > 0 ? std::sqrt(msN * nn / sn) : 1;
                for (Nucleon& q : out) if (!q.isP) { q.x = mx + (q.x - mx) * g; q.y = my + (q.y - my) * g; q.z = mz + (q.z - mz) * g; }
            }
        };
        fit();
        if (ownN) {
            const double dT = dmin * firstScale;               // what the layout gave until 0.41: the hard core times the (common) scale
            for (int pass = 0; pass < 8; pass++) {
                bool moved = false;
                for (int sweep = 0; sweep < 20; sweep++) {
                    bool any = false;
                    for (std::size_t i = 0; i < out.size(); i++) for (std::size_t j = i + 1; j < out.size(); j++) {
                        const double dx = out[j].x - out[i].x, dy = out[j].y - out[i].y, dz = out[j].z - out[i].z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
                        if (d >= dT || d <= 0) continue;
                        const double k = 0.5 * (dT * 1.001 - d) / d;
                        out[i].x -= dx * k; out[i].y -= dy * k; out[i].z -= dz * k; out[j].x += dx * k; out[j].y += dy * k; out[j].z += dz * k;
                        any = true;
                    }
                    if (!any) break;
                    moved = true;
                }
                if (!moved) break;
                fit();
            }
            mx = my = mz = 0; for (const Nucleon& q : out) { mx += q.x; my += q.y; mz += q.z; } mx /= A; my /= A; mz /= A;
        }
    }
    }   // (0.46.5: the end of the A >= 3 sampling; the deuteron's pair joins here for the tilt)
    // tilt the symmetry axis so the shape shows from the side. 0.46.1 (outside review: uranium-238, beta2 = 0.29, looked almost round): the axis lies across the line of sight of BOTH first views,
    // in the orbit plane (the Orbit view looks straight down z) and square to the Cloud 3D view's first direction (yaw 0.6, AtomState::yaw): (cos 0.6, -sin 0.6, 0). Until 0.46 it was tilted 0.95 rad
    // from z at a random azimuth, so seen from above it showed at most 81 % of its length and often much less. (The random number is still drawn, so the sequences after it are unchanged.)
    const double tzOld = tilt ? random() * 6.28 : 0; (void)tzOld;
    const double ty = tilt ? kPi / 2 : 0, tz = tilt ? -0.6 : 0, c1 = std::cos(ty), s1 = std::sin(ty), c2 = std::cos(tz), s2 = std::sin(tz);
    if (axisOut) { axisOut[0] = s1 * c2; axisOut[1] = s1 * s2; axisOut[2] = c1; }   // 0.22: where the symmetry axis (z) points after the tilt (the Density look uses the same axis)
    for (Nucleon& q : out) {
        const double x = q.x - mx, y = q.y - my, z = q.z - mz, x1 = x * c1 + z * s1, z1 = -x * s1 + z * c1;
        q.x = x1 * c2 - y * s2; q.y = x1 * s2 + y * c2; q.z = z1;
    }
    return toMetres();
}
// 0.22: the Density look. The charge density itself (what electron scattering measures), not nucleons: a two-parameter Fermi (Woods–Saxon) distribution
// rho(r) = rho0 / (1 + exp((r - c R(u)) / a)), the fit used for electron-scattering charge densities (e.g. arXiv:2104.12785; its 90 %-to-10 % surface is about 2.3 fm thick, t = 4 a ln 3),
// with a = 0.54 fm, the measured CHARGE diffuseness (radii.h kWsDiffuseFm; the nucleon layout above samples point centres with 0.50 fm, kWsPointDiffuseFm, which the nucleons' size folds to 0.53-0.54 fm), R(u) the lab's volume-keeping shape (quadrupole.h) about the symmetry axis, and for light nuclei (A <= 16, no flat interior) a Gaussian.
// The points are then scaled so that their rms radius is the measured rms CHARGE radius (IAEA table; else the lab's rule ball plus the proton's own size) and, for a deformed nucleus,
// so that <3 z^2 - r^2> / <r^2> is the shape's own ratio, as for the snapshot above. A = 1: the proton's own charge, exponential, e^(-r / b) with rms = sqrt(12) b (0.46: none for a neutron).
// 0.46.5: C-12 and O-16 from their measured HO charge densities (nucleusHoCharge: O-16 dips 8 % in the middle, C-12 is flat-topped), the deuteron from its wave function (CD-Bonn,
// a dense core and a long halo; round, a dumbbell or a doughnut by its spin state); the other light nuclei stay Gaussians (no profile saved: ROADMAP).
std::vector<double> nucleusChargeDots(int Z, int N, int count, double ax, double ay, double az, unsigned seed, int deuteronSpin) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    const auto u01 = [&] { double v = 0; while (v <= 0) v = U(rng); return v; };
    const auto gs = [&] { return std::sqrt(-2 * std::log(u01())) * std::cos(2 * kPi * U(rng)); };
    const int A = Z + N;
    if (Z == 0) return {};                               // 0.46 (review A5): a lone neutron has no net charge, so no charge cloud (its rest-frame charge density, a positive core and a negative edge,
                                                         // is model dependent and its transverse density has a negative centre: Kelly 2002, Miller 2007, docs/QUARKS.md; neither is drawn)
    const double rmsWant = chargeRmsWant(Z, N);
    double hoA = 0, hoAl = 0;
    const bool ho = A <= 16 && nucleusHoCharge(Z, N, hoA, hoAl);           // 0.46.5: C-12 and O-16 as measured (HO), no longer Gaussians
    const std::function<double()> uni = [&] { return U(rng); }, gsf = [&] { return gs(); };
    std::vector<double> d;
    d.reserve(3 * static_cast<std::size_t>(std::max(0, count)));
    const Shape shp = A >= 12 ? nucShape(Z, N) : Shape{};
    const double beta = shp.beta, a = kWsDiffuseFm;
    const double c = A > 16 ? std::sqrt(std::max(1.0, 5.0 / 3.0 * (rmsWant * rmsWant - 7.0 / 5.0 * kPi * kPi * a * a))) : 0;   // <r^2> = 3/5 c^2 + 7/5 pi^2 a^2 (Fermi distribution, c >> a)
    for (int i = 0; i < count; i++) {
        double x = 0, y = 0, z = 0;
        if (A <= 1) {                                    // r ~ Gamma(3, b): the density e^(-r/b) times r^2
            const double b = rmsWant / std::sqrt(12.0), r = -b * std::log(u01() * u01() * u01()), uu = 2 * U(rng) - 1, f = 2 * kPi * U(rng), sn = std::sqrt(1 - uu * uu);
            x = r * sn * std::cos(f); y = r * sn * std::sin(f); z = r * uu;
        } else if (A == 2) {                             // 0.46.5: the deuteron: the proton at half the pair's separation (CD-Bonn, in the chosen spin state about z), with its own exponential charge
            double sx, sy, sz;                           // (the neutron's small charge, + core and - edge, is left out; the scaling below gives the measured radius, which includes it)
            deuteronSeparation(deuteronSpin, uni, sx, sy, sz);
            const double b = kProtonRmsFm / std::sqrt(12.0), r = -b * std::log(u01() * u01() * u01()), uu = 2 * U(rng) - 1, f = 2 * kPi * U(rng), sn = std::sqrt(1 - uu * uu);
            x = sx / 2 + r * sn * std::cos(f); y = sy / 2 + r * sn * std::sin(f); z = sz / 2 + r * uu;
        } else if (ho) {
            const double r = hoRadius(hoA, hoAl, gsf, uni), uu = 2 * U(rng) - 1, f = 2 * kPi * U(rng), sn = std::sqrt(1 - uu * uu);
            x = r * sn * std::cos(f); y = r * sn * std::sin(f); z = r * uu;
            if (beta != 0) { const double fr = shapeRadiusFactor(beta, uu); x *= fr; y *= fr; z *= fr; }
        } else if (A <= 16) {
            x = gs(); y = gs(); z = gs();
            if (beta != 0) { const double r = std::sqrt(x * x + y * y + z * z), f = r > 0 ? shapeRadiusFactor(beta, z / r) : 1; x *= f; y *= f; z *= f; }
        } else {
            const double Rmax = c * 1.6 + 9 * a + 1;
            for (;;) {
                const double uu = 2 * U(rng) - 1, f = 2 * kPi * U(rng), sn = std::sqrt(1 - uu * uu), r = Rmax * std::cbrt(U(rng));
                const double Rth = c * (beta != 0 ? shapeRadiusFactor(beta, uu) : 1);
                if (U(rng) > 1 / (1 + std::exp((r - Rth) / a))) continue;
                x = r * sn * std::cos(f); y = r * sn * std::sin(f); z = r * uu;
                break;
            }
        }
        d.push_back(x); d.push_back(y); d.push_back(z);
    }
    const std::size_t n = d.size() / 3;
    if (n > 0 && A > 1) {                                // the measured size (and the shape's quadrupole) exactly
        double sxx = 0, syy = 0, szz = 0;
        for (std::size_t i = 0; i < n; i++) { sxx += d[3 * i] * d[3 * i]; syy += d[3 * i + 1] * d[3 * i + 1]; szz += d[3 * i + 2] * d[3 * i + 2]; }
        const double ms = rmsWant * rmsWant;
        double fx = std::sqrt(ms * n / (sxx + syy + szz)), fy = fx, fz = fx;
        if (beta != 0 && sxx > 0 && syy > 0 && szz > 0) {
            const double ratio = shapeMomentRatio(beta), tx = ms * (1 - ratio / 2) / 3, tz = ms * (1 + ratio) / 3;
            fx = std::sqrt(tx * n / sxx); fy = std::sqrt(tx * n / syy); fz = std::sqrt(tz * n / szz);
        }
        for (std::size_t i = 0; i < n; i++) { d[3 * i] *= fx; d[3 * i + 1] *= fy; d[3 * i + 2] *= fz; }
    }
    const double al = std::sqrt(ax * ax + ay * ay + az * az);   // turn the z axis onto (ax, ay, az): Rodrigues' formula
    if (al > 0 && n > 0) {
        const double vx = ax / al, vy = ay / al, vz = az / al;
        const double kx = -vy, ky = vx, ks = std::sqrt(kx * kx + ky * ky), cosT = vz;   // k = z x v
        if (ks > 1e-12) {
            const double ux = kx / ks, uy = ky / ks, sinT = ks;
            for (std::size_t i = 0; i < n; i++) {
                const double x = d[3 * i], y = d[3 * i + 1], z = d[3 * i + 2], dot = ux * x + uy * y;
                const double cx = uy * z, cy = -ux * z, cz = ux * y - uy * x;   // u x p, with u = (ux, uy, 0)
                d[3 * i] = x * cosT + cx * sinT + ux * dot * (1 - cosT);
                d[3 * i + 1] = y * cosT + cy * sinT + uy * dot * (1 - cosT);
                d[3 * i + 2] = z * cosT + cz * sinT;
            }
        } else if (cosT < 0) for (std::size_t i = 0; i < n; i++) { d[3 * i + 1] = -d[3 * i + 1]; d[3 * i + 2] = -d[3 * i + 2]; }
    }
    return d;
}
// 0.54.1: the Density look's charge density as a function, and its exact projection (atom_core.h). The same model as nucleusChargeDots above, case by case:
//   the proton: e^(-r/b) / (8 pi b^3), b = rms / sqrt(12) (not rescaled);
//   the deuteron: the proton at half the pair's separation s, s from |psi_M|^2 (u^2 + w^2 + k P2 (sqrt(2) u w - w^2/2)) / (4 pi s^2) as in deuteronSeparation, folded with the
//     proton's own exponential charge (b = kProtonRmsFm / sqrt(12)). A spherical kernel keeps the Legendre order, so the fold has an L = 0 and an L = 2 part, each a radial
//     integral done once here (deuteronFold); then scaled to the measured rms radius;
//   C-12, O-16: (1 + alpha s^2/A^2) e^(-s^2/A^2) (nucleusHoCharge) and the other A <= 16: e^(-s^2/2), both with s = r / f(u) and a factor f(u)^-3 (the points' radius is stretched
//     by f(u) = shapeRadiusFactor(beta, cos theta) in each direction, so each direction keeps its share);
//   A > 16: 1 / (1 + e^((r - c f(u)) / a)) inside the sampling ball r < 1.6 c + 9 a + 1 (the points' own cut);
//   then, as for the points, x and y scaled by fx and z by fz so that <r^2> is the measured rms^2 and, for a deformed nucleus, <3z^2 - r^2>/<r^2> the shape's own ratio; here the
//   moments come from the density itself (midpoint sums on 600 x 600 cells), not from a sample; and the z axis turned onto (ax, ay, az).
namespace {
struct DeuteronFold { static constexpr double kStep = 0.025, kMax = 22; std::vector<double> c0, c2; };
const DeuteronFold& deuteronFold() {
    static const DeuteronFold f = [] {
        DeuteronFold d;
        const double bp = kProtonRmsFm / std::sqrt(12.0), kn = 1 / (8 * kPi * bp * bp * bp);
        const int nx = int(DeuteronFold::kMax / DeuteronFold::kStep), nr = nx + 1;
        std::vector<double> px0(nx), px2(nx), xs(nx);
        for (int i = 0; i < nx; i++) {   // the proton's position x = s/2: P_x(x) = 8 P(2x)
            const double x = (i + 0.5) * DeuteronFold::kStep, s = 2 * x, u = deuteronU(s), w = deuteronW(s);
            xs[i] = x; px0[i] = 8 * (u * u + w * w) / (4 * kPi * s * s); px2[i] = 8 * (std::sqrt(2.0) * u * w - 0.5 * w * w) / (4 * kPi * s * s);
        }
        static const double gx[8] = {0.0950125098376374, 0.2816035507792589, 0.4580167776572274, 0.6178762444026438, 0.7554044083550030, 0.8656312023878318, 0.9445750230732326, 0.9894009349916499};
        static const double gw[8] = {0.1894506104550685, 0.1826034150449236, 0.1691565193950025, 0.1495959888165767, 0.1246289712555339, 0.0951585116824928, 0.0622535239386479, 0.0271524594117541};
        d.c0.assign(nr, 0.0); d.c2.assign(nr, 0.0);
        for (int j = 0; j < nr; j++) {
            const double R = j * DeuteronFold::kStep;
            double s0 = 0, s2 = 0;
            for (int i = 0; i < nx; i++) {
                const double x = xs[i];
                if (R == 0) { s0 += 4 * kPi * x * x * px0[i] * kn * std::exp(-x / bp); continue; }
                // the shell of radius x around the point R: (2 pi / (R x)) times the integral over the distance d of K(d) P_L(t) d, t = (R^2 + x^2 - d^2) / (2 R x)
                const double lo = std::fabs(R - x), hi = std::min(R + x, lo + 40 * bp), m = 0.5 * (lo + hi), hw = 0.5 * (hi - lo);
                double i0 = 0, i2 = 0;
                for (int g = 0; g < 16; g++) {
                    const double dd = m + (g < 8 ? -gx[g] : gx[g - 8]) * hw, wg = gw[g % 8], t = (R * R + x * x - dd * dd) / (2 * R * x), k = kn * std::exp(-dd / bp) * dd * wg * hw;
                    i0 += k; i2 += k * 0.5 * (3 * t * t - 1);
                }
                const double sh = 2 * kPi / (R * x) * x * x * DeuteronFold::kStep;
                s0 += sh * px0[i] * i0; s2 += sh * px2[i] * i2;
            }
            d.c0[j] = s0; d.c2[j] = s2;
        }
        return d;
    }();
    return f;
}
double foldAt(const std::vector<double>& t, double r) {
    const double i = r / DeuteronFold::kStep;
    if (i >= double(t.size() - 1)) return 0;
    const int k = int(i);
    return t[k] + (t[k + 1] - t[k]) * (i - k);
}
double baseCharge(const NucleusChargeDensity& d, double rp, double z) {
    const double r = std::sqrt(rp * rp + z * z), u = r > 0 ? z / r : 1;
    switch (d.kind) {
        case 0: return std::exp(-r / d.b);
        case 1: { const DeuteronFold& f = deuteronFold(); return foldAt(f.c0, r) + d.kk * foldAt(f.c2, r) * 0.5 * (3 * u * u - 1); }
        case 2: case 3: {
            const double f = d.beta != 0 ? shapeRadiusFactor(d.beta, u) : 1, s = r / f, f3 = f * f * f;
            return d.kind == 2 ? (1 + d.hoAl * s * s / (d.hoA * d.hoA)) * std::exp(-s * s / (d.hoA * d.hoA)) / f3 : std::exp(-0.5 * s * s) / f3;
        }
        case 4: {
            if (r >= d.rmax) return 0;
            const double rth = d.c * (d.beta != 0 ? shapeRadiusFactor(d.beta, u) : 1);
            return 1 / (1 + std::exp((r - rth) / d.a));
        }
        default: return 0;
    }
}
}  // namespace
double NucleusChargeDensity::atBody(double rp, double z) const { return kind < 0 ? 0 : norm * baseCharge(*this, rp / fx, z / fz); }
double NucleusChargeDensity::at(double x, double y, double z) const {
    const double zz = x * n[0] + y * n[1] + z * n[2], rp = std::sqrt(std::max(0.0, x * x + y * y + z * z - zz * zz));
    return atBody(rp, zz);
}
NucleusChargeDensity nucleusChargeDensity(int Z, int N, double ax, double ay, double az, int deuteronSpin) {
    NucleusChargeDensity d;
    d.Z = Z;
    const int A = Z + N;
    if (Z == 0) return d;                                // a lone neutron: no charge cloud (as nucleusChargeDots)
    const double al = std::sqrt(ax * ax + ay * ay + az * az);
    if (al > 0) { d.n[0] = ax / al; d.n[1] = ay / al; d.n[2] = az / al; }
    const double rmsWant = chargeRmsWant(Z, N);
    const Shape shp = A >= 12 ? nucShape(Z, N) : Shape{};
    d.beta = shp.beta; d.a = kWsDiffuseFm;
    double rb = 0;                                       // the base density's reach (fm)
    if (A <= 1) { d.kind = 0; d.b = rmsWant / std::sqrt(12.0); rb = 32 * d.b; d.beta = 0; }
    else if (A == 2) { d.kind = 1; d.kk = deuteronSpin == 1 ? 1.0 : deuteronSpin == 2 ? -2.0 : 0.0; rb = DeuteronFold::kMax; d.beta = 0; }
    else if (A <= 16 && nucleusHoCharge(Z, N, d.hoA, d.hoAl)) { d.kind = 2; rb = 6.2 * d.hoA * (1 + 2 * std::fabs(d.beta)); }
    else if (A <= 16) { d.kind = 3; rb = 8.5 * (1 + 2 * std::fabs(d.beta)); }
    else { d.kind = 4; d.c = std::sqrt(std::max(1.0, 5.0 / 3.0 * (rmsWant * rmsWant - 7.0 / 5.0 * kPi * kPi * d.a * d.a))); d.rmax = d.c * 1.6 + 9 * d.a + 1; rb = d.rmax; }
    // the base density's integral and moments
    double n0 = 0, mx = 0, mz = 0;
    if (d.kind == 0) { n0 = 8 * kPi * d.b * d.b * d.b; }
    else if (d.kind == 1) {
        const DeuteronFold& f = deuteronFold();
        for (std::size_t j = 1; j < f.c0.size(); j++) { const double r = (j - 0.5) * DeuteronFold::kStep, v = 0.5 * (f.c0[j] + f.c0[j - 1]) * 4 * kPi * r * r * DeuteronFold::kStep; n0 += v; mx += v * r * r; }
    } else {
        const int M = 600;
        const double h = rb / M;
        for (int iz = 0; iz < M; iz++) for (int ip = 0; ip < M; ip++) {
            const double rp = (ip + 0.5) * h, z = (iz + 0.5) * h, v = baseCharge(d, rp, z) * 2 * kPi * rp * h * h * 2;   // (both halves, z < 0 too)
            n0 += v; mx += v * rp * rp; mz += v * z * z;
        }
    }
    if (d.kind >= 1 && n0 > 0) {
        const double ms = rmsWant * rmsWant, r2 = (mx + mz) / n0;
        if (d.beta != 0 && mx > 0 && mz > 0) {
            const double ratio = shapeMomentRatio(d.beta), tx = ms * (1 - ratio / 2) / 3, tz = ms * (1 + ratio) / 3;
            d.fx = std::sqrt(tx / (0.5 * mx / n0)); d.fz = std::sqrt(tz / (mz / n0));
        } else d.fx = d.fz = std::sqrt(ms / r2);
    }
    d.norm = n0 > 0 ? Z / (n0 * d.fx * d.fx * d.fz) : 0;
    d.rEff = rb * std::max(d.fx, d.fz);
    d.radial = d.kind <= 1 || d.beta == 0;
    // the radius holding 99.7 % of the charge
    {
        const int nb = 4000;
        std::vector<double> sh(nb + 1, 0.0);
        const double hb = d.rEff / nb;
        if (d.radial) for (int i = 0; i < nb; i++) { const double r = (i + 0.5) * hb, v = d.kind == 1 ? d.norm * foldAt(deuteronFold().c0, r / d.fx) : d.atBody(0, r); sh[i + 1] = sh[i] + v * 4 * kPi * r * r * hb; }   // (the deuteron: its round part; the L = 2 part holds no charge)
        else {
            const int M = 500;
            const double h = d.rEff / M;
            std::vector<double> bin(nb, 0.0);
            for (int iz = 0; iz < M; iz++) for (int ip = 0; ip < M; ip++) {
                const double rp = (ip + 0.5) * h, z = (iz + 0.5) * h, r = std::sqrt(rp * rp + z * z);
                const int k = std::min(nb - 1, int(r / hb));
                bin[k] += d.atBody(rp, z) * 4 * kPi * rp * h * h;
            }
            for (int i = 0; i < nb; i++) sh[i + 1] = sh[i] + bin[i];
        }
        int i = 0;
        while (i < nb && sh[i + 1] < 0.997 * sh[nb]) i++;
        d.ext997 = (i + 1) * hb;
    }
    if (d.radial) {   // the line-of-sight integrals against R (midpoint sums along the line, 1200 steps from the middle out to rEff)
        d.nR = 1024; d.hR = d.rEff / (d.nR - 1);
        d.I0.assign(d.nR, 0.0); d.I2.assign(d.nR, 0.0); d.K0.assign(d.nR, 0.0); d.K2.assign(d.nR, 0.0);
        const DeuteronFold* f = d.kind == 1 ? &deuteronFold() : nullptr;
        for (int j = 0; j < d.nR; j++) {
            const double R = j * d.hR, L = std::sqrt(std::max(0.0, d.rEff * d.rEff - R * R));
            const int K = 1200;
            const double hl = L / K;
            double i0 = 0, i2 = 0, k0 = 0, k2 = 0;
            for (int k = 0; k < K; k++) {
                const double l = (k + 0.5) * hl, r2 = R * R + l * l, r = std::sqrt(r2);
                if (!f) { i0 += d.atBody(0, r); continue; }
                const double q = r / d.fx, f0 = d.norm * foldAt(f->c0, q), f2 = d.norm * d.kk * foldAt(f->c2, q);
                i0 += f0; i2 += f2; k0 += f2 / r2; k2 += f2 * l * l / r2;
            }
            d.I0[j] = 2 * i0 * hl; d.I2[j] = 2 * i2 * hl; d.K0[j] = 2 * k0 * hl; d.K2[j] = 2 * k2 * hl;
        }
    } else {          // the density on a grid over (rp, |z|), for the rays
        // (across the axis the table steps in rp^2: the density is a smooth function of rp^2, and a ray then needs no square root; at U-238's surface, rp = 7 fm, a step is 0.04 fm)
        d.nT = 512; d.hT = d.rEff / (d.nT - 1); d.hU = d.rEff * d.rEff / (d.nT - 1);
        d.T.assign(std::size_t(d.nT) * d.nT, 0.f);
        float peak = 0;
        for (int iz = 0; iz < d.nT; iz++) for (int iu = 0; iu < d.nT; iu++) { const float v = float(d.atBody(std::sqrt(iu * d.hU), iz * d.hT)); d.T[std::size_t(iz) * d.nT + iu] = v; peak = std::max(peak, v); }
        d.rCut = 0;   // the rays go out to where the density falls below 1e-6 of its top (the charge beyond is a few millionths of Z)
        for (int iz = 0; iz < d.nT; iz++) for (int iu = 0; iu < d.nT; iu++) if (d.T[std::size_t(iz) * d.nT + iu] > 1e-6f * peak) d.rCut = std::max(d.rCut, std::sqrt(iu * d.hU + iz * d.hT * iz * d.hT));
        d.rCut = std::min(d.rEff, d.rCut + 2 * d.hT);
    }
    return d;
}
void projectChargeDensity(const NucleusChargeDensity& d, const double e1[3], const double e2[3], const double e3[3], int G, double cellFm, std::vector<float>& out, int qn) {
    out.assign(std::size_t(std::max(0, G)) * std::max(0, G), 0.f);
    if (d.kind < 0 || G <= 0) return;
    const double n1 = d.n[0] * e1[0] + d.n[1] * e1[1] + d.n[2] * e1[2], n2 = d.n[0] * e2[0] + d.n[1] * e2[1] + d.n[2] * e2[2], n3 = d.n[0] * e3[0] + d.n[1] * e3[1] + d.n[2] * e3[2];
    const double half = 0.5 * G * cellFm;
    if (d.radial) {
        // round: I0(R). The deuteron's L = 2 part f2(r) P2(n.r/r): along the line r = x + l e3, (n.r)^2 = (n.x)^2 + 2 (n.x) n3 l + n3^2 l^2 (the odd term integrates to 0), so its
        // integral is 3/2 (n.x)^2 K0(R) + 3/2 n3^2 K2(R) - 1/2 I2(R), K0 the integral of f2 / r^2 and K2 of f2 l^2 / r^2: exact for every view, from tables made once
        const auto lerp = [&](const std::vector<double>& t, double i) { const int k = int(i); return k >= d.nR - 1 ? 0.0 : t[k] + (t[k + 1] - t[k]) * (i - k); };
        for (int row = 0; row < G; row++) for (int col = 0; col < G; col++) {
            const double x = -half + (col + 0.5) * cellFm, y = half - (row + 0.5) * cellFm, R = std::sqrt(x * x + y * y), i = R / d.hR;
            double v = lerp(d.I0, i);
            if (d.kk != 0) { const double nx = x * n1 + y * n2; v += 1.5 * nx * nx * lerp(d.K0, i) + 1.5 * n3 * n3 * lerp(d.K2, i) - 0.5 * lerp(d.I2, i); }
            out[std::size_t(row) * G + col] = float(std::max(0.0, v));
        }
        return;
    }
    // deformed: the rays on a quarter grid along (s) and across (t) the projected axis, the picture being symmetric in s and in t (the shape is round about its axis and the same
    // under r -> -r), then read at each cell by interpolation
    const double nperp = std::sqrt(n1 * n1 + n2 * n2), sx = nperp > 1e-9 ? n1 / nperp : 1, sy = nperp > 1e-9 ? n2 / nperp : 0;
    const double reach = half * std::sqrt(2.0), hq = reach / std::max(2, qn - 1);
    const double dl = (d.kind == 4 ? d.a : d.kind == 2 ? 0.5 * d.hoA : 1.0) * std::min(d.fx, d.fz);   // trapezoids of one surface thickness: on a smooth integrand their error is ~ e^(-2 pi^2) ~ 3e-9
    const double rE2 = d.rCut * d.rCut, inv = 1 / d.hT, invU = 1 / d.hU;
    const int nT = d.nT;
    const float* T = d.T.data();
    const auto rho = [&](double rp2, double z) {
        const double ip = rp2 * invU, iz = std::fabs(z) * inv;
        if (ip >= nT - 1 || iz >= nT - 1) return 0.0;
        const int a = int(ip), b = int(iz);
        const double fa = ip - a, fb = iz - b;
        const float* r0 = T + std::size_t(b) * nT + a;
        return (1 - fb) * ((1 - fa) * r0[0] + fa * r0[1]) + fb * ((1 - fa) * r0[nT] + fa * r0[nT + 1]);
    };
    std::vector<double> q(std::size_t(qn) * qn, 0.0);
    for (int it = 0; it < qn; it++) for (int is = 0; is < qn; is++) {
        const double s = is * hq, t = it * hq, q2 = s * s + t * t;
        if (q2 >= rE2) continue;
        const double L = std::sqrt(rE2 - q2), z0 = s * nperp;
        const int K = std::max(2, int(std::ceil(2 * L / dl)));
        const double h = 2 * L / K;
        double sum = 0;
        for (int k = 1; k < K; k++) {   // trapezoids (the ends are at rEff, where the density is 0)
            const double l = -L + k * h, z = z0 + l * n3, r2 = q2 + l * l;
            sum += rho(std::max(0.0, r2 - z * z), z);
        }
        q[std::size_t(it) * qn + is] = sum * h;
    }
    for (int row = 0; row < G; row++) for (int col = 0; col < G; col++) {
        const double x = -half + (col + 0.5) * cellFm, y = half - (row + 0.5) * cellFm;
        const double is = std::fabs(x * sx + y * sy) / hq, it = std::fabs(-x * sy + y * sx) / hq;
        if (is >= qn - 1 || it >= qn - 1) continue;
        const int a = int(is), b = int(it);
        const double fa = is - a, fb = it - b;
        const double* r0 = q.data() + std::size_t(b) * qn + a;
        out[std::size_t(row) * G + col] = float((1 - fb) * ((1 - fa) * r0[0] + fa * r0[1]) + fb * ((1 - fa) * r0[qn] + fa * r0[qn + 1]));
    }
}
std::vector<Nucleon> buildNucleus(int Z, int N, double* axis, int deuteronSpin) { return buildNucleusImpl(Z, N, sizedByMeasurement(Z, N), true, true, axis, deuteronSpin); }
std::vector<Nucleon> buildNucleusByRule(int Z, int N) { return buildNucleusImpl(Z, N, false, false); }
std::vector<Nucleon> buildNucleusAxial(int Z, int N) { return buildNucleusImpl(Z, N, sizedByMeasurement(Z, N), true, false); }

// ---------------------------------------------------------------- radioactive decay
static const std::vector<std::map<int, std::string>>& decTable() {
    static std::vector<std::map<int, std::string>> t = [] {
        std::vector<std::map<int, std::string>> v(119);
        for (const char* line : kDecayLines) {
            const std::string l = line;
            const std::size_t bar = l.find('|');
            const int z = std::atoi(l.substr(0, bar).c_str());
            for (const std::string& e : split(l.substr(bar + 1), ';')) {
                const std::size_t i = e.find(':');
                v[z][std::atoi(e.substr(0, i).c_str())] = e.substr(i + 1);
            }
        }
        return v;
    }();
    return t;
}
// half-life in s (none = not measured), whether the odds were measured, and the modes with their odds; none when stable
std::optional<DecayInfo> decInfo(int Z, int N) {
    if (Z < 1 || Z > 118) return std::nullopt;
    const auto& m = decTable()[Z];
    const auto it = m.find(N);
    if (it == m.end()) return std::nullopt;
    std::string r = it->second;
    double T = NAN;
    const std::size_t sl = r.find('/');
    if (sl != std::string::npos) { T = parseFloat(r.substr(0, sl)); r = r.substr(sl + 1); }
    DecayInfo d;
    d.unk = !r.empty() && r[0] == '?';
    if (d.unk) r = r.substr(1);
    for (const std::string& x : split(r, ',')) {
        const std::size_t eq = x.find('=');
        d.br.push_back({x.substr(0, eq), eq == std::string::npos ? NAN : parseFloat(x.substr(eq + 1))});
    }
    const std::string* c = nucCode(Z, N);
    if (c && *c != "U" && *c != "?" && *c != "S") T = parseFloat((*c)[0] == '~' ? c->substr(1) : *c);
    if (c && *c == "U" && !(T > 0)) T = 1e-22;   // unbound, no measured time: it comes apart at once
    if (T > 0) d.T = T;
    return d;
}
std::optional<std::pair<int, int>> decMove(const std::string& m) {
    static const std::map<std::string, std::pair<int, int>> mv = {
        {"A", {-2, -2}}, {"B-", {1, -1}}, {"B-n", {1, -2}}, {"B-2n", {1, -3}}, {"B-3n", {1, -4}}, {"B-4n", {1, -5}}, {"B-p", {0, -1}},
        {"B-A", {-1, -3}}, {"B-t", {0, -3}}, {"B-d", {0, -2}}, {"B+", {-1, 1}}, {"B+p", {-2, 1}}, {"B+2p", {-3, 1}}, {"B+3p", {-4, 1}},
        {"B+A", {-3, -1}}, {"B+pA", {-4, -1}}, {"p", {-1, 0}}, {"2p", {-2, 0}}, {"3p", {-3, 0}}, {"n", {0, -1}}, {"2n", {0, -2}},
        {"3n", {0, -3}}, {"2B-", {2, -2}}, {"2B+", {-2, 2}}};
    static const std::map<std::string, int> decZ = {{"C", 6}, {"O", 8}, {"F", 9}, {"Ne", 10}, {"Mg", 12}, {"Si", 14}};
    const auto it = mv.find(m);
    if (it != mv.end()) return it->second;
    // cluster decay "14C", "24Ne": a small nucleus is thrown out
    std::size_t i = 0;
    while (i < m.size() && std::isdigit(static_cast<unsigned char>(m[i]))) i++;
    if (i > 0 && i < m.size()) {
        const std::string el = m.substr(i);
        const auto z = decZ.find(el);
        const bool shape = std::isupper(static_cast<unsigned char>(el[0])) && (el.size() == 1 || (el.size() == 2 && std::islower(static_cast<unsigned char>(el[1]))));
        if (shape && z != decZ.end()) { const int a = std::atoi(m.substr(0, i).c_str()); return std::make_pair(-z->second, -(a - z->second)); }
    }
    return std::nullopt;   // SF, B-SF, B+SF: the nucleus splits in two
}
// "14C" -> {"14", "C"} when it is a cluster mode
static bool clusterParts(const std::string& m, std::string& a, std::string& el) {
    std::size_t i = 0;
    while (i < m.size() && std::isdigit(static_cast<unsigned char>(m[i]))) i++;
    if (i == 0 || i == m.size()) return false;
    el = m.substr(i);
    if (!std::isupper(static_cast<unsigned char>(el[0])) || el.size() > 2 || (el.size() == 2 && !std::islower(static_cast<unsigned char>(el[1])))) return false;
    a = m.substr(0, i);
    return true;
}
std::string decWord(const std::string& m) {
    static const std::map<std::string, std::string> w = {
        {"A", "alpha (α)"}, {"B-", "beta minus (β⁻)"}, {"B+", "beta plus or electron capture (β⁺/EC)"}, {"n", "neutron emission"},
        {"2n", "two-neutron emission"}, {"3n", "three-neutron emission"}, {"p", "proton emission"}, {"2p", "two-proton emission"},
        {"3p", "three-proton emission"}, {"SF", "spontaneous fission"}, {"B-SF", "β⁻ then fission"}, {"B+SF", "β⁺/EC then fission"},
        {"2B-", "double beta minus"}, {"2B+", "double beta plus"}};
    const auto it = w.find(m);
    if (it != w.end()) return it->second;
    if (m.size() >= 3 && m[0] == 'B' && (m[1] == '+' || m[1] == '-')) {   // a beta decay followed at once by more particles
        std::string rest = m.substr(2);
        const std::size_t ai = rest.find('A');
        if (ai != std::string::npos) rest.replace(ai, 1, "α");
        // /^(\d)?([a-zα]+)$/: an optional count, then the particle letters
        std::string k, q = rest;
        if (!q.empty() && std::isdigit(static_cast<unsigned char>(q[0]))) { k = q.substr(0, 1); q = q.substr(1); }
        bool letters = !q.empty();
        for (std::size_t i = 0; i < q.size(); ) {
            if (q.compare(i, strlen("α"), "α") == 0) { i += strlen("α"); continue; }
            if (q[i] >= 'a' && q[i] <= 'z') { i++; continue; }
            letters = false; break;
        }
        std::string tail = rest;
        if (letters) {
            static const std::map<std::string, std::string> nm = {{"n", "neutron"}, {"p", "proton"}, {"d", "deuteron"}, {"t", "triton"}, {"α", "α"}, {"pα", "proton + α"}};
            const auto f = nm.find(q);
            tail = (k.empty() ? "" : k + " ") + (f == nm.end() ? std::string("undefined") : f->second) + (k.empty() ? "" : "s");
        }
        return std::string("β") + (m[1] == '-' ? "⁻" : "⁺") + " then " + tail;
    }
    std::string a, el;
    if (clusterParts(m, a, el)) return "cluster decay (throws out " + el + "-" + a + ")";
    return m;
}
// what flies out, for the picture: label, colour, size
std::vector<Shot> decShots(const std::string& m) {
    const Shot E{"e⁻", "#4f8df0", 3}, AE{"ν̄", "#9aa0aa", 2}, P{"e⁺", "#f07a3a", 3}, NU{"ν", "#9aa0aa", 2},
               AL{"α", "#e0506a", 6}, NN{"n", "#969692", 4}, PP{"p", "#e0506a", 4};
    if (m == "A") return {AL};
    if (m == "B-") return {E, AE};
    if (m == "2B-") return {E, E, AE, AE};
    if (m == "B+") return {P, NU};
    if (m == "2B+") return {P, P, NU, NU};
    if (m == "n" || (m.size() == 2 && std::isdigit(static_cast<unsigned char>(m[0])) && m[1] == 'n')) return std::vector<Shot>(m[0] == 'n' ? 1 : m[0] - '0', NN);
    if (m == "p" || (m.size() == 2 && std::isdigit(static_cast<unsigned char>(m[0])) && m[1] == 'p')) return std::vector<Shot>(m[0] == 'p' ? 1 : m[0] - '0', PP);
    // /^B([+-])(\d?)([a-zA]+)$/
    if (m.size() >= 3 && m[0] == 'B' && (m[1] == '+' || m[1] == '-')) {
        std::size_t i = 2;
        int k = 1;
        if (i < m.size() && std::isdigit(static_cast<unsigned char>(m[i]))) { k = m[i] - '0'; i++; }
        const std::string q = m.substr(i);
        bool ok = !q.empty();
        for (char ch : q) if (!((ch >= 'a' && ch <= 'z') || ch == 'A')) ok = false;
        if (ok) {
            std::vector<Shot> out = m[1] == '-' ? std::vector<Shot>{E, AE} : std::vector<Shot>{P, NU};
            for (char ch : q) {
                if (ch == 'n') for (int j = 0; j < k; j++) out.push_back(NN);
                if (ch == 'p') for (int j = 0; j < k; j++) out.push_back(PP);
                if (ch == 'A') out.push_back(AL);
                if (ch == 'd') out.push_back({"d", "#e0506a", 5});
                if (ch == 't') out.push_back({"t", "#e0506a", 5});
            }
            return out;
        }
    }
    std::string a, el;
    if (clusterParts(m, a, el)) return {{el + "-" + a, "#d9a13a", 8}};
    return {};
}
std::string decShort(const std::string& m) {
    static const std::map<std::string, std::string> s = {
        {"A", "α"}, {"B-", "β⁻"}, {"B+", "β⁺ or electron capture"}, {"SF", "split in two (fission)"}, {"B-SF", "β⁻, then split in two"},
        {"B+SF", "β⁺/EC, then split in two"}, {"2B-", "double β⁻"}, {"2B+", "double β⁺"}, {"n", "neutron out"}, {"2n", "2 neutrons out"},
        {"3n", "3 neutrons out"}, {"p", "proton out"}, {"2p", "2 protons out"}, {"3p", "3 protons out"}};
    const auto it = s.find(m);
    return it != s.end() ? it->second : decWord(m);
}
static std::string pct(double v) { return v >= 1e-4 ? numText(v * 100, 2) : sci(v * 100); }
// one line about a nucleus: stable, or its half-life and how it decays
std::string isoLine(int Z, int N) {
    const auto d = decInfo(Z, N);
    const std::string nm = std::string(name(Z)) + "-" + std::to_string(Z + N);
    if (!d) return nm + ": " + isoInfo(Z, N);
    std::string modes;
    for (std::size_t i = 0; i < d->br.size() && i < 2; i++) {
        const auto& [m, v] = d->br[i];
        if (i) modes += ", ";
        modes += decWord(m) + (v < 0.9995 ? " " + pct(v) + " %" : "");
    }
    return nm + ": " + chainHalfLifeWords(Z, N) + " · decays by " + modes + (d->br.size() > 2 ? " …" : "") + (decUnquantified(Z, N).empty() ? "" : " · odds not complete (modes with no intensity left out)");
}
// 0.46.4 (outside review: every unbound nuclide was "~10⁻²² s", while NUBASE2020 gives B-9 800 zs and Cl-29 5.4 zs, and the decay line on the same screen showed them):
// the half-life of the lab's decay table (NUBASE2020, kDecayLines) where it has one, else no number
static double tableHalfLife(int Z, int N) {
    if (Z < 1 || Z > 118) return NAN;
    const auto& m = decTable()[Z];
    const auto it = m.find(N);
    if (it == m.end()) return NAN;
    const std::size_t sl = it->second.find('/');
    return sl == std::string::npos ? NAN : parseFloat(it->second.substr(0, sl));
}
static std::string unboundText(int Z, int N) {
    const double T = tableHalfLife(Z, N);
    return T > 0 ? "unbound: falls apart almost at once (half-life " + fmtLife(T) + ", NUBASE2020)" : std::string("unbound: falls apart almost at once (NUBASE2020 gives no time)");
}
// what this nucleus is like in real life
std::string isoInfo(int Z, int N) {
    const std::string* c = nucCode(Z, N);
    if (Z == 4 && N == 4) { const double T = tableHalfLife(4, 4); return "unbound: splits into two helium-4 nuclei" + (T > 0 ? " (half-life " + fmtLife(T) + ", NUBASE2020)" : std::string()); }
    if (!c) return "not in the table: NUBASE2020 lists no such nuclide";
    if (*c == "S") return "stable";
    if (*c == "U") return unboundText(Z, N);
    if (*c == "?") return chainHalfLifeWords(Z, N);   // 0.12 stage 2: what NUBASE2020 actually says (an extrapolation, a limit, an approximate value, nothing, p-unst); the old text called all 473 "seen only fleetingly"
    const double t = parseFloat((*c)[0] == '~' ? c->substr(1) : *c);
    if (t >= 1e15 * kYearS) return "practically stable (half-life " + fmtHalf(t) + ", far longer than the age of the universe)";
    return "radioactive (half-life " + fmtHalf(t) + ")";
}

// ---------------------------------------------------------------- mass
// total nuclear binding energy in MeV: measured (AME) where known, else the Bethe–Weizsäcker formula
Binding massBinding(int Z, int N) {
    const int A = Z + N;
    static std::vector<std::pair<int, std::vector<std::string>>> bea = [] {
        std::vector<std::pair<int, std::vector<std::string>>> v(119, {0, {}});
        for (const char* line : kBeaLines) {
            const auto parts = split(line, '|');
            v[std::atoi(parts[0].c_str())] = {std::atoi(parts[1].c_str()), split(parts[2], ',')};
        }
        return v;
    }();
    if (Z >= 1 && Z <= 118 && !bea[Z].second.empty()) {
        const int k = N - bea[Z].first;
        if (k >= 0 && k < static_cast<int>(bea[Z].second.size()) && !bea[Z].second[k].empty())
            return {std::strtod(bea[Z].second[k].c_str(), nullptr) * A / 1000, true};
    }
    if (A < 2) return {0, true};
    const double d = A % 2 ? 0 : (Z % 2 ? -1 : 1) * 11.18 / std::sqrt(static_cast<double>(A));
    // Bethe-Weizsacker coefficients (MeV): 15.75 volume, 17.8 surface, 0.711 Coulomb, 23.7 asymmetry, 11.18 pairing (/sqrt A): J. W. Rohlf, Modern Physics from
    // alpha to Z0 (Wiley, 1994), as tabulated in Wikipedia's 'Semi-empirical mass formula' (read 2026-11-05). Used only for nuclides AME2020 does not list.
    return {15.75 * A - 17.8 * std::pow(A, 2.0 / 3) - 0.711 * Z * (Z - 1) / std::pow(A, 1.0 / 3) - 23.7 * std::pow(A - 2 * Z, 2) / A + d, false};
}

// ---------------------------------------------------------------- temperature
std::optional<std::string> tempState(int Z, double T) {
    const Temp& t = tempData(Z);
    if (t.flag == 's') return std::string(T < t.boil ? "solid" : "gas");     // sublimes: solid → gas, no liquid at normal pressure
    if (t.flag == 'p') return std::string(T < t.boil ? "liquid" : "gas");    // helium: no solid at normal pressure
    if (std::isnan(t.melt)) return std::nullopt;
    if (T < t.melt) return std::string("solid");
    return std::string(std::isnan(t.boil) || T < t.boil ? "liquid" : "gas");
}
std::string fK(double k) { return k < 100 ? fmt::fixed(k, 1) : fmt::shortestNumber(roundHalfUp(k)); }
std::string fC(double k) { const double c = k - 273.15; return (c < 0 ? "−" : "") + fmt::localeNumber(std::fabs(roundHalfUp(c))); }
std::string fF(double k) { const double f = k * 9 / 5 - 459.67; return (f < 0 ? "−" : "") + fmt::localeNumber(std::fabs(roundHalfUp(f))); }
// one short line for the ⓘ readout
std::string tempLine(int Z) {
    const Temp& t = tempData(Z);
    const auto st = tempState(Z, kTRoom);
    auto c = [](double k) {
        if (fmt::imperial()) return fF(k) + " °F";
        std::string s = fmt::localeNumber(roundHalfUp(k - 273.15));
        const std::size_t i = s.find('-');
        if (i != std::string::npos) s.replace(i, 1, "−");
        return s + " °C";
    };
    if (std::isnan(t.melt) && std::isnan(t.boil)) return "melting/boiling point: never measured (too few atoms made)";
    std::string bits = t.flag == 's' ? "sublimes at " + c(t.boil)
        : (std::isnan(t.melt) ? std::string("no melting point at normal pressure") : "melts at " + c(t.melt) + (t.flag == 'e' ? " (predicted)" : "")) + ", " +
          (std::isnan(t.boil) ? std::string("boiling point not in the lab's source") : "boils at " + c(t.boil));
    return bits + (st ? " · " + *st + " at room temperature" : "");
}

std::string tAll(double k) {
    const double r = roundHalfUp(k);
    return "<b>" + (r >= 1000 ? fmt::localeNumber(r) : fK(k)) + " K</b> · " + fC(k) + " °C · " + fF(k) + " °F";
}
const char* stateIcon(const std::string& st) { return st == "solid" ? "🧊" : st == "liquid" ? "💧" : "☁️"; }

// ---------------------------------------------------------------- the element card and facts
std::string everyday(int Z) {
    if (kEveryday[Z]) return fmt::unitText(kEveryday[Z]);
    const std::string cat = categoryName(Z);
    if (cat == "lanthanide") return "One of the “rare earth” metals, used in magnets, lasers, screens and lamps.";
    if (Z >= 104) return "Made a few atoms at a time in particle accelerators. The atoms decay again quickly, often within seconds.";
    if (Z > 92 || cat == "actinide") return "Radioactive. Made in reactors or particle accelerators, or found only in tiny traces in nature.";
    return "You rarely meet this one directly, but it has its uses in industry and science.";
}
int periodOf(int Z) { return Z <= 2 ? 1 : Z <= 10 ? 2 : Z <= 18 ? 3 : Z <= 36 ? 4 : Z <= 54 ? 5 : Z <= 86 ? 6 : 7; }
std::vector<std::string> elementChips(int Z) {
    const std::string cat = categoryName(Z);
    const bool f = cat == "lanthanide" || cat == "actinide";
    const auto st = tempState(Z, kTRoom);
    std::vector<std::string> chips = {categoryShown(Z), f ? "period " + std::to_string(periodOf(Z)) : "period " + std::to_string(periodOf(Z)) + " · group " + std::to_string(ptPos(Z).second),
                                      st ? std::string(stateIcon(*st)) + " " + *st + " at room temperature" : "state at room temperature unknown"};
    if (!hasStable(Z)) chips.push_back("☢ radioactive: no stable isotope");
    return chips;
}
std::string factsLinesHtml(int Z) {
    const Temp& t = tempData(Z);
    const double m = t.melt, b = t.boil;
    const char f = t.flag;
    const auto st = tempState(Z, kTRoom);
    if (std::isnan(m) && std::isnan(b))
        return std::string("<div><b>Melting and boiling point:</b> unknown. Only a few atoms of ") + name(Z) + " have ever been made, so nobody has measured them (heavy elements like this are usually predicted to be solid).</div>";
    std::string L;
    if (f == 's') L += "<div><b>Melting point:</b> none at normal pressure. It <b>sublimes</b> (solid → gas) at " + tAll(b) + ".</div>";
    else {
        L += "<div><b>Melting point:</b> " + (std::isnan(m) ? std::string(f == 'p' ? "none at normal pressure (it needs squeezing to about 25 atmospheres to freeze)" : "unknown")
                                                       : tAll(m) + (f == 'e' ? " (predicted, never measured)" : "")) + "</div>";
        L += "<div><b>Boiling point:</b> " + (std::isnan(b) ? std::string("not in the lab's source (CRC Handbook)") : tAll(b)) + "</div>";
    }
    std::string dens;
    const double d = t.density;
    if (!std::isnan(d) && d != 0) {
        if (d < 0.01) dens = " · density " + fmt::fixed(d * 1000, 2) + " g/L (as a gas at 25 °C)";
        else {
            std::string v = fmt::toPrecision(d, 3);
            std::size_t e = v.size();
            while (e > 0 && v[e - 1] == '0') e--;
            if (e < v.size()) { if (e > 0 && v[e - 1] == '.') e--; v = v.substr(0, e); }
            dens = " · density " + v + " g/cm³";
        }
    }
    L += std::string("<div><b>At room temperature (") + (fmt::imperial() ? "68 °F" : "20 °C") + "):</b> " + (st ? std::string(stateIcon(*st)) + " " + *st : std::string("unknown")) + dens + "</div>";
    return L;
}
std::string factsNotesHtml(int Z) {
    const char f = tempData(Z).flag;
    std::vector<std::string> notes;
    if (f == 'a') notes.push_back(std::string(name(Z)) + " has several forms (allotropes); these numbers are for " + (Z == 15 ? std::string("white phosphorus") : std::string("monoclinic sulfur, which ordinary yellow (rhombic) sulfur turns into at 95 °C, before it melts")) + ".");
    if (f == 'p') notes.push_back("Helium is the only element that stays liquid at absolute zero unless squeezed.");
    if (Z == 6) notes.push_back("Only under enormous pressure does carbon melt.");
    if (Z == 33) notes.push_back("Under pressure arsenic melts at about 1,090 K.");
    if (Z == 80 || Z == 35) notes.push_back(std::string(name(Z)) + " is one of only two elements that are liquid at room temperature.");
    if (Z == 31 || Z == 55) notes.push_back(std::string(name(Z)) + " melts just above room temperature: it would melt in your hand or on a hot day.");
    std::string out;
    for (const std::string& n : notes) out += (out.empty() ? "" : " ") + n;
    return out;
}
const char* const kFactsFootnote = "Values are for a lump of the element at normal air pressure, not for the single atom drawn here.";

}  // namespace atom
