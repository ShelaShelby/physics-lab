# Native Physics Lab 0.10.5 (alpha) — stage 5 report: Orbit tests and About sources (last stage of 0.10.5)

Audit rows 18, 20, 21, 22, 23 (`git show 5239fcd:native/REPORT.md`). Version stays 0.10.5; history, ROADMAP ("0.10.5 stage 5 — done", "0.10.5: Orbit Lab audit and fixes — done"; speed rows 24-27 stay in Later "after the owner's timing"), RESUME, About updated. OWNER-TODO unchanged.

## 1. The AU resolution: not confirmed from the IAU
`iauarchive.eso.org/static/resolutions/IAU2012_English.pdf` and the SYRTE page `syrte.obspm.fr/IAU_resolutions/IAUResol_2012_0.html` were both tried and **neither was reachable** (the proxy refused; so were the IAU 2015 PDF, arXiv, NIST, Wikipedia). So units.h and About keep **JPL's wording ("IAU 2012 Resolution B1")**, say the owner's decision calls it B2, and say the IAU text was not read. Only ssd.jpl.nasa.gov and ssd-api.jpl.nasa.gov answered.

## 2. Row 18: every unsourced number, and what I did
| number | action |
|---|---|
| c | JPL astro_par page: "speed of light in vacuum c 299792458 m/s 2018 CODATA recommended values" (read 2026-10-03). Comment in units.h, About; test `c == 299792458` |
| kLightYearAU (63,241.077) | no source reachable, so **worked out** as c × 365.25 d / au = 63,241.07708 (`kLightYearAU = kLightSpeed * kYear / kAU`); About says so; test against 299,792,458 × 31,557,600 / 149,597,870,700 by hand |
| Sun 25.38 days | Horizons' Sun header, "Adopted sid. rot. per.= 25.38 d": cited in About; test 360 / 14.1844 = 25.38 |
| Sun equator 25 d, poles 34 d | no source: card line "the 25 and 34 days are rough" (the web-compared spin lines are untouched) |
| 5,772 K | IAU text unreachable: Sun card "Surface (rough): about 5,770 K" |
| habitable zone, frost line, lifetime | already "(rough)" (stage 2); no source opened (Kopparapu: arXiv blocked); listed in About |
| 30 % reflected | the app's own assumption, already on the card (stage 2); listed in About |
| belt counts and masses, Oort mass | no source: each belt card ends "The counts and masses above are rough figures (no named source)"; About |
| NEOWISE 5 km | SBDB has no diameter: card "(rough: JPL gives no size)" (new `nucleusRough` flag); About |
| other comet sizes | SBDB diameters (read 2026-10-03): Halley 11.0, Hale-Bopp 60, 67P 3.4, Swift-Tuttle 26 already equal; **Encke 4.9 → 4.8** (data fix, on purpose); source comment, test |
| phase cut-offs | the app's own convention, on the card and in About since stage 2; named again in About's "no named source" line |

## 3. Row 20 (Simple against Horizons): it already existed
`jpl_tests` measure Simple against `jpl_table.inc` (301 dates, 8 planets, km and arcsec), guard them, and check that `kSimpleErrKm` are the measured maxima (stages 3-4). **Added only what was missing:** the app's own path (`SolarSystem::loadAt` in Simple) gives the same positions as the measured `planetStateAt` (largest difference 0.000000 km over 301 × 8), so the quoted errors are what a user sees. `jpl_tests` 54 → 55.

## 4-5. Rows 21 and 22: new checks (section 16 of `orbit_tests`; queries in `tools/make_events_ref.py`, values in `tests/reference/events_table.inc`, Horizons, retrieved 2026-10-03)
All limits are regression guards about 1 % above the error measured here (Linux GCC), not physical bounds.
- Real, Mars seen from Earth, 27 Aug 2003 10:00: Horizons vector, measured **1,219 km** (distance 55.758 million km); guard 1,232. Replaces "±0.3 million km".
- Real, Jupiter-Saturn angle, 21 Dec 2020 12:00 (0.10578°): measured **0.00244″**; guard 0.005″ (a floor for compiler rounding). Replaces "0.1° ± 0.05°" (it gave 0.126°).
- Simple, Earth after one orbital period (P = 2π√(a³/GM) from the orbit's energy, Kepler's third law): **1,423 km** from its start (leapfrog, sub-steps ≤ 5.6 h); guard 1,438. Replaces "after one Julian year within 15,000 km" (a year is not the period); the energy check beside it stays.
- 2026 equinoxes (Horizons JD 2461120.1150, 2461306.5036): "next season starts in N days" from 5 start dates in Simple and in Real: worst **0.192 h**, limit 0.2 h.

## 6. Row 23: moved into `orbit/` (no ImGui) and tested
`orbit/date_text.h`: `parseDateTime`, the date limits, `clampJumpDate`, `jumpMessage`, `tspeedText`. `orbit/real_errors.h`: `realNoteText`, `simpleNoteText`. `orbit/card_text.cpp`: `selectionStillValid` (the hidden-selection rule), `dateLine`. `orbit/sky.cpp`: `voyagerCaption`. `ui/journey_dates.cpp`, `ui/orbit_panel.cpp`, `orbit_view.cpp` now call them (same texts). Section 16 has 49 new checks in all (rows 21-23; section 17, 17 checks, is row 18): dates (30 Feb, 24:00, seconds, leap day refused or kept), the clamp notes at 1799 and 2101, the 2050 note only in Simple, Time speed texts by hand (58 days at 0; 3,597 s = 60 minutes at the Journey's), the date line three ways, the selection rule (Dwarfs/Comets/Moons/Belts/Far off, wrong numbers, Voyagers before 1990), the Voyager caption in 1941, 1980 and today, both notes' numbers, and a **length guard of 460 characters** for the Real note (444 showed as 10 lines; the screen itself cannot be tested).
**Not moved:** the Journey stops' captions and zooms (they need the canvas), the Go-to tiles, the scale bar and "zoomed far out" text (inside `draw`), the two tooltips (static text).

## Tests changed on purpose, with why
- `test_orbit.cpp`: section 9 (Mars-Earth 0.3 Mkm, Jupiter-Saturn ±0.05°) and the "Earth back after a year" check removed, replaced by the stricter section 16 checks above; the imperial-cards loop now covers 5 more lines (belt and Sun cards, 1,523 checks).
- `test_app.cpp`: the saved About reference: the same 4 text changes as `app_text.inc` (c and the light year, the Sun's turning period, comet sizes, the "no named source" line); the edit count stays 14.
- `test_jpl.cpp`: one check added. No limit loosened; no other test changed.

## Tests: full suite once, Linux GCC Release, no warnings: 7 of 7 pass
orbit 1,540; atom 17,936; app 1,385; nbody 37; jpl 55; moon 24; moon_real 59. Looked at: the NEOWISE card once (1280×800, Xvfb; `PL_SELECT` cannot pick the Sun).

## Not tested
Windows, speed, the Real note's line count (a character guard instead), imperial text of the new lines on screen, the Sun and belt cards on screen (their lines are tested as text).
