// The Atom Lab's side panel and the button bar under its picture (ported from the web's Atom Lab
// params, actions and buildUI: the periodic table, element card, facts, decay history, light
// spectrum and temperature cards).
#pragma once

namespace ui {

struct AppState;

// The two buttons at the top of every panel: Orbit Lab / Atom Lab.
void labPicker(AppState& app);
void openLab(AppState& app, int lab);          // switch to a lab (0 Orbit Lab, 1 Atom Lab)

// Draws the Atom Lab panel in a window `width` points wide on the right of the main window.
void atomPanel(AppState& app, float width);
void atomBigTable(AppState& app);       // the large periodic table window (when app.bigTable)
// 0.15 stage 3: the same controls in a floating window (the new UI's temporary "Old panel"); window pixels.
void atomPanelFloating(AppState& app, bool* open, float x, float y, float w, float h);

// The bar under the Atom Lab picture: the view and zoom buttons (the web's "near" bar) and the hint.
// Returns its height in window coordinates; call with draw = false first to measure it.
float atomBottomBar(AppState& app, float x, float y, float width, bool draw);

}  // namespace ui
