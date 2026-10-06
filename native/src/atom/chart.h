// The chart of nuclides (0.11 stage 2): one tile per nuclide the lab knows (3,557, the same set NUBASE2020 lists as ground states), neutrons across, protons up, coloured by how the
// nuclide decays. Everything here is plain logic (tiles, the colour rule, the layout, which tile is under a point, the arrow-key step, the hover words) so it can be tested without a
// screen; the drawing is in chart_draw.cpp. The half-lives, decay modes and odds come from the lab's table (atom_core.h, NUBASE2020 through mendeleev), spins and the kind of half-life
// from nubase.h (NUBASE2020 itself).
#pragma once

#include <string>
#include <vector>

namespace atom {

inline constexpr const char* kChartView = "Chart of nuclides";   // the value of AtomParams::view

// How a nuclide decays, for the colour. THE RULE (stated in the legend and the About page):
//  - Stable: the lab's table calls it stable ("S"; NUBASE2020 "stbl"). Nothing else is called stable.
//  - Otherwise, when the odds are measured, the colour is the KIND with the largest total odds (the odds of all the modes of one kind added: a beta-delayed branch adds to its beta; the kind
//    listed first wins a tie). When the odds are NOT measured, it is the kind of all the listed modes if they agree, else Mixed. The kind of a mode: A alpha; SF fission; a mode that starts with B- (B-, 2B-, B-n, B-2n, B-A, B-SF ...) beta minus (a "beta-delayed" mode is coloured by its beta);
//    one that starts with B+ (B+, 2B+, B+p, B+A ...) beta plus / electron capture (the table writes electron capture as B+); p, 2p, 3p proton; n, 2n, 3n neutron; anything else Other.
enum class ChartClass : unsigned char { Stable, BetaMinus, BetaPlus, Alpha, Fission, Proton, Neutron, Other, Mixed };
inline constexpr int kChartClasses = 9;
struct ChartClassInfo { const char* name; const char* hex; const char* words; };   // hex "" = the theme's text colour (stable)
const ChartClassInfo& chartClassInfo(ChartClass c);

struct ChartTile { short Z, N; ChartClass cls; };
const std::vector<ChartTile>& chartTiles();              // one per nuclide, sorted by Z then N, built once
ChartClass chartClassOf(int Z, int N);                   // the rule above, for a nuclide in the table
ChartClass chartClassOfMode(const std::string& mode);    // the kind of one mode text
int chartClassCount(ChartClass c);                       // tiles of that class
bool chartHas(int Z, int N);                             // is there a tile here?
int chartMaxN();                                         // 177
int chartMaxZ();                                         // 118

// Magic numbers (same list for protons and neutrons): the lines of the chart.
inline constexpr int kChartMagic[7] = {2, 8, 20, 28, 50, 82, 126};
const char* chartMagicWords();
const char* chartMagicShort();                           // the same in one short line (small pictures)

// Where things are: picture points, y down. A tile for (Z, N) is the square [x(N), x(N)+cell] x [y(Z), y(Z)+cell]; the magic lines are the edges of the tiles' own row or column.
struct ChartLayout {
    double x0 = 0, y0 = 0, cell = 1;     // x0: left edge of column N = 0; y0: top edge of the row Z = zMax
    int nMax = 177, zMax = 118;
    double left = 34, bottom = 30, top = 24, right = 8;   // room for the axis numbers, titles and the status line
    double x(int N) const { return x0 + N * cell; }
    double y(int Z) const { return y0 + (zMax - Z) * cell; }
};
ChartLayout chartLayout(double w, double h);             // fits the whole chart into a picture w x h points
// 0.15 stage 5a (new UI): the picture fills the window, so the top bar and the toolbar float over it; the chart keeps this much room (points) at the top and the bottom. 0, 0 = as before.
void setChartInsets(double top, double bottom);
bool chartTileAt(const ChartLayout& L, double px, double py, int& Z, int& N);   // the nuclide under that point, false when none (outside, or a square with no nuclide)
// How tall a box can be that starts at the top margin (fromTop) or ends at the bottom margin, across the picture columns x0..x1, before it would cover a tile: the distance from that edge to the
// nearest tile in any of those columns (a column beyond the chart is empty all the way). Used to place the legend where the chart leaves room.
double chartFreeHeight(const ChartLayout& L, double h, bool fromTop, double x0, double x1);
bool chartMove(int& Z, int& N, int dZ, int dN);          // one step; true and moved only when the square there holds a nuclide; false and unchanged otherwise (the edge of what has been observed)

// ---- the valley and the island (0.11 stage 4): two different things, never blurred ----
// THE VALLEY OF STABILITY is MEASURED: it is the band of stable and very long-lived nuclides that the chart already draws (the tiles the lab's table calls stable, the ridge of the landscape). The chart
// names it where it stands: the label is attached to the MEDIAN stable tile (the stable tiles in order of Z, then N; the middle one of the 251), a tile found in the data, not a typed place.
// THE ISLAND OF STABILITY is a PREDICTION. A source for where theory puts it could NOT be reached from these sessions (every host tried answered "CONNECT tunnel failed, response 403", the
// proxy's policy denial: Wikipedia, arXiv, INSPIRE, Crossref, the journals and the laboratories' sites: REPORT.md lists them), so NOTHING SPECIFIC IS DRAWN OR QUOTED about it: no region, no magic
// numbers. The chart says only what the owner's brief says and the data can back: theory expects such a region beyond the heaviest known nuclides, the lab has no sourced prediction to draw, where
// it lies is disputed, and the measured chart simply ends at the heaviest known nuclides (the largest Z among the tiles, read from the table).
ChartTile chartValleyAnchor();                 // the median stable tile
int chartHeaviestZ();                          // the largest Z among the tiles: where the measured chart ends
std::vector<ChartTile> chartHeaviestTiles();   // the tiles with that Z
const char* chartValleyWords();                // legend: what the valley is (full)
const char* chartValleyShort();
const char* chartIslandWords();                // legend: what the island is: a prediction, not seen, not drawn (full)
const char* chartIslandShort();
std::string chartHeaviestLabel();              // "heaviest known: Z = 118 (oganesson); nothing observed beyond"

// The words of the hover box (one string per line): name, whether it is stable or its half-life, how it decays and the odds, the spin, and whether the lab has a measured charge radius
// and a measured quadrupole moment. Where there is no value the line says so.
std::vector<std::string> chartHover(int Z, int N);

}  // namespace atom
