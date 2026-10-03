# Native Physics Lab 0.10.3 so far (stage 1 of 2): JPL data and validation of the Real planets

Stage 1 changes nothing the app does (version stays 0.10.2; the app still starts Real from its own elements). It adds the JPL tables, a tool, a test program (`jpl_tests`) and a start-state argument to `NBodyClock`. `index.html` untouched. Older reports are in git history.

## What changed
- `native/data/jpl/`: the owner's 9 Horizons files (git mv, unchanged) + README.txt (URL pattern, body codes, date, a header).
- `native/tools/make_jpl_data.py` (Python 3, standard library): checks 301 rows, years 1800..2100, each Julian date against a Gregorian formula, centre/frame/units/step lines; writes `tests/reference/jpl_table.inc` (440 KB, m and m/s, the file's digits) and `src/orbit/jpl_epoch.inc` (2026-01-01 12:00 TDB row, JD 2461042.0 = `kNBodyRefJD`). Stops with a message on an error page or a short file; CRLF and a BOM are accepted (tried on a copy; not part of ctest).
- `NBodyClock(h, gr, start = nullptr)`: default = today's start. New `tests/test_jpl.cpp` + `jpl_tests` target in CMake.

## Errors against Horizons (heliocentric, km / arcsec)
The tables say `{source: DE441}`, Horizons' default (spans -13200 to +17191; JPL says it is a little less accurate than DE440 for the present century, mainly the Moon). The lab's GMs are DE440's, so this is a comparison with Horizons' DE441, **not** "against DE440". Planet minus Sun on both sides (the Sun from horizons-sun); 1-day steps from 2026-01-01 12:00 TDB through the clock to 1 January 12:00 of each year; arcsec = angle between the two heliocentric vectors. Earth = Earth-Moon barycentre.

| model | planet | max (year) | 1800 | 1900 | 2000 | 2050 | 2100 |
|---|---|---|---|---|---|---|---|
| Simple | Mercury | 7,641 / 34 (2089) | 1,582 / 4.83 | 1,795 / 4.44 | 1,728 / 2.7 | 2,438 / 10.2 | 3,127 / 4.24 |
| Simple | Venus | 13,958 / 26.5 (2041) | 7,938 / 15.2 | 7,831 / 10.6 | 8,702 / 14.8 | 846 / 1.45 | 12,064 / 20.1 |
| Simple | Earth | 16,314 / 22.6 (1844) | 3,197 / 4.48 | 4,982 / 5.25 | 2,611 / 2.76 | 3,893 / 5.46 | 4,958 / 5.43 |
| Simple | Mars | 90,247 / 85.4 (1861) | 33,350 / 27.1 | 20,708 / 12.6 | 5,733 / 3.75 | 69,380 / 58.9 | 41,802 / 23.6 |
| Simple | Jupiter | 1,840,647 / 506 (2048) | 891,334 / 232 | 345,267 / 44.8 | 1,152,447 / 309 | 1,132,832 / 259 | 545,224 / 129 |
| Simple | Saturn | 7,999,510 / 1186 (2067) | 3,616,250 / 544 | 4,386,887 / 545 | 3,839,564 / 517 | 2,999,464 / 400 | 6,195,363 / 831 |
| Simple | Uranus | 2,912,392 / 200 (2081) | 1,464,192 / 50.3 | 1,180,720 / 8.97 | 1,064,768 / 70.6 | 1,048,634 / 79.1 | 1,959,464 / 99.8 |
| Simple | Neptune | 2,302,277 / 90.8 (2097) | 1,150,446 / 45.1 | 1,098,942 / 22.5 | 1,072,125 / 45 | 615,752 / 25.3 | 1,794,127 / 37.9 |
| Elements, GR on | Mercury | 10,537,877 / 47053 (1806) | 10,035,115 / 40493 | 4,454,588 / 14191 | 824,102 / 2435 | 1,104,182 / 4623 | 2,581,897 / 8127 |
| Elements, GR on | Venus | 6,456,597 / 12389 (1800) | 6,456,597 / 12389 | 3,563,289 / 6755 | 736,292 / 1410 | 678,098 / 1285 | 2,094,256 / 3982 |
| Elements, GR on | Earth | 5,270,331 / 7391 (1800) | 5,270,331 / 7391 | 2,939,225 / 4122 | 608,047 / 853 | 557,367 / 782 | 1,722,763 / 2416 |
| Elements, GR on | Mars | 7,522,474 / 7498 (1808) | 7,039,172 / 6210 | 4,249,286 / 4119 | 828,348 / 820 | 668,293 / 565 | 2,417,016 / 2197 |
| Elements, GR on | Jupiter | 12,265,270 / 3224 (1802) | 12,002,921 / 3224 | 7,237,510 / 1853 | 538,495 / 141 | 1,328,638 / 327 | 3,280,807 / 829 |
| Elements, GR on | Saturn | 109,383,511 / 16581 (1800) | 109,383,511 / 16581 | 54,484,650 / 7453 | 13,240,178 / 1973 | 12,139,979 / 1614 | 26,263,456 / 3742 |
| Elements, GR on | Uranus | 403,794,966 / 30516 (1800) | 403,794,966 / 30516 | 212,005,339 / 15265 | 30,269,475 / 1458 | 16,020,188 / 530 | 127,150,718 / 8771 |
| Elements, GR on | Neptune | 110,629,640 / 4974 (1800) | 110,629,640 / 4974 | 78,613,718 / 3624 | 14,255,941 / 406 | 10,486,762 / 429 | 17,191,528 / 650 |
| JPL, GR on | Mercury | 70,044 / 314 (1806) | 67,414 / 277 | 29,686 / 95.1 | 5,464 / 16.1 | 7,256 / 30.3 | 17,114 / 54 |
| JPL, GR on | Venus | 45,430 / 87.2 (1800) | 45,430 / 87.2 | 25,047 / 47.5 | 5,200 / 9.95 | 4,766 / 9.03 | 14,734 / 28 |
| JPL, GR on | Earth | 22,080 / 31 (1800) | 22,080 / 31 | 12,313 / 17.3 | 2,540 / 3.56 | 2,345 / 3.29 | 7,230 / 10.1 |
| JPL, GR on | Mars | 22,340 / 22.3 (1808) | 20,583 / 18.1 | 12,570 / 12.2 | 2,672 / 2.65 | 2,136 / 1.81 | 6,942 / 6.31 |
| JPL, GR on | Jupiter | 2,823 / 0.776 (1800) | 2,823 / 0.757 | 1,526 / 0.391 | 318 / 0.088 | 293 / 0.077 | 845 / 0.214 |
| JPL, GR on | Saturn | 1,067 / 0.161 (1800) | 1,067 / 0.161 | 494 / 0.068 | 130 / 0.019 | 115 / 0.016 | 318 / 0.045 |
| JPL, GR on | Uranus | 876 / 0.066 (1800) | 876 / 0.066 | 478 / 0.035 | 41 / 0.003 | 33 / 0.002 | 204 / 0.014 |
| JPL, GR on | Neptune | 2,791 / 0.127 (1800) | 2,791 / 0.127 | 965 / 0.044 | 36 / 0.001 | 41 / 0.001 | 460 / 0.02 |
| JPL, GR off | Mercury | 29,230 / 87 (1803) | 19,143 / 79.4 | 14,749 / 48.1 | 3,433 / 10.1 | 2,005 / 8.46 | 8,838 / 28.3 |
| JPL, GR off | Venus | 4,762 / 9.14 (1800) | 4,762 / 9.14 | 2,485 / 4.71 | 541 / 1.04 | 473 / 0.897 | 1,490 / 2.83 |
| JPL, GR off | Earth | 3,960 / 5.55 (1800) | 3,960 / 5.55 | 2,206 / 3.09 | 456 / 0.639 | 421 / 0.59 | 1,295 / 1.82 |
| JPL, GR off | Mars | 7,292 / 7.27 (1808) | 6,010 / 5.26 | 4,009 / 3.87 | 867 / 0.858 | 600 / 0.506 | 2,093 / 1.89 |
| JPL, GR off | Jupiter | 728 / 0.202 (1809) | 703 / 0.188 | 353 / 0.09 | 85 / 0.024 | 72 / 0.019 | 186 / 0.047 |
| JPL, GR off | Saturn | 150 / 0.023 (1800) | 150 / 0.023 | 44 / 0.006 | 18 / 0.003 | 10 / 0.001 | 32 / 0.005 |
| JPL, GR off | Uranus | 619 / 0.044 (1829) | 561 / 0.042 | 319 / 0.023 | 23 / 0.002 | 14 / 0.001 | 98 / 0.007 |
| JPL, GR off | Neptune | 2,652 / 0.121 (1800) | 2,652 / 0.121 | 863 / 0.039 | 31 / 0.001 | 37 / 0.001 | 411 / 0.017 |

## Findings (measured; nothing changed because of them)
- **From the JPL state, GR on is farther from JPL than GR off**, for every planet, at the maximum and at all 5 dates; Mercury 67,414 vs 19,143 km at 1800, 17,114 vs 8,838 km at 2100. The code's own comment says the potential used (Nobili-Roxburgh, as REBOUNDx's gr_potential) gets the perihelion advance but the mean motion wrong (about 2.5e-8 relative for Mercury); that is the likely cause, not proven here. GR off is not right either (Mercury 29,230 km at most).
- **Real from the elements is worse than Simple at nearly every planet and sampled date** (the one exception: Jupiter at 2000, 538,495 vs 1,152,447 km) (Saturn 13 million km = 1,973 arcsec at 2000, vs 3.8 million for Simple; Mercury 824,102 vs 1,728 km). The elements start drifts (nbody.h says Saturn 0.1 deg/year); I did not analyse why. Stage 2 should decide what to do.
- **Existing doubtful values** (not changed): `kAU` = 149.6e9 m (the web's) vs IAU 149,597,870,700 m (IAU 2012 B2), 1.4e-5 relative, about 2,000 km at 1 au in Simple and in the elements start. `kNBodyRefJD` is documented as 12:00 UT, the Horizons rows are TDB; they differ by about a minute today (not looked up here), which matters for stage 2.
- Error grows away from the 2026 epoch (1800 is 226 years back); Jupiter-Neptune at 2000 and 2050 are within 85 km from the JPL state with GR off.

## Shift and the Sun
JPL's barycentre includes Pluto, the asteroids and so on. Moving the JPL epoch state to the lab's own 9-body centre of mass (GM-weighted, lab GMs; test path only) shifts positions by **128.5 km** and velocities by **0.039 mm/s**; heliocentric differences are unchanged (checked to 1e-3 m). The Sun's own barycentric position, lab (JPL start, GR on) minus JPL: at most 172.5 km over the 301 rows (142, 159, 99, 148, 162 at the 5 dates), i.e. 0.025 % of the Sun's radius (695,700 km); it includes the shift.

## GM comparison
Sun: Horizons header 132712440041.93938 km^3/s^2 vs lab 132712440041.279419 (DE440): difference +0.659958, relative +4.97e-12. Mercury header 22031.86855 vs 22031.868551 (-4.5e-11, the digits printed); Venus 324858.592 vs 324858.592000 (equal). The 6 barycentre headers give no GM. The header names no source for its Sun value. I could not reach JPL's Astrodynamic Parameters page (proxy 403), so the lab's DE440 value is still only from the earlier page excerpt and I changed nothing.

## Tests (full suite once, Linux GCC x86-64: orbit 1,377, atom 17,936, app 1,383, nbody 36, jpl 47; all pass)
- (a) epoch .inc = 2026 row exactly; JD gaps 365/366 (366 exactly in Gregorian leap years); Mercury 0.3075-0.4667 au, Earth-Moon 0.9832-0.9834 au from the Sun.
- (b) Real and Simple at 5 dates equal the 0.10.2 values (saved from the old code: our own earlier output) to 1e-9 relative; measured difference 0 here.
- (c) the table above; (d) limits are regression guards, each ~1 % above what was measured, labelled in the code, not physical bounds; GM limits likewise.
- Not tested: Windows or another compiler, any speed, the running app and screenshots (nothing visible changed), other start epochs. The saved 0.10.2 numbers and the guards are from GCC; other compilers may differ in the last digits.
