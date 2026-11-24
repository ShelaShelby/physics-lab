# 0.78.0: comet tails calculated, the photon's dipole pattern (second job from the outside physics review)

## What changed
- Item 0: run 45269193504 stopped rings at the 300 s part limit (no check failed). That run was about 1.4 times slower in every part (flyby 117 to 168 s). 0.76.0 already took 243 s. Now 0.7 s counted, picture at 0.5 s (was 2.0 and 1.5): 68 s to 30 s here, same checks.
- Item 1, dust: grains on Kepler orbits around (1 - beta) GM (Finson & Probstein 1968, orbit/comet_tail.h). 40 betas, a release every 6 h, 40 days back: 6,400 a comet. kGrainsCS solves them on the GPU (universal variables, float; the age in double). The CPU double path is the reference and the fallback.
- Item 1, gas tail: along V r_hat - v, V = 400 km/s (NASA MSFC, saved). CometData::tail is removed. Choices kept and labelled: 4 AU switch-on, beta 0 to 1, release grid, equal grain brightness ("model"), gas tail as long as the dust tail.
- Item 1: Hard scene orbit.comet_tails (Hale-Bopp at perihelion, zoom 1.00, camera turning, 1 day/s). No response line: the tail appears at once. The rocks' GPU object has 32 slots (was 16).
- Item 3: a one-electron Cloud-view photon leaves in 3D from p = 3/(8 pi)(1 - |n.u|^2), u = <to|r|from> (Jackson 3rd ed. sect. 9.2), seen from the camera. Absorption the same. "Emission pattern" switch draws the lobe. Orbit view and many-electron atoms keep the old angle and say so. The decay flash is labelled "a drawing".
- Item 2 skipped (stop rule, 2 tries): no saveable phase table for the main rings over all phase angles.

## Tests run
- orbit_tests: 2599 checks, 0 failed. testCometTail078: beta = 0 on the Kepler orbit 3.2e-12; offset 1/2 beta g t^2 to 6.3e-8, anti-sunward to 1 - cos = 4.7e-9; energy and h kept to 4e-14 (99 grains on hyperbolas); ion tail anti-sunward (5.6e-17) and atan(40/400) = 5.71 deg.
- atom_tests: 12153 checks, 0 failed. testPhotonPattern078: integrals 1 within 5e-7; chi-squared p 0.815, 0.243, 0.939 (19 dof, 400,000 each); within 3 deg of z: 4 of 2,000,000 (exact share 2.8e-6, P 0.81).
- vk_compare_tests: 707 checks, 0 failed. Grains GPU vs CPU over 230,400 places: worst 0.00107 points at 300 points to the tail (limit 0.01). Start-up check: 1.4e-5 point.
- check.sh job: quick 18 of 18, ui_smoke passed. By hand: check_orbit_ui.py rings 4, flyby 2, cardlines 8, planets 25 checks, 0 failed; check_atom_chip.py card 18, 0 failed (it draws atom.jump_all: many-electron, photon unchanged). check_sources.py: ok.

## Not done
- Item 2 (Saturn's ring phase law and colour): Déau et al. 2009 (arXiv:0902.0289) fits only the opposition surge. No table over all angles was found. ROADMAP.
- No published syndyne figure checked: the Finson & Probstein scan (ADS) answered 500 on 2026-11-24. ROADMAP. Dipole pattern for many-electron atoms and polarisation: ROADMAP.

## References changed on purpose
- vk_rocks.inc: "33 sets do not fit in the 32 slots" (was 17 in 16): the slots were raised for the comets' grains. test_app.cpp: 37 Hard scenes, 53 in All, plan up to 318 s. check_6b.py: orbit.comet_tails's zoom. test_shell.inc: the comet card's 4 AU line. No picture reference file changed.
