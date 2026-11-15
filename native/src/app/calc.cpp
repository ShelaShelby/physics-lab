// The calculator (0.49): see calc.h for the rules. No graphics.
#include "calc.h"

#include <algorithm>
#include <charconv>
#include <complex>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <system_error>

#include "atom/atom_core.h"
#include "atom/hfs.h"
#include "atom/threej.h"
#include "orbit/units.h"
#include <limits>

namespace calc {

const char* const kBaseSymbol[kBaseCount] = {"kg", "m", "s", "A", "K", "mol"};

namespace {

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- fractions and dimensions
struct Err { std::string msg; int pos, len; };
[[noreturn]] void fail(const std::string& msg, int pos = -1, int len = 0) { throw Err{msg, pos, len}; }

Rat norm(long long n, long long d) {
    if (d < 0) { n = -n; d = -d; }
    const long long g = std::gcd(n < 0 ? -n : n, d);
    if (g > 1) { n /= g; d /= g; }
    if (n == 0) d = 1;
    if (n > 1000000 || n < -1000000 || d > 1000000) fail("The units' powers became too large.");
    return Rat{n, d};
}
Rat add(Rat a, Rat b) { return norm(a.n * b.d + b.n * a.d, a.d * b.d); }
Rat mul(Rat a, Rat b) { return norm(a.n * b.n, a.d * b.d); }
Dim dimAdd(const Dim& a, const Dim& b, int sign) {
    Dim r;
    for (int i = 0; i < kBaseCount; i++) r.e[i] = add(a.e[i], Rat{sign * b.e[i].n, b.e[i].d});
    return r;
}
Dim dimScale(const Dim& a, Rat k) { Dim r; for (int i = 0; i < kBaseCount; i++) r.e[i] = mul(a.e[i], k); return r; }
double ratValue(Rat r) { return double(r.n) / double(r.d); }

// p as a simple fraction n/d with d <= 12 (1/2, 1/3, 2/3, ...), if it is one
bool ratFromDouble(double p, Rat& out) {
    if (!std::isfinite(p) || std::fabs(p) > 1e6) return false;
    for (long long d = 1; d <= 12; d++) {
        const double x = p * double(d), k = std::nearbyint(x);
        if (std::fabs(x - k) <= 1e-9 * std::max(1.0, std::fabs(x))) { out = norm(static_cast<long long>(k), d); return true; }
    }
    return false;
}

// ---------------------------------------------------------------- UTF-8
// The code point at s[i] and its length in bytes; 0xFFFD with length 1 for a broken sequence.
unsigned decode(const std::string& s, std::size_t i, int& len) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    auto cont = [&](std::size_t k) { return k < s.size() && (static_cast<unsigned char>(s[k]) & 0xC0) == 0x80; };
    if (c < 0x80) { len = 1; return c; }
    if ((c & 0xE0) == 0xC0 && cont(i + 1)) { len = 2; return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); }
    if ((c & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) {
        len = 3; return ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
        len = 4;
        return ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
    }
    len = 1; return 0xFFFD;
}
std::string encode(unsigned cp) {
    std::string o;
    if (cp < 0x80) o += char(cp);
    else if (cp < 0x800) { o += char(0xC0 | (cp >> 6)); o += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += char(0xE0 | (cp >> 12)); o += char(0x80 | ((cp >> 6) & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
    else { o += char(0xF0 | (cp >> 18)); o += char(0x80 | ((cp >> 12) & 0x3F)); o += char(0x80 | ((cp >> 6) & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
    return o;
}
// letters that may be part of a name besides a-z, A-Z and _ (and digits after the first character)
bool nameLetter(unsigned cp) {
    switch (cp) {
        case 0xB5: case 0x3BC:      // µ μ (micro)
        case 0xC5: case 0x212B:     // Å (angstrom)
        case 0x3A9: case 0x2126:    // Ω (ohm)
        case 0xB0:                  // ° (degree)
        case 0x127:                 // ħ
        case 0x3C0:                 // π
        case 0x3B1:                 // α
        case 0x3B5:                 // ε
        case 0x3C3:                 // σ
        case 0x3BB:                 // λ (0.50.1: λC)
        case 0x3A6:                 // Φ (0.50.1: Φ0)
            return true;
        default: return false;
    }
}
unsigned canonical(unsigned cp) { return cp == 0xB5 ? 0x3BC : cp == 0x212B ? 0xC5 : cp == 0x2126 ? 0x3A9 : cp; }   // one spelling for each look-alike pair
bool spaceChar(unsigned cp) { return cp == ' ' || cp == '\t' || cp == 0xA0 || cp == 0x2009 || cp == 0x200A || cp == 0x202F; }
int superDigit(unsigned cp) {
    switch (cp) {
        case 0x2070: return 0; case 0xB9: return 1; case 0xB2: return 2; case 0xB3: return 3;
        case 0x2074: return 4; case 0x2075: return 5; case 0x2076: return 6; case 0x2077: return 7; case 0x2078: return 8; case 0x2079: return 9;
        default: return -1;
    }
}
const char* const kSup[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
std::string supInt(long long n) {
    std::string o = n < 0 ? "⁻" : "";
    const std::string d = std::to_string(n < 0 ? -n : n);
    for (char c : d) o += kSup[c - '0'];
    return o;
}

// ---------------------------------------------------------------- the symbols
struct Prefix { const char* p; double f; const char* word; };
const Prefix kPrefixes[] = {
    {"Y", 1e24, "yotta"}, {"Z", 1e21, "zetta"}, {"E", 1e18, "exa"}, {"P", 1e15, "peta"}, {"T", 1e12, "tera"}, {"G", 1e9, "giga"}, {"M", 1e6, "mega"},
    {"k", 1e3, "kilo"}, {"h", 1e2, "hecto"}, {"d", 1e-1, "deci"}, {"c", 1e-2, "centi"}, {"m", 1e-3, "milli"}, {"μ", 1e-6, "micro"}, {"u", 1e-6, "micro"},
    {"n", 1e-9, "nano"}, {"p", 1e-12, "pico"}, {"f", 1e-15, "femto"}, {"a", 1e-18, "atto"}, {"z", 1e-21, "zepto"}, {"y", 1e-24, "yocto"}};

struct Table {
    std::vector<Symbol> list;
    std::map<std::string, std::size_t> index;
    void put(const Symbol& s) {
        if (index.count(s.name)) { std::fprintf(stderr, "calc: the name %s is defined twice\n", s.name.c_str()); std::abort(); }   // never: the tests build the table
        index[s.name] = list.size();
        list.push_back(s);
    }
};

const char* const kSI = "exact, by the definition of the SI (2019)";
const char* const kCodata = "CODATA 2022 (Mohr et al., arXiv:2409.03787)";

Table makeTable() {
    using namespace atom;
    Table t;
    const auto unit = [&](const std::string& name, double v, Dim d, const std::string& meaning, const std::string& source = "", bool listed = true) {
        Symbol s; s.name = name; s.kind = SymKind::Unit; s.q = {v, d}; s.meaning = meaning; s.source = source; s.listed = listed; t.put(s);
    };
    const auto constant = [&](const std::string& name, double v, Dim d, const std::string& meaning, const std::string& source, const std::string& published, bool listed = true) {
        Symbol s; s.name = name; s.kind = SymKind::Constant; s.q = {v, d}; s.meaning = meaning; s.source = source; s.published = published; s.listed = listed; t.put(s);
    };
    // a unit and its prefixed forms ("" = every prefix in kPrefixes); the prefixed forms are not listed except the few named in `shown`
    const auto prefixed = [&](const std::string& name, double v, Dim d, const std::string& meaning, const std::set<std::string>& allowed, const std::set<std::string>& shown,
                              const std::string& source = "") {
        unit(name, v, d, meaning, source, true);
        for (const Prefix& p : kPrefixes) {
            if (!allowed.empty() && !allowed.count(p.p)) continue;
            const std::string n = std::string(p.p) + name;
            unit(n, name == "g" ? p.f / 1000 : v * p.f, d, std::string(p.word) + meaning, "", shown.count(n) > 0);
        }
    };
    const Dim none{}, L = dimOf(0, 1, 0), M = dimOf(1, 0, 0), T = dimOf(0, 0, 1), I = dimOf(0, 0, 0, 1), TH = dimOf(0, 0, 0, 0, 1), N = dimOf(0, 0, 0, 0, 0, 1);
    const Dim energy = dimOf(1, 2, -2), force = dimOf(1, 1, -2), power = dimOf(1, 2, -3), pressure = dimOf(1, -1, -2), charge = dimOf(0, 0, 1, 1);
    const Dim volt = dimOf(1, 2, -3, -1), ohm = dimOf(1, 2, -3, -2), tesla = dimOf(1, 0, -2, -1), speed = dimOf(0, 1, -1);
    const std::set<std::string> all;
    const std::set<std::string> small = {"m", "μ", "u", "n"};

    // ---- the SI base units and the derived units with names (SI Brochure, 9th edition, 2019) ----
    prefixed("m", 1, L, "metre", all, {"km", "cm", "mm", "μm", "nm", "pm", "fm"});
    prefixed("g", 1e-3, M, "gram", all, {"kg", "mg"});
    prefixed("s", 1, T, "second", all, {"ms", "μs", "ns", "fs", "as"});
    prefixed("A", 1, I, "ampere", all, {"mA"});
    prefixed("K", 1, TH, "kelvin", {"m", "μ", "u", "n", "k"}, {});
    prefixed("mol", 1, N, "mole", {"m", "μ", "u", "n", "k"}, {});
    // (0.50.1: no μN or uN for the micronewton: μN is the nuclear magneton, as in every QM text; write 1e-6 N)
    prefixed("N", 1, force, "newton", {"Y", "Z", "E", "P", "T", "G", "M", "k", "h", "d", "c", "m", "n", "p", "f", "a", "z", "y"}, {"kN"});
    prefixed("J", 1, energy, "joule", all, {"kJ", "MJ"});
    prefixed("W", 1, power, "watt", all, {"kW", "MW"});
    prefixed("Pa", 1, pressure, "pascal", all, {"kPa", "hPa"});
    prefixed("C", 1, charge, "coulomb", all, {});
    prefixed("V", 1, volt, "volt", all, {"kV"});
    prefixed("Ω", 1, ohm, "ohm", {"k", "M", "m", "μ", "u"}, {"kΩ"});
    unit("ohm", 1, ohm, "ohm", "", false);
    prefixed("T", 1, tesla, "tesla", small, {});
    prefixed("Hz", 1, dimOf(0, 0, -1), "hertz", all, {"kHz", "MHz", "GHz"});
    // ---- units accepted for use with the SI (SI Brochure, table 8) and other exact definitions ----
    unit("min", 60, T, "minute = 60 s");
    unit("hr", 3600, T, "hour = 3600 s");
    unit("hour", 3600, T, "hour = 3600 s", "", false);
    unit("d", 86400, T, "day = 86400 s");
    unit("day", 86400, T, "day = 86400 s", "", false);
    prefixed("yr", 365.25 * 86400, T, "year (Julian year, 365.25 days)", {"k", "M", "G"}, {"Myr", "Gyr"}, "IAU: the Julian year of 365.25 days of 86400 s");
    unit("year", 365.25 * 86400, T, "year (Julian year, 365.25 days)", "IAU: the Julian year of 365.25 days of 86400 s", false);
    prefixed("L", 1e-3, dimOf(0, 3, 0), "litre = 0.001 m³", {"m", "μ", "u", "c", "d"}, {"mL"});
    unit("Å", 1e-10, L, "ångström = 10⁻¹⁰ m");
    unit("angstrom", 1e-10, L, "ångström = 10⁻¹⁰ m", "", false);
    prefixed("eV", kEV, energy, "electronvolt", all, {"meV", "keV", "MeV", "GeV"}, std::string("e × 1 V, ") + kSI);
    prefixed("bar", 1e5, pressure, "bar = 100000 Pa", {"m"}, {});
    unit("atm", 101325, pressure, "standard atmosphere", std::string("101 325 Pa, exact by definition; ") + kCodata);
    unit("tonne", 1000, M, "tonne = 1000 kg");
    unit("rad", 1, none, "radian (an angle; a plain number)");
    unit("deg", kPi / 180, none, "degree of angle = π/180 rad");
    unit("°", kPi / 180, none, "degree of angle = π/180 rad", "", false);
    unit("arcmin", kPi / 180 / 60, none, "arcminute = 1/60 degree");
    unit("arcsec", kPi / 180 / 3600, none, "arcsecond = 1/3600 degree");
    // ---- astronomy (the IAU's definitions) ----
    unit("au", ::units::kAU, L, "astronomical unit", "149 597 870 700 m exactly, IAU 2012 Resolution B2");
    unit("AU", ::units::kAU, L, "astronomical unit", "149 597 870 700 m exactly, IAU 2012 Resolution B2", false);
    unit("ly", 299792458.0 * 365.25 * 86400, L, "light year", "the distance light travels in a Julian year (IAU)");
    unit("pc", 648000 / kPi * ::units::kAU, L, "parsec", "648000/π au exactly, IAU 2015 Resolution B2");
    // ---- US / imperial (international yard and pound, 1959: exact) ----
    unit("mi", 1609.344, L, "mile = 1609.344 m");
    unit("ft", 0.3048, L, "foot = 0.3048 m");
    unit("inch", 0.0254, L, "inch = 0.0254 m");
    unit("lb", 0.45359237, M, "pound (mass) = 0.45359237 kg");
    unit("mph", 1609.344 / 3600, speed, "mile per hour");

    // ---- constants: the exact ones of the SI, then CODATA 2022 (the values the Atom Lab uses, atom_core.h), then astronomy ----
    constant("c", kC, speed, "speed of light in vacuum", kSI, "299 792 458");
    constant("hP", kPlanckJs, dimOf(1, 2, -1), "Planck constant h", kSI, "6.626 070 15 × 10^-34");
    constant("ħ", kPlanckJs / (2 * kPi), dimOf(1, 2, -1), "reduced Planck constant ħ = h/2π", kSI, "");
    constant("hbar", kPlanckJs / (2 * kPi), dimOf(1, 2, -1), "reduced Planck constant ħ = h/2π", kSI, "", false);
    constant("e", kEV, charge, "elementary charge", kSI, "1.602 176 634 × 10^-19");
    constant("kB", kBoltzmann, dimOf(1, 2, -2, 0, -1), "Boltzmann constant", kSI, "1.380 649 × 10^-23");
    constant("NA", 6.02214076e23, dimOf(0, 0, 0, 0, 0, -1), "Avogadro constant", kSI, "6.022 140 76 × 10^23");
    constant("R", 6.02214076e23 * kBoltzmann, dimOf(1, 2, -2, 0, -1, -1), "molar gas constant R = NA kB", kSI, "");
    constant("G", 6.67430e-11, dimOf(-1, 3, -2), "Newtonian constant of gravitation", kCodata, "6.674 30(15) × 10^-11");
    constant("me", kMeKg, M, "electron mass", kCodata, "9.109 383 7139(28) × 10^-31");
    constant("mp", kMpKg, M, "proton mass", kCodata, "1.672 621 925 95(52) × 10^-27");
    constant("mn", kMnKg, M, "neutron mass", kCodata, "1.674 927 500 56(85) × 10^-27");
    unit("u", kUKg, M, "atomic mass unit (dalton)", std::string(kCodata) + ": 1.660 539 068 92(52) × 10^-27 kg");
    constant("a0", kA0, L, "Bohr radius", kCodata, "5.291 772 105 44(82) × 10^-11");
    constant("α", kAlpha, none, "fine-structure constant", kCodata, "7.297 352 5643(11) × 10^-3");
    constant("alpha", kAlpha, none, "fine-structure constant", kCodata, "7.297 352 5643(11) × 10^-3", false);
    constant("Ry", kRydInf * kEV, energy, "Rydberg energy h c R∞ = 13.605 693 122 990(15) eV (an infinitely heavy nucleus)", kCodata, "");
    constant("ε0", 8.8541878188e-12, dimOf(-1, -3, 4, 2), "vacuum electric permittivity", kCodata, "8.854 187 8188(14) × 10^-12");
    constant("eps0", 8.8541878188e-12, dimOf(-1, -3, 4, 2), "vacuum electric permittivity", kCodata, "8.854 187 8188(14) × 10^-12", false);
    constant("μ0", 1.25663706127e-6, dimOf(1, 1, -2, -2), "vacuum magnetic permeability", kCodata, "1.256 637 061 27(20) × 10^-6");
    constant("mu0", 1.25663706127e-6, dimOf(1, 1, -2, -2), "vacuum magnetic permeability", kCodata, "1.256 637 061 27(20) × 10^-6", false);
    {
        const double hb = kPlanckJs / (2 * kPi), k = kBoltzmann;   // σ = π² k⁴ / (60 ħ³ c²): exact (CODATA 2022 gives 5.670 374 419... × 10^-8, exact)
        const double sigma = kPi * kPi * (k * k * k * k) / (60 * hb * hb * hb * kC * kC);
        constant("σ", sigma, dimOf(1, 0, -3, 0, -4), "Stefan-Boltzmann constant σ = π² kB⁴ / (60 ħ³ c²)", kSI, "");
        constant("sigma", sigma, dimOf(1, 0, -3, 0, -4), "Stefan-Boltzmann constant σ = π² kB⁴ / (60 ħ³ c²)", kSI, "", false);
    }
    constant("gn", ::units::kStandardGravity, dimOf(0, 1, -2), "standard acceleration of gravity", "exact by definition (3rd CGPM, 1901)", "9.806 65");
    constant("π", kPi, none, "pi", "", "");
    constant("pi", kPi, none, "pi", "", "", false);
    constant("GMsun", ::units::kGMSun, dimOf(0, 3, -2), "the Sun's mass times G", "JPL DE440 (ssd.jpl.nasa.gov/astro_par.html)", "1.327 124 400 412 794 19 × 10^20");
    constant("Msun", ::units::kGMSun / 6.67430e-11, M, "mass of the Sun = GMsun / G", "JPL DE440 GM over CODATA 2022 G (uncertain by 2.2 × 10⁻⁵, from G)", "");
    constant("Mearth", ::units::kEarthMassKg, M, "mass of the Earth", "NASA Earth Fact Sheet", "5.9722 × 10^24");
    constant("Rearth", ::units::kEarthRadius, L, "Earth's mean radius", "NASA Earth Fact Sheet (volumetric mean radius 6371.000 km)", "6.371 × 10^6");

    // ---- 0.50.1: quantum mechanics. CODATA 2022, Table XXXIII of Mohr et al. (arXiv:2409.03787, saved as data/calc/, read with pdftotext -layout) ----
    {
        const Dim moment = dimOf(0, 2, 0, 1);                       // J/T = A·m²
        const double muN = 5.0507837393e-27;
        constant("μB", atom::kMuBJT, moment, "Bohr magneton e ħ / 2me", kCodata, "9.274 010 0657(29) × 10^-24");
        constant("muB", atom::kMuBJT, moment, "Bohr magneton e ħ / 2me", kCodata, "9.274 010 0657(29) × 10^-24", false);
        constant("μN", muN, moment, "nuclear magneton e ħ / 2mp (not a micronewton)", kCodata, "5.050 783 7393(16) × 10^-27");
        constant("muN", muN, moment, "nuclear magneton e ħ / 2mp", kCodata, "5.050 783 7393(16) × 10^-27", false);
        // the proton's and neutron's moments: the Atom Lab's own ratios (atom_core.h kMuP, kMuN: CODATA 2022 μp/μN, μn/μN) times μN, so there is one value of each
        const std::string pText = "proton magnetic moment = 2.792 847 344 63(82) μN", nText = "neutron magnetic moment = -1.913 042 76(45) μN (negative: it points against the spin)";
        constant("μp", kMuP * muN, moment, pText, kCodata, "");
        constant("mup", kMuP * muN, moment, pText, kCodata, "", false);
        constant("μn", kMuN * muN, moment, nText, kCodata, "");
        constant("mun", kMuN * muN, moment, nText, kCodata, "", false);
        constant("gp", 2 * kMuP, none, "proton g-factor 2 μp/μN (CODATA prints 5.585 694 6893(16))", kCodata, "");
        constant("ae", atom::hfs::kAe, none, "electron magnetic-moment anomaly |μe|/μB - 1", kCodata, "1.159 652 180 46(18) × 10^-3");
        constant("ge", atom::kGe, none, "electron g-factor -2 (1 + ae) (negative: CODATA's sign, the moment points against the spin)", kCodata, "-2.002 319 304 360 92(36)");
        constant("Eh", 4.3597447222060e-18, energy, "Hartree energy α² me c² = 27.211 386 245 981(30) eV", kCodata, "4.359 744 722 2060(48) × 10^-18");
        unit("hartree", 4.3597447222060e-18, energy, "hartree, the atomic unit of energy = Eh", std::string(kCodata) + ": 4.359 744 722 2060(48) × 10^-18 J");
        unit("bohr", kA0, L, "bohr, the atomic unit of length = a0", std::string(kCodata) + ": 5.291 772 105 44(82) × 10^-11 m");
        constant("Rinf", 10973731.568157, dimOf(0, -1, 0), "Rydberg constant R∞", kCodata, "10 973 731.568 157(12)");
        constant("mμ", 1.883531627e-28, M, "muon mass", kCodata, "1.883 531 627(42) × 10^-28");
        constant("mmu", 1.883531627e-28, M, "muon mass", kCodata, "1.883 531 627(42) × 10^-28", false);
        constant("md", 3.3435837768e-27, M, "deuteron mass", kCodata, "3.343 583 7768(10) × 10^-27");
        constant("mα", 6.6446573450e-27, M, "alpha particle mass", kCodata, "6.644 657 3450(21) × 10^-27");
        constant("malpha", 6.6446573450e-27, M, "alpha particle mass", kCodata, "6.644 657 3450(21) × 10^-27", false);
        constant("λC", 2.42631023538e-12, L, "Compton wavelength of the electron h / (me c)", kCodata, "2.426 310 235 38(76) × 10^-12");
        constant("lambdaC", 2.42631023538e-12, L, "Compton wavelength of the electron h / (me c)", kCodata, "2.426 310 235 38(76) × 10^-12", false);
        // exact since 2019: worked out from the exact h and e (CODATA 2022 prints them with "...")
        const std::string exactFrom = std::string(kSI) + "; CODATA 2022 prints ";
        constant("Φ0", kPlanckJs / (2 * kEV), dimOf(1, 2, -2, -1), "magnetic flux quantum h / 2e", exactFrom + "2.067 833 848... × 10^-15 Wb", "");
        constant("Phi0", kPlanckJs / (2 * kEV), dimOf(1, 2, -2, -1), "magnetic flux quantum h / 2e", exactFrom + "2.067 833 848... × 10^-15 Wb", "", false);
        constant("RK", kPlanckJs / (kEV * kEV), ohm, "von Klitzing constant h / e²", exactFrom + "25 812.807 45... Ω", "");
        constant("KJ", 2 * kEV / kPlanckJs, dimOf(-1, -2, 2, 1), "Josephson constant 2e / h", exactFrom + "483 597.848 4... × 10^9 Hz/V", "");
        // the Fermi constant: CODATA (from the PDG) gives GF/(ħc)³ in GeV^-2; times (ħc)³ that is an energy times a volume, J·m³ (ħ, c and the GeV are exact)
        {
            const double hbarC = kPlanckJs / (2 * kPi) * kC, GeV = 1e9 * kEV;
            constant("GF", 1.1663787e-5 * hbarC * hbarC * hbarC / (GeV * GeV), dimOf(1, 5, -2),
                     "Fermi coupling constant in J·m³: GF/(ħc)³ = 1.166 3787(6) × 10^-5 GeV^-2 times (ħc)³", std::string(kCodata) + " (from the PDG)", "");
        }
        // atomic units (CODATA 2022, the "Atomic units (a.u.)" part of the table); the a.u. of time is the Atom Lab's kAuTime
        constant("au_time", kAuTime, T, "atomic unit of time ħ / Eh", kCodata, "2.418 884 326 5864(26) × 10^-17");
        constant("au_field", 5.14220675112e11, dimOf(1, 1, -3, -1), "atomic unit of electric field Eh / (e a0)", kCodata, "5.142 206 751 12(80) × 10^11");
        constant("au_dipole", 8.4783536198e-30, dimOf(0, 1, 1, 1), "atomic unit of electric dipole moment e a0", kCodata, "8.478 353 6198(13) × 10^-30");
        // units exact by definition
        unit("D", 1e-21 / kC, dimOf(0, 1, 1, 1), "debye = 10⁻²¹/c C·m with c in m/s (10⁻¹⁸ statC·cm), about 3.335 64 × 10⁻³⁰ C·m");
        unit("debye", 1e-21 / kC, dimOf(0, 1, 1, 1), "debye = 10⁻²¹/c C·m with c in m/s, about 3.335 64 × 10⁻³⁰ C·m", "", false);
        unit("gauss", 1e-4, tesla, "gauss = 10⁻⁴ T (the CGS unit of magnetic field)");
        unit("erg", 1e-7, energy, "erg = 10⁻⁷ J");
        unit("barn", 1e-28, dimOf(0, 2, 0), "barn = 10⁻²⁸ m²");
    }
    constant("Rsun", ::units::kSunRadius, L, "nominal radius of the Sun", "IAU 2015 Resolution B3 (695 700 km)", "6.957 × 10^8");
    return t;
}
const Table& table() { static const Table t = makeTable(); return t; }

const std::vector<std::string> kFunctions = {"sqrt", "cbrt", "abs", "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "exp", "ln", "log10",
    // 0.50.1: on plain numbers, for quantum mechanics
    "fact", "gamma", "lngamma", "erf", "erfc", "besselj", "sphbessel", "legendre", "assoclegendre", "laguerre", "assoclaguerre", "hermite", "binom", "cg", "wigner3j",
    "sum", "solve",
    // 0.50.2: complex numbers
    "re", "im", "conj", "arg",
    // 0.54.2: small matrices
    "transpose", "dagger", "det", "tr", "identity", "kron", "eig"};
bool isFunction(const std::string& n) { return std::find(kFunctions.begin(), kFunctions.end(), n) != kFunctions.end(); }
// how each function is written (its number of arguments is the number of commas + 1)
std::string usage(const std::string& f) {
    static const std::map<std::string, std::string> u = {
        {"fact", "fact(n)"}, {"gamma", "gamma(x)"}, {"lngamma", "lngamma(x)"}, {"erf", "erf(x)"}, {"erfc", "erfc(x)"}, {"besselj", "besselj(n, x)"},
        {"sphbessel", "sphbessel(l, x)"}, {"legendre", "legendre(l, x)"}, {"assoclegendre", "assoclegendre(l, m, x)"}, {"laguerre", "laguerre(n, x)"},
        {"assoclaguerre", "assoclaguerre(n, k, x)"}, {"hermite", "hermite(n, x)"}, {"binom", "binom(n, k)"}, {"cg", "cg(j1, m1, j2, m2, J, M)"},
        {"wigner3j", "wigner3j(j1, j2, j3, m1, m2, m3)"}, {"sum", "sum(expression, k, from, to)"}, {"solve", "solve(expression, x, a, b)"},
        {"transpose", "transpose(M)"}, {"dagger", "dagger(M)"}, {"det", "det(M)"}, {"tr", "tr(M)"}, {"identity", "identity(n)"}, {"kron", "kron(A, B)"}, {"eig", "eig(M)"}};
    const auto it = u.find(f);
    return it == u.end() ? f + "(x)" : it->second;
}
int arity(const std::string& f) { const std::string u = usage(f); return 1 + static_cast<int>(std::count(u.begin(), u.end(), ',')); }

// ---------------------------------------------------------------- tokens
enum class TT { Num, Ident, Plus, Minus, Star, Slash, Caret, LP, RP, Sup, To, Comma, Bang, Assign, LB, RB, End };   // LB, RB: 0.54.2, [ and ] of a matrix
struct Tok { TT t = TT::End; int pos = 0, len = 0; double num = 0; long long sup = 0; std::string text; };
Tok mk(TT t, int pos, int len) { Tok k; k.t = t; k.pos = pos; k.len = len; return k; }

std::vector<Tok> tokenize(const std::string& s) {
    std::vector<Tok> out;
    std::size_t i = 0;
    const auto at = [&](std::size_t k) -> char { return k < s.size() ? s[k] : '\0'; };
    // 0.50.1: which brackets are open, true for a function's ("sqrt(", or any name written right against its bracket, "foo("): only inside those does a comma
    // separate arguments; everywhere else it is refused, so a thousands separator or a decimal comma can never be misread
    std::vector<bool> brackets;
    const auto inCall = [&]() { return !brackets.empty() && brackets.back(); };
    const char* const kCommaMsg = "Write numbers without commas: 384400, not 384,400 (and use a point for decimals: 1.5). A comma only separates a function's arguments: besselj(0, 2.4).";
    while (i < s.size()) {
        int n = 1;
        const unsigned cp = decode(s, i, n);
        const int pos = static_cast<int>(i);
        if (spaceChar(cp)) { i += n; continue; }
        if (cp == 0xFFFD) fail("This character could not be read.", pos, n);
        const char c = static_cast<char>(cp < 0x80 ? cp : 0);
        if ((c >= '0' && c <= '9') || (c == '.' && at(i + 1) >= '0' && at(i + 1) <= '9')) {
            std::size_t j = i;
            while (at(j) >= '0' && at(j) <= '9') j++;
            if (at(j) == '.') { j++; while (at(j) >= '0' && at(j) <= '9') j++; }
            if (at(j) == '.') fail("A number can have only one decimal point.", static_cast<int>(j), 1);
            if ((at(j) == 'e' || at(j) == 'E') && ((at(j + 1) >= '0' && at(j + 1) <= '9') || ((at(j + 1) == '+' || at(j + 1) == '-') && at(j + 2) >= '0' && at(j + 2) <= '9'))) {
                j += 2;
                while (at(j) >= '0' && at(j) <= '9') j++;
            }
            if (at(j) == ',' && at(j + 1) >= '0' && at(j + 1) <= '9' && !inCall()) fail(kCommaMsg, static_cast<int>(j), 1);
            double v = 0;
            const auto r = std::from_chars(s.data() + i, s.data() + j, v);
            if (r.ec == std::errc::result_out_of_range || (r.ec == std::errc() && !std::isfinite(v)))
                fail("This number is too big or too small for the calculator (it works between about 10^-307 and 10^308).", pos, static_cast<int>(j - i));
            if (r.ec != std::errc() || r.ptr != s.data() + j) fail("This number could not be read.", pos, static_cast<int>(j - i));
            if (v != 0 && std::fabs(v) < DBL_MIN) fail("This number is too small for the calculator (it works down to about 10^-307).", pos, static_cast<int>(j - i));
            Tok t = mk(TT::Num, pos, static_cast<int>(j - i)); t.num = v; t.text = s.substr(i, j - i);
            out.push_back(t); i = j; continue;
        }
        if ((cp < 0x80 && (std::isalpha(static_cast<unsigned char>(c)) || c == '_')) || nameLetter(cp)) {
            std::string name;
            std::size_t j = i;
            while (j < s.size()) {
                int m = 1;
                const unsigned q = decode(s, j, m);
                const bool ok = (q < 0x80 && (std::isalnum(static_cast<int>(q)) || q == '_')) || nameLetter(q);
                if (!ok) break;
                name += encode(canonical(q));
                j += m;
            }
            Tok t = mk(name == "to" ? TT::To : TT::Ident, pos, static_cast<int>(j - i)); t.text = name;
            out.push_back(t); i = j; continue;
        }
        if (superDigit(cp) >= 0 || cp == 0x207B || cp == 0x207A) {
            std::size_t j = i;
            long long sign = 1, v = 0;
            int digits = 0, m = 1;
            unsigned q = decode(s, j, m);
            if (q == 0x207B || q == 0x207A) { sign = q == 0x207B ? -1 : 1; j += m; }
            while (j < s.size()) {
                q = decode(s, j, m);
                const int d = superDigit(q);
                if (d < 0) break;
                if (++digits > 6) fail("This power is too large.", pos, static_cast<int>(j - i));
                v = v * 10 + d; j += m;
            }
            if (digits == 0) fail("A raised minus or plus needs a raised digit after it, like m⁻¹.", pos, static_cast<int>(j - i));
            Tok t = mk(TT::Sup, pos, static_cast<int>(j - i)); t.sup = sign * v; t.text = std::to_string(sign * v);
            out.push_back(t); i = j; continue;
        }
        Tok t = mk(TT::End, pos, n);
        if (c == '+') t.t = TT::Plus;
        else if (c == '-' && at(i + 1) == '>') { t.t = TT::To; t.len = 2; }
        else if (c == '-' || cp == 0x2212) t.t = TT::Minus;
        else if (c == '*' && at(i + 1) == '*') { t.t = TT::Caret; t.len = 2; }
        else if (c == '*' || cp == 0xD7 || cp == 0xB7 || cp == 0x22C5 || cp == 0x2219) t.t = TT::Star;
        else if (c == '/' || cp == 0xF7) t.t = TT::Slash;
        else if (c == '^') t.t = TT::Caret;
        else if (c == '(') {
            t.t = TT::LP;
            const bool call = !out.empty() && out.back().t == TT::Ident && (isFunction(out.back().text) || out.back().pos + out.back().len == pos);
            brackets.push_back(call);
        } else if (c == ')') { t.t = TT::RP; if (!brackets.empty()) brackets.pop_back(); }
        else if (c == '[') { t.t = TT::LB; brackets.push_back(true); }          // 0.54.2: a matrix; commas separate its entries and its rows
        else if (c == ']') { t.t = TT::RB; if (!brackets.empty()) brackets.pop_back(); }
        else if (c == '!') t.t = TT::Bang;
        else if (cp == 0x2192) t.t = TT::To;
        else if (c == '=') {
            std::size_t j = i + 1;
            while (j < s.size()) { int m = 1; if (!spaceChar(decode(s, j, m))) break; j += m; }
            if (j == s.size()) { i = j; continue; }          // "2+2=": the = at the end is fine
            // 0.50.2: "x = 2 m" names a variable: = right after one name at the start of the line
            if (out.size() == 1 && out[0].t == TT::Ident) { out.push_back(mk(TT::Assign, pos, 1)); i += 1; continue; }
            fail("= can only stand right after a name at the start of the line (x = 2 m, to keep a variable) or at the very end.", pos, 1);
        } else if (c == ',') {
            if (!inCall()) fail(kCommaMsg, pos, 1);
            t.t = TT::Comma;
        }
        else if (c == '%') fail("% is not used here: for 5 % write 0.05.", pos, 1);
        else fail("The calculator does not know the sign \"" + s.substr(i, n) + "\".", pos, n);
        out.push_back(t);
        i += t.len;
    }
    Tok e = mk(TT::End, static_cast<int>(s.size()), 0);
    out.push_back(e);
    return out;
}

// ---------------------------------------------------------------- the tree
enum class NK { Num, Sym, Neg, Add, Sub, Mul, Div, Juxt, Pow, Call, Mat };   // Mat: 0.54.2, a typed matrix: args are its entries row after row, num its number of rows
struct Node {
    NK k = NK::Num; int a = -1, b = -1; double num = 0; std::string text; int pos = 0, len = 0;
    bool sup = false, paren = false, bang = false;   // paren: it was written in brackets; bang: n! (a Call of fact)
    std::vector<int> args;                           // a Call's arguments (a is the first)
};
Node mkNode(NK k, int a = -1, int b = -1) { Node n; n.k = k; n.a = a; n.b = b; return n; }

std::string show(const std::vector<Node>& nd, int id);
int prec(NK k);

constexpr int kMaxDepth = 120;
constexpr std::size_t kMaxLength = 1000;

struct Parser {
    const std::vector<Tok>& tk;
    std::vector<Node> nodes;
    std::size_t i = 0;
    int depth = 0;
    std::set<std::string> vars;           // 0.50.1: the index names of the sum() and solve() being read (they are numbers, not units)
    const Vars* userVars = nullptr;       // 0.50.2: the named variables (not units either)
    std::vector<std::string> notes;       // how something was read that is worth saying (the Coulomb form)
    explicit Parser(const std::vector<Tok>& t) : tk(t) {}
    const Tok& cur() const { return tk[i]; }
    int add(Node n) { nodes.push_back(n); return static_cast<int>(nodes.size()) - 1; }
    int bin(NK k, int a, int b, const Tok& op) { Node n = mkNode(k, a, b); n.pos = op.pos; n.len = op.len; return add(n); }
    struct Deeper {
        Parser& p;
        explicit Deeper(Parser& q) : p(q) { if (++p.depth > kMaxDepth) fail("Too many brackets or signs inside each other.", p.cur().pos, 1); }
        ~Deeper() { p.depth--; }
    };
    int expr() {
        Deeper d(*this);
        int t = term();
        while (cur().t == TT::Plus || cur().t == TT::Minus) {
            const Tok op = cur(); i++;
            t = bin(op.t == TT::Plus ? NK::Add : NK::Sub, t, term(), op);
        }
        return t;
    }
    int term() {
        int t = signedFactor();
        while (cur().t == TT::Star || cur().t == TT::Slash) {
            const Tok op = cur(); i++;
            const int r = signedFactor();
            if (op.t == TT::Slash) {
                const int cf = coulombForm(t, r, op);
                if (cf >= 0) { t = cf; continue; }
                checkAfterSlash(t, r, op);
            }
            t = bin(op.t == TT::Star ? NK::Mul : NK::Div, t, r, op);
        }
        return t;
    }
    // "a / b c" is (a / b) c to some calculators and a / (b c) to others. The calculator reads it as a / (b c) only when "b c" is a quantity: units, or
    // a number and its units ("/ 2 hr", "/ kg K"), and not straight after a plain number ("1/2 km": half a km, or 1 over 2 km?). Anything else is refused
    // with both readings, so it never guesses: "4/3 π r^3", "1/(4 π ε0) e^2", "hP/2 π".
    bool unitName(const std::string& n) const {
        if (n == "ans" || n == "i" || isFunction(n) || isPauliName(n) || vars.count(n) || (userVars && userVars->count(n))) return false;
        const Symbol* s = findSymbol(n);
        return s ? s->kind == SymKind::Unit : true;          // an unknown name: its own message comes when it is worked out
    }
    bool unitLike(int id) const {
        const Node& n = nodes[id];
        if (n.k == NK::Sym) return unitName(n.text);
        if (n.k != NK::Pow || nodes[n.a].k != NK::Sym || !unitName(nodes[n.a].text)) return false;
        const Node& e = nodes[n.b];
        return e.k == NK::Num || (e.k == NK::Neg && nodes[e.a].k == NK::Num);
    }
    // the factors of a juxtaposition "a b c" in order; a bracket the user wrote is one factor (0.50.1: it was split, so a message could misquote it)
    std::vector<int> leavesOf(int r) const {
        std::vector<int> leaves;
        int x = r;
        while (nodes[x].k == NK::Juxt && (x == r || !nodes[x].paren)) { leaves.push_back(nodes[x].b); x = nodes[x].a; }
        leaves.push_back(x);
        std::reverse(leaves.begin(), leaves.end());
        return leaves;
    }
    bool mentions(int id, const char* a, const char* b) const {
        const Node& n = nodes[id];
        if (n.k == NK::Sym && (n.text == a || n.text == b)) return true;
        if (n.a >= 0 && mentions(n.a, a, b)) return true;
        if (n.b >= 0 && mentions(n.b, a, b)) return true;
        for (int c : n.args) if (mentions(c, a, b)) return true;
        return false;
    }
    // 0.50.1: "1/(4 π ε0) e^2 / r" is how Coulomb's constant is written in every textbook: 1 over a bracket that holds ε0, then a quantity. It is read as
    // (1 / (4 π ε0)) × the quantity, and a note says so. Any other "1/(...) x" is still refused with both readings.
    int coulombForm(int left, int right, const Tok& op) {
        const Node& l = nodes[left];
        if (l.k != NK::Num || l.num != 1 || nodes[right].k != NK::Juxt || nodes[right].paren) return -1;
        const std::vector<int> leaves = leavesOf(right);
        if (leaves.size() < 2 || !nodes[leaves[0]].paren || !mentions(leaves[0], "ε0", "eps0")) return -1;
        int x = bin(NK::Div, left, leaves[0], op);
        for (std::size_t k = 1; k < leaves.size(); k++) { Node j = mkNode(NK::Juxt, x, leaves[k]); j.pos = nodes[leaves[k]].pos; j.len = nodes[leaves[k]].len; x = add(j); }
        const std::string note = "1/(4 π ε0) followed by a quantity is read as (1/(4 π ε0)) × that quantity, the way Coulomb's constant is written in textbooks.";
        if (std::find(notes.begin(), notes.end(), note) == notes.end()) notes.push_back(note);
        return x;
    }
    void checkAfterSlash(int left, int right, const Tok& op) {
        int r = right;
        if (nodes[r].k == NK::Neg && !nodes[r].paren) r = nodes[r].a;
        if (nodes[r].k != NK::Juxt || nodes[r].paren) return;       // "/ (2349 cm^-1)": the brackets say it

        const std::vector<int> leaves = leavesOf(r);
        bool quantity = true;
        for (std::size_t k = 0; k < leaves.size(); k++) {
            if (k == 0 && nodes[leaves[k]].k == NK::Num) continue;
            if (!unitLike(leaves[k])) quantity = false;
        }
        const Node& l = nodes[left];
        const bool plainLeft = l.k == NK::Num || (l.k == NK::Neg && nodes[l.a].k == NK::Num);
        if (quantity && !(plainLeft && nodes[leaves[0]].k == NK::Num)) return;
        const auto part = [&](int id) { const std::string t = show(nodes, id); return (nodes[id].paren || prec(nodes[id].k) < 5) ? "(" + t + ")" : t; };
        const std::string L = prec(l.k) < 2 ? "(" + show(nodes, left) + ")" : show(nodes, left), first = part(leaves[0]);
        std::string rest;
        for (std::size_t k = 1; k < leaves.size(); k++) rest += " " + part(leaves[k]);
        fail("This is unclear: it could mean (" + L + " / " + first + ")" + rest + " or " + L + " / (" + first + rest + "). Write the brackets you mean, or use *.", op.pos, op.len);
    }
    int signedFactor() {
        Deeper d(*this);
        if (cur().t == TT::Minus) { const Tok op = cur(); i++; Node n = mkNode(NK::Neg, signedFactor()); n.pos = op.pos; n.len = op.len; return add(n); }
        if (cur().t == TT::Plus) { i++; return signedFactor(); }
        return juxt();
    }
    int juxt() {
        int t = power();
        while (true) {
            if (cur().t == TT::Ident || cur().t == TT::LP || cur().t == TT::LB) { const Tok op = cur(); t = bin(NK::Juxt, t, power(), op); continue; }
            if (cur().t == TT::Num)
                fail("Two numbers, or a unit and then a number, stand next to each other. Write one number without spaces (384400, not 384 400), the number before its unit "
                     "(2 km, not km 2), or put * between them.", cur().pos, cur().len);
            if (cur().t == TT::Sup) fail("A raised number must follow a number or a unit directly, like m².", cur().pos, cur().len);
            return t;
        }
    }
    int power() {
        const int base = postfix();
        if (cur().t != TT::Caret) return base;
        const Tok op = cur(); i++;
        if (cur().t == TT::End) fail("A power needs a number after ^, like m^2.", op.pos, op.len);
        return bin(NK::Pow, base, exponent(), op);
    }
    int exponent() {
        Deeper d(*this);
        if (cur().t == TT::Minus) { const Tok op = cur(); i++; Node n = mkNode(NK::Neg, exponent()); n.pos = op.pos; n.len = op.len; return add(n); }
        if (cur().t == TT::Plus) { i++; return exponent(); }
        return power();
    }
    int postfix() {
        int p = primary();
        while (cur().t == TT::Sup || cur().t == TT::Bang) {
            const Tok s = cur(); i++;
            if (s.t == TT::Bang) {                                          // n! is fact(n), binding as tightly as a raised power
                Node f = mkNode(NK::Call, p); f.args = {p}; f.text = "fact"; f.bang = true; f.pos = s.pos; f.len = s.len;
                p = add(f);
                continue;
            }
            Node num = mkNode(NK::Num); num.num = double(s.sup); num.text = std::to_string(s.sup); num.pos = s.pos; num.len = s.len;
            const int e = add(num);
            Node pw = mkNode(NK::Pow, p, e); pw.pos = s.pos; pw.len = s.len; pw.sup = true;
            p = add(pw);
        }
        return p;
    }
    int primary() {
        Deeper d(*this);
        const Tok t = cur();
        switch (t.t) {
            case TT::Num: { i++; Node n = mkNode(NK::Num); n.num = t.num; n.text = t.text; n.pos = t.pos; n.len = t.len; return add(n); }
            case TT::Ident: {
                i++;
                if (isFunction(t.text)) {
                    if (cur().t != TT::LP) fail(t.text + " needs brackets round what it works on, like " + usage(t.text) + ".", t.pos, t.len);
                    const Tok lp = cur(); i++;
                    if (cur().t == TT::RP) fail(t.text + "() needs something inside the brackets: " + usage(t.text) + ".", lp.pos, 1);
                    // sum(f, k, a, b) and solve(f, x, a, b): the second argument names a number used inside f. Look ahead for it, so f is read with it as a number.
                    std::string var;
                    if (t.text == "sum" || t.text == "solve") {
                        int d = 0;
                        for (std::size_t k = i; k + 1 < tk.size() && tk[k].t != TT::End; k++) {
                            if (tk[k].t == TT::LP) d++;
                            else if (tk[k].t == TT::RP) { if (--d < 0) break; }
                            else if (tk[k].t == TT::Comma && d == 0) { if (tk[k + 1].t == TT::Ident) var = tk[k + 1].text; break; }
                        }
                        if (!var.empty()) {
                            if (const Symbol* sy = findSymbol(var))
                                fail(var + " is already the " + sy->meaning + ": name the " + (t.text == "sum" ? "index" : "unknown") + " something else, like " +
                                     (t.text == "sum" ? "k or j" : "x or z") + ".", t.pos, t.len);
                            if (isFunction(var) || var == "ans") fail(var + " is already a name the calculator uses: pick another, like k or x.", t.pos, t.len);
                        }
                    }
                    const bool fresh = !var.empty() && !vars.count(var);
                    if (fresh) vars.insert(var);
                    std::vector<int> args{expr()};
                    while (cur().t == TT::Comma) { i++; args.push_back(expr()); }
                    if (fresh) vars.erase(var);
                    if (cur().t != TT::RP) fail("This bracket is not closed: a ) is missing.", lp.pos, 1);
                    i++;
                    const int want = arity(t.text);
                    if (static_cast<int>(args.size()) != want)
                        fail(t.text + " takes " + std::to_string(want) + (want == 1 ? " argument" : " arguments") + ", " + usage(t.text) + ", not " + std::to_string(args.size()) + ".",
                             t.pos, t.len);
                    if ((t.text == "sum" || t.text == "solve") && (nodes[args[1]].k != NK::Sym || nodes[args[1]].paren || var.empty()))
                        fail("The second thing in " + t.text + "(...) must be a name for the " + (t.text == "sum" ? "index" : "unknown") + ", like " +
                             (t.text == "sum" ? "k: sum(1/k^2, k, 1, 100)." : "x: solve(x^2 - 2, x, 0, 2)."), nodes[args[1]].pos, nodes[args[1]].len);
                    Node n = mkNode(NK::Call, args[0]); n.args = args; n.text = t.text; n.pos = t.pos; n.len = t.len; return add(n);
                }
                if (cur().t == TT::LP && cur().pos == t.pos + t.len && !vars.count(t.text) && !(userVars && userVars->count(t.text)) && !findSymbol(t.text) && t.text != "ans" && t.text != "log" && t.text != "i" && !isPauliName(t.text)) {
                    std::string list;
                    for (const std::string& f : kFunctions) list += (list.empty() ? "" : ", ") + f;
                    fail("\"" + t.text + "\" is not a function the calculator knows. Its functions: " + list + ".", t.pos, t.len);
                }
                Node n = mkNode(NK::Sym); n.text = t.text; n.pos = t.pos; n.len = t.len; return add(n);
            }
            case TT::LP: {
                i++;
                if (cur().t == TT::RP) fail("Empty brackets: write something inside them.", t.pos, 1);
                const int a = expr();
                if (cur().t != TT::RP) fail("This bracket is not closed: a ) is missing.", t.pos, 1);
                i++;
                nodes[a].paren = true;
                return a;
            }
            case TT::LB: {                                               // 0.54.2: a matrix, [[a, b], [c, d]]; one pair of brackets, [a, b], is one row
                i++;
                const char* const kEmpty = "Empty brackets: a matrix is written [[1, 2], [3, 4]], each row in its own brackets.";
                if (cur().t == TT::RB) fail(kEmpty, t.pos, 1);
                std::vector<int> entries;
                int rows = 0, cols = -1;
                const auto row = [&](const Tok& open) {
                    int count = 0;
                    while (true) { entries.push_back(expr()); count++; if (cur().t != TT::Comma) break; i++; }
                    if (cur().t != TT::RB) fail("This bracket is not closed: a ] is missing.", open.pos, 1);
                    i++;
                    rows++;
                    if (cols >= 0 && count != cols)
                        fail("Every row of a matrix needs the same number of entries (the first row has " + std::to_string(cols) + ", row " + std::to_string(rows) + " has " +
                             std::to_string(count) + ").", open.pos, 1);
                    cols = count;
                };
                if (cur().t == TT::LB) {
                    while (true) {
                        if (cur().t != TT::LB) fail("Each row of a matrix goes in its own brackets: [[1, 2], [3, 4]].", cur().pos, cur().len);
                        const Tok open = cur(); i++;
                        if (cur().t == TT::RB) fail(kEmpty, open.pos, 1);
                        row(open);
                        if (cur().t != TT::Comma) break;
                        i++;
                    }
                    if (cur().t != TT::RB) fail("This bracket is not closed: a ] is missing.", t.pos, 1);
                    i++;
                } else row(t);
                const Tok& last = tk[i - 1];
                if (rows > kMatMax || cols > kMatMax)
                    fail("A matrix can have at most 4 rows and 4 columns (this one is " + std::to_string(rows) + "x" + std::to_string(cols) + ").", t.pos, last.pos + last.len - t.pos);
                Node n = mkNode(NK::Mat); n.args = entries; n.num = rows; n.pos = t.pos; n.len = last.pos + last.len - t.pos;
                return add(n);
            }
            case TT::RB: fail("This ] has no [ before it.", t.pos, t.len);
            case TT::RP: fail("This ) has no ( before it.", t.pos, t.len);
            case TT::End: fail("Something is missing at the end.", t.pos, 0);
            case TT::To: fail("Write what to convert before \"to\", like: 5 km to mi.", t.pos, t.len);
            case TT::Sup: fail("A raised number must follow a number or a unit directly, like m².", t.pos, t.len);
            case TT::Comma: fail("Something is missing before this comma.", t.pos, t.len);
            case TT::Bang: fail("! (factorial) must follow a number or a bracket, like 5! or (n+1)!.", t.pos, t.len);
            case TT::Assign: fail("= can only stand right after a name at the start of the line (x = 2 m).", t.pos, t.len);
            default: fail("Something is missing before this sign.", t.pos, t.len);
        }
    }
};

// ---------------------------------------------------------------- words for kinds of quantity
struct KindName { Dim d; const char* word; };
const std::vector<KindName>& kinds() {
    static const std::vector<KindName> k = {
        {dimOf(0, 1, 0), "length"}, {dimOf(1, 0, 0), "mass"}, {dimOf(0, 0, 1), "time"}, {dimOf(0, 0, 0, 1), "electric current"}, {dimOf(0, 0, 0, 0, 1), "temperature"},
        {dimOf(0, 0, 0, 0, 0, 1), "amount of substance"}, {dimOf(0, 2, 0), "area"}, {dimOf(0, 3, 0), "volume"}, {dimOf(0, 1, -1), "speed"}, {dimOf(0, 1, -2), "acceleration"},
        {dimOf(1, 1, -2), "force"}, {dimOf(1, 2, -2), "energy"}, {dimOf(1, 2, -3), "power"}, {dimOf(1, -1, -2), "pressure"}, {dimOf(1, 1, -1), "momentum"},
        {dimOf(0, 0, -1), "frequency"}, {dimOf(0, 0, 1, 1), "electric charge"}, {dimOf(1, 2, -3, -1), "voltage"}, {dimOf(1, -3, 0), "density"},
        {dimOf(1, 2, -1), "action (energy × time)"}, {dimOf(1, 2, -3, -2), "electrical resistance"}, {dimOf(1, 0, -2, -1), "magnetic field"}};
    return k;
}
std::string describe(const Dim& d) {
    if (d.none()) return "a plain number";
    const std::string w = kindWord(d), u = unitText(d);
    if (w.empty()) return "a quantity in " + u;
    const bool vowel = std::string("aeiou").find(w[0]) != std::string::npos;
    return std::string(vowel ? "an " : "a ") + w + " (" + u + ")";
}

// ---------------------------------------------------------------- evaluation
bool tiny(double v) { return v != 0 && std::fabs(v) < DBL_MIN; }
// 0.50.2, the underflow rule: a value below the smallest normal double (DBL_MIN = 2.2e-308) is 0, and a note says so; + and sum() just use it (1 + exp(-800) = 1,
// a partition function's far terms). An overflow is still an error, and so is a number TYPED below 1e-307.
const char* const kUnderflowNote = "A value below 2.2×10⁻³⁰⁸ (the smallest number the computer keeps with full precision) came up in this line: it is shown as 0 and counted as 0.";

// sin, cos, tan at an exact multiple of a right angle (as exact as the computer's π allows: within 8 rounding steps): the exact value, not 1.2e-16
constexpr int kMaxJ = 200;      // 0.50.2: cg and wigner3j take every j up to 200 (exact sums; tested to 12 digits up to j = 100 and in the stretched case to 200)

bool rightAngle(double x, long long& k) {
    if (!std::isfinite(x) || std::fabs(x) > 1e9) return false;
    const double q = x / (kPi / 2), r = std::nearbyint(q);
    if (r == 0) return false;
    if (std::fabs(x - r * (kPi / 2)) > 8 * DBL_EPSILON * std::fabs(x)) return false;
    k = static_cast<long long>(r);
    return true;
}

// ---------------------------------------------------------------- 0.54.2: small matrices (the arithmetic; the rules are in calc.h)
using cplx = std::complex<double>;
constexpr double kSumNoise = 4 * std::numeric_limits<double>::epsilon();      // as for numbers: what is left of x - x after rounding is 0
std::string sizeText(const Matrix& m) { return std::to_string(m.rows) + "x" + std::to_string(m.cols); }
// a part below 1e-15 of the whole entry is rounding (as for a complex number: exp(iπ) = -1)
cplx cleanParts(cplx z) {
    const double mag = std::abs(z);
    return {std::fabs(z.real()) <= 1e-15 * mag ? 0 : z.real(), std::fabs(z.imag()) <= 1e-15 * mag ? 0 : z.imag()};
}
Matrix matProduct(const Matrix& a, const Matrix& b) {
    Matrix m; m.rows = a.rows; m.cols = b.cols;
    for (int i = 0; i < a.rows; i++)
        for (int j = 0; j < b.cols; j++) {
            cplx sum = 0;
            double size = 0;
            for (int k = 0; k < a.cols; k++) { const cplx t = a.at(i, k) * b.at(k, j); sum += t; size += std::abs(t); }
            double re = sum.real(), im = sum.imag();
            if (std::fabs(re) <= kSumNoise * size) re = 0;                    // terms that cancel (σx σy + σy σx)
            if (std::fabs(im) <= kSumNoise * size) im = 0;
            m.at(i, j) = cleanParts({re, im});
        }
    return m;
}
Matrix matIdentity(int n) { Matrix m; m.rows = m.cols = n; for (int i = 0; i < n; i++) m.at(i, i) = 1; return m; }
// the determinant by the cofactor expansion along the first row (at most 4x4: no division, so whole-number matrices come out exact)
cplx detOf(const cplx* a, int n) {
    if (n == 1) return a[0];
    if (n == 2) return a[0] * a[3] - a[1] * a[2];
    cplx sum = 0;
    for (int col = 0; col < n; col++) {
        cplx minor[9];
        int k = 0;
        for (int i = 1; i < n; i++) for (int j = 0; j < n; j++) if (j != col) minor[k++] = a[i * n + j];
        const cplx term = a[col] * detOf(minor, n - 1);
        sum += (col % 2) ? -term : term;
    }
    return sum;
}
// The eigenvalues of a Hermitian matrix h (exactly Hermitian: the caller symmetrised it), smallest first. 1x1 and 2x2 by their closed forms
// ((a + b)/2 ± √(((a − b)/2)² + |m|²)); 3x3 and 4x4 by cyclic Jacobi rotations (Golub & Van Loan, "Matrix Computations", the cyclic Jacobi method, as in
// atom/zeeman.cpp; here each plane rotation carries the phase of the off-diagonal entry, so that it works on complex Hermitian matrices). Written here
// because the Zeeman effect's diagonaliser (atom/zeeman.cpp) is for real symmetric matrices only.
std::vector<double> hermitianEigenvalues(Matrix h) {
    const int n = h.rows;
    std::vector<double> w;
    if (n == 1) return {h.at(0, 0).real()};
    if (n == 2) {
        const double a = h.at(0, 0).real(), b = h.at(1, 1).real(), mid = (a + b) / 2, r = std::hypot((a - b) / 2, std::abs(h.at(0, 1)));
        return {mid - r, mid + r};
    }
    for (int sweep = 0; sweep < 60; sweep++) {
        double off = 0;
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) off += std::abs(h.at(p, q));
        if (off == 0) break;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) {
                const double m = std::abs(h.at(p, q));
                if (m == 0) continue;
                const double app = h.at(p, p).real(), aqq = h.at(q, q).real(), g = 100 * m;
                if (sweep > 3 && std::fabs(app) + g == std::fabs(app) && std::fabs(aqq) + g == std::fabs(aqq)) { h.at(p, q) = h.at(q, p) = 0; continue; }   // too small to matter
                const cplx ph = h.at(p, q) / m;                               // e^(iφ)
                const double d = aqq - app;
                double t;
                if (std::fabs(d) + g == std::fabs(d)) t = m / d;
                else { const double theta = 0.5 * d / m; t = 1 / (std::fabs(theta) + std::sqrt(theta * theta + 1)); if (theta < 0) t = -t; }
                const double c = 1 / std::sqrt(t * t + 1), sn = t * c;
                for (int k = 0; k < n; k++) {                                 // the columns: A J
                    const cplx kp = h.at(k, p), kq = h.at(k, q);
                    h.at(k, p) = c * kp - sn * std::conj(ph) * kq;
                    h.at(k, q) = sn * ph * kp + c * kq;
                }
                for (int k = 0; k < n; k++) {                                 // the rows: J† (A J)
                    const cplx pk = h.at(p, k), qk = h.at(q, k);
                    h.at(p, k) = c * pk - sn * ph * qk;
                    h.at(q, k) = sn * std::conj(ph) * pk + c * qk;
                }
                h.at(p, q) = h.at(q, p) = 0;
                h.at(p, p) = h.at(p, p).real();
                h.at(q, q) = h.at(q, q).real();
            }
    }
    for (int i = 0; i < n; i++) w.push_back(h.at(i, i).real());
    std::sort(w.begin(), w.end());
    return w;
}
const char* pauliAxis(const std::string& name) {       // "x", "y", "z" for a Pauli matrix's name, else nullptr
    for (const char* a : {"x", "y", "z"}) if (name == std::string("sigma") + a || name == std::string("σ") + a) return a;
    return nullptr;
}
Matrix pauliMatrix(char axis) {
    Matrix m; m.rows = m.cols = 2;
    if (axis == 'x') { m.at(0, 1) = 1; m.at(1, 0) = 1; }
    else if (axis == 'y') { m.at(0, 1) = cplx(0, -1); m.at(1, 0) = cplx(0, 1); }
    else { m.at(0, 0) = 1; m.at(1, 1) = -1; }
    return m;
}
const char* pauliWritten(char axis) { return axis == 'x' ? "[[0, 1], [1, 0]]" : axis == 'y' ? "[[0, -i], [i, 0]]" : "[[1, 0], [0, -1]]"; }
// a near miss of a Pauli matrix's name: sigma_x, sigmaX, pauli_x, σ_x → "sigmax"
std::string pauliGuess(const std::string& s) {
    std::string l;
    for (char c : s) if (c != '_') l += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* a : {"x", "y", "z"})
        if (l == std::string("sigma") + a || l == std::string("pauli") + a || l == std::string("σ") + a) return std::string("sigma") + a;
    return "";
}

struct Eval {
    const std::vector<Node>& nodes;
    const Quantity* ans;
    std::vector<std::string> used;      // the names, in the order first used
    std::vector<std::string> notes;     // things worth knowing about the answer (sin of a plain number is in radians)
    std::vector<std::pair<std::string, Quantity>> vars;   // 0.50.1: the index of a sum() or the unknown of a solve() being worked out (innermost last)
    explicit Eval(const std::vector<Node>& n, const Quantity* a) : nodes(n), ans(a) {}
    void note(const std::string& s) { if (std::find(notes.begin(), notes.end(), s) == notes.end()) notes.push_back(s); }
    void underflow() { note(kUnderflowNote); }
    bool complexOk = false;
    const Vars* userVars = nullptr;             // 0.50.2: the line uses i (or a complex ans): then sqrt(-4) and (-1)^0.5 are imaginary instead of refused
    // a complex result's parts, checked; the rounding left by π in exp(iπ) (1.2e-16) is dropped: a part below 1e-15 of the whole is 0
    Quantity complexOut(std::complex<double> z, const Dim& d, const Node& n, double hb = 0, double hp = 0) {
        const double mag = std::abs(z);
        double re = z.real(), im = z.imag();
        if (std::fabs(im) <= 1e-15 * mag) im = 0;
        if (std::fabs(re) <= 1e-15 * mag) re = 0;
        Quantity q{checked(re, n), d, hb, hp, checked(im, n)};
        return q;
    }
    static std::complex<double> cz(const Quantity& q) { return {q.v, q.im}; }
    [[noreturn]] void realOnly(const std::string& f, const Node& n) { fail(f + " works on real numbers only, not on a complex number.", n.pos, n.len); }
    double checked(double v, const Node& n) {
        if (std::isnan(v)) fail("This has no answer (it is not a number).", n.pos, n.len);
        if (std::isinf(v)) fail("The answer is too big for the calculator (over about 10^308).", n.pos, n.len);
        if (tiny(v)) { underflow(); return 0; }
        return v;
    }

    void use(const std::string& name) { if (std::find(used.begin(), used.end(), name) == used.end()) used.push_back(name); }

    // 0.54.2: small matrices
    Quantity matOut(Matrix m, const Dim& d, const Node& n, double hb, double hp) {
        for (int k = 0; k < m.rows * m.cols; k++) { cplx& z = m.a[static_cast<std::size_t>(k)]; z = cplx(checked(z.real(), n), checked(z.imag(), n)); }
        Quantity q; q.dim = d; q.hbarPow = hb; q.hPow = hp; q.mat = std::make_shared<const Matrix>(m);
        return q;
    }
    [[noreturn]] void notSupported(const std::string& f, const Node& n) {
        fail(f + " of a matrix is not supported: the calculator has no matrix functions (no matrix exponential, square root or inverse). "
                 "It has transpose, dagger, conj, det, tr, eig, kron, identity and whole powers.", n.pos, n.len);
    }
    Quantity matLiteral(const Node& n);
    Quantity matAdd(const Node& n, const Quantity& a, const Quantity& b);
    Quantity matMul(const Node& n, const Quantity& a, const Quantity& b);
    Quantity matDiv(const Node& n, const Quantity& a, const Quantity& b);
    Quantity matPow(const Node& n, const Quantity& x, const Quantity& p);
    Quantity matCall(const Node& n);

    Quantity symbol(const Node& n) {
        const std::string& s = n.text;
        for (auto it = vars.rbegin(); it != vars.rend(); ++it) if (it->first == s) return it->second;
        if (s == "i") { use(s); Quantity q; q.im = 1; return q; }                // 0.50.2: the imaginary unit
        if (const char* axis = pauliAxis(s)) { use(s); Quantity q; q.mat = std::make_shared<const Matrix>(pauliMatrix(axis[0])); return q; }   // 0.54.2: σx, σy, σz
        if (userVars) { const auto it = userVars->find(s); if (it != userVars->end()) { use(s); return it->second; } }   // 0.50.2: a named variable
        if (s == "ans") {
            if (!ans) fail("There is no earlier answer yet: \"ans\" is the last answer.", n.pos, n.len);
            use(s);
            return *ans;
        }
        if (const Symbol* sym = findSymbol(s)) {
            use(s);
            Quantity q = sym->q;
            if (s == "ħ" || s == "hbar") q.hbarPow = 1;                 // 0.50.2: the Planck powers, carried like a dimension
            else if (s == "hP") q.hPow = 1;
            return q;
        }
        if (s == "h") fail("\"h\" could mean hours or Planck's constant: write hr for hours (100 km/hr) or hP for Planck's constant.", n.pos, n.len);
        if (s == "log") fail("Write ln for the natural logarithm or log10 for base 10.", n.pos, n.len);
        if (s == "in") fail("To show an answer in another unit, use \"to\": 5 km to mi.", n.pos, n.len);
        if (s == "°C" || s == "°F" || s == "degC" || s == "degF" || s == "celsius" || s == "Celsius" || s == "fahrenheit" || s == "Fahrenheit" || s == "°K")
            fail("Temperatures work in kelvin only: write K (0 °C = 273.15 K).", n.pos, n.len);
        {
            std::size_t k = s.size();
            while (k > 0 && s[k - 1] >= '0' && s[k - 1] <= '9') k--;
            if (k > 0 && k < s.size() && findSymbol(s.substr(0, k)))
                fail("For a power write " + s.substr(0, k) + "^" + s.substr(k) + " (or with a raised number, like m²).", n.pos, n.len);
        }
        // 0.54.2: the Pauli matrices' names (sigma and σ alone are the Stefan-Boltzmann constant)
        if (const std::string g = pauliGuess(s); !g.empty())
            fail("\"" + s + "\" is not a name the calculator knows. Did you mean " + g + " (the Pauli matrix σ" + g.substr(5) + ", also typed σ" + g.substr(5) + ")?", n.pos, n.len);
        if (s == "pauli" || s == "Pauli" || s == "paulis") fail("The Pauli matrices are sigmax, sigmay and sigmaz (also typed σx, σy, σz).", n.pos, n.len);
        std::string msg = "\"" + s + "\" is not a unit or constant the calculator knows.";
        if (s == "x" || s == "y" || s == "z")
            for (const Node& o : nodes)
                if (o.k == NK::Sym && (o.text == "sigma" || o.text == "σ")) {
                    fail(msg + " For the Pauli matrix write sigma" + s + " with no space (also typed σ" + s + "): sigma alone is the Stefan-Boltzmann constant.", n.pos, n.len);
                }
        const std::vector<std::string> near = suggestions(s);
        if (!near.empty()) {
            msg += " Did you mean ";
            for (std::size_t k = 0; k < near.size(); k++) msg += (k ? (k + 1 == near.size() ? " or " : ", ") : "") + near[k];
            msg += "?";
        }
        fail(msg, n.pos, n.len);
    }
    static std::vector<std::string> suggestions(const std::string& s) {
        std::vector<std::string> out;
        const auto lower = [](std::string x) { for (char& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return x; };
        const std::string ls = lower(s);
        for (const Symbol& y : table().list) if (y.name != s && lower(y.name) == ls) out.push_back(y.name);
        if (out.empty() && s.size() >= 3) {
            for (const Symbol& y : table().list) {
                if (!y.listed || y.name.size() < 2) continue;
                const std::string& a = s; const std::string& b = y.name;   // one letter changed, added or removed
                const std::size_t la = a.size(), lb = b.size();
                if ((la > lb ? la - lb : lb - la) > 1) continue;
                std::size_t p = 0;
                while (p < la && p < lb && a[p] == b[p]) p++;
                std::size_t qa = la, qb = lb;
                while (qa > p && qb > p && a[qa - 1] == b[qb - 1]) { qa--; qb--; }
                if (qa - p <= 1 && qb - p <= 1) out.push_back(b);
                if (out.size() >= 3) break;
            }
        }
        if (out.size() > 3) out.resize(3);
        return out;
    }

    Quantity run(int id) {
        const Node& n = nodes[id];
        switch (n.k) {
            case NK::Num: return {n.num, Dim{}};
            case NK::Sym: return symbol(n);
            case NK::Mat: return matLiteral(n);
            case NK::Neg: { Quantity q = run(n.a);
                if (q.mat) { Matrix m = *q.mat; m.list = false; for (cplx& z : m.a) { z = -z; if (z.real() == 0) z.real(0); if (z.imag() == 0) z.imag(0); } return matOut(m, q.dim, n, q.hbarPow, q.hPow); }
                q.v = -q.v; q.im = -q.im; if (q.v == 0) q.v = 0; if (q.im == 0) q.im = 0; return q; }
            case NK::Add: case NK::Sub: {
                const Quantity a = run(n.a), b = run(n.b);
                if (a.mat || b.mat) return matAdd(n, a, b);
                if (a.dim != b.dim)
                    fail("You can't " + std::string(n.k == NK::Add ? "add " : "subtract ") + describe(b.dim) + (n.k == NK::Add ? " to " : " from ") + describe(a.dim) +
                         ": they are different kinds of quantity.", n.pos, n.len);
                double v = n.k == NK::Add ? a.v + b.v : a.v - b.v;
                // what is left of x - x after rounding (0.1 + 0.2 - 0.3) is 0. 0.50.3 (outside review): 4 units in the last place (4 × 2.2e-16), not 1e-15: 1 - 0.999999999999999 kept its 1e-15
                constexpr double kNoise = 4 * std::numeric_limits<double>::epsilon();
                if (v != 0 && std::fabs(v) <= kNoise * std::max(std::fabs(a.v), std::fabs(b.v))) v = 0;
                double w = n.k == NK::Add ? a.im + b.im : a.im - b.im;
                if (w != 0 && std::fabs(w) <= kNoise * std::max(std::fabs(a.im), std::fabs(b.im))) w = 0;
                return {checked(v, n), a.dim, mix(a.hbarPow, b.hbarPow), mix(a.hPow, b.hPow), checked(w, n)};
            }
            case NK::Mul: case NK::Juxt: {
                const Quantity a = run(n.a), b = run(n.b);
                if (a.mat || b.mat) return matMul(n, a, b);
                if (a.im != 0 || b.im != 0) {
                    const std::complex<double> z = cz(a) * cz(b);
                    if (z == 0.0 && cz(a) != 0.0 && cz(b) != 0.0) underflow();
                    return complexOut(z, dimAdd(a.dim, b.dim, 1), n, a.hbarPow + b.hbarPow, a.hPow + b.hPow);
                }
                const double v = a.v * b.v;
                if (v == 0 && a.v != 0 && b.v != 0) underflow();
                return {checked(v, n), dimAdd(a.dim, b.dim, 1), a.hbarPow + b.hbarPow, a.hPow + b.hPow};
            }
            case NK::Div: {
                const Quantity a = run(n.a), b = run(n.b);
                if (a.mat || b.mat) return matDiv(n, a, b);
                if (b.v == 0 && b.im == 0) fail("You can't divide by zero.", n.pos, n.len);
                if (b.im != 0) {
                    const std::complex<double> z = cz(a) / cz(b);
                    if (z == 0.0 && cz(a) != 0.0) underflow();
                    return complexOut(z, dimAdd(a.dim, b.dim, -1), n, a.hbarPow - b.hbarPow, a.hPow - b.hPow);
                }
                if (a.im != 0) return {checked(a.v / b.v, n), dimAdd(a.dim, b.dim, -1), a.hbarPow - b.hbarPow, a.hPow - b.hPow, checked(a.im / b.v, n)};
                const double v = a.v / b.v;
                if (v == 0 && a.v != 0) underflow();
                return {checked(v, n), dimAdd(a.dim, b.dim, -1), a.hbarPow - b.hbarPow, a.hPow - b.hPow};
            }
            case NK::Pow: return pow(n, run(n.a), run(n.b));
            case NK::Call: {
                if (n.text == "sum") return sum(n);
                if (n.text == "solve") return solve(n);
                if (n.text == "transpose" || n.text == "dagger" || n.text == "det" || n.text == "tr" || n.text == "identity" || n.text == "kron" || n.text == "eig") return matCall(n);
                if (n.args.size() <= 1 && n.text != "fact" && n.text != "gamma" && n.text != "lngamma" && n.text != "erf" && n.text != "erfc") return call(n, run(n.a));
                std::vector<Quantity> a;
                for (int id : n.args) a.push_back(run(id));
                return special(n, a);
            }
        }
        fail("Internal error.");
    }

    static double mix(double a, double b) { return a == b ? a : NAN; }     // a sum of terms with different Planck powers: unknown (no ω note)
    // x^p for a plain p. A negative x only to a whole power or to a fraction with an odd denominator (cube roots); a quantity with units only to a simple fraction.
    Quantity pow(const Node& n, const Quantity& x, const Quantity& p) {
        if (x.mat || p.mat) return matPow(n, x, p);
        if (!p.dim.none()) fail("A power must be a plain number, not " + describe(p.dim) + ".", n.pos, n.len);
        if (p.im != 0) {                                                        // 0.50.2: x^(a + bi) = exp((a + bi) ln x), for a plain x only
            if (!x.dim.none()) fail("A quantity with a unit can't be raised to a complex power.", n.pos, n.len);
            if (x.v == 0 && x.im == 0) fail("0 to a complex power has no value here.", n.pos, n.len);
            return complexOut(std::exp(cz(p) * std::log(cz(x))), Dim{}, n);
        }
        // 0.50.3 (outside review): a real negative number to a fraction with an odd denominator has a real root, given on every line ((-8)^(1/3) = -2 also when the line uses i)
        Rat oddRoot{};
        const bool realRoot = x.im == 0 && x.v < 0 && ratFromDouble(p.v, oddRoot) && (oddRoot.d % 2) == 1;
        if (x.im != 0 || (x.v < 0 && complexOk && std::nearbyint(p.v) != p.v && !realRoot)) {
            Rat r{};
            Dim d;
            if (!x.dim.none()) {
                if (!ratFromDouble(p.v, r)) fail("A quantity with a unit can only be raised to a whole number or a simple fraction (like 2, -1 or 1/2).", n.pos, n.len);
                d = dimScale(x.dim, r);
            }
            std::complex<double> z;
            if (std::nearbyint(p.v) == p.v && std::fabs(p.v) <= 1e6) {          // a whole power by repeated squaring: i^2 = -1 exactly
                std::complex<double> b = cz(x), acc = 1;
                long long k = static_cast<long long>(std::fabs(p.v));
                while (k) { if (k & 1) acc *= b; b *= b; k >>= 1; }
                z = p.v < 0 ? 1.0 / acc : acc;
            } else z = std::pow(cz(x), p.v);                                    // the principal value
            return complexOut(z, d, n, x.hbarPow * p.v, x.hPow * p.v);
        }
        Rat r{};
        const bool simple = ratFromDouble(p.v, r);
        Dim d;
        if (!x.dim.none()) {
            if (!simple) fail("A quantity with a unit can only be raised to a whole number or a simple fraction (like 2, -1 or 1/2).", n.pos, n.len);
            d = dimScale(x.dim, r);
        }
        double v;
        if (x.v == 0) {
            if (p.v < 0) fail("You can't divide by zero (0 to a negative power).", n.pos, n.len);
            v = p.v == 0 ? 1 : 0;
        } else if (x.v < 0) {
            if (std::nearbyint(p.v) == p.v) v = std::pow(x.v, p.v);
            else if (simple && (r.d % 2) == 1) v = ((r.n % 2) != 0 ? -1.0 : 1.0) * std::pow(-x.v, p.v);   // (-8)^(1/3) = -2
            else fail("A negative number has no real answer to this power (like the square root of -1).", n.pos, n.len);
        } else v = std::pow(x.v, p.v);
        if (v == 0 && x.v != 0) underflow();
        return {checked(v, n), d, x.hbarPow * p.v, x.hPow * p.v};
    }

    Quantity special(const Node& n, const std::vector<Quantity>& a);
    Quantity sum(const Node& n);
    Quantity solve(const Node& n);

    bool hasAngle(int id) const {       // does the argument say it is an angle (deg, rad, π, an inverse function, ans)?
        const Node& n = nodes[id];
        if (n.k == NK::Sym) return n.text == "deg" || n.text == "°" || n.text == "rad" || n.text == "arcmin" || n.text == "arcsec" || n.text == "π" || n.text == "pi" || n.text == "ans";
        if (n.k == NK::Call && (n.text == "asin" || n.text == "acos" || n.text == "atan")) return true;
        return (n.a >= 0 && hasAngle(n.a)) || (n.b >= 0 && hasAngle(n.b));
    }
    Quantity call(const Node& n, const Quantity& x) {
        const std::string& f = n.text;
        if (x.mat) {                                                            // 0.54.2: conj works entry by entry; the other functions are for numbers
            if (f != "conj") notSupported(f, n);
            Matrix m = *x.mat; m.list = false;
            for (cplx& z : m.a) z = cplx(z.real(), z.imag() == 0 ? 0 : -z.imag());
            return matOut(m, x.dim, n, x.hbarPow, x.hPow);
        }
        // 0.50.2: complex numbers
        if (f == "re") return {x.v, x.dim, x.hbarPow, x.hPow};
        if (f == "im") return {x.im, x.dim, x.hbarPow, x.hPow};
        if (f == "conj") return {x.v, x.dim, x.hbarPow, x.hPow, x.im == 0 ? 0 : -x.im};
        if (f == "arg") {
            if (x.v == 0 && x.im == 0) fail("arg(0) has no value (0 has no direction).", n.pos, n.len);
            return {std::atan2(x.im, x.v), Dim{}};
        }
        if (f == "abs" && x.im != 0) return {checked(std::hypot(x.v, x.im), n), x.dim, x.hbarPow, x.hPow};
        if (f == "sqrt" && (x.im != 0 || (x.v < 0 && complexOk))) return complexOut(std::sqrt(cz(x)), dimScale(x.dim, Rat{1, 2}), n, x.hbarPow / 2, x.hPow / 2);
        if (f == "exp" && x.im != 0) {
            if (!x.dim.none()) fail("exp works on a plain number, not on " + describe(x.dim) + ".", n.pos, n.len);
            return complexOut(std::exp(cz(x)), Dim{}, n);
        }
        if (x.im != 0) realOnly(f, n);
        if (f == "sqrt") {
            if (x.v < 0) fail("A negative number has no real square root.", n.pos, n.len);
            return {std::sqrt(x.v), dimScale(x.dim, Rat{1, 2}), x.hbarPow / 2, x.hPow / 2};
        }
        if (f == "cbrt") return {std::cbrt(x.v), dimScale(x.dim, Rat{1, 3}), x.hbarPow / 3, x.hPow / 3};
        if (f == "abs") return {std::fabs(x.v), x.dim, x.hbarPow, x.hPow};
        if (!x.dim.none()) {
            const bool trig = f == "sin" || f == "cos" || f == "tan";
            fail(f + " works on a plain number" + (trig ? " or an angle (sin(30 deg) or sin(0.5) in radians)" : "") + ", not on " + describe(x.dim) + ".", n.pos, n.len);
        }
        const double a = x.v;
        if ((f == "sin" || f == "cos" || f == "tan") && a != 0 && !hasAngle(n.a)) {
            const std::string arg = show(nodes, n.a), deg = prec(nodes[n.a].k) < 5 ? "(" + arg + ") deg" : arg + " deg";
            if (notes.empty()) notes.push_back("sin, cos and tan work in radians: here " + arg + " means " + arg + " radians. For degrees write " + f + "(" + deg + ").");
        }
        long long k = 0;
        double v = 0;
        if (f == "sin") v = rightAngle(a, k) ? (k % 2 == 0 ? 0.0 : (((k % 4) + 4) % 4 == 1 ? 1.0 : -1.0)) : std::sin(a);
        else if (f == "cos") v = rightAngle(a, k) ? (k % 2 != 0 ? 0.0 : (((k % 4) + 4) % 4 == 0 ? 1.0 : -1.0)) : std::cos(a);
        else if (f == "tan") {
            if (rightAngle(a, k)) {
                if (k % 2 != 0) fail("tan has no value at a right angle (90°, 270°, ...).", n.pos, n.len);
                v = 0;
            } else v = std::tan(a);
        } else if (f == "asin" || f == "acos") {
            if (a < -1 || a > 1) fail(f + " works only between -1 and 1.", n.pos, n.len);
            v = f == "asin" ? std::asin(a) : std::acos(a);
        } else if (f == "atan") v = std::atan(a);
        else if (f == "sinh") v = std::sinh(a);
        else if (f == "cosh") v = std::cosh(a);
        else if (f == "tanh") v = std::tanh(a);
        else if (f == "exp") v = std::exp(a);
        else if (f == "ln" || f == "log10") {
            if (a <= 0) fail(f + " works only on numbers above zero.", n.pos, n.len);
            v = f == "ln" ? std::log(a) : std::log10(a);
        } else fail("Unknown function " + f + ".", n.pos, n.len);
        if (v == 0 && f == "exp") underflow();
        return {checked(v, n), Dim{}};
    }
};

// ---------------------------------------------------------------- 0.50.1: special functions, Clebsch-Gordan coefficients (plain numbers only)
// n! for n = 0..170 (171! is over the largest double); exact while it fits in 53 bits (up to 22!)
double factorial(int n) { double f = 1; for (int k = 2; k <= n; k++) f *= k; return f; }

// 0.54.0: the exact Wigner 3j symbol moved to atom/threej.{h,cpp} (one copy for the calculator and the Zeeman effect, atom/zeeman.h)
using atom::threeJ;

Quantity Eval::special(const Node& n, const std::vector<Quantity>& a) {
    const std::string& f = n.text;
    for (const Quantity& q : a) {
        if (q.mat) notSupported(f, n);
        if (q.im != 0) realOnly(f, n);
        if (!q.dim.none()) fail(f + " works on plain numbers, not on " + describe(q.dim) + ".", n.pos, n.len);
    }
    const auto whole = [&](double v, double lo, double hi, const std::string& msg) -> int {
        if (std::nearbyint(v) != v || v < lo || v > hi) fail(msg, n.pos, n.len);
        return static_cast<int>(v);
    };
    const auto unit = [&](double x) { if (x < -1 || x > 1) fail(usage(f) + " needs x between -1 and 1.", n.pos, n.len); };
    double v = 0;
    if (f == "fact") {
        const int k = whole(a[0].v, 0, 170, "fact(n) and n! need a whole number n from 0 to 170 (171! is over 10^308).");
        v = factorial(k);
    } else if (f == "gamma") {
        const double x = a[0].v;
        if (x <= 0 && std::nearbyint(x) == x) fail("Γ has no value at 0 or a negative whole number (a pole).", n.pos, n.len);
        v = std::tgamma(x);
    } else if (f == "lngamma") {
        if (a[0].v <= 0) fail("lngamma works only on numbers above zero (it is ln Γ(x)).", n.pos, n.len);
        v = std::lgamma(a[0].v);
    } else if (f == "erf") v = std::erf(a[0].v);
    else if (f == "erfc") { v = std::erfc(a[0].v); if (v == 0) underflow(); }
    else if (f == "besselj") {                                                  // J_ν(x), DLMF 10.2.2; std::cyl_bessel_j wants x >= 0 and ν >= 0
        const double nu = a[0].v, x = a[1].v;
        const bool integer = std::nearbyint(nu) == nu;
        if (std::fabs(nu) > 1000) fail("besselj(n, x) works for orders up to 1000.", n.pos, n.len);
        if (!integer && (nu < 0 || x < 0)) fail("besselj(ν, x) with a order that is not whole needs ν ≥ 0 and x ≥ 0.", n.pos, n.len);
        double sign = 1;
        double order = nu;
        if (integer && nu < 0) { order = -nu; if (static_cast<long long>(order) % 2) sign = -sign; }         // J_-n = (-1)^n J_n
        if (x < 0 && static_cast<long long>(order) % 2) sign = -sign;                                         // J_n(-x) = (-1)^n J_n(x)
        try { v = sign * std::cyl_bessel_j(order, std::fabs(x)); } catch (const std::exception&) { fail("besselj has no value here.", n.pos, n.len); }
    } else if (f == "sphbessel") {                                              // j_l(x), DLMF 10.47.3; j_l(-x) = (-1)^l j_l(x)
        const int l = whole(a[0].v, 0, 127, "sphbessel(l, x) needs a whole number l from 0 to 127.");
        const double x = a[1].v;
        if (x == 0) v = l == 0 ? 1 : 0;
        else {
            try { v = std::sph_bessel(static_cast<unsigned>(l), std::fabs(x)); } catch (const std::exception&) { fail("sphbessel has no value here.", n.pos, n.len); }
            if (x < 0 && (l % 2)) v = -v;
        }
    } else if (f == "legendre" || f == "assoclegendre") {
        // P_l^m(x) WITH the Condon-Shortley phase (-1)^m (DLMF 14.7.10, the Ferrers function; Arfken; the convention of the spherical harmonics in most QM texts).
        // C++17's std::assoc_legendre leaves the phase out, so the recurrences are done here: P_m^m = (-1)^m (2m-1)!! (1-x²)^(m/2), P_(m+1)^m = (2m+1) x P_m^m,
        // (l-m+1) P_(l+1)^m = (2l+1) x P_l^m - (l+m) P_(l-1)^m (DLMF 14.10.3 and 14.7.15/16). m < 0: P_l^-m = (-1)^m (l-m)!/(l+m)! P_l^m (DLMF 14.9.3).
        const bool assoc = f == "assoclegendre";
        const int l = whole(a[0].v, 0, 127, usage(f) + " needs a whole number l from 0 to 127.");
        const int m = assoc ? whole(a[1].v, -1e9, 1e9, "assoclegendre(l, m, x) needs a whole number m.") : 0;
        const double x = a[assoc ? 2 : 1].v;
        unit(x);
        const int am = std::abs(m);
        if (am > l) {                                                           // 0.50.2: 0, the standard value (DLMF 14.7: P_l^m = 0 for |m| > l), not an error
            note("It is 0: |m| > l (P_l^m is 0 there).");
            return {0, Dim{}};
        }
        double pmm = 1;
        const double sq = std::sqrt((1 - x) * (1 + x));
        for (int k = 1; k <= am; k++) pmm *= -(2 * k - 1) * sq;
        double p = pmm;
        if (l > am) {
            double prev = pmm, cur = x * (2 * am + 1) * pmm;
            for (int ll = am + 1; ll < l; ll++) { const double next = ((2 * ll + 1) * x * cur - (ll + am) * prev) / (ll - am + 1); prev = cur; cur = next; }
            p = cur;
        }
        if (m < 0) p *= ((am % 2) ? -1.0 : 1.0) * factorial(l - am) / factorial(l + am);
        v = p;
        if (assoc) note("assoclegendre includes the Condon-Shortley phase (-1)^m (as DLMF 14.7.10 and most QM texts; C++'s std::assoc_legendre leaves it out): "
                        "P_1^1(x) = -√(1 - x²).");
    } else if (f == "laguerre" || f == "assoclaguerre") {
        // the generalized Laguerre polynomial L_n^(k)(x) (DLMF 18.5.12; no n! factor): L_0 = 1, L_1 = 1 + k - x,
        // (n+1) L_(n+1) = (2n + 1 + k - x) L_n - (n + k) L_(n-1) (DLMF 18.9.1). Done here: libstdc++'s std::laguerre refuses x < 0.
        const bool assoc = f == "assoclaguerre";
        const int nn = whole(a[0].v, 0, 127, usage(f) + " needs a whole number n from 0 to 127.");
        const int k = assoc ? whole(a[1].v, 0, 127, "assoclaguerre(n, k, x) needs a whole number k from 0 to 127.") : 0;
        const double x = a[assoc ? 2 : 1].v;
        double prev = 1, cur = 1 + k - x;
        if (nn == 0) cur = 1;
        for (int j = 1; j < nn; j++) { const double next = ((2 * j + 1 + k - x) * cur - (j + k) * prev) / (j + 1); prev = cur; cur = next; }
        v = cur;
        if (assoc) note("assoclaguerre is the generalized Laguerre polynomial L_n^(k)(x) of DLMF 18.5.12 (no n! factor); hydrogen's radial functions use L_(n-l-1)^(2l+1). "
                        "Older QM books (Schiff, Pauling-Wilson) use another normalisation.");
    } else if (f == "hermite") {
        // physicists' Hermite H_n (DLMF 18.9.1 / Table 18.9.1): H_0 = 1, H_1 = 2x, H_(n+1) = 2x H_n - 2n H_(n-1)
        const int nn = whole(a[0].v, 0, 127, "hermite(n, x) needs a whole number n from 0 to 127.");
        const double x = a[1].v;
        double prev = 1, cur = 2 * x;
        if (nn == 0) cur = 1;
        for (int j = 1; j < nn; j++) { const double next = 2 * x * cur - 2 * j * prev; prev = cur; cur = next; }
        v = cur;
    } else if (f == "binom") {
        const double nd = a[0].v, kd = a[1].v;
        if (std::nearbyint(nd) != nd || std::nearbyint(kd) != kd || nd < 0 || nd > 1e9 || std::fabs(kd) > 1e9) fail("binom(n, k) needs whole numbers, n from 0 to 10^9.", n.pos, n.len);
        if (kd < 0 || kd > nd) {                                                // 0.50.2: 0, the standard value (DLMF 26.3.1), not an error
            note(kd < 0 ? "It is 0: k < 0 (no way to choose fewer than none)." : "It is 0: k > n (no way to choose more than there are).");
            return {0, Dim{}};
        }
        const double k = std::min(kd, nd - kd);
        v = 1;
        for (double i = 1; i <= k; i++) { v = v * (nd - k + i) / i; if (!std::isfinite(v)) break; }
    } else if (f == "cg" || f == "wigner3j") {
        const bool cg = f == "cg";
        const char* const names = cg ? "j1, j2 and J" : "j1, j2 and j3";       // 0.50.2: the names of the function that was called (cg's third j is J)
        int t[6];
        for (int k = 0; k < 6; k++) {
            const double d = 2 * a[k].v;
            if (!std::isfinite(d) || std::fabs(d - std::nearbyint(d)) > 1e-9)
                fail(usage(f) + ": every j and m must be a whole or half-whole number (0, 0.5, 1, 1.5 ...; 1/2 also works).", n.pos, n.len);
            if (std::fabs(d) > 2 * kMaxJ) fail(usage(f) + " works for " + names + " up to " + std::to_string(kMaxJ) + ".", n.pos, n.len);
            t[k] = static_cast<int>(std::nearbyint(d));
        }
        // the j's and m's in the order (j1, m1, j2, m2, j3, m3)
        int j[3], m[3];
        if (cg) { j[0] = t[0]; m[0] = t[1]; j[1] = t[2]; m[1] = t[3]; j[2] = t[4]; m[2] = t[5]; }
        else { j[0] = t[0]; j[1] = t[1]; j[2] = t[2]; m[0] = t[3]; m[1] = t[4]; m[2] = t[5]; }
        for (int k = 0; k < 3; k++) {
            if (j[k] < 0) fail(usage(f) + ": a j must not be negative.", n.pos, n.len);
            if (((j[k] + m[k]) % 2) != 0) fail(usage(f) + ": j + m must be a whole number (j = 1/2 goes with m = ±1/2, j = 1 with m = -1, 0, 1).", n.pos, n.len);
        }
        // 0.50.2: where a selection rule gives 0, the answer is 0 with a note (as sympy and Mathematica do), not an error
        const int M3 = cg ? -m[2] : m[2];
        if (std::abs(m[0]) > j[0] || std::abs(m[1]) > j[1] || std::abs(m[2]) > j[2]) note("It is 0: an |m| is larger than its j.");
        else if (m[0] + m[1] + M3 != 0) note(cg ? "It is 0: M ≠ m1 + m2." : "It is 0: m1 + m2 + m3 ≠ 0.");
        else if (j[2] < std::abs(j[0] - j[1]) || j[2] > j[0] + j[1] || ((j[0] + j[1] + j[2]) % 2) != 0)
            note(std::string("It is 0: ") + (cg ? "J" : "j3") + " is not one of |j1 - j2|, |j1 - j2| + 1, ... j1 + j2 (the triangle rule).");
        const double w = threeJ(j[0], j[1], j[2], m[0], m[1], M3);
        if (cg) {   // <j1 m1 j2 m2 | J M> = (-1)^(j1 - j2 + M) √(2J + 1) (j1 j2 J; m1 m2 -M) (Edmonds eq. (3.7.1)); Condon-Shortley phase
            v = atom::clebschGordan(j[0], m[0], j[1], m[1], j[2], m[2]);   // 0.54.0: the one copy in atom/threej.h (the Zeeman effect uses it too)
            note("cg uses the Condon-Shortley phase convention (as Edmonds, Sakurai and the PDG tables).");
        } else v = w;
        if (v == 0) v = 0;                                                      // no -0
    } else fail("Unknown function " + f + ".", n.pos, n.len);
    return {checked(v, n), Dim{}};
}

// sum(expression, k, a, b): the expression added up for k = a, a+1, ... b (whole numbers, at most a million terms), with compensated (Neumaier) summation
Quantity Eval::sum(const Node& n) {
    const std::string k = nodes[n.args[1]].text;
    const Quantity qa = run(n.args[2]), qb = run(n.args[3]);
    if (qa.im != 0 || qb.im != 0) realOnly("sum's from and to", n);
    if (qa.mat || qb.mat || !qa.dim.none() || !qb.dim.none() || std::nearbyint(qa.v) != qa.v || std::nearbyint(qb.v) != qb.v || std::fabs(qa.v) > 1e15 || std::fabs(qb.v) > 1e15)
        fail("sum(expression, k, from, to) runs over whole numbers: from and to must be whole numbers.", n.pos, n.len);
    if (qb.v < qa.v) fail("The sum has nothing to add: \"to\" is smaller than \"from\".", n.pos, n.len);
    if (qb.v - qa.v + 1 > 1e6) fail("A sum can have at most 1000000 terms.", n.pos, n.len);
    double total = 0, comp = 0, hb = 0, hp = 0, totalIm = 0, compIm = 0;
    Dim d;
    for (double x = qa.v; x <= qb.v; x++) {
        vars.push_back({k, {x, Dim{}}});
        Quantity t;
        try { t = run(n.args[0]); } catch (Err& e) { e.msg += " (at " + k + " = " + formatNumber(x) + ")"; throw; }
        vars.pop_back();
        if (t.mat) notSupported("sum", n);
        if (x == qa.v) { d = t.dim; hb = t.hbarPow; hp = t.hPow; }
        else { hb = mix(hb, t.hbarPow); hp = mix(hp, t.hPow); }
        if (t.dim != d)
            fail("You can't add " + describe(t.dim) + " to " + describe(d) + ": the terms of the sum are different kinds of quantity (at " + k + " = " + formatNumber(x) + ").", n.pos, n.len);
        const double s = total + t.v;
        comp += std::fabs(total) >= std::fabs(t.v) ? (total - s) + t.v : (t.v - s) + total;
        total = s;
        const double si = totalIm + t.im;
        compIm += std::fabs(totalIm) >= std::fabs(t.im) ? (totalIm - si) + t.im : (t.im - si) + totalIm;
        totalIm = si;
    }
    return {checked(total + comp, n), d, hb, hp, checked(totalIm + compIm, n)};
}

// solve(expression, x, a, b): an x between a and b where the expression is 0, by bisection to the last bit (the expression must change sign between a and b)
Quantity Eval::solve(const Node& n) {
    const std::string x = nodes[n.args[1]].text;
    const Quantity qa = run(n.args[2]), qb = run(n.args[3]);
    if (qa.im != 0 || qb.im != 0) realOnly("solve", n);
    if (qa.mat || qb.mat) fail("solve works on numbers, not on matrices.", n.pos, n.len);
    if (qa.dim != qb.dim) fail("solve(expression, x, a, b): a and b must be the same kind of quantity (" + describe(qa.dim) + " and " + describe(qb.dim) + ").", n.pos, n.len);
    const auto f = [&](double v) {
        vars.push_back({x, {v, qa.dim, qa.hbarPow, qa.hPow}});
        Quantity r;
        try { r = run(n.args[0]); } catch (Err& e) { e.msg += " (at " + x + " = " + formatNumber(v) + ")"; throw; }
        vars.pop_back();
        if (r.mat) fail("solve works on numbers, not on matrices.", n.pos, n.len);
        if (r.im != 0) realOnly("solve", n);
        return r.v;
    };
    double lo = std::min(qa.v, qb.v), hi = std::max(qa.v, qb.v);
    const double flo0 = f(lo), fhi0 = f(hi);
    if (flo0 == 0) return {lo, qa.dim, qa.hbarPow, qa.hPow};
    if (fhi0 == 0) return {hi, qa.dim, qa.hbarPow, qa.hPow};
    if ((flo0 > 0) == (fhi0 > 0))
        fail("The expression has the same sign at both ends (" + formatNumber(flo0, 4) + " and " + formatNumber(fhi0, 4) +
             "): solve needs a and b on either side of the answer, where the expression changes sign.", n.pos, n.len);
    double flo = flo0, fhi = fhi0;
    for (int it = 0; it < 2200; it++) {
        const double mid = lo + (hi - lo) / 2;
        if (mid <= lo || mid >= hi) break;
        double fm = 0;
        try { fm = f(mid); } catch (const Err& e) {
            fail("The expression has no value at " + x + " = " + formatNumber(mid) + ", between a and b, so it jumps there (like tan at 90°) instead of passing through 0: "
                 "give a smaller a to b. (" + e.msg + ")", n.pos, n.len);
        }
        if (fm == 0) { lo = hi = mid; flo = fhi = 0; break; }
        if ((fm > 0) == (flo > 0)) { lo = mid; flo = fm; } else { hi = mid; fhi = fm; }
    }
    const double root = std::fabs(flo) <= std::fabs(fhi) ? lo : hi, fr = std::min(std::fabs(flo), std::fabs(fhi));
    if (fr > 1e-6 * std::max(std::fabs(flo0), std::fabs(fhi0)))
        fail("The expression jumps across 0 between a and b (like tan at 90°) instead of passing through it: give a smaller a to b.", n.pos, n.len);
    note("solve found the " + x + " where the expression changes sign between a and b (bisection, to the computer's last digit); there may be other answers in that range.");
    return {root, qa.dim, qa.hbarPow, qa.hPow};
}

// ---------------------------------------------------------------- 0.54.2: small matrices (see calc.h, "matrices")
Quantity Eval::matLiteral(const Node& n) {
    Matrix m;
    m.rows = static_cast<int>(n.num);
    m.cols = static_cast<int>(n.args.size()) / m.rows;
    Dim d;
    double hb = 0, hp = 0;
    for (std::size_t k = 0; k < n.args.size(); k++) {
        const Node& en = nodes[static_cast<std::size_t>(n.args[k])];
        const Quantity e = run(n.args[k]);
        if (e.mat) fail("A matrix can't hold another matrix as an entry: each row goes in one pair of brackets, [[1, 2], [3, 4]].", en.pos, en.len);
        if (k == 0) { d = e.dim; hb = e.hbarPow; hp = e.hPow; }
        else {
            if (e.dim != d)
                fail("The entries of a matrix must all be the same kind of quantity (here " + describe(d) + " and " + describe(e.dim) +
                     "): one unit belongs to the whole matrix. Write the unit outside: [[1, 2], [3, 4]] eV.", en.pos, en.len);
            hb = mix(hb, e.hbarPow); hp = mix(hp, e.hPow);
        }
        m.a[k] = cz(e);
    }
    return matOut(m, d, n, hb, hp);
}

Quantity Eval::matAdd(const Node& n, const Quantity& a, const Quantity& b) {
    const bool add = n.k == NK::Add;
    if (!a.mat || !b.mat)
        fail(std::string("You can't ") + (add ? "add a number and a matrix" : "subtract a number and a matrix") +
             ". For M + c write M + c identity(n): [[1, 2], [3, 4]] + 5 identity(2).", n.pos, n.len);
    if (a.mat->rows != b.mat->rows || a.mat->cols != b.mat->cols)
        fail("You can only add or subtract matrices of the same size: these are " + sizeText(*a.mat) + " and " + sizeText(*b.mat) + ".", n.pos, n.len);
    if (a.dim != b.dim)
        fail("You can't " + std::string(add ? "add " : "subtract ") + describe(b.dim) + (add ? " to " : " from ") + describe(a.dim) + ": they are different kinds of quantity.", n.pos, n.len);
    Matrix m = *a.mat;
    m.list = false;
    const auto part = [&](double x, double y) { const double v = add ? x + y : x - y; return (v != 0 && std::fabs(v) <= kSumNoise * std::max(std::fabs(x), std::fabs(y))) ? 0.0 : v; };
    for (int k = 0; k < m.rows * m.cols; k++) {
        const cplx x = a.mat->a[static_cast<std::size_t>(k)], y = b.mat->a[static_cast<std::size_t>(k)];
        m.a[static_cast<std::size_t>(k)] = cplx(part(x.real(), y.real()), part(x.imag(), y.imag()));
    }
    return matOut(m, a.dim, n, mix(a.hbarPow, b.hbarPow), mix(a.hPow, b.hPow));
}

Quantity Eval::matMul(const Node& n, const Quantity& a, const Quantity& b) {
    const Dim d = dimAdd(a.dim, b.dim, 1);
    const double hb = a.hbarPow + b.hbarPow, hp = a.hPow + b.hPow;
    if (a.mat && b.mat) {
        if (a.mat->cols != b.mat->rows) {
            std::string msg = "These sizes do not fit: a " + sizeText(*a.mat) + " matrix times a " + sizeText(*b.mat) + " one. In A * B, A must have as many columns as B has rows";
            const bool vectors = (a.mat->rows == 1 || a.mat->cols == 1) && a.mat->rows == b.mat->rows && a.mat->cols == b.mat->cols;
            msg += vectors ? " (for two rows or two columns turn one over: v * transpose(w), or dagger(v) * w)." : ".";
            fail(msg, n.pos, n.len);
        }
        return matOut(matProduct(*a.mat, *b.mat), d, n, hb, hp);
    }
    const Quantity& s = a.mat ? b : a;                                          // a number (or a quantity) times a matrix
    Matrix m = a.mat ? *a.mat : *b.mat;
    m.list = false;
    for (cplx& z : m.a) z = s.im == 0 ? cplx(z.real() * s.v, z.imag() * s.v) : cleanParts(z * cz(s));
    return matOut(m, d, n, hb, hp);
}

Quantity Eval::matDiv(const Node& n, const Quantity& a, const Quantity& b) {
    if (b.mat) fail("You can't divide by a matrix: the calculator has no matrix inverse.", n.pos, n.len);
    if (b.v == 0 && b.im == 0) fail("You can't divide by zero.", n.pos, n.len);
    Matrix m = *a.mat;
    m.list = false;
    for (cplx& z : m.a) z = b.im == 0 ? cplx(z.real() / b.v, z.imag() / b.v) : cleanParts(z / cz(b));
    return matOut(m, dimAdd(a.dim, b.dim, -1), n, a.hbarPow - b.hbarPow, a.hPow - b.hPow);
}

Quantity Eval::matPow(const Node& n, const Quantity& x, const Quantity& p) {
    if (p.mat) fail("A power must be a plain number, not a matrix.", n.pos, n.len);
    if (!p.dim.none()) fail("A power must be a plain number, not " + describe(p.dim) + ".", n.pos, n.len);
    if (p.im != 0 || std::nearbyint(p.v) != p.v) fail("A matrix can only be raised to a whole number 0, 1, 2, ... (it is multiplied by itself that many times).", n.pos, n.len);
    if (p.v < 0) fail("A matrix to a negative power needs its inverse, which the calculator does not have.", n.pos, n.len);
    if (x.mat->rows != x.mat->cols) fail("Only a square matrix can be raised to a power (this one is " + sizeText(*x.mat) + ").", n.pos, n.len);
    if (p.v > 1e6) fail("This power is too large.", n.pos, n.len);
    Matrix acc = matIdentity(x.mat->rows), b = *x.mat;
    b.list = false;
    for (long long k = static_cast<long long>(p.v); k; k >>= 1) {
        if (k & 1) acc = matProduct(acc, b);
        if (k > 1) b = matProduct(b, b);
        for (const cplx& z : acc.a) if (!std::isfinite(z.real()) || !std::isfinite(z.imag())) fail("The answer is too big for the calculator (over about 10^308).", n.pos, n.len);
        for (cplx& z : b.a) if (!std::isfinite(z.real()) || !std::isfinite(z.imag())) z = cplx(HUGE_VAL, 0);   // (kept as "too big"; never NaN)
    }
    return matOut(acc, dimScale(x.dim, Rat{static_cast<long long>(p.v), 1}), n, x.hbarPow * p.v, x.hPow * p.v);
}

Quantity Eval::matCall(const Node& n) {
    const std::string& f = n.text;
    if (f == "identity") {
        const Quantity x = run(n.a);
        if (x.mat || !x.dim.none() || x.im != 0 || std::nearbyint(x.v) != x.v || x.v < 1 || x.v > kMatMax)
            fail("identity(n) is the n x n unit matrix: n must be a whole number from 1 to 4.", n.pos, n.len);
        return matOut(matIdentity(static_cast<int>(x.v)), Dim{}, n, 0, 0);
    }
    if (f == "kron") {
        const Quantity a = run(n.args[0]), b = run(n.args[1]);
        if (!a.mat || !b.mat) fail("kron works on two matrices, like kron(sigmax, sigmaz); for a number times a matrix write 2 * M.", n.pos, n.len);
        const Matrix& A = *a.mat; const Matrix& B = *b.mat;
        if (A.rows * B.rows > kMatMax || A.cols * B.cols > kMatMax)
            fail("kron's answer would be " + std::to_string(A.rows * B.rows) + "x" + std::to_string(A.cols * B.cols) + ": a matrix can have at most 4 rows and 4 columns.", n.pos, n.len);
        Matrix m; m.rows = A.rows * B.rows; m.cols = A.cols * B.cols;
        for (int i = 0; i < A.rows; i++) for (int j = 0; j < A.cols; j++) for (int k = 0; k < B.rows; k++) for (int l = 0; l < B.cols; l++)
            m.at(i * B.rows + k, j * B.cols + l) = cleanParts(A.at(i, j) * B.at(k, l));
        return matOut(m, dimAdd(a.dim, b.dim, 1), n, a.hbarPow + b.hbarPow, a.hPow + b.hPow);
    }
    const Quantity x = run(n.a);
    if (!x.mat) fail(f + " works on a matrix, like " + f + "([[1, 2], [3, 4]]).", n.pos, n.len);
    const Matrix& M = *x.mat;
    if (f == "transpose" || f == "dagger") {
        Matrix m; m.rows = M.cols; m.cols = M.rows;
        for (int i = 0; i < M.rows; i++) for (int j = 0; j < M.cols; j++) { const cplx z = M.at(i, j); m.at(j, i) = f == "dagger" ? cplx(z.real(), z.imag() == 0 ? 0 : -z.imag()) : z; }
        return matOut(m, x.dim, n, x.hbarPow, x.hPow);
    }
    if (M.rows != M.cols) fail(f + " needs a square matrix (as many rows as columns); this one is " + sizeText(M) + ".", n.pos, n.len);
    const int N = M.rows;
    if (f == "tr") {
        cplx s = 0;
        for (int i = 0; i < N; i++) s += M.at(i, i);
        Quantity q{checked(s.real(), n), x.dim, x.hbarPow, x.hPow, checked(s.imag(), n)};
        return q;
    }
    if (f == "det") {
        const cplx dt = detOf(M.a.data(), N);
        if (dt == 0.0) { Quantity q; q.dim = dimScale(x.dim, Rat{N, 1}); q.hbarPow = x.hbarPow * N; q.hPow = x.hPow * N; return q; }
        return complexOut(dt, dimScale(x.dim, Rat{N, 1}), n, x.hbarPow * N, x.hPow * N);
    }
    // eig: Hermitian only. How far from Hermitian: the largest |M_ij − conj(M_ji)|; up to 1e-12 of the largest entry is taken as rounding (A * dagger(A) is
    // Hermitian only to the last digits) and the matrix is made exactly Hermitian, (M + M†)/2, before it is worked out.
    double big = 0, worst = 0;
    int wi = 0, wj = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
        big = std::max(big, std::abs(M.at(i, j)));
        const double off = std::abs(M.at(i, j) - std::conj(M.at(j, i)));
        if (j >= i && off > worst) { worst = off; wi = i; wj = j; }
    }
    if (worst > 1e-12 * big) {
        const std::string u = unitText(x.dim), amount = formatNumber(wi == wj ? worst / 2 : worst, 4) + (u.empty() ? "" : " " + u);
        const std::string where = wi == wj ? "the entry in row " + std::to_string(wi + 1) + ", column " + std::to_string(wj + 1) + " on the diagonal is not real (its imaginary part is " + amount + ")"
                                           : "the entry in row " + std::to_string(wi + 1) + ", column " + std::to_string(wj + 1) + " differs from the conjugate of the entry in row " +
                                             std::to_string(wj + 1) + ", column " + std::to_string(wi + 1) + " by " + amount;
        fail("eig gives the eigenvalues of Hermitian matrices only (a matrix equal to its conjugate transpose, dagger(M); a real symmetric matrix is one). "
             "This one is not Hermitian: " + where + ".", n.pos, n.len);
    }
    Matrix h = M;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) h.at(i, j) = i == j ? cplx(M.at(i, i).real(), 0) : (M.at(i, j) + std::conj(M.at(j, i))) / 2.0;
    const std::vector<double> w = hermitianEigenvalues(h);
    Matrix out; out.rows = N; out.cols = 1; out.list = true;
    // an eigenvalue below 16 rounding steps of the largest entry is rounding (the 0 of [[1, 1], [1, 1]] comes out as 0, not 1e-17)
    for (int i = 0; i < N; i++) out.a[static_cast<std::size_t>(i)] = std::fabs(w[static_cast<std::size_t>(i)]) <= 16 * DBL_EPSILON * big ? 0.0 : w[static_cast<std::size_t>(i)];
    return matOut(out, x.dim, n, x.hbarPow, x.hPow);
}

// ---------------------------------------------------------------- how the line was read
int prec(NK k) {
    switch (k) {
        case NK::Add: case NK::Sub: return 1;
        case NK::Mul: case NK::Div: return 2;
        case NK::Neg: return 3;
        case NK::Juxt: return 4;
        case NK::Pow: return 5;
        default: return 6;
    }
}
std::string show(const std::vector<Node>& nd, int id) {
    const Node& n = nd[id];
    const auto wrap = [&](int c, bool paren) { const std::string s = show(nd, c); return paren ? "(" + s + ")" : s; };
    switch (n.k) {
        case NK::Num: return n.text;
        case NK::Sym: return n.text;
        case NK::Mat: {                                                  // 0.54.2: always with the rows' own brackets
            const std::size_t cols = n.args.size() / static_cast<std::size_t>(n.num);
            std::string o = "[";
            for (std::size_t k = 0; k < n.args.size(); k++) o += (k % cols == 0 ? (k ? "], [" : "[") : ", ") + show(nd, n.args[k]);
            return o + "]]";
        }
        case NK::Call: {
            if (n.bang) return wrap(n.a, prec(nd[n.a].k) < 6) + "!";
            std::string o = n.text + "(";
            for (std::size_t k = 0; k < n.args.size(); k++) o += (k ? ", " : "") + show(nd, n.args[k]);
            return o + ")";
        }
        case NK::Neg: return "−" + wrap(n.a, prec(nd[n.a].k) < 3);
        case NK::Add: case NK::Sub:
            return wrap(n.a, false) + (n.k == NK::Add ? " + " : " − ") + wrap(n.b, prec(nd[n.b].k) <= 1);
        case NK::Mul: case NK::Div: {
            const NK la = nd[n.a].k, lb = nd[n.b].k;
            return wrap(n.a, prec(la) < 2 || la == NK::Juxt) + (n.k == NK::Mul ? " × " : " / ") + wrap(n.b, prec(lb) <= 2 || lb == NK::Juxt || lb == NK::Neg);
        }
        case NK::Juxt: return wrap(n.a, prec(nd[n.a].k) < 4) + " " + wrap(n.b, prec(nd[n.b].k) <= 4);
        case NK::Pow: return wrap(n.a, prec(nd[n.a].k) <= 5) + "^" + wrap(n.b, nd[n.b].k != NK::Num && nd[n.b].k != NK::Sym);
    }
    return "";
}
bool unitsOnly(const std::vector<Node>& nd, int id, bool exponent = false) {   // after "to": no plain numbers except as a power
    const Node& n = nd[id];
    switch (n.k) {
        case NK::Num: return exponent;
        case NK::Sym: return true;
        case NK::Pow: return unitsOnly(nd, n.a) && unitsOnly(nd, n.b, true);
        case NK::Neg: return exponent && unitsOnly(nd, n.a, true);
        case NK::Div: return !exponent && ((nd[n.a].k == NK::Num && nd[n.a].num == 1) || unitsOnly(nd, n.a)) && unitsOnly(nd, n.b);   // "to 1/cm"
        case NK::Mul: case NK::Juxt: return !exponent && unitsOnly(nd, n.a) && unitsOnly(nd, n.b);
        default: return false;
    }
}

std::string prettyPublished(const std::string& p) {           // "6.674 30(15) × 10^-11" → "6.674 30(15) × 10⁻¹¹"
    const std::size_t k = p.find("10^");
    if (k == std::string::npos) return p;
    return p.substr(0, k) + "10" + supInt(std::atoll(p.c_str() + k + 3));
}

std::string usedLine(const std::string& name, const Quantity* ans, const Vars* vars) {
    if (name == "i") return "i = the imaginary unit, i² = -1";
    if (const char* axis = pauliAxis(name)) return name + " = the Pauli matrix σ" + axis + " = " + pauliWritten(axis[0]);   // 0.54.2
    if (vars && vars->count(name) && !findSymbol(name)) return name + " = your variable = " + valueText(vars->at(name));
    if (name == "ans") return "ans = the last answer = " + valueText(*ans);
    const Symbol& s = *findSymbol(name);
    const std::string u = unitText(s.q.dim);
    std::string line = name + " = " + s.meaning;
    const bool baseItself = s.q.v == 1 && (u == name || u.empty());
    if (!baseItself) {
        const std::string val = !s.published.empty() ? prettyPublished(s.published) : formatNumber(s.q.v);
        line += " = " + val + (u.empty() ? "" : " " + u);
    }
    if (!s.source.empty()) line += " (" + s.source + ")";
    return line;
}

struct Alt { Dim d; std::vector<std::string> units; };
const std::vector<Alt>& alternatives() {
    static const std::vector<Alt> a = {
        {dimOf(0, 1, 0), {"m", "km", "au", "ly"}}, {dimOf(0, 0, 1), {"s", "min", "hr", "day", "yr"}}, {dimOf(1, 0, 0), {"kg", "g", "tonne"}},
        {dimOf(1, 2, -2), {"J", "eV", "keV", "MeV"}}, {dimOf(0, 1, -1), {"m/s", "km/s", "km/hr"}}, {dimOf(1, -1, -2), {"Pa", "kPa", "bar", "atm"}}};
    return a;
}

}  // namespace

// ---------------------------------------------------------------- public
Dim dimOf(int kg, int m, int s, int A, int K, int mol) {
    Dim d;
    const int e[kBaseCount] = {kg, m, s, A, K, mol};
    for (int i = 0; i < kBaseCount; i++) d.e[i] = Rat{e[i], 1};
    return d;
}

const std::vector<Symbol>& symbols() { return table().list; }
const Symbol* findSymbol(const std::string& name) {
    const Table& t = table();
    const auto it = t.index.find(name);
    return it == t.index.end() ? nullptr : &t.list[it->second];
}
const std::vector<std::string>& functionNames() { return kFunctions; }

std::string formatNumber(double v, int digits) {
    if (v == 0) return "0";
    char b[64];
    std::snprintf(b, sizeof b, "%.*g", digits, v);
    // 0.50.3 (outside review): a whole number below 10^15 is exact in a double; when 10 digits would cut it (3^21 = 10460353203), it is shown in full
    if (digits >= 10 && std::fabs(v) < 1e15 && std::nearbyint(v) == v && std::strtod(b, nullptr) != v) std::snprintf(b, sizeof b, "%.0f", v);
    const std::string s = b;
    const std::size_t e = s.find('e');
    if (e == std::string::npos) return s;
    return s.substr(0, e) + "×10" + supInt(std::atoll(s.c_str() + e + 1));
}
// 0.50.2: a complex number as shown: "3 + 4i", "0.5 − 0.5i", "-i", "2i"; brackets round it when a unit follows: "(3 + 2i)" m
std::string complexText(double re, double im, bool unitFollows, int digits) {
    if (im == 0) return formatNumber(re, digits);
    const std::string is = (std::fabs(im) == 1 ? std::string() : formatNumber(std::fabs(im), digits)) + "i";
    const std::string t = re == 0 ? (im < 0 ? "-" : "") + is : formatNumber(re, digits) + (im < 0 ? " − " : " + ") + is;
    return unitFollows && re != 0 ? "(" + t + ")" : t;
}
// 0.54.2: a matrix on one line, "[[0, 1], [1, 0]]"; eig's list as "-1, 1"
std::string matrixText(const std::vector<std::complex<double>>& cells, int rows, int cols, bool list, int digits) {
    std::string o;
    for (int r = 0; r < rows; r++) {
        if (!list) o += r ? ", [" : "[[";
        for (int c = 0; c < cols; c++) {
            const std::complex<double> z = cells[static_cast<std::size_t>(r * cols + c)];
            o += ((list ? r > 0 : c > 0) ? ", " : "") + complexText(z.real(), z.imag(), false, digits);
        }
        if (!list) o += "]";
    }
    return list ? o : o + "]";
}
std::string valueText(const Quantity& q, int matrixDigits) {
    const std::string u = unitText(q.dim);
    if (!q.mat) return complexText(q.v, q.im, !u.empty()) + (u.empty() ? "" : " " + u);
    const std::vector<std::complex<double>> cells(q.mat->a.begin(), q.mat->a.begin() + q.mat->rows * q.mat->cols);
    return matrixText(cells, q.mat->rows, q.mat->cols, q.mat->list, matrixDigits) + (u.empty() ? "" : " " + u);
}
bool isPauliName(const std::string& name) {
    for (const char* a : {"x", "y", "z"}) if (name == std::string("sigma") + a || name == std::string("σ") + a) return true;
    return false;
}
std::string exactNumber(double v) {
    if (v == 0) return "0";
    char b[64];
    std::snprintf(b, sizeof b, "%.17g", v);
    return b;
}

namespace {
double score(const Dim& d) { double s = 0; for (const Rat& r : d.e) s += std::fabs(ratValue(r)); return s; }
std::string power(const std::string& base, Rat r) {      // r > 0
    if (r.d == 1) return r.n == 1 ? base : base + supInt(r.n);
    return base + "^(" + std::to_string(r.n) + "/" + std::to_string(r.d) + ")";
}
std::string compose(const Dim& d) {
    std::vector<std::string> up, down;
    for (int i = 0; i < kBaseCount; i++) {
        const Rat r = d.e[i];
        if (r.n > 0) up.push_back(power(kBaseSymbol[i], r));
        else if (r.n < 0) down.push_back(power(kBaseSymbol[i], Rat{-r.n, r.d}));
    }
    const auto join = [](const std::vector<std::string>& v) { std::string o; for (std::size_t k = 0; k < v.size(); k++) o += (k ? "·" : "") + v[k]; return o; };
    std::string o = up.empty() ? "1" : join(up);
    if (!down.empty()) o += "/" + (down.size() > 1 ? "(" + join(down) + ")" : down[0]);
    return o;
}
struct Named { const char* sym; Dim d; };
const std::vector<Named>& named() {
    static const std::vector<Named> n = {{"J", dimOf(1, 2, -2)}, {"W", dimOf(1, 2, -3)}, {"N", dimOf(1, 1, -2)}, {"Pa", dimOf(1, -1, -2)}, {"C", dimOf(0, 0, 1, 1)},
                                         {"V", dimOf(1, 2, -3, -1)}, {"Ω", dimOf(1, 2, -3, -2)}, {"T", dimOf(1, 0, -2, -1)}};
    return n;
}
}  // namespace

std::string unitText(const Dim& d) {
    if (d.none()) return "";
    if (d == dimOf(0, 2, 0, 1)) return "J/T";            // 0.50.1: a magnetic moment (A·m²), as μB is always written
    for (const Named& n : named()) if (n.d == d) return n.sym;
    // a named unit times a short rest (J/K, J·s, W/m²), only when that is clearly simpler than the base units
    const double s0 = score(d);
    const Named* best = nullptr;
    double bestScore = 0;
    Dim bestRest;
    for (const Named& n : named()) {
        const Dim rest = dimAdd(d, n.d, -1);
        const double s = score(rest);
        bool per = true, single = false;
        int nonzero = 0;
        for (const Rat& x : rest.e) { if (x.n > 0) per = false; if (x.n != 0) nonzero++; }
        for (const Rat& x : rest.e) if (x.n == 1 && x.d == 1 && nonzero == 1) single = true;
        if (s0 >= 4 && (per || single) && s <= 2 && s + 1 < s0 && (!best || s < bestScore)) { best = &n; bestScore = s; bestRest = rest; }   // (s0 >= 4: m/s² stays m/s², not N/kg)
    }
    if (!best) return compose(d);
    const std::string rest = compose(bestRest);       // "K⁻¹" is written "1/K": put the named unit in front of it
    if (rest.rfind("1/", 0) == 0) return std::string(best->sym) + rest.substr(1);
    return std::string(best->sym) + "·" + rest;
}
std::string kindWord(const Dim& d) {
    for (const KindName& k : kinds()) if (k.d == d) return k.word;
    return "";
}

namespace {
Result evaluateImpl(const std::string& line, const Quantity* ans, bool alternativesToo, const Vars* vars = nullptr);
}
Result evaluate(const std::string& line, const Quantity* ans, const Vars* vars) { return evaluateImpl(line, ans, true, vars); }

namespace {
Result evaluateImpl(const std::string& line, const Quantity* ans, bool alternativesToo, const Vars* vars) {
    Result r;
    bool blank = true;
    for (std::size_t i = 0; i < line.size();) { int n = 1; if (!spaceChar(decode(line, i, n))) { blank = false; break; } i += n; }
    if (blank) return r;
    r.empty = false;
    try {
        {   // 0.50.3 (outside review): characters, not bytes (μ is 2 bytes)
            std::size_t chars = 0;
            for (unsigned char c : line) chars += (c & 0xC0) != 0x80;
            if (chars > kMaxLength) fail("The line is too long (at most 1000 characters).");
        }
        const std::vector<Tok> tk = tokenize(line);
        Parser p(tk);
        p.userVars = vars;
        if (tk[0].t == TT::End) { r.empty = true; return r; }       // only "=" or spaces
        if (tk[1].t == TT::Assign) {                                 // 0.50.2: "x = 2 m"
            const std::string& name = tk[0].text;
            if (const Symbol* sy = findSymbol(name)) fail(name + " is already the " + sy->meaning + ": name your variable something else, like x or r1.", tk[0].pos, tk[0].len);
            if (isPauliName(name)) fail(name + " is already a Pauli matrix: name your variable something else, like H or M1.", tk[0].pos, tk[0].len);
            if (isFunction(name) || name == "ans" || name == "i" || name == "to" || name == "h" || name == "log" || name == "in" || name == "pi")
                fail(name + " is already a name the calculator uses: pick another, like x or r1.", tk[0].pos, tk[0].len);
            if (tk[2].t == TT::End) fail("Write a value after =, like x = 2 m.", tk[1].pos, tk[1].len);
            r.assigned = name;
            p.i = 2;
        }
        const int lhs = p.expr();
        int rhs = -1;
        std::string target;
        if (p.cur().t == TT::To) {
            const Tok to = p.cur(); p.i++;
            if (p.cur().t == TT::End) fail("Write a unit after \"to\", like: 5 km to mi.", to.pos, to.len);
            const std::size_t start = p.i;
            rhs = p.expr();
            const Tok& last = tk[p.i - 1];
            target = line.substr(tk[start].pos, last.pos + last.len - tk[start].pos);
            if (!unitsOnly(p.nodes, rhs)) fail("After \"to\" write only units or constants, like: to km/hr (no plain numbers).", tk[start].pos, static_cast<int>(target.size()));
        }
        if (p.cur().t != TT::End) {
            const Tok& t = p.cur();
            if (t.t == TT::RP) fail("This ) has no ( before it.", t.pos, t.len);
            if (t.t == TT::RB) fail("This ] has no [ before it.", t.pos, t.len);
            if (t.t == TT::To) fail("Only one \"to\" is allowed.", t.pos, t.len);
            fail("This does not fit here.", t.pos, t.len);
        }
        Eval ev(p.nodes, ans);
        ev.userVars = vars;
        {   // 0.50.2: does the line use i (the imaginary unit, not a sum's or solve's own index called i) or a complex ans?
            std::function<bool(int, int)> walk = [&](int k, int boundI) -> bool {
                const Node& n = p.nodes[k];
                if (n.k == NK::Sym) return (n.text == "i" && !boundI) || (n.text == "ans" && ans && (ans->im != 0 || ans->mat));
                int b = boundI;
                if (n.k == NK::Call && (n.text == "sum" || n.text == "solve") && n.args.size() > 1 && p.nodes[n.args[1]].text == "i") b++;
                if (n.k == NK::Call || n.k == NK::Mat) { for (int c : n.args) if (walk(c, b)) return true; return false; }
                return (n.a >= 0 && walk(n.a, b)) || (n.b >= 0 && walk(n.b, b));
            };
            ev.complexOk = walk(lhs, 0);
        }
        const Quantity q = ev.run(lhs);
        r.q = q;
        r.readAs = show(p.nodes, lhs);
        const Dim perSecond = dimOf(0, 0, -1);
        // 0.50.2: a frequency is angular when the line's net power of ħ is not 0 and it does not divide by h (net power of h below 0): E/ħ, ħk²/2m and h f/ħ
        // are ω; E/h, μp B/h (gp e ħ/(2 mp) B / (2h)) and ħ 2π f / h are cyclic. Only the ħ, hbar and hP typed in the line count (μB, a0, Eh ... carry none).
        const auto angular = [&](const Quantity& x) {
            return x.dim == perSecond && std::isfinite(x.hbarPow) && std::isfinite(x.hPow) && x.hbarPow != 0 && x.hPow >= 0;
        };
        const auto mentions = [&](int id, const std::function<bool(const std::string&)>& is) {
            std::function<bool(int)> walk = [&](int k) -> bool {
                const Node& n = p.nodes[k];
                if (n.k == NK::Sym && is(n.text)) return true;
                if (n.a >= 0 && walk(n.a)) return true;
                if (n.b >= 0 && walk(n.b)) return true;
                for (int c : n.args) if (walk(c)) return true;
                return false;
            };
            return walk(id);
        };
        const auto isRad = [](const std::string& t) { return t == "rad"; };
        const auto isHz = [](const std::string& t) { const Symbol* y = findSymbol(t); return y && y->kind == SymKind::Unit && y->meaning.find("hertz") != std::string::npos; };
        const bool ang = angular(q);
        bool targetRad = false, targetHz = false;
        std::vector<std::string> convNotes;
        if (rhs >= 0) {
            const Quantity u = ev.run(rhs);
            if (u.mat) fail("The unit after \"to\" can't be a matrix.", tk[p.i - 1].pos, 0);
            if (u.v <= 0 || u.im != 0) fail("The unit after \"to\" must be positive" + std::string(u.im != 0 ? " (not a complex number)." : "."), tk[p.i - 1].pos, 0);   // 0.50.3: a complex variable was read by its real part
            targetRad = mentions(rhs, isRad);
            targetHz = mentions(rhs, isHz);
            const bool sourceRad = mentions(lhs, isRad), sourceHz = mentions(lhs, isHz);
            double v = 0, vIm = 0, factor = 1;
            if (u.dim == q.dim) {
                // 0.50.2: a cyclic frequency typed in Hz shown in rad/s is ω = 2π f, and one typed in rad/s shown in Hz is f = ω/2π (rad counts as 1 in SI)
                if (q.dim == perSecond && targetRad && !sourceRad && sourceHz && !ang) {
                    factor = 2 * kPi;
                    convNotes.push_back("Converted with ω = 2π f: a frequency in Hz is cyclic, one in rad/s angular.");
                } else if (q.dim == perSecond && targetHz && !targetRad && sourceRad) {
                    factor = 1 / (2 * kPi);
                    convNotes.push_back("Converted with f = ω/2π: a frequency in rad/s is angular, one in Hz cyclic.");
                }
                v = q.v * factor / u.v;
                vIm = q.im * factor / u.v;
            } else {
                if (q.mat)                                                    // 0.54.2
                    fail("A matrix can only be shown in a unit of the same kind (this one is " + describe(q.dim) + ", and " + target + " is " + describe(u.dim) +
                         "): the conversions through E = h f, E = kB T ... work on single numbers.", -1, 0);
                if (q.im != 0) fail("Converting " + describe(q.dim) + " to " + describe(u.dim) + " works on real values only, not on a complex number.", -1, 0);
                // the spectroscopy conversions: energy, wavenumber, frequency, temperature and (0.50.2) a photon's wavelength are each turned into an energy by one
                // exact relation (E = h c ν̃, E = h f or ħω, E = kB T, E = h c / λ; h, c and kB exact since 2019), and a mass by E = m c². Only after "to".
                struct Kind { Dim d; double k; std::string rule; bool inverse; };
                const double h = atom::kPlanckJs, c = atom::kC, hbarJs = h / (2 * kPi);
                const auto kindOf = [&](const Dim& d, bool rad) -> std::optional<Kind> {
                    if (d == dimOf(1, 2, -2)) return Kind{d, 1, "", false};
                    if (d == dimOf(0, -1, 0)) return Kind{d, h * c, "E = h c ν̃ (the wavenumber times hc; h and c are exact)", false};
                    if (d == perSecond)
                        return rad ? Kind{d, hbarJs, "E = ħω, ω the angular frequency in rad/s (ħ = h/2π, exact)", false}
                                   : Kind{d, h, "E = h f, f the cyclic frequency in Hz (h exact; an angular frequency ω in rad/s needs E = ħω)", false};
                    if (d == dimOf(0, 0, 0, 0, 1)) return Kind{d, atom::kBoltzmann, "E = kB T (kB exact)", false};
                    if (d == dimOf(1, 0, 0)) return Kind{d, c * c, "E = m c² (c exact)", false};
                    if (d == dimOf(0, 1, 0)) return Kind{d, h * c, "λ = h c / E: the vacuum wavelength of a photon of this energy (h and c exact)", true};
                    return std::nullopt;
                };
                const std::optional<Kind> from = kindOf(q.dim, sourceRad), to = kindOf(u.dim, targetRad);
                const Dim mass = dimOf(1, 0, 0), energyDim = dimOf(1, 2, -2), length = dimOf(0, 1, 0), temp = dimOf(0, 0, 0, 0, 1);
                bool allowed = from && to;
                if (allowed && (from->d == mass || to->d == mass)) allowed = from->d == energyDim || to->d == energyDim;
                if (allowed && (from->d == length || to->d == length)) allowed = from->d != temp && to->d != temp;
                if (!allowed) fail("You can't show " + describe(q.dim) + " in " + target + ", which is " + describe(u.dim) + ".", -1, 0);
                if (ang && !sourceRad)
                    fail("This frequency comes from ħ, so it is an angular frequency ω (rad/s): its energy is ħω, not h f. Multiply by ħ yourself: (...) ħ to " + target + ".", -1, 0);
                if (from->inverse && q.v == 0) fail("A wavelength of 0 has no photon energy.", -1, 0);
                const double E = from->inverse ? from->k / q.v : q.v * from->k;     // J
                if (to->inverse && E == 0) fail("An energy of 0 has no photon wavelength.", -1, 0);
                v = (to->inverse ? to->k / E : E / to->k) / u.v;
                for (const std::optional<Kind>& k : {from, to})
                    if (!k->rule.empty()) convNotes.push_back("Converted with " + k->rule + ".");
                if (convNotes.size() == 2 && convNotes[0] == convNotes[1]) convNotes.pop_back();
            }
            if (!std::isfinite(v)) fail("The answer is too big for the calculator in this unit.");
            if ((v == 0 && q.v != 0) || tiny(v)) { ev.underflow(); v = 0; }
            if (!std::isfinite(vIm)) fail("The answer is too big for the calculator in this unit.");
            if ((vIm == 0 && q.im != 0) || tiny(vIm)) { ev.underflow(); vIm = 0; }
            r.number = complexText(v, vIm, true);
            r.inUnit = v;
            r.unit = target;
            if (q.mat) {                                                      // 0.54.2: every entry in the unit asked for
                for (int k = 0; k < q.mat->rows * q.mat->cols; k++) {
                    const cplx z = q.mat->a[static_cast<std::size_t>(k)];
                    double re = z.real() * factor / u.v, im = z.imag() * factor / u.v;
                    if (!std::isfinite(re) || !std::isfinite(im)) fail("The answer is too big for the calculator in this unit.");
                    if ((re == 0 && z.real() != 0) || tiny(re)) { ev.underflow(); re = 0; }
                    if ((im == 0 && z.imag() != 0) || tiny(im)) { ev.underflow(); im = 0; }
                    r.cells.emplace_back(re, im);
                }
            }
            r.readAs += " → " + show(p.nodes, rhs);
        } else {
            r.unit = unitText(q.dim);
            r.number = complexText(q.v, q.im, !r.unit.empty());
            r.inUnit = q.v;
            if (q.mat) r.cells.assign(q.mat->a.begin(), q.mat->a.begin() + q.mat->rows * q.mat->cols);
        }
        if (q.mat) {                                                          // 0.54.2: a matrix answer: its grid, and the same on one line
            r.rows = q.mat->rows; r.cols = q.mat->cols; r.list = q.mat->list;
            r.number = matrixText(r.cells, r.rows, r.cols, r.list);
            r.inUnit = 0;
        }
        // ħ → ω (0.50.1, rule 0.50.2): an angular frequency in rad/s is shown in Hz or 1/s with no word (SI counts the radian as 1). After "to" a hertz unit
        // the note says so and gives f = ω/2π. With no "to" it may as well be a rate (a decay rate has no 2π): the note says that first, and the answer stays
        // the undivided value. No note after "to 1/s" (how a rate is asked for) or "to rad/s" (it says so itself).
        if (ang && q.mat && !targetRad && (rhs < 0 || targetHz)) {             // 0.54.2: the same rule for a whole matrix (or eig's list), without a single f
            if (rhs >= 0)
                convNotes.push_back("ħ is in this line (and no h divides it), so these are angular frequencies ω in rad/s (SI counts the radian as 1, so they are shown in " +
                                    r.unit + " with no word). The cyclic frequencies are f = ω/2π: divide by 2 π. (A rate, like a decay rate, has no 2π: write \"to 1/s\" for it.)");
            else
                convNotes.push_back("If these are rates (decay or transition rates), they have no 2π. If they are angular frequencies ω in rad/s (ħ is in the line and no h "
                                    "divides it), the cyclic frequencies are f = ω/2π: divide by 2 π.");
        } else if (ang && q.im == 0 && !targetRad && (rhs < 0 || targetHz)) {
            const std::string u = rhs >= 0 ? r.unit : "Hz";
            const std::string f = formatNumber(r.inUnit / (2 * kPi)) + " " + u;
            if (rhs >= 0) {
                convNotes.push_back("ħ is in this line (and no h divides it), so this is an angular frequency ω in rad/s (SI counts the radian as 1, so it is shown in " + u +
                                    " with no word). The cyclic frequency f = ω/2π = " + f + ". (A rate, like a decay rate, has no 2π: write \"to 1/s\" for it.)");
                r.also.push_back("f = ω/2π = " + f);
            } else {
                convNotes.push_back("If this is a rate (a decay or transition rate), it has no 2π: the answer is the rate in 1/s. If it is an angular frequency ω in rad/s "
                                    "(ħ is in the line and no h divides it), the cyclic frequency f = ω/2π = " + f + ".");
                r.also.push_back("if it is ω: f = ω/2π = " + f);
            }
        }
        r.kindWord = q.dim.none() ? "" : kindWord(q.dim);
        for (const std::string& n : ev.used) r.used.push_back(usedLine(n, ans, vars));
        r.notes = p.notes;
        for (const std::string& n : ev.notes) r.notes.push_back(n);
        for (const std::string& n : convNotes) r.notes.push_back(n);
        for (const Alt& a : alternatives()) {
            if (!alternativesToo || a.d != q.dim || !convNotes.empty() || q.im != 0 || q.mat) continue;   // (not after a spectroscopy conversion: "1 eV to K" would add "= 1 eV")
            for (const std::string& u : a.units) {
                if (u == r.unit) continue;
                const Result one = evaluateImpl("1 " + u, nullptr, false);
                if (!one.ok || one.q.dim != q.dim) continue;
                const double v = q.v / one.q.v;
                if (std::fabs(v) >= 0.01 && std::fabs(v) < 1e6 && r.also.size() < 3) r.also.push_back("= " + formatNumber(v) + " " + u);
            }
        }
        r.ok = true;
    } catch (const Err& e) {
        r.ok = false;
        r.error = e.msg;
        r.errPos = e.pos;
        r.errLen = e.len;
        r.inUnit = 0;
        r.number.clear(); r.unit.clear(); r.readAs.clear(); r.used.clear(); r.also.clear(); r.notes.clear(); r.kindWord.clear(); r.assigned.clear();
        r.rows = r.cols = 0; r.list = false; r.cells.clear();
    }
    return r;
}
}  // namespace

}  // namespace calc
