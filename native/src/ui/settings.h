// Settings (the web's ⚙ page: text size, font, units, motion, progress, developer), saving everything
// between runs (app/prefs.h), and Defaults, which puts a lab's settings back to how they started
// (after asking first).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "app/prefs.h"

namespace ui {

struct AppState;

void loadSettings(AppState& app);                         // at start-up: read the file and set up both labs
void saveSettingsWhenChanged(AppState& app, double nowS); // every frame: writes the file a moment after something changed
void settingsWindow(AppState& app);                       // the Settings window, when open
void setMotion(AppState& app, const std::string& k);       // auto / on (reduced) / off (full)
void themePicker(AppState& app, const ImVec2* hang = nullptr);                          // the menu itself: open it with ImGui::OpenPopup("##themes") from the button just before this call (the new UI's palette icon)
// 0.15 stage 5l: the Settings window's rows, in ONE list. The window takes each heading (and the Performance card each of its rows) from this list, and search builds its "Settings" results from the same
// list, so a row cannot be missing from search. `heading` is what the window draws; `label` is what search shows and matches; lab: -1 both labs, 0 only the Orbit Lab, 1 only the Atom Lab.
struct SettingRow { const char* id; const char* heading; const char* label; int lab; };
const std::vector<SettingRow>& settingRows();
const SettingRow& settingRow(const char* id);                 // (an id that is not in the list is a programming error: the first row comes back)
bool settingRowShown(const SettingRow& r, const AppState& app);   // is this row in the Settings window now (for this lab and this UI)?
void settingsGoto(AppState& app, const char* id);              // open the Settings window at that row: scrolls to it and highlights it for a moment
void settingMark(const char* id);                              // called by the window just before it draws a row (no-op outside the Settings window): remembers where it is, scrolls and highlights it
void defaultsPopups(AppState& app);                       // the "Put ... back?" questions: open with ImGui::OpenPopup("Put Orbit Lab back?") / ("Put Atom Lab back?") in the same window

}  // namespace ui
