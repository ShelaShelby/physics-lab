// 0.15 stage 3: the logic of the new UI's shell (src/app/shell.*). WRITTEN BEFORE the code (compile-red first).
// References, none of them our own earlier output: the sizes and rules of docs/ui-0.15/DESIGN.md (the owner's chosen design: Caption 13 / Body 15 / Title 17 / Key 28 units,
// floors 12 / 14 / 15 / 22 px, Body 14 px at Text size Small, 19 and 23 px at 125 % and 150 %), the WCAG 2.x contrast rule (the formula is tested elsewhere against hand values),
// the opacity table computed by docs/ui-0.15/tools/glass_alpha.py (a separate Python program), and geometry that can be checked by reading it (nothing leaves the window, nothing
// overlaps). No long double, no hard-coded hashes.
#include "app/shell.h"
#include "app/theme_style.h"

namespace {
using namespace app::shell;
using app::Rgb;

const double kDpis[] = {1.0, 1.25, 1.5, 2.0};
const double kTfs[] = {0.9, 1.0, 1.15, 1.3};
const char* kTfNames[] = {"small", "normal", "large", "xl"};

std::string num(double v) { char b[48]; std::snprintf(b, sizeof b, "%.3f", v); return b; }
std::string tag(double dpi, double tf) { return " (scale " + num(dpi) + ", text " + num(tf) + ")"; }

struct Lcg {                                                 // a fixed pseudo-random sequence, so a failure can be repeated
    unsigned long long s = 88172645463325252ull;
    double next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return double((s >> 11) & 0xFFFFFFFFFFFFFull) / double(0x10000000000000ull); }
    double in(double lo, double hi) { return lo + (hi - lo) * next(); }
};

void testShellTypeScale() {
    // the numbers written in DESIGN.md section 1
    checkTrue("type: Caption is 13 u with a 12 px floor", typeDef(Type::Caption).u == 13 && typeDef(Type::Caption).floorPx == 12);
    checkTrue("type: Body is 15 u with a 14 px floor", typeDef(Type::Body).u == 15 && typeDef(Type::Body).floorPx == 14);
    checkTrue("type: Title is 17 u with a 15 px floor", typeDef(Type::Title).u == 17 && typeDef(Type::Title).floorPx == 15);
    checkTrue("type: Key number is 28 u with a 22 px floor", typeDef(Type::Key).u == 28 && typeDef(Type::Key).floorPx == 22);
    checkTrue("type: Body is 15 px at 100 %, Normal", textPx(Type::Body, 1.0, 1.0) == 15);
    checkTrue("type: Body is 14 px at 100 % and Text size Small (13.5 floored to 14): the owner's rule holds in every setting", textPx(Type::Body, 1.0, 0.9) == 14);
    checkTrue("type: Body is 19 px at 125 %, Normal (DESIGN.md: 19)", textPx(Type::Body, 1.25, 1.0) == 19);
    checkTrue("type: Body is 23 px at 150 %, Normal (DESIGN.md: 23)", textPx(Type::Body, 1.5, 1.0) == 23);
    checkTrue("type: Body is 30 px at 200 %, Normal", textPx(Type::Body, 2.0, 1.0) == 30);
    checkTrue("type: Caption at Small, 100 % is 12 px (11.7 rounds to 12)", textPx(Type::Caption, 1.0, 0.9) == 12);
    checkTrue("type: Title at Small, 100 % is 15 px", textPx(Type::Title, 1.0, 0.9) == 15);
    checkTrue("type: Key number at 100 % Normal is 28 px", textPx(Type::Key, 1.0, 1.0) == 28);
    for (double dpi : kDpis)
        for (std::size_t k = 0; k < 4; k++) {
            const double tf = kTfs[k];
            const std::string t = tag(dpi, tf) + " " + kTfNames[k];
            checkTrue("type floors: Caption >= 12 px" + t, textPx(Type::Caption, dpi, tf) >= 12);
            checkTrue("type floors: Body >= 14 px" + t, textPx(Type::Body, dpi, tf) >= 14);
            checkTrue("type floors: Title >= 15 px" + t, textPx(Type::Title, dpi, tf) >= 15);
            checkTrue("type floors: Key >= 22 px" + t, textPx(Type::Key, dpi, tf) >= 22);
            checkTrue("type order: Caption <= Body <= Title <= Key" + t, textPx(Type::Caption, dpi, tf) <= textPx(Type::Body, dpi, tf) && textPx(Type::Body, dpi, tf) <= textPx(Type::Title, dpi, tf) && textPx(Type::Title, dpi, tf) <= textPx(Type::Key, dpi, tf));
            for (Type ty : {Type::Caption, Type::Body, Type::Title, Type::Key}) {
                // what ImGui will draw: the base given to PushFont, times the Text size, times the Windows scale
                const double drawn = double(fontBase(ty, dpi, tf)) * tf * dpi;
                checkTrue(std::string("type: PushFont base reproduces the pixel size, ") + typeDef(ty).name + t, std::fabs(drawn - textPx(ty, dpi, tf)) < 1e-3);
            }
            const Metrics m = metrics(dpi, tf);
            checkTrue("metrics: body px equals the type scale" + t, m.body == textPx(Type::Body, dpi, tf) && m.caption == textPx(Type::Caption, dpi, tf) && m.title == textPx(Type::Title, dpi, tf) && m.key == textPx(Type::Key, dpi, tf));
            checkTrue("metrics: a button is tall enough for its Body text with 4 u above and below" + t, m.control >= m.body + 2 * m.s4);
            checkTrue("metrics: the toolbar button is at least as tall as a control" + t, m.toolBtn >= m.control && m.toolbarH >= m.toolBtn);
            checkTrue("metrics: the chip holds a Title line and a Caption line" + t, m.chipH >= m.title + m.caption + m.s4);
            checkTrue("metrics: the top bar holds the chip with a gap above and below" + t, m.topBar >= m.chipH + m.s4);
            checkTrue("metrics: 20 em wide is 20 body widths, so a panel grows with the text" + t, m.em(20) == 20 * m.body);
            checkTrue("metrics: heading floors 22 / 16 / 15" + t, m.h1 >= 22 && m.h2 >= 16 && m.h3 >= 15 && m.h1 >= m.h2 && m.h2 >= m.h3 && m.h3 >= m.body);
            checkTrue("metrics: spacing 4 8 12 16 24 grows with the scale" + t, m.s4 < m.s8 && m.s8 < m.s12 && m.s12 < m.s16 && m.s16 < m.s24);
        }
    // growing the scale or the text size never makes anything smaller
    for (Type ty : {Type::Caption, Type::Body, Type::Title, Type::Key}) {
        for (std::size_t k = 1; k < 4; k++) checkTrue("type: a bigger Text size is never smaller", textPx(ty, 1.0, kTfs[k]) >= textPx(ty, 1.0, kTfs[k - 1]));
        for (std::size_t k = 1; k < 4; k++) checkTrue("type: a bigger Windows scale is never smaller", textPx(ty, kDpis[k], 1.0) >= textPx(ty, kDpis[k - 1], 1.0));
    }
}

void testShellFrame() {
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1000.0, 1280.0, 1920.0, 2560.0})
                for (double hu : {560.0, 640.0, 800.0, 1080.0}) {
                    const Metrics m = metrics(dpi, tf);
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi);
                    const std::string t = tag(dpi, tf) + " window " + num(W) + "x" + num(H);
                    const Frame f = makeFrame(W, H, m, 7 * (m.toolBtn + m.s4) + 6 * m.s8, 14 * m.caption, 3 * m.caption * 1.4);
                    checkTrue("frame: the window is the whole window" + t, f.window.x == 0 && f.window.y == 0 && f.window.w == W && f.window.h == H);
                    checkTrue("frame: the top bar spans the window at its height" + t, f.topBar.x == 0 && f.topBar.y == 0 && f.topBar.w == W && f.topBar.h == m.topBar);
                    checkTrue("frame: toolbar inside the window" + t, within(f.toolbar, f.window));
                    checkTrue("frame: readout inside the window" + t, within(f.readout, f.window));
                    checkTrue("frame: toolbar is centred" + t, std::fabs((f.toolbar.x + f.toolbar.w / 2) - W / 2) < 1.0);
                    checkTrue("frame: toolbar at the bottom, 12 u from the edge" + t, std::fabs(f.window.bottom() - f.toolbar.bottom() - m.s12) < 1.0);
                    checkTrue("frame: toolbar and readout never overlap" + t, !f.toolbar.intersects(f.readout));
                    checkTrue("frame: the readout is in the right half" + t, f.readout.right() > W * 0.5);
                    checkTrue("frame: neither covers the top bar" + t, !f.toolbar.intersects(f.topBar) && !f.readout.intersects(f.topBar));
                }
}

void testShellToolbarOverflow() {
    const Metrics m = metrics(1.0, 1.0);
    const std::vector<double> w = {36, 44, 44, 44, 44, 70, 36};
    checkTrue("toolbar: everything fits in a wide window", toolbarVisible(w, 4, 36, 1280, m) == 7);
    int last = 7;
    for (double winW = 1280; winW >= 300; winW -= 20) {
        const int n = toolbarVisible(w, 4, 36, winW, m);
        double used = 0;
        for (int i = 0; i < n; i++) used += w[i] + (i ? 4 : 0);
        if (n < 7) used += 4 + 36;                                // the "..." button
        checkTrue("toolbar: what is shown (and the ... button) is never wider than the window " + num(winW), used <= winW - 2 * m.s12 + 1e-6 || n == 0);
        checkTrue("toolbar: a narrower window never shows more buttons " + num(winW), n <= last);
        last = n;
    }
    checkTrue("toolbar: a very narrow window shows at least the first button or only the ... button", toolbarVisible(w, 4, 36, 100, m) >= 0);
}

void testShellTopBar() {
    for (double dpi : {1.0, 1.5}) {
        const Metrics m = metrics(dpi, 1.0);
        TopBarWidths w;
        w.menu = 34 * dpi; w.crumbFull = 330 * dpi; w.crumbShort = 220 * dpi; w.crumbIcon = 60 * dpi; w.chip = 260 * dpi; w.right = 230 * dpi;
        int lastMode = 0;
        for (double W = 2400 * dpi; W >= 500 * dpi; W -= 40) {
            const TopBarPlan p = planTopBar(W, m, w);
            const std::string t = " width " + num(W) + tag(dpi, 1.0);
            const double crumb = p.crumbMode == 0 ? w.crumbFull : p.crumbMode == 1 ? w.crumbShort : w.crumbIcon;
            const double leftEnd = m.s12 + w.menu + m.s8 + crumb, rightStart = W - m.s12 - w.right;
            checkTrue("top bar: the mode only gets shorter as the window narrows" + t, p.crumbMode >= lastMode);
            lastMode = p.crumbMode;
            checkTrue("top bar: the chip is inside the window" + t, p.chip.x >= 0 && p.chip.right() <= W + 1e-6);
            checkTrue("top bar: the chip does not touch the menu or the crumbs" + t, !p.fits || p.chip.x >= leftEnd);
            checkTrue("top bar: the chip does not touch the right icons" + t, !p.fits || p.chip.right() <= rightStart + 1e-6);
            if (p.crumbMode == 0 && W >= 1400 * dpi && p.fits) checkTrue("top bar: with room the chip is exactly centred" + t, std::fabs(p.chip.x + p.chip.w / 2 - W / 2) < 1.0);
            checkTrue("top bar: a mode is only left for a shorter one when the longer one does not fit" + t,
                      p.crumbMode == 0 || !(m.s12 + w.menu + m.s8 + (p.crumbMode == 1 ? w.crumbFull : w.crumbShort) + m.s16 + w.chip + m.s16 + w.right + m.s12 <= W));
        }
    }
}

void testShellPanels() {
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1000.0, 1280.0, 1920.0})
                for (double hu : {560.0, 640.0, 800.0, 1080.0}) {
                    const Metrics m = metrics(dpi, tf);
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi);
                    const Frame f = makeFrame(W, H, m, 7 * (m.toolBtn + m.s4) + 6 * m.s8, 14 * m.caption, 3 * m.caption * 1.4);
                    for (Anchor a : {Anchor::TopLeft, Anchor::TopRight, Anchor::UnderChip, Anchor::AboveToolbar, Anchor::Centre})
                        for (double ww : std::vector<double>{double(m.em(12.0)), double(m.em(20.0)), double(m.em(24.0)), double(m.em(36.0)), W * 2.0})
                            for (double hh : std::vector<double>{double(m.control), double(m.em(10.0)), double(m.em(30.0)), H * 2.0}) {
                                const std::string t = tag(dpi, tf) + " window " + num(W) + "x" + num(H) + " anchor " + std::to_string(int(a)) + " panel " + num(ww) + "x" + num(hh);
                                const Rect r = placePanel(a, ww, hh, f, m, W * 0.37);
                                checkTrue("panel: never leaves the window" + t, within(r, f.window));
                                checkTrue("panel: never covers the top bar" + t, !r.intersects(f.topBar) || a == Anchor::Centre);
                                checkTrue("panel: never covers the toolbar" + t, !r.intersects(f.toolbar) || a == Anchor::Centre);
                                checkTrue("panel: has a positive size" + t, r.w > 0 && r.h > 0);
                                checkTrue("panel: is never bigger than asked" + t, r.w <= ww + 1e-6 && r.h <= hh + 1e-6);
                                if (a == Anchor::TopLeft) checkTrue("panel: top left sits at the left margin" + t, r.x <= m.s24 + 1e-6 || ww >= W - 2 * m.s12);
                                if (a == Anchor::TopRight) checkTrue("panel: top right is flush to the right margin" + t, std::fabs(W - m.s24 - r.right()) < 1.0 || ww >= W - 2 * m.s12);
                                if (a == Anchor::UnderChip || a == Anchor::AboveToolbar) {
                                    const double fit = W - 2 * m.s12;
                                    if (ww <= fit) checkTrue("panel: a dropdown is centred on its anchor, or against the edge when it must be" + t,
                                                             std::fabs(r.x + r.w / 2 - W * 0.37) < 1.0 || r.x <= m.s12 + 1e-6 || r.right() >= W - m.s12 - 1e-6);
                                }
                                if (a == Anchor::AboveToolbar) checkTrue("panel: above the toolbar, with a gap" + t, r.bottom() <= f.toolbar.y - m.s4 + 1e-6);
                            }
                }
    const Metrics m = metrics(1.0, 1.0);
    checkTrue("collapsed panel: wide enough for its title and caret", collapsedWidth(80, m) >= 80 + m.caretW);
    checkTrue("collapsed panel: grows with the title", collapsedWidth(120, m) > collapsedWidth(80, m));
}

void testShellCentreShift() {
    const Rect win{0, 0, 1000, 640};
    {   // largest free rectangle: one card in the top right corner, the free part is the left strip or the strip under the card; here the left one is larger
        const Rect card{650, 44, 330, 300};
        const Rect r = largestFreeRect(win, {card});
        checkTrue("free rect: nothing in the way is the whole window", largestFreeRect(win, {}).w == 1000 && largestFreeRect(win, {}).h == 640);
        checkTrue("free rect: it does not touch the card", !r.intersects(card));
        checkTrue("free rect: it is inside the window", within(r, win));
        checkTrue("free rect: the left strip (650 x 640) is the biggest", r.area() >= 650.0 * 640.0 - 1);
    }
    {   // fuzz: a body under random panels
        Lcg g;
        int shifted = 0;
        for (int i = 0; i < 4000; i++) {
            std::vector<Rect> occ;
            const int n = 1 + int(g.in(0, 4));
            for (int k = 0; k < n; k++) { const double w = g.in(60, 500), h = g.in(40, 400); occ.push_back({g.in(0, 1000 - w), g.in(0, 640 - h), w, h}); }
            const double r = g.in(4, 40);
            const Pt c{g.in(0, 1000), g.in(0, 640)};
            Pt target{-1, -1};
            const bool want = hiddenBy(c, r, occ, 24);
            const bool got = centreShiftTarget(c, r, win, occ, 24, target);
            checkTrue("centre shift: it only moves what is hidden", got == want);
            if (!got) continue;
            shifted++;
            const Rect fr = largestFreeRect(win, occ);
            checkTrue("centre shift: the target is inside the largest free rectangle", fr.contains(target) && within(fr, win));
            checkTrue("centre shift: the target is the middle of that rectangle", std::fabs(target.x - (fr.x + fr.w / 2)) < 1e-6 && std::fabs(target.y - (fr.y + fr.h / 2)) < 1e-6);
            if (fr.w >= 2 * (r + 24) && fr.h >= 2 * (r + 24)) {          // room for the body: its circle clears every panel
                bool clear = true;
                for (const Rect& o : occ) if (o.inflated(0).intersects(Rect{target.x - r, target.y - r, 2 * r, 2 * r})) clear = false;
                checkTrue("centre shift: the body moved to the target is outside every open panel", clear);
            }
        }
        checkTrue("centre shift: the fuzz had many hidden cases", shifted > 800);
    }
    {   // the circle counts, not just the centre: a body whose edge reaches under a card is hidden
        const Rect card{600, 100, 300, 200};
        checkTrue("hidden: a centre inside the card", hiddenBy({700, 150}, 5, {card}, 0));
        checkTrue("hidden: a centre just left of the card with a radius that reaches it", hiddenBy({595, 150}, 10, {card}, 0));
        checkTrue("hidden: a centre well outside is not", !hiddenBy({300, 150}, 10, {card}, 0));
        checkTrue("hidden: the 24 u margin counts", hiddenBy({580, 150}, 5, {card}, 24) && !hiddenBy({580, 150}, 5, {card}, 0));
    }
    // easing
    double v = 0;
    for (int i = 0; i < 18; i++) v = easeToward(v, 100, 1.0 / 60, 0.3, false);
    checkTrue("ease: about 0.3 s gets most of the way", v > 80 && v <= 100);
    double w = 0, last = 0;
    bool mono = true;
    for (int i = 0; i < 600; i++) { w = easeToward(w, 100, 1.0 / 60, 0.3, false); if (w < last || w > 100) mono = false; last = w; }
    checkTrue("ease: rises without overshoot and arrives", mono && w == 100);
    checkTrue("ease: reduced motion jumps at once", easeToward(0, 100, 1.0 / 60, 0.3, true) == 100);
    checkTrue("ease: no time, no move", easeToward(5, 100, 0, 0.3, false) == 5);
    checkTrue("ease: a falling target works too", easeToward(100, 0, 1.0 / 60, 0.3, false) < 100);
}

void testShellGlass() {
    // the opacities of docs/ui-0.15/DESIGN.md section 3, which tools/glass_alpha.py (Python, its own contrast function) computed
    struct Row { const char* id; double use; };
    const Row table[] = {{"light", 0.97}, {"dark", 0.96}, {"black", 0.93}, {"midnight", 0.95}, {"solar", 0.97}, {"paper", 0.97}, {"sage", 0.95}, {"contrast", 0.70},
                         {"synthwave", 0.94}, {"monochrome", 0.87}, {"aero", 0.75}, {"y2kice", 0.80}, {"y2knight", 0.78}};
    for (const Row& r : table) {
        const app::ThemeColors c = app::themeColors(r.id, false);
        checkTrue(std::string("glass: opacity of ") + r.id + " equals the Python table (" + num(r.use) + ", got " + num(glassOpacity(c)) + ")", std::fabs(glassOpacity(c) - r.use) < 0.0051);
    }
    // contrast of every text colour on every panel over the worst picture, and over pure white and pure black, in all the themes (Auto is Dark or Light)
    std::vector<app::ThemeColors> all;
    for (const app::ThemeDef* t : app::allThemes()) if (std::string(t->id) != "auto") all.push_back(app::themeColors(t->id, false));
    all.push_back(app::themeColors("auto", false));
    all.push_back(app::themeColors("auto", true));
    checkTrue("glass: all 14 themes were checked (13 and Auto, Dark and Light)", all.size() == 15 && app::allThemes().size() == 14);
    for (const app::ThemeColors& c : all) {
        const double a = glassOpacity(c);
        checkTrue("glass: opacity of " + c.id + " is below 1 and at most 0.97", a > 0 && a <= 0.97 + 1e-9);
        for (int white = 0; white < 2; white++) {
            const Rgb behind = white ? Rgb{1, 1, 1} : Rgb{0, 0, 0};
            const Rgb face = glassOver(c, behind);
            const std::string t = " in " + c.id + (white ? " over a white picture" : " over a black picture");
            checkTrue("glass: main text 4.5:1" + t, app::contrastRatio(c.fg, face) >= 4.5);
            checkTrue("glass: muted text 4.5:1" + t, app::contrastRatio(c.muted, face) >= 4.5);
            checkTrue("glass: accent text 4.5:1" + t, app::contrastRatio(c.accent, face) >= 4.5);
        }
        const Rgb w = worstBehind(c);
        checkTrue("glass: the worst picture is white behind a dark panel and black behind a light one (" + c.id + ")",
                  (app::relativeLuminance(c.card) > 0.4) == (w.r == 0 && w.g == 0 && w.b == 0));
        checkTrue("accent fill: the text on a chosen segment / primary button is 4.5:1 in " + c.id, app::contrastRatio(onAccent(c), c.accentFill) >= 4.5);
        if (app::contrastRatio(c.onacc, c.accentFill) >= 4.5) checkTrue("accent fill: a theme whose own on-accent colour passes keeps it (" + c.id + ")", onAccent(c).r == c.onacc.r && onAccent(c).g == c.onacc.g && onAccent(c).b == c.onacc.b);
    }
    // FOUND, not hidden: the existing themes Dark and Black have white (onacc) on the accent #6c9cff = 2.68:1 (the old UI's chosen buttons); the new UI uses onAccent() there
    checkTrue("accent fill: Dark's own on-accent colour is under 4.5:1 (an existing finding, reported)", app::contrastRatio(app::themeColors("dark", false).onacc, app::themeColors("dark", false).accentFill) < 4.5);
    // sanity of the test itself: a clearly too clear panel DOES fail (so the checks above can fail)
    const app::ThemeColors d = app::themeColors("dark", false);
    checkTrue("glass: at 0.5 opacity the dark panel's muted text fails over white", app::contrastRatio(d.muted, app::over(d.card, 0.5f, Rgb{1, 1, 1})) < 4.5);
}

void testShellKeys() {
    UiState s;
    s.open = PanelId::Speed;
    KeyOutcome o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: Esc closes the open panel", s.open == PanelId::None && o.consumed && !o.togglePlay);
    o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: Esc with nothing open does nothing", !o.consumed);
    s.open = PanelId::Chip; s.search = true;
    o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: Esc closes search first, the panel stays", !s.search && s.open == PanelId::Chip && o.consumed);
    o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: then the panel", s.open == PanelId::None && o.consumed);
    s.reading = true; s.oldPanel = true;
    o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: then the reading view", !s.reading && s.oldPanel && o.consumed);
    o = applyKey(s, Key::Escape, false, true);
    checkTrue("keys: then the Old panel", !s.oldPanel && o.consumed);
    s = UiState{}; s.open = PanelId::Show;
    o = applyKey(s, Key::CtrlK, false, true);
    checkTrue("keys: Ctrl+K opens search and closes the open dropdown (one at a time)", s.search && s.open == PanelId::None && o.consumed);
    o = applyKey(s, Key::CtrlK, true, true);
    checkTrue("keys: Ctrl+K works even when a text field has the keyboard", s.search && o.consumed);
    s = UiState{};
    o = applyKey(s, Key::Space, false, true);
    checkTrue("keys: Space plays / pauses", o.consumed && o.togglePlay);
    o = applyKey(s, Key::Space, true, true);
    checkTrue("keys: Space in a text field is typing, not play", !o.consumed && !o.togglePlay);
    o = applyKey(s, Key::Space, false, false);
    checkTrue("keys: Space where the lab has no play does nothing", !o.consumed && !o.togglePlay);
    s.search = true;
    o = applyKey(s, Key::Space, false, true);
    checkTrue("keys: Space while searching is typing", !o.consumed && !o.togglePlay);
    o = applyKey(s, Key::Other, false, true);
    checkTrue("keys: any other key is not ours", !o.consumed);
}

void testShellReading() {
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1000.0, 1280.0, 1920.0, 2560.0, 3200.0}) {
                const Metrics m = metrics(dpi, tf);
                const double W = std::lround(wu * dpi), H = std::lround(800 * dpi);
                const ReadingGeom g = readingGeometry(W, H, m);
                const std::string t = tag(dpi, tf) + " window " + num(W);
                checkTrue("reading: the column is inside the window" + t, within(g.column, Rect{0, 0, W, H}));
                checkTrue("reading: the column is centred" + t, std::fabs(g.column.x + g.column.w / 2 - W / 2) < 1.0);
                checkTrue("reading: the column is at most 34 em (about 70 characters)" + t, g.column.w <= m.em(34) + 1e-6);
                checkTrue("reading: the column is 34 em when there is room" + t, W < m.em(34) + 2 * m.s16 || std::fabs(g.column.w - m.em(34)) < 1.0);
                checkTrue("reading: the rail only when the window is wider than 60 em plus the column" + t, g.rail == (W > m.em(60) + g.column.w));
                if (g.rail) checkTrue("reading: the rail is left of the column, inside the window" + t, g.toc.right() <= g.column.x && g.toc.x >= 0 && within(g.toc, Rect{0, 0, W, H}));
            }
}

void testShellSearch() {
    const std::vector<SearchItem> items = {
        {"Earth", "Bodies", "body:Earth"}, {"Mars", "Bodies", "body:Mars"}, {"Moon", "Bodies", "body:Moon"}, {"Hydrogen", "Elements and isotopes", "el:1"},
        {"Helium", "Elements and isotopes", "el:2"}, {"2D / 3D view", "Views", "view"}, {"Time speed", "Settings", "speed"}, {"Text size", "Settings", "textsize"},
        {"Why do we have seasons?", "Guides", "g:seasons"}, {"Earth and Moon", "Guides", "g:em"}, {"Heart of the matter", "Guides", "g:heart"}};
    auto labels = [&](const std::string& q, int n = 8) { std::string s; for (int i : searchResults(q, items, n)) s += items[i].label + "|"; return s; };
    checkStr("search: a prefix finds the label", labels("ear"), "Earth|Earth and Moon|Heart of the matter|");
    checkStr("search: case does not matter", labels("EARTH"), "Earth|Earth and Moon|");
    checkStr("search: labels that start with the query come before a word that starts with it, before a part inside", labels("m"), "Mars|Moon|Earth and Moon|Heart of the matter|Helium|Time speed|");
    checkStr("search: a word that starts with it", labels("moon"), "Moon|Earth and Moon|");
    checkStr("search: part of a word", labels("ydro"), "Hydrogen|");
    checkStr("search: nothing", labels("zzz"), "");
    checkStr("search: empty query lists nothing", labels(""), "");
    checkTrue("search: at most 8 results", searchResults("e", items, 8).size() <= 8);
    checkTrue("search: at most 3 when asked", searchResults("e", items, 3).size() == 3);
    checkStr("search: spaces around the query are ignored", labels("  moon "), "Moon|Earth and Moon|");
}

void testShellPanelStates() {
    checkTrue("panel state: default when nothing is saved", panelCollapsed("", "card", true) && !panelCollapsed("", "card", false));
    std::string s = withPanelState("", "card", true);
    checkStr("panel state: write one", s, "card=c");
    s = withPanelState(s, "legend", false);
    checkStr("panel state: write a second", s, "card=c,legend=o");
    checkTrue("panel state: read them back", panelCollapsed(s, "card", false) && !panelCollapsed(s, "legend", true));
    s = withPanelState(s, "card", false);
    checkStr("panel state: a change replaces, it does not append", s, "card=o,legend=o");
    checkTrue("panel state: garbage is ignored", panelCollapsed("zzz,=,card", "card", true) && !panelCollapsed("zzz,=,card", "card", false));
}

void testShellPrefs() {
    app::Prefs p;
    checkStr("prefs: New UI is Off by default", p.newUi, "off");
    p.newUi = "on"; p.shellPanels = "card=c,legend=o";
    const app::Prefs q = app::loadPrefs(app::savePrefs(p));
    checkStr("prefs: New UI saves and loads", q.newUi, "on");
    checkStr("prefs: panel states save and load", q.shellPanels, "card=c,legend=o");
    const app::Prefs bad = app::loadPrefs("newui=maybe\nshellpanels=card=c\n");
    checkStr("prefs: a bad New UI value falls back to Off", bad.newUi, "off");
    checkTrue("prefs: valid values are on and off", app::validNewUi("on") && app::validNewUi("off") && !app::validNewUi("yes") && !app::validNewUi(""));
    const std::string text = app::savePrefs(app::Prefs{});
    checkTrue("prefs: with the defaults the saved file has no new lines at all (a file from before 0.15 stage 3 is unchanged)", text.find("newui") == std::string::npos && text.find("shellpanels") == std::string::npos);
    checkStr("prefs: and loads back as Off", app::loadPrefs(text).newUi, "off");
}

void testShell() {
    testShellTypeScale();
    testShellFrame();
    testShellToolbarOverflow();
    testShellTopBar();
    testShellPanels();
    testShellCentreShift();
    testShellGlass();
    testShellKeys();
    testShellReading();
    testShellSearch();
    testShellPanelStates();
    testShellPrefs();
}
}  // namespace
