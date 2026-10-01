#include "fonts.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "imgui.h"

namespace ui {

static bool fileExists(const std::string& path) {
    if (FILE* f = std::fopen(path.c_str(), "rb")) { std::fclose(f); return true; }
    return false;
}

const char* loadFonts(float size) {
    ImGuiIO& io = ImGui::GetIO();
    static std::string used = "built-in";
#ifdef _WIN32
    std::string dir = "C:\\Windows\\Fonts\\";
    if (const char* w = std::getenv("WINDIR")) dir = std::string(w) + "\\Fonts\\";
    const std::string mains[] = {dir + "segoeui.ttf", dir + "arial.ttf"};
    const std::string extras[] = {dir + "seguisym.ttf"};
#else
    const std::string mains[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
                                 "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"};
    const std::string extras[] = {"/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeSerif.ttf"};
#endif
    for (const std::string& m : mains) {
        if (!fileExists(m) || !io.Fonts->AddFontFromFileTTF(m.c_str(), size)) continue;
        used = m;
        // extra fonts fill in letters the main one does not have (e.g. ☉)
        ImFontConfig cfg;
        cfg.MergeMode = true;
        for (const std::string& e : extras)
            if (fileExists(e)) io.Fonts->AddFontFromFileTTF(e.c_str(), size, &cfg);
        return used.c_str();
    }
    io.Fonts->AddFontDefault();
    return used.c_str();
}

}  // namespace ui
