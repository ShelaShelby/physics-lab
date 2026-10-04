# Native Physics Lab 0.10.9 (alpha) — stage 2 report: the nucleus's shape from measured quadrupole moments

Card **"Nuclear size and shape (measured)"** (stage 1's card, extended) and the readout line "shape: …" now name where a nucleus's shape comes from. Permanent, no setting. `index.html` untouched (sha256 e5501387ae64…). No helper agents. The deferred optimisation items stayed deferred. The stage 1 size work stands. Halo nuclei are still blocked on data.

## The data
- **URL** `https://www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv`, plain GET by `tools/make_quadrupole_data.py fetch` on **2026-10-04**: HTTP 200, **135,013 bytes**, 1,156 lines (header + 1,155 rows), Last-Modified 13 Feb 2026, **sha256 e5db6757…0c25** (same bytes as my first curl). Table `src/atom/quadrupole_data.inc` (50 KB, 598 rows). README: `data/quadrupole/README.txt`.
- **Refused:** the directory page and all IAEA services (bot challenge, 403); nds.iaea.org and the stage 1 hosts (policy 403). **The page that states how to cite the table could not be read**, so the credit is only what the file gives: every row names INDC report `indc-nds-0833` and an NSR key (title and author of the report not read). OWNER-TODO asks for the citation.
- **The second column ("n.n+n.z") is the MASS NUMBER A.** Established against the lab's NUBASE-derived nuclide table (the tool repeats it on every build): read as (Z, A−Z) **599 of 599** ground rows are known nuclides, read as a neutron count only **53 of 599**; of the 77 "stable" rows **76** are stable isotopes as A (the 77th, Eu-151, is in the table at 1.45e26 s), **0** as N; symbols match Z in all rows; rows Z=1/2 = deuteron (spin 1+, Q +0.0029 b), Li 6/7/8 = Li-6/7/8, holmium 152…165.
- **Isomers:** 556 rows with energy ≠ 0 are not stored (the lab draws ground-state nuclei; an isomer's moment says nothing about the ground state); counted in the table header. 599 ground rows; **one defect handled by name:** K-41 has a second "ground" row at 181 ms (K-41 is stable): left out → 598.
- **Cells, in barns (1 b = 100 fm²):** 546 signed, **41 unsigned (no sign in the file)**, 4 tentative sign "(−)", 7 not one clean number (Ga-74 "x or y", Pa-231 "[…]" not measured, and five typing errors such as `+0.70)15)`; At-218's row has a broken quotation). Nothing repaired (a repair is a guess). No ground row is even-even or spin 0/½ (as quantum mechanics requires).

## The conversion (a model assumption, said in the code, About and the card)
Spectroscopic Q → intrinsic Q0, strong coupling K = I: **Q0 = Q (I+1)(2I+3) / (I(2I−1))** (Q0 = 10, 5, 7/2, 14/5, 15/7, 11/6 × Q for I = 1 … 9/2; Bohr–Mottelson / Krane, **written from knowledge, not looked up**). Poor for light nuclei and near closed shells. Then β₂ from a uniformly charged body r = R(1+β₂Y₂₀) (assumption): ⟨3z²−r²⟩/⟨r²⟩ = N5/G5, solved exactly (first order: Q0 = 3ZR²β₂/√(5π); exact is 4–9 % lower at 0.2–0.45), with ⟨r²⟩ = measured rms² − 0.84² (the nucleon's own size adds nothing to a quadrupole). Ho-165: Q +3.58 b → Q0 +7.671 b → **β₂ +0.325 prolate**. **The lab's own limit |β₂| ≤ 0.6** (no source; OWNER-TODO): beyond it the moment is shown and not used.

## How many isotopes use which source (slider: 4,252 isotopes)
**498 measured moment**, **11 B(E2)**, **3,743 estimate** (1,057 of them even-even, spin 0). Of the 598 table rows: 512 qualify (signed, firm spin ≥ 1), 498 used, **14 need |β₂| > 0.6** (Li-7, Li-9, Be-9, B-8/10/11, Na-28, Mg-29, S-33/35, K-39/40/41, Cs-124); not used for a shape: 41 unsigned + 4 tentative sign, 34 tentative spin, 7 not clean. The B(E2) table (11 isotopes) has no named source in the code or the web page: reported, not changed.

## Spin 0 and ½
No row for them, and the card says in one line: even Z and N = spin 0, "no spectroscopic quadrupole moment whatever its shape, so this table cannot say whether it is round or deformed. Round is not implied"; H-1: spin ½ likewise; an odd isotope without a row: "if its spin is 0 or 1/2 …; the lab has no spin table, so it cannot say which applies".

## Most deformed cases (β₂ from the data, accepted)
Rare earths/actinides 0.26–0.35 prolate (Ho-165 +0.325, Er-167 +0.312, Eu-153 +0.315, Eu-159 +0.348, Gd-155 +0.290, U-235 +0.258). Larger values are odd–odd or light, where K = I is doubtful: Tb-160 +0.43, Sb-124 +0.48, Rb-77/80 +0.50, Mg-33 +0.50, Rb-75 +0.58, Li-8 +0.596. Oblate: Cs-126 −0.579, Pa-233 −0.563, Fr-226 −0.523, Cu-58 −0.459. Bi-209 −0.031, Sm-147 −0.030 (nearly round). No oblate benchmark was read; "known oblate" is the data's own.

## What moved
- **Saved web references: none moved** (web layout kept as `buildNucleusByRule`; the whole-lab runs pass unchanged).
- **Layout:** every deformed nucleus (3,880 of 4,252 isotopes have β₂ ≠ 0, most from the estimate) now keeps its volume (surface R_V·(1+β₂Y₂₀)·F^(−1/3)), and for a measured size or moment the protons' quadrupole is exact. The web form kept volume out of it. The stage 1 test "unmeasured isotopes equal the rule layout" had passed for Og-294 only by luck (seed 9); **changed on purpose**: identical for β₂ = 0, different for all 4 seeds for Og-294.
- **Readout "shape:"** names source: Pb-208 and 12 other doubly magic nuclei were "measured" (no suffix), are now "(an estimate)" (round by shell-model expectation, not a measurement).

## Tests (written first, Linux GCC Release): orbit 1,540; **atom 18,845 (+267)**; **app 1,520 (+1)**; nbody 37; jpl 55; moon 24; moon_real 59 — all pass
Typed from the CSV (counted separately): 1,155/599/598/556, kinds, 102 per-element counts, 22 spot rows incl. method/NSR, the second-column test. Hand arithmetic: six K = I factors, Ho-165 Q0, the first-order β₂. Exact β₂ against a separate Python (Simpson over the angle) for 12 nuclei (to 3e-6). **Drawn body by the test's own integrals** for 10 values of β₂ (±0.6): volume error 6.9e-15, mean-square radius 8.9e-15, Q0 of Ho-165 reproduced (1e-6). **Cloud, untilted, 12 nuclei (4 through the Gaussian branch): protons' ⟨3z²−r²⟩/⟨r²⟩ = Q0/(Z⟨r²⟩) of the data to 4.9e-13 (guard 1e-9), trace 6.7e-16, ⟨x²⟩=⟨y²⟩ 4.8e-16**; stage 1 radius kept for Ho-165, Eu-153, U-235 (1e-9). Closest pair in the deformed clouds 0.6125 fm (Li-8), guard 0.606 (measured −1 %). Tilted snapshots: sign only (for Z ≥ 13; a few-proton snapshot's largest eigenvalue has sd 0.3–0.5). Readout lines and card words. No `long double`, no byte hashes.

## Perturbations (each reverted, file compared byte for byte)
Sign flipped 40 failures; spectroscopic as intrinsic 64; volume not kept 19; barns as fm² 58; full rms instead of point-proton 34; unsigned used 4; |β₂| limit removed 3; layout not scaled to the moment 2; old layout kept 2; K = 0 54; column read as N: **test program crashed** (segfault after the first failure). **Tentative spin used: first try 0 failures, a weak perturbation** (tentative spins are excluded twice, the tool stores them as −1); with the spin check removed entirely 3 failures. All caught.

## Speed (in principle; no per-frame work added)
The shape is solved once per isotope change (60 bisections of a closed form, about a microsecond); the readout line asks it again only when the Nucleus-layer text is built. The cloud is built once per isotope as before. No new per-frame cost; the nucleus scenes stay on the re-measure list from stage 1.

## Not compiled, not seen
`atom_panel.cpp` (the card's extra lines) is not built here: compiled against stand-ins (`-Wall -Wextra`, clean). Never seen: the card's layout and wrapping, the glyphs (β₂ Q₀ ₂₀ × ± ²), the nuclei on screen, overlap. No Windows behaviour or speed tested here.

## Windows CI result
(pending: filled in after the push)
