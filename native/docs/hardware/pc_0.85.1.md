# Real-hardware checks, 0.85.1, the owner's PC (25 Nov 2026)

Run with native/tools/pc_tests.ps1 on release 0.85.1 (commit f6043ed): PhysicsLab-windows-x64.zip and PhysicsLab-tests-windows-x64.zip in one folder.
Every line of the three runs is in pc_0.85.1_results.txt. The benchmark's file is bench_0.85.1.txt.
The app in 0.85.1 is the app of 0.85.0 (this job changed no app code).

## 3a. The device

| What | Value | From |
|---|---|---|
| CPU, memory | AMD Ryzen 5 7500X3D, 6 cores, 12 threads, 32 GB | the benchmark's file |
| GPU the app uses | AMD Radeon RX 9060 XT (discrete) | the app's "Vulkan:" line, gpu-paths-log.txt card 0 |
| Second GPU | AMD Radeon(TM) Graphics (integrated, Vulkan 1.4.315, driver 2.0.353). The app takes card 0, the RX 9060 XT | gpu-paths-log.txt card 1, vulkaninfo |
| Driver | AMD proprietary driver 2.0.416, 26.11.2 (LLPC). Windows driver 32.0.32029.2010 | the app, vulkaninfo, Windows |
| Vulkan | 1.4.359 (the card's), loader 1.4.357. Double precision in shaders: yes | the app's log |
| Screen | main 2560 x 1440 at 255 Hz. A second screen at 165 Hz (left of the main one) | the benchmark's file "Display refresh rate: 255 Hz", Windows |
| Window | 2560 x 1369 maximised (the benchmark ran at that size) | the benchmark's file |
| Python | not installed (only the Microsoft Store alias). The UI check scripts cannot run there | python --version |
| Validation layers | not installed (only a game's own layer is registered) | the registry |

## The checks

| No. | Check | Result |
|---|---|---|
| 3b.1-4 | Start, each lab draws, exit 0: Orbit, Atom, Molecule, Black Hole | PASS (4 of 4, 1-2.5 s each, frames not empty) |
| 3b.5 | gpu-paths-log.txt [vulkan] lines of every run | FAIL: 4 findings (3b.6-3b.9) |
| 3b.6 | The light paths: the helper dies making their pipeline (0xC0000005); off for the version. The owner's own log (0.83.0) names kKerrTableCS variants step, ray and short | FAIL (known: ROADMAP **fix**) |
| 3b.7 | The spinning hole's sky per pixel: "the first full picture" did not finish within its time limit in the benchmark run (begun at 1.3 s, stopped at 11.3 s). Off for the version. The spinning-hole scenes then ran on the CPU table. Same in the 0.85.0 trial run at 1400 x 860. In the comparison runs the same path passed its start-up check | FAIL (new) |
| 3b.8 | The Orbit Lab at rest drawn anew for 30 frames in a row. The key's part that changed: the selection, follow and camera (yaw, pitch, the held place, the glide) in 25-30 of 30 frames | FAIL (= 3e.1) |
| 3c.1 | vk_compare_tests on the RX 9060 XT | FAIL: crashed 3 of 3 runs (exit 0xC0000005, 23-28 s). Twice in amdvlk64.dll (offset 0x1491cbc), once in vk_compare_tests.exe (0x126a0). Its output stops after "Vulkan surface, water vibrating" (stdout is buffered, so the last check run is not known) |
| 3c.2 | vk: Glow soft dots, Wheel, dark page (additive) | FAIL: 585 of 200000 pixels differ, at most 2 levels (limit: 1 level) |
| 3c.3 | vk: rocks scene 1 (6000) and scene 6 (48000), regression guard | FAIL: worst place 0.00578 points (guard 0.005777) |
| 3c.4 | vk: "no card programs" outline 2p z Cloud 3D against the CPU-outline picture | FAIL in run 2 (not in run 1, which crashed earlier) |
| 3c.5 | vk: every other check printed before the crash (clouds, wheel, outlines, lines, surfaces) | PASS |
| 3c.6 | Black Hole: the sky's start-up check, its camera curve probe (0 of 3840 over) | PASS |
| 3c.7 | Black Hole tone: near 4, far 1, spin 5, streaks 1, closespin 218, pole 101, closetable 6 pixels over 8 levels (all under 1 %) | PASS |
| 3c.8 | Black Hole tone: close (spin 0, 2.2 GM/c²) | FAIL: 102 of 5120 disc pixels over 8 levels = 1.99 % (limit 1 %), max 82. The app's own line says ok=1 |
| 3c.9 | Film's see-through disc, two tilts (2 and 1 pixels over) | PASS |
| 3c.10 | Film at 2560 x 1369, four tilts (86.56, 88, 90, 92 degrees), every pixel against double precision | PASS (none over) |
| 3c.11 | The shadow's edge and bent sky at 1000 and 2.5 GM/c² | PASS |
| 3c.12 | The shadow's edge and bent sky at 27 GM/c² | FAIL: from 0.1 px of the edge 2.795e-05 rad (limit 1e-5). The edge 3.1e-06 px and 1 px out 4.8e-06 rad pass |
| 3c.13 | Orbit Lab against planet_picture: True sunlight Earth 163 of 61456, Jupiter 31 of 48696, Neptune 194 of 58242; eclipse Earth 192 of 61456 (over 8 levels) | PASS (all under 1 %) |
| 3d | The full benchmark, Hard scenes, 2560 x 1369, 255 Hz, 305 s | PASS (ran, saved). 14 of 30 Hard scenes reach 255 Hz. No earlier saved run in the repo to compare |
| 3e.1 | rest.orbit: the Orbit Lab at rest | FAIL: busy, drawn anew 765 times in 3 s (0.75.0: 766). Reproduced |
| 3e.2 | The static near the disc's plane, pitch near 90 degrees: 16 views at 57.5 and 1000 GM/c² | PASS: the card = the double lookup (at most 4 of 628432 pixels), the near disc smooth (at most 4 of 508598 over 8 levels) |
| 3e.3 | ui.resize_blackhole from the maximised window | PASS: put back to 2560 x 1369, still maximised. But no new Black Hole picture while resizing from 2560 x 1369 (0 in 5.2 s, lag 1380 ms; the 1400 x 860 trial: 94 a second) |
| 3e.4 | Full screen (F11 twice over the Black Hole Lab) | PASS in the trial on the same app (a swapchain at 2560 x 1440, exit 0). Skipped in the final run (-NoKeys: the owner was using the PC) |
| 3e.5 | rest.blackhole_glare | FAIL: busy 46.2 % CPU, 766 redraws. With the moving gas off: still busy, 13.1 % CPU, 2 redraws. The gas explains the redraws, not all the CPU |
| 3e.6 | Scenes below 255 Hz (ROADMAP: film_tilt, real_move, extreme, all_light) | all four still below (pictures 7.4, 34.8, 43.9, 42.1 a second), on the CPU table (3b.7) |
| 3f.1 | blackhole.close_spin | 881.7 fps, 1 % low 491.5, 38.5 pictures a second, lag 119 ms: does not reach (CPU table, 3b.7) |
| 3f.2 | orbit.true_light | 619.7 fps, 1 % low 339.9: reaches |
| 3f.3 | orbit.sun_edge | 780.3 fps, 1 % low 505.7: reaches. The Sun's 7 groups drawn, GPU = CPU (93 of 115920 over 8 levels) |
| 3f.4 | orbit.oort_region | 817.8 fps, 1 % low 381.7: reaches |
| 3g | Validation layers | not run: not installed (installing them needs the owner's yes) |

Other scenes below 255 Hz in the benchmark: atom.emit_all (1 % low 3.97, worst frame 365 ms), atom.nucleus_zoom (1 % low 246.7, limited by the GPU at 2560 x 1369), orbit.saturn_real_fast (230.6), every Black Hole scene that makes pictures off the frame.
