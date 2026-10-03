# Resume

## Current state
- Version 0.10.5 alpha is finished (stage 5, the last): the Orbit Lab audit and its fixes (stage 1 report, 2 text fixes, 3 data fixes, 4 the IAU astronomical unit, 5 tests and sources). native/REPORT.md is the stage 5 report. The year (365.25 d) and the Sun's GM built from it are unchanged (3.8e-5 above JPL's). The IAU's own text for the au (Resolution B2) could not be opened: the code still quotes JPL's page ("B1"). Real is still an option, Simple the default, until the owner has tried it.
- Left from the audit: the speed rows (ROADMAP Later: wait for the owner's timing). Next on the roadmap: the audits of the Atom Lab and of the app and settings, each report-only first.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 17,936
  - app_tests: 1,385
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
