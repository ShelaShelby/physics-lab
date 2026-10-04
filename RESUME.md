# Resume

## Current state
- Version 0.10.6 alpha, stage 7 is finished: the benchmark has a tick box "Include Atom Lab speed-risk scenes" with 11 new scenes (numbers 8-18, `atom_bench.cpp`; a per-frame hook `atom::benchFrame`; `clearOutlineCache()` in outline.cpp) for audit findings 10, 11, 12. They were never timed (no GPU here); nothing was optimised. The next step is the owner's benchmark numbers from the PC, then a stage that fixes what is slow. Stage 6 (before it): the nucleus's mass is Z·m_p + N·m_n − B/c² (`nuclearMassMe`, from the binding table = AME2020, checked against `mass.mas20.txt`; 695 slider nuclides beyond the table keep A·u − Z·m_e), so the deuterium and tritium lines move (D-alpha −1.26 pm); the spectrum is now tested against the Rydberg formula by hand and against NIST ASD (components widened by (Zα)²); outline masses, the 1s norm, radial nodes and the sampled <r> at Z = 3, 26 are tested. NIST, ASD and AME were all reachable in that session. Stages 5 (words), 4 (outline, superposition norm), 3, 2 (CODATA 2022) and 1 (audit, `git show 90049fe:native/REPORT.md`) are in git history.
- Not done, still on the roadmap: the speed fixes for audit findings 10-12 (they wait for the owner's timing of the new scenes; candidates with line numbers are in the stage 7 report in git history). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u (AME2020 has 1.007825031898 u, not yet put in the code) and the proton radius (0.84 fm). The owner picks the next fix tasks.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,157
  - app_tests: 1,515
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
