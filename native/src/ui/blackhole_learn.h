// 0.55.0: the Black Hole Lab's guide "What is real in this picture" (as ui/molecule_learn.cpp): what is exact, what is soft, and what the lab leaves out.
// Plain words. Every number in it is the lab's own (the module, the readout, the measured precision of the drawing, named in REPORT.md 0.55.0); it adds none.
#pragma once

#include <string>
#include <vector>

namespace ui {

struct AppState;

struct BhLearnSection { std::string key, title, html; };
std::vector<BhLearnSection> blackholeLearnSections();
void blackholeLearnWindow(AppState& app);   // the open guide (app.bhLearn == 1), if any

}  // namespace ui
