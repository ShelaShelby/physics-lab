// Settings: metric or imperial units on screen (kept between runs), and Defaults, which puts
// Orbit Lab's own settings back to how they started (after asking first).
#pragma once

namespace ui {

struct AppState;

void loadSettings(AppState& app);      // at start-up
void settingsWindow(AppState& app);    // the Settings window, when open
void defaultsButton(AppState& app);    // the "Defaults" button and its "are you sure?" question

}  // namespace ui
