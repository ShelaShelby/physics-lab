# Resume

## Current state
- Version 0.10.7 alpha, stage 1 is finished: the Atom Lab's radial probability graph. `src/atom/radial_graph.*` (no drawing): `radialDensity` / `radialGraph` give P(r) = r²R² (area 1, normalised by the closed form; the lab's `radial()` lacks the q^l factor, handled there), the peak (exact roots of dP/dr), `meanRadius` (shared with the readout), the 90 % reach (outline rmax / Z), the axis step and the card's words. `AtomLab::radialMode()` says shown / hidden / superposition / jumping. The card is drawn in `atom_panel.cpp` (`radialPicture`, `radialCard`) like the spectrum picture, computed only when the orbital or Z changes; it was NOT compiled in the cloud (only a stand-in syntax check) and never seen: the owner must look (OWNER-TODO). Superposition: the card gives an explanation and no graph (DECISIONS). Tests: `testRadialGraph`, `testRadialDots`, `testRadialReadout` in test_atom.cpp. The 0.10.6 audit is closed; three optimisation items are deferred by the owner (ROADMAP).
- Not done, still on the roadmap: the three deferred optimisation items (phase-colour scan, overlapping superposition, outline first use; numbers in ROADMAP). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u (AME2020 has 1.007825031898 u, not yet put in the code) and the proton radius (0.84 fm). The owner picks what comes next.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,299
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
