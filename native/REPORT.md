# Native Physics Lab 0.15 — stage 5h: Atom centre, info text, data sheet tabs

`index.html` untouched. No helper agents. New UI Off = the old app (centre, text size and Old panel unchanged). No physics, formula or value changed. **No new number, fact or source** (the About list is unchanged).

## Part 1 — changed
- **Centre:** the atom's drawing centre (so its label "Uranium-238 · 92p, 146n") is the middle of the largest free rectangle left by top bar, toolbar, fact line, Controls panel and subject card (`largestFreeRect`, the Orbit Lab's helper). Before: window middle (640, 400 at 1280x800), under the card (x 614..914). Scale unchanged; Orbit Lab untouched.
- **Info text:** measured 13 px at 100 %. New UI: 14 px (line pitch size + 4); Old: 13.

## Part 2 — changed
- The data sheet (All values) has a second tab row: **Spectrum, Lifetimes, Nuclear size, Temperature, Radial**. A tab is listed only when the Old panel draws that card for this subject and view (Radial: not in Orbit view, not in the chart).
- **Same code, not a copy:** the tabs call the Old panel's own card functions (`atomSheetCard` -> `realSpectrumCard`, `lifetimeCard`, `nuclearSizeCard`, `temperatureCard`, `radialCard`, with their `realStrip` / `radialPicture` drawing). The Temperature block was moved out of `atomPanelContents` into `temperatureCard` unchanged. Old panel behaviour not changed.
- Their small text (12/16 of the font in the Old panel) is drawn at Body size or more in the sheet; the graphs' tick labels keep the Old panel's 11/12 ratio (about 12.8 px at Body 14: **under 14 px, as in the Old panel**). The "Check a jump" buttons and the temperature slider in those tabs are the Old panel's widgets.
- The sheet scrolls inside itself (wheel; Page Up / Page Down added in `sheetScrollKeys`), stays solid, inside the window and out from under the Controls panel. Tab labels shortened so they fit at 880 wide.
- The Old panel button stays.

## Tested (Linux, software OpenGL)
- `tests/check_5h.py`, ctest `ui5h_centre`, `ui5h_infosize`, `ui5h_cards`, `ui5h_fit`:
  - centre: 3 sizes x 3 views x card open / collapsed x panel open / closed. infosize 14+ / 13.
  - cards: Hydrogen, Helium-4, Uranium-238, Iron-56, in Cloud 3D and Orbit: the strings (and sums behind the spectrum strips and radial curve, `PLATOMCARD card.*` from `PL_LOG_CARD`) of the Old layout run (New UI Off) equal those of the sheet's tabs; no radial card in Orbit; tab present / absent per view (also chart).
  - fit: 5 tabs x 3 sizes: sheet inside window, solid, clear of the panel, wheel and PageDown move it (needs 880x560: 5 of 5 tabs taller than the sheet), no cut text.
- Break on purpose (reverted): old centring: 19 failures; info size 13: 3; sheet "size" tab with one extra string and "spectrum" tab with Dirac forced on (changes strip numbers): both caught in all 8 subject / view runs each.
- Looked once at each of the 5 tabs (Uranium-238, 1280x800) and the Atom at 1280x800. Found and fixed: two tab labels were cut ("Real spectrum (NIST)", "Lifetimes and jumps").
- End runs (one per part; not the full suite): `check.sh quick` 9/9 both times; part 1: 23 of 23 picture tests (ui5g_*, ui5h_*, orbit_ui_card, orbit_ui_coverage, theme / window / y2k / page_cache / effects pictures, atom_panel, atom_toolbar, atom_chip, cards_atom, controls_panel, shell_ui, shell_pictures); part 2: the same list plus ui5h_cards and ui5h_fit: 24 of 25 first time. `window_pictures` failed by 41/255 on a glyph edge of the new changelog entry: I reworded that sentence (test untouched) and re-ran that one test alone: passed. Only the changelog wording changed after the end run. No reference picture rewritten.

## Known / reported
- The "🏠 Room" and "💨 Just boiled" emoji do not draw in the sheet's Temperature tab (the Old panel's own buttons: not looked at there; font glyphs).
- The centre ignores pan; with a very large zoom the label can leave the free area (default zoom tested).

## NOT tested
Windows, a real mouse or wheel, a real GPU, speed, scale 125 / 150 %, the scroll bar by drag. No new per-frame work except the Old panel's cards while a sheet tab is open (the same as the Old panel).
