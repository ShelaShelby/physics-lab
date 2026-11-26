# 0.90.0: the rest moved to the GPU (Molecule surface, Glow core pass, exposure grid, Orbit lines, integrator ahead)

## What changed
- Feasibility (build machine, 4 cores, lavapipe; CPU time a frame): A, the vibrating surface (molecule_view.cpp blendVib + densitySurface) 1.23 ms per phase. B, atom.jump_all's glowDotsNear (cloud_dots.cpp) 1.43 ms. C, the grid wait in the batch's flush (renderer2d_vk.cpp) 8 ms (jump_all) and 36 ms (vib_density). D, LinePen lines 0.52 ms (real_fast) and 0.39-0.45 ms (heaviest), NBodyClock::ensure's wait 26 ms of a 131 ms frame (the largest share). All parts were done. Spin axes and drop lines (0.001 ms) were left on LinePen.
- Part A: kSurfaceMakeCS makes the vibrating surface on the GPU (blend, edges, prefix sums, the same tetrahedra as densitySurface) and the surface is drawn with indirect counts. Per phase 1.26 -> 0.07 ms. The CPU stays the reference and the fallback (start-up check, PL_NO_SURFACE_MAKE).
- Part B: kGlowNearCS finds the dots near the cores on the GPU. The CPU then runs the same exact double pass on those dots only, so the indices and the capped exposure are the CPU's. glowDotsNear 1.44 -> 0.0024 ms.
- Part C: the Glow and Density clouds use the exposure grid of the frame before, so the frame no longer waits for its own grid. What the user sees: the exposure is one frame late (a still view is the same picture). Wait 8.20 -> 0.00 ms (jump_all), 39.72 -> 0.01 (vib_density), 36.61 -> 0.01 (density).
- Part D: comets' dashed orbits, the Sun's path in Real and the moons' orbits go to the GPU lines (dashes in the shader, the same widths). CPU line time 0.52 -> 0.31 ms (real_fast) and 0.39-0.45 -> 0.29 ms (heaviest). The integrator now makes up to 8 stretches ahead on its worker, chained so positions are bit-exact: ensure wait 26 -> 0.21 ms, real_fast 6.6-7.6 -> 8.8 fps.

## Tests run
- vk_cards: surface made 12 cases (water, CO2, N2 at 4 phases; largest difference 2.5e-6 Å, within the derived bound), glownear (H, Na, U, Ra jump: the same index sets, exposure within the float bound), lines 21 checks with the dashed paths; all 0 failed. Before Part A, 13 of 33 surface checks failed.
- nbody_tests 59 checks (ahead = made on the frame, bit for bit), 0 failed.
- By hand: check_molecule.py cardsurface 9 and gridlate 4 (0 pixels over 8 levels), check_atom_chip.py card 18, check_orbit_ui.py cardlines 8 (at most 0.003 % over); all 0 failed.
- check.sh job passed (quick 18 of 18, ui_smoke 8.4 s). fps here is bound by lavapipe drawing: vib_density 3.8 -> 3.7, density 3.8 -> 3.9, jump_all 7.8 -> 7.9, heaviest 9.2 -> 8.9.

## Not done
- All times are from the build machine. This job needs pc_tests.ps1 on the owner's PC (molecule.vib_density, molecule.density, atom.jump_all, orbit.real_fast, orbit.heaviest, validation) on the new release.
- Spin axes and 3D drop lines stay on LinePen: about 0.006 ms a frame, and they are drawn between the balls (about 20 more draws against the 64-draw limit).

## References changed on purpose
- check_molecule.py cardsurface: the start-up check prints 2 more PLSURFACECASE lines ("made", water at phases -1 and 0.7). The plain cases are now filtered from them.
- lines_check: pollCheck returns 8 floats per piece (ends, then drawn length and dash place). New rule after a failure: a 3D segment shown by one side only passes when an end is within 4e-6 of its distance of the near plane (one moon-circle segment 100 m behind it at 13,000 km). It is counted and printed ("on its edge").
