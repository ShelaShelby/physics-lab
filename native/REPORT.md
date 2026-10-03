# Native Physics Lab 0.10.5 (alpha) — stage 2 report: Orbit Lab text fixes

Fixes for rows 1, 5, 6, 7, 8, 10, 11, 13, 14, 16 and 19 of the stage 1 audit (git history has it). **No physics number, orbit element or constant changed.** The only code changes besides text: row 19's `long long`, small helper functions the tests can reach, and two shared constants moved into `sky.h`. Version stays 0.10.5; history, ROADMAP, RESUME, OWNER-TODO updated (README needs nothing).

## What changed, per row
| row | change |
|---|---|
| 1 | Date box, jump note and the cards of dwarf planets and comets now say: dwarf planets and comets move on a simple orbit fixed at one date and drift from their real places the further the date is (jump note adds "Pluto is about 1 AU off in 1800", the audit's measured 1.07 AU). Planets, and Earth's Moon in Real, "follow real data". The old "dwarf planets, comets and belts are real" is gone. New `orbit/date_text.h` (`datePickHint`, `dateNotes`) |
| 5 | "After 2050 … extrapolated" only in Simple. Real says its errors were measured against JPL for 1800 to 1 Jan 2100 only |
| 6 | Voyager caption when it cannot be shown: before 1977 "not launched yet"; 1977-1989 "launched in 1977; this lab shows it from 1990 on". Launch year read from the probe data (`probeLaunchYear`), 1990 from a named constant (`kProbesShownFromYear`) now also used by `sky.cpp` |
| 7 | Oort card and Journey caption: travel times = distance / (Voyager 1's speed × 0.21095 AU per year per km/s), at two figures: about 560 years to 2,000 AU, about 56,000 to 200,000 AU. The edges are constants (`kOortInnerAU` etc.) used by the text and the time. Proxima: "4.24 light years away (268,000 AU)", the AU worked out from the same table value |
| 8 | No named source could be opened (arXiv, ADS, Wikipedia: 403 here), so: Sun card "Habitable zone (rough)", "Frost line (rough)", "Lifetime (rough)"; map label "habitable zone (rough)"; the "In the habitable zone / Outside" line removed from planet and dwarf cards. Numbers unchanged |
| 10 | "1800 to 1 Jan 2100" on the Moon card (both lines), jump notes, Real note, About, code comment |
| 11 | "Venus crosses the Sun, 2012" jumps to 01:26 UT. Horizons API (Venus 299 seen from Earth 500@399, QUANTITIES=23, 2012-06-06 00:30 to 02:30, step 1 min, read 2026-10-03): minimum 0.154° at 01:26. The query is in a comment at the tile |
| 13 | Temperature "(no atmosphere; assumes 30 % reflected)" |
| 14 | Moon card, one short note: the four main phases are exact; crescent and gibbous use this app's own cut-offs (2, 40, 60, 98 % lit). Same in About |
| 16 | Real note rewritten as one paragraph, the relativity paragraph removed (the on/off state is in the error list and the tooltip). **10 lines** (was 14 plus the relativity paragraph) |
| 19 | Moon card has no "(1.00 × our Moon)"; `long steps_` and `stepsTaken()` are `long long` |

Dropped from the Real note to fit: "(UTC difference not looked up)" became "dates used as TDB"; the Moon's "(2.9 arcsec)" and "no light time" (both stay on the Moon card and in About); "moons' pull" (covered by "each planet with its moons"). The relativity tooltip is unchanged.

## Tests
- Tests first: 50 new checks in `orbit_tests` (1,377 → 1,427), written before the code and failing to compile until it existed: travel times against `2000 / (16.94 × 0.21095)` = 559.7 and 55,968 years, the ratio 100, the card texts ("about 560 years", no "300"/"30,000"), Proxima 4.24 ly = 268,142 → 268,000 AU, launch year 1977, the jump notes in Simple and Real (after 2050, Moon range, no "where they really"), the Voyager caption for 1960 and 1980, dwarf and comet cards, no habitable verdict, "(rough)" on the Sun card, "assumes 30 %" on Ceres but not Earth, no Moon self-comparison (Io still compared).
- **Changed on purpose** (each needed by the new text): `test_moon_real.cpp` check text and match "Outside 1800 to 2100" → "Outside 1800 to 1 Jan 2100"; one new check there (59 now) for the phase-names note; `test_app.cpp`: the saved About reference for the ELP2000-82B entry has the same two edits as `app_text.inc` (range, phase names); `test_nbody.cpp` line 187: printf `%ld` → `%lld` for the `long long` (a warning, no check changed). Nothing loosened.
- Full suite once, Linux GCC Release, no warnings: 7 of 7 pass. orbit 1,427; atom 17,936; app 1,385; nbody 37; jpl 52; moon 24; moon_real 59.
- Looked at (Xvfb, software OpenGL, window 1280×800, normal text size): **three renders of the Real note, not one**: the first (before the rewrite) showed it at 14 lines plus the relativity paragraph, the second (shortened once) was cut off at the bottom and still about 13, so I counted line bands in the pixels (16 px apart) for the third, which is the final text: 10 lines. The Ceres card once.
- Not testable without ImGui, so only read and rendered: the date box text, the Venus tile's time field, the map label, the Oort Journey stop on screen.

## Not tested, not changed
- Windows, any speed, Extra large text for the shorter note, imperial units on the new card lines, the Journey run on screen, a jump to 1980 on screen.
- I installed GL and X11 dev packages with apt in this container to build the app.
- Left alone (outside this stage): the Orbit guide in `learn_data.inc` still has its web text about the habitable zone; row 18 (About sources for the habitable zone, albedo, au) and row 9 (Simple accuracy sentence) stay open, as do rows 2-4, 12 and 15 and the test and speed rows.
- Row 1 text uses the audit's Pluto figure (1.07 AU, 1800); the Ceres and Encke figures were not repeated, they do not fit one sentence.
