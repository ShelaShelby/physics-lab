# Native Physics Lab 0.16 stage 2: four fixes (owner's Windows findings)

## Fixes
1. About scrollbar. Cause (reproduced first: after close + reopen the thumb pixel equalled the panel colour in dark, light, sage, contrast, monochrome, solar, paper, midnight, black): the scrollbar is its own window `##readBar`; a reopened page takes the focus and is raised above it, hiding it. Not the theme tokens, not page_cache (12 lines, no height or colour data). Fix in ui/reading.cpp: the bar window is brought to the front each frame. After the fix thumb and track differ from the panel in all 9 themes checked after close + reopen (auto = dark here; auto_light was not run separately).
2. Benchmark window (ui/benchmark.cpp). The window had no height limit and was only re-centred when it appeared. Now everything above the buttons is one scrolling child, capped so the window is at most 92 % of the work area (and inside the app window); Start and Cancel sit below it, always in view. Window stays centred.
3. Contents rail (app/shell.cpp readingGeometry): 14 em -> 20 em wide. Rail only exists when the window is 60 em wider than the column, so the reading column is unchanged. Looked at 2560x1369, text sizes normal and xl (Extra large): all labels in full.
4. Atom Lab Controls > Nucleus (ui/shell_atom.cpp): section always shown (not on the chart) with Quark style, Nucleon detail, Nucleon edge and the grey caption "These show in the picture when zoomed in close." Test changed on purpose: atom_panel (check_atom_panel.py) expected the Nucleus section only at zoom 3; it now expects it in the Orbit and Cloud views at the default zoom.

## Tests
- New: ui6b_scrollbar, ui6b_bench, ui6b_rail, ui6b_nucleus (tests/check_6b.py), all pass. Failing-before shown for the scrollbar (all themes) and the bench window (Start outside its window at 880x560); rail and nucleus were not run against the old code (the old rail was 14 em, the old section absent by code).
- New env var PL_BENCH_RISK (test hook, ticks the speed-risk box). check.sh quick: 10 of 10 pass. Neighbours run and passing: ui5k_pages, ui5k_contents, ui5g_contents, controls_panel, ui5h_fit, ui5p_*, shell_pictures, atom_panel (after the update above). No full suite.

## Not tested
- Windows, a real GPU or mouse, speed. Only software OpenGL on Linux. The owner's own screen (100 % scale) was not reproduced beyond 2560x1369 renders.
