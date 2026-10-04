# Resume

## Current state
- Version 0.10.6 alpha, stage 4 is finished: the 90 % outline is drawn for the state on screen (`AtomLab::outlineStateId()`, a complex state's own id), and a superposition is normalised for any pair: `overlapAB`/`superNorm2` in atom_core, `psiSuper` divides by the norm only for overlapping pairs (same n and l, non-orthogonal angular parts), which are static, sampled directly with every dot shown; the readout says "amplitudes … renormalised" for them. Orthogonal pairs, saved references and hashes are bit for bit unchanged. Stages 3 (one energy, Orbit sub-steps, Fixed scale), 2 (CODATA 2022 constants) and 1 (audit, `git show 90049fe:native/REPORT.md`) are in git history.
- Not done, still on the roadmap: audit findings 5-7, 9-13, 15-17 (next: 6 heavy-atom text, 7 timeline texts). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u and the proton radius (kept at the web's rounded 0.84 fm). The owner picks the next fix tasks.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,098
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
