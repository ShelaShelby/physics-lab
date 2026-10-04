# Native Physics Lab 0.10.8 (alpha) — stage 2 report: the Dirac formula's precision (fix of the red stage 1 build)

`index.html` untouched (sha256 e5501387ae64…). No helper agents. Only the Atom Lab's Dirac code and its tests changed; no text the app shows changed.

## What I established about the cancellation (checked, not assumed)
- Python `decimal`, 60 digits, with the same double inputs as the lab: exact mc²(√(1−α²)−1) = −13.605874258016532628…; the same in plain double = −13.605874257982812, **relative error 2.48e-12**; written without the subtraction, −mc²α²/(1+√(1−α²)) = −13.605874258016533 (error 1.5e-18, i.e. correct to the last bit). √(1−α²) = 0.99997337…, so 1 − it = 2.66e-5 and the subtraction throws away 11 of 16 digits (ulp(1) / 2.66e-5 = 8.3e-12).
- **So the lab's value (−13.605874258016534) was the accurate one and the test's hand-made reference was the inaccurate one.** Stage 1's test used `long double` (80 bits on Linux, 64 on MSVC).
- Reproduced before fixing: a copy of the stage 1 test with every `long double` and `…L` literal made `double` failed **exactly the six checks of the Windows run, with the CI's numbers** (got −13.605874258016534 / want −13.605874257982812; got 121.56642146623935 / want …640098). No tolerance was loosened anywhere.

## The fix
1. **Test references** (`test_atom.cpp`): double-double arithmetic (two-sum, error-free product via `std::fma`, Newton division and sqrt; ~32 digits) of the plain textbook formula, so its subtractions are harmless. The double-double code itself is pinned by 80-digit values typed from Python `decimal` (10 levels, Z = 1 and 92; E, correction, δ). Measured accuracy: 3.6e-20 at worst (the correction at Z = 1, n = 6, where it is 4e-7 of the energy it is the difference of; the energy is good to 1.3e-26) — four orders finer than the lab's 1e-15, so it is a valid reference. A test comment says: never use `long double` or any extended precision in a reference. Also rewritten well-conditioned: stage 0.10.7's `diracE` helper and `testRelativityText`'s percentage (guards untouched). `grep` finds `long double` only in comments.
2. **The lab's own code** (`dirac.cpp`, `atom_sim.cpp`): stage 1's energy (expm1/log1p) and δ (as a quotient) were **already** cancellation-free, so the user's two named places needed no rescue in substance; the energy now uses −t/(s(1+s)) (only +, ×, ÷, sqrt, which are correctly rounded on every platform). Real cancellation found and fixed: **(a)** the relativistic correction c = E_Dirac − E_NR (a difference of two energies, 13.6 eV vs 1.8e-4 eV for H) — now worked out directly: c/mc² = (s−1)²(s+2)/(2s) − (Zα)²δ(2n−δ)/(2n²k²), s−1 = t/(s+1), no subtraction of near-equals; **(b)** the readout's percentage `100((1−√)/(½x²) −1)` → 100x²/(1+√)² and 1s energy `mc²(1−√)` → mc²x²/(1+√); **(c)** the level-gap rows now use differences of corrections, not of two levels. Looked at and left: spectrum lines R(1/lo²−1/hi²)+Δc (benign: exact rational, Δc now accurate), the misses (calculated − measured: that difference is the physics), nuclear mass (no near-equal terms). Not examined: `sqrt(1−e²)` in the Orbit Lab (eccentricity; not this code).

## Digits gained (worst relative error over n = 1…6, every j, against the double-double reference; measured in the test)
| | Z = 1 old → now | digits | Z = 92 old → now | digits |
|---|---|---|---|---|
| δ (naive → quotient) | 6.0e-11 → 7.0e-17 | 5.9 | 9.6e-15 → 1.8e-16 | 1.7 |
| energy (naive → now; stage 1 was 2.5e-16) | 1.45e-10 → 4.6e-16 | 5.5 | 1.46e-14 → 4.2e-16 | 1.5 |
| correction (stage 1 → now) | 2.07e-10 → 1.3e-15 | 5.2 | 3.7e-14 → 1.0e-15 | 1.5 |
Readout, Z = 1…118: percentage 1.9e-7 → 3.0e-16 (relative to a value of 0.0027 % for H), 1s energy 2.5e-12 → 2.8e-16.

## Did any app-visible value change? (a correction of arithmetic, not of physics)
Stage 1 and stage 2 dumped side by side (118 elements × 2 isotopes: lines, notes, level rows, the Dirac note, the NIST comparison rows, and info()/hint() for 5 elements × 4 views, Dirac on: 197,843 lines): **every displayed text is identical, 0 differences.** Underlying numbers moved only at rounding level: Dirac wavelengths ≤ 2.1e-15 relative, levels ≤ 7.9e-16, the tiny misses (~1e-6 of the wavelength) ≤ 4.7e-10 of themselves, hydrogen's 2p 3/2 − 2p 1/2 gap (a difference of two levels) 9.8e-12 (exact 4.5284106339798e-5 eV; the card now shows it from corrections, to 1e-16).

## Stage 1 conclusions re-derived (the guards of stage 1 are unchanged and all still pass)
Re-run, not assumed: improvement against ASD H I 2.643e-5 → 3.317e-6 (7.97×), He II 1.057e-4 → 1.093e-5 (9.67×), Fe XXVI 1.824e-2 → 5.72e-4 (31.9×); Lamb-side share (ASD − Dirac)/(NR − Dirac) 0.02203 … 0.27164 over 220 components, order NR > ASD > Dirac in all; selection rule vs ASD 90 of 90 for H I and He II; NIST gap 0.1772 % above Dirac; reduced-mass difference 9.24e-8 eV (9.1e-9 of Lyα). All equal stage 1's at the printed digits; **none moved** (the 2.5e-12 error was far below every tolerance, as expected).

## Nothing changes with the setting OFF (proved again)
Stage 1 vs stage 2 built separately, one program with the old API (specLines, visibleLines, compareIon, levels, notes, info(), hint(), first 3,000 cloud numbers): **2,422 lines, 1.6 MB, identical byte for byte.** (The readout's percentage/energy are Off-visible: their doubles moved by ≤ 1e-12 but the text shown is the same for all 118.)

## Tests (Linux GCC Release)
atom **18,382** (stage 1: 18,373; +9: double-double, typed 80-digit values, the digit table, the readout); app 1,517; orbit 1,540; nbody 37; jpl 55; moon 24; moon_real 59. All seven pass. Also passed: the atom tests built with `-O0 -ffp-contract=off`, with `-O2 -mfma -ffp-contract=fast` (fused multiply-add, as other compilers do) and with `-O3` — numbers identical. Guards: rounding-level ones in units of the last digit (2–4× measured), the old forms' errors measured − 1 % (deterministic IEEE), stated beside each. Tightened (never loosened): lab vs reference 1e-12 → 2e-15, compareIonDirac 1e-11 → 1e-14.

## Perturbations (each reverted; `dirac.cpp` compared byte for byte)
Correction's second term added instead of subtracted → 13 failed; (1+s) replaced by 2s in the energy → 8; sqrt added inside δ → 13; **t perturbed by 1e-12 relative → 4**; j off by one step → 15.

## Not tested
MSVC itself (the Windows run is the test; its `long double` was simulated as `double`). `atom_panel.cpp`/`settings.cpp` untouched this stage.

## Windows CI result
Pending at the time of writing; recorded below after the run.
