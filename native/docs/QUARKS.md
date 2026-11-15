# Quarks in the Atom Lab: facts, sources, choices (0.42; clouds 0.43; tomography 0.44; review fixes 0.45)

Every source below is saved as received in native/data/qcd/ (README.txt there lists each file, its URL and the numbers read from it).
0.16 stage 6 used facts from chat that could not be re-read; 0.42 re-read them all in the saved papers and corrected what was off.

## Facts and sources
- Proton: charge radius 0.84075(64) fm (CODATA 2022; PDG 2024 0.8409(4); older electron scattering ~0.88 fm, the radius puzzle, now mostly
  resolved), magnetic moment +2.792 847 344 63 muN (CODATA 2022). Neutron: <r^2> = -0.1155(17) fm^2 (PDG 2024) against -0.106(+7 -5) fm^2
  from the deuteron in chiral EFT (Filin et al., PRL 124, 082501 (2020), 1.7 sigma apart); A1 -0.110(8) (Atac et al. 2021). mu_n -1.913 042 76 muN;
  mu_n / mu_p = -0.6850, the simple quark model says -2/3 (PDG quark model review).
- Neutron charge picture depends on the frame: rest frame (Kelly, PRC 66, 065203 (2002)) positive core, negative edge (model dependent);
  transverse density (Miller, PRL 99, 112001 (2007), model independent) negative centre. Both are named in the app.
- Quark masses (PDG 2024, MS-bar at 2 GeV): u 2.16(7), d 4.70(7) MeV; FLAG 2024 2+1+1: 2.14(8), 4.70(5). Constituent mass ~336 MeV
  (PDG quark model, 2 m_p / g_p): a model quantity, not a mass of a free particle.
- Proton mass split (Yang et al., PRL 121, 212001 (2018), lattice, MS-bar 2 GeV): quark energy 32 %, gluon energy 36 %, trace anomaly 23 %,
  quark condensates 9 % (the text, at the physical point: 100 %; the abstract rounds 33 % and 37 %). Not unique: other schemes split it differently (Ji, Front. Phys. 16, 64601 (2021)). sigma_piN: 59.1(3.5) MeV
  (Hoferichter et al. 2015, pion-nucleon scattering) against 41.9(4.9) lattice (Gupta et al. 2021: 59.6(7.4) with N pi states): open.
- Quark size: none found. ZEUS (PLB 757, 468 (2016)): < 4.3e-19 m (95 % CL). Contact-interaction limits, Lambda > 24 TeV (PDG 2024
  compositeness review), read as hbar c / Lambda: ~8e-21 m, a different interpretation. The Accurate dots mark places, not quarks: 1.3 to 3 points
  on screen at any zoom. The Simple balls: radius 0.075 x 0.84075 fm = 6.3e-17 m, about 150 times the ZEUS limit.
- Gluon field of three quarks (Bissey et al., PRD 76, 114512 (2007)): quarks <= ~0.3 fm from the centre: a round bubble, strongest in the
  middle; ~0.4 fm: a filled triangle; > 0.5 fm: Y-shaped tubes, radius 0.38(3) fm. Quenched lattice, heavy STATIC quarks. Suganuma, Ichie,
  Takahashi (hep-lat/0407011) agree on the Y beyond ~0.5 fm. With dynamical light quarks only quark-antiquark tubes are done so far
  (Baker et al., EPJC 85, 29 (2025)): no calculation gives the field of three light, moving quarks.
- Gluon radii disagree and are model dependent: Duran et al., Nature 615, 813 (2023): mass radius 0.755(35) (holographic) / 0.472(42) (GPD),
  lattice 0.746(25) fm; 0.55(3) fm as quoted by Kaiser and Weise (arXiv:2404.11292).
- u vs d: central u transverse density ~70 % above d (Miller 2007); the d-quark form factors fall faster (Cates et al., PRL 106, 252003 (2011)).
- Sea: d-bar/u-bar 1.34 .. 1.65 for 0.13 < x < 0.45 (SeaQuest, PRC 108, 035202 (2023)); integral of (d-bar - u-bar) 0.118(12) (E866).
  Strange sea: ATLAS 2017 "close to unity" vs HERMES 2014 "softer": debated, so not drawn.
- Spin: quark spins carry Delta Sigma 0.26 .. 0.36 (COMPASS 2016), lattice 0.38 (Alexandrou et al. 2020); gluon spin for x > 0.05 about
  0.20 (DSSV14, via Ji, Yuan, Zhao 2021).
- Colour: a baryon is a colour singlet, antisymmetric in colour (PDG quark model review): no quark has a definite colour.

## What the app draws (choices, not measurements)
- Two quark styles (owner, 8 Nov 2026): Accurate and Simple. Cinematic was removed in 0.43; Classic was renamed Simple.
- Accurate (0.44): a cloud of dots, each a piece of the nucleon's momentum (quark_tomo.h; data/qcd/tomography/README.txt). Kind of parton picked by its momentum share
  (ABM11 NLO, 2 GeV: valence u 30 %, d 11 %, gluons 40 %, sea pairs 14 %; strange and charm 4.5 % not drawn), then x, then b from a Gaussian of width <b^2>(x):
  valence Diehl-Kroll 2013 (fit ABM 1, checked against their table 15), sea COMPASS 2019 / H1 2009 DVCS, gluons J/psi (Frankfurt-Strikman-Weiss 2005); linear in ln x
  between the measured points (a choice); above the largest measured x (77-81 % of the sea's momentum, 34 % of the gluons') shrunk as (1 - x)^2 (0.45; a choice; until 0.44
  held constant, too wide). Drawn clouds (momentum weighted): rms 0.42 fm u, 0.47 fm d, 0.54 fm gluons and sea; counting every quark the same, 0.63 / 0.64 fm.
  Up warm, down cool, gluons grey as bright as the quarks, antiquarks thin faint rings (0.45); no colour charge (0.42). Valence hides gluons and sea, + Gluons the sea.
  Seen along a very fast nucleon's motion, drawn in the screen plane. The glow under the dots (0.45) is the same density, a sum of 6 Gaussians (pkGlow); farther out the
  nucleons keep the at-rest charge profile. The 0.43 net clouds (BBBA05 + Atac) remain as a cross-check (7 % wider in b^2).
- Simple: three balls coloured by flavour (no colour charge drawn; the red/green/blue swap code, unused since 0.42, was removed in 0.45) joined by Y flux tubes drawn
  0.38 fm in radius (0.45; were about 5 times thinner), called a model for learning. The balls stay 0.18-0.42 fm from the centre, where Bissey's lattice finds a bubble or a
  triangle, not a Y: the hint says so. Balls about 150 times too big (0.075 x 0.84075 fm against the ZEUS limit). Sea: d and u pairs 1.26 : 1, the d-bar / u-bar momentum
  ratio of ABM11 at 2 GeV (0.45; was 1.5, "inside SeaQuest's range", which holds only for 0.13 < x < 0.45).
- Nucleus look (owner, 8 Nov 2026): Density (the measured charge density) or Nucleons (one snapshot; Fuzzy and Hard balls merged in 0.43).

## Not drawn (and the app says so where it matters)
- Strange quarks (debated), the pion cloud, pressure and shear inside the proton, a 3D quark density (no measurement gives one), the spin (unpolarised nucleons; a sideways spin shifts u and d to opposite sides, Burkardt; Carlson and Vanderhaeghen 2008 saved).
