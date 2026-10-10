# Native Physics Lab 0.15 stage 5p part 1: wheel after About (#6), About scroll (#2), Atom zoom-out (#3)

Fixes only, no physics or value changed, `index.html` untouched. Part 2 (#1, #4, #5) is not done yet; this file is rewritten at its end.

## Changed
- #6 CAUSE FOUND. `ui/reading.cpp`: `g.shown` ("a page is open") was set when a page was drawn and cleared only by `setReadingMode(false)`, which the New UI never calls. So after the first page (About, History, log) was closed, `readingShown()` stayed true and `main.cpp` dropped every wheel (and chart arrow-key) event meant for the picture, in both labs, until a restart. Now it only counts when the page was drawn in the frame the app is in (`pageDrawnNow`). The same fix covers the "pscroll" test readout.
- #2 What already worked (5k, tested): wheel, PageUp, PageDown, Home, End scroll About. Missing: a visible bar. `readingScrollbar`: 12 px wide at 100 % (was 8) with a faint track; hit area 24 px; still draggable.
- #3 `atom/atom_sim.h`: `kZoomMin = -log10(2)` (the picture scale 1/2; was 0), used by `setParam`, `clampZoom`, the saved-settings reader and both zoom sliders (label shows ×0.5 below 1). Chart: `kChartZoomMin = 0.5` (was 1), centred, no pan. Nucleus and Nucleon are zoom presets inside the Atom view, not separate limits; they zoom in and are unchanged. The zoom-in limit (10^7, chart 16) is unchanged.
- Tests changed ON PURPOSE: `atom_tests` "landscape zoom ... 1 at least" now says 1/2 at least. `shell_pictures_0.15s4.sha256`: the 4 About lines (looked at: only the scrollbar changed).

## Tested (cloud, Linux, Mesa llvmpipe, scripted events)
- New `tests/check_5p.py`: `ui5p_wheel` (baseline wheel, then About by click on "Physics Lab" and by Esc, History, log, Learn, Settings, theme, search, Labs menu: open, close, wheel; zoom must change exactly like the baseline; Orbit and Atom; 38 checks), `ui5p_scroll` (wheel, PageDown, PageUp, End, Home, drag, bar >= 12 px, inside page and window; 1280x800 and 880x560; 24 checks), `ui5p_zoomout` (5 views: many notches out stop at -0.30103 / chart 0.5, more notches do not go further, one notch in leaves; centre, label, ring in the window; 33 checks).
- Before the fix, kept: `ui5p_wheel` failed 16 of 38 (the owner's case, both labs: zoom -1.2 -> -1.2 against baseline -1.2 -> -0.99; after closing the app still said a page was shown); `ui5p_zoomout` failed 11 of 33 (zoom stayed 0, chart 1). I did not undo the fixes a second time: the pre-fix run is the same code. #2: bar width undone once: failed (8.0 px), restored.
- Another window type also failed: History and the log (same cause, same fix). Settings, Learn, theme, search, Labs menu never failed.
- Picture looked at: Cloud 3D, hydrogen, zoom at the new limit, 1280x800 (cloud half size, label at the nucleus, 5 Å bar); About 1280x800 (visible bar and track).
- Also run and passed: atom_tests (21,191 checks, 0 failed), ui5k_pages, ui5k_log, ui5k_windows, ui5k_toggle, atom_panel, controls_panel, windows_decorated, check.sh quick. shell_pictures: only the 4 About pictures differed (updated).
- Seen, not changed: one End press on About stops about 1 % above the bottom (the page grows while its bottom is first drawn); End again reaches it.

## NOT tested
Windows behaviour, a real mouse, a real GPU, speed, 125 / 150 % scale. The Windows run and release of this push are in my last message. Full suite: once at the end of part 2.
