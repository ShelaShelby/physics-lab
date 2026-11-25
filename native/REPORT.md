# 0.84.1: Research: the sources for every fix of the outside review (25 Nov 2026); a plan file; no change to what the app does

## What changed
- Step 0: the full-test run of 0.84.0 (fe8bd82, "Native full tests" run 45587630152) passed. Nothing added to ROADMAP.
- Saved 63 files in the private repo (sources commit 20be64d), each with a SOURCES.md line. 16 new extracts. Scans with no text layer have typed lines (_typed.txt), checked against the page images.
- Found: (a) Neckel & Labs 1994 Table I (ADS legacy scan). (b) Greenwich + USAF/NOAA groups May 1874 to 2 Oct 2016, NOAA's daily reports 1996-2026 (NCEI). (c) Oort: discovery papers and JPL SBDB. (d) Shimura & Takahara 1995, Done et al. 2012, Chandrasekhar 1946 Table 2. (e) Cunningham 1976. (f) Abramowicz, Lanza & Percival (astro-ph/9611101). (g) Yerokhin & Shabaev 2015 (2) (already saved). (h) IUP Bremen ozone, AFGL Handbook 1985 ch. 18. (i) Deau 2009, Dyudina 2016, Mayorga 2016, Heng & Li 2021, Irwin 2021/2022.
- native/PLAN_REVIEW.md: R1-R14, each with source, formula, tests, what the user sees, limits and GPU path. Order: 0.85.0 Orbit Lab, 0.86.0 Atom Lab and Film, 0.87.0 the disc.
- DECISIONS.md (25 Nov), ROADMAP.md, folder READMEs (sun, oort, blackhole, atmosphere, planets, lamb), changelog, VERSION 0.84.1. No app code changed.

## Tests run
- check_sources.py on a full checkout of the sources repo: ok (758 manifest lines, 62 extracts, 154 blocks).
- Typed tables checked by their own relations: NL94 rows by Sum A_n = 1 and Equation (6); Chandrasekhar's Table 2 by its column sums, ratios and polarisation degree.
- check.sh job: quick 18 of 18 passed, ui_smoke passed (full build of 0.84.1 on the build machine).

## Not done
- Doubtful values in sources: NL94's 365.875 nm row as printed sums to 0.99800 and gives F/I 0.74354, not 0.7445 (ultraviolet: the app does not use it). The disc's spin term: Zhu 2012 (6) and ALP97 (27) differ from AF13 (92). Only AF13's matches AF13 (40) (measured here: ALP97 up to 0.9 % off, Zhu up to 3 %). PLAN_REVIEW.md R12 uses AF13.
- Refused: ADS's new scan service (202), Debrecen (403, no answer), SWPC warehouse (503), DTIC (403 twice), AMS (403). Owner's files list in ROADMAP.
- source needed (ROADMAP): the sunspots' contrast, Uranus's and Neptune's limb darkening in numbers, an ozone column, phase laws beyond the surge. Chandrasekhar's 1946 table is the third approximation (flux 0.991 of 1 by a fit, measured here).

## References changed on purpose
- None. No test changed.
