# Roadmap

What is planned or worth doing. No dates, no promises beyond this list. Done work is in the app's changelog (`native/src/ui/changelog.cpp`) and git, never here.

## Rules for updating this file (every session follows them)
- One item = one line: what to do, and why if it is not obvious. No version history, no long explanations.
- When a job does an item, it DELETES that line in the same commit. If only part is done, the line is rewritten to say what is left.
- A new idea or a problem found goes under its lab as one line. Mark it: **fix** (something wrong now), **source needed** (a number or file must be found first), **owner decides** (do not start without the owner).
- Items marked **owner decides** or waiting for a file from the owner go under "Waiting for the owner", not in the lab lists.
- Whenever a session opens this file, it removes lines that are no longer true. Keep the whole file short (about 60 lines at most).
- Order inside a section = priority (top first). The owner may reorder; a session does not.

## Atom Lab
- **fix**: the jump's "first full use" fence is made again every frame until seen signalled (AtomCardPaths::jumpGroupsBuffer); on a card a frame behind it may never be seen and the step times out (0.57.1 found it in the Molecule Lab's copy: one fence, kept until signalled).
- **source needed**: charge profiles for He-4, Li, Be, B, N-14 (de Vries 1987; drawn as Gaussians, the app says so); nucleus views: Ra pear shapes (Butler et al. PRL 124, 042503 (2020)), alpha clusters in Be-8, C-12, O-16 (save the papers first), the deuteron's two nucleon centres (Forest et al. 1996).
- **fix**: the Slice view keeps the outer electron's true share of the dots, so its shape is faint (Na 3d): a denser option there, said in the readout (Cloud and Cloud 3D have the choice since 0.54.1).
- **fix**: levels with no cloud: Ca, Sr, Ba's lowest d levels (6-10 % low: core-core correlation left out); Rydberg orbitals whose Dirac-Fock does not converge (Hg 7s 7p 6d, Zn 5s, Cd 6d 7d); Ca 4p² ¹D is not identified among the roots; Ra 7s8s, 7s9s (+42, +14 %: the ns(n+1)s levels, as Yb 6s7s +33 %), 7s5f (+7 %), 7s8p (8p Dirac-Fock does not converge). Ca, Sr, Ba, Hg clouds were made before the 0.51.3 orthogonality fix (run_shared.ci_orbitals): recompute and compare.
- Ions between neutral and the one-electron ion (tools/dhf can compute them); clouds: the 7s and 7p shells of the heaviest atoms, correlation in the ground-state clouds of atoms with no jumps, more than 240,000 dots for heavy atoms.
- One-electron atom: **source needed** the Lamb shift for n >= 3 (Balmer lines miss it); **fix** the recoil beyond the reduced mass (hydrogen's 2p gap is 0.054 % below NIST without it); Dirac clouds: the 90 % outline, superpositions, states m != j; hyperfine structure beyond 1s, drawn in the spectrum.
- Labels drawn in depth order (needs a renderer change).
- **source needed**: measured B(E2) for the unstable even-even nuclei (Pritychenko et al. 2016; FRDM(2012) now) and a measured sign for the 29 nuclei that use FRDM's sign (U-238 among them) and the 16 with none.
- **source needed**: Ga-73's ground-state spin: the IAEA moment table (used for its hyperfine splitting) gives 3/2-, NUBASE2020 1/2-; μ/I differs by 3. Read Cheal et al., PRL 104, 252502 (2010) (refused the cloud).
- **source needed**: Geiger and Marsden's "about 1 in 8,000" may be platinum, not gold (Proc. R. Soc. A 82, 495, 1909): save the paper and check.
- Atoms in fields, what is left: hydrogen's Stark effect; the hyperfine regime of the Zeeman effect (Breit-Rabi, low fields); a synthetic spectrum with natural, Doppler and pressure broadening; a Boltzmann-plot temperature from line ratios; the cloud of one M sublevel in the field.
- **source needed**: measured Landé g for Li, Na, Rb, Cs, Fr, He, Be, Mg, Ca, Sr, Cd, Ra (NIST ASD lists none for their levels; 0.54.0 shows the LS value only there).
- More Data tables: <r>, <1/r>, <r²> and P(r), Q(r) of the app's DHF orbitals (**source needed**: Desclaux's tables to check them); higher alkali Rydberg levels for the quantum defects (the saved NIST lists end at Na 5s, 4p, 4d: a new ASD fetch); Q-values of typed reactions (AME2020).
- **fix**, to check in the owner's 0.57.0 run: atom.slice_drag and atom.jump_all with their new dots and groups on the card (0.57.0 estimate: slice at the screen's rate; jump about 4-5 ms of processor work, likely still below 255 Hz). Left on the processor in jump_all: the Glow core's pass over every dot (glowDotsNear) and the exposure grid read back each frame (all every-electron Glow scenes).

## Molecule Lab
- The Density surface on the graphics card if the owner's 0.57.1 run is still below 255 Hz: its triangles sorted and coloured each frame on the processor (benzene 14,128: about 1.4 ms on the test box), the vibrating surface made again each new phase (water: about 1 ms), and the cloud's exposure grid read back each frame.
- **source needed**: Bader et al. 1987 (JACS 109, 7968) for the Density surface's cut-off. **fix**: the Density cloud alone (Show: cloud) still clips the nuclei's cores white (0.52.1 keeps them in range only under the surface).
- Rotation, what is left: the non-linear molecules (symmetric and asymmetric tops), the infrared bands of CO2, C2H2 and HCN, Raman lines of the homonuclear ones; **source needed**: water's pure-rotation band (HITRAN or JPL).
- O2's triplet splitting of each N level.
- **source needed**: measured band origins of H2, N2 and O2 for the Birge-Sponer analysis and the levels above the fitted range (O2: Creek & Nicholls 1975, G(v) to v = 28; N2 and O2 pages do not state their constants' range, so only v = 0, 1 count as measured).
- The moving density for the other molecules (smaller file format); point groups with their axes and planes, linked to which vibrations absorb infrared; HOMO and LUMO from the same DFT runs.
- More molecules with S, F, P (H2S, SF6, PCl5 ...), same CCCBDB route; big molecules later, labelled computed or crystal.
- The look of the bonds (Ball-and-stick fills a third of the picture); vibrations as a coherent state (the swinging centre with the Gaussian spread), not only classical swings.

- **source needed**: HCl's reference dipole 1.093 D looks like mu_e; the ground-state mu_0 is 1.1086 D: check what CCCBDB means.
- Thermochemistry from partition functions (Cp, S, H, G against T; **source needed**: NIST-JANAF tables of the molecules), equilibrium constants; isotope substitution (Teller-Redlich rule); Franck-Condon factors.

## Orbit Lab
- **source needed**: Eris's size from its occultation, 2,326 +- 12 km (Sicardy et al., Nature 478, 493 (2011)); save the paper first (refused the cloud, 9 Nov 2026).
- Real mode: the asteroids' and Kuiper belt's mass are not in the model (Neptune 1,436 km off; Ceres 9,324 km; Earth 14 km); Europa's orbit up to 17,700 km off (Laplace terms); belt asteroids ignore Jupiter far from 2026.
- **fix**: outside 1800-2100 the moons start at made-up angles (the card says so): run JPL's mean elements.
- **source needed**, drawing choices with no named source: comet tails and the 4 AU switch-on, Haumea's egg shape, the belt cards' counts and masses, the Sun card's frost line and lifetime.
- Colours: **source needed**: ring spectra, Pluto, the grey moons and their global maps, McEwen 1996's own lunar phase polynomial, the giants' limb darkening.
- Saturn close up (belts; the rings as a GPU texture: the Cassini profile is cut into up to 700 bands on the CPU each frame); the night sky as a retained GPU mesh; Delta T after the last leap second and UT1 - UTC.
- Eclipses (0.51.2): **source needed** a dated check against a published eclipse catalogue (NASA's Five Millennium Canon); the red light Earth's air bends onto the eclipsed Moon; shadows on the grey moons, of moons on moons, and the giants' flattened shadows; the Sun's limb darkening (waits for Neckel & Labs).
- Earth's air (0.51.2): multiple scattering, the glow beyond the disc's edge, twilight; **source needed** a measured ocean colour (Blue Marble paints deep oceans one uniform blue).
- **fix**: Saturn's ring cards and saturn_rings.h use the fact sheet's optical depths, lower than Cassini's for the B ring (mean 3.3 against 0.4-2.5) and the Cassini Division (0.15 against 0-0.1): check against Colwell et al. 2010 and use Cassini's.
- Celestial mechanics in the Data panel: an ephemeris table (state vectors and elements at a date, in a chosen frame) beside the Horizons tables; an integrator check (energy and angular-momentum drift against the step); Lagrange points and the Jacobi constant; the asteroids' Kirkwood gaps; a Lambert solver with porkchop plots (Earth to Mars: dates, time, delta-v).

## Black Hole Lab
- **fix**: the graphics-card light paths stop the app on the owner's Radeon (0.55.6); switched off there by the guard until the log shows the cause; the driver dies at the shader's link (owner's log, 0.55.7): a much smaller shader, or several small ones, would be the next try
- **fix**: check_blackhole smooth, card part: "a new picture after nearly every frame that moved" gives 9-10 of 16 on the session box (0.55.6's own build too): the picture comes two frames after the first moved frame of each drag step
- Real light, what is left after 0.55.4: the stars' own glare (left out: under the true exposure no star reaches the screen beside the disc); the glare's change with colour and the eye's adaptation (the CIE equation has neither); a calibrated sky map (**source needed**: Gaia's map has no surface brightness); the Sun's limb darkening in the bridge (**source needed**, see "Waiting for the owner").
- The quality levels High and Ultra (only Basic exists). On the processor's path the fine table takes about 0.16 s on 4 cores (0.55.9, closed forms; the owner's PC estimated 0.1 s), so the sharp picture comes about 0.5 s after a stop.
- The stars' images on the graphics card (a compute pass over the card's own sky map; today they trail the sky while the view moves: the processor's moving table, star bins and images, about 23 ms a picture on the owner's PC in 0.55.9).
- **fix**: the processor's path (the fallback when the card's sky is off): seen nearly edge on, the disc still changes at the stop: largest patch 79 px at the paper's view, 86 and 132 px 2 degrees above and below the plane, the outline up to 3.5 px (0.56.0, check_blackhole smooth prints them). More columns far from the edge helped the disc but broke the moving table's far direction near the edge (testMovingTable); next try: more columns inside the shadow only (its rays end at the horizon, no far direction to lose).
- The panel's readout works out the spinning shadow's edge at 132 azimuths on the frame thread whenever spin, distance or tilt change (about 4 ms of each moving frame on the session box, 0.57.2): make it off the frame or from the spin table's own edge.
- The card's sky per pixel, faster (the owner's card: 5.5 ms a picture in 0.56.1, about 0.82 of that estimated since 0.56.2): one duplication shared by the two R_J of the same amplitude (the I+- jobs; estimated about a tenth), Carlson's stop at 0.03 instead of 4e-3 (measured 0.92 on llvmpipe): each below 0.56.2's 15 % gate, not done.
- Moving gas first (owner, 17 Nov 2026): native/PLAN_MOVINGGAS.md, steps 2 and 3 left (the card, time control and clocks, benchmark scene; Film's labels and the guide). Left open: a figure-point check of the clump's light curve against a published curve (Cunningham & Bardeen 1973 Figs 9-12 are for a = 1 exactly and a point star: the closed forms divide by r+ - r-, 0 at a = 1, and the lab stops at 0.99; REPORT 0.56.4).
- Then, in the order the owner chose (plan steps 5-7, after the moving gas): moving cameras (orbiting, then falling in; **source needed** before the spinning fall-in: the ray equations in ingoing Kerr coordinates), the Data panel's tables, the skin.
- A "1979 paper" view that recomputes Luminet's figures in ink-on-paper style, with a credit.
- **fix**: the processor's spin table on the far disc (0.58.0's edge, 2000 GM/c^2) nearly edge on: at spin 0.99, 4 degrees from the plane, up to 33 of 3871 pixels another image and r off by up to 79 % (grazing rays; check_blackhole precision measured it with the disc out to 2000; its tests keep the 6-30 disc); the spin-0 table's r within 1e-3 there (1e-4 for 6-30). Measure whether it shows.
- **source needed**: L79 Fig. 10 prints 2.62 F_S max as the largest observed flux at 10 degrees; the lab gives 2.81 (30 degrees: 2.11, matching the figure). The whole disc's light over all directions against the efficiency needs the shares that fall into the hole and back onto the disc (Thorne 1974, ApJ 191, 507, not saved).
- Spin, what is left after 0.55.2: "Show special radii" (the horizon's equator, the light orbits, the innermost stable orbit as thin lensed rings); spins above 0.99 (finer rows near the flat side of the shadow, so the paper's own a/M = 0.999 figures could be drawn too); a disc orbiting against the spin, tilted or warped; the sky's turning with time for the dragged camera.
- Idea, not planned: a "radio picture" look of the hot gas at 1.3 mm (needs a gas simulation).

## Whole app
- Vulkan step 2 (PLAN_VULKAN.md): renderer2d on Vulkan: every lab's normal drawing (lines, dots, discs, textures, text in the picture, the sky, the maps).
- Vulkan step 3: the card programs of the Atom, Molecule and Orbit Labs (atom_card, molecule_card, group/outline/rock compute).
- Vulkan step 4: the Black Hole Lab's programs (kerr_sky, kerr_compute, the lens and Kerr shaders); retry the compute path parked since 0.55.7.
- Vulkan step 5: the benchmark, picture tests and UI suite on Vulkan; Vulkan the default; OpenGL removed once the owner's PC shows every scene works.
- After the owner's 0.56.1 benchmark: move scenes far above his screen's rate to All scenes; fix the ones below it, slowest first; fix every response he would feel and every lab that is busy at rest.
- **fix**: check_molecule card "the benchmark's report names the card for both scenes" fails on the session box (0.57.2's build too, 0.58.0): the first scene's path still says "once its start-up check has passed".
- **fix**: check_6b benchscenes: a Black Hole scene (blackhole.extreme or all_light) sometimes leaves no picture to compare ("-1.0000 of the pixels") on this box; 0.58.0's build the same.
- **fix**: check_6b benchwaits' response.atom_density reaches its 15 s cap on this box's software renderer with one thread (ui_popups fails; 0.56.4 the same): a cap or a cloud filling that does not depend on the frame time.
- **fix**, to check in the owner's 0.56.1 run: orbit.sky had a 4.8-5.7 s frame while turning (here, inside the software renderer's shader code).
- The owner's benchmark scenes below his screen's rate (255 Hz), slowest first (0.56.1 run; owner decision 17 Nov 2026, evening: after the Black Hole Lab's look): atom.slice_drag 12.5 fps and atom.jump_all 20.5 (0.57.0 moved their heavy loops to the card: his next run decides); molecule.vib_density 68.9 and molecule.density 133 (0.57.1 moved the Density cloud to the card and took out the waste, estimate under 3.9 ms of processor work: his next run decides); atom.nucleus_zoom 108; atom.nucleon 127; atom.all_enlarged 199; atom.uranium_true 129 (in All scenes).
- Graphics quality presets, Low to Ultra, for older PCs; speed work only where the owner's benchmark shows a problem (0.50.3 review's list, now measured by ui.settings_open, ui.guide_scroll, atom.nucleus_density and molecule.density: the exposure-grid and Superposition read-backs each frame, settings text, guides, cards and Pick a body rebuilt each frame, the molecule surface colours; 0.56.1: the nucleus density grid is made once while still).
- Data panel: the user's typed data kept between runs; a fit to the user's own formula (typed like a calculator line); a setup as a file, not only the clipboard.
- Data panel (0.51.1 review): leave out a point by tapping it; asymmetric errors (profile likelihood or bootstrap) for nonlinear fits; a Poisson-likelihood fit for counts and half-lives; AIC/BIC between models; a Voigt profile; correlated x-y errors (York's r); a teaching mode that hides the expected answers; the sources as BibTeX.
- Data panel physics (0.51.1 review): hydrogen's lines against Dirac's levels (and the Lamb shift), so the Rydberg fit's leftovers are explained; Birge-Sponer from measured band origins (the ladder is now made from the constants it fits).

## Calculator
- Matrices, what is left after 0.54.2: eigenvectors, the inverse, the matrix exponential (and sqrt, exp of a matrix), eigenvalues of non-Hermitian matrices, sizes above 4x4; a bare 0 beside entries with a unit ([[1 eV, 0], [0, 2 eV]] is refused now: the unit goes outside).
- Uncertainties carried through (the constants store them); °C and °F (affine units need their own rules); ln, sin ... of complex numbers; units like GF's (J·m³) shown in base units.
- Measuring tools on the picture (ruler, angle, stopwatch, probe); a calculator curve (a function of x) sent to the Data panel.
- Worked calculations on the app's own data: the mass formula against AME2020, the molecules' heat capacity against NIST-JANAF; a decay-chain calculator (Bateman).
- A molecule's infrared absorption beside measured spectra (**source needed**: spectra files).

## Waiting for the owner
- the disc's patches from MHD simulations (PLAN_MOVINGGAS): owner decides; he wants no visuals that are not measured or calculated.
- **Paused by the owner (14 Nov 2026, after 0.51.3)**: element jumps with every electron shown. Left: Yb, Cu, Ag, Au, the p block, transition metals, lanthanides, actinides (native/PLAN_JUMPS.md), and the cloud size budget (16 atoms take about 16.5 MB).
- "Far labels" in the Orbit Lab: the owner describes what is wrong. Photon jumps from a complex state or a superposition: the owner picks an option of 0.11.
- **owner decides**: the Simple quark style's static-quark flux tubes inside a moving proton: remove, or keep as a labelled cartoon (review, 0.49.3).
- Files the owner saves (the cloud is refused): the island of stability (predicted magic numbers), halo nuclei's matter radii (IAEA), Neckel & Labs 1994 or Neckel 2005 for the Sun's limb darkening; for the Black Hole Lab, only to cite the originals (their results are in saved papers; ADS refused, 15 Nov 2026): J. M. Bardeen, "Timelike and null geodesics in the Kerr metric" (the shadow's edge) and I. D. Novikov & K. S. Thorne, "Astrophysics of black holes" (the thin disc), both in Black Holes, eds C. DeWitt & B. S. DeWitt (Gordon and Breach, 1973), pp. 215 and 343.
- **owner decides**: methanol's torsion at the measured shape is 118 cm^-1 with the app's B3LYP/aug-cc-pVTZ (reproduced in 0.52.1) but 177i with aug-cc-pVDZ (305 at the DZ minimum): at a shape that is not the minimum its sign depends on the basis. Calculate at the TZ minimum, or show no calculated value.
- **owner decides**: the Atom Lab's Glow core is kept inside the screen's range since 0.52.1, so a whole atom's outer shells are faint beside its core: a tone curve (it changes the dots' relative brightness) would show both.
- Molecule realism: one fixed charge-colour scale, a bond (deformation density) view. Maybe, or drop: a fog-style electron cloud.
