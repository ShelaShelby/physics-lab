# 0.16 stage 6: the Accurate quark style made more accurate (counts and CPU measured; GPU fps NOT tested)

## What changed (only Accurate changes its look; Classic only its sea colours; Cinematic unchanged)
1. Text: Fuzzy edge choice "Fuzzy (proton charge profile)" and the readout line say the neutron uses the proton's exp(-r/r0) profile only as a size (also the renderer2d.cpp comment). Learn card "About 99 %..." is now "The three quarks' own masses are only about 1 %", with lattice QCD's 9 % for all quark-mass effects and the 33 / 37 / 23 % split (Yang 2018); Higgs sentence kept. Quark note: dots about 3,000 x bigger than a quark can be (< about 1e-20 m); in Accurate, sea shows colour only and the real sea has more anti-d than anti-u. Readout "quarks are points (<0.001 fm)" -> "(< 0.00001 fm)".
2. Classic sea: 60 % d, 40 % u pairs (was 60 % u); ratio 1.5, illustrative.
3. Accurate valence quarks: new pure function atom::accurateQuarks (src/atom/quark_wander.cpp): each quark wanders on its own (seeded by q.ph, no camera, no allocation), centred every frame, softly kept inside 0.6 fm; p = u u d, n = u d d; colour swapping kept.
4. Accurate gluon field: tube rms radius 0.38 fm (was 0.04-0.09 fm sideways), halo rms 0.65 fm (per axis 0.375, was 0.252); light weighted to the axes and centre (same total, same particle count).
- About "Where the facts come from" lists PDG 2024, Yang, Bissey, arXiv:2404.11292, Hall C, Miller, E866, SeaQuest. New native/docs/QUARKS.md.

## Things the owner should know (found, not hidden)
- The old triangle's mean quark distance is 0.332 fm (measured over its motion), not "about 0.25 fm". Kept 0.33 fm ("same as now").
- With a cut at 0.6 fm, a_d / a_u = 1.7^(1/3) gives a d/u MEAN ratio of only 1.07. The test asks for a mean ratio of 1.193, so that is kept; the central density ratio is then larger than 1.7. Profile scales are fitted, not measured (QUARKS.md).
- The Learn "proton" tab still uses d = 4.67 MeV (PDG 2022 value) while PDG 2024 gives 4.70 +/- 0.07. Not changed (word-for-word web tests; 9.0 MeV and 0.96 % either way). Owner to decide.
- arxiv.org and nature.com were not reachable: Hall C's 0.75 / ~1 fm NOT confirmed in the paper (as reported by CERN Courier); no fact was re-read.
- Tube radius is from a quenched lattice with heavy static quarks; taken as the rms radius of the cross-section (definition not re-checked).

## Speed guard (Ds-281, Cloud, Fuzzy, Accurate, camera turning 0.15 rad/frame, 1280x800, last 30 of 90 frames; CPU us = nucleus part, mean of 3 runs, software GL, noisy)
| zoom | detail | theme | calls | vertices | blobs | CPU us before -> after |
|---|---|---|---|---|---|---|
| x2063 | + Sea | light | 5 -> 5 | 30 -> 30 | 1,642 -> 1,594 | 990 -> 908 (-8 %) |
| x2063 | + Sea | Midnight | 5 -> 5 | 30 -> 30 | 1,642 -> 1,594 | 920 -> 884 (-4 %) |
| x2063 | + Gluons | light / Midnight | 5 -> 5 | 30 -> 30 | 1,176 -> 1,128 | 745 -> 734 / 756 -> 727 |
| x5182 | + Sea | light / Midnight | 5 -> 5 | 30 -> 30 | 371 -> 368 | 353 -> 377 (+7 %) / 352 -> 346 |
| x5182 | + Gluons | light / Midnight | 5 -> 5 | 30 -> 30 | 263 -> 260 | 300 -> 341 (+14 %) / 300 -> 300 |
- The x5182 + Gluons light +14 % is noise as far as can be told: the draw stream is identical light vs Midnight (stage 5), and Midnight is +0 %. A first version was +15 % (exp() per particle, trig hash); replaced by closed forms and an integer hash.
- No new per-segment lines, no new batch flushes. ui6c_ds281sea unchanged and passing.

## Tested (Linux, software OpenGL)
- New unit test testQuarkWander (atom_physics): mean 0.3295 / 0.3276 fm (p / n), d/u 1.198 / 1.188, farthest 0.6000 fm, centred to 1e-16 fm, largest step 0.026 fm per 1/60 s (limit 0.04; old triangle 0.0099). It cannot run on the parent (no function); broken on purpose (b_d = b_u, no pull-in): 4 checks fail.
- Word-for-word: atom_learn_reference.inc updated on purpose for the new card (17 places); the web hint test now adds the two new quark-note sentences (adaptNucleusHint); About edit added to kEditsAbout.
- check.sh quick: 10 of 10. check.sh full (once): 73 of 75; the 2 failures were on purpose and fixed, then only those re-run and passing: app_settings (About now has 36 native edits and 261 entities: counts updated) and shell_pictures (shell_about_dark/light_1280x800 hashes updated: the About sources list is longer; the other 22 hashes unchanged). No saved picture of the Accurate quarks exists, so none changed.
- Pictures looked at (2 composites, 4 views): Ds-281 x5182, light + Gluons and Midnight + Sea, before vs after.

## Not tested / not done
- fps on a real GPU and Windows behaviour (OWNER-TODO); whether the wider Y reads well at every zoom (the 2 composites showed it faint, as before).
- Not done (no sourced numbers retrieved): strange sea, pion-cloud tail, spin, pressure.
