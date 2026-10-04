# Resume

## Current state
- Version 0.12 alpha, stage 1 of 2 is done: decay chains on the chart of nuclides. New: `atom/decay_chain.h/.cpp` (graph of every way down a nuclide's chain, the random walk with `decDrawLifetime`/`decDrawBranch` shared with `decTick`, all the words), `chartHalfLifeOf`, `ChartScene::block`, `ChainOverlay` in `chart_draw`, state `s.walk`/`s.showChain`/`s.walkAuto`, `AtomLab::decayGraph/walkOnce/walkRestart`, panel section, benchmark scene 22 (index 19; scenes 19-21 draw no chain), tests in `tests/test_chain.inc`. Rules: no time for limit/extrapolated/unknown half-lives; unmeasured odds only as a marked equal stand-in; odds normalised; never real time. Stage 2 (data clean-up) is next; its findings are in REPORT.md. Version 0.11 (flat chart, landscape, binding height, valley/island) stands.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 19,232
  - app_tests: 1,544
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
