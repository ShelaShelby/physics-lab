[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/qcd/tomography/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Nucleon tomography (0.44): where the proton's momentum is, by kind of parton and by momentum fraction x. Saved as received (MANIFEST.tsv: URL, date, size, sha256).
Used by native/tools/make_parton_tomography.py and native/src/atom/quark_tomo.h (the Accurate quark style's dots).

  1302.4604.pdf        Diehl and Kroll, EPJC 73, 2397 (2013). H_v^q(x,t) = q_v(x) exp(t f_q(x)) (eq. 33); <b^2>_x = 4 f_q(x) (eq. 36); f_q(x) = alpha'_q (1-x)^3 ln(1/x)
                       + B_q (1-x)^3 + A_q x (1-x)^2 (eq. 44). Default fit ABM 1: alpha'_d = 0.861 +- 0.026 GeV^-2 (eq. 50), alpha'_u - alpha'_d = 0.1 GeV^-2 (eq. 47);
                       table 11: A_u 1.264, B_u 0.545, A_d 4.198, B_d 0.206 GeV^-2. PDFs: ABM11 nf = 4, NLO, mu = 2 GeV (section 6.2, table 9).
                       Table 15: average impact parameters b_1^u = 0.627 +- 0.003 fm, b_1^d = 0.638 +- 0.003 fm. Table 14: r_E^p = 0.838 fm, r_nE^2 = -0.1129 fm^2.
  1202.2281.pdf        Alekhin, Bluemlein, Moch (ABM11), PRD 86, 054009 (2012): the parton densities.
  abm11_4n_nlo/        the LHAPDF grid of ABM11 NLO 4-flavour, central member only (from abm11_4n_nlo.tar.gz, LHAPDF set 42030). At 2 GeV (printed by the tool):
                       momentum shares u_v 0.297, d_v 0.115, u + u-bar sea 0.062, d + d-bar sea 0.077, s + c 0.045, gluons 0.405 (sum 1.0000); u_v 2.0002, d_v 1.0001.
  1802.02739.pdf       COMPASS, PLB 793, 188 (2019): DVCS t-slope B = 4.3 +- 0.6 (stat) GeV^-2 at <x_Bj> = 0.056 (x_Bj/2 = 0.028), Q^2 = 1.8 GeV^2;
                       <r_T^2> ~ 2B (hbar c)^2, sqrt = 0.58 +- 0.04 (stat) fm.
  0907.5289.pdf        H1, PLB 681, 391 (2009): DVCS b = 5.41 +- 0.14 +- 0.31 GeV^-2 at Q^2 = 10 GeV^2, "sqrt(<r_T^2>) = 0.64 +- 0.02 fm", x = 1.2e-3, "dominated by sea
                       quarks and gluons"; no shrinkage: alpha' < 0.20 GeV^-2 (95 % CL). (2b (hbar c)^2 gives 0.649 fm.)
  hep-ph_0507286.pdf   Frankfurt, Strikman, Weiss, Ann. Rev. Nucl. Part. Sci. 55, 403 (2005), p. 25: two-gluon form factor from J/psi photoproduction, dipole m_g^2 = 1.1 GeV^2
                       at x ~ 0.1, <rho^2> = 8/m_g^2 ~ 0.28 fm^2; at x ~ 1e-2 .. 1e-3 about 0.35 fm^2; slow growth with ln(1/x).
  1304.5162.pdf        H1, EPJC 73, 2466 (2013): J/psi photoproduction; alpha' = 0.164 +- 0.028 +- 0.030 GeV^-2 (the slow growth that FSW quote).
  0710.0835.pdf        Carlson and Vanderhaeghen, PRL 100, 032004 (2008): transverse densities of a transversely polarised nucleon (not drawn yet: the app's nucleons are unpolarised).

Choices (the app says so): between the measured x values the sea and gluon spreads are linear in ln x, below the smallest measured x constant, above the largest measured x shrunk as (1 - x)^2 from the last measured value (0.45;
b is measured from the centre of momentum, so <b^2> -> 0 as x -> 1, Diehl and Kroll section 7.3; the form is a choice; 0.44 held it constant); sea quarks and antiquarks share one spread;
the spread is a Gaussian in b at each x (exact for the valence fit, eq. 33; a choice for the sea and gluons). Left out: strange and charm, spin.
Compared, not used: the 0.43 clouds (BBBA05 + Atac 2021) give 7 % more <b^2> for the valence quarks; the lattice A_g form factor (Pefkou et al. 2022) gives the gluons
<b^2> = 0.174 fm^2 against 0.30 fm^2 here (momentum weighted).
