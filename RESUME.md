# Resume

## Current state
- Version 0.11 alpha, stage 3 of 4 is finished: the chart of nuclides as a landscape. New `atom/chart_view.h/.cpp` (pure logic: `chartHeightOf` = the stated log10 mapping, `ChartProjection`, `ChartScene` = blocks in a fixed painter's order, `pick` = the topmost drawn block, `clamped` = zoom and pan limits); `chart_draw.cpp` has `drawChartScene` (the landscape) beside stage 2's `drawChart`, which is still used when the view is exactly flat (tilt 0, zoom 1, no pan). State: `AtomState::chartView` (tilt, zoom, pan; not saved, default flat); `AtomLab::setChartTilt/chartZoomAt/chartPan/chartReset/chartScene`; `setParam("tilt")`; the panel slider, the wheel in `main.cpp`. Height scale 10^-23 s .. 10^32 s, 12 tiles per scale height, stable plateau 1.1; benchmark scene 17 (the number the owner sees: 20). Stage 4 (what the height can mean, the island of stability) is not started. Stages 1-2 and all of 0.10.9 stand unchanged.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 19,061
  - app_tests: 1,532
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
