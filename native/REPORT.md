# Native Physics Lab 0.14 (alpha) — stage 3 (FINAL): Theme effects switch and wrap-up

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push to `native`, no PR. 0.14 ends here by the owner's decision.
**NOT TESTED: a real PC, real GPU, real Segoe UI, speed.** Pictures are Mesa llvmpipe, headless (the real app on Linux, SDL offscreen).
**Tests were written first** (`tests/test_theme_stage3.inc`: compile-red, then green; `tests/check_effects_pictures.py`).

## What changed
- **Settings → Theme effects: On / Off** (saved as `effects=on|off`, default On; tiles after Motion, with a note). Off strips, in one function on the style data (`app::withoutEffects`, applied in `ui/look.cpp`): page textures (clouds, bubbles, grain, halftone, grid, streak), glints, brush lines, the reflection, corner brackets, HUD rules, the spec label. Colours, gloss gradients, rim, glow, shine, shadow, title bars and fonts stay. Test switch `PL_EFFECTS=off`. The benchmark report prints `Theme effects: on/off`.
- **Docs:** ROADMAP (0.14 complete in 3 stages; glass/chrome planets and nucleons, glossy chart tiles, theme-aware Cloud dots and animated backgrounds listed as DROPPED by the owner; the 0.15 "Full UI rework" entry with the owner's problems, the MyAnimeList → AniList target and the design-system-and-mockups start; "Later": the mascot), RESUME (0.14 complete), README, DECISIONS, OWNER-TODO, one summary entry at the top of the changelog (v0.14 history).

## Tests
- app_tests **3,036** (was 2,855): the setting (default On, saved, loaded, old file, bad value, benchmark line); for all 13 themes Off has none of the decoration and leaves the page gradient, gloss, rim, glow, title bar, fonts, button colour exactly; Aero, Ice and Night: with Off the page is the plain gradient at every height, and the whole stage 2 contrast set (page, panels, buttons, title bars, borders) passes again with Off.
- `effects_pictures` (new): Off = the header margin is the plain gradient (second difference 1 of 255; with On it is 22 and 29 for Ice and Night, so the test can tell); Aero's clouds and bubbles change 57,774 header pixels; the corner bracket arm is a line with On (217 / 175 levels) and the gradient with Off; the inside of a floating window is the same On and Off (0 to 6 of 282,568 pixels, worst 1/255); hover glint and brush lines gone; the saved setting draws the same picture as the switch; Dark and Light: 0 differing pixels On vs Off.
- **Effects On = stage 2, exactly:** against the stage 2 program, Aero / Ice / Night orbit, nucleus, Cloud 3D, chart, picker and guide: 0 differing pixels (checked pair by pair; 15 hashes of the stage 2 program are now a guard in `theme_pictures_0.14s2_native.sha256`).
- **72-picture test:** the 40 pictures (Orbit, nucleus, Cloud 3D, chart) of the 10 unchanged themes: 0 differing pixels vs 0.13. Listed as changed (all in `theme_pictures_0.14_changed.sha256`, made by this code, a regression guard not an independent reference): Light and Auto-in-light (stage 2: the darker accent); the picker (stage 2: two cards); **new in stage 3: the Settings screen of all 12 variants**, because it has a new section and the scroll bar thumb is shorter. Checked against the stage 2 program for Dark, Aero, Ice-night: differs only in x 727–736, y 344–432 (the scroll bar), 857–859 pixels. So "0 differing pixels" holds for everything but these named files.
- Other tests unchanged and passing (ctest below): orbit 1,612, atom 21,066, nbody 37, jpl 55, moon 24, moon_real 59, GL 503, glslang, windows_decorated, window_pictures, y2k_pictures.

## Perturbed, caught, reverted
1. Brackets, HUD and spec label not stripped: unit test fails (3 checks); my first picture check did NOT catch it (a bracket line is uniform along its row), so I changed it to compare the arm row with the rows below, and it then fails for Ice and Night on the perturbed program (217 vs 217, 175 vs 175).
2. Grid not stripped: 4 unit fails and the picture test fails (margin 22 and 29 > 2; hover glint check). All reverted; the clean build passes.

## Limits
Speed of the effects was never measured (OWNER-TODO asks the owner to compare On and Off on a slow PC). The GitHub connector was down for a while in this session; the Actions result is in the closing message if it could be read. No physics touched; no existing test loosened (one test file got a parameter, `checkLook(id, effectsOff)`, to run the same rules with Off).
- **Final diff against `origin/native`: 26 files, +302 / −43** (no build output, no pictures, no generated data).
