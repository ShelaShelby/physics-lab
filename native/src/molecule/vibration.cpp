#include "molecule/vibration.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace molecule::vib {

namespace {

constexpr double kHbar = 1.054571817e-34, kC = 299792458.0, kU = 1.66053906892e-27, kPi = 3.14159265358979323846;   // CODATA 2022

struct Raw { const char* id; double nu, ir; double L[36]; };   // (benzene: 12 atoms, 36 numbers; smaller molecules leave the rest 0)
const Raw kRaw[] = {
#include "molecule/vib_data.inc"
};

const Measured kMeasured[] = {
#include "molecule/vib_measured.inc"
};

const std::map<std::string, std::vector<Mode>>& table() {
    static const std::map<std::string, std::vector<Mode>> t = [] {
        std::map<std::string, std::vector<Mode>> m;
        for (const Raw& r : kRaw) {
            const int i = findId(r.id);
            if (i < 0) continue;
            const std::size_t n = 3 * get(i).atoms.size();
            if (n > sizeof r.L / sizeof r.L[0]) continue;      // (more than 12 atoms would not fit the table's row: left out)
            m[r.id].push_back({r.nu, r.ir, std::vector<double>(r.L, r.L + n)});
        }
        for (auto& [id, v] : m) std::stable_sort(v.begin(), v.end(), [](const Mode& a, const Mode& b) { return a.nuCalc < b.nuCalc; });
        for (auto& [id, v] : m)
            for (std::size_t k = 0; k < v.size(); k++) {
                v[k].nuSwing = v[k].nuCalc;
                if (v[k].nuCalc <= 0)                                       // imaginary: the swing uses the measured fundamental
                    for (const Measured& e : kMeasured) if (id == e.id && static_cast<int>(k) >= e.first && static_cast<int>(k) < e.first + e.count) v[k].nuSwing = e.fund;
            }
        return m;
    }();
    return t;
}

const char* sym(int el) { return element(el).sym; }

}  // namespace

const std::vector<Mode>& modes(const std::string& id) {
    static const std::vector<Mode> none;
    const auto it = table().find(id);
    return it == table().end() ? none : it->second;
}

int expectedCount(const Molecule& m) { const int n = static_cast<int>(m.atoms.size()); return n < 2 ? 0 : 3 * n - (m.linear || n == 2 ? 5 : 6); }

int degenerateFirst(const std::string& id, int k) {
    const auto& v = modes(id);
    if (k < 0 || k >= static_cast<int>(v.size())) return k;
    while (k > 0 && std::fabs(v[k - 1].nuCalc - v[k].nuCalc) < kDegenerateCm) k--;
    return k;
}
int degeneracy(const std::string& id, int k) {
    const auto& v = modes(id);
    if (k < 0 || k >= static_cast<int>(v.size())) return 0;
    int a = degenerateFirst(id, k), b = a;
    while (b + 1 < static_cast<int>(v.size()) && std::fabs(v[b + 1].nuCalc - v[a].nuCalc) < kDegenerateCm) b++;
    return b - a + 1;
}
bool irActive(const Mode& m) { return m.irKmMol >= kIrActive; }

const Measured* measured(const std::string& id, int k) {
    for (const Measured& e : kMeasured) if (id == e.id && k >= e.first && k < e.first + e.count) return &e;
    return nullptr;
}
const char* basisOf(const std::string&) { return "aug-cc-pVTZ"; }
double scaleOf(const std::string&) { return 0.9675; }
double scaleSigmaOf(const std::string&) { return 0.019; }
double nuShown(const std::string& id, int k) {
    const Measured* e = measured(id, k);
    if (e && std::isfinite(e->fund)) return e->fund;
    const auto& v = modes(id);
    return k >= 0 && k < static_cast<int>(v.size()) ? v[k].nuCalc : 0;
}
bool irMeasuredAll(const std::string& id) {
    const auto& v = modes(id);
    bool any = false;
    for (int k = 0; k < static_cast<int>(v.size()); k++) {
        if (!irActive(v[k])) continue;
        const Measured* e = measured(id, k);
        if (!e || !std::isfinite(e->ir)) return false;
        any = true;
    }
    return any;
}

std::vector<double> turningPoint(const Molecule& m, const Mode& md) {
    std::vector<double> d(md.L.size(), 0.0);
    if (!(md.nuSwing > 0)) return d;                         // (no real wavenumber to swing at: stands still; today's data has none)
    const double w = 2 * kPi * kC * md.nuSwing * 100, Q = std::sqrt(kHbar / w);   // kg^1/2 m
    for (std::size_t i = 0; i < m.atoms.size() && 3 * i + 2 < md.L.size(); i++) {
        const double s = Q / std::sqrt(element(m.atoms[i].el).massU * kU) * 1e10;   // angstrom per unit of L
        for (int k = 0; k < 3; k++) d[3 * i + k] = md.L[3 * i + k] * s;
    }
    return d;
}
double largestMoveA(const Molecule& m, const Mode& md) {
    const std::vector<double> d = turningPoint(m, md);
    double big = 0;
    for (std::size_t i = 0; i + 2 < d.size(); i += 3) big = std::max(big, std::sqrt(d[i] * d[i] + d[i + 1] * d[i + 1] + d[i + 2] * d[i + 2]));
    return big;
}
void displaced(const Molecule& eq, const Mode& md, double s, double scale, Molecule& out) {
    out = eq;
    const std::vector<double> d = turningPoint(eq, md);
    for (std::size_t i = 0; i < out.atoms.size() && 3 * i + 2 < d.size(); i++) {
        out.atoms[i].x += s * scale * d[3 * i]; out.atoms[i].y += s * scale * d[3 * i + 1]; out.atoms[i].z += s * scale * d[3 * i + 2];
    }
}

double drawnScale(const Molecule& m, const Mode& md, bool bigger) {
    if (!bigger) return 1;
    const double big = largestMoveA(m, md);
    return big > 0 ? std::max(1.0, std::min(kExaggerate, kMaxDrawnA / big)) : kExaggerate;
}
double periodS(double nu) { return 1 / (kC * nu * 100); }
double screenPeriodS(double nu) { return kScreenSAt1000 * 1000 / nu; }
double slowDown() { return kScreenSAt1000 / periodS(1000); }

// ---- 0.29: live, every mode at once (vibration.h) ----
namespace {
constexpr double kH = 6.62607015e-34, kKB = 1.380649e-23, kNA = 6.02214076e23;   // CODATA 2022 (exact)
// mode k's amplitude per unit of its mass-weighted vector, in angstrom * sqrt(u) ... kept in SI: kg^1/2 m
double liveAmp(const std::string& id, int k, double T) {
    const double nu = nuShown(id, k);
    if (!(nu > 0)) return 0;
    const double w = 2 * kPi * kC * nu * 100;
    return std::sqrt(kHbar / w * thermalFactor(nu, T));
}
}  // namespace
double meanQuanta(double nu, double T) {
    if (!(T > 0) || !(nu > 0)) return 0;
    return 1 / std::expm1(kH * kC * nu * 100 / (kKB * T));
}
double thermalFactor(double nu, double T) { return 2 * meanQuanta(nu, T) + 1; }
double thermalEnergyKJmol(const std::string& id, double T) {
    double e = 0;
    for (int k = 0; k < static_cast<int>(modes(id).size()); k++) { const double nu = nuShown(id, k); e += kH * kC * nu * 100 * meanQuanta(nu, T); }
    return e * kNA / 1000;
}
void liveDisplaced(const Molecule& eq, double T, double t, double scale, Molecule& out) {
    out = eq;
    const auto& md = modes(eq.id);
    for (int k = 0; k < static_cast<int>(md.size()); k++) {
        const double nu = nuShown(eq.id, k);
        if (!(nu > 0)) continue;
        double ph = std::fmod(0.6180339887498949 * (k + 1), 1.0) * 2 * kPi;     // fixed phases, spread evenly
        const double Q = liveAmp(eq.id, k, T) * std::cos(2 * kPi * kC * nu * 100 * std::fmod(t, 1.0 / (kC * nu * 100)) + ph) * scale;   // (t taken modulo the period: exact for long runs)
        for (std::size_t i = 0; i < out.atoms.size() && 3 * i + 2 < md[k].L.size(); i++) {
            const double s = Q / std::sqrt(element(eq.atoms[i].el).massU * kU) * 1e10;
            out.atoms[i].x += md[k].L[3 * i] * s; out.atoms[i].y += md[k].L[3 * i + 1] * s; out.atoms[i].z += md[k].L[3 * i + 2] * s;
        }
    }
}
double liveRmsA(const Molecule& m, double T) {
    const auto& md = modes(m.id);
    double big = 0;
    for (std::size_t i = 0; i < m.atoms.size(); i++) {
        double ms = 0;
        for (int k = 0; k < static_cast<int>(md.size()); k++) {
            if (3 * i + 2 >= md[k].L.size()) continue;
            const double a = liveAmp(m.id, k, T) / std::sqrt(element(m.atoms[i].el).massU * kU) * 1e10;
            ms += a * a * (md[k].L[3 * i] * md[k].L[3 * i] + md[k].L[3 * i + 1] * md[k].L[3 * i + 1] + md[k].L[3 * i + 2] * md[k].L[3 * i + 2]) / 2;   // <cos^2> = 1/2, cross terms average out
        }
        big = std::max(big, std::sqrt(ms));
    }
    return big;
}
double liveDrawnScale(const Molecule& m, double T, bool bigger) {
    if (!bigger) return 1;
    const double r = liveRmsA(m, T);
    return r > 0 ? std::max(1.0, std::min(kExaggerate, kMaxDrawnA / (2 * r))) : kExaggerate;
}

Character character(const Molecule& m, const Mode& md) {
    const std::vector<double> d = turningPoint(m, md);
    struct T { double share = 0; int n = 0, pos = 0, neg = 0; };
    std::map<std::string, T> types;
    double S = 0, B = 0;
    for (const Bond& b : m.bonds) {
        const Atom &A = m.atoms[b.a], &Bt = m.atoms[b.b];
        double u[3] = {Bt.x - A.x, Bt.y - A.y, Bt.z - A.z};
        const double r = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        for (double& x : u) x /= r;
        double rel[3], par = 0, all = 0;
        for (int k = 0; k < 3; k++) { rel[k] = d[3 * b.b + k] - d[3 * b.a + k]; par += rel[k] * u[k]; all += rel[k] * rel[k]; }
        S += par * par; B += std::max(0.0, all - par * par);
        int e0 = A.el, e1 = Bt.el;                                  // "O–H": hydrogen last, else in the app's element order (C, N, O, Cl)
        if (e0 == H || (e1 != H && e1 < e0)) std::swap(e0, e1);
        T& t = types[std::string(sym(e0)) + "–" + sym(e1)];
        t.share += par * par; t.n++;
        if (par > 1e-9) t.pos++; else if (par < -1e-9) t.neg++;
    }
    Character c;
    c.stretchShare = S + B > 0 ? S / (S + B) : 0;
    std::string top; double best = -1;
    for (const auto& [name, t] : types) if (t.share > best) { best = t.share; top = name; }
    if (c.stretchShare >= 0.7 && !top.empty()) {
        const T& t = types[top];
        c.shortWords = top + " stretch" + (t.n == 2 ? (t.pos && t.neg ? ", in turn" : ", together") : t.n > 2 && (t.pos == t.n || t.neg == t.n) ? ", together" : "");
        if (t.n == 1) c.words = "stretches the " + top + " bond";
        else if (t.n == 2 && t.pos && t.neg) c.words = "stretches the two " + top + " bonds in turn";
        else if (t.n == 2) c.words = "stretches the two " + top + " bonds together";
        else if (t.pos == t.n || t.neg == t.n) c.words = "stretches the " + std::to_string(t.n) + " " + top + " bonds together";
        else c.words = "stretches the " + top + " bonds";
        for (const auto& [name, u] : types) if (name != top && u.share > 0.25 * S) c.words += " and the " + name + (u.n == 1 ? " bond" : " bonds");
    } else if (c.stretchShare <= 0.3) {
        bool outOfPlane = m.planar && m.atoms.size() > 2;           // a planar molecule: every atom moves along the plane's normal
        if (outOfPlane) {
            const Atom &a0 = m.atoms[0], &a1 = m.atoms[1], &a2 = m.atoms[2];
            double p[3] = {a1.x - a0.x, a1.y - a0.y, a1.z - a0.z}, q[3] = {a2.x - a0.x, a2.y - a0.y, a2.z - a0.z};
            double nrm[3] = {p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]};
            const double nl = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
            double inPlane = 0, total = 0;
            for (std::size_t i = 0; i < m.atoms.size(); i++) {
                const double* v = &d[3 * i];
                const double along = nl > 0 ? (v[0] * nrm[0] + v[1] * nrm[1] + v[2] * nrm[2]) / nl : 0, v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
                total += v2; inPlane += v2 - along * along;
            }
            outOfPlane = nl > 0 && inPlane < 0.01 * total;
        }
        // 0.39.3: a torsion (methanol's 200 cm^-1 mode, Shimanouchi) turns one part about a bond: to first order no bond angle changes, only a dihedral.
        // Measured as the bond-angle change (radians times the mean bond length, angstrom) against the sideways motion B; a real bend has them alike.
        bool twist = false;
        if (!m.linear && !outOfPlane && m.atoms.size() > 3 && B > 0) {
            auto pos = [&](std::size_t i, double t, int k) { const double v[3] = {m.atoms[i].x, m.atoms[i].y, m.atoms[i].z}; return v[k] + t * d[3 * i + k]; };
            auto ang = [&](int a, int ce, int b, double t) {
                double p[3], q[3], pp = 0, qq = 0, pq = 0;
                for (int k = 0; k < 3; k++) { p[k] = pos(a, t, k) - pos(ce, t, k); q[k] = pos(b, t, k) - pos(ce, t, k); pp += p[k] * p[k]; qq += q[k] * q[k]; pq += p[k] * q[k]; }
                return std::acos(std::max(-1.0, std::min(1.0, pq / std::sqrt(pp * qq))));
            };
            double bendSum = 0, lenSum = 0;
            for (const Bond& b : m.bonds) { const Atom &A = m.atoms[b.a], &Bt = m.atoms[b.b]; lenSum += std::sqrt((A.x - Bt.x) * (A.x - Bt.x) + (A.y - Bt.y) * (A.y - Bt.y) + (A.z - Bt.z) * (A.z - Bt.z)); }
            const double rMean = lenSum / static_cast<double>(m.bonds.size()), e = 1e-3;
            for (int ce = 0; ce < static_cast<int>(m.atoms.size()); ce++) {
                std::vector<int> nb;
                for (const Bond& b : m.bonds) { if (b.a == ce) nb.push_back(b.b); else if (b.b == ce) nb.push_back(b.a); }
                for (std::size_t i = 0; i < nb.size(); i++)
                    for (std::size_t j = i + 1; j < nb.size(); j++) { const double dth = (ang(nb[i], ce, nb[j], e) - ang(nb[i], ce, nb[j], -e)) / (2 * e) * rMean; bendSum += dth * dth; }
            }
            twist = bendSum < 0.05 * B;
        }
        c.words = m.linear ? "bends the line of atoms" : outOfPlane ? "bends the atoms out of the molecule's plane"
                : twist ? "twists one part of the molecule against the other about a bond (no bond angle changes, a dihedral does)" : "bends the molecule (the angles change)";
        c.shortWords = outOfPlane ? "bend out of the plane" : twist ? "twist (torsion)" : "bend";
    } else {
        c.words = "stretches and bends at once" + (top.empty() ? std::string() : " (mostly the " + top + (types[top].n == 1 ? " bond)" : " bonds)"));
        c.shortWords = "stretch and bend";
    }
    return c;
}

namespace {
// Huber & Herzberg (1979) via the NIST WebBook, the X state row and its footnotes (data/molecules/vib/webbook_diatomic/):
//   H2  we 4401.213, wexe 121.336; footnote 118: "+0.8129 (v+1/2)^3; these constants (Foltz, Rank et al. 1966) represent only the levels v = 0,1,2,3"
//   N2  we 2358.57, wexe 14.324, weye -2.26e-3; footnote 109: weze = -0.00024 (Lofthus & Krupenie 1977)
//   O2  we 1580.193, wexe 11.981, weye 0.04747; footnote 69: weze = -0.001273 (Krupenie 1972; Albritton et al. 1979)
//   HCl we 2990.9463, wexe 52.8186, weye 0.22437; footnote 27: weze = -0.01218 (Rank, Rao et al. 1965); footnote 26: levels up to v = 5 observed in infrared absorption
const Anharmonic kAnharmonic[] = {
    {"H2", 4401.213, 121.336, 0.8129, 0, 3, "fitted to v = 0 to 3 (Foltz, Rank et al. 1966)"},
    {"N2", 2358.57, 14.324, -2.26e-3, -0.00024, 0, ""},
    {"O2", 1580.193, 11.981, 0.04747, -0.001273, 0, ""},
    {"HCl", 2990.9463, 52.8186, 0.22437, -0.01218, 5, "levels up to v = 5 observed in the infrared"},
};
}  // namespace
const Anharmonic* anharmonic(const std::string& id) {
    for (const Anharmonic& a : kAnharmonic) if (id == a.id) return &a;
    return nullptr;
}
double termG(const Anharmonic& a, int v) {
    const double x = v + 0.5;
    return a.we * x - a.wexe * x * x + a.weye * x * x * x + a.weze * x * x * x * x;
}

}  // namespace molecule::vib
