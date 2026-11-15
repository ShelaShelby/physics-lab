// See alkali.h.
#include "atom/alkali.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <map>
#include <memory>
#include <tuple>

namespace atom {

namespace {
#include "atom/alkali_data.inc"
#include "atom/valence_data.inc"
}

double AlkLine::nm() const { return std::isfinite(obsNm) ? obsNm : ritzNm; }
bool alkaliJumps(int Z) { return Z == 3 || Z == 11 || Z == 19 || Z == 37 || Z == 55 || Z == 87; }

std::vector<AlkLevel> alkaliLevels(int Z) {
    std::vector<AlkLevel> v;
    for (const AlkLevelRow& r : kAlkLevels) if (r.Z == Z) v.push_back({r.n, r.l, r.j2, r.cm, r.unc, r.cmText});
    return v;
}
std::vector<AlkLine> alkaliLines(int Z) {
    std::vector<AlkLine> v;
    for (const AlkLineRow& r : kAlkLines) if (r.Z == Z) v.push_back({r.nLo, r.lLo, r.j2Lo, r.nHi, r.lHi, r.j2Hi, r.obsNm, r.ritzNm, r.aki, r.acc});
    return v;
}
double alkaliTermCm(int Z, int n, int l) {
    double s = 0, w = 0;
    for (const AlkLevelRow& r : kAlkLevels) if (r.Z == Z && r.n == n && r.l == l) { s += (r.j2 + 1) * r.cm; w += r.j2 + 1; }
    return w > 0 ? s / w : NAN;
}
std::vector<AlkOrbital> alkaliOrbitals(int Z) {
    std::vector<AlkOrbital> v;
    for (const AlkLevelRow& r : kAlkLevels)
        if (r.Z == Z && std::none_of(v.begin(), v.end(), [&](const AlkOrbital& o) { return o.n == r.n && o.l == r.l; })) v.push_back({r.n, r.l});
    std::sort(v.begin(), v.end(), [&](const AlkOrbital& a, const AlkOrbital& b) { return alkaliTermCm(Z, a.n, a.l) < alkaliTermCm(Z, b.n, b.l); });
    return v;
}
std::vector<AlkLine> alkaliLinesBetween(int Z, int n1, int l1, int n2, int l2) {
    std::vector<AlkLine> v;
    for (const AlkLine& L : alkaliLines(Z))
        if ((L.nLo == n1 && L.lLo == l1 && L.nHi == n2 && L.lHi == l2) || (L.nLo == n2 && L.lLo == l2 && L.nHi == n1 && L.lHi == l1)) v.push_back(L);
    return v;
}
bool alkaliPickLine(int Z, int nFrom, int lFrom, int nTo, int lTo, double u, AlkLine& out, int j2From) {
    std::vector<AlkLine> c = alkaliLinesBetween(Z, nFrom, lFrom, nTo, lTo);
    if (j2From > 0) {
        std::vector<AlkLine> f;
        for (const AlkLine& L : c) if (((L.nLo == nFrom && L.lLo == lFrom) ? L.j2Lo : L.j2Hi) == j2From) f.push_back(L);
        if (!f.empty()) c = f;
    }
    if (c.empty()) return false;
    const bool anyA = std::any_of(c.begin(), c.end(), [](const AlkLine& L) { return std::isfinite(L.aki); });
    std::vector<double> w;
    double tot = 0;
    for (const AlkLine& L : c) { const double x = (L.j2Hi + 1) * (anyA ? (std::isfinite(L.aki) ? L.aki : 0.0) : 1.0); w.push_back(x); tot += x; }
    if (!(tot > 0)) { out = c[0]; return true; }
    double acc = 0;
    for (std::size_t i = 0; i < c.size(); i++) { acc += w[i] / tot; if (u < acc) { out = c[i]; return true; } }
    out = c.back();
    return true;
}

const hf::Subshell* valenceOrbital(int Z, int n, int l, bool rel) {
    static std::map<std::tuple<int, int, int, bool>, std::unique_ptr<hf::Subshell>> cache;
    const auto key = std::make_tuple(Z, n, l, rel);
    const auto it = cache.find(key);
    if (it != cache.end()) return it->second.get();
    auto s = std::make_unique<hf::Subshell>();
    s->n = n; s->l = l; s->occ = 1;
    double epsW = 0;
    for (const ValenceRow& r : kValence) {
        if (r.Z != Z || r.n != n || r.l != l || (r.rel != 0) != rel) continue;
        hf::Part p;
        p.j2 = r.j2; p.eps = r.eps; p.amin = r.amin; p.ratio = r.ratio;
        p.occ = rel ? (r.j2 + 1) / (2.0 * (2 * l + 1)) : 1.0;              // one electron, the j levels in proportion to 2j + 1 (the term's average)
        p.dP.assign(kValenceCoef + r.off, kValenceCoef + r.off + r.nb);
        p.dQ.assign(kValenceCoef + r.off + r.nb, kValenceCoef + r.off + 2 * r.nb);
        epsW += p.occ * r.eps;
        s->parts.push_back(p);
    }
    if (s->parts.empty()) { cache[key] = nullptr; return nullptr; }
    std::sort(s->parts.begin(), s->parts.end(), [](const hf::Part& a, const hf::Part& b) { return a.j2 < b.j2; });
    s->eps = epsW;
    return (cache[key] = std::move(s)).get();
}
double valenceEnergyHartree(int Z, int n, int l, int j2, bool rel) {
    for (const ValenceRow& r : kValence) if (r.Z == Z && r.n == n && r.l == l && r.j2 == j2 && (r.rel != 0) == rel) return r.eps;
    return NAN;
}

const hf::Atom* alkaliAtom(int Z, int n, int l, bool rel, int j2) {
    if (rel && graspHas(Z)) {
        if (j2 < 0) j2 = graspDefaultJ2(Z, n, l);
        const GraspLevel* g = graspLevel(Z, n, l, j2);
        return g ? &g->atom : nullptr;
    }
    return alkaliAtom(Z, n, l, rel);
}
const hf::Atom* alkaliAtom(int Z, int n, int l, bool rel) {
    if (rel && graspHas(Z)) return alkaliAtom(Z, n, l, true, -1);
    const hf::Atom* g = hf::find(Z, 0, rel);
    if (!g || !alkaliJumps(Z)) return nullptr;
    const std::vector<AlkOrbital> orbs = alkaliOrbitals(Z);
    if (orbs.empty()) return nullptr;
    if (n == orbs[0].n && l == orbs[0].l) return g;                   // the ground state: the atom as drawn before 0.46
    static std::map<std::tuple<int, int, int, bool>, std::unique_ptr<hf::Atom>> cache;
    const auto key = std::make_tuple(Z, n, l, rel);
    const auto it = cache.find(key);
    if (it != cache.end()) return it->second.get();
    const hf::Subshell* v = valenceOrbital(Z, n, l, rel);
    if (!v) { cache[key] = nullptr; return nullptr; }
    auto a = std::make_unique<hf::Atom>(*g);
    a->sub.erase(std::remove_if(a->sub.begin(), a->sub.end(), [&](const hf::Subshell& s) { return s.n == orbs[0].n && s.l == orbs[0].l; }), a->sub.end());
    a->sub.push_back(*v);
    std::sort(a->sub.begin(), a->sub.end(), [](const hf::Subshell& x, const hf::Subshell& y) { return x.n != y.n ? x.n < y.n : x.l < y.l; });
    a->level.clear(); a->J2 = -1;
    a->shells = hf::configText(*a);
    a->source = "frozen-core valence orbital (tools/dhf/valence.py)";
    return (cache[key] = std::move(a)).get();
}

// ---- 0.47.2: the GRASP2018 clouds (data/jumps/grasp/<X>/clouds.bin, format: tools/grasp/multipoles.py) ----
const unsigned char* embeddedGrasp(int Z, std::size_t& size);   // (cmake/embed_grasp.cmake)
namespace {
struct Reader {
    const unsigned char* p; std::size_t n, at = 0; bool ok = true;
    template <class T> T get() { T v{}; if (at + sizeof(T) > n) { ok = false; return v; } std::memcpy(&v, p + at, sizeof(T)); at += sizeof(T); return v; }
    std::vector<float> floats(int k) { std::vector<float> v(std::size_t(std::max(k, 0))); if (at + v.size() * 4 > n) { ok = false; return {}; } std::memcpy(v.data(), p + at, v.size() * 4); at += v.size() * 4; return v; }
};
const std::vector<GraspLevel>& graspLevels(int Z) {
    static std::map<int, std::vector<GraspLevel>> cache;
    const auto it = cache.find(Z);
    if (it != cache.end()) return it->second;
    std::vector<GraspLevel>& out = cache[Z];
    std::size_t size = 0;
    const unsigned char* b = embeddedGrasp(Z, size);
    // PLG1: the alkalis' levels, keyed by n, l, J; PLG2 (0.47.3, tools/grasp/multipoles_two.py): each level also carries NIST's configuration and term (levels.h)
    if (!b || size < 32 || (std::memcmp(b, "PLG1", 4) != 0 && std::memcmp(b, "PLG2", 4) != 0)) return out;
    const bool v2 = b[3] == '2';
    Reader rd{b, size, 4};
    const int z = rd.get<std::int32_t>(), ng = rd.get<std::int32_t>();
    const double r0 = rd.get<double>(), rmax = rd.get<double>();
    const int nl = rd.get<std::int32_t>();
    const double h = std::log(rmax / r0) / (ng - 1);
    if (z != Z || ng < 4) return out;
    for (int i = 0; i < nl && rd.ok; i++) {
        GraspLevel g;
        if (v2) {
            const auto str = [&] { const int k = rd.get<std::int32_t>(); std::string t; if (k < 0 || rd.at + std::size_t(k) > rd.n) { rd.ok = false; return t; } t.assign(reinterpret_cast<const char*>(rd.p + rd.at), std::size_t(k)); rd.at += std::size_t(k); return t; };
            g.conf = str(); g.term = str();
        }
        g.n = rd.get<std::int32_t>(); g.l = rd.get<std::int32_t>(); g.j2 = rd.get<std::int32_t>(); g.testOnly = rd.get<std::int32_t>() != 0;
        g.computedCm = rd.get<double>(); g.nistCm = rd.get<double>();
        const int nsub = rd.get<std::int32_t>(), nk = rd.get<std::int32_t>();
        g.r0 = r0; g.h = h;
        g.atom.Z = Z; g.atom.relativistic = true; g.atom.J2 = g.j2;
        for (int s = 0; s < nsub && rd.ok; s++) {
            hf::Subshell sh;
            sh.n = rd.get<std::int32_t>(); sh.l = rd.get<std::int32_t>();
            const int np = rd.get<std::int32_t>();
            sh.lumped = rd.get<std::int32_t>() != 0;
            double tot = 0;
            for (int q = 0; q < np && rd.ok; q++) {
                hf::Part pt;
                pt.j2 = rd.get<std::int32_t>(); pt.occ = rd.get<double>();
                pt.tabP = rd.floats(ng); pt.tabQ = rd.floats(ng); pt.tabR0 = r0; pt.tabH = h;
                tot += pt.occ;
                sh.parts.push_back(std::move(pt));
            }
            sh.focc = tot; sh.occ = int(std::lround(tot));
            g.atom.sub.push_back(std::move(sh));
        }
        for (int k = 0; k < nk && rd.ok; k++) { g.ks.push_back(rd.get<std::int32_t>()); g.Dk.push_back(rd.floats(ng)); }
        std::sort(g.atom.sub.begin(), g.atom.sub.end(), [](const hf::Subshell& x, const hf::Subshell& y) { return x.n != y.n ? x.n < y.n : x.l < y.l; });
        g.atom.shells = hf::configText(g.atom);
        g.atom.source = "GRASP2018 core-valence MCDHF + RCI (natural orbitals)";
        if (rd.ok) out.push_back(std::move(g));
    }
    if (!rd.ok) out.clear();
    return out;
}
}  // namespace
double GraspLevel::qualityPercent() const { return nistCm > 0 ? 100 * (computedCm - nistCm) / nistCm : NAN; }
bool graspHas(int Z) { return !graspLevels(Z).empty(); }
const std::vector<GraspLevel>& graspAll(int Z) { return graspLevels(Z); }
const GraspLevel* graspLevel(int Z, int n, int l, int j2) {
    for (const GraspLevel& g : graspLevels(Z)) if (!g.testOnly && g.n == n && g.l == l && g.j2 == j2) return &g;
    return nullptr;
}
const GraspLevel* graspOfAtom(const hf::Atom* a) {
    if (!a) return nullptr;
    for (const GraspLevel& g : graspLevels(a->Z)) if (&g.atom == a) return &g;
    return nullptr;
}
const GraspLevel* graspTestLevel(int Z) { for (const GraspLevel& g : graspLevels(Z)) if (g.testOnly) return &g; return nullptr; }
int graspDefaultJ2(int Z, int n, int l) {
    for (int j2 = 2 * l + 1; j2 >= std::max(1, 2 * l - 1); j2 -= 2) if (graspLevel(Z, n, l, j2)) return j2;
    return 2 * l + 1;
}
double graspDk(const GraspLevel& g, std::size_t i, double r) {
    if (i >= g.Dk.size() || !(r >= g.r0)) return 0;
    const std::vector<float>& f = g.Dk[i];
    const int n = int(f.size());
    const double t = std::log(r / g.r0) / g.h;
    if (t >= n - 1) return 0;
    const int j0 = std::min(std::max(int(std::floor(t)) - 1, 0), n - 4);
    const double x = t - j0, y0 = f[std::size_t(j0)], y1 = f[std::size_t(j0 + 1)], y2 = f[std::size_t(j0 + 2)], y3 = f[std::size_t(j0 + 3)];
    return -y0 * (x - 1) * (x - 2) * (x - 3) / 6 + y1 * x * (x - 2) * (x - 3) / 2 - y2 * x * (x - 1) * (x - 3) / 2 + y3 * x * (x - 1) * (x - 2) / 6;
}
double graspShape(const GraspLevel& g, double r, double cz, double& bound) {
    bound = 1;
    if (g.ks.empty()) return 1;
    const double d0 = hf::totalRadial(g.atom, r);
    if (!(d0 > 0)) return 1;
    double w = 1;
    for (std::size_t i = 0; i < g.ks.size(); i++) {
        const double a = graspDk(g, i, r) / d0;
        bound += std::fabs(a);
        // P_k(cz) by the recurrence
        double p0 = 1, p1 = cz;
        for (int k = 2; k <= g.ks[i]; k++) { const double p2 = ((2 * k - 1) * cz * p1 - (k - 1) * p0) / k; p0 = p1; p1 = p2; }
        w += a * (g.ks[i] == 0 ? 1.0 : p1);
    }
    return std::max(0.0, w);
}

void graspDirection(const GraspLevel& g, double r, orbit::Mulberry32& rng, double& ux, double& uy, double& uz) {
    double cz = 2 * rng.next() - 1;
    for (int t = 0; t < 10000 && !g.ks.empty(); t++) {
        double bound = 1;
        const double w = graspShape(g, r, cz, bound);
        if (rng.next() * bound <= w) break;
        cz = 2 * rng.next() - 1;
    }
    const double ph = 2 * 3.14159265358979323846 * rng.next(), sz = std::sqrt(std::max(0.0, 1 - cz * cz));
    ux = sz * std::cos(ph); uy = sz * std::sin(ph); uz = cz;
}
void graspSample(const GraspLevel& g, orbit::Mulberry32& rng, double& x, double& y, double& z, int& shellOut) {
    hf::sampleAtom(g.atom, rng, x, y, z, shellOut);
    if (g.ks.empty()) return;
    const double rad = std::sqrt(x * x + y * y + z * z);
    double ux, uy, uz;
    graspDirection(g, rad, rng, ux, uy, uz);
    x = rad * ux; y = rad * uy; z = rad * uz;
}
double partDensity(const hf::Part& p, int l, double r) {
    if (p.tabP.empty()) return 0;
    const double P = hf::tabValue(p.tabP, p.tabR0, p.tabH, l, r), Q = hf::tabValue(p.tabQ, p.tabR0, p.tabH, l, r);
    return P * P + Q * Q;
}

std::string graspModelNote() {
    return "every electron: this level's natural orbitals with their occupations (not whole numbers once configurations mix; they add up to the electron count), computed for this app with "
           "GRASP2018 (Froese Fischer, Gaigalas, Joensson, Bieron, Comput. Phys. Commun. 237, 184 (2019)): core-valence MCDHF (single and double excitations, at most one hole in the outer "
           "p shell of the core, 1s for lithium; three layers of correlation orbitals s to g), then CI with the Breit interaction and QED; natural orbitals under 0.001 electron drawn together. "
           "The shape is the M_J = J state's density, sum of D_k(r) P_k(cos theta), k even up to 2J. Energies and wavelengths: measured, NIST ASD";
}
std::string alkaliModelNote() {
    // 0.46.1 (outside review): the two models of the drawing said plainly: the ground atom is the neutral atom's own self-consistent calculation; an excited outer electron is a frozen-core orbital
    // (computed in the field of the ion's core) put on top of that neutral atom's core. For sodium the ground 3s computed both ways differs by under 0.5 % in <r> (test_atom046.inc).
    return "the ground atom: the neutral atom's own Dirac-Hartree-Fock calculation; an excited outer electron: a frozen-core Dirac-Hartree-Fock orbital (computed in the field of the ion's core, "
           "no core relaxation or correlation) drawn on top of the neutral atom's core, which is almost the same (sodium's 3s computed both ways: <r> within 0.5 %); energies and wavelengths: measured, NIST ASD";
}

}  // namespace atom
