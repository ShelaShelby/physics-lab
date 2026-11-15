// A tiny SVG painter for the Learn timelines' step pictures.
// (M, L, Q, T, Z; stroked, or filled), text, a radial gradient fill, opacity and stroke-dasharray.
#pragma once

#include <string>

#include "imgui.h"

namespace ui {

// Draws `svg` into the current window's draw list, its (0, 0) at `origin` (screen position), `scale` screen points per SVG unit.
void svgIcon(const std::string& svg, ImVec2 origin, float scale);

}  // namespace ui
