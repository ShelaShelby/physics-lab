// 0.14 stage 2: Frutiger Aero's real look, the Light accent fix and the two Y2K themes. Included by test_theme_style.inc (uses its helpers and test_app.cpp's
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
    for (const char* id : {"aero", "y2knight"}) checkTrue(std::string(id) + " has stage 2 decoration", hasStage2Decor(themeColors(id, false).style));
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

// ---- the faces of a theme's buttons in every state, over their gloss, with what is drawn on top of them at its brightest ----
struct FaceCheck { double text = 1e9, muted = 1e9, onChosen = 1e9, onDanger = 1e9; };
static FaceCheck checkFaces(const ThemeColors& c) {
    const WidgetStyle& w = c.style.widget;
    FaceCheck r;
    const double wash = std::min(1.0, double(w.glint) + double(w.brush));          // light streaks and brush lines over the text's own face
    auto lighten = [&](const Rgb& f) { return mixRgb(f, rgb8(255, 255, 255), float(wash)); };
    for (WidgetState st : {WidgetState::Normal, WidgetState::Hover, WidgetState::Pressed}) {
        const Rgb base = st == WidgetState::Normal ? c.button : st == WidgetState::Hover ? mixRgb(c.button, c.accent, w.hoverTint) : mixRgb(c.button, c.accent, w.pressTint);   // as ui/look.cpp gives Dear ImGui
        const Gloss g = glossColours(w, base, st);
        for (const Rgb& f : {g.top0, g.top1, g.bot0, g.bot1, lighten(g.top0), lighten(g.top1), lighten(g.bot0), lighten(g.bot1)}) { r.text = std::min(r.text, contrastRatio(c.fg, f)); r.muted = std::min(r.muted, contrastRatio(c.muted, f)); }
        const Gloss a = glossColours(w, c.accentFill, st), dg = glossColours(w, c.dangerFill, st);
        for (const Rgb& f : {a.top0, a.top1, a.bot0, a.bot1, lighten(a.top0), lighten(a.bot1)}) r.onChosen = std::min(r.onChosen, contrastRatio(c.onacc, f));
        for (const Rgb& f : {dg.top0, dg.top1, dg.bot0, dg.bot1, lighten(dg.top0), lighten(dg.bot1)}) r.onDanger = std::min(r.onDanger, contrastRatio(c.onDanger, f));
    }
    return r;
}

// the same checks for both new looks: Aero (light, saturated) and Y2K (Night, dark; the light Y2K Ice was removed in stage 5g)
static void checkLook(const std::string& id, bool effectsOff = false) {
    ThemeColors c = themeColors(id, id != "y2knight");
    if (effectsOff) c.style = withoutEffects(c.style);                   // 0.14 stage 3: the same rules with the Theme effects switch Off
    const ThemeStyle& s = c.style;
    const std::string w = id + (effectsOff ? " (effects off): " : ": ");
    // text over the real page, at every height, with the textures at their strongest, under the glass and over the plain glass
    double wText = 1e9, wMuted = 1e9, wAccent = 1e9, wDanger = 1e9, wBare = 1e9, wHud = 1e9, wBorder = 1e9;
    for (int i = 0; i <= 100; i++) {
        const Rgb page = pageColourAt(s.page, c.bg, i / 100.f);
        for (const Rgb& e : pageFxExtremes(s.page.fx, page)) {
            const Rgb docked = panelSurface(c, PanelKind::Docked, e);
            wText = std::min(wText, contrastRatio(c.fg, docked)); wMuted = std::min(wMuted, contrastRatio(c.muted, docked));
            wAccent = std::min(wAccent, contrastRatio(c.accent, docked)); wDanger = std::min(wDanger, contrastRatio(c.danger, docked));
            wBare = std::min(wBare, contrastRatio(c.fg, e));
            if (!s.panel.spec.empty()) wHud = std::min(wHud, contrastRatio(s.panel.hud, docked));
            wBorder = std::min(wBorder, contrastRatio(mixRgb(c.line, c.fg, s.panel.borderFgMix), docked));
        }
        for (const Rgb& e : {page})
            for (PanelKind k : {PanelKind::Floating})
                if (!s.panel.spec.empty()) wHud = std::min(wHud, contrastRatio(s.panel.hud, panelSurface(c, k, e)));
    }
    checkTrue(w + "text on panels over the page and its textures at their strongest " + std::to_string(wText) + " >= 4.5", wText >= 4.5);
    checkTrue(w + "muted text on panels over the textures " + std::to_string(wMuted) + " >= 4.5", wMuted >= 4.5);
    checkTrue(w + "accent text on panels over the textures " + std::to_string(wAccent) + " >= 4.5", wAccent >= 4.5);
    checkTrue(w + "error text on panels over the textures " + std::to_string(wDanger) + " >= 4.5", wDanger >= 4.5);
    checkTrue(w + "text straight on the page over the textures " + std::to_string(wBare) + " >= 4.5", wBare >= 4.5);
    checkTrue(w + "the tiny spec label over the page and over windows " + std::to_string(wHud) + " >= 4.5", wHud >= 4.5);
    checkTrue(w + "the border against panels over the textures " + std::to_string(wBorder) + " >= 3", wBorder >= 3.0);
    // floating windows and pop-ups: muted, accent and error text too (the 1.1 rule only asked for the main colour in the old themes)
    double wf = 1e9;
    for (int i = 0; i <= 20; i++)
        for (bool pop : {false, true})
            for (const Rgb* t : {&c.fg, &c.muted, &c.accent, &c.danger}) wf = std::min(wf, contrastRatio(*t, floatingSurface(c, c.bg, i / 20.f, pop)));
    checkTrue(w + "all four text colours on windows and pop-ups " + std::to_string(wf) + " >= 4.5", wf >= 4.5);
    // buttons
    const FaceCheck f = checkFaces(c);
    checkTrue(w + "button text on every face, state and highlight " + std::to_string(f.text) + " >= 4.5", f.text >= 4.5);
    checkTrue(w + "muted text on every face " + std::to_string(f.muted) + " >= 4.5", f.muted >= 4.5);
    checkTrue(w + "text on the chosen (accent) face " + std::to_string(f.onChosen) + " >= 4.5", f.onChosen >= 4.5);
    checkTrue(w + "text on the red button " + std::to_string(f.onDanger) + " >= 4.5", f.onDanger >= 4.5);
    // title bars: the title over every point of the bar, shine included
    const TitleStyle& t = s.panel.title;
    checkTrue(w + "windows have a styled title bar", t.set);
    double wt = 1e9;
    for (int i = 0; i <= 40; i++) wt = std::min(wt, contrastRatio(t.text, titleBarAt(t, i / 40.f)));
    checkTrue(w + "title text over the whole bar " + std::to_string(wt) + " >= 4.5", wt >= 4.5);
    // UI parts (3:1)
    const Rgb frame = mixRgb(c.card, c.fg, 0.16f);
    checkTrue(w + "text in a box against the box >= 4.5", contrastRatio(c.fg, frame) >= 4.5);
    checkTrue(w + "the check mark against its box >= 3", contrastRatio(c.accent, frame) >= 3.0);
    checkTrue(w + "the slider grab against its track >= 3", contrastRatio(mixRgb(c.accent, c.fg, 0.1f), frame) >= 3.0);
    // the 1.1 rules, again for these themes: a window is at least 94 % solid
    checkTrue(w + "floating windows are at least 94 % solid", floatingOpacity(s) >= 0.94f && popupOpacity(s) == 1.f);
}

// ---- Aero: the real look (owner's references: aqua and lime, glossy jelly, Vista title bars, glass all the way down, clouds and bubbles) ----
static void testAeroLook() {
    const ThemeColors c = themeColors("aero", true);
    const ThemeStyle& s = c.style;
    checkLook("aero");
    // the two accents are saturated: aqua about #19b5d6 (hue 191, saturation 0.88), lime about #6cc419 (hue 91, saturation 0.87)
    bool aqua = false, lime = false;
    for (int i = 0; i <= 100; i++) {
        const Rgb p = pageColourAt(s.page, c.bg, i / 100.f);
        if (std::fabs(hueOf(p) - 191) <= 8 && satOf(p) >= 0.75 && valOf(p) >= 0.75) aqua = true;
        if (std::fabs(hueOf(p) - 91) <= 12 && satOf(p) >= 0.6 && valOf(p) >= 0.75) lime = true;
    }
    checkTrue("the sky is at the top (blue, hue 190 to 215) and the leaf green at the bottom (hue 80 to 110): the stops are in order", hueOf(pageColourAt(s.page, c.bg, 0)) >= 190 && hueOf(pageColourAt(s.page, c.bg, 0)) <= 215 && hueOf(pageColourAt(s.page, c.bg, 1)) >= 80 && hueOf(pageColourAt(s.page, c.bg, 1)) <= 110);
    checkTrue("the page passes through a real aqua (about #19b5d6)", aqua);
    checkTrue("and ends in a leaf green", lime);
    checkTrue("the button face is aqua: hue 185 to 200, saturation >= 0.7", std::fabs(hueOf(c.button) - 191) <= 9 && satOf(c.button) >= 0.7);
    checkTrue("the chosen button is lime: hue 85 to 100, saturation >= 0.8", std::fabs(hueOf(c.accentFill) - 91) <= 10 && satOf(c.accentFill) >= 0.8);
    // glass all the way down: the side panel is never pale blue low on the page (stage 1: saturation 0.12 at the bottom)
    double palest = 1;
    for (int i = 70; i <= 100; i++) palest = std::min(palest, satOf(dockedSurface(c, i / 100.f)));
    checkTrue("the docked glass keeps its colour to the bottom of the page: saturation " + std::to_string(palest) + " >= 0.25", palest >= 0.25);
    // jelly buttons: strong white highlight over the top ~45 %, deep saturated colour below, a thin darker rim, a glow on hover
    const WidgetStyle& w = s.widget;
    checkTrue("jelly: the split is at about 45 % of the button", w.split >= 0.40f && w.split <= 0.50f);
    checkTrue("jelly: a strong white highlight (at least 0.55 towards white)", w.hi >= 0.55f);
    const Gloss g = glossColours(w, c.button, WidgetState::Normal);
    checkTrue("jelly: the top is much lighter and much less saturated than the colour", relativeLuminance(g.top0) - relativeLuminance(c.button) >= 0.25 && satOf(g.top0) < satOf(c.button) - 0.3);
    checkTrue("jelly: the lower half is deep (not lighter than the colour) and saturated (>= 0.8)", valOf(g.bot1) <= valOf(c.button) + 1e-6 && satOf(g.bot1) >= 0.8);
    checkTrue("jelly: a thin darker rim and a glow on hover", w.rimDark >= 0.2f && w.hoverGlow > 0);
    const Gloss gl = glossColours(w, c.accentFill, WidgetState::Normal);
    checkTrue("the chosen tab is a jelly too (light top, deep lower half)", relativeLuminance(gl.top0) > relativeLuminance(gl.bot1) + 0.2 && satOf(gl.bot1) >= 0.8);
    // Vista-style title bars: glossy blue, a shine, an aqua line below
    const TitleStyle& t = s.panel.title;
    checkTrue("title bar: blue (hue 200 to 230), darker below than at the top of its gradient", hueOf(t.top) >= 200 && hueOf(t.top) <= 230 && relativeLuminance(t.top) > relativeLuminance(t.bottom) && t.text.r > 0.9f);
    checkTrue("title bar: a shine band, lighter than the bar below it", t.shine >= 0.12f && relativeLuminance(titleBarAt(t, 0.1f)) > relativeLuminance(titleBarAt(t, 0.9f)));
    checkTrue("title bar: an aqua line along the bottom edge", t.glowAlpha > 0.5f && std::fabs(hueOf(t.glow) - 191) <= 10);
    // a bright rim and a glow so the window's edge reads over the dark space picture; a faint reflection; clouds and bubbles, no grain
    checkTrue("a bright rim (at least 0.6) and an outer glow", s.panel.rimAlpha >= 0.6f && s.panel.glowAlpha > 0 && s.panel.glowBlur > 0 && relativeLuminance(s.panel.rim) > 0.8);
    checkTrue("a faint reflection under panels (0.05 to 0.3)", s.panel.reflectAlpha >= 0.05f && s.panel.reflectAlpha <= 0.3f && s.panel.reflectHeight > 0);
    checkTrue("a still page: clouds and bubbles, no grain, no grid", s.page.fx.clouds > 0 && s.page.fx.bubbles > 0 && s.page.fx.grain == 0 && s.page.fx.grid == 0 && s.page.fx.halftone == 0);
    checkTrue("the clouds and bubbles only lighten: the tone is white", s.page.fx.tone.r == 1 && s.page.fx.tone.g == 1 && s.page.fx.tone.b == 1);
    checkTrue("no brackets, HUD or spec label on Aero", s.panel.bracketAlpha == 0 && s.panel.spec.empty());
    // every button text pair at the jelly's brightest, and the old stage 1 pair rules still hold (testAeroContrast); the old page-lightness rule was a pale-design rule, replaced on purpose
    checkTrue("the saturated page: the darkest point is darker than 0.55 and the lightest is not white (under 0.75)", [&] {
        double lo = 2, hi = -1;
        for (int i = 0; i <= 100; i++) { const double L = relativeLuminance(pageColourAt(s.page, c.bg, i / 100.f)); lo = std::min(lo, L); hi = std::max(hi, L); }
        return lo < 0.55 && hi < 0.75 && hi > 0.3;
    }());
}

// ---- Y2K Night (Y2K Ice was removed in 0.15 stage 5g) ----
static void testY2kLook() {
    for (const char* id : {"y2knight"}) {
        const bool night = std::string(id) == "y2knight";
        const ThemeColors c = themeColors(id, !night);
        const ThemeStyle& s = c.style;
        const std::string w = std::string(id) + ": ";
        checkLook(id);
        checkTrue(w + "light flag: Ice is light, Night is dark", c.light == !night);
        checkTrue(w + "not a secret theme, usable at once", !findTheme(id)->secret && themeUsable(Prefs{}, id) && validTheme(id));
        // nearly monochrome: every colour is a cold blue (hue 195 to 265) or a grey; violet only as the streak and glow, never a fill or text
        std::vector<std::pair<const char*, Rgb>> cols = {{"bg", c.bg}, {"fg", c.fg}, {"muted", c.muted}, {"card", c.card}, {"line", c.line}, {"accent", c.accent}, {"onacc", c.onacc}, {"accentFill", c.accentFill},
            {"button", c.button}, {"tone", s.page.fx.tone}, {"fill", s.panel.fill}, {"title top", s.panel.title.top}, {"title bottom", s.panel.title.bottom}, {"title text", s.panel.title.text},
            {"title line", s.panel.title.glow}, {"rim", s.panel.rim}, {"glow", s.panel.glow}, {"hud", s.panel.hud}, {"widget glow", s.widget.glow}, {"shadow", s.panel.shadow}, {"lo", s.widget.lo}};
        for (int i = 0; i < s.page.stops; i++) cols.push_back({"page stop", s.page.stop[i].c});
        bool cold = true, violetOnlyStreak = true;
        for (const auto& [name, col] : cols) {
            if (satOf(col) < 0.12) continue;                                           // a grey: fine
            const double h = hueOf(col);
            if (h < 195 || h > 265) { cold = false; checkTrue(w + name + " is cold blue (hue " + std::to_string(h) + ")", false); }
            if (h > 250 && satOf(col) > 0.4) { violetOnlyStreak = false; checkTrue(w + name + " must not be violet", false); }
        }
        checkTrue(w + "no rainbow: every colour is a cold blue or a grey", cold);
        checkTrue(w + "violet is not used for anything but the streak", violetOnlyStreak);
        if (night) checkTrue(w + "the streak is violet (about #7b5cff: hue 245 to 265)", s.page.fx.streak > 0 && hueOf(s.page.fx.streakColour) >= 245 && hueOf(s.page.fx.streakColour) <= 265 && satOf(s.page.fx.streakColour) >= 0.4);
        // the page: light (Ice) or near black (Night), the whole way down
        double lo = 2, hi = -1;
        for (int i = 0; i <= 100; i++) { const double L = relativeLuminance(pageColourAt(s.page, c.bg, i / 100.f)); lo = std::min(lo, L); hi = std::max(hi, L); }
        checkTrue(w + "the page is " + (night ? "near black (L under 0.06)" : "icy (L over 0.3)"), night ? hi < 0.06 : lo > 0.3);
        // textures: grain, halftone dots and grid lines, each subtle (at most 12 %), all together at most 32 % (the numbers of PageFx are the most a pixel of the page moves)
        checkTrue(w + "grain, halftone and grid are all there", s.page.fx.grain > 0 && s.page.fx.halftone > 0 && s.page.fx.grid > 0);
        checkTrue(w + "each texture is subtle (<= 0.12) and all together <= 0.32", s.page.fx.grain <= 0.12f && s.page.fx.halftone <= 0.12f && s.page.fx.grid <= 0.12f && pageFxTotal(s.page.fx) <= 0.32f);
        checkTrue(w + "no clouds or bubbles: the print look, not rainbow bubbles", s.page.fx.clouds == 0 && s.page.fx.bubbles == 0);
        // tech decoration: corner brackets, HUD rules, a spec label
        checkTrue(w + "corner brackets 4 to 14 points, HUD rules, a spec label SYS", s.panel.bracketSize >= 4 && s.panel.bracketSize <= 14 && s.panel.bracketAlpha > 0 && s.panel.hudAlpha > 0 && s.panel.spec == "SYS");
        // buttons: brushed chrome with a cold highlight line, a thin icy rim, a glint on hover, a soft blue glow on the chosen tab
        const WidgetStyle& wd = s.widget;
        checkTrue(w + "buttons: highlight line, rim, glint, brushed lines, and a soft blue glow on the chosen one", wd.shineLineAlpha > 0 && (wd.rimDark > 0 || wd.rimLight > 0) && wd.glint > 0 && wd.brush > 0 && wd.glowAlpha > 0 && hueOf(wd.glow) >= 195 && hueOf(wd.glow) <= 235);
        { const double chroma = std::max({c.button.r, c.button.g, c.button.b}) - std::min({c.button.r, c.button.g, c.button.b});   // not HSV saturation: a dark colour is always "saturated" in HSV
          checkTrue(w + "buttons are a metal: a grey with a cold tint (chroma <= 0.2)", chroma <= 0.2); }
        // window title bar
        checkTrue(w + "title bar is " + (night ? "dark" : "silver") + " with a shine band", s.panel.title.set && s.panel.title.shine > 0 && (night ? relativeLuminance(s.panel.title.bottom) < 0.1 : relativeLuminance(s.panel.title.top) > 0.5));
        checkTrue(w + "windows are solid (1.1 rule), with a bright rim and glow", s.panel.floatAlpha >= 0.94f && s.panel.rimAlpha > 0 && s.panel.glowAlpha > 0);
        // the title font: bundled, for titles only; the body font is unchanged
        checkStr(w + "title font is Michroma (SIL OFL, bundled)", s.titleFont, "michroma");
        checkStr(w + "the body font stays Auto", fontKeyFor(s, "auto", true), "auto");
        checkStr(w + "and the owner's own choice in Settings still wins", fontKeyFor(s, "serif", true), "serif");
        checkStr(w + "the title font when it is built in", titleFontKeyFor(s, true), "michroma");
        checkStr(w + "missing: the titles use the normal font (no crash)", titleFontKeyFor(s, false), "");
        // old-theme look of the sheen: the widget gloss still exists
        checkTrue(w + "chrome: a strong cold highlight and a split", wd.split > 0.3f && wd.split < 0.7f);
    }
    checkTrue("Night has the violet streak (the light Ice, which had none, was removed)", themeColors("y2knight", false).style.page.fx.streak > 0);
    checkTrue("Night chrome: a dark gunmetal button (value <= 0.45)", valOf(themeColors("y2knight", false).button) <= 0.45);
    checkTrue("the other themes have no title font", titleFontKeyFor(themeColors("aero", true).style, true).empty());
}

// ---- the bundled font and its licence, and the About page (needs the source folder: -DPL_ASSETS_DIR) ----
static void testY2kFont() {
#ifdef PL_ASSETS_DIR
    const std::string dir = std::string(PL_ASSETS_DIR) + "/fonts/";
    std::ifstream ttf(dir + "Michroma-Regular-latin.ttf", std::ios::binary);
    std::string head(512, '\0');
    ttf.read(&head[0], 512);
    checkTrue("the bundled font exists and is a TrueType file", ttf.gcount() == 512 && (head.compare(0, 4, std::string("\x00\x01\x00\x00", 4)) == 0 || head.compare(0, 4, "true") == 0));
    checkTrue("it has a character map (cmap) table", head.find("cmap") != std::string::npos);
    std::ifstream lic(dir + "OFL-Michroma.txt");
    std::string text((std::istreambuf_iterator<char>(lic)), std::istreambuf_iterator<char>());
    checkTrue("its licence file is next to it: the SIL Open Font License 1.1 with the font's own copyright line", text.find("SIL OPEN FONT LICENSE Version 1.1") != std::string::npos && text.find("Michroma") != std::string::npos);
    std::ifstream rd(dir + "README.md");
    std::string readme((std::istreambuf_iterator<char>(rd)), std::istreambuf_iterator<char>());
    checkTrue("the fonts README names Michroma and its source", readme.find("Michroma") != std::string::npos && readme.find("OFL-Michroma.txt") != std::string::npos);
#else
    checkTrue("PL_ASSETS_DIR is defined for the tests", false);
#endif
}

static void testStage2() {
    testStage2Maths();
    testStage2Fields();
    testLightAccent();
    testAeroLook();
    testY2kLook();
    testY2kFont();
}
