# 0.64.0: speed batch 1 (based on 0.63.1; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe; "processor ms" = the benchmark's CPU work per frame, same box, same scene)
Bigger than described: Parts C and D each needed a new card program with its own guard, start-up check, test and UI check (as the rules ask); all done.
## A. The heavy views the benchmark missed (six new Hard scenes: app/bench_scenes.cpp, atom/atom_bench.*)
- Measured here, each at the zooms -0.5, 0, 0.5, 1.0 (the slowest kept): atom.slice_dirac_drag 85 ms (zoom 0), atom.slice_all_drag 32 (-0.5), atom.side_all_glow 24
  (-0.5; while it runs zoomed to +0.5 and back as a wheel), atom.side_all_classic 21 (-0.5, the same), atom.slice_glow_ultra 5.0 (-0.5), atom.slice_classic_ultra 4.2 (-0.5): all
  over 3.9 ms, all Hard (ROADMAP, Atom Lab, slowest first). atom.slice_drag for comparison: 3.5 ms.
- Checked, no scene: Saturn still (the same loop as orbit.saturn); the side view with GPU colours Off and the Density cloud with GPU Density cloud Off (switches for
  comparison, twins exist); the Black Hole Lab at rest, the chart, the molecules' surfaces (kept between frames).
- For the owner: the Hard run is 32 scenes, about 4 min 45 s (was about 4 min; static_assert and app_tests changed on purpose).
## B. The Slice of a superposition (atom/atom_sim.cpp, atom/atom_card.*, render/shaders.h kSliceDotsCS, render/atom_card.cpp, render/vk_card.cpp)
- Where the time went (gdb samples): the inverse transform (radiusX) and, per attempt, planeMass twice and the plane's tail again, plus the option strings
  (sliceOffNow -> sizeN -> orbIndex). Plain waste cut: both states' PlaneSamplers and the odds once per batch (test_atom: the same dots and amplitudes bit for bit).
- Then on the card with the Slice's own program (no new pipeline, guard "[vulkan] [slice]", OpenGL's twin too): uStride/uFirst; the card makes both states' attempts
  (numbers 4t+1..4t+3), the processor picks the state from number 4t in double, exactly as before. Overlapping pairs (one more number per kept dot) stay the
  processor's (ROADMAP). Dirac state and many electrons: different samplers, not this program (ROADMAP).
- Start-up check: 7 planes (2 new: Md 2p z and 5f z3 at the cut -0.5, stride 4), 28672 attempts, 0 answered differently, 0 beyond their limit (same limits).
- processor ms: 268 -> 143 (plain waste) -> 4.4-6.8 (card; lavapipe's threads share the 4 cores, so runs vary).
## C. Saturn's rings (orbit/ring_draw.h, orbit/ring_check.*, orbit/orbit_view.cpp, render/ring_card.h, kRingsFS/kRingsProbeCS, VkDraw2D kRings)
- Plain waste: bins and the ring shadow's 32 bands kept while mu, mu0 and the pixel size stay; runs of radial steps one quad; nothing beyond the picture
  (pictures: 0.001 % of pixels differ, edge rounding). processor ms 23.7 -> 15.9.
- On the card (Vulkan; OpenGL keeps the triangles): per pixel, the same 128 chords, bands, half and shadow; 4 samples averaged in the shader where the picture is
  multisampled. Guard "[vulkan] [rings]". Check (limits fixed first): 3 views of 400 x 250, one sample: 0.49 %, 0.87 % (edge-on), 0.60 % of ring pixels over
  8 levels (limit 1 %). processor ms 15.9 -> 3.5-3.8. The whole picture against the processor's of the same moment: 0.68 % of pixels over 8 levels.
## D. Every electron as Classic dots (render/classic_card.h, kClassicCS, atom/classic_check.*, atom/atom_view.cpp)
- The time: fillGroups, projectCloud, the counting sort and 240,000 squares (80 bytes each) sent every frame. On the card (Vulkan): kClassicCS (count, starts,
  scatter) writes the squares in the processor's order of (band, group) (inside one key every square is alike, so their order does not change the blend);
  drawn with the blob program. Guard "[vulkan] [classic]". Check: 6000 dots, 0 in another group or band, places within 3.1e-5 points (limit 2e-3), 0 keys out
  of order, 0 squares wrong. processor ms 21.7 -> 10.5-12.2, now the same as atom.all_enlarged (Glow, on the card since 0.46); the rest: the groups made and
  sent every frame (ROADMAP).
## Checks
- check.sh quick 18 of 18. By hand: check_atom_chip card 42/0 (the settings line's test fixed for 0.63.1's app version field: it failed 10 on 0.63.1) and classic
  4/0; check_orbit_ui rings 4/0; gl_compare_tests (PL_VK_ONLY=1, new switch): Slice 7 planes and against OpenGL 0 differ, rings 3 views, Classic: pass, 0
  validation messages. Found (pre-existing since 0.63.0, not run by any ctest): its outline, rocks and Glow grid steps wait on the guards' turn, and the Black
  Hole part's end hangs on a thread join (ROADMAP). VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true: 0 messages for slice_super_drag, saturn, all_classic.
- Full UI suite (check.sh full, 34 tests): 31 passed; failing as in 0.63.0/0.63.1: ui_popups (check_6b benchscenes: film_tilt's unchanged picture;
  benchwaits: atom_density's 15 s cap), ui_molecule (check_molecule card), ui_blackhole (check_blackhole smooth). The side scenes' zoom was widened after it
  (their picture did not change at 880 x 560 under the panels); benchscenes then by hand: 135 checks, only film_tilt failed.
## For the owner's PC
- Run the Hard benchmark when you can (32 scenes); send it and gpu-paths-log.txt (look for "[vulkan] [rings]" and "[vulkan] [classic]" passing).
