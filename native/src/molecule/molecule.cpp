// Molecule Lab (0.18 stage 1): the data, the 3D shapes built from the listed internal coordinates, the camera, picking and the settings text. See molecule.h for the sources.
#include "molecule/molecule.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "atom/atom_core.h"

namespace molecule {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180;

// NIST "Atomic Weights and Isotopic Compositions" (physics.nist.gov/cgi-bin/Compositions/stand_alone.pl?ele=<X>&ascii=ascii2&isotype=some, read 18 Oct 2026, pages saved in
// native/data/molecules/): the most abundant isotope's "Relative Atomic Mass" (its uncertainty from the digits in brackets) and "Isotopic Composition".
// van der Waals radii (pm) and CPK colours: PubChem periodic table (as given to the job; drawing conventions).
const Element kEl[kElCount] = {
    {"H", "Hydrogen", 1, 1, 1.00782503223, 9e-11, 0.999885, 120, 0xFFFFFF},      // 1.00782503223(9), 0.999885(70)
    {"C", "Carbon", 6, 12, 12.0, 0.0, 0.9893, 170, 0x909090},                  // 12.0000000(00) (exact by definition of u), 0.9893(8)
    {"N", "Nitrogen", 7, 14, 14.00307400443, 2.0e-10, 0.99636, 155, 0x3050F8},   // 14.00307400443(20), 0.99636(20)
    {"O", "Oxygen", 8, 16, 15.99491461957, 1.7e-10, 0.99757, 152, 0xFF0D0D},     // 15.99491461957(17), 0.99757(16)
    {"Cl", "Chlorine", 17, 35, 34.968852682, 3.7e-8, 0.7576, 175, 0x1FF01F},     // 34.968852682(37), 0.7576(10)
};

Value len(int a, int b, double v, const char* ref, const char* note) { return Value{'r', a, b, -1, v, ref, note, 3, false, false}; }
Value angle(int a, int c, int b, double v, const char* ref, const char* note, int decimals, bool symmetry = false) { return Value{'a', a, c, b, v, ref, note, decimals, symmetry, false}; }

double find(const std::vector<Value>& vs, char kind, int e0, int e1, int e2 = -1) {
    for (const Value& v : vs)
        if (v.kind == kind && ((v.e0 == e0 && v.e1 == e1 && v.e2 == e2) || (v.e0 == (e2 < 0 ? e1 : e2) && v.e1 == (e2 < 0 ? e0 : e1) && v.e2 == (e2 < 0 ? -1 : e0)))) return v.value;
    std::abort();                                        // a builder asked for a value its table does not have: a bug in this file
}

// Jacobi eigen decomposition of a symmetric 3 x 3 (a: overwritten; vecs: columns are the eigenvectors)
void jacobi(double a[3][3], double vecs[3][3]) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) vecs[i][j] = i == j;
    for (int sweep = 0; sweep < 60; sweep++) {
        const double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-30) break;
        for (int p = 0; p < 2; p++)
            for (int q = p + 1; q < 3; q++) {
                if (std::fabs(a[p][q]) < 1e-300) continue;
                const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const double t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1)), c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < 3; k++) { const double akp = a[k][p], akq = a[k][q]; a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq; }
                for (int k = 0; k < 3; k++) { const double apk = a[p][k], aqk = a[q][k]; a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk; }
                for (int k = 0; k < 3; k++) { const double vkp = vecs[k][p], vkq = vecs[k][q]; vecs[k][p] = c * vkp - s * vkq; vecs[k][q] = s * vkp + c * vkq; }
            }
    }
}
void tensor(const Molecule& m, double t[3][3]) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] = 0;
    for (const Atom& a : m.atoms) {
        const double w = kEl[a.el].massU, r[3] = {a.x, a.y, a.z}, r2 = a.x * a.x + a.y * a.y + a.z * a.z;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] += w * ((i == j ? r2 : 0) - r[i] * r[j]);
    }
}

// the centre of mass to the origin, then the principal axes along x, y, z (smallest moment first), as a proper rotation
void place(Molecule& m) {
    double M = 0, c[3] = {0, 0, 0};
    for (const Atom& a : m.atoms) { const double w = kEl[a.el].massU; M += w; c[0] += w * a.x; c[1] += w * a.y; c[2] += w * a.z; }
    for (Atom& a : m.atoms) { a.x -= c[0] / M; a.y -= c[1] / M; a.z -= c[2] / M; }
    double t[3][3], v[3][3];
    tensor(m, t);
    jacobi(t, v);
    int ord[3] = {0, 1, 2};
    std::sort(ord, ord + 3, [&](int i, int j) { return t[i][i] < t[j][j] - 1e-12 || (std::fabs(t[i][i] - t[j][j]) <= 1e-12 && i < j); });
    double E[3][3];                                       // rows: the new axes
    for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) E[r][k] = v[k][ord[r]];
    const double det = E[0][0] * (E[1][1] * E[2][2] - E[1][2] * E[2][1]) - E[0][1] * (E[1][0] * E[2][2] - E[1][2] * E[2][0]) + E[0][2] * (E[1][0] * E[2][1] - E[1][1] * E[2][0]);
    if (det < 0) for (int k = 0; k < 3; k++) E[2][k] = -E[2][k];
    for (Atom& a : m.atoms) {
        const double p[3] = {a.x, a.y, a.z};
        a.x = E[0][0] * p[0] + E[0][1] * p[1] + E[0][2] * p[2];
        a.y = E[1][0] * p[0] + E[1][1] * p[1] + E[1][2] * p[2];
        a.z = E[2][0] * p[0] + E[2][1] * p[1] + E[2][2] * p[2];
    }
}

// The molecules. Every number is CCCBDB's (Release 22, experimental geometry / rotational constants pages, read by the owner on 18 Oct 2026; reference code after it).
// Names: the usual (IUPAC-accepted) names; not checked against CCCBDB's own page titles (not reachable from the cloud session).
std::vector<Molecule> buildAll() {
    std::vector<Molecule> all;
    const auto add = [&](Molecule m) { place(m); all.push_back(std::move(m)); };
    const auto diatomic = [&](const char* id, const char* formula, const char* hill, const char* name, int e0, int e1, int order, double r, const char* ref, const char* note,
                              double B, const char* bref) {
        Molecule m;
        m.id = id; m.formula = formula; m.hill = hill; m.name = name; m.linear = true;
        m.values = {len(e0, e1, r, ref, note)};
        m.atoms = {{e0, -r / 2, 0, 0}, {e1, r / 2, 0, 0}};
        m.bonds = {{0, 1, order}};
        m.rotB = B; m.rotRef = bref;
        m.shapeNote = "A diatomic molecule: the one bond length fixes the shape.";
        add(m);
    };
    diatomic("H2", "H2", "H2", "Hydrogen", H, H, 1, 0.741, "1979HUB/HER", "re", 60.853, "1979HUB/HER");
    diatomic("N2", "N2", "N2", "Nitrogen", N, N, 3, 1.098, "1979HUB/HER", "re", 1.99824, "1979HUB/HER");
    diatomic("O2", "O2", "O2", "Oxygen", O, O, 2, 1.208, "1979HUB/HER", "re", 1.43768, "1979HUB/HER");
    diatomic("HCl", "HCl", "ClH", "Hydrogen chloride", H, Cl, 1, 1.275, "NISTdiatomic", "re 1H35Cl", 10.59342, "1979HUB/HER");
    {   // CO2: linear, O=C=O
        Molecule m;
        m.id = "CO2"; m.formula = "CO2"; m.hill = "CO2"; m.name = "Carbon dioxide"; m.linear = true;
        m.values = {len(C, O, 1.162, "1966Herzberg", ""), angle(O, C, O, 180, "1966Herzberg", "", 0)};
        const double r = find(m.values, 'r', C, O);
        m.atoms = {{C, 0, 0, 0}, {O, -r, 0, 0}, {O, r, 0, 0}};
        m.bonds = {{0, 1, 2}, {0, 2, 2}};
        m.rotB = 0.39021; m.rotRef = "1966Herzberg";
        m.shapeNote = "Linear (O-C-O 180 degrees); two equal C-O bonds.";
        add(m);
    }
    const auto bent = [&](const char* id, const char* formula, const char* hill, const char* name, int ce, int ee, Value r, Value a, int order0, int order1, double A, double B, double C2,
                          const char* shape) {
        Molecule m;
        m.id = id; m.formula = formula; m.hill = hill; m.name = name; m.planar = true;
        m.values = {r, a};
        const double d = r.value, h = a.value * kRad / 2;
        m.atoms = {{ce, 0, 0, 0}, {ee, -d * std::sin(h), d * std::cos(h), 0}, {ee, d * std::sin(h), d * std::cos(h), 0}};
        m.bonds = {{0, 1, order0}, {0, 2, order1}};
        m.rotA = A; m.rotB = B; m.rotC = C2; m.rotRef = "1966Herzberg";
        m.shapeNote = shape;
        add(m);
    };
    bent("H2O", "H2O", "H2O", "Water", O, H, len(O, H, 0.958, "1979Hoy/Bun:1", "re"), angle(H, O, H, 104.4776, "1979Hoy/Bun:1", "equilibrium", 4), 1, 1, 27.877, 14.512, 9.285,
         "Two equal O-H bonds and the H-O-H angle fix the shape (C2v).");
    bent("O3", "O3", "O3", "Ozone", O, O, len(O, O, 1.278, "1966Herzberg", ""), angle(O, O, O, 116.8, "1966Herzberg", "", 1), 2, 1, 3.55348, 0.44525, 0.39479,
         "Two equal O-O bonds and the O-O-O angle fix the shape (C2v). Drawn as one double and one single bond: one of the two resonance structures (a convention).");
    {   // NH3: three equal N-H, three equal H-N-H (C3v)
        Molecule m;
        m.id = "NH3"; m.formula = "NH3"; m.hill = "H3N"; m.name = "Ammonia";
        m.values = {len(N, H, 1.012, "1966Herzberg", ""), angle(H, N, H, 106.67, "1966Herzberg", "", 2)};
        const double r = find(m.values, 'r', N, H), a = find(m.values, 'a', H, N, H) * kRad;
        const double s2 = (1 - std::cos(a)) / 1.5, sg = std::sqrt(s2), cg = std::sqrt(1 - s2);   // for three bonds at angle g to the axis, 120 degrees apart: cos(HNH) = 1 - 1.5 sin^2 g
        m.atoms = {{N, 0, 0, 0}};
        for (int k = 0; k < 3; k++) m.atoms.push_back({H, r * sg * std::cos(2 * kPi * k / 3), r * sg * std::sin(2 * kPi * k / 3), -r * cg});
        m.bonds = {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}};
        m.rotA = 9.4443; m.rotB = 9.4443; m.rotC = 6.196; m.rotRef = "1966Herzberg";
        m.shapeNote = "Assumed: three equivalent N-H bonds (symmetric top, C3v); one N-H length and one H-N-H angle then fix the pyramid.";
        add(m);
    }
    {   // CH4: tetrahedral (Td)
        Molecule m;
        m.id = "CH4"; m.formula = "CH4"; m.hill = "CH4"; m.name = "Methane";
        m.values = {len(C, H, 1.087, "1979Hir:213", "re"), angle(H, C, H, 109.471, "1974sve/kov", "", 3, true)};
        const double q = find(m.values, 'r', C, H) / std::sqrt(3.0);
        m.atoms = {{C, 0, 0, 0}, {H, q, q, q}, {H, q, -q, -q}, {H, -q, q, -q}, {H, -q, -q, q}};
        m.bonds = {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}};
        m.rotA = m.rotB = m.rotC = 5.2412; m.rotRef = "1966Herzberg";
        m.shapeNote = "Assumed: a regular tetrahedron (Td): four equal C-H bonds, every H-C-H = arccos(-1/3) = 109.4712 degrees (CCCBDB's 109.471 is this value rounded).";
        add(m);
    }
    {   // HCN: linear H-C#N
        Molecule m;
        m.id = "HCN"; m.formula = "HCN"; m.hill = "CHN"; m.name = "Hydrogen cyanide"; m.linear = true;
        m.values = {len(C, H, 1.064, "1966Herzberg", ""), len(C, N, 1.156, "1966Herzberg", ""), angle(H, C, N, 180, "1966Herzberg", "", 0)};
        m.atoms = {{H, -find(m.values, 'r', C, H), 0, 0}, {C, 0, 0, 0}, {N, find(m.values, 'r', C, N), 0, 0}};
        m.bonds = {{0, 1, 1}, {1, 2, 3}};
        m.rotB = 1.47822; m.rotRef = "1966Herzberg";
        m.shapeNote = "Linear (H-C-N 180 degrees); the two bond lengths fix the shape.";
        add(m);
    }
    {   // C2H2: linear H-C#C-H
        Molecule m;
        m.id = "C2H2"; m.formula = "C2H2"; m.hill = "C2H2"; m.name = "Acetylene (ethyne)"; m.linear = true;
        m.values = {len(C, H, 1.063, "1998Kuc", "rm structure"), len(C, C, 1.203, "1998Kuc", "rm structure"), angle(H, C, C, 180, "1966Herzberg", "", 0)};
        const double c = find(m.values, 'r', C, C) / 2, h = find(m.values, 'r', C, H);
        m.atoms = {{H, -c - h, 0, 0}, {C, -c, 0, 0}, {C, c, 0, 0}, {H, c + h, 0, 0}};
        m.bonds = {{0, 1, 1}, {1, 2, 3}, {2, 3, 1}};
        m.rotB = 1.1766; m.rotRef = "1966Herzberg";
        m.shapeNote = "Linear (H-C-C 180 degrees), two equal C-H bonds.";
        add(m);
    }
    {   // H2CO: planar, C2v
        Molecule m;
        m.id = "H2CO"; m.formula = "H2CO"; m.hill = "CH2O"; m.name = "Formaldehyde (methanal)"; m.planar = true;
        m.values = {len(C, H, 1.111, "Gurvich", ""), len(C, O, 1.205, "Gurvich", ""), angle(H, C, H, 116.133, "Gurvich", "", 3),
                    angle(H, C, O, 121.9, "Gurvich", "from aHCH by symmetry", 1, true)};
        const double h = find(m.values, 'r', C, H), o = find(m.values, 'r', C, O), a = find(m.values, 'a', H, C, H) * kRad / 2;
        m.atoms = {{C, 0, 0, 0}, {O, 0, o, 0}, {H, -h * std::sin(a), -h * std::cos(a), 0}, {H, h * std::sin(a), -h * std::cos(a), 0}};
        m.bonds = {{0, 1, 2}, {0, 2, 1}, {0, 3, 1}};
        m.rotA = 9.4053; m.rotB = 1.2953; m.rotC = 1.1342; m.rotRef = "1966Herzberg";
        m.shapeNote = "Planar, C2v: the C=O axis bisects H-C-H, so H-C-O = 180 - (H-C-H)/2 (CCCBDB's 121.9 is this rounded).";
        add(m);
    }
    {   // C2H4: planar, D2h
        Molecule m;
        m.id = "C2H4"; m.formula = "C2H4"; m.hill = "C2H4"; m.name = "Ethylene (ethene)"; m.planar = true;
        m.values = {len(C, C, 1.339, "1966Herzberg", ""), len(C, H, 1.086, "1966Herzberg", ""), angle(H, C, H, 117.6, "1966Herzberg", "", 1), angle(H, C, C, 121.2, "1966Herzberg", "", 1)};
        const double c = find(m.values, 'r', C, C) / 2, h = find(m.values, 'r', C, H), b = (180 - find(m.values, 'a', H, C, C)) * kRad;
        m.atoms = {{C, -c, 0, 0}, {C, c, 0, 0}, {H, -c - h * std::cos(b), h * std::sin(b), 0}, {H, -c - h * std::cos(b), -h * std::sin(b), 0},
                   {H, c + h * std::cos(b), h * std::sin(b), 0}, {H, c + h * std::cos(b), -h * std::sin(b), 0}};
        m.bonds = {{0, 1, 2}, {0, 2, 1}, {0, 3, 1}, {1, 4, 1}, {1, 5, 1}};
        m.rotA = 4.828; m.rotB = 1.0012; m.rotC = 0.8282; m.rotRef = "1966Herzberg";
        m.shapeNote = "Planar (CCCBDB), D2h: four equal C-H bonds; 2 x H-C-C + H-C-H = 360 degrees, as the listed values give.";
        add(m);
    }
    {   // CH3OH: Cs; the three C-H assumed equivalent and symmetric about the C-O axis
        Molecule m;
        m.id = "CH3OH"; m.formula = "CH3OH"; m.hill = "CH4O"; m.name = "Methanol"; m.derivedAngle = true;
        m.values = {len(O, H, 0.956, "1955Ven/Gor:1200", ""), len(C, O, 1.427, "1955Ven/Gor:1200", ""), len(C, H, 1.096, "1955Ven/Gor:1200", ""),
                    angle(H, C, H, 109.03, "1955Ven/Gor:1200", "", 2), angle(H, O, C, 108.87, "1955Ven/Gor:1200", "", 2)};
        Value d{'d', H, C, O, 180, "1955Ven/Gor:1200", "for one H (symmetry)", 0, true, false};
        const double rOH = find(m.values, 'r', O, H), rCO = find(m.values, 'r', C, O), rCH = find(m.values, 'r', C, H);
        const double a = find(m.values, 'a', H, C, H) * kRad, t = find(m.values, 'a', H, O, C) * kRad;
        const double s2 = (1 - std::cos(a)) / 1.5, sg = std::sqrt(s2), cg = -std::sqrt(1 - s2);   // the H's lean away from O: H-C-O > 90 degrees
        Value hco{'a', H, C, O, std::acos(cg) / kRad, "derived (not in CCCBDB)", "three equivalent C-H, symmetric about the C-O axis: from H-C-H", 2, false, true};
        m.atoms = {{C, 0, 0, 0}, {O, 0, 0, rCO}};
        for (int k = 0; k < 3; k++) m.atoms.push_back({H, rCH * sg * std::cos(2 * kPi * k / 3), rCH * sg * std::sin(2 * kPi * k / 3), rCH * cg});
        m.atoms.push_back({H, -rOH * std::sin(t), 0, rCO - rOH * std::cos(t)});   // in the plane of the first methyl H, on the far side: H-C-O-H = 180 for that H
        d.i0 = 2; d.i1 = 0; d.i2 = 1; d.i3 = 5;
        m.values.push_back(hco);
        m.values.push_back(d);
        m.bonds = {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}, {1, 5, 1}};
        m.rotA = 4.2573; m.rotB = 0.82338; m.rotC = 0.79273; m.rotRef = "1984Her/Mes:42";
        m.shapeNote = "Assumed: the three C-H bonds equivalent and symmetric about the C-O axis (CCCBDB gives no H-C-O angle, so it is DERIVED from H-C-H), and H-C-O-H = 180 for one H (Cs). "
                      "The real methyl group is slightly tilted; this simple model does not show that.";
        add(m);
    }
    {   // C6H6: planar regular hexagon (D6h)
        Molecule m;
        m.id = "C6H6"; m.formula = "C6H6"; m.hill = "C6H6"; m.name = "Benzene"; m.planar = true;
        m.values = {len(C, C, 1.397, "1966Herzberg", ""), len(C, H, 1.084, "1966Herzberg", ""), angle(C, C, C, 120, "1966Herzberg", "", 0), angle(H, C, C, 120, "1966Herzberg", "", 0)};
        const double c = find(m.values, 'r', C, C), h = find(m.values, 'r', C, H);
        for (int k = 0; k < 6; k++) m.atoms.push_back({C, c * std::cos(kPi * k / 3), c * std::sin(kPi * k / 3), 0});   // a regular hexagon's side is its radius
        for (int k = 0; k < 6; k++) m.atoms.push_back({H, (c + h) * std::cos(kPi * k / 3), (c + h) * std::sin(kPi * k / 3), 0});
        for (int k = 0; k < 6; k++) m.bonds.push_back({k, (k + 1) % 6, 4});
        for (int k = 0; k < 6; k++) m.bonds.push_back({k, k + 6, 1});
        m.rotA = 0.1896; m.rotB = 0.1896; m.rotC = 0.0948; m.rotRef = "1966Herzberg";
        m.shapeNote = "Planar regular hexagon (CCCBDB: C-C-C and H-C-C 120 degrees): six equal C-C, six equal C-H on the rays from the middle.";
        add(m);
    }
    return all;
}

const std::vector<Molecule>& table() { static const std::vector<Molecule> t = buildAll(); return t; }

bool finite(double v) { return std::isfinite(v); }
bool readD(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !finite(v)) return false;
    out = v;
    return true;
}
bool readList(const std::string& s, double* out, int n) {
    std::size_t at = 0;
    for (int i = 0; i < n; i++) {
        const std::size_t comma = s.find(',', at);
        if ((i < n - 1) != (comma != std::string::npos)) return false;
        if (!readD(s.substr(at, comma == std::string::npos ? std::string::npos : comma - at), out[i])) return false;
        at = comma + 1;
    }
    return true;
}
bool isRotation(const double R[9]) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        double d = 0;
        for (int t = 0; t < 3; t++) d += R[i * 3 + t] * R[j * 3 + t];
        if (std::fabs(d - (i == j ? 1 : 0)) > 1e-6) return false;
    }
    return R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) + R[2] * (R[3] * R[7] - R[4] * R[6]) > 0;
}
void mul(const double A[9], double B[9]) {               // B = A B
    double o[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) o[i * 3 + j] = A[i * 3] * B[j] + A[i * 3 + 1] * B[3 + j] + A[i * 3 + 2] * B[6 + j];
    for (int i = 0; i < 9; i++) B[i] = o[i];
}
void rotY(double t, double R[9]) { const double c = std::cos(t), s = std::sin(t), M[9] = {c, 0, s, 0, 1, 0, -s, 0, c}; mul(M, R); }
void rotX(double t, double R[9]) { const double c = std::cos(t), s = std::sin(t), M[9] = {1, 0, 0, 0, c, -s, 0, s, c}; mul(M, R); }

}  // namespace

const Element& element(int el) { return kEl[el < 0 || el >= kElCount ? 0 : el]; }
void placePrincipal(Molecule& m) { place(m); }
double ballRadiusA(int el) { return kBallScale * element(el).vdwPm / 100; }
int count() { return static_cast<int>(table().size()); }
const Molecule& get(int i) { return table()[std::clamp(i, 0, count() - 1)]; }
int findId(const std::string& id) { for (int i = 0; i < count(); i++) if (table()[i].id == id) return i; return -1; }

double dist(const Atom& a, const Atom& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
double angleDeg(const Atom& a, const Atom& c, const Atom& b) {
    const double u[3] = {a.x - c.x, a.y - c.y, a.z - c.z}, v[3] = {b.x - c.x, b.y - c.y, b.z - c.z};
    const double x[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    return std::atan2(std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]), u[0] * v[0] + u[1] * v[1] + u[2] * v[2]) / kRad;
}
double dihedralDeg(const Atom& a, const Atom& b, const Atom& c, const Atom& d) {
    const auto sub = [](const Atom& p, const Atom& q, double* o) { o[0] = p.x - q.x; o[1] = p.y - q.y; o[2] = p.z - q.z; };
    const auto cr = [](const double* p, const double* q, double* o) { o[0] = p[1] * q[2] - p[2] * q[1]; o[1] = p[2] * q[0] - p[0] * q[2]; o[2] = p[0] * q[1] - p[1] * q[0]; };
    double b1[3], b2[3], b3[3], n1[3], n2[3], m1[3];
    sub(b, a, b1); sub(c, b, b2); sub(d, c, b3);
    cr(b1, b2, n1); cr(b2, b3, n2); cr(n1, n2, m1);
    const double l2 = std::sqrt(b2[0] * b2[0] + b2[1] * b2[1] + b2[2] * b2[2]);
    return std::fabs(std::atan2((m1[0] * b2[0] + m1[1] * b2[1] + m1[2] * b2[2]) / l2, n1[0] * n2[0] + n1[1] * n2[1] + n1[2] * n2[2]) / kRad);
}
double massU(const Molecule& m) { double s = 0; for (const Atom& a : m.atoms) s += element(a.el).massU; return s; }
double massUncU(const Molecule& m) {
    int n[kElCount] = {};
    for (const Atom& a : m.atoms) n[a.el]++;
    double q = 0;
    for (int e = 0; e < kElCount; e++) q += std::pow(n[e] * kEl[e].massUncU, 2);
    return std::sqrt(q);
}
void principalMoments(const Molecule& m, double I[3]) {
    double t[3][3], v[3][3];
    tensor(m, t);
    jacobi(t, v);
    I[0] = t[0][0]; I[1] = t[1][1]; I[2] = t[2][2];
    std::sort(I, I + 3);
    for (int k = 0; k < 3; k++) if (std::fabs(I[k]) < 1e-12) I[k] = 0;
}
double rotConstCm(double I) {
    if (I < 1e-9) return 0;
    return atom::kPlanckJs / (8 * kPi * kPi * atom::kC * I * atom::kUKg * 1e-20) / 100;
}
const Value* lengthValue(const Molecule& m, int a, int b) {
    const int ea = m.atoms[a].el, eb = m.atoms[b].el;
    for (const Value& v : m.values) if (v.kind == 'r' && ((v.e0 == ea && v.e1 == eb) || (v.e0 == eb && v.e1 == ea))) return &v;
    return nullptr;
}
const Value* angleValue(const Molecule& m, int a, int c, int b) {
    const int ea = m.atoms[a].el, ec = m.atoms[c].el, eb = m.atoms[b].el;
    for (const Value& v : m.values) if (v.kind == 'a' && v.e1 == ec && ((v.e0 == ea && v.e2 == eb) || (v.e0 == eb && v.e2 == ea))) return &v;
    return nullptr;
}
std::string atomLabel(const Molecule& m, int i) {
    int n = 0;
    for (int k = 0; k <= i && k < static_cast<int>(m.atoms.size()); k++) n += m.atoms[k].el == m.atoms[i].el;
    return std::string(element(m.atoms[i].el).sym) + std::to_string(n);
}

// ---- camera ----
Camera defaultCamera() {
    Camera c;
    rotY(0.35, c.R);                                      // a design choice: a little turned and tipped, so a planar molecule is not seen exactly edge-on or face-on
    rotX(-0.3, c.R);
    return c;
}
void orthonormalize(double R[9]) {
    double* a = R; double* b = R + 3; double* c = R + 6;
    const double la = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    for (int i = 0; i < 3; i++) a[i] /= la;
    const double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    for (int i = 0; i < 3; i++) b[i] -= d * a[i];
    const double lb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    for (int i = 0; i < 3; i++) b[i] /= lb;
    c[0] = a[1] * b[2] - a[2] * b[1]; c[1] = a[2] * b[0] - a[0] * b[2]; c[2] = a[0] * b[1] - a[1] * b[0];
}
void rotate(Camera& c, double dx, double dy) {
    if (!finite(dx) || !finite(dy)) return;
    if (dx != 0) rotY(dx * 0.01, c.R);
    if (dy != 0) rotX(dy * 0.01, c.R);
    orthonormalize(c.R);
}
void spin(Camera& c, double radians) { if (finite(radians)) { rotY(radians, c.R); orthonormalize(c.R); } }
void zoomBy(Camera& c, double f) { if (finite(f) && f > 0) c.zoom = std::clamp(c.zoom * f, kZoomMin, kZoomMax); }
void pan(Camera& c, double dx, double dy, double s) { if (finite(dx) && finite(dy) && s > 0) { c.panX += dx / s; c.panY -= dy / s; } }
double radiusA(const Molecule& m) {
    double e = 0;
    for (const Atom& a : m.atoms) e = std::max(e, std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z) + element(a.el).vdwPm / 100);
    return e;
}
double fitScale(const Molecule& m, double w, double h) { return 0.42 * std::max(1.0, std::min(w, h)) / std::max(radiusA(m), 0.1); }
void clampPan(Camera& c, double radius) {
    const double lim = kPanLimitRadii * std::max(radius, 0.1), d = std::hypot(c.panX, c.panY);
    if (!(d <= lim)) { const double k = d > 0 && std::isfinite(d) ? lim / d : 0; c.panX *= k; c.panY *= k; }
}
void project(const Molecule& m, const Camera& c, Style st, double cx, double cy, double s, std::vector<Proj>& out) {
    out.resize(m.atoms.size());
    const double* R = c.R;
    for (std::size_t i = 0; i < m.atoms.size(); i++) {
        const Atom& a = m.atoms[i];
        const double vx = R[0] * a.x + R[1] * a.y + R[2] * a.z + c.panX, vy = R[3] * a.x + R[4] * a.y + R[5] * a.z + c.panY, vz = R[6] * a.x + R[7] * a.y + R[8] * a.z;
        out[i] = {cx + vx * s, cy - vy * s, vz, (st == Style::SpaceFill ? element(a.el).vdwPm / 100 : ballRadiusA(a.el)) * s};
    }
}
Pick pick(const Molecule& m, const Camera& c, Style st, double cx, double cy, double s, double x, double y) {
    static thread_local std::vector<Proj> p;
    project(m, c, st, cx, cy, s, p);
    Pick best;
    double bestDepth = -1e300;
    for (std::size_t i = 0; i < p.size(); i++) {
        const double d2 = (x - p[i].x) * (x - p[i].x) + (y - p[i].y) * (y - p[i].y);
        if (d2 > p[i].r * p[i].r) continue;
        const double depth = p[i].depth + std::sqrt(p[i].r * p[i].r - d2) / s;   // the ball's front surface under the pointer
        if (depth > bestDepth) { bestDepth = depth; best = {static_cast<int>(i), -1}; }
    }
    if (st == Style::BallStick)
        for (std::size_t b = 0; b < m.bonds.size(); b++) {
            const Proj& A = p[m.bonds[b].a]; const Proj& B = p[m.bonds[b].b];
            const double ux = B.x - A.x, uy = B.y - A.y, L2 = ux * ux + uy * uy;
            const double t = L2 > 1e-12 ? std::clamp(((x - A.x) * ux + (y - A.y) * uy) / L2, 0.0, 1.0) : 0.0;
            const double ex = A.x + t * ux - x, ey = A.y + t * uy - y;
            const int o = m.bonds[b].order;
            const double hw = kStickRadiusA * s * (o == 2 ? 2.6 : o == 3 ? 4.2 : 1.0) + 2;   // the drawn sticks' width (molecule_view.cpp) and 2 points to spare
            if (ex * ex + ey * ey > hw * hw) continue;
            const double depth = A.depth + t * (B.depth - A.depth) + kStickRadiusA;
            if (depth > bestDepth) { bestDepth = depth; best = {-1, static_cast<int>(b)}; }
        }
    return best;
}

// ---- settings ----
std::string saveSettings(const Settings& s) {
    std::ostringstream o;
    o.precision(17);
    o << "v" << kSettingsVersion << " mol=" << get(s.mol).id << " style=" << (s.style == Style::SpaceFill ? "fill" : "ball") << " labels=" << (s.labels ? 1 : 0) << " spin=" << (s.spin ? 1 : 0)
      << " zoom=" << s.cam.zoom << " pan=" << s.cam.panX << "," << s.cam.panY << " rot=";
    for (int i = 0; i < 9; i++) o << (i ? "," : "") << s.cam.R[i];
    return o.str();
}
Settings loadSettings(const std::string& text) {
    Settings s;
    s.cam = defaultCamera();
    std::istringstream in(text);
    std::string tok;
    if (!(in >> tok) || tok != "v" + std::to_string(kSettingsVersion)) return s;   // another version (or nothing): the defaults
    while (in >> tok) {
        const std::size_t eq = tok.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
        double d = 0;
        if (k == "mol") { const int i = findId(v); if (i >= 0) s.mol = i; }
        else if (k == "style") { if (v == "ball") s.style = Style::BallStick; else if (v == "fill") s.style = Style::SpaceFill; }
        else if (k == "labels") { if (v == "0" || v == "1") s.labels = v == "1"; }
        else if (k == "spin") { if (v == "0" || v == "1") s.spin = v == "1"; }
        else if (k == "zoom") { if (readD(v, d) && d > 0) s.cam.zoom = std::clamp(d, kZoomMin, kZoomMax); }
        else if (k == "pan") { double pp[2]; if (readList(v, pp, 2) && std::fabs(pp[0]) < 1e3 && std::fabs(pp[1]) < 1e3) { s.cam.panX = pp[0]; s.cam.panY = pp[1]; } }
        else if (k == "rot") { double R[9]; if (readList(v, R, 9) && isRotation(R)) { orthonormalize(R); for (int i = 0; i < 9; i++) s.cam.R[i] = R[i]; } }
    }
    clampPan(s.cam, radiusA(get(s.mol)));                // 0.20: a stored pan beyond the limit is pulled back, as a drag would be
    return s;
}

}  // namespace molecule
