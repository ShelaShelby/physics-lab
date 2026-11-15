// 0.15 stage 3: what the Orbit Lab's controls DO, in one place, so the old side panel and the new UI's chip and toolbar run exactly the same code (the state test compares the two).
// No Dear ImGui here: plain changes to the lab's state (ui/orbit_panel.h AppState).
#pragma once

#include <string>
#include <vector>

#include "ui/orbit_panel.h"

namespace ui {

// ---- Pick a body (the old "Go to" tiles, and the new toolbar's list) ----
struct PickItem {
    std::string name;
    const char* color = "#ffffff";       // the dot's colour (the body's own: a planet's, its moons' use their planet's)
    orbit::Selection sel;
    std::string tip;                     // "Moon · moon of Earth" for a moon, else empty
    double beltZoom = 0;                 // belts only: the zoom that shows all of it
};
struct PickGroup { const char* title; std::vector<PickItem> items; };
// The groups in the order of the old panel: Star and planets, Dwarf planets, Famous comets, Moons, Belts and the far edge, Far away.
std::vector<PickGroup> pickGroups(const AppState& app);
// A click on an item: select it and fly there (the selected one again lets go), switch on what it needs (Dwarf planets, Comets, Moons, Belts, Far-away things).
void pickItem(AppState& app, const PickItem& item);

// ---- Date jumps (the old "Jump to a date" buttons, and the new Time panel's list) ----
struct DateJump { const char* name; const char* date; const char* tip; const char* time; };   // date null: back to today
const DateJump* dateJumps(int& count);
void jumpFromBoxes(AppState& app);                         // the Jump button: reads app.dateText and app.timeText, jumps, sets the note
void doDateJump(AppState& app, int index);                 // fills the date and time boxes, jumps, and sets the note

// ---- the chip (the date and the speed) ----
struct ChipText { std::string date, extra, speed; };       // date: "5 Oct 2026" (a picked date adds its time); extra: "3.2 years after 5 Oct 2026" or empty; speed: "1 s = 29.1 days" / "Paused"
ChipText chipText(const AppState& app);

// The Time speed slider's words and range come from orbit/units.h (kTspeedMin, kTspeedMax) and tspeedText(); nothing else to do here.

// ---- the long tooltips of the two Real-physics tick boxes (the old panel and the new Show panel show the same words) ----
std::string relativityTip();
std::string wobbleTip();

// ---- the subject card's action links (the old side column's "Slow time to watch moons" / "Slow time to watch it turn") ----
bool slowMoonsAvailable(const AppState& app);              // Moons is on and the selected planet (or the planet of the selected moon) has moons shown
void slowMoons(AppState& app);                             // Time speed so that one lap of the moon (the fastest one of the planet) takes about 4 seconds, and follow
bool slowSpinAvailable(const AppState& app);               // a planet with real spin data is selected
void slowSpin(AppState& app);                              // Time speed so that one turn takes about 24 seconds, follow, and (True scale) zoom in close enough to see it

// One line of the lab's state, for the automatic checks (PL_SHELL_STATE): the date, time speed, pause, view, switches, selection, zoom. Plain text.
std::string orbitStateLine(const AppState& app);

}  // namespace ui
