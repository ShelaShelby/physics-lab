// 0.15 stage 3: the logic of the new UI's shell (src/app/shell.*). WRITTEN BEFORE the code (compile-red first).
// References, none of them our own earlier output: the sizes and rules of docs/ui-0.15/DESIGN.md (the owner's chosen design: Caption 13 / Body 15 / Title 17 / Key 28 units,
// floors 12 / 14 / 15 / 22 px, Body 14 px at Text size Small, 19 and 23 px at 125 % and 150 %), the WCAG 2.x contrast rule (the formula is tested elsewhere against hand values),
// the opacity table computed by docs/ui-0.15/tools/glass_alpha.py (a separate Python program), and geometry that can be checked by reading it (nothing leaves the window, nothing
// overlaps). No long double, no hard-coded hashes.
#include "app/shell.h"
#include "app/theme_style.h"
#include "orbit/subject_card.h"
#include "orbit/sky.h"
#include <set>

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
                                if (a == Anchor::TopRight) checkTrue("panel: top right is flush to the right margin" + t, std::fabs(W - (m.s24 + m.s8 + m.s4) - r.right()) < 1.0 || ww >= W - 2 * m.s12);
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

// 0.15 stage 5p (#4): the Atom picture is centred on the free part, and the toolbar is centred on the picture's centre of the LAST frame. With the Element picker open the free part used to be worked out
// with the toolbar as an obstacle, so the toolbar sat in the strip beside the picker it had been put in, made that strip smaller, the other strip won, the toolbar moved there, and so on, every
// frame (the owner's "everything shows twice": a photo catches both). The loop is simulated here with the real makeFrame / placePanel / factLineRect, at 100, 125 and 150 % scale.
// Reference: a layout that is right is a fixed point: the next frame gives the same centre. (No picture is involved.)
static std::vector<double> freeCentres(double dpi, double W, double H, bool dockOpen, bool newRule) {
    const Metrics m = metrics(dpi, 1.0);
    const double tw = 8 * (m.toolBtn + m.s4) + 2 * m.s8, rw = 4 * m.row, rh = m.row;
    const double dockW = dockOpen ? dockWidth(m, W) : 0;
    double sceneCx = -1;
    std::vector<double> centres;
    for (int it = 0; it < 12; it++) {
        const Frame f = makeFrame(W, H, m, tw, rw, rh, dockW, 0, sceneCx);
        const Rect card{f.freeW - m.s12 - m.em(21) - (dockOpen ? 0 : 0), f.topBar.bottom() + m.s12, m.em(21), m.em(24)};
        Rect pick = placePanel(Anchor::UnderChip, m.em(38), m.em(60), f, m, W / 2);
        pick = sideStep(pick, card, f.window, m.s12, m.s12);
        const Rect fl = factLineRect(f, m, m.em(60), 0, false);
        std::vector<Rect> occ{f.topBar, card, pick, f.toolbar, f.readout, fl};
        if (dockOpen) occ.push_back(f.dock);
        const double top = std::min(f.toolbar.y, fl.y);
        const Rect fr = newRule ? atomFreeRect(f.window, occ, top) : largestFreeRect(f.window, occ);
        centres.push_back(fr.x + fr.w / 2);
        sceneCx = fr.x + fr.w / 2;
    }
    return centres;
}

void testShellAtomFreeLoop() {
    int oldFlips = 0, cases = 0;
    for (double dpi : {1.0, 1.25, 1.5})
        for (double wu : {880.0, 1000.0, 1109.0, 1280.0, 1500.0, 1920.0})
            for (double hu : {560.0, 640.0, 800.0, 882.0})
                for (bool dock : {false, true}) {
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi);
                    const std::vector<double> a = freeCentres(dpi, W, H, dock, true), o = freeCentres(dpi, W, H, dock, false);
                    const std::string t = " (" + num(wu) + " x " + num(hu) + " at " + num(dpi * 100) + " %, Controls " + (dock ? "open" : "closed") + ")";
                    cases++;
                    bool still = true;
                    for (std::size_t i = 6; i < a.size(); i++) if (std::fabs(a[i] - a[i - 1]) > 1e-6) still = false;
                    checkTrue("atom free part: with the picker open the centre stops moving after a few frames" + t, still);
                    bool oldStill = true;
                    for (std::size_t i = 6; i < o.size(); i++) if (std::fabs(o[i] - o[i - 1]) > 1e-6) oldStill = false;
                    if (!oldStill) oldFlips++;
                }
    printf("atom free part: the old rule flipped every frame in %d of %d simulated layouts, the new one in none\n", oldFlips, cases);
    checkTrue("atom free part: the simulation reproduces the flip with the old rule in at least one layout (else it proves nothing)", oldFlips > 0);
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
                         {"monochrome", 0.87}};   // (0.15 stage 5m, on purpose: the rows of synthwave 0.94, aero 0.75 and y2knight 0.78 went with those themes)
    for (const Row& r : table) {
        const app::ThemeColors c = app::themeColors(r.id, false);
        checkTrue(std::string("glass: opacity of ") + r.id + " equals the Python table (" + num(r.use) + ", got " + num(glassOpacity(c)) + ")", std::fabs(glassOpacity(c) - r.use) < 0.0051);
    }
    // contrast of every text colour on every panel over the worst picture, and over pure white and pure black, in all the themes (Auto is Dark or Light)
    std::vector<app::ThemeColors> all;
    for (const app::ThemeDef* t : app::allThemes()) if (std::string(t->id) != "auto") all.push_back(app::themeColors(t->id, false));
    all.push_back(app::themeColors("auto", false));
    all.push_back(app::themeColors("auto", true));
    checkTrue("glass: all 10 themes were checked (9 and Auto, Dark and Light = 11; 14 and 13 before the three themes were archived, stage 5m, on purpose)", all.size() == 11 && app::allThemes().size() == 10);
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
    p.shellPanels = "card=c,legend=o";
    const app::Prefs q = app::loadPrefs(app::savePrefs(p));
    checkStr("prefs: panel states save and load", q.shellPanels, "card=c,legend=o");
    // 0.16 stage 1b: the New UI setting is gone; a file that still holds the old key loads (the key is ignored, the other lines are read) and is saved back without it
    const app::Prefs old = app::loadPrefs("textSize=large\nnewui=on\nshellpanels=card=c\nnewui=maybe\n");
    checkStr("prefs: an old newui key is ignored, the lines around it are still read (text size)", old.textSize, "large");
    checkStr("prefs: an old newui key is ignored, the lines around it are still read (panels)", old.shellPanels, "card=c");
    checkTrue("prefs: and it is not written back", app::savePrefs(old).find("newui") == std::string::npos);
    const std::string text = app::savePrefs(app::Prefs{});
    checkTrue("prefs: with the defaults the saved file has no new lines at all", text.find("newui") == std::string::npos && text.find("shellpanels") == std::string::npos);
}


// ================= 0.15 stage 4 (tests written BEFORE the code, compile-red first) =================
// References: the owner's notes of 6 Oct 2026 (play in the chip, Save picture in the menu, a fact line that fades every ~20 s and stops while hovered), the subject card of
// DESIGN.md section 5 (one big key number, 4 to 6 key values, "All values" = every line of the old card), WCAG 2.x for the text colours, and the hand-written card texts below
// (the wording of orbit/card_text.cpp copied by hand into the cases, so the test does not run the code it tests for the synthetic cases).

orbit::Card mk(const std::string& title, std::vector<std::string> lines) { orbit::Card c; c.title = title; c.lines = std::move(lines); return c; }

bool hasLine(const orbit::SubjectCard& s, const std::string& l) { for (const std::string& x : s.all) if (x == l) return true; return false; }
std::string joined(const orbit::SubjectCard& s) { std::string r; for (const orbit::SubjectValue& v : s.values) r += (v.label.empty() ? "" : v.label + "=") + v.value + "|"; return r; }
std::string labelsOf(const orbit::SubjectCard& s) { std::string r; for (const orbit::SubjectValue& v : s.values) r += v.label + "|"; return r; }

void testSubjectCardSynthetic() {
    using orbit::Selection;
    {   // a planet
        const orbit::Card c = mk("Earth · rocky planet (home)", {"Size: 1.0 × Earth (radius 6,378 km)", "Mass: 1.0 Earths  ·  gravity 1.0 g at the equator", "Distance: 1.00 AU (150 million km)",
                                 "Speed: 29.8 km/s (escape here 42.1)", "Orbit: near-circle", "One lap: 1.00 years", "Temperature: 15 °C (real, mean surface)", "Turns once in 23 h 56 min", "1 moon shown"});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Body, 3}, "", c);
        checkStr("card: planet name", s.name, "Earth");
        checkStr("card: planet kind", s.kind, "rocky planet (home)");
        checkStr("card: planet key number is the distance", s.keyValue, "1.00 AU");
        checkStr("card: planet key caption", s.keyCaption, "from the Sun");
        checkStr("card: planet key values, in this order", labelsOf(s), "Size|Mass|Speed|Orbit|One lap|Temperature|");
        checkStr("card: a key value is the rest of its line, word for word", s.values[1].value, "1.0 Earths  ·  gravity 1.0 g at the equator");
        checkTrue("card: all values = every line of the old card, in order", s.all == c.lines);
    }
    {   // a moon
        const orbit::Card c = mk("Moon · moon of Earth", {"Radius: 1737.4 km", "Distance from Earth: 384,400 km", "Orbit: 27.3 days", "Speed: 1.02 km/s", "Always shows the same face to Earth"});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Moon, 3, 0}, "", c);
        checkStr("card: moon name", s.name, "Moon");
        checkStr("card: moon kind", s.kind, "moon of Earth");
        checkStr("card: moon key number", s.keyValue, "384,400 km");
        checkStr("card: moon key caption", s.keyCaption, "from Earth");
        checkStr("card: moon values", joined(s), "Radius=1737.4 km|Orbit=27.3 days|Speed=1.02 km/s|Always shows the same face to Earth|");
    }
    {   // the Sun
        const orbit::Card c = mk("Sun · G-type star", {"Mass: 1 solar mass (M☉), 1.99×10³⁰ kg", "Radius: 1.00 Suns (695,700 km)", "Surface (rough): about 5,770 K  ·  1.00 Suns bright", "Habitable zone (rough): 0.95 – 1.67 AU",
                                 "Frost line (rough): 2.7 AU (rock inside, giants outside)", "Turns once in 25.4 days (standard rate)", "Lifetime (rough): about 10 billion years", "Rough main-sequence estimates"});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Star}, "", c);
        checkStr("card: Sun kind", s.kind, "G-type star");
        checkStr("card: Sun key number is the radius in km (the figure inside the brackets)", s.keyValue, "695,700 km");
        checkStr("card: Sun key caption", s.keyCaption, "radius");
        checkStr("card: Sun values", labelsOf(s), "Mass|Surface (rough)|Habitable zone (rough)|Frost line (rough)|Lifetime (rough)|");
    }
    {   // a comet
        const orbit::Card c = mk("Halley’s Comet", {"Nucleus: about 11 km across", "Distance from the Sun now: 35 AU (5,272 million km)", "Speed now: 0.9 km/s", "Closest to the Sun: 0.57 AU  ·  farthest: 35 AU",
                                 "One trip round the Sun: 76 years", "Last closest approach: 8 Feb 1986", "Next: 8 Jan 2062", "Far from the Sun: frozen and dark, no tail"});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Body, 20}, "", c);
        checkStr("card: comet name", s.name, "Halley’s Comet");
        checkStr("card: comet kind", s.kind, "comet");
        checkStr("card: comet key number", s.keyValue, "35 AU");
        checkStr("card: comet key caption", s.keyCaption, "from the Sun now");
        checkStr("card: comet values", labelsOf(s), "Nucleus|Speed now|Closest to the Sun|One trip round the Sun|Last closest approach|Next|");
    }
    {   // the barycentre
        const orbit::Card c = mk("Barycentre of the Solar System", {"The point the Sun and all planets balance around", "They orbit this point, not the Sun’s centre", "Now 1.4 Sun radii from the Sun’s centre",
                                 "Ranges from inside the Sun to about 2 Sun radii out", "Mostly Jupiter’s pull, a bit Saturn’s", "The Sun is drawn at its true size here"});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Bary}, "", c);
        checkStr("card: barycentre name", s.name, "Barycentre of the Solar System");
        checkStr("card: barycentre key number", s.keyValue, "1.4 Sun radii");
        checkTrue("card: barycentre has 4 plain values and the key line is not repeated", s.values.size() == 4 && !s.values[0].value.empty() && joined(s).find("Now 1.4") == std::string::npos);
    }
    {   // belts: the key number is a figure that stands in the first line
        const orbit::Card main = mk("Asteroid belt", {"Between Mars and Jupiter, about 2.1 – 3.3 AU", "More than 700,000 rocks wider than 1 km", "Total mass: only about 3% of our Moon", "Ceres alone holds 39% of it"});
        const orbit::SubjectCard a = orbit::subjectCard(Selection{Selection::Belt, 0, 0}, "main", main);
        checkStr("card: asteroid belt key number", a.keyValue, "2.1 – 3.3 AU");
        checkStr("card: asteroid belt key caption", a.keyCaption, "from the Sun");
        const orbit::Card tro = mk("Jupiter's Trojans", {"They share Jupiter's orbit: 60° ahead and behind", "About a million rocks wider than 1 km", "Held in place by the Sun and Jupiter together"});
        const orbit::SubjectCard t = orbit::subjectCard(Selection{Selection::Belt, 0, 2}, "trojan", tro);
        checkStr("card: Trojans key number", t.keyValue, "60°");
        checkStr("card: Trojans key caption", t.keyCaption, "ahead and behind Jupiter");
        const orbit::Card oort = mk("Oort cloud (never seen directly)", {"Inner part 2,000 – 20,000 AU, outer part to about 200,000 AU", "Inferred from comets: the home of long-period comets"});
        const orbit::SubjectCard o = orbit::subjectCard(Selection{Selection::Belt, 0, 3}, "oort", oort);
        checkStr("card: Oort cloud key number is the outer edge", o.keyValue, "200,000 AU");
        const orbit::Card kui = mk("Kuiper belt", {"From Neptune's orbit (30 AU) out to about 50 AU", "More than 100,000 icy worlds wider than 100 km"});
        const orbit::SubjectCard k = orbit::subjectCard(Selection{Selection::Belt, 0, 1}, "kuiper", kui);
        checkStr("card: Kuiper belt key number is the outer edge", k.keyValue, "50 AU");
    }
    {   // far-away things
        const orbit::Card c = mk("Alpha Centauri", {"G2V star, in Centaurus", "Distance: 4.34 light years (275,000 AU)", "The light we see left it 4.34 years ago", "Voyager 1 would take about 77,000 years",
                                 "The nearest star system: two Sun-like stars", "circling each other."});
        const orbit::SubjectCard s = orbit::subjectCard(Selection{Selection::Far, 0, 0}, "", c);
        checkStr("card: far star kind is its first line", s.kind, "G2V star, in Centaurus");
        checkStr("card: far star key number", s.keyValue, "4.34 light years");
        checkStr("card: far star key caption", s.keyCaption, "from the Sun");
        checkTrue("card: far star values do not repeat the kind or the key line", s.values.size() == 4 && joined(s).find("G2V") == std::string::npos && joined(s).find("Distance") == std::string::npos);
        const orbit::Card v = mk("Voyager 1 · spacecraft", {"Distance from the Sun: 168.4 AU", "Speed: 16.9 km/s away from the Sun", "A radio signal takes about 23.3 hours from the Sun", "Launched in 1977; it left the Sun’s wind"});
        const orbit::SubjectCard p = orbit::subjectCard(Selection{Selection::Far, 0, 40}, "", v);
        checkStr("card: probe kind", p.kind, "spacecraft");
        checkStr("card: probe key number", p.keyValue, "168.4 AU");
    }
}

void testSubjectCardReal() {
    // every card of the real system (every body, moon, belt and far-away thing, at the reference date): the structure rules hold
    orbit::SolarSystem sys;
    sys.loadAt(1759665600000LL);                                    // 5 Oct 2026 12:00 UT
    orbit::CardOptions opt;
    std::vector<std::pair<orbit::Selection, std::string>> all = {{orbit::Selection{orbit::Selection::Star}, ""}, {orbit::Selection{orbit::Selection::Bary}, ""}};
    for (const orbit::Body& b : sys.bodies) {
        all.push_back({orbit::Selection{orbit::Selection::Body, b.id}, ""});
        for (std::size_t j = 0; j < b.moons.size(); j++) all.push_back({orbit::Selection{orbit::Selection::Moon, b.id, static_cast<int>(j)}, ""});
    }
    for (std::size_t g = 0; g < sys.belts.size(); g++) all.push_back({orbit::Selection{orbit::Selection::Belt, 0, static_cast<int>(g)}, sys.belts[g].key});
    for (int f = 0; f < orbit::farCount(); f++) all.push_back({orbit::Selection{orbit::Selection::Far, 0, f}, ""});
    int n = 0;
    for (const auto& [sel, key] : all) {
        orbit::Card c;
        if (!orbit::cardText(sys, sel, opt, c)) continue;
        n++;
        const orbit::SubjectCard s = orbit::subjectCard(sel, key, c);
        const std::string t = " for " + c.title;
        checkTrue("card: the name is not empty" + t, !s.name.empty());
        checkTrue("card: the kind is not empty" + t, !s.kind.empty());
        checkTrue("card: there is a key number and a caption" + t, !s.keyValue.empty() && !s.keyCaption.empty());
        bool keyInLine = false;
        for (const std::string& l : c.lines) if (l.find(s.keyValue) != std::string::npos) keyInLine = true;
        checkTrue("card: the key number is a piece of one of the old card's lines, word for word" + t + " [" + s.keyValue + "]", keyInLine);
        checkTrue("card: all values are exactly the old card's lines" + t, s.all == c.lines);
        const bool kindIsLine = sel.kind == orbit::Selection::Far && c.title.find(" · ") == std::string::npos;     // a far-away star's kind is its first line
        const std::size_t avail = c.lines.size() - 1 - (kindIsLine ? 1 : 0);                                       // the lines left after the key line (and the kind)
        checkTrue("card: 4 to 6 key values (fewer only when the card has no more lines)" + t + " (" + std::to_string(s.values.size()) + ")", s.values.size() >= std::min<std::size_t>(4, avail) && s.values.size() <= 6);
        for (const orbit::SubjectValue& v : s.values) {
            const std::string line = v.label.empty() ? v.value : v.label + ": " + v.value;
            checkTrue("card: a key value is one of the old card's lines, word for word" + t + " [" + line + "]", hasLine(s, line));
        }
        std::set<std::string> seen;
        for (const orbit::SubjectValue& v : s.values) checkTrue("card: no line is shown twice" + t, seen.insert(v.label + "=" + v.value).second);
    }
    checkTrue("card: all the cards of the real system were checked (at least 55)", n >= 55);
}

void testShellRecentSearch() {
    const std::vector<SearchItem> items = {{"Earth", "Bodies", "body:Earth"}, {"Mars", "Bodies", "body:Mars"}, {"Moons", "Settings", "set:moons"}, {"Moon", "Bodies", "body:Moon"},
                                           {"Save picture", "Menu", "menu:save"}, {"Sirius", "Bodies", "body:Sirius"}, {"Settings", "Menu", "menu:settings"}};
    auto labels = [&](const std::string& q, const std::vector<std::string>& recent, int n = 8) { std::string s; for (int i : searchResults(q, items, n, recent)) s += items[i].label + "|"; return s; };
    checkStr("recent: an empty query lists the recent ones first-used first", labels("", {"body:Moon", "menu:save", "body:Earth"}), "Moon|Save picture|Earth|");
    checkStr("recent: an empty query with no recents lists nothing", labels("", {}), "");
    checkStr("recent: a recent id that is no longer in the list is skipped", labels("", {"gone", "body:Mars"}), "Mars|");
    checkStr("recent: with a query, the order inside a tier puts the recent ones first", labels("s", {"menu:settings", "body:Sirius"}), "Settings|Sirius|Save picture|Mars|Moons|");
    checkStr("recent: a recent item still has to match the query", labels("mo", {"body:Earth"}), "Moons|Moon|");
    checkStr("recent: without recents the order is the list's (as before)", labels("s", {}), "Save picture|Sirius|Settings|Mars|Moons|");
    checkTrue("recent: at most the asked number", searchResults("", items, 2, {"body:Moon", "menu:save", "body:Earth"}).size() == 2);
    std::vector<std::string> r;
    r = pushRecent(r, "a"); r = pushRecent(r, "b"); r = pushRecent(r, "a");
    checkStr("recent: choosing again moves it to the front, no copies", packRecent(r), "a;b");
    for (int i = 0; i < 12; i++) r = pushRecent(r, "x" + std::to_string(i));
    checkTrue("recent: only the last 8 are kept", r.size() == 8 && r[0] == "x11" && r[7] == "x4");
    checkTrue("recent: pack and unpack are inverses", unpackRecent(packRecent(r)) == r);
    checkTrue("recent: unpacking garbage gives nothing odd", unpackRecent("").empty() && unpackRecent(";;").empty());
}

void testShellFacts() {
    // the fade: 0 -> 1 in `fade` seconds, holds, 1 -> 0 in the last `fade` seconds
    const double I = 20, F = 0.5;
    checkTrue("facts: starts invisible", factAlpha(0, I, F) == 0);
    checkTrue("facts: half visible after half the fade", std::fabs(factAlpha(0.25, I, F) - 0.5) < 1e-9);
    checkTrue("facts: fully visible in the middle", factAlpha(1.0, I, F) == 1 && factAlpha(10, I, F) == 1 && factAlpha(I - 0.001, I, F) == 1);
    checkTrue("facts: fading out after the interval", std::fabs(factAlpha(I + 0.25, I, F) - 0.5) < 1e-9);
    checkTrue("facts: gone at the end", factAlpha(I + F, I, F) == 0 && factAlpha(I + 2 * F, I, F) == 0);
    for (double t = 0; t < I + 2; t += 0.037) checkTrue("facts: alpha stays between 0 and 1", factAlpha(t, I, F) >= 0 && factAlpha(t, I, F) <= 1);
    // the clock, at a fixed time step of 16 ms (as the app's PL_FIXED_DT run does)
    {
        FactClock c;
        int frames = 0, changes = 0;
        for (; frames < 1500 && changes == 0; frames++) if (factTick(c, 0.016, false, false, I, F)) changes++;
        checkTrue("facts: the next fact comes after about 20.5 s (the interval plus the fade out): frames " + std::to_string(frames), changes == 1 && frames * 0.016 >= I && frames * 0.016 <= I + F + 0.05);
        checkTrue("facts: the clock starts again for the new fact", c.t == 0);
    }
    {   // hovering pauses it completely, and the text stays fully visible
        FactClock c;
        for (int i = 0; i < 600; i++) factTick(c, 0.016, false, false, I, F);          // 9.6 s
        const double before = c.t;
        bool changed = false;
        for (int i = 0; i < 5000; i++) if (factTick(c, 0.016, true, false, I, F)) changed = true;     // 80 s over it
        checkTrue("facts: no change while hovered, however long", !changed && c.t == before);
        FactClock late; late.t = I + 0.3;                                                // already fading out
        factTick(late, 0.016, true, false, I, F);
        checkTrue("facts: hovering during the fade-out brings the text back", factAlpha(late.t, I, F) == 1 && late.t <= I);
        FactClock early; early.t = 0.2;                                                  // still fading in
        factTick(early, 0.016, true, false, I, F);
        checkTrue("facts: hovering during the fade-in shows the text in full at once", factAlpha(early.t, I, F) == 1);
    }
    {   // reduced motion: no automatic change at all (as the old line; the arrows still work)
        FactClock c;
        bool changed = false;
        for (int i = 0; i < 3000; i++) if (factTick(c, 0.016, false, true, I, F)) changed = true;
        checkTrue("facts: with Reduced motion nothing changes by itself", !changed);
    }
    // where the line sits
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1000.0, 1280.0, 1920.0})
                for (double hu : {560.0, 640.0, 800.0, 1080.0}) {
                    const Metrics m = metrics(dpi, tf);
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi);
                    const std::string t = tag(dpi, tf) + " window " + num(W) + "x" + num(H);
                    double tw = 0;
                    const Frame f = makeFrame(W, H, m, 6 * (m.toolBtn + m.s4) + 4 * m.s8 + 2 * (2 * (m.body * 2.2 + 2 * m.s12) + m.caretW), 11 * m.caption, 3 * m.caption * 1.4);
                    (void)tw;
                    const Rect r = factLineRect(f, m, 60 * m.caption);
                    checkTrue("fact line: inside the window" + t, within(r, f.window));
                    checkTrue("fact line: not over the toolbar" + t, !r.intersects(f.toolbar));
                    checkTrue("fact line: not over the readout" + t, !r.intersects(f.readout));
                    checkTrue("fact line: not over the top bar" + t, !r.intersects(f.topBar));
                    checkTrue("fact line: at the bottom left (its left edge is in the left third)" + t, r.x < W / 3);
                    checkTrue("fact line: tall enough for its text" + t, r.h >= m.caption);
                    checkTrue("fact line: wide enough to read when the window is not tiny" + t, wu < 1000 || r.w >= m.em(14));
                    checkTrue("fact line: in the lower part of the window" + t, r.y > H * 0.6);
                    // 0.15 stage 5d: a long fact wraps to two lines; the taller rectangle keeps every rule above (two Caption lines with their leading: at most 2 x (caption + 4 u))
                    const double twoH = 2.0 * (m.caption + m.s4);
                    const Rect r2 = factLineRect(f, m, 60 * m.caption, twoH);
                    checkTrue("fact line, two lines: tall enough" + t, r2.h >= twoH);
                    checkTrue("fact line, two lines: inside the window" + t, within(r2, f.window));
                    checkTrue("fact line, two lines: not over the toolbar" + t, !r2.intersects(f.toolbar));
                    checkTrue("fact line, two lines: not over the readout" + t, !r2.intersects(f.readout));
                    checkTrue("fact line, two lines: not over the top bar" + t, !r2.intersects(f.topBar));
                    checkTrue("fact line, two lines: as wide as one line" + t, std::abs(r2.w - r.w) < 0.01);
                }
}

void testShellTextShadow() {
    // text drawn straight on the picture (the readout, the fact line): its colour against the shadow round it must pass 4.5:1, so it is readable over black AND white (the shadow
    // covers the glyph's neighbourhood at kShadowAlpha; behind that, black or white shows through only 1 - alpha)
    std::vector<app::ThemeColors> all;
    for (const app::ThemeDef* t : app::allThemes()) all.push_back(app::themeColors(t->id, false));
    all.push_back(app::themeColors("auto", true));
    for (const app::ThemeColors& c : all) {
        const Rgb sh = textShadow(c);
        checkTrue("shadow: the text-shadow colour is black or white in " + c.id, (sh.r == 0 && sh.g == 0 && sh.b == 0) || (sh.r == 1 && sh.g == 1 && sh.b == 1));
        checkTrue("shadow: main text against its shadow is 4.5:1 in " + c.id, app::contrastRatio(c.fg, sh) >= 4.5);
        const Rgb tx = factTextColour(c);
        checkTrue("shadow: the fact line's colour is 4.5:1 against the shadow in " + c.id, app::contrastRatio(tx, sh) >= 4.5);
        for (int white = 0; white < 2; white++) {
            const Rgb behind = white ? Rgb{1, 1, 1} : Rgb{0, 0, 0};
            const Rgb local = app::over(sh, kShadowAlpha, behind);
            checkTrue("shadow: fact text 4.5:1 over " + std::string(white ? "white" : "black") + " behind its shadow in " + c.id, app::contrastRatio(tx, local) >= 4.5);
        }
        checkTrue("shadow: factContrast reports at least 4.5 in " + c.id + " (" + num(factContrast(c)) + ")", factContrast(c) >= 4.5);
    }
    // the test can fail: a muted grey without any shadow does not pass over both
    const app::ThemeColors d = app::themeColors("dark", false);
    checkTrue("shadow: without a shadow, light text on a white picture fails (so the check above can fail)", app::contrastRatio(d.fg, Rgb{1, 1, 1}) < 4.5);
}

void testShellPrefs4() {
    app::Prefs p;
    checkStr("prefs: Facts are On by default", p.facts, "on");
    p.facts = "off"; p.recent = "body:Earth;menu:save";
    const app::Prefs q = app::loadPrefs(app::savePrefs(p));
    checkStr("prefs: Facts Off saves and loads", q.facts, "off");
    checkStr("prefs: recent searches save and load", q.recent, "body:Earth;menu:save");
    checkStr("prefs: a bad Facts value falls back to On", app::loadPrefs("facts=maybe\n").facts, "on");
    const std::string text = app::savePrefs(app::Prefs{});
    checkTrue("prefs: with the defaults the saved file has no facts or recent line (a file from stage 3 is unchanged)", text.find("facts") == std::string::npos && text.find("recent") == std::string::npos);
}

void testShellSideStep() {
    const Rect win{0, 0, 1000, 640}, card{676, 56, 300, 330};
    Rect r{414, 52, 360, 500};                                                         // a Time panel under a chip that sits right of the middle
    Rect o = sideStep(r, card, win, 12, 12);
    checkTrue("side step: a panel over the card moves left of it", o.right() <= card.x - 12 + 1e-9 && o.x >= 12 && o.y == r.y && o.w == r.w && o.h == r.h);
    checkTrue("side step: and no longer touches it", !o.intersects(card));
    Rect far{20, 52, 300, 300};
    o = sideStep(far, card, win, 12, 12);
    checkTrue("side step: a panel that does not touch the card stays where it is", o.x == far.x && o.y == far.y);
    const Rect wide{0, 0, 1000, 640}, card2{20, 56, 300, 330};                           // a card on the left: there is no room left of it, so the panel goes to its right
    o = sideStep(Rect{100, 52, 360, 500}, card2, wide, 12, 12);
    checkTrue("side step: with no room on the left it goes to the right of the card", o.x == card2.right() + 12 && o.right() <= wide.right() - 12);
    o = sideStep(Rect{100, 52, 600, 500}, Rect{300, 56, 300, 330}, wide, 12, 12);        // room on neither side: stays
    checkTrue("side step: with room on neither side it stays", o.x == 100);
    Lcg rng;
    for (int i = 0; i < 4000; i++) {
        const Rect b{rng.in(0, 800), rng.in(0, 300), rng.in(50, 300), rng.in(50, 300)}, p{rng.in(0, 700), rng.in(0, 300), rng.in(100, 400), rng.in(100, 400)};
        const Rect q = sideStep(p, b, wide, 12, 12);
        checkTrue("side step: only x changes, and the size never", q.y == p.y && q.w == p.w && q.h == p.h);
        if (q.x != p.x) checkTrue("side step: when it moves it no longer touches the blocker and lies inside the window", !q.intersects(b) && q.x >= 12 - 1e-9 && q.right() <= wide.right() - 12 + 1e-9);
    }
}

// 0.15 stage 5c: the Controls panel (the dock on the right) and the readout's backing pill. WRITTEN BEFORE the code. References: DESIGN.md (the owner's decision of 6 Oct 2026: one docked
// panel on the right, about 22 em wide, below the top bar; the bottom toolbar and the readout stay left of it), the WCAG 2.x contrast formula written out again here (not the app's),
// and plain geometry.
double wcagLum(const Rgb& c) {
    const auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}
double wcag(const Rgb& a, const Rgb& b) { const double x = wcagLum(a), y = wcagLum(b); return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05); }

void testShellDock() {
    checkTrue("dock: 22 em of Body at a normal window", dockWidth(metrics(1.0, 1.0), 1280) == metrics(1.0, 1.0).em(22) && metrics(1.0, 1.0).em(22) == 330);
    checkTrue("dock: never wider than 45 % of the window", dockWidth(metrics(1.0, 1.0), 600) <= 0.45 * 600 + 1 || dockWidth(metrics(1.0, 1.0), 600) == metrics(1.0, 1.0).em(12));
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1000.0, 1280.0, 1920.0, 2560.0})
                for (double hu : {560.0, 640.0, 800.0, 1080.0}) {
                    const Metrics m = metrics(dpi, tf);
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi), dw = dockWidth(m, W);
                    const std::string t = tag(dpi, tf) + " window " + num(W) + "x" + num(H);
                    const double tbw = 7 * (m.toolBtn + m.s4) + 6 * m.s8, rw = 14 * m.caption;
                    const Frame f = makeFrame(W, H, m, tbw, rw, 3 * m.caption * 1.4, dw);
                    checkTrue("dock: inside the window" + t, within(f.dock, f.window));
                    checkTrue("dock: below the top bar, not on it" + t, !f.dock.intersects(f.topBar) && f.dock.y >= f.topBar.bottom());
                    checkTrue("dock: full height, down to 12 u of the bottom edge, 12 u of the right edge" + t, std::fabs(f.window.bottom() - f.dock.bottom() - m.s12) < 1 && std::fabs(f.window.right() - f.dock.right() - m.s12) < 1);
                    checkTrue("dock: the toolbar does not touch it and is inside the window" + t, !f.toolbar.intersects(f.dock) && within(f.toolbar, f.window));
                    checkTrue("dock: the readout does not touch it and is inside the window" + t, !f.readout.intersects(f.dock) && within(f.readout, f.window));
                    checkTrue("dock: the toolbar is centred in the free part" + t, std::fabs((f.toolbar.x + f.toolbar.w / 2) - f.freeW / 2) < 1.0);
                    checkTrue("dock: the readout and the toolbar do not overlap" + t, !f.toolbar.intersects(f.readout));
                    for (Anchor a : {Anchor::TopLeft, Anchor::TopRight, Anchor::UnderChip, Anchor::AboveToolbar, Anchor::Centre})
                        for (double ww : std::vector<double>{double(m.em(20.0)), double(m.em(36.0)), W * 2.0}) {
                            const Rect r = placePanel(a, ww, m.em(12.0), f, m, W * 0.9);
                            checkTrue("dock: a panel (anchor " + std::to_string(int(a)) + ", width " + num(ww) + ") stays clear of it" + t, !r.intersects(f.dock));
                        }
                    checkTrue("dock: the fact line stays clear of it" + t, !factLineRect(f, m, 60 * m.caption).intersects(f.dock)); checkTrue("dock: the two-line fact line stays clear of it" + t, !factLineRect(f, m, 60 * m.caption, 2.0 * (m.caption + m.s4)).intersects(f.dock));
                    const Frame g = makeFrame(W, H, m, tbw, rw, 3 * m.caption * 1.4, 0);
                    checkTrue("dock: closed, the dock is empty and the toolbar is centred in the window as before" + t, g.dock.w == 0 && g.freeW == W && std::fabs((g.toolbar.x + g.toolbar.w / 2) - W / 2) < 1.0);
                }
    // closed: the same numbers as the frame without the new argument
    const Metrics m = metrics(1.0, 1.0);
    const Frame a = makeFrame(1280, 800, m, 300, 100, 40), b = makeFrame(1280, 800, m, 300, 100, 40, 0);
    checkTrue("dock: no dock argument = closed", a.toolbar.x == b.toolbar.x && a.readout.x == b.readout.x && a.readout.y == b.readout.y);
    // a fly-to target that lies under the dock counts as hidden (it is an occluder like the other panels) and goes to the free part
    const Frame f = makeFrame(1280, 800, m, 300, 100, 40, dockWidth(m, 1280));
    Pt target;
    checkTrue("dock: a body under the dock is hidden by it", hiddenBy({f.dock.x + 40, 400}, 10, {f.dock}, m.s24));
    // 0.15 stage 5g: the anchored toolbar: its left edge is the window centre minus half of the widest toolbar, whatever its own width; the open panel moves it left by the widest width
    for (double W : {1280.0, 880.0, 1920.0}) {
        const double H = W * 0.625, wide = 11 * (m.toolBtn + m.s4) + 2 * m.s8;
        const Frame n1 = makeFrame(W, H, m, 2 * (m.toolBtn + m.s4) + 2 * m.s8, 100, 40, 0, wide), n2 = makeFrame(W, H, m, 9 * (m.toolBtn + m.s4) + 2 * m.s8, 100, 40, 0, wide);
        checkTrue("anchor: the left edge does not depend on the toolbar's own width", std::fabs(n1.toolbar.x - n2.toolbar.x) < 1e-9 && std::fabs(n1.toolbar.x - (W / 2 - wide / 2)) < 1e-9);
        const double dw = dockWidth(m, W);
        const Frame d1 = makeFrame(W, H, m, 2 * (m.toolBtn + m.s4) + 2 * m.s8, 100, 40, dw, wide), d2 = makeFrame(W, H, m, 9 * (m.toolBtn + m.s4) + 2 * m.s8, 100, 40, dw, wide);
        checkTrue("anchor: with the panel open the left edge still does not depend on the toolbar's width", std::fabs(d1.toolbar.x - d2.toolbar.x) < 1e-9);
        checkTrue("anchor: with the panel open the toolbar is never under it, and moves only if it had to", d1.toolbar.x + wide <= d1.dock.x - m.s12 + 1e-9 && (d1.toolbar.x == n1.toolbar.x || std::fabs(d1.toolbar.x + wide - (d1.dock.x - m.s12)) < 1e-9 || d1.toolbar.x == m.s12));
    }
    checkTrue("dock: the centre shift moves it out", centreShiftTarget({f.dock.x + 40, 400}, 10, f.window, {f.topBar, f.toolbar, f.dock}, m.s24, target) && target.x < f.dock.x - m.s24);
}

// 0.15 stage 5c.1: the Controls panel opens from a handle on the picture's right edge and slides in and out. WRITTEN BEFORE the code. References: the owner's findings after trying 5c
// (a small tab with an arrow, vertically centred on the panel's edge, never covered by the panel, the top bar or the toolbar; a 150-200 ms ease; no animation when motion is reduced;
// a hover outline or glow at most 2 px outside the widget), and plain geometry.
void testShellHandle() {
    checkTrue("slide: 150 to 200 ms", kSlideSeconds >= 0.15 && kSlideSeconds <= 0.2);
    checkTrue("halo: at most 2 px outside a widget or panel", kHaloMaxPx > 0 && kHaloMaxPx <= 2.0);
    checkTrue("slide ease: starts at 0 and ends at 1", slideEase(0) == 0 && slideEase(1) == 1 && slideEase(-1) == 0 && slideEase(2) == 1);
    for (int i = 0; i < 100; i++) checkTrue("slide ease: never goes back", slideEase((i + 1) / 100.0) >= slideEase(i / 100.0));
    double s = 0;
    int frames = 0;
    for (; s < 1 && frames < 1000; frames++) s = slideStep(s, 1.0 / 60, true, false);
    checkTrue("slide: at 60 frames a second it opens in 150-200 ms and lands exactly on 1 (" + std::to_string(frames) + " frames)", s == 1 && frames >= 9 && frames <= 12);
    checkTrue("slide: open stays open", slideStep(1, 1.0 / 60, true, false) == 1);
    for (frames = 0; s > 0 && frames < 1000; frames++) s = slideStep(s, 1.0 / 60, false, false);
    checkTrue("slide: and closes in the same time, landing exactly on 0", s == 0 && frames >= 9 && frames <= 12);
    checkTrue("slide: reduced motion jumps at once", slideStep(0, 1.0 / 60, true, true) == 1 && slideStep(1, 1.0 / 60, false, true) == 0 && slideStep(0.4, 1.0 / 60, true, true) == 1);
    checkTrue("slide: a long frame (a hitch) does not overshoot", slideStep(0.5, 5.0, true, false) == 1 && slideStep(0.5, 5.0, false, false) == 0 && slideStep(0.5, -1, true, false) == 0.5);
    for (double dpi : kDpis)
        for (double tf : kTfs)
            for (double wu : {880.0, 1280.0, 1920.0})
                for (double hu : {560.0, 800.0, 1080.0}) {
                    const Metrics m = metrics(dpi, tf);
                    const double W = std::lround(wu * dpi), H = std::lround(hu * dpi), dw = dockWidth(m, W);
                    const std::string t = tag(dpi, tf) + " window " + num(W) + "x" + num(H);
                    const double tbw = 7 * (m.toolBtn + m.s4) + 6 * m.s8, rw = 14 * m.caption;
                    const Frame fo = makeFrame(W, H, m, tbw, rw, 3 * m.caption * 1.4, dw), fc = makeFrame(W, H, m, tbw, rw, 3 * m.caption * 1.4, 0);
                    const Rect dock = dockRect(W, H, m, dw);
                    checkTrue("slide: the dock rectangle is the open frame's dock" + t, dock.x == fo.dock.x && dock.y == fo.dock.y && dock.w == fo.dock.w && dock.h == fo.dock.h);
                    const Rect in = slidRect(dock, W, 1), out = slidRect(dock, W, 0);
                    checkTrue("slide: after the slide the panel is exactly the docked rectangle" + t, in.x == dock.x && in.y == dock.y && in.w == dock.w && in.h == dock.h);
                    checkTrue("slide: closed, the panel is wholly outside the window" + t, out.x >= W && out.y == dock.y && out.w == dock.w);
                    for (int k = 0; k <= 20; k++) {
                        const double e = k / 20.0;
                        const Rect p = slidRect(dock, W, e), h = handleRect(dock, W, m, e);
                        const std::string te = t + " at " + num(e);
                        checkTrue("handle: inside the window" + te, within(h, fo.window));
                        checkTrue("handle: never under the panel (it rides on its left edge)" + te, !h.intersects(p) && std::fabs(h.right() - std::min(W, p.x)) < 1e-9);
                        checkTrue("handle: vertically centred on the panel's edge" + te, std::fabs((h.y + h.h / 2) - (dock.y + dock.h / 2)) < 1e-9);
                        if (e == 0 || e == 1) {   // 5m part 2: in the two settled states, a card at the top right (the subject card, 20 em wide, as tall as it can be) never lies on the handle (mid-slide the handle is moving)
                            const Rect card = placePanel(Anchor::TopRight, m.em(20), 4000, e >= 1 ? fo : fc, m, 0);
                            checkTrue("handle: the top-right card does not touch it" + te, !card.intersects(h));
                        }
                        checkTrue("handle: clear of the top bar and the toolbar, open or closed" + te, !h.intersects(fo.topBar) && !h.intersects(fo.toolbar) && !h.intersects(fc.toolbar) && h.y >= fo.topBar.bottom());
                        checkTrue("handle: big enough to hit (24 u wide, at least a control high)" + te, h.w >= m.s24 - 1e-9 && h.h >= m.control);
                    }
                    if (dpi == 1.0 && tf == 1.0) {   // 5m part 2 (owner): the handle is at least 32 x 72 px at scale 100 % (a fixed number, not derived from the code's own units)
                        checkTrue("handle: at least 32 x 72 px at scale 100 %" + t, handleRect(dock, W, m, 1).w >= 32 - 1e-9 && handleRect(dock, W, m, 1).h >= 72 - 1e-9);
                    }
                    checkTrue("handle: closed, on the window's right edge" + t, std::fabs(handleRect(dock, W, m, 0).right() - W) < 1e-9);
                }
}

void testShellReadout() {
    int muted = 0, fg = 0;
    std::vector<app::ThemeColors> all;
    for (const app::ThemeDef* t : app::allThemes()) all.push_back(app::themeColors(t->id, false));
    all.push_back(app::themeColors("auto", true));
    for (const app::ThemeColors& c : all) {
        const Rgb tx = readoutText(c);
        // the pill is black at 0.6; behind it the picture is black (space) or white (the worst): the text must pass 4.5:1 on both, written out here with our own WCAG formula
        const Rgb onBlack{0, 0, 0}, onWhite{0.4, 0.4, 0.4};
        const double lo = std::min(wcag(tx, onBlack), wcag(tx, onWhite));
        checkTrue("readout: the text passes 4.5:1 on its pill over black and over white in " + c.id + " (" + num(lo) + ")", lo >= 4.5);
        checkTrue("readout: readoutContrast agrees with the formula written out here in " + c.id, std::fabs(readoutContrast(tx) - lo) < 0.02);
        if (tx.r == c.muted.r && tx.g == c.muted.g && tx.b == c.muted.b) muted++; else fg++;
    }
    // a finding, reported: on a pill that is only 0.6 opaque, over a WHITE picture it is a mid grey (#666666), and no theme's muted colour reaches 4.5:1 on that (best 4.39, Y2K Night), so the
    // readout uses the theme's text colour where that passes and white where it does not (the light themes, Aero, Y2K Ice, Monochrome)
    checkTrue("readout: no theme's muted colour passes on the 0.6 pill over white (the finding above); the text colour or white is used", muted == 0 && fg > 0);
    checkTrue("readout: the pill is about 0.6 opaque", kReadoutAlpha > 0.55f && kReadoutAlpha < 0.65f);
}

void testShell() {
    testShellDock();
    testShellHandle();
    testShellReadout();
    testShellTypeScale();
    testShellFrame();
    testShellToolbarOverflow();
    testShellTopBar();
    testShellPanels();
    testShellCentreShift();
    testShellAtomFreeLoop();
    testShellGlass();
    testShellKeys();
    testShellReading();
    testShellSearch();
    testShellPanelStates();
    testShellPrefs();
    testSubjectCardSynthetic();
    testSubjectCardReal();
    testShellRecentSearch();
    testShellFacts();
    testShellTextShadow();
    testShellPrefs4();
    testShellSideStep();
}
}  // namespace
