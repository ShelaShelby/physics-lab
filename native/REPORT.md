# Native Physics Lab 0.15 — stage 5i: sheet above the toolbar + three look options

`index.html` untouched. No helper agents. New UI Off = the old app. No physics, formula or value changed. **No new source** (the About list is unchanged; the contrast rule is the W3C's WCAG 2.x, already used by the themes).

## Part 1 — changed (New UI)
- **Data sheet vs toolbar:** the sheet (`Anchor::Centre` in `app/shell.cpp`) ended at the window bottom minus 24 u; now it ends at the toolbar top minus 8 u (776 -> above 748 at 1280x800, 536 -> above 508 at 880x560). A taller sheet scrolls inside itself as before.
- **Found while testing:** the Orbit sheet at 880x560 was pushed under the Controls panel by the side step away from the card (x 136..650, panel from 538). It is now kept left of the panel (it may sit over the card instead).
- **Test reference rewritten on purpose:** the 4 `shell_sheet_*_1000x640` hashes in `shell_pictures_0.15s4.sha256` (that sheet moved up). Looked once at the dark one. Nothing else changed.

## Part 2 — three look options (render only; the default is untouched)
`PL_LOOK=a|b|c` is read once at start (`ui/shell_widgets.cpp`), Dark theme only. Tokens are in `app/look_options.cpp`. Unset = today's tokens (guards below). No setting, no UI entry, no About line.
- **A Soft glass:** card #1a2030 at 88 % opacity (see-through; **no blur: the app has no blur path**, so this is translucent, not frosted), radii 14 / 10, padding 16, border 18 %, shine 7 %, halo 30, accent #8fb5ff, fill #3d6fe0.
- **B Flat minimal:** card #1c1c1e solid, no shine, no halo, border 8 %, radii 8 / 6, padding 20, accent #7da6ff (links, sliders), selected fill neutral grey #d9d9de with dark text. Toolbar buttons were already box-less (hover pill only), so nothing to change there.
- **C Instrument:** bg #08090b, card #0f1114 (97 %), text #d8dee4, border 16 %, radii 4 / 3, padding 12, icon stroke 0.7x, no shine / halo, teal accent #2dd4cf, fill #0e7c86.
- **Could not do:** real blur; "tabular numbers" (the font has no such feature through our text path: none added); spacing changes beyond panel padding (the layout scale is shared); the top-bar / breadcrumb text sizes are fixed by the type scale. Other themes: stage 7.
- **Font:** I think the font is a smaller cause than the heavy rounded boxes and equal-weight text; if the owner still wants a change, one candidate is Inter (SIL Open Font Licence 1.1; licence from memory, not checked in this session). **Nothing added.**
- **See it on the PC (Windows):** Start menu -> type `cmd` -> Enter. In the window type `set PL_LOOK=a` Enter, then the full path of `PhysicsLab.exe` Enter (for example `"C:\Users\you\Downloads\PhysicsLab\PhysicsLab.exe"`). Use b or c for the others. Settings -> New UI = On, theme Dark. Close the window to go back to normal. (Not tried on Windows.)
- Pictures: `native/docs/look/a_1.png` ... `c_5.png` (1 Orbit + Controls "Pick a body", 2 Orbit card + toolbar, 3 Atom Cloud 3D U-238 card, panel closed, 4 Atom sheet Spectrum, 5 top bar crop at 2x) and `look_compare.png` (screens 2 and 3, A | B | C). Made by `native/tools/render_look.py`. Looked once each: nothing cut; the options differ mostly in surface tone, radii and accent.

## Tested (Linux, software OpenGL)
- **ui5i_sheet** (`tests/check_5i.py`, written first): 3 sizes x 8 tabs + Orbit: sheet bottom <= toolbar top - 4, inside the window, solid, clear of the Controls panel: 162 checks. Break on purpose: run **before** the fix = 12 failures (776 vs 748 ...; Orbit sheet on the panel); after: 0.
- **Look contrast** (`tests/test_look.inc`, in `app_tests`, written first; the WCAG formula is written out in the test and first checked on the W3C's values 21:1, 4.54, 4.48): per option 17 pairs (text, muted, accent on card / face / hover / press >= 4.5; fill text >= 4.5; icons, caret, check box border, slider track >= 3), the glass over pure black and pure white >= 4.5, radii and spec. Today's Dark as a sanity row. Break on purpose: B's muted text #55555c = 4 failures (reverted).
- **ui5i_look_a/b/c:** the app starts under each option, its picture differs from today's, and the 5g `toolbar`, `ring` and 5h `centre` rect tests pass unchanged at 1280x800.
- End runs (not the full suite): part 1 23 of 24 first (shell_pictures: the 4 hashes above); part 2: `check.sh quick` 9/9 and 27 of 27 picture tests (ui5g_*, ui5h_*, ui5i_sheet, ui5i_look_a/b/c, orbit_ui_card, orbit_ui_coverage, theme / window / y2k / page_cache / effects / shell pictures, cards_*, atom_panel, controls_panel) first time. With PL_LOOK unset `shell_pictures` and the other guards matched the saved references, so no existing picture changed and no reference was rewritten in part 2.

## Reported (not changed)
- Today's Dark: muted text #9a9a94 on a button face is 4.47:1 (under 4.5) and on a hovered face 3.35:1 (WCAG 2.x formula). Seen only on hovered rows with muted text; the options pass. To decide at stage 7.
- Still open from before: the "🏠 Room" / "💨 Just boiled" emoji do not draw in the sheet's Temperature tab (font glyphs).

## NOT tested
Windows, a real mouse or wheel, a real GPU, speed, scale 125 / 150 %, the other 12 themes under a look, the look options in light themes. No new per-frame work (one `getenv` at start; the look colours are built once).
