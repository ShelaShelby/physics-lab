# Resume

## Current state
- Version 0.10.6 alpha, stage 1: the Atom Lab audit, a report only (native/REPORT.md, 18 findings); nothing in the app changed. NIST was unreachable in that session (403), so no CODATA 2022 or NIST ASD value was compared. The Orbit Lab audit (0.10.5) is finished.
- Next: the owner reads the Atom Lab audit and picks the fix tasks (on the roadmap as planned). After them: the audit of the app and settings. The Orbit speed rows still wait for the owner's timing.
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
