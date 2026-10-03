# Native Physics Lab 0.10.3 (alpha) — final report (stages 1 and 2)

Real planets now start from JPL Horizons data; relativity tick off by default; Real stays an option (Simple is the default) until the owner has tried it. Orbit Lab only; the Atom Lab and `index.html` were not touched. Older reports are in git history. Version, changelog (🕰 history), README, ROADMAP, RESUME, DECISIONS and OWNER-TODO are updated.

## What changed
- **Start state** (`nbody.*`): the clock's default start is the stage-1 JPL epoch state (`jpl_epoch.inc`, 2026-01-01 12:00 TDB) moved to the lab's own 9-body centre of mass (GM-weighted mean position and velocity subtracted; 128.5 km, 0.039 mm/s), so the centre of mass is at rest at the origin. Heliocentric values are exactly JPL's. The old elements start is kept as `nbodyElementsStartState`, marked legacy/test-only (the "before" column and the proof that the integrator is unchanged). Simple is unchanged.
- **Time scale:** `kNBodyRefJD` is 12:00 TDB, not UT (comment fixed); dates are used as TDB without conversion, said in the app note and here. UT/TDB difference: "a small difference (not looked up)": IERS and USNO were not reachable from this session.
- **Relativity** defaults to off (`ViewSettings::relativity`; Defaults uses `ViewSettings{}`). A `gr` value already in an old settings file is **kept** (`readBool` only overwrites the default when the file has 0 or 1), so users who saved gr=1 still have it on until they press Defaults. Tooltip and note now give the measured numbers.
- **Real note** (under Planet physics): start from JPL Horizons (DE441), model, what is left out, dates as TDB, and the worst error of every planet 1800-2100 for the current relativity setting. 10 lines at normal text size. The numbers come from `orbit/real_errors.h` and a test checks them against the measurement.
- **About:** added the Horizons API documentation, the JPL DE440/DE441 note and "Horizons data retrieved 2026-10-03"; the astrodynamic-parameters entry now says its values were read from the page itself and matched the code (the owner's statement, 2026-10-03; code comments updated). I did not open the DE440/DE441 page: its figures (span, "a little less accurate than DE440") are as the owner gave them, and the About entry says so.

## Errors against Horizons (DE441, not DE440), heliocentric position, worst over 1800-2100, km
| planet | before: elements start, GR on (max / at 2000) | after: JPL start, GR off, the new default (max / at 2000) | after: JPL start, GR on (max / at 2000) |
|---|---|---|---|
| Mercury | 10,537,877 / 824,102 | 29,230 / 3,433 | 70,044 / 5,464 |
| Venus | 6,456,597 / 736,292 | 4,762 / 541 | 45,430 / 5,200 |
| Earth | 5,270,331 / 608,047 | 3,960 / 456 | 22,080 / 2,540 |
| Mars | 7,522,474 / 828,348 | 7,292 / 867 | 22,340 / 2,672 |
| Jupiter | 12,265,270 / 538,495 | 728 / 85 | 2,823 / 318 |
| Saturn | 109,383,511 / 13,240,178 | 150 / 18 | 1,067 / 130 |
| Uranus | 403,794,966 / 30,269,475 | 619 / 23 | 876 / 41 |
| Neptune | 110,629,640 / 14,255,941 | 2,652 / 31 | 2,791 / 36 |

## Tests (full suite once, Linux GCC x86-64: orbit 1,377, atom 17,936, app 1,385, nbody 37, jpl 52; all pass)
- **New:** the default start equals the JPL row minus the shift (heliocentric 7.6e-6 m, limit 1e-3), centre of mass and momentum zero; a clock with no start is bit for bit the JPL-started clock; the stage-1 guards hold through the default clock, GR off and on (limits unchanged, each ~1 % above the measurement, regression guards only); the quoted numbers equal the measured maxima rounded up; the GR default, Defaults, and a saved gr=1.
- **Changed on purpose:** (1) nbody "start state is the elements' state" now tests the legacy function, plus a new check that the default is not the elements' (Mercury > 1,000 km apart); (2) nbody "GR off equals the saved 0.10.0 positions" uses the legacy start (those numbers come from it); (3) jpl "nothing changed" runs Real from the legacy start (equal to the 0.10.2 values, measured difference 0) and Simple (equal, 0); (4) app "General relativity on by default" is now "off by default"; (5) the About edit count 10 → 11. Conservation (energy 1e-8, angular momentum, momentum, centre of mass, reversal, GR conservation) passed unchanged with the new start; no limit was loosened.
- The numbers in the app note round **up** ("measured, rounded up"), so some are 1 km above the owner's list (Mercury 29,231 not 29,230, 70,045 not 70,044; Mars 7,293; Saturn 151; Uranus 620; Neptune 2,653; Earth 22,081 and Mars 22,341 with GR on). The table above is rounded to the nearest km.

## Looked at (Linux, Xvfb, software OpenGL)
- The Real note at normal text (twice: the first draft was 11 lines, so I shortened it and looked at the final) and at Extra large text (once; my crop cut off the bottom half, so I saw only the first half wrapping cleanly). GR box off, version label 0.10.3.

## Doubtful points (not changed)
- GR on is farther from JPL than GR off (stage 1); a better 1PN method is on the roadmap. `kAU` = 149.6e9 m vs IAU 149,597,870,700 m (1.4e-5, about 2,000 km at 1 au): roadmap, orbit audit. The Sun's GM: Horizons header 132712440041.93938 vs lab 132712440041.279419 (+4.97e-12); not changed (no named source shows the DE440 value is different).
- Errors are measured only at the 301 tabulated dates (1 January 12:00 of each year), with a 1-day step; between them not measured. Moons, dwarfs, Pluto, asteroids are not in, as the note says.

## Not tested
- Windows or another compiler; any speed or smoothness; Real at the fastest Time speed, trails, the Journey, planet cards, a narrow window, the Extra large note's lower half: all on the owner's try-out list. The saved-settings path (`readBool`) was read, not run (it is UI code the test programs do not link).
