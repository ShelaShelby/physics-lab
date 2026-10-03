# Native Physics Lab 0.10.4 stage 1 (alpha) — report

The real Moon: data and validation only. **No app or UI change** except two text lines (About sources, 🕰 history). Orbit Lab and Atom Lab behave exactly as in 0.10.3. Older reports are in git history. Version, README, 🕰 history, ROADMAP, RESUME and DECISIONS are updated; nothing for OWNER-TODO.

## What changed
- `src/orbit/moon_elp.h/.cpp` (new, in `orbit_core`): `moonGeocentricElp(jdTDB)` = the Moon's centre seen from the Earth's centre, metres, mean dynamical ecliptic and inertial equinox of J2000, geometric (no light time). Meant for 1800-2100 (`kMoonElpFirstJD/LastJD`); outside that it runs but is not measured. The comment says what is left out.
- `src/orbit/elp_data.inc` (new, 32 KB, generated): constants and terms of **ELP2000-82B** (Chapront-Touzé and Chapront, A&A 190, 342, 1988; CDS VI/79). `tools/make_elp_data.py` downloads ELP1..ELP36 and `elp82b.f`, checks the record counts against the ReadMe, **reads every constant out of the Fortran text** (nothing typed from memory), merges the 36 files into one term list, measures, truncates, writes the `.inc`. The series form, arguments and the change to J2000 are the subroutine's (copy in `example.f` checked identical).
- `tools/make_moon_ref.py` + `tests/reference/moon_table.inc` (new): Horizons API, Moon (301) and Sun (10), `500@399`, ecliptic, km-s, geometric, TDB, DE441; source, the 6 queries and retrieval date (2026-10-03) in the header. 170 rows: 61 every 5 years 1800-2100 (1 Jan 12:00); 60 daily from 2025-12-02 (two lunar cycles); 49 hourly 2026-01-02..04. The Sun and the hourly group go beyond the brief: they are needed for the phase tests.
- `tests/test_moon.cpp` (new, `moon_tests`), CMake 0.10.4. About "Where the facts come from": ELP2000-82B and the Horizons Moon table added (`app_text.inc` + matching entry in `test_app.cpp`).

## Measured error against Horizons (DE441, not DE440), position in km and arcsec as seen from the Earth
| series | worst | rms | median |
|---|---|---|---|
| **all 37,872 terms** | 5.790 km (3.086") | 1.446 km (0.777") | 0.559 km (0.318") |
| **kept 487 terms** (in the app code) | 5.212 km (2.879") | 1.541 km (0.781") | 0.761 km (0.323") |

By group, kept series: every 5 years 5.212 km (2.88"), daily 1.384 km (0.77"), hourly 0.755 km (0.20"). Smallest near 2000 (0.65 km), growing away from it (1900 2.7 km, 1850 3.0 km, 1800 5.2 km; 2050 1.1 km, 2100 3.4 km). So the accuracy is **a few km at worst, about 3" in the sky, over 1800-2100, at the 170 dates**; it is not claimed between them. The kept series is not "better" than the full one (5.21 vs 5.79): the dropped terms just happen to cancel a little at these dates.
- **Truncation rule** (measured): sort terms by size (amplitude x the largest |T|^p over 1800-2100, in arcsec); drop from the smallest upward in steps of sqrt(2); stop when the worst error passes 1.5 x the full 5.790 km. **The brief's rule alone keeps 234 terms** (worst 7.357 km) **but** those differ from the full series by up to 9.353 km at 300 seeded dates not in the table (the table is only 170 dates). I added a second condition: at those 300 dates the kept series must stay within 0.5 x 5.790 km of the full one (then 1.5x holds there too by the triangle inequality). That gives **487 terms** (229 longitude, 128 latitude, 130 distance), 2.711 km from the full series at the 300 dates. The whole sweep is printed by the tool and the headline of `elp_data.inc` gives the numbers.

## Other results (all from `moon_tests`)
- Moon distance: Horizons table 357,177-406,592 km; ELP scan every 0.25 day 1800-2100: 356,380-406,711 km. Both inside 356,000-407,000 km (the brief's bound; the NASA Moon fact sheet values I remember, about 356,400 and 406,700 km, were **not opened**).
- Sidereal period from the ELP Moon alone, 120 one-year windows: mean 27.32206 d against Horizons' header "Orbit period = 27.321582 d" (+0.000475 d). A single year is off by up to 0.0737 d (range 27.248-27.379): a year holds 13.37 orbits, so the unfinished orbit matters. That is why the mean is tested.
- Phases (ELP Moon + JPL Sun, elongation in longitude): eight crossings (new, first quarter, full, last quarter, twice) in the right order; worst difference of a crossing time against Horizons 0.023 minutes (daily samples, 4-point interpolation). Full moon of 2026-01-03: Horizons JD 2461043.91990 TDB (about 10:04 TDB), ELP differs by 0.0007 minutes (0.04 s). Longitudes are geometric: no light time, no aberration (a real full moon time differs by tens of seconds; not computed).

## Tests (full suite once, Linux GCC x86-64, warnings on: orbit 1,377, atom 17,936, app 1,385, nbody 37, jpl 52, **moon 23**; all pass)
- Written from the Horizons table first (the test file existed before `moon_elp.cpp`); the error guards were filled in after the first measurement and are **regression guards about 1 % above the measured value**, each with the measured number beside it (worst 5.264 km, rms 1.556 km, 2.908", 0.788"; crossings 0.0233 min; full moon 0.00069 min; period mean 0.00048 d, single year 0.0744 d). Nothing was loosened.
- A test checks the C++ against the Python port at 24 dates (worst 0.0007 m): this checks the port, not the theory.
- **Changed on purpose:** `app_tests` "every native edit was used" counts About edits, 11 to 12 (one new edit); the full suite failed on that one check, and I re-ran only `app_tests` after the fix (1,385, 0 failed). No word-for-word reference was touched.
- During work: the first run of the period test was wrong (a floating-point step count in my test, not the Moon); fixed to an integer step count.

## Not tested
- Windows, MSVC, any compiler but GCC; **speed**: one call sums 487 terms, no timing was made (`moon_tests` takes about 12 s on Linux, mostly the period test, which makes about 175,000 calls). The default cache folder of the tool (`native/data/elp_cache`, with its own `.gitignore`) was not used; I passed a scratch folder.
- The Fortran itself was not run (no Fortran compiler here). Accuracy outside 1800-2100, between the 170 dates, UT versus TDB (dates are TDB), the Moon in the app.

## Doubtful points (reported, nothing hidden)
- The notice `elp82b.ps` (1.2 MB PostScript) was downloaded but **not read**: formulas and constants come from the Fortran subroutine and the series files, which the ReadMe presents as the same solution. Files 4-36 have no negative amplitudes (checked), so the subroutine's `x.lt.pre` test (no absolute value) cannot drop terms with no truncation.
- ELP2000-82B is a 1988 theory fitted to DE200, so its differences from DE441 (above) are larger than JPL's own. Differences not modelled: ELP's fixed dynamical J2000 frame against the ICRS-based Horizons ecliptic (size not looked up), tidal acceleration and lunar-figure differences between DE200 and DE441. They are inside the measured numbers, not separated.
- Horizons says its Moon is DE441; the 0.10.3 note says DE441 is "a little less accurate than DE440, mainly for the Moon" (the owner's reading of the JPL page, which no session opened). Errors here are against DE441.
- 27.321582 d comes from the Horizons Moon header, which names no source for it.
