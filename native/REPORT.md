# Native Physics Lab 0.13 (alpha) — stage 3 of 4: the 90 % outline on the GPU, exact orbital normalisation

`index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push; the Actions result is in the closing message.
**SPEED IS NOT TESTED.** No real GPU and no Windows here. The improvements are *expected, not measured*. A real driver may round floats differently from llvmpipe. Whether a real PC gets 4.3 or the fallback is untested until you run it.

## Part A, step 1: measured first (CPU, this machine, ms, `tests/timing_stage3.cpp`, relative guide only)
| first use of an outline | density grid 64³ | sort for the 90 % level | norm | rest (lines) | total |
|---|---|---|---|---|---|
| 4f z³ / 3d z² / 2p z / 1s | 13.0 / 12.2 / 11.5 / 10.0 | 16.9 / 15.6 / 16.4 / 12.0 | 1.0 / 0.9 / 1.3 / 0.7 | ≈1 | 30.4 / 30.3 / 29.2 / 23.6 |
After part B the norm is 0.00 ms (closed form). The level step itself (the running sum) is 0.03 ms. **Slice (scene 17), one frame:** 1.7 ms of density values in 1.9 ms (marching squares 0.2 ms). So the hitch is the grid (43 %) and the sort (56 %); the lines are cheap and stay on the CPU (0.9–1.1 ms, measured as `outlineFromGrid`).

## Part B: exact normalisation (a physics fix, on purpose)
`orbitalNorm` is now the closed form: radial (n/2)^(2l+3)·2n·(n+l)!/(n−l−1)! × the exact sphere integral of each angular polynomial squared (`outline.cpp`, comment). The standard formulas written from knowledge; the DLMF/Abramowitz–Stegun chapters were not opened (About and DECISIONS say so). **Tests (written and run red first):** hand-derived exact values for 1s (π), 2p z (32π), 3d z² (39366π), 4f z³ (47185920π) to 1e-14; every orbital the app offers, real and complex, Z = 1, 2, 5 (150 cases) integrates to 1 by an independent quadrature: worst |I−1| **3.1e-13** (before: 8.75e-5, 4f z³). Your checker's numbers are confirmed: 2p 3.125e-6, 3d z² 1.5625e-5 in amplitude (4f z³ was worse, 4.4e-5).
**Tightened, not loosened** (stage 2 test): density 1e-4 → 1e-10, phase 2e-5 → 1e-10 rad, norm 5e-5 → 1e-8.
**What else it moved:** only 4 recorded numbers in `testJumpSameDots` (amplitude sums, cases 4f→3d z² and 2p→3d z²: 6031.6558→6031.4603, 11212.0810→11211.8145, 7868.2822→7868.2322, 3264.3804→3264.3454). A new check proves each moved by a factor between the two orbitals' predicted factors (old norms 148225969.17, 123668.07, 100.5303 against the exact ones). Also: the stage 2 About sentence "not changed" now says "made exact".

## Part A: what changed
- **Density grid on the GPU:** `kOutlineDensityCS` (GLSL 430) ports `atom::density`, `radial`, `angular`; the state is sent as a description (`outlineDensityOf`; a test checks it gives the lab's density for 4f, 2p, 3d, complex 4f m=+3, 3s to 1e-12).
- **90 % level on the GPU, no sort:** `kOutlineLevelCS`: ONE work group bisects over the float bit patterns (31 passes over the grid); the level is a cell's value, as the CPU's. Chosen over reading the grid back and sorting (that keeps the 17 ms).
- **No CPU wait** (stage 2's pattern not repeated): job + fence, `glClientWaitSync(…, 0)` each frame; the grid (1 MB) is read back ONCE per orbital after the fence, and the lines are built by the CPU's own code (`outlineFromGrid`). `outlineReady` returns nothing on the first call; the outline appears a frame or two later (CPU path: at once, with the old hitch). Radial graph: 90 % mark pending (`radialGraphNoWait`); no panel wait.
- **Slice:** the 200×200 values are a GPU job too (`SliceContour`, one job in flight): **the contour trails the cut by a frame while dragging** (a visible behaviour change on the GPU path only; DECISIONS, OWNER-TODO).
- **Third switch** `AtomLab::gpuOutline` (Performance card, default On, not saved; report prints "OpenGL 4.3: GPU outline on"). Scenes **29, 30** = 16, 17 with it Off (others On); old scene numbers and names kept, texts gained a 0.13 note.
- Docs: ROADMAP (stage 3 done; the last deferred audit item out), RESUME, README, changelog, About, DECISIONS, OWNER-TODO.

## Tests (Linux GCC Release, all pass; 9 ctests incl. glsl_shaders (13 shaders, 3 compute) and gl_phase_colours)
orbit 1,540; **atom 20,982 (+58)**; **app 1,600 (+17)**; nbody 37; jpl 55; moon 24; moon_real 59; GL 258 checks. Written before the GPU code: the physics and normalisation tests, the logic tests with a stand-in GPU, the llvmpipe harness; the CPU split into public pieces and the API were drafted first.
- **Independent 90 % level:** 1s from 1−e^(−2R)(1+2R+2R²)=0.9: R = 2.66116 a0, residual 0 (< 1e-9); 2p z, 3d z² from textbook formulas by a fine (r, cosθ) sort. Lab (64³ grid) is off by −1.91 % (1s), −0.30 % (2p z), +0.60 % (3d z²): the grid's resolution; tolerance 3 %.
- **GPU vs CPU** (llvmpipe GL 4.5; 7 orbitals incl. complex m=+3, 3s, 1s): grid cells within 1.1e-5 relative (bound 2e-4 above 0.1 % of max, 2e-6 of max elsewhere: near a radial node the polynomial cancels); GPU level encloses 0.9 on its own grid (e.g. 0.90006) with the next cell value below, and equals the CPU's within 9e-7. Outline: same mass, reach, side and wire counts. Slice: 4 orbitals × 8 planes within 4.3e-5, same segment counts; the app's `SliceContour` catches up in 1 update.
- **Pictures** (4f, 2p z, 3d z²; Cloud 3D and Slice; dark and light; 12 pairs): ≤ 1 of 741,520 pixels differs (one line end); **3.3 fallback** (context forced to 3.3): no compute object, outline at once, picture identical (0 pixels).
- **Perturbations (13 distinct, 15 runs, all reverted, hashes identical), all caught:** 0.90→0.89 in GLSL only (31 GL fails), a grid layer dropped (6), wrong factorial (50 atom), wrong angular coefficient (11), wrong Laguerre term (43), complex weight dropped (1 + 6), level search stops early (18), planes swapped (15), late result for another orbital (1), no cancel on clear (1), outlineReady waits (5), wrong angular table entry (22), broken shader. **Two slipped through first:** flipping the Slice offset's sign is an equivalent mutation (every real orbital's density is symmetric under y→−y), replaced by the plane swap; "late result for another orbital" was a real test gap, fixed (the held contour must stay unchanged).
- Existing tests changed on purpose, nothing loosened: scene count 26→28, last scene 28→30; About edits 27→28.

## Looked at
4f z³ Cloud 3D (wireframe round the blue/orange lobes), 3d z² Slice (four-lobed contour matching the cloud), 2p z Cloud 3D light page: CPU left, GPU right, identical to the eye; the others by the numbers.

## Not tested / limits
Speed; a real GPU/driver; Windows/MSVC; the app itself (`atom_view.cpp`, `main.cpp`, panels, benchmark: syntax-checked against ImGui 1.92.9b / SDL 3.4.16 headers only; the compiler caught a `%` in a tooltip, fixed). First-use hitch removal is expected, not measured. 1s has many cells with identical values, so its region mass is 0.9009, not 0.9.
