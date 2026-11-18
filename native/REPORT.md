# 0.63.0: Vulkan, step 5 of 5: Vulkan the default, the benchmark and the tests on Vulkan, the guard's wrong-program fix, steps 1-3 on Vulkan 1.3 (based on 0.62.0; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe)
Bigger than written (Part D: 9 check scripts); Part E's present.frag part left (below).
## Part A: the guard blamed the wrong program (app/gpu_guard.*)
- Cause: a marker stayed on disk after its step ended (the outline's "first small run" was there when the light paths' pipeline crashed). Now it goes at end(),
  and one guarded step is open in the whole process: a worker waits (logged, naming the other); the frame asks hold() and moves to a later frame (logged once);
  a run of steps keeps the turn, rest() gives it back before the wait for a first full use; "ok" guards are not held. A deferred use: that frame on the processor.
- Tests first: test_app (two guards, a worker's begin waits, hold, rest, ok) and check_shell.py:guardturn (PL_GPU_DIE in the light paths' pipeline; next start:
  light paths off, outline allowed and used): 0.62.0's build 1 of 4 failed (gpuvkoutline=off, as on the owner's PC); 0.63.0: 4 of 4.
- Cost: at a first start here the sky waits for the light paths' check and first fill (over a minute on lavapipe); on the owner's PC they are off. PL_BH_SETTLE waits too.
## Part B: Vulkan the default
- prefs: "vulkan" the default (not written), "opengl" written when chosen, either kept from a file (test_app); Settings "Graphics: Vulkan / OpenGL (backup)" with
  its help (older cards, a driver problem). Fall-back to OpenGL unchanged and checked (check_shell). No lab's Most realistic names a graphics system.
## Part C: the benchmark on Vulkan
- The refusal removed. Card time per frame from timestamp queries: the frame's command buffer plus the card batches submitted for it (VkCards::setTiming), summed;
  span first start to last end. Timer check as OpenGL's: the same known load drawn into an image, timestamps against a clock (here: 31.4 vs 33.4 ms, reliable).
  Report: "Graphics: Vulkan 1.4.318, FIFO ..." / "Graphics: OpenGL ...", the timer's name, each card guard's state. Zero validation errors in a run.
- New Hard scene atom.slice_super_drag (stable id, which 39): Md-258, Slice, 2p + 5f 50 %, Relativity (Dirac levels) On, Wheel, Classic Ultra, the cut dragged as
  slice_drag. Zooms tried (1 % low fps here): -0.5 2.70, 0 2.70, 0.5 2.95, 1.0 3.35, 1.5 2.86: zoom 0 kept. Processor work on both APIs (ROADMAP Atom Lab).
## Part D: the tests on Vulkan
- The UI suite runs on the default (Vulkan). On OpenGL by their own setting (PL_GRAPHICS=opengl): check_shell's reference runs, check_blackhole's PL_BH_COMPARE,
  guard and smooth parts, check_atom_chip card and check_molecule card (OpenGL's guard records), check_5h cards (compared with stored OpenGL runs). New part
  check_shell.py:openglbackup (ui_shell): no choice = Vulkan; graphics=opengl in the file = OpenGL; each lab opens on OpenGL and draws.
- The checks' diagnostics (PL_DRAWSTAT, PL_BH_FRAMETIME ...) were printed in main.cpp's OpenGL branch only: now on both. Deliberate reference changes: check_5l
  (the row's new label), test_app (Graphics: line, 26 / 42 scenes, kBenchAtomScenes 40), check_5q sliders (the fill sampled left of the thumb: the thumb's
  anti-aliased edge differs between drivers).
- Full suite on Vulkan (with Part E): 50 min (0.62.0 on OpenGL: 35 min); 31 of 34 passed: ui_popups (check_6b, below), ui_molecule (check_molecule card,
  the old failure), ui_blackhole (check_blackhole smooth, below). check_shell vulkan2d/vulkancards/vulkanbh 140 of 140; check.sh quick 18 of 18.
## Part E: steps 1-3 on Vulkan 1.3
- vk_draw2d, vk_card (the exposure grid), vk_backend (window, Save image): render passes and framebuffers -> dynamic rendering, every barrier ->
  synchronization2, each picture image's layout tracked. Before == after: orbit, atom Glow + outline, atom Slice, monochrome, molecule, black hole Film:
  0.000 % of pixels differ; zero validation errors with VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true in each (one sync error found and fixed: the present barrier).
- Left: Vulkan's rows and no present.frag: shaders.h's gl_FragCoord readers (lens, sky, mask, grids, grey) are OpenGL's text too; with OpenGL's removal (ROADMAP).
## Not fixed (3 tries each)
- check_6b benchscenes: blackhole.film_tilt shows no new picture in its counted time on a fresh settings folder (both APIs; 0.62.0 passed): the guards' one-at-a-
  time order (Part A) holds the sky back; warm-up starts before the run did not cure it. all_light's -1 and benchwaits as before (ROADMAP).
- check_blackhole smooth, the card's light paths: a new picture in 3 to 9 of 16 moved frames on Vulkan here (OpenGL in the same box under load: 4 of 16 for
  the processor's too): timing on the software renderer; tried on both APIs, left (ROADMAP).
## For the owner's PC
- Start 0.63.0: it opens on Vulkan. Run the benchmark (Hard) and send it with gpu-paths-log.txt. The outline is "allowed again, guarded" and should now run its
  first step; the light paths stay off (the crash is caught).
