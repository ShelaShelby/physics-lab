# 0.56.4: moving gas, step 1: the physics (no change on screen)
Based on "native 0.56.3". PLAN_MOVINGGAS.md section 7 step 1. No shader, UI or app drawing touched; the full UI suite was not run (small version).
## What changed
- src/blackhole/gas.h/.cpp (new): the clocks (far, on the gas C75 (A7a), the hovering camera alpha), the clump (BL06 (5), R_S = 1.5 M; choices A = 4, r = 6 M,
  no shear), the SKH06 pattern (32-bit hash, Gaussians in (ln r, phi), carried at Omega(r), T_life = 0.3 T_orb), the integrator with t (and Mino time).
  Moved from tools/movinggas, which now calls gas.h (one copy); its clock and period checks moved to blackhole_tests (same limits), --check keeps the integrator's.
- kerr_closed: per crossing phi_k and t_k (ClosedEnd::phi, ::t) from GLa (11), (12), (B2), (B3): I1, I2 of every case ((B17)-(B18), (B37)-(B38), (B68)-(B69)
  with R2 (B64), (B98)-(B99) with S2 (B94)), I+- of case (1) ((B19), (B23), re-derived), G_t of both polar types ((48), (68), without dividing by m). Written per
  amplitude of the substitution (odd antiderivatives), so turning points need no case; only differences camera - crossing. No recursion, bounded loops.
  closedStateAt (tests): the point at any Mino time. E(phi|m) and D = (F - E)/m by Carlson's R_D.
- spin_sky: SpinHit stores phi, t, nCross, timed per entry (closed-form rays); nothing reads them yet.
## Found and fixed
- kerr_closed Polar: Psi_0 was asin(s0); where the camera sits at the ray's polar turning point (looks exactly along e_phi) that turned rounding into 2e-8 rad
  of G_phi (and of the crossings' Mino times). Now atan2(s0, c0) with c0 from p_theta. All 0.55.9 closed-form tests still pass.
- The pattern's norm (0.56.3 tool: 20000 points in one 2000 M window) was about 20 % high (the disc 19 % darker on average): found by the conservation test.
  Now the light-weighted mean over independent samples (10 lifetimes apart). The tool's pictures of 0.56.3 were made with the old norm (rms unaffected).
## Tests (blackhole_tests testGas, testClosedTime; all 905 checks pass)
- Clocks: 1/u^t vs the normalisation with J15 (A.1) 1.5e-14; -u_t vs BPT (2.12) 1.8e-15; camera alpha vs 1/hoverShift 1e-16; Kepler in SI 1e-9.
  GRAVITY 2018 (30.3 min vs "31"; 39.5 in 40 +- 8) and Wielgus 2022 (t_g 20.465 s; 67.8-80.3 min) now read from new extracts of the saved papers.
- Travel time: GLb (72) at a = 0: 16.324195 M vs 3 sqrt(3) pi = 16.324194 (limit 1e-4); nearly radial light at a = 0 within 0.47 of the bound b^2/r2 (2e-7 M).
- Closed t, phi, r, cos theta vs the integrator (rtol 1e-12) at 947 crossings and about 1000 points, spins 0.01, 0.6, 0.9, 0.99, every substitution (1) 21 rays,
  (2) 178, (3) 261, (4) 7, case II 3, vortical 21. LIMIT (set before looking): per camera, the integrator's own spread between rtol 1e-10 and 1e-12 (crossings;
  points compared at the same Mino time). Closed errors 10-100x below it, e.g. t 1.5e-6 M vs 1.3e-4, phi 2e-8 rad vs 4e-5 near the edge.
- Cancellation at r3 = r+: rays with lambda = 2 r+ / a exactly (found to 2e-16) and 1 +- 1e-6, spins 0.01 (camera 1000 M), 0.6, 0.9, 0.99: within the limit;
  at 1e-13 against the integrator at rtol 1e-14 the crossing's phi agrees to 1e-14 rad and t to 3e-13 M.
- Clump light curve (closed forms, 120 x 72 px): one orbit later 5e-14 of the peak, half an orbit later 0.98.
- Pattern: an unfading mark returns after T_orb = 2 pi (r^1.5 + a) exactly; the light-weighted factor averages 1.018 (3 standard errors 0.068);
  face-on 6-10 M ring 42.4 % rms vs SKH06 Table 2 43.8 % (within the plan's 20 %; new extract of SKH06).
- Spin table: phi and t at 464 crossings of its entries = the closed forms exactly.
- check.sh quick with PL_SOURCES_DIR: 17/17 (font_coverage needs the app build); movinggas_physics passes; check_sources ok (38 extracts).
## Not done
- Cunningham & Bardeen 1973 figure-point check (ROADMAP line kept): their star orbits an a = 1 hole exactly (Figs 9-12, r_s = 20, 7, 3, 1.5); the closed forms
  divide by r+ - r- (0 at a = 1) and the lab stops at 0.99, and theirs is a point star, not the clump: their setup cannot be reproduced exactly, so not tried.
- The plan's "3 phi cells" per eta bin and slot for the card: a patch's birth time spreads over 0.3 orbit (108 degrees of turn), so more cells can reach a
  point; step 2 counts them. About's sources (BL06, SKH06, GRAVITY, Wielgus) come with step 3, as the plan says.
## Docs
- changelog.cpp, RESUME.md, ROADMAP.md, DECISIONS.md (the owner's choice), PLAN_MOVINGGAS.md (step 1 done), data/SOURCES.md (users), CMake VERSION 0.56.4.
