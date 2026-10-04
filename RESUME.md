# Resume

## Current state
- Version 0.10.8 alpha, stage 1 is finished: relativity as a real option in the Atom Lab. `src/atom/dirac.*` has the exact Dirac (Sommerfeld) level of a one-electron atom with a point nucleus, `diracEV(Z, N, n, twoJ)` (the lab's reduced-mass level + the infinite-mass relativistic correction), the selection rule (l changes by 1, j by 0 or 1), `diracLines` (lines split by j, equal energies merged), `specLinesFor` / `visibleLinesFor` (the old lists when off, bit for bit), the level rows and the words of the setting. `asd.*` gained `asdParseTransition` and `compareIonDirac` (NIST components matched by n and j). `AtomParams::dirac` ("Off" by default; setting key `dirac`, saved only when On). The panel shows the setting under "Light spectrum"; the strip, the visible lines and the Real spectrum card all use the same line list. Only the two spectrum cards change: pictures, readouts, photon jumps and beats stay non-relativistic (said in the app). Measured against NIST (worst component, n <= 6): H I 7.97x closer, He II 9.67x, Fe XXVI 31.9x; what is left is the Lamb shift, QED and nuclear size. The drawing is not compiled or seen in the cloud (OWNER-TODO). The earlier 0.10.7 work (radial graph, NIST real-spectrum card) is described in REPORT history in git.
- Not done, still on the roadmap: the three deferred optimisation items (phase-colour scan, overlapping superposition, outline first use; numbers in ROADMAP). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u (AME2020 has 1.007825031898 u, not yet put in the code) and the proton radius (0.84 fm). The owner picks what comes next.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,373
  - app_tests: 1,517
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
