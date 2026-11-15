[0.54.4] The papers, preprints, journal scans, saved web pages and downloads of unclear terms that this README names are NOT in this public repo any more: they are in the private
repo ShelaShelby/physics-lab-sources at data/molecules/ (same names; the other files named here are still in this folder). native/data/SOURCES.md lists each moved file with its
reference, URL, date read and sha256. Where a test reads one, its extract (<name>.extract.txt: the lines it needs, copied exactly) is in this folder.

Molecule Lab data (0.18 stage 1)
================================
(0.21: the Realistic style's COMPUTED electron density and potential are in density/, with their own README.txt; the coordinates they were computed at are in xyz/.)

1. Geometry and rotational constants: NIST CCCBDB
   NIST Computational Chemistry Comparison and Benchmark Database, NIST Standard Reference Database 101, Release 22,
   experimental geometry pages and experimental rotational constants pages, https://cccbdb.nist.gov
   History: on 18 Oct 2026 cccbdb.nist.gov could not be reached from the cloud session, so the owner read the values on CCCBDB and
   gave them to the job as text, saved word for word in cccbdb_values.txt. The site has been reachable since (sections 4, 5 and 7);
   the app cites it as "NIST CCCBDB, SRD 101, Release 22" with each value's reference code.
   0.50 (11 Nov 2026): every geometry value is now checked against CCCBDB's own pages, see section 7.
   Point groups: not in the text given, so the app shows none; the symmetry each shape needs is stated below and in the app.
   Units: distances in angstrom, angles in degrees, rotational constants in cm^-1. "re" = equilibrium distance; "rm structure"
   (acetylene) as CCCBDB calls it. Uncertainties: none were given with these values.

   Symmetry used to build each 3D shape from the listed values (src/molecule/molecule.cpp; tests/test_molecule.cpp checks it):
     H2, N2, O2, HCl     diatomic: the bond length fixes it.
     CO2, HCN, C2H2      linear (listed angle 180).
     H2O, O3             two equal bonds and the listed angle (C2v).
     NH3                 three equivalent N-H (symmetric top, C3v): one length and one angle fix the pyramid.
     CH4                 regular tetrahedron (Td): every H-C-H = arccos(-1/3) = 109.4712; the listed 109.471 is that value rounded.
     H2CO                planar C2v: H-C-O = 180 - (H-C-H)/2 = 121.93; the listed 121.9 is that rounded ("from aHCH by symmetry").
     C2H4                planar (CCCBDB), D2h: four equal C-H; 2 x 121.2 + 117.6 = 360 exactly.
     CH3OH               Cs: the three C-H assumed equivalent and symmetric about the C-O axis, so H-C-H 109.03 fixes H-C-O
                         (DERIVED, 109.89; CCCBDB lists no H-C-O angle); H-C-O-H = 180 for one methyl H (the O-H in that plane).
     C6H6                planar regular hexagon (C-C-C and H-C-C 120), D6h.

2. Isotope masses: NIST "Atomic Weights and Isotopic Compositions"
   https://physics.nist.gov/cgi-bin/Compositions/stand_alone.pl?ele=<X>&ascii=ascii2&isotype=some  for X = H, C, N, O, Cl,
   fetched 18 Oct 2026 by curl with the User-Agent "PhysicsLab-native-data-tool", HTTP 200, saved as received:
   nist_compositions_<X>.html. Used: the most abundant isotope's "Relative Atomic Mass" (uncertainty in brackets) and
   "Isotopic Composition": 1H 1.00782503223(9), 0.999885(70); 12C 12.0000000(00), 0.9893(8); 14N 14.00307400443(20), 0.99636(20);
   16O 15.99491461957(17), 0.99757(16); 35Cl 34.968852682(37), 0.7576(10). The page's citation text was not found on the pages read
   (the main page and notes.html); the app names the database and its URL.

3. Colours and radii: PubChem periodic table (https://pubchem.ncbi.nlm.nih.gov/periodic-table/), as given in the job text; not
   fetched (pubchem.ncbi.nlm.nih.gov is not reachable from the cloud session). CPK colours H FFFFFF, C 909090, N 3050F8,
   O FF0D0D, Cl 1FF01F; van der Waals radii H 120, C 170, N 155, O 152, Cl 175 pm. Both are drawing conventions.
   Ball-and-stick ball radius = 0.2 x the van der Waals radius (molecule.h kBallScale; 0.25 before 0.22): only a drawing scale.

4. 0.19 audit (18 Oct 2026, later the same day): cccbdb.nist.gov WAS reachable from the cloud session (plain GET, HTTP 200).
   The NH3 experimental data page https://cccbdb.nist.gov/exp2x.asp?casno=7664417 is saved as received:
   cccbdb_nh3_exp2x_2026-10-18.html. It lists the rotational constants A = B = 9.44430, C = 6.19600 cm^-1 (1966Herzberg),
   rNH 1.012, aHNH 106.67 (1966Herzberg) and point group C3v, so the values in cccbdb_values.txt match CCCBDB. Its own
   "Product of moments of inertia" (8.668394 amu^3 A^6) follows from those listed constants, while its own Cartesians (from the
   listed geometry) give 7.588 (A = B = 9.974, C = 6.346 cm^-1): CCCBDB's NH3 page is not consistent with itself. The listed
   value stays in the app, marked doubtful, until a second source is checked (for the owner to check). Other hosts tried for a second
   source (NIST WebBook, PubChem, JPL spectral catalogue, CDMS, Wikipedia): not reachable from the cloud.

5. 2026-11-05 audit: pubchem.ncbi.nlm.nih.gov WAS reachable. Its periodic-table CSV (pug/periodictable/CSV) gives the same CPK colours (H FFFFFF,
   C 909090, N 3050F8, O FF0D0D, Cl 1FF01F) and van der Waals radii (120, 170, 155, 152, 175 pm) as section 3. The NH3 doubt of section 4 is resolved:
   the JPL Molecular Spectroscopy catalogue, entry 17002 version 5 (spec.jpl.nasa.gov/ftp/pub/catalog/doc/d017002.pdf), gives the ground state
   B = 298192.92 MHz and C = 186695.86 MHz, i.e. 9.94662 and 6.22750 cm^-1; CCCBDB's 9.44430 is a slip for about 9.944. The app uses the JPL values.
   The 15 geometries and rotational constants of cccbdb_values.txt match CCCBDB's pages saved in vib/cccbdb/ (4 Nov 2026), and the measured dipoles
   in tests/test_density.cpp match CCCBDB's experimental dipole list (diplistx.asp, read 2026-11-05).

6. 0.46.6 (9 Nov 2026): CO2's Fermi dyad. https://arxiv.org/html/physics/0601182 (C. W. McCluskey and D. S. Stoker, "Raman observations of quantum
   interference in the nu1/2nu2 Fermi dyad region of carbon dioxide", arXiv:physics/0601182, 2006) is saved as received:
   arxiv_physics0601182_co2_fermi_dyad.html. Section II.1: "The Q-branches in CO2 at 1285 cm-1 and 1388 cm-1 are the components of the Fermi dyad",
   nu1 mixed with 2nu2. The app's 1333 cm^-1 for nu1 (CCCBDB, 1945Herzberg) is therefore not an observed line; the panel says so and names the two lines.

7. 0.50 (11 Nov 2026): the experimental-geometry page of each of the 15 molecules, https://cccbdb.nist.gov/expgeom2x.asp?casno=<CAS>&charge=0,
   fetched 11 Nov 2026 by curl (User-Agent "PhysicsLab-native-data-tool"), HTTP 200, saved as received: cccbdb/geom/cccbdb_expgeom2x_<id>_2026-11-11.html
   (CAS: H2 1333-74-0, N2 7727-37-9, O2 7782-44-7, HCl 7647-01-0, CO2 124-38-9, H2O 7732-18-5, O3 10028-15-6, NH3 7664-41-7, CH4 74-82-8,
   HCN 74-90-8, C2H2 74-86-2, H2CO 50-00-0, C2H4 74-85-1, CH3OH 67-56-1, C6H6 71-43-2). The rotational-constant pages are already saved
   (vib/cccbdb/cccbdb_exp2x_<id>_2026-11-04.html; the geometry pages repeat the rotational constants too).
   molecule_logic (testCccbdbPages) parses each page's "Internal coordinates" table and checks every value and reference code in
   src/molecule/molecule.cpp against it: all 38 (lengths, angles and CH3OH's dihedral) equal. NH3's page also lists aXNH 112.15 "from symmetry. X is on symmetry axis." (a dummy atom);
   the test checks the built pyramid gives it (112.15 to the listed digits). The app's derived CH3OH H-C-O angle is not on the page and is not checked.
