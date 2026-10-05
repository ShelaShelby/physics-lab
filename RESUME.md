# Resume

## Current state
- Version 0.13 alpha, "GPU update", is COMPLETE (stage 4 of 4 done: the Orbit Lab's rocks on the GPU, the chart of nuclides kept in a GPU buffer, the Cloud 2D view's cost fixed). 0.14 is not decided. What 0.13 is: OpenGL 4.3 with a 3.3 fallback (the CPU paths stay as the reference); phase colours (stage 1), superposition and jump colour groups (stage 2), the 90 % outline and exact normalisation (stage 3), rocks and chart (stage 4) on the GPU; five switches in the Performance card (GPU colours, superposition and jumps, outline, rocks, chart cache), none saved. New in stage 4: `src/render/rock_compute.*` + `kRocksCS`, `src/orbit/belt_layer.*`, `solveKepler`/`beltScreen` in `src/orbit/belts.*`, `src/render/mesh.h` + `kChartVS` + `Renderer2D::meshCreate/drawMesh`, `src/atom/chart_mesh.*`, `ChartCache` in `chart_draw`, `tests/test_rocks.inc` + `rock_reference.h`, `tests/test_chart_cache.inc`, `tests/test_stage4_cloud.inc`, `tests/gl_rocks.inc`, `tests/gl_chart.inc`, `tests/timing_stage4.cpp` (`-DPL_TIMING=ON`). Speed is NOT measured: the owner runs the benchmark (OWNER-TODO).
- Test programs and their last known check counts:
  - orbit_tests: 1,612 (72 added in 0.13 stage 4)
  - atom_tests: 21,066 (84 added in 0.13 stage 4)
  - app_tests: 1,616 (16 added in 0.13 stage 4)
  - glsl_shaders (glslang, needs `glslang-tools`; skipped if absent; 15 shaders, 4 compute) and gl_phase_colours (only with -DPL_GL_TESTS=ON: 503 checks on llvmpipe)
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
