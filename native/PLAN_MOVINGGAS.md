Steps 1-3 built (0.56.4: the processor's physics; 0.72.0: the card, the lab, the guide). True at 0.72.0. What is left: section 7 and ROADMAP (Black Hole Lab, Moving gas).
0.78.1: the owner rejected the look (24 Nov 2026). 0.79.0: section 5's patches are replaced by the streaks of native/PLAN_DISC.md section 4 (gas.h StreakField); the clocks, the travel time and the clump stay. The sections below are the record of 0.56-0.78.

# Plan: moving gas in the Black Hole Lab (0.56.3 research, 17 Nov 2026; no app code yet)
Owner's choice (17 Nov 2026, DECISIONS.md): something must happen; moving gas before flying the camera. Pictures: native/docs/blackhole/moving_clump_strip.png,
moving_clump_lightcurve.png, disc_structure_options.png (each with a .txt). Numbers: native/tools/movinggas (`--table`, `--check`, `--render`).

## 0. In plain words (for the owner)
1. **The gas turns at its true speed.** Near the hole it moves at half the speed of light or more. Round Sagittarius A* one turn of the innermost gas takes
   about half an hour; round M87* about a month; round a 10-Sun hole a few thousandths of a second. A time control sets how fast the film runs.
2. **A bright clump circles the hole.** Behind the hole its light comes over the top, and a second, smaller image of it slides along the thin ring under the hole.
3. **Two clocks**: "far away" and "on the clump". The clump's clock runs slow: at Sagittarius A*'s innermost orbit, 23 minutes pass on the clump for 33 far away.
4. **Light takes time.** Each part of the picture shows the gas as it was when its light left: the far side and the second image are older than the near side.
   This changes what you see (the clump's flash is sharper), so the app keeps it exactly.
5. **The disc gets structure**: bright and dark patches that each ring carries round at its own speed, so they stretch into spirals, as real gas does.
6. Nobody has seen the face of such a disc: the patches come from a computer simulation of a magnetised disc and are labelled as a model. The clump, too, is
   the model astronomers fit to their flare data, labelled.
- **Recommended version**: Real and Film both get the moving disc pattern and the clump, a time control (pause, real time, faster or slower), the two clocks,
  and the exact light travel time per pixel.
- Alternative A: the clump only, on today's smooth disc (half the work; no structure).
- Alternative B: as recommended, but Film stays as today (still and smooth): the film's paper says nothing about the disc moving.
- **Real**: the speeds, the clocks, the bending, the colour and brightness shifts, the travel time, the shearing at the true speeds.
  **Model**: where the patches and the clump are, their size, brightness and lifetime (taken from named papers, said in the app).

## 1. True speeds and times
- Orbit: Omega = M^(1/2) / (r^(3/2) + a M^(1/2)) (BPT (2.16), disc.h orbitOmega); period far away T = 2 pi / Omega. Speed seen by the local non-rotating observer
  V = (varpi / alpha)(Omega - omega) (C75 (A7c) = J15 (A.7)). One M of time = GM/c^3: 10 Suns 0.0493 ms, Sagittarius A* (4.297e6) 21.2 s, M87* (6.5e9) 8.89 h.

| spin | r (M) | V (c) | T (M) | gas clock / far | 10 Suns | Sgr A* | M87* |
|---|---|---|---|---|---|---|---|
| 0 | 6 (r_ms) | 0.500 | 92.34 | 0.7071 | 4.55 ms | 32.6 min | 34.2 days |
| 0 | 10 | 0.354 | 198.7 | 0.8367 | 9.79 ms | 70.1 min | 73.6 days |
| 0 | 20 | 0.236 | 562.0 | 0.9220 | 27.7 ms | 3.3 h | 208 days |
| 0.9 | 2.321 (r_ms) | 0.625 | 27.87 | 0.3709 | 1.37 ms | 9.83 min | 10.3 days |
| 0.9 | 6 | 0.417 | 98.00 | 0.7434 | 4.83 ms | 34.6 min | 36.3 days |
| 0.9 | 10 | 0.325 | 204.4 | 0.8459 | 10.1 ms | 72.1 min | 75.7 days |
- Checks (movinggas --check, a quick test): Kepler's law in SI far out (1e-8); GRAVITY 2018 p. 2: "4 million solar masses near the ISCO ... an orbital period of
  31 minutes" (here 30.3 min at 4.0e6, 31.4 at their 4.14e6: their rounding); p. 3: R ~ 7 +- 0.5 Rg, "P = 40+-8 minutes" (here 39.5 min); Wielgus 2022 sect. 3:
  t_g = 20.46 s and 74 +- 6 min = 10.0-11.2 r_g (a = 0), 9.8-11.0 (a = 1) (here 67.8-80.3 and 67.9-80.3 min).

## 2. The two clocks
- Far away: Boyer-Lindquist t (the hole's time; what the period above uses). On the gas: dtau/dt = 1/u^t = alpha (1 - V^2)^(1/2) (C75 (A7a)); tested against
  the normalisation in J15 (A.1) (1e-14) and -u_t against BPT (2.12)'s E (2e-15). The hovering camera: dtau/dt = alpha (J15 (A.1)-(A.3); kerr.h hoverShift(b = 0) = 1/alpha):
  0.983 at 60 M, so its clock and the far one differ by 1.7 %: shown as a third line only when the camera is close (below about 10 M).
- Example: Sagittarius A*, spin 0, clump at 6 M: one orbit = 32.6 min far away, 23.0 min on the clump's own clock.
- Recommended display: "Far away: 12 min 03 s" and "On the clump: 8 min 31 s" (with the camera's clock as a third line when it differs by over 1 %).

## 3. Light travel time
- Per ray: t_o - t_s = I_t + a^2 G_t (GLa (12)) with (13c), (13f); written per affine step dt/dzeta = [(r^2 + a^2) P / Delta + a (b - a sin^2 theta)] / rho^2
  (the same integrand; tools/movinggas adds it to the app's integrator). Checks: GLb (72), successive images near the edge at a = 0 delayed by 3 sqrt(3) pi M
  = 16.3242 M (here 16.3242); radial light at a = 0 against r1 - r2 + 2 ln((r1 - 2)/(r2 - 2)) (1e-9).
- Closed forms exist for every case: GLa (B3) I_t from I0, I1, I2, I+- ((B17)-(B18), (B37)-(B38), (B68)-(B69), (B98)-(B99)) and G_t (31), (48), (58), (68). New
  pieces for the card: E(phi|k) (Carlson R_D = R_J(x, y, z, z), already written) and the azimuth at the crossing, I_phi + lambda G_phi (11), (B2), at the crossing radius.
  I2 grows without bound at infinity, but the lab's camera is at a finite radius: only differences between the camera and the crossing are used (finite).
- Size (spin 0.6, camera 60 M, 10 degrees above the disc, clump at 6 M, orbit 96.1 M): across the direct image of the r = 6 M ring the travel time spans
  16.5 M = 0.17 orbit; the secondary image shows the clump 3.0 to 33.1 M (0.03 to 0.34 orbit) further in the past than the direct one. Visible: the clump's
  flash peaks at 4.4 times its mean with it and 3.1 without (the same mean; moving_clump_lightcurve.png). **Recommended: the exact delay per pixel** (not ignored).

## 4. The bright clump
- Observed: GRAVITY 2018: three flares of Sgr A* in the near infrared whose centres move in loops, one common orbit "R = 6 - 10 Rg and P = 33 - 65 minutes"; "the differential rotation in an accretion disk
  would in a single orbital period shear out an initially compact gas cloud into an elongated arc". Wielgus 2022: ALMA loops of 74 +- 6 min, a Gaussian hot spot
  "diameter of ~6 rg", light travel time and secondary images in their model. (Both are Sgr A*'s hot thin gas, not a thin disc: plan PLAN_BLACKHOLE section 2.)
- Classic models: Cunningham & Bardeen 1973 (a star round an a = 1 hole: direct and one-orbit images, light curves Figs 9-12); Broderick & Loeb 2005, 2006: a spot
  Gaussian in its own rest frame (2006 (5)), R_S = 1.5 M, on a Keplerian orbit, not sheared.
- Defensible: the orbit and its period (exact); size R_S = 1.5 M (BL06) or about 6 r_g across (Wielgus); a life of about one orbit or less (GRAVITY's loops).
  **Choices**: the brightness (here 5 times the gas at its centre), the radius (0.72.0: 8 M by default, inside GRAVITY's 6-10 Rg; a control under More settings), keeping its shape (as BL06; real gas would shear it: said in the app).
- Computed by the processor reference (tools/movinggas: the app's KerrBL integrator, its crossings, discShiftKerr, discFlux): moving_clump_strip.png, moving_clump_lightcurve.png.
  Checked: the curve repeats after one orbit exactly and not after half (0.98 of its peak apart); the secondary image's peak comes before the direct one's,
  as Cunningham & Bardeen 1973 p. 254 say for an observer off the plane ("shifts to earlier times"; qualitative only: their a = 1 point star is not this setup;
  a figure-point comparison is left as a ROADMAP line).

## 5. Structure in the disc
- What drives it: the magneto-rotational instability (Balbus & Hawley 1991: a disc whose angular velocity falls outward, with a weak field that has a poloidal part, is unstable, growth rate
  of order the rotation rate) makes the gas turbulent. **No telescope has resolved the face of a thin disc**; what it looks like close up is known only from simulations.
- Named simulation: Schnittman, Krolik & Hawley 2006 (GRMHD, Schwarzschild, the inner 25 M): density patches self-similar, 25 degrees in azimuth by dr/r = 0.3
  (sect. 5), lifetime T_life = 0.3 T_orb ((36)), sheared into arcs by the differential rotation; Table 2: the 6-10 M ring's light varies by 43.8 % rms (thermal model).
- **Proposed model** (tools/movinggas Structure): patches born at random places and times (an integer hash: the same on card and processor), Gaussian in
  (ln r, phi) with full widths 0.26 and 25 degrees, light x e^(1-x) with x = (t - t_birth)/T_life; every piece carried at its own Omega(r):
  phi = phi_birth + Omega(r)(t - t_birth). Brightness factor exp(c S), divided by its mean (the disc's total light unchanged); c = 0.65 made the face-on
  6-10 M ring vary by 42 % rms (SKH06: 43.8 %) in 0.56.4; 0.72.0 tapers each patch (no seams at the bins' edges) and recalibrates: c = 0.8 (full pattern),
  1.1 (the lab's reduced one, section 7). A single point varies by about 150 %. In Real the factor multiplies PT's flux, so colour follows (sigma T^4 = F).
- Real: the shearing at the true speeds, the lifetime in orbits. Model: where the patches are, their exact shape and strength (calibrated, said in the app).

## 6. Film
- J15 App. A.6: the film's disc was an artist's (a thin-disc image; a volumetric disc of about 17 million voxels built in Houdini; close-ups with procedural textures,
  "fractal noise"); "not currently accreting", T = 4500 K. J15 says nothing about the disc moving.
- Recommended: Film gets the same moving pattern and clump, labelled "added by the lab: the film's own disc texture is not published and J15 does not say how it moved";
  Alternative B above keeps Film still.

## 7. The build (true at 0.72.0)
- **On the card** (0.72.0): the sky shader (spin > 0) computes phi_k and t_k at the crossings with the disc (kerr_closed.cpp's closed forms in single precision:
  four elliptic jobs per amplitude, the camera's and each timed crossing's; the polar parts at the crossing's half turn j pi), then render/shaders.h kGasPart:
  the clump (BL06 (5)) and the pattern, widened by the pixel's footprint (fwidth of ln r and phi; the sheared patch's covariance plus the footprint's; the
  mean of exp(c S) over the footprint with the patches' spread). Spin 0: kLensFS, phi from the geometry, t from the disc table's travel times (disc.h
  DiscTable::tValues). The card's time: an epoch kept in double (gas.h gasCard). Start-up check: phi and t against traceClosed, the gas against gas.cpp.
- **The pattern the lab draws** is GasPattern::reduced() (half the patches, ages to 3.5 lifetimes, reach 2.5 sigma, c = 1.1 recalibrated to SKH06's 43.8 %):
  the full one per pixel took 7.7 times 0.71.0's sky pass on the working box, this one 4.0 with the travel time and the clump (the job's stop rule). The full
  pattern (cells found from the point: at most 12 of 28 per bin and slot) stays the processor's reference.
- **Benchmark scene**: blackhole.moving_gas (Hard list).
- Left: the processor's table path for a spinning hole (kKerrFS, the fallback when the card's sky is off) does not draw the moving gas yet (SpinFill::timed
  has phi and t); the full pattern per pixel; a figure-point check of the clump's light curve (ROADMAP).
