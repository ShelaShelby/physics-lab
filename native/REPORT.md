# 0.55.7: the graphics card's light paths can no longer stop the app (a guard and a log)
No source read or saved; no physics changed; no test loosened. The owner's crash (0.55.6, Radeon RX 9060 XT, driver 26.10.1) is NOT explained here (no AMD card): made harmless, and logged.
## The guard (app/gpu_guard.h/.cpp, no graphics; used by render/kerr_compute, blackhole/spin_card, blackhole_view)
- Before each step of the card's path, until the whole path (check + first fill + the worker's read) has succeeded once for this app version and driver
  (driver = GL_VENDOR / GL_RENDERER / GL_VERSION), a marker `gpu-paths-step.txt` (app version, driver, step) is written beside the settings file:
  fopen/fwrite/fflush, then `_commit` on Windows (`fsync` elsewhere). Removed when the sequence ends (passed or failed) and at a normal close.
- At start a marker left behind = the last run died inside that step: the path is off, the shader is not compiled, the processor's path of 0.55.5 (with 0.55.6's
  faster stars) draws. The settings file keeps `gpupaths=off | version | driver | step` (written at once; the marker is removed only after that write).
  Same version and driver: stays off. Another driver string or app version: tried once more (guarded). Success keeps `gpupaths=ok | ...` (no markers after).
- Settings, Performance (Black Hole Lab): "The graphics card's light paths stopped the app last time (in the step '...') and are switched off for this graphics
  driver.", a button **Try again** (clears it; the path is tried in the same run) and **Open the log's folder**.
- Time limit: a check step, pass A or a piece of the first fill has 10 s (`cardStepLimitMs`), a later fill's whole pass B 60 s. Over it: off for this run and kept as
  `gpupaths=slow | ... | step` (same rules). The frame never waits: the start-up check is now a step per frame (CardStartup; `cardSelfCheck` keeps waiting for the
  tests, each wait bounded to 10 s, a timed-out read gives NaN, never stale numbers). `done()` no longer treats GL_WAIT_FAILED as finished.
- Nothing before the first frames: the shader is compiled at the view's third asynchronous frame; until the check has decided, no processor table is started.
## The log (`gpu-paths-log.txt` beside the settings file; on Windows `%APPDATA%\PhysicsLab\PhysicsLab\gpu-paths-log.txt`)
- Overwritten at each start, at most 400 lines, each flushed before the step it names (committed while guarded), with ms since start: app version, GPU, vendor,
  OpenGL and GLSL strings, double precision reported; the guard's decision; compile begin/end with the driver's info log (also when it compiled), link likewise;
  the check: dispatch 1, its fence wait, read-back, the functions' difference, per case the edges' and rays' dispatch / fence wait / read-back, the numbers and
  verdict; the first fill: pass A, the rows' edges, every piece with its rays and time, the copy, the mapping, the worker's read; the fallback's reason.
  The benchmark report ends with the log's last 40 lines.
- Smallest first dispatch: the check's first dispatch is now the shader's double functions at 8 values (one work group; limit 1e-13 against the C++ library, measured 2e-16 here, float
  would be 1e-7). The run's first fill goes piece by piece: 64, 256, 1024, 4096, then 8,192 rays (the 0.55.6 piece); later fills as before.
## Part 3: glslangValidator and the read-through of kKerrTableCS. glslang: no error and no warning for any shader (also before). check_shaders.py (ctest glsl_shaders, quick) now fails on a warning and compiles the compute
  shaders to SPIR-V for OpenGL, validated by spirv-val (all pass).
- Read-through: no recursion; every loop bounded (ray 200,000 steps, bisections 80/200, Newton 100); double built-ins only frexp, ldexp, floor, abs, sqrt, min,
  max, clamp, isnan, isinf (all GLSL 4.00); buffers std430 double arrays at offset 0 (8-byte stride); private arrays indexed by variables (u[found], t[i]: scratch
  memory, legal). The worry is size: every call inlined the shader is ~700 KB of SPIR-V. **Changed**: the equator crossing's Newton steps and its last trial step
  share one dpStep call (same steps, same order): inlined 700 -> 556 KB. Precision 97 of 97 with unchanged limits.
- Mapping: the worker reads the mapped nodes while the frame goes on (legal, non-persistent map). **Changed**: pass B is refused while that buffer is mapped
  (its storage cannot be replaced under the worker). A dispatch over 2 s on Windows (TDR) resets the driver: the log's piece times will show that.
## Tests
- app_tests (no graphics): the settings line, every decision rule, the marker, the next start, same/other driver, Try again, success, timed out, the log. 0 failed.
- check_blackhole.py smooth (new part `smooth_guard`, no new file or ctest test): the log's 28 steps in order; PL_GPU_DIE in compile, in a check fence wait and
  in fill piece 2: the marker names the step, the next start compiles nothing, draws on the processor's path, Settings names it, the file keeps it; same
  driver stays off; PL_GPU_DRIVER other: tries again and passes; Try again (PL_OPEN=settings:gpupaths, new) passes and keeps ok; PL_BH_CARDHANG with a 3 s limit:
  frames go on during the wait, then the processor's path, kept slow. All pass.
- By hand: smooth 35 checks, 1 failed: "card: a new picture after 9 of 16 frames in which the view moved" (needs 90 %). **Not this job**: 0.55.6's own build gives
  10 of 16 on this box too (the picture comes two frames after the first moved frame of each drag step); ROADMAP line. settings 4/4; precision 97/97.
- check.sh quick: 17 of 17 passed. **The full UI suite was not run (small version).** Not testable here: anything on an AMD card (the log will say where it dies).
