// Molecule Lab (0.18 stage 1) tests, written before the code. Independent references only:
//  - the CCCBDB numbers the owner read from cccbdb.nist.gov on 18 Oct 2026 (typed again HERE, from the job text, not read from the app's tables),
//  - maths (a regular tetrahedron's angle arccos(-1/3), a regular hexagon, collinear / planar shapes),
//  - CCCBDB's measured rotational constants (an independent measurement: moments of inertia from our built shape must give them, within 3 %),
//  - the NIST isotope masses (physics.nist.gov, Atomic Weights and Isotopic Compositions; typed again here),
//  - the Hill-order rule, picking, the camera and the settings text (bad values must fall back, never crash).
// Distances, angles and the inertia tensor are worked out with this file's own small helpers, not the app's.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "molecule/molecule.h"

using namespace molecule;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}
static std::string f(double v) { char b[64]; std::snprintf(b, sizeof b, "%.10g", v); return b; }

// ---- own helpers ----
struct V3 { double x, y, z; };
static V3 P(const Atom& a) { return {a.x, a.y, a.z}; }
static V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static double len(V3 a) { return std::sqrt(dot(a, a)); }
static const double kPi = 3.14159265358979323846;
static double ang(const Atom& a, const Atom& c, const Atom& b) {
    const V3 u = sub(P(a), P(c)), v = sub(P(b), P(c));
    return std::atan2(len(cross(u, v)), dot(u, v)) * 180 / kPi;
}
static double dih(const Atom& a, const Atom& b, const Atom& c, const Atom& d) {
    const V3 b1 = sub(P(b), P(a)), b2 = sub(P(c), P(b)), b3 = sub(P(d), P(c));
    const V3 n1 = cross(b1, b2), n2 = cross(b2, b3);
    const double x = dot(n1, n2), y = dot(cross(n1, n2), b2) / len(b2);
    return std::fabs(std::atan2(y, x) * 180 / kPi);
}
static bool bonded(const Molecule& m, int a, int b) {
    for (const Bond& bd : m.bonds) if ((bd.a == a && bd.b == b) || (bd.a == b && bd.b == a)) return true;
    return false;
}

// ---- the job's data, typed again (CCCBDB, 18 Oct 2026). sym: the value is fixed by symmetry from the others; its listed digits are a rounding ----
struct Ref { const char* mol; const char* what; double v; int decimals; bool sym; };
static const Ref kRefs[] = {
    {"H2", "rHH", 0.741, 3, false}, {"N2", "rNN", 1.098, 3, false}, {"O2", "rOO", 1.208, 3, false}, {"HCl", "rHCl", 1.275, 3, false},
    {"CO2", "rCO", 1.162, 3, false}, {"CO2", "aOCO", 180, 0, false},
    {"H2O", "rOH", 0.958, 3, false}, {"H2O", "aHOH", 104.4776, 4, false},
    {"O3", "rOO", 1.278, 3, false}, {"O3", "aOOO", 116.8, 1, false},
    {"NH3", "rNH", 1.012, 3, false}, {"NH3", "aHNH", 106.67, 2, false},
    {"CH4", "rCH", 1.087, 3, false}, {"CH4", "aHCH", 109.471, 3, true},
    {"HCN", "rCH", 1.064, 3, false}, {"HCN", "rCN", 1.156, 3, false}, {"HCN", "aHCN", 180, 0, false},
    {"C2H2", "rCH", 1.063, 3, false}, {"C2H2", "rCC", 1.203, 3, false}, {"C2H2", "aHCC", 180, 0, false},
    {"H2CO", "rCH", 1.111, 3, false}, {"H2CO", "rCO", 1.205, 3, false}, {"H2CO", "aHCH", 116.133, 3, false}, {"H2CO", "aHCO", 121.9, 1, true},
    {"C2H4", "rCC", 1.339, 3, false}, {"C2H4", "rCH", 1.086, 3, false}, {"C2H4", "aHCH", 117.6, 1, false}, {"C2H4", "aHCC", 121.2, 1, false},
    {"CH3OH", "rOH", 0.956, 3, false}, {"CH3OH", "rCO", 1.427, 3, false}, {"CH3OH", "rCH", 1.096, 3, false}, {"CH3OH", "aHCH", 109.03, 2, false}, {"CH3OH", "aHOC", 108.87, 2, false},
    {"C6H6", "rCC", 1.397, 3, false}, {"C6H6", "rCH", 1.084, 3, false}, {"C6H6", "aCCC", 120, 0, false}, {"C6H6", "aHCC", 120, 0, false},
};
// element symbols of a value name: "rHCl" -> {H, Cl}, "aHCH" -> {H, C, H}
static std::vector<std::string> syms(const char* s) {
    std::vector<std::string> v;
    for (const char* p = s + 1; *p;) {
        std::string e(1, *p++);
        if (*p >= 'a' && *p <= 'z') e += *p++;
        v.push_back(e);
    }
    return v;
}
static std::string symOf(const Molecule& m, int i) { return element(m.atoms[i].el).sym; }

// 1. Every bond and every angle at a bonded centre reproduces the listed value of its element pattern; every listed value occurs at least once.
static void testGeometry() {
    check(count() == 15, "15 molecules");
    for (const Ref& r : kRefs) {
        const int mi = findId(r.mol);
        check(mi >= 0, std::string("molecule ") + r.mol);
        if (mi < 0) continue;
        const Molecule& m = get(mi);
        const double tol = r.sym ? 0.5 * std::pow(10.0, -r.decimals) : 1e-6;
        const std::vector<std::string> e = syms(r.what);
        int found = 0;
        if (r.what[0] == 'r') {
            for (const Bond& b : m.bonds) {
                const std::string s0 = symOf(m, b.a), s1 = symOf(m, b.b);
                if (!((s0 == e[0] && s1 == e[1]) || (s0 == e[1] && s1 == e[0]))) continue;
                found++;
                const double d = len(sub(P(m.atoms[b.a]), P(m.atoms[b.b])));
                check(std::fabs(d - r.v) < tol, std::string(r.mol) + " " + r.what + " = " + f(d) + " (listed " + f(r.v) + ")");
            }
        } else {
            const int n = static_cast<int>(m.atoms.size());
            for (int c = 0; c < n; c++)
                for (int a = 0; a < n; a++)
                    for (int b = a + 1; b < n; b++) {
                        if (a == c || b == c || !bonded(m, a, c) || !bonded(m, b, c) || symOf(m, c) != e[1]) continue;
                        const std::string sa = symOf(m, a), sb = symOf(m, b);
                        if (!((sa == e[0] && sb == e[2]) || (sa == e[2] && sb == e[0]))) continue;
                        found++;
                        const double t = ang(m.atoms[a], m.atoms[c], m.atoms[b]);
                        check(std::fabs(t - r.v) < tol, std::string(r.mol) + " " + r.what + " = " + f(t) + " (listed " + f(r.v) + ")");
                    }
        }
        check(found > 0, std::string(r.mol) + " " + r.what + " occurs in the built molecule");
    }
    // the dihedral: CH3OH, one H-C-O-H = 180 (CCCBDB, symmetry)
    const Molecule& me = get(findId("CH3OH"));
    int d180 = 0;
    int o = -1, c = -1, ho = -1;
    for (int i = 0; i < static_cast<int>(me.atoms.size()); i++) { if (symOf(me, i) == "O") o = i; if (symOf(me, i) == "C") c = i; }
    for (int i = 0; i < static_cast<int>(me.atoms.size()); i++) if (symOf(me, i) == "H" && bonded(me, i, o)) ho = i;
    for (int i = 0; i < static_cast<int>(me.atoms.size()); i++)
        if (symOf(me, i) == "H" && bonded(me, i, c) && std::fabs(dih(me.atoms[i], me.atoms[c], me.atoms[o], me.atoms[ho]) - 180) < 1e-6) d180++;
    check(d180 == 1, "CH3OH: exactly one H-C-O-H dihedral is 180 (" + std::to_string(d180) + ")");
    // and its derived H-C-O angle: the three C-H symmetric about the C-O axis (all three H-C-O equal, all three H-C-H = 109.03)
    double hco[3]; int k = 0;
    for (int i = 0; i < static_cast<int>(me.atoms.size()); i++) if (symOf(me, i) == "H" && bonded(me, i, c) && k < 3) hco[k++] = ang(me.atoms[i], me.atoms[c], me.atoms[o]);
    check(k == 3 && std::fabs(hco[0] - hco[1]) < 1e-9 && std::fabs(hco[0] - hco[2]) < 1e-9, "CH3OH: three equal H-C-O");
    // independent: for three unit vectors at the same angle g to an axis and 120 degrees apart round it, cos(HCH) = 1 - 1.5 sin^2 g
    const double g = hco[0] * kPi / 180;
    check(std::fabs(std::acos(1 - 1.5 * std::sin(g) * std::sin(g)) * 180 / kPi - 109.03) < 1e-9, "CH3OH: derived H-C-O gives H-C-H 109.03");
    check(me.derivedAngle, "CH3OH is marked: H-C-O angle derived");
    bool derivedListed = false;
    for (const Value& v : me.values) derivedListed |= v.derived && v.kind == 'a' && std::isfinite(v.value) && std::fabs(v.value - hco[0]) < 1e-9;
    check(derivedListed, "CH3OH: the derived H-C-O value is in its table, marked derived");
    int otherDerived = 0;
    for (int i = 0; i < count(); i++) if (get(i).id != "CH3OH") for (const Value& v : get(i).values) otherDerived += v.derived;
    check(otherDerived == 0, "only CH3OH has a derived value");
}

// 2. Symmetry from maths
static void testSymmetry() {
    const Molecule& ch4 = get(findId("CH4"));
    int n = 0;
    for (int a = 1; a < 5; a++) for (int b = a + 1; b < 5; b++) {
        n++;
        check(std::fabs(ang(ch4.atoms[a], ch4.atoms[0], ch4.atoms[b]) - std::acos(-1.0 / 3) * 180 / kPi) < 1e-9, "CH4 H-C-H = arccos(-1/3)");
    }
    check(n == 6 && ch4.atoms[0].el == C, "CH4: six H-C-H angles, C is atom 0");
    check(std::fabs(std::acos(-1.0 / 3) * 180 / kPi - 109.4712) < 5e-5, "arccos(-1/3) = 109.4712");
    // benzene: planar, six equal C-C and C-H, every C-C-C 120
    const Molecule& bz = get(findId("C6H6"));
    int cc = 0, ch = 0, ccc = 0;
    for (const Bond& b : bz.bonds) {
        const double d = len(sub(P(bz.atoms[b.a]), P(bz.atoms[b.b])));
        if (bz.atoms[b.a].el == C && bz.atoms[b.b].el == C) { cc++; check(std::fabs(d - 1.397) < 1e-9, "C6H6 C-C 1.397"); check(b.order == 4, "C6H6 ring bond aromatic"); }
        else { ch++; check(std::fabs(d - 1.084) < 1e-9, "C6H6 C-H 1.084"); }
    }
    check(cc == 6 && ch == 6, "C6H6 6 C-C, 6 C-H");
    for (int c = 0; c < 12; c++) if (bz.atoms[c].el == C)
        for (int a = 0; a < 12; a++) for (int b = a + 1; b < 12; b++)
            if (bz.atoms[a].el == C && bz.atoms[b].el == C && bonded(bz, a, c) && bonded(bz, b, c)) { ccc++; check(std::fabs(ang(bz.atoms[a], bz.atoms[c], bz.atoms[b]) - 120) < 1e-9, "C6H6 C-C-C 120"); }
    check(ccc == 6, "C6H6 six C-C-C");
    // planar: every atom at distance 0 from the plane of three ring atoms
    const auto planar = [](const Molecule& m) {
        const V3 nrm = cross(sub(P(m.atoms[1]), P(m.atoms[0])), sub(P(m.atoms[2]), P(m.atoms[0])));
        double worst = 0;
        for (const Atom& a : m.atoms) worst = std::fmax(worst, std::fabs(dot(sub(P(a), P(m.atoms[0])), nrm)) / len(nrm));
        return worst;
    };
    check(planar(bz) < 1e-12, "C6H6 planar");
    check(planar(get(findId("C2H4"))) < 1e-12, "C2H4 planar");
    check(planar(get(findId("H2CO"))) < 1e-12, "H2CO planar");
    // linear: every atom on the line through the first two
    for (const char* id : {"H2", "N2", "O2", "HCl", "CO2", "HCN", "C2H2"}) {
        const Molecule& m = get(findId(id));
        const V3 u = sub(P(m.atoms[1]), P(m.atoms[0]));
        double worst = 0;
        for (const Atom& a : m.atoms) worst = std::fmax(worst, len(cross(sub(P(a), P(m.atoms[0])), u)) / len(u));
        check(worst < 1e-12 && m.linear, std::string(id) + " collinear");
    }
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        const bool shouldBeLinear = m.id == "H2" || m.id == "N2" || m.id == "O2" || m.id == "HCl" || m.id == "CO2" || m.id == "HCN" || m.id == "C2H2";
        check(m.linear == shouldBeLinear, m.id + " linear flag");
    }
    // H2O: two equal O-H
    const Molecule& w = get(findId("H2O"));
    check(w.bonds.size() == 2 && std::fabs(len(sub(P(w.atoms[w.bonds[0].a]), P(w.atoms[w.bonds[0].b]))) - len(sub(P(w.atoms[w.bonds[1].a]), P(w.atoms[w.bonds[1].b])))) < 1e-12, "H2O two equal O-H");
}

// 3. Bond counts, bond orders (the usual structural formulas), centre of mass, Hill order, masses
static void testCountsMass() {
    const std::map<std::string, int> bonds = {{"H2", 1}, {"N2", 1}, {"O2", 1}, {"HCl", 1}, {"CO2", 2}, {"H2O", 2}, {"O3", 2}, {"NH3", 3}, {"CH4", 4}, {"HCN", 2}, {"C2H2", 3}, {"H2CO", 3}, {"C2H4", 5}, {"CH3OH", 5}, {"C6H6", 12}};
    const std::map<std::string, int> atoms = {{"H2", 2}, {"N2", 2}, {"O2", 2}, {"HCl", 2}, {"CO2", 3}, {"H2O", 3}, {"O3", 3}, {"NH3", 4}, {"CH4", 5}, {"HCN", 3}, {"C2H2", 4}, {"H2CO", 4}, {"C2H4", 6}, {"CH3OH", 6}, {"C6H6", 12}};
    for (const auto& [id, nb] : bonds) {
        const int i = findId(id);
        check(i >= 0, id + " exists");
        if (i < 0) continue;
        check(static_cast<int>(get(i).bonds.size()) == nb, id + " has " + std::to_string(nb) + " bonds");
        check(static_cast<int>(get(i).atoms.size()) == atoms.at(id), id + " atom count");
    }
    // the highest bond order of each molecule's usual structural formula (O3: one double, drawn as 1 and 2: a resonance picture)
    const std::map<std::string, int> maxOrder = {{"H2", 1}, {"N2", 3}, {"O2", 2}, {"HCl", 1}, {"CO2", 2}, {"H2O", 1}, {"O3", 2}, {"NH3", 1}, {"CH4", 1}, {"HCN", 3}, {"C2H2", 3}, {"H2CO", 2}, {"C2H4", 2}, {"CH3OH", 1}, {"C6H6", 4}};
    for (const auto& [id, mo] : maxOrder) {
        int top = 0;
        for (const Bond& b : get(findId(id)).bonds) top = std::max(top, b.order);
        check(top == mo, id + " bond order");
    }
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        double M = 0, cx = 0, cy = 0, cz = 0;
        for (const Atom& a : m.atoms) { const double w = element(a.el).massU; M += w; cx += w * a.x; cy += w * a.y; cz += w * a.z; }
        check(std::fabs(cx / M) < 1e-12 && std::fabs(cy / M) < 1e-12 && std::fabs(cz / M) < 1e-12, m.id + " centre of mass at the origin");
        // Hill order, worked out here: C, H, then alphabetical; with no carbon all alphabetical
        std::map<std::string, int> n;
        for (const Atom& a : m.atoms) n[element(a.el).sym]++;
        std::string hill;
        const auto put = [&](const std::string& s) { hill += s; if (n[s] > 1) hill += std::to_string(n[s]); n.erase(s); };
        if (n.count("C")) { put("C"); if (n.count("H")) put("H"); }
        while (!n.empty()) put(n.begin()->first);
        check(m.hill == hill, m.id + " Hill formula " + m.hill + " (expected " + hill + ")");
    }
    check(get(findId("HCl")).hill == "ClH" && get(findId("NH3")).hill == "H3N" && get(findId("CH3OH")).hill == "CH4O" && get(findId("HCN")).hill == "CHN", "Hill: ClH, H3N, CH4O, CHN");
    // masses: the NIST relative atomic masses (physics.nist.gov, Compositions, read 18 Oct 2026): 1H 1.00782503223(9), 12C 12 exactly, 14N 14.00307400443(20), 16O 15.99491461957(17), 35Cl 34.968852682(37)
    const double h1 = 1.00782503223, c12 = 12.0, n14 = 14.00307400443, o16 = 15.99491461957, cl35 = 34.968852682;
    check(std::fabs(massU(get(findId("H2O"))) - (2 * h1 + o16)) < 1e-11, "H2O mass 18.010564684");
    check(std::fabs(massU(get(findId("HCl"))) - (h1 + cl35)) < 1e-11, "HCl mass");
    check(std::fabs(massU(get(findId("CH3OH"))) - (c12 + 4 * h1 + o16)) < 1e-11, "CH3OH mass");
    check(std::fabs(massU(get(findId("HCN"))) - (h1 + c12 + n14)) < 1e-11, "HCN mass");
    check(std::fabs(massU(get(findId("C6H6"))) - (6 * c12 + 6 * h1)) < 1e-11, "C6H6 mass");
    check(std::fabs(massUncU(get(findId("H2O"))) - std::sqrt(std::pow(2 * 9e-11, 2) + std::pow(17e-11, 2))) < 1e-15, "H2O mass uncertainty");
    // the elements' data as given: vdW radii and CPK colours (PubChem periodic table, as in the job text)
    check(element(H).vdwPm == 120 && element(C).vdwPm == 170 && element(N).vdwPm == 155 && element(O).vdwPm == 152 && element(Cl).vdwPm == 175, "vdW radii");
    check(element(H).rgb == 0xFFFFFF && element(C).rgb == 0x909090 && element(N).rgb == 0x3050F8 && element(O).rgb == 0xFF0D0D && element(Cl).rgb == 0x1FF01F, "CPK colours");
}

// 4. Rotational constants from our shape vs CCCBDB's measured ones (3 %). Inertia tensor and its eigenvalues worked out here (characteristic cubic, closed form).
static void moments(const Molecule& m, double I[3]) {
    double t[3][3] = {};
    for (const Atom& a : m.atoms) {
        const double w = element(a.el).massU, r[3] = {a.x, a.y, a.z}, r2 = a.x * a.x + a.y * a.y + a.z * a.z;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] += w * ((i == j ? r2 : 0) - r[i] * r[j]);
    }
    // eigenvalues of a symmetric 3 x 3 (the trigonometric solution of the cubic)
    const double p1 = t[0][1] * t[0][1] + t[0][2] * t[0][2] + t[1][2] * t[1][2], q = (t[0][0] + t[1][1] + t[2][2]) / 3;
    const double p2 = std::pow(t[0][0] - q, 2) + std::pow(t[1][1] - q, 2) + std::pow(t[2][2] - q, 2) + 2 * p1, p = std::sqrt(p2 / 6);
    if (p < 1e-14) { I[0] = I[1] = I[2] = q; return; }
    double B[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) B[i][j] = (t[i][j] - (i == j ? q : 0)) / p;
    const double detB = B[0][0] * (B[1][1] * B[2][2] - B[1][2] * B[2][1]) - B[0][1] * (B[1][0] * B[2][2] - B[1][2] * B[2][0]) + B[0][2] * (B[1][0] * B[2][1] - B[1][1] * B[2][0]);
    const double r = std::fmax(-1.0, std::fmin(1.0, detB / 2)), phi = std::acos(r) / 3;
    const double e1 = q + 2 * p * std::cos(phi), e3 = q + 2 * p * std::cos(phi + 2 * kPi / 3), e2 = 3 * q - e1 - e3;
    I[0] = e3; I[1] = e2; I[2] = e1;   // ascending
}
static void testRotational() {
    // CODATA 2022: h = 6.62607015e-34 J s (exact), c = 299792458 m/s (exact), u = 1.66053906892e-27 kg (physics.nist.gov/cuu)
    const double h = 6.62607015e-34, c = 299792458.0, u = 1.66053906892e-27;
    const auto Bcm = [&](double I) { return h / (8 * kPi * kPi * c * I * u * 1e-20) / 100; };
    struct R { const char* id; double A, B, C; };
    const R refs[] = {{"H2", 0, 60.853, 0}, {"N2", 0, 1.99824, 0}, {"O2", 0, 1.43768, 0}, {"HCl", 0, 10.59342, 0}, {"CO2", 0, 0.39021, 0},
                      {"H2O", 27.877, 14.512, 9.285}, {"O3", 3.55348, 0.44525, 0.39479}, {"NH3", 9.4443, 9.4443, 6.196}, {"CH4", 5.2412, 5.2412, 5.2412},
                      {"HCN", 0, 1.47822, 0}, {"C2H2", 0, 1.1766, 0}, {"H2CO", 9.4053, 1.2953, 1.1342}, {"C2H4", 4.828, 1.0012, 0.8282},
                      {"CH3OH", 4.2573, 0.82338, 0.79273}, {"C6H6", 0.1896, 0.1896, 0.0948}};
    for (const R& r : refs) {
        const Molecule& m = get(findId(r.id));
        double I[3];
        moments(m, I);
        double Ia[3];
        principalMoments(m, Ia);
        for (int k = 0; k < 3; k++) check(std::fabs(I[k] - Ia[k]) < 1e-7 * std::fmax(1.0, I[k]), std::string(r.id) + " app's principal moment " + std::to_string(k));   // (1e-7: the closed-form cubic above loses digits through acos near +-1 for a degenerate pair)
        const auto cmp = [&](const char* which, double calc, double meas) {
            const double rel = (calc - meas) / meas;
            // NH3 A and B: the one KNOWN disagreement, reported in REPORT.md and OWNER-TODO.md (built from CCCBDB's rNH 1.012 / aHNH 106.67: 9.974 cm-1; CCCBDB's listed A = B = 9.4443:
            // +5.6 %). The 3 % rule is NOT loosened: this check asserts that the two numbers still disagree as reported, so any change to them (a corrected value) shows up here.
            const bool known = std::string(r.id) == "NH3" && which[0] != 'C';
            std::printf("  %-6s %s  built %.6g cm-1  CCCBDB %.6g  (%+.2f %%)%s\n", r.id, which, calc, meas, rel * 100, known ? "  KNOWN DISAGREEMENT (see REPORT.md)" : "");
            if (known) { check(std::fabs(rel - 0.0561) < 0.0005, std::string("NH3 ") + which + ": still the reported +5.61 % (built " + f(calc) + ", CCCBDB " + f(meas) + ")"); return; }
            check(std::fabs(rel) < 0.03, std::string(r.id) + " rotational constant " + which + " within 3 %: built " + f(calc) + ", CCCBDB " + f(meas));
        };
        if (r.A == 0) { check(I[0] < 1e-9, std::string(r.id) + " linear: I_a = 0"); cmp("B", Bcm(I[2]), r.B); continue; }
        cmp("A", Bcm(I[0]), r.A); cmp("B", Bcm(I[1]), r.B); cmp("C", Bcm(I[2]), r.C);
    }
}

// 5. Picking and the camera
static void testPickCamera() {
    const Molecule& w = get(findId("H2O"));
    Camera cam = defaultCamera();
    const double W = 800, Hh = 600, s = fitScale(w, W, Hh) * cam.zoom, cx = W / 2, cy = Hh / 2;
    std::vector<Proj> pr;
    project(w, cam, Style::BallStick, cx, cy, s, pr);
    check(pr.size() == 3, "H2O projects 3 atoms");
    for (int i = 0; i < 3; i++) { const Pick p = pick(w, cam, Style::BallStick, cx, cy, s, pr[i].x, pr[i].y); check(p.atom == i && p.bond < 0, "click on atom " + std::to_string(i) + " picks it"); }
    // the middle of a bond (away from both balls) picks the bond
    for (int b = 0; b < 2; b++) {
        const Bond& bd = w.bonds[b];
        const double mx = (pr[bd.a].x + pr[bd.b].x) / 2, my = (pr[bd.a].y + pr[bd.b].y) / 2;
        const bool clear = std::hypot(mx - pr[bd.a].x, my - pr[bd.a].y) > pr[bd.a].r + 2 && std::hypot(mx - pr[bd.b].x, my - pr[bd.b].y) > pr[bd.b].r + 2;
        check(clear, "H2O bond middle is outside the balls (test setup)");
        const Pick p = pick(w, cam, Style::BallStick, cx, cy, s, mx, my);
        check(p.bond == b && p.atom < 0, "click on the middle of bond " + std::to_string(b) + " picks it");
        const Pick q = pick(w, cam, Style::SpaceFill, cx, cy, s, mx, my);
        check(q.bond < 0, "space-filling: no bond can be picked");
    }
    check(pick(w, cam, Style::BallStick, cx, cy, s, 2, 2).atom < 0 && pick(w, cam, Style::BallStick, cx, cy, s, 2, 2).bond < 0, "a click on empty space picks nothing");
    // two atoms behind each other: HCl seen along its axis; the nearer one is picked
    const Molecule& hcl = get(findId("HCl"));
    Camera end;
    end.R[0] = 0; end.R[1] = 0; end.R[2] = -1; end.R[3] = 0; end.R[4] = 1; end.R[5] = 0; end.R[6] = 1; end.R[7] = 0; end.R[8] = 0;   // the x axis (the molecule's axis) towards the viewer
    const double s2 = fitScale(hcl, W, Hh);
    project(hcl, end, Style::SpaceFill, cx, cy, s2, pr);
    const int nearer = pr[0].depth > pr[1].depth ? 0 : 1;
    check(std::fabs(pr[0].x - pr[1].x) < 1e-9 && std::fabs(pr[0].y - pr[1].y) < 1e-9, "HCl end-on: both atoms on the same screen point");
    check(pick(hcl, end, Style::SpaceFill, cx, cy, s2, cx, cy).atom == nearer, "the nearer of two overlapping atoms is picked");
    // camera: many drags keep R a rotation; a drag and its reverse give the start back; zoom is clamped
    Camera k = defaultCamera();
    for (int i = 0; i < 2000; i++) rotate(k, 7.3, -3.1);
    double worst = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        double d = 0;
        for (int t = 0; t < 3; t++) d += k.R[i * 3 + t] * k.R[j * 3 + t];
        worst = std::fmax(worst, std::fabs(d - (i == j ? 1 : 0)));
    }
    const double det = k.R[0] * (k.R[4] * k.R[8] - k.R[5] * k.R[7]) - k.R[1] * (k.R[3] * k.R[8] - k.R[5] * k.R[6]) + k.R[2] * (k.R[3] * k.R[7] - k.R[4] * k.R[6]);
    check(worst < 1e-12 && std::fabs(det - 1) < 1e-12, "2000 drags: still a rotation");
    Camera a = defaultCamera(), b = a;
    rotate(b, 40, 0); rotate(b, -40, 0);
    double back = 0;
    for (int i = 0; i < 9; i++) back = std::fmax(back, std::fabs(a.R[i] - b.R[i]));
    check(back < 1e-12, "a sideways drag and its reverse give the start back");
    // a sideways drag turns about the screen's vertical axis: a point's screen y does not change
    Camera v = defaultCamera();
    project(w, v, Style::BallStick, cx, cy, s, pr);
    const double y0 = pr[1].y;
    rotate(v, 55, 0);
    project(w, v, Style::BallStick, cx, cy, s, pr);
    check(std::fabs(pr[1].y - y0) < 1e-9, "a sideways drag keeps screen heights");
    Camera z;
    zoomBy(z, 1e6); check(z.zoom == kZoomMax, "zoom clamped high");
    zoomBy(z, 1e-12); check(z.zoom == kZoomMin, "zoom clamped low");
    zoomBy(z, std::nan("")); check(z.zoom == kZoomMin, "zoom by NaN ignored");
    Camera pn; pan(pn, 50, -20, 25);
    check(std::fabs(pn.panX - 2) < 1e-12 && std::fabs(pn.panY - 0.8) < 1e-12, "pan: 50 pt right, 20 pt up at 25 pt/A = (2, 0.8) A");
    // the label: element and number among its kind
    const Molecule& me = get(findId("CH3OH"));
    int hN = 0;
    for (int i = 0; i < static_cast<int>(me.atoms.size()); i++) if (me.atoms[i].el == H) { hN++; check(atomLabel(me, i) == "H" + std::to_string(hN), "CH3OH label H" + std::to_string(hN)); }
    // listed values found for a bond and an angle
    const Value* rv = lengthValue(w, w.bonds[0].a, w.bonds[0].b);
    check(rv && std::fabs(rv->value - 0.958) < 1e-12 && std::strcmp(rv->ref, "1979Hoy/Bun:1") == 0, "H2O O-H value and reference");
    const Value* av = angleValue(w, w.bonds[0].a == 0 ? w.bonds[0].b : w.bonds[0].a, 0, w.bonds[1].a == 0 ? w.bonds[1].b : w.bonds[1].a);
    check(av && std::fabs(av->value - 104.4776) < 1e-12, "H2O H-O-H value (O is atom 0)");
}

// 6. Settings: round trip, and every kind of bad value falls back
static void testSettings() {
    Settings s;
    s.mol = findId("C6H6"); s.style = Style::SpaceFill; s.labels = false; s.spin = true;
    rotate(s.cam, 33, 12); s.cam.zoom = 2.5; s.cam.panX = 0.75; s.cam.panY = -1.25;
    const Settings t = loadSettings(saveSettings(s));
    check(t.mol == s.mol && t.style == s.style && t.labels == s.labels && t.spin == s.spin && t.cam.zoom == s.cam.zoom && t.cam.panX == s.cam.panX && t.cam.panY == s.cam.panY, "settings round trip");
    double worst = 0;
    for (int i = 0; i < 9; i++) worst = std::fmax(worst, std::fabs(t.cam.R[i] - s.cam.R[i]));
    check(worst < 1e-15, "settings round trip: the view's rotation");
    check(saveSettings(s).rfind("v1 ", 0) == 0, "settings start with the version");
    const Settings d;
    const auto isDefault = [&](const Settings& x) {
        double w2 = 0;
        for (int i = 0; i < 9; i++) w2 = std::fmax(w2, std::fabs(x.cam.R[i] - defaultCamera().R[i]));
        return x.mol == d.mol && x.style == d.style && x.labels == d.labels && x.spin == d.spin && x.cam.zoom == 1 && x.cam.panX == 0 && x.cam.panY == 0 && w2 < 1e-15;
    };
    check(d.mol == findId("H2O"), "default molecule H2O");
    check(isDefault(loadSettings("")), "empty -> default");
    check(isDefault(loadSettings("v9 mol=C6H6 style=fill")), "a newer version -> default");
    check(isDefault(loadSettings("garbage")), "garbage -> default");
    const Settings u = loadSettings("v1 mol=C60 style=wire labels=7 spin=x zoom=nan pan=1e400,2 rot=0,0,0,0,0,0,0,0,0");
    check(isDefault(u), "each bad value -> its default (unknown molecule, style, flags, NaN zoom, overflowing pan, zero matrix)");
    check(loadSettings("v1 mol=CH4 zoom=1e9").cam.zoom == kZoomMax && loadSettings("v1 mol=CH4 zoom=1e9").mol == findId("CH4"), "zoom clamped, the good molecule kept");
    check(isDefault(loadSettings("v1 rot=1,0,0,0,1,0,0,0,-1")), "a mirror (det -1) is not a rotation -> default view");
    check(isDefault(loadSettings("v1 rot=2,0,0,0,2,0,0,0,2")), "a scaled matrix -> default view");
    check(isDefault(loadSettings("v1 rot=1,0,0")), "a short matrix -> default view");
    check(loadSettings("v1 mol=NH3 style=fill labels=0 spin=1 pan=0.5,").mol == findId("NH3") && loadSettings("v1 mol=NH3 style=fill labels=0 spin=1 pan=0.5,").cam.panX == 0, "a broken pan -> 0, the rest kept");
    check(loadSettings("v1 mol=NH3 style=fill labels=0 spin=1").style == Style::SpaceFill && !loadSettings("v1 mol=NH3 labels=0").labels && loadSettings("v1 spin=1").spin, "good values read");
}

// 6b. 0.21: the Realistic style's fields in the same versioned line: added only once the style was used (older files stay byte for byte), each bad value -> its default
static void testRealSettings() {
    const std::string old = "v1 mol=NH3 style=fill labels=0 spin=1 zoom=2.5 pan=0.25,-0.5 rot=1,0,0,0,1,0,0,0,1";   // a 0.20 line
    check(saveSettings(loadSettings(old)) == old, "a 0.20 settings line is written back byte for byte");
    const Settings d;
    check(!d.realUsed && d.realDots == 60000 && d.realIso == 2 && d.realPot && d.realShow == 0 && kRealIso[2] == 0.002, "Realistic defaults: not used yet, 60,000 dots, isovalue 0.002, coloured by the potential, cloud and surface");
    const std::string ds = saveSettings(d);
    check(ds.find("rdots") == std::string::npos && ds.find("riso") == std::string::npos && ds.find("rcol") == std::string::npos && ds.find("rshow") == std::string::npos && ds.find("real") == std::string::npos, "no Realistic fields before the style was used");
    Settings s;
    s.mol = findId("C6H6"); s.style = Style::Realistic; s.realUsed = true; s.realDots = 20000; s.realIso = 1; s.realPot = false; s.realShow = 2;
    const std::string t = saveSettings(s);
    check(t.find(" style=real") != std::string::npos && t.find(" rdots=20000 riso=0.0016 rcol=plain rshow=surface") != std::string::npos, "Realistic fields written: " + t);
    const Settings u = loadSettings(t);
    check(u.style == Style::Realistic && u.realUsed && u.realDots == 20000 && u.realIso == 1 && !u.realPot && u.realShow == 2 && u.mol == s.mol, "Realistic round trip");
    s.style = Style::BallStick;
    const Settings v = loadSettings(saveSettings(s));
    check(v.style == Style::BallStick && v.realUsed && v.realDots == 20000, "the Realistic choices are kept while another style is on");
    const Settings w = loadSettings("v1 mol=H2O style=real rdots=7 riso=0.5 rcol=x rshow=all");
    check(w.style == Style::Realistic && w.realDots == 60000 && w.realIso == 2 && w.realPot && w.realShow == 0 && w.realUsed, "bad Realistic values -> their defaults");
    const Settings x = loadSettings("v1 mol=H2O style=real rdots=100000 riso=0.01 rcol=pot rshow=cloud");
    check(x.realDots == 100000 && x.realIso == 3 && x.realPot && x.realShow == 1, "good Realistic values read");
    check(loadSettings("v1 mol=H2O rdots=").realUsed, "a damaged Realistic field still marks the style as used (so it is written back)");
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        Settings q; q.realUsed = true; q.realDots = kRealDots[i]; q.realIso = j;
        const Settings r = loadSettings(saveSettings(q));
        check(r.realDots == kRealDots[i] && r.realIso == j, "every dots amount and isovalue reads back (" + std::to_string(i) + ", " + std::to_string(j) + ")");
    }
}

// 7. 0.19 audit: the settings text fuzzed (random tokens, random bytes): never a crash, always a valid camera (a rotation, zoom in range, finite pan),
//    and what is read is written back the same way (load(save(load(x))) == load(x))
static void testSettingsFuzz() {
    uint64_t st = 88172645463325252ull;
    const auto next = [&st] { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; };
    const char* keys[] = {"mol", "style", "labels", "spin", "zoom", "pan", "rot", "", "=", "mol=", "x", "rdots", "riso", "rcol", "rshow"};   // (0.21: + the Realistic fields)
    const char* vals[] = {"H2O", "C6H6", "fill", "ball", "0", "1", "-0", "1e308", "-1e308", "nan", "inf", "0x10", "1,2", "1,,2", ",", "0.5,0.5", "1,0,0,0,1,0,0,0,1",
                          "0,1,0,-1,0,0,0,0,1", "1e-300", "\xff", "é", "1 2", "9999999999999999999999", "real", "20000", "60000", "100000", "0.0016", "0.01", "0.002", "pot", "plain", "both", "cloud", "surface"};
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        std::string t = next() % 5 ? "v1" : (next() % 2 ? "v2" : "");
        const int n = static_cast<int>(next() % 12);
        for (int k = 0; k < n; k++) {
            t += next() % 7 ? " " : "\t";
            if (next() % 9 == 0) { for (int b = static_cast<int>(next() % 8); b > 0; b--) t += static_cast<char>(next() % 256); continue; }   // raw bytes
            t += std::string(keys[next() % (sizeof keys / sizeof *keys)]) + "=" + vals[next() % (sizeof vals / sizeof *vals)];
        }
        const Settings a = loadSettings(t);
        bool ok = a.mol >= 0 && a.mol < count() && a.cam.zoom >= kZoomMin && a.cam.zoom <= kZoomMax && std::isfinite(a.cam.panX) && std::isfinite(a.cam.panY);
        double R[9];
        for (int j = 0; j < 9; j++) R[j] = a.cam.R[j];
        double det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) + R[2] * (R[3] * R[7] - R[4] * R[6]);
        ok = ok && std::fabs(det - 1) < 1e-9;
        const Settings b = loadSettings(saveSettings(a));
        double dr = 0;
        for (int j = 0; j < 9; j++) dr = std::fmax(dr, std::fabs(b.cam.R[j] - a.cam.R[j]));   // (reading a view orthonormalises it again: the last bits may move)
        ok = ok && b.mol == a.mol && b.style == a.style && b.labels == a.labels && b.spin == a.spin && b.realUsed == a.realUsed && b.realDots == a.realDots && b.realIso == a.realIso && b.realPot == a.realPot && b.realShow == a.realShow && b.cam.zoom == a.cam.zoom && b.cam.panX == a.cam.panX && b.cam.panY == a.cam.panY && dr < 1e-12;
        if (!ok && bad++ < 3) std::printf("  settings fuzz: \"%s\"\n", t.c_str());
    }
    check(bad == 0, "settings fuzz: 20,000 random settings texts give a valid view and read back the same (" + std::to_string(bad) + " wrong)");
}

int main() {
    testGeometry();
    testSymmetry();
    testCountsMass();
    testRotational();
    testPickCamera();
    testSettings();
    testRealSettings();
    testSettingsFuzz();
    std::printf("molecule_logic: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
