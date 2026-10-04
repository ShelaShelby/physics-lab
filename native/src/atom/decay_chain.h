// Decay chains on the chart of nuclides (0.12 stage 1): the path a nuclide takes down to a stable one, as a graph (every way it can decay is followed), and a RANDOM WALK down that graph that uses
// the same two draws as the lab's decay clock (AtomLab::decTick): a lifetime from the exponential law and a branch from the listed odds. Plain logic, no drawing, so it can be tested without a
// screen. The half-lives, modes and odds are the lab's table (atom_core.h, NUBASE2020), which is the only source of numbers here; the way a mode moves on the chart is decMove().
//
// WHAT THE CHAIN SAYS AND DOES NOT SAY (the rules, stated here and in the app's words):
//  - A nuclide is TIMED only when the table has a measured half-life for it (HeightKind::Measured) or an approximate one (Approx, "about"). A nuclide whose half-life is only a limit, only an
//    extrapolation from systematics, or not given at all is NEVER given a time: the chain stops there, says why, and does not follow its listed modes (the lab's decay table holds a number for
//    some of these, an estimate, and the lab's own decay clock still uses it; the chain does not).
//  - The odds are the listed branching shares, normalised to add up to 1 (the table lists, for example, alpha 1 and fission 5.4e-7 for uranium-238: the shares are not exclusive fractions).
//    Where NUBASE2020 marks the odds as NOT MEASURED, the lab's table lists the modes with equal shares; the chain then uses those equal shares only as a stand-in, and marks every such branch
//    and step as "odds not measured", so an equal split is never presented as data. Such a node has no "likeliest" branch.
//  - A mode that splits the nucleus (SF, B-SF, B+SF) ends the chain: the pieces differ every time. A mode whose daughter is not a nuclide of the table ends it too.
//  - The chain is a graph computed ONCE per chosen nuclide; the walk draws one lifetime and one branch per step.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "atom_core.h"
#include "chart_view.h"

namespace atom {

// ---- the two draws (shared with the lab's decay clock) ----
std::vector<double> decOdds(const DecayInfo& d);                         // each listed mode's share, in the listed order, normalised to add up to 1
double decDrawLifetime(double halfLifeS, double u);                      // the exponential law: -T / ln 2 * ln(1 - u), u in [0, 1)
std::size_t decDrawBranch(const DecayInfo& d, double u);                 // the index of the mode drawn with the normalised odds, u in [0, 1)

// ---- can a nuclide be timed? ----
enum class ChainTime : unsigned char { Measured, Approximate, None };
struct ChainTiming { ChainTime kind; double halfLife; HeightKind why; };   // halfLife only when kind != None; why: the height kind when None (Limit, Extrapolated, Unknown)
ChainTiming chainTiming(int Z, int N);                                    // for a nuclide that is not stable
inline constexpr double kChainPracticalStableS = 1e15 * kYearS;           // the lab's own threshold (isoInfo): a half-life this long is "practically stable"

// ---- the graph ----
enum class ChainStop : unsigned char { Continues, Stable, NoHalfLife };    // what a node is: it decays on, it is stable, or it cannot be timed (stop)
enum class ChainEnd : unsigned char { Node, Fission, NotInTable };         // where an edge goes: to a nuclide, to a split (no one daughter), to a square the table does not have
struct ChainEdge {
    std::string mode;          // the table's mode text ("A", "B-", "B+", "2B-", "14C", "B-n" ...)
    ChainEnd end = ChainEnd::Node;
    int to = -1;               // the node index when end == Node
    int dZ = 0, dN = 0;        // the move on the chart (0, 0 for a fission)
    double p = 0;              // the normalised share of this mode among the node's listed modes
    bool measured = true;      // the odds are measured (false: NUBASE2020 marks them not measured; p is then an equal stand-in)
    bool likeliest = false;    // the mode with the largest share (the first listed on a tie); never set where the odds are not measured and there is more than one mode
};
struct ChainNode {
    int Z = 0, N = 0;
    ChainStop stop = ChainStop::Continues;
    ChainTiming timing{ChainTime::None, 0, HeightKind::Unknown};
    bool observationallyStable = false;   // Stable, and NUBASE2020 lists a half-life lower limit and/or decay entries marked "?" for it (the lab's 69 + 2 of the 71)
    std::vector<ChainEdge> edges;         // empty unless Continues
    double reach = 0;                     // the chance that a walk from the start passes through this nuclide
};
struct ChainGraph {
    std::vector<ChainNode> nodes;         // nodes[0] is the start; the others in the order found
    std::vector<int> main;                // the likeliest path: node indices from the start, following each node's likeliest edge while it goes to a node
    std::string mainWhy;                  // why the likeliest path stops: stable, no half-life, odds not measured, a split, not in the table
    int find(int Z, int N) const { for (std::size_t i = 0; i < nodes.size(); i++) if (nodes[i].Z == Z && nodes[i].N == N) return int(i); return -1; }
    int edgeCount() const { int n = 0; for (const ChainNode& x : nodes) n += int(x.edges.size()); return n; }
};
ChainGraph decayChain(int Z, int N);                                      // every way down from this nuclide
struct ChainEndProb { int Z = 0, N = 0; ChainStop stop = ChainStop::Stable; ChainEnd end = ChainEnd::Node; double p = 0; };   // end == Node: it ends AT this nuclide (Z, N); else it ends in a split / a missing square
std::vector<ChainEndProb> chainEnds(const ChainGraph& g);                // where a walk ends, with the chance of each; the chances add up to 1

// ---- the walk ----
struct WalkStep {
    int fromZ = 0, fromN = 0;
    std::string mode;
    ChainEnd end = ChainEnd::Node;
    int toZ = 0, toN = 0;      // when end == Node
    double p = 0;              // the share of the mode drawn
    bool measured = true;      // the odds were measured (false: an equal stand-in was used)
    int modes = 1;             // how many modes the nuclide lists
    double halfLife = 0;       // the half-life the lifetime was drawn from, s
    bool approximate = false;
    double lived = 0;          // the drawn lifetime, s (a random draw from the exponential law, not a schedule)
    double shown = 0;          // the screen time between the previous step and this one, s
};
struct ChainWalk {
    int startZ = 0, startN = 0, Z = 0, N = 0;      // where it began and where it is now
    std::vector<WalkStep> steps;
    bool done = false;
    ChainStop stop = ChainStop::Continues;         // why it is done: Stable, or NoHalfLife; a split or a missing square: stop stays Continues and the last step's end says
    HeightKind why = HeightKind::Unknown;          // NoHalfLife: Limit, Extrapolated or Unknown
    double drawnS = 0;                             // the drawn lifetimes added up, s
    double screenS = 0;                            // the screen time since the walk began, s
    double sinceStep = 0;                          // the screen time since the last step, s
};
void walkStart(ChainWalk& w, int Z, int N);                               // begin at this nuclide (done at once when it is stable or cannot be timed)
void walkTime(ChainWalk& w, double dt);                                   // screen time passes (only while the walk is not done)
bool walkStep(ChainWalk& w, const std::function<double()>& rnd);          // one decay: draws the lifetime (one number from rnd) and then the branch (one number); false when done
inline constexpr double kWalkPaceS = 1.6;                                  // auto-walk: one step per this many seconds of SCREEN time, whatever the drawn lifetimes were

// ---- the words ----
std::string chainNuclide(int Z, int N);                                    // "uranium-238"
std::string chainModeShort(const std::string& mode);                       // "α", "β⁻", "β⁺/EC", "14C" for the chart
std::string chainModeWords(const std::string& mode, int dZ, int dN);       // "alpha: two protons and two neutrons leave"
std::string chainStepWords(const WalkStep& s);                             // what happened: mode words and "uranium-238 → thorium-234"
std::string chainStepTime(const WalkStep& s);                              // the drawn lifetime, the half-life, the screen time and how much faster than real time
std::string chainStopWords(const ChainNode& n);                            // why a chain stops at this node
std::string chainWalkEndWords(const ChainWalk& w);                         // why the walk is over
std::vector<std::string> chainSummary(const ChainGraph& g);                // the chain in words: the likeliest path, the branches, the ends and their chances
const char* chainTimeNote();                                               // the standing note about time: never real time, each step a random draw

}  // namespace atom
