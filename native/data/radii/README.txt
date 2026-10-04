Measured nuclear charge radii for the Atom Lab's nucleus size (0.10.9 stage 1)
=============================================================================

Retrieved on 2026-10-04 (UTC) by native/tools/make_radii_data.py fetch, one plain GET of
  https://www-nds.iaea.org/radii/charge_radii.csv
(HTTP 200, Content-Type text/x-comma-separated-values, Last-Modified: Wed, 21 Feb 2024 07:11:13 GMT, 28,869 bytes, 958 lines,
sha256 683d028dae93235376ddf7a345f736561ee7c0dba070091912c034bdf740eee6). The file is saved exactly as received: charge_radii.csv.
The tool sent its own User-Agent ("PhysicsLab-native-data-tool").

What was and was not reachable (0.10.9 stage 1, from the cloud session):
  - this one static file: yes.
  - the IAEA pages and services around it (www-nds.iaea.org/radii/, /nuclearmoments/, the relnsd data services, the INDC report PDFs): a bot challenge
    ("Just a moment...", HTTP 403) that no script can pass. nds.iaea.org, doi.org, sciencedirect.com, arxiv.org, nndc.bnl.gov, osti.gov, zenodo.org: refused
    by the session's network policy (CONNECT 403). So the page that may say how to cite the file, the file's edition and any units statement could NOT be read.

What the file is (read from the file only):
  header: z,symbol,n,a,radius_val,radius_unc,radius_preliminary_val,radius_preliminary_unc
  957 rows. z = proton number, n = neutron number, a = mass number (a = z + n in every row, checked by the tool).
  radius_val / radius_unc: a radius and its uncertainty; radius_preliminary_val / _unc: a preliminary value and its uncertainty.
  There is no unit column and no text. The unit is the compilation's: fm, and the value is the rms charge radius (see the citation below; the app says
  "rms charge radius" and the unit fm; that the numbers are fm and rms is taken from the compilation's name, it is not stated inside the file).

What the numbers are (counted by native/tools/make_radii_data.py build and again, separately, by the test):
  - 957 rows = the neutron (z = 0) + 956 isotopes of hydrogen to curium (Z = 1..96, 90 of the 96 elements have a value; Tc, Pm, At, Ac, Pa and Np have no row at all, checked against the CSV).
  - 908 isotopes have radius_val (0.8783 fm, hydrogen-1, to 5.9048 fm, americium-243; uncertainties 0.0010 to 0.3139 fm).
  - 48 have ONLY a preliminary value (Be, Mg, K, Ga, Ag, Yb, Tl, Po, Fr isotopes): NOT used by the lab; the card names the preliminary value and says it is not used.
  - 99 have both: the lab uses radius_val and ignores the preliminary one (they differ by up to 0.0215 fm, Fr-228). One preliminary uncertainty is 0.0000 (K-47): as the file has it.
  - the neutron row (z = 0, a = 1): radius_val = -0.1149, radius_unc = 0.0027. A radius cannot be negative, so it can only be a mean-square charge radius in fm^2.
    The file does not say that; the owner's task text does, and the sign leaves no other reading. It is NOT in the table (src/atom/radii_data.inc) and not used.
  - hydrogen-1 (a single proton): 0.8783 +- 0.0086 fm is in the table, but the lab keeps its own proton radius, CODATA 2022 (0.84075 +- 0.00064 fm, drawn 0.84), as everywhere else
    in the app; the card shows the table's number and says it is not used.

Citation. The file carries none. The IAEA page that offers it could not be read. The compilation a table of "experimental nuclear ground state charge radii" from the IAEA's Nuclear Data
Section goes back to is: I. Angeli and K. P. Marinova, "Table of experimental nuclear ground state charge radii: An update", Atomic Data and Nuclear Data Tables 99 (2013) 69.
(Title, authors and the "909 isotopes of 92 elements, 1H to 96Cm" were read from a web-search listing on 2026-10-04; the volume, the page and which edition the IAEA file is were NOT checked
on the journal's or the IAEA's page. The file has 908 isotopes with a value plus the neutron's row = 909 values, which fits that count but proves nothing.) The same listing says the IAEA data
library holds "a modified and updated version" of the tables: the preliminary columns are probably that. OWNER-TODO asks the owner to read the IAEA page and send what it asks to be cited.

What the app does with it (the same words are in About, "Where the facts come from"):
  drawn size   R = sqrt(5/3) x rms charge radius (a uniform ball of radius R has rms radius sqrt(3/5) R: a calculus result; "a uniform ball" is an ASSUMPTION), where radius_val exists
               (and A > 1). Without a value, the usual rule r = 1.2 A^(1/3) fm stays, and the card says "no measured radius for this isotope; size from the usual rule".
  deviation    shown beside the rule: the rule's ball has rms radius sqrt(3/5) x 1.2 A^(1/3) = 0.9295 A^(1/3) fm.
  The nucleon positions are scaled so that the protons' rms radius about the centre of mass is the measured one, with the proton's own size (0.84 fm) added in quadrature.
  A charge radius is the PROTONS' charge distribution; it is not a matter radius, so halo nuclei cannot be drawn from this file, and it carries no shape.

Rebuild: python3 native/tools/make_radii_data.py build  (offline; writes native/src/atom/radii_data.inc, 956 rows).
