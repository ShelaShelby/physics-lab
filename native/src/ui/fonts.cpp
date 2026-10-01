#include "fonts.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

namespace ui {

static Fonts g_fonts;

const Fonts& fonts() { return g_fonts; }

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
    const std::string faces[3][2] = {{dir + "segoeui.ttf", dir + "arial.ttf"}, {dir + "segoeuib.ttf", dir + "arialbd.ttf"}, {dir + "segoeuii.ttf", dir + "ariali.ttf"}};
    const std::string extras[] = {dir + "seguisym.ttf"};
#else
    const std::string faces[3][2] = {{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"},
                                     {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
                                     {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf"}};
    const std::string extras[] = {"/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeSerif.ttf"};
#endif
    ImFont* got[3] = {nullptr, nullptr, nullptr};
    for (int f = 0; f < 3; f++)
        for (const std::string& path : faces[f]) {
            if (!fileExists(path)) continue;
            got[f] = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
            if (!got[f]) continue;
            if (f == 0) used = path;
            ImFontConfig cfg;                              // extra fonts fill in letters the face does not have (e.g. ☉)
            cfg.MergeMode = true;
            for (const std::string& e : extras)
                if (fileExists(e)) io.Fonts->AddFontFromFileTTF(e.c_str(), size, &cfg);
            break;
        }
    if (!got[0]) got[0] = io.Fonts->AddFontDefault();
    g_fonts.regular = got[0];
    g_fonts.bold = got[1] ? got[1] : got[0];
    g_fonts.italic = got[2] ? got[2] : got[0];
    io.FontDefault = got[0];
    return used.c_str();
}

std::string displayable(const std::string& text, ImFont* font) {
    if (!font) font = g_fonts.regular ? g_fonts.regular : ImGui::GetFont();
    std::string out;
    const char* p = text.c_str();
    const char* end = p + text.size();
    bool skippedSpace = false;
    while (p < end) {
        unsigned int c = 0;
        const int n = ImTextCharFromUtf8(&c, p, end);
        if (n <= 0) break;
        // U+FE0F only asks for colour-emoji style: never drawn on its own
        const bool drawable = c == 0xFE0F ? false : (c < 0x80 || font->IsGlyphInFont(static_cast<ImWchar>(c)));
        if (drawable) {
            if (!(skippedSpace && c == ' ')) out.append(p, n);   // no double space where a symbol was taken out
            skippedSpace = false;
        } else skippedSpace = out.empty() || out.back() == ' ';
        p += n;
    }
    return out;
}

}  // namespace ui
