// Shared by ui/shell.cpp (the frame, the bars, the keys, the shift) and ui/shell_orbit.cpp (what the Orbit Lab's chip, toolbar and panels hold). Not for other files.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "app/shell.h"
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
struct ChipInfo { std::string title, sub; bool today = false; bool opens = false; };
ChipInfo chipInfo(AppState& app);
std::string subjectName(const AppState& app);                // the selected thing's name (Orbit) or the element (Atom); "" if none
std::string subjectKind(const AppState& app);                // its one-line kind
// The subject card's body (Orbit): its lines; false when nothing is selected
bool subjectLines(const AppState& app, std::string& title, std::vector<std::string>& lines);
void legendRows(const AppState& app, std::vector<std::pair<std::string, std::string>>& rows);   // (colour hex, "Planets  8")

// The Time panel's pieces that need state of their own
void timePanel(AppState& app, sh::Panel& p);

}  // namespace ui::shellimpl
