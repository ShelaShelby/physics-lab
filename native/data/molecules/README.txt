Molecule Lab data (0.18 stage 1)
================================

1. Geometry and rotational constants: NIST CCCBDB
   NIST Computational Chemistry Comparison and Benchmark Database, NIST Standard Reference Database 101, Release 22,
   experimental geometry pages and experimental rotational constants pages, https://cccbdb.nist.gov
   Reachability: cccbdb.nist.gov is NOT reachable from the cloud session (the network refuses the host: proxy 403, and no DNS
   for the web-fetch path; tried 18 Oct 2026). The owner read the values on CCCBDB on 18 Oct 2026 and gave them to the job as
   text; that text is saved word for word in cccbdb_values.txt. No CCCBDB page could be saved, and the citation form CCCBDB
   asks for could NOT be read from the site; the app cites it as "NIST CCCBDB, SRD 101, Release 22" with each value's reference code.
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
   Ball-and-stick ball radius = 0.25 x the van der Waals radius: only a drawing scale.
