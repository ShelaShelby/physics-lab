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
// "Check a jump" (0.10.8 stage 3): the result lines for the upper and lower shell ("2p", "1s"), for element Z: each allowed or refused jump (atom/lifetime.h, NIST ASD), then the standing note
std::vector<std::string> atomJumpCheck(int Z, const std::string& from, const std::string& to);
std::pair<std::string, std::string> atomSplitLabel(const std::string& s);   // "1s · sphere" -> {"1s", "sphere"}; "Auto-fit (orbit fills the canvas)" -> {"Auto-fit", "orbit fills the canvas"}

// 0.15 stage 5f: the Atom Lab's subject card and data sheet, made from the strings the old panel's element card, facts card, isotope line, decay history and hint line draw (the old panel
// logs the same strings with PL_LOG_CARD, so a test can compare them). Nothing here is a new fact or number.
struct AtomSubject {
    std::string name, kind, keyValue, keyCaption;                       // "Uranium-238", the element's category, Z, "protons"
    std::vector<std::pair<std::string, std::string>> values;           // card rows: label, value (an empty label: a line of its own)
    std::string isotope;                                                // the old isotope line ("☢ Uranium-238: half-life ...")
    std::vector<std::string> valuesTab;                                 // name, category / period / state chips, "Where you meet it", the melting / boiling / density lines, their notes and footnote
    std::vector<std::string> decayTab;                                  // the old Decay history lines
    std::string hint;                                                   // the old bar's hint line (it depends on the view)
};
AtomSubject atomSubject(const AppState& app);
struct AtomSheetCard { const char* id; const char* label; };   // 0.15 stage 5h: a data sheet tab that shows one of the old panel's cards (tab label)
std::vector<AtomSheetCard> atomSheetCards(const AppState& app);   // the cards the old panel shows now for this subject and view
void atomSheetCard(AppState& app, const std::string& id);         // draws one at the current ImGui cursor, by the very same function the old panel calls
std::vector<std::string> atomPlainLines(const std::string& html);      // "<div><b>A:</b> x</div>" -> {"A: x"}: tags removed, entities decoded, one line per <div> / <p>

void openLab(AppState& app, int lab);          // switch to a lab (0 Orbit Lab, 1 Atom Lab)


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

}  // namespace ui
