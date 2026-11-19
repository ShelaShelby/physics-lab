# 0.65.0: speed batch 2 (based on 0.64.0; box: 4 cores, Mesa 25.2.8, lavapipe; "processor ms" = the benchmark's CPU work per frame, same box, same scene, a warm-up run first so the guards' checks are done)
Bigger than described: Part B's Glow needs its cells read back before its dots are written (a second card batch a frame); Part C's drawing serves every Slice.
## 0. The card-vs-processor comparison (tests/gl_vulkan.inc)
- Cause: the test never ended a program's first full use (the app's frame does: vkPollFirstUses), so the guards' turn stayed held; now it does (settle),
  and the Black Hole part's hang went too. New ctest gl_vulkan_cards (PL_VK_ONLY=1, pictures; PL_GL_TESTS on with PL_PICTURE_TESTS on Linux): 164/0.
## A. The every-electron groups (atom/cloud_dots.h keepGroups; vk_card.cpp sendGroups)
- The groups are kept between frames; only the dots recorded since the last frame get theirs (a standing cloud's dots are replaced a few a frame), and the
  card programs send only those, as the places. app_tests testKeptGroups: 120 frames, 3 scenes, 0 groups differ from fillGroups'; gl_vulkan_cards: the Glow
  grid with 300 new dots' groups only, then none: 0 cells beyond rounding (with the sending switched off on purpose: 283 cells wrong).
- Bugs fixed: in a Slice of every electron or of a Dirac state the Slice's program held the guards' turn for ever (a first use that never comes), so the
  Classic dots' check never ran in a benchmark after atom.slice_all_drag; the Classic program's start-up now waits while the Slice's program starts (the
  full suite's vulkan2d "card slice" found it: 3.99 % of pixels against OpenGL, now 0.002 %).
- processor ms: atom.all_classic 28.9 -> 4.1 (in the benchmark's order, so the bug's share; 0.64.0 measured it alone: 10.5-12.2), atom.all_enlarged 10.5 -> 4.7.
## B. The side view with every electron (kClassicCS's side mode, render/classic_card.h, atom/classic_check.*)
- Classic: places, groups and the counting sort on the card. Glow: the dots counted per 8-point cell on the card, the cells read back with the frame's
  grids, the exposure on the processor (glowExposure, the same function), then the soft dots written with that light. Guard: the Classic dots' ("classic").
- Check (limits set first: the Classic check's places 2e-3 points, exact groups and order; a cell off by a dot only for a dot within 2e-3 points of its
  edge): side Classic 5502 dots, Glow 5502 dots, 2028 cells: 0 beyond. processor ms: side_all_glow 23.3 -> 17.6 (A) -> 5.7; side_all_classic 20.3 -> 14.5 -> 3.6.
## C. The Slice with every electron
- Plain waste: recordDot searched the every-electron tables per dot (cloudCap, aUnit -> manyOn); samplePlane looked up each subshell's tables per dot.
  Now once a batch: the same dots bit for bit (0.64.0's atom_core against this one: Og's Slice, the Dirac Slice and a Dirac Cloud 3D, 40 steps, identical).
- The dots drawn on the card (kClassicCS's Slice mode, every Slice, Classic and Glow): each dot at its own index in dot order (the processor draws them
  unsorted). Check: Classic (vertical cut) 5020 dots, Glow (horizontal) 4996 dots and 2028 cells, 0 beyond the limits.
- The many-electron sampler stays the processor's: 0.93 -> 0.62 ms per physics step (1,500 dots); this box's frames run up to 12 steps (0.05 s cap),
  so processor ms 28.4 -> 12.2 here; its card version: ROADMAP (D's table question). slice_glow_ultra 5.0 -> 3.4, slice_classic_ultra 4.2 -> 3.2 (0.64.0's).
## D. The Slice of a Dirac state (atom/dirac_cloud.cpp)
- Where the time went (callgrind): invertTable 58 % (each of its 60 bisection steps searched the table again), the state looked up per attempt by name
  (orbIndex, string compares) 33 %. Cut: the state and cut once a batch; the segment's cubic evaluated directly; four attempts' radii side by side
  (diracPlaneDots): the same dots and numbers bit for bit (app_tests testDiracPlaneDots; the 0.64.0 comparison above); the Dirac maths unchanged.
  7.14 -> 1.62 ms per step of 1,500 dots; processor ms 76.1 -> 24.3 here (up to 12 steps a frame).
- Not on the card: u in single precision moves the radius by up to 3.5e-5 of it at the 99.99 % radius (U 4f: 1.9e-5 a0), 600 times a float place's own
  rounding (5.96e-8): a card version needs the tail inverted from 1 - u and its own derived limits and check (ROADMAP).
## Checks
- check.sh quick 18/18 after each part. By hand: check_atom_chip card 42/0, classic 10/0 (new: side view and Slice; card against processor, same
  moment: 0 pixels over 8 levels in 5 scenes); gl_vulkan_cards after each part; VALIDATE_SYNC=true: 0 messages in 6 scenes.
- Full UI suite (36 tests): 32 passed. ui_shell (vulkan2d's card slice, fixed above: the check 140/0 by hand; ui_shell again: card slice 0.002 %, but its Black Hole 'broken sky program' timed out, which passed in the full run). Left failing as in 0.64.0: ui_molecule
  (check_molecule card: its settings-line test predates 0.63.1's app field), ui_blackhole (smooth), ui_popups (benchscenes film_tilt and all_light;
  benchwaits capped under the suite's load, 10/0 alone).
- For the owner's PC: run the Hard benchmark (32 scenes); send it and gpu-paths-log.txt. "[vulkan] [classic]" should say its check passed (five cases now: Cloud 3D, the side
  view Classic and Glow, the Slice Classic and Glow); the Paths of atom.side_all_*, atom.slice_* name "the graphics card".
