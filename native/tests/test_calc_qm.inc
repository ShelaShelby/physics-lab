// 0.50.1: the calculator for quantum mechanics (an outside review, 12 Nov 2026, worked 40 QM lines through it). Included by test_calc.cpp.
// Independent references only:
//   - CODATA 2022: P. J. Mohr et al., arXiv:2409.03787, Table XXXIII (data/calc/codata2022_mohr2024_arxiv2409.03787.pdf, read with pdftotext -layout);
//   - the exact SI constants h, c, e, kB (every conversion factor below is worked out here from them, never typed);
//   - closed forms of the special functions (DLMF chapters 5, 7, 10, 14, 18) and of the Clebsch-Gordan coefficients for j2 = 1/2 (Condon-Shortley phase);
//     J0's first zero is found here by bisection on J0's own power series, not with the library;
//   - the Atom Lab's own Dirac and hyperfine functions (src/atom/dirac.h, hfs.h), NIST ASD (data/asd/asd_lines.tsv: the H I 2p -> 1s rates and the 2p
//     level energies) and the measured 21-cm line, Eides, Grotch & Shelyuto, Phys. Rep. 342 (2001) eq. (406) (data/moments/).
#include "atom/dirac.h"
#include "atom/hfs.h"

static bool has(const std::vector<std::string>& v, const std::string& part) {
    for (const std::string& s : v) if (s.find(part) != std::string::npos) return true;
    return false;
}
static double val(const std::string& in) { const calc::Result r = ev(in); return r.ok ? r.q.v : NAN; }
// a line with a spectroscopy conversion after "to": the shown value (Result::inUnit) and the shown number
static void conv(const std::string& in, double want, double tol) {
    const calc::Result r = ev(in);
    check("\"" + in + "\" shows " + calc::formatNumber(want) + (r.ok ? " (shows " + r.number + " " + r.unit + ")" : " (error: " + r.error + ")"),
          r.ok && rel(r.inUnit, want, tol) && r.number == calc::formatNumber(r.inUnit));
}
static void near0(const std::string& in, double tol) {
    const calc::Result r = ev(in);
    check("\"" + in + "\" is 0 within " + calc::formatNumber(tol) + (r.ok ? " (got " + calc::formatNumber(r.q.v, 17) + ")" : " (error: " + r.error + ")"), r.ok && std::fabs(r.q.v) <= tol);
}
// J0 from its power series sum (-1)^k (x/2)^(2k) / (k!)^2 (DLMF 10.2.2), for the test's own bisection
static double j0series(double x) {
    double term = 1, sum = 1;
    for (int k = 1; k < 60; k++) { term *= -(x / 2) * (x / 2) / (double(k) * k); sum += term; }
    return sum;
}

static void testReview2();
static void testReview3();

static void testQm() {
    using namespace calc;
    const double c = 299792458, h = 6.62607015e-34, e = 1.602176634e-19, k = 1.380649e-23, pi = 3.14159265358979323846, hbar = h / (2 * pi);
    const double me = 9.1093837139e-31, mp = 1.67262192595e-27, alpha = 7.2973525643e-3;

    // ---------------------------------------------------------------- what already worked (the review's closed forms), kept as regression tests
    shown("-me*e^4/(8*ε0^2*hP^2) to eV", -13.605693122990, 2e-9);              // hcR∞ = 13.605 693 122 990(15) eV
    shown("4*π*ε0*ħ^2/(me*e^2) to pm", 52.917721054, 1e-9);                    // a0 = 5.291 772 105 44(82) e-11 m
    ok("me*e^4/(8*ε0^2*hP^3*c)", 10973731.568157, "1/m", 1e-9);                // R∞ = 10 973 731.568 157(12) 1/m
    ok("1/(e^2/(4*π*ε0*ħ*c))", 137.035999177, "", 1e-9);                        // 1/α = 137.035 999 177(21)
    shown("hP/(me*c) to pm", 2.42631023538, 1e-9);                             // λC = 2.426 310 235 38(76) e-12 m
    {   // the free-electron Fermi energy of copper, ħ²/2me (3π² n)^(2/3), n = 8.47e28 m^-3: fractional powers of units
        const double ef = hbar * hbar / (2 * me) * std::pow(3 * pi * pi * 8.47e28, 2.0 / 3) / e;
        shown("ħ^2/(2*me)*(3*π^2*8.47e28 m^-3)^(2/3) to eV", ef, 1e-9);
        check("Cu Fermi energy is 7.03 eV", std::fabs(ef - 7.03) < 0.005);
    }

    // ---------------------------------------------------------------- A1: ħ makes an angular frequency
    {
        const Result r = ev("e*a0*1e5 V/m/ħ to MHz");
        const double f = e * 5.29177210544e-11 * 1e5 / h / 1e6;                // the cyclic frequency, E/h, in MHz: 1279.54...
        check("Rabi line: 8039.617101 MHz shown (ω)", r.ok && r.number == "8039.617101");
        check("Rabi line: a note says it is an angular frequency ω in rad/s (" + (r.notes.empty() ? std::string("no note") : r.notes[0]) + ")",
              r.ok && has(r.notes, "angular frequency ω") && has(r.notes, "rad/s"));
        check("Rabi line: the note gives f = ω/2π = 1279.5 MHz", r.ok && has(r.notes, "1279.5") && std::fabs(f - 1279.54) < 0.01);
        check("Rabi line: \"also\" has the cyclic value", r.ok && has(r.also, "f = ω/2π = 1279.5") && has(r.also, " MHz"));
        const Result p = ev("1 eV/hP to THz");
        check("hP line: 1 eV/hP = 241.7989242 THz with no ω note", p.ok && p.number == "241.7989242" && !has(p.notes, "angular"));
        check("ħ line with no frequency (ħ c): no ω note", ev("ħ c to eV nm").ok && !has(ev("ħ c to eV nm").notes, "angular"));
        check("a rate written \"to 1/s\" gets no ω note", ev("(2/3)^8 α^5 me c^2 / ħ to 1/s").ok && !has(ev("(2/3)^8 α^5 me c^2 / ħ to 1/s").notes, "angular"));
    }

    // ---------------------------------------------------------------- A2: brackets and commas
    {
        const Result r = ev("1/(4 π ε0) e^2/a0 to eV");
        check("1/(4 π ε0) e^2/a0 is the Coulomb form: = 27.21138625 eV (" + (r.ok ? r.number : r.error) + ")", r.ok && rel(r.q.v / e, 27.211386245981, 5e-10));
        check("... read as (1 / (4 π ε0)) e^2 / a0 (" + r.readAs + ")", r.ok && r.readAs.find("(1 / (4 π ε0)) e^2") != std::string::npos);
        check("... and a note says so", r.ok && has(r.notes, "1/(4 π ε0)"));
        shown("1 / (4 π ε0) e^2 / (1 fm) to MeV", 7.2973525643e-3 * 197.3269804, 5e-10);   // α ħc
        const Result u = ev("1/(2 π) hP");
        check("unclear message quotes the user's own brackets: (1 / (2 π)) hP or 1 / ((2 π) hP) (" + u.error + ")",
              !u.ok && u.error.find("(1 / (2 π)) hP or 1 / ((2 π) hP)") != std::string::npos);
        bad("sum(1, J, 0, 50)", "joule");
        check("a comma inside a function's brackets is never \"without commas\"", ev("sum(1, J, 0, 50)").error.find("without commas") == std::string::npos);
        bad("foo(1, 2)", "not a function the calculator knows"); bad("sqrt(2, 3)", "sqrt takes 1"); bad("besselj(1)", "besselj takes 2");
        bad("cg(1, 1, 0.5)", "cg takes 6"); bad("(1,5)", "without commas"); bad("1, 2", "without commas"); bad("2 m, 3", "without commas");
        check("besselj(1,5): the comma separates the arguments", ev("besselj(1,5)").ok && ev("besselj(1,5)").q.v == val("besselj(1, 5)"));
    }

    // ---------------------------------------------------------------- B1: constants (CODATA 2022 Table XXXIII), read back digit for digit by the table test
    {
        struct C { const char* name; double v; double tol; } cs[] = {
            {"μB", 9.2740100657e-24, 0}, {"muB", 9.2740100657e-24, 0}, {"μN", 5.0507837393e-27, 0}, {"muN", 5.0507837393e-27, 0},
            {"Eh", 4.3597447222060e-18, 0}, {"hartree", 4.3597447222060e-18, 0}, {"Rinf", 10973731.568157, 0}, {"ge", -2.00231930436092, 1e-15},
            {"ae", 1.15965218046e-3, 0}, {"gp", 5.5856946893, 1e-10}, {"μp", 1.41060679545e-26, 5e-10}, {"mup", 1.41060679545e-26, 5e-10},
            {"μn", -9.6623653e-27, 1e-8}, {"mun", -9.6623653e-27, 1e-8}, {"mμ", 1.883531627e-28, 0}, {"mmu", 1.883531627e-28, 0}, {"md", 3.3435837768e-27, 0},
            {"mα", 6.6446573450e-27, 0}, {"malpha", 6.6446573450e-27, 0}, {"λC", 2.42631023538e-12, 0}, {"lambdaC", 2.42631023538e-12, 0},
            {"Φ0", 2.067833848e-15, 1e-9}, {"Phi0", 2.067833848e-15, 1e-9}, {"RK", 25812.80745, 1e-9}, {"KJ", 483597.8484e9, 1e-9},
            {"au_time", 2.4188843265864e-17, 0}, {"au_field", 5.14220675112e11, 0}, {"au_dipole", 8.4783536198e-30, 0}, {"bohr", 5.29177210544e-11, 0}};
        for (const C& x : cs) {
            const Symbol* s = findSymbol(x.name);
            check(std::string("constant ") + x.name + " = " + formatNumber(x.v) + (s ? " (got " + formatNumber(s->q.v, 17) + ")" : " (missing)"),
                  s && (x.tol == 0 ? s->q.v == x.v : rel(s->q.v, x.v, x.tol)));
        }
        check("μp/μN = 2.792 847 344 63 and μn/μN = -1.913 042 76 (the Atom Lab's own kMuP, kMuN)", rel(val("μp/μN"), 2.79284734463, 1e-15) && rel(val("μn/μN"), -1.91304276, 1e-15));
        check("gp = 2 μp/μN", rel(val("gp"), 2 * val("μp/μN"), 1e-15));
        check("ge = -2 (1 + ae)", rel(val("ge"), -2 * (1 + 1.15965218046e-3), 1e-15));
        check("μB = e ħ / 2me within 3e-10", rel(val("μB"), e * hbar / (2 * me), 3e-10));
        check("μN = e ħ / 2mp within 3e-10", rel(val("μN"), e * hbar / (2 * mp), 3e-10));
        check("Eh = α² me c² within 1e-9", rel(val("Eh"), alpha * alpha * me * c * c, 1e-9));
        shown("1 hartree to eV", 27.211386245981, 1e-12);
        check("λC = h / (me c) within 3e-10", rel(val("λC"), h / (me * c), 3e-10));
        check("Φ0 = h/2e, RK = h/e², KJ = 2e/h (exact)", rel(val("Φ0"), h / (2 * e), 1e-15) && rel(val("RK"), h / (e * e), 1e-15) && rel(val("KJ"), 2 * e / h, 1e-15));
        shown("GF / (ħ c)^3 to GeV^-2", 1.1663787e-5, 1e-12);                     // GF/(ħc)³ = 1.166 3787(6) e-5 GeV^-2
        check("a.u. of time = ħ/Eh, field = Eh/(e a0), dipole = e a0 (within 1e-9)",
              rel(val("au_time"), hbar / 4.3597447222060e-18, 1e-9) && rel(val("au_field"), 4.3597447222060e-18 / (e * 5.29177210544e-11), 1e-9) &&
              rel(val("au_dipole"), e * 5.29177210544e-11, 1e-9));
        check("μN is the nuclear magneton (J/T), not a micronewton", findSymbol("μN") && findSymbol("μN")->q.dim == dimOf(0, 2, 0, 1) && findSymbol("uN") == nullptr);
        check("D is the debye; d is still the day", findSymbol("D") && findSymbol("d") && findSymbol("d")->q.v == 86400);
        // B2: units, exact by definition
        check("debye = 1e-21/c C m", rel(val("1 D"), 1e-21 / c, 1e-15) && rel(val("1 debye"), 1e-21 / c, 1e-15) && ev("1 D").q.dim == dimOf(0, 1, 1, 1));
        check("gauss = 1e-4 T, erg = 1e-7 J, barn = 1e-28 m²", val("1 gauss") == 1e-4 && ev("1 gauss").unit == "T" && val("1 erg") == 1e-7 && ev("1 erg").unit == "J" &&
                                                                  val("1 barn") == 1e-28 && ev("1 barn").unit == "m²");
        shown("1 au_dipole to D", e * 5.29177210544e-11 / (1e-21 / c), 1e-9);    // 2.541 746 D
        check("no statC or esu (Gaussian charge is not a multiple of the coulomb)", findSymbol("statC") == nullptr && findSymbol("esu") == nullptr);
    }

    // ---------------------------------------------------------------- B3: spectroscopy conversions after "to"
    {
        const Result w = ev("1 eV to cm^-1");
        check("1 eV to cm^-1 = e/(hc) / 100 (" + (w.ok ? w.number : w.error) + ")", w.ok && rel(std::stod(w.number), e / (h * c) / 100, 1e-9) && has(w.notes, "hc"));
        conv("1 eV to cm^-1", e / (h * c) / 100, 1e-12);
        conv("1 eV to Hz", e / h, 1e-12);
        conv("1 eV to THz", e / h / 1e12, 1e-12);
        check("1 eV to Hz says h, the cyclic frequency", has(ev("1 eV to Hz").notes, "cyclic"));
        conv("1 eV to K", e / k, 1e-12);
        check("1 eV to K says kB", has(ev("1 eV to K").notes, "kB"));
        conv("1 u to MeV", 931.49410372, 1e-10);                               // CODATA: mu c² = 931.494 103 72(29) MeV
        check("1 u to MeV says c²", has(ev("1 u to MeV").notes, "c²"));
        shown("1 u c^2 to MeV", 931.49410372, 1e-10);
        conv("8065.54 cm^-1 to eV", 8065.54 * 100 * h * c / e, 1e-12);
        conv("1 cm^-1 to GHz", c * 100 / 1e9, 1e-12);                          // through hc and h: c
        conv("300 K to meV", 300 * k / e * 1e3, 1e-12);
        conv("1 MeV to u", 1 / 931.49410372, 1e-10);
        bad("1 eV + 1 K", "can't add"); bad("1 eV + 1 cm^-1", "can't add"); bad("1 kg to K", "can't show"); bad("1 m to K", "can't show");   // 0.50.2: "1 m to eV" is now allowed (E = hc/λ)
        bad("e*a0*1e5 V/m/ħ to eV", "angular frequency");
        check("conversions keep ans in SI (1 eV to K: ans is 1.602e-19 J)", ev("1 eV to K").q.v == e);
    }

    // ---------------------------------------------------------------- C2: functions on plain numbers, against closed forms
    {
        ok("gamma(0.5)", std::sqrt(pi), "", 1e-14); ok("gamma(1/2)^2", pi, "", 1e-14);
        double f = 1;
        bool facts = true;
        for (int n = 0; n <= 20; n++) {
            if (n > 0) f *= n;
            const std::string s = std::to_string(n);
            if (!rel(val("gamma(" + s + " + 1)"), f, 1e-14) || val("fact(" + s + ")") != f || val(s + "!") != f) { facts = false; std::printf("  %d!\n", n); }
        }
        check("Γ(n+1) = n! = fact(n) for n = 0..20", facts);
        ok("3!^2", 36, ""); ok("-3!", -6, ""); ok("(2+1)!", 6, ""); ok("2^3!", 64, "");
        check("fact(170) is finite, 171 is refused", std::isfinite(val("fact(170)")) && !ev("fact(171)").ok);
        bad("fact(2.5)", "whole number"); bad("fact(-1)", "0 to 170"); bad("2.5!", "whole number"); bad("fact(3 m)", "plain number");
        {
            double s = 0;
            for (int n = 1; n < 100; n++) s += std::log(double(n));
            ok("lngamma(100)", s, "", 1e-13);                                   // ln 99!
        }
        bad("gamma(0)", "no value"); bad("gamma(-2)", "no value"); bad("lngamma(-1)", "above zero");
        ok("erf(0)", 0, "");
        bool comp = true;
        for (double x : {-2.0, -0.3, 0.1, 0.5, 1.0, 2.5, 4.0}) if (std::fabs(val("erf(" + num(x) + ") + erfc(" + num(x) + ")") - 1) > 2e-16) comp = false;
        check("erf(x) + erfc(x) = 1 (7 values)", comp);
        bad("erf(1 m)", "plain number");
        bool p2 = true, h3 = true, l1 = true, j0 = true, p11 = true, p21 = true, p22 = true, p1m = true, l2 = true, j1 = true;
        for (double x : {-0.9, -0.5, 0.0, 0.3, 0.77, 1.0}) {
            const std::string xs = num(x);
            if (std::fabs(val("legendre(2, " + xs + ")") - (3 * x * x - 1) / 2) > 1e-15) p2 = false;
            if (std::fabs(val("assoclegendre(1, 1, " + xs + ")") + std::sqrt(1 - x * x)) > 1e-15) p11 = false;           // with the Condon-Shortley phase (-1)^m
            if (std::fabs(val("assoclegendre(2, 1, " + xs + ")") + 3 * x * std::sqrt(1 - x * x)) > 1e-14) p21 = false;
            if (std::fabs(val("assoclegendre(2, 2, " + xs + ")") - 3 * (1 - x * x)) > 1e-14) p22 = false;
            if (std::fabs(val("assoclegendre(1, -1, " + xs + ")") - std::sqrt(1 - x * x) / 2) > 1e-15) p1m = false;    // P_l^-m = (-1)^m (l-m)!/(l+m)! P_l^m
        }
        for (double x : {-2.0, -0.5, 0.0, 0.7, 1.5, 3.0}) {
            const std::string xs = num(x);
            if (std::fabs(val("hermite(3, " + xs + ")") - (8 * x * x * x - 12 * x)) > 1e-12) h3 = false;
            for (int kk = 0; kk <= 4; kk++) if (std::fabs(val("assoclaguerre(1, " + std::to_string(kk) + ", " + xs + ")") - (-x + kk + 1)) > 1e-14) l1 = false;
            if (std::fabs(val("laguerre(2, " + xs + ")") - (x * x - 4 * x + 2) / 2) > 1e-14) l2 = false;
            if (x != 0 && std::fabs(val("sphbessel(0, " + xs + ")") - std::sin(x) / x) > 1e-15) j0 = false;
            if (x != 0 && std::fabs(val("sphbessel(1, " + xs + ")") - (std::sin(x) / (x * x) - std::cos(x) / x)) > 1e-15) j1 = false;
        }
        check("legendre(2, x) = (3x² - 1)/2", p2);
        check("assoclegendre(1, 1, x) = -√(1-x²): the Condon-Shortley phase is in", p11);
        check("assoclegendre(2, 1, x) = -3x√(1-x²), (2, 2, x) = 3(1-x²), (1, -1, x) = √(1-x²)/2", p21 && p22 && p1m);
        check("hermite(3, x) = 8x³ - 12x (physicists')", h3);
        check("assoclaguerre(1, k, x) = -x + k + 1 (k = 0..4), laguerre(2, x) = (x² - 4x + 2)/2", l1 && l2);
        check("sphbessel(0, x) = sin x / x, sphbessel(1, x) = sin x/x² - cos x/x", j0 && j1);
        check("sphbessel(0, 0) = 1, sphbessel(1, -2) = -sphbessel(1, 2)", val("sphbessel(0, 0)") == 1 && val("sphbessel(1, -2)") == -val("sphbessel(1, 2)"));
        check("assoclegendre says which phase convention it uses", has(ev("assoclegendre(1, 1, 0.5)").notes, "Condon-Shortley"));
        check("assoclaguerre says which convention it uses", has(ev("assoclaguerre(1, 1, 0.5)").notes, "L_n^(k)"));
        {   // J0's first zero: bisection on the power series, against the published 2.404825557695773 (DLMF Table 10.21 lists j0,1 = 2.40482 55577)
            double a = 2, b = 3;
            for (int i = 0; i < 200; i++) { const double m = (a + b) / 2; if ((j0series(a) > 0) == (j0series(m) > 0)) a = m; else b = m; }
            check("J0's first zero by bisection on its series: 2.404825557695773", std::fabs(a - 2.404825557695773) < 2e-15);
            near0("besselj(0, 2.404825557695773)", 1e-15);
            bool j0s = true, j1s = true;
            for (double x : {0.5, 1.0, 3.7, 6.0}) {
                double t = x / 2, s = t;                                       // J1 series: sum (-1)^k (x/2)^(2k+1) / (k! (k+1)!)
                for (int kk = 1; kk < 60; kk++) { t *= -(x / 2) * (x / 2) / (double(kk) * (kk + 1)); s += t; }
                if (std::fabs(val("besselj(0, " + num(x) + ")") - j0series(x)) > 1e-14) j0s = false;
                if (std::fabs(val("besselj(1, " + num(x) + ")") - s) > 1e-14) j1s = false;
            }
            check("besselj(0, x) and besselj(1, x) against their power series", j0s && j1s);
            check("besselj(1, -x) = -besselj(1, x)", val("besselj(1, -2)") == -val("besselj(1, 2)"));
        }
        ok("binom(5, 2)", 10, ""); ok("binom(52, 5)", 2598960, ""); ok("binom(10, 0)", 1, "");   // 0.50.2: binom(3, 5) is 0 (part B3 below)
         bad("binom(2.5, 1)", "whole number");
        bad("legendre(-1, 0.5)", "whole number"); bad("legendre(1.5, 0.5)", "whole number"); bad("legendre(2, 1.5)", "between -1 and 1"); bad("hermite(2, 1 m)", "plain number");
        // 0.50.2: assoclegendre(1, 2, 0.5) is 0 (part B3 below)
        bad("hermite(200, 1)", "0 to 127");
    }

    // ---------------------------------------------------------------- C3: Clebsch-Gordan coefficients and Wigner 3j symbols
    {
        ok("cg(1, 1, 0.5, -0.5, 0.5, 0.5)", std::sqrt(2.0 / 3), "", 1e-15);
        ok("cg(1, 0, 1/2, 1/2, 1/2, 1/2)", -std::sqrt(1.0 / 3), "", 1e-15);
        // the closed forms for j2 = 1/2 (Condon-Shortley): J = j1 + 1/2: <j1 M-1/2; 1/2 1/2|J M> = √((j1+M+1/2)/(2j1+1)), <j1 M+1/2; 1/2 -1/2|J M> = √((j1-M+1/2)/(2j1+1));
        // J = j1 - 1/2: -√((j1-M+1/2)/(2j1+1)) and +√((j1+M+1/2)/(2j1+1))
        bool closed = true;
        int count = 0;
        for (int tj1 = 1; tj1 <= 8; tj1++) {
            const double j1 = tj1 / 2.0;
            for (int up = 0; up < 2; up++) {
                const double J = up ? j1 + 0.5 : j1 - 0.5;
                if (J < 0) continue;
                for (double M = -J; M <= J + 1e-9; M += 1) {
                    const auto line = [&](double m1, double m2) { return "cg(" + num(j1) + ", " + num(m1) + ", 0.5, " + num(m2) + ", " + num(J) + ", " + num(M) + ")"; };
                    const double a = up ? std::sqrt((j1 + M + 0.5) / (2 * j1 + 1)) : -std::sqrt((j1 - M + 0.5) / (2 * j1 + 1));
                    const double b = up ? std::sqrt((j1 - M + 0.5) / (2 * j1 + 1)) : std::sqrt((j1 + M + 0.5) / (2 * j1 + 1));
                    if (M - 0.5 >= -j1 - 1e-9 && std::fabs(val(line(M - 0.5, 0.5)) - a) > 1e-14) { closed = false; std::printf("  %s\n", line(M - 0.5, 0.5).c_str()); }
                    if (M + 0.5 <= j1 + 1e-9 && std::fabs(val(line(M + 0.5, -0.5)) - b) > 1e-14) { closed = false; std::printf("  %s\n", line(M + 0.5, -0.5).c_str()); }
                    count++;
                }
            }
        }
        check("cg: the j2 = 1/2 closed forms, j1 = 1/2 .. 4 (" + std::to_string(count) + " (J, M))", closed && count > 50);
        // orthogonality: sum over m1, m2 of <j1 m1 j2 m2|J M><j1 m1 j2 m2|J' M> = δ(J, J'), and sum over J of <..|J M><..|J M>' = δ(m1, m1')
        bool orth = true;
        for (const auto& jj : std::vector<std::pair<double, double>>{{1, 1}, {1.5, 1}, {2, 1.5}, {2.5, 2}}) {
            const double j1 = jj.first, j2 = jj.second;
            for (double J = std::fabs(j1 - j2); J <= j1 + j2 + 1e-9; J += 1)
                for (double J2 = std::fabs(j1 - j2); J2 <= j1 + j2 + 1e-9; J2 += 1)
                    for (double M = -std::min(J, J2); M <= std::min(J, J2) + 1e-9; M += 1) {
                        double s = 0;
                        for (double m1 = -j1; m1 <= j1 + 1e-9; m1 += 1) {
                            const double m2 = M - m1;
                            if (std::fabs(m2) > j2 + 1e-9) continue;
                            const std::string args = num(j1) + ", " + num(m1) + ", " + num(j2) + ", " + num(m2) + ", ";
                            s += val("cg(" + args + num(J) + ", " + num(M) + ")") * val("cg(" + args + num(J2) + ", " + num(M) + ")");
                        }
                        if (std::fabs(s - (std::fabs(J - J2) < 1e-9 ? 1 : 0)) > 1e-13) { orth = false; std::printf("  orth %g %g %g %g %g: %g\n", j1, j2, J, J2, M, s); }
                    }
        }
        check("cg: orthogonality, (j1, j2) = (1, 1), (3/2, 1), (2, 3/2), (5/2, 2)", orth);
        ok("wigner3j(1, 1, 0, 0, 0, 0)", -1 / std::sqrt(3.0), "", 1e-15);       // (j j 0; m -m 0) = (-1)^(j-m) / √(2j+1)
        ok("wigner3j(2, 2, 0, 1, -1, 0)", -1 / std::sqrt(5.0), "", 1e-15);
        bool rel3j = true, orth3j = true;
        for (double m1 = -1; m1 <= 1; m1 += 1)
            for (double m2 = -1.5; m2 <= 1.5; m2 += 1)
                for (double J = 0.5; J <= 2.5; J += 1) {
                    const double M = m1 + m2;
                    if (std::fabs(M) > J) continue;   // (-1)^(j1 - j2 + M) √(2J+1) (j1 j2 J; m1 m2 -M) = <j1 m1 j2 m2|J M>
                    const double three = val("wigner3j(1, 1.5, " + num(J) + ", " + num(m1) + ", " + num(m2) + ", " + num(-M) + ")");
                    const double sign = std::fmod(std::fabs(1 - 1.5 + M), 2.0) < 0.5 ? 1 : -1;
                    if (std::fabs(sign * std::sqrt(2 * J + 1) * three - val("cg(1, " + num(m1) + ", 1.5, " + num(m2) + ", " + num(J) + ", " + num(M) + ")")) > 1e-14) rel3j = false;
                }
        for (double m1 = -1; m1 <= 1; m1 += 1)
            for (double m1b = -1; m1b <= 1; m1b += 1) {   // sum over j3, m3 of (2j3+1) (1 1 j3; m1 m2 m3)(1 1 j3; m1' m2' m3) = δ δ, with m2 = m2' = 0 - ... (m1 + m2 = m1' + m2')
                const double m2 = 0 - m1, m2b = 0 - m1b;
                double s = 0;
                for (int j3 = 0; j3 <= 2; j3++)
                    s += (2 * j3 + 1) * val("wigner3j(1, 1, " + std::to_string(j3) + ", " + num(m1) + ", " + num(m2) + ", 0)") *
                         val("wigner3j(1, 1, " + std::to_string(j3) + ", " + num(m1b) + ", " + num(m2b) + ", 0)");
                if (std::fabs(s - (m1 == m1b ? 1 : 0)) > 1e-14) orth3j = false;
            }
        check("wigner3j: (-1)^(j1-j2+M) √(2J+1) (j1 j2 J; m1 m2 -M) = cg, for j1 = 1, j2 = 3/2", rel3j);
        check("wigner3j: orthogonality over j3 for j1 = j2 = 1", orth3j);
        near0("cg(1, 1, 1, 1, 2, 1)", 0);                                        // M ≠ m1 + m2: 0
        check("cg with M ≠ m1 + m2 says why it is 0", has(ev("cg(1, 1, 1, 1, 2, 1)").notes, "m1 + m2"));
        near0("cg(1, 0, 1, 0, 3, 0)", 0);                                        // J outside |j1 - j2| .. j1 + j2
        bad("cg(1, 0.3, 1, 0, 1, 0.3)", "half");   // 0.50.2: cg(1, 2, 1, 0, 1, 2) is 0 (part B3 below)
        bad("cg(1, 0.5, 1, 0, 1, 0.5)", "j + m"); bad("cg(-1, 0, 1, 0, 1, 0)", "must not be negative");
        bad("cg(1 m, 0, 1, 0, 1, 0)", "plain number"); bad("wigner3j(1, 1, 1, 0, 0)", "wigner3j takes 6");
    }

    // ---------------------------------------------------------------- D2: sums with a named index
    {
        const double N = 100000;
        ok("sum(1/k^2, k, 1, 100000)", pi * pi / 6 - 1 / N + 1 / (2 * N * N), "", 1e-13);   // the Basel series' tail: 1/N - 1/(2N²) + 1/(6N³) - ...
        ok("sum(k, k, 1, 100)", 5050, ""); ok("sum(2^n, n, 0, 10)", 2047, ""); ok("sum(k m, k, 1, 4)", 10, "m");
        {   // the rotational partition function of CO at 300 K: sum (2J+1) exp(-J(J+1) θ/T), θ = h c B / kB, B = 1.93 cm^-1; checked against the same sum done here
            const double theta = h * c * 193 / k;
            double z = 0;
            for (int J = 0; J <= 100; J++) z += (2 * J + 1) * std::exp(-J * (J + 1) * theta / 300);
            ok("sum((2 j + 1) exp(-j (j + 1) hP c * 1.93 cm^-1 / (kB * 300 K)), j, 0, 100)", z, "", 1e-13);
            check("... and it is near kB T / (h c B) + 1/3 (the high-temperature expansion)", rel(z, 300 / theta + 1.0 / 3, 1e-3));
        }
        check("sum: the index is not in the \"used\" list", ev("sum(k, k, 1, 3)").ok && !has(ev("sum(k, k, 1, 3)").used, "k ="));
        check("sum: read as keeps the commas", ev("sum(k, k, 1, 3)").readAs == "sum(k, k, 1, 3)");
        ok("sum(sum(i j, i, 1, 3), j, 1, 3)", 36, "");
        bad("sum(k, k, 1.5, 3)", "whole numbers"); bad("sum(k, k, 1, 1e7)", "at most 1000000"); bad("sum(k, 2, 1, 3)", "name"); bad("sum(k, c, 1, 3)", "speed of light");
        bad("sum(m^k, k, 1, 3)", "can't add"); bad("sum(k, k, 3, 1)", "nothing to add"); bad("k + 1", "not a unit or constant");
    }

    // ---------------------------------------------------------------- D3: a root on a bracket
    {
        ok("solve(x^2 - 2, x, 0, 2)", std::sqrt(2.0), "", 1e-15);
        ok("solve(x^2 - 2 m^2, x, 0 m, 2 m)", std::sqrt(2.0), "m", 1e-15);
        {   // the finite square well, z0 = 4: tan z = √(z0²/z² - 1) on 0 < z < π/2
            const Result r = ev("solve(tan(z) - sqrt(4^2/z^2 - 1), z, 0.1, 1.5)");
            const double z = r.q.v;
            check("solve: the finite well's even ground state, z0 = 4 (" + (r.ok ? r.number : r.error) + ")",
                  r.ok && z > 0.1 && z < 1.5 && std::fabs(std::tan(z) - std::sqrt(16 / (z * z) - 1)) < 1e-12);
        }
        bad("solve(x^2 + 1, x, 0, 2)", "same sign"); bad("solve(x, x, 0 m, 2 s)", "same kind"); bad("solve(tan(x), x, 1, 2)", "jump");
        bad("solve(x, x, 0)", "solve takes 4");
    }

    // ---------------------------------------------------------------- D1: the Atom Lab's new formulas, against the lab's own functions and NIST
    {
        const auto line = [](const std::vector<Recipe>& v, const std::string& start) -> std::string {
            for (const Recipe& r : v) if (r.label.rfind(start, 0) == 0) return r.line;
            return "";
        };
        const auto noteOf = [](const std::vector<Recipe>& v, const std::string& start) -> std::string {
            for (const Recipe& r : v) if (r.label.rfind(start, 0) == 0) return r.note;
            return "";
        };
        const std::vector<Recipe> H = atomRecipes("Hydrogen-1", 1, mp, 2, 0, 1);
        bool allOk = true;
        for (const Recipe& r : H) if (!ev(r.line).ok) { allOk = false; std::printf("  %s: %s\n", r.line.c_str(), ev(r.line).error.c_str()); }
        check("lab: every hydrogen formula works (" + std::to_string(H.size()) + " formulas)", allOk && H.size() == 11);   // 0.50.2: no binding energy per nucleon for A = 1
        // the Dirac level n = 2, j = 1/2 (infinite mass): diracInfiniteEV
        shown(line(H, "Dirac energy"), atom::diracInfiniteEV(1, 2, 1), 1e-10);
        const std::vector<Recipe> U = atomRecipes("Uranium-238", 92, 3.95e-25, 1, -1, 0);
        shown(line(U, "Dirac energy"), atom::diracInfiniteEV(92, 1, 1), 1e-10);   // (the lab uses me c² in eV, the line me in kg: they agree to 1e-11)
        // the fine-structure gap 2p1/2 - 2p3/2: leading order in (Zα)² against the exact Dirac gap
        {
            const double dirac = (atom::diracInfiniteEV(1, 2, 3) - atom::diracInfiniteEV(1, 2, 1)) * e / h / 1e9;   // GHz
            shown(line(H, "Fine-structure gap"), dirac, 1e-9);                  // 0.50.2: the exact Dirac difference (was the leading order, 3.3e-5 low)
            // measured: NIST ASD's 2p3/2 and 2p1/2 level energies, 82259.2850014 and 82258.9191133 cm^-1 (data/asd/asd_lines.tsv): 10.969 GHz, QED included
            const double measured = (82259.2850014 - 82258.9191133) * 100 * c / 1e9;
            check("lab: hydrogen's measured 2p gap (NIST ASD, 10.969 GHz) is 0.1-0.3 % above the Dirac one (the Lamb shift)", measured / dirac - 1 > 1e-3 && measured / dirac - 1 < 3e-3);
            check("lab: the gap's note names what it leaves out", noteOf(H, "Fine-structure gap").find("Lamb") != std::string::npos);
        }
        // the Zeeman shift of 2p3/2, mJ = 3/2, in 1 T: 0.50.2: gJ = 1 + (|ge| - 1)/3 (was 4/3, g = 2), so μB gJ (3/2) (1 T)
        shown(line(H, "Zeeman shift"), 9.2740100657e-24 * (1 + (2.00231930436092 - 1) / 3) * 1.5 / e * 1e6, 1e-12);
        // <r> of the level n, l = 0 with the reduced mass: (3/2) n² a0 / Z (1 + me/M)
        shown(line(H, "Mean distance"), 1.5 * 4 * 52.917721054 * (1 + me / mp), 1e-9);
        // the 2p -> 1s rate: NIST ASD 6.2648e8 (2p1/2) and 6.2647e8 (2p3/2) 1/s (data/asd/asd_lines.tsv, accuracy AAA)
        {
            const Result r = ev(line(H, "Rate of the jump 2p"));
            check("lab: the 2p -> 1s rate is 6.2648e8 1/s (NIST ASD) within 5e-5 (" + (r.ok ? r.number : r.error) + ")", r.ok && rel(r.q.v, 6.2648e8, 5e-5));
            check("lab: and it has no ω note (a rate)", r.ok && !has(r.notes, "angular"));
            const std::vector<Recipe> he = atomRecipes("Helium-4", 2, 6.6446573450e-27, 1, -1, 0);
            check("lab: the rate scales as Z⁴ (He+: 16 x hydrogen's, mass aside)", rel(val(line(he, "Rate of the jump 2p")) / r.q.v, 16 * (6.6446573450e-27 / (6.6446573450e-27 + me)) / (mp / (mp + me)), 1e-12));
        }
        // the Fermi hyperfine formula for hydrogen's 1s (21-cm line)
        {
            const Result r = ev(line(H, "Hyperfine splitting of 1s"));
            const double lab = atom::hfs::splittingEV(1, 0, 1, 0, 1) * e / h / 1e6;            // MHz, the lab's own (Breit's factor and the IAEA moment in)
            check("lab: Fermi's 21-cm line = 1420.49 MHz (" + (r.ok ? r.number : r.error) + ")", r.ok && std::fabs(std::stod(r.number) - 1420.485) < 0.005);
            check("lab: within 1e-4 of the Atom Lab's hyperfine splitting", r.ok && rel(std::stod(r.number), lab, 1e-4));
            const double measured = 1420405.7517667 / 1000;                                         // Eides, Grotch, Shelyuto 2001, eq. (406)
            check("lab: 5.6e-5 above the measured 1420.405 751 766 7 MHz (what it leaves out)", r.ok && std::stod(r.number) / measured - 1 > 4e-5 && std::stod(r.number) / measured - 1 < 7e-5);
            check("lab: hydrogen only (He-4 has no hyperfine formula)", line(atomRecipes("Helium-4", 2, 6.6446573450e-27, 1, -1, 0), "Hyperfine").empty());
        }
    }
    // the QM examples group
    {
        bool allOk = true;
        for (const Recipe& r : qmRecipes()) if (!ev(r.line).ok) { allOk = false; std::printf("  %s: %s\n", r.line.c_str(), ev(r.line).error.c_str()); }
        check("QM examples: every line works, the Rabi line among them", allOk && qmRecipes().size() >= 3 && qmRecipes()[0].line == "e*a0*1e5 V/m/ħ to MHz");
    }
    testReview2();
    testReview3();
}


// ---------------------------------------------------------------- 0.50.2: the second outside review (of 0.50.1). Every case here failed on 0.50.1.
// More independent references: sympy 1.x's exact wigner_3j and mpmath (30 digits), both run in the 0.50.2 session (their values are quoted where used);
// the closed forms of the 3j symbol (DLMF 34.3.1 and 34.3.5, Edmonds ch. 3: source not opened, each form checked against sympy's exact values first).
// the stretched 3j symbol (j1 j2 j1+j2; m1 m2 -M), M = m1 + m2: (-1)^(j1-j2+M) √[(2j1)!(2j2)!(J+M)!(J-M)! / ((2J+1)!(j1+m1)!(j1-m1)!(j2+m2)!(j2-m2)!)], J = j1 + j2,
// worked out with binomial coefficients (product formula), so it keeps about 13 digits up to j = 200. Doubled arguments.
static double stretched3j(int tj1, int tj2, int tm1, int tm2) {
    const int J = tj1 + tj2, M = tm1 + tm2;                           // (doubled)
    // ln of the bracket via sums of logs of whole numbers: ln n! = Σ ln k (each term exact to 1 ulp; the sums of a few hundred terms keep ~1e-14)
    // = C(2j1, j1+m1) C(2j2, j2+m2) / (C(2J, J+M) (2J+1)), each binomial by its product formula (at most 200 roundings: about 2e-14)
    const auto binom = [](int n, int kk) { double b = 1; for (int i = 1; i <= kk; i++) b = b * (n - kk + i) / i; return b; };
    const double sq = binom(tj1, (tj1 + tm1) / 2) * binom(tj2, (tj2 + tm2) / 2) / (binom(J, (J + M) / 2) * (J + 1));
    const int ph = (tj1 - tj2 + M) / 2;
    return ((ph % 2) != 0 ? -1.0 : 1.0) * std::sqrt(sq);
}
// (j1 j2 j3; 0 0 0), J = j1 + j2 + j3 even: (-1)^(J/2) √[(J-2j1)!(J-2j2)!(J-2j3)!/(J+1)!] (J/2)! / ((J/2-j1)!(J/2-j2)!(J/2-j3)!) (DLMF 34.3.5), whole j's
static double zero3j(int j1, int j2, int j3) {
    const int J = j1 + j2 + j3, g = J / 2;
    const auto lf = [](int n) { double s = 0; for (int k = 2; k <= n; k++) s += std::log(double(k)); return s; };
    const double ln = 0.5 * (lf(J - 2 * j1) + lf(J - 2 * j2) + lf(J - 2 * j3) - lf(J + 1)) + lf(g) - lf(g - j1) - lf(g - j2) - lf(g - j3);
    return ((g % 2) != 0 ? -1.0 : 1.0) * std::exp(ln);
}

static void testReview2() {
    using namespace calc;
    const double c = 299792458, h = 6.62607015e-34, e = 1.602176634e-19, k = 1.380649e-23, pi = 3.14159265358979323846, hbar = h / (2 * pi);
    const double mp = 1.67262192595e-27, muB = 9.2740100657e-24, gp = 5.5856946893;
    const auto noOmega = [](const std::string& in) {
        const Result r = ev(in);
        check("\"" + in + "\": no ω note (" + (r.ok ? (r.notes.empty() ? std::string("no note") : r.notes.back()) : r.error) + ")",
              r.ok && !has(r.notes, "angular") && !has(r.also, "ω/2π"));
    };
    const auto omega = [](const std::string& in, const std::string& f) {
        const Result r = ev(in);
        check("\"" + in + "\": the ω note offers f = ω/2π = " + f + " (" + (r.ok ? (r.notes.empty() ? std::string("no note") : r.notes.back()) : r.error) + ")",
              r.ok && has(r.notes, "angular frequency ω") && has(r.notes, f) && has(r.also, f));
    };
    const auto zeroWith = [](const std::string& in, const std::string& why) {
        const Result r = ev(in);
        check("\"" + in + "\" = 0 with a note \"" + why + "\" (" + (r.ok ? r.number + (r.notes.empty() ? "" : "; " + r.notes.back()) : r.error) + ")",
              r.ok && r.q.v == 0 && r.number == "0" && has(r.notes, why));
    };

    // ---------------------------------------------------------------- A1: the 2π note follows the powers of ħ and h, not the text
    {
        // μp B / h: ħ / h leaves no ħ: a cyclic frequency, gp e ħ/(2 mp) (1 T) / (2 h) = gp e / (8 π mp) (1 T)
        const Result r = ev("gp*e*ħ/(2*mp)*1 T/(2*hP) to MHz");
        check("μp B / h = 21.28873923 MHz (" + (r.ok ? r.number : r.error) + ")", r.ok && r.number == "21.28873923" && rel(r.inUnit, gp * e / (8 * pi * mp) / 1e6, 1e-9));
        noOmega("gp*e*ħ/(2*mp)*1 T/(2*hP) to MHz");
        shown("ħ*2*π/hP*1 GHz to GHz", 1, 1e-15);
        noOmega("ħ*2*π/hP*1 GHz to GHz");
        omega("2*μp*1 T/ħ to MHz", "42.57747846");                          // the proton's γp/2π (CODATA: 42.577 478 461 MHz/T)
        omega("muB*1 T/ħ to GHz", "13.99624492");                          // μB/h = 13.996 244 92 GHz/T (CODATA 2022: 13.996 244 9361)
        omega("e*a0*1e5 V/m/ħ to MHz", "1279.544802");
        {
            const Result eh = ev("Eh/ħ");
            check("Eh/ħ gets the ω note (" + (eh.ok && !eh.notes.empty() ? eh.notes.back() : eh.error) + ")", eh.ok && has(eh.notes, "angular frequency ω"));
        }
        noOmega("muB*1 T/hP to GHz"); noOmega("KJ*1 μV to MHz"); noOmega("2*e*1 μV/hP to MHz");
        // h/ħ times a frequency is ω = 2π f: angular, and f is the 1 GHz typed
        omega("hP/ħ*1 GHz to GHz", "f = ω/2π = 1 GHz");
        // a rate with no "to": the note says first that a rate has no 2π, and the answer stays the rate (never divided)
        const Result rate = ev("(2/3)^8*α^5*me*c^2/ħ");
        const std::string n = rate.ok && !rate.notes.empty() ? rate.notes.back() : "";
        check("a rate (no \"to\"): the note starts with the rate reading, before any number (" + n + ")", n.rfind("If this is a rate", 0) == 0);
        const double alpha = 7.2973525643e-3, me = 9.1093837139e-31;
        check("... and the answer is the undivided rate (2/3)^8 α^5 me c²/ħ in 1/s", rate.ok && rel(rate.q.v, std::pow(2.0 / 3, 8) * std::pow(alpha, 5) * me * c * c / hbar, 1e-9) && rate.unit == "1/s");
        bool onlyIf = true;
        for (const std::string& x : rate.also) if (x.find("ω/2π") != std::string::npos && x.rfind("if it is ω", 0) != 0) onlyIf = false;
        check("... and \"also\" offers ω/2π only as \"if it is ω\"", rate.ok && has(rate.also, "if it is ω") && onlyIf);
    }

    // ---------------------------------------------------------------- A2: an energy in rad/s is E/ħ (in Hz it stays E/h)
    {
        conv("1 eV to rad/s", e / hbar, 1e-12);                              // 1.519 267 447 × 10^15 rad/s (e and ħ exact)
        check("1 eV to rad/s shows 1.519267448×10¹⁵ (e/ħ = 1.519 267 447 88 × 10^15)", ev("1 eV to rad/s").number == "1.519267448×10¹⁵" && has(ev("1 eV to rad/s").notes, "ħω"));
        conv("1 eV to Hz", e / h, 1e-12);
        conv("1e15 rad/s to eV", 1e15 * hbar / e, 1e-12);
        check("1e15 rad/s to eV says ħω", has(ev("1e15 rad/s to eV").notes, "ħω"));
        conv("1 GHz to rad/s", 2 * pi * 1e9, 1e-15);                         // ω = 2π f
        conv("1 rad/s to Hz", 1 / (2 * pi), 1e-15);
        check("1 GHz to rad/s says 2π", has(ev("1 GHz to rad/s").notes, "2π"));
    }

    // ---------------------------------------------------------------- A3: 3j and Clebsch-Gordan to j = 100 (was an alternating sum in doubles)
    {
        bool diag = true, str = true, zer = true;
        double worst = 0;
        for (int j = 1; j <= 100; j += (j < 20 ? 1 : 7)) {
            for (int m : {0, 1, j / 2, j - 1, j}) {                          // (j j 0; m -m 0) = (-1)^(j-m) / √(2j+1) (DLMF 34.3.1)
                const double want = (((j - m) % 2) ? -1.0 : 1.0) / std::sqrt(2.0 * j + 1);
                const double got = val("wigner3j(" + std::to_string(j) + ", " + std::to_string(j) + ", 0, " + std::to_string(m) + ", " + std::to_string(-m) + ", 0)");
                if (!rel(got, want, 1e-12)) { diag = false; std::printf("  (j j 0; m -m 0) j=%d m=%d: %.17g, want %.17g\n", j, m, got, want); }
            }
        }
        for (int tj1 = 1; tj1 <= 200; tj1 += 13)                               // the stretched case, j1 + j2 = j3, half-integers too
            for (int tj2 : {1, 2, 7, 40, 101, 200}) {
                for (int tm1 : {-tj1, tj1 - 2 * (tj1 / 2), tj1}) for (int tm2 : {-tj2, tj2}) {
                    if (((tj1 + tm1) % 2) || ((tj2 + tm2) % 2)) continue;
                    const double want = stretched3j(tj1, tj2, tm1, tm2);
                    const auto hs = [](int t) { return num(t / 2.0); };
                    const std::string line = "wigner3j(" + hs(tj1) + ", " + hs(tj2) + ", " + hs(tj1 + tj2) + ", " + hs(tm1) + ", " + hs(tm2) + ", " + hs(-tm1 - tm2) + ")";
                    const double got = val(line);
                    worst = std::max(worst, std::fabs(got / want - 1));
                    if (!rel(got, want, 1e-12)) { str = false; std::printf("  %s = %.17g, want %.17g\n", line.c_str(), got, want); }
                }
            }
        for (const auto& t : std::vector<std::array<int, 3>>{{50, 50, 50}, {100, 100, 100}, {100, 60, 70}, {99, 100, 51}, {80, 40, 60}, {2, 3, 3}}) {
            const double want = zero3j(t[0], t[1], t[2]);
            const std::string line = "wigner3j(" + std::to_string(t[0]) + ", " + std::to_string(t[1]) + ", " + std::to_string(t[2]) + ", 0, 0, 0)";
            if (!rel(val(line), want, 1e-12)) { zer = false; std::printf("  %s = %.17g, want %.17g\n", line.c_str(), val(line), want); }
        }
        check("3j: (j j 0; m -m 0) = (-1)^(j-m)/√(2j+1) to 12 digits, j = 1..100", diag);
        check("3j: the stretched case (j1 j2 j1+j2; m1 m2 -M) to 12 digits, j up to 100 (worst " + formatNumber(worst, 2) + ")", str);
        check("3j: (j1 j2 j3; 0 0 0) closed form to 12 digits, j up to 100 (DLMF 34.3.5)", zer);
        // sympy's exact value (wigner_3j(50, 50, 50, 0, 0, 0), 30 digits): -0.0120047139798500242776641813376; 0.50.1 said "too big"
        ok("wigner3j(50, 50, 50, 0, 0, 0)", -0.0120047139798500242776641813376, "", 1e-13);
        // orthogonality: Σ_m1 <j1 m1 j2 M-m1|J M><j1 m1 j2 M-m1|J' M> = δ(J, J'), j1 = 60, j2 = 40, M = 3
        bool orth = true;
        for (const auto& JJ : std::vector<std::pair<int, int>>{{20, 20}, {20, 21}, {57, 57}, {57, 99}, {100, 100}, {80, 81}}) {
            double s = 0;
            for (int m1 = -60; m1 <= 60; m1++) {
                const int m2 = 3 - m1;
                if (std::abs(m2) > 40) continue;
                const std::string args = "cg(60, " + std::to_string(m1) + ", 40, " + std::to_string(m2) + ", ";
                s += val(args + std::to_string(JJ.first) + ", 3)") * val(args + std::to_string(JJ.second) + ", 3)");
            }
            if (std::fabs(s - (JJ.first == JJ.second ? 1 : 0)) > 1e-12) { orth = false; std::printf("  orth J=%d J'=%d: %.17g\n", JJ.first, JJ.second, s); }
        }
        check("cg: orthogonality at j1 = 60, j2 = 40 (within 1e-12)", orth);
        // the symmetries (Edmonds 3.7.5-3.7.6, DLMF 34.3.8-34.3.10): an even permutation keeps the 3j symbol, an odd one and m → -m multiply it by (-1)^(j1+j2+j3)
        bool sym = true;
        for (const auto& t : std::vector<std::array<double, 6>>{{70, 45.5, 60.5, 3, -1.5, -1.5}, {33, 47, 52, -10, 4, 6}, {99.5, 0.5, 100, 2.5, 0.5, -3}}) {
            const auto w = [&](int a, int b, int cc) {
                return val("wigner3j(" + num(t[a]) + ", " + num(t[b]) + ", " + num(t[cc]) + ", " + num(t[a + 3]) + ", " + num(t[b + 3]) + ", " + num(t[cc + 3]) + ")");
            };
            const double base = w(0, 1, 2), sgn = std::fmod(t[0] + t[1] + t[2], 2.0) < 0.5 ? 1 : -1;
            const double flip = val("wigner3j(" + num(t[0]) + ", " + num(t[1]) + ", " + num(t[2]) + ", " + num(-t[3]) + ", " + num(-t[4]) + ", " + num(-t[5]) + ")");
            if (base == 0 || !rel(w(1, 2, 0), base, 1e-12) || !rel(w(2, 0, 1), base, 1e-12) || !rel(w(1, 0, 2), sgn * base, 1e-12) || !rel(flip, sgn * base, 1e-12)) {
                sym = false; std::printf("  symmetry (%g %g %g; %g %g %g): %.17g\n", t[0], t[1], t[2], t[3], t[4], t[5], base);
            }
        }
        check("3j: permutation and m → -m symmetries at j up to 100", sym);
    }

    // ---------------------------------------------------------------- A4: an answer below the smallest normal number is 0, with a note
    {
        zeroWith("exp(-800)", "shown as 0");
        zeroWith("1e-200*1e-200", "shown as 0");
        zeroWith("erfc(27)", "shown as 0");                                    // 5.237e-319
        zeroWith("exp(-1000)", "shown as 0");
        const Result one = ev("1 + exp(-800)");
        check("1 + exp(-800) = 1 with the note", one.ok && one.q.v == 1 && has(one.notes, "shown as 0"));
        bad("1e-400", "too small");                                            // a number typed below it is still refused
        bad("exp(1000)", "too big");                                           // and overflow is still an error
        // the rotational partition function of HCl (B = 10.59 cm^-1); mpmath nsum, 30 digits: 300 K to j = 200: 20.0261235529029615; 10 K to j = 30: 1.14299215810153782
        ok("sum((2*j+1)*exp(-j*(j+1)*hP*c*10.59 cm^-1/(kB*300 K)), j, 0, 200)", 20.0261235529029615, "", 1e-9);
        ok("sum((2*j+1)*exp(-j*(j+1)*hP*c*10.59 cm^-1/(kB*10 K)), j, 0, 30)", 1.14299215810153782, "", 1e-12);
        {
            double z = 0;                                                      // and the same sum done here
            for (int j = 0; j <= 200; j++) z += (2 * j + 1) * std::exp(-j * (j + 1) * h * c * 1059 / (k * 300));
            check("... and the 300 K sum done here agrees", rel(val("sum((2*j+1)*exp(-j*(j+1)*hP*c*10.59 cm^-1/(kB*300 K)), j, 0, 200)"), z, 1e-13));
        }
    }

    // ---------------------------------------------------------------- B: the Atom Lab's formulas and small errors
    {
        const auto line = [](const std::vector<Recipe>& v, const std::string& start) -> std::string {
            for (const Recipe& r : v) if (r.label.rfind(start, 0) == 0) return r.line;
            return "";
        };
        const auto label = [](const std::vector<Recipe>& v, const std::string& start) -> std::string {
            for (const Recipe& r : v) if (r.label.rfind(start, 0) == 0) return r.label;
            return "";
        };
        // B1: the fine-structure gap 2p1/2 - 2p3/2 is the exact Dirac difference for every Z (0.50.1: leading order, -29 % at Z = 92)
        for (int Z : {1, 30, 92}) {
            const std::vector<Recipe> v = atomRecipes("X", Z, 1e-25, 2, -1, 0);
            const double want = (atom::diracInfiniteEV(Z, 2, 3) - atom::diracInfiniteEV(Z, 2, 1)) * e / h / 1e9;
            const std::string l = line(v, "Fine-structure gap");
            shown(l.empty() ? "missing" : l, want, Z == 1 ? 1e-9 : 1e-11);
        }
        check("B1: the gap's label no longer says leading order", label(atomRecipes("Hydrogen-1", 1, mp, 2, 0, 1), "Fine-structure gap").find("leading") == std::string::npos);
        // B2: the Zeeman shift uses gJ with the measured |ge|: 2p3/2: gJ = 1 + (|ge| - 1)(j(j+1) - l(l+1) + s(s+1))/(2j(j+1)) = 1 + (|ge| - 1)/3
        {
            const std::vector<Recipe> H = atomRecipes("Hydrogen-1", 1, mp, 2, 0, 1);
            shown(line(H, "Zeeman shift"), muB * (1 + (2.00231930436092 - 1) / 3) * 1.5 * 1 / e * 1e6, 1e-12);
            check("B2: the Zeeman line uses ge", line(H, "Zeeman shift").find("ge") != std::string::npos);
        }
        // B5: hydrogen-1 has no binding energy per nucleon line; deuterium has one
        check("B5: no binding energy per nucleon for A = 1", label(atomRecipes("Hydrogen-1", 1, mp, 2, 0, 1), "Binding energy").empty());
        check("B5: deuterium keeps it", !label(atomRecipes("Hydrogen-2", 1, 3.3435837768e-27, 2, 2.224566, 2), "Binding energy").empty());
    }
    // B3: 0 where 0 is the standard value (sympy and Mathematica do the same), with a note saying why
    zeroWith("assoclegendre(2, 3, 0.5)", "|m| > l");
    zeroWith("binom(3, 5)", "k > n");
    zeroWith("binom(3, -1)", "k < 0");
    zeroWith("cg(1, 2, 1, 0, 1, 2)", "|m|");
    zeroWith("wigner3j(1, 1, 1, 2, -2, 0)", "|m|");
    zeroWith("cg(1, 1, 1, 1, 2, 1)", "m1 + m2");
    zeroWith("wigner3j(1, 1, 3, 0, 0, 0)", "triangle");
    zeroWith("cg(1, 0, 1, 0, 3, 0)", "triangle");
    bad("binom(2.5, 1)", "whole number"); bad("cg(1, 0.3, 1, 0, 1, 0.3)", "half"); bad("assoclegendre(1.5, 0, 0.5)", "whole number");
    // B4: the error names the arguments of the function that was called (cg's third j is J)
    {
        const Result a = ev("cg(250, 0, 1, 0, 250, 0)"), b = ev("wigner3j(250, 1, 250, 0, 0, 0)");
        check("B4: cg's limit message says j1, j2 and J (" + a.error + ")", !a.ok && a.error.find("j1, j2 and J") != std::string::npos && a.error.find("j3") == std::string::npos);
        check("B4: wigner3j's says j1, j2 and j3 (" + b.error + ")", !b.ok && b.error.find("j1, j2 and j3") != std::string::npos);
    }

    // ---------------------------------------------------------------- C1: energy and wavelength through E = hc/λ, after "to" only
    {
        conv("1 eV to nm", h * c / e * 1e9, 1e-12);                          // 1239.841 984 nm
        check("1 eV to nm shows 1239.841984", ev("1 eV to nm").number == "1239.841984");
        conv("500 nm to eV", h * c / 500e-9 / e, 1e-12);
        check("500 nm to eV says: the vacuum wavelength of a photon", has(ev("500 nm to eV").notes, "vacuum wavelength") && has(ev("1 eV to nm").notes, "vacuum wavelength"));
        conv("500 nm to THz", c / 500e-9 / 1e12, 1e-12);
        conv("1 cm^-1 to cm", 1, 1e-12);
        bad("1 eV + 500 nm", "can't add"); bad("500 nm to K", "can't show"); bad("1 kg to nm", "can't show");
    }

    // ---------------------------------------------------------------- C2: complex numbers (i is the imaginary unit; nothing else was called i)
    {
        const auto cx = [](const std::string& in, double re, double im, const std::string& unit, const std::string& shownAs) {
            const Result r = ev(in);
            check("\"" + in + "\" = " + shownAs + (unit.empty() ? "" : " " + unit) + (r.ok ? " (got " + r.number + " " + r.unit + ")" : " (error: " + r.error + ")"),
                  r.ok && std::fabs(r.q.v - re) <= 1e-15 * std::max(1.0, std::fabs(re)) && std::fabs(r.q.im - im) <= 1e-15 * std::max(1.0, std::fabs(im)) &&
                  r.unit == unit && (shownAs.empty() || r.number == shownAs));
        };
        cx("i^2", -1, 0, "", "-1");
        cx("i*i*i", 0, -1, "", "-i");
        cx("(3+4i)*(3-4i)", 25, 0, "", "25");
        cx("1/(1+i)", 0.5, -0.5, "", "0.5 − 0.5i");
        cx("conj(1+2i)", 1, -2, "", "1 − 2i");
        cx("abs(3+4i)", 5, 0, "", "5");
        cx("arg(i)", pi / 2, 0, "", "");
        cx("arg(-1 + 0 i)", pi, 0, "", "");
        cx("re(3+4i)", 3, 0, "", "3"); cx("im(3+4i)", 4, 0, "", "4");
        cx("exp(i*π)", -1, 0, "", "-1");                                        // Euler: e^(iπ) = -1 (the 1.2e-16 left by π's rounding is dropped, see calc.h)
        cx("exp(i*π/2)", 0, 1, "", "i");
        cx("exp(1 + i*π/3)", std::exp(1.0) / 2, std::exp(1.0) * std::sqrt(3.0) / 2, "", "");
        cx("2 i + sqrt(-4)", 0, 4, "", "4i");                                   // sqrt of a negative number is imaginary when the line uses i
        cx("(2+3i) m + (1-i) m", 3, 2, "m", "(3 + 2i)");                       // units stay on the whole number
        cx("(1+i) m * (1-i) m", 2, 0, "m²", "2");
        cx("sum(i^k, k, 0, 3)", 0, 0, "", "0");
        cx("(1+i)^2", 0, 2, "", "2i");
        cx("i^0.5", std::sqrt(0.5), std::sqrt(0.5), "", "");
        cx("(-1)^0.5 + 0 i", 0, 1, "", "i");
        cx("1 + 1e-20 i", 1, 1e-20, "", "1 + 1×10⁻²⁰i");
        ok("sum(sum(i j, i, 1, 3), j, 1, 3)", 36, "");                          // a sum's own index i is still its index
        bad("sqrt(-4)", "no real square root");                                 // without i in the line: as before
        bad("sin(i)", "real"); bad("ln(i)", "real"); bad("gamma(1+i)", "real"); bad("(1+i) eV to K", "real"); bad("solve(x^2 + i, x, 0, 1)", "real");
        bad("2 m + i", "can't add");
        cx("(3+4i) km to m", 3000, 4000, "m", "(3000 + 4000i)");
        const Result u = ev("i");
        check("i says what it is", u.ok && has(u.used, "imaginary unit"));
    }

    // ---------------------------------------------------------------- C4: named variables, x = 2 m on one line and x on the next (the panel keeps them)
    {
        Vars v;
        const Result a = evaluate("x = 2 m");
        check("x = 2 m: kept as x (" + (a.ok ? a.number + " " + a.unit : a.error) + ")", a.ok && a.assigned == "x" && a.q.v == 2 && a.unit == "m");
        v["x"] = a.q;
        const Result b = evaluate("x^2 / 4", nullptr, &v);
        check("x^2 / 4 = 1 m² with x = 2 m", b.ok && b.q.v == 1 && b.unit == "m²" && has(b.used, "x = your variable = 2 m"));
        bad("T = 300 K", "already the tesla");
        const Result t = evaluate("temp = 300 K", nullptr, &v);
        v["temp"] = t.q;
        const Result c2 = evaluate("kB temp to meV", nullptr, &v);
        check("kB temp to meV with temp = 300 K", c2.ok && rel(c2.inUnit, k * 300 / e * 1e3, 1e-12));
        check("x = x + 1 m uses the old x", evaluate("x = x + 1 m", nullptr, &v).q.v == 3);
        check("1/2 x is unclear (x is a number, not a unit)", !evaluate("1/2 x", nullptr, &v).ok);
        check("a sum's own index shadows a variable", evaluate("sum(x, x, 1, 3)", nullptr, &v).ok && evaluate("sum(x, x, 1, 3)", nullptr, &v).q.v == 6);
        bad("c = 3", "already the speed of light"); bad("sqrt = 2", "already a name"); bad("i = 2", "already a name"); bad("ans = 1", "already a name");
        check("x = at the end shows x, as 2 + 2 = does", evaluate("x =", nullptr, &v).ok && evaluate("x =", nullptr, &v).assigned.empty()); bad("x = 2 = 3", "right after a name"); bad("y", "not a unit or constant");
        check("a variable is not kept by evaluate itself", !evaluate("x").ok);
    }
}

// 0.50.3: the outside review of 0.50.2 (calculator points 1-4, 33, 57)
static void testReview3() {
    using namespace calc;
    // 1: a whole number below 10^15 is exact in a double, so it is shown in full (3^21 = 10460353203: integer arithmetic)
    check("3^21 shows 10460353203", evaluate("3^21").number == "10460353203");
    check("2^40 shows 1099511627776", evaluate("2^40").number == "1099511627776");
    check("1e12 stays 1×10¹² (no lost digits)", evaluate("1e12").number == "1×10¹²");
    check("fact(20) (above 10^15) keeps 10 digits", evaluate("fact(20)").number == "2.432902008×10¹⁸");
    check("1/3 keeps 10 digits", evaluate("1/3").number == "0.3333333333");
    // 2: only a few units in the last place are rounding noise; 1 - 0.999999999999999 is the double difference 9.992007221626409e-16 (a 1e-15 step, written to 15 nines)
    check("1 - 0.999999999999999 is not 0", evaluate("1 - 0.999999999999999").q.v == 1.0 - 0.999999999999999);
    check("0.1 + 0.2 - 0.3 is still 0", evaluate("0.1 + 0.2 - 0.3").q.v == 0);
    check("1 - 0.9 - 0.1 is still 0", evaluate("1 - 0.9 - 0.1").q.v == 0);
    check("(1 + i) - (1 + 0.999999999999999i) keeps its imaginary part", evaluate("(1 + i) - (1 + 0.999999999999999i)").q.im != 0);
    // 3: a real negative number to a fraction with an odd denominator is the real root on every line (adding 0i does not change it)
    check("(-8)^(1/3) + 0i = -2", evaluate("(-8)^(1/3) + 0i").ok && evaluate("(-8)^(1/3) + 0i").q.v == -2 && evaluate("(-8)^(1/3) + 0i").q.im == 0);
    check("(-8)^(2/3) + 0i = 4", rel(evaluate("(-8)^(2/3) + 0i").q.v, 4, 1e-15) && evaluate("(-8)^(2/3) + 0i").q.im == 0);
    check("(-4)^0.5 + 0i = 2i (no real root: still the principal value)", evaluate("(-4)^0.5 + 0i").q.v == 0 && rel(evaluate("(-4)^0.5 + 0i").q.im, 2, 1e-15));
    // 4: a variable after "to" is a named quantity (5000 m = 2500 x with x = 2 m: kept); a complex one is refused
    {
        Vars v;
        v["x"] = evaluate("x = 2 m").q;
        v["z"] = evaluate("z = (1 + i) m").q;
        const Result a = evaluate("5000 m to x", nullptr, &v);
        check("5000 m to x = 2500 x with x = 2 m", a.ok && a.inUnit == 2500);
        check("5 m to z (complex) is refused", !evaluate("5 m to z", nullptr, &v).ok);
    }
    // 33: the limit counts characters, not bytes: 600 μ (1,200 bytes) is under 1000 characters
    {
        std::string mu;
        for (int k = 0; k < 600; k++) mu += "μ";
        const Result r = evaluate(mu);
        check("600 μ is not 'too long'", r.ok || r.error.find("too long") == std::string::npos);
        std::string mu2;
        for (int k = 0; k < 1001; k++) mu2 += "μ";
        check("1001 μ is too long", evaluate(mu2).error.find("too long") != std::string::npos);
    }
    // 57: 3j at j = 200 (the advertised limit): (j j 0; m -m 0) = (-1)^(j-m) / √(2j+1) (DLMF 34.3.1)
    for (int m : {0, 1, 100, 199, 200}) {
        const double want = (((200 - m) % 2) ? -1.0 : 1.0) / std::sqrt(401.0);
        const Result r = evaluate("wigner3j(200, 200, 0, " + std::to_string(m) + ", " + std::to_string(-m) + ", 0)");
        check("(200 200 0; m -m 0), m = " + std::to_string(m), r.ok && rel(r.q.v, want, 1e-12));
    }
}
