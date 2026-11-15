# 0.55.2: the spinning black hole (plan step 3), two pushes (checkpoint 1050a11 with the physics, then this)
## What the user gets
- Controls › Spin: 0 to 0.99 of the largest spin (slider, typed value; key `blackhole.spin`); 0 is the 0.55.1 hole to the pixel (closed forms). The shadow turns D-shaped (flat on the side
  moving towards the camera, PeT Table II), the sky near it is dragged round, the disc reaches in to r_ms(a) and is hotter there. Coarse table at once while spin, distance or latitude
  change, the fine one 0.3 s after they rest (own thread, all cores but one); turning round the axis never refills. Never a picture from another setting's table.
- Card: spin, horizon (GM/c², km), shadow wide × high, "Dragged round … once every N" (2π/ω, J15 (A.2)), innermost stable orbit and the share of the infalling mass turned into light; for
  Sgr A*/M87* the shadow from Earth at that spin (GLb (55)). Presets start at 0 with "Spin not measured" and what the EHT papers say (M87-I §6: a*=0 disfavoured by the jet power, spin
  away from Earth; Sgr-I abstract, §6: non-spinning disfavoured, the two passing models a* = 0.5 and 0.94). Most realistic leaves spin alone. Guide: a part "Spin" with every limit the
  job lists; About: the spin uses of J15, BPT, GLb, PeT, C75. "Show special radii": not done (ROADMAP line).
## The Cunningham question (ROADMAP fix): explained at the first try
C75 finds the rays from a ring by solving (A5) for (λ, q). For an observer exactly in the plane (θo = π/2, "idealized", footnote 2: such an observer "is in a shadow") those are the q → 0+
rays whose θ-motion makes half an oscillation over the plane, ∫R^-½ dr = π/(λ²−a²)^½, not the rays along the plane (ours 1.89 at 89.9°). With C75's family and (A9): 1.73 at C75's
tabulated r = 6.25 M next to r_ms (1.79 at r_ms) → "1.7"; 15.18 at a = 0.9981 → "15"; Fig. 1a's equatorial curve too (1.03, 1.10, 1.25 at x = 4, 6, 8). Both ratios are tests now.
## Module (src/blackhole/spin.h, spin_sky.h; geodesic.h: moving camera, plane crossings) and blackhole_physics (726 checks, 112 new)
- J15 (A.5), (A.6) = BPT (2.18), GLb (38)-(40) to 1e-14 (a ≥ 0.1; at 1e-6 b²+q = 27 to 1e-5); trapped orbits double roots of R (GLb (37)).
- J15's closed test (v) = the integrator for 2,304 rays (a 0.5-0.999, r 2.5-1000 M, four latitudes, ±1e-2..1e-6 rad round the edge): 0 wrong, 0 lost; the edge is crossed once along every
  azimuth (64 cameras × 24). a → 0: Synge's circle to 7e-7 rad. 1e6 M away: the edge = GLb's curve (55) to 1.0e-6 (= M/r). PeT Tables III, II via GLb: 9.000 × 10.392 M and α −2.000..7.000
  (a = M, equator), 6√3 up-down at any spin, 9.657 along the axis; flat side 2.25 against 6.98 M (a 0.99, equator); round to 1e-3 from 1e-3 rad off the axis.
- Further images thinner by e^-γ (GLb (42), (43)), from the integrator's equator crossings at a 0.5, 0.9 and 60°, 90°: within 0.6 %; γ → π at a → 0 (GLb (72)).
- Gas: C75 (A9) = u·u = −1 in J15's metric to 1e-14, −u_t = BPT (2.12), a = 0 = L79 (19). At a 0.9: inner edge BPT (2.21); hottest ring 9.55 → 3.44 M, 70,100 → 121,800 K; light =
  efficiency × feeding rate to 1e-7. Camera: aberrate = J15 (A.9)+(A.10) written out = special-relativity aberration (1e-14); a static camera (β = −ωϖ/α) = its own frame built from
  the metric (4e-16); (A.16) at β = 0 = the FIDO's shift. Mirrors north-south and spin reversal 6e-11 (direction, b), 1.4e-6 (u). Spin 1e-6 = a = 0's closed forms (1e-6).
- Fine table: 192 azimuths (spaced by a probe of the near-edge far direction) × 384 columns per side (s = ln|x| + |x|/0.3, |x| ≥ 1e-8). Against the integrator: far direction 2e-3 rad
  from 1e-3 ψe outward, 2e-2 nearer; disc r, g 5e-3 (direct and second image beyond 1e-2 rad), 5e-2 further in; b 1e-4; edge cubic = closed test to 1e-11 rad. A crossing a ray does
  not make is continued smoothly (none of those values lies on the disc), so the cubic does not ring where an image starts. Stars by inverting the table: magnification = the integrator's
  neighbouring rays to 0.2 %; a uniform sky stays uniform (summed magnifications 0.9827 against 0.9856).
## The spin range ends at 0.99 because the tables' accuracy ends there: at 0.999 the far direction near the shadow's flat side changes too fast from row to row (2 rad errors close to the edge with 192 rows). J15 uses 0.999 and C75 0.9981, above our top.
## Picture test (check_blackhole precision, PL_BH_COMPARE; Mesa llvmpipe, 4 cores) and measurements
- Spin 0.5 and 0.99 × 20 and 4 M × near the plane and near the axis: the card's edge within 4.4e-4 px of the closed test, none on the wrong side; far direction = the double lookup within
  1.2e-3 rad a pixel or more from the edge; disc: no pixel of another image away from image edges, those edges moved ≤ 0.023 px, r 1.3e-4, g 6e-5. Table against the integrator every
  16th pixel: far direction ≤ 1.1e-3 (near), 4.8e-4, 2.3e-4 rad; disc r/g 2.5 %/0.3 % at 0.5 and 9.9 %/1.9 % at 0.99 near the plane (rays grazing the disc from close above it, where r
  changes slowly across the picture: 0.4 % of r is a 3 px shift there), ≤ 3.2e-4 near the axis. A camera inside the disc's radius and close above it sees the disc below (correct).
- Fills at 20 M: coarse 4,096 rays 80-95 ms (4 cores), 309-353 ms (one); fine 147,456 rays 6.3 s (a 0.01) and 7.8 s (0.99) on 4 cores, 24.5 and 28.7 s on one. Card pass at 0.9:
  47 ms sky / 60 ms disc at 1280×800, 387 / 461 at 3840×2160 (a = 0: 24/57, 186/388). Stars: binning the table 101 ms per new table; a turn (new star images) 77 ms. Benchmark scene 13: now spin 0.9.
- docs/blackhole/reference_spin099_r60.png (+ .txt): the feeding preset at 0.99 from 60 M, 10° above the disc, 1280×800, 1.66 MB.
## Differences from the plan and the job
- The card's shadow test: J15's closed test runs in double on the processor at 2,048 azimuths and the card interpolates that edge (4.4e-4 px from the test per pixel), not the formula
  per pixel (its case (c) needs a quartic's largest root). C75 Table 1 is f, not g; its g are a graph: only the two printed ratios are references. Readout "your clock" at spin: the FIDO's lapse.
- check.sh quick (PL_SOURCES_DIR set): 17 of 17 (again after rewriting C75's family without nested generic lambdas: they crashed the Windows compiler on the checkpoint). By hand: check_blackhole spin (new, 19), disc (23), precision (67; 8 spin cases new), layout (12; its section list has "spin":
  reference changed on purpose). Full UI suite not run (small version). Looks: equator sky, equator disc (60 M), near the axis, close in.
