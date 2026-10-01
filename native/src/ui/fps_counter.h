// The frames-per-second counter (the web's #fps), drawn in one place for every lab.
#pragma once
#include "imgui.h"
namespace ui {
// Draws it in the top right corner of the picture area (window coordinates: it starts at y = pictureTop,
// is pictureWidth wide) on the given draw list. `scale` is the screen's scale; Text size is applied inside.
void drawFpsCounter(ImDrawList* dl, float pictureWidth, float pictureTop, float scale, double fps);
}
