# Native Physics Lab 0.15 stage 5o: the four old test failures (parts 1 and 2)

Fixes only, no new features, no physics or value changed, `index.html` untouched. All four failed on the commit before 5n too (5n report); each was run alone first and the output kept.

## Changed
- A `cards_orbit` (Orbit 880x560): the subject card ran 33 px into the readout pill (card 56..445, readout from 412). `app/shell.cpp`: a corner card now also stops above the readout when they share columns (new `cornerBottom`), and scrolls inside itself as it already did for the toolbar. Seen in the picture: the card now ends at the pill; "All values" needs a scroll at this small size.
- B `ui5g_atomtext` (Atom, Iron, Controls open, 1280x800): the fact line had 301 px beside the toolbar, too short for two lines, so it ended "...hotter than the…". `ui/shell.cpp` + `factLineRect(..., above)`: when beside the toolbar it needs more than two lines and the row above fits it in fewer, it takes the row above (as it already does at small windows). Seen: the whole sentence shows, the scale bar rose clear of it.
- D `shell_ui`: the longest body line in About was 80 characters (limit 75). Three About paragraphs reworded, same meaning (developer line, "Built with Claude", intro of "Where the facts come from"; all in `app_text.inc`, one language). The wrap is by pixels, so the longest line is now exactly 75: no margin. Wording was chosen with a width simulation, then measured in the real app.
- Saved references changed ON PURPOSE (CLAUDE.md rule): the About "word for word" test (`app_settings`) takes the web text plus a list of native edits, so I added 3 edits to that list (test_app.cpp and tools/native_edits.js) and the edit counter 32 -> 35; the 4 About lines of `shell_pictures_0.15s4.sha256` (looked at: the text only changed).

## C `ui5l_popups` NOT fixed: the test is wrong, left failing
Test: at 1280x800 Orbit Lab, nothing inside the Labs dropdown, inset 3 px from its rectangle, may depend on what is behind it. Truth: 1 pixel, (93,164), differs (RGB 19,20,25 vs 35,37,44), and it lies on the panel's rounded-corner edge, not on the fill (fill is (23,23,26) in both pictures). The panel radius is 10 u; its pixel centre is 9.48 px from the arc centre (edge ring 9..10), so it is the anti-aliased rim, which blends with the page by construction. A radius-10 corner leaves the 3 px inset square at R(1-1/sqrt2)=2.93 px, so the test's inset is under 1 px too small. With inset 4 or 5, 0 pixels differ (scratch script, repo test untouched). It only fails where the page behind happens to differ at that corner. I did not shrink the corner radius or change the test: owner decides (a test inset of 4 would fit).

- Part 2: README line 4 now says 0.15 alpha (the project's version: RESUME, ROADMAP, changelog entries). The "latest" release title now says 0.15: the Windows workflow has `$ver = '0.15'` (was read from CMake). I did NOT change the app's own label (CMake `VERSION 0.14`, window title, About, "v0.14 alpha · history" chip): I tried, and it is drawn into the old UI's header, so every saved old-UI picture hash (`theme_pictures`, all pictures) changed. That needs its own decision; app and release now differ (0.14 vs 0.15) until then. A first full-suite run on that tree was stopped and thrown away; the one real run is below.
- Docs: ROADMAP (5o done, next stage 6, 7, 0.16 list), RESUME, OWNER-TODO (4 items), changelog.cpp (one entry for A, B, C-free fixes).

## Tested (cloud, Linux, Mesa llvmpipe)
- Before and after, each alone: cards_orbit failed -> passed; ui5g_atomtext failed -> passed; shell_ui failed -> passed (readmax 80 -> 75); ui5l_popups failed -> still fails (see C).
- Break on purpose (fixes undone, one rebuild): cards_orbit failed again (card 56..445 vs readout 412); ui5g_atomtext failed again (3 checks, the cut "…hotter than the…"); shell_ui `reading` part failed again (readmax 80). Restored (diff identical), rebuilt.
- Also run alone and passed: atom_panel, cards_atom, ui5h_cards, ui5h_fit; shell_pictures failed only on the 4 About pictures (updated after looking). `app_tests` 202,232 checks, 0 failed; `check.sh quick` once.
- Full suite once, on the final tree (`check.sh full`, 4 cores): 57 tests, 56 passed, 1 failed, 0 skipped. The one failure is `ui5l_popups` (C above). Nothing else failed.

## NOT tested
Windows behaviour, real mouse, real GPU, speed, 125 / 150 % scale. The Windows run and release of the final push are in my last message.
