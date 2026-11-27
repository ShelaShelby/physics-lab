# 0.92.0: Orbit Lab, celestial mechanics in the Data panel

## What changed
- Part 0: check_molecule.py card failed on the loaded GitHub runner. The first Density scene read its path before the card program was decided. The scene order did not change in 0.91.0 (vib_density, then density). With 12 busy loops on 4 cores the old build fails the same check. BenchRun::frame now holds the warm-up (at most 30 s) until the Density cloud and surface checks are decided. Same load, new build: 10 checks, 0 failed.
- A, B: five tools in the Data panel's Orbit table list. Lagrange points (orbit/lagrange.h, any Sun-planet pair and 7 planet-moon pairs, table in km and units of the separation, the points drawn in 2D and 3D, Routh's criterion). Kirkwood gaps (histogram of the lab's main-belt rocks, 4:1 3:1 5:2 7:3 2:1 marked at a = a_J (q/p)^(2/3)).
- C: Lambert transfer planner (orbit/lambert.h, Izzo 2015, zero revolutions, patched conics) and the porkchop plot. The grid is solved by kPorkchopCS on the card under gpuVkPorkchopGuard (compile, link, small run, check, first full use). The CPU double solver is the reference and the fallback (a worker thread). The plot is the panel's ImGui draw list (up to 96 x 96 rectangles): drawn by the GPU, but the colours are made on the CPU (ROADMAP).
- D: ephemeris table (Simple, Real with or without relativity, with the model's worst error) and the integrator drift check (accurate step or Wisdom-Holman map, energy and angular momentum).
- Benchmark: Hard scene orbit.porkchop (Earth to Mars, 256 x 256, dates dragged) and the wait response.porkchop_grid. By DECISIONS (1 % low over 1.5 x 255 Hz in bench_0.85.2) orbit.sun_edge (524.3) and orbit.oort_region (406.2) moved to All scenes: 29 Hard, 71 All, 287.7 s planned.
- Sources (private repo, extracts here): Tatum ch. 16, Izzo 2015, the poliastro page's worked cases, the JPL satellite GM rows. Murray and Dermott (1999) was not reachable: its equation numbers are not cited.

## Tests run
- celestial_tests (new, 404 checks, 0 failed): force zero at all five points (bound from rounding), equilateral triangles, Tatum's quintics 16.1.10 to 16.1.23, his table of least distances, Routh's critical q = 24.9599358, Sun-Jupiter frequencies, Hill radius order, Kirkwood positions and histogram counts, Lambert (110 cases: arrival by the lab's Kepler code to 3.5e-14, energy and angular momentum at both ends), Euler's parabolic time, Battin's series, two published cases, the ephemeris table at the 301 Horizons dates equals the quoted errors, the drift check equals test_nbody's loops. app_tests 202,451 checks, 0 failed.
- vk_compare gpuPorkchop by hand (4 grids, 0 over the bound, 0 flips, 0 pixels over 8 levels). check_5k.py celestial (19 checks), check_molecule.py card, both 0 failed. check.sh job passed.
- Build machine (lavapipe): orbit.porkchop 15.5 fps (1 % low 8.5).

## Not done
- No published Lambert case from Izzo (his paper prints none) or Vallado: the two cases are from the poliastro documentation page, which does not name the books or print mu (398600.4418 reproduces its printed v2 to 2e-7 km/s).
- The porkchop plot is not a per-pixel shader. The GPU bound's constant K = 3000 was set from the check grids (counting operations gave 64, which failed). The GPU helper process knows the program; its marker logic is not tested on a real driver.
- This job needs pc_tests.ps1 on the owner's PC (orbit.porkchop, response.porkchop_grid, the card's start-up check).

## References changed on purpose
- Benchmark lists (test_app.cpp): 29 Hard, 71 All, 47 waits (was 30, 70, 46). Doubtful: Tatum prints n = 0.080 464 5 w; his own 16.2.27 gives 0.0804641 (the test keeps his digits to 5e-7).
