[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/moments/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Nuclear magnetic dipole moments and measured hyperfine splittings (0.31, the Atom Lab's hyperfine structure).

magn_mom_recomm.csv
  IAEA Nuclear Data Section, table of recommended nuclear magnetic dipole moments (the source report of each value is in its last two columns),
  https://www-nds.iaea.org/nuclearmoments/magn_mom_recomm.csv, downloaded 6 Nov 2026, saved as received (295,121 bytes).
  Columns: z, A ("n.n+n.z"), symbol, level energy (keV), half-life, spin and parity, magnetic dipole moment (nuclear magnetons, uncertainty in brackets),
  method, description, NSR key, journal, INDC report. tools/make_hfs_data.py keeps the ground states (energy 0): 923 nuclides, one line each.

eides_grotch_shelyuto_2001.pdf
  M. I. Eides, H. Grotch, V. A. Shelyuto, "Theory of light hydrogenlike atoms", Physics Reports 342 (2001) 63-261, arXiv:hep-ph/0002158,
  https://arxiv.org/pdf/hep-ph/0002158, downloaded 6 Nov 2026. Used for the MEASURED ground-state hyperfine splittings the tests compare with:
  eq. (406) hydrogen 1 420 405.751 766 7 (9) kHz; eq. (410) deuterium 327 384.352 521 9 (17) kHz.
  0.33: also eq. (274)/(275), Breit's relativistic factor of an nS level, and Table XIX (hydrogen: the nonrecoil total 1.001 136 089 6, Zemach -42.4e-6,
  recoil +5.22e-6 and +0.4585e-6), which tests/test_hfs.inc uses to close the gap between the lab and the measured 21 cm line.

shabaev_et_al_1997.pdf (0.33)
  V. M. Shabaev, M. Tomaselli, T. Kuehl, A. N. Artemyev, V. A. Yerokhin, "Ground state hyperfine splitting of high Z hydrogenlike ions",
  Phys. Rev. A 56, 252 (1997), arXiv:physics/9706031, https://arxiv.org/pdf/physics/9706031, downloaded 6 Nov 2026, saved as received.
  Used: eq. (2), the 1s relativistic factor A(aZ) = 1 / (gamma (2 gamma - 1)); Table 3, A(aZ) and the nuclear size correction delta for 15 ions
  (In-113 to Bi-209); Table 4, hydrogen-like holmium-165 for mu = 4.132: non-relativistic 1.4945 eV, relativistic point nucleus 2.3007 eV, and the
  measurement it quotes, 2.1645(6) eV = 572.79(15) nm (J. R. Crespo Lopez-Urrutia et al., Phys. Rev. Lett. 77, 826 (1996); the OSTI abstract of that
  paper gives 5726.4(1.5) Angstrom, 0.03 % shorter: not read here, the tests' margin is far wider).
