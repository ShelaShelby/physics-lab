# 0.63.0: Vulkan, step 5 of 5: Vulkan the default, the benchmark and the tests on Vulkan, the guard's wrong-program fix, steps 1-3 on Vulkan 1.3 (based on 0.62.0; box: 4 cores, Mesa 25.2.8, lavapipe / llvmpipe)
The job was bigger than written (Part D touched 9 check scripts, Part E four files); the present.frag part of Part E is left (below).
## Part A: the guard blamed the wrong program (app/gpu_guard.*)
- Cause: a step's marker stayed on disk after the step ended, until the whole path had passed: the outline's "first small run" marker was still there when the light
  paths' pipeline crashed the driver. Now a guarded step's marker goes when it ends, and at most one guarded step is open in the whole process (all guards, both APIs).
  A worker's begin() waits for its turn (logged: "the step '...' waits for its turn: <other> is open", then "has its turn (waited N ms)"); the frame never waits:
  each step the frame starts asks hold() first and moves to a later frame (logged once: "... waits for its turn (moved to a later frame)"). A run of steps (the
  light paths' pipeline, check and first fill; the sky's) keeps the turn between its steps; rest() gives it back before a wait of unknown length (the first full
  use). Guards that passed ("ok") are not held back. Deferred first uses: that frame on the processor; the outline asks again next frame (waiting()).
- Tests first: test_app (two guards: the second's begin on a worker writes no marker until the first ends; hold() says no on the frame; rest; ok not held) and
  check_shell.py:guardturn (ui_shell): PL_GPU_DIE in the light paths' pipeline, next start: light paths off, outline allowed, its first full use drawn.
  On 0.62.0's build: 1 of 4 failed (gpuvkoutline=off, "the outline's first small run", as on the owner's PC); on 0.63.0: 4 of 4.
- Cost on this box: at a FIRST start the sky per pixel now waits for the light paths' check and first fill (over a minute on lavapipe). On the owner's PC the light
  paths are off (AMD crash) and the other guards are "ok", so nothing waits. PL_BH_SETTLE also waits while a guarded step is open or waiting (gpuGuardBusy).
## Part B: Vulkan the default
- prefs: default "vulkan" (not written), "opengl" written when chosen; a file with graphics=vulkan/opengl keeps it (test_app). Settings row "Graphics: Vulkan /
  OpenGL (backup)", radio "OpenGL (backup)" with its help (older cards, a driver problem). The fall-back to OpenGL (card below 1.3, PL_VK_FAIL, PL_VK_CHECK=break)
  unchanged and checked (check_shell vulkan, vulkan2d, vulkanbh). No lab's Most realistic names a graphics system (nothing to change there).
## Part C: the benchmark on Vulkan
- The refusal removed. Card time per frame from timestamp queries: the frame's command buffer plus the card batches submitted for it (VkCards::setTiming), summed;
  span first start to last end. Timer check as OpenGL's: the same known load drawn into an image, timestamps against a clock (here: 31.4 vs 33.4 ms, reliable).
  Report: "Graphics: Vulkan 1.4.318, FIFO ..." / "Graphics: OpenGL ...", the timer's name, and "The card programs' guards (Vulkan|OpenGL)" with each state.
  Shots and film_tilt's grab read back from Vulkan. Zero validation errors in a benchmark run.
- New Hard scene atom.slice_super_drag (stable id, which 39): Md-258, Slice, 2p + 5f 50 %, Relativity (Dirac levels) On, Wheel, Classic Ultra, the cut dragged as
  slice_drag. Zooms tried (1 % low fps here): -0.5 2.70, 0 2.70, 0.5 2.95, 1.0 3.35, 1.5 2.86: zoom 0 kept. Processor work on both APIs (ROADMAP Atom Lab).
## Part D: the tests on Vulkan
- The UI suite runs on the default (Vulkan). On OpenGL by their own setting (PL_GRAPHICS=opengl): check_shell's reference runs, check_blackhole's PL_BH_COMPARE,
  guard and smooth parts, check_atom_chip card and check_molecule card (OpenGL's guard records), check_5h cards (compared with stored OpenGL runs). New part
  check_shell.py:openglbackup (ui_shell): no choice = Vulkan; graphics=opengl in the file = OpenGL; each lab opens on OpenGL and draws.
- The checks' diagnostics (PL_DRAWSTAT, PL_BH_FRAMETIME ...) were printed in main.cpp's OpenGL branch only: now on both. Deliberate reference changes: check_5l
  (the row's new label), test_app (Graphics: line, 26 / 42 scenes, kBenchAtomScenes 40), check_5q sliders (the fill sampled left of the thumb: the thumb's
  anti-aliased edge differs between drivers).
- Suite time: about 64 min on Vulkan (0.62.0 on OpenGL: 35 min).
## Part E: steps 1-3 on Vulkan 1.3
- vk_draw2d, vk_card (the exposure grid), vk_backend (window, Save image): render passes and framebuffers -> dynamic rendering, every barrier ->
  synchronization2, each picture image's layout tracked. Before == after: orbit, atom Glow + outline, atom Slice, monochrome, molecule, black hole Film:
  0.000 % of pixels differ; zero validation errors with VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true in each (one sync error found and fixed: the present barrier).
- Left (3-try rule not needed, a design limit): drawing in Vulkan's rows and dropping present.frag. shaders.h's lens, sky, mask, grid and grey shaders read
  gl_FragCoord with OpenGL's bottom-left origin and are OpenGL's text too; changing them is for the job that removes OpenGL (ROADMAP "Whole app").
## Not fixed (3 tries each)
- check_6b benchscenes: blackhole.film_tilt shows no new picture in its counted time on a fresh settings folder (both APIs; 0.62.0 passed): the guards' one-at-a-
  time order (Part A) holds the sky back; warm-up starts before the run did not cure it. all_light's -1 and benchwaits as before (ROADMAP).
## For the owner's PC
- Start 0.63.0: it opens on Vulkan. Run the benchmark (Hard) and send it with gpu-paths-log.txt. The outline is "allowed again, guarded" and should now run its
  first step; the light paths stay off (the crash is caught).
