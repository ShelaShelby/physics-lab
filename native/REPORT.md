# 0.97.0: the outside review of 28 Nov 2026 (second), parts 1 to 5

## What changed
- 1: Molecule Lab, Controls › Rotation › Ortho and para (H2, N2, C2H2): Kept as when warm (default, Most realistic, saved) or In balance. Kept gives each spin form g_set / (g_even + g_odd) over its own levels. Ladder, shares, most populated level, card, lines and the Data panel's table follow. Balance against Woolley, Scott & Brickwedde 1948 Table 12: worst gap 0.0041 percentage points (40 K). Their liquid-H2 rate gives "about 5 days" for the extra ortho to halve. No gas rate was found.
- 2: eq. (14) of Ballesteros 2012 is NOT a black body's colour temperature. tools/bv_blackbody.py (Bessell 1990 B and V, SVO Vega zero points): B-V -0.301 needs a 36,300 K black body; eq. (14) gives 16,640 K, whose own B-V is -0.129. Against Mamajek's 2022 table (B0V: 31,400 K, -0.301; Pecaut & Mamajek 2013 v1 Table 4: 31,500 K, -0.307) eq. (14) is within 1.0 % (G), 1.6 % (K), 3.8 % (F). Colours unchanged; texts and sky.cpp say what is drawn. The paper states no range for eq. (14).
- 3: D0: H2 para 36118.06962(37) cm-1 (Hoelsch et al. 2019, Table II note a). N2 941.310, O2 493.686, HCl 427.643 kJ/mol from ATcT 1.228 (0 K formation enthalpies). Darwent 1970 agrees within its printed uncertainty (O2, HCl). N2 differs by 0.38 kJ/mol.
- 4: Stark limits text: below 2.9 (n = 2), 0.38, 0.089, 0.029 (n = 5) kV/cm the fine-structure splitting exceeds the largest linear shift.
- 5: light.h (the bridge uses the Sun's mean brightness: exact for the total light), the exchange rule for spin-0 nuclei, the Absorbance choice, the dotted ring, the ROADMAP Saturn line rebuilt from 0.54.4 and the thin-ring line merged.

## Tests run
- rotation_tests 558/0 (new testOrthoPara; testMorseMeasured on Hoelsch and ATcT). atom_tests 12419/0 (testStarkFineStructure). orbit_tests 3145/0 (testStarTemperature097). check_sources ok (831 lines, 104 extracts).
- `check.sh job`: 17/18 at first. app_settings failed on the Most realistic list's count (now 6 Molecule rows), fixed; then app_settings 202,612/0 and ui_smoke passed. check_molecule rotation by hand: 11/0.
- The new tests fail on 0.96.0: they call rot::SpinMix, formShare, diracFineSplitAu, and read the new D0 values.

## Not done
- (b) of part 1: kept and in balance agree within 1e-6 at 1000 K (N2, C2H2 also at 3000 K). H2 at 3000 K differs by 3.4e-5: its sum is cut at J = 25 (the centrifugal formula). The test's limit there is the cut's last term, 2.6e-4.
- Part 1: the conversion time is the liquid's only (Woolley et al. p. 455). The owner's "days to weeks" for the gas has no source here.
- Part 2: the owner's premise (eq. (14) = the black body of the star's colour) does not hold. The B and O colours are left as they are: ROADMAP owner-decides line. No Teff swap. The Woolley 1948 scan is 87 MB in the sources repo (GitHub warned, accepted).
- Hardware: none needed.

## References changed on purpose
- test_app: the Most realistic list has 6 Molecule rows (the new Ortho and para row), was 5.
- test_rotation testMorseMeasured: H2's D0 was footnote 125 (36118.3 cm-1, an upper limit), N2, O2, HCl were Darwent 1970. Now Hoelsch 2019 and ATcT 1.228 (newer measurements). The Morse gaps move: H2 +4.5 %, N2 +21.9 %, O2 +24.3 %, HCl +14.3 %.
