#include "chart.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

#include "atom_core.h"
#include "decay_chain.h"
#include "nubase.h"
#include "quadrupole.h"
#include "radii.h"

namespace atom {

// Colours: the usual chart colours (beta minus blue, beta plus / electron capture red, alpha yellow, fission green), chosen here (no standard is cited); stable uses the page's text colour.
static const ChartClassInfo kInfo[kChartClasses] = {
    {"stable", "", "Stable: no decay has ever been seen."},
    {"beta minus (β⁻)", "#3b82f6", "Beta minus: a neutron turns into a proton, so the nucleus moves up one square. It has too many neutrons."},
    {"beta plus / electron capture (β⁺/EC)", "#ef4444", "Beta plus or electron capture: a proton turns into a neutron, so the nucleus moves down one square. It has too many protons."},
    {"alpha (α)", "#eab308", "Alpha: it throws out a helium-4 nucleus (2 protons, 2 neutrons) and moves down two squares and left two. Common in heavy nuclei."},
    {"spontaneous fission", "#22c55e", "Fission: it splits in two on its own. Only the very heaviest nuclei."},
    {"proton emission", "#f97316", "Proton emission: it spits out a proton. Nuclei far too short of neutrons."},
    {"neutron emission", "#a855f7", "Neutron emission: it spits out a neutron. Nuclei far too rich in neutrons."},
    {"other", "#92400e", "Another way, such as a heavier cluster being thrown out."},
    {"several ways, odds not measured", "#9ca3af", "More than one kind of decay is listed and their odds have not been measured, so no single colour is honest."}};
const ChartClassInfo& chartClassInfo(ChartClass c) { return kInfo[int(c)]; }

ChartClass chartClassOfMode(const std::string& m) {
    if (m == "A") return ChartClass::Alpha;
    if (m == "SF") return ChartClass::Fission;
    if (m == "2B-" || m.compare(0, 2, "B-") == 0) return ChartClass::BetaMinus;
    if (m == "2B+" || m.compare(0, 2, "B+") == 0) return ChartClass::BetaPlus;
    if (m == "p" || m == "2p" || m == "3p") return ChartClass::Proton;
    if (m == "n" || m == "2n" || m == "3n") return ChartClass::Neutron;
    return ChartClass::Other;
}

ChartClass chartClassOf(int Z, int N) {
    const std::string* c = nucCode(Z, N);
    if (c && *c == "S") return ChartClass::Stable;
    const auto d = decInfo(Z, N);
    if (!d || d->br.empty()) return ChartClass::Other;           // (no table nuclide has no mode: counted by the test)
    if (d->unk) {
        const ChartClass first = chartClassOfMode(d->br[0].first);
        for (const auto& b : d->br) if (chartClassOfMode(b.first) != first) return ChartClass::Mixed;
        return first;
    }
    std::vector<std::pair<ChartClass, double>> tot;              // the odds added up per kind, in the order the kinds first appear (a beta-delayed branch adds to its beta)
    for (const auto& b : d->br) {
        const ChartClass k = chartClassOfMode(b.first);
        auto it = std::find_if(tot.begin(), tot.end(), [k](const auto& t) { return t.first == k; });
        if (it == tot.end()) tot.push_back({k, b.second}); else it->second += b.second;
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i < tot.size(); i++) if (tot[i].second > tot[best].second) best = i;   // the first one on a tie
    return tot[best].first;
}

const std::vector<ChartTile>& chartTiles() {
    static const std::vector<ChartTile> t = [] {
        std::vector<ChartTile> v;
        for (int Z = 1; Z <= 118; Z++)
            for (const auto& kv : nuclides(Z)) v.push_back({short(Z), short(kv.first), chartClassOf(Z, kv.first)});
        return v;
    }();
    return t;
}
int chartClassCount(ChartClass c) {
    static const std::array<int, kChartClasses> n = [] { std::array<int, kChartClasses> a{}; for (const ChartTile& t : chartTiles()) a[int(t.cls)]++; return a; }();
    return n[int(c)];
}
bool chartHas(int Z, int N) { return Z >= 1 && Z <= 118 && nucCode(Z, N) != nullptr; }
int chartMaxN() { static const int m = [] { int k = 0; for (const ChartTile& t : chartTiles()) k = std::max<int>(k, t.N); return k; }(); return m; }
int chartMaxZ() { return 118; }

const char* chartMagicWords() {
    return "Magic numbers: 2, 8, 20, 28, 50, 82 and 126. A nucleus with this many protons or neutrons has a filled shell of nucleons, like the filled electron shells of the noble gases, "
           "and is held together unusually tightly (the nuclear shell model). The lines mark those rows and columns.";
}

const char* chartMagicShort() { return "Lines: the magic numbers 2, 8, 20, 28, 50, 82, 126, where a nuclear shell is full (held unusually tightly)."; }

ChartTile chartValleyAnchor() {
    static const ChartTile t = [] {
        std::vector<ChartTile> s;
        for (const ChartTile& x : chartTiles()) if (x.cls == ChartClass::Stable) s.push_back(x);      // already in order of Z, then N
        return s[s.size() / 2];
    }();
    return t;
}
int chartHeaviestZ() { static const int z = [] { int m = 0; for (const ChartTile& t : chartTiles()) m = std::max<int>(m, t.Z); return m; }(); return z; }
std::vector<ChartTile> chartHeaviestTiles() { std::vector<ChartTile> v; for (const ChartTile& t : chartTiles()) if (t.Z == chartHeaviestZ()) v.push_back(t); return v; }
const char* chartValleyWords() {
    return "Valley of stability: the narrow band of stable and very long-lived nuclides (the black tiles, the ridge in the landscape) that runs through the whole chart. It is measured, not predicted: "
           "nuclides on either side decay back towards it.";
}
const char* chartValleyShort() { return "Valley of stability: the band of stable nuclides (measured)."; }
const char* chartIslandWords() {
    static const std::string w = "Island of stability: a PREDICTION, not something seen. Theory expects a region of superheavy nuclides beyond the heaviest known (Z = " + std::to_string(chartHeaviestZ()) +
        ") that might live far longer than their neighbours. None of them has been observed, and where it lies is disputed. This lab has no sourced prediction to draw, so nothing is drawn there.";
    return w.c_str();
}
const char* chartIslandShort() {
    static const std::string w = "Island of stability: a prediction, not seen, beyond Z = " + std::to_string(chartHeaviestZ()) + "; not drawn (no sourced prediction).";
    return w.c_str();
}
std::string chartHeaviestLabel() { return "heaviest known: Z = " + std::to_string(chartHeaviestZ()) + " (" + [] { std::string n = name(chartHeaviestZ()); n[0] = char(std::tolower(static_cast<unsigned char>(n[0]))); return n; }() + "); nothing observed beyond"; }

static double g_insetTop = 0, g_insetBottom = 0;
void setChartInsets(double top, double bottom) { g_insetTop = top; g_insetBottom = bottom; }

ChartLayout chartLayout(double w, double h) {
    ChartLayout L;
    L.top += g_insetTop; L.bottom += g_insetBottom;
    L.nMax = chartMaxN(); L.zMax = chartMaxZ();
    const double availW = std::max(10.0, w - L.left - L.right), availH = std::max(10.0, h - L.top - L.bottom);
    L.cell = std::max(1.0, std::min(availW / (L.nMax + 1), availH / L.zMax));
    L.x0 = L.left;
    L.y0 = h - L.bottom - L.zMax * L.cell;
    return L;
}

bool chartTileAt(const ChartLayout& L, double px, double py, int& Z, int& N) {
    const double fx = (px - L.x0) / L.cell, fy = (py - L.y0) / L.cell;
    if (fx < 0 || fy < 0) return false;
    const int n = int(std::floor(fx)), z = L.zMax - int(std::floor(fy));
    if (n > L.nMax || z < 1 || z > L.zMax || !chartHas(z, n)) return false;
    Z = z; N = n;
    return true;
}

double chartFreeHeight(const ChartLayout& L, double h, bool fromTop, double x0, double x1) {
    static const std::vector<std::pair<int, int>> col = [] {                  // per column N: the lowest and the highest Z that has a tile (0, 0: none)
        std::vector<std::pair<int, int>> c(chartMaxN() + 1, {1000, 0});
        for (const ChartTile& t : chartTiles()) { auto& e = c[t.N]; e.first = std::min<int>(e.first, t.Z); e.second = std::max<int>(e.second, t.Z); }
        return c;
    }();
    const double edgeTop = L.top, edgeBottom = h - L.bottom;
    double free = edgeBottom - edgeTop;
    const int n0 = std::max(0, int(std::floor((x0 - L.x0) / L.cell))), n1 = std::min(L.nMax, int(std::floor((x1 - L.x0) / L.cell)));
    for (int n = n0; n <= n1; n++) {
        if (col[n].second == 0) continue;
        free = std::min(free, fromTop ? L.y(col[n].second) - edgeTop : edgeBottom - (L.y(col[n].first) + L.cell));
    }
    return std::max(0.0, free);
}

bool chartMove(int& Z, int& N, int dZ, int dN) {
    if (!chartHas(Z + dZ, N + dN)) return false;
    Z += dZ; N += dN;
    return true;
}

static std::string pct(double v) { return v >= 1e-4 ? jsNum(v * 100, 2) : sci(v * 100); }

std::vector<std::string> chartHover(int Z, int N) {
    std::vector<std::string> out;
    const std::string zn = "  ·  Z " + std::to_string(Z) + " (protons), N " + std::to_string(N) + " (neutrons)";
    const std::string* c = nucCode(Z, N);
    if (!c) {
        out.push_back(std::string(Z >= 1 && Z <= 118 ? symbol(Z) : "?") + "-" + std::to_string(Z + N) + zn);
        out.push_back("Not in the table: NUBASE2020 lists no such nuclide (never observed, or it falls apart at once). The lab's neutron slider can still show it as a ball of nucleons.");
        return out;
    }
    out.push_back(nuclideLabel(Z, N) + zn);
    const NubaseRow* nb = nubase(Z, N);
    // stable, or the half-life
    if (*c == "S") {
        out.push_back("Stable: no decay has been seen.");
        if (nb && (*nb->lim || *nb->modes)) {
            std::string t = "NUBASE2020 also gives, for this stable nuclide:";
            if (*nb->lim) t += std::string(" a lower limit for the half-life, ") + nb->lim + ";";
            if (*nb->modes) t += std::string(" the decay entries \"") + nb->modes + "\" (the file marks them with a question mark; it does not say more).";
            out.push_back(t);
        }
    } else if (*c == "?") {
        if (!nb) out.push_back("Half-life: not known.");
        else if (nb->kind == 'E') out.push_back("Half-life: not measured (NUBASE2020 has only an extrapolation from systematics, which the lab does not show).");
        else if (nb->kind == 'B') out.push_back(std::string("Half-life: only a limit is known, ") + nb->lim + ".");
        else if (nb->kind == 'A') out.push_back(std::string("Half-life: only an approximate value is known, ") + nb->lim + ".");
        else if (nb->kind == 'P') out.push_back("Half-life: not given (NUBASE2020 says only that it is unstable to emitting a particle).");
        else out.push_back("Half-life: not given by NUBASE2020.");
    } else {
        std::string t = isoInfo(Z, N);
        t[0] = char(std::toupper(static_cast<unsigned char>(t[0])));
        out.push_back(t + ".");
        // (0.12 stage 2: the lab's table no longer disagrees with NUBASE2020 about which nuclides are stable, so there is no "the table has a number instead" line; a test checks every tile)
    }
    // how it decays
    if (*c != "S") {
        const auto d = decInfo(Z, N);
        if (!d || d->br.empty()) out.push_back("Decay: no mode is listed.");
        else {
            std::string t = "Decays by: ";
            for (std::size_t i = 0; i < d->br.size(); i++) {
                const auto& [m, v] = d->br[i];
                t += (i ? "; " : "") + decWord(m) + (!d->unk && !std::isnan(v) && (v < 0.9995 || d->br.size() > 1) ? " " + pct(v) + " %" : "");
            }
            out.push_back(t + (d->unk ? " (the odds are not measured)." : "."));
            const std::string unq = decUnquantifiedWords(Z, N);
            if (!unq.empty()) out.push_back(unq);
        }
    }
    // the spin
    if (nb) {
        const SpinText sp = nubaseSpin(*nb);
        static const char* const how[] = {"", " (directly measured)", " (estimated from systematics, not measured)", " (tentative)", " (as evaluated; not marked as directly measured)"};
        out.push_back(sp.how == 0 ? "Spin and parity: not given by NUBASE2020." : "Spin and parity: " + sp.text + how[sp.how] + ".");
    } else out.push_back("Spin and parity: not given.");
    // measured size and shape (0.10.9)
    const RadiusRow* rr = chargeRadius(Z, N);
    out.push_back(sizedByMeasurement(Z, N) ? "Charge radius: measured, rms " + jsNum(rr->rms, 5) + " fm (IAEA table)." : "Charge radius: no measured value in the IAEA table; the lab draws its usual size.");
    const QuadRow* q = quadRow(Z, N);
    out.push_back(q && !std::isnan(q->Qb) ? "Quadrupole moment: measured, Q = " + std::string(q->raw) + " b (IAEA table)." : "Quadrupole moment: no usable measured value in the IAEA table.");
    return out;
}

}  // namespace atom
