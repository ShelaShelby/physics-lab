// Molecule Lab rotation (0.53) tests, written before the code. Independent references only:
//  - every constant equals the saved page it comes from (NIST WebBook: Huber & Herzberg's tables; NIST's atomic masses and isotopic compositions; NUBASE2020's spins),
//    read here from the files as received;
//  - textbook formulas: the rigid rotor's spacing 2B(J+1); the band's first R and P lines 2B from the band origin with no line at the origin; the most populated level
//    J = sqrt(kT / 2hcB) - 1/2 (weights 1); the high-temperature partition function kT / (sigma hcB) (1 + hcB / 3kT) (its next term is of order (hcB/kT)^2);
//    ortho : para hydrogen 3 : 1 when hot and all para when cold; the levels the Pauli principle forbids (16O2 even N, 12C16O2 odd J) hold exactly nothing;
//  - the Born-Oppenheimer mass rules against the measured D35Cl constants (NIST WebBook's DCl page, saved for this test);
//  - HCl's measured fundamental lines (Rank, Eastman, Rao & Wiggins, J. Opt. Soc. Am. 52, 1 (1962), Table III, saved).
// 0.53.1, the Morse curve and the bond that breaks (molecule/morse.h), written before the code:
//  - the closed-form Morse levels are we (v+1/2) - wexe (v+1/2)^2 exactly, the last bound level floor(we / 2wexe - 1/2); V(re) = 0, V -> De far out;
//  - the motion: the energy (1/2) mu (dr/dt)^2 + V(r) stays the level's energy along the path (1e-9 relative); the turning points solve V(r) = E; the period and
//    the mean bond length against a direct quadrature of dr / speed (independent of the closed form); the mean grows with v; the period tends to 1 / (c we);
//    the harmonic limit (wexe -> 0) gives the lab's harmonic swing (vib::turningPoint);
//  - each D0 and re equals its saved page (Huber & Herzberg's footnote 125 for H2; Darwent, NSRDS-NBS 31 (1970) for N2, O2, HCl; Rank et al. 1962's band origins);
//  - the Pekeris alpha_e against the measured one, within the limits set from the comparison (REPORT 0.53.1).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "molecule/molecule.h"
#include "molecule/morse.h"
#include "molecule/rotation.h"
#include "molecule/vibration.h"

using namespace molecule;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}
static std::string f(double v) { char b[64]; std::snprintf(b, sizeof b, "%.8g", v); return b; }
static bool rel(double a, double b, double tol) { return std::fabs(a - b) <= tol * std::fabs(b); }
static const double kH = 6.62607015e-34, kC = 299792458.0, kK = 1.380649e-23, kPi = 3.14159265358979323846, kHbar = kH / (2 * kPi);   // CODATA 2022 (exact)
static const double kHcK = kH * kC * 100 / kK;   // hc/k in cm K (1.4387769)

static std::string readFile(const std::string& rel) {
    std::ifstream in(std::string(PL_DATA_DIR) + "/" + rel, std::ios::binary);
    std::stringstream s; s << in.rdbuf();
    std::string t = s.str();
    t.erase(std::remove(t.begin(), t.end(), '\r'), t.end());   // (a Windows checkout has CRLF line ends)
    return t;
}
// The X state's row of a saved WebBook "Constants of diatomic molecules" page: the cells as numbers. A subscript digit is the table's extra, uncertain digit (Huber &
// Herzberg print it small), so "2990.946<sub>3</sub>" is 2990.9463; footnote links are dropped; brackets (a v = 0 value) and the "Z" flag are removed.
static std::vector<double> xRow(const std::string& file) {
    const std::string s = readFile(file);
    std::vector<double> out;
    const std::size_t at = s.find("> X <sup>");
    if (at == std::string::npos) return out;
    const std::size_t end = s.find("</tr>", at);
    std::size_t p = s.find("<td", at);
    while (p != std::string::npos && p < end) {
        const std::size_t q = s.find("</td>", p);
        std::string cell = s.substr(s.find('>', p) + 1, q - s.find('>', p) - 1);
        for (std::size_t a; (a = cell.find("<a ")) != std::string::npos;) cell.erase(a, cell.find("</a>", a) + 4 - a);
        std::string t;
        bool tag = false;
        for (char ch : cell) { if (ch == '<') tag = true; else if (ch == '>') tag = false; else if (!tag && ch != '[' && ch != ']' && ch != ' ' && ch != 'Z') t += ch; }
        if (t.rfind("&nbsp;", 0) == 0) t.clear();
        out.push_back(t.empty() ? NAN : std::strtod(t.c_str(), nullptr));
        p = s.find("<td", q);
    }
    return out;   // (after the state's cell) Te, we, wexe, weye, Be, alpha_e, gamma_e, De, beta_e, re, ...
}
// A footnote of a saved WebBook page written as a series in (v+1/2), e.g. Huber & Herzberg's HCl footnote 29 "+0.001772<sub>4</sub>(v+1/2)<sup>2</sup> - 0.0001201(v+1/2)<sup>3</sup>":
// the coefficients by power of (v+1/2) (the subscript digit joined as in xRow; the sign as printed). Empty: the footnote is not there.
static std::vector<std::pair<int, double>> footnoteSeries(const std::string& file, int note) {
    const std::string s = readFile(file);
    const std::string key = "id=\"Dia" + std::to_string(note) + "\">";
    std::size_t a = s.find(key);
    std::vector<std::pair<int, double>> out;
    if (a == std::string::npos) return out;
    a = s.find("<td>", a);
    const std::size_t e = s.find("</td>", a);
    std::string c = s.substr(a + 4, e - a - 4), t;
    for (std::size_t q; (q = c.find("<sub>")) != std::string::npos;) c.erase(q, 5);
    for (std::size_t q; (q = c.find("</sub>")) != std::string::npos;) c.erase(q, 6);
    c = c.substr(0, c.find(';'));   // footnote 30 goes on in words after the series
    bool tag = false;
    for (char ch : c) { if (ch == '<') tag = true; else if (ch == '>') tag = false; else if (!tag && ch != ' ') t += ch; }
    const std::string x = "(v+1/2)";
    std::size_t from = 0;
    for (std::size_t q; (q = t.find(x, from)) != std::string::npos;) {
        const double k = std::strtod(t.substr(from, q - from).c_str(), nullptr);
        std::size_t r = q + x.size();
        int pw = 1;
        if (r < t.size() && std::isdigit(static_cast<unsigned char>(t[r]))) { pw = t[r] - '0'; r++; }
        out.push_back({pw, k});
        from = r;
    }
    return out;
}
// NIST compositions page: the "Relative Atomic Mass" and "Isotopic Composition" of mass number a
static void composition(const std::string& file, int a, double& mass, double& ab) {
    const std::string s = readFile(file);
    const std::size_t p = s.find("Mass Number = " + std::to_string(a) + "\n");
    mass = ab = NAN;
    if (p == std::string::npos) return;
    const std::size_t m = s.find("Relative Atomic Mass = ", p), c = s.find("Isotopic Composition = ", p);
    mass = std::strtod(s.c_str() + m + 23, nullptr);
    ab = std::strtod(s.c_str() + c + 23, nullptr);
}
// NUBASE2020: the spin of a ground state (2I), from its "J pi" column
static int twoSpin(const std::string& key) {
    std::istringstream in(readFile("nubase/nubase_4.mas20.txt"));
    for (std::string ln; std::getline(in, ln);) {
        if (ln.compare(0, key.size(), key) != 0) continue;
        std::istringstream w(ln.substr(ln.find("stbl") + 4));
        std::string j; w >> j;
        const std::size_t sl = j.find('/');
        return sl != std::string::npos ? std::atoi(j.c_str()) : 2 * std::atoi(j.c_str());
    }
    return -1;
}

static void testConstants() {
    struct P { const char* id; const char* file; };
    for (const P& p : {P{"H2", "h2"}, P{"N2", "n2"}, P{"O2", "o2"}, P{"HCl", "hcl"}}) {
        const std::vector<double> x = xRow(std::string("molecules/vib/webbook_diatomic/nist_webbook_") + p.file + "_constants.extract.txt");
        const rot::Measured* m = rot::measured(p.id);
        check(x.size() >= 10 && m, std::string(p.id) + ": the X row read (" + std::to_string(x.size()) + " cells)");
        if (x.size() < 10 || !m) continue;
        check(m->Be == x[4], std::string(p.id) + ": B equals the page's " + f(x[4]) + " (has " + f(m->Be) + ")");
        check(m->alphaE == x[5], std::string(p.id) + ": alpha_e equals the page's " + f(x[5]) + " (has " + f(m->alphaE) + ")");
        check(m->De == x[7], std::string(p.id) + ": D equals the page's " + f(x[7]) + " (has " + f(m->De) + ")");
    }
    check(rot::measured("O2") && rot::measured("O2")->b0 && !rot::measured("HCl")->b0, "O2's table gives B0 (in brackets), the others Be");
    // 0.54.1: HCl's higher terms, Huber & Herzberg's footnotes 29 (to alpha_e: B_v's terms in (v+1/2)^2 and ^3) and 30 (to De: D_v's terms in (v+1/2) and (v+1/2)^2)
    {
        const std::string file = "molecules/vib/webbook_diatomic/nist_webbook_hcl_constants.extract.txt";
        const auto n29 = footnoteSeries(file, 29), n30 = footnoteSeries(file, 30);
        const rot::Measured* h = rot::measured("HCl");
        check(n29.size() == 2 && n29[0].first == 2 && n29[1].first == 3 && h->bv2 == n29[0].second && h->bv3 == n29[1].second,
              "HCl: B_v's (v+1/2)^2 and ^3 terms equal footnote 29's (" + (n29.size() == 2 ? f(n29[0].second) + ", " + f(n29[1].second) : std::string("not read")) + ")");
        check(n30.size() == 2 && n30[0].first == 1 && n30[1].first == 2 && h->dv1 == n30[0].second && h->dv2 == n30[1].second,
              "HCl: D_v's (v+1/2) and (v+1/2)^2 terms equal footnote 30's (" + (n30.size() == 2 ? f(n30[0].second) + ", " + f(n30[1].second) : std::string("not read")) + ")");
        for (const char* id : {"H2", "N2", "O2"}) { const rot::Measured* o = rot::measured(id); check(o->bv2 == 0 && o->bv3 == 0 && o->dv1 == 0 && o->dv2 == 0, std::string(id) + ": no higher terms used"); }
    }
    // the masses and abundances (NIST compositions pages)
    double m1, a1, m2, a2, m35, a35, m37, a37;
    composition("molecules/nist_compositions_H.extract.txt", 1, m1, a1); composition("molecules/nist_compositions_H.extract.txt", 2, m2, a2);
    composition("molecules/nist_compositions_Cl.extract.txt", 35, m35, a35); composition("molecules/nist_compositions_Cl.extract.txt", 37, m37, a37);
    check(rot::massU(1, 1) == m1 && rot::massU(1, 2) == m2 && rot::massU(17, 35) == m35 && rot::massU(17, 37) == m37,
          "the masses of 1H, 2H, 35Cl, 37Cl equal NIST's pages (" + f(m1) + " " + f(m2) + " " + f(m35) + " " + f(m37) + ")");
    check(rot::abundance(17, 35) == a35 && rot::abundance(17, 37) == a37 && rot::abundance(1, 2) == a2, "the abundances equal NIST's pages (" + f(a35) + " " + f(a37) + " " + f(a2) + ")");
    // the linear molecules' B: CCCBDB's (the lab's), for the rigid three
    for (const char* id : {"HCN", "CO2", "C2H2"}) {
        const rot::Rotor r = rot::rotor(id);
        check(r.B[0] == get(findId(id)).rotB && r.D[0] == 0 && r.D[1] == 0, std::string(id) + ": the lab's measured B (CCCBDB), rigid");
    }
}

static void testWeights() {
    // the spins from NUBASE2020 and the rule give the textbook weights
    const int sH = twoSpin("001 0010"), sN = twoSpin("014 0070"), sO = twoSpin("016 0080"), sC = twoSpin("012 0060");
    check(sH == 1 && sN == 2 && sO == 0 && sC == 0, "NUBASE2020 spins: 1H 1/2, 14N 1, 16O 0, 12C 0 (" + std::to_string(sH) + " " + std::to_string(sN) + " " + std::to_string(sO) + " " + std::to_string(sC) + ")");
    int e, o;
    rot::spinWeights(sH, false, e, o); check(e == 1 && o == 3, "two protons, Sigma_g+: even 1, odd 3");
    rot::spinWeights(sN, false, e, o); check(e == 6 && o == 3, "two 14N, Sigma_g+: even 6, odd 3");
    rot::spinWeights(sO, true, e, o);  check(e == 0 && o == 1, "two 16O, Sigma_g-: only odd N");
    rot::spinWeights(sO, false, e, o); check(e == 1 && o == 0, "two 16O in CO2 (Sigma_g+): only even J");
    struct W { const char* id; int e, o; };
    for (const W& w : {W{"H2", 1, 3}, W{"N2", 6, 3}, W{"O2", 0, 1}, W{"CO2", 1, 0}, W{"C2H2", 1, 3}, W{"HCl", 1, 1}, W{"HCN", 1, 1}}) {
        const rot::Rotor r = rot::rotor(w.id);
        check(r.gEven == w.e && r.gOdd == w.o, std::string(w.id) + ": weights " + std::to_string(w.e) + ", " + std::to_string(w.o));
    }
    check(rot::rotor("O2").nLevels && rot::lowestJ(rot::rotor("O2")) == 1 && rot::lowestJ(rot::rotor("CO2")) == 0, "O2: N levels, the lowest N = 1");
    // which molecules turn (linear) and which have lines
    int n = 0;
    for (int i = 0; i < count(); i++) n += rot::hasRotation(get(i)) ? 1 : 0;
    check(n == 7 && !rot::hasRotation(get(findId("H2O"))), "seven linear molecules turn here (" + std::to_string(n) + "); water is not shown yet");
    for (const char* id : {"H2", "N2", "O2", "CO2", "C2H2"}) check(!rot::rotor(id).dipole && rot::lines(id, 0, rot::Spectrum::PureRotation, 298.15).empty(), std::string(id) + ": no permanent dipole, no pure-rotation lines");
    check(rot::rotor("HCl").dipole && rot::rotor("HCN").dipole, "HCl and HCN have a dipole");
    check(rot::hasSpectrum(rot::rotor("HCl"), rot::Spectrum::Fundamental) && !rot::hasSpectrum(rot::rotor("HCN"), rot::Spectrum::Fundamental), "the fundamental band: HCl only");
}

static void testLevels() {
    // rigid: F(J+1) - F(J) = 2B(J+1) exactly
    for (const char* id : {"HCN", "CO2", "C2H2"}) {
        const rot::Rotor r = rot::rotor(id);
        bool ok = true;
        for (int J = 0; J < 40; J++) ok = ok && std::fabs(rot::term(r, 0, J + 1) - rot::term(r, 0, J) - 2 * r.B[0] * (J + 1)) < 1e-9;
        check(ok, std::string(id) + ": rigid spacing 2B(J+1)");
    }
    // the diatomics: B_v = Be - alpha_e (v + 1/2), F = B J(J+1) - D [J(J+1)]^2
    const rot::Rotor h = rot::rotor("HCl");
    const rot::Measured* m = rot::measured("HCl");
    // (0.54.1: with Huber & Herzberg's footnotes 29 and 30: B_v = Be - alpha_e x + bv2 x^2 + bv3 x^3, D_v = De + dv1 x + dv2 x^2, x = v + 1/2)
    for (int v = 0; v < 2; v++) {
        const double x = v + 0.5, Bv = m->Be - m->alphaE * x + m->bv2 * x * x + m->bv3 * x * x * x, Dv = m->De + m->dv1 * x + m->dv2 * x * x;
        check(std::fabs(h.B[v] - Bv) < 1e-12 && std::fabs(h.D[v] - Dv) < 1e-18, "HCl: B_" + std::to_string(v) + " and D_" + std::to_string(v) + " from the series in v + 1/2 (" + f(h.B[v]) + ", " + f(h.D[v]) + ")");
        check(std::fabs(rot::term(h, v, 3) - (Bv * 12 - Dv * 144)) < 1e-9, "HCl: F_" + std::to_string(v) + "(3) = 12 B_v - 144 D_v");
    }
    const rot::Rotor o = rot::rotor("O2");
    check(o.B[0] == rot::measured("O2")->Be, "O2: B0 is the table's bracketed value");
    // the band: R(0) = nu0 + 2B1 - 4D and P(1) = nu0 - 2B0 + 4D: about 2B on each side of the origin, where no line is
    const auto L = rot::lines("HCl", rot::kH35Cl, rot::Spectrum::Fundamental, 298.15);
    const auto* an = vib::anharmonic("HCl");
    const double nu0 = vib::termG(*an, 1) - vib::termG(*an, 0);
    check(std::fabs(h.nu0 - nu0) < 1e-9, "HCl: the band origin is G(1) - G(0) of the lab's ladder (" + f(nu0) + ")");
    double r0 = NAN, p1 = NAN, nearest = 1e9;
    for (const rot::Line& l : L) {
        if (l.branch == 'R' && l.J == 0) r0 = l.nu;
        if (l.branch == 'P' && l.J == 1) p1 = l.nu;
        nearest = std::min(nearest, std::fabs(l.nu - nu0));
        if (l.branch == 'P') check(l.J >= 1, "no P(0)");
    }
    check(std::fabs(r0 - (nu0 + 2 * h.B[1] - 4 * h.D[1])) < 1e-9 && std::fabs(p1 - (nu0 - 2 * h.B[0] + 4 * h.D[0])) < 1e-9, "R(0) and P(1) about 2B from the origin (" + f(r0) + ", " + f(p1) + ")");
    check(nearest > h.B[0], "no line at the band origin (the gap: nearest " + f(nearest) + " cm^-1)");
    // pure rotation: J -> J+1 at F(J+1) - F(J)
    const auto PR = rot::lines("HCN", 0, rot::Spectrum::PureRotation, 298.15);
    bool ok = !PR.empty();
    for (const rot::Line& l : PR) ok = ok && std::fabs(l.nu - 2 * rot::rotor("HCN").B[0] * (l.J + 1)) < 1e-9 && l.jHi == l.J + 1 && l.m == l.J + 1;
    check(ok, "HCN: pure-rotation lines at 2B(J+1)");
    check(rot::lines("HCl", 0, rot::Spectrum::PureRotation, 298.15).size() > 5, "HCl: pure-rotation lines");
    // names
    rot::Line a; a.branch = 'R'; a.J = 3; rot::Line b; b.branch = 'P'; b.J = 2; rot::Line c; c.branch = 'J'; c.J = 2; c.jLo = 2; c.jHi = 3;
    check(a.name() == "R(3)" && b.name() == "P(2)" && c.name() == "J 2 → 3", "line names R(3), P(2), J 2 → 3");
}

static void testPopulations() {
    // a rigid rotor with weights 1 (HCl's B0, no D): the most populated level is sqrt(kT / 2hcB) - 1/2 rounded (where that is not near a half)
    rot::Rotor r = rot::rotor("HCl");
    r.D[0] = r.D[1] = 0;
    int tested = 0; bool ok = true;
    for (double T = 20; T <= 3000; T += 10) {
        const double j = std::sqrt(T / (2 * kHcK * r.B[0])) - 0.5;
        if (std::fabs(j - std::floor(j) - 0.5) < 0.15) continue;
        tested++;
        if (rot::mostPopulated(r, T) != static_cast<int>(std::lround(j))) { ok = false; std::printf("  T %g: %d against %.3f\n", T, rot::mostPopulated(r, T), j); }
    }
    check(ok && tested > 100, "rigid, weights 1: the most populated J is sqrt(kT/2hcB) - 1/2 rounded (" + std::to_string(tested) + " temperatures)");
    // the partition function at high T: kT / (sigma hcB) (1 + hcB / 3kT), within the next term (hcB/kT)^2 of kT / (sigma hcB)
    struct C { const char* id; double T; };
    for (const C& c : {C{"HCl", 1000}, C{"HCl", 3000}, C{"CO2", 300}, C{"N2", 300}, C{"H2", 3000}, C{"C2H2", 300}, C{"HCN", 300}, C{"O2", 300}}) {
        rot::Rotor q = rot::rotor(c.id);
        q.D[0] = q.D[1] = 0;
        const double x = kHcK * q.B[0] / c.T, sig = (q.gEven + q.gOdd) == 1 ? 2 : 1;   // only one parity allowed: sigma = 2, weights 1
        const double gAvg = (q.gEven + q.gOdd) == 1 ? 1 : (q.gEven + q.gOdd) / 2.0;   // the weights' mean (H2, N2, C2H2: each parity holds half the levels)
        const double Q = rot::partition(q, c.T).Q / gAvg, want = 1 / (sig * x) * (1 + x / 3);
        check(std::fabs(Q - want) < (1 / (sig * x)) * x * x && !rot::partition(q, c.T).cut, std::string(c.id) + " at " + f(c.T) + " K: Q = kT/(σhcB)(1 + hcB/3kT) within (hcB/kT)^2 (" + f(Q) + " against " + f(want) + ")");
    }
    // the shares add up to 1
    for (const char* id : {"HCl", "H2", "O2", "CO2"}) {
        const rot::Rotor q = rot::rotor(id);
        double s = 0;
        for (int J = 0; J <= rot::partition(q, 500).top; J++) s += rot::share(q, J, 500);
        check(std::fabs(s - 1) < 1e-12, std::string(id) + ": the shares add up to 1");
    }
    // hydrogen: ortho : para -> 3 : 1 when hot, all para when cold
    const rot::Rotor h2 = rot::rotor("H2");
    rot::Rotor h2r = h2; h2r.D[0] = h2r.D[1] = 0;
    const auto orthoPara = [](const rot::Rotor& q, double T) { double o = 0, p = 0; for (int J = 0; J <= rot::partition(q, T).top; J++) (J % 2 ? o : p) += rot::share(q, J, T); return o / p; };
    check(std::fabs(orthoPara(h2r, 3000) - 3) < 0.01, "H2 (rigid) at 3000 K: ortho : para = 3 within 0.01 (" + f(orthoPara(h2r, 3000)) + ")");
    check(std::fabs(orthoPara(h2, 1000) - 3) < 0.03, "H2 at 1000 K: ortho : para = 3 within 1 % (" + f(orthoPara(h2, 1000)) + ")");
    check(rot::share(h2, 0, 0) == 1 && rot::share(h2, 0, 10) > 1 - 1e-6, "H2 at 0 K: all para (J = 0); at 10 K all but 9 exp(-2hcB0/kT) = 3.5e-7");
    check(rot::mostPopulated(h2, 298.15) == 1, "H2 at room temperature: J = 1 holds most (ortho)");
    // the forbidden levels hold exactly nothing
    for (double T : {50.0, 300.0, 1500.0}) {
        bool zero = true;
        for (int J = 0; J < 40; J += 2) zero = zero && rot::share(rot::rotor("O2"), J, T) == 0;
        for (int J = 1; J < 40; J += 2) zero = zero && rot::share(rot::rotor("CO2"), J, T) == 0;
        check(zero, "16O2 even N and CO2 odd J: exactly zero at " + f(T) + " K");
    }
    check(rot::share(rot::rotor("O2"), 1, 0) == 1, "O2 at 0 K: all in N = 1");
    // the one-term centrifugal formula's end: H2's sum stops at J = 25 when hot, and says so
    check(rot::risingTop(h2) == 25, "H2: F rises up to J = 25, where (J+1)^2 passes B0 / 2D = 630 (" + std::to_string(rot::risingTop(h2)) + ")");
    check(rot::partition(h2, 3000).cut, "H2 at 3000 K: the sum is cut at J = 25, and says so (" + f(rot::partition(h2, 3000).lastRel) + ")");
    check(!rot::partition(h2, 500).cut && !rot::partition(rot::rotor("HCl"), 3000).cut, "H2 at 500 K and HCl at 3000 K: not cut (" + f(rot::partition(h2, 500).lastRel) + ", " + f(rot::partition(rot::rotor("HCl"), 3000).lastRel) + ")");
}

static void testIsotopes() {
    // the Born-Oppenheimer rules from H35Cl against the measured D35Cl (NIST WebBook's DCl page, Huber & Herzberg)
    const std::vector<double> x = xRow("molecules/vib/webbook_diatomic/nist_webbook_dcl_constants.extract.txt");
    check(x.size() >= 10, "the DCl page's X row read");
    if (x.size() < 10) return;
    const rot::Rotor d = rot::rotor("HCl", rot::kD35Cl);
    const double mu = rot::massU(1, 1) * rot::massU(17, 35) / (rot::massU(1, 1) + rot::massU(17, 35)), muD = rot::massU(1, 2) * rot::massU(17, 35) / (rot::massU(1, 2) + rot::massU(17, 35));
    check(rel(d.muU, muD, 1e-15) && rel(rot::rotor("HCl").muU, mu, 1e-15), "reduced masses from NIST's atomic masses");
    const double s = mu / muD;
    const rot::Measured* m = rot::measured("HCl");
    const double we = vib::anharmonic("HCl")->we * std::sqrt(s), Be = m->Be * s, al = m->alphaE * std::pow(s, 1.5);
    std::printf("  D35Cl scaled against measured: we %.4f / %.4f (%+.4f %%), Be %.6f / %.6f (%+.4f %%), alpha_e %.6f / %.6f (%+.3f %%), De %.4g / %.4g (%+.2f %%)\n",
                we, x[1], (we / x[1] - 1) * 100, Be, x[4], (Be / x[4] - 1) * 100, al, x[5], (al / x[5] - 1) * 100, m->De * s * s, x[7], (m->De * s * s / x[7] - 1) * 100);
    // 0.54.1: the higher terms scaled too, Y_kl ~ mu^-(k/2 + l): gamma_e (Y_21) and De (Y_02) ~ mu^-2, the (v+1/2)^3 term of B_v (Y_31) and beta_e (Y_12) ~ mu^-5/2, Y_22 ~ mu^-3
    const double g2 = m->bv2 * s * s, g3 = m->bv3 * std::pow(s, 2.5), De = m->De * s * s, d1 = m->dv1 * std::pow(s, 2.5), d2 = m->dv2 * s * s * s;
    bool bo = d.scaled;
    for (int v = 0; v < 2; v++) {
        const double xv = v + 0.5;
        bo = bo && std::fabs(d.B[v] - (Be - al * xv + g2 * xv * xv + g3 * xv * xv * xv)) < 1e-12 && std::fabs(d.D[v] - (De + d1 * xv + d2 * xv * xv)) < 1e-18;
    }
    check(bo, "D35Cl: B0, B1, D0, D1 from the scaled series (marked scaled)");
    // the DCl page has its own measured gamma_e (x[6]): the scaled footnote-29 term against it (the sign and the size of the reading)
    std::printf("  D35Cl gamma_e scaled %.6g / measured %.6g (%+.2f %%)\n", g2, x[6], (g2 / x[6] - 1) * 100);
    check(x[6] > 0 && std::fabs(g2 / x[6] - 1) < 0.03, "D35Cl: H35Cl's footnote-29 gamma_e scaled by mu^-2 within 3 % of DCl's measured " + f(x[6]));
    check(std::fabs(we / x[1] - 1) < 1e-4, "D35Cl: scaled we within 0.01 % of the measured " + f(x[1]));
    check(std::fabs(Be / x[4] - 1) < 1e-4, "D35Cl: scaled Be within 0.01 % of the measured " + f(x[4]));
    check(std::fabs(al / x[5] - 1) < 5e-4, "D35Cl: scaled alpha_e within 0.05 % of the measured " + f(x[5]));
    // the scaled band origin uses the scaled ladder: wexe ~ 1/mu (the measured D35Cl wexe 27.1825: within 0.1 %)
    check(std::fabs(vib::anharmonic("HCl")->wexe * s / x[2] - 1) < 1e-3, "D35Cl: scaled wexe within 0.1 % of the measured " + f(x[2]));
    // H37Cl: a little heavier, so its lines sit a little lower; the natural mix has both, H37Cl about 0.2424 / 0.7576 as strong
    const auto L = rot::lines("HCl", rot::kNatural, rot::Spectrum::Fundamental, 298.15);
    double r35 = 0, r37 = 0, s35 = 0, s37 = 0;
    for (const rot::Line& l : L) if (l.branch == 'R' && l.J == 3) { if (l.iso == rot::kH35Cl) { r35 = l.nu; s35 = l.strength; } else if (l.iso == rot::kH37Cl) { r37 = l.nu; s37 = l.strength; } }
    check(r37 < r35 && r35 - r37 > 1.5 && r35 - r37 < 3, "natural mix: R(3) of H37Cl about 2 cm^-1 below H35Cl's (" + f(r35 - r37) + ")");
    check(std::fabs(s37 / s35 / (rot::abundance(17, 37) / rot::abundance(17, 35)) - 1) < 0.01, "natural mix: H37Cl's R(3) as strong as the abundance ratio, within 1 % (" + f(s37 / s35) + ")");
    double top = 0; for (const rot::Line& l : L) top = std::max(top, l.strength);
    check(top == 1, "the strongest line is 1");
}

static void testMeasuredLines() {
    // Rank, Eastman, Rao & Wiggins 1962, Table III (H35Cl, 1-0, observed). Up to 0.54.0 the lab's lines used only Be, alpha_e and De (worst gap 1.514 cm^-1 at R(15),
    // limit 0.02 + 0.006 m^2). 0.54.1 adds Huber & Herzberg's footnotes 29 and 30 (gamma_e, the (v+1/2)^3 term, D's change with v): worst 0.086 cm^-1 at R(15), the rest
    // odd in m and growing faster than m^2 (the higher terms the page leaves to Rank, Rao et al. 1965). The limit, set from that: 0.01 + 0.0004 m^2 cm^-1.
    std::istringstream in(readFile("molecules/vib/hcl_lines/rank1962_table3.txt"));
    const auto L = rot::lines("HCl", rot::kH35Cl, rot::Spectrum::Fundamental, 298.15, 0);
    int n = 0, same = 0; double worst = 0; bool ok = true;
    for (std::string ln; std::getline(in, ln);) {
        if (ln.empty() || ln[0] == '#') continue;
        std::istringstream w(ln);
        char b; int J; double obs;
        w >> b >> J >> obs;
        same += rot::measuredHClLine(b, J) == obs ? 1 : 0;
        for (const rot::Line& l : L) if (l.branch == b && l.J == J) {
            n++;
            const double d = l.nu - obs, lim = 0.01 + 0.0004 * l.m * l.m;
            worst = std::max(worst, std::fabs(d));
            if (std::fabs(d) > lim) { ok = false; std::printf("  %c(%d): lab %.4f, measured %.4f, %+.4f (limit %.3f)\n", b, J, l.nu, obs, d, lim); }
        }
    }
    std::printf("  HCl 1-0 band: %d measured lines, worst gap %.3f cm^-1\n", n, worst);
    check(n == 31, "the 31 measured lines R(0..15), P(1..15) found (" + std::to_string(n) + ")");
    check(ok, "every measured line within 0.01 + 0.0004 m^2 cm^-1");
    check(worst < 0.1, "the worst gap below 0.1 cm^-1 (0.54.0: 1.514) (" + f(worst) + ")");
    check(same == 31 && rot::measuredHClLines().size() == 31, "the app's copy of the measured lines equals the saved table, row for row");
}

static void testTurning() {
    for (const char* id : {"HCl", "H2", "CO2", "HCN"}) {
        const rot::Rotor r = rot::rotor(id);
        check(rot::omegaRadS(r, 0) == 0 && rot::screenTurnS(r, 0) == 0, std::string(id) + ": J = 0 does not turn");
        // the classical energy L^2 / 2I = I omega^2 / 2 equals the rigid level hc B J(J+1)
        const double I = rot::inertiaKgM2(r), w = rot::omegaRadS(r, 3);
        check(rel(0.5 * I * w * w, kH * kC * 100 * r.B[0] * 12, 1e-12), std::string(id) + ": I omega^2 / 2 = hcB J(J+1) at J = 3");
        check(rel(w, std::sqrt(12.0) * kHbar / I, 1e-12) && rel(rot::screenTurnS(r, 3), 2 * kPi / w * rot::kSlowDown, 1e-12), std::string(id) + ": omega = sqrt(J(J+1)) hbar / I, slowed kSlowDown times");
    }
    check(rel(rot::inertiaKgM2(rot::rotor("HCl")), kH / (8 * kPi * kPi * kC * 100 * rot::rotor("HCl").B[0]), 1e-12), "I = h / (8 pi^2 c B0)");
}

// ---- 0.53.1: the Morse curve (molecule/morse.h) ----
static const double kU = 1.66053906892e-27;   // CODATA 2022
// E = (1/2) mu (dr/dt)^2 + V(r), in cm^-1
static double energyCm(const morse::Curve& c, double rA, double drdtAs) {
    const double v = drdtAs * 1e-10;   // m/s
    return 0.5 * c.muU * kU * v * v / (kH * kC * 100) + morse::V(c, rA);
}
// The period and the time-averaged r by quadrature of dt = dr / |dr/dt| from the energy alone (r = mid - half cos(phi): smooth at the turning points)
static void quadrature(const morse::Curve& c, double E, double rIn, double rOut, double& T, double& rMean) {
    const int n = 200000;
    const double mid = (rIn + rOut) / 2, half = (rOut - rIn) / 2;
    double t = 0, tr = 0;
    for (int i = 0; i < n; i++) {
        const double ph = kPi * (i + 0.5) / n, r = mid - half * std::cos(ph), dr = half * std::sin(ph) * kPi / n;
        const double ke = std::max(0.0, E - morse::V(c, r)) * kH * kC * 100;          // J
        const double sp = std::sqrt(2 * ke / (c.muU * kU)) * 1e10;                     // angstrom/s
        t += dr / sp; tr += r * dr / sp;
    }
    T = 2 * t; rMean = tr / t;
}
static double pageFootnoteNumber(const std::string& file, const std::string& anchor, const std::string& after) {
    const std::string s = readFile(file);
    const std::size_t a = s.find(anchor);
    if (a == std::string::npos) return NAN;
    const std::size_t b = s.find(after, a);
    return b == std::string::npos ? NAN : std::strtod(s.c_str() + b + after.size(), nullptr);
}

static void testMorseLevels() {
    // a made-up curve and the four molecules: the levels in closed form, the last bound level, V at re and far out
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        check(morse::has(id), std::string(id) + ": has a Morse curve");
        const morse::Curve c = morse::curve(id);
        const vib::Anharmonic* a = vib::anharmonic(id);
        check(a && c.we == a->we && c.wexe == a->wexe, std::string(id) + ": the curve uses the lab's measured we and wexe");
        check(rel(c.De, c.we * c.we / (4 * c.wexe), 1e-15), std::string(id) + ": De = we^2 / (4 wexe) (" + f(c.De) + ")");
        // the levels of the curve, from the Schroedinger equation's closed form, against the expansion: exact (rounding only)
        double worst = 0;
        for (int v = 0; v <= morse::vMax(c); v++) {
            const double x = v + 0.5, ref = c.we * x - c.wexe * x * x;
            worst = std::max(worst, std::fabs(morse::level(c, v) - ref) / ref);
        }
        check(worst < 1e-14, std::string(id) + ": Morse levels = we (v+1/2) - wexe (v+1/2)^2 (worst " + f(worst) + ")");
        const int vm = static_cast<int>(std::floor(c.we / (2 * c.wexe) - 0.5));
        check(morse::vMax(c) == vm, std::string(id) + ": v_max = floor(we / 2wexe - 1/2) = " + std::to_string(vm));
        check(morse::level(c, vm) < c.De && morse::level(c, vm) > morse::level(c, vm - 1), std::string(id) + ": the last bound level is below De and still above the one before");
        check(morse::V(c, c.re) == 0, std::string(id) + ": V(re) = 0");
        check(rel(morse::V(c, c.re + 60 / c.a), c.De, 1e-12), std::string(id) + ": V -> De far out");
        check(morse::V(c, c.re - 0.1) > morse::V(c, c.re + 0.1), std::string(id) + ": steeper inside than outside");
        // the curve's harmonic frequency is we: V''(re) = mu omega0^2 with omega0 = 2 pi c we
        const double h = 1e-5, k2 = (morse::V(c, c.re + h) - 2 * morse::V(c, c.re) + morse::V(c, c.re - h)) / (h * h);   // cm^-1 / A^2
        const double k2SI = k2 * kH * kC * 100 * 1e20, w0 = 2 * kPi * kC * 100 * c.we;
        check(rel(k2SI, c.muU * kU * w0 * w0, 1e-5), std::string(id) + ": V''(re) = mu (2 pi c we)^2");
        check(rel(c.muU, rot::rotor(id).muU, 0), std::string(id) + ": the reduced mass of the rotation's (" + f(c.muU) + " u)");
    }
    // the measured levels and where the model starts
    check(morse::measuredTop("H2") == 3 && morse::measuredTop("HCl") == 5 && morse::measuredTop("N2") == 1 && morse::measuredTop("O2") == 1, "measured ladder up to v = 3 (H2), 5 (HCl), 1 (N2, O2)");
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const vib::Anharmonic& a = *vib::anharmonic(id);
        const morse::Curve c = morse::curve(id);
        const int top = morse::measuredTop(id);
        check(morse::isMeasured(id, top) && !morse::isMeasured(id, top + 1), std::string(id) + ": measured up to v = " + std::to_string(top) + ", model above");
        check(morse::levelShown(id, top) == vib::termG(a, top) && morse::levelShown(id, top + 1) == morse::level(c, top + 1), std::string(id) + ": the shown level is the measured G, then the Morse level");
        check(morse::dissociatedE(c) > c.De, std::string(id) + ": above the last bound level the energy is above De");
    }
}

static void testMorseMotion() {
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const morse::Curve c = morse::curve(id);
        double lastMean = 0; bool grows = true; double worstE = 0, worstTurn = 0, worstT = 0, worstMean = 0, worstDeriv = 0;
        const int vm = morse::vMax(c);
        for (int v = 0; v <= vm; v++) {
            const double E = morse::levelShown(id, v);
            const morse::Motion mo = morse::motion(c, E);
            if (!mo.bound) { grows = false; break; }
            worstTurn = std::max({worstTurn, std::fabs(morse::V(c, mo.rIn) - E) / E, std::fabs(morse::V(c, mo.rOut) - E) / E});
            for (int i = 0; i <= 200; i++) {
                const double t = mo.periodS * i / 200.0, r = morse::bondAt(c, E, t), d = morse::speedAt(c, E, t);
                worstE = std::max(worstE, std::fabs(energyCm(c, r, d) - E) / E);
                if (i % 50 == 25) {   // the speed is the derivative of the path (central difference)
                    const double h = mo.periodS * 1e-6, fd = (morse::bondAt(c, E, t + h) - morse::bondAt(c, E, t - h)) / (2 * h);
                    worstDeriv = std::max(worstDeriv, std::fabs(fd - d) / std::max(std::fabs(d), 1e-3 * (mo.rOut - mo.rIn) / mo.periodS));
                }
            }
            if (v == 0 || v == vm / 2 || v == vm) {
                double T, rm; quadrature(c, E, mo.rIn, mo.rOut, T, rm);
                worstT = std::max(worstT, std::fabs(mo.periodS - T) / T);
                worstMean = std::max(worstMean, std::fabs(mo.rMean - rm) / rm);
            }
            check(std::fabs(morse::bondAt(c, E, 0) - mo.rIn) < 1e-12 && std::fabs(morse::bondAt(c, E, mo.periodS / 2) - mo.rOut) < 1e-9 * mo.rOut, std::string(id) + " v " + std::to_string(v) + ": starts at the inner turning point, half a period later the outer");
            if (v > 0 && !(mo.rMean > lastMean)) grows = false;
            lastMean = mo.rMean;
        }
        check(worstE < 1e-9, std::string(id) + ": energy constant along every bound path (worst " + f(worstE) + ")");
        check(worstTurn < 1e-9, std::string(id) + ": the turning points solve V(r) = G(v) (worst " + f(worstTurn) + ")");
        check(worstDeriv < 1e-5, std::string(id) + ": the speed is the path's derivative (worst " + f(worstDeriv) + ")");
        check(worstT < 1e-5 && worstMean < 1e-6, std::string(id) + ": period and mean bond length equal the quadrature of dr / speed (worst " + f(worstT) + ", " + f(worstMean) + ")");
        check(grows, std::string(id) + ": the mean bond length grows with v");
        check(morse::motion(c, morse::level(c, 0)).rMean > c.re, std::string(id) + ": even at v = 0 the mean is beyond re (the wall inside is steeper)");
        // the period tends to 1 / (c we) at small energy
        const double T0 = 1 / (kC * 100 * c.we);
        check(rel(morse::motion(c, 1e-6 * c.De).periodS, T0, 1e-6), std::string(id) + ": at small energy the period is 1 / (c we)");
        check(morse::motion(c, morse::level(c, 0)).periodS > T0 && morse::motion(c, morse::level(c, 5)).periodS > morse::motion(c, morse::level(c, 0)).periodS, std::string(id) + ": higher levels swing slower");
        // above the last bound level: unbound, energy kept, the atoms separate for good
        const double Ed = morse::dissociatedE(c);
        const morse::Motion md = morse::motion(c, Ed);
        check(!md.bound && std::fabs(morse::V(c, md.rIn) - Ed) / Ed < 1e-9, std::string(id) + ": dissociated, from the inner turning point");
        const double Tsc = 1 / (kC * 100 * c.we);
        double worstD = 0, last = 0; bool apart = true;
        for (int i = 0; i <= 100; i++) {
            const double t = Tsc * i / 10.0, r = morse::bondAt(c, Ed, t);
            worstD = std::max(worstD, std::fabs(energyCm(c, r, morse::speedAt(c, Ed, t)) - Ed) / Ed);
            if (i > 0 && !(r > last)) apart = false;
            last = r;
        }
        // far out V -> De, so the speed tends to sqrt(2 (E - De) / mu)
        double tFar = Tsc;
        while (c.a * (morse::bondAt(c, Ed, tFar) - c.re) < 30) tFar *= 2;      // (far: V within 2 exp(-30) of De)
        const double vInf = std::sqrt(2 * (Ed - c.De) * kH * kC * 100 / (c.muU * kU)) * 1e10, vEnd = morse::speedAt(c, Ed, tFar);
        check(worstD < 1e-9 && apart, std::string(id) + ": dissociated: energy kept, the bond only grows (" + f(last) + " A after 10 periods)");
        check(rel(vEnd, vInf, 1e-6) && std::fabs(energyCm(c, morse::bondAt(c, Ed, tFar), vEnd) - Ed) < 1e-9 * Ed, std::string(id) + ": dissociated: far out the atoms fly apart at sqrt(2 (E - De) / mu) = " + f(vInf * 1e-10) + " m/s (has " + f(vEnd * 1e-10) + ")");
    }
    // the harmonic limit (wexe -> 0) gives the lab's harmonic zero-point swing: the bond's change at vib::turningPoint, the wavenumber nuSwing
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const Molecule& mol = get(findId(id));
        const vib::Mode md = vib::modes(id)[0];
        const std::vector<double> d = vib::turningPoint(mol, md);
        double u[3] = {mol.atoms[1].x - mol.atoms[0].x, mol.atoms[1].y - mol.atoms[0].y, mol.atoms[1].z - mol.atoms[0].z};
        const double L = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        double dr = 0; for (int k = 0; k < 3; k++) dr += (d[3 + k] - d[k]) * u[k] / L;
        const double mu = element(mol.atoms[0].el).massU * element(mol.atoms[1].el).massU / (element(mol.atoms[0].el).massU + element(mol.atoms[1].el).massU);
        const morse::Curve c = morse::make(md.nuSwing, md.nuSwing * 1e-9, L, mu);
        const morse::Motion mo = morse::motion(c, morse::level(c, 0));
        check(rel((mo.rOut - mo.rIn) / 2, std::fabs(dr), 1e-6), std::string(id) + ": harmonic limit: half the swing is the lab's harmonic amplitude " + f(std::fabs(dr)) + " A (has " + f((mo.rOut - mo.rIn) / 2) + ")");
    }
}

static void testMorseMeasured() {
    // re from each page's X row
    struct P { const char* id; const char* file; };
    for (const P& p : {P{"H2", "h2"}, P{"N2", "n2"}, P{"O2", "o2"}, P{"HCl", "hcl"}}) {
        const std::vector<double> x = xRow(std::string("molecules/vib/webbook_diatomic/nist_webbook_") + p.file + "_constants.extract.txt");
        check(x.size() >= 10 && morse::reMeasured(p.id) == x[9] && morse::curve(p.id).re == x[9], std::string(p.id) + ": re equals the page's " + (x.size() >= 10 ? f(x[9]) : std::string("?")) + " A");
    }
    // D0: H2 from Huber & Herzberg's footnote 125 ("This is an upper limit (36118.3 ..."), the others from Darwent 1970 (kJ/mol, converted with CODATA's exact constants)
    const double h2 = pageFootnoteNumber("molecules/vib/webbook_diatomic/nist_webbook_h2_constants.extract.txt", "id=\"Dia125\"", "This is an upper limit (");
    check(h2 == 36118.3 && morse::d0Measured("H2") == h2, "H2: D0 equals footnote 125 of the saved page (" + f(h2) + " cm^-1)");
    const double kJ = 1000 / (6.02214076e23 * kH * kC * 100);
    check(rel(morse::kKJmolToCm, kJ, 1e-15) && std::fabs(kJ - 83.5935) < 1e-4, "1 kJ/mol = " + f(kJ) + " cm^-1");
    std::istringstream in(readFile("molecules/vib/dissociation/darwent1970_d0.txt"));
    int rows = 0;
    for (std::string ln; std::getline(in, ln);) {
        if (ln.empty() || ln[0] == '#') continue;
        std::istringstream w(ln); std::string id, bond; double kjmol;
        w >> id >> bond >> kjmol;
        rows++;
        check(rel(morse::d0Measured(id), kjmol * kJ, 1e-12), id + ": D0 equals Darwent 1970's " + f(kjmol) + " kJ/mol = " + f(kjmol * kJ) + " cm^-1 (has " + f(morse::d0Measured(id)) + ")");
    }
    check(rows == 3, "Darwent's file: N2, O2, HCl read (" + std::to_string(rows) + ")");
    check(std::isnan(morse::d0Measured("H2O")) && !morse::has("H2O") && !morse::has("CO2"), "no Morse curve or D0 for the polyatomics");
    // Morse against measured: the D0 gaps printed, the model too deep for all four (a two-constant curve's straight Birge-Sponer line, REPORT)
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const morse::Curve c = morse::curve(id);
        const double dm = morse::d0Morse(c), d0 = morse::d0Measured(id);
        std::printf("  %s: Morse De %.1f, De - G(0) %.1f, measured D0 %.1f cm^-1: %+.1f %%\n", id, c.De, dm, d0, (dm / d0 - 1) * 100);
        check(rel(dm, c.De - (c.we / 2 - c.wexe / 4), 1e-15), std::string(id) + ": the Morse D0 is De - G(0) = De - we/2 + wexe/4");
        check(dm > d0, std::string(id) + ": the Morse curve is deeper than the measured D0");
    }
    // HCl's band origins: the saved table, and the lab's ladder through them (the constants were fitted to these levels)
    std::istringstream bo(readFile("molecules/vib/hcl_lines/rank1962_band_origins.txt"));
    std::vector<double> nu;
    for (std::string ln; std::getline(bo, ln);) { if (ln.empty() || ln[0] == '#') continue; std::istringstream w(ln); int v; double x; w >> v >> x; nu.push_back(x); }
    check(nu.size() == 5 && morse::bandOrigins("HCl") == nu && morse::bandOrigins("N2").empty(), "HCl: the band origins equal Rank et al. 1962's table (" + std::to_string(nu.size()) + ")");
    double worst = 0;
    const vib::Anharmonic& a = *vib::anharmonic("HCl");
    for (std::size_t v = 0; v < nu.size(); v++) worst = std::max(worst, std::fabs(vib::termG(a, static_cast<int>(v) + 1) - vib::termG(a, 0) - nu[v]));
    std::printf("  HCl: the ladder's G(v) - G(0) against Rank's band origins, worst %.3f cm^-1\n", worst);
    check(worst < 0.5, "HCl: the measured ladder passes through Rank's band origins within 0.5 cm^-1 (" + f(worst) + ")");
    // Birge-Sponer: exact for a Morse ladder (area = De - G(0)); HCl from the measured origins, H2 from its ladder's constants; N2 and O2 have one measured spacing
    {
        const morse::Curve c = morse::make(2000, 20, 1.0, 1.0);
        std::vector<double> x, y;
        for (int v = 0; v < 6; v++) { x.push_back(v + 1); y.push_back(morse::level(c, v + 1) - morse::level(c, v)); }
        double sx = 0, sy = 0, sxx = 0, sxy = 0; for (std::size_t i = 0; i < x.size(); i++) { sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; }
        const double n = static_cast<double>(x.size()), B = (n * sxy - sx * sy) / (n * sxx - sx * sx), A = (sy - B * sx) / n, area = (A + B / 2) * (A + B / 2) / (-2 * B);
        check(rel(area, morse::d0Morse(c), 1e-12), "Birge-Sponer: for a Morse ladder the area from v+1 = 1/2 is De - G(0) exactly");
    }
    const morse::BirgeSponer bh = morse::birgeSponer("HCl"), b2 = morse::birgeSponer("H2");
    check(bh.ok && bh.measuredOrigins && bh.x.size() == 5 && rel(bh.dG[0], nu[0], 1e-15) && rel(bh.dG[4], nu[4] - nu[3], 1e-15), "HCl Birge-Sponer: the 5 measured spacings of Rank's origins");
    check(b2.ok && !b2.measuredOrigins && b2.x.size() == 3, "H2 Birge-Sponer: the 3 spacings of the ladder's fitted range");
    check(!morse::birgeSponer("N2").ok && !morse::birgeSponer("O2").ok, "N2, O2: one measured spacing, no line");
    for (const char* id : {"H2", "HCl"}) {
        const morse::BirgeSponer b = morse::birgeSponer(id);
        double resid = 0; for (std::size_t i = 0; i < b.x.size(); i++) resid = std::max(resid, std::fabs(b.intercept + b.slope * b.x[i] - b.dG[i]));
        check(b.slope < 0 && rel(b.xZero, -b.intercept / b.slope, 1e-15) && rel(b.area, (b.intercept + b.slope / 2) * (b.intercept + b.slope / 2) / (-2 * b.slope), 1e-12), std::string(id) + ": Birge-Sponer zero and area of the fitted line");
        std::printf("  %s Birge-Sponer: %.2f %+.4f (v+1), zero at v+1 = %.2f, area %.0f cm^-1 against measured D0 %.0f (%+.1f %%); largest residual %.2f cm^-1\n", id, b.intercept, b.slope, b.xZero, b.area, morse::d0Measured(id), (b.area / morse::d0Measured(id) - 1) * 100, resid);
    }
    // the rotation link: Pekeris' alpha_e against the measured; the limits set from this comparison (REPORT 0.53.1)
    struct L { const char* id; double lim; };
    for (const L& l : {L{"H2", 0.35}, L{"N2", 0.02}, L{"O2", 0.07}, L{"HCl", 0.10}}) {
        const rot::Measured* m = rot::measured(l.id);
        const morse::Curve c = morse::curve(l.id);
        const double Be = m->b0 ? m->Be + m->alphaE / 2 : m->Be;
        check(rel(morse::alphaPekeris(l.id), 6 * std::sqrt(c.wexe * Be * Be * Be) / c.we - 6 * Be * Be / c.we, 1e-15) && morse::alphaMeasured(l.id) == m->alphaE, std::string(l.id) + ": Pekeris alpha_e = 6 sqrt(wexe Be^3)/we - 6 Be^2/we, with the lab's constants");
        const double gap = morse::alphaPekeris(l.id) / morse::alphaMeasured(l.id) - 1;
        std::printf("  %s: Pekeris alpha_e %.5f against measured %.5f cm^-1: %+.1f %%\n", l.id, morse::alphaPekeris(l.id), morse::alphaMeasured(l.id), gap * 100);
        check(std::fabs(gap) < l.lim, std::string(l.id) + ": Pekeris alpha_e within " + f(l.lim * 100) + " % of the measured (" + f(gap * 100) + " %)");
    }
}

// 0.86.0 (Part D): the Bond energy card's Morse gap is computed, not typed. For each molecule with a measured D0: De = we^2 / (4 wexe) and
// G(0) = we / 2 - wexe / 4 written out here from the lab's measured constants (vib::anharmonic), D0(Morse) = De - G(0); the words give exactly that gap
// above the measured D0 in cm^-1 and per cent (as printed: %.0f and %.1f), and say "overestimates" where it is above.
static void testMorseGap() {
    int n = 0;
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const vib::Anharmonic* a = vib::anharmonic(id);
        const double d0 = morse::d0Measured(id);
        if (!a || !std::isfinite(d0)) continue;
        n++;
        const double De = a->we * a->we / (4 * a->wexe), G0 = a->we / 2 - a->wexe / 4, dm = De - G0;
        const morse::MorseGap g = morse::morseGap(id);
        const std::string w = morse::morseGapWords(id);
        char cm[32], pc[32];
        std::snprintf(cm, sizeof cm, "%.0f cm⁻¹", dm - d0); std::snprintf(pc, sizeof pc, "(%.1f %%)", 100 * (dm / d0 - 1));
        check(std::fabs(g.cm - (dm - d0)) < 1e-9 * d0 && std::fabs(g.share - (dm / d0 - 1)) < 1e-12 && g.over == (dm > d0) &&
              w.find(cm) != std::string::npos && w.find(pc) != std::string::npos && (dm > d0) == (w.find("overestimates") != std::string::npos),
              std::string("Morse gap (") + id + "): the card says " + cm + " " + pc + ", the computed D0(Morse) - D0(measured)");
    }
    check(n == 4 && morse::morseGapWords("H2O").empty(), "Morse gap: all four diatomics have a measured D0; none for a molecule without one");
}

// 0.96.0 (outside review of 28 Nov 2026, part D): the spectrum's Absorbance. HITRAN's spectral line intensity (data/molecules/hitran/
// hitran_definitions_2026-11-28.extract.txt): S = I_a A / (8 pi c nu^2) g' exp(-c2 E''/T) (1 - exp(-c2 nu/T)) / Q, with the electric dipole's
// g'A = 64 pi^4 nu^3 g'' R / (3h): S = (8 pi^3 / 3hc) nu [g'' exp(-c2 E''/T) / Q] R (1 - exp(-c2 nu/T)). The lab's Line strength is the lower level's share
// times the Honl-London factor over 2J+1, which is the [..] R part; so the absorbance is the strength times nu (1 - exp(-c2 nu/T)), scaled again so the
// strongest line is 1. References: the factor worked by hand, its two limits (c2 nu << T: c2 nu^2 / T; c2 nu >> T: nu), and HITRAN's c2.
static void testAbsorbance() {
    using namespace molecule::rot;
    std::ifstream f(PL_DATA_DIR "/molecules/hitran/hitran_definitions_2026-11-28.extract.txt");
    std::stringstream ss; ss << f.rdbuf();
    const std::string ex = ss.str();
    const std::size_t at = ex.find("$c_2 = ");
    const double c2Hitran = at == std::string::npos ? 0 : std::atof(ex.c_str() + at + 7);
    check(c2Hitran == 1.4387769, "absorbance: HITRAN's c2 read from the extract (1.4387769 cm K)");
    check(std::fabs(kC2 / c2Hitran - 1) < 1e-7, "absorbance: the lab's c2 = hc/k (CODATA 2022) is HITRAN's 1.4387769 cm K to its 8 digits");
    check(ex.find("\\left( 1 - e^{-c_2\\nu_{ij}/T} \\right)") != std::string::npos && ex.find("\\frac{A_{ji}}{8 \\pi c \\nu_{ij}^2}") != std::string::npos
          && ex.find("g'A_{ji} = \\frac{64\\pi^4}{3h}\\nu_{ij}^3g''") != std::string::npos, "absorbance: the extract holds HITRAN's S_ij and the dipole A (nu^3 / nu^2 = nu)");
    // the factor by hand: nu = 1000 cm^-1 at 300 K: c2 nu / T = 1438.7769 / 300 = 4.795923, exp(-4.795923) = 0.0082634, 1000 (1 - 0.0082634) = 991.7366
    check(std::fabs(absorbanceFactor(1000, 300) - 991.7366) < 1e-3, "absorbance: nu (1 - exp(-c2 nu / T)) at 1000 cm^-1, 300 K is 991.7366 (by hand)");
    check(absorbanceFactor(20, 0) == 20, "absorbance: at T = 0 no level above is held, the factor is nu");
    // two lines: HCl's pure rotation J = 0 -> 1 and J = 5 -> 6 at 296 K; the ratio of absorbances is the ratio of strengths times the ratio of the factors
    const auto find = [](const std::vector<Line>& v, char br, int J) { for (const Line& l : v) if (l.branch == br && l.J == J && l.iso == kH35Cl) return l; return Line{}; };
    const std::vector<Line> s296 = lines("HCl", kH35Cl, Spectrum::PureRotation, 296), a296 = lines("HCl", kH35Cl, Spectrum::PureRotation, 296, 1e-3, Measure::Absorbance);
    const Line s0 = find(s296, 'J', 0), s5 = find(s296, 'J', 5), a0 = find(a296, 'J', 0), a5 = find(a296, 'J', 5);
    const double want = s0.strength / s5.strength * (s0.nu * (1 - std::exp(-1.4387769 * s0.nu / 296))) / (s5.nu * (1 - std::exp(-1.4387769 * s5.nu / 296)));
    check(s0.nu > 0 && s5.nu > 0 && a5.strength > 0 && std::fabs(a0.strength / a5.strength / want - 1) < 1e-6,
          "absorbance: HCl at 296 K, J 0 -> 1 over J 5 -> 6 is the strengths' ratio times nu (1 - exp(-c2 nu / T))'s (" + std::to_string(a0.strength / a5.strength) + ")");
    double topA = 0; for (const Line& l : a296) topA = std::max(topA, l.strength);
    check(std::fabs(topA - 1) < 1e-12, "absorbance: the strongest line is 1");
    const auto strongest = [](const std::vector<Line>& v) { std::size_t k = 0; for (std::size_t i = 0; i < v.size(); i++) if (v[i].strength > v[k].strength) k = i; return v[k].J; };
    check(strongest(a296) > strongest(s296), "absorbance: weighted by nu, the strongest pure-rotation line is at a higher J than the strongest transition (" + std::to_string(strongest(s296)) + " -> " + std::to_string(strongest(a296)) + ")");
    // the limits. High T (c2 nu << T): the factor is c2 nu^2 / T, so the ratio is the strengths' times (nu0 / nu5)^2
    const std::vector<Line> sHot = lines("HCl", kH35Cl, Spectrum::PureRotation, 2e5, 1e-9), aHot = lines("HCl", kH35Cl, Spectrum::PureRotation, 2e5, 1e-9, Measure::Absorbance);
    const Line h0 = find(sHot, 'J', 0), h5 = find(sHot, 'J', 5), g0 = find(aHot, 'J', 0), g5 = find(aHot, 'J', 5);
    check(h5.strength > 0 && std::fabs(g0.strength / g5.strength / (h0.strength / h5.strength * std::pow(h0.nu / h5.nu, 2)) - 1) < 1e-3,
          "absorbance: high temperature (c2 nu / T < 1e-3): the ratio tends to the strengths' times (nu ratio)^2");
    // low T (c2 nu >> T): the factor is nu. HCl's fundamental band at 296 K (c2 nu / T about 14): R(0) over P(1)
    const std::vector<Line> sB = lines("HCl", kH35Cl, Spectrum::Fundamental, 296), aB = lines("HCl", kH35Cl, Spectrum::Fundamental, 296, 1e-3, Measure::Absorbance);
    const Line r0 = find(sB, 'R', 0), p1 = find(sB, 'P', 1), ar0 = find(aB, 'R', 0), ap1 = find(aB, 'P', 1);
    check(p1.strength > 0 && ap1.strength > 0 && std::fabs(ar0.strength / ap1.strength / (r0.strength / p1.strength * r0.nu / p1.nu) - 1) < 2e-6,
          "absorbance: low temperature (c2 nu / T about 14): R(0) over P(1) is the strengths' times the nu ratio");
    // Line strength stays as it was
    const std::vector<Line> sDef = lines("HCl", kH35Cl, Spectrum::PureRotation, 296, 1e-3, Measure::Strength);
    bool same = sDef.size() == s296.size();
    for (std::size_t i = 0; same && i < sDef.size(); i++) same = sDef[i].strength == s296[i].strength && sDef[i].nu == s296[i].nu;
    check(same, "absorbance: Line strength is unchanged (the default)");
}

int main() {
    testAbsorbance();
    testConstants();
    testWeights();
    testLevels();
    testPopulations();
    testIsotopes();
    testMeasuredLines();
    testTurning();
    testMorseLevels();
    testMorseMotion();
    testMorseMeasured();
    testMorseGap();
    std::printf("rotation_tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
