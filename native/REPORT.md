# Native Physics Lab 0.10.8 (alpha) — stage 3 report: real lifetimes and selection rules, from NIST ASD

New card **"Lifetimes and jumps (NIST)"** in the Atom Lab, under "Real spectrum (NIST)". `index.html` untouched (sha256 e5501387ae64…). No helper agents. The three deferred optimisation items stayed deferred.

## Was Aki already in the data? Yes — no refetch
`data/asd/asd_lines.tsv` (retrieved 2026-10-04, query in its README: lines1.pl, vacuum, allowed_out=1, forbid_out=1 …) already had the columns **Aki(s^-1), Acc and Type**; the compiled table had dropped them. So no new request was made. `tools/make_asd_data.py build` now writes three more columns (`aki`, `acc`, `type`) into `asd_data.inc`; `AsdLine` gained the fields. Of 5,029 ion lines 4,925 have an A; accuracy AA 4,762, AAA 159, none 108 (4 of these have an A and no rating: the app says "no rating given"); types E1 5,011, M1 11, E2 5, UT 1, M1+E2 1 (counted by the test). ASD's help page (`lineshelp.html`, read 2026-10-04) gave the accuracy table (AAA ≤ 0.3 % … E > 50 %, a prime = a split multiplet), the Type meaning (blank = E1) and ASD's own rule (opposite parity, J changes by at most 1); all quoted in the data README and About (the ASD entry was extended, not duplicated).

## What the card does
- **Lifetime** of the state on screen (every j of its shell, since the lab's orbitals have no j): τ = 1 / ΣA over every allowed jump to a lower n, shown only when ASD has an A for **every** such way ("complete"); unit chosen automatically (as … s). Partial set: only "at most …" (upper limit). The explanation is in plain words on the card.
- **Check a jump** (two rows of shell buttons): allowed → A, ASD's accuracy meaning, branching share = A/ΣA; not allowed → every rule that fails, in words (parity, l, j) and, where ASD lists it as forbidden (M1, E2 …), ASD's rate under that type, labelled a different process, not a lifetime.
- No A from ASD → "allowed (electric dipole), but NIST ASD has no transition probability for this jump, so no rate or share is given." No ASD ion (Z = 111–118) → "NIST ASD has no data for …". Nothing is calculated; no hydrogenic rate was implemented.
- Jumps between levels of one n are not in the data (centimetre waves) and are not counted; the card says so.

## Numbers (ASD, as read; accuracy in brackets)
H 2p 3/2: A = 6.2647×10⁸ s⁻¹ (AAA) → **1.596 ns**; 2p 1/2: 6.2648×10⁸ (AAA). 3p 3/2: two ways, 88.2 % to 1s and 11.8 % to 2s (AAA) → 5.272 ns. 3d 3/2: 83.3 % to 2p 1/2, 16.7 % to 2p 3/2 → 15.47 ns. 3s: 33.3 % / 66.7 % → 158.4 ns. He II 2p 3/2: 1.0029×10¹⁰ (AA) → 99.71 ps. Fe XXVI 2p 3/2: 2.8370×10¹⁴ (AA) → 3.525 fs.
**Coverage (ways down with no ASD A, 110 ions):** upper n = 2: 0 of 220; n = 3: 51 of 990; n = 4: 1,583 of 2,420; n = 5: 2,960 of 4,510; n = 6: 6,036 of 7,260. So the card often says "no value" for n = 4 states (lab orbitals reach n = 4), and for 920 allowed jumps to 1s within n ≤ 6.

## Metastable 2s
2s 1/2 has **no allowed way down** (l and parity unchanged), so no lifetime number appears, for any of the 110 ions (tested: 220 states, the 1s and 2s of each, none has a number). The card says a real 2s state still decays by emitting two photons, which ASD does not list and the lab does not model (textbook physics, no number given). I did **not** use the 0.12 s of the job text: it is not in ASD and was not checked. ASD does list the one-photon M1 jump 2s → 1s for hydrogen (A = 2.495×10⁻⁶ s⁻¹, AAA); the jump check shows it as "forbidden type M1, a different process, not counted".

## Tests (written first; Linux GCC Release) — atom 18,418 (was 18,382: +36), app 1,518 (+1)
- Typed from the TSV: A of Lyα 2p 3/2 and 1/2 (6.2647×10⁸ / 6.2648×10⁸, AAA), He II, the M1/E2 lines, 3p/3d/3s rates; the lab's τ equals 1/ΣA worked out from them to 1e-12 (H 2p: 1.5962 ns).
- **Selection rules vs ASD, all 110 ions** (extends stage 1's 90 of 90): of 4,771 ASD lines with blank Type, the lab allows all but **one**; of the 16 forbidden lines (E2, M1, UT) it allows none; ASD's wording (parity + J) and the lab's rule (l + j) agree on all 505 pairs of levels n ≤ 6; `jumpsBetween` agrees with ASD's Type for every line; reasons name the right rule (2s→1s: parity and l, not j; 3d 5/2→2p 1/2: only j).
- **The one exception, found by the test:** ASD lists **1s–2s of Cs LV (Z = 55) with a blank Type (= allowed) and A = 7.591×10¹¹ s⁻¹**, yet l does not change; the same line is M1 for Z = 56 and UT for Z = 38. The lab follows the rule, says so on screen and does not use the value; the test names it; data README and OWNER-TODO record it. ASD's data file was not changed.
- **Branching ratios:** of 3,850 states (n = 2…6, every l, j, 110 ions) ASD has the complete set for **1,531**: hydrogen 34 of 35 (all but 2s, which has no way down), He II 34, Fe XXVI 26. For all 1,531 the shares sum to 1 (worst 2.2e-16) and τΣA = 1 (1.1e-16); the lab's complete/ways/withA agree with the test's separate scan for every state.
- **A ∝ Z⁴ from ASD's own numbers** (2p 3/2→1s, 2p 1/2→1s, 3d 5/2→2p 3/2; 109, 109, 104 ions): for Z ≤ 10, A/(Z⁴A(H)) is within 6.5×10⁻⁴, 1.2×10⁻³, 8.7×10⁻⁴ of 1; for Z ≥ 10, |r−1|/(Zα)² ≤ 0.361, 0.231, 0.145 (r falls to 0.77 at Z = 110 for 2p 3/2: a relativistic correction). Guards = measured + 1 %, labelled in the test.
- Words: units (ps/ns/µs/ms/s), the explanation, the accuracy-table text, the state and jump lines, the no-A and Cs LV messages. App test: the extended About entry, the settings keys. No `long double`, no hashes.
- Also passed: the atom tests built with `-O2 -mfma -ffp-contract=fast` (fused multiply-add), 18,418 checks.
- **Nothing existing changed:** stage 2 and stage 3 built apart and compared with the Dirac setting off (2,422 lines, 1.6 MB) and on (197,842 lines): **identical byte for byte**.

## Perturbations (each reverted, file compared byte for byte)
A short by 1 % → 12 failed; τ = 1/(2ΣA) → 10; ways include the same n → 15; rule loosened (l unchanged allowed) → 4; ASD line looked up with levels swapped → 12; accuracy limit AAA 0.3→0.5 % → 1; share with the wrong denominator → 1.

## Not compiled, not seen
`atom_panel.cpp` (`lifetimeCard`, its call) is not built here: the card compiled against stand-ins (`-Wall -Wextra`, clean). **Never compiled with ImGui, never seen:** layout, theme, text sizes, the two ten-button rows, long lines at large text, glyphs (→ − ⁻¹ ×10ⁿ µ). OWNER-TODO has the checks. The two "Check a jump" choices are not saved (UI only).

## Windows CI result
Pending at the time of writing; recorded below after the run.
