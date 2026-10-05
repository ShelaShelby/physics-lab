// 0.15 stage 2, C3 and C4: power throttling request, the "window lost focus" flag of the benchmark, and the display scale / minimum window size.
// WRITTEN BEFORE the code (compile-red first). There is no Windows in the cloud session: the Windows call is replaced by a STAND-IN function that records what it was
// asked; what Windows does with it is NOT tested here. References: the rules of the brief (never ask for throttling; flag a scene that lost focus; a scale change
// resizes text and the minimum window), and plain arithmetic.
#include "app/display_scale.h"
#include "app/power.h"

static void testPowerRequest() {
    const app::PowerThrottlingRequest r = app::powerThrottlingOff();
    checkTrue("power request: it names a version", r.version >= 1);
    checkTrue("power request: it controls at least the execution speed bit", r.controlMask != 0);
    checkTrue("power request: state 0 = throttling OFF for every bit it controls (never ON)", r.stateMask == 0);
    checkTrue("power request: the state has no bit outside the control mask", (r.stateMask & ~r.controlMask) == 0);
    int calls = 0; app::PowerThrottlingRequest seen{};
    const bool ok = app::disablePowerThrottling([&](const app::PowerThrottlingRequest& q) { calls++; seen = q; return true; });
    checkTrue("power request: the setter is called once with that request", ok && calls == 1 && seen.version == r.version && seen.controlMask == r.controlMask && seen.stateMask == 0);
    checkTrue("power request: a refusing system reports false", !app::disablePowerThrottling([](const app::PowerThrottlingRequest&) { return false; }));
    checkTrue("power request: no setter (not Windows) is false and does not crash", !app::disablePowerThrottling(nullptr));
    checkStr("power note when accepted", app::powerNote(true, true), "Windows power throttling: switched off for this app (accepted)");
    checkStr("power note when refused", app::powerNote(true, false), "Windows power throttling: switch-off was asked and refused");
    checkStr("power note when not Windows", app::powerNote(false, false), "Windows power throttling: not applicable (not Windows)");
}

static void testBenchFocusFlag() {
    using namespace app;
    BenchRun run(3, 2.0, 8.0);
    // drives scene k to its end; at frame `flagAt` (or never: -1) the window reports `state` (0 = lost focus, 1 = minimised)
    const auto driveScene = [&](int k, int flagAt, int state) {
        if (run.sceneToSetUp() >= 0) run.setUpDone();
        int n = 0;
        while (!run.finished() && run.scene() == k && n < 1000) {
            const bool flag = n == flagAt;
            run.noteWindow(!(flag && state == 0), flag && state == 1);
            run.frame(100, 5);
            n++;
        }
    };
    driveScene(0, -1, 0);
    checkTrue("focus flag: scene 0 stayed focused -> not flagged", !run.result(0).windowInactive);
    driveScene(1, 50, 0);                                                                             // the window loses focus once in scene 1
    checkTrue("focus flag: scene 1 lost focus once -> flagged", run.result(1).windowInactive);
    checkTrue("focus flag: scene 0 is NOT flagged by a later scene", !run.result(0).windowInactive);
    driveScene(2, 20, 1);                                                                             // minimised once in scene 2
    checkTrue("focus flag: scene 2 minimised once -> flagged", run.result(2).windowInactive && run.finished());
    checkTrue("focus flag: scene 1 and 2 flagged, scene 0 not", !run.result(0).windowInactive && run.result(1).windowInactive && run.result(2).windowInactive);
    // a flag during the WARM-UP counts too (frame 5 of 20 warm-up frames)
    BenchRun warm(2, 2.0, 8.0);
    warm.setUpDone();
    for (int n = 0; n < 5; n++) { warm.noteWindow(true, false); warm.frame(100, 5); }
    warm.noteWindow(false, false); warm.frame(100, 5);
    checkTrue("focus flag: lost in the warm-up (not counted frames) -> still flagged", warm.result(0).windowInactive);
    // a run that is not running (before the set-up is done) flags nothing
    BenchRun idle(2);
    idle.noteWindow(false, true);
    checkTrue("focus flag: nothing is flagged before the first scene is set up", !idle.result(0).windowInactive);
    // the report says so: the warning only for flagged scenes, and a count at the top
    BenchEnv env; env.version = "0.15"; env.when = "t";
    std::vector<BenchScene> scenes(2);
    scenes[0].name = "1. Orbit Lab: A"; scenes[1].name = "2. Orbit Lab: B"; scenes[1].windowInactive = true;
    scenes[0].frames = benchStats({10, 10}); scenes[1].frames = benchStats({10, 10});
    const std::string txt = benchResultText(env, scenes);
    checkTrue("report: the flagged scene has the warning", txt.find("2. Orbit Lab: B") != std::string::npos && txt.find("lost focus or was minimised") != std::string::npos);
    std::size_t n = 0, at = 0; while ((at = txt.find("lost focus or was minimised", at)) != std::string::npos) { n++; at++; }
    checkTrue("report: the warning appears for the flagged scene only (once per scene, plus one summary line)", n == 2);
    std::vector<BenchScene> clean(2); clean[0].frames = benchStats({10}); clean[1].frames = benchStats({10});
    checkTrue("report: no warning when no scene was flagged", benchResultText(env, clean).find("lost focus") == std::string::npos);
    env.power = "Windows power throttling: switched off for this app (accepted)";
    checkTrue("report: the power note is printed", benchResultText(env, clean).find(env.power) != std::string::npos);
}

static void testDisplayScale() {
    using namespace app;
    // sanitising: a scale the system does not give properly is 1; absurd values are clamped to 0.5 .. 4
    checkTrue("scale: 0 and negative are 1", sanitizeDisplayScale(0) == 1.0f && sanitizeDisplayScale(-2) == 1.0f);
    checkTrue("scale: NaN is 1", sanitizeDisplayScale(std::nanf("")) == 1.0f);
    checkTrue("scale: normal values pass", sanitizeDisplayScale(1.25f) == 1.25f && sanitizeDisplayScale(1.5f) == 1.5f && sanitizeDisplayScale(1.0f) == 1.0f);
    checkTrue("scale: clamped to 0.5 .. 4", sanitizeDisplayScale(0.1f) == 0.5f && sanitizeDisplayScale(9) == 4.0f);
    // the minimum window: 880 x 560 units times the scale (DESIGN.md section 5)
    int w = 0, h = 0;
    minimumWindowSize(1.0f, w, h); checkTrue("minimum window at 100 %: 880 x 560", w == 880 && h == 560);
    minimumWindowSize(1.5f, w, h); checkTrue("minimum window at 150 %: 1320 x 840", w == 1320 && h == 840);
    minimumWindowSize(1.25f, w, h); checkTrue("minimum window at 125 %: 1100 x 700", w == 1100 && h == 700);
    // the side panel is 360 units wide, times the scale
    checkTrue("panel width follows the scale", panelWidthFor(1.0f) == 360.0f && panelWidthFor(1.5f) == 540.0f && panelWidthFor(2.0f) == 720.0f);
    // the stand-in for SDL's display-scale events: the first reading, an event that changes the scale, one that repeats it, a bad one
    DisplayScale ds(1.0f);
    checkTrue("tracker starts at its first reading", ds.scale() == 1.0f && ds.changes() == 0);
    checkTrue("tracker: 1.0 again is no change", !ds.update(1.0f) && ds.changes() == 0);
    checkTrue("tracker: the window moves to a 150 % monitor -> changed", ds.update(1.5f) && ds.scale() == 1.5f && ds.changes() == 1);
    int mw = 0, mh = 0; ds.minimumWindow(mw, mh);
    checkTrue("tracker: the minimum window follows (1320 x 840)", mw == 1320 && mh == 840);
    checkTrue("tracker: a tiny wobble (0.0004) is no change", !ds.update(1.5004f) && ds.scale() == 1.5f);
    checkTrue("tracker: a bad reading (0) is read as 1.0 and counts as a change from 1.5", ds.update(0.0f) && ds.scale() == 1.0f);
    checkTrue("tracker: back to 125 %", ds.update(1.25f) && ds.scale() == 1.25f && ds.changes() == 3);
    // the text size in pixels follows: 15 u body at 100 / 125 / 150 % and Text size Small (0.9, floor 14), Normal, Large, Extra large (DESIGN.md section 1)
    checkTrue("text px: 15 u at 100 % Normal = 15", scaledTextPx(15, 14, 1.0, 1.0) == 15);
    checkTrue("text px: Small at 100 % is floored to 14 (13.5 -> 14)", scaledTextPx(15, 14, 1.0, 0.9) == 14);
    checkTrue("text px: 125 % Normal = 19 (18.75)", scaledTextPx(15, 14, 1.25, 1.0) == 19);
    checkTrue("text px: 150 % Normal = 23 (22.5 rounds up)", scaledTextPx(15, 14, 1.5, 1.0) == 23);
    checkTrue("text px: 100 % Large = 17 (17.25), Extra large = 20 (19.5)", scaledTextPx(15, 14, 1.0, 1.15) == 17 && scaledTextPx(15, 14, 1.0, 1.3) == 20);
}

// ---- C2: the page effects cache (src/app/page_cache.h; the GL part is src/ui/page_cache.cpp and is checked by tests/check_page_cache.py) ----
#include "app/page_cache.h"
#include "app/theme_style.h"

static void testPageCacheLogic() {
    using namespace app;
    // which themes use the cache: exactly those whose page has textures (Aero's clouds and bubbles, Y2K Ice and Night), and only with Theme effects On
    int cached = 0;
    for (const ThemeDef* t : allThemes()) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeStyle full = themeColors(id, false).style;
        const bool want = id == "aero" || id == "y2kice" || id == "y2knight";
        checkTrue("page cache is used for " + id + " exactly when it has page textures", pageNeedsCache(full.page) == want);
        checkTrue("page cache is never used with Theme effects Off: " + id, !pageNeedsCache(withoutEffects(full).page));
        if (pageNeedsCache(full.page)) cached++;
    }
    checkTrue("three themes use the cache", cached == 3);
    // a page with textures but no gradient under it (kind 0) is not cached: the cached picture would carry transparent pixels
    PageStyle none; none.kind = 0; none.stops = 0; none.fx.grain = 0.05f;
    checkTrue("no gradient, no cache", !pageNeedsCache(none));
    // the key: the texture is rebuilt when (and only when) one of its inputs changes
    PageCacheTracker tr;
    PageCacheKey k; k.theme = "aero"; k.effects = true; k.w = 1000; k.h = 640; k.fontPx100 = 1600;
    checkTrue("first use: build", tr.update(k) && tr.rebuilds() == 1);
    for (int i = 0; i < 1000; i++) tr.update(k);
    checkTrue("1000 more frames with the same key: no rebuild", tr.rebuilds() == 1);
    PageCacheKey r = k; r.w = 1001;
    checkTrue("resize (width): rebuild", tr.update(r) && tr.rebuilds() == 2);
    r.h = 641;
    checkTrue("resize (height): rebuild", tr.update(r) && tr.rebuilds() == 3);
    r.theme = "y2knight";
    checkTrue("theme change: rebuild", tr.update(r) && tr.rebuilds() == 4);
    r.effects = false;
    checkTrue("effects switched off: rebuild (the texture is then dropped)", tr.update(r) && tr.rebuilds() == 5);
    r.effects = true;
    checkTrue("effects switched on again: rebuild", tr.update(r) && tr.rebuilds() == 6);
    r.fontPx100 = 2000;
    checkTrue("text size / display scale (the textures are sized in text units): rebuild", tr.update(r) && tr.rebuilds() == 7);
    checkTrue("the same key again: no rebuild", !tr.update(r) && tr.rebuilds() == 7);
    tr.invalidate();
    checkTrue("invalidate (a lost GL context): the next use rebuilds", tr.update(r) && tr.rebuilds() == 8);
}
