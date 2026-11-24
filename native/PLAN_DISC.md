# Plan: a realistic gas disc for the Black Hole Lab (0.78.1 research, 24 Nov 2026; no app code yet)
The owner (24 Nov 2026, DECISIONS.md): the gas disc is ugly in both looks, Film worst: flat like a sticker, too smooth and plastic, hard edges, cheap colour
and glow. He wants the most realistic option. Pictures: native/docs/blackhole/disc078_real_feeding.png, disc078_real_sgra.png, disc078_film.png (each with a
.txt). Made by native/tools/discproto (processor, double precision, the lab's own ray paths). Sources: section 8; all saved in the private sources repo.

## 0. In plain words (for the owner)
1. **The bright disc of a feeding hole really is almost flat.** At the lab's feeding rate (a tenth of the hole's limit) its thickness is under 2 % of its
   radius (section 2). Made thicker, it would be false. What makes it look like a sticker today is something else: it is perfectly smooth, its brightness
   changes only with distance, and its patches are round blobs.
2. **Real discs are streaked.** Magnetic turbulence in the gas makes long thin filaments, each tilted about 15 degrees from its orbit and wound into
   trailing spirals by the faster inner gas. That is what computer simulations of such discs show (section 4). The proposed disc draws those streaks in
   place of today's round patches. Where each streak is stays a labelled model; their shape, tilt and size come from the simulations.
3. **Edges.** The outer edge already fades by its own physics. The inner edge: the disc thins to nothing at its innermost orbit, and simulations put a
   little light (about 4 %) inside it. The proposed disc thins there instead of stopping at a sharp line.
4. **Sagittarius A* and M87* have no bright disc.** Their gas is a hot, thick, see-through fog. The lab can now show it from a published model whose
   numbers were fitted to Sagittarius A*'s measured radio spectrum. Seen close up it is about a six-hundredth of the Sun's surface brightness in visible
   light, nearly white, and strongest in a thin ring hugging the shadow, brighter on the side coming towards you (picture disc078_real_sgra.png). At the
   bright disc's exposure it would be black: the readout then says "shown N times brighter than real". Its visible light has never been measured (dust
   hides the centre of the Galaxy); the lab works it out from the model and checks it against the measured near-infrared light (section 3).
5. **Film** keeps every number from the film's paper (spin 0.6, 4500 K, edges 9.26 and 18.70, no shifts). It adds, labelled as the lab's own: fading edges,
   the same streaks, and a disc you can partly see through ("marginally optically thick" in the paper's words). The film's own textures and lens flare are
   not published, so they are not copied. No measured camera glow with usable numbers was found: the glow stays the lab's own (section 5).
- **Recommended version**: Real feeding disc: true thickness, the streaks, a thinning inner edge. Sagittarius A* and M87*: their hot flow at its true
  visible brightness (M87* after its model is sourced), the disc still one click away. Film: soft edges, streaks, partly see-through, labelled.
- Alternative A: as recommended, but the hot flows from a real simulation's saved data (the Illinois library, free to use with credit, section 4) in place of
  the fitted model: the most realistic gas structure, about 20 times more work and a large download.
- Alternative B: only the streaks and the soft edges (Real and Film); the presets keep "no disc". Smallest change, no hot flow.
- **Real**: the thickness, the flux, the colours, the shifts and bending, the hot flow's light from its fitted model, the streaks' shape and tilt.
  **Model, labelled**: where each streak is, its strength; the film's texture and see-through amount; the glow.

## 1. What was read (the code, by grep and line ranges) and how a ray meets the disc today
- Docs: CLAUDE.md, native/REPORT.md, ROADMAP.md (Black Hole Lab), DECISIONS.md 68-96, native/PLAN_BLACKHOLE.md, native/PLAN_MOVINGGAS.md.
- Code: src/blackhole/disc.h (all), gas.h 1-119, looks.h (all), light.h 1-40, lab.h 20-193, lab.cpp 40-140 and 221-245, tone.h (all), geodesic.h (all),
  kerr_closed.h 55-91, presets.h (all), blackhole_view.cpp 320-345 and 1891-1906, spin_sky.h 13-54, spin.cpp 116-120; src/render/kerr_compute.h (all),
  kerr_sky.h (all), shaders.h 510-528, 740-750, 1474, 1625-1673, 1760-1770, 1923-2131, 2305-2471 (by grep); tools/movinggas (via PLAN_MOVINGGAS.md);
  tests/blackhole_picture.cpp 1-60 and 234-330; render/cie1931_data.inc; CMakeLists.txt 176-259.
- **Today a ray meets the disc only where it crosses the equatorial plane**, at most four crossings (kDiscOrders, kSpinCross = 4), the first with
  r_in <= r <= r_out shown (opaque), the next ones laid behind with coverage. Spin 0: kLensFS reads the processor's table DiscTable (u = 1/r along every ray:
  rows by angle from the shadow's edge, 256 columns along the swept angle, plus Phi and the travel time), crossings at phi_0 + k pi. Spin > 0: kKerrSkyFS
  computes per pixel the closed forms (kerr_closed.h traceClosed: u, and phi, t at the timed crossings); the fallback kKerrFS reads SpinTable (far
  direction, b, u at four crossings), whose card fill is kKerrTableCS. Light per crossing: Page-Thorne flux, g (C75 (A9)), black body, gas.h's factor.
- **What the card keeps of a ray**: nothing between frames; per pixel it holds the far direction, b, and per crossing u (and phi, t when timed). The whole
  path exists only in the processor's reference (geodesic.h trace, every accepted state when asked). Spin 0 is the exception that helps: its table already
  holds u along the whole ray (256 columns), so a sample anywhere along a ray is a table look-up.

## 2. The thin bright disc (the feeding preset), its true thickness and its edges
- Model kept: Novikov-Thorne / Page-Thorne flux (disc.h), black body per face, 10^8 Suns, 0.1 L_Edd. Its thickness (new): the one-zone vertical structure of
  Zhu et al. 2012 App. B: (B1a) P_tot / h = rho Q h, (B1b) P_rad = F_tot kappa Sigma / (4c), (B1f) Sigma = 2 h rho, with Q = Omega_k^2 R_z, (5)-(7),
  R_z = (u_phi^2 + a*^2 (u_t - 1)) / r as printed in (6) (the spin term is checked against Abramowicz et al. 1997 before spin is coded; spin 0 needs only
  u_phi^2). Where radiation pressure dominates: **h = kappa F / (2 c Q)** (computed here); with L = eta Mdot c^2 = l L_Edd, L_Edd = 4 pi G M c / kappa:
  h / M = 2 pi (l / eta) f(r) / Q(r), f = disc.h discFlux. The factor 1/2 against the textbook kappa F / (c Omega^2) is Zhu's own (their note under (B1b)).
- Numbers (tools/discproto real): largest h/R = 0.0177 at R = 20.5 M; h = 0.104, 0.353, 0.654, 0.962, 1.197 M at 10, 20, 50, 200, 2000 M; far out
  h -> (3/4)(l / eta) M = 1.31 M. Independent check: Zhu Fig. 3, TLUSTY at L/L_Edd = 0.1: about 0.02 near 20 M (read from the plot; their h is the
  density-weighted mean |z| with gas pressure included). Where gas pressure wins (AF (98) beta/(1-beta) = 1: about 800 M at 10^8 Suns, alpha = 0.1, computed
  here) h grows again (AF (98)); the prototype leaves that out and the build job adds it.
- **Found**: AF (99) prints the inner region's H = [1 x 10^5 cm] (mdot) A^2 B^-3 C^1/2 D^-1 E^-1 Q with no power of m, while (97), (98) carry m^(18/20) and
  m^(9/10). The textbook form above scales as M (H ~ (3/2) mdot GM/c^2 far out). REPORT.md records it; the lab uses Zhu's equations, not AF (99).
- The surface: opaque at |z| = h (the one-zone density is uniform in z; the photosphere is drawn at h, a labelled simplification). The gas there moves as
  at the plane (h/R < 0.02). Light: the same flux per face. Limb darkening stays the existing ROADMAP line.
- **Edges**: inner: h -> 0 and F -> 0 at r_ms (PT's zero torque), so the disc thins smoothly into the ISCO. Light from inside the ISCO: "about 4 per cent of
  the total" in Penna et al. 2010's simulations (Zhu et al. 2012 sect. 1); Zhu Fig. 4 shows T_eff continuing inside the ISCO at 0.3-0.7 L_Edd. **source
  needed**: a T_eff(r) inside the ISCO with numbers at 0.1 L_Edd (Zhu's Table/figures are at 0.32-0.71 L_Edd); until then nothing is drawn inside, and the
  thinning carries the edge. Outer: 2000 M (Goodman 2003, unchanged); T falls as r^-3/4, so it fades by itself (0.58.0).
- What this changes in the picture: very little directly (disc078_real_feeding.png): the inner rim is a little fuller, the far limb slightly thicker. The
  owner's "flat" is mostly the missing structure (section 4). At 0.3 L_Edd (the model's top, kThinMax) h is 3 times larger.

## 3. Sagittarius A* and M87*: the hot, thick, see-through flow, and what an eye would see
- Kind of gas: a radiatively inefficient flow, geometrically thick, optically thin above about 10^3 GHz (BL06 sect. 3, after Broderick & Loeb 2005), electrons
  at 10^11-10^12 K (PLAN_BLACKHOLE.md section 3).
- **Model for Sagittarius A***: BL06 sect. 2.2 (4) and Table 1 (fitted to the radio spectrum for a = 0, 0.5, 0.998): n_e = n0 (rho/M)^-1.1 exp(-z^2/2rho^2),
  T_e = T0 (r/M)^-0.84, n_nth = n0nth (rho/M)^pnth exp(-z^2/2rho^2); a = 0: n0 = 3e7 cm^-3, T0 = 1.7e11 K, n0nth = 8e4 cm^-3, pnth = -2.9; non-thermal
  spectral index 1.25, gamma_min 100; B at 30 % of equipartition with the protons, toroidal; Keplerian outside the ISCO, free fall inside. Its thickness:
  a Gaussian of width rho in z (H/R = 1). Broderick et al. 2011 (4) writes the equipartition energy as n m_p c^2 r_S / (12 r); the prototype reads BL06's
  30 % as beta^-1 = 0.3 there (B = 36 G at 6 M, BL06 (6)'s "30 G" scale). **Check before use** (build job, test written first): the model's 1.3 mm flux at Earth
  against Broderick et al. 2011 sect. 3.1 ("typical 1.3 mm flux of ~3 Jy"; their images normalised to 2.5 Jy), with thermal absorption by Kirchhoff's law
  (Pandya (25)). Limits said in the app: a fitted, smooth, self-similar model; no outflow or jet; spin 0, 0.5, 0.998 only (Table 1).
- **Its light**: synchrotron. Pandya et al. 2016 (24) thermal (Leung et al. 2011's fit, K_2(1/Theta_e)), (26) power law, (27) its absorption; along each ray
  I_obs(nu) = Int g^2 j(nu/g) dlambda (I/nu^3 invariant). Prototype results (tools/discproto sgra, 550 nm): thermal light at most 8e-12 of the power-law
  light; optical depth at most 5e-13; brightest point 0.00179 x the Sun's surface in visible light (about 1/560); colour of I ~ nu^-1.25: CIE x 0.3135,
  y 0.3165 (white, faint pink). The shift changes only its brightness (a power law stays a power law).
- **Check against a measurement**: the picture's 2.2 um flux moved to Earth by (r_cam / D)^2: 0.195 mJy. GRAVITY 2020 Table 1 (dereddened, A_Ks = 2.43):
  median 1.1 +- 0.3 mJy, 5 % percentile 0.28 +- 0.07, 95 % 6.9 +- 3.8. The model sits at the faint end. The build job computes the flux for a far observer
  (not the camera's view) and tests it against GRAVITY's 5-95 % range.
- **What an eye would see, honestly**: no visible-light measurement exists. The measured near-infrared slope differs from the model's: nu F_nu ~ nu^0.5 in
  flares, ~ nu^-2 when quiet (GRAVITY 2020 sect. 5 item 4, after Gillessen et al. 2006). The colour then ranges (computed here, CIE 1931): flares x 0.2945,
  y 0.2985 (bluish white); model x 0.3135, y 0.3165; quiet x 0.3605, y 0.3532 (pale orange). The app shows the model's colour and says the range.
- **The readout**: today "shown N times dimmer than real". The hot flow is drawn brighter than real at the lab's usual exposure: the readout says "shown N times
  brighter than real" when N < 1 (the prototype's right panel: white at 0.00195 x the Sun's surface). A dim flow beside a lit disc is shown on one exposure
  (the middle panel: black).
- **M87***: **source needed**: a published semi-analytic model of M87*'s flow with numbers fitted to its spectrum (Broderick & Loeb 2009, ApJ 697, 1164, is a
  candidate to fetch and read). Until then the preset keeps "no disc" with today's sentence, or Alternative A's simulation data.

- **0.80.0, the check done (REPORT 0.80.0; DECISIONS 24 Nov 2026)**: BL06 as printed passes the 1.3 mm check (3 Jy +- 30 %) and GRAVITY's 2.2 um range under
  no sourced reading (lengths in M or r_S, 0.3 of the energy or of the field), so nothing is drawn. The visible brightness above (1/560 of the Sun's surface)
  and the prototype's 2.2 um flux rest on the unchecked reading; Pandya (27) as printed is 0.563 of the true absorption (hot.cpp). The GPU table (section 6),
  the Hard scene and the guide text wait for a model that passes both checks (ROADMAP).

## 4. Fine structure (replaces gas.h's patches)
- **What simulations show**: shearing-box MRI turbulence (Guan et al. 2009, zero and net azimuthal field): two-point correlation functions are tilted
  ellipses, trailing; the magnetic field's major axis about 15 degrees from the azimuth (Table 2, converged runs y128a-y256b: 15-17 degrees), density and
  velocity about 6-7 degrees (Table 1); lengths in units of the scale height H: lambda_B,maj 0.27-0.33 H, lambda_B,min 0.049-0.058 H (Table 2, y128a-y256b).
  Global GR simulation (SKH06): bright patches about 25 degrees by dR/R = 0.3, living about 0.3 orbits, sheared into arcs; the 6-10 M ring's light varies by
  43.8 % rms (Table 2). Lee & Gammie 2021: disc images as an anisotropic Gaussian random field, spectrum (1 + k.Lambda k)^-2 ((13), a Matern field
  approximating Guan's), spirals at 20 degrees "consistent with local model results (Guan et al. 2009)", lambda proportional to r, brightness
  mu = g(r) exp(f/n) ((15)), correlation time lambda_0 = 2 pi / Omega_K; their code inoisy is public (github.com/AFD-Illinois/inoisy; its licence to be read
  before any use; the lab writes its own field and copies no code).
- **Proposed model (labelled)**: S = exp(c f - c^2/2) (mean exactly 1: light moved, not added), f of unit variance in (ln R, phi) (lambda ~ R makes it
  homogeneous there), Lee & Gammie's spectrum, major axis 15 degrees trailing (Guan), major length 25 degrees x R (SKH06's patch size), minor 0.18 x the
  major (Guan y256a). Time: f advected at Omega(R) and renewed: two or three copies of one field texture at different offsets, each faded in and out over
  SKH06's life 0.3 T_orb, so the winding never runs away. c calibrated so the face-on 6-10 M ring varies by SKH06's 43.8 % rms (as gas.h c was).
  **Settled (0.79.0)**: SKH06's radial patch size (0.3 R) against Guan's aspect ratio (minor 0.18 of major): the field's full width at half correlation along
  ln R measures 0.197 (tests/test_blackhole.cpp testStreaks), against SKH06's 0.3. Guan's is kept: it comes from converged shearing-box runs (resolution up to
  256 cells per scale height, their Table 2), SKH06's from one global run whose patches are brightness features at a coarser grid; the owner chose streaks. The clump (BL06 (5), GRAVITY's flares) stays as today, labelled.
- **Hot flows**: the same kind of field in 3-D (Lee & Gammie's (9)), strength **source needed**: measured from one snapshot of the public GRMHD data
  (Dhruv et al. 2025, Illinois v3 library: 10 MAD/SANE runs, spins -0.94 to 0.94, 288x128x128, 30,000 t_g; "Licensed under the Creative Commons Attribution
  4.0 International License" on thz.astro.illinois.edu, saved 24 Nov 2026). Until then the hot flow is drawn smooth (disc078_real_sgra.png).
- Statistics the tests check (section 7): variance 1, mean of S 1, the measured correlation lengths and tilt of the generated field against the inputs,
  the ring's rms against SKH06.

## 5. Film and the glow
- **Stay (J15, looks.h)**: spin 0.6, T = 4500 K, edges 9.26 and 18.70 M, no colour or brightness shift, white balance at 6500 K, Figure 13's view button.
- **J15 on the film's disc** (App. A.6 p. 42-43): a thin-disc image sampled "at each intersection" with the beam attenuated "by the optical thickness"; a
  volumetric disc "built by an artist using SideFX Houdini ... roughly 17 million voxels" with optical density and colour per voxel; close-ups with Mantra's
  procedural textures ("such as fractal noise"). Section 4.1.1: "physically thin and marginally optically thick". No texture, density or voxel numbers
  are published: none is copied.
- **Added, labelled "the lab's own, not the film's"**: (1) edges fading over 0.12 in ln r inside J15's edges (prototype choice; the build shows 0.06, 0.12,
  0.2 to the owner if asked; 0.81.0 kept 0.12, docs/blackhole/disc081_film.txt); (2) the same streak field as Real (section 4); (3) partial transparency: each crossing covers 1 - exp(-tau), tau = 1.5 x S x
  edge (prototype choice standing for "marginally optically thick"), crossings laid front to back. disc078_film.png.
- **Glow**: Real keeps the eye's glare (light.h, CIE disability glare). For Film, a published camera spread: Talvala et al. 2007 measured glare spread
  functions of three cameras but publishes them as plots only (one number: the Canon 20D's uniform glare 20 stops below the point source). J15's IMAX
  point-spread function is not published (section 4.3.1). **source needed**: a lens or camera spread function with tabulated values and terms that allow
  use. Until then Film keeps the lab's own blur (looks.h), labelled as now.

## 6. GPU plan and cost (the owner's PC: RX 9060 XT, 2560 x 1369, 255 Hz)
- Budget: 2560 x 1369 = 3,504,640 pixels x 255 = 893.7 million pixels a second. AMD's page (saved): 32 compute units, 2048 stream processors, peak FP32 vector
  25.6 TFLOPs: at most 28,600 single-precision operations per pixel per frame at peak, for everything. Operation counts below are estimates (marked), to be
  replaced by the build jobs' measurements.
- Measured on the build machine (tools/discproto count, one core, double): the lab's integrator takes 444 steps a ray (rtol 1e-11) and 0.127 ms a ray;
  one hot-flow emissivity sample (power law) 77 ns. Integrating every ray per frame is out (way (a) of PLAN_BLACKHOLE.md section 6, rejected then too).
- **Thin disc (per frame, cheap)**: at each crossing the card already has r (and phi, t when timed). New per crossing: the surface offset from the
  crossing's direction (dz/ds from p_theta, the step back h / |dz/ds|, one more table or closed-form evaluation; tested against the processor's full-path
  search of tools/discproto) and three look-ups of one field texture (2048 x 1024, 16-bit: 4 MB) for the streaks. Estimate: under 100 operations a
  crossing, under 1 % of the budget. Made once: the field texture (at start-up; the processor's FFT reference), h(r) as a 1-D table per feeding rate.
- **Hot flow (once when the view changes, then per frame cheap)**: the flow's mean light is still, so per camera distance and tilt one table I(x, chi)
  (angle from the shadow's edge, azimuth) holds the integral along each ray, plus its light-weighted centre (r, phi) for the moving structure. Spin 0: the
  rays from the existing DiscTable (u along each ray), 768 rows x 360 azimuths x 64 samples = 17.7 million samples a refill: on the processor about
  17.7e6 x 77 ns / 4 cores = 0.34 s (computed from the measurement; a response line in waitDefs), on the card an estimated few ms. Per frame: one table
  look-up and one structure look-up per pixel. The structure applied at the light-weighted centre is an approximation: tested against the full march (rms
  difference stated). Turning the view (yaw) is free; distance or tilt refills the table (the picture stays sharp: the previous table is used until the new
  one is done, as the spin table today; DECISIONS 17 Nov: no lower resolution while moving).
- Alternative per frame: march 64 samples per pixel (estimate 100 operations a sample: 6,400 a pixel, about 22 % of peak) on top of today's sky pass,
  whose Hard scenes already run at 152-172 Hz (1 % low) in the owner's 0.68.1 run: not recommended.
- **Spin > 0 hot flow**: the same table from closed-form states (kerr_closed.h closedStateAt) or from the processor (spin_sky.h's machinery). If its card fill
  reuses the kKerrTableCS structure, it waits for that crash fix (ROADMAP **fix**, kKerrTableCS "step" part on the owner's Radeon). **The recommended work
  does not depend on kKerrTableCS**: spin 0 uses DiscTable and kLensFS; the thin disc uses kKerrSkyFS and kLensFS; spin > 0 hot flow is filled on the
  processor until the crash is fixed.
- Benchmark scenes (Hard list, app/bench_scenes.cpp): blackhole.disc_streaks (feeding preset, camera turning at the closest distance, the streaks moving at
  the top rate, Real), blackhole.hot_flow (Sagittarius A*, the flow on, camera moving in and out), blackhole.film_soft (Film, the paper's view, turning).
  Response line: "hot flow table" (the refill after a distance change).

## 7. Tests (written first in each build job; each must fail on the old build)
| Part | Independent reference |
|---|---|
| thickness h(r) | far-out limit h -> (3/4)(l/eta) M from (B1) with Newtonian Q = 1/r^3 (algebra); h = 0 at r_ms (PT's zero torque); h/R near 20 M within the reading of Zhu Fig. 3 (0.1 L_Edd curve, stated as a plot reading); h ~ M at fixed r/M (scaling) |
| surface hit | the card's crossing-plus-offset against tools/discproto's full-path search (double), in pixels |
| streak field | unit variance; mean of S = 1 (exact lognormal mean); measured correlation lengths and tilt of the generated field against the inputs (Guan's 15 degrees); the 6-10 M ring's rms with the field moving against SKH06 Table 2's 43.8 % (within the plan's 20 %, as gas.h) |
| hot flow emissivity | Pandya (25) Kirchhoff for the thermal part (j = alpha B_nu); the power law's slope -(p-1)/2 and its alpha slope -(p+2)/2 (26), (27); BL06's B scale ("30 G" at the ISCO, (6)) |
| hot flow spectrum | the model's 1.3 mm flux at Earth against Broderick et al. 2011's ~3 Jy (2.5 Jy normalisation); its 2.2 um flux against GRAVITY 2020's 5-95 % range (0.28-6.9 mJy) |
| transfer | I/nu^3 invariant: a flat-space slab of known j gives j L (closed form); a ray far from the hole gives g -> Doppler factor of special relativity |
| colours | the power law's CIE chromaticity at alpha = 0.5, 1.25, 3 (the three numbers of section 3) |
| GPU against CPU | the thin disc's surface, streaks and the hot-flow table within float rounding of the double reference, counted (DECISIONS "Tests and precision") |

## 8. Sources (all saved in ShelaShelby/physics-lab-sources, data/blackhole/; native/data/SOURCES.md lines of 0.78.1)
- Zhu, Davis, Narayan, Kulkarni, Penna & McClintock 2012, MNRAS 424, 2504, arXiv:1202.1530 (vertical structure, plunging-region light).
- Broderick & Loeb 2006 (BL06, saved since 0.56.3); Broderick, Fish, Doeleman & Loeb 2011, ApJ 735, 110, arXiv:1011.2770 (the RIAF's equipartition (4)).
- Pandya, Zhang, Chandra & Gammie 2016, ApJ 822, 34, arXiv:1602.08749 (synchrotron emissivities).
- GRAVITY Collaboration 2020, A&A 638, A2, arXiv:2004.07185 (Sagittarius A*'s 2.2 um flux distribution and slope).
- Guan, Gammie, Simon & Johnson 2009, ApJ 694, 1010, arXiv:0901.0273; Lee & Gammie 2021, ApJ 906, 39, arXiv:2011.07151; Schnittman, Krolik & Hawley 2006 (saved).
- Dhruv et al. 2025, ApJS 277, 16, arXiv:2411.12647, and the Illinois data pages (CC BY 4.0).
- Abramowicz & Fragile 2013 (saved; (97)-(99)); James et al. 2015 (saved; App. A.6); Talvala, Adams, Horowitz & Levoy 2007 (ACM TOG 26(3)); AMD's RX 9060 XT page.

## 9. Build steps (the owner's order of 24 Nov 2026, DECISIONS.md; each its own job; tests first)
| Version | Step | The user sees | State |
|---|---|---|---|
| 0.79.0 | D1's thin-disc parts + D2: the thickness and the surface (disc.h), the streak field (gas.h, replacing the patches), on the card from the start | streaks in place of patches (Real and Film), the true thickness and the thinning inner edge (Real, spin 0), guide text; Hard scene blackhole.disc_streaks; response line response.blackhole_streaks | **done** (REPORT 0.79.0) |
| 0.80.0 | hot-flow physics, tested; nothing drawn: hot.h (the RIAF, emissivities, transfer, colours), the 1.3 mm and 2.2 um checks of every sourced reading | no change: Sagittarius A* keeps today's picture | **done** (REPORT 0.80.0) |
| 0.81.0 | D4: Film | soft edges, streaks, partly see-through, labelled; Film better than disc078_film.png; Hard scene blackhole.film_soft | **done** (REPORT 0.81.0; docs/blackhole/disc081_film.txt) |
| 0.82.0 | the camera round the black hole (as the Orbit Lab's round a planet) | planned with the owner first | next |
D5 (the Illinois data for the hot flow's structure, M87*'s model) is not chosen (24 Nov 2026).
0.79.0's findings: the thickness uses Zhu's own opacity and particle weight (0.3383 cm^2/g, 0.615 m_H) with the lab's L_Edd (0.4 cm^2/g), so h is 0.846 of
section 2's numbers (largest h/R 0.0150); the gas's pressure is in (Zhu (B1e)), half the total from 567 M at 10^8 Suns (alpha 0.1, a labelled choice), so far
out h grows (5.85 M at 2000 M against 1.01 M from radiation alone). Section 4's open point is settled below. The streaks' strength c = 1.3 (SKH06's 43.8 %).

## 10. Questions for the owner (answered 24 Nov 2026: DECISIONS.md, Black Hole Lab)
1. The recommended version (section 0), or Alternative A (real simulation data for the hot flows) or B (streaks and edges only)? *Recommended: the recommended version.*
2. At the lab's feeding rate the true disc is under 2 % thick and still looks flat from most views. Keep the true thickness (recommended), or start the
   feeding preset at 0.3 L_Edd (the model's top, 3 times thicker and hotter)?
3. Sagittarius A*'s colour is not measured. Show the model's colour (near white) and name the range in the guide (recommended), or a switch "quiet / flare"?
4. The streaks replace 0.72's round patches in Real and Film; the bright clump stays (labelled). *Recommended: yes.*
5. Film's glow: keep the lab's own blur (recommended, labelled), or use the eye's measured glare there too (an eye, not a camera)?
