# Resume

## Current state
- Version 0.10.9 alpha, stage 1 is finished: every nucleus at its measured size. `atom/radii.h/.cpp` + `radii_data.inc` (from data/radii/charge_radii.csv, IAEA, fetched 2026-10-04 by tools/make_radii_data.py): `nuclearRadius(Z, N)` = sqrt(5/3) x the measured rms charge radius (908 isotopes), else the rule 1.2 A^(1/3); `buildNucleus` scales each snapshot so its protons have exactly the measured rms (`buildNucleusByRule` is the web's layout, kept for the saved web positions); the card "Nuclear size (measured)"; About entry. Shape (quadrupole moments) and halo (matter radii) are BLOCKED ON DATA the owner must download (ROADMAP, OWNER-TODO).
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,578
  - app_tests: 1,519
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
