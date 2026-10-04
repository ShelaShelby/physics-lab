# Native Physics Lab 0.10.6 (alpha) — stage 6 report: nuclear mass from a source, and stronger tests

Fixes findings 5, 16 and 17 of the stage 1 audit (`git show 90049fe:native/REPORT.md`). Only the Atom Lab; `index.html` untouched (sha256 e5501387ae64…). Tests were written first: the new mass test and the D-alpha test failed on the old code (numbers below). Retrieval date of every source: 2026-10-04.

## Sources: all answered
- `physics.nist.gov` (CODATA pages): **answered**. `https://physics.nist.gov/cgi-bin/ASD/lines1.pl` (ASD lines form): **answered**; it needs `allowed_out=1&forbid_out=1` and gives the tab-delimited list with `format=3`, vacuum wavelengths with `show_av=3` (full query in the test comment).
- `https://www-nds.iaea.org/amdc/` and `…/ame2020/mass.mas20.txt` (AME2020, 3,558 nuclides): **answered**. Nothing was refused. No number below was typed from memory: the AME masses and ASD wavelengths are copied from those files.

## Finding 5: the nuclear mass (atom_core.cpp `nuclearMassMe`, used by `rydM`)
- **Binding table:** `kBeaLines` (generated from the web page; "binding energy per nucleon in keV, from measured atomic masses, mendeleev/AME") holds the **per-nucleon** energy in whole keV; `massBinding` multiplies by A to give the **total** B (MeV). I checked it against AME2020: **all 3,557 nuclides of the table are in AME** (1,008 of them AME's own extrapolated values, marked #).
- **New form:** M = Z·m_p + N·m_n − B/c² (CODATA 2022 masses from stage 2). Against AME2020's atomic mass / m_e − Z the new M is within **5.4e-7** in the worst case over all 3,557 (median 2.6e-7; the rounding to 1 keV per nucleon and the electrons' binding energy, which the table includes, are the cause). The old A·u − Z·m_e was off by up to **1.0e-2** (median 4–5e-4).
- **Which nuclides:** table nuclides **3,557** use the new form; the neutron slider reaches **4,252** nuclides, so **695** beyond the table keep the old form (no mass is known for them; nothing invented). The false "binding energy under 10⁻⁶" comment is replaced by this explanation.
- **Masses before → after (electron masses, relative change)** and effect on the Rydberg energy / wavelength of the 2→3 line:

| nuclide | M old → new | M change | rydM change | 2→3 line |
|---|---|---|---|---|
| H-1 | 1836.153 (special case) → same | 0 | 0 | 0 |
| H-2 | 3644.777 → 3670.484 | +7.05e-3 | +1.92e-6 | 656.292291 → 656.291030 nm (**−1.26 pm**) |
| H-3 | 5467.665 → 5496.923 | +5.35e-3 | +9.7e-7 | −0.64 pm |
| He-4 | 7289.554 → 7294.299 | +6.5e-4 | +8.9e-8 | −0.015 pm |
| Fe-56 | 102055.755 → 101937.190 | −1.16e-3 | −1.1e-8 | +1e-5 pm |
| U-238 | 433755.460 → 433848.100 | +2.1e-4 | +4.9e-10 | below 1e-7 pm |
| Og-294 | 535811.215 → 536201.482 | +7.3e-4 | +1.4e-9 | below 1e-7 pm |
Over the slider: 2 nuclides move the Rydberg energy by more than 1e-6 (H-2, H-3), 45 by more than 1e-7, 295 by more than 1e-8. **Nothing else visible changes** (the H-2 spectrum note still reads 656.29 nm).
- **Test** (`testNuclearMass`, from AME2020 atomic masses typed in with the file URL; NIST m_e): lab M vs AME M − Z for H-1, H-2, H-3, He-4, C-12, Fe-56, U-238: worst **3.899e-7** (Fe-56), guard **3.94e-7** (+1 %); before the fix 7.003e-3 (H-2: −0.700 %; H-3 −0.532 %, Fe-56 +0.116 %). D-alpha by hand from the AME deuteron mass 656.291030 nm vs lab: before 656.292291 (+1.9e-6), after **−8.4e-11**; tolerance 1e-9.
- **Saved references moved (rescaled, none loosened):** `kSpecRef` for the web nuclides H-2, He-4, Fe-56, Og-294 (H-1 unchanged): 4 `rydM` values and 420 lines, each web number × (new ratio of Rydberg energies, written in the test from the NIST masses and the table's B) at the old 1e-14 / 1e-13 limits. Readouts, hints, hashes, dots, Learn texts did not move.

## Finding 16: the spectrum against things the lab did not make (`testSpectrumIndependent`)
- **By hand:** H Lyman-alpha, H-alpha, H-beta and He II 30.4 and 468.7 nm from the Rydberg formula written in the test from the NIST values (He-4 mass from AME2020): lab agrees to 1e-9 (worst 1e-10).
- **NIST ASD:** vacuum Ritz wavelengths of the fine-structure components (J_i, J_k given) of H I and He II. The lab is vacuum, one electron, non-relativistic and level-averaged, so it is compared with the **range** of the components widened by (Zα)² of the wavelength (5.3e-5 for H, 2.1e-4 for He⁺: the size of fine structure, which the lab leaves out). Measured: inside for H-alpha (656.4696 in 656.4523–656.4722 nm) and He II 468.7 nm; just outside by 8.9e-6 (Lyman-alpha 121.568446 vs 121.566824–121.567364 nm), 1.0e-6 (H-beta) and 3.8e-5 (He II 30.38 nm).
- **About:** one new entry for AME2020 and one for the ASD (both "used … in the tests"); `test_app` About edits 15 → 16, same words as `app_text.inc`.

## Finding 17: four basic checks (`testBasicOrbitChecks`, `testSampledRadiusZ`)
- 90 % outline mass of all 30 real orbitals: **0.90000–0.90254** (asserted: ≥ 0.9 and ≤ 0.912 = measured + 1 %).
- 1s normalisation integral = π: measured relative **5.83e-10**; guard 5.9e-10.
- Radial nodes: every one of the 30 orbitals has n − l − 1 sign changes of R(q) (counted on a 0.001 grid up to q = 80).
- Sampled <r>: stage 5 already tests Z = 1 (12 orbitals × 8 seeds, band 10 %); this adds Z = 3 and 26 (6 orbitals × 4 seeds each): worst **0.07226**, guard 0.0730, and the "usually within 10 %" band holds.

## Tests (Linux GCC Release, Ninja, `PL_BUILD_APP=OFF`): all seven pass
orbit 1,540; **atom 18,157** (was 18,147: +10); app 1,385; nbody 37; jpl 55; moon 24; moon_real 59.

## Not done / not tested
Screen, speed, Windows. The 695 slider nuclides beyond the table keep the old mass. The binding table is rounded to 1 keV per nucleon (could be re-made from AME's unrounded values; the effect is below 1e-9 on the lines). Still untouched from the audit: findings 9–12 (unsourced constants already done in stage 2; speed risks wait for timing) and the deuterium/tritium story is closed.
