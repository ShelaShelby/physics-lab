// The calculator panel (0.49): the 🧮 button at the right of the top bar (or Ctrl+=) opens it as a card under the top bar that stays open while the lab is used
// (click a planet and its formulas appear). The maths is app/calc.* (tested by tests/test_calc.cpp); this file only draws it and collects the labs' numbers.
#pragma once

#include <string>
#include <vector>

#include "app/shell.h"

struct AppState;

namespace ui {

bool calcOpen();
void calcToggle();
void calcClose();
bool calcFieldActive();
// 0.51, the Data panel: opens the calculator and adds text at the end of its line (a value with its unit, "(1.523 au)")
void calcInsert(const std::string& text);
// 0.51: keeps a named variable as if the line "name = value" had been typed and kept ("fit_b = 1.0002 (yr/au)"); false and why when the calculator refuses it
bool calcDefine(const std::string& name, const std::string& value, std::string& why);          // its text box had the keyboard last frame (Esc then closes the calculator first)
// Draws it (when open) and returns where it is; {0,0,0,0} when closed. cards: the floating cards it steps aside from.
app::shell::Rect calcPanel(AppState& app, const app::shell::Frame& fr, const app::shell::Metrics& m, const std::vector<app::shell::Rect>& cards);

}  // namespace ui
