# 0.85.3: the Black Hole Lab's GPU paths work on the owner's Radeon

Built and tested on the owner's PC (RX 9060 XT, AMD driver 26.11.2), which now has VS 2026 Build Tools, the Vulkan SDK and RenderDoc.

## What changed
- The light paths' shader (kKerrTableCS) crashed AMD's shader compiler. Cause found by making cut-down shaders on the card one by one: `isnan(err) || isinf(err)` on a double at the end of dpStep. It is now `!(abs(err) <= DBL_MAX)`, the same test. The pipeline is made, and the light paths pass their check on the Radeon.
- The sky's circular wait (pc_0.85.2 C1): the sky now takes the guards' turn only after the lens pipeline is made (blackhole_view.cpp skyReady, Renderer2D::skyLensState).
- A sky kept "slow" at 'the first full picture' by 0.85.2 or older is tried once more, guarded (gpu_guard.cpp gpuGuardDecide).
- The frame counter counts always (main.cpp), so the Orbit Lab uses its kept picture outside tests (pc_0.85.2 C3).
- The jump's first-full-use fence is made once (atom_card_paths.cpp). vk_compare.cpp fallback33 read the outline after the cache freed it: it now copies it.

## Tests run
- Unit tests (ctest, the 17 quick tests) on Windows: all passed. app_tests: 202,429 checks, 0 failed (5 new guard checks).
- vk_compare_tests on the Radeon with validation: no crash, 707 checks, 3 failed (below), 0 validation errors. "Vulkan light paths against the processor: passed".
- pc_tests.ps1 -Parts smoke,bh on the local build: 46 passed, 2 failed (tone.close, precision.27, both known).
- The app on the owner's settings: the light paths and the sky per pixel both "the whole path succeeded" in the log.

## Not done
- check.sh job: its UI tests are Linux only. The Windows smoke of pc_tests.ps1 ran instead (passed).
- Still failing on the Radeon (ROADMAP): tone close 1.99 %, the bent sky at 27 GM/c² 2.795e-05 rad, Glow soft dots additive 2 levels, the rocks' llvmpipe guard 0.00578.
- The benchmark and the rest checks were not run again.

## References changed on purpose
- None.
