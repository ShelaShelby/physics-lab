// The Atom Lab's side panel and the button bar under its picture (ported from the web's Atom Lab
// params, actions and buildUI: the periodic table, element card, facts, decay history, light
// spectrum and temperature cards).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

namespace ui {

struct AppState;

// 0.15 stage 5d: the old panel's choices for a setting (key, value, "words · note"; the words before " · " are on the button, the note is shown under the row) and its long notes
// ("energy", "phase", "super", "chart", "chain"), shared with the new UI's Controls panel so both change the same setting with the same values and say the same words.
using AtomChoices = std::vector<std::pair<std::string, std::string>>;
const AtomChoices& atomChoices(const std::string& key);   // empty for an unknown key
const char* atomNote(const std::string& which);
std::pair<std::string, std::string> atomSplitLabel(const std::string& s);   // "1s · sphere" -> {"1s", "sphere"}; "Auto-fit (orbit fills the canvas)" -> {"Auto-fit", "orbit fills the canvas"}

// The two buttons at the top of every panel: Orbit Lab / Atom Lab.
void labPicker(AppState& app);
void openLab(AppState& app, int lab);          // switch to a lab (0 Orbit Lab, 1 Atom Lab)

// Draws the Atom Lab panel in a window `width` points wide on the right of the main window.
void atomPanel(AppState& app, float width);
void atomBigTable(AppState& app);       // the large periodic table window (when app.bigTable)
// 0.15 stage 3: the same controls in a floating window (the new UI's temporary "Old panel"); window pixels.
void atomPanelFloating(AppState& app, bool* open, float x, float y, float w, float h);

// The periodic table's cells (0.15 stage 5b: shared by the old panel, the large table window and the new UI's element dropdown). Geometry in window pixels; s = pixels per unit.
struct PtGeom {
    ImVec2 o;                                   // top left of the table
    float cw = 20, ch = 20, gap = 2, rowGap = 8;
    float s = 1, corner = 3, symPx = 10, markPx = 11;
    bool boldAll = false;                       // every symbol bold (the large table)
    const char* note = nullptr;                 // not null: each cell is also noted for PL_SCRIPT as note + Z ("pt.92")
    ImVec2 cellPos(int row, int col) const;     // rows 1-7, then 9 and 10 for the lanthanides and actinides
};
int periodicTableCells(const PtGeom& g, int sel, ImU32 fg, ImU32 accent, ImU32 muted);   // the element clicked, or 0

// The bar under the Atom Lab picture: the view and zoom buttons (the web's "near" bar) and the hint.
// Returns its height in window coordinates; call with draw = false first to measure it.
float atomBottomBar(AppState& app, float x, float y, float width, bool draw);

}  // namespace ui
