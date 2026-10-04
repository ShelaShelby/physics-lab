# Resume

## Current state
- Version 0.10.9 alpha, stage 2 is finished: the nucleus's shape from measured quadrupole moments. `atom/quadrupole.h/.cpp` + `quadrupole_data.inc` (from data/quadrupole/elec_mom_recomm.csv, IAEA, fetched 2026-10-04 by tools/make_quadrupole_data.py; the file's second column is the MASS NUMBER): `nucShape(Z, N)` = a measured ground-state moment converted with the strong-coupling relation K = I (|beta2| <= 0.6, signed, firm spin >= 1: 498 of the slider's isotopes), else the B(E2) value (11), else the estimate (3,743); `buildNucleus` draws a volume-keeping body whose protons have exactly the measured rms (stage 1) and quadrupole; card "Nuclear size and shape (measured)" and the readout line name the source. Stage 1 (sizes from the IAEA charge radii) stands. Halo nuclei stay BLOCKED ON DATA the owner downloads (ROADMAP, OWNER-TODO).
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,845
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
