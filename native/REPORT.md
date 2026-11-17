# 0.57.0 (checkpoint): Part 0, the Atom Lab's two slowest scenes measured; the plain waste taken out
Based on "native 0.56.4". The card's programs come after this checkpoint; the full UI suite was not run yet.
## Part 0: where the time goes (this box: Xeon 2.3 GHz, llvmpipe; each scene alone, PL_BENCH_ONLY; pieces timed alone with the lab's own functions)
- atom.slice_drag (U-238 4f, one electron, Slice, 12,000 dots; manyPlane_ is not used: one electron). A frame's work is the new dots on the moved plane:
  kSliceDotsPerFrame (1,500) per 1/240 s physics step, so a frame of 80 ms runs ~19 steps and remakes all 12,000 dots (8 steps), a self-feeding loop.
  One step (1,500 dots) 9.90 ms = 6.6 us a dot, the same as the owner's 79.34 ms / 12,000. Inside a dot (gdb samples, 50): the state looked up by name
  (orbIndex, fullOrb, stateId) and the plane's offset 40 %; the radius by Newton's inversion of the tail (radiusX, exp) 54 %. The contour's values are on
  the card (processor reference 1.0 ms for 200 x 200), its marching squares 0.10 ms; the 12,000 dots' upload is small.
  0.55.5 -> 0.56.4 (65.8 -> 79.3 ms): git shows no change to the Slice's dot code since 0.55.5 (cloud_sample.cpp, atom_sim.cpp untouched); both frames
  remake the whole 12,000 a frame. The benchmark itself was rebuilt in 0.56.1 (ui/benchmark.cpp, 1,122 lines); the gap is not explained by the atom code.
- atom.jump_all (Ra-226, every electron, 2 x 240,000 dots): the GPU cloud buffer is not used during a jump (atom_view.cpp: gpuCloud needs !jump), so the
  processor does everything: each dot's group (AtomLab::jumpGroups, many path: jumpGroup dot by dot, the share's exp/expm1 again for every dot) 39.3 ms,
  projecting 3.7, building glow dots 1.7, blobs + exposure 5.3, the Glow core's pass 3.4, the copy of old + new dots 1.1 ms; then 19.2 MB of blobs sent
  a frame. The owner's 48.6 ms is mostly the groups (~70 %).
- atom.all_enlarged's 5 ms: the colour groups of 240,000 dots (plainGroups), the Glow core's pass over every dot (glowDotsNear, 1.4 ms here after the fix
  below) and the exposure grid read back each frame.
## Plain waste taken out (no shader; the same results, bit for bit, tested)
- cloud_sample: PlaneSampler (planeSampler, tryPoint): the plane's tail G(x0), the orbital and the bound once per batch; AtomLab::addDots uses it on the
  Slice and looks up the state once per batch (idNow_, also for recordDot's colour). One step 9.90 -> 4.66 ms.
- AtomLab::jumpGroups, the every-electron jump: the share and the mode once, not per dot: 39.3 -> 5.1 ms.
- atom_view.cpp: the jump's old + new dots put together once per jump (jumpEpoch), not every frame (1.1 ms).
- cloud_dots.cpp glowDotsNear: a single-precision pass first that can only drop dots a point beyond the largest disc: 3.35 -> 1.27 ms (all scenes).
- Tests (atom_tests testPlainWaste057, written first): PlaneSampler = tryStateOnPlane on 1.6 million attempts (8 states, 2 axes, 5 planes); a Slice step's
  1,500 dots = tryStateOnPlane's one by one with their signs; Ca's level jump: jumpGroups = jumpGroup for every dot, Sign and Wheel, 6 moments; the Glow
  core's dots = the old full pass at 24 cameras. check.sh quick: 18/18.
