# Decisions (made by the owner)

A record of the owner's decisions, one or two lines each. Sessions do not have to read it: CLAUDE.md holds the rules in force (23 Nov 2026).
A line marked "Superseded" is kept for the record only. Older wording is in git history.

## 25 Nov 2026
- All problems found by the outside review of 25 Nov 2026 are to be fixed. Where a fix needs a source, the source is found first (research job 0.84.1, native/PLAN_REVIEW.md).
- In the realistic choice the planets are shown as they really are: true dimming with distance.
- Film shows only the film paper's choices (J15). The lab's own glow and the see-through, soft-edged disc go.
- The made-up learning pictures (Simple quark style, nucleus colours and glow, the Orbit view's electron ball and glide) stay as options, labelled as made up.
  The realistic choice (the dot) and Most realistic never show them.
- Orbit Lab, realistic choice: planets lit by the true sunlight at their distance (inverse square), with a camera-like exposure set for the followed body and a readout of it.
- The Sun: real spots for dates where they were measured, a plain Sun with a sentence where they were not. The edge darkening from a measured law, the same law in the eclipse code.
- Oort cloud: no invented dots. Real objects only if a published list says they belong to it. Otherwise a labelled region.
- The order of the fixes: 0.85.0 Orbit Lab, 0.86.0 Atom Lab and Film, 0.87.0 the Black Hole disc.
- R3: the packed sunspot table goes in the public repo (it holds only measured positions and areas). No download step.
- R4: the Oort cloud stays visible. Its dots are placed by a published model of the cloud (how many objects at each distance, its shape), labelled "Estimated from models: no object in it has been seen". Sedna, 2012 VP113 and 2015 TG387 are shown on their JPL SBDB orbits with the sentence "called inner Oort cloud objects by their discoverers; JPL lists them as trans-Neptunian objects".
- The benchmark limit is raised once to 4.9 minutes for the three scenes of 0.85.0 (orbit.true_light, orbit.sun_edge, orbit.oort_region).
- Checks that need real hardware (the GPU on a real driver, the benchmark, the refresh rate, window and screen behaviour) run on the owner's PC through Remote Control with native/tools/pc_tests.ps1, after the push, on the new release (job 0.85.1). The benchmark for the planning chat comes from this run.
- For 0.85.1 only: two pushes (the runner and the test package first, then the results). This breaks the one-push rule of 20 Nov once, because the checks need the new release.

## 23 Nov 2026
- One short CLAUDE.md holds the rules. DECISIONS.md is a record only.
- No fixed Opus/Haiku split. The session decides when a helper agent is worth it and checks what a helper reports.
- Standard terms in developer files (GPU, CPU, shader, build machine).
- The full UI suite runs automatically on GitHub on every push.
- Small bug fixes may go straight to a working chat without the planning chat.
- 255 Hz stays.
- Superseded later on 23 Nov 2026 (every job runs quick plus ui_smoke). A job runs only check.sh quick and the ui_* test of the screen it changed. The full suite runs on GitHub after the push; its failures become the next job and do not block the current one. check.sh full by hand only when the owner asks.
- Any single command running longer than 15 minutes is stopped, and the reason written in REPORT.md.
- The UI suite is cut to what catches real breakage; target 10 minutes; every job runs quick plus ui_smoke.

## How the work is done
- Superseded on 23 Nov 2026 (no fixed Opus/Haiku split; small fixes may skip the planning chat). Three roles (14 Nov 2026): a planning chat leads (architecture, design, what comes next) and writes each job; it writes no code and does not push. An Opus
  session builds the job and leads its Haiku helpers (CLAUDE.md, "Two models", 4 Nov 2026); Haiku helps, and Opus spot-checks what Haiku reports.
- Quiet chat (14 Nov 2026): a working session does not report progress. It writes in chat only when it cannot go on without the owner's answer, and once at
  the end (the outcome first); everything else goes in REPORT.md. Every message costs the owner's usage.
- No time limit for a job; a problem that fails 3 times is skipped and written in REPORT.md (18 Oct 2026).
- Superseded on 20 Nov and 23 Nov 2026 (the full suite runs on GitHub on every push). Tests in batches (6 Nov 2026): every job runs the quick tests and the UI check it adds or changes; the full UI suite only at a new version X.Y or when the owner asks.
- Versions are named X.Y.Z only; commits are "native X.Y.Z"; no "stage" names (8 Nov 2026). No 1.0 milestone: the app keeps growing (7 Nov 2026).
- No connectors, plugins or extra skills unless a job asks for one (they cost usage). No code signing (it costs money); the README and release text explain the warning.

## Project
- The app (native/) is C++ for Windows and stays C++; Rust only for separate apps, never in this repo (18 Oct 2026). Four labs: Orbit, Atom, Molecule and the
  Black Hole Lab (15 Nov 2026; this replaces "three labs"; the fourth is planned, not built yet).
- The Defense Lab is removed (18 Oct 2026); an old settings file still loads and opens the Orbit Lab.
- There is no other version of the app (3 Nov 2026): no copy, no tests against one, no mentions. The old web page (branch `main`) is deleted (14 Nov 2026; the session may not delete a branch, so the owner does it on GitHub).
- This repo is public and stays public (15 Nov 2026). Papers, preprints, journal scans, saved web pages and downloads of unclear terms live only in the private repo ShelaShelby/physics-lab-sources (same relative paths under `data/`); this repo keeps the extracts the tests read and native/data/SOURCES.md. The old repo was renamed ShelaShelby/physics-lab-archive and made private (it keeps the full history); ShelaShelby/physics-lab is a fresh public repo with one commit and one branch `native` (15 Nov 2026).
- Serious work only, no games (13 Nov 2026). No game parts (14 Nov 2026): achievements, the surprise and the secret unlocking are removed; Journey (a scale tour) stays.

## Orbit Lab
- Alpha Centauri is the next feature (22 Nov 2026): the system as a switch in the Orbit Lab (ROADMAP), with deep research first (native/PLAN_ALPHACEN.md, 0.72.1) so the build jobs never guess. The Black Hole Lab's look and speed work are paused for now.
- Alpha Centauri, the owner's answers (22 Nov 2026, PLAN_ALPHACEN.md sect. 14): (1) Proxima c drawn dashed, labelled "disputed"; (2) the JWST candidate around A
  as a faint dashed band, "unconfirmed, seen once"; (3) Proxima's mass 0.1221 (Kervella 2017) for the physics, the lensing 0.150 shown beside it; (4) A and B's
  masses and distance from Akeson et al. 2021; (5) our Sun in their sky as the last build step; (6) the planets' unknown tilt drawn at the star's equator (47°,
  Klein 2021), labelled "assumed".
- Real planets start from the JPL Horizons state of 1 January 2026 12:00 TDB; the IAU astronomical unit, 149,597,870,700 m exactly (3 Oct 2026).
- Planet physics Real stays an option (default Simple) until the owner has tried it on the PC (3 Oct 2026). In Real, relativity (1PN) is on by default (5 Nov 2026).
- The Moon in Real mode comes from ELP2000-82B, checked against Horizons (3 Oct 2026).
- "Reset to today" keeps the zoom, the Moons/Dwarfs/Comets/Belts switches and Time speed; it resets the selection and Follow.
- The Milky Way is ESA's Gaia all-sky colour map (5 Nov 2026). Planet maps: NASA/USGS where one exists, Hubble OPAL for Saturn, Uranus and Neptune (the owner
  downloads them), Venus a smooth ball in its measured colour; true colour, no grid, no made-up bands (5 Nov 2026).
- The Orbit Lab opens fitted to the whole Solar System on every start (4 Nov 2026).
- The Orbit Lab's camera (22 Nov 2026; the rework in three jobs: 0.74.0 handling, 0.75.0 depth, 0.76.0 free flight): it never lets go by accident; only a pick of
  something else, Reset view, Escape or picking the followed thing again let go (an empty click changes nothing). A right drag (Shift-drag, a drag in 2D) slides the
  view but stays attached: the followed thing keeps its new place on the screen.
- The Orbit Lab's camera, part 3, free flight (WASD or arrows, mouse look): not now (owner, 23 Nov 2026); the 0.76.0 slot is gone, the idea waits under "Waiting for the owner" in ROADMAP.md.

## Atom Lab and chart of nuclides
- Electron cloud defaults: style Glow, density High. The whole atom opens at True scale (Shells enlarged is a choice).
- More electrons (6 Nov 2026): Hartree-Fock-based orbitals for every electron, shells as layers with one orbital to pick, only measured (NIST) energies;
  every electron in every view, the Orbit view as shell rings labelled as a picture, not real orbits (0.39.1).
- Relativity: the one-electron cloud has a Cloud model setting (Auto: Dirac from Z = 27; Dirac; Non-relativistic); the spectrum's relativity is on by default
  (Dirac energy plus the Lamb shift of n = 1, 2 from Yerokhin & Shabaev 2015) (11 Nov 2026).
- A photon jump is drawn as the exact one-photon process, slowed down (τ = 0.2 s on screen, cut at τ ln 48,000); placing an electron by hand is a 1.2 s cross-fade (11 Nov 2026).
- Lifetimes and jump rates come only from NIST ASD's A values; where ASD has none the card says so (4 Oct 2026). The nucleus is drawn at its measured size
  (IAEA charge radii) and measured shape where one can be used (IAEA moments, ENSDF B(E2), else FRDM(2012)), and the card says which (4 Oct 2026).
- The CODATA 2022 constants (4 Oct 2026). The real NIST ASD spectrum is shown as two comparisons, the hydrogen-like ion and the neutral atom, never one list (5 Oct 2026).
- Quarks (8 Nov 2026): styles Accurate or Simple; Accurate draws no colour charge and its quarks are clouds from measured form factors. Nucleus looks Density or Nucleons.
- Element jumps (9 Nov 2026): D1 a jump is measured only when both its levels are; D2 30-50 levels with a cloud per atom; D3 the clouds may add 10-20 MB;
  D4 where nobody measured a rate, the app's own GRASP value, marked computed; D5 atoms with no jumps: grey buttons with the reason; D6 the lined-up (M_J = J) shape by default.
- Element jumps are paused after 0.51.3 (14 Nov 2026; replaces D7, "a group of atoms per update"): no further parts are planned; what is done stays.

## Black Hole Lab (15 Nov 2026; the plan is native/PLAN_BLACKHOLE.md)
- A fourth lab. The Orbit Lab's Sagittarius A* dot gets a way to open it.
- The goal: a black hole that looks as realistic as possible, everything deeply researched and traceable. It is shown as a calculated model and the app says
  so; where the real look is not known, the app says that too.
- One black hole with mass and spin controls, and presets for the two imaged ones (Sagittarius A*, M87*) with their measured masses.
- Moving: first turning around the hole and moving closer or farther; later orbiting at speed and falling in. The foundation must not block those.
- Looks as options, like the other labs: "Real" (the closest to reality, with the dot) and "Film" (labelled as the film's choices).
- It must run on low-end graphics cards too, not only the owner's (a Radeon RX 9060 XT); how low is proposed by the research (PLAN_BLACKHOLE.md, section 6).
- The skin (corrected 15 Nov 2026; replaces the earlier "white curved surface" line): the whole picture becomes white paper in place of black space and stars;
  black handwritten-style calculations, the real equations the lab uses, cover it and run across it; near the hole the writing stretches, curls into arcs
  and wraps around it, bent by the same light-path calculation as the real view. It comes after the real view works. No embedding diagram.
- The plan's eight questions (15 Nov 2026), answered as recommended: our own sky behind the hole with one line saying so, and a switch for a black background;
  the Sagittarius A* and M87* presets open with no disc and a sentence about their real faint gas, the disc one click away and labelled, plus a third preset
  "A feeding black hole (model)"; automatic exposure with a slider and the readout "shown N times dimmer than real"; the Film look gets a simple soft glow
  labelled as the app's own; presets set mass and distance only, spin starts at 0 with "spin not measured"; no new minimum graphics card; the radio-picture
  look stays a roadmap idea; build order: the lab, the disc, spin, the looks.
- The gas disc (owner, 24 Nov 2026; replaces "The moving gas" of 23 Nov 2026): the gas disc is ugly in both looks, Film worst. All four bother him: it is
  flat like a sticker, too smooth and plastic, its edges are hard, and its colour and glow look cheap. He wants the most realistic option. Research job
  0.78.1 wrote the plan (native/PLAN_DISC.md); its questions (section 10) wait for him (ROADMAP, Waiting for the owner).
- The gas disc, the owner's answers (24 Nov 2026, after seeing native/docs/blackhole/disc078_*.png): build the recommended version of PLAN_DISC.md.
  (1) The recommended version. (2) Keep the true thickness at 0.1 L_Edd. (3) Sagittarius A*: the model's colour, its range named in the guide. (4) The streaks
  replace 0.72's round patches in Real and Film; the clump stays (labelled). (5) Film keeps the lab's own glow, but Film should look better than
  disc078_film.png (labelled as the lab's own). D5 (the Illinois simulation data, M87*'s model) is not chosen.
  The order: 0.79.0 the Real feeding disc (D1's thin-disc parts and D2: streaks, true thickness, the thinning inner edge); 0.80.0 the hot flow for
  Sagittarius A* (D1's hot-flow parts and D3); 0.81.0 Film (D4); 0.82.0 the camera (planned with the owner first).
- Sagittarius A*'s hot flow (24 Nov 2026, during 0.80.0): BL06 as printed passes the 1.3 mm and the 2.2 um checks under no sourced reading. The owner: stop
  drawing. Ship only the tested physics, no change on screen; do not loosen or drop either check, and draw nothing that fails them. First look for the missing
  definition in Yuan, Quataert & Narayan 2003 and Broderick & Loeb 2005; a reading that then passes both waits for a later job. The GPU table, the scene
  blackhole.hot_flow and the guide text wait for a model that passes both (REPORT 0.80.0, ROADMAP).
- The camera after the disc (24 Nov 2026): a camera that moves round the black hole as the Orbit Lab's moves round a planet, "to see the black hole from every
  side". Planned with the owner before it is built (0.82.0; ROADMAP, Black Hole Lab).
- The camera handles as the Orbit Lab's (24 Nov 2026, 0.82.0): "in BH lab you like move the BH itself which is so weird". He circles the black hole as he
  circles a planet in the Orbit Lab: the same turn per point, the same way, the same glide on release (off with Reduce motion), the eased wheel, Reset view
  and Escape. The camera still hovers at each place (a static observer); a truly moving camera (orbiting, falling in) stays on ROADMAP.
- A benchmark job after the camera (24 Nov 2026): 0.83.0, after the camera job (0.82.0). New Black Hole scenes that move as a user does (circling, moving in and
  out, over the top and under), and the Hard list cleaned: scenes that reach 255 Hz on the owner's PC move to All scenes, decided from his benchmark.
- Sagittarius A*'s hot flow (24 Nov 2026): 0.80.0 ships the tested physics with nothing drawn (BL06's model passes neither reading's two checks). After
  0.81.0 Film, 0.82.0 the camera and 0.83.0 the benchmark, a research job: find the missing definition (or another published, fitted model of Sagittarius
  A*'s flow) and test it against the 1.3 mm and 2.2 um checks. It is drawn only if a model passes both.
- No lower resolution while moving (17 Nov 2026): the picture stays sharp while the user drags. A lower resolution only on a card whose whole picture takes
  over 16.7 ms (fewer than 60 sharp pictures a second); Settings, Performance then says so, with the switch "Lower resolution while moving" (Auto / Off).
- Something must happen in the lab (17 Nov 2026): moving gas comes before flying the camera. Wishes: true speeds, a bright clump circling the hole, a time
  control and two clocks, structure in the disc. Rule: as realistic as possible and researched; where the real look is unknown, a labelled model (native/PLAN_MOVINGGAS.md).
- Moving gas, the recommended version (17 Nov 2026, PLAN_MOVINGGAS.md section 0): Real and Film both get the moving pattern and the clump (in Film labelled as
  the lab's own addition), a time control, two clocks (far away, on the clump) and the exact light travel time per pixel.
- The order (21 Nov 2026): 0.70.0 the Real look (no white-outs, a camera-like picture, a good starting view, the ring round the shadow); 0.71.0 fewer
  settings (one short panel, the rest under one "More settings"); 0.72.0 moving gas from real physics (PLAN_MOVINGGAS.md steps 2 and 3), on the graphics card
  from the start.
- Real and Film stay two separate looks (21 Nov 2026): Real keeps true colour (a disc this hot is blue-white); Film keeps the film's golden disc, labelled as
  the film's choice.
- No benchmark scenes are added to find problems; the owner's usual benchmark after each version stays (21 Nov 2026). Its second half is replaced by the benchmark rule of 21 Nov 2026 (Graphics and speed).

## Molecule Lab
- The Molecule Lab is the 15 measured molecules; the builder is removed (4 Nov 2026). Density shows a DFT electron density at the measured shape (18 Oct 2026).
- Space-filling shows true, overlapping van der Waals spheres; heavier atoms get more cloud dots (7 Nov 2026).

## Tests and precision
- Tests never compare raw floating-point bytes from another compiler; Atom Lab references use double-double arithmetic, never `long double` (4 Oct 2026).
- The GPU (32-bit) may differ from the 64-bit CPU path only within float rounding of a threshold, counted and bounded by the tests (0.13).
- Orbital normalisation is the exact closed form, not a numerical sum (0.13). Belt rocks are Kepler ellipses; anything that grows without limit stays in double (0.13).

## Graphics and speed
- Every simulation is built to run on the graphics card. The processor's version is kept: it is the exact reference the tests check the card against, and it
  takes over on a PC whose card or driver cannot do it (through the guard of 0.55.7). A feature is not finished while its heavy repeated work runs only on the
  processor. A job that adds something drawn adds a benchmark scene for it; the target is the screen's refresh rate on the owner's PC for every scene. The owner
  sends the benchmark after each version X.Y and the planning chat checks every scene (16 Nov 2026; replaces the line of 5 Oct 2026). "The owner sends the
  benchmark after each version X.Y" is replaced by the benchmark rule of 21 Nov 2026 below.
- No scene below the screen's refresh rate is acceptable (his PC: 255 Hz). Moving heavy repeated work to the graphics card comes before new features, slowest
  benchmark scene first, until every scene of the Hard list reaches it; the Black Hole Lab's moving gas waits until then (17 Nov 2026).
- The Black Hole Lab's look comes first, then the speed work; every new drawing on the graphics card from the start (the processor as reference and fallback).
  Replaces the order of 17 Nov 2026 (speed before new features) for now; the 255 Hz target stays (17 Nov 2026, evening).
- 18 Nov 2026: the app moves to Vulkan in five steps (native/PLAN_VULKAN.md, finished in 0.69.0 and deleted: git keeps it); OpenGL is the backup until every part works on Vulkan on the owner's PC, then removed.
- Vulkan needs version 1.3 and uses 1.4 where the card has it (owner decision 18 Nov 2026). Replaced on 20 Nov 2026 by the next decision.
- 20 Nov 2026: OpenGL is removed completely, after the owner's benchmark showed every scene works on Vulkan (his 0.68.0 run did). No frozen OpenGL download for old cards. The app accepts Vulkan 1.2 when the driver offers VK_KHR_dynamic_rendering and VK_KHR_synchronization2, plus 1.3 and 1.4, chosen automatically; one drawing path (present.frag goes); a card that cannot run it gets a clear message naming what is needed. The removal job first checks vulkan.gpuinfo.org (saved in the sources repo) for which old Windows cards 1.2 plus the two extensions actually adds, and leaves the 1.2 part out if it adds almost nothing. Replaces "Vulkan needs version 1.3" (18 Nov).
- The graphics card, every lab (21 Nov 2026): everything that fits the card goes there (work done for many dots, pixels or lines every frame). Chains of
  calculation and tables made once stay on the processor (the atoms' equations, the planets' motion, the light-path tables); a chain that slows a frame moves
  to a background thread. The processor keeps an exact reference and the fallback for everything on the card. Speed work is paused until the Black Hole
  Lab's look is done.
- The benchmark rule (21 Nov 2026): the owner sends a benchmark only after a version that adds a new benchmark scene; the planning chat asks for one only
  then (not after every X.Y, and not before every job). Replaces "the owner sends the benchmark after each version X.Y" (16 Nov) and "Before any new job,
  the owner's benchmark of the previous version" (Workflow).
- Order: finish Vulkan fully, then all the speed work on Vulkan, slowest scene first; the outside review, the Black Hole Lab's look and new features are talked about after that (owner decision 18 Nov 2026).

- Order of the next jobs (24 Nov 2026): this benchmark job (0.83.0), then the Black Hole Lab's camera, then the rest.
- The Hard list's rule (24 Nov 2026): a scene leaves the Hard list when, in the owner's latest benchmark, its 1 % low is at least 1.5 x the display's refresh
  rate (his PC: 1.5 x 255 = 382.5 fps). It stays in All scenes. Every later benchmark job applies the same rule.
- The run's limit (24 Nov 2026): fixed at 4.5 minutes. A new scene replaces one, or the rule above makes room. The limit is not raised again.
- The picture keeps up (24 Nov 2026): in a moving Black Hole scene the results have a column "lag", the worst time the shown picture was behind the settings.
  "Reaches" also needs the lag at most 2 frames at the display's rate (his PC: 2 / 255 s = 7.8 ms). The rule is written under the table.

## Look and interface
- The 0.38 look (6 Nov 2026): sharp corners, flat surfaces with a 1 px edge, a chosen option as a soft accent tint, the labs as tabs in the top bar; in every theme.
- The 0.15 layout (5 Oct 2026): the picture fills the window; a slim top bar, floating cards, a toolbar bottom centre, a reading view for pages; body text never
  under 14 px at 100 % scale; desktop-sized controls; every feature reachable. The toolbar is centred on its scene.
- All simulation options live in one Controls panel on the right, opened from a handle on the picture's edge, sliding in and out (6 Oct 2026).
- Panels are nearly solid (opacity found so text passes WCAG 4.5:1 over the worst picture); pages of text are solid; the picture dims behind Settings, About and the guides.
- Science colours (phase wheel, decay modes, elements, planets, spectra) do not follow the theme. The theme picker is a 3-column grid. Inter is the font of the look.
- The choice closest to reality is marked with a dot, and each lab's Controls has a "Most realistic" button (5 Nov 2026).
- The app asks Windows not to power-throttle it; the smallest window is 880 x 560 units.
- About ends the developer's paragraph with "Free Palestine" and a small drawn flag of Palestine (22 Nov 2026).
- The Orbit Lab's 3D view is a pinhole camera with a 45-degree vertical field (a phone camera's), and the zoom is the camera's distance at the target: the target's plane shows the zoom's span, so zooming flies the camera closer and the scale bar holds at the target's distance (the planning chat's design for 0.75.0, after the owner asked for "a better camera, less 2D", 22 Nov 2026).

## Workflow (owner decisions 20 Nov 2026)
- Exactly one push per job, at the end, after the tests and the docs are final; no checkpoint pushes; no doc-only push after it; a fix found after testing goes into that same push.
- Superseded on 23 Nov 2026 (the full suite runs on GitHub on every push; by hand only when the owner asks). The full UI suite runs only when the owner asks. Every job runs check.sh quick plus the ui_* tests of the screens it changes.
- No known failure carries over to the next version: fixed, or its test corrected with a written reason; never loosened.
- The planning chat reads the processor's code (data, precision, calls) before planning any move to the card, and names in the job the files and lines it read.
- Superseded. Before any new job, the owner's benchmark of the previous version. Replaced by the benchmark rule of 21 Nov 2026 (Graphics and speed).
- Who keeps each plan file true: the job that changes a plan's area updates it; the planning chat names the plan files in the job.

## Atoms in fields (owner decision 27 Nov 2026)
- 0.93.0 ships Part 0, A (Stark) and B (Breit-Rabi). Parts C (broadened spectrum, Boltzmann plot) and D (many-electron Slice sampler on the GPU) are skipped to save usage. They stay on the ROADMAP.
