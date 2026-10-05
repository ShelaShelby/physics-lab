# Native Physics Lab 0.14 (alpha) — stage 1 of 5: richer theme styles + Frutiger Aero

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push; the Actions result is in the closing message.
**NOT TESTED: the look on a real PC (Windows, real GPU, real Segoe UI) and speed.** The owner judges the look (OWNER-TODO).

## Tests written FIRST (`tests/test_theme_style.inc`, compile-red before `theme_style.*` existed)
WCAG 2.x formula against values worked by hand (black/white 21:1, #777/white 4.478:1, #767676 4.54, red 4.00, blue 8.59, mid grey L 0.2159, the 0.04045 linear part); gradients by hand; theme data (all 12 themes have every field in range; the 11 old themes are "plain" = old 7-colour look, old numbers 0.90/0.35/0.30, #d0453b, 120/140/200, 128 kept); gloss colours (top lighter, pressed turns over, plain = flat); panel colours; font fallback; corner scale. app_tests 1,616 → **2,014**.
**Aero contrast (computed from the theme's real data):** text, muted, accent and error text on docked glass and floating windows over the page at 101 heights, plus floating over pure white and black; button text and muted text in all 3 states; green and red button text; highlighted rows and title bars; borders 3:1, check mark, slider grab. Worst body pair ≥ 4.5.
**Perturbed, caught, reverted (12):** panel alpha default 0.9 (21 fails); floating alpha 0.97 (20); gloss top/bottom swapped (5); Aero muted too light (5); Aero accent too light (5); border mix default 0.3 (10); corner scale 1.1 (21); hover tint 0.2 (11); contrast exponent 2.2 (4); contrast +0.1 instead of +0.05 (17); font fallback returns a path that is not there (2); Paper loses its serif (2). Picture test: a corner radius changed in `look.cpp` → every picture differed; reverted.

## Pictures: old themes unchanged
Real app built on Linux (`-DPL_BUILD_APP=ON`, SDL offscreen, Mesa 25.2.8 llvmpipe). `tools/theme_pictures.sh`: 6 screens (Orbit Lab, Atom Lab nucleus, Cloud 3D, chart, Settings, theme picker) × 12 variants (auto dark / auto light + 10 themes) = 72. Baseline: the 0.13 code with only the version number set to 0.14 and the `PL_FIXED_DT` test switch (else the header text differs everywhere). **60 of 60 pictures identical (0 pixels)**. The 12 theme-picker pictures differ on purpose: the 12th card (Frutiger Aero) fills the last row (differences only in y 147–190). Reference hashes in `tests/reference/theme_pictures_*.sha256`; ctest `theme_pictures` (`-DPL_PICTURE_TESTS=ON`, Linux only, not run by the Windows workflow). Needed `PL_FIXED_DT`, `PL_DYK_FIRST`, `PL_DATE` to make two runs equal.

## Audit: colours and styles written into drawing code
| where | what | kind → action |
|---|---|---|
| look.cpp gift page | red button #d0453b, white text | UI → `dangerFill`, `onDanger` |
| shot.cpp, benchmark.cpp | error text (0.90, 0.35, 0.30) | UI → `danger` |
| atom_learn, app_pages, learn, html_view (5) | faint blue tint (120,140,200,31/38) | UI → `soft` |
| atom_panel (3), html_view | grey (128,128,128,x) tracks/frames | UI → `neutral` |
| pushOn, selected tab, toast/achievement icon, Header*, TitleBg, TabSelected | accent as a FILL behind text | UI → `accentFill` (text/marks keep `accent`) |
| settings.cpp Auto swatch | #f7f7f5 / #151517 | UI → taken from the Light / Dark themes |
| look.cpp, settings.cpp, app_pages.cpp | `id == "monochrome"/"synthwave"/"paper"` (corners, grey pictures, glow, fonts) | per-theme code → data (`square`, `grayscale`, `glowTop`, `titleGlow`, `font`) |
| ~40 `corner(r)`, rounding 6, border 1, mix 0.25, tints 0.22/0.45, PopupBg alpha | style values | → `roundScale`, `borderWidth`, `borderFgMix`, `hoverTint`, `pressTint`, `floatAlpha` |
| orbit_view, atom_view, cloud_dots, chart_*, atom_panel (spectrum, s/l/p bars, marks), learn.cpp illustration | star, planet, phase wheel, decay-mode, element, spectrum colours | **science, fixed on purpose** |
| orbit_view kBg/kCard/kLine (sky on), overlay cards (alpha 0.84/0.72, radii), fps card, orbit_panel pink | picture-area UI | left for stages 3–4 / matches picture highlight |
| look.cpp giftPreview | Synthwave / Monochrome preview illustrations | fixed (they draw those themes) |
About 60 sites, far fewer than feared; no stop needed.

## What was built
`theme_types.h`, `theme_style.*` (pure, tested), `style_draw.*`: one helper per kind (page, panel, card, face; own Button / Checkbox / Slider that copy Dear ImGui 1.92.9b's steps and hand over to plain ImGui when the theme is plain). Docked panels paint the page gradient on the background layer (windows see-through), floating windows paint their own and a soft shadow. Radial pages are implemented but used by no theme and not drawn in any test picture (pure gradient maths is tested). `fonts.cpp`: theme fonts by file name (`PL_FONT_DIR` for checks). Aero is a native theme in `theme_style.cpp` (the web list `kThemes` is untouched, its 11-theme tests unchanged).
**Font fallback, real app:** no Segoe UI here → default font (DejaVu), no crash; with a stand-in file named `segoeui.ttf` in a temp folder the theme font loads. Real Segoe UI: unseen.

## Looked at (llvmpipe pictures, Aero and two old themes)
Aero **reads as Aero in the header and on buttons** (sky gradient, bright top half and aqua lower half, green chosen tabs, rounded floating windows, soft shadow) but **only partly**: lower on the page the gradient fades to near-white so the side panel looks pale blue rather than glass; the shadow is invisible over the dark space picture; combo boxes stay flat grey; the lab pictures (space, cloud) are untouched by design. Fixed after the first look: panels were white (page gradient made deeper, glass 0.58 → 0.55), shadow hidden under docked panels, square floating windows. Old themes compared above.

## Changed on purpose / found
About edits 29 → **30** (`g_editUses`), About now lists the WCAG formula; `app_text.inc` edited by hand as in earlier stages. **The W3C WCAG page could not be opened (network blocked): the formula is from knowledge and hand arithmetic; About and OWNER-TODO say so.** No existing test loosened; no physics changed (orbit 1,612, atom 21,066, nbody 37, jpl 55, moon 24, moon_real 59, GL 503, glslang pass; fresh configure ok).

## Not tested / limits
Windows/MSVC build (GitHub Actions only); real GPU; speed; Segoe UI itself; contrast of progress bars, chart/picture overlay cards and science colours on Aero (not in this stage); combo boxes, tooltips and some pop-ups are only plain-ImGui glass (no shine/shadow).
