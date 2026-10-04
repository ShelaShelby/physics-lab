# Native Physics Lab 0.10.9 (alpha) — stage 1 report: every nucleus at its measured size

New card **"Nuclear size (measured)"** and a permanent change to how the Atom Lab sizes every nucleus (no setting). `index.html` untouched (sha256 e5501387ae64…). No helper agents. The three deferred optimisation items stayed deferred. Shape and halo work did NOT start: it is blocked on data (ROADMAP, OWNER-TODO).

## The data
- **URL:** `https://www-nds.iaea.org/radii/charge_radii.csv`, one plain GET by `tools/make_radii_data.py fetch` on **2026-10-04**: HTTP 200, 28,869 bytes, 958 lines, Last-Modified 21 Feb 2024, sha256 683d028d…eee6 (same bytes as my first curl). `build` writes `src/atom/radii_data.inc` (39 KB, 956 rows). README: `data/radii/README.txt`.
- **Refused:** every IAEA page and service (`/radii/`, `/nuclearmoments/`, relnsd, INDC PDFs: bot challenge, 403); `nds.iaea.org`, doi.org, sciencedirect, arxiv, nndc.bnl.gov, osti.gov, zenodo, magneticmoments.info: network policy 403. So the page that says how to cite the file could not be read.
- **Verified from the file, not the task text:** H-1 0.8783±0.0086, He-3 1.9661, He-4 1.6755, Li-9 2.2450, Li-11 2.4820 (±0.0430), neutron row (z=0) −0.1149±0.0027.
- **Citation (unconfirmed):** the file has none. I credit Angeli & Marinova, At. Data Nucl. Data Tables 99 (2013) 69 (title and "909 isotopes of 92 elements" from a search listing; volume/page and the file's edition NOT checked; OWNER-TODO). In About, the card and the README.
- **Counts:** 957 rows = neutron + 956 isotopes (Z 1–96; 90 elements; Tc, Pm, At, Ac, Pa, Np have no row). **908 have a value** (0.8783–5.9048 fm, ±0.0010–0.3139), **48 only a preliminary one**, 99 both.
- **Neutron row:** left out. A negative number cannot be a radius; it is a mean-square charge radius in fm² (the task says so, the file has no unit column).
- **Preliminary columns: ignored.** The file marks them preliminary. Where both exist the value is used (they differ by ≤ 0.0215 fm, Fr-228). The 48 preliminary-only isotopes keep the rule and the card names the preliminary value and says it is not used. K-47's preliminary uncertainty is 0.0000 in the file (kept as is).
- **H-1 is not used:** a single proton keeps the lab's CODATA 2022 proton radius (0.84075(64) fm, drawn 0.84); the table's 0.8783 differs and the card says so. A departure from "replace where a measurement exists", on purpose.

## The relation (an assumption, said in the code, About and the card)
The usual rule r = 1.2 A^(1/3) fm is an **edge** radius; the table is an **rms** radius. A uniform ball of radius R has rms √(3/5)R, so a measured rms is drawn as a uniform ball **R = √(5/3) × rms** (He-4: 1.2910 × 1.6755 = 2.1631 fm vs the rule's 1.9049). Compared with the rule in the same quantity (rule rms = 0.9295 A^(1/3) fm): heavy nuclei agree (Pb-208 −0.11 %, U-238 +1.68 %, Sn-132 −0.50 %, the smallest value); light ones do not: **H-2 +82.9 %, Li-6 +53.3 %, Be-7 +48.8 %, He-3 +46.7 %, Li-7 +37.5 %, H-3 +31.2 %**, C-12 +16.1 %, Fe-56 +5.1 %; median +2.2 %. The relation was written from knowledge, not looked up.

## What is measured vs not
Slider offers **4,252 isotopes: 907 sized by a measurement** (908 minus H-1), **3,345 by the rule** (48 of them have only a preliminary value). The card says "no measured radius for this isotope; size from the usual rule" for those.

## What changed in the picture
- `nuclearRadius(Z,N)` (framing, zoom, "The nucleus is ~N× smaller", "chance the electron is inside the nucleus") and `buildNucleus` (nucleon positions). Each snapshot is **scaled so its protons' rms about the centre of mass is exactly the measured one** (proton size 0.84 fm in quadrature). Reason: first try (just starting from the measured size) drew 2–5 % too big, C-12 +12.8 %, because the 1 fm hard core spreads nucleons. The same inflation is in the web's rule layout (C-12 +5.3 %, Fe-56 +3.8 %) and the rule layout is also **too small for few-nucleon nuclei** (He-3 −19.4 %, Li-6 −29.9 %, Be-9 −15.6 %): doubtful existing detail, reported, not changed (`buildNucleusByRule`).
- Deuteron: half-separation 1.6 → 1.9705 fm (= √(2.1421² − 0.84²)).
- **Saved references moved (all nine measured ones of the ten saved nuclei):** the web's saved positions are not changed; they now test `buildNucleusByRule`, which is the web layout (word for word). In the whole-lab runs 11 lines moved, rebuilt from typed CSV values and printed by the test: hint "~N× smaller": He-3 15,000→10,000, He-4 14,000→12,000, Li-7 120,000→89,000 / 69,000→50,000 / 31,000→22,000, Fe-56 440→420, U-233 78→77; "chance inside": He-3 3.7e-13→1.2e-12, He-4 5.0e-13→7.3e-13. Pb, Bi, Cm lines did not change at 2 digits.
- The shape (β₂, from B(E2) or an estimate) already existed and is untouched; the task text said the nucleus is drawn round, which is not right. Measured quadrupole moments are not used.

## Tests (written first, Linux GCC Release): orbit 1,540; **atom 18,578 (+160)**; **app 1,519 (+1)**; nbody 37; jpl 55; moon 24; moon_real 59 — all pass
- From the CSV (typed, counted by a separate script): 956/908/48/99 rows, counts for each of the 96 elements, 24 spot values, ranges (guard = CSV extremes ±1 %), no z=0 row, K-47, Be-12, Li-10.
- Hand arithmetic: √(5/3), He-4 2.1631, Pb-208 7.1020 and rule rms 5.5074, eight deviations (to 0.01 %), deuteron 1.9705, Og-294 rule 7.9793 (my first typed value, 7.9902, was wrong; the test caught it).
- Layout: the solver against the test's own Simpson integral (1e-5) and 3/5R²+7/5π²a² (2e-3); drawn protons' rms = measured for 9 nuclei, worst **5.6e-16** (guard 1e-9; exact by construction); the rule layout is off by up to 29.9 % so the check tells them apart. **Overlap:** closest two nucleons in 9 nuclei × many builds 0.680 fm (C-12), guard 0.673 (measured −1 %). Guarantee: hard core 1.0 fm, relaxed to 0.55 only when crowded, then the snapshot scale (about 1). Nucleon balls are 0.84 fm in radius, so they overlap as they did before. Density of the drawn ball, A ≥ 12: 0.06705 (Ne-17) – 0.14023 fm⁻³ (guard ±1 %).
- Known cases from the data: He-4 < He-3; Li falls to Li-9, rises at Li-11. **Li-11 vs Li-7 is +0.038 ± 0.060 fm: not distinguishable by charge radius**, so the app does not call it larger (the task's "much larger" was a matter-radius idea).
- No `long double`, no byte hashes. Also passed with `-O2 -mfma -ffp-contract=fast` (fused multiply-add): 18,578 checks, layout numbers identical.

## Perturbations (each reverted, file compared byte for byte)
Radius ignored 39 failures; units ×1000 45; fallback scaled wrongly 22; √(5/3) dropped 45; preliminary used 4; H-1 from the table 102; rule rms without √(3/5) 12; snapshot not scaled 1 (the layout check); layout ignores measurement 3; deuteron kept at 1.6 fm 1; lab frames by the rule 13. All caught.

## Speed (in principle; Linux GCC, not the owner's PC)
Per frame: one binary search in 956 rows (≈ 15 ns) for the size. Per isotope change: the cloud is built once (key check): measured 0.4–0.56 ms for A > 16 (the starting-radius solver; 2.0 ms before I cut its steps), rule 0.04–0.15 ms. **Nucleus scenes (benchmark scene 18 and the other nucleus scenes) are on the owner's re-measure list** (OWNER-TODO), because the drawn size changed per frame.

## Not compiled, not seen
`atom_panel.cpp` (the new `nuclearSizeCard`) and `atom_view.cpp` (one changed line, one include) are not built here: the card function compiled against stand-ins (`-Wall -Wextra`, clean). Never seen: the card's layout and wrapping, the glyphs (± × √ ²), the nucleus at its new sizes, overlap on screen. No Windows behaviour or speed tested here.

## Windows CI result
Run 37206334083 on commit bc374ae (Native Windows build, windows-latest, Visual Studio 64-bit): **success**. Configure, Build (the whole app: `atom_panel.cpp` with the new card and `atom_view.cpp` with the changed line **compiled on MSVC**, which the cloud could not do) and "Physics unit tests" (all seven programs) passed, so the new radii, layout and text tests hold under MSVC. I did not read the printed counts from the log, so "18,578 on Windows" is not claimed; the step's success is. Compiling is not seeing: the card and the nucleus at its new sizes have never been looked at on a screen (OWNER-TODO).
