// Loads a font with the letters the app needs (°, ×, ☉, ², –, ’, ...).
#pragma once

namespace ui {

// Uses the system's fonts: Segoe UI (+ Segoe UI Symbol) on Windows, DejaVu Sans on Linux.
// Falls back to Dear ImGui's built-in font (plain ASCII) if none is found.
// Returns the name of the font file used, or "built-in".
const char* loadFonts(float sizePixels);

}  // namespace ui
