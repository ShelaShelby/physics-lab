// The web app's tabs and their pages (📚 Learn, 🏆 Achievements), the 💡 "Did you know?" line with
// its 🎁 surprise, the secret Monochrome word, and the achievements with their pop-ups.
#pragma once

#include "orbit_panel.h"

namespace ui {

int factCount();                                     // 0.15 stage 3: the new UI's 💡 card: how many facts, which one is shown (0-based), its words, 
int factPosition(AppState& app);                     // a step to the next (+1) or the previous (-1), 
std::string factText(AppState& app);
void factStep(AppState& app, int dir);
std::string factTextAt(int index);                   // 0.15 stage 4: the words of fact number `index` (in the data's own order; "" for the gift), for search
void factShow(AppState& app, int index);             // make that one the fact on show
void paneWindow(AppState& app, float top);           // the Learn or Achievements page, below `top`, while one is chosen
void appTabsFrame(AppState& app);                    // every frame: achievement triggers and pop-ups, the secret pop-ups
void grantAchievement(AppState& app, const char* id);   // earns it (with its pop-up) if it is new
void secretMono();                                   // the guide's "monochrome" word was clicked

}  // namespace ui
