# Quarks in the Atom Lab: facts, sources, choices (0.16 stage 6)

Facts the owner researched (in chat, 2026-10-11). The build machine could not reach arxiv.org or nature.com, so none was re-read here.

## Facts and sources
- Quark masses (PDG 2024, MS-bar at 2 GeV): u 2.16 +/- 0.07 MeV, d 4.70 +/- 0.07 MeV. Three valence quarks of a proton: 2 x 2.16 + 4.70 = 9.0 MeV = 0.96 % of 938.27 MeV.
- Proton mass split at 2 GeV, lattice QCD (Y.-B. Yang et al., PRL 121, 212001 (2018), arXiv:1808.08677): quark energy 33 %, gluon energy 37 %,
  trace anomaly 23 %, quark-mass term (u, d, s scalar, sea included) 9 %. Rounded; they carry errors of a few percent.
- Quark size: contact-interaction limits Lambda > about 24 TeV (CMS fit, PDG 2024 review "Searches for quark and lepton compositeness"),
  so size < 0.197 GeV fm / 24,000 GeV = 8e-6 fm = 8e-21 m. The dots are drawn with radius 0.03 x 0.84 fm = 0.025 fm: about 3,000 times bigger.
- Flux tube (F. Bissey et al., PRD 76, 114512 (2007), arXiv:hep-lat/0606016): Y-shaped at large separation, no Delta shape, tube radius
  0.38 +/- 0.03 fm. Quenched lattice, heavy STATIC quarks: not the light, moving quarks of a real proton.
- Gluon radii are model dependent and disagree: mass radius 0.55 +/- 0.03 fm (as quoted in "Sizes of the Nucleon", arXiv:2404.11292);
  Jefferson Lab Hall C (Nature 615, 813 (2023)): about 0.75 fm mass radius, about 1 fm scalar radius, as reported by CERN Courier.
  NOT confirmed in the paper itself (not reachable from the build machine).
- u vs d (G. A. Miller, PRL 99, 112001 (2007), arXiv:0705.2409): the central u-quark charge density is about 70 % larger than the d one.
  This is a transverse density in the infinite-momentum frame: used here as a heuristic, not a 3D snapshot. Same paper: the proton's
  long-range part is positive, the neutron's negative (not drawn).
- Sea: anti-d more common than anti-u (E866/NuSea, hep-ex/0103030; SeaQuest, arXiv:2103.04024; d-bar/u-bar levels near 1.6).
- Baryons are colour singlets, antisymmetric in colour (PDG quark model review): quark colours are not observable. The colour swapping
  animation stays as a teaching picture.

## What the app draws (choices, not measurements)
- Accurate valence quarks (src/atom/quark_wander.cpp, atom::accurateQuarks): each quark wanders smoothly on its own, seeded by the nucleon.
  The three are centred on the nucleon each frame and gently pulled in so none is past 0.6 fm.
- Mean distance from the centre: 0.33 fm, kept from the old tumbling triangle (measured 0.332 fm over its motion; the brief said
  "about 0.25 fm", which the old motion does not give). A choice, not a measurement.
- Mean d distance / mean u distance = 1.7^(1/3) = 1.193 (density per quark ~ 1/a^3, Miller's 70 %).
- Profile before centring: distance distribution r^2 e^(-r/b) cut at 0.8 fm. b is FITTED so that after centring the targets hold:
  proton b_u 0.128, b_d 0.314 fm; neutron b_u 0.104, b_d 0.207 fm. Measured (atom_tests, 2,000 nucleons x 300 times):
  proton mean 0.3295 fm, d/u 1.198; neutron mean 0.3276 fm, d/u 1.188; farthest 0.6000 fm.
- Worked out for a pure exponential cut at 0.6 fm with no centring (a_d / a_u = 1.193, mean 0.33 fm): proton a_u 0.137, a_d 0.164 fm;
  neutron a_u 0.129, a_d 0.154 fm. With the cut that close, those give a d/u MEAN ratio of only 1.07: the brief's two wishes
  (a_d / a_u = 1.7^(1/3) and mean ratio 1.7^(1/3)) cannot both hold. The app keeps the mean ratio (it is what the test checks),
  so its central-density ratio is larger than 1.7.
- Gluon field (Accurate only): tube cross-section rms radius 0.38 fm (0.269 fm on the one sideways axis on screen); halo 3D rms 0.65 fm
  (per axis 0.375 fm), a choice inside 0.55-0.75 fm. The particles' light is weighted toward the tube axes and the centre (same total
  light, same particle count) so the Y still reads.
- Classic sea: 60 % d, 40 % u pairs (ratio 1.5, a little below the SeaQuest plateau; illustrative). Accurate sea: colour only.

## Not done (no sourced numbers retrieved)
- Strange sea quarks, the pion-cloud tail, spin, pressure.
