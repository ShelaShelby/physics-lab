[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/be2/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

B(E2; 0+ -> 2+) of the 11 nuclei the Atom Lab draws from a measured B(E2) (atom_data.inc kBetaKnown, written by tools/make_be2_data.py).
iaea_<A><sym>_gammas.csv: ENSDF gamma rays from the IAEA LiveChart API (https://nds.iaea.org/relnsd/v1/data?fields=gammas&nuclides=<A><sym>),
  fetched 2026-11-05, saved as received. Each file names its ENSDF evaluators and cut-off date. Column b_e2 = B(E2) down in Weisskopf units.
The sign of each deformation comes from the 2+ spectroscopic quadrupole moments in ../quadrupole/elec_mom_recomm.csv (IAEA, N. J. Stone).
The NNDC B(E2) tables (Pritychenko et al., ADNDT 107 (2016) 1) could not be read from the cloud (their site's certificate chain does not verify).
