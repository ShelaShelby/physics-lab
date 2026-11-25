# 0.85.2 results: the real-hardware run of 0.85.2 on the owner's PC

No app code changed (this PC cannot build the app: REPORT 0.85.1, MSVC error C2026). No VERSION bump, no changelog entry.

## What changed
- native/docs/hardware/pc_0.85.2.md (the checks, the benchmark against 0.85.1, "Causes found"), pc_0.85.2_results.txt (every line of results\pc-results.txt), bench_0.85.2.txt.
- ROADMAP.md: the open items rewritten with the cause found (file and line); nothing removed.

## Causes found
- The sky's "first full picture" (3b.7): never counted, not slow. A circular wait: the sky's open step holds the guards' turn (blackhole_view.cpp:1277); lens() draws nothing until the lens pipeline is made (vk_blackhole.cpp:1216); that pipeline's step waits for the turn (gpu_guard.cpp:125). Timed out after 10 s. Also why ui.resize_blackhole made 0 pictures.
- vk_compare_tests: run 1 died in PART fallback33 (after "it is the CPU's outline" failed, vk_compare.cpp:867; fault in the exe). Runs 2-3 died in PART vulkanTwins at the light paths' pipeline (vk_cards.inc:261-267; amdvlk64.dll 0x1491cbc).
- rest.orbit: all 765 redraws "the key unchanged: the kept picture not used". The frame counter only counts with PL_EXIT_AFTER_FRAMES (main.cpp:1070) and the keep needs frame > 0 (main.cpp:891).
- Tone close: 1.99 % (102 of 5120, max 82), PLBHTONE ok=0. Bent sky at 27 GM/c²: 2.795e-05 rad. No cause in the logs.
- atom.emit_all: the frame thread's own work (368.5 ms, card waits 0.0 ms); the part not named. rest.blackhole_glare: the worker flag joins three causes (benchmark.cpp:1603), so not named.

## Tests run
- pc_tests.ps1, every part, on the owner's PC (RX 9060 XT, 255 Hz, 2560 x 1369 maximised), no -NoKeys: 53 passed, 12 failed. Full screen (F11) passed.
- vk_compare_tests alone, 2 more runs (both crashed): the stop rule's 3 crashes reached.
- The benchmark against bench_0.85.1.txt: 15 of 30 Hard scenes reach 255 Hz (0.85.1: 14; atom.nucleus_zoom now reaches). The app is unchanged, so the differences are run-to-run spread.

## Not done
- check.sh not run: this PC cannot build the app.
- vk_compare_tests skipped after 3 crashes; its checks after the crash points did not run on this card.
- Validation layers and Python not installed: 3g and the UI check scripts not run.
- The fixes themselves: a later cloud job (ROADMAP).
- pc_tests.ps1 not changed. Its setup line prints an empty version (the exe has no version resource); 0.85.2 was checked from the exe's history text.
