# Native Physics Lab 0.15 stage 5r: flat pills

Bug fix only, no physics or value changed. `index.html` untouched. App label stays 0.14.

## Changed
- `shell_widgets.cpp` beginPanel: the cause was the "top-lit gradient" (AddRectFilledMultiColor, a straight-edged lighter block over the straight part of a pill, measured luminance ~40 vs ~30 at the round ends) plus a 1 px outline on the edge pixel. Pills now get no gradient and no outline, except where `F().outline` is true (Monochrome, High contrast).
- Sage: its glass is only 3 of 255 away from the page behind it, so with no outline it was nearly invisible; a flat 1.5 % white layer is added for Sage pills (test needs 4..40).
- New test `tests/check_5r.py`, ctest `ui5r_pill` (dark, light, sage, monochrome, 1280x800, Atom Lab): toolbar and the 2D/3D (tilt) pill: flat interior (<= 2 of 255 over 16 points), no outline (kept in monochrome), fill differs from the page by 4..40. Before the fix it failed (13 failures); after, 0.
- Docs: CLAUDE.md (new working rules), RESUME, DECISIONS, changelog.

## Shape::Pill panel calls (shell.cpp)
- line ~580: collapsed card chips (the "Protactinium-231" chip is one of these; same beginPanel path, same cause as the toolbar, no different cause found)
- line ~735: the bottom toolbar (`##shellTool`)
- line ~784: the chart tilt pill (`##shellTilt`, "flat ... 3D")
- Other panels (Controls panel, cards): shine gradient left as is, not looked at.

## Tested (cloud, Linux, software OpenGL)
- `check.sh quick` 10/10; ui5r_pill; ui5q, ui5l, atom_chip, shell_pictures passed (24 guard hashes re-written after looking at the Orbit 2D/3D pill in Dark; every picture with a pill changed on purpose).
- Looked at crops (4x): toolbar in Sage and Light, tilt pill in Dark: flat, no rectangle, no outline.

## NOT tested
- Windows, a real GPU, a real mouse, speed.
- The chip picture itself was not rendered (only shares the code path); the full suite was not run (new rules).
