// The Data panel (0.51): the 📈 button in the top bar (or Ctrl+D) opens it at the left of the picture. It shows the lab's data as tables (each value with its unit
// and source), plots any column against any other (with 1/x, ln x ... transforms and error bars), fits a model with uncertainties, chi^2 and residuals, sends values
// and fitted parameters to the calculator, exports CSV, reads the user's own data, and records the selected planet as the Orbit Lab runs.
// The logic is analysis/* (tested by tests/test_analysis.cpp); this file draws it.
#pragma once

#include <vector>

#include "app/shell.h"

struct AppState;

namespace ui {

bool dataOpen();
void dataToggle();
void dataClose();
bool dataFieldActive();          // its text box had the keyboard last frame (Esc then closes the panel first)
// Every frame, open or not: the recording of the Orbit Lab keeps its samples while the panel is closed.
void dataTick(AppState& app);
// Draws it (when open) and returns where it is; {0,0,0,0} when closed.
app::shell::Rect dataPanelDraw(AppState& app, const app::shell::Frame& fr, const app::shell::Metrics& m, const std::vector<app::shell::Rect>& cards);

}  // namespace ui
