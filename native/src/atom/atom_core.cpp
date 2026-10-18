#include "atom_core.h"
#include "decay_chain.h"
#include "outline.h"
#include "radii.h"
#include "quadrupole.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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

// ---------------------------------------------------------------- small JavaScript helpers
// parseFloat: the number at the start of the text, NaN when there is none
static double parseFloat(const std::string& s) {
    const char* b = s.c_str();
    char* e = nullptr;
    const double v = std::strtod(b, &e);
    return e == b ? NAN : v;
}
// Math.round: halves go up, and a small negative rounds to −0
static double jsRound(double x) {
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
std::string jsNum(double x, int digits) { return fmt::jsString(std::strtod(fmt::jsToPrecision(x, digits).c_str(), nullptr)); }

// ---------------------------------------------------------------- element and nuclide data
const char* name(int Z) { return Z >= 0 && Z <= 118 ? kName[Z] : ""; }
const char* symbol(int Z) { return Z >= 0 && Z <= 118 ? kSymbol[Z] : ""; }
int defaultN(int Z) { return kDefaultN[Z]; }

// NUC: parsed once from the web's lines "Z|neutrons:code,..."
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
    const int e = static_cast<int>(std::floor(std::log10(x)));
    const double m = x / std::pow(10.0, e);
    return fmt::fixed(m, 1) + "×10" + supDigits(e);
}
std::string metres(double x) {
    int e = static_cast<int>(std::floor(std::log10(x) + 1e-9));
    double m = jsRound(x / std::pow(10.0, e) * 10) / 10;
    if (m >= 10) { m /= 10; e++; }
    return (m == 1 ? std::string("10") : fmt::jsString(m) + "×10") + supDigits(e) + " m";
}
std::string fmtLam(double l) { return l < 0.1 ? fmt::fixed(l * 1000, 1) + " pm" : fmt::fixed(l, l < 100 ? 1 : 0) + " nm"; }
std::string fmtLen(double a) { return a < 0.1 ? fmt::fixed(a * 100, a < 0.01 ? 2 : 1) + " pm" : fmt::fixed(a, 2) + " Å"; }
std::string fmtT(double as) { return as < 1 ? fmt::fixed(as * 1000, as < 0.01 ? 1 : 0) + " zs" : fmt::fixed(as, 0) + " as"; }
std::string fmtEV(double e) {
    return e >= 1e5 ? fmt::fixed(e / 1e3, 0) + " keV" : e >= 1e3 ? fmt::fixed(e / 1e3, 1) + " keV" : fmt::fixed(e, 2) + " eV";
}
std::string fmtHalf(double t, int sf) {
    auto num = [&](double x, const char* u, bool pl) {
        const double v = std::strtod(fmt::jsToPrecision(x, sf).c_str(), nullptr);
        return fmt::jsLocale(v) + " " + u + (pl && v != 1 ? "s" : "");
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
    if (x >= 0.1 && x < 10000) return x < 10 ? fmt::fixed(x, 1) : fmt::jsLocale(jsRound(x));
    const int e = static_cast<int>(std::floor(std::log10(x)));
    return fmt::fixed(x / std::pow(10.0, e), 1) + "×10" + supAbs(e);
}
std::string lrnSci3(double x, int d) {
    if (!std::isfinite(x) || x <= 0) return "0";
    const int e = static_cast<int>(std::floor(std::log10(x)));
    if (e >= -2 && e < 4) {
        std::string s = fmt::jsToPrecision(x, d);
        if (s.find('.') != std::string::npos) {   // drop trailing zeros, then a trailing point
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        return s.find('e') != std::string::npos ? s : fmt::jsLocale(std::strtod(s.c_str(), nullptr), 6);
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
        for (int n = 1; n <= 4; n++)
            for (int l = 0; l < n; l++)
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
void jumpFractions(std::complex<double> A, std::complex<double> B, double theta, double phi, double& fA, double& fB) {
    const double a2 = std::norm(A), b2 = std::norm(B);
    const double rho = std::norm(std::cos(theta) * A + std::sin(theta) * B * std::exp(std::complex<double>(0, -phi))), lam = std::cos(theta) * std::cos(theta);
    const double aFirstA = a2 > 0 ? std::min(1.0, rho / a2) : 0, aFirstB = b2 > 0 ? std::max(0.0, (rho - a2 * aFirstA) / b2) : 0;     // the old dots fill first
    const double bFirstB = b2 > 0 ? std::min(1.0, rho / b2) : 0, bFirstA = a2 > 0 ? std::max(0.0, (rho - b2 * bFirstB) / a2) : 0;     // the new dots fill first
    fA = std::max(0.0, std::min(1.0, lam * aFirstA + (1 - lam) * bFirstA));
    fB = std::max(0.0, std::min(1.0, lam * aFirstB + (1 - lam) * bFirstB));
}
JumpTerms jumpTerms(double theta, double phi, double u) {
    JumpTerms T;
    T.cth = std::cos(theta); T.sth = std::sin(theta); T.lam = T.cth * T.cth;
    T.em = std::exp(std::complex<double>(0, -phi)); T.ep = std::exp(std::complex<double>(0, phi * u));
    return T;
}
void jumpFractions(std::complex<double> A, std::complex<double> B, const JumpTerms& T, double& fA, double& fB) {   // (the same lines as above, in the same order)
    const double a2 = std::norm(A), b2 = std::norm(B);
    const double rho = std::norm(T.cth * A + T.sth * B * T.em), lam = T.lam;
    const double aFirstA = a2 > 0 ? std::min(1.0, rho / a2) : 0, aFirstB = b2 > 0 ? std::max(0.0, (rho - a2 * aFirstA) / b2) : 0;
    const double bFirstB = b2 > 0 ? std::min(1.0, rho / b2) : 0, bFirstA = a2 > 0 ? std::max(0.0, (rho - b2 * bFirstB) / a2) : 0;
    fA = std::max(0.0, std::min(1.0, lam * aFirstA + (1 - lam) * bFirstA));
    fB = std::max(0.0, std::min(1.0, lam * aFirstB + (1 - lam) * bFirstB));
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
// 1,500 evenly spread points (a Fibonacci sphere), as in the web.
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
// wavelength (nm) -> colour; UV and IR get stand-in colours
Rgb lamColor(double l) {
    if (l < kVisibleLoNm) return {0xb0, 0x6c, 0xff};
    if (l > kVisibleHiNm) return {0xd2, 0x4a, 0x3c};
    double r = 0, g = 0, b = 0;
    if (l < 440) { r = (440 - l) / 60; b = 1; }
    else if (l < 490) { g = (l - 440) / 50; b = 1; }
    else if (l < 510) { g = 1; b = (510 - l) / 20; }
    else if (l < 580) { r = (l - 510) / 70; g = 1; }
    else if (l < 645) { r = 1; g = (645 - l) / 65; }
    else r = 1;
    const double f = l < 420 ? 0.3 + 0.7 * (l - kVisibleLoNm) / 40 : l > 700 ? 0.3 + 0.7 * (kVisibleHiNm - l) / 80 : 1;   // the dim ends: 40 nm above the low edge, 80 nm below the high one
    auto c = [&](double v) { return static_cast<int>(jsRound(255 * std::pow(v * f, 0.8))); };
    return {c(r), c(g), c(b)};
}
std::string lamColorCss(double l) {
    if (l < kVisibleLoNm) return "#b06cff";
    if (l > kVisibleHiNm) return "#d24a3c";
    const Rgb c = lamColor(l);
    return "rgb(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")";
}
const char* bandTag(double nm) { return nm < 10 ? " (X-ray)" : nm < kVisibleLoNm ? " (UV)" : nm > kVisibleHiNm ? " (IR)" : ""; }
std::vector<SpecLine> visibleLines(int Z, int N) { std::vector<SpecLine> v; for (const SpecLine& l : specLines(Z, N)) if (l.hi != 0 && isVisibleNm(l.lam)) v.push_back(l); return v; }
double nuclearR(int A) { return A == 1 ? kNucleonR : 1.2 * std::cbrt(static_cast<double>(A)); }   // r = 1.2 A^(1/3) fm

// ---------------------------------------------------------------- the nucleus
static const BetaKnown* betaKnown(int Z, int A) {
    for (const BetaKnown& b : kBetaKnown) if (b.Z == Z && b.A == A) return &b;
    return nullptr;
}
// quadrupole deformation β₂: + stretched (rugby ball), − flattened (pancake)
double nucBeta(int Z, int N) {
    const int A = Z + N;
    if (A < 12) return 0;
    int dZ = 1 << 30, dN = 1 << 30;
    for (int m : kMagicZ) dZ = std::min(dZ, std::abs(Z - m));
    for (int m : kMagicN) dN = std::min(dN, std::abs(N - m));
    if (const BetaKnown* k = betaKnown(Z, A)) {   // β₂ = 4π / (3 Z R0²) · √B(E2↑)
        const double R0 = 1.2 * std::cbrt(static_cast<double>(A));
        return k->sign * 4 * kPi / (3 * Z * R0 * R0) * std::sqrt(k->be2 * 1e4);
    }
    if (dZ == 0 && dN == 0) return 0;              // doubly magic: round
    const double P = static_cast<double>(dZ) * dN / (dZ + dN);   // Casten's P-factor: valence protons × neutrons / their sum
    return A <= 40 ? 0.6 * (1 - std::exp(-P / 1.5)) : 0.35 * (1 - std::exp(-P / 3));
}
bool nucUsesUnsourcedMagic(int Z, int N) {
    if (Z + N < 12 || betaKnown(Z, Z + N)) return false;                  // the same cases nucBeta decides by measurement or leaves round
    int dZ = 1 << 30, dN = 1 << 30, dZs = 1 << 30, dNs = 1 << 30;         // nearest magic number with and without the unsourced ones (the last entry of each list)
    const int nZ = int(std::size(kMagicZ)), nN = int(std::size(kMagicN));
    for (int i = 0; i < nZ; i++) { dZ = std::min(dZ, std::abs(Z - kMagicZ[i])); if (i + 1 < nZ) dZs = std::min(dZs, std::abs(Z - kMagicZ[i])); }
    for (int i = 0; i < nN; i++) { dN = std::min(dN, std::abs(N - kMagicN[i])); if (i + 1 < nN) dNs = std::min(dNs, std::abs(N - kMagicN[i])); }
    return dZ != dZs || dN != dNs;
}
bool nucBE2Known(int Z, int N) { return betaKnown(Z, Z + N) != nullptr; }   // the lab's own table of measured B(E2; 0+ -> 2+) values has this isotope
bool nucMeasured(int Z, int N) {
    if (betaKnown(Z, Z + N)) return true;
    return std::find(std::begin(kMagicZ), std::end(kMagicZ), Z) != std::end(kMagicZ) &&
           std::find(std::begin(kMagicN), std::end(kMagicN), N) != std::end(kMagicN);
}
const char* nucShapeWord(double b) {
    return b >= 0.15 ? "stretched like a rugby ball" : b <= -0.15 ? "flattened like a pancake"
         : b >= 0.07 || b <= -0.07 ? "slightly deformed" : "nearly round";
}
// Nucleon positions are sampled from the real density profile (Woods–Saxon, as in heavy-ion
// Glauber Monte Carlo codes; a Gaussian for light nuclei), with a hard core so nucleons do not
// overlap, a neutron skin and the measured quadrupole deformation. One possible snapshot: really,
// every nucleon is a wave spread through the nucleus. Worked out in fm, stored in metres (kFmLab).
static std::vector<Nucleon> buildNucleusImpl(int Z, int N, bool measured, bool newShape, bool tilt = true) {
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
    if (A == 2) {                                  // the deuteron is a loose pair
        const double a = random() * 6.28, c = std::cos(a), sn = std::sin(a);
        // half the distance of the pair: 1.6 fm by the rule; measured: the proton's own distance to the centre is d/2, so the charge radius is sqrt((d/2)^2 + r_p^2) and d/2 = sqrt(rms^2 - r_p^2)
        const double h = measured ? std::sqrt(std::max(0.5, chargeRadius(Z, N)->rms * chargeRadius(Z, N)->rms - kNucleonR * kNucleonR)) : 1.6;
        out.push_back({h * c, h * sn, 0, types[0] != 0, 0});
        out.push_back({-h * c, -h * sn, 0, types[1] != 0, 1.7});
        return toMetres();
    }
    // The shape (0.10.9 stage 2). newShape: the shape of nucShape (a measured quadrupole moment where it can be used, else the lab's B(E2) value or estimate), drawn as the surface r = R f(cos theta) of quadrupole.h
    // whose VOLUME is kept for every beta2; the rule's layout (newShape false) is the web's: beta2 from nucBeta, the radius stretched by (1 + beta2 Y20) and a Gaussian's z by (1 + beta2), volume not kept.
    const Shape shp = newShape ? nucShape(Z, N) : Shape{};
    const double beta = newShape ? shp.beta : nucBeta(Z, N), cA = std::cbrt(static_cast<double>(A)), a = 0.54;
    const double skin = std::max(-0.3, std::min(0.6, 1.7 * (N - Z) / A));   // neutron minus proton radius (measured 0.28 fm for lead-208)
    const bool gauss = A <= 16;
    // The size. By the rule: R0 = 1.12 A^(1/3) - 0.86 / A^(1/3), and for A <= 16 a Gaussian of rms 1.76 (A = 3) or 1.68 + 0.085 (A - 4) fm. Measured (0.10.9 stage 1, radii.h): the sampling starts from the measured
    // size (protons' density rms^2 = measured rms^2 - r_p^2, r_p = 0.84 fm, the lab's nucleon radius; the neutrons keep their skin), and after the nucleons are placed and centred the whole snapshot is scaled about the
    // centre of mass so that its PROTONS' rms charge radius is the measured one EXACTLY. The scale absorbs what the sampling adds on its own: the hard core between nucleons spreads them out, re-centring on the
    // centroid pulls them in (by (A - 1) / A), and a deformed nucleus' rms grows with beta2^2. Each picture is one random snapshot (as before) with the measured size; the rule's own layout is not scaled.
    double R0 = 1.12 * cA - 0.86 / cA;
    double rms = A == 3 ? 1.76 : 1.68 + 0.085 * (A - 4), sg = std::sqrt(std::max(0.5, rms * rms - 0.84 * 0.84)) / std::sqrt(3.0);
    // The snapshot is also made EXACT in its protons' quadrupole when the size is exact and the shape is not round (below): the protons' <x^2> = <y^2> and <z^2> are scaled to the values of the uniformly charged body of
    // quadrupole.h (<3 z^2 - r^2> / <r^2> = shapeMomentRatio(beta2)); the trace, so the rms radius, is unchanged. The size is exact for a measured radius, and for a shape from a measured moment (then at the rule's rms).
    const bool exact = measured || (newShape && shp.src == ShapeSource::MeasuredMoment);
    double chargeTarget = exact ? pointProtonMs(Z, N) : 0;                   // the protons' mean square radius about the centre of mass the picture must have, fm^2 (the measured rms^2 - r_p^2, or the rule ball's)
    if (measured) {
        const double m = chargeRadius(Z, N)->rms;
        chargeTarget = std::max(0.1, m * m - kNucleonR * kNucleonR);
        if (gauss) sg = std::sqrt(std::max(0.5, chargeTarget)) / std::sqrt(3.0);
        else R0 = wsHalfDensityRadiusForChargeRms(m) + 0.5 * skin;          // protons sit at R0 - skin / 2 = the solved radius
    }
    double dmin = 1.0;
    for (int i = 0; i < A; i++) {
        const bool isP = types[i] != 0;
        const double Ri = R0 + (isP ? -0.5 : 0.5) * skin;
        bool found = false;
        double px = 0, py = 0, pz = 0;
        for (int tries = 0; !found; tries++) {
            double x, y, z;
            if (gauss) {
                x = sg * gs(); y = sg * gs(); z = sg * gs();
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
    double mx = 0, my = 0, mz = 0;
    for (const Nucleon& q : out) { mx += q.x; my += q.y; mz += q.z; }
    mx /= A; my /= A; mz /= A;
    double fx = 1, fy = 1, fz = 1;                 // exact: the scales that give the protons exactly the wanted mean square radius about the centre of mass (and, for a deformed nucleus, the wanted quadrupole)
    if (exact) {
        double sxx = 0, syy = 0, szz = 0; int np = 0;
        for (const Nucleon& q : out) if (q.isP) { sxx += (q.x - mx) * (q.x - mx); syy += (q.y - my) * (q.y - my); szz += (q.z - mz) * (q.z - mz); np++; }
        if (np > 0 && sxx + syy + szz > 0) {
            fx = fy = fz = std::sqrt(chargeTarget * np / (sxx + syy + szz));
            if (newShape && beta != 0 && np >= 2 && sxx > 0 && syy > 0 && szz > 0) {
                const double ratio = shapeMomentRatio(beta), tx = chargeTarget * (1 - ratio / 2) / 3, tz2 = chargeTarget * (1 + ratio) / 3;   // <x^2> = <y^2>, <z^2> of the body; 2 tx + tz2 = chargeTarget
                fx = std::sqrt(tx * np / sxx); fy = std::sqrt(tx * np / syy); fz = std::sqrt(tz2 * np / szz);
            }
        }
    }
    // tilt the symmetry axis so the shape shows from the side
    const double ty = tilt ? 0.95 : 0, tz = tilt ? random() * 6.28 : 0, c1 = std::cos(ty), s1 = std::sin(ty), c2 = std::cos(tz), s2 = std::sin(tz);
    for (Nucleon& q : out) {
        const double x = (q.x - mx) * fx, y = (q.y - my) * fy, z = (q.z - mz) * fz, x1 = x * c1 + z * s1, z1 = -x * s1 + z * c1;
        q.x = x1 * c2 - y * s2; q.y = x1 * s2 + y * c2; q.z = z1;
    }
    return toMetres();
}
std::vector<Nucleon> buildNucleus(int Z, int N) { return buildNucleusImpl(Z, N, sizedByMeasurement(Z, N), true); }
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
static std::string pct(double v) { return v >= 1e-4 ? jsNum(v * 100, 2) : sci(v * 100); }
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
static const char* kUnbound = "unbound: falls apart almost instantly (~10⁻²² s)";
// what this nucleus is like in real life
std::string isoInfo(int Z, int N) {
    const std::string* c = nucCode(Z, N);
    if (Z == 4 && N == 4) return "unbound: splits into two helium-4 nuclei in ~10⁻¹⁶ s";
    if (!c) return "not in the table: NUBASE2020 lists no such nuclide";
    if (*c == "S") return "stable";
    if (*c == "U") return kUnbound;
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
std::string fK(double k) { return k < 100 ? fmt::fixed(k, 1) : fmt::jsString(jsRound(k)); }
std::string fC(double k) { const double c = k - 273.15; return (c < 0 ? "−" : "") + fmt::jsLocale(std::fabs(jsRound(c))); }
std::string fF(double k) { const double f = k * 9 / 5 - 459.67; return (f < 0 ? "−" : "") + fmt::jsLocale(std::fabs(jsRound(f))); }
// one short line for the ⓘ readout
std::string tempLine(int Z) {
    const Temp& t = tempData(Z);
    const auto st = tempState(Z, kTRoom);
    auto c = [](double k) {
        if (fmt::imperial()) return fF(k) + " °F";
        std::string s = fmt::jsLocale(jsRound(k - 273.15));
        const std::size_t i = s.find('-');
        if (i != std::string::npos) s.replace(i, 1, "−");
        return s + " °C";
    };
    if (std::isnan(t.melt) && std::isnan(t.boil)) return "melting/boiling point: never measured (too few atoms made)";
    std::string bits = t.flag == 's' ? "sublimes at " + c(t.boil)
        : (std::isnan(t.melt) ? std::string("no melting point at normal pressure") : "melts at " + c(t.melt)) + ", " +
          (std::isnan(t.boil) ? std::string("boiling point unknown") : "boils at " + c(t.boil));
    return bits + (st ? " · " + *st + " at room temperature" : "");
}

std::string tAll(double k) {
    const double r = jsRound(k);
    return "<b>" + (r >= 1000 ? fmt::jsLocale(r) : fK(k)) + " K</b> · " + fC(k) + " °C · " + fF(k) + " °F";
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
    std::vector<std::string> chips = {cat, f ? "period " + std::to_string(periodOf(Z)) : "period " + std::to_string(periodOf(Z)) + " · group " + std::to_string(ptPos(Z).second),
                                      st ? std::string(stateIcon(*st)) + " " + *st + " at room temperature" : "state at room temperature unknown"};
    if (!hasStable(Z)) chips.push_back("☢ radioactive: no stable isotope");
    return chips;
}
std::string factsLinesHtml(int Z) {
    const Temp& t = tempData(Z);
    const double m = t.melt, b = t.boil;
    const char f = t.flag;
    const std::string est = f == 'e' ? " (estimated)" : "";
    const auto st = tempState(Z, kTRoom);
    if (std::isnan(m) && std::isnan(b))
        return std::string("<div><b>Melting and boiling point:</b> unknown. Only a few atoms of ") + name(Z) + " have ever been made, so nobody has measured them (heavy elements like this are usually predicted to be solid).</div>";
    std::string L;
    if (f == 's') L += "<div><b>Melting point:</b> none at normal pressure. It <b>sublimes</b> (solid → gas) at " + tAll(b) + ".</div>";
    else {
        L += "<div><b>Melting point:</b> " + (std::isnan(m) ? std::string(f == 'p' ? "none at normal pressure (it needs squeezing to about 25 atmospheres to freeze)" : "unknown")
                                                       : tAll(m) + (f == 'e' && Z != 85 ? est : "")) + "</div>";
        L += "<div><b>Boiling point:</b> " + (std::isnan(b) ? std::string("unknown") : tAll(b) + est) + "</div>";
    }
    std::string dens;
    const double d = t.density;
    if (!std::isnan(d) && d != 0) {
        if (d < 0.01) dens = " · density " + fmt::fixed(d * 1000, 2) + " g/L (as a gas)";
        else {
            std::string v = fmt::jsToPrecision(d, 3);   // then the web's .replace(/\.?0+$/, '')
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
    if (f == 'a') notes.push_back(std::string(name(Z)) + " has several forms (allotropes); these numbers are for " + (Z == 15 ? "white phosphorus" : "ordinary yellow (α) sulfur") + ".");
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
