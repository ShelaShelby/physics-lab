# Real-hardware checks, 0.85.2, the owner's PC (25 Nov 2026)

Run with native/tools/pc_tests.ps1 on release 0.85.2 (commit e35d00f) in C:\Users\alexm\Desktop\CPhysicsLab085: one run of every part, no -NoKeys (the owner left the PC free), windows on the 255 Hz main screen.
Every line of the run is in pc_0.85.2_results.txt. The benchmark's file is bench_0.85.2.txt. Two more runs of vk_compare_tests alone (for the stop rule and to name its crash) are in "Causes found".
The exe has no version resource (the setup line prints an empty version); its newest history entry is "0.85.2 Better checks after ...", so it is 0.85.2.

## 3a. The device

Unchanged from pc_0.85.1.md: AMD Ryzen 5 7500X3D, 32 GB; AMD Radeon RX 9060 XT (card 0, used), AMD proprietary driver 2.0.416, 26.11.2 (LLPC), Windows driver 32.0.32029.2010; Vulkan 1.4.359; main screen 2560 x 1440 at 255 Hz, window 2560 x 1369 maximised; Python and the validation layers not installed.

## The checks

| No. | Check | Result |
|---|---|---|
| 3b.1-4 | Start, each lab draws, exit 0: Orbit, Atom, Molecule, Black Hole | PASS (4 of 4) |
| 3b.5 | gpu-paths-log.txt [vulkan] lines of every run | FAIL: the light paths (3b.6), the sky (3b.7), the Orbit Lab at rest (3b.8) |
| 3b.6 | The light paths: the helper dies making their pipeline (0xC0000005); off for the version. The owner's own log names kKerrTableCS variants step, ray and short | FAIL (known: ROADMAP **fix**) |
| 3b.7 | The spinning hole's sky per pixel: "the first full picture" begun at 1248 ms, not finished within its time limit (11246 ms); off for the version. Cause found (C1) | FAIL |
| 3b.8 | The Orbit Lab redrawn for 30 frames in a row at rest. Cause found (C3) | FAIL (= 3e.1) |
| 3c.1 | vk_compare_tests on the RX 9060 XT | FAIL: crashed (0xC0000005, 23.8 s), last part begun fallback33. Cause in C2 |
| 3c.2 | vk: Glow soft dots, Wheel, dark page (additive) | FAIL: "differ by at most 1 level(s)" not met |
| 3c.3 | vk: rocks scene 1 (6000) and scene 6 (48000), regression guard 0.005777 | FAIL: worst 0.00578 points (both scenes) |
| 3c.4 | vk: "no card programs: it is the CPU's outline" | FAIL |
| 3c.6 | Black Hole: the sky's start-up check, camera curve probe (0 of 3840 over) | PASS |
| 3c.7 | Black Hole tone: near 4, far 1, spin 5, streaks 1, closespin 218, pole 101, closetable 6 pixels over 8 levels (all under 1 %) | PASS |
| 3c.8 | Black Hole tone: close (spin 0) | FAIL: 102 of 5120 over 8 levels (share 0.01992, limit 1 %), max 82. PLBHTONE now says ok=0 expok=1 |
| 3c.9 | Film's see-through disc, two tilts (2 and 1 pixels over) | PASS |
| 3c.10 | Film at 2560 x 1369, four tilts, every pixel against double precision | PASS (none over) |
| 3c.12 | The shadow's edge and bent sky at 27 GM/c² | FAIL: from 0.1 px of the edge 2.795e-05 rad (limit 1e-5). The edge 3.139e-06 px, 1 px out 4.761e-06 rad pass |
| 3c.13 | Orbit Lab against planet_picture: True sunlight Earth 163 of 61456, Jupiter 31 of 48696, Neptune 194 of 58242; eclipse Earth 192 of 61456; the Sun 93 of 115920 (over 8 levels) | PASS |
| 3d | The full benchmark, Hard scenes, 2560 x 1369, 255 Hz | PASS (ran, saved). 15 of 30 Hard scenes reach 255 Hz (0.85.1: 14 of 30) |
| 3e.1 | rest.orbit | FAIL: busy, drawn anew 765 times in 3 s. Every redraw: "the key unchanged: the kept picture not used" (C3) |
| 3e.2 | The static near the disc's plane, 16 views at 57.5 and 1000 GM/c² | PASS (the card = the double lookup, at most 4 of 628432; the near disc at most 4 of 508598 over 8 levels) |
| 3e.3 | ui.resize_blackhole from the maximised window | PASS (put back to 2560 x 1369, maximised). But 0 new pictures in 5.21 s, lag 1505.5 ms: the same stall as 3b.7 (C1) |
| 3e.4 | Full screen, F11 twice over the Black Hole Lab | PASS (exit 0, a swapchain at 2560 x 1440) |
| 3e.5 | rest.blackhole_glare | FAIL: bench 46.5 % CPU, 765 redraws. Item runs: moving gas on 25.3 %, 410 redraws, worker busy; gas off 13.2 %, 2 redraws, worker busy (C5) |
| 3f.1 | blackhole.close_spin | 912.0 fps, 1 % low 549.4, 41.6 pictures a second, lag 84.9 ms: does not reach (CPU table, C1) |
| 3f.2-4 | orbit.true_light, orbit.sun_edge, orbit.oort_region | reach (1 % low 344.9, 524.3, 406.2) |
| 3g | Validation layers | not run: not installed |

Summary line of the run: 53 passed, 12 failed.

## 3d. The benchmark against 0.85.1 (bench_0.85.1.txt), scene by scene

fps, 1 % low and worst frame (ms): 0.85.1 → 0.85.2. Faster/slower: the 1 % low by more than 5 %. Below: still below 255 Hz ("reaches" no). Limited by is unchanged in every scene except blackhole.edge_on (both → graphics card).

| Scene | fps | 1 % low | worst frame | Limited by (0.85.2) | Mark |
|---|---|---|---|---|---|
| atom.emit_all | 171.8 → 179.8 | 3.97 → 4.18 | 365.28 → 368.54 | processor | faster, below |
| blackhole.film_tilt | 965.3 → 1010 | 442.7 → 467.9 | 14.37 → 14.29 | processor (pictures off the frame) | faster, below (8.01 pictures/s) |
| blackhole.everything | 858.1 → 866.9 | 506.8 → 427.2 | 6.76 → 19.40 | processor (pictures) | slower, below (31.1 pictures/s) |
| blackhole.real_move | 1138 → 1150 | 451.6 → 426.0 | 8.67 → 8.21 | processor (pictures) | slower, below (35.5 pictures/s, lag 1437.7 ms) |
| blackhole.close_spin | 881.7 → 912.0 | 491.5 → 549.4 | 4.61 → 4.00 | processor (pictures) | faster, below (41.6) |
| blackhole.over_poles | 914.2 → 946.4 | 529.9 → 639.3 | 4.85 → 3.00 | processor (pictures) | faster, below (43.5) |
| blackhole.circle_spin | 965.6 → 991.4 | 639.3 → 600.6 | 2.59 → 4.86 | processor (pictures) | slower, below (44.0) |
| blackhole.all_light | 907.4 → 941.5 | 510.3 → 607.5 | 4.67 → 2.72 | processor (pictures) | faster, below (44.0) |
| blackhole.dolly_spin | 909.3 → 945.7 | 573.6 → 627.1 | 3.28 → 3.18 | processor (pictures) | faster, below (44.8) |
| blackhole.extreme | 904.1 → 927.5 | 434.0 → 480.8 | 5.59 → 4.86 | processor (pictures) | faster, below (45.0) |
| ui.resize_blackhole | 151.3 → 151.3 | 62.9 → 64.7 | 17.32 → 16.13 | processor (pictures) | below (84.5) |
| ui.settings_over_blackhole | 806.3 → 817.5 | 570.1 → 600.4 | 2.33 → 3.28 | processor (pictures) | faster, below (84.8) |
| blackhole.moving_gas | 918.2 → 944.5 | 611.2 → 650.9 | 2.49 → 3.54 | processor (pictures) | faster, below (85.4) |
| blackhole.film_soft | 839.9 → 871.1 | 516.5 → 578.1 | 2.78 → 3.14 | processor (pictures) | faster, below (210.8) |
| orbit.saturn_real_fast | 481.1 → 504.0 | 230.6 → 253.2 | 5.37 → 4.83 | both | faster, below (253.2 < 255) |
| atom.nucleus_zoom | 311.1 → 316.5 | 246.7 → 280.7 | 4.37 → 4.19 | graphics card | faster, now reaches |
| atom.nucleon | 343.8 → 343.3 | 281.2 → 294.0 | 3.80 → 4.42 | graphics card | reaches |
| molecule.vib_density | 349.4 → 360.0 | 303.6 → 328.6 | 4.39 → 3.24 | processor | faster, reaches |
| atom.slice_dirac_drag | 370.3 → 372.7 | 312.9 → 330.1 | 3.37 → 3.19 | processor | faster, reaches |
| ui.settings_open | 603.7 → 619.7 | 327.4 → 341.5 | 3.94 → 3.95 | processor | reaches |
| orbit.real_fast | 594.4 → 616.0 | 327.4 → 341.8 | 3.79 → 3.74 | processor | reaches |
| orbit.true_light | 619.7 → 628.0 | 339.9 → 344.9 | 3.26 → 2.98 | processor | reaches |
| atom.jump_all | 360.7 → 387.6 | 295.4 → 345.3 | 4.19 → 3.20 | processor | faster, reaches |
| atom.side_all_glow | 482.5 → 494.6 | 334.7 → 351.2 | 3.14 → 3.09 | both | reaches |
| orbit.heaviest | 640.2 → 662.7 | 339.7 → 357.4 | 3.56 → 3.53 | processor | faster, reaches |
| atom.all_enlarged | 413.0 → 420.2 | 349.3 → 376.6 | 3.07 → 3.37 | both | faster, reaches |
| orbit.oort_region | 817.8 → 847.1 | 381.7 → 406.2 | 3.47 → 3.24 | processor | faster, reaches |
| blackhole.edge_on | 644.5 → 659.7 | 403.3 → 435.8 | 2.79 → 2.41 | graphics card (was both) | faster, reaches |
| orbit.sun_edge | 780.3 → 792.7 | 505.7 → 524.3 | 5.81 → 5.64 | processor | reaches |
| molecule.live_hot | 902.6 → 942.2 | 655.8 → 684.4 | 1.76 → 1.92 | processor | reaches |

15 scenes still below 255 Hz (0.85.1: 16; atom.nucleus_zoom now reaches). The app is unchanged between 0.85.1 and 0.85.2 for these scenes, so the differences are run-to-run spread, not a code change. Every spinning-hole scene ran on the CPU table again (C1).

## Causes found

- **C1, the sky's first full picture (3b.7, 3e.3)**: a circular wait, not a slow picture. The sky's step "the first full picture" opens at 1248 ms (src/blackhole/blackhole_view.cpp:1277) and holds the guards' one-step turn. It can end only when VkBlackHole::times() counts a drawn sky picture (src/render/vk_blackhole.cpp:944, called at :1255). But VkBlackHole::lens() returns before drawing anything while the lens pass's pipeline is not made (src/render/vk_blackhole.cpp:1216, `prog_[pLens].state != 2`). That pipeline's step waits for the turn the sky holds (src/app/gpu_guard.cpp:125): the log says "the step 'the lens pass's pipeline' waits for its turn: ... (its step 'the first full picture') is open" at 3113.7 ms. So no sky picture is drawn, its finish is never counted, and the 10 s limit switches the sky off at 11246 ms (blackhole_view.cpp:1292). The lens pipeline is made right after (bench.out: "PLVKBH pipeline lens (kLensFS) made" after "PLBHSKY state=-1"). In ui.resize_blackhole the same: sky step open from 1282 ms, the lens pipeline begun only at 12695 ms, 0 pictures in the scene. On this PC the lens helper starts late (2884 ms) because the light paths' helper crashes first (died at 2189.6 ms), so the sky's step opens before the lens pipeline exists. Not measured: how long the picture itself takes at 2560 x 1369 (it was never drawn).
- **C2, vk_compare_tests' crash (3c.1)**: three crashes, two places. Run 1 (pc_tests): last part begun **fallback33**, after its check "no card programs: it is the CPU's outline" failed (tests/vk_compare.cpp:867); fault in vk_compare_tests.exe at offset 0x126f0. So it dies between tests/vk_compare.cpp:868 (report of outlinePicture with the failed outline) and the teardown before PART vulkanTwins (:871-922). Runs 2 and 3 (vk_compare_tests alone): last part begun **vulkanTwins**, after "Vulkan surface, water vibrating"; fault in amdvlk64.dll at offset 0x1491cbc (as 0.85.1). The next step is vkBlackHole's light paths: render::makeVkKerrCompute, the check "Vulkan light paths: the pipeline is made" (tests/vk_cards.inc:261-267), which builds kKerrTableCS, the pipeline whose helper dies the same way in the app (3b.6). Stop rule reached (3 crashes): not run again.
- **C3, rest.orbit (3e.1)**: reasons counted: "the key unchanged: the kept picture not used" 765 (no other reason). Cause: the main loop's frame counter only counts when PL_EXIT_AFTER_FRAMES is set (src/main.cpp:1070, `maxFrames && ++frame`), and the keep needs `frame > 0` (src/main.cpp:891). Without that test switch (the owner's app, the benchmark, pc_tests with PL_EXIT_AFTER_SECONDS) frame stays 0 and the kept picture is never used: PLORBITKEEP kept=0 in every run here (bench drawn=33612). That may be why the build machine did not reproduce it (not checked: REPORT 0.85.2 does not say how its rest run was started). The 30-frame log line (camera 30, the card's lines still being decided 30) is from the start, before the lines are decided (main.cpp:236-241), not the rest.
- **C4, the numbers**: tone close 102 of 5120 over 8 levels, share 0.01992 (limit 1 %), max 82, PLBHTONE ok=0 expok=1 (the new ok uses the limit). Bent sky at 27 GM/c²: dir01 2.795e-05 rad (limit 1e-5), dir1 4.761e-06, edge 3.139e-06 px, dir001 1.920e-04, dir0 6.719e-04. Same as 0.85.1: no cause in the logs.
- **C5, atom.emit_all and rest.blackhole_glare**: emit_all's three worst frames (368.5, 362.6, 358.2 ms) are the frame thread's own work: the card waited for 0.0 ms, one batch waited 0.4 ms (the exposure grids). 20 of 935 frames over 16.7 ms, 13 presses. The logs do not say which part of the press. rest.blackhole_glare: the worker is "busy" with the gas on and off; the benchmark's worker flag joins three causes (src/ui/benchmark.cpp:1603: the view not settled, glareBusy, the fine thread busy), so the log cannot say which one uses the CPU.
