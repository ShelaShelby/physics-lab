// The chart of nuclides' static geometry as a retained mesh (0.13 stage 4 part B; the types are render/mesh.h). What is in it is exactly what the per-frame drawing code (chart_draw.cpp, drawChart and
// drawChartScene) used to make every frame for the 3,557 squares or blocks and the fixed ground; everything that depends on the chosen nuclide, the pointer, the legend or the chain stays per frame.
//   flat chart:  the magic-number bands, the tiles, the magic-number edges (drawChart's first three loops)
//   landscape:   part A, the ground plate and the magic bands on it, with their outlines; part B, the blocks, far to near in chartPaintOrder() (drawChartScene's two static loops). The chosen
//                nuclide's footprint is drawn between A and B each frame (it depends on the choice, so it is not in the mesh).
// The vertices are in the flat layout's own points (a, b) and the height z, in the order and with the colours the per-frame code pushed them in; the shader (shaders.h, kChartVS) projects them.
// REBUILD TRIGGERS (DECISIONS.md): the picture's size (the layout), the colours of the theme (the palette the tiles, ground and blocks take), the height meaning (landscape only). Nothing else:
// not tilt, zoom or pan (uniforms), not the chosen nuclide, the hovered one, the legend, the chain, the window position. No graphics calls here, so it can be tested.
#pragma once

#include <vector>

#include "chart.h"
#include "chart_view.h"
#include "render/mesh.h"
#include "render/renderer2d.h"

namespace atom {

// The colours the chart is drawn with (the theme's): text, muted text, accent, card and line colour. (Here, not in chart_draw.h, because the mesh is made from them.)
struct ChartColours { render::Color fg, muted, accent, card, line; };

struct ChartMesh {
    std::vector<render::MeshVertex> v;
    int partA = 0, partACount = 0;                     // landscape: the ground (first vertex, vertices); flat: not used
    int partB = 0, partBCount = 0;                     // landscape: the blocks; flat: the whole static part
};
ChartMesh chartFlatMesh(const ChartLayout& L, const ChartColours& C);
ChartMesh chartLandscapeMesh(const ChartLayout& L, const ChartColours& C, ChartHeightMode mode);
render::MeshProjection chartMeshProjection(const ChartProjection& P);

// The CPU twin of the shader: the screen positions (x, y, two floats a vertex) and colours of the triangles of vertices [first, first + count) under projection P. Side faces the shader would drop and
// degenerate lines are left out, so the result is what the GPU draws, triangle by triangle. In double, then rounded to float once.
// tri (optional): for each triangle kept, the index in v of its first vertex.
void chartMeshReference(const std::vector<render::MeshVertex>& v, int first, int count, const ChartProjection& P, std::vector<float>& xy, std::vector<float>& rgba, std::vector<int>* tri = nullptr);

}  // namespace atom
