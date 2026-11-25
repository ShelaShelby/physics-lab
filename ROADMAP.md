# Roadmap

What is planned or worth doing. No dates, no promises beyond this list. Done work is in `native/src/ui/changelog.cpp` and git, never here.

## Rules for updating this file
- One item = one line: what to do, and why if it is not obvious. No version history.
- A job that does an item deletes its line in the same commit. A partly done item is rewritten to what is left.
- New ideas and found problems go under their lab as one line, marked **fix** (wrong now), **source needed** (a number or file must be found first) or **owner decides**.
- Items waiting for the owner go under "Waiting for the owner".
- Keep it short: one line per item, merge or remove stale ones. Order = priority (top first). The owner may reorder; a session does not.
- Use standard terms: GPU, CPU, shader, build machine.

## Atom Lab
- Slice samplers on the GPU: the Dirac sampler (its radius table in single precision loses 3.5e-5 of the radius at the 99.99 % radius: invert the tail from 1 - u) and the many-electron sampler.
- atom.nucleon on the GPU: its quark dots use a double-precision sine hash. A GPU version needs an integer hash on both sides (the dots then change).
- The nucleus views at full zoom fill about 16 pictures of pixels a frame (one glow quad per nucleon). One pass over the picture for all glows would fill one.
- The Slice of an overlapping superposition on the GPU: a kept point takes one more random number, so attempts have no fixed numbering.
- **fix**: the jump's "first full use" fence is made again every frame until seen signalled (AtomCardPaths::jumpGroupsBuffer). Keep one fence until signalled, as the Molecule Lab does.
- **fix**: the Slice keeps the outer electron's true share of dots, so its shape is faint (Na 3d). Add a denser option there.
- **fix**: levels with no cloud: Ca, Sr, Ba lowest d levels (6-10 % low), Hg 7s 7p 6d, Zn 5s, Cd 6d 7d, Ca 4p² ¹D, Ra 7s8s, 7s9s, 7s5f, 7s8p. Recompute the Ca, Sr, Ba, Hg clouds made before the 0.51.3 orthogonality fix.
- **source needed**: charge profiles for He-4, Li, Be, B, N-14 (de Vries 1987). Ra pear shapes (Butler et al. 2020), alpha clusters in Be-8, C-12, O-16, the deuteron's nucleon centres (Forest et al. 1996).
- **source needed**: measured B(E2) for unstable even-even nuclei (Pritychenko et al. 2016) and measured signs for the 29 nuclei on FRDM's sign and the 16 with none.
- **source needed**: Ga-73 ground-state spin (IAEA 3/2-, NUBASE2020 1/2-): Cheal et al., PRL 104, 252502 (2010).
- **source needed**: Geiger and Marsden's "about 1 in 8,000" may be platinum, not gold (Proc. R. Soc. A 82, 495, 1909).
- **source needed**: measured Landé g for Li, Na, Rb, Cs, Fr, He, Be, Mg, Ca, Sr, Cd, Ra (NIST ASD lists none).
- One-electron atom: **source needed** the Lamb shift for n >= 3. **fix** the recoil beyond the reduced mass (H 2p gap 0.054 % low). Dirac clouds: 90 % outline, superpositions, m != j. Hyperfine beyond 1s.
- Ions between neutral and one-electron (tools/dhf). Clouds for the 7s and 7p shells of the heaviest atoms. More than 240,000 dots for heavy atoms.
- The photon's dipole pattern for the many-electron atoms (the lab does not follow m or M there) and the light's polarisation.
- Atoms in fields: hydrogen's Stark effect, Breit-Rabi at low fields, a broadened synthetic spectrum, a Boltzmann-plot temperature, the cloud of one M sublevel.
- Data tables: <r>, <1/r>, <r²>, P(r), Q(r) of the DHF orbitals (**source needed**: Desclaux's tables), higher alkali Rydberg levels (new ASD fetch), Q-values (AME2020).
- Labels drawn in depth order (needs a renderer change).
- Still on the CPU in atom.jump_all: the Glow core's pass over every dot (glowDotsNear) and the exposure grid read back each frame.
- Benchmark: atom.bfield_drag was dropped in 0.83.0. The field slider remakes nothing heavy (only the readout, the spectrum's lines and the Zeeman tables).

## Molecule Lab
- The Density surface: the vibrating surface is rebuilt on the CPU for each new phase (water about 1 ms). Each new level's 60,000 dots are sampled once. The exposure grid is read back each frame.
- **fix**: the Density cloud alone (Show: cloud) clips the nuclei's cores white. **source needed**: Bader et al. 1987 (JACS 109, 7968) for the surface's cut-off.
- **source needed**: HCl's reference dipole 1.093 D looks like mu_e (mu_0 is 1.1086 D). Check what CCCBDB means.
- **source needed**: measured band origins of H2, N2, O2 for Birge-Sponer and levels above the fitted range (O2: Creek & Nicholls 1975).
- Rotation of non-linear molecules, the IR bands of CO2, C2H2, HCN, Raman lines, O2's triplet splitting. **source needed**: water's pure-rotation band (HITRAN or JPL).
- Moving density for the other molecules, point groups with their axes, HOMO and LUMO. More molecules with S, F, P (CCCBDB route).
- The look of the bonds (Ball-and-stick fills a third of the picture). Vibrations as a coherent state.
- Thermochemistry from partition functions (**source needed**: NIST-JANAF), equilibrium constants, isotope substitution, Franck-Condon factors.

## Orbit Lab
- **fix**: the Orbit Lab at rest drew its picture anew every frame on the owner's PC (0.75.0: rest.orbit 766 drawings in 3 s). The build machine does not reproduce it. Read the changed key part (main.cpp orbitFrameKey) in his next benchmark, then fix it.
- **fix**: outside 1800-2100 the moons start at made-up angles and planes. Use JPL's mean elements (saved: data/jpl/sats_mean_elements_2026-11-23.html) and their reference planes.
- **source needed**: a measured phase law for each main ring (A, B, C, Cassini Division; brightness against phase angle up to the forward-scattering side), to replace the rings' even scattering (0.78.0 item 2, skipped after 2 tries: Déau et al. 2009, arXiv:0902.0289, has the opposition surge only; the UVIS power law is far-UV only).
- **source needed**: a published syndyne or synchrone with readable numbers (Finson & Probstein 1968: the ADS scan did not load on 2026-11-24) to check one dust tail curve. The comets' dust: grain ejection speeds (the tail's width) and the gas tail's length are not modelled.
- **source needed**: the comets' 4 AU dust switch-on (a choice since 0.78.0; a distance where water ice starts to sublimate), Haumea's shape,, lower than Cassini's for the B ring and the Cassini Division. Check against Colwell et al. 2010.
- **fix**: a released turn coasts up to 6 % past its stated most (kMomentumMax / kMomentumFade = 0.33 rad) at 60 Hz, 1.5 % at 255 Hz: it steps before the fade. The Black Hole Lab uses the exact step (orbit/camera_motion.h momentumCoastSeconds); the Orbit Lab was kept as it is (0.82.0's stop rule).
- A Field of view setting for the 3D camera (45 degrees now, orbit/camera.h). A pick of a closer body lowers the zoom to its cap at once.
- Alpha Centauri (PLAN_ALPHACEN.md): the Sun's pick looking back in 2D, A and B seen from Proxima b, Fit planets with the card open, Proxima's orbit time on its card and a faster time choice, A and B as one label when close.
- Real mode: the belts' mass is not in the model (Neptune 1,436 km off). Europa up to 17,700 km off (Laplace terms). Belt asteroids ignore Jupiter far from 2026.
- **source needed**: the comets' 4 AU dust switch-on (a choice since 0.78.0; a distance where water ice starts to sublimate), Haumea's shape, the belt cards' counts and masses, the Sun card's frost line and lifetime, comets active far out (Hale-Bopp, C/2017 K2).
- **source needed**: ring spectra, Pluto, the grey moons and their maps, McEwen 1996's lunar phase polynomial, the giants' limb darkening, photometric functions for Mercury and Pluto, Neckel & Labs 1994 for the Sun's edge.
- **source needed**: the full papers for the rotation periods (Lamy et al. 2025 Uranus, Karkoschka 2011 Neptune: only abstract or news saved).
- Speed: comets' dotted orbits, the Sun's path in Real, moons' orbits and spin axes are still drawn by the CPU's LinePen. In real_fast the frame thread waits for the integrator (NBodyClock::ensure).
- Saturn close up, the night sky as a retained GPU mesh, Delta T after the last leap second and UT1 - UTC.
- Eclipses: **source needed** a check against NASA's Five Millennium Canon. The red eclipsed Moon, shadows on and of moons, the giants' flattened shadows.
- Earth's air: multiple scattering, the glow beyond the edge, twilight. **source needed**: a measured ocean colour.
- **fix**: Earth fully lit is drawn at 0.21 of the measured 0.434 (0.77.0: the ground 0.06, clouds as white cover, single-scattering air). Missing: the clouds' own measured albedo and depth, multiple scattering. **source needed**: a cloud albedo or optical-depth map.
- **source needed**: calibrated reflectance maps for the moons, Pluto and the Moon (the USGS mosaics and the Moon Kit are display pictures: only their mean brightness is measured since 0.77.0) and phase functions with the opposition surge (Europa, Rhea and Triton are brighter than any matte ball can be).
- **fix**: Pluto's map bytes are a linear stretch of the measured values (its USGS page), decoded as sRGB: decode them linearly (its contrast changes).
- The far markers are hidden by balls and rings since 0.77.0, but drawn over the orbit lines and trails in front of them.
- Celestial mechanics in the Data panel: ephemeris tables, an integrator drift check, Lagrange points, Kirkwood gaps, a Lambert solver with porkchop plots.

## Black Hole Lab
- **source needed**: a colour temperature (or spectrum) for each pixel of the Gaia sky map, so the held camera's own shift can change the map's colour and light as it does the stars' (0.84.0: the map is not shifted; shown only with Sky beside the disc: Shown).
- **fix**: the GPU light paths crash the owner's Radeon driver when their pipeline is made (off there by the guard). The crash is in the "step" part of kKerrTableCS: split it into smaller parts.
- rest.blackhole_glare is busy at rest on the owner's PC (45 % CPU). Suspected cause: the moving gas remakes the glare on the CPU. Check his next run with the gas off.
- Scenes below 255 Hz in the owner's 0.68.1 run: film_tilt, real_move, extreme, all_light (1 % low 152-172). Their slow frames mostly wait for a swapchain image (6-9 ms, 2 images). Measure first.
- **fix**: the CPU spin table (the fallback) is off on the far disc nearly edge on: the exposure's log-average 1.4 % at spin 0.9, up to 33 of 3871 pixels another image at spin 0.99.
- **fix**: the CPU path seen nearly edge on still changes the disc at the stop (largest patch up to 132 px). Next try: more columns inside the shadow only.
- The ring round the shadow: the sky in the gap is drawn at the map's brightness beside a disc on its exposure. Say so in the guide, or dim that sky by the disc's exposure.
- The Milky Way looks grainy close in (Gaia's map is 4000 x 2000). **source needed**: ESA's larger Gaia map and its terms.
- The stars' images on the GPU (they trail the sky while moving). Needs the tables sent per picture, analytic derivatives and its own limits.
- The GPU sky pass per new picture, faster: one duplication shared by the two R_J of the same amplitude, Carlson's stop at 0.03.
- Quality levels High and Ultra (only Basic exists).
- Real light: the stars' own glare, colour and adaptation in the glare, a calibrated sky map (**source needed**).
- The realistic gas disc (native/PLAN_DISC.md section 9, the owner's order): Sagittarius A*'s hot flow waits (the **source needed** line below).
- Film's see-through disc, what is left: the stars on the CPU table's picture (the fallback) stay hidden behind the whole disc (the GPU sky pass lets them show through it).
- Tilting at spin on the CPU table (the owner's path): each new tilt refills the coarse table on a worker (build machine, 0.81.0's real_move: 27 ms a fill, 47 worst; the whole new picture about 8 a second), and the fine one comes 2.2 s after a tilt (response.blackhole_tilt, 4 cores). The frame itself is not held up (0.02 ms). Faster needs a cheaper table or the GPU light paths back on his card (the **fix** above). Measure on his PC with blackhole.circle_spin first.
- After 0.83.0 (the owner, 24 Nov 2026): a research job for Sagittarius A*'s hot flow: the missing source (BL06's definition, or another fitted model), tested against the 1.3 mm and 2.2 um checks; drawn only if both pass.
- Moving gas, what is left: the streaks and the clump on the CPU path (the fallback draws the surface but no moving gas), a check of the clump's light curve against a published curve.
- The disc's thickness round a spinning hole: Zhu's R_z spin term (their (6), a^2 (u_t - 1) as printed) checked against Abramowicz et al. 1997 (**source needed**: not saved). Until then a spinning hole's disc is drawn in its plane.
- The camera inside the disc's thickness (|z| below h at its own radius, for example 57.5 GM/c² and 0.2 degrees from the plane) is drawn like any other view and equals the CPU reference, but the gas there is optically thick and the lab has no model of what is seen from inside it. **owner decides**: keep the camera outside the thickness (with a line in the app), or draw it as it is.
- The static near the disc's plane was a float error in the GPU's coverage window (REPORT 0.82.1). It was not seen on the owner's Radeon at 1000 GM/c² here (no Radeon): check his next run at pitch near 90 degrees.
- The disc's surface: rays that graze it without crossing the plane near it are not drawn (0.3 % of the disc's pixels at 20 M, 5 degrees above; REPORT 0.79.0).
- **source needed**: Sagittarius A*'s hot flow (src/blackhole/hot.h, 0.80.0: tested, nothing drawn). BL06 as printed passes both checks (1.3 mm 3 Jy +- 30 %, Broderick et al. 2011; 2.2 um 0.28-6.9 mJy, GRAVITY 2020) under no reading: lengths in M, 0.3 of the energy 4.54 Jy and 0.155 mJy, of the field 2.13 Jy and 0.040 mJy; lengths in r_S 19.8 Jy and 2.77 mJy, 13.4 Jy and 0.70 mJy. Its near infrared is 20 times below BL06 Fig. 1's own curve; Yuan, Quataert & Narayan 2003 and Broderick & Loeb 2005a do not close the gap. Needed: BL06's definition of its non-thermal electrons or its spectrum in numbers (Broderick & Blandford 2004, Broderick's 2006 thesis). The GPU table, the Hard scene blackhole.hot_flow and the guide text wait for a model that passes both checks.
- **source needed**: a semi-analytic model of M87*'s flow with fitted numbers (Broderick & Loeb 2009, ApJ 697, 1164, is a candidate). T_eff inside the ISCO at 0.1 L_Edd. The hot flow's structure strength (to measure from the Illinois GRMHD data, CC BY 4.0). A camera or lens spread function with tabulated values for Film's glow.
- Next, after the moving gas: moving cameras (orbiting, falling in; **source needed** the ray equations in ingoing Kerr coordinates), the Data panel's tables, the skin. A "1979 paper" view of Luminet's figures.
- **source needed**: L79 Fig. 10's 2.62 F_S max at 10 degrees (the lab gives 2.81). The disc's total light needs Thorne 1974 (ApJ 191, 507).
- Spin, what is left: "Show special radii", spins above 0.99, a counter-rotating, tilted or warped disc, the sky's turning for the dragged camera.
- The disc's atmosphere: limb darkening by electron scattering (Chandrasekhar 1950) and the colour-correction factor (Shimura & Takahara 1995), both named in the guide since 0.77.0. **source needed**: Chandrasekhar's table and the factor's value for the lab's discs (Li et al. 2005 is saved).
- Idea, not planned: a "radio picture" look at 1.3 mm.

## Whole app
- **owner decides**: the benchmark's lag is the age of the shown picture (blackhole_view.h lagMs), not behindWorst. behindWorst counts from the first frame behind and grows for the whole movement (REPORT 0.83.0).
- A GPU timing of the Black Hole sky pass alone (timestamps around one pass), if needed again.
- The Glow and Density clouds' exposure grid is read back in the frame that draws it. A grid one frame late would remove the wait.
- The crash-safe helper: the sky's, lens's and light paths' first runs still happen in the app first. The sky's helper also makes the lens's and table pass's pipelines.
- The owner's slowest scenes (0.56.1 run, the newer runs decide): atom.slice_drag, atom.jump_all, molecule.vib_density, molecule.density, atom.nucleus_zoom, atom.nucleon, atom.all_enlarged, atom.uranium_true. Move scenes far above 255 Hz to All scenes.
- Graphics quality presets, Low to Ultra, for older PCs.
- Data panel: typed data kept between runs, a fit to the user's own formula, a setup as a file, leaving out a point, asymmetric errors, Poisson fits, AIC/BIC, Voigt, York's r, a teaching mode, BibTeX. Hydrogen against Dirac levels, Birge-Sponer from measured band origins.

## Calculator
- Matrices: eigenvectors, inverse, exponential, non-Hermitian eigenvalues, sizes above 4x4, a bare 0 beside entries with a unit.
- Uncertainties carried through, °C and °F, complex ln and sin, units like GF's in base units.
- Measuring tools on the picture (ruler, angle, stopwatch, probe). A calculator curve sent to the Data panel.
- Worked calculations: the mass formula against AME2020, heat capacity against NIST-JANAF, a Bateman decay chain. IR absorption beside measured spectra (**source needed**).

## Waiting for the owner
- Camera part 3, free flight (WASD or arrows, mouse look, speed scaling with distance). Not now (23 Nov 2026).
- After the owner's 0.63.1 benchmark: the three worst frames of blackhole.film_tilt (969 ms), orbit.heaviest (20.7 ms) and atom.nucleus_zoom (19.1 ms).
- Paused (14 Nov 2026): element jumps with every electron for the other atoms (native/PLAN_JUMPS.md).
- "Far labels" in the Orbit Lab: the owner describes what is wrong. Photon jumps from a complex state: the owner picks an option.
- **owner decides**: the Simple quark style's flux tubes inside a moving proton: remove, or keep as a labelled cartoon.
- **owner decides**: methanol's torsion is 118 cm^-1 (aug-cc-pVTZ) but 177i (aug-cc-pVDZ) at the measured shape. Calculate at the TZ minimum, or show no value.
- **owner decides**: a tone curve for the Glow core so a whole atom's outer shells show beside its core.
- Molecule realism: one fixed charge-colour scale, a deformation-density view. Maybe a fog-style cloud.
- Files the owner saves (the cloud is refused): Alpha Centauri papers (Damasso et al. 2020, Benedict & McArthur 2020, Dumusque et al. 2012, the IAU WGSN list, the IAU constellation boundaries), the island of stability, halo radii, Neckel & Labs 1994, Bardeen 1973 and Novikov & Thorne 1973.
