# Native Physics Lab 0.15 "Full UI rework" — stage 4 of ~7: the Orbit Lab complete in the new UI

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push to `native`, no PR. Physics untouched (no formula, constant or number changed).
**NOT tested: a real PC, real GPU, Windows, Segoe UI, Windows scale events, speed.** Pictures are Mesa llvmpipe, headless (the real app). The owner judges the look on his screen.
**Tests were written first** (`tests/check_orbit_ui.py`, the new cases of `test_shell.inc`, `check_shell_state.py`), compile-red / run-red, then the code.

## What is built (Settings → New UI (preview), Off by default; Off = exactly today's app)
- **Owner's notes of 6 Oct:** play / pause is an icon at the left of the chip (Space too); Save picture is in the ☰ menu and search, not on the toolbar; the Atom toolbar lost both (it is empty and not drawn until stage 5; its play is in its chip); the 💡 button is gone: a **fact line** (bottom left, beside the toolbar, muted text with a soft shadow, fades out and the next fades in about every 20 s, still while hovered, a click opens the whole fact, ‹ › on hover, Settings → Facts On / Off).
- **Subject card** (`orbit/subject_card.*`): name, kind, ONE big number with a caption, 4–6 values, an action link ("Slow time to …"), **All values** = the data sheet with every line of the old card word for word. Made from the card text the picture drew, so the two cannot differ.
- **Time panel:** Live (real date now, running) / Set manually (date + Go, 24 h dial, steppers), 4 jumps + More dates, (i) for the help, shorter. Chip keeps one width (its words change every frame).
- **Everything else:** Show has Sky, Constellations, Far-away; Pick has a filter and the NASA/JPL words; real **Legend** (colours from `orbit/marker_colors.h`, which the picture reads too); Performance and Benchmark in Settings; readout shadow black / white by contrast (no theme had a text-shadow colour); reading-view links underline as one line; panels step aside from the card.
- **Search:** bodies, dates, every Orbit setting, menu actions, guides, the 26 facts; Enter, ↑↓, recent choices first (saved as `recent=`).

## Tests (Linux GCC Release)
- Unit: app_tests **177,190** (+9,291: card for all 60 real cards, search recents, fact clock / place / shadow contrast in 15 themes, side step, prefs); orbit 1,612 and atom 21,066 unchanged.
- `orbit_ui_*` (9 ctests): **coverage** (every INVENTORY.md Orbit item by real clicks, dates vs Python `datetime`, all **47** Go-to names read from INVENTORY.md, no play / save on the toolbar, no Old panel), **card** (8 things: data sheet == old layout's card, line for line), **time** (fits above the toolbar at 1280×800 and 1000×640, Live and Set manually), **chip**, **facts** (visible, changes after 20 s counted in 16 ms frames, still while hovered, click, arrow, Off, Reduced motion), **facts_long** (no overlap at 3 sizes × 5 screens, 12 bodies), **readable** (found by picture difference: letters vs shadow ≥ 4.5:1 in 9 themes, over space and over a bright Sun), **search** (23 things found and opened, arrows, recents), **links**.
- `shell_state` now also Sky, Constellations, Dwarfs, Far-away, wobble, Follow, Slow time (2), Reset view, Journey: old panel and new UI give the same state. `shell_ui` passes (new texts exactly on the type scale). 48 picture guards (Dark, Aero, Y2K Night, Light × 1280×800, 1000×640 × 6 screens) are **regression guards, not references**, written after I looked at them.
- Old layout unchanged: all picture tests pass with 0 differing pixels (the new link underline and Settings rows are New-UI-only).
- **Perturbed, caught, reverted:** Far-away tick box removed (coverage), a card value cut by a letter (unit 40+), Time panel 300 u too tall (time, 3), fact line moved under the toolbar (unit 364), link gaps back (links, 648 gaps), chip width unstable (chip).
- Last full ctest before the final edits: 26 of 27 (shell_pictures: 2 search pictures differed, the text cursor blinked on the real clock; fixed with no blink under `PL_FIXED_DT`, guards rewritten, then it passed). The final full ctest run was STILL RUNNING when this was pushed (the owner's usage limit was near): 3 of 27 had passed (theme_pictures, orbit_ui_facts_long and one more), none had failed; every other test passed on its own run today after the last code change except those two untouched by it. Its result is not confirmed here: re-run `ctest` on the next session.

## Found (and fixed unless said)
- The fact line's arrows never worked with a real click (hover was blocked while the button was held); the chip slid under the pointer as its words changed; hover during fade-in left it half visible; Pick-list rows below the fold cannot be clicked in a script (filter box used).
- INVENTORY.md said 45 body tiles; it lists **47** (fixed). Its "Jupiter's Trojans" has a curly apostrophe in the app.
- **None of the 27 "Did you know?" facts has a named source** in the app or in About; the full fact says so. Not invented. (Reported, owner decides.)
- The habitable zone has no switch in the Orbit Lab (always drawn, "rough"): Legend row only.

## Performance (600 frames, 1280×800, llvmpipe, New UI, Earth selected; stage 3 binary vs now)
CPU user+sys 79.5 / 81.5 s (stage 3) vs 80.2 s (now): the same within noise (software drawing dominates). Peak RSS 237.3–237.5 MB vs 237.9 MB (+0.5 MB). No new font sizes (the 28 px Key size is the stage 3 scale; the fade changes colour only). Per-frame work removed: the card text is made once per frame (was twice), the legend only while open, the selected name is cached. Not measured: a real GPU, Windows.

## Looked at (Dark, Aero, Y2K Night, Light; 1280×800, 1000×640; closed, Time, Show, data sheet, search, About; Atom Lab)
Clean, desktop-sized, picture first. Fixed on sight: Show panel too narrow for "Lines and names" (25 em), Time panel and data sheet covering the card at 1000 px, the fact line over the Atom Lab's own scale bar (hidden there until stage 5). Rough: in light themes the readout / fact text is dark with a white halo over the dark picture (readable, a little heavy).

## Not done / limits
Atom Lab (stage 5); Learn / Achievements still old pages (stage 6); no Language entry; icons drawn by code; a true overlapping cross-fade not built (fade out, then in); Segoe UI widths are untested.
