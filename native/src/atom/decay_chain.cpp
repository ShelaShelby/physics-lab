#include "decay_chain.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include "nubase.h"

namespace atom {

// ---------------------------------------------------------------- the two draws
std::vector<double> decOdds(const DecayInfo& d) {
    std::vector<double> o(d.br.size(), 0.0);
    double sum = 0;
    for (const auto& b : d.br) if (std::isfinite(b.second) && b.second > 0) sum += b.second;
    for (std::size_t i = 0; i < o.size(); i++) o[i] = sum > 0 ? (std::isfinite(d.br[i].second) && d.br[i].second > 0 ? d.br[i].second / sum : 0.0) : 1.0 / double(o.size());
    return o;
}
double decDrawLifetime(double halfLifeS, double u) { return -halfLifeS / kLn2 * std::log(1 - u); }
std::size_t decDrawBranch(const DecayInfo& d, double u) {
    const std::vector<double> o = decOdds(d);
    double r = u;
    for (std::size_t i = 0; i < o.size(); i++) { if (r < o[i]) return i; r -= o[i]; }
    return o.empty() ? 0 : o.size() - 1;
}

// ---------------------------------------------------------------- who can be timed
ChainTiming chainTiming(int Z, int N) {
    const ChartHeight h = chartHeightOf(Z, N);
    if (h.kind == HeightKind::Measured || h.kind == HeightKind::Approx) {
        const double T = chartHalfLifeOf(Z, N);
        if (T > 0) return {h.kind == HeightKind::Measured ? ChainTime::Measured : ChainTime::Approximate, T, h.kind};
        return {ChainTime::None, 0, HeightKind::Unknown};
    }
    return {ChainTime::None, 0, h.kind == HeightKind::Limit || h.kind == HeightKind::Extrapolated ? h.kind : HeightKind::Unknown};
}

// ---------------------------------------------------------------- the words (first: they are used by the graph)
static std::string lower(std::string s) { for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string chainNuclide(int Z, int N) { return lower(name(Z)) + "-" + std::to_string(Z + N); }
static std::string signedN(int v) { return (v > 0 ? "+" : v < 0 ? "−" : "") + std::to_string(std::abs(v)); }
static std::string pctOf(double v) {                          // 3 digits; close to 100 % enough digits that it does not read as certain (99.999946 %, not 100 %)
    if (v >= 1 - 1e-12) return "100";
    if (v >= 0.999) return jsNum(v * 100, std::min(15, int(std::ceil(-std::log10(1 - v))) + 1));
    return v >= 1e-4 ? jsNum(v * 100, 3) : sci(v * 100);
}

std::string chainModeShort(const std::string& m) {
    if (m == "A") return "α";
    if (m == "B-") return "β⁻";
    if (m == "B+") return "β⁺/EC";
    if (m == "2B-") return "2β⁻";
    if (m == "2B+") return "2β⁺";
    std::string out;
    for (std::size_t i = 0; i < m.size(); i++) {
        if (m.compare(i, 2, "B-") == 0) { out += "β⁻"; i++; }
        else if (m.compare(i, 2, "B+") == 0) { out += "β⁺"; i++; }
        else if (m[i] == 'A') out += "α";
        else out += m[i];
    }
    return out;
}
std::string chainModeWords(const std::string& m, int dZ, int dN) {
    if (m == "A") return "alpha: two protons and two neutrons leave";
    if (m == "B-") return "beta minus: a neutron turns into a proton, and an electron and an antineutrino leave";
    if (m == "B+") return "beta plus or electron capture: a proton turns into a neutron";
    if (m == "2B-") return "double beta minus: two neutrons turn into two protons";
    if (m == "2B+") return "double beta plus or double capture: two protons turn into two neutrons";
    if (m == "p") return "proton emission: one proton leaves";
    if (m == "2p") return "two-proton emission: two protons leave";
    if (m == "n") return "neutron emission: one neutron leaves";
    if (m == "2n" || m == "3n") return std::string(m == "2n" ? "two" : "three") + "-neutron emission: " + (m == "2n" ? "two" : "three") + " neutrons leave";
    if (m == "SF" || m == "B-SF" || m == "B+SF") return decWord(m) + ": the nucleus splits in two";
    return decWord(m) + " (" + signedN(dZ) + " protons, " + signedN(dN) + " neutrons)";
}

static std::string stopText(int Z, int N, ChainStop stop, HeightKind why) {
    const std::string nm = chainNuclide(Z, N);
    if (stop == ChainStop::Stable) {
        const NubaseRow* nb = nubase(Z, N);
        if (nb && (*nb->lim || *nb->modes))
            return nm + " is observationally stable: no decay has been seen, but NUBASE2020 lists " + (*nb->lim ? std::string("a half-life limit, ") + nb->lim + (*nb->modes ? " and " : "") : std::string()) +
                   (*nb->modes ? std::string("a decay entry marked with a question mark, \"") + nb->modes + "\"" : std::string()) + " for it, so it is not proven stable.";
        return nm + " is stable: no decay has been seen.";
    }
    if (stop == ChainStop::NoHalfLife && !nucCode(Z, N)) return nm + " is not in the table (NUBASE2020 lists no such nuclide), so it has no chain.";
    if (stop == ChainStop::NoHalfLife) {
        const NubaseRow* nb = nubase(Z, N);
        if (why == HeightKind::Limit) return std::string(nm) + ": its half-life is only a limit" + (nb && *nb->lim ? std::string(" (") + nb->lim + ")" : std::string()) + ", so no time is made up for it and the chain is not followed past it.";
        if (why == HeightKind::Extrapolated) return nm + ": NUBASE2020 has only an extrapolation from systematics for its half-life, not a measurement, so no time is made up for it and the chain is not followed past it.";
        return nm + ": no half-life is given for it, so no time is made up for it and the chain is not followed past it.";
    }
    return nm + " decays on.";
}
std::string chainStopWords(const ChainNode& n) { return stopText(n.Z, n.N, n.stop, n.timing.why); }

// ---------------------------------------------------------------- the graph
static ChainNode makeNode(int Z, int N) {
    ChainNode x; x.Z = Z; x.N = N;
    const std::string* c = nucCode(Z, N);
    if (!c) { x.stop = ChainStop::NoHalfLife; x.timing = {ChainTime::None, 0, HeightKind::Unknown}; return x; }
    if (*c == "S") {
        x.stop = ChainStop::Stable;
        const NubaseRow* nb = nubase(Z, N);
        x.observationallyStable = nb && (*nb->lim || *nb->modes);
        return x;
    }
    x.timing = chainTiming(Z, N);
    if (x.timing.kind == ChainTime::None) x.stop = ChainStop::NoHalfLife;
    return x;
}

ChainGraph decayChain(int Z, int N) {
    ChainGraph g;
    std::map<std::pair<int, int>, int> index;
    g.nodes.push_back(makeNode(Z, N));
    index[{Z, N}] = 0;
    for (std::size_t i = 0; i < g.nodes.size(); i++) {             // breadth first: the nodes found are added at the end
        if (g.nodes[i].stop != ChainStop::Continues) continue;
        const int z = g.nodes[i].Z, n = g.nodes[i].N;
        const auto d = decInfo(z, n);
        if (!d || d->br.empty()) { g.nodes[i].stop = ChainStop::NoHalfLife; g.nodes[i].timing = {ChainTime::None, 0, HeightKind::Unknown}; continue; }
        const std::vector<double> odds = decOdds(*d);
        std::vector<ChainEdge> edges;
        for (std::size_t k = 0; k < d->br.size(); k++) {
            ChainEdge e; e.mode = d->br[k].first; e.p = odds[k]; e.measured = !d->unk;
            const auto mv = decMove(e.mode);
            if (!mv) e.end = ChainEnd::Fission;
            else {
                e.dZ = mv->first; e.dN = mv->second;
                if (!chartHas(z + e.dZ, n + e.dN)) e.end = ChainEnd::NotInTable;
                else {
                    const auto key = std::make_pair(z + e.dZ, n + e.dN);
                    auto it = index.find(key);
                    if (it == index.end()) { it = index.emplace(key, int(g.nodes.size())).first; g.nodes.push_back(makeNode(key.first, key.second)); }
                    e.to = it->second;
                }
            }
            edges.push_back(e);
        }
        if (!d->unk || edges.size() == 1) {                          // the likeliest: the largest share, the first listed on a tie; not where the odds are not measured and there is a choice
            std::size_t best = 0;
            for (std::size_t k = 1; k < edges.size(); k++) if (edges[k].p > edges[best].p) best = k;
            edges[best].likeliest = true;
        }
        g.nodes[i].edges = std::move(edges);                         // (assigned last: the nodes vector may have grown above)
    }
    // the chance of passing through each node: in topological order (a depth-first search; the lab's table has no loops, a test checks every start; an edge back to a node being visited is ignored)
    std::vector<int> state(g.nodes.size(), 0), order;
    std::function<void(int)> visit = [&](int i) {
        state[std::size_t(i)] = 1;
        for (const ChainEdge& e : g.nodes[std::size_t(i)].edges) if (e.end == ChainEnd::Node && state[std::size_t(e.to)] == 0) visit(e.to);
        state[std::size_t(i)] = 2; order.push_back(i);
    };
    visit(0);
    g.nodes[0].reach = 1;
    for (std::size_t k = order.size(); k-- > 0;) {
        const ChainNode& x = g.nodes[std::size_t(order[k])];
        for (const ChainEdge& e : x.edges) if (e.end == ChainEnd::Node) g.nodes[std::size_t(e.to)].reach += x.reach * e.p;
    }
    // the likeliest path
    g.main.push_back(0);
    for (std::size_t guard = 0; guard <= g.nodes.size(); guard++) {
        const ChainNode& x = g.nodes[std::size_t(g.main.back())];
        if (x.stop == ChainStop::Stable) { g.mainWhy = chainNuclide(x.Z, x.N) + (x.observationallyStable ? " is observationally stable" : " is stable"); break; }
        if (x.stop == ChainStop::NoHalfLife) { g.mainWhy = chainStopWords(x); break; }
        const ChainEdge* best = nullptr;
        for (const ChainEdge& e : x.edges) if (e.likeliest) best = &e;
        if (!best) { g.mainWhy = chainNuclide(x.Z, x.N) + " can decay several ways and the odds are not measured, so no way is the likeliest"; break; }
        if (best->end == ChainEnd::Fission) { g.mainWhy = chainNuclide(x.Z, x.N) + " most likely splits in two (" + decWord(best->mode) + "): the pieces differ every time, so the path stops"; break; }
        if (best->end == ChainEnd::NotInTable) { g.mainWhy = "the likeliest daughter of " + chainNuclide(x.Z, x.N) + " is not a nuclide of the table"; break; }
        if (std::find(g.main.begin(), g.main.end(), best->to) != g.main.end()) { g.mainWhy = "a loop"; break; }
        g.main.push_back(best->to);
    }
    return g;
}

std::vector<ChainEndProb> chainEnds(const ChainGraph& g) {
    std::vector<ChainEndProb> out;
    double split = 0, missing = 0;
    for (const ChainNode& x : g.nodes) {
        if (x.stop != ChainStop::Continues) out.push_back({x.Z, x.N, x.stop, ChainEnd::Node, x.reach});
        for (const ChainEdge& e : x.edges) { if (e.end == ChainEnd::Fission) split += x.reach * e.p; else if (e.end == ChainEnd::NotInTable) missing += x.reach * e.p; }
    }
    if (split > 0) out.push_back({0, 0, ChainStop::Continues, ChainEnd::Fission, split});
    if (missing > 0) out.push_back({0, 0, ChainStop::Continues, ChainEnd::NotInTable, missing});
    std::stable_sort(out.begin(), out.end(), [](const ChainEndProb& a, const ChainEndProb& b) { return a.p > b.p; });
    return out;
}

// ---------------------------------------------------------------- the walk
void walkStart(ChainWalk& w, int Z, int N) {
    w = ChainWalk();
    w.startZ = w.Z = Z; w.startN = w.N = N;
    const ChainNode x = makeNode(Z, N);
    if (x.stop != ChainStop::Continues) { w.done = true; w.stop = x.stop; w.why = x.timing.why; }
}
void walkTime(ChainWalk& w, double dt) { if (!w.done && dt > 0) { w.sinceStep += dt; w.screenS += dt; } }
bool walkStep(ChainWalk& w, const std::function<double()>& rnd) {
    if (w.done) return false;
    const ChainTiming tm = chainTiming(w.Z, w.N);
    const auto d = decInfo(w.Z, w.N);
    if (tm.kind == ChainTime::None || !d || d->br.empty()) { w.done = true; w.stop = ChainStop::NoHalfLife; w.why = tm.why; return false; }
    WalkStep s;
    s.fromZ = w.Z; s.fromN = w.N; s.halfLife = tm.halfLife; s.approximate = tm.kind == ChainTime::Approximate;
    s.lived = decDrawLifetime(tm.halfLife, rnd());                    // first number: the lifetime, from the exponential law
    const std::size_t k = decDrawBranch(*d, rnd());                   // second number: the branch, from the listed odds
    s.mode = d->br[k].first; s.p = decOdds(*d)[k]; s.measured = !d->unk; s.modes = int(d->br.size());
    s.shown = w.sinceStep;
    const auto mv = decMove(s.mode);
    if (!mv) { s.end = ChainEnd::Fission; w.done = true; }
    else if (!chartHas(w.Z + mv->first, w.N + mv->second)) { s.end = ChainEnd::NotInTable; s.toZ = w.Z + mv->first; s.toN = w.N + mv->second; w.done = true; }
    else {
        s.end = ChainEnd::Node; s.toZ = w.Z + mv->first; s.toN = w.N + mv->second;
        w.Z = s.toZ; w.N = s.toN;
        const ChainNode x = makeNode(w.Z, w.N);
        if (x.stop != ChainStop::Continues) { w.done = true; w.stop = x.stop; w.why = x.timing.why; }
    }
    w.drawnS += s.lived; w.sinceStep = 0;
    w.steps.push_back(s);
    return true;
}

// ---------------------------------------------------------------- the words
std::string chainStepWords(const WalkStep& s) {
    int dz = 0, dn = 0;
    if (const auto mv = decMove(s.mode)) { dz = mv->first; dn = mv->second; }
    std::string t = chainModeWords(s.mode, dz, dn) + "; " + chainNuclide(s.fromZ, s.fromN);
    if (s.end == ChainEnd::Node) t += " → " + chainNuclide(s.toZ, s.toN);
    else if (s.end == ChainEnd::Fission) t += " splits in two; the pieces differ every time, so the chain stops here";
    else t += " → a square with no nuclide in the table (" + std::to_string(s.toZ) + " protons, " + std::to_string(s.toN) + " neutrons), so the chain stops here";
    t += ".";
    if (!s.measured) t += s.modes > 1 ? " The odds of the " + std::to_string(s.modes) + " listed ways are not measured: an equal chance for each was used as a stand-in (not data)."
                                      : std::string(" The odds are not measured (only this way is listed).");
    else if (s.modes > 1) t += " Chance of this way: " + pctOf(s.p) + " %.";
    return t;
}
std::string chainStepTime(const WalkStep& s) {
    std::string t = "Lived " + fmtLife(s.lived) + " before it decayed: a random draw from the exponential law with this nuclide's " + (s.approximate ? "approximate " : "") + "half-life, " + fmtHalf(s.halfLife) + ".";
    if (s.shown < 1e-3) return t + " It was shown with no screen time passed since the last step, so no speed-up is stated.";
    const double r = s.lived / s.shown;
    if (r > 0.9 && r < 1.1) return t + " It was shown after " + jsNum(s.shown, 2) + " s on the screen, about real time.";
    return t + " It was shown after " + jsNum(s.shown, 2) + " s on the screen: about " + sci(r > 1 ? r : 1 / r) + " times " + (r > 1 ? "faster" : "slower") + " than real time.";
}
std::string chainWalkEndWords(const ChainWalk& w) {
    if (!w.done) return "The walk is not over.";
    if (!w.steps.empty() && w.steps.back().end == ChainEnd::Fission) return chainNuclide(w.steps.back().fromZ, w.steps.back().fromN) + " split in two (" + decWord(w.steps.back().mode) + "). The pieces differ every time, so the walk ends here.";
    if (!w.steps.empty() && w.steps.back().end == ChainEnd::NotInTable) return "The daughter of " + chainNuclide(w.steps.back().fromZ, w.steps.back().fromN) + " is not a nuclide the table lists, so the walk ends here.";
    return (w.steps.empty() ? std::string() : "After " + std::to_string(w.steps.size()) + (w.steps.size() == 1 ? " decay: " : " decays: ")) + stopText(w.Z, w.N, w.stop, w.why);
}
const char* chainTimeNote() {
    return "Time: this is not real time. Each step's lifetime is a random draw from the real exponential law for that nuclide's half-life (not a fixed schedule: walk again and the times and the branches differ), "
           "but the screen shows the steps one after another about 1.6 seconds apart (or when you press the button), so a step that really takes billions of years is shown billions of years faster than it happened. "
           "Each step says how many times faster than real time it was shown.";
}

std::vector<std::string> chainSummary(const ChainGraph& g) {
    std::vector<std::string> out;
    const ChainNode& s = g.nodes[0];
    if (s.stop != ChainStop::Continues) { out.push_back(chainStopWords(s)); return out; }
    std::string path;
    for (std::size_t k = 0; k < g.main.size(); k++) path += (k ? " → " : "") + chainNuclide(g.nodes[std::size_t(g.main[k])].Z, g.nodes[std::size_t(g.main[k])].N);
    out.push_back("Likeliest path, " + std::to_string(g.main.size() - 1) + (g.main.size() == 2 ? " step: " : " steps: ") + path + ". It stops because " + g.mainWhy + (g.mainWhy.back() == '.' ? "" : "."));
    // the nuclides on the path whose half-life is far beyond the age of the universe
    for (std::size_t k = 0; k < g.main.size(); k++) {
        const ChainNode& x = g.nodes[std::size_t(g.main[k])];
        if (x.stop == ChainStop::Continues && x.timing.halfLife > kChainPracticalStableS)
            out.push_back(chainNuclide(x.Z, x.N) + " on this path has a half-life of " + fmtHalf(x.timing.halfLife) + ", far longer than the age of the universe: in practice the chain stops there, but the data's chain goes on.");
    }
    // the branches: nuclides a walk reaches with a chance of at least 0.1 % whose second way has a share of at least 0.1 %, with the odds; the rarer ways and the nuclides only they reach are counted
    int rare = 0, unmeasured = 0, stops = 0, onlyRare = 0;
    double maxOnlyRare = 0;
    std::string list;
    std::vector<std::pair<double, std::string>> rareList;                       // the rarer ways from the nuclides a walk really reaches, to name the largest
    for (const ChainNode& x : g.nodes) {
        if (x.reach < 1e-3) { onlyRare++; maxOnlyRare = std::max(maxOnlyRare, x.reach); continue; }
        if (x.edges.size() < 2) { if (x.stop == ChainStop::NoHalfLife && &x != &s) stops++; continue; }
        if (!x.edges[0].measured) { unmeasured++; continue; }
        std::vector<const ChainEdge*> big;
        for (const ChainEdge& e : x.edges) { if (e.p >= 1e-3) big.push_back(&e); else { rare++; rareList.push_back({e.p, chainNuclide(x.Z, x.N) + " " + decShort(e.mode) + " " + pctOf(e.p) + " %"}); } }
        if (big.size() < 2) continue;
        std::string one = chainNuclide(x.Z, x.N) + ": ";
        for (std::size_t k = 0; k < big.size(); k++) one += (k ? ", " : "") + decShort(big[k]->mode) + " " + pctOf(big[k]->p) + " %";
        list += (list.empty() ? "" : "; ") + one;
    }
    if (!list.empty()) out.push_back("It branches where the data lists more than one way (each with at least 0.1 %): " + list + ". Every way is drawn, the likeliest solid and thick, the others thinner.");
    if (rare) out.push_back(std::to_string(rare) + (rare == 1 ? " rarer way (below 0.1 %" : " rarer ways (each below 0.1 %") + ", such as a double beta decay, a cluster such as carbon-14 or fission) " + (rare == 1 ? "is" : "are") + " drawn as very faint lines; a way that splits the nucleus has no single daughter and is not drawn as a line." + [&] {
        std::sort(rareList.begin(), rareList.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::string t;
        for (std::size_t k = 0; k < rareList.size() && k < 4; k++) t += (k ? ", " : " The largest: ") + rareList[k].second;
        return t + (rareList.empty() ? std::string() : ".");
    }());
    if (onlyRare) out.push_back(std::to_string(onlyRare) + (onlyRare == 1 ? " nuclide is" : " nuclides are") + " reached only by such rare ways (a chance of at most " + pctOf(maxOnlyRare) + " % each); they are drawn, faintly, and a walk almost never goes there.");
    if (unmeasured) out.push_back(std::to_string(unmeasured) + (unmeasured == 1 ? " nuclide in this chain lists" : " nuclides in this chain list") + " several ways whose odds are not measured (marked ?): no way is called likeliest, and a walk uses an equal chance for each as a stand-in, marked at the step (not data).");
    if (stops) out.push_back(std::to_string(stops) + (stops == 1 ? " nuclide in this chain has" : " nuclides in this chain have") + " a half-life that is only a limit, only an extrapolation or not given (marked with a cross): the chain stops there without a time.");
    // where it ends
    const std::vector<ChainEndProb> ends = chainEnds(g);
    std::string e;
    int shown = 0, more = 0; double moreP = 0;
    for (const ChainEndProb& x : ends) {
        if (shown >= 4) { more++; moreP += x.p; continue; }
        shown++;
        std::string what = x.end == ChainEnd::Fission ? std::string("a split in two") : x.end == ChainEnd::NotInTable ? std::string("a square with no nuclide") :
                           chainNuclide(x.Z, x.N) + (x.stop == ChainStop::Stable ? (g.nodes[std::size_t(g.find(x.Z, x.N))].observationallyStable ? " (observationally stable)" : " (stable)") : [&] {
                               const HeightKind why = g.nodes[std::size_t(g.find(x.Z, x.N))].timing.why;
                               return std::string(why == HeightKind::Limit ? " (stops: its half-life is only a limit)" : why == HeightKind::Extrapolated ? " (stops: its half-life is only extrapolated)" : " (stops: no half-life given)");
                           }());
        e += (e.empty() ? "" : ", ") + what + " " + pctOf(x.p) + " %";
    }
    if (more) e += ", and " + std::to_string(more) + " more with " + pctOf(moreP) + " % together";
    out.push_back("A walk ends at: " + e + ". (Chances from the data's odds; where odds are not measured the stand-in equal chances.)");
    return out;
}

}  // namespace atom
