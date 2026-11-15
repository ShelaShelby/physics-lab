// 0.14 stage 2: the maths of the richer styles and the Light accent fix. (0.15 stage 5m: the tests of Frutiger Aero's and Y2K Night's looks and of the Michroma font were deleted on purpose with those themes, see REPORT.md.) Included by test_theme_style.inc (uses its helpers and test_app.cpp's
// checkTrue / checkStr). WRITTEN BEFORE the code (compile-red, then run-red, then green). References, none of them our own earlier output:
//   * the W3C's WCAG 2.x formula (see test_theme_style.inc) for every contrast;
//   * the owner's colours (Aero: aqua about #19b5d6 and lime about #6cc419; Y2K: icy blue #3a8dde to #bfe3ff, violet about #7b5cff) as hue / saturation windows, not as exact values;
//   * (0.28: the title-bar and texture maths and their hand-worked checks were removed with the unused theme code; 0.29: their fields too.)
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

// ---- the fields: defaults reproduce the old look, every theme is plain (0.29: the glass / gloss / title-bar / texture fields were removed) ----
static void testStage2Fields() {
    const ThemeStyle d;                                           // the defaults
    checkTrue("defaults: no accent override", !d.accent.set);
    checkTrue("defaults count as plain", isPlainStyle(d));
    for (const ThemeDef& t : kThemes) {
        const std::string id = t.id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        checkTrue(id + ": plain (the old look)", isPlainStyle(c.style));
        checkTrue(id + ": the button face is the card colour", c.button.r == c.card.r && c.button.g == c.card.g && c.button.b == c.card.b);
    }
}

// ---- Part B: Light's accent text was 4.24:1 on its page (found in 0.14 stage 1.1) ----
static void testLightAccent() {
    const Rgb page = hex6("#f7f7f5"), card = hex6("#ffffff"), oldAccent = hex6("#2f6fed");
    checkTrue("the old Light accent #2f6fed was 4.24:1 on its page (the bug)", nearTo(contrastRatio(oldAccent, page), 4.24, 5e-3) && contrastRatio(oldAccent, page) < 4.5);
    const ThemeColors c = themeColors("light", true);
    checkTrue("Light's accent now reads on its page: " + std::to_string(contrastRatio(c.accent, c.bg)) + " >= 4.5", contrastRatio(c.accent, c.bg) >= 4.5);
    checkTrue("and on its card: " + std::to_string(contrastRatio(c.accent, c.card)) + " >= 4.5", contrastRatio(c.accent, c.card) >= 4.5);
    checkTrue("the page and card colours are the theme's own", nearTo(c.bg.r, page.r, 1e-6) && nearTo(c.card.g, card.g, 1e-6));
    checkTrue("same hue (within 1 degree) as the old accent", std::fabs(hueOf(c.accent) - hueOf(oldAccent)) <= 1.0);
    checkTrue("same saturation (HSV, within 0.02): only darker", std::fabs(satOf(c.accent) - satOf(oldAccent)) <= 0.02 && valOf(c.accent) < valOf(oldAccent));
    checkTrue("darkened just enough, not more (under 4.9:1)", contrastRatio(c.accent, c.bg) < 4.9);
    checkTrue("it really is not the old colour", !(c.accent.r == oldAccent.r && c.accent.g == oldAccent.g && c.accent.b == oldAccent.b));
    checkTrue("the chosen button is the new accent too, and its white text reads (>= 4.5)", c.accentFill.r == c.accent.r && contrastRatio(c.onacc, c.accentFill) >= 4.5);
    checkTrue("the theme list is untouched: Light's swatch is still #2f6fed", std::string(findTheme("light")->accent) == "#2f6fed");
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
    testStage2Fields();
    testLightAccent();
}
