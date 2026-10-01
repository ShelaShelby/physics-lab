// The web app's tabs and their pages (📚 Learn, 🏆 Achievements), the 💡 "Did you know?" line with
// its 🎁 surprise, the secret Monochrome word, and the achievements with their pop-ups.
#pragma once

#include "orbit_panel.h"

namespace ui {

void factsLine(AppState& app);                       // the 💡 line (in the header)
void tabsBar(AppState& app);                         // 🔬 Simulations · 📚 Learn · 🏆 Achievements (in the header)
void paneWindow(AppState& app, float top);           // the Learn or Achievements page, below `top`, while one is chosen
void appTabsFrame(AppState& app);                    // every frame: achievement triggers and pop-ups, the secret pop-ups
void grantAchievement(AppState& app, const char* id);   // earns it (with its pop-up) if it is new
void secretMono();                                   // the guide's "monochrome" word was clicked

}  // namespace ui
