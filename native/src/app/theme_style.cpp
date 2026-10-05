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

// ---- the page ----
Rgb pageGradientAt(const PageStyle& p, float t) {
    if (p.stops <= 0) return {0, 0, 0};
    if (t <= p.stop[0].pos) return p.stop[0].c;
    for (int i = 1; i < p.stops; i++)
        if (t <= p.stop[i].pos) return mixRgb(p.stop[i - 1].c, p.stop[i].c, (t - p.stop[i - 1].pos) / (p.stop[i].pos - p.stop[i - 1].pos));
    return p.stop[p.stops - 1].c;
}
Rgb pageColourAt(const PageStyle& p, const Rgb& flatBg, float t) { return p.kind == 0 || p.stops <= 0 ? flatBg : pageGradientAt(p, t); }

// ---- panels ----
Rgb panelSurface(const ThemeColors& c, PanelKind kind, const Rgb& under) {
    const PanelStyle& p = c.style.panel;
    const float a = kind == PanelKind::Docked ? p.fillAlpha : p.floatAlpha;
    if (a >= 1) return kind == PanelKind::Docked ? c.bg : c.card;
    return over(p.fill, a, under);
}

Rgb floatingSurface(const ThemeColors& c, const Rgb& /*backdrop*/, float t, bool popup) {
    const PanelStyle& p = c.style.panel;
    if (isPlainPanelStyle(p, c.style.page)) return popup ? c.card : c.bg;
    const Rgb page = pageColourAt(c.style.page, c.bg, popup ? 0.5f : t);        // the window paints the page itself first, so what is behind it never counts
    return over(p.fill, p.floatAlpha, page);
}
Rgb dockedSurface(const ThemeColors& c, float t) { return panelSurface(c, PanelKind::Docked, pageColourAt(c.style.page, c.bg, t)); }
float floatingOpacity(const ThemeStyle& s) { return s.page.kind != 0 || s.panel.floatAlpha >= 1 ? 1.f : s.panel.floatAlpha; }   // a page is painted opaque under the glass
float popupOpacity(const ThemeStyle&) { return 1.f; }

static bool plainWidget(const WidgetStyle& w) { return w.hi == 0 && w.split == 0 && w.loAmount == 0 && w.shineLineAlpha == 0 && w.pressDark == 0; }
bool isPlainWidgetStyle(const WidgetStyle& w) { return plainWidget(w); }
bool isPlainPanelStyle(const PanelStyle& p, const PageStyle& page) {
    return page.kind == 0 && p.fillAlpha == 1 && p.floatAlpha == 1 && p.shineAlpha == 0 && p.shineLineAlpha == 0 && p.shadowAlpha == 0;
}
bool isPlainStyle(const ThemeStyle& s) {
    const PanelStyle& p = s.panel;
    return isPlainPanelStyle(p, s.page) && p.borderFgMix == 0.25f && p.borderWidth == 1 && plainWidget(s.widget) && s.widget.hoverTint == 0.22f &&
           s.widget.pressTint == 0.45f && s.roundScale == 1;
}
float scaledRadius(const ThemeStyle& s, float r) { return s.square ? 0.f : r * s.roundScale; }

// ---- widgets ----
Gloss glossColours(const WidgetStyle& w, const Rgb& base, WidgetState state) {
    if (plainWidget(w)) return {base, base, base, base};
    const Rgb white{1, 1, 1}, black{0, 0, 0};
    const Rgb tint{base.r * w.lo.r, base.g * w.lo.g, base.b * w.lo.b};        // the colour of the lower half: the base, filtered by `lo`
    const float lo = state == WidgetState::Hover ? std::min(1.f, w.loAmount * 1.25f) : w.loAmount;
    if (state == WidgetState::Pressed) {                                      // the gloss turns over: dark on top, light below, like a pressed-in key
        const Rgb dark = mixRgb(mixRgb(base, tint, w.loAmount * 0.5f), black, w.pressDark);
        return {dark, mixRgb(base, tint, w.loAmount * 0.6f), mixRgb(base, white, w.hi * 0.45f), mixRgb(base, white, w.hi * 0.7f)};
    }
    return {mixRgb(base, white, w.hi), mixRgb(base, white, w.hi * 0.45f), mixRgb(base, tint, lo * 0.6f), mixRgb(base, tint, lo)};
}

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
// The native themes (not in the web's list, which app_text.inc is generated from). Same fields as the web's ThemeDef.
static const ThemeDef kNativeThemes[] = {
    {"aero", "Frutiger Aero", "Glossy glass, sky blue and fresh green", "#9ad6f6", "#0b5f20", false,
     "#9ad6f6", "#06283a", "#234457", "#ffffff", "#5aa6c8", "#0b5f20", "#04260d", true},
};

const std::vector<const ThemeDef*>& allThemes() {
    static const std::vector<const ThemeDef*> list = [] {
        std::vector<const ThemeDef*> v;
        for (const ThemeDef& t : kThemes) if (!t.secret) v.push_back(&t);
        for (const ThemeDef& t : kNativeThemes) if (!t.secret) v.push_back(&t);
        for (const ThemeDef& t : kThemes) if (t.secret) v.push_back(&t);
        for (const ThemeDef& t : kNativeThemes) if (t.secret) v.push_back(&t);
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
    if (id == "paper") s.font = "serif";                                    // the web's --ffd: Paper's book font
    else if (id == "monochrome") { s.square = true; s.grayscale = true; s.font = "mono"; }
    else if (id == "synthwave") {
        s.glowTop = {true, rgb8(0x3b, 0x14, 0x66)};                         // radial-gradient(130% 55% at 50% 0%, #3b1466 0%, var(--bg) 62%)
        s.titleGlow = {true, rgb8(0xff, 0x4f, 0xd8)};
    } else if (id == "aero") {
        // Frutiger Aero (the late-2000s Windows Vista / 7 look): a sky-blue-to-white page, frosted glass, glossy aqua and green buttons.
        // Every text / background pair was checked with the W3C's WCAG 2.x formula (tests/test_theme_style.inc, About → Where the facts come from).
        s.page.kind = 1; s.page.stops = 3;
        s.page.stop[0] = {rgb8(0x4f, 0xb4, 0xee), 0.00f};                   // sky blue (the darkest point of the page)
        s.page.stop[1] = {rgb8(0x86, 0xcd, 0xf4), 0.55f};
        s.page.stop[2] = {rgb8(0xd4, 0xef, 0xff), 1.00f};                   // almost white
        s.panel.fill = {1, 1, 1};
        s.panel.fillAlpha = 0.55f; s.panel.floatAlpha = 0.94f;              // glass: the page shows through a docked panel; a floating window is nearly solid
        s.panel.shineAlpha = 0.55f; s.panel.shineHeight = 0.16f; s.panel.shineLineAlpha = 0.9f;
        s.panel.borderFgMix = 0.3f; s.panel.borderWidth = 1; s.panel.floatRounding = 12;
        s.panel.shadowDx = 0; s.panel.shadowDy = 6; s.panel.shadowBlur = 18; s.panel.shadowAlpha = 0.28f; s.panel.shadow = rgb8(0x1d, 0x6f, 0x9c);   // a soft blue shadow
        s.widget.hi = 0.45f; s.widget.split = 0.5f; s.widget.loAmount = 0.9f; s.widget.lo = rgb8(0x9f, 0xdc, 0xf5);
        s.widget.shineLineAlpha = 0.85f; s.widget.pressDark = 0.05f; s.widget.hoverTint = 0.14f; s.widget.pressTint = 0.26f;
        s.roundScale = 1.5f;
        s.accentFill = {true, rgb8(0x58, 0xcf, 0x45)};                      // the bright green of a chosen button (its text is the dark onacc)
        s.danger = {true, rgb8(0xa3, 0x1d, 0x16)};
        s.dangerFill = {true, rgb8(0xf8, 0xc0, 0xba)}; s.onDanger = {true, rgb8(0x2b, 0x05, 0x02)};
        // Segoe UI is Microsoft's: it is used from the PC when it is there (Windows), never shipped with the app.
        s.fontRegular = "segoeui.ttf"; s.fontBold = "segoeuib.ttf"; s.fontItalic = "segoeuii.ttf";
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
