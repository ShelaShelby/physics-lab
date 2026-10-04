# Native Physics Lab 0.10.6 (alpha) — stage 7 report: the Atom Lab's speed risks made measurable

Audit findings 10, 11, 12 (`git show 90049fe:native/REPORT.md`). Only the Atom Lab; `index.html` untouched (sha256 e5501387ae64…). **Nothing was optimised and nothing was timed**: this session has no GPU and no Windows. The scenes were added and run headlessly for correctness only. **Every speed number must come from the owner's PC** (OWNER-TODO.md).

## How it is built (the existing benchmark, no second timer)
- Same system as before: `BenchRun` (2 s warm-up, 8 s counted), the result table with Average, fps, 1 % low, **Worst**, CPU, GPU. The table and the text file already give the worst frame for every scene, so the new scenes report it too.
- New tick box in the benchmark's start window: "Include Atom Lab speed-risk scenes (11 extra)", after the stress scenes. The 7 old scenes keep their numbers and settings (a test checks scene 3 is unchanged). New scenes are numbered 8 to 18.
- New in `atom_bench.cpp/.h`: scenes `5…15` of `benchSetup` (the Atom Lab side), `benchSceneInfo` (names and settings in words), `benchFrame` (a small per-frame step: jump held, cut moving, outline cache emptied). New in `outline.cpp/.h`: `clearOutlineCache()`, `outlineCacheSize()` (a cold start for "first use"; called only between frames).
- Wiring in `ui/benchmark.cpp`: an optional per-frame function per scene, a scene list per run, the tick box. Each scene starts from the lab's defaults, fixed seed 20260101, fixed Z/N, orbital, view, speed 5, camera still. The settings text in the result now also names orbital, cloud style, dots, phase, superposition, outline for cloud views.

## The 11 scenes (all uranium-238, Z = 92, unless noted; "48,000" = density Ultra)
| # | Name (settings are in it) | Exercises | What a bad number looks like |
|---|---|---|---|
| 8 | Risk 10: phase colours (Wheel), Classic dots, Cloud 3D, 48,000 | finding 10: 24 colour bins × 3 depth bands, each rescans all dots | average far above 9 |
| 9 | Risk 10 baseline: two colours (Sign), same | the same without the 24 bins | the baseline for 8, 10–15 |
| 10 | Risk 10: Wheel, Classic, Cloud 2D side view, 48,000 | finding 10 in 2D (24 scans) | far above 9 |
| 11 | Risk 11: cloud jump in flight (held half way) | finding 11: per-dot `jumpFractions`, copy of all dots | average far above 9 |
| 12 | Risk 11: jumps one after another (emit, absorb back) | the start of a jump builds 48,000 new dots in one frame | **worst** far above the average |
| 13 | Risk 11: superposition, orthogonal 2p z + 2p x (one energy) | finding 11: per-dot thinning, hash | above 9 |
| 14 | Risk 11: superposition, overlapping 2p x + 2p m+1 | the stage 4 sampling path | compare with 13 |
| 15 | Risk 11: superposition, orthogonal 2p z + 3d z² (two energies, beats) | thinning with a changing beat phase | compare with 13 |
| 16 | Risk 12: first use of an outline (cache emptied every 120 frames), Cloud 3D, 4f, 12,000 dots | finding 12: the 64³ density pass | **worst** far above the average (the stutter) |
| 17 | Risk 12: outline with the cut dragged, Slice, 4f, 12,000 dots | `outlineSlice`, 40,000 values, every frame (cut moves 0.01 per frame, as a drag does) | average far above scene 3 |
| 18 | Risk 12: nucleus view, oganesson-294 (294 nucleons), whole nucleus | per-frame vectors in the nucleus view | far above scene 2 |
Jump scene 11: the jump is started and its clock held at 0.6 s of 1.2 s, so every frame draws a jump in flight, whatever the fps. Scene 12: emit 4f→lower, drop the emitted photon, absorb back to 4f; in the headless run the starts came at lab frames 0, 674, 963 (a start about every 2.8 to 4 s of lab time). Scene 16 clears the cache at counted frame 0 and every 120 counted frames after (about 17 first uses in 8 s at 255 fps); the setup also clears it, so the first run and later runs start cold alike.
The normFactor first use (a cached 12,000-step integral per orbital) cannot be reset without changing the core; it hits the first jump or superposition of a process, in the warm-up.

## Optimisation candidates (seen, NOT implemented; lines as of this commit)
- `atom_view.cpp:355` (2D) and `:395` (3D): for each of the 24 colour groups every dot is scanned again; one pass bucketing dots by group would do it once (finding 10).
- `atom_view.cpp:304`: `allPts.assign` + `insert` copies up to 96,000 floats every jump frame; `atom_sim.cpp:287` (`jumpFractions` per dot per frame, complex exp, sin, cos) could be computed once per frame (finding 11).
- `atom_sim.cpp:791`: `info1` sums 48,000 `hypot` every frame; a running sum would do.
- `outline.cpp:77–140` (`outlineOf`) runs on the drawing thread; `atom_view.cpp:346` recomputes `outlineSlice` (`outline.cpp:141`) on every cut change; could be done at orbital change or coarser while dragging.
- `atom_view.cpp:501, 560–566`: per-frame `list`, `softOps`, `make_shared` in the nucleus view; buffers could be reused.

## Tests (Linux GCC Release, Ninja, `PL_BUILD_APP=OFF`): all seven pass
orbit 1,540; atom 18,157; **app 1,515** (was 1,385: +130 in `testBenchRiskScenes`); nbody 37; jpl 55; moon 24; moon_real 59.
`testBenchRiskScenes`, for each of the 11 scenes: set up from an odd lab and from a default lab gives the same settings; stepped 600 frames (1500 for the jump loop, 200 warm-up + counted) it ends as declared (Z, N, view, orbital, cloud style, phase, dot count, superposition on/off, outline); name says phase colours / 2D or 3D / 48,000; settings text names them; scene 11 is held between 0.3 and 0.7 s with one start; scene 12 starts at least 3 jumps; the pairs are orthogonal / overlapping (`overlapAB`) and one-energy / two-energy as named; scene 16 emptied the cache; scene 17's cut moved and stays in −1…1; scene 18 has 294 nucleons; each scene is repeatable (same dots, cut, count); old scene 3 is unchanged; the outline cache can be emptied and refilled. Tests were written first (they did not compile before the code). No existing test was changed or loosened.

## Not compiled / not tested here
- **`ui/benchmark.cpp`** (the tick box, scene list, per-frame call, numbering) is in the app target, which needs SDL3 and ImGui and does not build here. I checked its new scene-list and numbering code by compiling it in a small harness with stub types (names 8 to 18, scene 8 sets up U-238 Cloud 3D), but the file itself was never compiled: a typo there would only show on the PC. Confidence: medium for the wiring, high for the scenes (compiled and tested).
- `atom_view.cpp`, `atom_panel.cpp` were not touched. No timing, no screen, no GPU, no Windows; the scenes were never drawn. Whether the per-frame hook runs at the right moment relative to the sim step in the real loop is unverified.
