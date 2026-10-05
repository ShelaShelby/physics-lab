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
// The native themes (not in the web's list, which app_text.inc is generated from). Same fields as the web's ThemeDef.
static const ThemeDef kNativeThemes[] = {
    // 0.14 stage 2: Aero's aqua and leaf green are saturated now (the swatches are the two accents); the colours here are the flat fallbacks and the text colours.
    {"aero", "Frutiger Aero", "Glossy glass, aqua and leaf green", "#19b5d6", "#6cc419", false,
     "#8fd3ee", "#031f2e", "#0a3348", "#ffffff", "#4c9fbd", "#05501f", "#04260d", true},
    // 0.14 stage 2: Y2K futurism, the owner's version: the cyber / techno print look. Nearly monochrome icy blue, cold white, black and brushed silver.
    {"y2kice", "Y2K Ice", "Cold white, icy blue and brushed silver", "#bfe3ff", "#3a8dde", false,
     "#e4f1ff", "#06142e", "#1b3358", "#f4f9ff", "#6a8cba", "#16479a", "#06142e", true},
    {"y2knight", "Y2K Night", "Night city, a violet light streak and film grain", "#0e1a3c", "#7b5cff", false,
     "#060a16", "#eaf4ff", "#d5e2f7", "#0c1428", "#5878b8", "#a6cdff", "#031026", false},
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
    } else if (id == "light") {
        // 0.14 stage 2, on purpose (found in stage 1.1): the web's accent #2f6fed is 4.24:1 on Light's page #f7f7f5, under the 4.5:1 that body-size text needs.
        // Same hue (220 degrees) and saturation, a little darker: #2d6ae3 is 4.57:1 on the page and 4.9:1 on the card.
        s.accent = {true, rgb8(0x2d, 0x6a, 0xe3)};
    } else if (id == "aero") {
        // Frutiger Aero as the owner sees it (0.14 stage 2): the Vista / Windows 7 look: glossy aqua and leaf-green jelly, a saturated sky-to-leaf page with soft clouds
        // and glass bubbles, blue glossy title bars, a bright rim and a soft glow so the windows read over the dark space picture. Style only, no artwork.
        // Every text / background pair is checked with the W3C's WCAG 2.x formula (tests/test_theme_stage2.inc), over the page and its clouds at their strongest.
        s.page.kind = 1; s.page.stops = 3;
        s.page.stop[0] = {rgb8(0x2a, 0x9e, 0xe0), 0.00f};                   // sky blue
        s.page.stop[1] = {rgb8(0x19, 0xb5, 0xd6), 0.50f};                   // aqua (the owner's #19b5d6)
        s.page.stop[2] = {rgb8(0x8f, 0xd6, 0x3f), 1.00f};                   // leaf green: the page stays saturated all the way down
        s.page.fx.tone = {1, 1, 1}; s.page.fx.clouds = 0.45f; s.page.fx.bubbles = 0.30f;   // soft clouds and glass bubbles: they only lighten the page
        s.panel.fill = rgb8(0xf4, 0xfd, 0xff);
        s.panel.fillAlpha = 0.36f; s.panel.floatAlpha = 0.94f;              // glass: the page shows through a docked panel; a floating window is nearly solid
        s.panel.shineAlpha = 0.55f; s.panel.shineHeight = 0.16f; s.panel.shineLineAlpha = 0.9f;
        s.panel.borderFgMix = 0.5f; s.panel.borderWidth = 1; s.panel.floatRounding = 12;   // the border is the line colour half way to the text colour: 3.75:1 on the glass
        s.panel.shadowDx = 0; s.panel.shadowDy = 6; s.panel.shadowBlur = 18; s.panel.shadowAlpha = 0.28f; s.panel.shadow = rgb8(0x1d, 0x6f, 0x9c);   // a soft blue shadow
        s.panel.rim = {1, 1, 1}; s.panel.rimAlpha = 0.85f;                  // a thin bright rim ...
        s.panel.glow = rgb8(0xbf, 0xf3, 0xff); s.panel.glowAlpha = 0.5f; s.panel.glowBlur = 14;   // ... and an outer glow, so the edge reads over the dark picture
        s.panel.reflectAlpha = 0.14f; s.panel.reflectHeight = 0.2f;        // a faint mirrored shine under windows and cards (the glossy floor)
        s.panel.title.set = true;                                           // a Vista-style window title bar: deep blue, a shine band, an aqua line below
        s.panel.title.top = rgb8(0x0a, 0x58, 0xa8); s.panel.title.bottom = rgb8(0x07, 0x40, 0x8a); s.panel.title.text = {1, 1, 1};
        s.panel.title.shine = 0.18f; s.panel.title.shineHeight = 0.5f;
        s.panel.title.glow = rgb8(0x19, 0xb5, 0xd6); s.panel.title.glowAlpha = 0.9f;
        // jelly: a strong white highlight over the top 46 %, deep saturated colour below, a thin darker rim, a glow on hover
        s.widget.hi = 0.62f; s.widget.split = 0.46f; s.widget.loAmount = 0.85f; s.widget.lo = {0.62f, 0.92f, 1.0f};
        s.widget.shineLineAlpha = 0.85f; s.widget.pressDark = 0.04f; s.widget.hoverTint = 0.05f; s.widget.pressTint = 0.10f;
        s.widget.rimDark = 0.45f; s.widget.hoverGlow = 0.55f;
        s.roundScale = 1.5f;
        s.button = {true, rgb8(0x25, 0xc4, 0xe6)};                          // aqua jelly
        s.accentFill = {true, rgb8(0x6c, 0xc4, 0x19)};                      // lime jelly for a chosen button / tab (its text is the dark onacc)
        s.danger = {true, rgb8(0x8e, 0x18, 0x12)};
        s.dangerFill = {true, rgb8(0xf8, 0xc0, 0xba)}; s.onDanger = {true, rgb8(0x2b, 0x05, 0x02)};
        // Segoe UI is Microsoft's: it is used from the PC when it is there (Windows), never shipped with the app.
        s.fontRegular = "segoeui.ttf"; s.fontBold = "segoeuib.ttf"; s.fontItalic = "segoeuii.ttf";
    } else if (id == "y2kice" || id == "y2knight") {
        // Y2K futurism, the owner's version (0.14 stage 2): the cyber / techno PRINT look (not rainbow bubbles). Nearly monochrome icy blue, cold white, black and
        // brushed silver; violet only as a small streak. Fine film grain, halftone dots and thin grid lines on the page (very subtle, under the glass, never behind
        // text), thin HUD rules, corner brackets and a tiny spec label as decoration, chrome buttons with a cold highlight line, silver or dark title bars in a wide
        // free font (Michroma, SIL OFL). Style only: no logos, characters, photos or artwork. Contrast is tested over the textures at their strongest.
        const bool night = id == "y2knight";
        s.page.kind = 1; s.page.stops = 3;
        s.panel.floatAlpha = 0.96f; s.panel.floatRounding = 4; s.roundScale = 0.5f;
        s.panel.borderFgMix = 0.4f; s.panel.borderWidth = 1;
        s.panel.shadowDy = 4; s.panel.shadowBlur = 14;
        s.panel.rim = night ? rgb8(0xbc, 0xd8, 0xff) : rgb8(0xff, 0xff, 0xff); s.panel.rimAlpha = night ? 0.8f : 0.9f;
        s.panel.glow = night ? rgb8(0x5a, 0x8c, 0xff) : rgb8(0x7a, 0xb6, 0xf2); s.panel.glowAlpha = night ? 0.45f : 0.35f; s.panel.glowBlur = night ? 12 : 10;
        s.panel.reflectAlpha = night ? 0.06f : 0.08f; s.panel.reflectHeight = night ? 0.12f : 0.15f;
        s.panel.bracketAlpha = 1.0f; s.panel.bracketSize = 12; s.panel.hudAlpha = 0.7f; s.panel.spec = "SYS";
        s.panel.title.set = true; s.panel.title.glowAlpha = 0.9f; s.panel.title.shineHeight = 0.45f;
        s.widget.split = 0.5f; s.widget.shineLineAlpha = night ? 0.6f : 0.9f;
        s.titleFont = "michroma";
        s.danger = {true, rgb8(0x8e, 0x18, 0x12)};
        if (!night) {
            s.page.stop[0] = {rgb8(0xf4, 0xfa, 0xff), 0.00f}; s.page.stop[1] = {rgb8(0xb9, 0xda, 0xfa), 0.50f}; s.page.stop[2] = {rgb8(0x7d, 0xb4, 0xec), 1.00f};   // cold white to icy blue
            s.page.fx.tone = rgb8(0x3a, 0x8d, 0xde); s.page.fx.grain = 0.08f; s.page.fx.halftone = 0.12f; s.page.fx.grid = 0.10f; s.page.fx.gridStep = 44; s.page.fx.halftoneStep = 6;
            s.panel.fill = rgb8(0xea, 0xf4, 0xff); s.panel.fillAlpha = 0.40f;
            s.panel.shineAlpha = 0.35f; s.panel.shineHeight = 0.10f; s.panel.shineLineAlpha = 0.8f;
            s.panel.shadowAlpha = 0.22f; s.panel.shadow = rgb8(0x1e, 0x4a, 0x85);
            s.panel.hud = rgb8(0x1b, 0x33, 0x58);
            s.panel.title.top = rgb8(0xee, 0xf3, 0xf9); s.panel.title.bottom = rgb8(0xb7, 0xc4, 0xd6); s.panel.title.text = rgb8(0x06, 0x14, 0x2e);   // brushed silver
            s.panel.title.shine = 0.35f; s.panel.title.glow = rgb8(0x3a, 0x8d, 0xde);
            s.widget.hi = 0.8f; s.widget.loAmount = 0.7f; s.widget.lo = {0.7f, 0.78f, 0.9f}; s.widget.pressDark = 0.05f; s.widget.hoverTint = 0.06f; s.widget.pressTint = 0.10f;
            s.widget.rimDark = 0.3f; s.widget.hoverGlow = 0.4f; s.widget.glint = 0.22f; s.widget.brush = 0.08f; s.widget.glow = rgb8(0x5a, 0xa8, 0xff); s.widget.glowAlpha = 0.6f;
            s.button = {true, rgb8(0xc3, 0xcf, 0xdc)};                      // silver
            s.accentFill = {true, rgb8(0x7a, 0xb6, 0xf2)};
            s.dangerFill = {true, rgb8(0xf3, 0xc2, 0xbd)}; s.onDanger = {true, rgb8(0x2b, 0x05, 0x02)};
        } else {
            s.page.stop[0] = {rgb8(0x04, 0x06, 0x0c), 0.00f}; s.page.stop[1] = {rgb8(0x08, 0x0e, 0x20), 0.50f}; s.page.stop[2] = {rgb8(0x16, 0x28, 0x6a), 1.00f};   // night
            s.page.fx.tone = rgb8(0x7a, 0xae, 0xff); s.page.fx.grain = 0.08f; s.page.fx.halftone = 0.10f; s.page.fx.grid = 0.10f; s.page.fx.gridStep = 44; s.page.fx.halftoneStep = 6;
            s.page.fx.streak = 0.45f; s.page.fx.streakColour = rgb8(0x7b, 0x5c, 0xff);   // the violet / blue light-streak swoosh
            s.panel.fill = rgb8(0x0a, 0x12, 0x26); s.panel.fillAlpha = 0.40f;
            s.panel.shineAlpha = 0.12f; s.panel.shineHeight = 0.08f; s.panel.shineLineAlpha = 0.5f;
            s.panel.shadowAlpha = 0.5f; s.panel.shadow = rgb8(0, 0, 0);
            s.panel.hud = rgb8(0x9d, 0xb4, 0xd8);
            s.panel.title.top = rgb8(0x2a, 0x35, 0x50); s.panel.title.bottom = rgb8(0x0f, 0x17, 0x30); s.panel.title.text = rgb8(0xea, 0xf4, 0xff);
            s.panel.title.shine = 0.10f; s.panel.title.glow = rgb8(0x6f, 0xb4, 0xff);
            s.widget.hi = 0.06f; s.widget.loAmount = 0.5f; s.widget.lo = {0.6f, 0.65f, 0.8f}; s.widget.pressDark = 0.10f; s.widget.hoverTint = 0.04f; s.widget.pressTint = 0.06f;
            s.widget.rimLight = 0.45f; s.widget.hoverGlow = 0.35f; s.widget.glint = 0.12f; s.widget.brush = 0.06f; s.widget.glow = rgb8(0x5a, 0x9b, 0xff); s.widget.glowAlpha = 0.55f;
            s.button = {true, rgb8(0x1c, 0x26, 0x3f)};                      // dark gunmetal chrome
            s.accentFill = {true, rgb8(0x6f, 0xb4, 0xff)};
            s.dangerFill = {true, rgb8(0x9c, 0x1f, 0x18)}; s.onDanger = {true, rgb8(0xff, 0xff, 0xff)};
            s.danger = {true, rgb8(0xff, 0x9a, 0x90)};
        }
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
