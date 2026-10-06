// Shared by ui/shell.cpp (the frame, the bars, the keys, the shift) and ui/shell_orbit.cpp (what the Orbit Lab's chip, toolbar and panels hold). Not for other files.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "app/shell.h"
#include "orbit/subject_card.h"
#include "ui/orbit_panel.h"
#include "ui/shell_widgets.h"

namespace ui::shellimpl {

using app::shell::PanelId;

// One thing on the toolbar. A Button acts at once; a Menu opens its panel above it (icon and caret are one button); a Segment is a two-way switch (2D / 3D) with its own caret.
struct ToolItem {
    enum Kind { Button, Sep, Menu, Segment } kind = Button;
    const char* id = "";
    std::string tip;                         // the name shown in the tooltip, with its key
    sh::Icon icon = sh::Icon::Dots;
    bool on = false;
    std::function<void()> act;               // Button: what it does
    PanelId panel = PanelId::None;           // Menu / Segment: the panel its caret opens
    std::vector<std::string> labels;         // Segment: its choices
    int sel = 0;                             // Segment: the chosen one
    std::function<void(int)> pick;           // Segment: a choice was made
};

inline ToolItem separator() { ToolItem t; t.kind = ToolItem::Sep; return t; }
std::vector<ToolItem> orbitTools(AppState& app);
std::vector<ToolItem> atomTools(AppState& app);

// What the Orbit Lab's panels hold (layout is by ui/shell.cpp). closeAfter is set by a panel that wants to close once something was chosen. False / 0: not an Orbit panel.
double orbitPanelWidthEm(PanelId id);
bool buildOrbitPanel(PanelId id, AppState& app, sh::Panel& p, bool& closeAfter);

// The chip, the crumbs and the cards (strings and what they do)
struct ChipInfo {
    std::string title, sub;
    bool today = false;       // a Today button at its right end
    bool opens = false;       // a click opens its panel
    bool play = false;        // a play / pause button at its left end (stage 4: moved here from the toolbar)
    bool paused = false;      // the lab is paused (the button shows Play)
    bool stop = false;        // a Journey runs: a "Stop" button replaces Today
};
ChipInfo chipInfo(AppState& app);
void pickFilterClear();                                      // the Pick a body list's filter text goes (the panel was closed)
void todayAction(AppState& app);                             // the Today button: the date goes back to now (also leaves the Time panel's "Set manually" view)
std::string subjectName(const AppState& app);                // the selected thing's name (Orbit) or the element (Atom); "" if none
// The subject card (Orbit): made from the card text the picture drew (orbit/subject_card.h). False when nothing is selected.
bool subjectCardOf(const AppState& app, orbit::SubjectCard& out);
// The card's action link, if the selected thing has one (id "card.slowmoons" / "card.slowspin"; text; what it does)
struct CardAction { const char* id = ""; std::string text; std::function<void()> act; };
bool cardAction(AppState& app, CardAction& out);
// The legend: what the picture really draws (colours from orbit/marker_colors.h and the bodies' own colours)
struct LegendEntry { enum Marker { Disc, Ring, Band, Line, Cross, Dots } marker = Disc; std::vector<std::string> hex; std::string text; };
std::vector<LegendEntry> legendEntries(const AppState& app);
// Search: everything the Orbit Lab can do, as (group, label, id, action). Ids are stable ("body:Earth", "set:moons", "date:1986-02-09") so recent choices can be kept.
using SearchAdd = std::function<void(const char* group, const std::string& label, const std::string& id, std::function<void()> act)>;
void orbitSearch(AppState& app, const SearchAdd& add);
void shellOpenPanel(PanelId id);                              // opens a dropdown or toolbar panel (search uses it); defined in ui/shell.cpp

// The Time panel's pieces that need state of their own
void timePanel(AppState& app, sh::Panel& p);

}  // namespace ui::shellimpl
