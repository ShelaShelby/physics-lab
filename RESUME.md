# Resume

## Current state
- Version 0.11 alpha, stage 2 of 4 is finished: the chart of nuclides (flat). Logic in `atom/chart.h/.cpp` (tiles, the colour rule, layout, hit test, arrow step, hover words, free-room measure; all tested), drawing in `atom/chart_draw.cpp` (rect calls only, tested through a recording stand-in for the renderer), data in `atom/nubase.h/.cpp` + `nubase_data.inc` (from data/nubase/nubase_4.mas20.txt by tools/make_nubase_data.py: spin, kind of half-life, '?' entries of stable nuclides). View name `kChartView` = "Chart of nuclides"; `AtomLab::chartOn()`, `chooseNuclide`, `chartStep`; in the chart view `step` returns early, `decTick` waits, auto photons wait, Emit/Absorb say so. The hover is an ImGui tooltip in `atom_view.cpp`, arrow keys in `main.cpp`. Benchmark scene 16. App version 0.11. Stages 3 (2.5D landscape) and 4 (heights, island of stability) are not started. Stage 1 (Auto photons say why) and all of 0.10.9 stand unchanged.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,987
  - app_tests: 1,526
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
