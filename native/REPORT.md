# 0.85.1: Real-hardware tests on the owner's PC; a Windows test package; the rule that real-hardware checks run there

## What changed
- Read: CLAUDE.md, REPORT 0.85.0, ROADMAP, PLANNER ("The owner's PC"), TESTS.md; by grep and line ranges native-windows.yml, CMakeLists.txt (vk_compare_tests, ui_* groups, PL_VK_TESTS), benchmark.h/.cpp (PL_BENCH, PLBENCH-RESULT, the saved file, PL_BENCH_ONLY, rest lines), bench.cpp (benchFileName), gpu_guard.h/.cpp (the log), main.cpp (PL_* settings, createWindow), settings.cpp (PL_SETTINGS), shot.cpp (PL_SHOT_DIR), prefs.cpp (window line), shell_test.cpp (PL_SCRIPT keys), run_group.py, shell_lib.py, check_shell.py smoke, check_blackhole.py (sky, precision, edgeon), check_orbit_ui.py (review085, compare_cpu), vk_ctx.inc, planet_picture.cpp.
- The Windows workflow's new job test-package builds vk_compare_tests, planet_picture and blackhole_picture and adds PhysicsLab-tests-windows-x64.zip (with the UI scripts and the runner) to "latest". The app's zip is unchanged.
- native/tools/pc_tests.ps1: the runner (PowerShell 5.1, installs nothing, its own settings file). Parts device, smoke, vkcards, bh, planets, bench, items, logs, validation. vk_compare_tests takes a discrete GPU first (the owner's PC has a second, integrated Radeon).
- Rule in CLAUDE.md "Build and test", PLANNER.md step 3, DECISIONS.md (and the owner's one-off: two pushes for this job), TESTS.md. Results: native/docs/hardware/ (pc_0.85.1.md, its results file, bench_0.85.1.txt).
- Run on this session's machine, which is the owner's PC (no Remote Control needed). Its MSVC 14.44 cannot build the app (C2026: kAboutHtml and shaders.h strings over 16 KB; GitHub's newer compiler can), so the package comes from GitHub.

## Tests run
- 0.85.0 (62a4a24): Windows run green, "latest" targeted it. Full tests failed ui_orbit (check_shell_state "the same fields"): ROADMAP **fix**. 0.85.1 first commit f6043ed: Windows run green with test-package; the release has both zips.
- On the owner's PC (RX 9060 XT, driver 26.11.2, Vulkan 1.4.359, 255 Hz): 4 of 4 labs start and draw. Black Hole: 19 checks pass, tone close 1.99 % (limit 1 %) and the sky at 27 GM/c² 2.8e-5 rad (limit 1e-5) fail. Edge-on near 90 degrees: 16 of 16 pass (the static is gone there). Orbit review085: 5 of 5 pass.
- vk_compare_tests crashed 3 of 3 runs (0xC0000005, twice in amdvlk64.dll). Before the crash: Glow additive 2 levels (limit 1), the rocks' guard 0.00578 (0.005777), one outline picture fail.
- Benchmark at 2560 x 1369: 14 of 30 Hard scenes reach 255 Hz. The spinning hole's GPU sky switched itself off in that run (its first full picture over its time limit), so the spinning-hole scenes ran on the CPU table. rest.orbit 765 redraws (reproduced). rest.blackhole_glare 46 % CPU, 13 % with the gas off. Resize from the maximised window: 0 new pictures.
- Logs: light paths' helper died (0xC0000005) on kKerrTableCS, known. No path fell back in the smoke and comparison runs.

## Not done
- Validation layers and the UI check scripts: neither the layers nor Python are installed on the owner's PC (ROADMAP: owner decides). Nothing was installed.
- Full screen: passed in the trial on the same app, skipped in the final run (-NoKeys) because the owner was using the PC. The light paths' card comparison (precisioncard) was not run: their pipeline crashes this driver.
- No earlier saved benchmark in the repo to compare with. bench_0.85.1.txt is the first. Two pushes for this job (the owner's one-off, DECISIONS).

## References changed on purpose
- None.
