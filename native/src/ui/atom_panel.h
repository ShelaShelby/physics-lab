// The Atom Lab's side panel and the button bar under its picture (ported from the web's Atom Lab
// params, actions and buildUI: the periodic table, element card, facts, decay history, light
// spectrum and temperature cards).
#pragma once

namespace ui {

struct AppState;

// The two buttons at the top of every panel: Orbit Lab / Atom Lab.
void labPicker(AppState& app);

// Draws the Atom Lab panel in a window `width` points wide on the right of the main window.
void atomPanel(AppState& app, float width);

// The bar under the Atom Lab picture: the view and zoom buttons (the web's "near" bar) and the hint.
// Returns its height in window coordinates; call with draw = false first to measure it.
float atomBottomBar(AppState& app, float x, float y, float width, bool draw);

}  // namespace ui
