// Drawing the chart of nuclides (0.11 stage 2). Only rectangles and lines go to the renderer (they all land in the renderer's ONE triangle batch: about 3,600 squares are one draw call);
// the text goes through ChartText, so this file needs nothing from Dear ImGui. The layout, colours and words come from chart.h.
#pragma once

#include <string>

#include "chart.h"
#include "render/renderer2d.h"

namespace atom {

struct ChartText {
    virtual ~ChartText() = default;
    virtual double width(const std::string& t, double size, bool bold) const = 0;
    virtual void draw(const std::string& t, double x, double y, double size, render::Color c, double align, bool bold) const = 0;   // y: the baseline, align 0 left, 0.5 centre, 1 right
};
struct ChartColours { render::Color fg, muted, accent, card, line; };

// w x h points. selZ/selN: the nuclide chosen in the lab (marked); hovZ/hovN: the one under the pointer (0, 0 = none).
void drawChart(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, int selZ, int selN, int hovZ, int hovN);


// The landscape (0.11 stage 3): the scene's blocks in painter's order (see chart_view.h), with the ground, the magic rows and columns on it, the chosen nuclide's ring and pin, the legend. Used whenever the
// view is not exactly the flat chart (tilt, zoom or pan different from 0, 1, 0).
class ChartScene;
void drawChartScene(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChartScene& S, double w, double h, int selZ, int selN, int hovZ, int hovN);

}  // namespace atom
