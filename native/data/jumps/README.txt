[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/jumps/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Data for "jumps in every element" (0.47.1 research; the plan is native/PLAN_JUMPS.md)
===================================================================================
asd/<X>I_levels.tsv.gz, asd/<X>I_lines.tsv.gz  NIST ASD levels (energy1.pl) and lines (lines1.pl) of the 110 neutral atoms listed on the ASD
    holdings pages (data/asd/holdings), retrieved 2026-11-09 11:29-11:50 UTC by tools/fetch_jumps_asd.py (request parameters in that file), saved exactly
    as received and then gzip-compressed (zcat gives the bytes ASD sent). Lines: all wavelengths, observed and Ritz, Aki with its accuracy grade, both
    levels' configuration, term and J where ASD has them. When no line of a spectrum is classified, ASD leaves out the configuration columns (Se, Zr, Nb,
    Pr, Re, Os) or returns an HTML "Input Error" page when there are no lines at all (Fm..Hs); both are kept as received.
    Citation: A. Kramida, Yu. Ralchenko, J. Reader and NIST ASD Team, NIST Atomic Spectra Database (ver. 5.x), NIST, Gaithersburg, MD (2026).
asd_g/<X>I_levels_g.tsv.gz  (0.54.0) NIST ASD levels with the Landé-g column (lande_out=on) of H, Li, Na, K, Rb, Cs, Fr, He, Be, Mg, Ca, Sr, Ba, Ra, Zn, Cd, Hg, Yb,
    retrieved 2026-11-14 by tools/fetch_lande_asd.py (request parameters in that file), saved exactly as received and gzip-compressed. ASD leaves the Landé column out
    when no level of a spectrum has a g: only K I (12 levels), Zn I (3), Ba I (24), Yb I (111) and Hg I (20) list any. tools/make_lande_data.py -> src/atom/lande_data.inc.
elements.tsv  one row per neutral atom Z = 1..118, made by tools/make_jumps_table.py from asd/ and sources/ (column meanings in that file's header).
grasp/<X>/  (0.47.2) the GRASP2018 clouds the app draws for Li, Na, K, Rb, Cs, Fr (tools/grasp: run_level.py, multipoles.py; README.txt there):
    levels.tsv: every level's computed excitation energy against NIST, the outer natural orbital's occupation, CSF count, run time, how its Dirac-Fock started;
    clouds.bin: the natural orbitals (P, Q on 300 log points, float32) with their occupations and the aligned state's multipoles D_k(r) (format: multipoles.py).
    Not measured data: our own calculations, kept so the app's pictures can be checked and remade.
grasp/<X>/ for He, Be, Mg, Zn, Cd (0.47.3; tools/grasp/run_two.py, multipoles_two.py): the same files for the levels of the two-s-electron atoms, keyed by NIST's configuration,
    term and J ("PLG2"); levels.tsv lists each level's LS weight, and the D2 levels without a cloud with the reason (not identified, or more than 5 % from NIST).
trial_runs.tsv  the test calculations of 0.47.1 (GRASP2018, AMBiT) next to NIST and a published calculation: our own numbers, from the runs described in
    PLAN_JUMPS.md; not used by the app.
sources/  measured data outside NIST ASD and the method papers, saved as received (retrieved 2026-11-09 UTC):
  review_block2021.txt                 M. Block, M. Laatiaoui, S. Raeder, "Recent progress in laser spectroscopy of the actinides", Prog. Part. Nucl. Phys. 116,
                                       103834 (2021), arXiv:2010.11941: text extracted with pdftotext -layout from https://arxiv.org/pdf/2010.11941 (8 MB PDF,
                                       sha256 332c7705a4d7ebcc48bb62a7e0bf74c49289fe3fba2d03f99b9a75fdf47e93cf, not stored for size); its abstract page
                                       review_block2021_arxiv2010.11941_abs.html as received. Used: Fig. 2 caption (Blaise & Wyart compilation Ac..Es and later
                                       level papers), Fm seven transitions (Sewtz 2003), No 1S0-1P1 29961.5 cm-1 (Laatiaoui 2016), Rydberg states (Chhetri 2018),
                                       Pa about 1500 high levels (Naubereit), Lr: first laser spectroscopy still to come (section 6.2).
  osti_blaise_wyart_1992.html          OSTI/ETDEWEB record of J. Blaise, J.-F. Wyart, "Selected constants, energy levels and atomic spectra of actinides" (1992).
                                       The online tables at Laboratoire Aime Cotton could not be reached (HTTP 503 / reset, 9 Nov 2026), so no level of them is used.
  fm_warbinek2024_pmc.html             J. Warbinek et al., Nature 634, 1075 (2024), PMC11525170: Fm ground 5f12.7s2 3H6 -> 5f12.7s7p 5G*5 near 25111.8 cm-1.
  no_laatiaoui2016_nature.html         M. Laatiaoui et al., Nature 538, 495 (2016): nobelium ground-state transition 1S0 - 1P1.
  astatine_rothe2013_doi.html          S. Rothe et al., Nat. Commun. 4, 1835 (2013): astatine Rydberg states and ionization potential 9.31751(8) eV.
  ci_allorder_safronova2009_arxiv0905.2578.pdf  M. S. Safronova, M. G. Kozlov, W. R. Johnson, D. Jiang, Phys. Rev. A 80, 012516 (2009): CI, CI+MBPT and
                                       CI+all-order energies of Mg, Ca, Zn, Sr, Cd, Ba, Hg against experiment (Tables I, II); the accuracy reference of the plan.
  asd_lines_help.html                  NIST ASD lines help page (https://physics.nist.gov/PhysRefData/ASD/Html/lineshelp.html): the A accuracy grades AAA..E.
  ambit_kahl2019_arxiv1805.11265.html  E. V. Kahl, J. C. Berengut, "AMBiT: A program for high-precision relativistic atomic structure calculations",
                                       Comput. Phys. Commun. 238, 232 (2019), arXiv:1805.11265 (abstract page).
Not reachable on 9 Nov 2026 (not used): Weber et al., Phys. Rev. Research 4, 043053 (2022) (einsteinium levels; HTTP 403); the 2026 EPJ D paper
"Progress toward identifying atomic transitions in lawrencium" (doi 10.1140/epjd/s10053-026-01198-9; HTTP 429, then a stub page).
