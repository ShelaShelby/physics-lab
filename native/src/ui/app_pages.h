// The app-wide parts of the web page: the header with its buttons (🎨 Theme, 🌐 language, 👤 About,
// ⚙ Settings, 🕰 history and the lab's Update badge), the About page and the app's history.
#pragma once

#include <string>

#include "orbit_panel.h"

namespace ui {

bool pillButton(const std::string& label, const char* tip);   // a header button (.ver)
void aboutWindow(AppState& app);
void historyWindow(AppState& app);

}  // namespace ui
