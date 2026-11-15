GRASP2018 pipeline of the jump clouds (native 0.47.2; plan: native/PLAN_JUMPS.md). GRASP itself is not in the repo.

  tools/grasp/build.sh [dir]           clone GRASP2018 (github.com/compas/grasp, MIT) at commit 9006157730a8 into dir (default $HOME/grasp), build it (about 3 min;
                                       apt: gfortran cmake libblas-dev liblapack-dev libopenmpi-dev openmpi-bin), and build our rmultipole against its libraries (bin/rmultipole)
  tools/grasp/run_level.py X [series]  run every series of atom X (s1 p1 p3 d3 d5: one l and one J; all its levels in one run) in --work (default $HOME/gwork)
  tools/grasp/run_level.py Na p3 --one-config    the lowest level of a series as one configuration (the tests' Na 3p 2P3/2)
  tools/grasp/multipoles.py X [--test-level p3:df0]   -> native/data/jumps/grasp/X/levels.tsv and clouds.bin
  python3 native/tools/make_alkali_data.py            the NIST levels and lines (src/atom/alkali_data.inc) the runs take their level list from

Method (run_level.py): Dirac-Fock one valence orbital at a time (the lowest level varies everything; each higher n alone with the core fixed, made orthogonal to
the lower ones); then core-valence MCDHF: single and double excitations with at most one hole in the outer p shell of the core (1s for Li), three layers of
correlation orbitals (one more of each l = s..g per layer, only the new ones varied, all levels of the series together); then RCI with Breit and QED.
multipoles.py: natural orbitals and occupations from the k = 0 one-body density matrix, and D_k(r) of the M_J = J state (rmultipole: GRASP's ONESCALAR for k = 0,
ONEPARTICLEJJ with rhfs's <||C^k||> for k > 0; checked: occupations sum to Z; one configuration p3/2 and d5/2 give the spinor shapes exactly; D_2 of Na 3p 2P3/2
reproduces rhfs's electric quadrupole constant B, 27.74097 against 27.74096 MHz for Q = 1 b).
Lessons (0.47.2, all seen): rmcdhf exits 0 on failure and can also run away without a message (Cs 5d5/2 layer 1 went to -4e8 hartree, then NaN): every output is
checked for "Convergence not obtained", NaN and, in a layer, a change of more than 1 hartree. Optimizing a higher root of a multi-configuration list failed (Na 4d,
5s); one level at a time works. GRASP's solver diverges for the nearly hydrogen-like Na 4d from every start: the hydrogen-like orbital of charge 1 is then kept
(flagged "hydrogen-like kept" in levels.tsv). Thomas-Fermi failed for Cs's core in the 5d5/2 run: its core is taken from another series ("sibling").
ONEPARTICLEJJ with rank 0 connects CSFs that differ only in a subshell's J (occupations summed to 11.0009): rank 0 uses ONESCALAR (as rdensity does).
A fourth layer did not help K (4p1/2: +2.07 % with 3 layers, +2.14 % with 4); the remaining error is the correlation left out (core-core, inner shells).
0.47.3: two s electrons outside closed shells (He, Be, Mg, Zn, Cd in the app; Ca, Sr, Ba, Ra, Hg, Yb tried, see native/PLAN_JUMPS.md part 2):
  tools/grasp/run_two.py X [blocks] [--resume] [--finish KEY:LAYER]   Dirac-Fock orbitals one valence orbital at a time (ns2 first, then each n'l' in the lowest listed
                                       configuration that holds it, averaged over its levels); then per block (parity, J) core-valence + valence-valence MCDHF in layers, EOL over
                                       every state of the block's configurations; RCI with Breit and QED; jj2lsj names; rmultipole
  tools/grasp/multipoles_two.py X      -> data/jumps/grasp/X/{levels.tsv, clouds.bin "PLG2"}: roots paired with NIST levels by LS weight (an assignment); each level measured from the
                                       ground level with the same number of layers; a level more than 5 % from NIST gets no cloud
  python3 native/tools/make_level_data.py   the NIST level and line tables of all part-2 atoms and the jump status of all 118 (src/atom/level_data.inc); the D2 level choice
Lessons (0.47.3): rmcdhf can also stop half-way with status 0 ("maximum tabulation point exceeds", helium's correlation orbitals beyond its 10p): every output must end with
"Execution complete". Correlation orbitals for an l whose orbitals in the block reach n >= 7 run off to the continuum: none are made for it; when no layer converges at all
(helium's Rydberg blocks) the CI uses the orbitals of another block whose layers did. rci's Breit integrals failed once ("Brint1 Integral not found"): redone without Breit.
Unbalanced layers (ground 3, a level 1) put Mg 3s3p 1P1 10 % off: compare at the same number of layers. A layer above about 10,000 CSFs takes hours (MAXCSF caps it). The mcp.*
scratch files filled the disk (now deleted per finished block). Background tasks stop after 2 hours: run detached (setsid nohup) with --resume. Each J alone spreads a term's fine
structure (Zn 3P0..2: -3.4..-5.3 %). Levels where d mixes in (Ca 4s4p: +76..+173 %, 3d4s, 3d2) and the heavy Hg, Ra need a better recipe; Yb's ground DF did not converge.
trial_0.47.1/ holds the 0.47.1 scratch drivers (reference only).
0.47.4: one orbital set per atom (Ca, Sr, Ba, Hg, Ra; tried on Yb):
  GRASP_NP=4 tools/grasp/run_shared.py X [--work W] [--stage df|orb|ci|all] [--resume]   Dirac-Fock as run_two; one MCDHF over several blocks for the correlation orbitals
                                       (main valence orbitals ns, (n+1)s, lowest p, lowest d; one block per main configuration); then per block RCI on ONE orbital file (ci.w)
  tools/grasp/multipoles_two.py X --work W    as before; also <W>/X/roots.json (the CI root of each level)
  GRASP_NP=4 tools/grasp/rates_two.py X --work W    D4: rbiotransform + rtransition per pair of blocks -> data/jumps/grasp/X/rates.tsv (S -> A with NIST's wavelength)
  GRASP_NP=N runs rangular, rmcdhf_mem, rci, rtransition, rbiotransform as their MPI versions (build.sh with libopenmpi-dev installed; Dirac-Fock stays serial: the MPI
  rmcdhf aborts on a one-CSF list); rmultipole splits its CSF loop over N processes (partial sums added by run_two.multipoles; checked equal to one process within 2e-14).
  Speed (4 cores): Ca's three orbital layers 6k/15k/24k CSFs in 2, 6, 6 min; RCI blocks of 6k-53k CSFs in 1-10 min; rates 12 min; one atom about 1-1.5 h.
Lessons (0.47.4, all seen on Ca and Sr):
  - rwfnestimate asks "Revise any of these estimates?" after a file start: the h1 start had no answer and crashed every time (0.47.3 never used it); Ca's 4p then fell back to
    the hydrogen-like orbital of charge 1, the main cause of 0.47.3's Ca 4s4p 3P1 +173 %. Ca 4p converges only with 4s varied too (kept: only the new orbital).
  - An orbital that converges onto a lower one of its kappa (Ca 6p onto 5p, 11 % left after projection) is now a failed start, not kept.
  - Core-valence excitations must start from EVERY configuration of a block: from the main ones only, Ca 4s5p and 4s6p came out +10 % (their 3p 4s pair correlation is a triple).
  - Every block of one parity needs the same core-valence references (the parity's main configurations first): with different sets Ca 4s4p 3P1 came out below 3P0; now the
    3P0-3P1 gap is NIST's 52 cm-1.
  - Equal level weights against "one vote per configuration" made no difference to Ca 3d4s (-7 % either way): the d levels' error is the correlation left out
    (core-core; Safronova 2009's CI+MBPT is off the same way, -2 %, its all-order fixes it), not the orbitals' weighting.
  - A list that never uses a subshell leaves it out of its header (Ca J = 0 lacks 5g+): merge lists under the wider header (CSF lines name their own subshells).
  - rmultipole computed each CSF pair's angular factor again per level (3 min per level for 29,000 CSFs): now once per pair for all levels, then split over processes.
  - Rates: GRASP's two gauges disagreeing by more than 50 % mark a computed A as unusable (Ca's 4s5d lines: 34-38 % apart and 82 % below NIST); even with good agreement
    the d-electron lines are often 30-70 % off (Sr), so the app shows each atom's median error against NIST with its own values.
  - Yb (4f14 6s2): the ground Dirac-Fock does not converge from Thomas-Fermi or hydrogenic starts; staged (n <= 3, then one shell at a time by n, 4f from the screened
    hydrogenic start) converges every shell but 6s, which oscillates near 0.06 hartree (TF start) or 0.17 (hydrogenic start), also when everything is varied at once.
0.51.3 (Ra shipped; Yb tried):
  - ci.w must be orthonormal per kappa: ci_orbitals cut a LONGER spectroscopic orbital (Ra's hydrogen-like 8p, 493 bohr) to the correlation orbital's length (116 bohr) before
    projecting, leaving up to 1.4 % overlap; GRASP takes the file as orthonormal and every Ra block's lowest roots fell onto correlation orbitals (0.47.4). Now the shorter one is
    extended first (also in run_two.merge and run_level), and ci_orbitals refuses a file with an overlap over 1e-6 (Ra after the fix: 1.5e-11).
  - Yb's ground (4f14 6s2): converges with rmcdhf's non-default "accelerating parameter" held constant at 0.7 for every subshell (mcdhf_blocks damp=0.7; 0.4 did not).
  - A run killed half-way can leave a block folder without info.json (Ra o4): --resume redoes it; check that every block of the plan has a line in the log.

