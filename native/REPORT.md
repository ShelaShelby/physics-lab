# Native Physics Lab 0.13 (alpha) — stage 2 of 4: OpenGL 4.3, superposition and jumps on the GPU

No physics changed. `index.html` untouched (sha256 e5501387ae64…). No helper agents. One commit, one push; the Actions result is in the closing message.
**SPEED IS NOT TESTED.** No real GPU and no Windows here. The speed-up is *expected, not measured*. A real driver may round floats differently from llvmpipe (rare edge dots only). Whether a real PC gets 4.3 or the fallback is untested until you run it.

## Step 2, measured first (CPU, this machine, ms per frame, `tests/timing_stage2.cpp`, relative guide only)
| scene | step | **colour groups** | copy | project | build | upload* | total |
|---|---|---|---|---|---|---|---|
| 11 jump held | 0.01 | **5.15** | 0.38 | 1.10 | 2.22 | 2.24 | 11.09 |
| 12 jumps | 0.04 | **4.02** | 0.30 | 1.07 | 1.69 | 2.04 | 9.16 |
| 13 2p z+2p x | 0.05 | **2.23** | 0 | 0.54 | 0.84 | 0.57 | 4.22 |
| 14 overlap | 0.06 | **3.54** | 0 | 0.58 | 1.38 | 1.93 | 7.49 |
| 15 beats | 0.06 | **5.55** | 0 | 0.67 | 1.23 | 1.01 | 8.52 |
| 8 / 9 plain orbital | 0.07 | 0.62 / 0.01 | 0 | 0.59 | 1.20 | 1.5 / 1.9 | 4.0 |
\* a stand-in (two copies of the blobs), not a glBufferData. Colour groups = thinning + phase + bin of every dot (`superGroup`, `jumpGroups`).
**Result:** the wave function is NOT evaluated per frame (the two amplitudes of each dot are made once with the dot; `step` is 0.05 ms). The hot spot is the colour group (2–5.5 ms; a plain orbital 0–0.6). Scene 12's worst frame is the jump start: 7.2 ms in `step()` (a quarter of the 48,000 new dots). Of that only ~⅓ is wave evaluation (4 × 0.05 µs of 0.6 µs a dot); the rest is the Metropolis chain of random steps, which is sequential → **left on the CPU, not moved** (said in DECISIONS).

## What changed
- **OpenGL 4.3 with fallback:** `main.cpp` asks for a 4.3 core context (with/without multisampling), else 3.3 as before. 4.3 functions are loaded separately (`loadGL43`, `render/gl_loader.h`); 3.3 PCs never need them. `Renderer2D::compute()` is null on 3.3 (or if the shader fails): CPU path, same picture. `PL_NO_COMPUTE=1` forces it. Performance card (Atom Lab): "OpenGL 4.3: GPU superposition on" / "OpenGL 3.3: CPU path" + switch; the benchmark report prints the same line.
- **Compute shader** `kGroupsCS` (GLSL 430, `shaders.h`): from each dot's stored amplitudes it thins (fixed random number: 64-bit splitmix done in 32-bit pairs), forms aA + bB·e^(−iθ), takes the phase → bin; the jump mode ports `jumpFractions`/`jumpPhaseOf`. Each function names its C++ twin. The CPU sends θ, a, b (or the jump terms), the amplitudes only when new (epoch) or changed (dirty range, set where `walkSuper` writes), and reads back 1 byte a dot; projection, depth bands, draw order, upload stay as stage 1. Covers every view and style (all go through `fillGroups`, `cloud_dots.cpp`).
- **Second switch** `AtomLab::gpuCompute` (default On, not saved), apart from stage 1's, so each stage compares alone. Benchmark scenes **25–28** = scenes 11, 13, 14, 15 with it Off (same dots, tested); 11–15 set it On; old numbers and names unchanged; run text says "GPU superposition On/Off".
- **Float rule** (DECISIONS): a dot may differ from the CPU only if within float rounding of its threshold; bounds worked out per dot; >0.05 % differing = failure.
- Docs: ROADMAP (stage 2 done; scene-14 item out of Deferred), RESUME, README, changelog, About (textbook + CODATA 2022 as used, edge dots, normalisation), DECISIONS, OWNER-TODO.

## Tests (written before the GPU code: the physics test and the comparison harness first; they failed to build until the code existed) — Linux GCC Release, all pass
orbit 1,540; **atom 20,924 (+66)**; **app 1,583 (+25)**; nbody 37; jpl 55; moon 24; moon_real 59; **glsl_shaders** (now 11 shaders, the compute one as GLSL 430); **gl_phase_colours** (168 checks; Linux CI job runs both).
- **Physics, independent** (`test_gpu_compute.inc`): textbook 2p z, 2p x, 2p m=+1, 3d z² typed in the test (CODATA 2022 a0, Hartree, m_p/m_e, h): superposed density and phase at 5 points × 4 beat phases × Z=1,2 × 2 mixes for the 3 pairs; ⟨2p x|2p m+1⟩ = −1/√2 by hand; beat energy and period h/ΔE (1e-9); T/4, T/2, T; normalisation of each pair by quadrature.
- **GPU vs CPU** (`gl_compare.cpp`, llvmpipe GL 4.5 core, real renderer): scenes 13/14/15 at beat clock 0, ¼, ½ beat + 2 more; jumps (scene 11 at 5 jump times, scene 12 running; Sign and Wheel). 2.6 M dot-frames: **0 differing**; 40 running frames (1.9 M): 3 differ, worst at 46 % of its bound. Directed: 1,644 dots put on bin edges → 512 differ (≤19 % of bound); 2,000 on thinning thresholds → 165 differ (≤6 %): every differing dot bounded, test not vacuous. Pictures (scenes 13–15 at 3 beat times, jump in flight): largest difference 0 of 255. Dirty-range upload, restart epoch tested.
- **3.3 fallback:** a context forced to 3.3 (Mesa override): renderer starts, no compute object, `fillGroups` uses the CPU, groups identical, picture identical (0/741,520 pixels). Stand-in tests (also on Windows CI): switch Off, failing GPU, null object → CPU.
- **Perturbations (12, each reverted, hashes identical), all caught:** beat sign in C++ (1 atom + 6 GL) and in GLSL (6 GL); 1/√2 weight dropped (3 + 20); bin edge in GLSL only (12 GL); thinning ×0.6 (14); splitmix constant (24); dirty range ignored (2); epoch ignored (1); jump em sign (2 + 10); jump reads wrong buffer (10); switch ignored (3 + 1); broken shader (glslang + renderer).
- Existing tests changed on purpose, nothing loosened: scene count 22→26 and "last scene 24→28"; About edits 26→27.

## Looked at
Scenes 13 (half beat: pink and green lobes), 14 (beat 0: blue/orange blobs), 15 (quarter beat: colour changing along z), jump in flight (old and new dots mixed): CPU left, GPU right, identical to the eye; not every picture viewed, the rest by the numbers.

## Found, not fixed
The lab's orbital normalisation (`orbitalNorm`, midpoint rule, 400 points in cos θ) leaves amplitude too big by **3.1e-6 (2p) and 1.6e-5 (3d z²)**; exact textbook value is 1. The physics may not change this stage; the test tolerances (1e-4 density, 2e-5 rad) state it. Owner to decide.

## Not tested / limits
Speed; a real GPU/driver; Windows/MSVC; the app itself (`main.cpp`, `orbit_panel.cpp`, `benchmark.cpp`, `atom_view.cpp` only syntax-checked against ImGui 1.92.9b / SDL 3.4.16 headers). The per-frame wait for the GPU read-back is an unmeasured assumption. Still on the CPU: projection, draw order, upload, and the jump's dot building.
