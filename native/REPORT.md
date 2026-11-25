# 0.84.0: Black Hole Lab: the camera's own shift drawn; the spinning hole's camera named; the two failing full tests

## What changed
- Read: CLAUDE.md, REPORT 0.83.0, ROADMAP; by grep and line ranges kerr.h/.cpp, schwarzschild.h/.cpp, disc.h, gas.h, hot.h, light.h/.cpp, spin_sky.h, spin.h/.cpp, blackhole_view.cpp, lab.h/.cpp, looks.h, kerr_compute.h, kerr_sky.cpp, vk_blackhole.cpp, shaders.h (kLensFS, kKerrFS, kKerrSkyFS), renderer2d.h/.cpp, sky_card.cpp, orbit/sky.h/.cpp, shell_blackhole.cpp, blackhole_learn.cpp, PLAN_BLACKHOLE.md 125-160, bench_scenes, benchmark.cpp, shell_test.cpp, main.cpp; R19's saved text (Fig. 20 caption, (52), (53)).
- Feasibility: every GPU path already had each ray's b (kKerrSkyFS: the traced ray's R.b; kKerrFS: the table's b in uKA). No new data had to be carried. New uniform uFido = (1/alpha, omega/alpha), f = uFido.x - b uFido.y. At spin 0 the one factor goes into the disc's temperature scale.
- Drawn (Real): lab.h heldCameraShift (J15 (A.16), beta = 0), discLightSeen (T_obs = g f T), starSeen (colour of a black body at f T, V light by its eye's light). Stars: one f at spin 0; with spin each image's b from the table. The readout's brightest star uses the b = 0 factor. Film unchanged. The sky map is not shifted: it has no colour temperature (ROADMAP).
- Names and texts: lab.h line 5 (FIDO, held and carried round), Camera caption, Learn, the card ("(not drawn)" gone), Film's Shifts label, comments in disc.h, schwarzschild.h, gas.h, hot.h.
- gasrow: in Film (spin 0.6) a caption "Working out the light paths" stood above the row. Under load it vanished between the frame that read the row's place and the press, so the click missed (reproduced: 4 runs at once). Fix: PL_BH_SPINWAIT, and gasrun=off so the gas time is the same in every run. Pause/Play check reads "Play". The checks are unchanged.
- atom.all_classic: the scene turned by the real frame times, so the card's and the processor's runs were at different angles at frame 30 (reproduced under CPU stress: exactly GitHub's 5402). Fix: with PL_BENCH_SHOT_FRAME and PL_FIXED_DT the moves follow the frame count (benchmark.cpp). Under the same stress: 128 of 256000.
- Benchmark: blackhole.close_spin (Hard, 27 scenes, 266.7 s). blackhole.extreme reaches the same view but drags the spin for 40 % of its time and turns by setting angles.

## Tests run
- blackhole_tests: 1141 checks, 0 failed. New testCameraShift: a = 0 against (1 - 2M/r)^(-1/2) and R19 Fig. 20's caption; spin against the FIDO from the textbook metric (a 0.3-0.99, r 2.2-100, 4 theta, 5 b); disc pixels at g f T; all light by f^4; stars. It does not build on 0.83.0.
- GPU vs CPU (toneCompare, at most 1 % over 8 levels): close (spin 0, 2.2, 360 deg) 0.39 %; closespin 0.15 %; pole 0.07 %; closetable (kKerrFS) 0.004 %.
- Against a 0.83.0 build: Film pictures identical to the pixel (2 views); Real at 1000 M: 0 pixels over 8 levels (spin 0 and 0.9).
- check_blackhole sky 59 s and gasrow 39 s (run_group); gasrow 4 of 4 with 4 at once; check_atom_chip classic passed; app_tests 202,365, 0 failed; close_spin ran once; check.sh job: quick 18 of 18, ui_smoke passed.

## Not done
- check_sources.py: only data/blackhole/arxiv is checked out here; it lists the rest as missing. The R19 file's sha256 matches SOURCES.md.

## References changed on purpose
- test_app: 27 Hard and 66 scenes, the id list (+blackhole.close_spin). check_blackhole: the tone cases' set (+4); close's pixel floor 2000 (the disc covers 5,120 pixels from 2.2 M, spin 0).
- New test, my own bound corrected before the GPU work: 1000 M was first written as 0.5 % (not derived). It is now 3.76 %, derived from 8 levels; measured 0.79 %.
- ROADMAP had no line for this job; one added (the map's colour temperature).
