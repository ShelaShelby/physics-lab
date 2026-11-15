Quark and gluon clouds of the Accurate quark style (0.43). Computed by native/tools/make_quark_density.py, not downloaded.

What: the transverse densities rho(b), fm^-2: where a quark (or the gluons' momentum) is found at distance b from the centre, in the plane across
the motion of a very fast nucleon (the infinite-momentum frame; the only frame in which a nucleon density is model independent: Miller, PRL 99,
112001 (2007), ../nucleon/miller_2007_0705.2409.pdf). rho(b) = integral Q dQ / (2 pi) J0(Q b) F(Q^2).

Inputs (all in ../nucleon and ../quarks, see ../README.txt):
  F1 = (GE + tau GM) / (1 + tau), tau = Q^2 / 4M^2. GEp, GMp, GMn: BBBA05 (Bradford et al. 2006, Table 1). GEn: Atac et al. 2021, eq. 5.
  (BBBA05's own GEn gives <r_n^2> = -0.083 fm^2, far from the measured -0.1155 (PDG 2024); Atac's gives -0.110.) Masses and moments: CODATA 2022.
  Flavours by isospin symmetry, strange quarks left out (Miller 2007): rho_u = 2 rho_p + rho_n (two u quarks), rho_d = rho_p + 2 rho_n (one d),
  written PER QUARK (each normalised to 1). These are NET densities (quarks minus antiquarks): the sea's pairs cancel; where the sea itself sits is
  not measured. In the neutron the profiles swap.
  Gluons: A_g(t) = A_g(0) / (1 - t / m_A^2)^3, m_A = 1.641 GeV (lattice, Pefkou et al. 2022, in Duran et al. 2023 Table 1); its 2D transform in closed
  form rho_g(b) = m^2 (m b)^2 K2(m b) / (16 pi), <b^2> = 12 / m_A^2 = 0.174 fm^2. (Fits to J/psi data give other masses: 1.575 holographic, 2.71 GPD.)

Results (printed by the tool): u per quark centre 1.73 fm^-2, <b^2> 0.419 fm^2; d centre 1.06 fm^-2, <b^2> 0.436 fm^2 (d wider, as Miller says);
central u / d 1.63 (Miller: about 70 % with Kelly's fit); neutron centre -0.446 fm^-2 (negative, as Miller). Proton centre 1.95 fm^-2, Venkat et al.
2011's independent fit 1.97 (1 %). Grid 0.01 fm to 4 fm; 99.96 % of each cloud lies inside 4 fm.

Limits: fits to data (the high-Q^2 tails are extrapolated); isospin symmetry; no strange quarks; a 2D picture of one frame, not a 3D shape at rest.
