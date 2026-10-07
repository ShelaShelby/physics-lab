// 0.15 stage 5j.1: the fonts the loader (ui/fonts.cpp) merges BEHIND every face, per platform. One list, used by the loader AND by tests/test_fonts.cpp, so the test checks what the app really uses.
// Header only, no Dear ImGui.
#pragma once

#include <cstdlib>
#include <string>
#include <vector>

namespace ui {

inline std::string windowsFontDir() {
    std::string dir = "C:\\Windows\\Fonts\\";
    if (const char* w = std::getenv("WINDIR")) dir = std::string(w) + "\\Fonts\\";
    return dir;
}

inline std::vector<std::string> fallbackFontChain() {
#ifdef _WIN32
    const std::string dir = windowsFontDir();
    return {dir + "segoeui.ttf", dir + "seguisym.ttf", dir + "seguiemj.ttf"};   // emoji as plain outlines
#else
    return {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeSerif.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"};
#endif
}

}  // namespace ui
