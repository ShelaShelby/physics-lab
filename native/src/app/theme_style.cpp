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

static bool plainWidget(const WidgetStyle& w) {
    return w.hi == 0 && w.split == 0 && w.loAmount == 0 && w.shineLineAlpha == 0 && w.pressDark == 0 && w.rimDark == 0 && w.rimLight == 0 && w.hoverGlow == 0 && w.glint == 0 && w.brush == 0 && w.glowAlpha == 0;
}
bool isPlainWidgetStyle(const WidgetStyle& w) { return plainWidget(w); }
bool isPlainPanelStyle(const PanelStyle& p, const PageStyle& page) {
    return page.kind == 0 && p.fillAlpha == 1 && p.floatAlpha == 1 && p.shineAlpha == 0 && p.shineLineAlpha == 0 && p.shadowAlpha == 0 && pageFxTotal(page.fx) == 0 && page.fx.streak == 0 &&
           !p.title.set && p.rimAlpha == 0 && p.glowAlpha == 0 && p.reflectAlpha == 0 && p.bracketAlpha == 0 && p.hudAlpha == 0 && p.spec.empty();
}
bool isPlainStyle(const ThemeStyle& s) {
    const PanelStyle& p = s.panel;
    return !hasStage2Decor(s) && isPlainPanelStyle(p, s.page) && p.borderFgMix == 0.25f && p.borderWidth == 1 && plainWidget(s.widget) && s.widget.hoverTint == 0.22f &&
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

// ---- stage 2 ----
Rgb titleBarAt(const TitleStyle& t, float y) {
    const Rgb base = mixRgb(t.top, t.bottom, y);
    return t.shine > 0 && y <= t.shineHeight ? over({1, 1, 1}, t.shine, base) : base;
}
float pageFxTotal(const PageFx& fx) { return std::min(1.f, fx.clouds + fx.bubbles + fx.grain + fx.halftone + fx.grid); }
std::vector<Rgb> pageFxExtremes(const PageFx& fx, const Rgb& page) {
    const Rgb toned = mixRgb(page, fx.tone, pageFxTotal(fx));
    return {page, toned, mixRgb(page, fx.streakColour, fx.streak), mixRgb(toned, fx.streakColour, fx.streak)};
}
bool hasStage2Decor(const ThemeStyle& s) {
    const PanelStyle& p = s.panel;
    const WidgetStyle& w = s.widget;
    return pageFxTotal(s.page.fx) > 0 || s.page.fx.streak > 0 || p.title.set || p.rimAlpha > 0 || p.glowAlpha > 0 || p.reflectAlpha > 0 || p.bracketAlpha > 0 || p.hudAlpha > 0 || !p.spec.empty() ||
           w.rimDark > 0 || w.rimLight > 0 || w.hoverGlow > 0 || w.glint > 0 || w.brush > 0 || w.glowAlpha > 0 || s.button.set || !s.titleFont.empty();
}
std::string titleFontKeyFor(const ThemeStyle& s, bool available) { return available ? s.titleFont : std::string(); }

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
// 0.15 stage 5m: the native themes (Frutiger Aero, Y2K Night) and the web's Synthwave were taken out of the app; their code is in native/archive/themes/. The web's list (kThemes, generated from index.html)
// still holds Synthwave, so allThemes() skips the archived ids.
bool themeArchived(const std::string& id) { return id == "aero" || id == "y2knight" || id == "synthwave" || id == "y2kice"; }   // y2kice: removed in 0.15 stage 5g

const std::vector<const ThemeDef*>& allThemes() {
    static const std::vector<const ThemeDef*> list = [] {
        std::vector<const ThemeDef*> v;
        for (const ThemeDef& t : kThemes) if (!t.secret && !themeArchived(t.id)) v.push_back(&t);
        for (const ThemeDef& t : kThemes) if (t.secret && !themeArchived(t.id)) v.push_back(&t);
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
    else if (id == "light") {
        // 0.14 stage 2, on purpose (found in stage 1.1): the web's accent #2f6fed is 4.24:1 on Light's page #f7f7f5, under the 4.5:1 that body-size text needs.
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
