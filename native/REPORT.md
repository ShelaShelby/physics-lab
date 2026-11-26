# 0.89.0: the full-test failures of 0.86-0.88, jumps with every electron on threads, resizing, the swapchain

## What changed
- Part 0, check_blackhole.py disc (failed on 0.87.0 and 0.88.0). Each value was moved by one 0.87.0 change, found by switching each off (PL_BH_REALOFF). The hottest ring (154,847 K at 9.5032 M, feeding preset 47,146.7 K) comes from the returning light (R11): with it off, 154,016 K at 9.551 and 46,893.7 K, the papers' values. The brightness comes from the colour correction (R9): with it off, 150 -> 296. Limb darkening moves neither. Both changes are deliberate and follow their sources, so the app stays and the check's references are corrected (below).
- Part 0, the other three: nucdenscard (13 s), classicsideglow (80 s) and sky (79 s, 50 checks) pass here. They are slow-runner timeouts on GitHub (300 s per part, with 4 parts at once).
- Part A: buildManyJump makes its dots in fixed chunks of 1,024. Each chunk has its own random stream (seeded from the jump's seed and the chunk number), and the chunks run on cores - 1 threads (1 to 16). The sampler's lazy caches (hf.cpp prep, enlargedRadius, manyExtM) are filled on the frame thread first and only read in the threads. The dots changed once (new streams), and are now the same for any thread count.
- Part C: vk_backend.cpp makes the swapchain itself. Dear ImGui's helper moved every new image to PRESENT_SRC before it was acquired, in a submit of its own, and waited for it. Now no image is touched before it is acquired (each frame starts its image from UNDEFINED, as before).
- Part B: each new size remade the swapchain, the 2D picture (picture, grey picture, multisampled) and the hole's three layers (counted with a temporary log). All of these hold pixels at the size, except the grey picture: it is now made only when the Monochrome theme draws it. The benchmark gives the swapchain's count and time per scene.

## Tests run
- Part A (written first; failed on 0.88.0, which has no thread setting): atom_tests testManyJumpThreads089, 12,174 checks, 0 failed. Radium-226 level jump, sodium 3s valence jump, sodium on the Slice: 1, 2, 8 and 16 threads give identical dots, and over frames on 8 threads = at once on 1. The jump's physics tests are unchanged.
- Part A times (build machine, 4 cores, 240,000 dots, all at once): radium 127.5 ms on 1 thread (the old serial work), 69.1 on 2, 38.4 on 8, 34.2 on 16. Sodium 68.4 -> 23.2, Slice 73.9 -> 20.3. 0.85.5 measured 63 ms a frame (a quarter of the dots) on the frame thread.
- Part B, ui.resize_blackhole here (lavapipe, GPU paths off as on the owner's PC; 2 runs each): before 10.8 and 13.6 fps, 1 % low 1.4 and 1.5, swapchain 1.96 and 1.91 ms on average. After 15.5 and 15.6 fps, 1 % low 7.6 and 8.8, swapchain 0.67 and 0.64 ms, the grey picture no longer remade. Frames here wait on lavapipe's drawing (present), so the fps varies 30 % between runs.
- check_blackhole.py disc 30 checks and sky 50 checks, 0 failed; check_5t.py grey 115 and tiles 61, 0 failed; check.sh job passed (quick 18 of 18, ui_smoke 7.7 s); the Monochrome theme with the validation layers: 0 errors, 0 warnings.

## Not done
- Part C's test is in ui_smoke (no line with non-acquired-swapchain-image-used), but the Ubuntu validation layer here (1.3.275) does not report this message even for the old helper, so the check cannot fail here. A newer layer from the LunarG SDK was not loaded. The owner's PC (pc_tests.ps1, part validation) has to confirm it.
- The times above are from the build machine. This job needs pc_tests.ps1 on the owner's PC (atom.emit_all, ui.resize_blackhole, validation) on the new release.
- The six size-tied images are still made at each new size (ROADMAP).

## References changed on purpose
- check_blackhole.py disc: the hottest ring is the papers' ring at the readout's radius with the returning share added (T^4 x (1 + F_o/F), within 1e-5, ret printed in the bhdisc note and the same as the card's "+2.2 %"). The papers' 154,016 K at 9.551 and 46,893.7 K are still checked exactly, in runs with the returning light off.
- check_blackhole.py disc: "a higher feeding rate is brighter" holds for black-body rings, so it is checked with the colour correction off. With it on, f rises with T (Done et al. (2)), and the check is that the brightest part grows less than for black-body rings (here 93.74 -> 73.34 against 150 -> 296). New: the card's f = Done et al.'s at the readout's temperature (1.449, 2.323).
