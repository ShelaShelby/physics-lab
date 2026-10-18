Molecule Lab, Realistic style: computed electron density and electrostatic potential (0.21 stage 1)
====================================================================================================

Everything in this folder is COMPUTED, not measured. Made once by tools/make_density_data.py (not by the app); the app embeds the .pld files
(cmake/embed_density.cmake) and reads them with src/molecule/density.cpp. tests/test_density.cpp (density_logic) checks them.

How it was made (18 Oct 2026, in the cloud session):
  1. Geometry: the app's own viewer coordinates, exported by the molecule_xyz tool (tools/molxyz/export_xyz.cpp, built with the unit tests) into
     ../xyz/<id>.xyz (angstrom, CCCBDB experimental shapes, principal axes, centre of mass at 0), converted to bohr with CODATA 2022 a0 = 0.529177210544 A.
       /home/user/build/molecule_xyz data/molecules/xyz
  2. pip install pyscf numpy   (PySCF 2.14.0 was used), then from native/:
       python3 tools/make_density_data.py data/molecules/xyz data/molecules/density
     Kohn-Sham DFT, PySCF's "b3lyp" (libxc HYB_GGA_XC_B3LYP, id 402), basis aug-cc-pVTZ for all 15 (none needed the aug-cc-pVDZ fallback: the slowest,
     benzene, took about 10 minutes), DFT grid level 3, SCF converged to 1e-10 hartree. O2: triplet, UKS, total density (alpha + beta). Every other
     molecule closed shell, RKS. Ozone: the closed-shell RKS is an APPROXIMATION (ozone has multi-reference character); the app says so.
  3. Per molecule, <id>.pld (format at the top of the tool) and <id>.txt (the data sheet: method, energy, the dipole with components, grid sizes):
     - the density on a cubic grid, point values, step 0.25 bohr, 5 bohr beyond the outermost nuclei, grid points at whole multiples of the step
       (so mirror planes and inversions of the molecule map grid points onto grid points); uint16, log-scaled (relative step 3.9e-4) from 1e-7 to 1e4 e/bohr^3;
     - the same density on fine cubes round every non-hydrogen nucleus (step 0.05 bohr; chlorine 0.03), +-0.6 bohr: only for the integrals. The density has a
       cusp at a nucleus, which a 0.25 bohr grid cannot integrate (the plain grid sum gives 10.6 electrons for water). The integral of f rho is
       sum_grid f rho (1 - sum_A w_A) h^3 + sum_A sum_cube f rho w_A hf^3, with w_A a C-infinity step from 1 (r <= 0.1 bohr) to 0 (r >= 0.6 bohr);
     - the electrostatic potential (nuclei minus electrons) on a 0.4 bohr grid, int16 with 1e-4 au per unit (clamped at +-3.27 au, only next to a nucleus);
     - the dipole moment from the density matrix (PySCF dip_moment), atomic units in the file, debye in the sheet (1 e bohr = 2.5417464715 D, CODATA 2022).
  Size: 5.5 MB for the 15 files.

Checks (tests/test_density.cpp, against independent references):
  - electron count of every stored grid: within 0.26 % of 2, 14, 16, 18, 22, 10, 24, 10, 10, 14, 14, 16, 16, 18, 42 (limit 0.5 %);
  - computed dipole vs NIST CCCBDB experimental (debye; given to the job as text, the site was not opened): HCl 1.107 vs 1.093 (NISTDiatomic), HCN 3.038 vs 2.980
    (NSRDS-NBS10), H2O 1.855 vs 1.857 (1991Sho/Ebe:5875-5882), O3 0.659 vs 0.530 (NSRDS-NBS10), NH3 1.519 vs 1.476 (1970Shi:3572-3576), H2CO 2.403 vs 2.332
    (2003The/Cal:8910-8915), CH3OH 1.673 vs 1.672 (1981Sas/Lee:228-230); the eight symmetric molecules 0.0000 (zero by symmetry, NSRDS-NBS10). Limit 0.25 D
    (zeros 0.02 D): all within. No molecule is marked doubtful. The dipole recomputed from the stored grid is within 0.022 D of the exact one (limit 0.05 D).
  - symmetry of the stored density (H2O mirrors, CO2 and N2 inversion and axis, CH4 S4 and C3, C6H6 mirror, half turn and sixth of a turn) within 1 %.
