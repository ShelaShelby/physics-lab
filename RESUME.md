# Resume

## Current state
- Version 0.12 alpha is finished (stage 2 of 2, the accuracy clean-up, on top of stage 1's decay chains). New in stage 2: `tests/test_clean.inc`, `tools/fix_halflives_0_12.py` (rewrites half-life codes in `atom_data.inc` from the NUBASE file), `chainHalfLifeWords`, `decUnquantified`, `nucUsesUnsourcedMagic`, `nubase_data.inc` now carries the half-life text of plain values and the '?' decay entries for every nuclide. Rules now: a half-life is shown or timed only if NUBASE2020 has a measured or approximate value; Kr-78/Xe-124 are stable; Z = 114 and N = 184 are kept as labelled unsourced magic numbers; the odds are said to be incomplete where NUBASE lists modes with no intensity. Stage 1 (decay chains) and all of 0.11 stand. Next: nothing planned beyond the ROADMAP.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 20,474
  - app_tests: 1,545
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
