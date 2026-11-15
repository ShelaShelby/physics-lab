[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/molecules/vib/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Molecule Lab vibrations (0.23 stage 1): CALCULATED, not measured
==================================================================

Made once by native/tools/make_vib_data.py (the app only reads its result, native/src/molecule/vib_data.inc):
  python3 native/tools/make_vib_data.py native/data/molecules/xyz native/src/molecule/vib_data.inc native/data/molecules/vib
  PySCF 2.14.0 (pip install pyscf), run in the cloud session on 4 Nov 2026 (4 cores, about 1-2 hours, benzene most of it).

Method, for each of the 15 viewer molecules, at EXACTLY the app's geometry (xyz/<id>.xyz: the measured CCCBDB shape):
  - Kohn-Sham DFT, B3LYP (PySCF "b3lyp"), aug-cc-pVTZ, DFT grid level 3, SCF to 1e-11 hartree; O2 unrestricted triplet, the rest restricted
    (ozone closed-shell: an approximation, it has multi-reference character).
  - The analytic Hessian; masses of the most abundant isotopes (NIST, as the app: 1H 1.00782503223, 12C 12, 14N 14.00307400443, 16O 15.99491461957,
    35Cl 34.968852682 u); translations and rotations projected out (Eckart); harmonic wavenumbers and mass-weighted unit vectors.
  - Dipole derivatives by central differences (0.005 bohr on every Cartesian coordinate); harmonic IR intensity N_A/(12 eps0 c^2) |dmu/dQ|^2, km/mol.
  - Constants CODATA 2022.
  A first run with aug-cc-pVDZ for all 15 put the bends of acetylene (-20 %, -10 %) and HCN (-10 %) outside CCCBDB's 3-sigma band and H2 (+8 %) just
  outside; with aug-cc-pVTZ (the Density style's basis) they are inside. Benzene's aug-cc-pVDZ run (4615 s) put its out-of-plane bend at -8.2 %
  (3 sigma for that basis: 7.8 %). Its aug-cc-pVTZ run (0.23 stage 2): VIB_FD=1 VIB_SYM=1, the Hessian by central differences of the analytic
  gradient (0.005 bohr), only one C and one H displaced, the other atoms' rows from benzene's own symmetry operations; checked first on water (FD against
  the analytic Hessian: 0.03 cm^-1) and methane (with symmetry: 0.2 cm^-1); each displaced result saved at once (the cloud machine restarts while idle).
  12 calculations, 50 min on 4 cores. All 20 fundamentals within 3.2 % of CCCBDB. Run: VIB_BASIS=aug-cc-pvtz (C6H6 also VIB_FD=1 VIB_SYM=1);
  vib_data.inc is the 15 .inc.part files put together (the order of tools/make_vib_data.py's IDS).

The measured shape is not the B3LYP minimum, so the energy gradient is not zero there (each <id>.txt gives the largest component); the harmonic
analysis is done at the measured shape anyway, so the modes belong to the shape the app shows.

<id>.txt: the data sheet of each molecule (energy, largest gradient, every mode's wavenumber and intensity).

MEASURED (cccbdb/): NIST CCCBDB, SRD 101, experimental pages, fetched 4 Nov 2026 by curl (User-Agent "PhysicsLab-native-data-tool"), saved as received:
  cccbdb_exp2x_<id>_2026-11-04.html  https://cccbdb.nist.gov/exp2x.asp?casno=<CAS>&charge=0 : the "Vibrational levels" table (mode, symmetry,
                                     fundamental, harmonic, reference, IR intensity, uncertainty, reference, description)
  cccbdb_vibscalex_2026-11-04.html   https://cccbdb.nist.gov/vibscalex.asp : the scaling factor and its 1-sigma uncertainty; B3LYP/aug-cc-pVDZ
                                     0.970 +- 0.026 (182 molecules, 1418 frequencies); aug-cc-pVTZ 0.968 +- 0.019 (137 molecules, 1447 frequencies)
  cccbdb_vsfx_2026-11-04.html        https://cccbdb.nist.gov/vsfx.asp : the same factors to four digits, 0.9704 and 0.9675
  native/tools/make_vib_measured.py reads them and pairs each measured mode with its calculated mode(s) (symmetry first: totally symmetric or
  not, g or u; then the scaled wavenumber and the stretch / bend description), writing native/src/molecule/vib_measured.inc.

Checks (tests/test_vibration.cpp): every calculated mode paired with a measured one; 0.9675 x calculated within 3 sigma of the measured
fundamental, except methanol's torsion (on CCCBDB's big-anharmonicity list, cccbdb_biganharm1x_2026-11-04.html) and ammonia's umbrella
(compared with the nearer level of its measured inversion doublet, 932.5 / 968.3 cm^-1, NIST WebBook, webbook_NH3_vib_2026-11-04.html); measured bands only on modes that can absorb; 3N-6 / 3N-5 modes, orthonormal, no translation or rotation, the group-theory selection rules and degeneracies,
the diatomic turning point, the X-H stretch region.

Correction (0.41): HCN's CN stretch. CCCBDB lists 2089 (fundamental) and 2041 (harmonic), reference 1945Herzberg; the NIST WebBook's Shimanouchi table
(https://webbook.nist.gov/cgi/cbook.cgi?ID=C74908&Mask=800, saved as cccbdb/webbook_HCN_vib_2026-11-08.html) gives 2096.85 cm^-1 for the gas (infrared,
rated A) and 2089 for the liquid (Raman). make_vib_measured.py uses 2096.85 and leaves the harmonic out (CORRECT in the tool).

webbook_diatomic/ (0.34)
  NIST Chemistry WebBook (SRD 69), "Constants of diatomic molecules" (K. P. Huber & G. Herzberg, 1979) for H2, N2, O2 and HCl,
  https://webbook.nist.gov/cgi/cbook.cgi?ID=<CAS id>&Mask=1000 (C1333740, C7727379, C7782447, C7647010), downloaded 6 Nov 2026 in a cloud session,
  saved as received. Used: the X ground state row (we, wexe, weye) and its footnotes (H2 118, N2 109, O2 69, HCl 26 and 27: the higher terms and
  where the constants hold); the tests also use the measured Delta G(1/2) the footnotes give (N2 2329.9168, O2 1556.381 cm^-1).
  0.53: the same rows' Be, alpha_e and De (N2 D0; O2 B0 and D0, in brackets) for rotation (src/molecule/rotation.cpp). DCl's page
  (C7698057, D35Cl) downloaded 14 Nov 2026 the same way, saved as nist_webbook_dcl_constants.html: only tests/test_rotation.cpp reads it, to check
  the Born-Oppenheimer scaling of H35Cl (we -0.0022 %, Be +0.0046 %, alpha_e +0.029 %, De +1.3 %).

hcl_lines/ (0.53)
  D. H. Rank, D. P. Eastman, B. S. Rao and T. A. Wiggins, "Rotational and vibrational constants of the HCl35 and DCl35 molecules", J. Opt. Soc. Am.
  52, 1 (1962), the PDF as received (diverdi.colostate.edu course references, 14 Nov 2026), and rank1962_table3.txt: its Table III (H35Cl 1-0 band,
  observed and calculated lines), read from the PDF's text layer and checked against the page image (how: the file's head).
