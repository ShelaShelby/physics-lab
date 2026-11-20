Paused; last true at 0.54.4; not kept up to date until the owner restarts it.

**Paused by the owner, 14 Nov 2026, after 0.51.3.** No further jump parts are planned; what is done (jumps with every electron shown for 16 atoms; the one-electron jumps for every element) stays in the app. This file is kept as the record of the method.

# Plan: every element jumps, with every electron shown (0.47.1 research; parts 0.47.2 .. 0.48)

Written 9 Nov 2026 by the 0.47.1 research job.
Every number below names its file under native/data/. Our own trial calculations are in data/jumps/trial_runs.tsv (scratch builds, not in the repo).

## 0. In short
- **Data.** NIST ASD has measured, classified electric-dipole (E1) jumps for 83 neutral atoms (H to Th, with gaps). Seven more have measured levels
  but no classified lines in ASD (Se, Zr, Nb, Pr, Pm, Re, Os). Pa to Es (91–99) were measured, but the tables are not in ASD and the one
  compilation (Blaise & Wyart 1992) could not be reached. Fm and No have a few laser-measured lines (not in ASD). Md, Lr and 104–118 have none.
  The full list is data/jumps/elements.tsv.
- **Method.** Draw each level from a **multiconfiguration Dirac-Hartree-Fock (MCDHF) + relativistic CI** calculation made with GRASP2018 (MIT licence),
  computed ahead of time and saved. It builds and runs here. For Na and Mg it lands within 0.2–1 % of NIST, while the present
  frozen-core picture is 6 % off (trial_runs.tsv). The app always shows NIST's measured energies and wavelengths. The calculation gives
  the cloud only, and its own energy error is shown as the cloud's quality figure.
- **Honest limits.** For open d and f shells (Fe, lanthanides, actinides) a one-configuration calculation is 50–140 % off. Correlated
  calculations are much slower; how close they get for these atoms is not measured yet (each part measures it). The app must say so per atom.
- **Size.** About 30–50 levels per atom (the levels of the 30 strongest measured lines). That is about 4,000 levels for all atoms, and about 10–20 MB of cloud
  data in the app (decision D3).

## 1. Data per element (question 1)
Sources: data/jumps/asd (NIST ASD levels and lines of all 110 neutral atoms ASD lists, fetched 9 Nov 2026), data/asd/holdings (the counts pages),
data/jumps/sources (everything outside ASD). Table: data/jumps/elements.tsv (columns: levels with an energy, named levels, lines, classified E1 lines with
both levels named, those with an A value, those with A graded B or better, i.e. ≤ 10 %, other source, measured).
Cross-check: the line counts equal ASD's own holdings page for every atom; the level counts are lower only by the limits and the terms ASD lists without an energy.

| group | atoms | what exists |
|---|---|---|
| measured jumps in ASD | 83: Z 1–33, 35–39, 42–58, 60, 62–74, 77–90 | classified E1 lines; A values for some lines of 78 of them; none at all for Tb (7 classified lines), Po (4), At (2), Rn (10), Th (2,053) |
| levels only | 7: Se, Zr, Nb, Pr, Pm, Re, Os | named levels; ASD's lines carry no classification (Pm: 16 lines with energies but unnamed levels) |
| measured, not in ASD | Pa, U, Np, Pu, Am, Cm, Bk, Cf, Es | Blaise & Wyart 1992 compilation + later papers (review_block2021.txt Fig. 2); the online tables (Laboratoire Aimé Cotton) gave HTTP 503 on 9 Nov 2026; Es levels of Weber et al. 2022: HTTP 403 |
| a few laser lines | Fm, No | Fm: 5f12 7s2 3H6 → 5f12 7s7p 5G°5 near 25 111.8 cm⁻¹ (fm_warbinek2024_pmc.html); seven Fm transitions (Sewtz 2003, in review_block2021.txt). No: 1S0 → 1P1 at 29 961.5 cm⁻¹ and Rydberg states (review_block2021.txt; no_laatiaoui2016_nature.html) |
| nothing measured | Md, Lr, Rf–Og (17) | Lr: "first laser spectroscopy" still to come (review section 6.2); a 2026 EPJ D paper titled "Progress toward identifying atomic transitions in lawrencium" could not be fetched |

**Decision D1 (what counts as measured):** proposed: "measured" = a named source reports the energy of both levels (NIST ASD first; otherwise a saved paper).
Lines in ASD with no classification do not count (we cannot tell which levels they join).

## 2. The best cloud a level can get (question 2)
All differences are (computed − NIST)/NIST; Safronova 2009 prints the opposite sign, (expt − calc)/expt, converted here.

| method | code, licence | here | time per level (this container) | energy vs NIST |
|---|---|---|---|---|
| frozen core (the app now) | tools/dhf (ours) | yes | seconds | Na 3p −6.1 % |
| relaxed one-configuration DHF | GRASP2018 (github compas/grasp, MIT) | built (gfortran 13 needs -fallow-argument-mismatch) | 0.1 s Na; 0.1 s Fe; 2–5 s Hg, Eu | Na 3p −5.9 %; Fe a5F +139 %, z7D −51 %; Hg 3P1 −28 %; Eu z10P −48 % |
| MCDHF + RCI (core-valence + valence-valence, n ≤ 6) | GRASP2018 | yes | Na 30–40 s; Mg 1.5–4.5 min; Fe z7D5: 72 min (valence only) | Na 3p +0.20 %; Mg 3P1 −0.16 %, 1P1 +0.26 %, 3P0/3P2 −1.1/−0.8 % (each J run alone) |
| CI (valence only) | AMBiT (Kahl & Berengut CPC 238, 232 (2019); GPL-3) | built (needed one missing #include) | Mg, 25 levels: 122 s | Mg 3P1 −4.3 % (equals the published CI value 20 926) |
| CI+MBPT | AMBiT | yes (option -s12) | Mg, all 25 levels: 25 min | ours: Mg 3P0,1,2 −0.43/−0.42/−0.41 %, 1P1 −0.03 % (fine structure in the right order); published 3P1: Mg −0.32 %, Ca +2.1 %, Cd +2.6 %, Ba +6.7 % (Safronova 2009 Table II) |
| CI+all-order (coupled cluster) | the Safronova group's codes (released as pCI, arXiv:2410.06680; not saved) | not tried | | published 3P1: Mg −0.08 %, Ca +1.15 %, Cd −0.03 %, Ba +2.2 % (Safronova 2009 Table II) |
| Fock-space coupled cluster | e.g. DIRAC (not saved) | not tried | | used for the No and Lr predictions cited in review_block2021.txt; not benchmarked here |

Fe with correlation (GRASP, valence 3d 4s 4p only, layers n = 4, 5; trial_runs.tsv): z7D°5 goes from −51 % (one configuration) to −17 % (n = 4) and −15 % (n = 5).
That one level (63,187 CSFs at n = 5) took 72 min and 7 GB of memory, and the RCI with Breit did not finish in 20 min. Reaching a few % needs core-valence (3s 3p) correlation
and more layers: hours to days per level, more memory than this container's 15 GB unless the expansions are pruned (GRASP's rcsfinteract,
MPI versions). **Transition metals are the hard part of the plan;** part 0.47.6 must first show what accuracy is reachable before computing all.

**What limits even the best method.** (1) Correlation: open 3d, 4f and 5f shells have thousands to millions of configurations, so the expansions are
cut short and the error stays large (Fe: see above; lanthanides not yet tried with correlation). (2) Convergence: GRASP's self-consistent field needs per-atom care. With default starting
orbitals Eu, Hg 6s6p and Na 3p3/2 failed, and staged orbital updates fixed Eu and Hg 3P1 but not Hg 1P1 (trial_runs.tsv notes). (3) QED and Breit: included
in RCI, small for valence energies. (4) Rydberg and continuum-near levels: very diffuse orbitals need big grids.
**Choice: GRASP2018 MCDHF + RCI** for every cloud. Reasons: open source (MIT); it builds and runs here; it gives orbitals and mixing coefficients (the cloud),
natural orbitals and densities (rdensity), and E1 rates (rtransition) for cross-checks; it handles any number of open shells. AMBiT (CI+MBPT) is the
independent energy cross-check for 2–3 valence-electron atoms. Neither code goes into the repo: tools/grasp/build.sh downloads a pinned commit.

## 3. What the cloud of a level with total J really is (question 3)
- The density averaged over all M_J states (2J+1 of them) is exactly round: the k > 0 multipoles of the one-particle density cancel in the sum over M.
  A free atom with no field, no polarized light and no preferred direction is in this mixture, so **round is the true picture of a free atom**.
- One M_J state has the shape ρ(r,θ) = Σ_k ρ_k(r) P_k(cos θ), with k even and k ≤ min(2J, 2 l_max). **A J = 1/2 level is round in every M_J state.**
  Proof for Na 3p1/2, m = +1/2: the spinor is √(1/3) Y10 α − √(2/3) Y11 β, so its density is (1/3)|Y10|² + (2/3)|Y11|² = 1/(4π).
  The 0.47 app draws Na 3p1/2 as a pz dumbbell (an m_l = 0 orbital): a **fix** (roadmap line).
- Proposed view: "Free atom (average over directions)": round, from the natural orbitals and their occupation numbers (GRASP rdensity), which already
  hold all the configuration mixing. "Aligned atom (M_J = J, e.g. in a magnetic field)": adds the k > 0 terms, computed from the
  MCDHF mixing coefficients with GRASP's one-body tensor library. rhfs already does ranks 1 and 2 this way. A new small tool, tools/grasp/multipoles,
  writes ρ_k(r). The readout states which view is drawn and why.
- Every electron shown: each natural orbital is drawn with its occupation number (not a whole number once configurations mix). The dots of a level then add up to the
  correlated density. For the aligned view the k > 0 part is drawn as the whole atom's shape (it cannot be split per electron without the full density matrix).

## 4. Which jumps to offer (question 4)
Textbooks and Grotrian diagrams group levels by configuration, then term (²S, ³P …), then J. Strong lines are what learners know (the Na D lines, iron lines in the Sun, the mercury-lamp lines).
Counted from data/jumps/asd: the 30 strongest lines by gA (A known) touch these numbers of levels: He 31, C 29, Ne 30, Ti 44, Fe 46, Cu 31, Hg 22, Eu 36, Er 11.
All E1 levels with A: He 183, Fe 435.
**Proposal (D2):** per atom, compute the ground level plus the levels of the 30 strongest A-known lines plus the lowest level of each low configuration
(about 30–50 levels). The panel shows them grouped as configuration → term → J, with "strongest lines" first. "All measured levels" lists the rest with
their NIST energy, but no cloud ("no cloud computed for this level").

## 5. Jump rates (question 5)
Use ASD's A value and show its accuracy grade (AAA ≤ 0.3 %, AA ≤ 1 %, A+ ≤ 2 %, A ≤ 3 %, B+ ≤ 7 %, B ≤ 10 %, C+ ≤ 18 %, C ≤ 25 %, D+ ≤ 40 %, D ≤ 50 %, E > 50 %: data/jumps/sources/asd_lines_help.html; a prime ′ means split from a multiplet assuming LS coupling, which elements.tsv does not count as B or better). Note: many ASD A values are themselves evaluated calculations (the grade says how good).
When ASD has no A: the jump is offered at its measured wavelength. The rate line reads "rate not known (no measured or evaluated value in NIST)". The jump
then plays at a fixed speed marked "not to scale", and the decay picker does not use it. Decision **D4**: may the app show a GRASP-calculated
A value marked "calculated by this app (GRASP2018), not measured"?

## 6. Size, time and tests (question 6)
- **Size:** per level, the natural orbitals that differ from the atom's ground-state core (valence and the relaxed outer core) on a 256-point grid, plus
  ρ_2, ρ_4, ρ_6 for the aligned view. The ground-state DHF data is 2.4 MB for 118 atoms (data/dhf/dhf.bin). Estimate (not measured): a radial function on 256 points in 4-byte floats is 1 KB; about 3–5 per level, so 4,000 levels
  come to 12–20 MB (decision D3).
- **Time:** measured here: alkalis about 40 s per level; Mg 1.5–4.5 min. Fe z7D5 with valence correlation only: 72 min and 7 GB (section 2); real accuracy for
  transition metals and lanthanides needs more: hours to days per level. All atoms: days to weeks of one container. Parts run their own groups; the computation is restartable and the results are saved per level
  under data/jumps/grasp/<X>/.
- **Tests (written first, in each part):** (a) every computed level's energy against NIST, with a per-group limit from the trial runs; a failure names the
  level. (b) Against a published calculation of the same level (Safronova 2009 Table II for Mg, Ca, Cd, Ba; more saved per part). (c) Conservation and
  physics checks: the electron count equals the sum of occupations; the M_J-averaged density is round (k > 0 sums to zero); a J = 1/2 level has no k > 0
  term; the ground level equals the app's present DHF ground state in the one-configuration limit. (d) Every jump offered has both levels in NIST
  (or a saved source); every A shown equals ASD's. (e) The "not measured" atoms offer no jump.

## 7. Atoms without measured jumps (question 7)
- Nothing measured (Md, Lr, 104–118): Photons section greyed out, with "No jumps: nobody has measured light from this atom yet." (Proposed wording, D5.)
- Levels only (Se, Zr, Nb, Pr, Pm, Re, Os): "Measured levels exist, but NIST has not matched its lines to them yet; no jumps are shown."
- Pa–Es: "Measured (Blaise & Wyart 1992 and later), but the tables are not in NIST and could not be reached; no jumps are shown." (0.52.0: "no jumps yet" became "no jumps are shown": the jumps are paused.)
- Fm, No: their few measured jumps only, each with its paper.
- Buttons greyed: Emit, Absorb, the jump list and "Put the outer electron in". The cloud (ground state) stays.

## 8. Li, Na, K, Rb, Cs now (question 8)
Frozen-core Na 3p is 6.1 % below NIST. Relaxing the core alone changes this by only 0.2 % (GRASP DHF −5.9 %). The missing part is core polarization:
the CV-MCDHF run gives +0.2 %. The cloud changes accordingly (the valence orbital pulled in). **Recommendation: redo all five (and add Fr) with the new
method in 0.47.2.** It is cheap (about 40 s per level) and makes every atom use one method.

## 9. Expected accuracy per group (energies against NIST; the cloud's quality figure)
Only what was measured here or published:
- one s electron (Li–Fr): about 0.2 % (Na, our run).
- two s electrons (He, Be, Mg … Ra, Zn, Cd, Hg, Yb): Mg 0.2–1.1 % (our run); 3P1 with CI+MBPT / CI+all-order (Safronova 2009 Table II): Ca 2.1 / 1.2 %, Cd 2.6 / 0.03 %, Ba 6.7 / 2.2 %.
- p block, Cu/Ag/Au: not measured yet; set in part 0.47.5 from its first runs.
- two s electrons (measured, worst shipped level; levels over 5 % get no cloud): He 0.12 % (0.47.3); 0.47.4, one orbital set per atom: Mg 0.54 %, Cd 2.1 %, Hg 2.5 % (6 levels), Zn 3.4 %, Sr 4.0 %, Ba 4.1 %, Ca 4.9 %; Be 1.9 % (0.47.3: its recompute failed a density self-check); the lowest d levels of Ca, Sr, Ba 6-10 % low; Ra 4.6 % (0.51.3, 17 levels; its 7s8s +42 %); Yb: 2 of 5 levels within 5 % (0.51.3, not shipped).
- 3d/4d/5d metals: one configuration is 50–140 % off for Fe; with valence correlation (n ≤ 5) still −15 % (z7D5); core-valence needed, not yet measured.
- lanthanides, actinides: one configuration about 50 % off (Eu); correlated: unknown until 0.47.7 measures it (no published neutral benchmark saved yet).

## 10. Build parts (each with its tests first; sizes: S one session, M 1–2, L several). PAUSED: none of the parts below is planned.
2. **Left (M), first in 0.51.4: Yb** (section 12; Ra done in 0.51.3), the Rydberg orbitals whose Dirac-Fock fell back (Hg 7s 7p 6d, Zn 5s, Cd 6d 7d), and if
   cheap the d levels of Ca, Sr, Ba (6-10 % low: core-core correlation).
3. **0.47.5 (L): Cu, Ag, Au and the p block** (B–Rn rows, noble gases' np⁵ n'l levels), about 33 atoms.
4. **0.47.6 (L): transition metals** (Sc–Ni, Y–Pd, Hf–Pt; Zr, Nb, Re, Os are levels only); compute time large; per-atom accuracy shown.
5. **0.47.7 (L): lanthanides** La–Lu (Pr, Pm levels only).
6. **0.48 (M): actinides with data** (Ac, Th, Fm, No; Pa–Es as "measured, not available"), the aligned-atom view for every atom, full UI run.

## 11. Decisions for the owner: ANSWERED 9 Nov 2026 (also in DECISIONS.md)
- **D1:** a jump is "measured" only when a real lab measured both of its levels. **Yes.**
- **D2:** 30–50 levels with a cloud per atom: the levels of the ~30 strongest A-known lines plus the lowest level of each low configuration; every other
  measured level is listed with its NIST energy and "no cloud computed for this level". **Yes.**
- **D3:** the app may grow about 10–20 MB for the clouds. **Yes.**
- **D4:** where NIST has no rate A, the app shows its own GRASP2018 value, marked "computed by this app (GRASP2018), not measured". **Yes.**
- **D5:** atoms with no jumps: Emit, Absorb, the jump list and "Put the outer electron in" greyed, with the wording of section 7. **Yes.**
- **D6:** default view = the lined-up (aligned, M_J = J) shape; the round free-atom average is a button; the readout says which is drawn and why. **Lined up.**
- **D7:** the computing is done ahead, a group of atoms per update (0.47.3 … 0.48). **Yes.** (Replaced on 14 Nov 2026: the jumps are paused after 0.51.3.)

## 12. Handout: job 0.51.4 (NOT TO BE RUN: the jumps are paused, owner decision of 14 Nov 2026; kept only as the record of what was known)

```
Job: native 0.51.4: first Yb (left by 0.51.3), then part 3 of native/PLAN_JUMPS.md (Cu, Ag, Au and the p block), once the owner has decided the cloud size budget (ROADMAP.md).
Follow CLAUDE.md (Opus leads, Haiku helpers, cost rules, tests first). If it is much bigger than one session, say so in REPORT.md and finish what fits.

READ FIRST (only these)
- CLAUDE.md; native/REPORT.md (0.51.3); PLAN_JUMPS.md sections 2, 3, 9, 10, 11; native/tools/grasp/README.txt (the lessons up to 0.51.3).
- By grep plus line ranges: tools/grasp/run_shared.py (plan, stage_orb, ci_orbitals, stage_ci), run_two.py (orbitals, mcdhf_blocks damp), multipoles_two.py, rates_two.py,
  tools/make_level_data.py (PART2, rates), tests/test_atom0474.inc.

SET UP (fresh container)
- apt: gfortran cmake libblas-dev liblapack-dev libopenmpi-dev openmpi-bin; pip: scipy sympy; tools/grasp/build.sh; export GRASP_NP=4.
- One atom = run_shared.py X --stage all --resume, then multipoles_two.py X, then rates_two.py X (Ra: about 1.5 h on 3-4 cores). Run detached (setsid nohup, logs to files).

WHAT IS KNOWN (0.51.3, measured here)
- Ra shipped (17 levels, worst -4.62 %). Its 0.47.4 failure was ci.w not orthonormal (1.4 %): ci_orbitals cut the longer hydrogen-like 8p to the correlation orbital's length
  before projecting; fixed, and ci_orbitals now refuses a file with an overlap over 1e-6. Ca, Sr, Ba, Hg were computed before the fix (ROADMAP: recompute and compare).
- Yb: the ground Dirac-Fock converges with constant damping 0.7 (mcdhf_blocks damp=, strategy '2d'). With the 4f hole only (888-7138 CSFs): 1S0 ground, 6s6p 1P1 -2.4 %,
  6s6p 3P1 -8.8 %, 6s7s 3S1 +33 % (its weight is split between two CI roots, 6s7s 82 % and 6s8s 70 %), 6s7p 1P1 -8.7 %. Only 5 levels pass D1/D2 (the others open 4f or
  have no observed line). Try 2 (5p may also lose one electron) made layer 1 12,600 CSFs and did not finish it in over 2 h on 2 cores (stopped). Try 3 is left: e.g. 6s relaxed
  in the 6s7s Dirac-Fock, or Yb's s Rydberg as for Ra 7s8s (+42 %): the ns(n+1)s levels are the common failure.
- The test requires >= 4 levels with clouds per shipped atom (test_atom0474.inc).

DO (tests first)
1. Yb: at most 1 more try (the 3rd), then write it down; if it ships, set kLimit0474Yb.
2. Part 3 as in the 0.47.5 handout: Cu, Ag, Au (one s electron outside d10: run_level.py may fit) and the p block B..Rn (ns2 np^k: generalise run_shared.plan() and the CSF
   references to np^k); each atom: energies within a per-atom limit from your runs; its resonance line offered; D4 rates as in 0.47.4.
3. Docs as usual; PLAN_JUMPS.md: delete what is done, write the next handout here. check.sh quick; at most 4 cropped looks.
4. Commit "native 0.51.4", one push (git push origin HEAD:refs/heads/native), git ls-remote, wait for the Windows run and the "latest" release on that commit.
```
