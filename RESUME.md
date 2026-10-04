# Resume

## Current state
- Version 0.10.7 alpha, stage 2 is finished: the real NIST ASD spectrum beside the lab's. `tools/make_asd_data.py fetch` (network: physics.nist.gov) wrote `native/data/asd/asd_lines.tsv` + `manifest.tsv` + README.txt (query, date 2026-10-04, selection, citation) and `build` makes `src/atom/asd_data.inc`; `src/atom/asd.*` gives `asdIon(Z)` (hydrogen-like ion: validates the lab), `asdAtom(Z)` (neutral atom: shows its limit), `compareIon`, and the card's words; the card is drawn in `atom_panel.cpp` (`realStrip`, `realSpectrumCard`) like the spectrum picture, NOT compiled in the cloud (stand-in syntax check only) and never seen: the owner must look (OWNER-TODO). Tests: `testAsdData` in test_atom.cpp (data counts, H I / He II within (Zα)², Fe XXVI Lyα against the Dirac formula) and one new About entry in test_app.cpp. NIST's general licence statement could not be read (www.nist.gov unreachable): OWNER-TODO. Stage 1 (before it): the radial probability graph (`radial_graph.*`, card "Where the electron is found"). The 0.10.6 audit is closed; three optimisation items are deferred by the owner (ROADMAP).
- Not done, still on the roadmap: the three deferred optimisation items (phase-colour scan, overlapping superposition, outline first use; numbers in ROADMAP). Not re-read from a source: the hydrogen-1 atom mass 1.00782503223 u (AME2020 has 1.007825031898 u, not yet put in the code) and the proton radius (0.84 fm). The owner picks what comes next.
- Test programs and their last known check counts:
  - orbit_tests: 1,540
  - atom_tests: 18,326
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
