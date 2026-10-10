# Native Physics Lab 0.15 stage 5p (parts 1 and 2): the owner's six PC findings

Bug fixes only, no physics or value changed, `index.html` untouched. New tests: `tests/check_5p.py` (ui5p_wheel, _scroll, _zoomout, _lit, _picker) and a unit test in `test_shell.inc`.

## Changed
- #6 dead wheel, CAUSE: `reading.cpp` kept "a page is open" on after the first page closed (cleared only in the Old layout), so `main.cpp` dropped every wheel event, both labs. Now it counts only in the frame the page was drawn. History and the log failed the same way; Settings, Learn, theme, search, Labs menu never did.
- #2 About: wheel and PageUp/Down/Home/End already worked (5k). Scrollbar now 12 px wide (was 8) with a faint track, still draggable.
- #3 Zoom-out twice as far: smallest scale 1/2 (log10 -0.30103, was 0), Chart 0.5 (was 1), in Atom/Cloud/Cloud 3D/Slice/Chart (Nucleus, Nucleon are presets, unchanged). Slider shows x0.5 below 1.
- #1 Lit button (M1 Dark): dark fill (card 14 % toward text, 53 vs bar 23), icon white, no outline, no glow; unlit icons 15 % dimmer (plain muted gave only 2.79:1, now 3.6:1). Also the Orbit toolbar's 2D/3D segment (its only lit button). Controls segments and Settings tiles untouched.
- #4 CAUSE (reproduced, not seen on the PC): the toolbar is centred on last frame's free strip, which was computed with the toolbar in the way; with the picker open it flipped left/right strip EVERY frame (880x560: centre 123 <-> 415, toolbar, FPS pill, fact line too). New `atomFreeRect`: bottom row is one band.
- #5 Picker: stops above fact line, toolbar and FPS pill (scrolls inside); Atom info text hidden while it is open. Also shortens the Orbit Time panel (hangs from the top bar) above the readout.
- Tests changed ON PURPOSE: `atom_tests` chart zoom "1 at least" -> "1/2 at least"; `check_5g.py` info-text rule extended to the picker; `shell_pictures_0.15s4.sha256`: 13 lines (12 dark: dimmer icons / lit look; 1 light, Time panel 1000x640, now shorter: looked at).

## Tested (cloud, Linux, Mesa llvmpipe, scripted events, pixels)
- Before/after kept: wheel 16/38 failed -> 0; zoom-out 11/33 -> 0; lit 13/21 -> 0; picker 9/68 -> 0. Break on purpose (one undo each, restored): scrollbar width (8.0 px), lit fill + idle colour (6 fails), free-rect rule (4 fails). #6 and #3: the pre-fix run is the proof, not a second undo.
- Unit: the loop with real makeFrame/placePanel at 100/125/150 %, 144 layouts: old rule flips in 16, new in 0. atom_tests 21,191+ checks 0 failed; app_tests 202,377 checks 0 failed.
- Looked at: About bar, Cloud 3D at the limit, 3 lit-button screens, picker at 1280x800 and 880x560.
- Full suite once on the final code (`check.sh full`, 4 cores, 32 min): 62 tests, 60 passed, 2 failed: `ui5l_popups` (known, 5o) and `shell_pictures` (expected hash change, fixed after looking, then passed alone). Nothing else failed.
- Seen, not changed: one End press on About stops ~1 % short (page grows); at 880x560 the picker covers the subject card, at 1280x800 the scale bar's left end.

## NOT tested
Windows behaviour, real mouse, real GPU, speed, 125/150 % scale as a picture (only the rectangle maths). #4 was NOT seen as the owner saw it; the flip I found is a proven cause of the same symptom, not a proof it is the only one.

## Haiku helper (4 calls, one after another, read-only)
(a) wheel paths: right, found the stuck flag. (b) zoom limits: right (missed the chart pan special case at zoom 1; I found it). (c) lit drawing: right. (d) picker/toolbar/backdrop: right on lines, found no cause itself; its pointers led me to the loop. Each answer took 40-55 s, 74-92k tokens (340k total); I re-read every line, so they saved little time and cost more tokens than grep. Session total: not visible (only the context counter, 15.0M to 14.7M left).
