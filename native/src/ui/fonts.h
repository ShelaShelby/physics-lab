// Fonts: a regular, a bold and an italic face with the letters the app needs (°, ×, ☉, ², –, ’, ...).
#pragma once

#include <string>

struct ImFont;

namespace ui {

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;      // same as regular if no bold face was found
    ImFont* italic = nullptr;    // same as regular if no italic face was found
};

// Uses the system's fonts: Segoe UI (+ Segoe UI Symbol) on Windows, DejaVu Sans on Linux.
// Falls back to Dear ImGui's built-in font (plain ASCII) if none is found.
// Returns the name of the font file used, or "built-in".
const char* loadFonts(float sizePixels);
const Fonts& fonts();

// The text without characters the font cannot draw (such as an emoji the system font lacks),
// so a label never shows an empty box. Text that can be drawn is unchanged.
std::string displayable(const std::string& text, ImFont* font = nullptr);

}  // namespace ui
