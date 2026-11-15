// The app-wide tabs and lines: the 💡 facts and the 📚 Learn tab (0.52.0: the achievements, their pop-ups, the 🎁 surprise and the secret word were removed).
#pragma once

#include "orbit_panel.h"

namespace ui {

int factCount();                                     // 0.15 stage 3: the new UI's 💡 card: how many facts, which one is shown (0-based), its words, 
int factPosition(AppState& app);                     // a step to the next (+1) or the previous (-1), 
std::string factText(AppState& app);
void factStep(AppState& app, int dir);
std::string factSourceLabel(AppState& app);          // 0.40: the source of the fact on show and the words that find it in About
std::string factSourceAbout(AppState& app);
std::string factTextAt(int index);                   // 0.15 stage 4: the words of fact number `index` (in the data's own order), for search
void factShow(AppState& app, int index);             // make that one the fact on show
int factLabCount(const AppState& app);               // 0.22: how many facts belong to the open lab, and which of them is on show (0-based)
int factLabPosition(AppState& app);
void paneWindow(AppState& app, float top);           // the Learn page, below `top`, while it is chosen
void appTabsFrame(AppState& app);                    // every frame: a guide opened from Learn goes back to its lab when it closes

}  // namespace ui
