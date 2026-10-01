// The Atom Lab's Learn guides (web: lrnLook, lrnScale, lrnMass, lrnTimeline), drawn with Dear ImGui.
// Their text comes from atom/atom_learn.* and is checked word for word against the web's.
#pragma once

namespace ui {

struct AppState;

void atomLearnButtons(AppState& app);   // the Learn card
void atomHelpButton(AppState& app);     // "❓ What am I even looking at?" at the top of the panel
void atomLearnWindow(AppState& app);    // the open guide (if any)

}  // namespace ui
