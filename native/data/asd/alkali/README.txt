NIST Atomic Spectra Database (ASD) levels and lines of the five alkali atoms (0.46, the outer electron's jumps)
==============================================================================================================
Retrieved on 2026-11-08 (UTC) by native/tools/fetch_alkali_asd.py (the request parameters are in that file), saved exactly as received:
  <X>I_levels.tsv  https://physics.nist.gov/cgi-bin/ASD/energy1.pl   (spectrum "<X> I", cm-1, tab-delimited, with configuration, term, J, uncertainty)
  <X>I_lines.tsv   https://physics.nist.gov/cgi-bin/ASD/lines1.pl    (spectrum "<X> I", 200-20,000 nm, VACUUM wavelengths, observed and Ritz, Aki with its accuracy)
for X = Li, Na, K, Rb, Cs. Citation: A. Kramida, Yu. Ralchenko, J. Reader and NIST ASD Team, NIST Atomic Spectra Database (ver. 5.x), National Institute of
Standards and Technology, Gaithersburg, MD (2026). Used by native/tools/make_alkali_data.py -> src/atom/alkali_data.inc: for each atom the ns, np, nd, nf
levels that sit on the valence orbitals of the app (11 levels per atom) and the electric-dipole lines between them (Type empty), with Aki where ASD gives it.
