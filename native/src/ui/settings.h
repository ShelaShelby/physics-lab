// Settings (the web's ⚙ page: text size, font, units, motion, progress, developer), saving everything
// between runs (app/prefs.h), and Defaults, which puts a lab's settings back to how they started
// (after asking first).
#pragma once

#include <string>
#include <utility>

#include "app/prefs.h"

namespace ui {

struct AppState;

void loadSettings(AppState& app);                         // at start-up: read the file and set up both labs
void saveSettingsWhenChanged(AppState& app, double nowS); // every frame: writes the file a moment after something changed
void settingsWindow(AppState& app);                       // the Settings window, when open
void defaultsButton(AppState& app);                       // the "Defaults" button and its "are you sure?" question (for the lab on screen)
void setMotion(AppState& app, const std::string& k);       // auto / on (reduced) / off (full)
void themeButton(AppState& app);                          // 🎨 Theme and its menu

}  // namespace ui
