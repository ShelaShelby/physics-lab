# Resume

## Current state
- Version 0.11 alpha is finished (stage 4 of 4): the chart of nuclides is flat, a landscape with two choices of height, with the valley (measured) named and the island of stability stated as a prediction and not drawn. New: `atom/ame.h/.cpp` + `ame_data.inc` (AME2020 binding energy per nucleon with the '#' estimate flag; tools/make_ame_data.py; data/ame), `chart_view.h` (`ChartHeightMode`, `chartHeightFromBinding`, `kBindingTopKeV` 9000 and `kBindingCells` 28, the words), `chart.h` (`chartValleyAnchor` = the median stable tile, `chartHeaviestZ`, the valley / island / heaviest words), `chart_draw.cpp` (`drawNotes` labels placed where they cover nothing, `drawNotesBox`). State: `ChartViewState::height`, `setParam("cheight")` (not saved); the fit no longer depends on the height mode. Benchmark scene 18 (the owner sees 21). The island: NO source could be read (every host answered 403), so no region and no predicted number are drawn or quoted; the lab's own unsourced constants kMagicN 184 / kMagicZ 114 in atom_data.inc are NOT used for it. Stages 1-3 and all of 0.10.9 stand, except what REPORT.md lists as moved on purpose.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 19,106
  - app_tests: 1,538
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
