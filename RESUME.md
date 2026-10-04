# Resume

## Current state
- Version 0.11 alpha, stage 1 of 4 is finished: Auto photons now say why they find no jump (`AtomLab::whyNoJump`, shared by `manualJump` and the auto block in `advance`; the reason is said once per cause via `s.autoSaid`, reset by `init()` and `toggleAuto()`; no random number is used differently). Complex states and superpositions still refuse jumps, on purpose: the options (and the physics) are in native/REPORT.md and wait for the owner. The CMake/app version number is still 0.10.9 (changelog entry is labelled 0.11); stages 2-4 (the chart of nuclides) are not started. Everything of 0.10.9 (measured nuclear size and shape) stands unchanged.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,889
  - app_tests: 1,520
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
