# 0.55.5: the Black Hole Lab's frame never waits for the processor
No benchmark file was pasted (the job's placeholder was empty): all numbers are from this Linux box (4 cores, Mesa llvmpipe, a SOFTWARE renderer), not the owner's RX 9060 XT.
## Step 1, measured before (frame thread = BlackHoleView::draw + the glare's read-back and veil; ms mean / worst over 20 moving frames; spin drag = +0.005 a frame)
| 1280 x 800 | turn | tilt | distance | spin drag | glare at rest |   | 3840 x 2160 | turn | tilt | distance | spin drag | glare at rest |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Real, spin 0 | 32 / 69 | 30 / 49 | 91 / 158 | 181 / 331 | 2224 | | Real, spin 0 | 178 / 267 | 183 / 228 | 249 / 348 | 437 / 646 | 5989 |
| Real, spin 0.9 | 64 / 133 | 201 / 457 | 200 / 311 | 174 / 358 | 3394 | | Real, spin 0.9 | 355 / 594 | 435 / 859 | 497 / 722 | 402 / 915 | 4890 |
| Film | 2240 / 3072 | 2424 / 3158 | 2475 / 3033 | (locked) | none | | Film (3 frames each) | 12668 / 23939 | 13505 / 23961 | 13333 / 22668 | (locked) | none |
- Stages (spin 0.9, 1280): coarse table 111-129, star bins 16-21, star images 18-27, exposure scan 37-40 (4K: 293-333) per change. At rest: no redraw, no fill.
- **A cause the planner did not name**: in Film the exposure scan took 2.3 s (4K: 20 s) per change: lab.cpp discLight called looks.cpp filmDiscLinear() at every point,
  which works out the white balance and two black-body integrals each time. Now made once (function statics; the same numbers bit for bit). This alone was ~1 frame per second.
## Step 2, after (the app's path, PL_BH_ASYNC; frames paced to 16 ms as VSync would; ms mean / worst)
| 1280 x 800 | turn | tilt | distance | spin drag | glare share at rest |   | 3840 x 2160 | turn | tilt | distance | spin drag | glare share at rest |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Real, spin 0 | 0.03 / 0.06 | 0.11 / 1.8 | 0.02 / 0.06 | 0.02 / 0.04 | 37 | | Real, spin 0 | 0.04 / 0.06 | 0.20 / 3.4 | 0.68 / 5.0 | 0.04 / 0.12 | 586 |
| Real, spin 0.9 | 0.02 / 0.04 | 0.02 / 0.04 | 0.02 / 0.04 | 0.02 / 0.05 | 60 | | Real, spin 0.9 | 0.83 / 5.5 | 0.18 / 3.0 | 0.04 / 0.10 | 0.04 / 0.14 | 669 |
| Film | 0.21 / 4.0 | 0.03 / 0.08 | 0.03 / 0.05 | (locked) | none | | Film | 0.32 / 4.4 | 0.21 / 4.0 | 0.08 / 0.23 | (locked) | none |
- The limit chosen: **8 ms** worst during drags (UI part `smooth`); typical 0.02-0.2 ms, the worst frames (3-6 ms) are this busy box preempting the thread.
- Worker stages now (1280, spin 0.9): coarse table 127-431, bins 17-60, star images 23-90, exposure 24-40; the moving scan has at most 1280 x 800 / 16 points at any size.
- **Glare share at rest is not met on this box**: the veil runs on its own thread, the read-back goes through a pixel buffer and fences and never waits for one, but on
  llvmpipe the GL calls themselves (drawing the blocks, the copy into the buffer) are executed by the processor inside the call (37-670 ms, once per rest). On a graphics card
  they are queued commands; unverified here: the owner's benchmark will show it.
- **Behind**: while moving, the picture was behind in 20-22 of 22 frames, at most 1-2 s at 1280 and 8-21 s at 4K on this box (llvmpipe's drawing of every new 4K picture
  takes all 4 cores at normal priority; the worker is below it). At rest it catches up. ROADMAP line added.
- At rest: no redraw, no fill, the lab's worker and fine threads use 0.000 of a core (per-thread /proc times over 10 s); the rest is llvmpipe redrawing the kept picture each frame.
## What moved where
- Worker thread (below normal; BlackHoleView::workLoop): the lens/disc/spin tables, SpinStars, star images, exposure; it hands back a Picture (tables, dots, exposure,
  stats of ONE set of settings); the frame installs it whole. A request not yet taken is replaced (newest wins); every finished picture is shown; the coarse scan's rows on threads.
- Fine table: its own thread(s) after 0.3 s of rest, dropped when spin/distance/tilt change, never joined while running. Glare: renderer glarePump (fences, PBO, veil thread).
- Priority: low_priority.cpp (Windows SetThreadPriority BELOW_NORMAL, Linux nice +5 per thread); the pools of disc.cpp, light.cpp, spin_sky.cpp lower their threads too.
- Card: "Working out the picture: the one shown is the last finished." while behind. No frame rate promised anywhere.
- Synchronous path kept for the checks (PL_BH_SPINWAIT, glareNow, or PL_DRAWSTAT without PL_BH_ASYNC): the same code as before, run in draw.
## Step 3, benchmark
- Scene 13 now turns, tilts and changes distance in turn (2 s each + 1 s rest, by the clock); its report adds per movement: frame thread average/worst, each worker stage
  (coarse table, bins, star images, exposure), frames behind and the longest behind; at rest the glare and the fine table. Same length. Texts in bench_scenes.cpp; app_tests testBenchBlackHole.
## Tests
- New UI part `smooth` (ui_blackhole): tilt drag + spin drag; worst frame-thread time < 8 ms; every frame's sky = the settings its exposure and stars were made for; pictures
  never older; at rest caught up, 60 frames without any counter growing, glare made. Failing before: worst 1665 ms. (Ends the spin drag at 0: a fine-table picture takes over a minute on llvmpipe.)
- No unit check of "newest wins": the logic lives in the view with the renderer; `smooth` covers it. Test helpers added: PL_BH_MOVE, PL_BH_FRAMETIME, PL_FRAME_MS, PL_BH_SETTLE.
- By hand: smooth 10, precision 78 (Real saved hash unchanged), spin 19, disc 23, looks 39, truelight 28, all passing. check.sh quick 17 of 17. **The full UI suite was not run (small version).**
- No physics test or reference changed.
