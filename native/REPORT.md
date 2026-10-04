# Native Physics Lab 0.10.8 (alpha) — stage 1 report: relativity as a real option (Dirac levels)

New setting "Relativity (Dirac levels)" in the Atom Lab, under "Light spectrum", **off by default**. `index.html` untouched (sha256 e5501387ae64…). The three deferred optimisation items stayed deferred. No helper agents.

## What it does
- On: the **Light spectrum** and **Real spectrum (NIST)** cards use Dirac levels (one electron, point nucleus). A level depends on n and j, so 2p splits into 2p 1/2 and 2p 3/2 and every line splits (more as Z grows). The strip, the visible-line marks and the real-spectrum card draw ONE list of lines (`specLinesFor`), so the card compares exactly what is drawn. The card compares each NIST component with the lab line of the same n and j (not the nearest line) and shows the miss with and without Dirac. A short list gives the split levels (n = 1–3, gap in µeV/GHz).
- **Does not change, and the app says so (one-line explanation, long note when on, spectrum note):** clouds, orbitals, outlines and radial graph (non-relativistic wave functions); the energy readout, the photon of a jump (and its ▼ mark) and the quantum beats (the lab's orbitals have no spin, so no j to give them). No Lamb shift, other QED or nuclear size.
- **Selection rules:** the spectrum enforces l changes by 1 and j by 0 or 1 (`diracAllowed`). **Not changed: `targets()`** (which orbitals a jump may reach): it enforces l ± 1 and the orientation rule, which is already the l part of the Dirac rule; the j part needs a j the orbital picture does not have, so a 2p → 1s jump cannot say which component it is. Left as is, said in the app and OWNER-TODO (a question for the owner).

## The formula and its source
E(n,j) = mc²[(1 + (Zα)²/(n − δ)²)^(−1/2) − 1], δ = (j+½) − √((j+½)² − (Zα)²): the Sommerfeld fine-structure formula, the exact Dirac level for a point nucleus (comment in `dirac.h`; About entry added: Bethe & Salpeter, already listed, and Griffiths for the expansion). **Written from knowledge, not looked up** (only physics.nist.gov was reachable). The code uses expm1/log1p and δ = (Zα)²/(…) so the small correction keeps its digits. Checks are below; the book was not re-read (OWNER-TODO).

## Reduced mass (not quietly reused)
Done: the lab's own reduced-mass level (`levelEV`) **plus the relativistic correction of an infinitely heavy nucleus**. Not done: m → μ in the whole Dirac formula (that scales the correction by μ/m too, which is only a leading-order recoil term), and the full recoil terms beyond the reduced mass (no source could be read; their size is not known to me). For hydrogen Lyα (2p 3/2): the two treatments differ by 9.2e-8 eV = **9.1e-9 of the photon energy** (10.1989 eV), which is 5.4e-4 of the Dirac correction itself (1.70e-4 eV). The app says: reduced mass in the main energy, infinitely heavy nucleus in the correction, further recoil not included.

## Against NIST ASD (worst of all n ≤ 6 components, matched by n and j; relative to the wavelength)
| | components | without Dirac | with Dirac | closer by |
|---|---|---|---|---|
| H I | 140 | 2.643e-5 | 3.317e-6 | 7.97× |
| He II | 140 | 1.057e-4 | 1.093e-5 | 9.67× |
| Fe XXVI | 91 | 1.824e-2 | 5.723e-4 | 31.9× |

Fe XXVI Lyα: lab off 0.179738854 nm; Dirac 0.177701150 (2p 3/2), 0.178241973 (2p 1/2); NIST Ritz 0.177801471, 0.178344041.
- **Rule against NIST data:** for H I and He II the set of (n, j, n′, j′) lines the rule gives for n ≤ 6 equals ASD's electric-dipole set exactly (90 of 90); for all 110 ions every ASD E1 component is allowed by the rule. Lines of equal energy are one line (Hα: 5 lines, ASD has 7 components).
- NIST's own 2p 3/2 − 2p 1/2 gap for hydrogen (from its two Lyα Ritz wavelengths) is 10.96905 GHz; Dirac gives 10.94965 GHz (m c² α⁴/32 = 10.9493 GHz): NIST is 0.1772 % larger. That part is not in Dirac theory; its cause was not checked against a source.

## The leftover (shown, not hidden)
Lyα of all 110 ions (220 components): non-relativistic lab > NIST > Dirac in wavelength every time; NIST sits 0.022 to 0.272 of the way from Dirac to the old line. Size of the Dirac line's miss: 3.3e-6 (H), 1.1e-5 (He II), 5.7e-4 (Fe XXVI) of the wavelength. This is the Lamb shift (which lifts 1s), other QED and the nuclear size; **I did not separate them or look up their sizes**. The Real spectrum note says so on screen.

## Nothing changes with the setting off (proved)
The previous commit was built in a separate tree. One program using only the old API dumped specLines, visibleLines, compareIon, levels, spectrum notes (118 elements × 2 isotopes), info() and hint() and the first 3,000 cloud numbers (5 elements × 4 views): **2,442 lines, 1.6 MB, identical byte for byte** old vs new. In the tests: the lists and notes compared in the same run (no stored hashes), and a lab told "Off" by hand equals one never told. The settings file has no "dirac" line when off. The existing counts and every existing test and saved reference are unchanged; none was loosened.

## Tests (first written from independent references, then the code; Linux GCC Release)
atom 18,373 (was 18,326: +47); app 1,517 (+2; About edit list 17 → 18); orbit 1,540; nbody 37; jpl 55; moon 24; moon_real 59. All seven pass.
- Hand values from CODATA 2022 (H 1s = mc²(√(1−α²)−1), Fe 1s likewise, both at 1e-12); textbook arrangement in long double for Z up to 118, n ≤ 6, every j: worst 7.9e-14. The expansion to (Zα)⁴ misses by 0.0078 (Zα)⁴. 2p 3/2 − 2p 1/2 for H = m c² α⁴/32 within α² (measured 0.62 α²). The Dirac − non-relativistic difference scales as (Zα)² for Z = 1–20 (next term 1/2 (Zα)²); the reduced mass is a separate 5.4e-4 factor, checked apart.
- Guards are measured + 1 % (improvement factors: − 1 %), written next to each test. Two guards (1e-12, 1e-11) are rounding-level, ≥ 10× the measured 7.9e-14 / 1.1e-13, as in the radial-graph tests.
- **Mutations (each reverted, file compared byte for byte):** j off by one step → 11 failed; sign of the correction flipped → 7; j rule loosened to Δj ≤ 2 → 1; δ added instead of subtracted → 13; upper level's j taken from the lower → 2.

## Found, not changed
- `compareIon` (0.10.7, Off) counts forbidden M1/E2 ASD lines as components for H I (e.g. 1s–2s at 121.5673 nm); left, since Off must stay identical. The Dirac comparison uses E1 lines only.
- `tools/native_edits.js` lists 3 About edits but `test_app.cpp` and `app_text.inc` have 18: running `make_app_text.js` would drop the others. My About entry went into `app_text.inc` and the test list, as in earlier stages.

## Not compiled, not seen, not tested
- `atom_panel.cpp` and `settings.cpp` are not built here. Compiled against stand-ins (`-Wall -Wextra`, clean): `spectrumPicture`, `realStrip`, `realTicks`, `realSpectrumCard` and the new settings block. **Not compiled:** the `settings.cpp` lines (three short edits, read by eye). **Never seen:** layout, theme colours, text sizes, the long notes at large text, glyphs (−, →, ▼). OWNER-TODO has the checks.
- Not measured: speed (the Dirac list is rebuilt each frame while on, ~325 level energies; cheap, not timed). Not tested: Windows behaviour beyond CI.

## Windows CI result
Pending at the time of writing; recorded below after the run.
