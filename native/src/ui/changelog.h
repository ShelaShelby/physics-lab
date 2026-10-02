// The native app's version and history, and the web's changelog pop-up for each lab (the Update badge).
#pragma once

#include <string>

namespace ui {

// The version of the native app (whole number = major update, .5 = medium, other decimals = small).
const char* appVersion();
const char* appStage();          // "alpha", later "beta", then "" for the plain release
std::string appVersionLabel();   // "0.9.2 alpha" (just the number when the stage is empty)

// The native app's own updates, newest first (shown in 🕰 history after the web's).
struct NativeEntry { const char* version; const char* title; const char* text; };
const NativeEntry* nativeHistory(int& count);

// The lab's changelog (the web's, word for word) while *open is true; lab: 0 Orbit Lab, 1 Atom Lab.
void labLogWindow(bool* open, int lab);

}  // namespace ui
