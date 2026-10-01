// A tiny SVG painter for the Learn timelines' step pictures (the web's `svg` strings, drawn in a 56 × 56 box).
// It knows what the web's 22 atom icons and 29 Solar System icons use: circle, ellipse (with rotate), rect, path
// (M, L, Q, T, Z; stroked, or filled), text, a radial gradient fill, opacity and stroke-dasharray.
#pragma once

#include <string>

#include "imgui.h"

namespace ui {

// Draws `svg` into the current window's draw list, its (0, 0) at `origin` (screen position), `scale` screen points per SVG unit.
void svgIcon(const std::string& svg, ImVec2 origin, float scale);

}  // namespace ui
