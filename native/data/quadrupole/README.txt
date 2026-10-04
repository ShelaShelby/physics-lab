Recommended nuclear electric quadrupole moments for the Atom Lab's nucleus shape (0.10.9 stage 2)
==================================================================================================

Retrieved on 2026-10-04 (UTC) by native/tools/make_quadrupole_data.py fetch, one plain GET of
  https://www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv
(HTTP 200, Content-Type text/x-comma-separated-values, Last-Modified: Fri, 13 Feb 2026 12:30:30 GMT, 135,013 bytes, 1,156 lines = the header + 1,155 data rows,
sha256 e5db6757b13062e79e85456e77fbbfa3d2f1adefbd00bda69c738b06ad640c25). The file is saved exactly as received: elec_mom_recomm.csv. The tool sent its own User-Agent
("PhysicsLab-native-data-tool"). The task text gave the URL and the byte and line counts; the counts and the checksum were checked here, not taken from it.

What was and was not reachable (from the cloud session): this one static file: yes. The directory page www-nds.iaea.org/nuclearmoments/ and the interactive services around it:
a bot challenge ("Just a moment...", 403). nds.iaea.org and the other hosts of stage 1: refused by the network policy. So THE PAGE THAT STATES HOW TO CITE THE TABLE COULD NOT BE READ,
as in stage 1; the citation below is only what the file itself says.

Citation, as far as the file gives it: every row names an INDC report (1,153 rows: indc-nds-0833; 2 rows have none) and an NSR key (Nuclear Science References, e.g. 2008Py02) with the journal
reference of the measurement. The title and author of indc-nds-0833 were not read (a web-search listing in stage 1 showed an INDC(NDS)-0833 page without its title; the table is N. J. Stone's
compilation of nuclear electric quadrupole moments: one row of the sister magnetic-moment file says "Re-evaluated by N.J. Stone (2013StZZ)"). OWNER-TODO asks the owner to read the IAEA page and send the citation.

The columns (names as in the file's header): z, n.n+n.z, symbol, energy [keV], halflife, spin, electric quadrupole [b], method, description, nsr, journal, indc.
THE SECOND COLUMN IS THE MASS NUMBER A (its header "n.n+n.z" is misleading). How it was established (tools/make_quadrupole_data.py repeats the check on every build and stops if it fails; the test repeats it):
  - every one of the 599 ground-state rows is a known nuclide of the lab's NUBASE-derived table (atom_data.inc) when read as (Z, A - Z): 599 of 599. Read as a neutron count, (Z, column) is a known nuclide for only 53 of 599.
  - the 77 rows whose half-life says "stable" are the stable isotopes of that table when read as A: 76 of 77 (the 77th is Eu-151, which the table lists with a half-life of 1.45e26 s); read as N: 0 of 77.
  - first rows: Z = 1 with 2 is the deuteron (spin 1+, Q = +0.0028578 b); Z = 3 with 6, 7, 8 are Li-6, Li-7, Li-8 (spins 1+, 3/2-, 2+); the holmium (Z = 67) rows run 152, 153, 154 ... 165: holmium mass numbers.
  - the element symbol of every row matches its Z in the lab's table (0 mismatches). In the sister file magn_mom_recomm.csv the neutron's row has 1 in this column.
energy [keV] = 0 marks a GROUND STATE: 599 rows. The other 556 rows are excited states (isomers; some have an energy written "0 + x" or "0+x", an unknown energy above the ground state, others "~200", "-70",
"(<50)" ...). THE LAB USES GROUND STATES ONLY: its slider chooses an isotope, which is a nucleus in its ground state, and a moment measured in an isomer says nothing about the ground state. The 556 isomer rows are
counted in the table's header (kQuadIsomerRows) and not stored.
Value cells (units: BARNS, 1 b = 10^-28 m^2 = 100 fm^2 exactly; the brackets give the uncertainty in the last digits): of the 599 ground rows 547 are signed numbers ("+3.58(2)"), 41 UNSIGNED ("0.0132(3)": the file
gives no sign), 4 have a tentative sign ("(-)0.0333(5)"), and 7 are not one clean number: "+0.55(4) or +0.60(4)" (Ga-74), "[-1.72(5)]" with method "not measured" (Pa-231), and five with typing errors:
Rb-97 "+0.70)15)", Xe-117 "+1.14((4)", Pb-199 "'+0.08(9)", Am-242 "-2.44((3)" and At-218, whose row has a broken quotation in the file (its cells are shifted). They are kept as rows that say so and are NEVER repaired
or used: a repair would be a guess.
One defect is handled by name (KNOWN_DEFECTS in the tool): K-41 has two ground-state rows, one "stable" (+0.734(7) b, the stable isotope, as the lab's table has it) and one with half-life 181 ms and -0.15(11) b, which
cannot be K-41; that second row is left out. So the table has 598 rows: 546 signed, 41 unsigned, 4 tentative sign, 7 not clean.
Spin cells: "3/2-" is a firm spin; "(3/2-)" tentative; "9(-)" a firm spin with a tentative parity; the rest ("3- or 4-", "(2)-", "4") is not used. No ground row has spin 0 or 1/2 and none is even-even (as quantum
mechanics requires: such a nucleus has no spectroscopic quadrupole moment); the test checks both.

What the app does with it (the same words are in About, "Where the facts come from"):
  Used for the shape: a SIGNED value with a FIRM spin >= 1 whose conversion needs |beta2| <= 0.6: 498 of the table's 598 rows (512 qualify; 14 need more than 0.6). Not used: 41 unsigned and 4 tentative-sign values
  (the sign is what tells prolate from oblate), 34 signed values with a tentative spin (the conversion needs the spin), 7 not clean rows. The card shows the table's number for every one of them and says why it is not used.
  conversion   the table gives the SPECTROSCOPIC moment Q; the shape needs the INTRINSIC Q0. Strong coupling, K = I: Q = Q0 I (2I - 1) / ((I + 1)(2I + 3)). A model assumption, said in the app.
  shape        a uniformly charged body r = R (1 + beta2 Y20) with the volume kept; beta2 solved from Q0 and the point-proton mean square radius (measured rms charge radius^2 - 0.84^2, data/radii).
  Where no moment is usable the lab keeps its B(E2) value (11 isotopes) or its estimate; the card names which of the three sources the nucleus on screen uses.

Not used in this stage: the magnetic moments, www-nds.iaea.org/nuclearmoments/magn_mom_recomm.csv (295,121 bytes, 2,365 lines; the same columns with "magnetic dipole [nm]"; reachable by a plain GET on 2026-10-04,
checked, not stored). ROADMAP lists it for a later hyperfine stage.

Rebuild: python3 native/tools/make_quadrupole_data.py build  (offline; writes native/src/atom/quadrupole_data.inc, 598 rows).
