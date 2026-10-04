# Resume

## Current state
- Version 0.10.8 alpha, stage 3 is finished: real lifetimes and selection rules from NIST ASD. `AsdLine` gained `aki` (ASD's A, NaN when none), `acc` (its accuracy code) and `type` (blank = E1) from columns already in the 2026-10-04 fetch (no refetch; `tools/make_asd_data.py build`). `src/atom/lifetime.*`: `lifeOf(Z, level)` (every allowed way to a lower n, sum A, complete?, lifetime 1/sum A only when complete, an upper limit when partial, metastable 2s = no number), `jumpsBetween` (allowed with A, accuracy, share; or the rules that forbid it named, and ASD's forbidden-type line), the words. `AtomParams::jfrom/jto` (UI only, not saved). New card "Lifetimes and jumps (NIST)" in `atom_panel.cpp` (not compiled or seen in the cloud: OWNER-TODO). Only new functions and data columns were added: with the Dirac setting off and on, the dumps of stage 2 and stage 3 are byte-identical. ASD data oddity found: Cs LV 1s-2s listed as allowed (data/asd README).\n- Stage 2 (earlier) fixed the red Windows build of stage 1: test references use double-double and never `long double`; the Dirac correction and readout formulas have no cancellation. Stage 1 added the Dirac setting.\n- Not done, still on the roadmap: the three deferred optimisation items (phase-colour scan, overlapping superposition, outline first use; numbers in ROADMAP). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u (AME2020 has 1.007825031898 u, not yet put in the code) and the proton radius (0.84 fm). The owner picks what comes next.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,418
  - app_tests: 1,518
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
