# Resume

## Current state
- Version 0.13 alpha, stage 2 of 4 ("GPU update", OpenGL 4.3 and the superposition and jump colour groups on the GPU) is done; stages 3 and 4 are on the ROADMAP. Stage 1 (phase colours on the GPU) is done. New in stage 2: `src/render/group_compute.*` (the OpenGL 4.3 compute class and the `IGroupCompute` interface), `kGroupsCS` in `src/render/shaders.h`, `render/gl_loader.h` has the 4.3 functions (`loadGL43`), `Renderer2D::compute()`, `AtomLab::gpuCompute`, `AtomLab::superGroupsGpu / jumpGroupsGpu`, `fillGroups` in `src/atom/cloud_dots.*`, benchmark scenes 25 to 28, `main.cpp` asks for 4.3 then 3.3, `tests/test_gpu_compute.inc`, `tests/gl_compare.cpp` (now also the 4.3 comparison and the 3.3 fallback), `tests/timing_stage2.cpp` (`-DPL_TIMING=ON`). Speed is NOT measured yet: the owner runs the benchmark (OWNER-TODO). One thing found and left alone: the orbital normalisation is off by 3e-6 to 1.6e-5 (DECISIONS.md)
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 20,924 (66 added in 0.13 stage 2)
  - app_tests: 1,583 (25 added in 0.13 stage 2)
  - glsl_shaders (glslang, needs `glslang-tools`; skipped if absent) and gl_phase_colours (only with -DPL_GL_TESTS=ON)
  - nbody_tests: 37
  - jpl_tests: 55 (new in 0.10.3: the Real planets against the Horizons tables in native/data/jpl)
  - moon_tests: 24 (new in 0.10.4 stage 1: the ELP2000-82B Moon against the Horizons Moon table; takes about 12 s)
  - moon_real_tests: 59 (new in 0.10.4 stage 2: Real-mode Earth and Moon against the Horizons Earth table, phases, fast speeds, the other planets not worse)

## How work flows
- The owner pastes a written task into a new Claude Code session (Sonnet 5.5, branch box `native`).
- The session works on its own side branch and pushes to `native` with `git push origin HEAD:native`. It is a plain push, never a PR.
- Each push is checked separately by a planning chat: fresh clone, build, all test programs, REPORT.md and the diff. After that come the GitHub build and the release tagged `latest`. The release is a Pre-release while the stage is alpha or beta.
- GitHub Actions builds the Windows zip only on pushes that change native/** or the workflow.

## Rules that matter most
- index.html is frozen (sha256 starts e5501387ae64).
- The accuracy and honesty rules in CLAUDE.md.
- No helper agents.
- One task per session.

## Keeping this file up to date
- At the end of each version, replace (do not append to) the lines in "Current state" with the new version and test counts.
