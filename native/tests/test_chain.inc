// 0.12 stage 1: decay chains on the chart of nuclides (included by test_atom.cpp, after testChartBinding).
// REFERENCES (none is the lab's own earlier output):
//  (1) the four classic decay series, typed here from the textbooks (members, mode of each step, end point). No online source could be reached from this session (every host answered with the proxy's
//      denial), so they are typed from standard textbook tables; two checks on them do not depend on any table: the mass-number rule (an alpha takes 4 off A, a beta changes Z only, so every member of a
//      series has A = A0 mod 4: 4n+2 for U-238, 4n+3 for U-235, 4n for Th-232, 4n+1 for Np-237) and the counts (a step of 4 per alpha, and the charge: Z0 - 2 alphas + betas = Z of the end);
//  (2) the move of each decay mode on the chart, typed here by hand (alpha: Z-2, N-2; beta minus: Z+1, N-1; ...), not taken from decMove;
//  (3) the exponential law: the median of a lifetime is the half-life, and the mean is T / ln 2;
//  (4) binomial statistics for the random walk: n runs, share p, the margin is 5 standard deviations sqrt(p (1 - p) / n) (a chance of about 6e-7 that a correct walk fails one check; the seed is fixed);
//  (5) counts stage 3 and stage 2 made from the data: 87 unknown, 369 extrapolated, 11 limit half-lives; 71 stable nuclides that NUBASE2020 lists with a limit or a "?" entry.
#include "atom/decay_chain.h"

static std::string chNam(int Z, int N) { return std::string(symbol(Z)) + "-" + std::to_string(Z + N); }
static std::string chPathNames(const ChainGraph& g) { std::string s; for (int i : g.main) s += (s.empty() ? "" : " ") + chNam(g.nodes[std::size_t(i)].Z, g.nodes[std::size_t(i)].N); return s; }
static std::string chPathModes(const ChainGraph& g) {
    std::string s;
    for (std::size_t k = 0; k + 1 < g.main.size(); k++) {
        const ChainNode& a = g.nodes[std::size_t(g.main[k])];
        for (const ChainEdge& e : a.edges) if (e.end == ChainEnd::Node && e.to == g.main[k + 1]) { s += (s.empty() ? "" : " ") + e.mode; break; }
    }
    return s;
}
// the move of each mode, by hand: {dZ, dN}. A cluster "14C" throws out carbon-14 (6 protons, 8 neutrons), and so on.
static bool chHandMove(const std::string& m, int& dZ, int& dN) {
    static const struct { const char* m; int z, n; } t[] = {
        {"A", -2, -2}, {"B-", 1, -1}, {"B+", -1, 1}, {"2B-", 2, -2}, {"2B+", -2, 2}, {"p", -1, 0}, {"2p", -2, 0}, {"3p", -3, 0}, {"n", 0, -1}, {"2n", 0, -2}, {"3n", 0, -3},
        {"B-n", 1, -2}, {"B-2n", 1, -3}, {"B-3n", 1, -4}, {"B-4n", 1, -5}, {"B-A", -1, -3}, {"B-d", 0, -2}, {"B-t", 0, -3}, {"B-p", 0, -1},
        {"B+p", -2, 1}, {"B+2p", -3, 1}, {"B+3p", -4, 1}, {"B+A", -3, -1}, {"B+pA", -4, -1},
        {"12C", -6, -6}, {"14C", -6, -8}, {"18O", -8, -10}, {"20O", -8, -12}, {"20Ne", -10, -10}, {"22Ne", -10, -12}, {"24Ne", -10, -14}, {"25Ne", -10, -15}, {"23F", -9, -14},
        {"28Mg", -12, -16}, {"30Mg", -12, -18}, {"32Si", -14, -18}, {"34Si", -14, -20}};
    for (const auto& k : t) if (m == k.m) { dZ = k.z; dN = k.n; return true; }
    return false;
}
static double chMargin(double p, int n) { return 5 * std::sqrt(p * (1 - p) / n); }
static bool chIsPlainBeta(const std::string& m) { return m == "A" || m == "B-" || m == "B+" || m == "2B-" || m == "2B+"; }

static void testDecayChain() {
    // ---- the two draws: the exponential law and the listed odds
    {
        checkNum("draw: a lifetime drawn at u = 0.5 is the half-life (the median of the exponential law), for 4 half-lives from 1e-7 s to 1e17 s",
                 std::fabs(decDrawLifetime(1e-7, 0.5) / 1e-7 - 1) + std::fabs(decDrawLifetime(3.63e3, 0.5) / 3.63e3 - 1) + std::fabs(decDrawLifetime(2.77e9, 0.5) / 2.77e9 - 1) + std::fabs(decDrawLifetime(1.41e17, 0.5) / 1.41e17 - 1), 0, 0, 1e-12);
        checkNum("draw: u = 0 gives a lifetime of exactly 0, and u = 1 - 2^-3 gives three half-lives", decDrawLifetime(10, 0), 0, 0);
        checkNum("draw: u = 7/8 gives 3 half-lives", decDrawLifetime(10, 0.875), 30, 1e-12);
        seedRandom(424242);
        const int n = 200000; double sum = 0; int below = 0;
        for (int i = 0; i < n; i++) { const double x = decDrawLifetime(1.0, atom::random()); sum += x; if (x < 1.0) below++; }
        const double mean = sum / n, tmean = 1 / std::log(2.0), sd = tmean / std::sqrt(double(n));          // the exponential's standard deviation equals its mean
        checkTrue("draw: over 200,000 lifetimes of half-life 1, the mean is 1 / ln 2 = 1.4427 within 5 standard errors (0.016), and half of them are shorter than the half-life within " + std::to_string(chMargin(0.5, n)).substr(0, 6),
                  std::fabs(mean - tmean) < 5 * sd && std::fabs(double(below) / n - 0.5) < chMargin(0.5, n));
        DecayInfo d; d.br = {{"B-", 0.6405}, {"A", 0.3594}, {"B-A", 0.00014}};
        const std::vector<double> o = decOdds(d);
        checkTrue("draw: the odds are normalised to add up to 1 (0.6405, 0.3594, 0.00014 add up to 1.00004), in the listed order", std::fabs(o[0] + o[1] + o[2] - 1) < 1e-12 && std::fabs(o[0] - 0.6405 / 1.00004) < 1e-12 && o.size() == 3);
        checkTrue("draw: u = 0 is the first listed mode, 0.5 the first, 0.8 the second, 0.99999 the third (by the cumulative odds)", decDrawBranch(d, 0) == 0 && decDrawBranch(d, 0.5) == 0 && decDrawBranch(d, 0.8) == 1 && decDrawBranch(d, 0.99999) == 2);
        DecayInfo u; u.br = {{"A", 1}, {"SF", 5.44e-7}};
        checkTrue("draw: odds that add up to more than 1 (uranium-238: alpha 1, fission 5.44e-7) still reach the last mode: u just below 1 gives fission", decDrawBranch(u, 0.9999999) == 1 && decDrawBranch(u, 0.5) == 0);
    }
    // ---- the lab's decay clock uses the same draws (it was not rewritten: decTick calls them)
    {
        AtomLab lab; lab.p = AtomParams(); lab.paused = false; lab.restart(); lab.nowMs = [] { return 0.0; };
        lab.setParam("Z", 83.0); lab.setParam("N", 129.0);                     // bismuth-212, half-life 3,630 s
        lab.setParam("view", std::string("Cloud"));
        seedRandom(99); const double u1 = atom::random();
        seedRandom(99); lab.tick(0);
        checkNum("clock: the lab's decay clock draws the lifetime with decDrawLifetime (first random number after the nucleus is chosen)", lab.s.decAt, decDrawLifetime(3630, u1), 1e-12);
    }
    // ---- who can be timed: counts from stages 2 and 3 (typed), and nothing else
    {
        int none = 0, lim = 0, ext = 0, unk = 0, meas = 0, appr = 0, stable = 0, wrongKind = 0, obs = 0;
        for (const ChartTile& t : chartTiles()) {
            if (t.cls == ChartClass::Stable) { stable++; continue; }
            const ChainTiming c = chainTiming(t.Z, t.N);
            const ChartHeight h = chartHeightOf(t.Z, t.N);
            if (c.kind == ChainTime::None) {
                none++; if (c.why == HeightKind::Limit) lim++; else if (c.why == HeightKind::Extrapolated) ext++; else if (c.why == HeightKind::Unknown) unk++; else wrongKind++;
                if (!(h.kind == c.why)) wrongKind++;
            } else if (c.kind == ChainTime::Measured) { meas++; if (h.kind != HeightKind::Measured || !(c.halfLife > 0)) wrongKind++; }
            else { appr++; if (h.kind != HeightKind::Approx || !(c.halfLife > 0)) wrongKind++; }
        }
        for (const ChartTile& t : chartTiles()) { const NubaseRow* nb = nubase(t.Z, t.N); if (nb && nb->kind == 'S' && (*nb->lim || *nb->modes)) obs++; }
        checkNum("timing: nuclides that cannot be timed: limit only (stage 3 counted 11)", lim, 11, 0);
        checkNum("timing: ... extrapolated only (369)", ext, 369, 0);
        checkNum("timing: ... no half-life at all (87)", unk, 87, 0);
        checkNum("timing: the sum is 467 and no other reason exists, and each reason is the stage 3 height kind of that tile", none * 1000 + wrongKind, 467000, 0);
        checkNum("timing: not-stable nuclides with a measured half-life (0.12 stage 2: 2,831 = NUBASE2020's plain values; 2,720 before) and an approximate one (6 = NUBASE2020's '~'; 119 before)", meas * 1000 + appr, 2831 * 1000 + 6, 0);
        checkNum("timing: stable tiles in the lab's table (182 + 71 = 253; 251 before 0.12 stage 2 made Kr-78 and Xe-124 stable)", stable, 253, 0);
        checkNum("timing: nuclides NUBASE2020 calls stable and lists with a half-life limit or a '?' decay entry (the brief's 71)", obs, 71, 0);
    }
    // ---- the four classic series, from the data: members, steps, end point (typed from the textbooks)
    struct Series { int Z, N; const char* members; const char* modes; const char* end; int a0mod; int alphas, betas; const char* minor; };
    static const Series ser[] = {
        {92, 146, "U-238 Th-234 Pa-234 U-234 Th-230 Ra-226 Rn-222 Po-218 Pb-214 Bi-214 Po-214 Pb-210 Bi-210 Po-210 Pb-206", "A B- B- A A A A A B- B- A B- B- A", "Pb-206", 2, 8, 6, "At-218 Tl-210 Tl-206"},
        {92, 143, "U-235 Th-231 Pa-231 Ac-227 Th-227 Ra-223 Rn-219 Po-215 Pb-211 Bi-211 Tl-207 Pb-207", "A B- A B- A A A A B- A B-", "Pb-207", 3, 7, 4, "Fr-223 Po-211 At-215"},
        {90, 142, "Th-232 Ra-228 Ac-228 Th-228 Ra-224 Rn-220 Po-216 Pb-212 Bi-212 Po-212 Pb-208", "A B- B- A A A A B- B- A", "Pb-208", 0, 6, 4, "Tl-208"},
        {93, 144, "Np-237 Pa-233 U-233 Th-229 Ra-225 Ac-225 Fr-221 At-217 Bi-213 Po-213 Pb-209 Bi-209 Tl-205", "A B- A A B- A A A B- A B- A", "Tl-205", 1, 8, 4, "Tl-209"}};
    for (const Series& s : ser) {
        const std::string tag = std::string("series from ") + chNam(s.Z, s.N) + ": ";
        const ChainGraph g = decayChain(s.Z, s.N);
        checkStr(tag + "the likeliest path's members, in order, are the textbook series", chPathNames(g), s.members);
        checkStr(tag + "the mode of each step", chPathModes(g), s.modes);
        const ChainNode& last = g.nodes[std::size_t(g.main.back())];
        checkTrue(tag + "the likeliest path ends at " + s.end + ", which the data calls stable (nothing is said to follow it)", chNam(last.Z, last.N) == s.end && last.stop == ChainStop::Stable && last.edges.empty());
        int al = 0, be = 0, z = s.Z, a = s.Z + s.N;
        for (std::size_t k = 0; k + 1 < g.main.size(); k++) {
            const ChainNode& x = g.nodes[std::size_t(g.main[k])]; const ChainNode& y = g.nodes[std::size_t(g.main[k + 1])];
            const int dz = y.Z - x.Z, dn = y.N - x.N;
            if (dz == -2 && dn == -2) al++; else if (dz == 1 && dn == -1) be++;
        }
        z -= 2 * al; z += be; a -= 4 * al;
        checkTrue(tag + "by the mass number and charge: " + std::to_string(s.alphas) + " alphas and " + std::to_string(s.betas) + " betas, so A and Z at the end are those of " + s.end,
                  al == s.alphas && be == s.betas && z == last.Z && a == last.Z + last.N);
        // the mass-number rule, over the whole graph: along every alpha and beta edge A changes by -4 or 0, so every member that is reached by those edges alone has A = A0 mod 4
        int off = 0, edges = 0;
        for (const ChainNode& x : g.nodes) for (const ChainEdge& e : x.edges) if (chIsPlainBeta(e.mode) && e.end == ChainEnd::Node) { edges++; const ChainNode& y = g.nodes[std::size_t(e.to)]; if ((((x.Z + x.N) - (y.Z + y.N)) % 4 + 4) % 4 != 0) off++; }
        checkTrue(tag + "every alpha / beta edge in its graph keeps A = A0 mod 4 (the series' rule, 4n + " + std::to_string(s.a0mod) + "); " + std::to_string(edges) + " edges checked", off == 0 && edges > 10 && (s.Z + s.N) % 4 == s.a0mod);
        // branches that every textbook shows (the minor members)
        std::string miss; std::string list = s.minor; std::size_t i = 0;
        while (i < list.size()) { const std::size_t sp = list.find(' ', i); const std::string m = list.substr(i, sp == std::string::npos ? std::string::npos : sp - i); bool f = false; for (const ChainNode& x : g.nodes) if (chNam(x.Z, x.N) == m) f = true; if (!f) miss += m + " "; if (sp == std::string::npos) break; i = sp + 1; }
        checkStr(tag + "the textbook's side branches (" + s.minor + ") are in the graph; missing", miss, "");
        // the end: where a walk ends, with the chances; all of them add up to 1
        const std::vector<ChainEndProb> ends = chainEnds(g);
        double sum = 0, pEnd = 0; for (const ChainEndProb& e : ends) { sum += e.p; if (e.end == ChainEnd::Node && e.stop == ChainStop::Stable && chNam(e.Z, e.N) == s.end) pEnd = e.p; }
        checkNum(tag + "the chances of the ends add up to 1", sum, 1, 0, 1e-9);
        checkTrue(tag + "the chance of ending at " + s.end + " is above 1 - 1e-6 (the rest is splitting and rare cluster branches that end elsewhere)", pEnd > 1 - 1e-6);
    }
    {
        const ChainGraph g = decayChain(93, 144);
        const int b = g.find(83, 126);
        checkTrue("series from Np-237: bismuth-209 is a member whose half-life (data: 6.34e26 s = 2.0e19 years) is far longer than the age of the universe; the data's chain goes on to Tl-205 and the words say so",
                  b >= 0 && g.nodes[std::size_t(b)].timing.halfLife > kChainPracticalStableS && g.nodes[std::size_t(b)].edges.size() == 1 && g.nodes[std::size_t(b)].edges[0].mode == "A" &&
                  [&] { for (const std::string& l : chainSummary(g)) if (l.find("bismuth-209") != std::string::npos && l.find("age of the universe") != std::string::npos) return true; return false; }());
        const ChainGraph t = decayChain(90, 142);
        const int bi = t.find(83, 129);
        double pb = 0, pa = 0;
        if (bi >= 0) for (const ChainEdge& e : t.nodes[std::size_t(bi)].edges) { if (e.mode == "B-") pb = e.p; if (e.mode == "A") pa = e.p; }
        checkTrue("Th-232 series: bismuth-212 branches, beta minus about 64 % to Po-212 and alpha about 36 % to Tl-208 (textbook); the likeliest branch is beta minus", bi >= 0 && pb > 0.63 && pb < 0.65 && pa > 0.35 && pa < 0.37 &&
                  t.find(84, 128) >= 0 && t.find(81, 127) >= 0 && [&] { for (const ChainEdge& e : t.nodes[std::size_t(bi)].edges) if (e.likeliest) return e.mode == "B-"; return false; }());
    }
    // ---- every edge of every graph: the move is the hand-typed move; A and Z bookkeeping; each end is what it says
    {
        int edges = 0, wrongMove = 0, unknownMode = 0, notThere = 0, badP = 0, cyc = 0, badSum = 0, badStart = 0, nodesN = 0, maxNodes = 0, noEdge = 0, stableWrong = 0, stopWrong = 0, obsWrong = 0, fissionTo = 0;
        std::set<std::string> modesSeen;
        for (const ChartTile& t : chartTiles()) {
            const ChainGraph g = decayChain(t.Z, t.N);
            if (g.nodes.empty() || g.nodes[0].Z != t.Z || g.nodes[0].N != t.N) { badStart++; continue; }
            nodesN += int(g.nodes.size()); maxNodes = std::max<int>(maxNodes, int(g.nodes.size()));
            for (const ChainNode& x : g.nodes) {
                const std::string* c = nucCode(x.Z, x.N);
                if ((x.stop == ChainStop::Stable) != (c && *c == "S")) stableWrong++;
                if (x.stop == ChainStop::Stable && x.observationallyStable != (nubase(x.Z, x.N) && (*nubase(x.Z, x.N)->lim || *nubase(x.Z, x.N)->modes))) obsWrong++;
                if (x.stop == ChainStop::NoHalfLife && (!x.edges.empty() || x.timing.kind != ChainTime::None)) stopWrong++;
                if (x.stop == ChainStop::Continues && x.timing.kind == ChainTime::None) stopWrong++;
                if (x.stop == ChainStop::Continues && x.edges.empty()) noEdge++;
                double sum = 0;
                for (const ChainEdge& e : x.edges) {
                    edges++; modesSeen.insert(e.mode); sum += e.p;
                    if (!(e.p > 0 && e.p <= 1)) badP++;
                    int dz = 0, dn = 0;
                    if (e.end == ChainEnd::Fission) { if (e.mode != "SF" && e.mode != "B-SF" && e.mode != "B+SF") fissionTo++; continue; }
                    if (!chHandMove(e.mode, dz, dn)) { unknownMode++; continue; }
                    if (e.dZ != dz || e.dN != dn) wrongMove++;
                    if (e.end == ChainEnd::Node) {
                        const ChainNode& y = g.nodes[std::size_t(e.to)];
                        if (y.Z != x.Z + dz || y.N != x.N + dn) wrongMove++;
                        if (!chartHas(y.Z, y.N)) notThere++;
                        if (e.to == 0) cyc++;
                    } else if (chartHas(x.Z + dz, x.N + dn)) notThere++;
                }
                if (!x.edges.empty() && std::fabs(sum - 1) > 1e-9) badSum++;
            }
            double ends = 0; for (const ChainEndProb& e : chainEnds(g)) ends += e.p;
            if (std::fabs(ends - 1) > 1e-9) badSum++;
        }
        checkNum("chain graphs: a graph whose first node is not the start nuclide (all 3,557 starts)", badStart, 0, 0);
        checkNum("chain edges: modes the hand table does not know (all modes of all 3,557 graphs)", unknownMode, 0, 0);
        checkNum("chain edges: an edge whose move on the chart differs from the hand-typed move of its mode", wrongMove, 0, 0);
        checkNum("chain edges: a daughter that is not a nuclide of the table, or an edge back to the start (a loop)", notThere + cyc, 0, 0);
        checkNum("chain edges: a NotInTable edge whose square does have a nuclide (checked above in notThere); a split that is not SF, B-SF or B+SF", fissionTo, 0, 0);
        checkNum("chain edges: shares outside (0, 1], or the shares of a node (or the chances of the ends) not adding up to 1 within 1e-9", badP + badSum, 0, 0);
        checkNum("chain nodes: stable according to the graph differs from stable in the lab's table; observationally-stable flag differs from NUBASE (limit or '?' entry)", stableWrong + obsWrong, 0, 0);
        checkNum("chain nodes: a node that stops without a time but has edges or a time, a timed node marked as stopped, or a node that decays on but has no edge", stopWrong + noEdge, 0, 0);
        checkTrue("chain graphs: the largest graph from any start has " + std::to_string(maxNodes) + " nuclides (guard 110, about 25 % above the measured 87): a chain is computed once per chosen nuclide and is small", maxNodes > 50 && maxNodes <= 110);
        (void)nodesN;
        checkTrue("chain edges: the modes that occur include all the plain ones and the cluster ones", modesSeen.count("A") && modesSeen.count("B-") && modesSeen.count("B+") && modesSeen.count("14C") && modesSeen.count("SF"));
    }
    // ---- the likeliest branch, the unmeasured odds, the ends that are not stable
    {
        int multiUnk = 0, likelyWithUnk = 0, noLikely = 0, wrongLikely = 0, unkEqual = 0, unkChecked = 0;
        for (const ChartTile& t : chartTiles()) {
            const auto d = decInfo(t.Z, t.N);
            if (!d || t.cls == ChartClass::Stable) continue;
            const ChainTiming tm = chainTiming(t.Z, t.N);
            if (tm.kind == ChainTime::None) continue;
            const ChainGraph g = decayChain(t.Z, t.N);
            const ChainNode& x = g.nodes[0];
            int n = 0; for (const ChainEdge& e : x.edges) if (e.likeliest) n++;
            if (d->unk && d->br.size() > 1) { multiUnk++; if (n != 0) likelyWithUnk++; for (const ChainEdge& e : x.edges) { if (e.measured) unkEqual++; } }
            else {
                if (n != 1) noLikely++;
                double best = -1; std::size_t bi = 0; for (std::size_t i = 0; i < x.edges.size(); i++) if (x.edges[i].p > best) { best = x.edges[i].p; bi = i; }
                if (n == 1 && !x.edges[bi].likeliest) wrongLikely++;      // the first listed of the largest
            }
            if (d->unk) { unkChecked++; const std::vector<double> o = decOdds(*d); for (double v : o) if (std::fabs(v - 1.0 / double(o.size())) > 1e-9) unkEqual++; }
        }
        checkTrue("odds not measured: " + std::to_string(multiUnk) + " timed nuclides list several modes with odds NUBASE2020 marks not measured: none of them has a likeliest branch, and each branch is marked not measured", multiUnk > 20 && likelyWithUnk == 0 && unkEqual == 0);
        checkNum("odds measured (or a single mode): every such node has exactly one likeliest branch, and it is the first listed of the largest share", noLikely + wrongLikely, 0, 0);
        checkTrue("odds not measured: the lab's table lists equal shares for them (that is what the chain then uses as a stand-in, marked), " + std::to_string(unkChecked) + " timed nuclides checked (guard: at least 25; measured 32)", unkChecked >= 25);
    }
    // ---- nothing that cannot be timed gets a time (467 nuclides), and a chain that runs into one stops there and says why
    {
        int tested = 0, drew = 0, hasSteps = 0, notDone = 0, noWords = 0, called = 0;
        for (const ChartTile& t : chartTiles()) {
            if (t.cls == ChartClass::Stable) continue;
            if (chainTiming(t.Z, t.N).kind != ChainTime::None) continue;
            tested++;
            ChainWalk w; walkStart(w, t.Z, t.N);
            if (!w.done || w.stop != ChainStop::NoHalfLife) notDone++;
            int calls = 0; const bool ran = walkStep(w, [&] { calls++; return 0.5; });
            if (ran || calls) drew++;
            if (!w.steps.empty() || w.drawnS != 0) hasSteps++;
            const std::string why = chainWalkEndWords(w);
            if (why.find("no time is made up") == std::string::npos || (why.find("only a limit") == std::string::npos && why.find("extrapolat") == std::string::npos && why.find("no half-life") == std::string::npos)) noWords++;
            const ChainGraph g = decayChain(t.Z, t.N);
            if (g.nodes.size() != 1 || !g.nodes[0].edges.empty() || g.nodes[0].stop != ChainStop::NoHalfLife) called++;
        }
        checkNum("no made-up time: nuclides tested (87 + 369 + 11)", tested, 467, 0);
        checkNum("no made-up time: a walk from one of them that draws a lifetime or takes a step (it must draw nothing: not one random number)", drew + hasSteps, 0, 0);
        checkNum("no made-up time: such a walk that is not over at once with the reason 'no half-life'", notDone, 0, 0);
        checkNum("no made-up time: the words do not say why (they must name the kind, and say that no time is made up), or the graph follows its listed modes", noWords + called, 0, 0);
        // a chain that runs INTO one: walk everything from every start with a seeded generator and look at each step's source
        int steps = 0, fromUntimed = 0, badLife = 0, wrongApprox = 0, randomCalls = 0, expectCalls = 0, loops = 0, running = 0;
        seedRandom(777);
        for (const ChartTile& t : chartTiles()) {
            ChainWalk w; walkStart(w, t.Z, t.N);
            int guard = 0;
            while (!w.done && guard++ < 400) {
                const bool ok = walkStep(w, [&] { randomCalls++; return atom::random(); });
                if (!ok) break;
                expectCalls += 2;
            }
            if (guard >= 400) loops++;
            if (!w.done) running++;
            for (const WalkStep& s : w.steps) {
                steps++;
                const ChartHeight h = chartHeightOf(s.fromZ, s.fromN);
                if (h.kind != HeightKind::Measured && h.kind != HeightKind::Approx) fromUntimed++;
                if (!(s.lived > 0) || !std::isfinite(s.lived) || !(s.halfLife > 0)) badLife++;
                if (s.approximate != (h.kind == HeightKind::Approx)) wrongApprox++;
            }
        }
        checkTrue("no made-up time: " + std::to_string(steps) + " steps over a walk from every one of the 3,557 nuclides: none starts from a nuclide whose half-life is a limit, an extrapolation or missing", steps > 10000 && fromUntimed == 0);
        checkNum("no made-up time: a step with a zero, negative or not-a-number lifetime or half-life", badLife, 0, 0);
        checkNum("no made-up time: a step from an approximate half-life not marked approximate, or one marked that is not", wrongApprox, 0, 0);
        checkNum("the walk draws exactly two random numbers per decay (the lifetime, then the branch), like the decay clock", randomCalls - expectCalls, 0, 0);
        checkNum("every walk from every nuclide is over (it reaches a stable nuclide, a nuclide it cannot time, a split, or a square the table lacks) in under 400 steps", loops + running, 0, 0);
    }
    // ---- the walk obeys the odds (binomial): n single steps from a branching nuclide
    {
        struct Br { int Z, N; const char* what; };
        static const Br br[] = {{83, 129, "bismuth-212"}, {89, 138, "actinium-227"}, {83, 128, "bismuth-211"}, {83, 130, "bismuth-213"}, {83, 131, "bismuth-214"}};
        const int n = 20000;
        seedRandom(31337);
        for (const Br& b : br) {
            const auto d = decInfo(b.Z, b.N);
            const std::vector<double> o = decOdds(*d);
            std::vector<int> cnt(d->br.size(), 0);
            bool allMeasured = true;
            for (int i = 0; i < n; i++) {
                ChainWalk w; walkStart(w, b.Z, b.N);
                walkStep(w, [] { return atom::random(); });
                for (std::size_t k = 0; k < d->br.size(); k++) if (w.steps[0].mode == d->br[k].first) cnt[k]++;
                if (!w.steps[0].measured) allMeasured = false;
            }
            std::string bad; double worst = 0;
            for (std::size_t k = 0; k < o.size(); k++) {
                const double share = double(cnt[k]) / n, mg = std::max(chMargin(o[k], n), 1.0 / n);
                worst = std::max(worst, std::fabs(share - o[k]) / mg);
                if (std::fabs(share - o[k]) > mg) bad += d->br[k].first + " ";
            }
            checkStr(std::string("random walk from ") + b.what + ": modes whose share over " + std::to_string(n) + " runs is further than 5 standard deviations from the data's odds", bad, "");
            checkTrue(std::string("random walk from ") + b.what + ": the steps say the odds are measured (largest deviation " + std::to_string(worst).substr(0, 4) + " of the 5-sigma margin)", allMeasured);
        }
        // not measured: pick the first timed nuclide with two listed modes whose odds are not measured
        int uz = 0, un = 0, dz = 0, dn = 0;
        for (const ChartTile& t : chartTiles()) { const auto d = decInfo(t.Z, t.N); if (d && d->unk && d->br.size() == 2 && t.cls != ChartClass::Stable && chainTiming(t.Z, t.N).kind != ChainTime::None && chHandMove(d->br[0].first, dz, dn) && chHandMove(d->br[1].first, dz, dn)) { uz = t.Z; un = t.N; break; } }
        const auto d = decInfo(uz, un);
        int c0 = 0; bool allMarked = true;
        for (int i = 0; i < n; i++) { ChainWalk w; walkStart(w, uz, un); walkStep(w, [] { return atom::random(); }); if (w.steps[0].mode == d->br[0].first) c0++; if (w.steps[0].measured || w.steps[0].modes != 2) allMarked = false; }
        checkTrue("random walk, odds not measured (" + chNam(uz, un) + ", two modes): equal shares within 5 sigma (" + std::to_string(chMargin(0.5, n)).substr(0, 6) + "), and every step is marked 'odds not measured'", uz > 0 && std::fabs(double(c0) / n - 0.5) < chMargin(0.5, n) && allMarked);
        {
            ChainWalk w; walkStart(w, uz, un); walkStep(w, [] { return 0.1; });
            checkTrue("random walk, odds not measured: the step's words say so and that an equal chance is a stand-in, not data", chainStepWords(w.steps[0]).find("not measured") != std::string::npos && chainStepWords(w.steps[0]).find("equal") != std::string::npos);
        }
    }
    // ---- whole walks: uranium-238 to lead-206, and the others, from the lab's random numbers
    {
        struct Run { int Z, N; const char* end; int steps; };
        static const Run runs[] = {{92, 146, "Pb-206", 14}, {92, 143, "Pb-207", 11}, {90, 142, "Pb-208", 10}, {93, 144, "Tl-205", 12}};
        const int n = 2000;
        seedRandom(2718);
        for (const Run& r : runs) {
            int ends = 0, shortest = 0, chained = 0, bookkeeping = 0, mainLen = 0;
            const ChainGraph gr = decayChain(r.Z, r.N);
            int minSteps = 0;                                               // the fewest steps to a stable nuclide by any way in the graph (breadth first search, not the walk)
            { std::vector<int> dist(gr.nodes.size(), -1); std::vector<int> q{0}; dist[0] = 0; minSteps = 1000;
              for (std::size_t h = 0; h < q.size(); h++) { const ChainNode& x = gr.nodes[std::size_t(q[h])]; if (x.stop == ChainStop::Stable) { minSteps = std::min(minSteps, dist[std::size_t(q[h])]); continue; }
                  for (const ChainEdge& e : x.edges) if (e.end == ChainEnd::Node && dist[std::size_t(e.to)] < 0) { dist[std::size_t(e.to)] = dist[std::size_t(q[h])] + 1; q.push_back(e.to); } } }
            for (int i = 0; i < n; i++) {
                ChainWalk w; walkStart(w, r.Z, r.N);
                while (walkStep(w, [] { return atom::random(); })) {}
                if (w.done && w.stop == ChainStop::Stable && chNam(w.Z, w.N) == r.end) ends++;
                if (int(w.steps.size()) == r.steps) mainLen++;
                if (int(w.steps.size()) >= minSteps) shortest++;
                int z = r.Z, nn = r.N; bool ok = true;
                for (const WalkStep& s : w.steps) { int dz = 0, dn = 0; if (s.fromZ != z || s.fromN != nn || !chHandMove(s.mode, dz, dn) || s.toZ != z + dz || s.toN != nn + dn) ok = false; z = s.toZ; nn = s.toN; }
                if (ok && z == w.Z && nn == w.N) chained++;
                if (!ok) bookkeeping++;
            }
            const std::string tag = std::string("walk from ") + chNam(r.Z, r.N) + " (" + std::to_string(n) + " runs): ";
            checkTrue(tag + "every run reaches " + r.end + " (the chance of anything else is below 1e-6)", ends == n);
            checkTrue(tag + "no run has fewer steps than the shortest way in the graph (" + std::to_string(minSteps) + ", found by a separate breadth-first search), and at least 99 % have exactly the likeliest path's " + std::to_string(r.steps), shortest == n && mainLen >= int(0.99 * n) && minSteps <= r.steps);
            checkTrue(tag + "each step starts where the last one ended, and each step's change in Z and N is the hand-typed move of its mode", bookkeeping == 0 && chained == n);
        }
    }
    // ---- the words about time: never real time, each step a random draw
    {
        ChainWalk w; walkStart(w, 92, 146); w.sinceStep = 1.6; walkStep(w, [] { return 0.5; });
        const WalkStep& s = w.steps[0];
        const std::string words = chainStepWords(s), tm = chainStepTime(s), note = chainTimeNote();
        checkNum("time: the step from U-238 at u = 0.5 lived exactly one half-life (1.41e17 s), and the screen time was 1.6 s", s.lived / 1.41e17 + s.shown, 1 + 1.6, 1e-9);
        checkTrue("time: the step's words name the mode, what leaves and the move: 'alpha: two protons and two neutrons leave; uranium-238 → thorium-234'", words.find("alpha") != std::string::npos && words.find("two protons and two neutrons leave") != std::string::npos && words.find("uranium-238 → thorium-234") != std::string::npos);
        checkTrue("time: the step's time line gives the drawn lifetime, the half-life (4.47 billion years), says it is a random draw, and how many times faster than real time it was shown (1.41e17 s / 1.6 s = 8.8×10¹⁶)",
                  tm.find("4.47 billion years") != std::string::npos && tm.find("random draw") != std::string::npos && tm.find("faster than real time") != std::string::npos && tm.find("8.8×10¹⁶") != std::string::npos);
        checkTrue("time: the standing note says the walk is not real time, that each lifetime is a random draw from the real law and not a schedule, and that the screen paces the steps", note.find("not real time") != std::string::npos &&
                  note.find("random draw") != std::string::npos && note.find("not a fixed schedule") != std::string::npos);
        ChainWalk w2; walkStart(w2, 92, 146); walkStep(w2, [] { return 0.5; });
        checkTrue("time: a step shown with no screen time between (shown = 0) does not invent a factor", chainStepTime(w2.steps[0]).find("faster than real time") == std::string::npos && chainStepTime(w2.steps[0]).find("no screen time") != std::string::npos);
        ChainWalk w3; walkStart(w3, 26, 30);
        checkTrue("end: a stable start (iron-56) is over at once and says it is stable", w3.done && w3.stop == ChainStop::Stable && chainWalkEndWords(w3).find("stable") != std::string::npos);
        ChainWalk w4; walkStart(w4, 82, 122);
        checkTrue("end: lead-204 is stable in the lab's table but NUBASE2020 lists a limit and a '?' entry: the words say observationally stable, not 'certainly stable'", chainWalkEndWords(w4).find("observationally stable") != std::string::npos && decayChain(82, 122).nodes[0].observationallyStable);
        ChainWalk w5; walkStart(w5, 2, 2);
        checkTrue("end: helium-4 is stable and NUBASE2020 lists no limit and no '?' entry for it: it is not called observationally stable", !decayChain(2, 2).nodes[0].observationallyStable && chainWalkEndWords(w5).find("observationally") == std::string::npos);
        checkTrue("end: lead-206 and lead-208, the ends of two classic series, are both observationally stable in NUBASE2020 (a limit >2.5 Zy and >2.6 Zy and an 'A ?' entry): the words say so, not 'stable' alone",
                  decayChain(82, 124).nodes[0].observationallyStable && decayChain(82, 126).nodes[0].observationallyStable && chainSummary(decayChain(92, 146)).back().find("lead-206 (observationally stable)") != std::string::npos);
    }
    // ---- a nuclide the table does not have (the slider can choose one): it says so, it is not told "no half-life given"
    {
        const ChainGraph g = decayChain(2, 0);
        ChainWalk w; walkStart(w, 2, 0);
        checkTrue("end: helium-2 is not in the table: its chain is just itself and says it is not in the table", g.nodes.size() == 1 && chainSummary(g)[0].find("not in the table") != std::string::npos && w.done && chainWalkEndWords(w).find("not in the table") != std::string::npos);
    }
    // ---- a split and a missing square end a chain
    {
        const ChainGraph g = decayChain(100, 158);                       // fermium-258, spontaneous fission
        checkTrue("end: fermium-258 splits: the chain has an edge that ends in a split, no daughter, and the likeliest path stops there with the reason", g.nodes[0].edges.size() >= 1 && g.nodes[0].edges[0].end == ChainEnd::Fission && g.main.size() == 1 && g.mainWhy.find("split") != std::string::npos);
        ChainWalk w; walkStart(w, 100, 158); walkStep(w, [] { return 0.1; });
        checkTrue("end: the walk from fermium-258 draws a lifetime, then ends with 'splits in two', and says the pieces differ every time", w.done && w.steps.size() == 1 && w.steps[0].end == ChainEnd::Fission && chainWalkEndWords(w).find("pieces differ") != std::string::npos);
    }
}

// the drawing: the trail goes from the middle of a square to the middle of the next one, the walker is marked, a stable nuclide gets no trail, and the tiles are untouched
static void testDecayChainDraw() {
    const double W = 900, H = 700;
    const render::Color fg{0.1f, 0.2f, 0.3f, 1}, muted{0.4f, 0.5f, 0.6f, 1}, accent{0.9f, 0.1f, 0.7f, 1}, card{0.95f, 0.95f, 0.9f, 1}, line{0.5f, 0.5f, 0.5f, 1};
    const atom::ChartColours C{fg, muted, accent, card, line};
    render::Renderer2D R; RecText T;
    const ChartLayout L = chartLayout(W, H);
    const ChainGraph g = decayChain(92, 146);
    ChainWalk walk; walkStart(walk, 92, 146);
    ChainOverlay ov{&g, &walk};
    g_rec.clear(); g_lines.clear(); g_discs.clear(); g_tri.clear();
    drawChart(R, T, C, W, H, 92, 146, 0, 0);
    const std::size_t rects0 = g_rec.size(), lines0 = g_lines.size(), discs0 = g_discs.size();
    g_rec.clear(); g_lines.clear(); g_discs.clear(); g_tri.clear();
    drawChart(R, T, C, W, H, 92, 146, 0, 0, &ov);
    {
        const double gap = L.cell >= 3 ? 0.6 : 0, side = L.cell - gap;
        int tileRects = 0, found = 0;
        for (const RecRect& r : g_rec) if (std::fabs(r.w - side) < 1e-9 && std::fabs(r.h - side) < 1e-9) tileRects++;
        for (const ChartTile& t : chartTiles()) for (const RecRect& r : g_rec) if (std::fabs(r.x - (L.x(t.N) + gap / 2)) < 1e-9 && std::fabs(r.y - (L.y(t.Z) + gap / 2)) < 1e-9 && std::fabs(r.w - side) < 1e-9) { found++; break; }
        checkTrue("chain drawing (flat): the 3,557 tiles are drawn as before, each at its own place (the chain adds lines, discs, label backgrounds and text, drawn over them), and it adds lines", tileRects == 3557 && found == 3557 && g_lines.size() > lines0 && g_discs.size() > discs0 && g_rec.size() >= rects0);
    }
    // each edge: lines in the trail lie ON the segment between the two squares' middles and reach both ends
    int missing = 0, offLine = 0, edges = 0;
    const auto mid = [&](int Z, int N, double& x, double& y) { x = L.x(N) + L.cell / 2; y = L.y(Z) + L.cell / 2; };
    for (const ChainNode& a : g.nodes) for (const ChainEdge& e : a.edges) {
        if (e.end != ChainEnd::Node) continue;
        edges++;
        const ChainNode& b = g.nodes[std::size_t(e.to)];
        double x0, y0, x1, y1; mid(a.Z, a.N, x0, y0); mid(b.Z, b.N, x1, y1);
        const double len = std::hypot(x1 - x0, y1 - y0);
        double tmin = 2, tmax = -1;
        for (const RecLine& l : g_lines) {
            const auto proj = [&](double px, double py, double& t, double& dist) { t = ((px - x0) * (x1 - x0) + (py - y0) * (y1 - y0)) / (len * len); dist = std::fabs((px - x0) * (y1 - y0) - (py - y0) * (x1 - x0)) / len; };
            double ta, da, tb, db; proj(l.x0, l.y0, ta, da); proj(l.x1, l.y1, tb, db);
            if (da > 0.01 || db > 0.01 || ta < -0.001 || tb < -0.001 || ta > 1.001 || tb > 1.001 || std::fabs(ta - tb) < 1e-9) continue;
            tmin = std::min(tmin, std::min(ta, tb)); tmax = std::max(tmax, std::max(ta, tb));
        }
        if (tmin > 0.001 || tmax < 0.999) missing++;
        (void)offLine;
    }
    checkNum("chain drawing (flat): edges of the uranium-238 graph (" + std::to_string(edges) + ") with no line from the middle of one square to the middle of the other", missing, 0, 0);
    checkTrue("chain drawing (flat): the walker is marked at the start with a disc in the accent colour at the middle of the uranium-238 square", [&] { double x, y; mid(92, 146, x, y); for (const RecDisc& d : g_discs) if (std::fabs(d.x - x) < 1e-6 && std::fabs(d.y - y) < 1e-6 && d.c.r == accent.r && d.c.g == accent.g) return true; return false; }());
    // after two steps the walked path is drawn, and the walker has moved
    walkStep(walk, [] { return 0.5; }); walkStep(walk, [] { return 0.5; });
    g_rec.clear(); g_lines.clear(); g_discs.clear();
    drawChart(R, T, C, W, H, 92, 146, 0, 0, &ov);
    checkTrue("chain drawing (flat): after two steps (U-238 -> Th-234 -> Pa-234) the walker is at the middle of the Pa-234 square", [&] { double x, y; mid(91, 143, x, y); for (const RecDisc& d : g_discs) if (std::fabs(d.x - x) < 1e-6 && std::fabs(d.y - y) < 1e-6 && d.c.r == accent.r && d.c.g == accent.g) return true; return false; }() && walk.Z == 91 && walk.N == 143);
    // a stable nuclide: nothing is added
    {
        const ChainGraph gs = decayChain(26, 30); ChainWalk ws; walkStart(ws, 26, 30); ChainOverlay ovs{&gs, &ws};
        g_rec.clear(); g_lines.clear(); g_discs.clear(); drawChart(R, T, C, W, H, 26, 30, 0, 0);
        const std::size_t l1 = g_lines.size(), d1 = g_discs.size();
        g_rec.clear(); g_lines.clear(); g_discs.clear(); drawChart(R, T, C, W, H, 26, 30, 0, 0, &ovs);
        checkTrue("chain drawing: a stable nuclide (iron-56) has no trail: the same lines and discs with and without the overlay", g_lines.size() == l1 && g_discs.size() == d1);
    }
    // the landscape: the trail goes between the middles of the blocks' top faces, at every tilt
    for (const double tilt : {0.4, 1.0}) {
        ChartScene S; S.build(W, H, ChartViewState{tilt, 1, 0, 0});
        ChainWalk w2; walkStart(w2, 92, 146); ChainOverlay ov2{&g, &w2};
        g_rec.clear(); g_lines.clear(); g_discs.clear(); g_tri.clear(); drawChartScene(R, T, C, S, W, H, 92, 146, 0, 0);
        const std::size_t l1 = g_lines.size();
        g_rec.clear(); g_lines.clear(); g_discs.clear(); g_tri.clear(); drawChartScene(R, T, C, S, W, H, 92, 146, 0, 0, &ov2);
        int miss = 0, n = 0;
        for (const ChainNode& a : g.nodes) for (const ChainEdge& e : a.edges) {
            if (e.end != ChainEnd::Node) continue;
            n++;
            const ChartBlock* A = blockOf(S, a.Z, a.N); const ChartBlock* B = blockOf(S, g.nodes[std::size_t(e.to)].Z, g.nodes[std::size_t(e.to)].N);
            const double x0 = (A->top[0] + A->top[4]) / 2, y0 = (A->top[1] + A->top[5]) / 2, x1 = (B->top[0] + B->top[4]) / 2, y1 = (B->top[1] + B->top[5]) / 2;
            const double len = std::hypot(x1 - x0, y1 - y0);
            double tmin = 2, tmax = -1;
            for (const RecLine& l : g_lines) {
                const auto proj = [&](double px, double py, double& t, double& dist) { t = ((px - x0) * (x1 - x0) + (py - y0) * (y1 - y0)) / (len * len); dist = std::fabs((px - x0) * (y1 - y0) - (py - y0) * (x1 - x0)) / len; };
                double ta, da, tb, db; proj(l.x0, l.y0, ta, da); proj(l.x1, l.y1, tb, db);
                if (da > 0.02 || db > 0.02 || ta < -0.001 || tb < -0.001 || ta > 1.001 || tb > 1.001 || std::fabs(ta - tb) < 1e-9) continue;
                tmin = std::min(tmin, std::min(ta, tb)); tmax = std::max(tmax, std::max(ta, tb));
            }
            if (tmin > 0.001 || tmax < 0.999) miss++;
        }
        checkNum("chain drawing (landscape, tilt " + std::to_string(tilt).substr(0, 3) + "): edges with no line between the middles of the two blocks' top faces (of " + std::to_string(n) + ")", miss, 0, 0);
        checkTrue("chain drawing (landscape): the chain adds lines, and the walker disc sits on the uranium-238 block's top",
                  g_lines.size() > l1 && [&] { const ChartBlock* A = blockOf(S, 92, 146); const double x = (A->top[0] + A->top[4]) / 2, y = (A->top[1] + A->top[5]) / 2; for (const RecDisc& d : g_discs) if (std::fabs(d.x - x) < 1e-3 && std::fabs(d.y - y) < 1e-3 && d.c.r == accent.r) return true; return false; }());
    }
}

// the lab: the chain follows the chosen nuclide, the walk uses the lab's random numbers, auto-walk goes at the screen's pace, nothing moves the chosen nuclide or the decay clock
static void testDecayChainLab() {
    AtomLab lab; lab.p = AtomParams(); lab.paused = false; lab.restart(); lab.nowMs = [] { return 0.0; };
    lab.setParam("Z", 92.0); lab.setParam("N", 146.0); lab.setParam("view", std::string(kChartView));
    lab.tick(0);
    const ChainGraph& g = lab.decayGraph();
    checkTrue("lab: the chain is that of the chosen nuclide (uranium-238: starts there, 14 steps along the likeliest path), computed once (the same object on the next call)", g.nodes[0].Z == 92 && g.nodes[0].N == 146 && g.main.size() == 15 && &g == &lab.decayGraph());
    checkTrue("lab: the walk starts at the chosen nuclide, with nothing walked and the chain showing by default", lab.s.walk.Z == 92 && lab.s.walk.N == 146 && lab.s.walk.steps.empty() && lab.s.showChain);
    seedRandom(5);
    lab.walkOnce();
    checkTrue("lab: one press walks one step (uranium-238 gives thorium-234 here, the one way with real odds) and the chosen nuclide stays where the user put it; the lab's own decay clock did not run", lab.s.walk.steps.size() == 1 && lab.s.walk.Z == 90 && lab.s.walk.N == 144 && lab.p.Z == 92 && lab.p.N == 146 && lab.s.decays == 0);
    // auto-walk: one step per 1.6 s of screen time
    lab.s.walkAuto = true;
    int steps0 = int(lab.s.walk.steps.size());
    for (int i = 0; i < 60 * 8 + 6; i++) lab.tick(1.0 / 60);                   // 8.1 s of screen time (so the fifth step, at 8.0 s, is not lost to rounding)
    const int after8 = int(lab.s.walk.steps.size()) - steps0;
    checkTrue("lab: auto-walk takes one step per " + std::to_string(kWalkPaceS).substr(0, 3) + " s of screen time (5 steps in 8.1 s), whatever the drawn lifetimes were (they run from 1.6e-4 s to 4.5e9 years)", after8 == 5 && lab.p.Z == 92 && lab.p.N == 146);
    for (int i = 0; i < 60 * 60; i++) lab.tick(1.0 / 60);
    checkTrue("lab: the walk ends at lead-206 and stops (done, stable) and does not go on", lab.s.walk.done && lab.s.walk.stop == ChainStop::Stable && lab.s.walk.Z == 82 && lab.s.walk.N == 124 && lab.s.walk.steps.size() >= 14);
    checkTrue("lab: the screen time of each step in the walk is about the pace (the first one was a button press, later ones 1.6 s)", lab.s.walk.steps.size() > 3 && std::fabs(lab.s.walk.steps[3].shown - kWalkPaceS) < 0.05);
    // choosing another nuclide restarts the walk and rebuilds the graph
    lab.chooseNuclide(90, 142); lab.tick(0);
    checkTrue("lab: choosing another nuclide (thorium-232) restarts the walk from it and the chain is its chain", lab.s.walk.startZ == 90 && lab.s.walk.startN == 142 && lab.s.walk.steps.empty() && !lab.s.walk.done && lab.decayGraph().nodes[0].Z == 90 && lab.decayGraph().main.size() == 11);
    // a pause stops the screen time, so auto-walk waits
    lab.paused = true; const std::size_t k0 = lab.s.walk.steps.size();
    for (int i = 0; i < 60 * 10; i++) lab.tick(1.0 / 60);
    checkTrue("lab: while the app is paused the auto-walk waits", lab.s.walk.steps.size() == k0);
    lab.paused = false; lab.s.walkAuto = false;
    for (int i = 0; i < 60 * 10; i++) lab.tick(1.0 / 60);
    checkTrue("lab: with auto-walk off nothing walks by itself", lab.s.walk.steps.size() == k0);
    lab.walkRestart();
    checkTrue("lab: restart puts the walker back on the chosen nuclide", lab.s.walk.steps.empty() && lab.s.walk.Z == 90 && lab.s.walk.N == 142);
}
