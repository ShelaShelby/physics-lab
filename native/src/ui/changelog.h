// The update badge and its changelog panel.
#pragma once

namespace ui {

// The version of the native app (whole number = major update, .5 = medium, other decimals = small).
const char* appVersion();

// Draws the changelog window while *open is true (the window's close button clears it).
void changelogWindow(bool* open);

}  // namespace ui
