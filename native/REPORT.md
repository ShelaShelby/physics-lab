# Native Physics Lab 0.14 (alpha) — stage 2: Aero's real look, Light's accent, Y2K Ice and Y2K Night

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push to `native`, no PR; the Actions result is in the closing message.
**NOT TESTED: a real PC, real GPU, real Segoe UI, speed.** Pictures are Mesa llvmpipe, headless (the real app on Linux, SDL offscreen). The owner judges the look.
**Tests were written first** (`tests/test_theme_stage2.inc`: link-red, then run-red, then green).

## What changed
- **Aero (A):** saturated page (sky blue → aqua #19b5d6 → leaf green), aqua jelly buttons, lime jelly chosen tabs (highlight over the top 46 %, deep colour below, darker rim, hover glow), deep-blue glossy title bars (shine band, aqua line), bright rim + aqua glow, faint reflection, still clouds and bubbles, glossy combo boxes/lists and tooltips (`ui::BeginCombo/Combo/SetTooltip`; a source scan forbids the plain ones). Muted text and a few colours darkened so every pair passes.
- **Light (B):** accent text #2f6fed (4.24:1 on #f7f7f5) → #2d6ae3 (4.57 on the page, same hue, 220°). Web theme list untouched (`ThemeStyle::accent`).
- **Y2K Ice / Night (C):** icy blue + silver / near-black; grain, halftone, grid (each ≤ 12 %), Night's violet streak; corner brackets, HUD rules, "SYS 0.14" label in the title bar; chrome buttons (highlight line, rim, glint, brush lines, glow on the chosen tab); Michroma (SIL OFL, 64 KB .ttf + licence, built in) for window titles and the app name only. New style fields are data with defaults = old look.
- Test switches: `PL_MOUSE`, `PL_OPEN_COMBO`, `PL_FX`, `PL_DECOR`, `PL_TITLEFONT`.

## Tests
- app_tests 2,855 (was 2,192). Contrast over page textures at their extremes (page moved by the sum of the textures, and by the streak), docked/floating/pop-up surfaces, every button state with glint and brush lines, chosen and red buttons, title bars, spec label, borders; hue rules (Aero aqua/lime; Y2K cold blue only, violet only as streak); Light accent; font file + licence + About; title/texture maths by hand.
- Changed on purpose (not loosened): theme count 12 → 14 and picker order; Aero's old "page lightest > 0.75" (a pale-design rule the owner rejected) → "> 0.3". New rules for Y2K: Ice page L > 0.3, textures ≤ 12 % each and ≤ 32 % together (my numbers).
- `windows_decorated` (+ title-bar and wrapper rules), `window_pictures` (now Aero, Y2K Ice, Y2K Night, Dark, Light), new `y2k_pictures`: decoration only in the 7 pt edge band and title bars; window inside identical with the title font off (7 of 282,568 pixels, worst 1/255: a letter edge, same tolerance as 1.1); textures change at most 32/255 (Ice, bound 79) and 75/255 (Night, bound 156); glow over the dark picture 61–114 vs 8–9 for Dark's none; hover + tooltip draw.
- **Pictures vs 0.13:** 50 of 50 pictures of auto, black, midnight, solar, paper, sage, contrast, dark, synthwave, monochrome identical (0 differing pixels). Changed on purpose: Light and Auto-in-light, 5 screens each: 1–6 % of pixels, all darker, ≤ 10/255 (the accent); the theme picker in all 12 variants (two new cards). Their new hashes are in `theme_pictures_0.14s2_changed.sha256` (made by this code after looking: a regression guard, not an independent reference).
- Other tests: orbit 1,612, atom 21,066, nbody 37, jpl 55, moon 24, moon_real 59, GL 503, glslang: unchanged and passing (ctest result below).

## Perturbed, caught, reverted
Aero page stops swapped: **not caught at first** (tests only looked for aqua/lime anywhere) → added "sky at the top, leaf at the bottom", then caught. Gloss top/bottom swapped: 4+ fails. Night jelly highlight 0.06 → 0.6: 3 fails (text 1.7:1). Aero title shine 0.18 → 0.7: title text 1.6:1. Rim alpha 0: rim test. Light accent back to #2f6fed: 4 fails. All reverted.

## LOOKED AT (Aero before/after, Y2K, guide, settings, combo, chart; one look each)
- **Aero:** now saturated and glossy: aqua/lime jelly, blue title bars with glow, visible clouds, page stays coloured to the bottom. Honest limits: docked panels are still light glass (about 36 % white over the page), so the header reads light aqua, not deep teal; the clouds are soft and quiet; it is "glossy Aero", not the full photo collage (no water, leaves or globes: no artwork on purpose).
- **Y2K Ice:** icy blue panels, silver buttons, grid and dots only faintly visible under the glass: reads as a clean cold light theme more than an overexposed poster. **Y2K Night:** navy, violet streak and dots at the top right, icy rim and chrome-ish buttons; reads as cold techno but not as bright chrome (text contrast limits the gloss). Neither is "iridescent". Owner to judge; OWNER-TODO has it.

## Found / limits
- A bad automated edit made `style_draw.cpp` 650,000 lines for a while (found by the diff-size check, fixed before pushing). Final diff against `origin/native`: see below (largest files: tests, `theme_style.cpp`, `style_draw.cpp`, the Michroma licence text).
- Not tested: Windows/MSVC build (Actions only); benchmark windows and "put back" pop-ups in the new themes (source scan only); tooltips only at one place; speed of the page textures (many small shapes per frame; no off switch until stage 5).
- Heading icons in the Settings window render as tiny marks in all themes (unchanged). No physics touched; no existing test changed except as listed.
- **Final diff against `origin/native`: 46 files, +1,252 / −146** (no build output, no pictures, no generated data; the font is the 64 KB .ttf plus its licence, embedded at build time; `.gitignore` covers build folders and .bmp).
