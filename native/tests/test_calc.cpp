// The calculator (0.49, src/app/calc.*, calc_lab.*): its rules, its units and constants against their published values, physics worked out with it
// against independent values, every message it refuses with, and a fuzz test (random lines never crash, never give a non-finite answer).
// Independent references: CODATA 2022 (Mohr et al., arXiv:2409.03787, saved as data/calc/), the SI and IAU definitions, NASA fact sheets, textbook formulas.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "app/calc.h"
#include "app/calc_lab.h"

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }
static bool rel(double a, double b, double tol) { return std::fabs(a - b) <= tol * std::fabs(b); }

static calc::Result ev(const std::string& s, const calc::Quantity* ans = nullptr) { return calc::evaluate(s, ans); }

// value in SI, its unit text
static void ok(const std::string& in, double want, const std::string& unit, double tol = 1e-12) {
    const calc::Result r = ev(in);
    const bool good = r.ok && !r.empty && rel(r.q.v, want, tol) && r.unit == unit;
    check("\"" + in + "\" = " + calc::formatNumber(want) + " " + unit + (r.ok ? " (got " + calc::formatNumber(r.q.v, 17) + " " + r.unit + ")" : " (error: " + r.error + ")"), good);
}
// the shown number after "to" (value in the target unit)
static void shown(const std::string& in, double want, double tol) {
    const calc::Result r = ev(in);
    double v = NAN;
    if (r.ok) {
        const calc::Result u = r.unit.empty() ? ev("1") : ev("1 (" + r.unit + ")");   // (a plain number has no unit)
        v = u.ok ? r.q.v / u.q.v : NAN;
        check("\"" + in + "\": the shown number is the answer written with 10 digits", u.ok && r.number == calc::formatNumber(v));
    }
    if (false) {   // (the shown number back to a double: not used)
        std::string s = r.number;
        const std::size_t x = s.find("×10");
        if (x != std::string::npos) {
            std::string e = s.substr(x + std::strlen("×10"));
            std::string digits;
            for (std::size_t i = 0; i < e.size();) {
                static const char* const sup[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
                if (e.compare(i, std::strlen("⁻"), "⁻") == 0) { digits += '-'; i += std::strlen("⁻"); continue; }
                bool found = false;
                for (int d = 0; d < 10; d++) if (e.compare(i, std::strlen(sup[d]), sup[d]) == 0) { digits += char('0' + d); i += std::strlen(sup[d]); found = true; break; }
                if (!found) break;
            }
            s = s.substr(0, x) + "e" + digits;
        }
        v = std::strtod(s.c_str(), nullptr);
    }
    check("\"" + in + "\" shows " + calc::formatNumber(want) + (r.ok ? " (shows " + r.number + " " + r.unit + ")" : " (error: " + r.error + ")"), r.ok && rel(v, want, tol));
}
static void bad(const std::string& in, const std::string& part) {
    const calc::Result r = ev(in);
    check("\"" + in + "\" is refused with \"" + part + "\"" + (r.ok ? " (but it gave " + r.number + " " + r.unit + ")" : " (said: " + r.error + ")"),
          !r.ok && !r.empty && r.error.find(part) != std::string::npos);
}

// a published value "6.674 30(15) × 10^-11" as a double: the spaces and the uncertainty in brackets dropped
static double published(const std::string& p) {
    std::string s, q;
    bool skip = false;
    for (char c : p) if (c != ' ') q += c;   // the spaces first, so "× 10^" is found
    for (std::size_t i = 0; i < q.size(); i++) {
        const char c = q[i];
        if (c == '(') { skip = true; continue; }
        if (c == ')') { skip = false; continue; }
        if (skip) continue;
        if (q.compare(i, std::strlen("×10^"), "×10^") == 0) { s += 'e'; i += std::strlen("×10^") - 1; continue; }
        s += c;
    }
    return std::strtod(s.c_str(), nullptr);
}

#include "test_calc_qm.inc"
#include "test_calc_matrix.inc"   // 0.54.2: small matrices

int main() {
    using namespace calc;

    // ---------------------------------------------------------------- the table
    {
        std::set<std::string> names;
        bool unique = true, finite = true;
        for (const Symbol& s : symbols()) { unique = unique && names.insert(s.name).second; finite = finite && std::isfinite(s.q.v) && (s.q.v > 0 || (s.q.v < 0 && s.kind == SymKind::Constant && s.meaning.find("negative") != std::string::npos)); }
        check("table: every name is defined once (no prefix makes a name that already exists)", unique && names.size() > 300);
        check("table: every value is finite and positive (0.50.1: but the constants CODATA gives as negative, ge and μn, which say so)", finite);
        int compared = 0, wrong = 0;
        for (const Symbol& s : symbols()) {
            if (s.published.empty()) continue;
            compared++;
            if (published(s.published) != s.q.v) { wrong++; std::printf("  %s: %s vs %.17g\n", s.name.c_str(), s.published.c_str(), s.q.v); }
        }
        // 0.50.3 (outside review): this checks the number against the source's printed value stored beside it in the same table (a typing check: both were copied from CODATA 2022);
        // the independent checks of the values are the physics tests below (relations such as e/h, Rydberg, a0)
        check("table: every constant's number equals its stored published string, digit for digit (" + std::to_string(compared) + " constants)", wrong == 0 && compared >= 20);
        for (const Symbol& s : symbols())
            if (s.kind == SymKind::Constant && s.name != "π" && s.name != "pi") check("table: " + s.name + " names its source", !s.source.empty());
        // the prefixed units: base value x the prefix (SI Brochure table 7)
        struct P { const char* name; double v; } pre[] = {{"km", 1e3}, {"cm", 1e-2}, {"mm", 1e-3}, {"μm", 1e-6}, {"um", 1e-6}, {"nm", 1e-9}, {"pm", 1e-12}, {"fm", 1e-15},
                                                         {"kg", 1}, {"mg", 1e-6}, {"Mg", 1e3}, {"ms", 1e-3}, {"ns", 1e-9}, {"as", 1e-18}, {"GHz", 1e9}, {"kPa", 1e3},
                                                         {"hPa", 1e2}, {"MJ", 1e6}, {"mL", 1e-6}, {"dL", 1e-4}, {"mbar", 1e2}, {"kΩ", 1e3}, {"mT", 1e-3}, {"Gyr", 1e9 * 365.25 * 86400}};
        for (const P& p : pre) { const Symbol* s = findSymbol(p.name); check(std::string("prefix: ") + p.name, s && rel(s->q.v, p.v, 1e-15)); }
        check("prefix: 1 kg is exactly 1", findSymbol("kg") && findSymbol("kg")->q.v == 1.0);
        check("prefix: MeV = 10^6 x 1.602176634e-19 J", findSymbol("MeV") && rel(findSymbol("MeV")->q.v, 1.602176634e-13, 1e-15));
        check("prefix: kT (kilotesla) is not defined, so kB T can't be mistyped as it", findSymbol("kT") == nullptr);
        check("no name for h (hours or Planck?), hours are hr, Planck's constant hP", findSymbol("h") == nullptr && findSymbol("hr") && findSymbol("hP"));
    }
    // ---------------------------------------------------------------- the constants against independent relations (CODATA 2022)
    {
        const double c = 299792458, h = 6.62607015e-34, e = 1.602176634e-19, k = 1.380649e-23, NA = 6.02214076e23, pi = 3.14159265358979323846;
        const double alpha = 7.2973525643e-3, mu0 = findSymbol("μ0")->q.v, eps0 = findSymbol("ε0")->q.v;
        check("μ0 = 2αh / (e²c) within its 1.6e-10 (CODATA 2022 table: 4παħ/e²c)", rel(mu0, 2 * alpha * h / (e * e * c), 3e-10));
        check("ε0 μ0 c² = 1 (within 3e-10)", rel(eps0 * mu0 * c * c, 1, 3e-10));
        check("σ = 5.670 374 419e-8 W m^-2 K^-4 (CODATA 2022, exact)", rel(findSymbol("σ")->q.v, 5.670374419e-8, 1e-9));
        check("R = 8.314 462 618 J/(mol K) (CODATA 2022, exact)", rel(findSymbol("R")->q.v, 8.314462618, 1e-9));
        check("ħ = 1.054 571 817e-34 J s (CODATA 2022, exact)", rel(findSymbol("ħ")->q.v, 1.054571817e-34, 1e-9));
        check("Ry = 13.605 693 122 990 eV (CODATA 2022)", rel(findSymbol("Ry")->q.v / e, 13.605693122990, 1e-13));
        check("u = 1.660 539 068 92e-27 kg (CODATA 2022)", findSymbol("u")->q.v == 1.66053906892e-27);
        check("ly = 9 460 730 472 580 800 m exactly (IAU)", findSymbol("ly")->q.v == 9460730472580800.0);
        check("pc = 3.085 677 581 491 3673e16 m (IAU 2015 B2: 648000/π au)", rel(findSymbol("pc")->q.v, 3.0856775814913673e16, 1e-15));
        check("au = 149 597 870 700 m exactly (IAU 2012 B2)", findSymbol("au")->q.v == 149597870700.0);
        check("atm = 101 325 Pa exactly", findSymbol("atm")->q.v == 101325.0);
        check("NA u = 1.000 000 001 05e-3 kg/mol (CODATA 2022 molar mass constant M_u)", rel(NA * findSymbol("u")->q.v, 1.00000000105e-3, 1e-10));
        check("kB in eV/K = 8.617 333 262e-5 (CODATA 2022, exact)", rel(k / e, 8.617333262e-5, 1e-9));
        check("π", findSymbol("π")->q.v == pi);
    }
    // ---------------------------------------------------------------- numbers and operators
    ok("2+3", 5, ""); ok("2 + 3 * 4", 14, ""); ok("(2+3)*4", 20, ""); ok("10/4", 2.5, ""); ok("2-3-4", -5, ""); ok("100/10/5", 2, "");
    ok("-2^2", -4, ""); ok("(-2)^2", 4, ""); ok("2^3^2", 512, ""); ok("2^-1", 0.5, ""); ok("2**3", 8, ""); ok("2^-3^2", 1.0 / 512, "");
    ok("2²", 4, ""); ok("10⁻³", 1e-3, ""); ok("3×10⁸", 3e8, ""); ok("3 × 10^8", 3e8, ""); ok("6 ÷ 2", 3, ""); ok("5 − 3", 2, ""); ok("2 · 3", 6, ""); ok("--2", 2, ""); ok("+5", 5, "");
    ok(".5", 0.5, ""); ok("5.", 5, ""); ok("1.5e3", 1500, ""); ok("1.5E-3", 1.5e-3, ""); ok("2e+2", 200, ""); ok("1.e5", 1e5, "");
    ok("2+2=", 4, ""); ok("  7  ", 7, "");
    ok("0.1 + 0.2 - 0.3", 0, ""); ok("1 - 0.9 - 0.1", 0, "");
    check("0.1 + 0.2 shows 0.3", ev("0.1 + 0.2").number == "0.3");
    check("0.1 + 0.2 - 0.3 is exactly 0 (rounding noise removed)", ev("0.1 + 0.2 - 0.3").q.v == 0 && ev("0.1+0.2-0.3").number == "0");
    // 0.50.3: the rule removes at most 4 units in the last place (was 1e-15 of the larger, about 4.5): 1e-30 on 1e-15 (about 4.5 units) is kept now, as 1 - 0.999999999999999 is
    check("1 + 1e-14 - 1 and 1e-15 + 1e-30 - 1e-15 are not thrown away (the rule only removes 4 rounding steps' worth)", ev("1e-15 + 1e-30 - 1e-15").q.v != 0 && ev("1 + 1e-14 - 1").q.v != 0 && ev("1e-15 + 1e-31 - 1e-15").q.v == 0);
    ok("2(3)", 6, ""); ok("(1+2)(3+4)", 21, ""); ok("2 (3+4)", 14, "");
    // ---------------------------------------------------------------- units
    ok("5 km", 5000, "m"); ok("3km", 3000, "m"); ok("2 m * 3 m", 6, "m²"); ok("m^3", 1, "m³"); ok("m⁻¹", 1, "1/m"); ok("1/s", 1, "1/s");
    ok("100 km / 2 hr", 100000.0 / 7200, "m/s"); ok("9.81 m/s^2", 9.81, "m/s²"); ok("9.81 m/s²", 9.81, "m/s²");
    ok("1 J/kg K", 1, "J/(kg·K)"); ok("kg m^2 / s^2", 1, "J"); ok("N m", 1, "J"); ok("J/s", 1, "W"); ok("N/m^2", 1, "Pa"); ok("A s", 1, "C"); ok("W/A", 1, "V");
    ok("V/A", 1, "Ω"); ok("kB", 1.380649e-23, "J/K"); ok("hP", 6.62607015e-34, "J·s"); ok("G", 6.6743e-11, "m³/(kg·s²)"); ok("μ0", 1.25663706127e-6, "N/A²");
    ok("R", 8.31446261815324, "J/(K·mol)", 1e-12); ok("GMsun", 1.32712440041279419e20, "m³/s²"); ok("e", 1.602176634e-19, "C"); ok("e V", 1.602176634e-19, "J");
    ok("sqrt(m^2)", 1, "m"); ok("sqrt(4 m^2)", 2, "m"); ok("sqrt(m)", 1, "m^(1/2)"); ok("sqrt(m)^2", 1, "m"); ok("cbrt(27 m^3)", 3, "m"); ok("cbrt(-27 m^3)", -3, "m");
    ok("(8 m^3)^(1/3)", 2, "m"); ok("(-8)^(1/3)", -2, ""); ok("(-8)^(2/3)", 4, ""); ok("abs(-3 m)", 3, "m"); ok("(m/s)^-2", 1, "s²/m²");
    ok("1 µm", 1e-6, "m"); ok("1 μm", 1e-6, "m"); ok("1 Å", 1e-10, "m"); ok("1 Å", 1e-10, "m"); ok("1 kΩ", 1000, "Ω"); ok("1 kΩ", 1000, "Ω");
    ok("20 deg", 20 * 3.14159265358979323846 / 180, ""); ok("20°", 20 * 3.14159265358979323846 / 180, "");
    // ---------------------------------------------------------------- "to"
    shown("5 km to mi", 5000 / 1609.344, 1e-12); shown("60 mph to km/hr", 96.56064, 1e-12); shown("1 day to min", 1440, 1e-15); shown("1 yr to day", 365.25, 1e-15);
    shown("1 au to km", 149597870.7, 1e-15); shown("1 ly to au", 63241.07708426628, 1e-12); shown("1 pc to ly", 3.261563777167433, 1e-12);
    shown("180 deg to rad", 3.14159265358979323846, 1e-15); shown("1 rad to deg", 57.29577951308232, 1e-14); shown("1 atm to bar", 1.01325, 1e-15);
    shown("1 eV to J", 1.602176634e-19, 1e-15); shown("2 m -> cm", 200, 1e-15); shown("2 m → cm", 200, 1e-15); shown("0.5 c to km/s", 149896.229, 1e-12);
    shown("1 / (2349 cm^-1) to μm", 1e4 / 2349, 1e-12); shown("4 m^2 to cm²", 40000, 1e-15); shown("3e8 m/s to c", 3e8 / 299792458, 1e-15);
    shown("1 GHz to 1/s", 1e9, 1e-15);
    check("\"5 km to mi\" shows the unit as typed", ev("5 km to mi").unit == "mi");
    check("after \"to\" ans is the answer in SI", ev("5 km to mi").q.v == 5000);
    // ---------------------------------------------------------------- functions and angles
    ok("sqrt(2)", std::sqrt(2.0), ""); ok("sin(30 deg)", 0.5, "", 1e-15); ok("cos(60 deg)", 0.5, "", 1e-15); ok("tan(45 deg)", 1, "", 1e-15);
    check("sin(180 deg) is exactly 0, cos(90 deg) exactly 0, sin(90 deg) exactly 1, cos(π) exactly -1, sin(-π/2) exactly -1, tan(π) exactly 0",
          ev("sin(180 deg)").q.v == 0 && ev("cos(90 deg)").q.v == 0 && ev("sin(90 deg)").q.v == 1 && ev("cos(π)").q.v == -1 && ev("sin(-π/2)").q.v == -1 && ev("tan(pi)").q.v == 0 &&
          ev("sin(360 deg)").q.v == 0 && ev("cos(270 deg)").q.v == 0);
    check("sin of a tiny angle is not rounded to 0: sin(1e-20) = 1e-20, sin(3.1415926535898) = -2.07e-14 (not exactly π)",
          ev("sin(1e-20)").q.v == 1e-20 && rel(ev("sin(3.1415926535898)").q.v, std::sin(3.1415926535898), 1e-9) && ev("sin(3.1415926535898)").q.v != 0);
    check("sin(30 deg) shows 0.5", ev("sin(30 deg)").number == "0.5");
    ok("asin(0.5) / (1 deg)", 30, "", 1e-14); shown("asin(1) to deg", 90, 1e-15); shown("atan(1) to deg", 45, 1e-15); shown("acos(-1) to deg", 180, 1e-15);
    ok("ln(e^0)", 0, ""); ok("exp(1)", 2.718281828459045, "", 1e-15); ok("ln(exp(2))", 2, "", 1e-15); ok("log10(1000)", 3, "", 1e-15); ok("sinh(0)", 0, ""); ok("cosh(0)", 1, ""); ok("tanh(0)", 0, "");
    // ---------------------------------------------------------------- ans
    {
        const Quantity a{1.282, dimOf(0, 0, 1)};
        const Result r = ev("ans * 2", &a);
        check("ans: the last answer with its unit (1.282 s x 2 = 2.564 s)", r.ok && rel(r.q.v, 2.564, 1e-15) && r.unit == "s");
        check("ans: in the list of what was used", r.ok && !r.used.empty() && r.used[0].find("ans = the last answer") == 0);
        check("ans without an earlier answer is refused", !ev("ans").ok && ev("ans").error.find("no earlier answer") != std::string::npos);
    }
    // ---------------------------------------------------------------- refused, with what to write instead
    bad("5 km + 3 s", "can't add a time (s) to a length (m)"); bad("5 km - 3 s", "can't subtract"); bad("1 + 1 m", "can't add"); bad("5 km to s", "can't show a length");
    bad("384,400", "without commas"); bad("1,5", "without commas"); bad("384 400", "Two numbers"); bad("km 2", "Two numbers"); bad("kB 293.15 K", "put * between them"); bad("m/s 2", "Two numbers"); bad("(2)3", "Two numbers");
    bad("100 km/h", "hours or Planck"); bad("h", "hr for hours"); bad("log(2)", "ln for the natural logarithm"); bad("5 km in mi", "use \"to\"");
    bad("20 °C", "kelvin"); bad("20 degC", "kelvin"); bad("300 celsius", "kelvin");
    bad("sqrt(-1)", "no real square root"); bad("(-1)^0.5", "no real answer"); bad("(-8)^(1/4)", "no real answer"); bad("1/0", "divide by zero"); bad("0^-1", "divide by zero");
    bad("1 m / (0 s)", "divide by zero"); bad("ln(0)", "above zero"); bad("log10(-1)", "above zero"); bad("asin(2)", "between -1 and 1"); bad("acos(-1.0000001)", "between -1 and 1");
    bad("sin(5 m)", "works on a plain number or an angle"); bad("exp(1 s)", "works on a plain number"); bad("tan(90 deg)", "right angle"); bad("tan(-pi/2)", "right angle");
    bad("2^(1 m)", "must be a plain number"); bad("m^0.31", "simple fraction"); bad("m^pi", "simple fraction");
    bad("(1+2", "not closed"); bad("1+2)", "no ( before"); bad("1 +", "missing at the end"); bad("* 2", "missing before"); bad("()", "Empty brackets"); bad("sqrt 4", "needs brackets");
    bad("sqrt()", "needs something inside"); bad("2^", "after ^"); bad("1.2.3", "only one decimal point"); bad("2 = 3", "right after a name");   // 0.50.2: named variables (was "no variables") bad("5 %", "0.05");
    bad("5 km to mi to ft", "Only one"); bad("5 km to 2 m", "only units"); bad("5 km to", "Write a unit after"); bad("to km", "before \"to\""); bad("5 to km", "can't show a plain number");
    bad("Km", "Did you mean km"); bad("j", "Did you mean J"); bad("mpp", "Did you mean"); bad("foo", "not a unit or constant"); bad("m2", "m^2"); bad("kg3", "kg^3");
    bad("1e999", "too big"); bad("1e-400", "too small"); bad("10^400", "too big"); bad("1e200 * 1e200", "too big"); bad("exp(1000)", "too big");
    bad("1e308 + 1e308", "too big");   // 0.50.2: an answer below the smallest normal number is 0 with a note (1e-200 * 1e-200, exp(-1000): tests/test_calc_qm.inc)
    bad("5 km to -m", "only units"); bad("$5", "does not know the sign"); bad("2 ⁻", "raised digit");
    bad(std::string(200, '(') + "1" + std::string(200, ')'), "Too many brackets"); bad(std::string(2000, '1'), "too long"); bad(std::string(300, '-') + "1", "Too many");
    bad("\xff", "could not be read");
    check("an empty line is no answer and no error", ev("").empty && ev("   ").empty && ev("=").empty && ev("").error.empty());
    check("an error says where: \"5 km + 3 s\" points at the +", ev("5 km + 3 s").errPos == 5 && ev("5 km + 3 s").errLen == 1);
    // ---------------------------------------------------------------- how the line was read
    check("read as: 100 km / 2 hr = (100 km) / (2 hr)", ev("100 km / 2 hr").readAs == "(100 km) / (2 hr)");
    // after /, a juxtaposition that is not a quantity is refused with both readings (calculators disagree about "4/3 π r^3" and "1/2π")
    check("unclear: 1/2 km names both readings", ev("1/2 km").error.find("(1 / 2) km or 1 / (2 km)") != std::string::npos);
    check("unclear: 4/3 π (6371 km)^3 names both readings", ev("4/3 π (6371 km)^3").error.find("(4 / 3) π (6371 km)^3 or 4 / (3 π (6371 km)^3)") != std::string::npos &&
                                                            ev("4/3 π (6371 km)^3").errPos == 1);
    bad("hP/2 π", "unclear");   // (0.50.1: 1 / (4 π ε0) e^2 is the Coulomb form now, test_calc_qm.inc)
    bad("6 / 2 m", "unclear"); bad("1 / 2349 cm^-1", "unclear"); bad("1 / -2 m", "unclear");
    bad("c / 2 me", "unclear"); bad("1 m / 2 (3 s)", "unclear"); bad("1 m / 2 ans", "unclear");
    ok("(4/3) π (6371 km)^3", 4.0 / 3 * 3.14159265358979323846 * 6371000.0 * 6371000.0 * 6371000.0, "m³", 1e-15);
    ok("4/3*π*(6371 km)^3", 4.0 / 3 * 3.14159265358979323846 * 6371000.0 * 6371000.0 * 6371000.0, "m³", 1e-15);
    ok("hP/(2 π)", 6.62607015e-34 / (2 * 3.14159265358979323846), "J·s", 1e-15); ok("1 / (2 km)", 0.0005, "1/m"); ok("1 / (2349 cm^-1)", 1 / 234900.0, "m", 1e-15);
    ok("6 m / 2 s", 3, "m/s"); ok("1 J / kg K", 1, "J/(kg·K)"); ok("c / 2 hr", 299792458.0 / 7200, "m/s²", 1e-15); ok("1 m / -2 s", -0.5, "m/s");
    shown("1/(4 π ε0) * e^2 / (1 fm) to MeV", 7.2973525643e-3 * 197.3269804, 5e-10);   // e²/(4πε0) = α ħc = 1.439 964 5 MeV fm (CODATA 2022 α and ħc)
    check("note: sin(30) says it is in radians and how to write degrees",
          ev("sin(30)").notes.size() == 1 && ev("sin(30)").notes[0].find("radians") != std::string::npos && ev("sin(30)").notes[0].find("sin(30 deg)") != std::string::npos);
    check("note: cos(1 + 2) suggests cos((1 + 2) deg)", ev("cos(1 + 2)").notes.size() == 1 && ev("cos(1 + 2)").notes[0].find("cos((1 + 2) deg)") != std::string::npos);
    check("note: none for sin(30 deg), sin(π/6), cos(0), sin(asin(0.5))",
          ev("sin(30 deg)").notes.empty() && ev("sin(π/6)").notes.empty() && ev("cos(0)").notes.empty() && ev("sin(asin(0.5))").notes.empty());
    bad("1 J + 1 m", "to an energy (J)");
    check("read as: -2^2 = −2^2 and 2^-1 = 2^(−1)", ev("-2^2").readAs == "−2^2" && ev("2^-1").readAs == "2^(−1)");
    check("read as: a - (b + c) keeps its brackets", ev("1 - (2 + 3)").readAs == "1 − (2 + 3)" && ev("1 - (2 + 3)").q.v == -4);
    check("read as: 2*(3/4) and 2/(3*4) keep theirs", ev("2*(3/4)").readAs == "2 × (3 / 4)" && ev("2/(3*4)").readAs == "2 / (3 × 4)");
    {
        // every line read back as written in "read as" gives the same value (the brackets it adds are the ones the rules imply)
        const char* lines[] = {"100 km / 2 hr", "1 / (2 km)", "-2^2", "2^-1", "2^3^2", "hP c / 500 nm", "sqrt(2 G Mearth / Rearth)", "J/kg K", "2 (3+4) m", "-3 km - -2 km",
                               "(1+2)(3+4)", "2*(3/4)", "2/(3*4)", "1 - (2 + 3)", "e^2/(4 π ε0 a0)", "2^-3^2", "m⁻¹ s²"};
        bool same = true;
        for (const char* l : lines) {
            const Result a = ev(l);
            std::string back = a.readAs;
            for (std::size_t p; (p = back.find("−")) != std::string::npos;) back.replace(p, std::strlen("−"), "-");
            const Result b = ev(back);
            if (!a.ok || !b.ok || !rel(b.q.v, a.q.v, 1e-15) || b.q.dim != a.q.dim) { same = false; std::printf("  read back: %s -> %s\n", l, back.c_str()); }
        }
        check("read as: reading the \"read as\" text again gives the same answer and unit (17 lines)", same);
    }
    check("used: a constant's line has its published value and source",
          ev("G").used.size() == 1 && ev("G").used[0].find("6.674 30(15) × 10⁻¹¹") != std::string::npos && ev("G").used[0].find("CODATA 2022") != std::string::npos);
    check("used: a unit's line says what it is", ev("3 km").used.size() == 1 && ev("3 km").used[0] == "km = kilometre = 1000 m");
    check("used: base units are not explained with themselves", ev("3 m").used.size() == 1 && ev("3 m").used[0] == "m = metre");
    check("kind: the answer names its kind", ev("100 km / 2 hr").kindWord == "speed" && ev("hP c / 500 nm").kindWord == "energy" && ev("2+2").kindWord.empty());
    {
        const Result r = ev("1 day");
        bool hr = false, mn = false;
        for (const std::string& a : r.also) { hr = hr || a == "= 24 hr"; mn = mn || a == "= 1440 min"; }
        check("also: 1 day = 24 hr = 1440 min", hr && mn && r.also.size() <= 3);
        check("also: 1 day's answer is in seconds", r.number == "86400" && r.unit == "s");
    }
    // ---------------------------------------------------------------- number writing
    check("format: 6.02214076×10²³, 1×10⁻⁵, 0.0001, -0 → 0, 1234567890123 (0.50.3: a whole number below 10^15 in full; was 1.23456789×10¹²)",
          formatNumber(6.02214076e23) == "6.02214076×10²³" && formatNumber(1e-5) == "1×10⁻⁵" && formatNumber(1e-4) == "0.0001" && formatNumber(-0.0) == "0" &&
          formatNumber(1234567890123.0) == "1234567890123" && formatNumber(1234567890123.5) == "1.23456789×10¹²" && formatNumber(299792458) == "299792458");
    {
        bool round = true;
        unsigned s = 12345;
        for (int i = 0; i < 20000; i++) {
            s = s * 1103515245u + 12345u;
            const double m = (s >> 8) / double(1 << 24), x = (i % 2 ? -1 : 1) * m * std::pow(10.0, (int(s % 600) - 300));
            const Result r = ev(exactNumber(x));
            if (!r.ok || r.q.v != x) { round = false; std::printf("  exact: %.17g -> %s\n", x, r.ok ? exactNumber(r.q.v).c_str() : r.error.c_str()); break; }
            const Result r10 = ev(num(x));
            if (!r10.ok || !rel(r10.q.v, x, 5.01e-10)) { round = false; std::printf("  num: %.17g -> %s\n", x, num(x).c_str()); break; }
        }
        check("exactNumber reads back as the same double, num() within 10 digits (5e-10) (20,000 random numbers, 1e-300 to 1e300)", round);
    }
    // ---------------------------------------------------------------- physics worked out with it, against independent values
    // light time Moon: 384 400 km / c = 1.282 211 s (from the definition of c)
    ok("384400 km / c", 384400000.0 / 299792458.0, "s");
    // a 500 nm photon: hc/λ with hc = 1239.841 984 eV nm (CODATA 2022, exact: h c / e)
    shown("hP c / 500 nm to eV", 1239.841984332002 / 500, 1e-12);
    // the hartree energy from e²/(4π ε0 a0): 27.211 386 245 981 eV (CODATA 2022), within ε0's and a0's uncertainties
    shown("e^2 / (4 π ε0 a0) to eV", 27.211386245981, 5e-10);
    // α from its definition e²/(4π ε0 ħ c) (CODATA 2022 7.297 352 5643e-3)
    ok("e^2 / (4 π ε0 ħ c)", 7.2973525643e-3, "", 5e-10);
    // proton/electron mass ratio: 1836.152 673 426 (CODATA 2022)
    ok("mp / me", 1836.152673426, "", 1e-11);
    // Earth's escape speed: NASA Earth Fact Sheet 11.186 km/s (from its mean radius: G Mearth / Rearth with NASA's mass and radius)
    shown("sqrt(2 G Mearth / Rearth) to km/s", 11.186, 1e-4);
    // Earth's surface gravity from its mass and mean radius: G M / R² = 9.820 m/s² (NASA Earth Fact Sheet: 9.820 "surface gravity", before the spin)
    shown("G Mearth / Rearth^2 to m/s^2", 9.820, 2e-4);
    // Earth's year from Kepler's third law with a = 1 au: the Gaussian year 365.256 898 3 days (IAU), to 1e-6 (DE440's GM vs Gauss's k)
    shown("2 π sqrt(au^3 / GMsun) to day", 365.2568983, 1e-6);
    // the Sun's luminosity from its nominal radius and temperature, 4π R² σ T⁴: IAU 2015 Resolution B3, L = 3.828e26 W with Teff 5772 K
    shown("4 π Rsun^2 σ (5772 K)^4 to W", 3.828e26, 1e-3);
    // thermal energy at 293.15 K: 293.15 x 8.617 333 262e-5 eV (CODATA 2022, exact)
    shown("kB * 293.15 K to eV", 293.15 * 8.617333262e-5, 1e-9);
    // the molar volume of an ideal gas at 273.15 K and 100 kPa: 22.710 954 64e-3 m³/mol (CODATA 2022, exact)
    ok("R * 273.15 K / 100 kPa", 22.71095464e-3, "m³/mol", 1e-9);
    // Lyman alpha of infinitely heavy hydrogen in the Bohr model: 4 / (3 R∞), R∞ = 10 973 731.568 157 m^-1 (CODATA 2022)
    shown("hP c / (Ry (1 - 1/4)) to nm", 4 / (3 * 10973731.568157) * 1e9, 1e-11);
    // Wien's displacement law with x = 4.965 114 231 744 276 (the root of x = 5(1 - e^-x)): b = hc / (x k) = 2.897 771 955e-3 m K (CODATA 2022, exact)
    ok("hP c / (4.965114231744276 kB)", 2.897771955e-3, "m·K", 1e-9);
    // speed of an electron in hydrogen's first Bohr orbit: α c = 2187.691 26 km/s (CODATA 2022 atomic unit of velocity 2.187 691 262 16e6 m/s)
    shown("α c to km/s", 2187.69126216, 1e-10);
    // the classical electron radius e²/(4π ε0 me c²) = 2.817 940 3205e-15 m (CODATA 2022)
    ok("e^2 / (4 π ε0 me c^2)", 2.8179403205e-15, "m", 1e-9);
    // Earth's orbital speed from v = 2π au / year: 29.78 km/s (NASA Earth Fact Sheet mean orbital velocity 29.78)
    shown("2 π au / 1 yr to km/s", 29.78, 2e-4);
    // a mole of carbon-12 atoms: 12 u NA = 12.000 000 0126 g (CODATA 2022 M_u = 1.000 000 001 05 g/mol)
    shown("12 u NA to g/mol", 12.0000000126, 1e-10);
    // a 1 kg mass fallen 10 m: m g h = 98.0665 J (standard gravity, exact)
    ok("1 kg * gn * 10 m", 98.0665, "J", 1e-15);
    // the Schwarzschild radius of the Sun 2 G M / c² = 2953.25 m (from JPL's GM: 2 GM/c² = 2 x 1.327 124 4e20 / c²)
    ok("2 GMsun / c^2", 2 * 1.32712440041279419e20 / (299792458.0 * 299792458.0), "m", 1e-15);
    // de Broglie wavelength of an electron at 100 eV: h / sqrt(2 me E) = 0.122 642 6 nm (from the constants, non-relativistic)
    shown("hP / sqrt(2 me * 100 eV) to nm", 6.62607015e-34 / std::sqrt(2 * 9.1093837139e-31 * 100 * 1.602176634e-19) * 1e9, 1e-12);
    // ---------------------------------------------------------------- the lab formulas, against independent values
    {
        const auto line = [](const std::vector<Recipe>& v, const std::string& start) -> std::string {
            for (const Recipe& r : v) if (r.label.rfind(start, 0) == 0) return r.line;
            return "";
        };
        // Earth (NASA Earth Fact Sheet: mass 5.9722e24 kg, equatorial radius 6378.137 km, mean distance 1 au) and a = 1 au
        const std::vector<Recipe> e = orbitRecipes("Earth", 1.495978707e11, 29780, 6378137, 5.9722e24, 1.495978707e11);
        check("lab: Earth has 6 formulas", e.size() == 6);
        bool allOk = true;
        for (const Recipe& r : e) if (!ev(r.line).ok) { allOk = false; std::printf("  %s: %s\n", r.line.c_str(), ev(r.line).error.c_str()); }
        check("lab: every Earth formula works", allOk);
        shown(line(e, "Time light"), 149597870700.0 / 299792458.0 / 60, 1e-9);                       // 8.316746 min
        shown(line(e, "Its year"), 365.2568983, 1e-6);                                                 // the Gaussian year
        shown(line(e, "Escape speed from Earth"), 11.18, 1e-3);                                        // from the equator's radius: √(2GM/R) = 11.18 km/s
        shown(line(e, "Gravity at Earth"), 9.798, 2e-4);                                               // GM / R² at the equator, no spin: 9.798 m/s² (NASA: 9.798 at the equator)
        shown(line(e, "Speed needed to escape the Sun"), 42.12, 1e-3);                                 // √2 x 29.78 km/s = 42.12 km/s at 1 au
        check("lab: a comet with no mass gets no surface formulas", orbitRecipes("Halley", 1e12, 1e4, 5500, 0, 2.7e12).size() == 4);
        // hydrogen-1 (the nucleus: the proton, CODATA 2022), n = 2: Lyman alpha with the reduced mass, 4 (1 + me/mp) / (3 R∞)
        const double mp = 1.67262192595e-27, me = 9.1093837139e-31, Rinf = 10973731.568157;
        const std::vector<Recipe> h = atomRecipes("Hydrogen-1", 1, mp, 2, 0, 1);
        allOk = true;
        for (const Recipe& r : h) if (!ev(r.line).ok) { allOk = false; std::printf("  %s: %s\n", r.line.c_str(), ev(r.line).error.c_str()); }
        check("lab: every hydrogen formula works", allOk && h.size() == 11);   // (0.50.1: 6 more, test_calc_qm.inc; 0.50.2: no binding energy per nucleon for A = 1)
        shown(line(h, "Light from the jump 2 → 1"), 4 / (3 * Rinf) * (1 + me / mp) * 1e9, 1e-9);   // 121.5023 x (1 + me/mp) = 121.568 nm
        shown(line(h, "Energy of level n = 2"), -13.605693122990 / 4 / (1 + me / mp), 1e-9);            // -3.4014 / (1 + me/mp) eV
        shown(line(h, "Size of orbit n = 2"), 4 * 52.917721054 * (1 + me / mp), 1e-9);                  // 4 a0 (1 + me/mp) in pm
        shown(line(h, "Electron speed"), 2187.69126216 / 2, 1e-9);
        shown(line(h, "Mass of this nucleus"), 1.0072764665789, 1e-9);                                 // the proton in u (CODATA 2022)
        // iron-56 (AME2020: binding energy per nucleon 8790.354 keV): only the division is the calculator's
        const std::vector<Recipe> fe = atomRecipes("Iron-56", 26, 9.288e-26, 1, 56 * 8.790354, 56);
        shown(line(fe, "Binding energy per nucleon"), 8.790354, 1e-9);
        shown(line(fe, "Light from the jump 2 → 1"), 4 / (3 * Rinf) / (26 * 26) * (1 + me / 9.288e-26) * 1e9, 1e-9);   // Z² shorter
        // carbon dioxide (12C 16O2: 43.98982923914 u from NIST's isotope masses) and its 2349 cm^-1 band (CCCBDB: the asymmetric stretch)
        const std::vector<Recipe> co2 = moleculeRecipes("CO2", 43.98982923914, 2349);
        allOk = true;
        for (const Recipe& r : co2) if (!ev(r.line).ok) { allOk = false; std::printf("  %s: %s\n", r.line.c_str(), ev(r.line).error.c_str()); }
        check("lab: every CO2 formula works", allOk && co2.size() == 5);
        shown(line(co2, "Molecules in 1 gram"), 1e-3 / (43.98982923914 * 1.66053906892e-27), 1e-9);  // 1.369e22
        shown(line(co2, "Mass of one mole"), 43.98982923914 * 1.00000000105, 1e-9);
        shown(line(co2, "Infrared wavelength"), 1e4 / 2349, 1e-9);                                    // 4.257 μm
        shown(line(co2, "Energy of one quantum"), 2349 * 1.239841984332e-4, 1e-9);                     // hc ν̃, hc = 1.239 841 984e-4 eV cm
        allOk = true;
        for (const Recipe& r : generalRecipes()) if (!ev(r.line).ok) allOk = false;
        check("lab: every general formula works", allOk);
        check("lab: an atom with no known mass still gets the electron speed, the Dirac level, the fine-structure gap and the Zeeman shift", atomRecipes("X", 30, 0, 1, -1, 0).size() == 4);
    }
    testQm();
    // ---------------------------------------------------------------- fuzz: random lines never crash and never give a non-finite or unexplained answer
    {
        const char* parts[] = {"1", "2", "0", "-", "+", "*", "/", "^", "(", ")", " ", "km", "m", "s", "c", "G", "e", "hP", "sqrt(", "sin(", "ln(", "to", " to km", "²", "⁻",
                               ".", "e5", "3.5", "deg", "°", "µ", "Ω", "=", ",", "1e308", "1e-308", "ans", "h", "x", "−", "×", "→", "->", "**", "kg", "J", "eV", "abs(", "π", "\xc3"};
        const int np = sizeof(parts) / sizeof(parts[0]);
        unsigned s = 777;
        int okCount = 0, errCount = 0;
        bool fine = true;
        const Quantity a{2, dimOf(0, 1, 0)};
        for (int i = 0; i < 200000 && fine; i++) {
            std::string l;
            s = s * 1664525u + 1013904223u;
            const int len = 1 + int(s >> 28);
            for (int k = 0; k < len; k++) { s = s * 1664525u + 1013904223u; l += parts[(s >> 16) % np]; }
            const Result r = ev(l, (i % 3) ? &a : nullptr);
            if (r.empty) continue;
            if (r.ok) {
                okCount++;
                if (!std::isfinite(r.q.v) || r.number.empty() || r.readAs.empty()) { fine = false; std::printf("  fuzz ok but bad: [%s]\n", l.c_str()); }
                const Result again = ev(l, (i % 3) ? &a : nullptr);
                if (!again.ok || again.q.v != r.q.v || again.number != r.number) { fine = false; std::printf("  fuzz: not the same twice [%s]\n", l.c_str()); }
            } else {
                errCount++;
                if (r.error.empty() || r.errPos < -1 || r.errPos > int(l.size()) || r.errPos + r.errLen > int(l.size())) {
                    fine = false; std::printf("  fuzz error bad: [%s] %s %d %d\n", l.c_str(), r.error.c_str(), r.errPos, r.errLen);
                }
            }
        }
        std::printf("fuzz: %d answers, %d refusals\n", okCount, errCount);
        check("fuzz: 200,000 random lines: no crash, every answer finite and shown, every refusal explained and pointing inside the line, same answer twice", fine && okCount > 1000 && errCount > 1000);
    }
    mt::run();                                                   // 0.54.2: small matrices (test_calc_matrix.inc)
    std::printf("calc_logic: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
