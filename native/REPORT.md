# 0.63.1: Vulkan fixes from the owner's first Vulkan benchmark (0.63.0 on his PC; based on 0.63.0; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe)
## 1. The outline switched off by a stale marker (app/gpu_guard.*, main.cpp)
- Proved on this box with 0.62.0's build (8e094dc), Vulkan, a fresh settings folder, 120 frames, a normal close (exit 0): gpu-vkoutline-step.txt was still
  there, naming "the outline's first small run" (id vulkan outline cdf40fa24b97d9f0, the owner's), and gpu-vkgroups-step.txt and gpu-vkcloud-step.txt too.
  Two causes: 0.62.0's end() kept a marker after its step passed, and main.cpp never called closing() for the Vulkan card guards (0.63.0 neither).
- Fix: a marker and a settings line name the app version that wrote them ("app=0.63.1", fifth field "app 0.63.1"). A crash kept without one (0.63.0 or older:
  0.63.0's markers name no version either, so they cannot be told from 0.62.0's) or before 0.63.1 is tried once more, guarded, and the log says why ("Not
  trusted: ... 0.62.0 could leave one after a step that passed ..."); the old line stays until the path passes or a real crash is kept with "app 0.63.1".
  The light paths' guards (both APIs) keep an unnamed crash off. All ten Vulkan guards are now closed at the app's end.
- Tests first (test_app, 14 new checks; they do not compile on 0.63.0). Here 0.63.1 on 0.62.0's folder: outline, groups, cloud "Not trusted", steps passed.
## 2. The Vulkan GPU timer (render/vk_backend.*, render/vk_card.*, ui/benchmark.cpp)
- The owner's ~1e12 ms cannot be made here (lavapipe gives sane values). Changed: a frame's timestamps are read only after its fence has signalled (a
  serial per swapchain fence; 0.63.0 trusted the availability bits alone); a pair with a zero, an end before its start or over 10 s is thrown away and
  counted on the timer check's line; only frames that are counted are timed (a warm-up frame's late result was counted in the scene); only the frame
  thread's card batches count (a worker's light-path fill was summed into a frame: film_tilt's card time came out above its frame time here).
- CPU work per frame: the waits for the card (the swapchain fence, the acquire, batches waited for) are taken out and printed as "Waited for the card per
  frame", so "limited by" is right again (also on the Performance card's work time).
- Tested: check_6b benchscenes (new checks: a GPU time for every scene below 1.25 x its frame time, rest.orbit and rest.molecule with a GPU time). Zero
  validation errors with VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true (orbit.sky, atom.slice_super_drag, blackhole.film_tilt, rest.orbit).
## 3. The worst frames (app/frame_cost.*, app/bench.*, main.cpp)
- Every scene lists its three worst counted frames: the frame thread's work, waiting for the card, acquire, present, pipelines made, batches waited for
  (the longest named: the glare's blocks, the sky's check, the exposure grids ...), the swapchain made again, a worker joined, guarded steps deferred, the
  benchmark's own steps, events, and what is left. test_app checks the order and the text.
- Found here and fixed (plain waste): film_tilt compared its two whole pictures again in every frame after the one at rest (counted time); the frame after a
  picture read back for the benchmark's own check was counted (its time held the read-back: vkQueueWaitIdle and a copy of the whole window).
- Found here, not fixed: reading the GPU results waits up to 260 ms on lavapipe while the card is busy (ROADMAP); film_tilt's present takes up to 800 ms on
  lavapipe. To be read in the owner's next benchmark: film_tilt's 969 ms frame, orbit.heaviest's 20.7 ms and nucleus_zoom's 19.1 ms frames, the Black Hole
  scenes' lower 1 % low on Vulkan (ROADMAP, Waiting for the owner).
## 4. The benchmark's start
- Here: 362-533 ms from Start to the first scene (timer check 276-421 ms, 9 known loads). Waste cut: each known load of the timer check made a host buffer,
  copied the whole picture to it and unpacked it (on a fast card the load doubles up to 16384 rectangles: up to ~99 loads of 2560 x 1369). The header now
  has the start part by part; the box shows "Preparing the benchmark ..." for two frames before the check.
## 5. Texts
- The header says "off by its guard: <why>" for the outline, rocks and superposition when their Vulkan guard is off; the Path lines name Vulkan or OpenGL 4.3
  as in use; scene texts, tooltips and the Settings rows name both (check_5l search: ROWS_LAB labels changed deliberately, 292 checks, 0 failed).
## Checks
- check.sh quick 18 of 18; check_5l search 292/0; check_6b benchscenes 104 of 105 (film_tilt's unchanged picture, as 0.63.0, ROADMAP), benchwaits 9 of 10
  (atom_density's 15 s cap, as 0.56.4-0.63.0, ROADMAP). UI suite not run (X.Y.Z).
## For the owner's PC
- Start 0.63.1 (log: "[vulkan] [outline] Not trusted ...", then "... the whole path succeeded" once an outline is drawn); run Hard; send it and gpu-paths-log.txt.
