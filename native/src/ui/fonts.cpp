#include "fonts.h"
#include "style_draw.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "app/look_options.h"
#include "app/theme_style.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ui {

// the fonts built into the .exe (cmake/embed_fonts.cmake makes this table from assets/fonts)
struct EmbeddedFont { const char* name; const unsigned char* data; std::size_t size; };
extern const EmbeddedFont kEmbeddedFonts[];

static std::map<std::string, Fonts> g_families;
static Fonts g_fonts;
static std::string g_family = "auto";

const Fonts& fonts() { return g_fonts; }

static bool fileExists(const std::string& path) {
    if (FILE* f = std::fopen(path.c_str(), "rb")) { std::fclose(f); return true; }
    return false;
}

static const EmbeddedFont* embedded(const std::string& name) {
    for (const EmbeddedFont* e = kEmbeddedFonts; e->name; e++) if (name == e->name) return e;
    return nullptr;
}

const char* loadFonts(float size) {
    ImGuiIO& io = ImGui::GetIO();
    static std::string used = "built-in";
    // Each family: regular, bold, italic. A face is a list of files tried in order (the first one found is used);
    // "@Name" is a font built into the .exe. Letters a face lacks come from the fallbacks (merged in after it).
#ifdef _WIN32
    std::string dir = "C:\\Windows\\Fonts\\";
    if (const char* w = std::getenv("WINDIR")) dir = std::string(w) + "\\Fonts\\";
    const std::vector<std::string> fallbacks = {dir + "segoeui.ttf", dir + "seguisym.ttf", dir + "seguiemj.ttf"};   // emoji as plain outlines
    const std::map<std::string, std::vector<std::vector<std::string>>> families = {
        {"auto", {{dir + "segoeui.ttf", dir + "arial.ttf"}, {dir + "segoeuib.ttf", dir + "arialbd.ttf"}, {dir + "segoeuii.ttf", dir + "ariali.ttf"}}},
        {"serif", {{dir + "georgia.ttf", dir + "times.ttf"}, {dir + "georgiab.ttf", dir + "timesbd.ttf"}, {dir + "georgiai.ttf", dir + "timesi.ttf"}}},
        {"mono", {{dir + "consola.ttf", dir + "cour.ttf"}, {dir + "consolab.ttf", dir + "courbd.ttf"}, {dir + "consolai.ttf", dir + "couri.ttf"}}},
#else
    const std::vector<std::string> fallbacks = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf",
                                                "/usr/share/fonts/truetype/freefont/FreeSerif.ttf"};
    const std::string dv = "/usr/share/fonts/truetype/dejavu/", lb = "/usr/share/fonts/truetype/liberation/";
    const std::map<std::string, std::vector<std::vector<std::string>>> families = {
        {"auto", {{dv + "DejaVuSans.ttf", lb + "LiberationSans-Regular.ttf"}, {dv + "DejaVuSans-Bold.ttf", lb + "LiberationSans-Bold.ttf"}, {dv + "DejaVuSans-Oblique.ttf", lb + "LiberationSans-Italic.ttf"}}},
        {"serif", {{dv + "DejaVuSerif.ttf", lb + "LiberationSerif-Regular.ttf"}, {dv + "DejaVuSerif-Bold.ttf", lb + "LiberationSerif-Bold.ttf"}, {dv + "DejaVuSerif-Italic.ttf", lb + "LiberationSerif-Italic.ttf"}}},
        {"mono", {{dv + "DejaVuSansMono.ttf", lb + "LiberationMono-Regular.ttf"}, {dv + "DejaVuSansMono-Bold.ttf", lb + "LiberationMono-Bold.ttf"}, {dv + "DejaVuSansMono-Oblique.ttf", lb + "LiberationMono-Italic.ttf"}}},
#endif
        {"easy", {{"@AtkinsonHyperlegible-Regular"}, {"@AtkinsonHyperlegible-Bold"}, {"@AtkinsonHyperlegible-Italic"}}},
        {"round", {{"@Nunito-Regular"}, {"@Nunito-Bold"}, {"@Nunito-Italic"}}},
    };
    auto addFace = [&](const std::vector<std::string>& files) -> ImFont* {
        ImFont* f = nullptr;
        for (const std::string& path : files) {
            if (path[0] == '@') {                                    // built in: the latin part, then latin-ext merged into it
                const EmbeddedFont* a = embedded(path.substr(1) + "-latin");
                const EmbeddedFont* b = embedded(path.substr(1) + "-latin-ext");
                if (!a) { a = embedded(path.substr(1)); b = nullptr; }    // stage 5j: a whole font in one file (Inter, IBM Plex Sans)
                if (!a) continue;
                ImFontConfig cfg;
                cfg.FontDataOwnedByAtlas = false;                    // the data lives in the .exe
                f = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(a->data), int(a->size), size, &cfg);
                if (f && b) { ImFontConfig m = cfg; m.MergeMode = true; io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(b->data), int(b->size), size, &m); }
            } else if (fileExists(path)) f = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
            if (!f) continue;
            ImFontConfig m;                                          // letters this face lacks (☉, ⁻, ☢ …) come from the fallbacks
            m.MergeMode = true;
            for (const std::string& e : fallbacks)
                if (e != path && fileExists(e)) io.Fonts->AddFontFromFileTTF(e.c_str(), size, &m);
            return f;
        }
        return nullptr;
    };
    // 0.14: a theme's own system font by file name (Frutiger Aero: Segoe UI). Looked for in the system's font folder (Windows) and in PL_FONT_DIR (for automatic
    // checks). Not found: no family is made, and the theme uses Auto (see useFontFamily). The font files are Microsoft's: used from the PC, never shipped.
    std::vector<std::string> sysDirs;
#ifdef _WIN32
    sysDirs.push_back(dir);
#endif
    if (const char* e = std::getenv("PL_FONT_DIR")) sysDirs.push_back(std::string(e) + "/");
    const auto exists = [](const std::string& p) { return fileExists(p); };
    std::map<std::string, std::vector<std::vector<std::string>>> themeFamilies;
    for (const app::ThemeDef* t : app::allThemes()) {
        const std::string themeId = t->id;
        const app::ThemeStyle& ts = app::themeStyle(themeId);
        const std::string regular = app::resolveFontFile(ts.fontRegular, sysDirs, exists);
        if (regular.empty()) continue;
        const std::string bold = app::resolveFontFile(ts.fontBold, sysDirs, exists), italic = app::resolveFontFile(ts.fontItalic, sysDirs, exists);
        themeFamilies[app::fontFamilyKeyOf(ts)] = {{regular}, {bold.empty() ? regular : bold}, {italic.empty() ? regular : italic}};
    }
    for (const auto& kv : themeFamilies) {
        Fonts fam;
        fam.regular = addFace(kv.second[0]);
        if (!fam.regular) continue;
        fam.bold = addFace(kv.second[1]);
        fam.italic = addFace(kv.second[2]);
        if (!fam.bold) fam.bold = fam.regular;
        if (!fam.italic) fam.italic = fam.regular;
        g_families[kv.first] = fam;
    }
    for (const auto& [key, faces] : families) {
        Fonts fam;
        fam.regular = addFace(faces[0]);
        if (!fam.regular) continue;
        fam.bold = addFace(faces[1]);
        fam.italic = addFace(faces[2]);
        if (!fam.bold) fam.bold = fam.regular;
        if (!fam.italic) fam.italic = fam.regular;
        g_families[key] = fam;
        if (key == "auto") used = faces[0][0];
    }
    {   // 0.14 stage 2: the display face of the Y2K themes' titles (Michroma, SIL OFL, built into the .exe): one face. If it is missing, no family is made and titles use the bold face.
        Fonts fam;
        fam.regular = addFace({"@Michroma-Regular"});
        if (fam.regular) { fam.bold = fam.italic = fam.regular; g_families["michroma"] = fam; }
    }
    {   // 0.15 stage 5j: the font of look M1..M3, loaded ONLY when PL_LOOK asks for it (today's fonts and atlas stay as they are otherwise). Regular + SemiBold (the "bold" face: titles); no italic file,
        // so italic text is drawn upright. Today's font is merged behind it (addFace) so a letter the look font lacks is still drawn, never a box.
        if (const app::lookopt::Tokens* lt = app::lookopt::tokens(app::lookopt::fromEnv(std::getenv("PL_LOOK")))) {
            const bool inter = std::strcmp(lt->font, "m_inter") == 0;
            Fonts fam;
            fam.regular = addFace({inter ? "@Inter-Regular" : "@IBMPlexSans-Regular"});
            if (fam.regular) {
                fam.bold = addFace({inter ? "@Inter-SemiBold" : "@IBMPlexSans-SemiBold"});
                if (!fam.bold) fam.bold = fam.regular;
                fam.italic = fam.regular;
                g_families[lt->font] = fam;
            }
        }
    }
    if (!g_families.count("auto")) {
        ImFont* f = io.Fonts->AddFontDefault();
        g_families["auto"] = {f, f, f};
    }
    g_fonts = g_families["auto"];
    io.FontDefault = g_fonts.regular;
    return used.c_str();
}

void logFontAtlas() {
    if (!std::getenv("PL_LOG_ATLAS")) return;
    ImFontAtlas* a = ImGui::GetIO().Fonts;
    ImTextureData* t = a->TexData;
    std::printf("PLATLAS tex=%dx%d used=%dx%d bytes=%d packedRects=%d packedPixels=%d bakedSizes=%d\n", t ? t->Width : 0, t ? t->Height : 0, t ? int(t->UsedRect.w) : 0, t ? int(t->UsedRect.h) : 0, t ? t->Width * t->Height * t->BytesPerPixel : 0,
                a->Builder ? a->Builder->RectsPackedCount : -1, a->Builder ? a->Builder->RectsPackedSurface : -1, a->Builder ? a->Builder->BakedPool.Size : -1);
}

bool fontFamilyAvailable(const std::string& key) { return g_families.count(key) > 0; }

void useFontFamily(const std::string& key) {
    const std::string k = g_families.count(key) ? key : "auto";
    if (k == g_family && g_fonts.regular) return;
    g_family = k;
    g_fonts = g_families[k];
    ImGui::GetIO().FontDefault = g_fonts.regular;
}

// PL_TITLEFONT=off (for the automatic picture checks): the titles use the bold face, to see what the title font changes
static bool titleFontSwitchedOff() { static const bool off = std::getenv("PL_TITLEFONT") && std::strcmp(std::getenv("PL_TITLEFONT"), "off") == 0; return off; }
float titleFontScale() { return titleFontSwitchedOff() || app::titleFontKeyFor(style(), fontFamilyAvailable(style().titleFont)).empty() ? 1.f : 0.85f; }
ImFont* titleFont() {
    const std::string key = titleFontSwitchedOff() ? std::string() : app::titleFontKeyFor(style(), fontFamilyAvailable(style().titleFont));
    return key.empty() ? g_fonts.bold : fontFamily(key).regular;
}

const Fonts& fontFamily(const std::string& key) { return g_families.count(key) ? g_families[key] : g_families["auto"]; }

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
        // U+1F1E6–1F1FF pair up into flags, which the fonts here cannot draw (they would show as boxed letters)
        const bool drawable = c == 0xFE0F || (c >= 0x1F1E6 && c <= 0x1F1FF) ? false : (c < 0x80 || font->IsGlyphInFont(static_cast<ImWchar>(c)));
        if (drawable) {
            if (!(skippedSpace && c == ' ')) out.append(p, n);   // no double space where a symbol was taken out
            skippedSpace = false;
        } else skippedSpace = out.empty() || out.back() == ' ';
        p += n;
    }
    return out;
}

std::string wrapLabel(const std::string& text, float width) {
    std::string out, line;
    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t e = text.find(' ', i);
        if (e == std::string::npos) e = text.size();
        const std::string word = text.substr(i, e - i);
        const std::string tryLine = line.empty() ? word : line + " " + word;
        if (!line.empty() && ImGui::CalcTextSize(tryLine.c_str()).x > width) { out += line + "\n"; line = word; }
        else line = tryLine;
        i = e + 1;
    }
    return out + line;
}

bool wideButton(const char* label, const char* id) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const std::string shown = wrapLabel(label, ImGui::GetContentRegionAvail().x - 2 * st.FramePadding.x - 2);
    return ui::Button((shown + "###" + (id ? id : label)).c_str(), ImVec2(-1, 0));
}

bool wrappedCheckbox(const char* label, bool* v) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const std::string shown = wrapLabel(label, ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - st.ItemInnerSpacing.x - 2);
    return ui::Checkbox((shown + "###" + label).c_str(), v);
}

}  // namespace ui
