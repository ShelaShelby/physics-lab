# Native Physics Lab 0.15 — stage 5h part 1: Atom Lab centre and info text (New UI only)

`index.html` untouched. No helper agents. New UI Off = the old app (centre, text size unchanged). No physics, scale or value changed. **No new number or fact, no new source.**

## Changed
- **Centre:** the atom's drawing centre (and so its label "Uranium-238 · 92p, 146n") is now the middle of the largest part of the window that the top bar, toolbar, fact line, Controls panel and subject card leave free (`largestFreeRect`, the same helper the Orbit Lab uses). Before it was the window middle (640, 400 at 1280x800), under the card (x 614..914). Scale is still that of the whole picture; only where it is centred changed. `shell.cpp` hands the free rectangle to the picture, `atom_view.cpp` uses it.
- **Info text:** measured 13 px at 100 % (13 points x text size 1). In the New UI it is now 14 px (the body size); line pitch follows (size + 4). Old app: still 13.

## Tested (Linux, software OpenGL)
- New `tests/check_5h.py` (ctest `ui5h_centre`, `ui5h_infosize`): 3 sizes x 3 views (Cloud 3D, Orbit, Slice) x card open / collapsed x panel open / closed: centre and label not under card, panel, top bar, toolbar, fact line, handle; label inside the window. Font size 14+ (New UI) / 13 (Off).
- Break on purpose (each reverted): old centring back: `ui5h_centre` 19 failures; info size back to 13: `ui5h_infosize` 3 failures.
- End run: `check.sh quick` 9/9; ui5g_*, ui5h_*, orbit_ui_card, orbit_ui_coverage, theme / window / y2k / page_cache / effects pictures, atom_panel, atom_toolbar, atom_chip, cards_atom, controls_panel, shell_ui, shell_pictures: 23/23. No reference picture rewritten.
- Looked once at Uranium-238, Cloud 3D, 1280x800, card open: atom and label in the free area left of the card.

## NOT tested
Windows, a real mouse, a real GPU, speed, scale 125 / 150 %. The centre is not re-centred when the user drags the picture (pan is unchanged). With a very large zoom the label can still move under a card (only the default zoom is tested).
