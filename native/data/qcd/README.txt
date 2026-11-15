[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/qcd/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Sources for the nucleus, nucleon and quark views of the Atom Lab (native 0.42)
==============================================================================

Every file here was fetched on 2026-11-08 (UTC) with one plain GET (curl, User-Agent "PhysicsLab-native-data-tool") and saved exactly as received.
Each folder's MANIFEST.tsv lists: file, URL, date, HTTP status, size in bytes, the first 16 hex digits of its sha256.
Every number the app or its tests use from these papers was read in the saved file (pdftotext), not from memory. Journal references were checked against
INSPIRE-HEP (inspirehep.net API). Reachable: arxiv.org, pdg.lbl.gov, physics.nist.gov, inspirehep.net. Not reachable: www-nds.iaea.org pages (bot challenge),
journals.aps.org (403), so journal versions were not read; the arXiv versions were.

nucleus/
  prex2_adhikari_2021_2102.10767.pdf   PREX-II, PRL 126, 172502 (2021): Pb-208 skin R_n - R_p = 0.283 +- 0.071 fm.
  crex_adhikari_2022_2205.11593.pdf    CREX, PRL 129, 042501 (2022): Ca-48 skin 0.121 +- 0.026 (exp) +- 0.024 (model) fm.
  centelles_2009_0806.2886.pdf         Centelles et al., PRL 102, 122502 (2009): quotes the antiprotonic-atom fit of 26 nuclei, S = (0.9 +- 0.15) I + (-0.03 +- 0.02) fm
                                       (Trzcinska et al., PRL 87, 082501 (2001), not on arXiv, not reached).
  klos_2007_nucl-ex0702016.pdf         Klos et al., PRC 76, 014311 (2007): the earlier fit "gave alpha = 0.90 +- 0.15 fm"; a global fit "allows alpha values between 0.9 fm
                                       and 1.3 fm" (0.154 to 0.240 fm for Pb-208).
  ong_berengut_flambaum_2010_1006.5508.pdf  PRC 82, 014320 (2010), eq. (8): <r^2>_ch = <r^2>_pp + <R_p^2> + (N/Z) <R_n^2> + 3/(4M^2) + <r^2>_so; 3/(4M^2) = 0.033 fm^2.
  kitano_koike_okada_2002_hep-ph0203110.pdf  PRD 66, 096002 (2002), Table 7: proton-density parameters (2pF c, z) taken from de Vries et al. 1987.
  phobos_glauber_loizides_2015_1408.2549.pdf Loizides, Nagle, Steinberg, arXiv:1408.2549: Table I, charge-density Fermi parameters from de Vries et al. 1987
                                       (Au-197 6.38 / 0.535, Pb-207 6.62 / 0.546, Pb-208 6.624 / 0.549 fm); Glauber hard core d_min default 0.4 fm.
                                       (de Vries, de Jager, de Vries, ADNDT 36, 495 (1987) itself was not reached: two secondary quotes are used.)
  drischler_2016_1510.06728.pdf        PRC 93, 054314 (2016): empirical saturation density n0 = 0.164 +- 0.007 fm^-3.
  machleidt_cdbonn_2001_nucl-th0006014.pdf  (0.46.5, fetched 2026-11-09) Machleidt, PRC 63, 024001 (2001), the CD-Bonn potential. Appendix D: the deuteron's S and D waves
                                       u(r), w(r) as sums of exponentials, eqs. (D19)-(D25), gamma = 0.2315380 fm^-1, m0 = 0.9 fm^-1, Table XX (C1..C10, D1..D8; C11, D9..D11 from
                                       the constraints). Table XV: matter radius 1.966 fm, Q_d 0.270 fm^2 (no meson currents), P_D 4.85 %; Table XIX: u, w on a grid (the tests' check).
  shukla_2003_nucl-th0302030.pdf       (0.46.5, fetched 2026-11-09) Shukla, arXiv:nucl-th/0302030, eq. (19) and Table I: harmonic-oscillator charge densities
                                       rho = rho0 (1 + alpha r^2/R^2) exp(-r^2/R^2) from de Vries et al. 1987: C-12 alpha 1.082, R 1.692 fm; O-16 alpha 1.544, R 1.833 fm
                                       (O-16 the same as Loizides Table I "Oho": two secondary quotes agree).
  star_2024_2401.06625.pdf             (0.46.5, fetched 2026-11-09) STAR Collaboration, Nature 635, 67 (2024), "Imaging shape of atomic nuclei in high-energy nuclear collisions":
                                       U-238 beta2 = 0.297 +- 0.015, gamma = 8.5 +- 4.8 deg; "The nuclear shape in the intrinsic frame is not directly observable in low-energy
                                       experiments"; spectroscopy captures "a coherent superposition of wavefunctions in all orientations"; "Nuclear shapes, even in ground states, are not fixed."

nucleon/
  codata2022_*.html                    NIST CODATA 2022 pages: rp 0.840 75(64) fm; rd 2.127 78(27) fm; mupsmun 2.792 847 344 63(82); munsmun -1.913 042 76(45);
                                       mp, mn (kg); mpc2mev 938.272 089 43(29) MeV; mnc2mev 939.565 421 94(48) MeV.
  pdg2024_list_p.pdf, _n.pdf           PDG 2024 listings: proton radius 0.8409 +- 0.0004 fm (with the older ~0.88 fm results), magnetic radius 0.851 +- 0.026 fm;
                                       neutron <r^2> = -0.1155 +- 0.0017 fm^2, magnetic radius 0.864 (+0.009 -0.008) fm.
  filin_2020_1911.04877.pdf            PRL 124, 082501 (2020): r_n^2 = -0.106 (+0.007 -0.005) fm^2 (chiral EFT, deuteron), 1.7 sigma from PDG; eq. (2): r_str^2 = r_d^2 - r_p^2 - r_n^2 - 3/(4 m_p^2).
  atac_2021_2103.10840.pdf             A1 (Atac et al., Nature Commun. 12, 1759 (2021)): <r_n^2> = -0.110 +- 0.008 fm^2; eq. (5) GEn = (1 + Q^2/A)^-2 B tau / (1 + C tau),
                                       A = 0.505 (GeV/c)^2, B = 1.655, C = 0.909 (0.43: the neutron's GEn in the quark clouds).
  bbba05_bradford_2006_hep-ex0602017.pdf  Bradford, Bodek, Budd, Arrington (BBBA05), Nucl. Phys. B Proc. Suppl. 159, 127 (2006), Table 1: GEp a1 = -0.0578, b = 11.1, 13.6, 33.0;
                                       GMp/mu_p a1 = 0.150, b = 11.1, 19.6, 7.54; GMn/mu_n a1 = 1.81, b = 14.1, 20.7, 68.7 (0.43: the quark clouds).
  venkat_2011_1010.3629.pdf            Venkat, Arrington, Miller, Zhan, PRC 83, 015203 (2011), Table II: an independent proton fit (0.43: cross-check of the clouds, 1 %).
  kelly_2002_hep-ph0204239.pdf         PRC 66, 065203 (2002): rest-frame densities; neutron "positive interior density is balanced by a negative surface lobe" (model dependent).
  miller_2007_0705.2409.pdf            PRL 99, 112001 (2007): transverse densities (model independent); the neutron's central density is negative; central u density ~70 % above d.

quarks/
  pdg2024_sum_quarks.pdf               PDG 2024: m_u 2.16 +- 0.07, m_d 4.70 +- 0.07, m_s 93.5 +- 0.8 MeV (MS-bar, 2 GeV).
  flag2024_2411.04268.pdf              FLAG 2024: Nf = 2+1+1 m_u 2.14(8), m_d 4.70(5) MeV; Nf = 2+1 m_u 2.27(9), m_d 4.67(9) MeV.
  pdg2024_rev_quark_masses.pdf         constituent masses "of order 350 MeV"; quark masses are scheme dependent.
  pdg2024_rev_quark_model.pdf          colour singlets; mu_n/mu_p = -2/3 predicted, -0.6849793 measured; constituent mass m = 2 m_p / g_p = 336 MeV.
  yang_2018_1808.08677.pdf             PRL 121, 212001 (2018): quark energy 33(4)(4) %, glue 37(5)(4) %, trace anomaly 23(1)(1) %, quark condensates 9(2)(1) % (MS-bar, 2 GeV).
  ji_2021_2102.07830.pdf               Front. Phys. 16, 64601 (2021): the mass decomposition and its interpretations (not unique).
  hoferichter_2015_1506.04142.pdf      PRL 115, 092301 (2015): sigma_piN = 59.1 +- 3.5 MeV (Roy-Steiner, pi N scattering).
  gupta_2021_2105.12095.pdf            PRL 127, 242002 (2021): lattice sigma_piN 41.9(4.9) MeV (standard), 59.6(7.4) MeV (with N pi states).
  zeus_2016_1604.01280.pdf             PLB 757, 468 (2016): effective quark radius < 0.43e-16 cm (95 % C.L.).
  pdg2024_rev_compositeness.pdf        PDG 2024: contact-interaction limits 9.2-29.5 TeV by model; Lambda-_LL > 24 TeV.
  bissey_2007_hep-lat0606016.pdf       PRD 76, 114512 (2007): baryon gluon field: bubble (<= ~0.3 fm), filled triangle (~0.4 fm), Y tubes (> 0.5 fm); tube radius 0.38(3) fm,
                                       node 0.47(2) fm; no Delta shape. Quenched, static quarks.
  takahashi_2001_hep-lat0006005.pdf    PRL 86, 18 (2001): the three-quark potential follows the Y ansatz (sigma_3Q ~ sigma_QQbar). Read for context, no number used.
  duran_2023_2207.05212.pdf            Nature 615, 813 (2023), Table 1: gluonic mass radius 0.755 +- 0.035 (holographic), 0.472 +- 0.042 (GPD), lattice 0.7464 +- 0.025 fm;
                                       scalar radius 1.069 / 0.695 / 1.073 fm; A_g tripole mass m_A = 1.641 +- 0.043 GeV (lattice; Pefkou, Hackett, Shanahan, PRD 105,
                                       054509 (2022)), used for the gluon cloud (0.43).
  sizes_of_nucleon_2404.11292.pdf      Kaiser and Weise, PRC 110, 015202 (2024): mass radius 0.55 +- 0.03 fm (from GlueX J/psi photoproduction, as quoted).

sea/
  e866_towell_2001_hep-ex0103030.pdf   PRD 64, 052002 (2001): integral of (dbar - ubar) = 0.118 +- 0.012.
  seaquest_dove_2021_2103.04024.pdf    Nature 590, 561 (2021): dbar > ubar over 0.13 < x < 0.45.
  seaquest_2023_2212.12160.pdf         PRC 108, 035202 (2023), Table VII: dbar/ubar 1.338 .. 1.645 in six bins, 0.13 < x < 0.45.
  atlas_strange_2017_1612.03016.pdf    EPJC 77, 367 (2017): strange-to-light sea ratio "close to unity".
  hermes_2014_1312.7028.pdf            PRD 89, 097101 (2014): strange distribution softer than the light antiquarks (debate with ATLAS).
  compass_g1p_2016_1503.08935.pdf      PLB 753, 18 (2016): quark spin Delta Sigma 0.26 .. 0.36.
  defl_2014_1404.4293.pdf              PRL 113, 012001 (2014): gluon polarisation non-zero for 0.05 < x < 0.2.
  ji_yuan_zhao_2021_2009.01291.pdf     Nature Rev. Phys. 3, 27 (2021): integral over x > 0.05 of Delta g = 0.20 (+0.06 -0.07) (DSSV14); Delta Sigma ~ 0.30 +- 0.10.
  alexandrou_2020_2003.08486.pdf       PRD 101, 094513 (2020): lattice: 1/2 Delta Sigma = 0.191(15) (Delta Sigma = 0.38), J_g = 0.187(46), momentum fractions quarks 0.618(60), gluons 0.427(92).
quarks/ (added in a later research round)
  fullqcd_fluxtube_2024_2409.20168.pdf Baker, Cea, Chelnokov, Cosmai, Papa, EPJC 85, 29 (2025): flux tubes with dynamical light quarks studied for quark-antiquark pairs only
                                       (so the three-quark field shape of Bissey 2007 is still from heavy static quarks). Read for context, no number used.
  takahashi_suganuma_2004_hep-lat0407011.pdf  Suganuma, Ichie, Takahashi, hep-lat/0407011 (2004, conference proceedings): Y-type flux tube in baryons when the quarks are
                                       more than about 0.5 fm apart. Read for context, no number used.
  cates_2011_1103.1808.pdf             Cates, de Jager, Riordan, Wojtsekhowski, PRL 106, 252003 (2011): flavour-separated form factors; the d-quark parts fall faster with Q^2
                                       (the reason the drawn d quarks sit a little farther out). No number used.

densities/ (0.43, computed, not downloaded: native/tools/make_quark_density.py; see densities/README.txt)
  densities.csv                        transverse densities of the proton and neutron charge, u and d per quark, and the gluons, b = 0 .. 4 fm.

tomography/ (0.44: where the momentum is, by parton and momentum fraction; see tomography/README.txt and MANIFEST.tsv there)
