# 0.56.3: research: moving gas in the Black Hole Lab (no change to what the app does)
Based on "native 0.56.2". The full UI suite was not run (small version). The app's behaviour is unchanged; the plan is native/PLAN_MOVINGGAS.md.
## Slip from 0.56.2
- native/CMakeLists.txt said `VERSION 0.56.1`, so the 0.56.2 build called itself 0.56.1. Now 0.56.3 (0.56.2's changelog entry stays).
## Papers saved (private sources repo, data/blackhole/, commit c94baa4; one SOURCES.md line each; check_sources ok, 566 lines)
- arxiv/1810.12641_gravity2018 (GRAVITY, A&A 618, L10; volume and page from Crossref for its DOI, the arXiv page has none), arxiv/2209.09926_wielgus2022
  (A&A 665, L6), arxiv/astro-ph_0506433_broderick_loeb2005 (MNRAS 363, 353), arxiv/astro-ph_0509237_broderick_loeb2006 (MNRAS 367, 905),
  arxiv/astro-ph_0606615_schnittman_krolik_hawley2006 (ApJ 651, 1031): each .pdf, .txt.gz (pdftotext -layout), _abs.html; scans/balbus_hawley_1991_ApJ376_214.pdf (ADS).
- Read again (already saved): GLa (B3), (12), (13c), (13f) and the I1, I2 of every case; GLb (52), (72); J15 App. A.6; Cunningham & Bardeen 1973 pp. 249-256 (text of the scan).
- Not used: arXiv:2207.06829 (Wielgus et al., the 2017 EHT millimetre light curves; fetched by mistake for the A&A paper) and Noble, Krolik & Hawley 2009 (arXiv:0808.3140): titles only, deleted.
  Nothing the job named was unreachable.
## The answers (details and equations in the plan)
- Q1 speeds and times: at the innermost stable orbit V = 0.500 c (spin 0) and 0.625 c (0.9); one orbit 4.55 ms / 32.6 min / 34.2 days (10 Suns / Sgr A* / M87*, spin 0),
  1.37 ms / 9.83 min / 10.3 days (0.9). Checked: Kepler in SI (1e-8); GRAVITY's "31 minutes" (30.3 at 4.0e6 Suns, 31.4 at their 4.14e6) and 40 +- 8 min at 7 Rg (39.5);
  Wielgus's 74 +- 6 min at 10.0-11.2 r_g (67.8-80.3) and t_g = 20.46 s (20.465).
- Q2 clocks: far away (t), the gas's own dtau/dt = alpha (1 - V^2)^(1/2) (C75 (A7a), = the metric's normalisation to 1e-14, -u_t = BPT's E to 2e-15), the hovering
  camera's alpha (kerr.h hoverShift). Sgr A*, spin 0, 6 M: one orbit 32.6 min far away, 23.0 min on the clump.
- Q3 light travel time: added to the app's integrator in the tool (GLa (12) as an affine-step integrand); GLb (72)'s 3 sqrt(3) pi M = 16.3242 M reproduced
  between successive crossings near the edge, radial light against its closed form to 1e-9. Closed forms exist for every case (GLa (B3) with E(phi|k)).
  At spin 0.6, 10 degrees above the disc: 0.17 orbit across the direct image at 6 M, the secondary image 0.03-0.34 orbit older; the clump's flash 4.4 x mean
  with it, 3.1 without. Recommended: exact per pixel.
- Q4 clump: observed (GRAVITY, Wielgus) and modelled (CB73, BL05, BL06); defensible: orbit, period, size R_S = 1.5 M or ~6 r_g; a choice: brightness, radius,
  no shear. Light curve by the processor reference; checked: periodic in one orbit, not in half; the secondary peak earlier than the direct one, as CB73 p. 254 say
  (qualitative only; a figure-point check against a published curve is NOT done: ROADMAP).
- Q5 structure: MRI (Balbus & Hawley 1991); no thin disc's face has been resolved; model from SKH06 (25 degrees by dr/r = 0.3, life 0.3 orbit), sheared at the
  true speeds; its strength set so the 6-10 M ring flickers 42 % rms (SKH06 Table 2: 43.8 %).
- Q6 Film: J15 A.6 describes an artist's disc (image, voxels, procedural "fractal noise" textures, not accreting, 4500 K) and says nothing about motion.
  Recommended: Film gets the moving gas, labelled as the lab's; alternative: Film stays still.
## Pictures (native/docs/blackhole, each with a .txt; 1000 px wide)
- moving_clump_strip.png (6 frames, the secondary image visible in the 3rd), moving_clump_lightcurve.png (direct, secondary, both, both without travel time),
  disc_structure_options.png (smooth today; the model; half an orbit later). False colour, log brightness: not the app's looks (said in each .txt).
## Tools
- native/tools/movinggas/movinggas.cpp (--check, --table, --render; the processor reference: KerrBL with t added, BL06's clump, SKH06's patches) and
  native/tools/movinggas_pictures.py (needs numpy and matplotlib; about 40 s). Its checks are the new quick test movinggas_physics (0.01 s).
## Tests
- check.sh quick with PL_SOURCES_DIR: 18/18 (movinggas_physics new, 11 checks); check_sources.py ok. No shader, UI or app code touched.
## Docs
- PLAN_MOVINGGAS.md (new), PLAN_BLACKHOLE.md section 9 (row 4d), ROADMAP.md (moving gas first; the open figure check), DECISIONS.md (the owner's choice of
  17 Nov 2026), RESUME.md, changelog.cpp, data/blackhole/README.txt, data/SOURCES.md.
