# Native Physics Lab 0.10.6 (alpha) — stage 2 report: Atom Lab constants from CODATA 2022

Fixes stage 1 findings 4, 9 and 18 and the "Known item" (the Bohr radius). Stage 1's report is in git history (`git show 90049fe:native/REPORT.md`). Only the Atom Lab changed; no Orbit Lab code; `index.html` untouched (sha256 e5501387ae64…).

## NIST pages: reached, read in this session (2026-10-04)
Plain curl worked, **all pages answered 200**, and each says "Source: **2022 CODATA recommended values**" (`https://physics.nist.gov/cgi-bin/cuu/Value?<key>`): bohrrada0, hrev, aut, alph, rydhcev, mec2mev, meu, mpsme, ukg, muc2mev, me, mp, mn, mpu, mnu, k, h, c, e, hev, rp. So step 1b (second-hand values) was **not used**; every value in the code was read here. Not usable: `mu` is the *molar* mass constant (not the atomic mass constant), `hcevnm` and `hcev` do not exist: h·c is **derived** from the exact h, c and e (1239.841 984 332).
Not from NIST, not re-read, left as they were: the hydrogen-1 **atom** mass 1.00782503223 u (AME2020, `atom_learn.cpp`, comment says so) and the proton radius **0.84 fm** (web's rounded value kept; CODATA 2022 `rp` = 0.84075(64) fm, in the comment).

## Every constant: old → new (relative change)
| constant | old (web / typed) | new (CODATA 2022) | change |
|---|---|---|---|
| Bohr radius a0 | 0.529e-10 m | 5.29177210544e-11 m | **+3.35e-4** |
| Hartree energy | 27.211 eV | 27.211386245981 eV | +1.4e-5 |
| atomic unit of time | 24.19 as | 24.188843265864 as | −4.8e-5 |
| atomic unit of speed a0/τ | 2.186854e6 m/s | 2.187691262e6 m/s (= αc to 5.6e-12) | **+3.83e-4** |
| fm per a0 / metres per fm | 52917.7 / 9.99666e-16 | 52917.7210544 (derived) / 1e-15 exact | +4.0e-7 / **+3.35e-4** |
| α | 1/137.036 | 7.2973525643e-3 | +6.0e-9 |
| Rydberg energy | 13.605693123 eV | 13.605693122990 eV | −7.4e-13 |
| h·c | 1239.841984 eV nm | 1239.841984332 (derived) | +2.7e-10 |
| u / m_e; m_p / m_e | 1822.888486; 1836.15267 | 1822.888486278 (1/meu); 1836.152673426 | +1.5e-10; +1.9e-9 |
| m_e c² | 510998.95 eV | 510998.95069 eV | +1.4e-9 |
| u, m_p, m_n, m_e (kg) | 1.66053907, 1.67262192, 1.67492750e-27, 9.1093837e-31 | …892, …92595, …50056e-27, 9.1093837139e-31 | −6.5e-10, +3.6e-9, +3.3e-10, +1.5e-9 |
| u (MeV); m_p, m_n, m_e (u) | 931.49410242; 1.007276467, 1.008664916, 5.48579909e-4 | 931.49410372; 1.0072764665789, 1.00866491606, 5.485799090441e-4 | +1.4e-9; −4.2e-10, +6e-11, +8e-11 |
| u in the temperature text; Learn a0 | 1.66054e-27; 5.29177e-11 | the new u; kA0 | −5.6e-7; +4.0e-7 |
| k_B, h, c, e | exact SI | same (now named, with their NIST pages) | 0 |
All are in `atom_core.h`, one block, each with the page key, the year and the date; derived ones (kAuSpeed, kAuTimeAs, kA0Ang, kFmPerA0, kFmLab, kPlanckEVs, kHcEVnm, kMeU, kHartree) are worked out, not typed again. The old comment block about the web's two-number fm conversion is gone. Moved into the block: 510998.95 and 137.036 (`atom_sim.cpp`), the masses and 931.49 (`atom_learn.cpp`), k_B and u (temperature text), the 5.29177e-11 of the guides, 1836.15267 (`atom_core.cpp`).

## What moved in the tests (all rescaled by the constant ratio, none loosened)
Before: 782 of 17,936 atom checks failed. All of them were one of:
- **Orbit-run numbers** (x, y, vx, vy, first dots; 212 checks): web value × a0 ratio (1.000335) for lengths, × (a0/τ) ratio (1.000383) for speeds, same 1e-9 / 1e-6 limits. They match exactly: the motion in atomic units is unchanged.
- **Spectra** (5 `rydM`, 525 lines): web value × the ratio the new Rydberg energy, h·c, u/m_e and m_p/m_e give (≤ 2.7e-10; Hα moves 1.8e-7 nm), same 1e-14 / 1e-13 limits. The NIST values are typed again in the test (`nist::` block), not taken from the code.
- **6 dot hashes** (positions are stored in metres): re-recorded. Checked first, with the 0.10.5 code built in a scratch folder: all 288,000 stored values agree in Bohr radii to **1.2e-7** (float rounding); 3 old dots per run are now also in the test, compared to 1e-6.
- **Learn "weight" texts** (34): only the 16-digit CSS bar width (`width:99.94556794247602%`) moved, by ~1e-10. Now compared as: all other characters identical, each width within 1e-8 relative (`checkTextWidths`).
- **6 readout lines** (the existing last-digit "tie" rule took them): e.g. 3dxy "theory 5.55 Å" → 5.56 (10.5 a0 = 5.5545 → 5.5564 Å); sampled 3.68 → 3.69; U⁹¹⁺ "r 0.57 pm" → 0.58. All up by the +0.033 % in a0: real changes, not noise.
Did **not** move: orbitals (psi), Laguerre, dipole table, outlines, beats, complex states, phase wheel, nuclei (compared in fm), hints, the other readouts and Learn texts, About-independent texts.
**Tightened:** `testOrbitGlide` compared the lab's circle with CODATA's to 4e-4; now 1e-12. Comments "CODATA 2018 (written from knowledge)" replaced by the real source (NIST pages, 2022).
**New** (32 checks, `testConstants`): all 21 constants against the NIST values typed in the test (1e-14); relations independent of how each was typed: a0/τ = αc (1e-10, measured 5.6e-12), Hartree = 2 Rydberg (1e-11), Hartree = α²m_ec², a0 = ħ/(m_e c α), u/m_e = uc²/m_ec², m_p/m_e from kg and from u (all 1e-9).

## About page (`app_text.inc` and its edit list in `test_app.cpp`, same words)
The two "CODATA 2018 … written from knowledge" lines now say CODATA 2022, read from NIST on 2026-10-04; the "0.529 Å is 0.03 % smaller" line is replaced by one entry naming the NIST pages and every constant, saying what changed and what was not from NIST (H-1 atom mass, proton radius). Edit count in `test_app` unchanged (14).

## Tests: all seven pass (Linux GCC Release, Ninja scratch build in /tmp/claude-0/build, `PL_BUILD_APP=OFF`)
orbit 1,540; **atom 17,974** (was 17,936); app 1,385; nbody 37; jpl 55; moon 24; moon_real 59.

## Not done / not tested
Windows, speed, anything on screen (the Bohr circle is 0.03 % bigger: not looked at; OWNER-TODO has a check). Other stage 1 findings (1-3, 5-8, 10-17) are untouched: finding 3 (hydrogen shows −13.61 eV from ½·Hartree·Z², not the reduced-mass level) and 5 (A·u − Z mass) still stand. Hydrogen-1 atom mass and proton radius not re-read from a source. `ATOM_PLAN.md` still describes the web's rounded constants (history, not edited).
