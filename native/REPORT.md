# Native Physics Lab 0.15 — stage 5f (of 8): Atom panel tidy, subject card, data sheet, legend, hint line

`index.html` untouched. No helper agents. New UI Off = the old app (guards below). No physics, formula, value or default changed. **No new number or fact:** every string on the new card, sheet and legend comes from existing code (listed below), so the About page needs no new source.

## Part 1 — what changed (New UI On, Atom Controls panel)
- **View selector:** the plain list is now one compact control, two rows of segmented buttons (Atom · Nucleus · Nucleon · Chart / Cloud · Cloud 3D · Slice), the current view highlighted; each part calls the old action (`atom::pressTool`). Fits the 22 em panel at 14 px.
- **Notes fold:** the long notes (energy, cloud, super, phase, chart, chain, photon jump, the two View notes) sit behind "Notes about this choice (show)", closed by default, same words. The short words under a chosen button stay visible.
- **Chart view:** no Cloud section (style, density, superposition did nothing there). New UI only; the old panel still shows them. In DECISIONS.
- Tests (`check_atom_panel.py`, new part `tidy`, 129 checks, written first): every selector part, 3 sizes, gives the same view and zoom as the OLD panel's View rows (Nucleus and Nucleon: the toolbar buttons, themselves compared with the old bar by `atom_toolbar`); the highlighted part is read from the app; folds closed by default, open and close by their row, no note text drawn while closed; the Chart has no Cloud rows; panel text >= 14 px in Orbit and Chart at 1280x800, 880x560, 1920x1080.
- **Changed on purpose in an old test:** the 5d section-list check no longer expects "cloud" on the chart. The selector parts keep the old ids (`cloud.slice` ...), so `atom_toolbar` and `controls_panel` needed no change. The selector tests use carbon (Z=6): hydrogen's "nucleus" is one proton, which the app calls the Nucleon layer.
- **Break on purpose:** Cloud button wired to Slice -> 6 failures; reverted, 0.

## Part 2 — what changed (New UI On)
- **Orbit Lab:** card, legend and data sheet already existed (stage 4); checked against DESIGN.md for the first time. Found and fixed: small text in the cards and the sheet was 13 px (Caption); it is now drawn at Body size (14 px floor, as in the Controls panel). Everything else matched (card 20 em, top right, left of the panel and handle; Legend top left, collapsed).
- **Atom subject card** (top right, 20 em): name "Uranium-238", the category, the proton number as the one big number, Neutrons, Nucleons, the period / state / radioactive chips, the old isotope line, **More info / Less info** (the old bar's button, same action) and **All values**. No action link (nothing existing fits; DECISIONS).
- **Atom data sheet** (36 em, centred, cross and Esc): tabs **Values** (isotope line, name, chips, "Where you meet it", melting / boiling / density lines, their notes and footnote), **Decay** (the old Decay history), **This view** (the old bar's hint line). Strings come from `elementChips`, `everyday`, `factsLinesHtml`, `factsNotesHtml`, `isotopeLine`, `decayHistory`, `AtomLab::hint` (all existing).
- **Atom legend:** on the Chart of nuclides only, the chart's colour legend (8 colours with counts), the magic numbers, the landscape height line and the valley / island notes (behind a row) are in the Legend chip (collapsed). The picture leaves its own copy out in the New UI (it was cut off by the Controls panel in the look). Old UI: unchanged.
- The old "Old panel" no longer shows the hint and More / Less info (moved to the card / sheet). Not moved yet: real spectrum, lifetimes and jumps, nuclear size, temperature picture and radial graph cards stay in the Old panel (stage 5g, ROADMAP).
- **Hint line:** DESIGN.md says "hint -> readout". The hint is up to a paragraph long, so it is in the sheet's "This view" tab instead. Owner to decide.
- For the checks: `PL_LOG_CARD` makes the OLD Atom panel print its strings (`PLATOMCARD`); `drawnLegend` in `astate`; the notes `atomnotes`, `atomlegend`, `viewsel`.

## Part 2 — tests (written first; none loosened): `check_cards.py` (`cards_orbit` 66, `cards_atom` 116 checks)
- Each lab, 1280x800, 880x560, 1920x1080: the card exists, is inside the window, 20 em wide, does not overlap the top bar, toolbar, readout, fact line, Controls panel, handle, Old panel or Legend, is left of the panel and handle; card and sheet small text >= 14 px; the Legend is top left, collapsed, inside the window; the sheet opens from All values, is inside the window and at most 80 % wide, and closes with Esc and with its cross; the card follows the chosen body / element (set at the start, picked in the list or the periodic table).
- **Atom sheet = old panel:** for hydrogen, iron, uranium-238 and oganesson the sorted lines of the sheet equal the strings the OLD panel (New UI Off, same build) printed for its element card, facts card, isotope line, decay history and hint. Orbit sheet = old card word for word: the existing `orbit_ui_card` (8 things).
- Legend: 8 colours with counts when opened; the new picture draws no legend, the old picture does; no chip in the Orbit view. More / Less info flips its words.
- **Break on purpose:** card 40 px too wide -> 3 failures (the first version of the test did NOT catch it; I added the 20 em check); a sheet value from the wrong element (`everyday(Z - 1)`) -> 4 failures. Both reverted. For the Orbit sheet no wrong-body break was run (its old-card comparison is `orbit_ui_card`, not re-broken here).

## End runs (changed-only, not the full suite)
- Part 1: `check.sh quick` 9/9; 11/11 passed: shell_ui, controls_panel, atom_panel, atom_toolbar, atom_chip, shell_pictures, theme / window / y2k / page_cache / effects pictures (window_pictures passed; no glyph-edge failure).
- Part 2: `check.sh quick` 9/9. Passed first time: shell_ui, shell_state, controls_panel, atom_panel (all parts), atom_toolbar, cards_orbit, cards_atom, orbit_ui_time / chip / facts / facts_long / readable / search / links, and the old-app guards theme / window / y2k / page_cache / effects pictures (window_pictures: no glyph-edge failure).
- **Failed on purpose, fixed:** `shell_pictures`: all 38 of the 48 saved pictures that show the Orbit card changed (14 px text; looked at: Orbit card, dark, 1280x800) -> `tests/reference/shell_pictures_0.15s4.sha256` rewritten. `atom_chip`: "a click outside closes the dropdown" clicked at (100,640), which the dropdown now covers because it steps aside from the new Atom card (DESIGN.md section 6) -> the click is now at (770,600), between the dropdown and the Controls panel; nothing loosened. Both re-run alone: both passed.
- **Failed, NOT from this stage:** `orbit_ui_card` (16 checks) and `orbit_ui_coverage` ("each of the 47 Go to clicks selected something"). I built the stage 5e commit (ebd8dee) in a separate folder and ran `orbit_ui_card` there: the same 16 failures. Clicking a body in the Pick list at 1280x800 does not select it in the scripted run (it does at 1000x640); neither test was in the changed-only runs of 5c.1 to 5e, so I cannot say which stage broke it. NOT fixed, NOT investigated further (time). Owner: try the Pick list on the PC (OWNER-TODO). The Orbit sheet = old card comparison therefore did NOT run this stage; the Atom sheet comparison did.

## Looked at (software OpenGL, once each, dark)
Atom panel Orbit view and Chart view (1280x800); Orbit card; Orbit data sheet; Atom card (Uranium-238); Atom chart with the Legend open; Orbit 880x560. Found: the Atom picture's info text (More info) runs under the card in a narrow window (OWNER-TODO); the open Atom Legend scrolls inside when tall.

## Not tested / limits
- Windows, a real GPU, speed and scale 125 / 150 %: **not tested**. No new per-frame work except building the card strings each frame (a few small strings; not measured).
- Orbit and Atom commits are ONE commit for part 2 (the card code is shared); not split.
- Old app, not changed: "Restart walk" looks cut off in the old 360 u Atom column.
- Atom cloud dots did not show in the short paused software frames; not investigated (OWNER-TODO: check on the PC).
- Pick-list clicks at 1280x800: see "Failed, NOT from this stage" above; `cards_orbit` selects bodies with PL_SELECT and clicks at 1000x640.
