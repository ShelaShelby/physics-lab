# 0.79.0: the feeding black hole's disc: streaks, true thickness, a thinning inner edge (Real; CPU reference and GPU)

## What changed
- Read: CLAUDE.md, REPORT.md, ROADMAP.md (Black Hole Lab), PLAN_DISC.md (all), disc078_real_feeding.txt; by grep and ranges: discproto.cpp, disc.h, gas.h, gas.cpp, lab.h, lab.cpp, tone.h, blackhole_view.cpp, shaders.h (kLensFS, kKerrSkyFS, kGasPart), vk_blackhole.*, test_blackhole.cpp, blackhole_picture.cpp, bench_scenes.cpp, benchmark.cpp waitDefs; Zhu, Guan, Lee & Gammie, SKH06 in the sources repo.
- Thickness (disc.h DiscPhys::height, Zhu et al. 2012 App. B (B1a)-(B1f) with the gas's pressure). Zhu's kappa 0.3383 cm^2/g and mu 0.615 with the lab's L_Edd: h is 0.846 of PLAN_DISC's numbers (largest h/R 0.0150). Gas pressure is half the total from 567 M (alpha 0.1, labelled). Spin > 0 stays flat: the R_z spin term is not checked (ROADMAP).
- Surface: the crossing plus a step back to |z| = h(R) on the lens table (4 straight-line steps, 2 table steps), orders 0-1, in kLensFS and in discRayTable. The disc thins to nothing at r_ms.
- Streaks (gas.h StreakField) replace the patches in Real and Film: a Lee & Gammie field (15 degrees trailing, 25 degrees by 0.18), rank-Gaussian, 8 mip levels, exact variance between texels. Two renewed copies per clock bin of 0.5 in ln R, c = 1.3. Made at start-up on a worker (0.55 s); response line response.blackhole_streaks. Hard scene blackhole.disc_streaks.
- The gas epoch is now 64 M (was 1024): float rounding of the streaks' clock reached 1.3e-3 of S at spin 0.99. Guide, About (Zhu, Guan, Lee & Gammie), DECISIONS, ROADMAP, PLAN_DISC section 9.
- Look (discproto real079, each panel at its own exposure, docs/blackhole/disc079_*): not paler (saturation 0.358 against 0.363), slightly flatter (ln Y spread 1.47 against 1.71): the thin streaks average within a pixel. Feasibility: the CPU reference takes 0.7 s more for 2560 x 1369 on 4 threads. Estimate about 70 texel reads and 600 operations a crossing, about 2-3 % of the card's peak: over PLAN_DISC's 1 % (stated before the GPU work).

## Tests run
- blackhole_physics: 1018 checks, 0 failed. New: thickness (far limit 1e-3, r_ms, M scaling, Zhu Fig. 3 reading 0.0150 vs 0.02, (B1) residual 4e-16, gas closed form 1.0001); streaks (variance 1, mean 0.9965 +- 0.006, lengths 0.554 and 0.095 vs 0.549 and 0.0995, tilt 15.09, ring 43.2 % vs 43.8 %); surface vs full path (99.8 % and 100 % within 0.5 px, 0.3 % one-sided).
- check_blackhole precision: 80 checks, 0 failed; the card's surface r within 1.9e-5 (0 over 3e-4), factor 5.6e-5. sky: 43 checks, 0 failed; spin 0.99 factor 6.9e-5; picture "streaks" 1 of 216,898 pixels over 8 levels.
- check.sh job: quick 18 of 18 passed, ui_smoke passed (first run: app_settings failed twice: the new scene's name over 90 characters, and the plan's 318 s limit; both fixed, below). check_sources.py: ok (689 lines, 42 extracts).

## Not done
- Streaks and clump on the CPU fallback (it draws the surface only), grazing rays that miss the plane near the surface, the spinning hole's thickness: ROADMAP.
- Not timed on a GPU: the card here is Mesa's CPU Vulkan. The owner's benchmark decides.

## References changed on purpose
- Patch tests removed with the patches (testGas part 5, testGasCard parts 1-2). Clump phase limit kept (2e-5), its float bound now smaller.
- Two new tests corrected before they first passed: the tilt's sign (my formula), and the mean of S limit 5e-3 is now 3 standard errors (its own sampling error was 2e-3).
- Surface: tries used 2 of 3 (the first, straight-line steps only, failed close in).
- test_app: the benchmark's planned time limit 318 s -> 330 s (one Hard scene and one wait added on purpose: 328.6 s); Hard 37 -> 38, All 53 -> 54.
