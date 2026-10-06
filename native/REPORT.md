# Native Physics Lab 0.15 — stage 4.1: fast workflow (testing speed). No app, look, physics or test-check change.

`index.html` untouched. No helper agents. One commit, one push to `native`, no PR. Changed: `CMakeLists.txt` (labels, test environment, ccache), `tools/check.sh`, `tools/theme_pictures.sh`, `tests/shell_lib.py` and the picture scripts (parallel starts), `CLAUDE.md` (workflow rules + build commands), RESUME. Time used: about 60 minutes (most of it the two timed full runs).

## Result in one paragraph (honest)
`check.sh quick` is **65 s** (was: no such thing; the unit programs were mixed in with the picture tests). A single picture test run by hand is now **3-4x faster** (it uses every core: `orbit_ui_facts_long` 505 s -> 126 s alone, `shell_pictures` 233 s -> 201 s, a `check_shell.py` part of 8 starts 47 s). **The full run did NOT get faster: about 24-26 min before and after (1,461 s and, after the last edits, 1,541 s: 27 of 27 pass).** The reason is measured, not a guess: the cost is software rendering of the frames, about 90 ms of one CPU core per Orbit Lab frame at 1000x640 (45 frames = 3.8 s; an app start itself is 1.2 s), and the suite makes 529 app starts of 40-110 frames: about 5,800 CPU-seconds, so 4 cores cannot go below about 24 min. The old tests already kept all 4 cores busy (llvmpipe used 4 threads per start, now 1 thread per start and 4 starts at once: the same total). The 6-minute target is therefore not reachable without cutting work, and I did not cut any (see "Options").

## What was done
- ctest labels `quick` (9 unit/shader/source-scan tests) and `pictures` (18 tests that start the app); `tools/check.sh quick|full` (full = `ctest -j<cores>`, `PL_JOBS=1` so ctest, not the scripts, spreads the work).
- Picture tests get `LP_NUM_THREADS=1` and a shared `MESA_SHADER_CACHE_DIR` (an app start: 2.8 s -> 1.3 s with a warm cache). **Same pixels, proved:** `shell_pictures` (48 hash guards) and `theme_pictures` (the 0.13 / 0.14 / 0.15 hashes), `window`, `y2k`, `page_cache`, `effects` all pass under it.
- `shell_lib.prefetch()`: independent starts run in a pool (`PL_JOBS` workers, one per core when a test is run alone); used by `check_shell.py` (sizes, reading, small), `check_shell_pictures.py` (68 starts), `check_orbit_ui.py` facts_long. `tools/theme_pictures.sh` draws its themes in parallel (own HOME and log each); the three native themes are drawn at once.
- ccache is used by CMake when installed (`apt-get install -y ccache`): a rebuild after reverting a break-it-on-purpose edit hits the cache. (Not measured separately; the first build after enabling it was a full 386-step build.) A fresh container has nothing cached: FetchContent sources (SDL, ImGui) are fetched again (about 1 min of the configure); no way to keep them between sessions except in the repo, which I did not do (it would add large files).
- CLAUDE.md: the exact one-line build commands (app, unit-only) and the WORKFLOW RULES the owner listed (quick while working, one test per perturbation, `full` once, no polling, time budget 60 min with PROGRESS.md lines).
- Not done (not reachable here): reusing one app start for several pictures. A picture of another state needs another start, and a script step changes the screen it is then compared with, so pixels would differ.

## Timing (seconds, `ctest -j4`; before = stage 4 runs, 4 llvmpipe threads per start; after = 1 thread, shared shader cache; app starts counted by a wrapper)
| test | before | after | app starts |
|---|---|---|---|
| shell_ui | 940 | 1057 | 53 |
| shell_pictures | nan | 579 | 68 |
| theme_pictures | 552 | 526 | 90 |
| orbit_ui_facts_long | 388 | 505 | 31 |
| orbit_ui_coverage | 541 | 458 | 56 |
| window_pictures | 368 | 424 | 40 |
| y2k_pictures | 319 | 332 | 30 |
| shell_state | 238 | 260 | 12 |
| orbit_ui_readable | 256 | 243 | 36 |
| effects_pictures | 240 | 216 | 25 |
| orbit_ui_facts | 216 | 173 | 11 |
| orbit_ui_search | 188 | 164 | 28 |
| orbit_ui_card | 173 | 159 | 16 |
| page_cache_pictures | 174 | 145 | 24 |
| atom_physics | 108 | 64 | 0 |
| orbit_ui_chip | 70 | 52 | 3 |
| gl_phase_colours | 58 | 45 | 0 |
| orbit_ui_time | 32 | 24 | 4 |
| orbit_ui_links | 18 | 16 | 2 |
| 9 quick tests | 108 (atom_physics) | 65 (all, parallel) | 0 |
Total: **529 app starts**, full run 1,461 s / 1,541 s after (1,492 s before). Pass/fail is the same as the stage 4 run (27 of 27); the check counts are untouched (no check was edited).

## Options for the owner (each trades something, so not done without a decision)
1. Run the logic-only tests (`orbit_ui_*`, `shell_ui`) in a smaller window: cost is about proportional to pixels, so about 35 % less for them; the checks stay, but some read layout at 1280x800.
2. Fewer frames per start where a test only needs the state (the scripts wait 10-30 frames before and after the clicks): a few minutes saved, with a small risk of timing flakes.
3. A split of `check.sh full` into "changed screens" and "everything" per stage (the rules in CLAUDE.md already say: while working, only the one test for what changed).
4. A real GPU would make frames about 100x cheaper; the cloud session has none.

## Not tested
Windows, GitHub's build: see the closing message.
