# 0.85.5: the eye's glare veil on the GPU

Built and tested on the owner's PC (RX 9060 XT, AMD driver 26.11.2, VS 2026 Build Tools).

## What changed
- The glare's veil (light.cpp glareVeil) ran on the CPU: 350-400 ms on 8 threads, redone every 0.25 s while the gas moved. rest.blackhole_glare used 45 % of the whole CPU (540 % of a thread).
- The veil now runs on the GPU, step for step as glareVeil: kGlareLevelFS makes each level's light, directions and spreads, kGlareSumFS lays the veil top level first. The CPU makes only the near-weight and flat-ratio tables (light.cpp glareTables), again only when the block size changes.
- No read-back and no CPU thread: the veil is drawn in the same batch as its picture. The CPU veil stays as the reference, the fallback (the programs fail, PL_VK_CARDFAIL=glare) and for comparing (PL_BH_GLARE_CPU=1). The synchronous checks (glareNow) still use the CPU veil.
- New check (PL_BH_COMPARE, check_blackhole.py PLBHGLARECARD): the GPU veil against glareVeil on two grids (320 x 200 and 400 x 250 blocks).

## Tests run
- PLBHGLARECARD on the RX 9060 XT: largest difference 2.33e-6 and 2.12e-6 of the largest veil value; 7.3e-5 and 7.0e-5 relative where the veil is over 1 % of it. Limits 1e-4 and 1e-3.
- rest.blackhole_glare (local, 2560 x 1369): 1.6 % CPU with the gas (0.85.4: 45 %), quiet with the gas off. It is still "busy" with the gas, from the gas's 754 redraws.
- blackhole.real_move 215.9 fps and blackhole.everything 138.2 fps, the same as with the CPU veil (215.9, 137.9). The 1 % lows are slightly higher (54.5 against 51.7, 72.7 against 68.4).
- blackhole_tests 1141 checks, app_tests 202,429 checks, 0 failed. vk_compare_tests 707 checks, the same 3 known failures, 0 validation errors.
- Memory, sampled every 2 s over long runs: Black Hole with gas 679 MB, Atom (auto photons) 611 MB, Orbit 608 MB, Molecule 591 MB. None grows.
- Validation layer on the app: 2 errors at the start, from Dear ImGui's swapchain helper (it changes the layout of images not yet acquired). 0.85.4 the same. ROADMAP has it.

## Not done
- The 0.85.4 full tests failed: two UI parts (atom classicsideglow, blackhole sky) hit the 300 s limit. Every test in that run took 1.4 to 1.8 times as long as in 0.85.3's run (orbit_physics 34 s against 19 s): a slower runner. To watch in this run.
- check_blackhole.py itself was not run here: the Linux UI scripts are not run on the owner's PC. The GitHub full tests run it.
- The spinning sky shader's cost, tone.close and precision.27 on AMD, atom.emit_all (timed: buildManyJump 63 ms a frame on the frame thread). All in ROADMAP.

## References changed on purpose
- None.
