# Native Physics Lab 0.16 stage 4: the in-app Benchmark trimmed to meaningful scenes

## What changed
- Default run: 11 scenes x 10 s (2 s warm-up + 8 s counted) = about 110 s (the window and the Performance card say "about 110 seconds"; the card now counts the 11, not the whole list).
  Order: 1 Orbit Lab heaviest, 2 Orbit Lab 8x rocks, 3 nucleus view Og-294, 4 full-zoom nucleus (camera moving), 5 one nucleon, 6 phase colours Wheel / Cloud 3D / 48,000, 7 cloud jump in flight, 8 superposition 2p z + 3d z2, 9 first use of an outline, 10 outline with the cut dragged, 11 chart tilted with the decay chain.
- New box "Compare old CPU paths" (off by default) adds 12 to 18: the CPU twins of 6, 7, 8, 9, 10, 1, 2 (old 23, 25, 28, 29, 30, 31, 32). The boxes "Include stress scenes" and "Include speed-risk scenes" are gone. PL_BENCH_RISK now ticks the new box; PL_BENCH_STRESS is no longer read (removed from check_6b.py).
- Removed from the list: old 2, 3, 5, 7, 9, 10, 12, 13, 14, 19, 20, 21, 24, 26, 27, 33, 34.
- Every "scene N" in names and texts, and the "Atom Lab settings used" line, use the new numbers; "Stress:" and "Risk NN:" prefixes are dropped; texts that pointed at removed scenes (old 16, 18, 22, 35) were reworded.
- Names and texts of the list now live in app/bench_scenes.cpp (no graphics, so it can be tested); ui/benchmark.cpp only attaches the set-up code. atom/atom_bench.cpp keeps its scene table and its old-numbered texts unchanged: the removed scenes' set-up code is still used by the existing tests (none deleted), and bench_scenes.cpp renumbers them.
- Docs: README Benchmark paragraph, changelog.cpp, RESUME.md, OWNER-TODO.md (one line: run default, then ticked, do not touch the PC, send the text). Stage 2 window-height cap untouched.

## Tests
- New in app_settings: testBenchSceneNumbers (18 scenes / 11 default, names numbered in order, no "Stress"/"Risk", every "scene N" in any name or text exists and is not the scene itself, each twin names its original and the original names its twin). It failed on its first run (a wrong text index showed up), then passed.
- Changed on purpose: check_6b.py (ui6b_bench) no longer sets PL_BENCH_STRESS. check_5k.py (ui5k_windows): the "handle's centre is flat" pixel check assumed blank window there; the longer scene list puts text under it (range 204), so it now compares the 5x5 block with the handle's own block from the size where the handle lies free (1920x1080) and requires it to differ; the draw-order checks are unchanged. Not shown to catch a handle drawn on top (no break-on-purpose run).
- Passed: check.sh quick 10/10, app_settings, ui6b_bench, ui5k_windows (ui5k_contents/log, others in the same ctest call also passed). No full suite.

## Not tested
- Speed of anything and the real run time: no GPU, no Windows, no real mouse in the cloud. The 110 s is 11 x (2 + 8) s from the constants, not a measured run.
