// 0.14 stage 1: the richer theme styles and Frutiger Aero. Included by test_app.cpp (uses its checkTrue / checkStr).
// Written BEFORE src/app/theme_style.* (compile-red first). References used, none of them our own earlier output:
//   * the W3C's WCAG 2.x contrast formula (https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio and #dfn-relative-luminance), worked by hand below;
//   * the W3C's own worked values (#767676 on white is the lightest grey that reaches 4.5:1; pure red on white is 4:1);
//   * linear interpolation worked by hand for the gradients.
#include "app/theme_style.h"

static bool nearTo(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
static Rgb rgb8(int r, int g, int b) { return {r / 255.f, g / 255.f, b / 255.f}; }

// ---- the contrast formula (W3C WCAG 2.x), worked by hand ----
static void testWcagFormula() {
    const Rgb white = rgb8(255, 255, 255), black = rgb8(0, 0, 0);
    // relative luminance L = 0.2126 R + 0.7152 G + 0.0722 B, with R = c/12.92 for c <= 0.04045, else ((c + 0.055)/1.055)^2.4
    checkTrue("L(white) = 1", nearTo(relativeLuminance(white), 1.0, 1e-9));
    checkTrue("L(black) = 0", nearTo(relativeLuminance(black), 0.0, 1e-12));
    checkTrue("L(red) = 0.2126", nearTo(relativeLuminance(rgb8(255, 0, 0)), 0.2126, 1e-9));
    checkTrue("L(green) = 0.7152", nearTo(relativeLuminance(rgb8(0, 255, 0)), 0.7152, 1e-9));
    checkTrue("L(blue) = 0.0722", nearTo(relativeLuminance(rgb8(0, 0, 255)), 0.0722, 1e-9));
    // #808080: c = 128/255 = 0.50196; ((0.50196 + 0.055)/1.055)^2.4 = 0.52796^2.4 = 0.21586 (the W3C's mid grey)
    checkTrue("L(#808080) = 0.2159", nearTo(relativeLuminance(rgb8(128, 128, 128)), 0.21586, 5e-5));
    // a dark value on the linear part: c = 10/255 = 0.0392 <= 0.04045 -> 0.0392/12.92 = 0.003035
    checkTrue("L(#0a0a0a) uses the linear part", nearTo(relativeLuminance(rgb8(10, 10, 10)), 10.0 / 255 / 12.92, 1e-9));
    // ratio = (L1 + 0.05) / (L2 + 0.05), L1 the lighter
    checkTrue("black on white = 21:1", nearTo(contrastRatio(black, white), 21.0, 1e-9));
    checkTrue("white on black = 21:1 (order does not matter)", nearTo(contrastRatio(white, black), 21.0, 1e-9));
    checkTrue("a colour on itself = 1:1", nearTo(contrastRatio(rgb8(30, 90, 140), rgb8(30, 90, 140)), 1.0, 1e-12));
    // #777 on white: L = 0.18447 -> 1.05 / 0.23447 = 4.478 (just under 4.5: the textbook example)
    checkTrue("#777 on white is about 4.48:1", nearTo(contrastRatio(rgb8(0x77, 0x77, 0x77), white), 4.478, 2e-3));
    checkTrue("#777 on white fails body text", contrastRatio(rgb8(0x77, 0x77, 0x77), white) < 4.5);
    // the W3C's lightest grey that passes 4.5:1 on white
    checkTrue("#767676 on white is 4.54:1", nearTo(contrastRatio(rgb8(0x76, 0x76, 0x76), white), 4.54, 5e-3));
    // pure red on white: 1.05 / (0.2126 + 0.05) = 3.998
    checkTrue("red on white is 4:1", nearTo(contrastRatio(rgb8(255, 0, 0), white), 3.998, 2e-3));
    // pure blue on white: 1.05 / 0.1222 = 8.592
    checkTrue("blue on white is 8.59:1", nearTo(contrastRatio(rgb8(0, 0, 255), white), 8.592, 2e-3));
    // black on pure green: (0.7152 + 0.05) / 0.05 = 15.304
    checkTrue("black on green is 15.3:1", nearTo(contrastRatio(black, rgb8(0, 255, 0)), 15.304, 2e-3));
    // alpha: 50 % black over white = (0.5, 0.5, 0.5); 25 % white over black = 0.25
    const Rgb half = over(black, 0.5f, white);
    checkTrue("50 % black over white", nearTo(half.r, 0.5, 1e-6) && nearTo(half.g, 0.5, 1e-6) && nearTo(half.b, 0.5, 1e-6));
    const Rgb q = over(white, 0.25f, black);
    checkTrue("25 % white over black", nearTo(q.r, 0.25, 1e-6));
    checkTrue("alpha 0 shows what is under, alpha 1 the top", nearTo(over(black, 0, white).r, 1, 1e-9) && nearTo(over(black, 1, white).r, 0, 1e-9));
}

// ---- gradients, worked by hand ----
static void testGradients() {
    PageStyle g;
    g.kind = 1; g.stops = 2;
    g.stop[0] = {rgb8(0, 0, 0), 0.f}; g.stop[1] = {rgb8(255, 255, 255), 1.f};
    checkTrue("two stops: the middle is half way", nearTo(pageGradientAt(g, 0.5f).r, 0.5, 1e-6));
    checkTrue("two stops: the ends", nearTo(pageGradientAt(g, 0).r, 0, 1e-9) && nearTo(pageGradientAt(g, 1).r, 1, 1e-9));
    checkTrue("outside 0..1 the ends are held", nearTo(pageGradientAt(g, -3).r, 0, 1e-9) && nearTo(pageGradientAt(g, 7).r, 1, 1e-9));
    g.stops = 3;                                                     // black at 0, red at 0.25, white at 1
    g.stop[0] = {rgb8(0, 0, 0), 0.f}; g.stop[1] = {rgb8(255, 0, 0), 0.25f}; g.stop[2] = {rgb8(255, 255, 255), 1.f};
    checkTrue("three stops: at the middle stop", nearTo(pageGradientAt(g, 0.25f).r, 1, 1e-6) && nearTo(pageGradientAt(g, 0.25f).g, 0, 1e-6));
    checkTrue("three stops: half way to the middle stop (t = 0.125)", nearTo(pageGradientAt(g, 0.125f).r, 0.5, 1e-6));
    // t = 0.625 is half way between the red (0.25) and the white (1): g = b = 0.5, r = 1
    checkTrue("three stops: half way from the middle stop to the end", nearTo(pageGradientAt(g, 0.625f).g, 0.5, 1e-6) && nearTo(pageGradientAt(g, 0.625f).r, 1, 1e-6));
    PageStyle flat;                                                  // a flat page is the theme's own bg colour
    const Rgb bg = rgb8(10, 20, 30);
    checkTrue("a flat page is the bg colour everywhere", nearTo(pageColourAt(flat, bg, 0.3f).g, bg.g, 1e-9) && nearTo(pageColourAt(flat, bg, 0.9f).b, bg.b, 1e-9));
}

// ---- the theme data: every theme has every field; the old themes keep their exact look ----
static void testThemeStyleData() {
    const std::vector<const ThemeDef*> all = allThemes();
    checkTrue("12 themes in the app: the web's 11 and Frutiger Aero", all.size() == 12);
    // the web's order is kept; Aero comes before the two secret themes, so the "???" cards stay at the end
    const char* order[] = {"auto", "light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "aero", "synthwave", "monochrome"};
    for (std::size_t i = 0; i < all.size() && i < 12; i++) checkStr("picker order " + std::to_string(i), all[i]->id, order[i]);
    checkTrue("the web's own list is untouched (still 11)", count(kThemes) == 11);
    std::set<std::string> ids;
    for (const ThemeDef* t : all) ids.insert(t->id);
    checkTrue("theme ids are unique", ids.size() == all.size());
    checkTrue("aero is a valid theme and can be used without unlocking", validTheme("aero") && themeUsable(Prefs{}, "aero"));
    checkStr("a saved aero theme is kept by the settings file", loadPrefs("theme=aero\n").theme, "aero");
    checkTrue("aero is not secret", !std::string(themeShownName(*all[9], {})).empty() && all[9]->secret == false && themeShownName(*all[9], {}) == "Frutiger Aero");

    for (const ThemeDef* t : all) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        const ThemeStyle& s = c.style;
        const std::string w = id + ": ";
        checkTrue(w + "page stops are 0..3 and in order", s.page.stops >= 0 && s.page.stops <= 3 && (s.page.kind == 0) == (s.page.stops == 0));
        bool ordered = true;
        for (int i = 0; i < s.page.stops; i++) ordered = ordered && s.page.stop[i].pos >= 0 && s.page.stop[i].pos <= 1 && (i == 0 || s.page.stop[i].pos > s.page.stop[i - 1].pos);
        checkTrue(w + "stop positions rise from 0 to 1", ordered);
        for (float a : {s.widget.hoverTint, s.widget.pressTint, s.panel.fillAlpha, s.panel.floatAlpha, s.panel.shineAlpha, s.panel.shineHeight, s.panel.shineLineAlpha, s.panel.shadowAlpha, s.panel.borderFgMix, s.widget.hi, s.widget.loAmount, s.widget.split, s.widget.shineLineAlpha, s.widget.pressDark})
            checkTrue(w + "every fraction is 0..1", a >= 0 && a <= 1);
        checkTrue(w + "alpha is more than 0 (a panel never vanishes)", s.panel.fillAlpha > 0.3f && s.panel.floatAlpha > 0.3f);
        checkTrue(w + "border width and rounding are positive", s.panel.borderWidth > 0 && s.roundScale > 0 && s.panel.shadowBlur >= 0);
        checkTrue(w + "the accent fill and the danger colours exist", c.accentFill.r >= 0 && c.danger.r >= 0 && c.dangerFill.r >= 0 && c.onDanger.r >= 0 && c.soft.r >= 0 && c.neutral.r >= 0);
    }

    // the old themes: the plain defaults reproduce the old 7-colour look exactly
    for (const ThemeDef& t : kThemes) {
        const std::string id = t.id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        const std::string w = id + " keeps its old look: ";
        checkTrue(w + "no gradient, solid panels, no shine, no shadow, no gloss", isPlainStyle(c.style));
        checkTrue(w + "panel alpha 1 (solid) and 1 for floating windows", c.style.panel.fillAlpha == 1.f && c.style.panel.floatAlpha == 1.f);
        checkTrue(w + "the accent fill is the accent", c.accentFill.r == c.accent.r && c.accentFill.g == c.accent.g && c.accentFill.b == c.accent.b);
        // the values that used to sit in the drawing code (orbit/atom panels, Settings, the Learn pages): same numbers
        checkTrue(w + "error red is (0.90, 0.35, 0.30)", c.danger.r == 0.90f && c.danger.g == 0.35f && c.danger.b == 0.30f);
        checkTrue(w + "the red button is #d0453b with white text", nearTo(c.dangerFill.r, 0xd0 / 255.0, 1e-6) && nearTo(c.dangerFill.g, 0x45 / 255.0, 1e-6) && nearTo(c.dangerFill.b, 0x3b / 255.0, 1e-6) && c.onDanger.r == 1.f && c.onDanger.g == 1.f);
        checkTrue(w + "the illustration tint is (120, 140, 200)", nearTo(c.soft.r * 255, 120, 1e-3) && nearTo(c.soft.g * 255, 140, 1e-3) && nearTo(c.soft.b * 255, 200, 1e-3));
        checkTrue(w + "the neutral grey is 128", nearTo(c.neutral.r * 255, 128, 1e-3) && nearTo(c.neutral.g * 255, 128, 1e-3));
        checkTrue(w + "corners are scaled by 1 (square only for Monochrome)", c.style.roundScale == 1.f && c.style.square == (id == "monochrome"));
    }
    // what used to be written as `id == "synthwave"` or `id == "monochrome"` is data now
    checkTrue("only Synthwave has the purple glow, #3b1466 behind the page and #ff4fd8 in the title", themeColors("synthwave", false).style.glowTop.set && themeColors("synthwave", false).style.titleGlow.set &&
              !themeColors("dark", false).style.glowTop.set && !themeColors("aero", false).style.glowTop.set && !themeColors("monochrome", false).style.glowTop.set);
    checkTrue("Synthwave's glow colours", nearTo(themeColors("synthwave", false).style.glowTop.c.r * 255, 0x3b, 1e-3) && nearTo(themeColors("synthwave", false).style.titleGlow.c.g * 255, 0x4f, 1e-3));
    checkTrue("Monochrome: square corners and grey pictures", themeColors("monochrome", false).style.square && themeColors("monochrome", false).style.grayscale && !themeColors("paper", false).style.grayscale);
    checkStr("Paper's own font is the serif one", themeColors("paper", false).style.font, "serif");
    checkStr("Monochrome's own font is the monospace one", themeColors("monochrome", false).style.font, "mono");
    checkStr("Dark has no font of its own", themeColors("dark", false).style.font, "");
}

// ---- gloss: the colours a widget gets ----
static void testGloss() {
    const Rgb card = rgb8(255, 255, 255), accent = rgb8(0x0d, 0x6a, 0x24);
    const ThemeColors aero = themeColors("aero", true), dark = themeColors("dark", false);
    for (WidgetState st : {WidgetState::Normal, WidgetState::Hover, WidgetState::Pressed}) {
        const Gloss p = glossColours(dark.style.widget, card, st);
        checkTrue("a plain widget is its flat colour, all four", p.top0.r == card.r && p.top1.g == card.g && p.bot0.b == card.b && p.bot1.r == card.r);
    }
    const Gloss n = glossColours(aero.style.widget, card, WidgetState::Normal);
    checkTrue("normal: the top is lighter than the bottom", relativeLuminance(n.top0) > relativeLuminance(n.bot1) && relativeLuminance(n.top0) >= relativeLuminance(n.top1));
    checkTrue("normal: the lower half is tinted (aqua: less red than blue)", n.bot1.r < n.bot1.b);
    const Gloss pr = glossColours(aero.style.widget, card, WidgetState::Pressed);
    checkTrue("pressed: the gloss turns over (the top is darker than the bottom)", relativeLuminance(pr.top0) < relativeLuminance(pr.bot1));
    checkTrue("pressed is darker than normal at the top", relativeLuminance(pr.top0) < relativeLuminance(n.top0));
    const Gloss hv = glossColours(aero.style.widget, card, WidgetState::Hover);
    checkTrue("hover is a different face from normal", hv.top0.g != n.top0.g || hv.bot1.g != n.bot1.g || hv.bot1.r != n.bot1.r);
    // the same rule on a coloured base: the top lighter than the base, the base lighter than the bottom tint
    const Gloss a = glossColours(aero.style.widget, accent, WidgetState::Normal);
    checkTrue("a coloured button: the top is lighter than the bottom", relativeLuminance(a.top0) > relativeLuminance(a.bot1));
    checkTrue("the gloss only ever lightens the top (never darker than the base)", relativeLuminance(a.top0) >= relativeLuminance(accent));
    checkTrue("plain themes keep the hover and press tints that were fixed in the code (0.22, 0.45)", dark.style.widget.hoverTint == 0.22f && dark.style.widget.pressTint == 0.45f);
    checkTrue("Aero's tints are softer (so the text stays readable on a pressed face)", aero.style.widget.hoverTint < 0.22f && aero.style.widget.pressTint < 0.45f);
    checkTrue("the split is in the middle of the button", aero.style.widget.split > 0.3f && aero.style.widget.split < 0.7f);
}

// ---- the panel colours ----
static void testPanelColours() {
    const ThemeColors aero = themeColors("aero", true), dark = themeColors("dark", false);
    const Rgb under = rgb8(0, 0, 0);
    // a solid panel hides what is behind it; a glass panel shows it in proportion to 1 - alpha
    const Rgb solid = panelSurface(dark, PanelKind::Docked, under);
    checkTrue("a plain panel is the theme's bg colour whatever is behind it", nearTo(solid.r, dark.bg.r, 1e-9) && nearTo(solid.b, dark.bg.b, 1e-9));
    const Rgb glass = panelSurface(aero, PanelKind::Docked, under);
    const float a = aero.style.panel.fillAlpha;
    checkTrue("a glass panel over black is the glass colour times its alpha", nearTo(glass.g, aero.style.panel.fill.g * a, 1e-6));
    checkTrue("glass is more solid in a floating window than docked", aero.style.panel.floatAlpha > aero.style.panel.fillAlpha);
    checkTrue("a floating window shows less of what is behind it", panelSurface(aero, PanelKind::Floating, under).g > glass.g);
}

// ---- Frutiger Aero: every text / background pair passes WCAG 2.x (4.5:1 body text, 3:1 large text and UI parts) ----
static void testAeroContrast() {
    const ThemeColors c = themeColors("aero", true);
    const ThemeStyle& s = c.style;
    const Rgb white = rgb8(255, 255, 255), black = rgb8(0, 0, 0);
    checkTrue("Aero is a light theme", c.light);
    checkTrue("the page is a vertical gradient", s.page.kind == 1 && s.page.stops >= 2);
    // the page's lightest and darkest points; everything below is also checked at 101 points down the page
    float darkest = 2, lightest = -1;
    for (int i = 0; i <= 100; i++) {
        const Rgb g = pageColourAt(s.page, c.bg, i / 100.f);
        darkest = std::min(darkest, float(relativeLuminance(g)));
        lightest = std::max(lightest, float(relativeLuminance(g)));
    }
    checkTrue("sky blue at the top is darker than the white at the bottom", darkest < 0.55f && lightest > 0.75f);

    struct Pair { const char* what; Rgb text; double need; };
    const Pair texts[] = {{"text", c.fg, 4.5}, {"muted text", c.muted, 4.5}, {"accent text", c.accent, 4.5}, {"error text", c.danger, 4.5}};
    double worstBody = 1e9;
    for (const Pair& p : texts) {
        double worst = 1e9;
        for (int i = 0; i <= 100; i++) {
            const float t = i / 100.f;
            const Rgb page = pageColourAt(s.page, c.bg, t);
            const Rgb docked = panelSurface(c, PanelKind::Docked, page);                  // glass over the page at this height
            const Rgb floating = panelSurface(c, PanelKind::Floating, page);
            worst = std::min({worst, contrastRatio(p.text, docked), contrastRatio(p.text, floating)});
            if (std::string(p.what) == "text") worst = std::min(worst, contrastRatio(p.text, page));   // plain text on the bare page (only the main text colour goes there)
        }
        // a floating window can also sit over a white or a black picture: the lightest and the darkest backdrop there is
        worst = std::min({worst, contrastRatio(p.text, panelSurface(c, PanelKind::Floating, white)), contrastRatio(p.text, panelSurface(c, PanelKind::Floating, black))});
        checkTrue(std::string("aero: ") + p.what + " on panels: " + std::to_string(worst) + " >= 4.5", worst >= p.need);
        worstBody = std::min(worstBody, worst);
    }
    // the faces of the buttons, in every state, top to bottom (the highlight line at the top is white, which only helps dark text)
    for (WidgetState st : {WidgetState::Normal, WidgetState::Hover, WidgetState::Pressed}) {
        const Rgb base = st == WidgetState::Normal ? c.card : st == WidgetState::Hover ? mixRgb(c.card, c.accent, s.widget.hoverTint) : mixRgb(c.card, c.accent, s.widget.pressTint);   // the colours Dear ImGui is given (ui/look.cpp)
        const Gloss g = glossColours(s.widget, base, st);
        double worst = 1e9;
        double worstMuted = 1e9;                                          // the small grey line under a name on a Settings tile (muted) sits on the same face
        for (const Rgb& f : {g.top0, g.top1, g.bot0, g.bot1}) { worst = std::min(worst, contrastRatio(c.fg, f)); worstMuted = std::min(worstMuted, contrastRatio(c.muted, f)); }
        checkTrue(std::string("aero: muted text on a button in state ") + std::to_string(int(st)) + ": " + std::to_string(worstMuted) + " >= 4.5", worstMuted >= 4.5);
        checkTrue(std::string("aero: button text in state ") + std::to_string(int(st)) + ": " + std::to_string(worst) + " >= 4.5", worst >= 4.5);
    }
    {   // a chosen button / tab: the bright green with its dark text
        double worst = 1e9;
        for (WidgetState st : {WidgetState::Normal, WidgetState::Hover, WidgetState::Pressed}) {
            const Gloss g = glossColours(s.widget, c.accentFill, st);
            for (const Rgb& f : {g.top0, g.top1, g.bot0, g.bot1}) worst = std::min(worst, contrastRatio(c.onacc, f));
        }
        checkTrue("aero: text on the green chosen button " + std::to_string(worst) + " >= 4.5", worst >= 4.5);
        double wd = 1e9;                       // the red (danger) button
        for (WidgetState st : {WidgetState::Normal, WidgetState::Hover, WidgetState::Pressed}) {
            const Gloss g = glossColours(s.widget, c.dangerFill, st);
            for (const Rgb& f : {g.top0, g.top1, g.bot0, g.bot1}) wd = std::min(wd, contrastRatio(c.onDanger, f));
        }
        checkTrue("aero: text on the red button " + std::to_string(wd) + " >= 4.5", wd >= 4.5);
    }
    {   // the highlight of a chosen list row, a hovered row and the title bar of a window (ui/look.cpp: Header, HeaderHovered, HeaderActive, TitleBgActive)
        double worst = 1e9;
        for (float f : {0.15f, 0.25f, 0.4f}) worst = std::min(worst, contrastRatio(c.fg, mixRgb(c.card, c.accentFill, f)));
        checkTrue("aero: text on a highlighted row or a title bar " + std::to_string(worst) + " >= 4.5", worst >= 4.5);
        checkTrue("aero: text on the box of the Settings done-line (the line colour) >= 4.5", contrastRatio(c.fg, c.line) >= 4.5);
    }
    // UI parts (3:1): the border of a panel or button against what it sits on, the check mark and the slider grab against the box
    {
        const Rgb border = mixRgb(c.line, c.fg, s.panel.borderFgMix);                      // the colour ui/look.cpp gives Dear ImGui's Border
        double worst = 1e9;
        for (int i = 0; i <= 100; i++) {
            const Rgb page = pageColourAt(s.page, c.bg, i / 100.f);
            worst = std::min({worst, contrastRatio(border, panelSurface(c, PanelKind::Docked, page)), contrastRatio(border, panelSurface(c, PanelKind::Floating, page))});
        }
        checkTrue("aero: the border against panels " + std::to_string(worst) + " >= 3", worst >= 3.0);
        const Rgb frame = mixRgb(c.card, c.fg, 0.16f);                                      // check box and slider track (ui/look.cpp FrameBg)
        checkTrue("aero: the check mark against its box >= 3", contrastRatio(c.accent, frame) >= 3.0);
        checkTrue("aero: the slider grab against its track >= 3", contrastRatio(mixRgb(c.accent, c.fg, 0.1f), frame) >= 3.0);
        checkTrue("aero: the text in a box against the box >= 4.5", contrastRatio(c.fg, frame) >= 4.5);
        checkTrue("aero: the accent as a drawn mark against the glass >= 3", contrastRatio(c.accent, panelSurface(c, PanelKind::Docked, pageColourAt(s.page, c.bg, 0))) >= 3.0);
    }
    checkTrue("aero: the worst body-text pair is at least 4.5", worstBody >= 4.5);
}

// ---- fonts: a system font by file name, with the fallback ----
static void testThemeFonts() {
    const ThemeColors aero = themeColors("aero", true);
    checkStr("Aero asks for Segoe UI by file name", aero.style.fontRegular, "segoeui.ttf");
    checkTrue("and its bold and italic faces", aero.style.fontBold == "segoeuib.ttf" && aero.style.fontItalic == "segoeuii.ttf");
    checkTrue("Segoe UI is not shipped (no font file is bundled for it)", aero.style.fontRegular.find('/') == std::string::npos && aero.style.fontRegular.find('\\') == std::string::npos);
    const std::vector<std::string> dirs = {"C:\\Windows\\Fonts\\", "/fonts/"};
    auto none = [](const std::string&) { return false; };
    auto onlyIn2 = [](const std::string& p) { return p == "/fonts/segoeui.ttf"; };
    auto in1 = [](const std::string& p) { return p == "C:\\Windows\\Fonts\\segoeui.ttf" || p == "/fonts/segoeui.ttf"; };
    checkStr("a missing font file gives no path (the fallback)", resolveFontFile("segoeui.ttf", dirs, none), "");
    checkStr("found in the second folder", resolveFontFile("segoeui.ttf", dirs, onlyIn2), "/fonts/segoeui.ttf");
    checkStr("the first folder wins", resolveFontFile("segoeui.ttf", dirs, in1), "C:\\Windows\\Fonts\\segoeui.ttf");
    checkStr("an empty name asks for no file", resolveFontFile("", dirs, in1), "");
    checkStr("no folders, no path", resolveFontFile("segoeui.ttf", {}, in1), "");
    // which family the app uses: Settings' choice wins; with Auto the theme's own font if it was found, else Auto (the bundled / default one)
    checkStr("Aero, Auto, font found: Segoe UI", fontKeyFor(aero.style, "auto", true), "segoeui");
    checkStr("Aero, Auto, font missing: the default font, no crash", fontKeyFor(aero.style, "auto", false), "auto");
    checkStr("Aero, the owner picked Serif: Serif", fontKeyFor(aero.style, "serif", true), "serif");
    checkStr("Paper, Auto: its book font", fontKeyFor(themeColors("paper", false).style, "auto", false), "serif");
    checkStr("Monochrome, Auto: monospace", fontKeyFor(themeColors("monochrome", false).style, "auto", true), "mono");
    checkStr("Dark, Auto: Auto", fontKeyFor(themeColors("dark", false).style, "auto", true), "auto");
    checkStr("Dark, Rounded: Rounded", fontKeyFor(themeColors("dark", false).style, "round", true), "round");
}

// ---- corners ----
static void testRoundScale() {
    checkTrue("a plain theme keeps every radius", scaledRadius(themeColors("dark", false).style, 12.f) == 12.f && scaledRadius(themeColors("paper", false).style, 6.f) == 6.f);
    checkTrue("Monochrome has square corners", scaledRadius(themeColors("monochrome", false).style, 12.f) == 0.f);
    checkTrue("Aero is rounder", scaledRadius(themeColors("aero", true).style, 12.f) > 12.f);
}


// ---- 0.14 stage 1.1: a floating window or pop-up that holds text hides what is behind it, whatever that is ----
// The owner saw the Learn cards through the guide window in Aero. The stage 1 rule only looked at glass over the page gradient; this one lets ANYTHING be behind.
static void testFloatingReadability() {
    for (const ThemeDef* t : allThemes()) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        const std::string w = id + ": ";
        const Rgb backs[] = {rgb8(0, 0, 0), rgb8(255, 255, 255), c.bg, c.fg, c.card, c.line, c.accent, c.onacc, c.muted, c.accentFill, c.danger, c.dangerFill};
        checkTrue(w + "the stack of a floating window is at least 94 % solid", floatingOpacity(c.style) >= 0.94f);
        checkTrue(w + "a pop-up list is fully solid", popupOpacity(c.style) == 1.f);
        double worst = 1e9, worstPop = 1e9, leak = 0;
        for (int i = 0; i <= 20; i++) {
            const float pos = i / 20.f;
            const Rgb ref = floatingSurface(c, backs[0], pos, false), refPop = floatingSurface(c, backs[0], pos, true);
            for (const Rgb& b : backs) {
                const Rgb sw = floatingSurface(c, b, pos, false), sp = floatingSurface(c, b, pos, true);
                leak = std::max({leak, std::fabs(double(sw.r - ref.r)), std::fabs(double(sw.g - ref.g)), std::fabs(double(sw.b - ref.b)),
                                 std::fabs(double(sp.r - refPop.r)), std::fabs(double(sp.g - refPop.g)), std::fabs(double(sp.b - refPop.b))});
                for (const Rgb& txt : {c.fg}) { worst = std::min(worst, contrastRatio(txt, sw)); worstPop = std::min(worstPop, contrastRatio(txt, sp)); }
                if (id == "aero") for (const Rgb& txt : {c.muted, c.accent, c.danger}) { worst = std::min(worst, contrastRatio(txt, sw)); worstPop = std::min(worstPop, contrastRatio(txt, sp)); }
            }
        }
        checkTrue(w + "nothing behind a window or pop-up shows through it (leak " + std::to_string(leak) + ")", leak < 1e-6);
        checkTrue(w + "text on a floating window over any backdrop " + std::to_string(worst) + " >= 4.5", worst >= 4.5);
        checkTrue(w + "text on a pop-up over any backdrop " + std::to_string(worstPop) + " >= 4.5", worstPop >= 4.5);
        // the glass layer alone (if a window were ever drawn without the page under it) must still read over every backdrop: the real alpha
        if (c.style.panel.floatAlpha < 1) {
            double alone = 1e9;
            for (const Rgb& b : backs) alone = std::min({alone, contrastRatio(c.fg, over(c.style.panel.fill, c.style.panel.floatAlpha, b)), contrastRatio(c.muted, over(c.style.panel.fill, c.style.panel.floatAlpha, b))});
            checkTrue(w + "the glass alone, over any backdrop: " + std::to_string(alone) + " >= 4.5", alone >= 4.5);
            checkTrue(w + "the glass alone is at least 94 % solid", c.style.panel.floatAlpha >= 0.94f);
        }
        // a docked panel is laid over the page and nothing else: dockedSurface is exactly panelSurface over the page colour at that height
        for (int i = 0; i <= 10; i++) {
            const Rgb a = dockedSurface(c, i / 10.f), b = panelSurface(c, PanelKind::Docked, pageColourAt(c.style.page, c.bg, i / 10.f));
            checkTrue(w + "docked surface at " + std::to_string(i) + "/10 is the glass over the page", nearTo(a.r, b.r, 1e-6) && nearTo(a.g, b.g, 1e-6) && nearTo(a.b, b.b, 1e-6));
        }
    }
}

static void testThemeStyle() {
    testWcagFormula();
    testGradients();
    testThemeStyleData();
    testGloss();
    testPanelColours();
    testAeroContrast();
    testThemeFonts();
    testRoundScale();
    testFloatingReadability();
}
