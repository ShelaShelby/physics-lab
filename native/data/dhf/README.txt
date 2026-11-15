[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/dhf/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Many-electron atoms of the Atom Lab, 0.41: relativistic (Dirac-Hartree-Fock) orbitals of every neutral atom He..Og, and their nonrelativistic limit.

dhf.bin            The orbitals, built into the app (cmake/embed_dhf.cmake; read by src/atom/hf.cpp). Made by
                       python3 native/tools/dhf/run_atom.py prod Z        (Z = 1..118; numpy and scipy; the results go to $DHF_WORK)
                       python3 native/tools/dhf/run_atom.py cont 116 0.3 1000   (livermorium: its last orbital step was continued with gentler mixing)
                       python3 native/tools/dhf/run_atom.py vis Z         (Z = 1..109)    and    run_atom.py point Z    (Z = 80, 86, 88, 102)
                       python3 native/tools/dhf/make_dhf_data.py          (packs them; the format is in its docstring)
summary.tsv        Per element: configuration and level with their source, the computed ground J against it, total energies, convergence.
visscher_check.tsv Every orbital's <r> of run_atom.py in Visscher & Dyall's own configuration (vis mode) against their Table IV.
koga_point_check.tsv Every orbital's <r> of the nonrelativistic limit with a POINT nucleus (run_atom.py point Z) for Hg, Rn, Ra and No: the model of Koga et al.,
                   whose orbitals the tests compare with it (for these heavy atoms the app's finite nucleus alone moves an s orbital by about 1e-4).
visscher_table1.html, visscher_table2.html, visscher_table4.html
                   Saved as received on 7 Nov 2026 from the authors' page of
                   L. Visscher and K. G. Dyall, "Dirac-Fock atomic electronic structure calculations using different nuclear charge distributions",
                   At. Data Nucl. Data Tables 67, 207-224 (1997): http://www.few.vu.nl/~visscher/FiniteNuclei/FiniteNuclei.htm (Table1.html, Table2.html, Table4.html):
                   nuclear parameters, total energies and <r> of every orbital, Z = 1..109. Used as the reference of the tests and of visscher_check.tsv.

The calculation (tools/dhf/radhf.py, ci.py, run_atom.py; written for this app, 0.41):
  - Dirac-Coulomb Hamiltonian, no Breit or QED terms; speed of light 137.035999177 a.u. (CODATA 2022).
  - Nucleus: a Gaussian charge distribution with R_rms = 0.836 A^(1/3) + 0.570 fm (Visscher & Dyall's model); A = the app's default isotope (src/atom/atom_data.inc).
  - Radial functions in even-tempered Gaussians (ratio 1.9, from 0.004 bohr^-2 to 40 times the nuclear exponent) with restricted kinetic balance; integrals on a
    logarithmic grid (1,601 points; elements computed first used 4,001: the results agree to 1e-7).
  - Average-of-configuration Hartree-Fock energy for weighted relativistic configurations ("extended average level"); open shells coupled to the closed ones of
    the same kappa by a Guest-Saunders operator.
  - Configurations: NIST ASD 5.12 ground shells (data/hf/nist_ie_all.tsv, H..Hs); Mt..Og from O. R. Smits, P. Indelicato, W. Nazarewicz, M. Piibeleht and
    P. Schwerdtfeger, Phys. Rep. 1035 (2023), doi:10.1016/j.physrep.2023.09.004, Table 2 (arXiv:2301.02553v1, read 7 Nov 2026).
  - Open shells: a configuration interaction among all determinants of the open shells (fixed occupation of each nonrelativistic shell, M = 0 or 1/2) gives the
    ground level and its J (<J^2>); the orbitals are then optimised for the weights of the relativistic configurations in that level, repeated until the weights
    settle. The density drawn is the level's average over M_J (spherical).
  - The nonrelativistic limit: the same code without the small components (average level of the configuration).
Checks: Visscher & Dyall Table IV (every orbital, Z = 1..109, vis mode: worst 1.5e-5 (Lr 6d3/2), except actinium in their configuration [Rn] 7s2 5f1, where run_atom.py
converges to another self-consistent solution with the 5f electron in the outer well (<r> 17.3 bohr against their 2.24; total energy 0.041 hartree LOWER than their
table's), also when started from the collapsed 5f of Th+: not resolved; the app's actinium is NIST's [Rn] 6d 7s2, with no 5f electron; and the closed-shell atoms in the app's tests), P. S. Bagus, Y. S. Lee and K. S. Pitzer,
report LBL-3787 (1975) (gold), the Koga et al. Hartree-Fock orbitals (data/hf) for the nonrelativistic limit, NIST's ground J.
Not included: electron correlation (the largest missing effect: with it the summed <r^2> of neon's electrons is 2.0 % larger, argon's 0.4 % smaller: Lesiuk & Jeziorski,
arXiv:2311.11357, Table IX), Breit and QED terms (much smaller: Breit about 4e-5 of those <r^2> (ibid.); QED makes gold's 6s 0.15 % larger: Sunaga, Salman & Saue 2022).
valence_summary.tsv (0.46): the outer electron's orbitals of Li, Na, K, Rb, Cs (7 per atom, ns..nf) in the frozen-core (V^(N-1)) Dirac-Hartree-Fock potential of the
closed-shell ion (He, Ne, Ar, Kr, Xe cores), made by tools/dhf/valence.py and tools/dhf/make_valence_data.py (-> src/atom/valence_data.inc). Check: Safronova, Johnson &
Derevianko, PRA 60, 4476 (1999), Table I (data/alkali): K 4s 32370.47 (32370), Rb 5s 30570.88 (30571), Cs 6s 27954.08 (27954) cm-1.
