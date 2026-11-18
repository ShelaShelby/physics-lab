# 0.62.0: Vulkan, step 4 of 5: the Black Hole Lab's card programs on Vulkan; Vulkan 1.3 required, 1.4 used where present (based on 0.61.0; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe)
## Part 0: Vulkan 1.3 required (owner decision 18 Nov 2026)
- The instance asks for the loader's newest version up to 1.4 (here 1.4); a card below 1.3 is not used: "this graphics card offers Vulkan X.Y; Physics Lab needs 1.3", the app on
  OpenGL (PL_VK_PRETEND_API=1.2: checked). The device step logs the card's version and the one asked for; dynamic rendering and synchronization2 are queried and enabled.
- Oldest families with a Vulkan 1.3 conformance submission (Khronos, "Conformant Products: Vulkan", read 18 Nov 2026; saved in the sources repo, data/vulkan/):
  NVIDIA: first Maxwell (GTX 745/750/750 Ti, GTX 9xx), Windows driver 510.57 and Linux 510.24 (entries 614-617, 25 Jan 2022); NVIDIA's own page lists Vulkan 1.4 for Turing
  and later. AMD's own drivers: Vega (RX Vega 56/64, Radeon VII) on Windows (entry 654), Polaris (RX 460-590) on Linux (655, 657); Mesa's RADV: GCN 1 (HD 7730 ...) on Linux
  (820). Intel: Gen9, Skylake HD 510-580 (634) and Apollo Lake HD 500/505 (646) on Windows; Broadwell HD 5300-6200 on Linux with Mesa 22.1 (622).
## Part 1: times (ms, 1280 x 800, software renderers: the record only). Benchmark on 0.61.0, OpenGL: blackhole.real_move 1077.9 a frame, sky pass 627.4;
  film_tilt 436.3; extreme 2038.6 (the benchmark does not run on Vulkan until step 5). Same three views by PL_BH_FRAMETIME on 0.62.0 (frame median / sky pass),
  OpenGL -> Vulkan: real_move's view turning 67.7 / 282.7 -> 78.7 / 496.4; Film with glow at rest 3.4 / 119.2 -> 23.7 / 131.1; feeding hole with glare at rest
  3.0 / 127.9 -> 21.8 / 129.8 (Vulkan's frame waits for its fence: on lavapipe that is the processor's own drawing time).
## Part 2: the programs (render/vk_blackhole.*): shaders.h's own text as SPIR-V (extract_glsl.cmake joins kKerrSkyFS's parts with "+"), Vulkan 1.3 dynamic rendering
- Ported pass by pass from renderer2d.cpp: lens, table pass, sky per pixel, exposure chain + white read-back, lower resolution while moving (stretched only where the card
  filters RGBA32F), probe + light check, Film glow, glare (blocks read back in the batch after the picture, veil on its thread; at once in the checks), mask for maskedDisc,
  the kept picture (Renderer2D::lensSameView shared). Batches on VkCards before the frame; the frame lays the picture over (kComposite, no blend). Images' layouts and last
  uses tracked, synchronization2 barriers per use. Timestamps: queries (timestampValidBits 64 here). Read-backs: host-visible buffers, read once their batch is done.
- Pipelines (lavapipe, first start; SPIR-V bytes): whole sky 253-286 ms, 103,908 (OpenGL: 103.5 / 64.7 KB plain / inlined); table pass 21 ms, 28,756 (28.4 / 20.1 KB);
  lens 52 ms, 30,560; elliptic functions 59 ms, 26,660; tiny 1 ms, 776; light paths 422 ms, 61,216; small ones 1-8 ms. Lens, table pass, sky stages, light paths on a worker.
- Pipeline cache (settings folder, vulkan-pipeline-cache.bin: our size + hash header, then the driver's, checked against this card): written, read at the next start, a
  damaged file ignored (PL_VK_CACHE_CORRUPT). lavapipe keeps only the 32-byte header, no pipelines: the speed-up can only show on the owner's PC.
- Guards: Vulkan twins of gpuGuard ("[vulkan] [lightpaths] ", "gpuvklightpaths=", "vulkan lightpaths <hash>") and gpuSkyGuard ("[vulkan] [sky] "), new gpuVkLensGuard
  ("[vulkan] [lens] ": pipelines, first full picture). OpenGL's records are never written from a Vulkan start. blackhole_view: PL_BH_COMPARE skipped on Vulkan (no
  OpenGL call); its hooks (probe, kerrCompute) go through the renderer's Vulkan halves; the sky's and light paths' steps wait for their worker (building()).
- Light paths on Vulkan (full steps: pipeline, the start-up check, first fills): passed here: math 2.1e-16, edges 0, dir 1.0e-13, b 8.5e-17, u 1.8e-10, 0 ends differ.
## Part 3: zero validation errors with synchronisation checks in every view; formats queried (RGBA8, RGBA32F + linear filter, R8, R32F); includes, no nested generic lambdas.
## Part 4: tests
- check_shell.py vulkanbh (in the vulkancards half of ui_shell), Vulkan vs OpenGL, one sample, settled: lens (spin 0) 0.001 %, Real 0.001, Film with glow 0.001, glare
  0.001, All light 0.001, Sky black 0.001, True brightness 0.001 % over 8 levels (limit 1 %); glare made over the frames = at once (0.000 %); PL_VK_CARDFAIL=sky: only the
  sky falls back, log says why; Vulkan 1.2 card -> OpenGL with the reason; cache as above. 42 checks, 0 failed; on 0.61.0's build 27 failed.
  (PL_BH_GLARENOW, checks only: the glare at once; OpenGL's low-priority veil thread can wait minutes here behind llvmpipe's threads, so settled pictures differed.)
- gl_compare_tests' Vulkan twin: light paths vs the processor (cardSelfCheck, OpenGL's limits) passed; vs OpenGL's, 384 rays: within 2.4e-6 relative (the end state,
  number 10 of 16; limit max(1e-5, kCardUTol)); sky probes: dir 2.2e-6 / 1.7e-6 rad, b 3.4e-6 / 6.4e-6, r 3.9e-6 / 4.0e-6, light 0.077 / 0.012 of its limit. 647 checks, 0 failed.
- OpenGL screenshots (orbit, atom Glow, atom Slice, molecule, black hole) byte-identical to 0.61.0's; check_blackhole precision 131 / 0; check.sh quick 18 / 18.
- Full UI suite: 32 of 34 (this build has no gl_phase_colours: run alone, above); ui_shell with vulkanbh passed; ui_popups (check_6b benchwaits) and ui_molecule
  (check_molecule card) fail as in 0.60.0 and 0.61.0 (ROADMAP).
## For the owner's PC
- Settings, Performance, Graphics: Vulkan (test), restart. Open the Black Hole Lab: each preset, Real and Film, glare on; restart once (the pipeline cache: the
  sky's pipeline should be faster the second time). Send gpu-paths-log.txt (its "[vulkan] [lens]", "[vulkan] [sky]", "[vulkan] [lightpaths]" and "[cache]" lines).
