# 0.72.1: research for the Alpha Centauri system (PLAN_ALPHACEN.md); no change to what the app does (box: 4 cores; the full UI suite was not run; the Windows result: the final chat message)
**Size**: much bigger than a usual research job: 57 papers saved (arXiv, 22 Nov 2026) in the sources repo under data/alphacen/ (about 175 MB), one SOURCES.md line each, native/data/alphacen/README.txt. Every number in the plan was read by the lead session in the saved PDF; no helper agents were used (the reading was the job).
## Headline facts (details, places and the rest in PLAN_ALPHACEN.md)
- A-B orbit (Akeson et al. 2021, ALMA): P 79.762 yr, a 17.4930" = 23.30 au, e 0.51947, i 79.243°, next periastron 2035.3; masses 1.0788 ± 0.0029 and 0.9092 ± 0.0025; parallax 750.81 ± 0.38 mas (4.344 ly).
- The three published orbits (PB16, K16, A21) disagree by more than their error bars (masses by 5 %). Checked here: each paper's a³/P² equals its own mA + mB; A21's elements reproduce ALMA's positions within 1.3 mas, K16's miss by 28 mas, PB16's by 89-93 mas.
- Proxima is bound (8.3 σ), ~12,000 au from A-B, one orbit in 511 kyr; it comes to 0.958 pc of the Sun in 27 kyr (Gaia DR3).
- Proxima b confirmed (NIRPS 2025: m sin i 1.055 ± 0.055 M_E, 11.18465 d, 0.04848 au); Proxima d confirmed by NIRPS 2025 (0.260 ± 0.038 M_E, 5.12338 d); Proxima c disputed (Artigau 2022, NIRPS 2025 against); the JWST candidate around A seen once (Aug 2024), not in 2025, no result of the Aug 2026 chance found; Alpha Cen Bb refuted; the ALMA dust belt explained by a flare.
- Our Sun from Alpha Centauri: V 0.43 in Cassiopeia (computed here).
## Doubtful values in the app now (CLAUDE.md, Accuracy; NOT changed in this job; ROADMAP, Orbit Lab, **fix**)
1. far_masses.h "acen" A 1.13, B 0.97 M_sun (Pourbaix & Boffin 2016) with "the paper's own uncertainties were not read": PB16 Table 1 gives 1.133 ± 0.0050 and 0.972 ± 0.0045. The newer orbit (Akeson et al. 2021, Table 8; data/alphacen/akeson_2021_arxiv2104.10086.pdf) gives **1.0788 ± 0.0029 and 0.9092 ± 0.0025**: recommended (newest, ALMA astrometry, reproduces the measured positions; plan sect. 2).
2. sky_data.inc "acen" 4.390 ly (PB16, 743 mas): Akeson et al. 2021 give 750.81 ± 0.38 mas = **4.3441 ly** (computed here, 1 pc = 3.26156 ly).
3. far_masses.h comment "Mann et al. 2015 give 0.12 +- 0.02": Proxima is not in Mann 2015's tables; the 0.1221 ± 0.0022 that the planet papers use is Kervella et al. 2017 Table 1 (Mann's relation with Proxima's K magnitude).
4. sky_data.inc "prox" 4.2465 ly (Gaia DR3 768.0665 mas "read from the Gaia archive 2026-11-05"): the archive page was never saved; the saved sources now give 768.067 ± 0.050 mas (Libralato et al. 2025 Table 2, Suárez Mascareño et al. 2025 Table 2): same value, the comment should name them.
5. sky_data.inc "prox" type "M5Ve" (HYG): the papers give M5.5 V (Bessell 1991 as quoted by NIRPS 2025 and Faria 2022 Table 1).
## Errors found in the papers (reported, not hidden)
- K16 Table 1 and A21 Table 8 quote PB16's Ω as 204.85 ± 0.084, which is PB16's 2002 column; PB16's own fit says 204.75 ± 0.087.
- K17a Table 5 prints the parallax as 717.17 mas (747.17 everywhere else in the paper). Ribas 2017 prints L = 0.00151 ± 0.0008 (its erg/s error gives ± 0.00008).
- NIRPS 2025 Table 2 gives Proxima's µδ as −20.578 mas/yr; Gaia DR3 (Libralato 2025 Table 2) is +769.465. Its habitable-zone edges are not reproduced with its own listed L (plan sect. 6).
- Published Teq of Proxima b / d (218 / 282 K) come from Teff and R; Proxima's published L, Teff and R disagree (R 0.141 and 2900 K give 0.00127 L_sun, not 0.00151).
## Not reached (proxy refused; ROADMAP "Waiting for the owner")
Damasso et al. 2020 (Sci. Adv., Proxima c; its numbers used as Kervella 2020 and NIRPS 2025 quote them), Benedict & McArthur 2020 (RNAAS 4, 86), Dumusque et al. 2012 (Nature 491, 207; numbers via Rajpaul 2016), the IAU WGSN name list (dates of Rigil Kentaurus, Toliman, Proxima Centauri), NASA Exoplanet Archive and Gaia archive queries (not needed: the values are in saved papers), IAU constellation boundaries (Cassiopeia from HYG's labels instead), Kopparapu et al. 2017 (on arXiv, not saved: only to reproduce NIRPS's inner edge).
## What was made
native/PLAN_ALPHACEN.md (sections 0-14), native/tools/alphacen_numbers.py (every "(computed here)" number; --picture draws docs/alphacen/alphacen_overview.png, with its .txt), native/data/alphacen/ (README.txt, a 67-row HYG extract), 57 SOURCES.md lines; DECISIONS.md (22 Nov 2026), ROADMAP.md, RESUME.md, changelog 0.72.1, VERSION 0.72.1.
## Tests
check_sources.py with the sources checkout: ok (635 manifest lines). check.sh quick with PL_SOURCES_DIR set: 18/18 passed (the source guard and provenance check first: ok). No screen changed: no ui_* test was run; the UI suite was not run.
