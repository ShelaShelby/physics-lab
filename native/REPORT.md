# 0.78.1: research: a realistic gas disc for the Black Hole Lab (plan and pictures, no change to what the app does)

## What changed
- native/PLAN_DISC.md: the plan. Section 0 for the owner, one recommended version and two alternatives, the physics with sources, tests, GPU plan, build steps D1-D5, five questions.
- Findings: at 0.1 L_Edd the feeding disc is at most 1.8 % of its radius thick (h = kappa F / (2 c Q), Zhu et al. 2012 App. B; their Fig. 3 shows about 2 %). Sagittarius A*'s hot flow (BL06 model) is about 1/560 of the Sun's surface brightness in visible light, near white, a thin ring round the shadow.
- tools/discproto (new CMake target, not a test): CPU double, the lab's KerrBL paths. Three pictures in docs/blackhole (disc078_*.png with .txt): today beside proposed at the same view and exposure.
- 24 new source files in the sources repo (commit 13f6d55), 24 lines in data/SOURCES.md. DECISIONS (the owner's words of 24 Nov), ROADMAP, PLAN_BLACKHOLE, PLAN_MOVINGGAS, data/blackhole/README.txt. Changelog entry, VERSION 0.78.1.

## Tests run
- check.sh job: quick 18 of 18 passed, ui_smoke passed. check_sources.py on the sources checkout: ok (689 manifest lines, 42 extracts).
- discproto checks: Sagittarius A*'s model gives 0.195 mJy at 2.2 um at Earth (GRAVITY 2020: median 1.1 +- 0.3, 5 % percentile 0.28). Optical depth at 550 nm at most 5e-13. Thermal light at 550 nm at most 8e-12 of the power-law light.
- Measured on the build machine: 444 integrator steps and 0.127 ms a ray, 77 ns an emissivity sample (one core, double).

## Not done
- No app code (a research job). No M87* picture: no fitted semi-analytic model of its flow was found (source needed, ROADMAP).
- No GPU prototype: the cost estimate uses the CPU measurement and operation counts, labelled estimates in PLAN_DISC.md section 6.
- Hot-flow structure not drawn: its strength is not sourced. The Illinois GRMHD data (CC BY 4.0) could give it: step D5.
- Film's glow: no camera or lens spread function with tabulated values was found (Talvala et al. 2007 gives plots only). Source needed.
- Doubtful value: Abramowicz & Fragile 2013 (99) prints the inner region's H = [1e5 cm] (mdot) with no power of m, while (97) and (98) carry m. The textbook form scales with M. The lab does not use (99).
- BL06's "30 % of equipartition" is read through Broderick et al. 2011 (4) as beta^-1 = 0.3. D1 tests it first against the 1.3 mm flux (about 3 Jy).

## References changed on purpose
- None. No test changed.
