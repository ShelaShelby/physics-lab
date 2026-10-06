// Drawing the chart of nuclides (0.11 stage 2). Only rectangles and lines go to the renderer (they all land in the renderer's ONE triangle batch: about 3,600 squares are one draw call);
// the text goes through ChartText, so this file needs nothing from Dear ImGui. The layout, colours and words come from chart.h.
#pragma once

#include <string>

#include "chart.h"
#include "chart_mesh.h"
#include "decay_chain.h"
#include "render/renderer2d.h"

namespace atom {

struct ChartText {
    virtual ~ChartText() = default;
    virtual double width(const std::string& t, double size, bool bold) const = 0;
    virtual void draw(const std::string& t, double x, double y, double size, render::Color c, double align, bool bold) const = 0;   // y: the baseline, align 0 left, 0.5 centre, 1 right
};

// The decay chain of the chosen nuclide on the chart (0.12 stage 1): the trail of every way it can decay, the likeliest solid and thick, the other measured ways thinner, the rare ones very faint, a way whose
// odds are not measured has a label ending in ?, a mode label at the middle of the likeliest steps; the walker (the walk of decay_chain.h) as a disc, the squares it has been through and the path it took
// over the trail. Drawn only for a nuclide that has a chain; a stable nuclide gets nothing. Pointers may be null (graph: nothing; walk: no walker).
struct ChainOverlay { const ChainGraph* graph = nullptr; const ChainWalk* walk = nullptr; };

// The retained copy of the chart's static geometry on the GPU (0.13 stage 4 part B, atom/chart_mesh.h): the tiles / blocks and the ground are made once and drawn from the GPU buffer every frame;
// the chosen nuclide, the hover, the legend, the notes and the chain are still drawn per frame on top. One slot for the flat chart and one for the landscape (switching between them rebuilds nothing).
// A slot is rebuilt ONLY when the picture's size, the theme's colours or (landscape) the height meaning changes. Tilt, zoom and pan are uniforms. Kept by AtomView; pass nullptr to draw as up to 0.13
// stage 3 did (everything per frame: the reference, and the "GPU chart cache" switch Off).
struct ChartCache {
    struct Slot { int id = -1; ChartMesh mesh; double w = -1, h = -1; ChartColours colours{}; int mode = -1; long builds = 0; };
    Slot flat, land;
    void release(render::Renderer2D& R);       // frees the GPU buffers (the next draw makes them again)
};

// 0.15 stage 5f: with the new UI the legend and the notes box are in the Legend chip (the picture's own copies were under the Controls panel); false = the picture leaves them out. Default true (the old UI,
// the tests of the pictures). chartLegendDrawn(): what the last picture did (for the automatic checks: 1 drawn, 0 left out, -1 no chart drawn yet).
void setChartLegendInPicture(bool on);
int chartLegendDrawn();

// w x h points. selZ/selN: the nuclide chosen in the lab (marked); hovZ/hovN: the one under the pointer (0, 0 = none).
void drawChart(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, int selZ, int selN, int hovZ, int hovN, const ChainOverlay* chain = nullptr, ChartCache* cache = nullptr);
// The per-frame builders of the static parts, as they were drawn every frame up to 0.13 stage 3: still what runs with no cache, and the reference the retained mesh is tested against.
void drawChartStaticFlat(render::Renderer2D& R, const ChartColours& C, const ChartLayout& L);
void drawChartStaticGround(render::Renderer2D& R, const ChartColours& C, const ChartLayout& L, const ChartProjection& P);
void drawChartStaticBlocks(render::Renderer2D& R, const ChartColours& C, const ChartScene& S);


// The landscape (0.11 stage 3): the scene's blocks in painter's order (see chart_view.h), with the ground, the magic rows and columns on it, the chosen nuclide's ring and pin, the legend. Used whenever the
// view is not exactly the flat chart (tilt, zoom or pan different from 0, 1, 0).
class ChartScene;
void drawChartScene(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChartScene& S, double w, double h, int selZ, int selN, int hovZ, int hovN, const ChainOverlay* chain = nullptr, ChartCache* cache = nullptr);

}  // namespace atom
