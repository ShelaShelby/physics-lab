# Resume

## Current state
- Version 0.13 alpha, stage 3 of 4 ("GPU update", the 90 % outline on the GPU and exact orbital normalisation) is done; stage 4 (Orbit Lab rocks on the GPU, chart and landscape geometry cached on the GPU) is on the ROADMAP. Stages 1 and 2 are done. New in stage 3: `src/render/outline_compute.*` (OpenGL 4.3 jobs with fences, the `IOutlineCompute` interface), `kOutlineDensityCS` and `kOutlineLevelCS` in `shaders.h`, `outlineReady`, `SliceContour`, the public CPU pieces (`outlineGridCpu` ...) in `src/atom/outline.*`, `radialGraphNoWait`, `AtomLab::gpuOutline`, benchmark scenes 29 and 30, the exact `orbitalNorm`, `tests/test_outline_norm.inc`, `tests/timing_stage3.cpp` (`-DPL_TIMING=ON`). Speed is NOT measured yet: the owner runs the benchmark (OWNER-TODO). The Slice contour trails the cut by a frame on the GPU path (DECISIONS.md)
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 20,982 (58 added in 0.13 stage 3)
  - app_tests: 1,600 (17 added in 0.13 stage 3)
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
