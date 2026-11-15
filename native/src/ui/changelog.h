#pragma once

#include <string>

namespace ui {

// The version of the app (whole number = major update, .5 = medium, other decimals = small).
const char* appVersion();
std::string appVersionLabel();   // "0.9.2 alpha" (just the number when the stage is empty)

// The app's updates, newest first (shown in 🕰 history).
struct NativeEntry { const char* version; const char* title; const char* text; };
const NativeEntry* nativeHistory(int& count);


}  // namespace ui
