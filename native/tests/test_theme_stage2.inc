// 0.14 stage 2: the maths of the richer styles and the Light accent fix. (0.15 stage 5m: the tests of Frutiger Aero's and Y2K Night's looks and of the Michroma font were deleted on purpose with those themes, see REPORT.md.) Included by test_theme_style.inc (uses its helpers and test_app.cpp's
// checkTrue / checkStr). WRITTEN BEFORE the code (compile-red, then run-red, then green). References, none of them our own earlier output:
//   * the W3C's WCAG 2.x formula (see test_theme_style.inc) for every contrast;
//   * the owner's colours (Aero: aqua about #19b5d6 and lime about #6cc419; Y2K: icy blue #3a8dde to #bfe3ff, violet about #7b5cff) as hue / saturation windows, not as exact values;
//   * the maths of the title bar and the texture extremes worked by hand below.
#include <fstream>

// ---- colour helpers (hue in degrees, saturation and value 0..1) ----
static void hsvOf(const Rgb& c, double& h, double& s, double& v) {
    const double mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b}), d = mx - mn;
    v = mx; s = mx > 0 ? d / mx : 0; h = 0;
    if (d > 0) {
        if (mx == c.r) h = 60 * std::fmod((c.g - c.b) / d, 6.0);
        else if (mx == c.g) h = 60 * ((c.b - c.r) / d + 2);
        else h = 60 * ((c.r - c.g) / d + 4);
        if (h < 0) h += 360;
    }
}
static double hueOf(const Rgb& c) { double h, s, v; hsvOf(c, h, s, v); return h; }
static double satOf(const Rgb& c) { double h, s, v; hsvOf(c, h, s, v); return s; }
static double valOf(const Rgb& c) { double h, s, v; hsvOf(c, h, s, v); return v; }
static Rgb hex6(const char* s) { return hexRgb(s); }

// ---- the title bar and the page textures, worked by hand ----
static void testStage2Maths() {
    TitleStyle t;
    t.set = true; t.top = rgb8(0, 0, 0); t.bottom = rgb8(255, 255, 255); t.shine = 0.5f; t.shineHeight = 0.4f;
    // no shine below 0.4 of the bar: the plain gradient (0.8 down = 0.8 grey)
    checkTrue("title bar: below the shine it is the plain gradient", nearTo(titleBarAt(t, 0.8f).g, 0.8, 1e-6));
    // inside the shine: 0.2 grey, 50 % white over it = 0.2 + (1 - 0.2) 0.5 = 0.6
    checkTrue("title bar: inside the shine band it is lighter (0.2 -> 0.6)", nearTo(titleBarAt(t, 0.2f).r, 0.6, 1e-6));
    checkTrue("title bar: the top edge is the top colour plus the shine (0 -> 0.5)", nearTo(titleBarAt(t, 0).b, 0.5, 1e-6));
    checkTrue("title bar: the bottom edge is the bottom colour", nearTo(titleBarAt(t, 1).r, 1.0, 1e-6));
    t.shine = 0;
    checkTrue("title bar: shine 0 is the plain gradient everywhere", nearTo(titleBarAt(t, 0.2f).r, 0.2, 1e-6));

    PageFx fx;
    fx.tone = rgb8(255, 255, 255); fx.clouds = 0.2f; fx.grid = 0.1f; fx.streak = 0.5f; fx.streakColour = rgb8(255, 0, 0);
    checkTrue("texture total is the sum", nearTo(pageFxTotal(fx), 0.3, 1e-6));
    const std::vector<Rgb> e = pageFxExtremes(fx, rgb8(0, 0, 0));
    checkTrue("four extremes: the page, towards tone, towards the streak, both", e.size() == 4);
    checkTrue("extreme 0 is the page", nearTo(e[0].r, 0, 1e-9) && nearTo(e[0].g, 0, 1e-9));
    checkTrue("extreme 1: black moved 0.3 towards white", nearTo(e[1].g, 0.3, 1e-6) && nearTo(e[1].r, 0.3, 1e-6));
    checkTrue("extreme 2: black moved 0.5 towards red", nearTo(e[2].r, 0.5, 1e-6) && nearTo(e[2].g, 0, 1e-6));
    checkTrue("extreme 3: both (tone first, then the streak): r = 0.3 + (1 - 0.3) 0.5 = 0.65, g = 0.3 (1 - 0.5) = 0.15", nearTo(e[3].r, 0.65, 1e-6) && nearTo(e[3].g, 0.15, 1e-6));
    fx.clouds = 0.9f; fx.grid = 0.9f;
    checkTrue("texture total is capped at 1", nearTo(pageFxTotal(fx), 1.0, 1e-9));
    checkTrue("no textures: the page itself four times", pageFxExtremes(PageFx{}, rgb8(10, 20, 30)).size() == 4 && nearTo(pageFxExtremes(PageFx{}, rgb8(10, 20, 30))[3].g, 20 / 255.0, 1e-6));
}

// ---- the new fields: defaults reproduce the old look, every theme has every field ----
static void testStage2Fields() {
    const ThemeStyle d;                                           // the defaults
    checkTrue("defaults: no textures", pageFxTotal(d.page.fx) == 0 && d.page.fx.streak == 0);
    checkTrue("defaults: no title bar style, rim, glow, reflection, brackets, HUD or label", !d.panel.title.set && d.panel.rimAlpha == 0 && d.panel.glowAlpha == 0 && d.panel.reflectAlpha == 0 &&
              d.panel.bracketAlpha == 0 && d.panel.hudAlpha == 0 && d.panel.spec.empty());
    checkTrue("defaults: widgets have no rim, glow, glint or brush", d.widget.rimDark == 0 && d.widget.rimLight == 0 && d.widget.hoverGlow == 0 && d.widget.glint == 0 && d.widget.brush == 0 && d.widget.glowAlpha == 0);
    checkTrue("defaults: no accent override, no button colour, no title font", !d.accent.set && !d.button.set && d.titleFont.empty());
    checkTrue("defaults count as plain", isPlainStyle(d) && !hasStage2Decor(d));
    for (const ThemeDef& t : kThemes) {
        const std::string id = t.id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        checkTrue(id + ": none of the stage 2 decoration (the old look)", !hasStage2Decor(c.style) && isPlainStyle(c.style));
        checkTrue(id + ": the button face is the card colour", c.button.r == c.card.r && c.button.g == c.card.g && c.button.b == c.card.b);
        checkTrue(id + ": no title font", titleFontKeyFor(c.style, true).empty());
    }
    { ThemeStyle d2; d2.page.fx.grain = 0.1f; checkTrue("a style with one decoration set counts as decorated (a fixture, not a theme)", hasStage2Decor(d2)); }
    for (const ThemeDef* t : allThemes()) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeStyle& s = themeColors(id, false).style;
        const PageFx& f = s.page.fx;
        const std::string w = id + ": ";
        for (float a : {f.clouds, f.bubbles, f.grain, f.halftone, f.grid, f.streak, s.panel.title.shine, s.panel.title.shineHeight, s.panel.title.glowAlpha, s.panel.rimAlpha, s.panel.glowAlpha,
                        s.panel.reflectAlpha, s.panel.reflectHeight, s.panel.bracketAlpha, s.panel.hudAlpha, s.widget.rimDark, s.widget.rimLight, s.widget.hoverGlow, s.widget.glint, s.widget.brush, s.widget.glowAlpha})
            checkTrue(w + "every stage 2 fraction is 0..1", a >= 0 && a <= 1);
        checkTrue(w + "steps, blur and sizes are not negative", f.gridStep > 0 && f.halftoneStep > 0 && s.panel.glowBlur >= 0 && s.panel.bracketSize >= 0);
        checkTrue(w + "a title bar that is set has a visible bar and text", !s.panel.title.set || (s.panel.title.shineHeight > 0 && contrastRatio(s.panel.title.text, s.panel.title.top) > 1.5));
        checkTrue(w + "the brackets, HUD rules and label come together or not at all", (s.panel.bracketAlpha > 0) == (s.panel.hudAlpha > 0) && s.panel.spec.empty() == (s.panel.hudAlpha == 0));
        checkTrue(w + "a glow of the chosen button that is on has a colour that shows (not black)", s.widget.glowAlpha == 0 || s.widget.glow.r + s.widget.glow.g + s.widget.glow.b > 0.3f);
    }
}

// ---- Part B: Light's accent text was 4.24:1 on its page (found in 0.14 stage 1.1) ----
static void testLightAccent() {
    const Rgb page = hex6("#f7f7f5"), card = hex6("#ffffff"), oldAccent = hex6("#2f6fed");   // the web's Light (kThemes)
    checkTrue("the old Light accent #2f6fed was 4.24:1 on its page (the bug)", nearTo(contrastRatio(oldAccent, page), 4.24, 5e-3) && contrastRatio(oldAccent, page) < 4.5);
    const ThemeColors c = themeColors("light", true);
    checkTrue("Light's accent now reads on its page: " + std::to_string(contrastRatio(c.accent, c.bg)) + " >= 4.5", contrastRatio(c.accent, c.bg) >= 4.5);
    checkTrue("and on its card: " + std::to_string(contrastRatio(c.accent, c.card)) + " >= 4.5", contrastRatio(c.accent, c.card) >= 4.5);
    checkTrue("the page and card colours are the web's own", nearTo(c.bg.r, page.r, 1e-6) && nearTo(c.card.g, card.g, 1e-6));
    checkTrue("same hue (within 1 degree) as the old accent", std::fabs(hueOf(c.accent) - hueOf(oldAccent)) <= 1.0);
    checkTrue("same saturation (HSV, within 0.02): only darker", std::fabs(satOf(c.accent) - satOf(oldAccent)) <= 0.02 && valOf(c.accent) < valOf(oldAccent));
    checkTrue("darkened just enough, not more (under 4.9:1)", contrastRatio(c.accent, c.bg) < 4.9);
    checkTrue("it really is not the old colour", !(c.accent.r == oldAccent.r && c.accent.g == oldAccent.g && c.accent.b == oldAccent.b));
    checkTrue("the chosen button is the new accent too, and its white text reads (>= 4.5)", c.accentFill.r == c.accent.r && contrastRatio(c.onacc, c.accentFill) >= 4.5);
    checkTrue("the theme list the web generated is untouched: Light's swatch is still #2f6fed", std::string(findTheme("light")->accent) == "#2f6fed");
    // every theme: text, muted text and accent text read on the flat page colour and on the card (WCAG 4.5:1); found by running this over the old themes
    for (const ThemeDef* t : allThemes()) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeColors k = themeColors(id, false);
        for (const Rgb* txt : {&k.fg, &k.muted, &k.accent})
            for (const Rgb* back : {&k.bg, &k.card})
                checkTrue(id + ": text colour on page and card " + std::to_string(contrastRatio(*txt, *back)) + " >= 4.5", contrastRatio(*txt, *back) >= 4.5);
    }
}

static void testStage2() {
    testStage2Maths();
    testStage2Fields();
    testLightAccent();
}
