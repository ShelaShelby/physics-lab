# Native Physics Lab 0.14 (alpha) — stage 1.1: Frutiger Aero floating windows were unreadable (fix before stage 2)

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push; the Actions result is in the closing message.
**NOT TESTED: a real PC, real GPU, real Segoe UI, speed.** Pictures are Mesa llvmpipe, headless (the real app on Linux, SDL offscreen).

## What the owner saw, and the cause (reproduced first, in Aero)
1. Guide ("What am I even looking at?") and changelog windows see-through; the Learn cards and the picture showed through the text.
2. The "Solar System … Nothing is selected yet" card inside the guide, the tip bar text colliding near the window title.
**One cause, not three.** Stage 1 set every window background to transparent in glass themes and painted the glass only in windows that called `panelDecor`. Eight windows never did: the Orbit and Atom guides (`learn.cpp`, `atom_learn.cpp`), the changelog, the two benchmark windows, the benchmark run box and the two "put back" pop-ups. Transparent and unpainted = everything behind showed. The "Solar System" card is the guide's OWN card (it shows the selected body), not a layer bug: nothing in the app draws on Dear ImGui's foreground layer (searched; the picture's text is on the background layer, under all windows), and a window covers it once it is solid. The tip bar line is under the window for the same reason. The stage 1 contrast test modelled glass over the page gradient only; it could not see an unpainted window.

## Fix (decision and reason in DECISIONS)
Frosted, not clear: every window is solid by default again (`WindowBg` opaque); only the docked panels (header, side panels, tab page) are see-through (`pushDockedBg`, behind them there is only the page). A floating window paints the page gradient (opaque) and the glass (alpha 0.94) over it; a pop-up list is one solid colour. The glass look is the gradient, shine, rim and shadow. **No blur:** needs a frame copy and a second pass on a renderer that draws in layers, not testable on a real GPU here. All eight windows now call `panelDecor` and get the rounded floating style. Also fixed on the way: the painted glass covered the scroll bars (they vanished in the guide); it now stops before them.

## Tests written FIRST (red before the fix, green after)
- `test_theme_style.inc` (+178 checks, app_tests 2,014 → **2,192**): for all 12 themes, a floating window and a pop-up show the same surface over 12 different backdrops (black, white, the theme's own colours): leak 0; stack ≥ 94 % solid, pop-up solid; text 4.5:1 over every backdrop (Aero: also muted, accent, error); the glass alone over any backdrop still reads (real alpha); docked surface = glass over the page.
- `tests/check_windows_decorated.py` (ctest `windows_decorated`, needs no graphics): every `Begin` / `BeginPopup` / `BeginPopupModal` in `src` must call `panelDecor`; every docked one must `pushDockedBg`. Red: **13 problems** before, 0 after (20 windows).
- `tests/check_window_pictures.py` (ctest `window_pictures`, `-DPL_PICTURE_TESTS=ON`): Aero, Dark, Light; the guide and the changelog over the Orbit picture against over the Learn cards; Settings over the Earth card against no card (layering); the side panel after 40 against 70 frames. Inside of the window, inset 14 px. Tolerance stated: ≤ 0.03 % of pixels, none over 40/255. Measured 8 to 15 pixels, worst 29: one letter's anti-aliased edge. (My first layering test used the guide and failed on purpose-free grounds: the guide shows the selected body in its own card. Changed to Settings, said here.)
- Old themes: `theme_pictures` (72 pictures vs the 0.13 code, 60 identical + picker vs its 0.14 file) passes, **0 differing pixels; the fix changes no old picture**, so there is nothing to list.

## Perturbed, caught, reverted
Stage 1 bug recreated (transparent background, window not painted): Aero guide **136,223** of 295,152 pixels differ, changelog 199,669, layering 41,954; Dark and Light unchanged. Overlay card drawn on the foreground layer: layering fails in all 3 themes (≈52,300 pixels). Aero floating alpha 0.5: 10 fails. Glass lets the backdrop through: leak 0.06 caught. Pop-up not solid: 11 fails. One `panelDecor` removed from `changelog.cpp`: the scan names it. (One of my perturbations did not compile, so it tested nothing; redone and caught.)

## Looked at (Aero, before and after)
Guide over Learn, guide over the Orbit picture, changelog over Learn, Settings: before, titles, card icons and picture labels ran through the paragraphs; after, the text reads cleanly on a plain near-white window with scroll bar, the Solar System card sits inside the guide's own text, the tip bar is hidden under the window. Honest limit: the windows are now plain light panels; the Aero feel there is the gradient edge, rounded corners, shadow and the green / aqua buttons, not a see-through glass.

## Found, not changed (report rule)
Light theme: accent text #2f6fed on its page colour #f7f7f5 is **4.24:1** (4.55 on its card): just under 4.5 for body-size accent text ("Did you know?" label). All other old themes pass 4.5 for text, muted and accent on bg and card (computed by hand). No existing test loosened or changed; no physics touched; `PL_PANE` and `PL_LOG_WINDOWS` are test switches.

## Tests run
orbit 1,612, atom 21,066, app **2,192**, nbody 37, jpl 55, moon 24, moon_real 59, GL 503, glslang pass, `windows_decorated`, `window_pictures`, `theme_pictures`: all pass.

## Not tested / limits
Windows/MSVC build (Actions only); benchmark windows and the two "put back" pop-ups in Aero have no picture, only the source scan; combo lists are solid but flat; pop-up shadows and shine only on windows that call `panelDecor`.
