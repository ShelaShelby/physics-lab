// 0.15 stage 3: the new UI's shell (DESIGN.md): the picture fills the window; a slim top bar (menu, breadcrumbs, the chip, search, icons), a floating toolbar at the bottom, a
// readout at the bottom right, floating panels that collapse to their title, dropdown panels, search (Ctrl+K), the 💡 card and the reading view. Behind Settings → New UI (preview),
// Off by default: with it Off nothing here runs and the app is exactly the 0.14 layout. The sizes and positions come from app/shell.* (tested); this only draws and routes input.
#pragma once

#include "ui/orbit_panel.h"

namespace ui {

// Builds this frame's bars and panels (Dear ImGui windows) and sets how the lab picture is drawn (the full window, no overlays that moved into the bars, the centre shift).
// Call after ImGui::NewFrame and before the picture is drawn. winW, winH: the window in pixels; fps and frameDt: for the readout and the easing.
void shellFrame(AppState& app, int winW, int winH, double fps, double frameDt);
float shellTopBarH();                                    // the top bar's height in window pixels (the Learn and Achievements pages start below it); 0 before the first frame
bool shellSpace(AppState& app, bool textInput);          // the Space key: plays / pauses unless a text field has the keyboard. True: it did
void shellNoteDrag();                                    // the user dragged the picture: the centre shift holds still until something opens or closes

}  // namespace ui
