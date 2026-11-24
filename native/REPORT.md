# 0.83.0: the benchmark cleaned and renewed

## What changed
- Read: CLAUDE.md, REPORT 0.82.1, ROADMAP, DECISIONS (benchmark rules); by grep and line ranges bench_scenes.h/.cpp, bench.h/.cpp (BenchBhMove, behindWorst), ui/benchmark.cpp (set-ups, frames, waitDefs, the table, the saved text), blackhole/lab.h, blackhole_view.h/.cpp (behind, keyOf), atom_bench.cpp, atom_sim (bfield, emit, absorb), shell_atom.cpp (Photons), shell_molecule.cpp (Live), molecule_view.h (liveOn), main.cpp (Settings, liveFrame, the window), test_app, check_6b.py, TESTS.md.
- Hard list: the 23 scenes of the job moved to All scenes (ids and set-ups the same). 17 kept + 9 new = 26 scenes, 42 waits: 260.2 s planned. The limit is kBenchLimitS = 4.5 minutes. The 3.5-minute floor stays.
- New: blackhole.dolly_spin, over_poles, everything, edge_on, orbit.saturn_real_fast, atom.emit_all, molecule.live_hot, ui.settings_over_blackhole, ui.resize_blackhole; wait response.blackhole_dolly. Black Hole settings in blackhole/bench_settings.cpp. Dropped: atom.bfield_drag. setParam("bfield") changes only the readout, the spectrum's lines and the Zeeman tables.
- Tried on the build machine: everything with the glare 4.97 fps, with All light 5.46 (glare kept). live_hot Space-filling 4.8 fps, Ball and stick 14.6 (Space-filling kept; Density does not offer Live). Settings does not pause the Black Hole picture (only a tab page does). emit_all ends a flight still going and lands an absorbed photon at once, so each press starts a jump (7 presses, 7 jumps).
- Lag column and rule (bench.cpp benchReaches, the table, the in-app table, the rule under it). Lag is NOT behindWorst. behindMs counts from the first frame behind. A moving view asks for a new view every frame, so it never catches up and behindMs grows for the whole movement (9.2 s in edge_on here) on any PC. New: lagMs (blackhole_view.h), how long ago the shown picture's view was the one asked for. Owner decides (ROADMAP).
- ui.resize_blackhole: SDL_SetWindowSize every 4 frames, from the start size down to the minimum window and back. A maximised window is restored first and maximised again after. Full screen is not resized (its line says so). Put back exactly here (1280 x 800 and 880 x 560). Not tried on Windows.

## Tests run
- test_app: 202,363 checks, 0 failed. New: the Hard list's ids in the written order, every moved id in All only, each new Black Hole scene's settings read back, emit_all's atom and a started jump, the 4.5-minute limit, the lag rule (7.9 ms at 255 Hz reads no, 7.8 yes), the lag column and its rule text. They do not build on 0.82.1 (benchReaches has no lag, no bench_settings.h). The old test binary on the new lists failed 5 checks.
- Each new scene and the new wait ran once here (lavapipe, numbers not the owner's): all started, moved and ended cleanly. The wait took 11 to 14 s here.
- check_6b.py benchrestore by hand (now with ui.resize_blackhole): 4 checks, 0 failed, 67 s. check.sh job: quick 18 of 18, ui_smoke passed.

## Not done
- The 1 % lows of the job were not checked against the owner's 0.82.0 file. It is not in the repo. The list follows the job.
- The Orbit, Molecule and Settings set-ups are not read back in test_app (it links no Orbit or Molecule Lab). Their report lines say what ran.

## References changed on purpose
- test_app: the list's counts (26 / 65), the ids, the planned time (at most 270 s), kBenchAtomScenes 47 (atom.emit_all), the report row (a "-" lag column), the texts' check by id instead of index.
