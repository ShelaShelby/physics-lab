# 0.82.1: the Black Hole Lab's bugs the owner saw

## What changed
- Read: CLAUDE.md, REPORT 0.82.0, ROADMAP (Black Hole Lab); by grep and line ranges shell_blackhole.cpp (Controls, More settings, every BeginDisabled), lab.h/.cpp (Settings, shown, gasOn, save and load), realest.cpp, main.cpp (PL_BH_SET, PL_BH_GAS), disc.h/.cpp (surface, discRayTable), shaders.h (kLensFS, kKerrSkyFS), blackhole_view.cpp (compareHook), check_blackhole.py.
- Static near the disc's plane. Cause: kLensFS made a disc's coverage from the window w = fwidth(r) as min(r + w/2, r_out) - max(r - w/2, r_in). Seen nearly edge on, r hardly changes from pixel to pixel, w falls below the float's steps in r, and the difference is rounding noise, so disc and sky mix one pixel at a time. kKerrSkyFS already had a guard for this. kLensFS now has the same one (shaders.h).
- Ruled out with the CPU reference at the same settings: the CPU lookup (discPixel, with the surface) has no static (24 isolated pixels of 638,000 at 57.5 GM/c², 0 at 1000), and the card's order and radius equal it (0 differ). The camera inside the thickness is not the cause: 57.5 GM/c² at pitch 1.2 and views outside it were noisy too, and the fix clears the views inside it. Nothing was restricted. Not ruled in: grazing rays.
- The Moving gas button is now a row "Streaks and clump" with Off / On. Pause and the rate show only while it is on. Nothing but the user turns it off: presets, Reset view, the look and the saved settings leave it alone. Most realistic turns the clump and the streaks off, and the old button then still read On while the disc was flat. The row now reads Off then, and On sets the two models back.
- Film: the Gas disc Off / On row, the Spin slider and More settings' typed spin are not drawn. A line says "Film always draws the film's disc / spin." The other BeginDisabled (the rate while the gas is off) is gone the same way.
- New hook 2e (PL_BH_COMPARE_VIEWS) in blackhole_view.cpp; check_blackhole.py parts gasrow and edgeon (ui_blackhole). VERSION 0.82.1, changelog, TESTS.md, ROADMAP.

## Tests run
- edgeon (33 checks, 0 failed): 16 views (57.5 and 1000 GM/c², the owner's pitch 1.575, the plane, 0.2 degrees either side in 6 steps). Card = lookup in every one. Over 8 levels: at most 66 of 574,000 pixels. On 0.82.0 it fails 15 checks (61 % of the near disc over 8 levels at 57.5 and 1.575, 81 % at 1000 and the plane).
- gasrow (13 checks, 0 failed): the row reads On, Off after a click, Off for gas=off, On again; the click on Off equals gas=off to the pixel; Off then On equals the start; Most realistic's both models off reads Off. Film: no Spin slider and no disc row, two plain lines. Real: both drawn.
- check.sh job: quick 18 of 18 passed, ui_smoke passed.

## Not done
- No Radeon here: the owner's 1000 GM/c² view was checked on lavapipe at pitch 1.575 and the plane (clean after the fix, 17 pixels before at 1.575). His next run decides (ROADMAP).
- What is seen from inside the disc's thickness is not modelled (ROADMAP, owner decides).

## References changed on purpose
- None. A new GLSL line only; no test's expected value changed.
