# Resume

## Current state
- Version 0.10.6 alpha, stage 3 is finished: one energy for one atom (the Cloud and Orbit readouts use the reduced-mass level `levelEV`; hydrogen reads −13.60 eV everywhere; the Orbit view's energy is scaled by rydM/kRydInf), the Orbit view splits its leapfrog step so no sub-step turns more than 2π/300 rad (radius error 2.2e-4 at every speed, was 7.8e-3 at speed 30, n = 1), and Scale Fixed uses kExt[4] = 40 a0 for clouds. The `webJumps` test flag also keeps the web's single leapfrog step. Stage 2 (CODATA 2022 constants) and stage 1 (audit, `git show 90049fe:native/REPORT.md`) are in git history.
- Not done, still on the roadmap: audit findings 1, 2, 5-7, 9-13, 15-17 (next: the 90 % outline of complex states, Superposition of equal states). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u and the proton radius (kept at the web's rounded 0.84 fm). The owner picks the next fix tasks.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,085
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
