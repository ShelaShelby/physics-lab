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

// Loads the five font families of Settings → Font (the web's PL_FONTS):
//   auto  = Segoe UI (Windows) / DejaVu Sans (Linux)      serif = Georgia / DejaVu Serif
//   mono  = Consolas / DejaVu Sans Mono                     easy  = Atkinson Hyperlegible (built in)
//   round = Nunito (built in)
// with Segoe UI and Segoe UI Symbol filling in letters a face lacks. Falls back to Dear ImGui's
// built-in font (plain ASCII) if none is found. Returns the name of the main font file, or "built-in".
const char* loadFonts(float sizePixels);
const Fonts& fonts();                                   // the family in use
const Fonts& fontFamily(const std::string& key);        // any family (the Settings samples)
bool fontFamilyAvailable(const std::string& key);
void useFontFamily(const std::string& key);             // switch the whole app's text (call before NewFrame)

// The text without characters the font cannot draw (such as an emoji the system font lacks),
// so a label never shows an empty box. Text that can be drawn is unchanged.
std::string displayable(const std::string& text, ImFont* font = nullptr);

}  // namespace ui
