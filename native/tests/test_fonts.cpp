// 0.15 stage 5j: glyph coverage of the fonts shipped for the looks M1..M3 (PL_LOOK). Written BEFORE the loader code.
// Reference: the font files themselves: their 'cmap' tables read with stb_truetype (the same reader Dear ImGui builds its atlas with). No app, no window.
//   1. Every non-ASCII character inside a string literal of src/ (the string tables and UI texts) is looked up in the new font (Inter, IBM Plex Sans) and in the fallback chain
//      the loader merges behind it (the loader's own list, ui/font_chain.h: Linux DejaVu Sans, Noto Sans Symbols, FreeSerif; Windows Segoe UI, Segoe UI Symbol, Segoe UI Emoji).
//   2. TEXT characters (below U+2100: Latin, Greek, punctuation, superscripts / subscripts) must be drawable by new font OR chain, else the test fails: that would be a box in the app.
//   3. SYMBOL / emoji characters (U+2100 and up) found in neither are only LISTED (they are missing in today's app too; on Windows Segoe UI Symbol / Emoji draw most of them).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
#include "ui/font_chain.h"

static int failures = 0;
static void checkTrue(const std::string& what, bool ok) {
    if (!ok) { std::printf("FAIL: %s\n", what.c_str()); failures++; }
}

struct FontFile {
    std::string path;
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
    bool has(unsigned cp) const { return ok && stbtt_FindGlyphIndex(&info, int(cp)) != 0; }
};

static FontFile load(const std::string& path) {
    FontFile f;
    f.path = path;
    std::ifstream in(path, std::ios::binary);
    if (!in) return f;
    f.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const int off = stbtt_GetFontOffsetForIndex(f.data.data(), 0);
    f.ok = off >= 0 && stbtt_InitFont(&f.info, f.data.data(), off) != 0;
    return f;
}

// the UTF-8 decoder: adds the non-ASCII code points of a string literal
static void decode(const std::string& s, std::set<unsigned>& out) {
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = c;
        int n = 1;
        if (c >= 0xF0) { cp = c & 7; n = 4; } else if (c >= 0xE0) { cp = c & 15; n = 3; } else if (c >= 0xC0) { cp = c & 31; n = 2; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 63);
        if (cp > 127) out.insert(cp);
        i += n;
    }
}

// the text between double quotes on the lines of a source file that are not comments (escapes skipped)
static void literals(const std::filesystem::path& p, std::set<unsigned>& out) {
    std::ifstream in(p);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 2, "//") == 0) continue;
        for (std::size_t i = 0; i < line.size(); i++) {
            if (line[i] != '"') continue;
            std::string lit;
            for (i++; i < line.size() && line[i] != '"'; i++) { if (line[i] == '\\' && i + 1 < line.size()) i++; lit += line[i]; }
            decode(lit, out);
        }
    }
}

int main() {
    const std::string assets = PL_ASSETS_DIR "/fonts/", src = PL_SRC_DIR;
    std::set<unsigned> used;
    for (const auto& e : std::filesystem::recursive_directory_iterator(src)) {
        const std::string ext = e.path().extension().string();
        if (e.is_regular_file() && (ext == ".cpp" || ext == ".h" || ext == ".inc")) literals(e.path(), used);
    }
    used.erase(0xFE0F);                                              // asks for colour style only: ui::displayable() never draws it
    for (unsigned c = 0x1F1E6; c <= 0x1F1FF; c++) used.erase(c);     // regional indicators (flags): dropped on purpose by ui::displayable()
    std::printf("%zu different non-ASCII characters in the string literals of src/\n", used.size());
    checkTrue("the scan found the characters the New UI uses (superscripts, Greek, arrows)", used.count(0x2079) && used.count(0x207A) && used.count(0x207B) && used.count(0x03B1) && used.count(0x2192) && used.count(0x00D7) && used.count(0x00B0));

    // the fallback chain the loader merges behind the look font: THE loader's own list (ui/font_chain.h), per platform (Windows: Segoe UI, Segoe UI Symbol, Segoe UI Emoji)
    std::vector<FontFile> chain;
    for (const std::string& p : ui::fallbackFontChain()) {
        FontFile f = load(p);
        if (f.ok) chain.push_back(std::move(f)); else std::printf("note: fallback %s not on this machine\n", p.c_str());
    }
    std::printf("%zu of the platform's fallback fonts found\n", chain.size());
    checkTrue("at least one font of this platform's fallback chain is available (so the test means something)", !chain.empty());
    const auto inChain = [&](unsigned cp) { for (const FontFile& f : chain) if (f.has(cp)) return true; return false; };

    const std::map<std::string, std::vector<std::string>> looks = {
        {"Inter (M1, M2)", {"Inter-Regular.otf", "Inter-SemiBold.otf"}},
        {"IBM Plex Sans (M3)", {"IBMPlexSans-Regular.ttf", "IBMPlexSans-SemiBold.ttf"}},
    };
    for (const auto& [name, files] : looks) {
        std::vector<FontFile> faces;
        for (const std::string& f : files) {
            faces.push_back(load(assets + f));
            checkTrue(name + ": " + f + " opens (cmap readable)", faces.back().ok);
        }
        for (const FontFile& f : faces) {
            if (!f.ok) continue;
            bool ascii = true;
            for (unsigned c = 0x20; c < 0x7F; c++) ascii = ascii && f.has(c);
            checkTrue(name + ": " + f.path.substr(f.path.rfind('/') + 1) + " has all of printable ASCII", ascii);
        }
        std::string missing;
        int gained = 0, nMissing = 0;
        for (const unsigned cp : used) {
            bool inNew = true;
            for (const FontFile& f : faces) inNew = inNew && f.has(cp);      // Regular AND SemiBold (both are drawn)
            const bool inOld = inChain(cp);
            if (inNew && !inOld) gained++;
            if (inNew || inOld) continue;
            char b[24];
            std::snprintf(b, sizeof b, "U+%04X ", cp);
            if (cp < 0x2100) checkTrue(name + ": text character " + b + "is drawable (new font or fallback)", false);
            else { missing += b; nMissing++; }
        }
        std::printf("%s: %d characters only the new font adds; %d symbol / emoji characters in NEITHER the new font NOR this platform's fallbacks:\n  %s\n", name.c_str(), gained, nMissing, missing.c_str());
    }
    std::printf("%s (%d failures)\n", failures ? "font coverage FAILED" : "font coverage passed", failures);
    return failures ? 1 : 0;
}
