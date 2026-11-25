# 0.85.0: Orbit Lab: true sunlight, the Sun's measured edge, real sunspot groups, the Oort cloud from a published model (PLAN_REVIEW R1-R4)

## What changed
- Read: CLAUDE.md, REPORT 0.84.1, ROADMAP (Orbit Lab), DECISIONS 25 Nov, PLAN_REVIEW R1-R4; by grep and line ranges shaders.h (kFragmentShader, kPlanetFS, kRingsPart), shading.h, renderer2d.h/.cpp/_vk.cpp, ring_card.h, ring_check.cpp, orbit_view.cpp, card_text.cpp, belts.h/.cpp, date_text.h, sky.h/.cpp, spin.h, sun_facts.h, realest.cpp, shell_orbit.cpp, orbit_actions.cpp, settings.cpp, learn_data.inc, app_text.inc, bench_scenes, benchmark.cpp, main.cpp, planet_picture.cpp, check_orbit_ui.py, test_app.cpp.
- R1: Lighting True sunlight / Even (not realistic); the dot and Most realistic pick True. Factor (d_exposure / d)^2 on balls, moons, rings (GPU uniform uLightK, lit-ball light length; CPU the same). Readout under the choice. Learn "Brightness" has the limits. Markers keep their brightness.
- R2: Neckel & Labs 1994 per channel (render/sun_limb.h; inputs by tools/make_sun_limb_data.py). The Sun is a ball in kPlanetFS uMode 4. Eclipses per channel, 16-point Gauss-Legendre. Alpha Centauri keeps the old blend.
- R3: patches gone. Group outlines from tools/make_sunspot_table.py (255,250 rows, 1874-05-09 to 2026-11-24, built in). Card and sentences for no record, spotless, unknown. Measured here: from 1992 the g-file places match 24:00 UT, not the printed .500 (rows shifted). 58 rows print Carrington longitude 360.1-408.9 (taken mod 360). 1874-2016 has 246,266 rows (0.84.1: 246,265).
- R4: Oort dots sample Vokrouhlicky, Nesvorny & Dones 2019 (n(r) ~ r^-0.72, -1.35, -1.5; joined continuously by the app; directions even). Sedna, 2012 VP113, 2015 TG387 as far objects on full-precision SBDB elements, cards with the owner's sentence. About lists every source. Owner raised the benchmark limit once to 4.9 min: 3 Hard scenes, response.sunspot_table.

## Tests run
- orbit_tests 2872 checks, 0 failed (testReview085 new: does not build on 0.84.1). app_tests 202,426, 0 failed.
- check_orbit_ui.py review085 (new, 26 s): GPU vs CPU true_Earth 0.27 %, true_Jupiter 0.07 %, true_Neptune 0.33 %, eclipse_Earth 0.31 %, Sun with 7 groups 0.08 %. planets: 6 of 6 as before.
- Benchmark: the 3 scenes and the response line ran once (validation 0 errors; the table reads in 11 ms here). check_sources.py: ok (774 lines, 74 extracts). check.sh job: quick 18 of 18, ui_smoke passed.

## Not done
- Spots' darkness (3 tries: two searches, Mathew 2007: one wavelength, per spot, needs umbral radii). Outlines labelled. Owner's files list.
- Oort inner-cloud lean: figure only (VND19 Fig. 7). DQT 1987: ADS 500/504. Kaib & Quinn 2008: laws in figures.
- Brown et al. 2004: the saved arXiv page gives ApJ 617, 589-599 (not checked against the journal). The app cites arXiv:astro-ph/0404456 only.

## References changed on purpose
- test_app: 30 Hard and 69 scenes (+3 ids), 45 waits, limit 294 s (owner, 25 Nov 2026).
- check_orbit_ui planets: PLPLANET line gained the factor (planet_picture reads 27 fields).
