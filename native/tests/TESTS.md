# The UI tests

Every part of the ui_* tests, with its time and why it is kept or was deleted (0.76.0, DECISIONS 23 Nov 2026). This file replaces the long comments.
Rules for new checks are in CLAUDE.md (Build and test). Git keeps every deleted test.

- Times: seconds per part in 0.75.3, measured on the build machine with 4 parts at a time (LP_NUM_THREADS=1 each). "over 300": stopped at the 5-minute limit.
- KEEP: catches breakage a user would see, or compares a GPU picture or result with the CPU reference. DELETE: one old fixed bug, pixel layout, the same check
  repeated at sizes or themes, smoothness or speed. MERGE: none. No two parts could share one start without rewriting their checks.
- 154 parts before: 52 kept, 0 merged, 102 deleted. Split for the 5-minute part limit: classic into 5 parts, molecule card into 3, black hole precision into 3.
- The group of a kept part is its ctest test (native/CMakeLists.txt). run_group.py stops a part after 5 minutes.

## New and split parts

| Part | Seconds | Group | Test | Reason |
|---|---|---|---|---|
| check_shell.py:smoke | new | KEEP | ui_smoke | the app starts, each lab draws a frame that is not empty with zero validation errors and no swapchain image used before it is acquired (0.89.0), the app ends cleanly |
| check_atom_chip.py:classicside, classicsideglow, classicslice, classicsliceglow | 150 each | KEEP | ui_atom* | GPU vs CPU: the other four Classic scenes (split from classic); 0.80.1: the picture taken at the scene's 30th frame (by the clock, the slower CPU run was caught at another zoom) and the two runs side by side: 43-77 s each on the build machine |
| check_molecule.py:cardpic, cardpiclight | 172, 86 | KEEP | ui_molecule* | GPU vs CPU: the Density cloud's pictures (split from card) |
| check_blackhole.py:sky | 112 | KEEP | ui_blackhole_sky | GPU vs CPU: the bent sky per pixel against the double-precision reference (split from precision); 0.79.0: the streaks' factor at spin 0.99, and toneCompare's case "streaks" (the disc's surface, streaks and clump, at most 1 % of pixels over 8 levels) (0.81.0 to 0.85: Film's see-through disc, removed in 0.86.0 with that drawing, R6); 0.84.0: toneCompare's cases close, closespin, pole and closetable (the held camera's own shift at 2.2 GM/c², no spin, spin 0.99, over the pole, the table's pass); 0.87.0: cases hot, edgesky, edgetable (the disc's real light: the colour correction, the limb darkening at 85°), every case with its returning light, and the pixels whose radius range reaches past the disc's edge left out (the shader's coverage) |
| check_blackhole.py:precisioncard | 74 | KEEP | ui_blackhole_sky | GPU vs CPU: the card's light paths against the double-precision reference (split from precision); 0.87.0: PLBHRET, the returning light's table made on the card against the processor's (spin 0 and 0.99) |
| check_orbit_ui.py:planets | 22 (new in 0.77.0) | KEEP | ui_orbit_card | GPU vs CPU: six balls with maps (measured brightness, Earth's clouds and air) against tests/planet_picture.cpp |
| check_orbit_ui.py:review085 | 26 (new in 0.85.0, alone on the build machine) | KEEP | ui_orbit_card | GPU vs CPU: True sunlight (Earth, Jupiter, Neptune followed), an eclipse with the Sun's measured edge, the Sun with its sunspot outlines, against tests/planet_picture.cpp |

## The groups in 0.76.0 (seconds, build machine, 4 at a time)

| Test | Seconds | Test | Seconds |
|---|---|---|---|
| ui_orbit_card | 431 | ui_molecule_card | 275 |
| ui_atom_classic2 | 402 | ui_atom (with the failing classicsliceglow) | 249 |
| ui_blackhole_sky | 371 | ui_blackhole | 243 |
| ui_atom_classic | 358 | ui_shell | 232 |
| ui_orbit | 290 | ui_molecule | 203 |
| ui_atom_card | 180 | ui_screens | 112 |
| ui_smoke | 6-16 | | |

The GPU-vs-CPU parts take about 40 of the suite's 64 core-minutes and the unit tests about 8, so the full suite cannot reach 10 minutes on 4 cores without deleting a GPU-vs-CPU comparison (REPORT.md).

## Every part of 0.75.3

| Part | Seconds | Group | Test | Reason |
|---|---|---|---|---|
| check_shell.py:gpuhelper | 300 | KEEP | ui_shell | the crash-safe helper (trimmed: the hang and silent-helper cases of 0.68.1 deleted, old fixed bugs) |
| check_shell.py | 300 | DELETE | - | text sizes at six scale pairs, window layout, keys, the reading view: pixel-exact layout; keys are in check_022 keys |
| check_controls.py | 300 | DELETE | - | panel geometry, halos in 11 themes, readout contrast per theme: layout and theme repeats |
| check_6b.py:benchscenes | 300 | DELETE | - | every scene's set-up and moving picture: the owner's benchmark shows them |
| check_5k.py:pages | 300 | DELETE | - | wheel and keys scroll pages at several sizes: one old fixed bug, repeated |
| check_5l.py:search | 300 | DELETE | - | search with late clicks and pixel comparison: repeats check_022s7 search |
| check_orbit_ui.py:coverage | 300 | DELETE | - | every inventory item of 0.15: the old migration; the main controls are in check_shell_state |
| check_atom_panel.py | 300 | DELETE | - | the Atom panel against the old panel's saved states: an old migration; the main controls are in check_atom_toolbar and check_atom_chip |
| check_atom_chip.py:classic | 737 | KEEP | ui_atom_classic | GPU vs CPU: the Classic dots, card against processor; split into five parts, one per scene (737 s, over the 5-minute limit) |
| check_5h.py:fit | 300 | DELETE | - | the sheet fits at three sizes: pixel layout |
| check_molecule.py:card | 465 | KEEP | ui_molecule | GPU vs CPU: the Density cloud's start-up check against the processor (guard runs trimmed to one step); its pictures are parts cardpic and cardpiclight |
| check_blackhole.py:precision | 300 | KEEP | ui_blackhole | GPU vs CPU: the card's light paths and bent sky against the double-precision reference; its sky and card halves are now parts sky and precisioncard; 0.79.0: the disc's surface radius (PLBHGAS0 rbad) and the streaks' factor at spin 0; 0.86.0: PLBHFILM checks the one colour of Film's opaque disc on the table's pass (0.81.0 to 0.85: the see-through disc's laid colour) |
| check_shell.py:vulkan2d | over 300 | DELETE | - | Vulkan coverage views with validation, 1500-frame runs (2 h once). Its glare check compares the GPU with the GPU, not with the CPU (job rule: delete). The GPU light paths against the CPU stay in check_blackhole precision |
| check_blackhole.py:smooth | 596 (GitHub) | DELETE | - | frame times and new pictures per frame: smoothness (failed in 0.75.3, job rule: delete) |
| check_atom_chip.py:card | 319 | KEEP | ui_atom_card | GPU vs CPU: the Slice's dots and the jump's groups against the processor (guard runs trimmed to one step per program) |
| check_5l.py:toolbar | 295 | DELETE | - | toolbar clicks at sizes and themes: repeats |
| check_orbit_ui.py:facts_long | 286 | DELETE | - | the longest fact in two lines: layout |
| check_6c.py:fullzoom | 260 | DELETE | - | draw counts at full zoom: a speed check (the owner's benchmark) |
| check_cards.py:atom | 257 | DELETE | - | the Atom sheet against the old panel at two sizes: an old migration |
| check_blackhole.py:gas | 248 | DELETE | - | the moving gas: the clock formulas are unit-tested (blackhole_physics, testGas) |
| check_6b.py:benchwaits | 244 | DELETE | - | the waits are timed to the finished picture: timing, the owner's benchmark judges it |
| check_5i.py:sheet | 243 | DELETE | - | the sheet above the toolbar at three sizes: pixel layout |
| check_5q.py:lit | 239 | DELETE | - | lit buttons in 11 themes: theme repeats |
| check_5l.py:legend | 232 | DELETE | - | the legend at two sizes: pixel layout |
| check_5h.py:centre | 230 | DELETE | - | the Atom picture's centre at three sizes: pixel layout |
| check_orbit_ui.py:rings | 213 (0.78.0: 30 alone on the build machine, was 68; 0.7 s counted, was 2.0) | KEEP | ui_orbit_card | GPU vs CPU: Saturn's rings on the card against the processor |
| check_orbit_ui.py:facts | 201 | DELETE | - | the fact line's timing and places: layout |
| check_5p.py:picker | 199 | DELETE | - | the picker's place: pixel layout |
| check_5k.py:contents | 191 | DELETE | - | the Contents list three times in a row: one old fixed bug |
| check_orbit_ui.py:search | 183 | DELETE | - | 20 named Orbit things: repeats of check_022s7 search |
| check_atom_chip.py | 174 | KEEP | ui_atom | the element chip: the table opens, elements across the table can be picked (12 of 118 now: the same click repeated), steppers, words |
| check_022.py:camera | 170 | DELETE | - | glides, coasting and easing of the camera: smoothness |
| check_orbit_ui.py:readable | 159 | DELETE | - | fact line contrast: covered by the look test |
| check_6c.py:nucglow | 154 | DELETE | - | the new glow against the old glow: GPU against GPU, a speed change of 0.66.0 |
| check_5q.py:sliders | 150 | DELETE | - | sliders in 11 themes and 2 labs: theme repeats |
| check_blackhole.py:short | 150 | DELETE | - | every old control reachable under More settings: one old migration (failed once in 0.75.3) |
| check_shell_state.py | 146 | KEEP | ui_orbit | the Orbit Lab's main controls give the old panel's states (time, toggles, 3D, pick, save picture) |
| check_5g.py:pages | 144 | DELETE | - | pages scroll at three sizes: repeats of check_5k pages |
| check_molecule.py:cardsurface | 133 | KEEP | ui_molecule_card | GPU vs CPU: the Density surface, card against processor |
| check_5m.py:themes | 131 | DELETE | - | archived themes open as Dark: an old migration |
| check_5p.py:wheel | 128 | DELETE | - | the wheel after a window closed: one old fixed bug |
| check_6a.py:gaps | 126 | DELETE | - | old-panel features in three themes: an old migration |
| check_022.py:depth | 122 | DELETE | - | the 3D camera's depth: the pinhole is unit-tested (test_camera.inc); the rest is the camera's motion |
| check_orbit_ui.py:flyby | 117 | KEEP | ui_orbit_card | GPU vs CPU: the fly-by scene, card against processor |
| check_5l.py:popups | 111 | DELETE | - | pop-ups opened with a late click: one old fixed bug |
| check_layout020.py:u238 | 108 | DELETE | - | the info text clear of the nucleus: one old fixed bug |
| check_5q.py:picker | 105 | DELETE | - | the picker's place: pixel layout |
| check_6c.py:ds281sea | 98 | DELETE | - | draw counts with the sea: a speed check |
| check_blackhole.py:whiteout | 98 | KEEP | ui_blackhole_sky | GPU vs CPU: no white frame where the processor's settled picture is not white |
| check_5m2.py:windows | 94 | DELETE | - | windows inside the window at two sizes: pixel layout |
| check_5g.py:atomtext | 94 | DELETE | - | the info text clear of the card: pixel layout |
| check_shell_pictures.py | 94 | DELETE | - | glass opacity in 9 themes: the look test is check_5t grey |
| check_5h.py:cards | 92 | DELETE | - | the sheet tabs against the old panel: an old migration |
| check_orbit_ui.py:acen | 90 | DELETE | - | the Alpha Centauri switch, Fit buttons and words: the data have unit tests (orbit_physics, test_orbit.cpp) |
| check_blackhole.py:looks | 89 | DELETE | - | the Film look's locks and six themes' contrast: theme repeats |
| check_cards.py:orbit | 88 | KEEP | ui_orbit | the Orbit card opens, follows the body, its sheet opens and closes (one size now) |
| check_6b.py:orbitkeep | 86 | DELETE | - | the Orbit Lab's kept picture at rest: a frame-time check |
| check_5m2.py:tilt | 83 | DELETE | - | the chart tilt pill: pixel layout |
| check_5q.py:endkey | 82 | DELETE | - | the End key in the panel: one old fixed bug |
| check_blackhole.py:truelight | 79 | DELETE | - | true brightness and glare: the exposure rule is unit-tested (blackhole_physics); six themes' contrast |
| check_022.py:followmoon | 74 | DELETE | - | the camera follows a moon: smoothness of the camera; the camera logic has unit tests (test_camera.inc) |
| check_orbit_ui.py:card | 69 | DELETE | - | the sheet against the old card: an old migration |
| check_5g.py:contents | 66 | DELETE | - | the Contents list: one old fixed bug |
| check_orbit_ui.py:cardlines | 62 | KEEP | ui_orbit_card | GPU vs CPU: trails, orbits and rings on the card against the processor |
| check_5k.py:windows | 62 | DELETE | - | Settings then Benchmark inside the window: pixel layout |
| check_orbit_ui.py:chip | 59 | DELETE | - | the chip's place while time runs: pixel layout |
| check_5g.py:ring | 57 | DELETE | - | the phase ring clear of the card at three sizes: pixel layout |
| check_atom_toolbar.py | 57 | KEEP | ui_atom | the Atom Lab's views, levels, reset and drag give the right states |
| check_5m2.py:popups | 55 | DELETE | - | pop-ups inside the window at two sizes: pixel layout |
| check_024.py:densvib | 54 | DELETE | - | the vibrating density: unit-tested (test_density.cpp) |
| check_blackhole.py:disc | 70 | KEEP | ui_blackhole | the gas disc's controls and the papers' hottest ring (0.89.0: exactly with the returning light off; with it, the papers' ring at the readout's radius plus its share; the card's f = Done et al.'s; black-body rings brighter with the feeding rate, the colour-corrected ones less so) |
| check_blackhole.py:gasrow | 35 | KEEP | ui_blackhole | 0.82.1, a bug the owner saw: the Streaks and clump row reads Off or On and the picture changes; Film draws no locked controls; 0.84.0: the fine light paths at once and the gas held still (a caption above the row moved it under the click on a loaded machine) |
| check_blackhole.py:edgeon | 54 | KEEP | ui_blackhole | 0.82.1, a bug the owner saw, GPU vs CPU: the disc seen nearly edge on is smooth (57.5 and 1000 GM/c^2, 0.2 degrees round the plane) |
| check_orbit_ui.py:time | 45 | DELETE | - | the Time panel's fit: pixel layout |
| check_realistic.py | 44 | DELETE | - | the Realistic style's colours and draw counts: the Density pictures are compared with the processor in check_molecule card |
| check_5g.py:toolbar | 44 | DELETE | - | the toolbar's place: pixel layout |
| check_5m2.py:layout | 43 | DELETE | - | layout tokens: pixel layout |
| check_5t.py:solid | 43 | DELETE | - | a guide page is solid: one old fixed bug |
| check_022.py:density | 42 | DELETE | - | the nucleus' Density look is the default: one old decision |
| check_5k.py:calc | 42 | KEEP | ui_shell | the calculator opens and works |
| check_blackhole.py:spin | 41 | KEEP | ui_blackhole | the spin control and the papers' horizon values |
| check_5k.py:toggle | 39 | DELETE | - | the Settings button toggles: Settings opening is in check_5t tiles |
| check_5p.py:zoomout | 38 | DELETE | - | zoom-out limits: one old fixed bug |
| check_022s7.py:scalebar | 38 | DELETE | - | the scale bar's light time: one old fixed bug (0.22 stage 7) |
| check_022.py:spectrum | 38 | DELETE | - | the spectrum strip's lines: the wavelengths are unit-tested (atom_physics, test_atom046.inc) |
| check_5t.py:tiles | 35 | KEEP | ui_screens | Settings opens, its tiles hold their names, the picture dims |
| check_5q.py:scrollbar | 34 | DELETE | - | the scrollbar's width in pixels: pixel layout |
| check_022.py:centre | 34 | DELETE | - | the scene centred in the free part: pixel layout |
| check_6b.py:scrollbar | 33 | DELETE | - | the scrollbar thumb after reopening: one old fixed bug |
| check_5p.py:scroll | 33 | DELETE | - | scrolling one old fixed bug |
| check_022.py:facts | 32 | KEEP | ui_orbit | each lab shows its own facts |
| check_5k.py:data | 29 | KEEP | ui_shell | the Data panel opens and fits |
| check_blackhole.py:look | 28 | DELETE | - | text contrast in six themes: theme repeats |
| check_023.py:vib | 28 | DELETE | - | the vibration's phase: the physics is unit-tested (test_vibration.cpp) |
| check_024.py:densfit | 27 | DELETE | - | the surface fits the picture: one old fixed bug |
| check_layout020.py:sheet | 25 | DELETE | - | the sheet clear of the Legend chip: one old fixed bug |
| check_5t.py:grey | 25 | KEEP | ui_screens | the one look test of the themes: a selected segment and a checked box in four themes, WCAG contrast |
| check_6b.py:rail | 25 | DELETE | - | the Contents rail's width: pixel layout |
| check_025.py:molecule | 25 | DELETE | - | Most realistic: the same check as check_025 orbit |
| check_6c.py:nucdens | 24 | KEEP | ui_atom_card | GPU vs CPU: the nucleus' Density look against the processor |
| check_022.py:keys | 23 | KEEP | ui_orbit | 1 2 3 open the labs, + and - zoom, R resets |
| check_022.py:axes | 23 | DELETE | - | no ring label on the axes: one old fixed bug |
| check_6c.py:nucdenscard | 20 | KEEP | ui_atom_card | GPU vs CPU: the same on the card |
| check_5k.py:matrix | 20 | DELETE | - | matrix layout at two sizes: pixel layout |
| check_6b.py:benchrestore | 67 | KEEP | ui_screens | after the run every lab, page and panel is as it was, and the window ui.resize_blackhole resized is put back exactly (0.83.0; 67 s on the build machine) |
| check_022.py:stage6 | 18 | DELETE | - | the owner's second look-through of 0.22: old fixed bugs |
| check_shell.py:guardturn | 18 | KEEP | ui_shell | a GPU program that crashes the driver is switched off at the next start, the others stay on |
| check_orbit_ui.py:links | 18 | DELETE | - | a link's underline: pixel detail |
| check_025.py:atom | 17 | DELETE | - | Most realistic: the same check as check_025 orbit |
| check_5p.py:lit | 17 | DELETE | - | lit buttons: theme repeat of the same rule |
| check_5q.py:theme | 17 | DELETE | - | the theme list's place at three sizes: pixel layout |
| check_022s7.py:search | 17 | KEEP | ui_screens | search finds elements, molecules and the other labs |
| check_orbit_ui.py:ringslant | 16 | KEEP | ui_orbit_card | the rings' opacity against the textbook formulas and the Cassini profile |
| check_molecule.py | 16 | KEEP | ui_molecule | the Molecule Lab draws, picks, saves its settings |
| check_5r.py:pill | 14 | DELETE | - | pill fills in four themes: pixel look |
| check_025.py:guide | 14 | DELETE | - | the guide's words: one old fix |
| check_orbit_ui.py:journey | 14 | DELETE | - | the Journey's caption: one old fixed bug |
| check_blackhole.py:alllight | 14 | KEEP | ui_blackhole | All light's sentence and its effect |
| check_shell.py:vulkan | 13 | KEEP | ui_shell | the app starts on Vulkan with the validation layers, zero errors, every start-up step logged, the swapchain made again |
| check_5g.py:panel | 13 | DELETE | - | a section scrolls into view: one old fixed bug |
| check_6b.py:bench | 13 | KEEP | ui_screens | the Benchmark window opens with Start in view |
| check_022.py:molbar | 12 | DELETE | - | the bar's buttons: one old fixed bug |
| check_orbit_ui.py:farnote | 12 | DELETE | - | the notes zoomed far out: one old fixed bug |
| check_audit19.py:molbar | 12 | DELETE | - | no accent colour in the bar: one old fixed bug |
| check_023.py:orbits | 11 | KEEP | ui_orbit | Orbit paths switch the drawing and are saved |
| check_022.py:honest | 10 | KEEP | ui_atom | the honest names and captions (the limits) are on screen |
| check_022s7.py:jumpcheck | 9 | DELETE | - | the same-n note written once: one old fixed bug |
| check_025.py:orbit | 9 | KEEP | ui_orbit | Most realistic in the Orbit Lab |
| check_layout020.py:molpan | 8 | DELETE | - | the pan limit: one old fixed bug |
| check_molecule.py:rotation | 8 | KEEP | ui_molecule | the Rotation section's controls |
| check_022s7.py:space | 8 | DELETE | - | Space in the Molecule Lab: one old fixed bug |
| check_6b.py:nucleus | 8 | DELETE | - | nucleus controls clickable at the default zoom: one old fixed bug |
| check_blackhole.py:distance | 8 | KEEP | ui_blackhole | the shadow's size against Synge's formula |
| check_blackhole.py:light | 7 | KEEP | ui_blackhole | bending and sky controls change the picture |
| check_5k.py:log | 7 | KEEP | ui_shell | the changelog page opens |
| check_022s7.py:dateenter | 7 | DELETE | - | Enter in the date box: one old fixed bug |
| check_5g.py:default | 6 | DELETE | - | the Atom Lab starts in Cloud 3D: one old fixed bug |
| check_blackhole.py:settings | 6 | KEEP | ui_blackhole | settings are saved and loaded |
| check_blackhole.py:layout | 6 | DELETE | - | the bar and card at two sizes: pixel layout |
| check_audit19.py:molsearch | 6 | KEEP | ui_screens | search in the Molecule Lab lists nothing that does nothing |
| check_blackhole.py:open | 6 | KEEP | ui_blackhole | the lab opens and draws; a drag circles the camera as the Orbit Lab's, 1.0 rad for 100 points plus the glide (0.82.0) |
| check_6b.py:benchrest | 6 | KEEP | ui_screens | a busy lab is said to be busy at rest |
| check_022.py:factsrc | 6 | KEEP | ui_orbit | a fact names its source and opens it in About |
| check_5k.py:about | 6 | KEEP | ui_shell | About opens with its last line |
| check_5h.py:infosize | 5 | DELETE | - | the info text size: pixel layout |
| check_shell.py:novulkan | 5 | KEEP | ui_shell | a PC without Vulkan gets the message and a clean exit, not a crash |
| check_blackhole.py:presets | 5 | KEEP | ui_blackhole | the presets set the papers' numbers |
| check_orbit_ui.py:airedge | 4 | KEEP | ui_orbit_card | no rim of air at Earth's limb (the picture against the physics) |
| check_molecule.py:anharm | 4 | KEEP | ui_molecule | the anharmonic bond's controls |
| check_blackhole.py:orbitcard | 4 | KEEP | ui_blackhole | the Orbit Lab's card opens the lab |
| check_022.py:words | 4 | DELETE | - | the view buttons carry words: pixel layout |
| check_orbit_ui.py:nightside | 4 | KEEP | ui_orbit_card | Jupiter's night side is dark (the picture against the physics) |
| check_022.py:start | 2 | DELETE | - | the start zoom and speed: one old fixed bug |
| check_024.py:nobuild | 2 | DELETE | - | no Build mode: one old decision |
| check_blackhole.py:realest | 2 | KEEP | ui_blackhole | Most realistic |
| check_022s7.py:factmol | 2 | DELETE | - | the fact line clear of the Molecule bar: one old fixed bug |

## The owner's PC (0.85.1)

The checks that need real hardware. They run on the owner's PC (AMD Radeon RX 9060 XT, 2560 x 1369 window, 255 Hz) after the push, on the new release (CLAUDE.md "Build and test").
- The package: PhysicsLab-tests-windows-x64.zip on the "latest" release, beside the app's zip (the Windows workflow's job test-package). It holds vk_compare_tests.exe
  (the card programs and the renderer's pictures against the CPU: what ctest -R "glsl_shaders|vk_cards" runs on Linux; 0.85.1: it takes a discrete GPU first),
  planet_picture.exe and blackhole_picture.exe (the CPU twins), the UI check scripts (tests\, they need Python 3) and pc_tests.ps1.
- The runner: native/tools/pc_tests.ps1. Windows PowerShell 5.1 only, nothing installed. Unzip both zips into one folder and run
  `powershell -ExecutionPolicy Bypass -File pc_tests.ps1` (or `-Parts smoke,bh`). Results: results\pc-results.txt, one PASS, FAIL or INFO line per check.
  The app runs with its own settings file in results\ (PL_SETTINGS), so the owner's settings stay untouched. Each app start stops after 14 minutes, the benchmark after 20.

| Part | What | From |
|---|---|---|
| device | the GPUs, drivers, Vulkan version and refresh rate (Windows and vulkaninfo); Python present or not | new |
| smoke | the app starts on Vulkan, each of the four labs draws a frame that is not empty, exit code 0 | check_shell.py:smoke |
| vkcards | vk_compare_tests on the real driver: every comparison within its limit | vk_cards |
| bh | the sky's start-up checks, the camera curve (near, far, spin, streaks, close, closespin, pole, closetable, hot, edgesky, edgetable), the full picture at 2560 x 1369, the shadow's edge, the edge-on views near pitch 90 degrees | check_blackhole.py:sky, precision, edgeon |
| planets | True sunlight (Earth, Jupiter, Neptune), the 2024 eclipse, the Sun with its 7 sunspot groups, against planet_picture | check_orbit_ui.py:review085 |
| bench | the full benchmark (PL_BENCH=start), its result file kept | new |
| logs | every [vulkan] line of each run's gpu-paths-log.txt and of the owner's own (read only); a path off, died or drawn anew is a FAIL | new |
| validation | the four labs with the Khronos validation layers, if installed | check_shell.py:smoke |

Not in the runner: the light paths' card comparison (check_blackhole.py:precisioncard). The light paths' pipeline crashes the owner's driver (ROADMAP, Black Hole Lab).
