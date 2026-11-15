// 0.14 stage 1: the richer theme styles (0.15 stage 5m: the Frutiger Aero tests were deleted or moved to a test fixture, see REPORT.md). Included by test_app.cpp (uses its checkTrue / checkStr).
// Written BEFORE src/app/theme_style.* (compile-red first). References used, none of them our own earlier output:
//   * the W3C's WCAG 2.x contrast formula (https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio and #dfn-relative-luminance), worked by hand below;
//   * the W3C's own worked values (#767676 on white is the lightest grey that reaches 4.5:1; pure red on white is 4:1);
//   * linear interpolation worked by hand for the gradients.
#include "app/theme_style.h"
#include <set>

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


// 0.15 stage 5m: a made-up glossy glass style (NOT a theme; Frutiger Aero, which these tests used, was archived). Numbers are the test's own: it checks the maths of the style engine, not a look.
static ThemeColors fixtureGlass() {
    ThemeColors c = themeColors("light", true);
    ThemeStyle& s = c.style;
    s.widget.hoverTint = 0.05f; s.widget.pressTint = 0.10f;
    s.roundScale = 1.5f;
    s.fontRegular = "segoeui.ttf"; s.fontBold = "segoeuib.ttf"; s.fontItalic = "segoeuii.ttf";
    return c;
}

// ---- the theme data: every theme has every field; the old themes keep their exact look ----
static void testThemeStyleData() {
    const std::vector<const ThemeDef*> all = allThemes();
    checkTrue("10 themes in the app", all.size() == 10);
    const char* order[] = {"auto", "light", "dark", "black", "midnight", "solar", "paper", "sage", "contrast", "monochrome"};
    for (std::size_t i = 0; i < all.size() && i < 10; i++) checkStr("picker order " + std::to_string(i), all[i]->id, order[i]);
    std::set<std::string> ids;
    for (const ThemeDef* t : all) ids.insert(t->id);
    checkTrue("theme ids are unique", ids.size() == all.size());
    for (const char* gone : {"aero", "y2knight", "synthwave", "y2kice"}) {
        checkTrue(std::string(gone) + " is archived, not valid, not usable, not in the list", themeArchived(gone) && !validTheme(gone) && !themeUsable(Prefs{}, gone) && !ids.count(gone));
        checkStr(std::string("a saved ") + gone + " opens as Dark (once, no error)", loadPrefs(std::string("theme=") + gone + "\n").theme, "dark");
        Prefs q = loadPrefs(std::string("theme=") + gone + "\n");
        checkStr(std::string("and Dark is what is written back, so ") + gone + " is gone after one run", loadPrefs(savePrefs(q)).theme, "dark");
    }
    checkStr("an archived theme with an old unlock line (removed in 0.52.0) still opens as Dark", loadPrefs("unlock=synthwave\ntheme=synthwave\n").theme, "dark");
    checkTrue("the non-archived themes are not archived", !themeArchived("dark") && !themeArchived("midnight") && !themeArchived("monochrome") && !themeArchived("auto"));

    for (const ThemeDef* t : all) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        const ThemeStyle& s = c.style;
        const std::string w = id + ": ";
        for (float a : {s.widget.hoverTint, s.widget.pressTint, s.panel.borderFgMix})
            checkTrue(w + "every fraction is 0..1", a >= 0 && a <= 1);
        checkTrue(w + "border width and rounding are positive", s.panel.borderWidth > 0 && s.roundScale > 0);
        checkTrue(w + "the accent fill and the danger colours exist", c.accentFill.r >= 0 && c.danger.r >= 0 && c.dangerFill.r >= 0 && c.onDanger.r >= 0 && c.soft.r >= 0 && c.neutral.r >= 0);
    }

    // the old themes: the plain defaults reproduce the old 7-colour look exactly
    for (const ThemeDef& t : kThemes) {
        const std::string id = t.id;
        if (id == "auto") continue;
        const ThemeColors c = themeColors(id, false);
        const std::string w = id + " keeps its old look: ";
        checkTrue(w + "the default border, tints and corners", isPlainStyle(c.style));
        checkTrue(w + "the accent fill is the accent", c.accentFill.r == c.accent.r && c.accentFill.g == c.accent.g && c.accentFill.b == c.accent.b);
        // the values that used to sit in the drawing code (orbit/atom panels, Settings, the Learn pages): same numbers
        checkTrue(w + "error red is (0.90, 0.35, 0.30)", c.danger.r == 0.90f && c.danger.g == 0.35f && c.danger.b == 0.30f);
        checkTrue(w + "the red button is #d0453b with white text", nearTo(c.dangerFill.r, 0xd0 / 255.0, 1e-6) && nearTo(c.dangerFill.g, 0x45 / 255.0, 1e-6) && nearTo(c.dangerFill.b, 0x3b / 255.0, 1e-6) && c.onDanger.r == 1.f && c.onDanger.g == 1.f);
        checkTrue(w + "the illustration tint is (120, 140, 200)", nearTo(c.soft.r * 255, 120, 1e-3) && nearTo(c.soft.g * 255, 140, 1e-3) && nearTo(c.soft.b * 255, 200, 1e-3));
        checkTrue(w + "the neutral grey is 128", nearTo(c.neutral.r * 255, 128, 1e-3) && nearTo(c.neutral.g * 255, 128, 1e-3));
        checkTrue(w + "corners are scaled by 1 (square only for Monochrome)", c.style.roundScale == 1.f && c.style.square == (id == "monochrome"));
    }
    // what used to be written as `id == "monochrome"` is data now (Synthwave's purple glow was archived with the theme)
    checkTrue("Monochrome: square corners and grey pictures", themeColors("monochrome", false).style.square && themeColors("monochrome", false).style.grayscale && !themeColors("paper", false).style.grayscale);
    checkStr("Paper's own font is the serif one", themeColors("paper", false).style.font, "serif");
    checkStr("Monochrome's own font is the monospace one", themeColors("monochrome", false).style.font, "mono");
    checkStr("Dark has no font of its own", themeColors("dark", false).style.font, "");
}



// ---- fonts: a system font by file name, with the fallback ----
static void testThemeFonts() {
    const ThemeColors aero = fixtureGlass();
    checkStr("the fixture asks for Segoe UI by file name", aero.style.fontRegular, "segoeui.ttf");
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
    checkTrue("a style with roundScale 1.5 is rounder (the fixture)", scaledRadius(fixtureGlass().style, 12.f) > 12.f);
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
        double worst = 1e9, worstPop = 1e9, leak = 0;
        for (int i = 0; i <= 20; i++) {
            const float pos = i / 20.f;
            const Rgb ref = floatingSurface(c, backs[0], pos, false), refPop = floatingSurface(c, backs[0], pos, true);
            for (const Rgb& b : backs) {
                const Rgb sw = floatingSurface(c, b, pos, false), sp = floatingSurface(c, b, pos, true);
                leak = std::max({leak, std::fabs(double(sw.r - ref.r)), std::fabs(double(sw.g - ref.g)), std::fabs(double(sw.b - ref.b)),
                                 std::fabs(double(sp.r - refPop.r)), std::fabs(double(sp.g - refPop.g)), std::fabs(double(sp.b - refPop.b))});
                for (const Rgb& txt : {c.fg}) { worst = std::min(worst, contrastRatio(txt, sw)); worstPop = std::min(worstPop, contrastRatio(txt, sp)); }
                            }
        }
        checkTrue(w + "nothing behind a window or pop-up shows through it (leak " + std::to_string(leak) + ")", leak < 1e-6);
        checkTrue(w + "text on a floating window over any backdrop " + std::to_string(worst) + " >= 4.5", worst >= 4.5);
        checkTrue(w + "text on a pop-up over any backdrop " + std::to_string(worstPop) + " >= 4.5", worstPop >= 4.5);
    }
}

#include "test_theme_stage2.inc"   // 0.14 stage 2

static void testThemeStyle() {
    testWcagFormula();
    testThemeStyleData();
    testThemeFonts();
    testRoundScale();
    testFloatingReadability();
    testStage2();
}
