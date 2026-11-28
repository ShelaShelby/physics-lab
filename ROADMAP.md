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
- Slice samplers on the GPU: the many-electron sampler (hf::samplePlane with the level clouds' aligned shape and the outer electron alone). The Dirac and one-electron samplers are there.
- atom.nucleon on the GPU: its quark dots use a double-precision sine hash. A GPU version needs an integer hash on both sides (the dots then change).
- The nucleus views at full zoom fill about 16 pictures of pixels a frame (one glow quad per nucleon). One pass over the picture for all glows would fill one.
- The Slice of an overlapping superposition on the GPU: a kept point takes one more random number, so attempts have no fixed numbering.
- **fix**: levels with no cloud (0.91.0 tried up to 11 starts per orbital, with damping and highest-J blocks): Ca 3d4s ³D and Ba 6s5d (6-10 % low: core-core correlation is left out), Ca 4p² ¹D (not identified, 0.30 of its weight). ns(n+1)s levels come out 7-42 % high (Hg 6s7s, Ra 7s8s 7s9s, Cd 5s6s, Ba 6s7s): the RCI spreads them over the correlation s orbitals. No Dirac-Fock orbital with the right node count for Hg 7p 6d 8p 5f, Cd 5d 6d 7d, Ra 8p. Ra 7s5f is +7.1 %.
- **source needed**: charge profiles for He-4, Li, Be, B, N-14 (de Vries 1987). Ra pear shapes (Butler et al. 2020), alpha clusters in Be-8, C-12, O-16, the deuteron's nucleon centres (Forest et al. 1996).
- **source needed**: measured B(E2) for unstable even-even nuclei (Pritychenko et al. 2016) and measured signs for the 29 nuclei on FRDM's sign and the 16 with none.
- **source needed**: Ga-73 ground-state spin (IAEA 3/2-, NUBASE2020 1/2-): Cheal et al., PRL 104, 252502 (2010).
- **source needed**: Geiger and Marsden's "about 1 in 8,000" may be platinum, not gold (Proc. R. Soc. A 82, 495, 1909).
- **source needed**: measured Landé g for Li, Rb, Cs, Fr, He, Be, Mg, Ca, Sr, Cd, Ra (NIST ASD lists none; Na 3P is checked against Arimondo et al. 1977 via Steck since 0.96.0).
- One-electron atom: **source needed** the Lamb shift for n >= 3. Dirac clouds: 90 % outline, superpositions, m != j. Hyperfine beyond 1s.
- Ions between neutral and one-electron (tools/dhf). Clouds for the 7s and 7p shells of the heaviest atoms. More than 240,000 dots for heavy atoms.
- The photon's dipole pattern for the many-electron atoms (the lab does not follow m or M there) and the light's polarisation.
- Atoms in fields (rest): a broadened synthetic spectrum, a Boltzmann-plot temperature, the cloud of one M sublevel.
- Data tables: <r>, <1/r>, <r²>, P(r), Q(r) of the DHF orbitals (**source needed**: Desclaux's tables), higher alkali Rydberg levels (new ASD fetch), Q-values (AME2020).
- Labels drawn in depth order (needs a renderer change).
- Benchmark: atom.bfield_drag was dropped in 0.83.0. The field slider remakes nothing heavy (only the readout, the spectrum's lines and the Zeeman tables).

## Molecule Lab
- The Density surface: each new level's 60,000 dots are sampled once on the CPU.
- **fix**: the Density cloud alone (Show: cloud) clips the nuclei's cores white. **source needed**: Bader et al. 1987 (JACS 109, 7968) for the surface's cut-off.
- **source needed**: HCl's reference dipole 1.093 D looks like mu_e (mu_0 is 1.1086 D). Check what CCCBDB means.
- **source needed**: measured band origins of H2, N2, O2 for Birge-Sponer and levels above the fitted range (O2: Creek & Nicholls 1975).
- Rotation of non-linear molecules, the IR bands of CO2, C2H2, HCN, Raman lines, O2's triplet splitting. **source needed**: water's pure-rotation band (HITRAN or JPL).
- Moving density for the other molecules, point groups with their axes, HOMO and LUMO. More molecules with S, F, P (CCCBDB route).
- The look of the bonds (Ball-and-stick fills a third of the picture). Vibrations as a coherent state.
- Thermochemistry from partition functions (**source needed**: NIST-JANAF), equilibrium constants, isotope substitution, Franck-Condon factors.

## Orbit Lab
- The stars on the exposure (0.86.0) in the Alpha Centauri scene too (it keeps its own lighting and the stars' zoom rule). The Milky Way under True sunlight: **source needed** a calibrated surface brightness of the Milky Way (the Gaia map has none), then draw it on the same exposure. 0.96.0 search: GAMBONS (Masana et al. 2021, arXiv:2101.01500, Gaia and Hipparcos integrated starlight in V and other bands; abstract saved in data/stars) is the candidate: get its maps and their terms.
- **source needed**: the penumbra's contrast against wavelength (0.94.0 uses Mathew et al. 2007's one wavelength, 676.8 nm, in every channel; 4 searches, REPORT 0.94.0). Candidates not reachable: Kjeldseth-Moe & Maltby 1969 (ADS 504), Allen's Astrophysical Quantities.
- The Greenwich rows of 1874-1976 carry each group's measured umbral area: pack it into the sunspot table and use it in place of Jha et al.'s mean split for those years.
- The umbra is drawn at the dark core of large spots (Maltby et al. 1986): Mathew et al. 2007 Table 2 gives the mean umbra against umbral radius at 676.8 nm (a size-dependent umbra needs its wavelength shape: **source needed**).
- NASA's photo of the Sun: no download on Linux (no HTTP library in the build). The granulation's centre-to-limb change of contrast is not drawn (**source needed**).
- **owner decides**: the brightest granules near the Sun's middle are past the screen's white and are clipped (the disc centre is set at the white since 0.85.0).
- **source needed**: the Oort cloud model's inner part as numbers (its lean towards the ecliptic: Vokrouhlicky et al. 2019 give it only as Fig. 7). Duncan, Quinn & Tremaine 1987 (ADS refused: 500, 504). Until then the dots' directions are spread evenly (the card says so).
- **source needed**: the Debrecen Photoheliographic Data (DPD, 1974 on) for the spots' layout inside each group (each spot's position and area). The row format is saved (0.96.0, data/sun/debrecen); the catalogue files (NOAA NCEI or fenyi.solarobs.unideb.hu) are not.
- The sunspot groups turn as one body within the half day to the shown time; differential rotation and the spots' evolution are not drawn. NOAA's SRS after the last saved date needs a new fetch (the table ends 24 Nov 2026).
- **fix**: orbit.saturn_real_fast on the owner's PC: 1 % low 253.2 fps (0.85.1: 230.6; below 255 Hz), limited by both CPU and GPU (bench_0.85.2.txt).
- **source needed**: a measured phase law for each main ring (A, B, C, Cassini Division; brightness against phase angle up to the forward-scattering side), to replace the rings' even scattering (0.78.0 item 2, skipped after 2 tries: Déau et al. 2009, arXiv:0902.0289, has the opposition surge only; the UVIS power law is far-UV only; the 0.96.0 search found only Déau again and Hedman & Stark 2015, arXiv:1508.00261, for the dusty G and D68 rings).
- **source needed**: a published syndyne or synchrone with readable numbers (Finson & Probstein 1968: the ADS scan did not load on 2026-11-24) to check one dust tail curve. The comets' dust: grain ejection speeds (the tail's width) and the gas tail's length are not modelled.
- **source needed**: the comets' 4 AU dust switch-on (a choice since 0.78.0; a distance where water ice starts to sublimate), Haumea's shape,, lower than Cassini's for the B ring and the Cassini Division. Check against Colwell et al. 2010.
- **fix**: a released turn coasts up to 6 % past its stated most (kMomentumMax / kMomentumFade = 0.33 rad) at 60 Hz, 1.5 % at 255 Hz: it steps before the fade. The Black Hole Lab uses the exact step (orbit/camera_motion.h momentumCoastSeconds); the Orbit Lab was kept as it is (0.82.0's stop rule).
- **source needed**: JPL's table (read 2026-11-23) is far from Horizons for Rhea and Titan (mean longitude 157 deg, node 134 and 54 deg), Enceladus (mean longitude 5.9 deg), Triton (inclination 0.5 deg, period 5.8792 d against 5.8766) and Deimos (period 1.2625 d against 1.262441). Check against a newer table or the SAT and URA kernels. Details in REPORT 0.88.0.
- Outside 1800-2100 the big moons cast no shadow on their planet (satState has none there): add them from satPlace.
- A Field of view setting for the 3D camera (45 degrees now, orbit/camera.h). A pick of a closer body lowers the zoom to its cap at once.
- Alpha Centauri (PLAN_ALPHACEN.md): the Sun's pick looking back in 2D, A and B seen from Proxima b, Fit planets with the card open, Proxima's orbit time on its card and a faster time choice, A and B as one label when close.
- Real mode: the belts' mass is not in the model (Neptune 1,436 km off). Europa up to 17,700 km off (Laplace terms). Belt asteroids ignore Jupiter far from 2026.
- **source needed**: the comets' 4 AU dust switch-on (a choice since 0.78.0; a distance where water ice starts to sublimate), Haumea's shape, the belt cards' counts and masses, the Sun card's frost line and lifetime, comets active far out (Hale-Bopp, C/2017 K2).
- **source needed**: ring spectra, Pluto, the grey moons and their maps, McEwen 1996's lunar phase polynomial, Uranus's and Neptune's limb darkening in numbers (Irwin et al. 2021, 2022 give figures only; Jupiter's and Saturn's are saved, PLAN_REVIEW.md R14), photometric functions for Mercury and Pluto.
- **source needed**: the full papers for the rotation periods (Lamy et al. 2025 Uranus, Karkoschka 2011 Neptune: only abstract or news saved).
- Spin axes and 3D drop lines are still drawn by the CPU's LinePen (about 0.006 ms a frame). They are drawn between the balls, so the GPU needs about 20 more draws.
- Saturn close up, the night sky as a retained GPU mesh, Delta T after the last leap second and UT1 - UTC.
- Eclipses: **source needed** a check against NASA's Five Millennium Canon. The red eclipsed Moon, shadows on and of moons, the giants' flattened shadows.
- Earth's air: multiple scattering, the glow beyond the edge, twilight. Ozone and aerosol (sources saved: PLAN_REVIEW.md R13). **source needed**: a measured ocean colour, a standard ozone column and profile.
- **fix**: Earth fully lit is drawn at 0.21 of the measured 0.434 (0.77.0: the ground 0.06, clouds as white cover, single-scattering air). Missing: the clouds' own measured albedo and depth, multiple scattering. **source needed**: a cloud albedo or optical-depth map (0.96.0 search: NASA Earth Observations has MODIS monthly cloud FRACTION maps, page saved in data/planets/earth; CERES is the next place to look).
- **source needed**: calibrated reflectance maps for the moons, Pluto and the Moon (the USGS mosaics and the Moon Kit are display pictures: only their mean brightness is measured since 0.77.0) and phase functions beyond the opposition surge (Europa, Rhea and Triton are brighter than any matte ball can be; the surge near opposition: Deau et al. 2009, saved, PLAN_REVIEW.md R14).
- The far markers are hidden by balls and rings since 0.77.0, but drawn over the orbit lines and trails in front of them.
- Celestial mechanics, what is left (0.92.0): the porkchop plot is drawn by the Data panel's draw list (up to 96 x 96 rectangles a frame, rasterised by the graphics card): a per-pixel shader on the card's own grid buffer would draw all 256 x 256 cells. Lambert with one or more revolutions, and the planets on the Real ephemeris (the grid uses Simple). Capture and escape orbits at the planets (patched conics stop at v-infinity). Lagrange points: the pairs among dwarf planets (Pluto - Charon, no named GM saved yet), the points drawn for the Real Moon's true place. Kirkwood: the resonances of the other planets' belts; the Kuiper belt's Neptune resonances. Ephemeris table: moons, dwarf planets, comets. Drift check: the accurate step with the Earth-Moon split (its energy is not conserved by design).
- **fix**: the benchmark's ui.data_plot scene never draws the Data panel (dataPanelDraw returns early while a benchmark runs). 0.92.0 draws it for orbit.porkchop only (D.benchDraw); ui.data_plot measures the picture behind a hidden panel.
- **source needed**: Tatum's Celestial Mechanics ch. 16 prints n = 0.080 464 5 w for the Sun and Jupiter (16.2.27 with q = 1047.35 gives 0.0804641): a second printing or the book itself. Murray and Dermott 1999 (ch. 3) was not reachable: its equation numbers are not cited.

## Black Hole Lab
- The disc's real light (0.87.0), what is left: the returning light's push on the gas (Cunningham 1976 (8b), needs his T_s, typed in the extract), the falling gas inside r_ms shining by the light it gets back, and the limb darkening on the thick disc's tilted surface (now against the plane).
- **fix**: the camera curve's picture at the closest distance with no spin (toneCompare case close) was 1.99 % over 8 levels on the owner's Radeon (pc_0.85.1, pc_0.85.2). 0.87.0 found a likely cause: grazing rays at the picture's corners sweep more than 13 GM/c² of radius a pixel, so the shader's coverage takes part of the pixel off the disc; the check now leaves those pixels out (REPORT 0.87.0). Confirm on the owner's PC (pc_tests.ps1); if it still fails, find the AMD difference.
- **fix**: the bent sky at 27 GM/c², 0.1 px or more from the shadow's edge, is 2.795e-5 rad off the double reference on the owner's Radeon (limit 1e-5; pc_0.85.1 and pc_0.85.2 the same; PLBHCMP dir1 4.761e-06, dir001 1.920e-04). The logs give no cause.
- **fix**: ui.resize_blackhole: 0.89.0 made the swapchain cheaper and the grey picture lazy. The six images that hold pixels at the window's size are still made at each new size; keeping them at the largest size and drawing into a part of them would save that (needs the passes to take a sub-rectangle). Times on the owner's PC: pc_tests.ps1.
- **source needed**: a colour temperature (or spectrum) for each pixel of the Gaia sky map, so the held camera's own shift can change the map's colour and light as it does the stars' (0.84.0: the map is not shifted; shown only with Sky beside the disc: Shown).
- **owner decides**: rest.blackhole_glare with the moving gas counts as busy only because the gas is drawn anew each frame (0.85.5: 1.6 % CPU, 754 redraws; gas off: quiet). Keep it in the rest list, or rest it with the gas off.
- **fix**: the spinning hole's sky per pixel (kKerrSkyFS) is still the limit on the owner's PC (0.85.3: about 6 ms on the card for a 2560 x 1369 picture, 110-135 fps, 1 % low 55-64; the owner chose no lower resolution while moving, DECISIONS 17 Nov 2026). 0.95.0 on the build machine (1280 x 800): the shared duplication and Carlson's stop at 0.03 made the pass 31-33 % faster with the moving gas and 8 % without, then the disc's surface took more than that back: against 0.94.1 +9-10 % with the gas, +84 % without (REPORT 0.95.0). Measure it on the Radeon (pc_tests.ps1 3c.bh.skytime). What is left: the integrals and crossings (73 % of the pass before the surface), the surface's F and Jacobi amplitude per crossing, the five amplitudes a ray (amAGM), and the timed crossings behind an opaque one. Its size: 100,968 bytes inlined, 233 under the limit of tests/check_shaders.py (0.95.0). The next addition to it needs room first.
- **fix**: the CPU spin table (the fallback) is off on the far disc nearly edge on: the exposure's log-average 1.4 % at spin 0.9, up to 33 of 3871 pixels another image at spin 0.99.
- **fix**: the CPU spin table's disc surface (the fallback, and kKerrFS) is the line through each crossing with its slope interpolated over the table: against the exact surface of the sky pass, 1.3 % of the disc's pixels over 8 levels at spins 0.5 and 0.99 from 20 GM/c^2, 5 degrees above (toneCompare thick05, thick99 with the table as the reference; 0.95.0). Storing each node's surface hit needs the table per feeding rate and mass.
- **fix**: the CPU path seen nearly edge on still changes the disc at the stop (largest patch up to 132 px). Next try: more columns inside the shadow only.
- The ring round the shadow: the sky in the gap is drawn at the map's brightness beside a disc on its exposure. Say so in the guide, or dim that sky by the disc's exposure.
- The Milky Way looks grainy close in (Gaia's map is 4000 x 2000). **source needed**: ESA's larger Gaia map and its terms.
- The stars' images on the GPU (they trail the sky while moving). Needs the tables sent per picture, analytic derivatives and its own limits.
- Quality levels High and Ultra (only Basic exists).
- Real light: the stars' own glare, colour and adaptation in the glare, a calibrated sky map (**source needed**).
- The realistic gas disc (native/PLAN_DISC.md section 9, the owner's order): Sagittarius A*'s hot flow waits (the **source needed** line below).
- Tilting at spin on the CPU table: the owner's Radeon keeps the light paths off since 0.85.4 (a whole fill took 1,964 ms: its double precision is slow), so the stars' table is the processor's there. Measure response.blackhole_tilt on his PC.
- After 0.83.0 (the owner, 24 Nov 2026): a research job for Sagittarius A*'s hot flow: the missing source (BL06's definition, or another fitted model), tested against the 1.3 mm and 2.2 um checks; drawn only if both pass.
- Moving gas, what is left: the streaks and the clump on the CPU path (the fallback draws the surface but no moving gas), a check of the clump's light curve against a published curve.
- **owner decides**: the dotted curve of the 0.93.0 screenshots (27 Nov 2026). 0.94.1 found one thin dotted curve near the plane: the disc's images of order 1 and 2 along the shadow's edge, thinner than a pixel, in the CPU reference too (real, explained in the guide). If his curve is another one (a third of the shadow's width), the owner sends its settings: distance, tilt, spin, look.
- **fix**: the Black Hole Lab at rest uses 58-62 % of a thread on the owner's PC (0.93.0; 19-22 % in 0.85.2). Not seen on the build machine (0.85.2 and 0.94.1 equal there). Measure on his PC with pc_tests.ps1 after 0.94.1.
- The disc's surface: rays that graze it without crossing the plane near it are not drawn (spin 0: 0.3 % of the disc's pixels at 20 M, 5 degrees above, REPORT 0.79.0; at spin the same rule since 0.95.0).
- **source needed**: Sagittarius A*'s hot flow (src/blackhole/hot.h, 0.80.0: tested, nothing drawn). BL06 as printed passes both checks (1.3 mm 3 Jy +- 30 %, Broderick et al. 2011; 2.2 um 0.28-6.9 mJy, GRAVITY 2020) under no reading: lengths in M, 0.3 of the energy 4.54 Jy and 0.155 mJy, of the field 2.13 Jy and 0.040 mJy; lengths in r_S 19.8 Jy and 2.77 mJy, 13.4 Jy and 0.70 mJy. Its near infrared is 20 times below BL06 Fig. 1's own curve; Yuan, Quataert & Narayan 2003 and Broderick & Loeb 2005a do not close the gap. Needed: BL06's definition of its non-thermal electrons or its spectrum in numbers (Broderick & Blandford 2004, Broderick's 2006 thesis). The GPU table, the Hard scene blackhole.hot_flow and the guide text wait for a model that passes both checks.
- **source needed**: a semi-analytic model of M87*'s flow with fitted numbers (Broderick & Loeb 2009, ApJ 697, 1164, is a candidate). T_eff inside the ISCO at 0.1 L_Edd. The hot flow's structure strength (to measure from the Illinois GRMHD data, CC BY 4.0).
- Next, after the moving gas: moving cameras (orbiting, falling in; **source needed** the ray equations in ingoing Kerr coordinates), the Data panel's tables, the skin. A "1979 paper" view of Luminet's figures.
- **source needed**: L79 Fig. 10's 2.62 F_S max at 10 degrees (the lab gives 2.81). The disc's total light needs Thorne 1974 (ApJ 191, 507).
- Spin, what is left: "Show special radii", spins above 0.99, a counter-rotating, tilted or warped disc, the sky's turning for the dragged camera.
- Idea, not planned: a "radio picture" look at 1.3 mm.

## Whole app
- **fix**: vk_compare_tests on the owner's Radeon: Glow soft dots, Wheel, dark page (additive) differ by 2 levels (585 of 200,000 pixels; the limit is 1). The rocks' regression guard (llvmpipe's measured 0.00357 points + 1 %) is exceeded there: 0.00578 points in scenes 1 and 6 (the stated bound kRockPx 0.01 holds).
- **owner decides**: the benchmark's lag is the age of the shown picture (blackhole_view.h lagMs), not behindWorst. behindWorst counts from the first frame behind and grows for the whole movement (REPORT 0.83.0).
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
- **owner decides**: Most realistic opens the Atom Lab at the Fixed scale (since 0.96.0 every lab opens most realistic), where a small atom's cloud is drawn small. Keep it, or open at Auto.
- **owner decides**: methanol's torsion is 118 cm^-1 (aug-cc-pVTZ) but 177i (aug-cc-pVDZ) at the measured shape. Calculate at the TZ minimum, or show no value.
- **owner decides**: a tone curve for the Glow core so a whole atom's outer shells show beside its core.
- Molecule realism: one fixed charge-colour scale, a deformation-density view. Maybe a fog-style cloud.
- Files the owner saves (the cloud is refused): Alpha Centauri papers (Damasso et al. 2020, Benedict & McArthur 2020, Dumusque et al. 2012, the IAU WGSN list, the IAU constellation boundaries), the island of stability, halo radii, Bardeen 1973 and Novikov & Thorne 1973, Chandrasekhar 1950 (Radiative Transfer, Table XXIV: the exact electron-scattering limb darkening), the Debrecen Photoheliographic Data (individual spots 1974-2018; the site refused, 2 tries), Shettle & Fenn 1979 (AFGL-TR-79-0214; DTIC refused twice) and OPAC (Hess, Koepke & Schult 1998, BAMS; refused once).
