// Molecule Lab stage 2 (0.20): the builder (see builder.h for the sources and the owner's decisions).
#include "molecule/builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace molecule::build {
namespace {

constexpr double kPi = 3.14159265358979323846;

// NIST CCCBDB, SRD 101, Release 22, experimental geometries (given to the job by the owner as text, fetched 18 Oct 2026): one reference molecule per bond type and its reference code.
// Bond types not listed here (N=N, N=O, N#O, O=Cl, O-Cl, N-Cl, C#O and every other order) are refused: no length is ever filled in from memory.
const LengthRef kLen[] = {
    {H, H, 1, 0.741, "H2", "1979HUB/HER"},
    {C, H, 1, 1.087, "CH4", "1979Hir:213"},
    {N, H, 1, 1.012, "NH3", "1966Herzberg"},
    {O, H, 1, 0.958, "H2O", "1979Hoy/Bun:1"},
    {H, Cl, 1, 1.275, "HCl", "NISTdiatomic"},
    {C, C, 1, 1.536, "ethane C2H6", "1966Herzberg"},
    {C, C, 2, 1.339, "C2H4", "1966Herzberg"},
    {C, C, 3, 1.203, "C2H2", "1998Kuc"},
    {C, N, 1, 1.471, "methylamine CH3NH2", "1998Kuc"},          // CCCBDB: an r0 value
    {C, N, 2, 1.273, "methanimine CH2NH", "1979Har/Lau:619"},
    {C, N, 3, 1.156, "HCN", "1966Herzberg"},
    {C, O, 1, 1.427, "methanol", "1955Ven/Gor:1200"},
    {C, O, 2, 1.205, "H2CO", "Gurvich"},
    {C, Cl, 1, 1.785, "chloromethane CH3Cl", "1973Dun/McK:232"},
    {N, N, 1, 1.446, "hydrazine N2H4", "1974Tsu/Ove:256"},
    {N, N, 3, 1.098, "N2", "1979HUB/HER"},
    {N, O, 1, 1.453, "hydroxylamine NH2OH", "1972Tsu:167-174"},
    {O, O, 1, 1.475, "hydrogen peroxide H2O2", "1962Red/Ols:1311"},
    {O, O, 2, 1.208, "O2", "1979HUB/HER"},
    {Cl, Cl, 1, 1.988, "Cl2", "1979HUB/HER"},                     // CCCBDB: re
};
constexpr int kLenCount = static_cast<int>(sizeof kLen / sizeof *kLen);

const int kValence[kElCount] = {1, 4, 3, 2, 1};          // H, C, N, O, Cl (owner decision 3)
const int kValenceE[kElCount] = {1, 4, 5, 6, 7};         // the main-group column
const int kNameRank[kElCount] = {3, 0, 1, 2, 4};         // bond names as the job writes them: C, N, O, then H, then Cl ("C-H", "N-H", "H-Cl", "O-Cl")
const char* const kOrderWord[4] = {"", "single", "double", "triple"};

std::string fmt(const char* f, ...) {
    char b[256];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}
const char* plural(int n) { return n == 1 ? "" : "s"; }
bool validEl(int el) { return el >= 0 && el < kElCount; }

// the 8 molecules beyond the viewer's 15 that get a name (the job's list); names are conventions (the About says so)
struct Extra { const char* id; const char* name; std::vector<int> els; std::vector<SBond> bonds; };
const std::vector<Extra>& extras() {
    static const std::vector<Extra> e = {
        {"C2H6", "Ethane", {C, C, H, H, H, H, H, H}, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}, {1, 5, 1}, {1, 6, 1}, {1, 7, 1}}},
        {"CH3NH2", "Methylamine", {C, N, H, H, H, H, H}, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}, {1, 5, 1}, {1, 6, 1}}},
        {"CH3Cl", "Chloromethane", {C, Cl, H, H, H}, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}}},
        {"N2H4", "Hydrazine", {N, N, H, H, H, H}, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 4, 1}, {1, 5, 1}}},
        {"Cl2", "Chlorine", {Cl, Cl}, {{0, 1, 1}}},
        {"H2O2", "Hydrogen peroxide", {O, O, H, H}, {{0, 1, 1}, {0, 2, 1}, {1, 3, 1}}},
        {"NH2OH", "Hydroxylamine", {N, O, H, H, H}, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 4, 1}}},
        {"CH2NH", "Methanimine", {C, N, H, H, H}, {{0, 1, 2}, {0, 2, 1}, {0, 3, 1}, {1, 4, 1}}},
    };
    return e;
}
struct KnownSet { std::vector<Known> list; std::vector<std::string> ids, names; std::vector<Graph> graphs; };
const KnownSet& knownSet() {
    static const KnownSet k = [] {
        KnownSet s;
        for (int i = 0; i < molecule::count(); i++) {
            const Molecule& m = get(i);
            Graph g;
            for (const Atom& a : m.atoms) g.atoms.push_back({a.el, a.x, a.y});
            int arom = 0;
            for (const Bond& b : m.bonds) g.bonds.push_back({b.a, b.b, b.order == 4 ? (arom++ % 2 == 0 ? 2 : 1) : b.order});   // benzene's ring (listed in ring order): a Kekule structure
            s.ids.push_back(m.id); s.names.push_back(m.name); s.graphs.push_back(g);
            s.list.push_back({nullptr, nullptr, i});
        }
        for (const Extra& e : extras()) {
            Graph g;
            for (std::size_t i = 0; i < e.els.size(); i++) g.atoms.push_back({e.els[i], 1.3 * double(i), 0});
            g.bonds = e.bonds;
            s.ids.push_back(e.id); s.names.push_back(e.name); s.graphs.push_back(g);
            s.list.push_back({nullptr, nullptr, -1});
        }
        for (std::size_t i = 0; i < s.list.size(); i++) { s.list[i].id = s.ids[i].c_str(); s.list[i].name = s.names[i].c_str(); }
        return s;
    }();
    return k;
}

// is b the same molecule as a (elements, bonds, bond orders), whatever the atom order? A small backtracking search; a and b have at most kMaxAtoms atoms.
bool isomorphic(const Graph& a, const Graph& b) {
    const int n = static_cast<int>(a.atoms.size());
    if (n != static_cast<int>(b.atoms.size()) || a.bonds.size() != b.bonds.size()) return false;
    if (n == 0) return true;
    std::vector<int> adjA(n * n, 0), adjB(n * n, 0), degA(n, 0), degB(n, 0), sumA(n, 0), sumB(n, 0);
    for (const SBond& x : a.bonds) { adjA[x.a * n + x.b] = adjA[x.b * n + x.a] = x.order; degA[x.a]++; degA[x.b]++; sumA[x.a] += x.order; sumA[x.b] += x.order; }
    for (const SBond& x : b.bonds) { adjB[x.a * n + x.b] = adjB[x.b * n + x.a] = x.order; degB[x.a]++; degB[x.b]++; sumB[x.a] += x.order; sumB[x.b] += x.order; }
    std::vector<int> sigA, sigB;
    for (int i = 0; i < n; i++) { sigA.push_back((a.atoms[i].el * 8 + degA[i]) * 16 + sumA[i]); sigB.push_back((b.atoms[i].el * 8 + degB[i]) * 16 + sumB[i]); }
    std::vector<int> sa = sigA, sb = sigB;
    std::sort(sa.begin(), sa.end()); std::sort(sb.begin(), sb.end());
    if (sa != sb) return false;
    // a's atoms in breadth-first order (so every atom after the first of its piece has a mapped neighbour: strong pruning)
    std::vector<int> order, seen(n, 0);
    for (int s = 0; s < n; s++) {
        if (seen[s]) continue;
        seen[s] = 1; order.push_back(s);
        for (std::size_t q = order.size() - 1; q < order.size(); q++)
            for (int v = 0; v < n; v++) if (!seen[v] && adjA[order[q] * n + v]) { seen[v] = 1; order.push_back(v); }
    }
    std::vector<int> map(n, -1), used(n, 0);
    const auto rec = [&](auto&& self, int k) -> bool {
        if (k == n) return true;
        const int u = order[k];
        for (int v = 0; v < n; v++) {
            if (used[v] || sigB[v] != sigA[u]) continue;
            bool ok = true;
            for (int j = 0; j < k && ok; j++) ok = adjA[u * n + order[j]] == adjB[v * n + map[order[j]]];
            if (!ok) continue;
            map[u] = v; used[v] = 1;
            if (self(self, k + 1)) return true;
            used[v] = 0;
        }
        map[u] = -1;
        return false;
    };
    return rec(rec, 0);
}

using V3 = std::array<double, 3>;
V3 operator-(const V3& a, const V3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V3 operator+(const V3& a, const V3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V3 operator*(double s, const V3& a) { return {s * a[0], s * a[1], s * a[2]}; }
double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 cross(const V3& a, const V3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const V3& a) { return std::sqrt(dot(a, a)); }
V3 unit(const V3& a) { const double l = norm(a); return l > 0 ? (1 / l) * a : V3{1, 0, 0}; }

bool readD(const std::string& s, double& out) {
    if (s.empty() || s.size() > 40) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !std::isfinite(v) || std::fabs(v) > 1000) return false;   // a sketch is a few units across; anything far beyond is damage
    out = v;
    return true;
}
bool readI(const std::string& s, int& out) {
    if (s.empty() || s.size() > 3) return false;
    for (char ch : s) if (ch < '0' || ch > '9') return false;
    out = std::atoi(s.c_str());
    return true;
}
std::vector<std::string> split(const std::string& s, char c) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = s.find(c, a);
        out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}
int elOf(const std::string& sym) { for (int e = 0; e < kElCount; e++) if (sym == element(e).sym) return e; return -1; }

}  // namespace

int valence(int el) { return validEl(el) ? kValence[el] : 0; }
int valenceElectrons(int el) { return validEl(el) ? kValenceE[el] : 0; }
const char* orderSymbol(int order) { return order == 2 ? "=" : order == 3 ? "#" : "-"; }
std::string bondName(int e0, int e1, int order) {
    if (!validEl(e0) || !validEl(e1)) return "?";
    if (kNameRank[e1] < kNameRank[e0]) std::swap(e0, e1);
    return std::string(element(e0).sym) + orderSymbol(order) + element(e1).sym;
}
const LengthRef* bondLength(int e0, int e1, int order) {
    for (const LengthRef& l : kLen)
        if (l.order == order && ((l.e0 == e0 && l.e1 == e1) || (l.e0 == e1 && l.e1 == e0))) return &l;
    return nullptr;
}
int lengthCount() { return kLenCount; }
const LengthRef& lengthAt(int i) { return kLen[std::clamp(i, 0, kLenCount - 1)]; }

bool Graph::operator==(const Graph& o) const {
    if (atoms.size() != o.atoms.size() || bonds.size() != o.bonds.size()) return false;
    for (std::size_t i = 0; i < atoms.size(); i++) if (atoms[i].el != o.atoms[i].el || atoms[i].x != o.atoms[i].x || atoms[i].y != o.atoms[i].y) return false;
    for (std::size_t i = 0; i < bonds.size(); i++) if (bonds[i].a != o.bonds[i].a || bonds[i].b != o.bonds[i].b || bonds[i].order != o.bonds[i].order) return false;
    return true;
}
int bondOrderSum(const Graph& g, int atom) { int s = 0; for (const SBond& b : g.bonds) if (b.a == atom || b.b == atom) s += b.order; return s; }
int openValences(const Graph& g) {
    int s = 0;
    for (int i = 0; i < static_cast<int>(g.atoms.size()); i++) s += std::max(0, valence(g.atoms[i].el) - bondOrderSum(g, i));
    return s;
}
int findBond(const Graph& g, int a, int b) {
    for (int i = 0; i < static_cast<int>(g.bonds.size()); i++) if ((g.bonds[i].a == a && g.bonds[i].b == b) || (g.bonds[i].a == b && g.bonds[i].b == a)) return i;
    return -1;
}
int pieces(const Graph& g) {
    const int n = static_cast<int>(g.atoms.size());
    std::vector<int> up(n);
    for (int i = 0; i < n; i++) up[i] = i;
    const auto root = [&](int i) { while (up[i] != i) i = up[i] = up[up[i]]; return i; };
    for (const SBond& b : g.bonds) if (b.a >= 0 && b.b >= 0 && b.a < n && b.b < n) up[root(b.a)] = root(b.b);
    int k = 0;
    for (int i = 0; i < n; i++) k += root(i) == i;
    return k;
}
bool complete(const Graph& g) { return !g.atoms.empty() && openValences(g) == 0 && pieces(g) == 1; }

std::string refuseBond(const Graph& g, int a, int b, int order) {
    const int n = static_cast<int>(g.atoms.size());
    if (a < 0 || b < 0 || a >= n || b >= n) return "no such atom";
    if (a == b) return "an atom cannot bond to itself";
    if (order < 1 || order > 3) return "a bond is single, double or triple";
    const int ex = findBond(g, a, b), exOrder = ex >= 0 ? g.bonds[ex].order : 0, extra = order - exOrder;
    if (extra > 0)
        for (int at : {a, b}) {
            const int el = g.atoms[at].el, val = valence(el), cur = bondOrderSum(g, at);
            if (cur + extra <= val) continue;
            if (cur >= val) return fmt("%s already has %d bond%s", element(el).sym, val, plural(val));
            if (exOrder == 0) return fmt("%s has room for only %d more bond%s; a %s bond needs %d", element(el).sym, val - cur, plural(val - cur), kOrderWord[order], order);
            return fmt("%s has room for only %d more bond%s; a %s bond needs %d more", element(el).sym, val - cur, plural(val - cur), kOrderWord[order], extra);
        }
    if (!bondLength(g.atoms[a].el, g.atoms[b].el, order)) return "no measured bond length for " + bondName(g.atoms[a].el, g.atoms[b].el, order) + " in our sources";
    return "";
}

std::string hill(const Graph& g) {
    int cnt[kElCount] = {};
    for (const SAtom& a : g.atoms) if (validEl(a.el)) cnt[a.el]++;
    std::string s;
    const auto put = [&](int e) { if (cnt[e]) { s += element(e).sym; if (cnt[e] > 1) s += std::to_string(cnt[e]); } };
    if (cnt[C]) { put(C); put(H); put(Cl); put(N); put(O); }
    else { put(Cl); put(H); put(N); put(O); }            // alphabetical: Cl, H, N, O
    return s;
}
Molecule toMolecule(const Graph& g) {
    Molecule m;
    for (const SAtom& a : g.atoms) m.atoms.push_back({a.el, a.x, a.y, 0});
    for (const SBond& b : g.bonds) m.bonds.push_back({b.a, b.b, b.order});
    m.hill = m.formula = hill(g);
    return m;
}
double massU(const Graph& g) { return molecule::massU(toMolecule(g)); }
double massUncU(const Graph& g) { return molecule::massUncU(toMolecule(g)); }

int knownCount() { return static_cast<int>(knownSet().list.size()); }
const Known& known(int i) { return knownSet().list[std::clamp(i, 0, knownCount() - 1)]; }
const Graph& knownGraph(int i) { return knownSet().graphs[std::clamp(i, 0, knownCount() - 1)]; }
int matchKnown(const Graph& g) {
    if (g.atoms.empty()) return -1;
    for (int i = 0; i < knownCount(); i++) if (isomorphic(knownGraph(i), g)) return i;
    return -1;
}

int lonePairs(const Graph& g, int atom) {
    if (atom < 0 || atom >= static_cast<int>(g.atoms.size())) return 0;
    const int free = valenceElectrons(g.atoms[atom].el) - bondOrderSum(g, atom);
    return free > 0 ? free / 2 : 0;
}
int steric(const Graph& g, int atom) {
    int deg = 0;
    for (const SBond& b : g.bonds) deg += b.a == atom || b.b == atom;
    return deg + lonePairs(g, atom);
}
double idealAngle(int n) { return n == 2 ? 180.0 : n == 3 ? 120.0 : n == 4 ? kTetrahedral : std::nan(""); }

bool estimateShape(const Graph& g, Molecule& out, std::string& why) {
    const int n = static_cast<int>(g.atoms.size());
    why.clear();
    if (n == 0) { why = "the sketch is empty"; return false; }
    if (n > kMaxAtoms) { why = fmt("more than %d atoms", kMaxAtoms); return false; }
    for (const SAtom& a : g.atoms) if (!validEl(a.el)) { why = "an unknown element"; return false; }
    for (const SBond& b : g.bonds) if (b.a < 0 || b.b < 0 || b.a >= n || b.b >= n || b.a == b.b) { why = "a broken bond"; return false; }
    const int pc = pieces(g);
    if (pc > 1) { why = fmt("the sketch is %d separate pieces; bond them into one molecule", pc); return false; }
    std::vector<double> blen(g.bonds.size());
    for (std::size_t i = 0; i < g.bonds.size(); i++) {
        const SBond& b = g.bonds[i];
        const LengthRef* L = bondLength(g.atoms[b.a].el, g.atoms[b.b].el, b.order);
        if (!L) { why = "no measured bond length for " + bondName(g.atoms[b.a].el, g.atoms[b.b].el, b.order) + " in our sources"; return false; }
        blen[i] = L->angstrom;
    }
    std::vector<std::vector<int>> nb(n), nbBond(n);                        // neighbours in index order, and the bond to each
    for (int a = 0; a < n; a++)
        for (int b = 0; b < n; b++) { const int k = findBond(g, a, b); if (k >= 0) { nb[a].push_back(b); nbBond[a].push_back(k); } }
    std::vector<int> slots(n);
    for (int a = 0; a < n; a++) {
        const int deg = static_cast<int>(nb[a].size());
        if (deg > 4) { why = "an atom with more than 4 bonded neighbours"; return false; }
        slots[a] = std::max(deg, std::min(4, steric(g, a)));
    }
    const auto lenOf = [&](int a, int b) { for (std::size_t k = 0; k < nb[a].size(); k++) if (nb[a][k] == b) return blen[nbBond[a][k]]; return 0.0; };

    // ---- 1. place by walking the graph ----
    std::vector<V3> P(n, V3{0, 0, 0});
    std::vector<int> placed(n, 0), parent(n, -1), queue;
    // a ring? The first bond outside a breadth-first spanning tree from atom 0 closes one: lay that ring out flat as a regular polygon first
    {
        std::vector<int> tp(n, -1), seen(n, 0), q{0};
        seen[0] = 1;
        std::vector<int> treeBond(g.bonds.size(), 0);
        for (std::size_t i = 0; i < q.size(); i++)
            for (std::size_t k = 0; k < nb[q[i]].size(); k++) {
                const int v = nb[q[i]][k];
                if (!seen[v]) { seen[v] = 1; tp[v] = q[i]; treeBond[nbBond[q[i]][k]] = 1; q.push_back(v); }
            }
        int close = -1;
        for (std::size_t i = 0; i < g.bonds.size() && close < 0; i++) if (!treeBond[i]) close = static_cast<int>(i);
        if (close >= 0) {
            std::vector<int> pu, pv;                                     // the tree paths from each end up to the root
            for (int x = g.bonds[close].a; x >= 0; x = tp[x]) pu.push_back(x);
            for (int x = g.bonds[close].b; x >= 0; x = tp[x]) pv.push_back(x);
            while (pu.size() > 1 && pv.size() > 1 && pu[pu.size() - 2] == pv[pv.size() - 2]) { pu.pop_back(); pv.pop_back(); }   // up to the lowest common ancestor
            std::vector<int> ring(pu.begin(), pu.end());                 // a ... lca
            for (int k = static_cast<int>(pv.size()) - 2; k >= 0; k--) ring.push_back(pv[k]);   // ... b
            const int m = static_cast<int>(ring.size());
            double sum = 0;
            for (int k = 0; k < m; k++) sum += lenOf(ring[k], ring[(k + 1) % m]);
            const double rr = (sum / m) / (2 * std::sin(kPi / m));
            for (int k = 0; k < m; k++) {
                P[ring[k]] = {rr * std::cos(2 * kPi * k / m), rr * std::sin(2 * kPi * k / m), 0};
                placed[ring[k]] = 1;
                parent[ring[k]] = ring[(k + m - 1) % m];
                queue.push_back(ring[k]);
            }
        }
    }
    if (queue.empty()) {                                                   // no ring: start at the atom with the most neighbours (the lowest index of those)
        int root = 0;
        for (int a = 1; a < n; a++) if (nb[a].size() > nb[root].size()) root = a;
        placed[root] = 1;
        queue.push_back(root);
    }
    const double s8 = std::sqrt(8.0) / 3, s2 = std::sqrt(2.0) / 3, s6 = std::sqrt(6.0) / 3, c120 = -0.5, s120 = std::sqrt(3.0) / 2;
    for (std::size_t qi = 0; qi < queue.size(); qi++) {
        const int a = queue[qi];
        bool any = false;
        for (int v : nb[a]) any |= !placed[v];
        if (!any) continue;
        // the ideal directions for its steric number, in a local frame (e0, e1, e2): slot 0 along e0
        V3 dirs[4];
        int ns = slots[a];
        if (ns == 1) dirs[0] = {1, 0, 0};
        else if (ns == 2) { dirs[0] = {1, 0, 0}; dirs[1] = {-1, 0, 0}; }
        else if (ns == 3) { dirs[0] = {1, 0, 0}; dirs[1] = {c120, s120, 0}; dirs[2] = {c120, -s120, 0}; }
        else { ns = 4; dirs[0] = {1, 0, 0}; dirs[1] = {-1.0 / 3, s8, 0}; dirs[2] = {-1.0 / 3, -s2, s6}; dirs[3] = {-1.0 / 3, -s2, -s6}; }
        V3 e0{1, 0, 0}, e1{0, 1, 0};
        const int p = parent[a];
        if (p >= 0) {
            e0 = unit(P[p] - P[a]);
            // slot 1 is anti (torsion 180) to a placed neighbour of the parent: staggered by default
            V3 w{0, 0, 0};
            for (int r : nb[p]) if (r != a && placed[r]) { const V3 d = P[r] - P[p]; w = d - dot(d, e0) * e0; if (norm(w) > 1e-9) break; }
            if (norm(w) > 1e-9) e1 = -1.0 * unit(w);
            else {
                for (const V3& fx : {V3{0, 0, 1}, V3{0, 1, 0}, V3{1, 0, 0}}) { w = fx - dot(fx, e0) * e0; if (norm(w) > 1e-6) break; }
                e1 = unit(w);
            }
        }
        const V3 e2 = cross(e0, e1);
        V3 world[4];
        for (int k = 0; k < ns; k++) world[k] = dirs[k][0] * e0 + dirs[k][1] * e1 + dirs[k][2] * e2;
        bool used[4] = {false, false, false, false};
        if (p >= 0) used[0] = true;
        for (int v : nb[a]) {                                              // the other neighbours already placed (a ring) take the slot nearest to them
            if (v == p || !placed[v]) continue;
            const V3 d = unit(P[v] - P[a]);
            int best = -1;
            for (int k = 0; k < ns; k++) if (!used[k] && (best < 0 || dot(world[k], d) > dot(world[best], d))) best = k;
            if (best >= 0) used[best] = true;
        }
        for (int v : nb[a]) {
            if (placed[v]) continue;
            int k = 0;
            while (k < ns && used[k]) k++;
            const V3 d = k < ns ? world[k] : world[0];                     // (never: there are at least as many slots as neighbours)
            if (k < ns) used[k] = true;
            P[v] = P[a] + lenOf(a, v) * d;
            placed[v] = 1;
            parent[v] = a;
            queue.push_back(v);
        }
    }

    // ---- 2. relax: springs on the bonds and on the 1-3 distances of the ideal angles, a short-range repulsion between the other pairs (FIRE minimiser, fixed steps: deterministic) ----
    struct Spring { int i, j; double d0, k; };
    std::vector<Spring> springs;
    std::vector<int> near(n * n, 0);                                       // 1 = bonded or 1-3 (no repulsion)
    for (std::size_t b = 0; b < g.bonds.size(); b++) { springs.push_back({g.bonds[b].a, g.bonds[b].b, blen[b], 1.0}); near[g.bonds[b].a * n + g.bonds[b].b] = near[g.bonds[b].b * n + g.bonds[b].a] = 1; }
    for (int c = 0; c < n; c++) {
        if (nb[c].size() < 2) continue;
        const double th = idealAngle(std::min(4, slots[c])) * kPi / 180;
        for (std::size_t x = 0; x < nb[c].size(); x++)
            for (std::size_t y = x + 1; y < nb[c].size(); y++) {
                const int i = nb[c][x], j = nb[c][y];
                if (near[i * n + j]) continue;
                const double li = blen[nbBond[c][x]], lj = blen[nbBond[c][y]];
                springs.push_back({i, j, std::sqrt(std::max(0.0, li * li + lj * lj - 2 * li * lj * std::cos(th))), 1.0});
                near[i * n + j] = near[j * n + i] = 1;
            }
    }
    constexpr double kRmin = 2.0, kRep = 0.2;                             // repulsion inside 2.0 angstrom (a modelling choice, not a measured radius)
    std::vector<V3> F(n), Vv(n, V3{0, 0, 0});
    const auto forces = [&]() {
        double fmax = 0;
        for (V3& f : F) f = {0, 0, 0};
        for (const Spring& s : springs) {
            const V3 d = P[s.j] - P[s.i];
            const double l = norm(d);
            if (l < 1e-12) continue;
            const V3 f = (2 * s.k * (l - s.d0) / l) * d;                    // pulls i towards j when stretched
            F[s.i] = F[s.i] + f; F[s.j] = F[s.j] - f;
        }
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                if (near[i * n + j]) continue;
                const V3 d = P[j] - P[i];
                const double l = norm(d);
                if (l >= kRmin || l < 1e-12) continue;
                const V3 f = (-2 * kRep * (kRmin - l) / l) * d;             // pushes them apart
                F[i] = F[i] + f; F[j] = F[j] - f;
            }
        for (const V3& f : F) fmax = std::max(fmax, norm(f));
        return fmax;
    };
    double dt = 0.02, alpha = 0.1;
    int good = 0;
    if (forces() > 1e-10)
        for (int it = 0; it < 6000; it++) {
            double pw = 0;
            for (int i = 0; i < n; i++) pw += dot(F[i], Vv[i]);
            if (pw > 0) {
                double vn = 0, fn = 0;
                for (int i = 0; i < n; i++) { vn += dot(Vv[i], Vv[i]); fn += dot(F[i], F[i]); }
                vn = std::sqrt(vn); fn = std::sqrt(fn);
                if (fn > 0) for (int i = 0; i < n; i++) Vv[i] = (1 - alpha) * Vv[i] + (alpha * vn / fn) * F[i];
                if (++good > 5) { dt = std::min(dt * 1.1, 0.1); alpha *= 0.99; }
            } else {
                for (V3& v : Vv) v = {0, 0, 0};
                dt *= 0.5; alpha = 0.1; good = 0;
            }
            for (int i = 0; i < n; i++) { Vv[i] = Vv[i] + dt * F[i]; P[i] = P[i] + dt * Vv[i]; }
            if (forces() < 1e-10) break;
        }

    // ---- 3. the molecule, placed as every shape here (centre of mass, principal axes) ----
    out = Molecule{};
    for (int a = 0; a < n; a++) out.atoms.push_back({g.atoms[a].el, P[a][0], P[a][1], P[a][2]});
    for (const SBond& b : g.bonds) out.bonds.push_back({b.a, b.b, b.order});
    out.id = "built";
    out.hill = out.formula = hill(g);
    const int k = matchKnown(g);
    out.name = k >= 0 ? known(k).name : "";
    out.shapeNote = "Estimated, not measured: ideal VSEPR angles and one CCCBDB bond length per bond type.";
    placePrincipal(out);
    return true;
}

// ---- the editor ----
void Editor::step() {
    undo_.push_back(g_);
    if (static_cast<int>(undo_.size()) > kUndoSteps) undo_.erase(undo_.begin());
    redo_.clear();
}
int Editor::addAtom(int el, double x, double y) {
    if (!validEl(el)) { message = "only H, C, N, O and Cl"; return -1; }
    if (static_cast<int>(g_.atoms.size()) >= kMaxAtoms) { message = fmt("at most %d atoms in a sketch (the estimated shape is built for up to %d)", kMaxAtoms, kMaxAtoms); return -1; }
    if (!std::isfinite(x) || !std::isfinite(y)) return -1;
    step();
    g_.atoms.push_back({el, std::clamp(x, -999.0, 999.0), std::clamp(y, -999.0, 999.0)});
    rev_++;
    message.clear();
    picked = static_cast<int>(g_.atoms.size()) - 1;
    return picked;
}
bool Editor::bond(int a, int b, int order) {
    if (findBond(g_, a, b) >= 0) { message = "these two atoms are already bonded"; return false; }
    const std::string why = refuseBond(g_, a, b, order);
    if (!why.empty()) { message = why; return false; }
    step();
    g_.bonds.push_back({a, b, order});
    rev_++;
    message.clear();
    return true;
}
void Editor::beginDrag(int atom) {
    if (atom < 0 || atom >= static_cast<int>(g_.atoms.size())) return;
    dragStart_ = g_;
    dragging_ = true;
    picked = atom;
}
void Editor::moveAtom(int atom, double x, double y) {
    if (atom < 0 || atom >= static_cast<int>(g_.atoms.size()) || !std::isfinite(x) || !std::isfinite(y)) return;
    g_.atoms[atom].x = std::clamp(x, -999.0, 999.0);
    g_.atoms[atom].y = std::clamp(y, -999.0, 999.0);
    rev_++;
}
int Editor::drop(int atom) {
    const int n = static_cast<int>(g_.atoms.size());
    int made = -1;
    if (atom >= 0 && atom < n) {
        int best = -1;
        double bd = kSnapRadius;
        for (int j = 0; j < n; j++) {
            if (j == atom || findBond(g_, atom, j) >= 0) continue;
            const double d = std::hypot(g_.atoms[j].x - g_.atoms[atom].x, g_.atoms[j].y - g_.atoms[atom].y);
            if (d < bd) { bd = d; best = j; }
        }
        if (best >= 0) {
            const std::string why = refuseBond(g_, best, atom, 1);
            if (!why.empty()) message = why;
            else {
                double dx = g_.atoms[atom].x - g_.atoms[best].x, dy = g_.atoms[atom].y - g_.atoms[best].y;
                const double l = std::hypot(dx, dy);
                if (l < 1e-9) { dx = 1; dy = 0; } else { dx /= l; dy /= l; }
                g_.atoms[atom].x = g_.atoms[best].x + kSketchBond * dx;
                g_.atoms[atom].y = g_.atoms[best].y + kSketchBond * dy;
                g_.bonds.push_back({best, atom, 1});
                made = static_cast<int>(g_.bonds.size()) - 1;
                message.clear();
            }
        }
    }
    if (dragging_ && !(g_ == dragStart_)) {                                // the whole drag (and its snap) is one undo step
        undo_.push_back(dragStart_);
        if (static_cast<int>(undo_.size()) > kUndoSteps) undo_.erase(undo_.begin());
        redo_.clear();
    }
    dragging_ = false;
    rev_++;
    return made;
}
int Editor::cycleBond(int b) {
    if (b < 0 || b >= static_cast<int>(g_.bonds.size())) return 0;
    const int cur = g_.bonds[b].order;
    std::string first;
    int firstOrder = 0;
    for (int k = 1; k <= 2; k++) {                                         // 1 -> 2 -> 3 -> 1: the next orders in turn, skipping a refused one
        const int next = (cur + k - 1) % 3 + 1;
        const std::string why = refuseBond(g_, g_.bonds[b].a, g_.bonds[b].b, next);
        if (why.empty()) {
            step();
            g_.bonds[b].order = next;
            rev_++;
            message = first.empty() ? std::string() : std::string(kOrderWord[firstOrder]) + " bond skipped: " + first;
            return next;
        }
        if (first.empty()) { first = why; firstOrder = next; }
        if (next == 1) break;
    }
    message = first;                                                       // nothing allowed: it stays as it is, and the reason is shown
    return cur;
}
bool Editor::deleteAtom(int atom) {
    if (atom < 0 || atom >= static_cast<int>(g_.atoms.size())) return false;
    step();
    std::vector<SBond> keep;
    for (const SBond& b : g_.bonds)
        if (b.a != atom && b.b != atom) keep.push_back({b.a > atom ? b.a - 1 : b.a, b.b > atom ? b.b - 1 : b.b, b.order});
    g_.bonds = keep;
    g_.atoms.erase(g_.atoms.begin() + atom);
    picked = -1;
    rev_++;
    message.clear();
    return true;
}
void Editor::clear() {
    if (g_.atoms.empty()) return;
    step();
    g_ = Graph{};
    picked = -1;
    rev_++;
    message.clear();
}
bool Editor::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(g_);
    g_ = undo_.back();
    undo_.pop_back();
    picked = -1; message.clear(); rev_++;
    return true;
}
bool Editor::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(g_);
    g_ = redo_.back();
    redo_.pop_back();
    picked = -1; message.clear(); rev_++;
    return true;
}
void Editor::load(const Graph& g) { g_ = g; undo_.clear(); redo_.clear(); picked = -1; message.clear(); rev_++; }
int Editor::hitAtom(double x, double y) const {
    for (int i = static_cast<int>(g_.atoms.size()) - 1; i >= 0; i--)
        if (std::hypot(g_.atoms[i].x - x, g_.atoms[i].y - y) <= kAtomRadius * 1.2) return i;
    return -1;
}
int Editor::hitBond(double x, double y, double hw) const {
    for (int i = static_cast<int>(g_.bonds.size()) - 1; i >= 0; i--) {
        const SAtom &A = g_.atoms[g_.bonds[i].a], &B = g_.atoms[g_.bonds[i].b];
        const double ux = B.x - A.x, uy = B.y - A.y, L2 = ux * ux + uy * uy;
        const double t = L2 > 1e-12 ? std::clamp(((x - A.x) * ux + (y - A.y) * uy) / L2, 0.0, 1.0) : 0.0;
        if (std::hypot(A.x + t * ux - x, A.y + t * uy - y) <= hw) return i;
    }
    return -1;
}

// ---- settings ----
namespace {
std::string num(double v) {                                                // the shortest of %.15g / %.17g that reads back bit for bit
    char b[40];
    std::snprintf(b, sizeof b, "%.15g", v);
    if (std::strtod(b, nullptr) != v) std::snprintf(b, sizeof b, "%.17g", v);
    return b;
}
}  // namespace
std::string saveBuild(const BuildSettings& s) {
    std::ostringstream o;
    o << "v1 mode=" << (s.build ? "build" : "viewer") << " view=" << (s.view == 1 ? "3d" : "sketch") << " el=" << element(s.element).sym << " atoms=";
    for (std::size_t i = 0; i < s.graph.atoms.size(); i++) o << (i ? "/" : "") << element(s.graph.atoms[i].el).sym << ":" << num(s.graph.atoms[i].x) << ":" << num(s.graph.atoms[i].y);
    o << " bonds=";
    for (std::size_t i = 0; i < s.graph.bonds.size(); i++) o << (i ? "/" : "") << s.graph.bonds[i].a << ":" << s.graph.bonds[i].b << ":" << s.graph.bonds[i].order;
    return o.str();
}
BuildSettings loadBuild(const std::string& text) {
    const BuildSettings def;
    BuildSettings s;
    std::istringstream in(text);
    std::string tok;
    if (!(in >> tok) || tok != "v1") return def;                          // another version (or nothing): the defaults
    std::string atoms, bonds;
    while (in >> tok) {
        const std::size_t eq = tok.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
        if (k == "mode") { if (v == "build") s.build = true; else if (v == "viewer") s.build = false; else return def; }
        else if (k == "view") { if (v == "3d") s.view = 1; else if (v == "sketch") s.view = 0; else return def; }
        else if (k == "el") { const int e = elOf(v); if (e < 0) return def; s.element = e; }
        else if (k == "atoms") atoms = v;
        else if (k == "bonds") bonds = v;
    }
    Graph g;
    for (const std::string& a : split(atoms, '/')) {
        const std::vector<std::string> f = split(a, ':');
        SAtom at{};
        if (f.size() != 3 || (at.el = elOf(f[0])) < 0 || !readD(f[1], at.x) || !readD(f[2], at.y)) return def;
        g.atoms.push_back(at);
        if (static_cast<int>(g.atoms.size()) > kMaxAtoms) return def;
    }
    for (const std::string& b : split(bonds, '/')) {
        const std::vector<std::string> f = split(b, ':');
        SBond bd{};
        if (f.size() != 3 || !readI(f[0], bd.a) || !readI(f[1], bd.b) || !readI(f[2], bd.order)) return def;
        if (bd.a >= static_cast<int>(g.atoms.size()) || bd.b >= static_cast<int>(g.atoms.size()) || findBond(g, bd.a, bd.b) >= 0) return def;
        if (!refuseBond(g, bd.a, bd.b, bd.order).empty()) return def;      // valence, order, table length, itself
        g.bonds.push_back(bd);
    }
    s.graph = g;
    return s;
}

}  // namespace molecule::build
