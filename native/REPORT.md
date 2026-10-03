# Native Physics Lab 0.10.5 (alpha) — stage 4 report: the IAU astronomical unit

Audit row 2, owner decision of 3 Oct 2026 (now in DECISIONS.md). `kAU` = **149,597,870,700 m** (was 149.6e9 m). Version stays 0.10.5; history, About, ROADMAP ("Orbit audit: the web's AU" removed from Later), RESUME updated. No card text template changed, so no screenshot.

## Source
JPL Astrodynamic Parameters page, https://ssd.jpl.nasa.gov/astro_par.html, read 2026-10-03: **"astronomical unit au 149597870700 m IAU 2012 Resolution B1"**. The page says B1; the owner's decision says B2. I quoted the page's wording in the `units.h` comment and in About, said the two differ, and said the IAU text itself was not read (not reachable from here).

## Code changes
- `units.h`: `kAU`, with the comment. Typed au values replaced by `kAU`: the comet card's "million km" (`card_text.cpp`), the Learn seasons text (`learn.cpp`). Left as they are, because they are rounded text and still right: "149.6 million km" in a Journey caption; `384,400 km / 149.6 million km` in a comment became 149.598.
- **`kGMSun` (built from kAU, year not changed):** was 1.3272312e20 m³/s² (+8.05e-5 over JPL's GM of the Sun, 1.32712440041279419e20), is now **1.3271745e20 (+3.78e-5)**. The rest is the 365.25-day Julian year (the Gaussian year is 365.2568983 d). With the GM built this way every Simple period stays a^1.5 years, so **no time, eccentricity, inclination or period changed**; only lengths and speeds, by s = 149597870700 / 149.6e9 = 0.99998576671123.

## What was rescaled, kind by kind (script `tests/reference/rescale_au.py`, header says why)
They were made by the frozen web page's JavaScript with the web's au, which cannot give numbers for another au. In Simple every length is a multiple of the au and GM, softening and crash radius scale with au³, so a run is the old one scaled with time unchanged. Checked by the tests passing at their **old, unchanged tolerances** (1e-12 positions, 1e-9 runs and orbit shapes, 1e-6 belts): worst difference from the references 5.2e-11 (was 2.6e-11).
- `orbit_tests`, `reference_values.h`: positions and velocities of the 13 non-comet bodies in the initial set and the three runs (312 numbers), orbit sizes `a` (13), barycentre (3), belt-rock positions (108) × s. e, i, periods, times unchanged. The six comet rows (168 numbers) are not rescaled: `make_comet_refs.py` recomputes them from the JPL records with the new au.
- `jpl_tests`, `test_jpl_saved.inc`: the 120 numbers of `kSavedSimple` × s (match to 5e-16).
- **Not rescalable, so not touched, and the stage's brief called for rescaling them:** the nbody "saved 0.10.0" positions and the jpl "0.10.2 Real from the legacy elements start" values. They come from integrating the legacy elements start with the **real GMs, which do not scale**: the start's positions go × s but its velocities × s^(-1/2) (elements velocity ~ s, then × sqrt(GM_real / kGMSun) ~ s^(-3/2)), so the integrated orbit is not the old one scaled. Instead the new `tests/au_legacy.h` rebuilds the old start exactly (x / s, v × s^½) and the integrator must still give the saved numbers at the old 1e-9: nbody 3.9e-12, jpl 1.5e-10. The saved values are unchanged and just as strict.

## Re-measured numbers (all "regression guards, about 1 % above", not physical bounds)
- **Simple against Horizons, worst error 1800-2100 (km), before → after, and what the au change can explain** (1.42e-5 × a(1+e)): Mercury 7,641 → 7,618 (−23; ≤ +994); Venus 13,959 → 13,858 (−102; ≤ +1,551); Earth 16,314 → 16,112 (−202; ≤ +2,165); Mars 90,248 → 90,493 (+245; ≤ +3,547); Jupiter 1,840,647 → 1,839,933 (−714; ≤ +11,614); Saturn 7,999,511 → 8,000,757 (+1,245; ≤ +21,400); Uranus 2,912,393 → 2,942,873 (+30,480; ≤ +42,790); Neptune 2,302,277 → 2,341,475 (+39,198; ≤ +64,577). **Nothing is worse by more than the au change explains.** `kSimpleErrKm` updated; a new `jpl_tests` check does this comparison. The Simple note's text did not change (still Saturn 8.0, Uranus 2.9, Neptune 2.3, Jupiter 1.8, Mars 0.09 …).
- `jpl_tests` guards: Simple (km and arcsec) and "Real from the legacy elements start" re-set from the new measurements. The legacy start is test-only; its errors fell a lot (Mercury 10.5 → 1.85 million km: its velocities used a GM that was 8e-5 off, now 3.8e-5), so those guards are tighter; Saturn's rose 1.4 % (109.4 → 110.9 million km), Simple Neptune's 1.7 %.
- **Real from JPL (the app's default): unchanged**, same numbers to the digit (Mercury 29,230.1 / 70,044.3 km …), `real_errors.h` unchanged. Moon, ELP and Horizons-based tests unchanged.
- Comet guards (12) re-set: NEOWISE at perihelion 628 → 1 km and 67P 2,578 → 0 km (it was mostly the au; floor of 100 km there), Halley 27.0 → 26.98 million km at 2026-01-01, others within 1 %.

## Tests: full suite once, Linux GCC Release, no warnings: 7 of 7 pass
orbit 1,469; atom 17,936; app 1,385; nbody 37; jpl 54 (one new); moon 24; moon_real 59. `app_tests` (every word-for-word text): the only change is the About reference, edited like `app_text.inc` (one new line for the au source). No limit loosened; no test failed for another reason than the au.

## Not tested, not changed
- Windows, speed, any screen. The Journey caption "149.6 million km" and the card distances in "million km" can differ in the last digit from before (1.4e-5): not looked at.
- Year and Sun's GM (3.8e-5 above JPL) as asked; `kLightYearAU` (63,241.077 AU per light year) is now consistent with the IAU au, its source (IAU) was not read; the Real-mode tests do not use the au.
