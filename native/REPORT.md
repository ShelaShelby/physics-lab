# 0.66.0: speed batch 3 (based on 0.65.0; box: 4 cores, Mesa 25.2.8, lavapipe; "processor ms" = the benchmark's CPU work per frame, same box, both builds in one session; its frames are slow (lavapipe), so each runs up to 12 physics steps)
## A. The Slice of a Dirac state without stalls (atom/atom_sim.cpp oweOrMake, makeOwedDots; main.cpp)
- One rule for every Slice (plain, superposition, every electron, Dirac): a physics step (or a paused frame) only owes its new dots; after the frame's steps
  makeOwedDots makes them in order until 1.5 ms is spent (the clock read every 128 attempts; the Dirac sampler in batches of 128 dots). The same dots in the
  same order (an attempt takes only its own numbers), spread over more frames; at most 240 steps owed. PL_FIXED_DT runs (the checks) make them at once, as before.
- Test first (app_tests testSliceBudget; 0.65.0 has no budget: 18,000 dots a frame): a fake clock of 1 us a dot, 3 scenes (Dirac, every electron, one
  electron), 10 frames of 12 steps after the cut moved: most dots a frame 1,536 (budget 1,500 + one check), 0 frames over, then dots, count and generator
  identical to the steps made at once.
- Plain waste: the Dirac sampler's bisection stops where its interval no longer changes (the same numbers: hashes of 240,000 dots equal); 1.12 -> 1.04 us a dot.
- atom.slice_dirac_drag: processor ms 20.4 -> 3.1; its worst frames' frame thread 24.9 -> 3.2 ms. The sampler's card version stays on ROADMAP.
## B. The nucleus views drawing less (atom/atom_view.cpp; blob kind 10 in shaders.h kBlobVS/kBlobFS, Renderer2D::glowStacks; both APIs)
- Measured: almost all the pixels were the nucleons' glows in the float layer: per nucleon a kind-1 profile and up to 7 kind-4 Gaussians, each its own quad,
  at full zoom each over the whole picture. Now one quad a nucleon (a glow stack: the shader sums the same glows; the table in uSph), a stack whose light on
  the picture is under its share of half a level is left out, and a quad reaches only as far as its light is above that share.
- Counts (PLDRAWSTAT ncover: quads' area on the picture, square points; 1280 x 800 = 1,024,000), before -> after: atom.nucleus_zoom 86 -> 16 blobs,
  67,584,000 -> 16,384,000 (66 -> 16 pictures); atom.nucleon 5,800 -> 5,562 blobs, 46,068,407 -> 9,100,814 (45 -> 8.9); Og-294 large nucleons (scene 3)
  20,400,314 -> 14,082,857. Draw calls (5, 7) and layers (2) unchanged.
- Check (tests/check_6c.py nucglow, in ui_atom; limit set first: at most 1 % of pixels over 8 levels, Vulkan, multisampling off; cover at nucleus_zoom at
  most 30 % of the old): 0 pixels over 8 levels in all three views (largest difference 1 level); 24 %. OpenGL (same shader text): 0 levels apart at scene 30.
  VALIDATE_SYNC=true: 0 messages in 2 scenes. No new card program (the blob program's own start-up check stays its guard).
## C. The processor-bound scenes (processor ms 0.65.0 -> 0.66.0, one session; this box's numbers move 5-15 % run to run)
- atom.nucleus_density 4.29 -> 3.65: the line-of-sight sums, made again every frame while it turns, shared by 4 threads (the same numbers; 1.1 -> 0.75 ms here).
- molecule.density 4.44 -> 4.04: the surface's triangles by a radix sort (molecule/depth_order.h; 0.81 -> 0.12 ms for 14,128; one order for equal depths;
  app_tests testDepthOrder against std::stable_sort, 6 sizes, 0 differ). Left: projection, colours and 42,000 vertices a frame (ROADMAP).
- orbit.heaviest 4.51 -> 4.27, orbit.real_fast 25.8 -> 21.9: the whole orbits' sines from a table (27,000 a frame, the same numbers), Renderer2D::line without
  hypot; the draw 1.94 -> 1.80 ms (a driver of OrbitView::draw). Left: 13,000 lines a frame (ROADMAP). real_fast's frame thread waits for the integrator's
  next stretch here (the software renderer takes the cores); the integrator keeps up with one free core (2 ms of its work a physics step).
- molecule.vib_density 6.2 -> 7.2 (not changed; its first swing samples each level's 60,000 dots during the counted time): the surface made again each new
  phase (blendVib + densitySurface) is its frame's work (callgrind: the shell's realisticControls); on ROADMAP.
- New for measuring: PL_SECTSTAT=1 prints the frame thread's parts (physics, shell, picture, panels, draw, rest) every 60 frames.
## Checks
- check.sh quick 18/18 after each part; app_tests 202,212 checks 0 failed; check_6c nucglow 10/0 and fullzoom 12/0 by hand.
- Full UI suite (36 tests): 32 passed. ui_shell failed on check_shell vulkan2d (its draw-kind list wanted blob1, now drawn as kind 10): list fixed, the check
  140/0 by hand, ui_shell again: passed. Left failing as in 0.65.0: ui_molecule (check_molecule card), ui_blackhole (smooth), ui_popups (benchscenes, benchwaits).
- For the owner's PC: run the Hard benchmark with every other program closed (32 scenes); send it and gpu-paths-log.txt.
