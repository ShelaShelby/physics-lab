# 0.57.0: the Atom Lab's Slice drag and every-electron jump on the graphics card
Based on "native 0.56.4" (checkpoint "native 0.57.0 (checkpoint ...)" after Part 0). This box: Xeon 2.3 GHz, 4 threads, Mesa llvmpipe (software: its card times are NOT the owner's).
## Part 0 (measured first; each scene alone, PL_BENCH_ONLY; the pieces timed with the lab's own functions and gdb samples)
- atom.slice_drag (U-238 4f, ONE electron: manyPlane_ not used): all of it is the new dots on the moved plane. 1,500 dots per 1/240 s step; a slow frame runs
  more steps, so at 80 ms a frame all 12,000 are remade (self-feeding). One step 9.90 ms = 6.6 us a dot (the owner: 79.34 ms / 12,000 = 6.6 us). Inside:
  the state looked up by name per dot (orbIndex/fullOrb/stateId) and the plane's tail per attempt 40 %, the radius's Newton inversion 54 %. Contour on the
  card (its marching squares 0.10 ms), uploads small. 0.55.5 -> 0.56.4: git shows no change in the Slice's dot code; the benchmark was rebuilt in 0.56.1.
- atom.jump_all (Ra-226, 2 x 240,000 dots): the card's cloud buffer was not used during a jump; the processor: each dot's group 39.3 ms (jumpGroups, every-
  electron path: the share's exp/expm1 again per dot), project 3.7, glow dots 1.7, blobs + exposure 5.3, Glow core pass 3.4, old + new copy 1.1; 19 MB of blobs sent.
- atom.all_enlarged's 5 ms (code read, not measured on his PC): the groups pass over 240,000 dots, the Glow core's pass in double, and the exposure grid read
  back each frame (glReadPixels waits for the card's grid pass).
## Plain waste out (no shader, bit for bit; atom_tests testPlainWaste057)
- PlaneSampler (cloud_sample.h) and the state once per batch (AtomLab::addDots, recordDot): a step 9.90 -> 4.66 ms. AtomLab::jumpGroups, every electron:
  39.3 -> 5.1 ms. atom_view.cpp: old + new dots joined once per jump. glowDotsNear: a single-precision pre-pass, 3.35 -> 1.27 ms (every Glow scene).
## Part 1: two compute shaders, each under its own guard (gpuSliceGuard / gpuJumpGuard: compile, link, first small run, check, first full use)
- kSliceDotsCS: one invocation per attempt (Mulberry32 is a counter: attempt t = numbers 3t..3t+2), the radius by Newton in logarithms, own sin/cos.
  AtomLab::addDotsCard takes the kept dots in attempt order, so the cloud gets the processor's dots up to float rounding. Gate: 12,011 attempts 1.22 ms on
  llvmpipe (2.31 one thread) against the processor's 3.62 ms for the same 1,500 dots: 3.0x. The processor's part of a step after: 0.19 ms (was 9.90).
- kJumpShowCS: each dot shown when splitmix64 z >> 11 < ceil(share 2^53), an integer test equal to the processor's double one; groups fixed per jump; the
  cloud's programs read them in place (CloudGpuDraw::grpBuf). Gate: 480,000 dots 0.58 ms on llvmpipe with read-back against 3.22 ms: 5.6x, 0 differ.
- SPIR-V (plain / inlined): kSliceDotsCS 16.1 / 13.0 KB, kJumpShowCS 7.4 / 4.1 KB (known to link on his card: kKerrFS 27.3 / 19.5, kKerrSkyFS 101.4 / 62.3). Float only.
- Limits (atom_card.cpp, derived before looking): per attempt from eps = 2^-24 through log (GLSL 4.30 4.7.1), Horner with its condition number, Newton's stop,
  sqrt, the own sin/cos; a kept/rejected answer may differ only within the card's error at its margin. Start-up check here: 5 planes, 20,480 attempts,
  7,270 kept by both, 0 different answers, largest point error 0.091 of its limit; jump 80,014 groups, 0 differ.
- What the eye can meet: the Slice's new dots arrive one step (a frame) after the cut moved, as its contour already did. Nothing checks for the benchmark.
- Changed: cloud_sample (PlaneSampler), AtomLab::addDots/addDotsCard/recordDot/jumpGroups/jumpFixedGroups/jumpThresholds, cloud_dots glowDotsNear, AtomView::draw,
  Renderer2D::cloudGpu/end (grpBuf), render/atom_card.*, atom/atom_card.*, atom/atom_card_paths.*, gpu_guard, prefs, settings, Performance, benchmark paths.
## Estimate for the owner's PC (an estimate: llvmpipe is not his card)
- slice_drag: processor ~0.2 ms for the dots + the frame's rest (79.34 - 12,000 x 6.6 us leaves under 1 ms) -> well under 3.9 ms; card 0.26 ms + one
  ~10,000-attempt dispatch -> pictures at his 255 Hz expected.
- jump_all: processor ~1-1.5 ms Glow core pass (480,000 dots) + the every-electron frame's rest (all_enlarged: 4.99 ms incl. its groups and read-back) -> 3-5 ms;
  card ~3-4 ms (all_enlarged 2.59 ms for 240,000) -> 200-300 fps: may stay below 255. Left (ROADMAP): the core pass and the grid read-back.
- Tests: atom_tests testCardReference057: the 53-bit threshold = jumpUniform < share (1,632,816 cases: 408 shares on, beside and between the grid points, 2,001 dots each, both levels); the Slice comparison passes
  the processor's own answers in float and catches a 1e-3 a0 move and a flipped answer. check.sh quick 18/18.
- check_atom_chip.py card (ui_atom): both checks pass in the app, paths in the benchmark, every step logged in order, kept ok; the process killed in each of the 10
  steps: the next start runs nothing of that program, Settings names the step, kept off; a changed shader tries again; Try again rebuilds and checks. 41/41.
- Full UI suite (check.sh full): 32 of 34 at first. ui_search: check_5l's own list of Settings rows gained gpuslice and gpujump (a deliberate change; the test's
  reference fixed): passes. ui_popups: check_6b benchscenes' second pictures came after the counted time in the slow Black Hole scenes (0.56.4 failed the same):
  now 4 s counted, pictures at 0.1 and 2.4 s: passes. check_6b benchwaits' response.atom_density sits at its 15 s cap here with LP_NUM_THREADS=1 with 0.56.4 too
  (13.7-14.7 s against 14.6-15.5 s now; the card programs do not run in that scene): 3 tries, skipped, ROADMAP. ui_popups therefore still fails here.
