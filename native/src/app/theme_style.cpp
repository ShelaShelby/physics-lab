#include "app/theme_style.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace app {

// ---- contrast ----
static double linearChannel(float c) { return c <= 0.04045f ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }   // WCAG 2.x, relative luminance
double relativeLuminance(const Rgb& c) { return 0.2126 * linearChannel(c.r) + 0.7152 * linearChannel(c.g) + 0.0722 * linearChannel(c.b); }
double contrastRatio(const Rgb& a, const Rgb& b) {
    const double la = relativeLuminance(a), lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);                                                   // WCAG 2.x, contrast ratio
}

Rgb mixRgb(const Rgb& a, const Rgb& b, float f) { return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f}; }
Rgb over(const Rgb& top, float alpha, const Rgb& under) { return mixRgb(under, top, alpha); }

// ---- panels ----
// 0.28: every theme is plain (solid panels), so a floating window is its bg colour and a pop-up its card colour, whatever is behind it.
Rgb floatingSurface(const ThemeColors& c, const Rgb& /*backdrop*/, float /*t*/, bool popup) { return popup ? c.card : c.bg; }


// 0.29: the glass / gloss / texture fields are gone, so "plain" is the default border, tints and corner scale.
bool isPlainStyle(const ThemeStyle& s) {
    const PanelStyle& p = s.panel;
    return p.borderFgMix == 0.25f && p.borderWidth == 1 && s.widget.hoverTint == 0.22f && s.widget.pressTint == 0.45f && s.roundScale == 1;
}
float scaledRadius(const ThemeStyle& s, float r) { return s.square ? 0.f : r * s.roundScale; }

// ---- fonts ----
std::string resolveFontFile(const std::string& file, const std::vector<std::string>& dirs, const std::function<bool(const std::string&)>& exists) {
    if (file.empty()) return "";
    for (const std::string& d : dirs)
        if (exists(d + file)) return d + file;
    return "";
}
std::string fontFamilyKeyOf(const ThemeStyle& s) {
    const std::string& f = s.fontRegular;
    const std::size_t dot = f.rfind('.');
    return dot == std::string::npos ? f : f.substr(0, dot);
}
std::string fontKeyFor(const ThemeStyle& s, const std::string& userFont, bool themeFontFound) {
    if (userFont != "auto") return userFont;
    if (!s.font.empty()) return s.font;
    if (themeFontFound && !s.fontRegular.empty()) return fontFamilyKeyOf(s);
    return "auto";
}

// ---- the themes ----
// still holds Synthwave, so allThemes() skips the archived ids.
bool themeArchived(const std::string& id) { return id == "aero" || id == "y2knight" || id == "synthwave" || id == "y2kice"; }   // y2kice: removed in 0.15 stage 5g

const std::vector<const ThemeDef*>& allThemes() {
    static const std::vector<const ThemeDef*> list = [] {
        std::vector<const ThemeDef*> v;
        for (const ThemeDef& t : kThemes) if (!themeArchived(t.id)) v.push_back(&t);
        return v;
    }();
    return list;
}
const ThemeDef* findTheme(const std::string& id) {
    for (const ThemeDef* t : allThemes()) if (id == t->id) return t;
    return nullptr;
}

static Rgb rgb8(int r, int g, int b) { return {r / 255.f, g / 255.f, b / 255.f}; }

// The styles. A theme that is not here has the plain defaults (the look of 0.13). Data only: the drawing code never asks which theme it is.
static ThemeStyle makeStyle(const std::string& id) {
    ThemeStyle s;
    if (id == "paper") s.font = "serif";                                    // Paper's book font
    else if (id == "monochrome") { s.square = true; s.grayscale = true; s.font = "mono"; }
    else if (id == "light") {
        // Same hue (220 degrees) and saturation, a little darker: #2d6ae3 is 4.57:1 on the page and 4.9:1 on the card.
        s.accent = {true, rgb8(0x2d, 0x6a, 0xe3)};
    }
    return s;
}
const ThemeStyle& themeStyle(const std::string& id) {
    static std::map<std::string, ThemeStyle> cache;           // a map: the references stay valid
    auto it = cache.find(id);
    if (it == cache.end()) it = cache.emplace(id, makeStyle(id)).first;
    return it->second;
}

}  // namespace app
