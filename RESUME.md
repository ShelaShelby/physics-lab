# Resume

## Current state
- 0.20: Molecule Lab stage 2, the builder (src/molecule/builder.*: sketch editor with strict valences, refusals with reasons, undo / redo, Hill formula, mass, names of 23 molecules, ESTIMATED VSEPR shape with CCCBDB bond lengths, molbuild settings line; tests molbuild_logic + molbuild_* pictures) and layout fixes (Atom info text folds over the nucleus, Orbit sheet below the Legend chip, Molecule pan limited to 1.5 radii).
- 0.19: Defense Lab removed (src/defense, tests, docs; labs Orbit 0, Atom 1, Molecule 2; a 0.18 settings file still loads), dead code removed, full audit (native/AUDIT.md): fixes with tests (guide's "monochrome" word, number formatting, date overflow, search cut, Molecule Lab search, settings re-save); stop rule instead of a time limit; next: Molecule Lab stage 2 builder (C++).
- 0.18 stage 1: new Molecule Lab (lab index 3 then, 2 since 0.19; shown 3rd), viewer only: 15 molecules from CCCBDB values (owner-given; site unreachable), shapes built + centred + principal axes, NIST masses, ball-and-stick / space-filling, one instanced draw call, click atom/bond, settings v1; src/molecule, ui/shell_molecule.cpp; tests molecule_logic + molecule_pictures. NH3 A/B 5.6 % off CCCBDB (reported). Next: stage 2 builder.
- 0.16 stage 6: Accurate quarks more accurate (own wander per quark, d farther out than u, cut 0.6 fm, atom::accurateQuarks + testQuarkWander; tube 0.38 fm, halo rms 0.65 fm); Classic sea 60 % d; proton-mass card (Yang 2018), quark note, Fuzzy text; docs/QUARKS.md. Classic / Cinematic look unchanged.
- 0.16 stage 5: nucleus view with + Sea / Classic tubes: antiquark rings and flux tubes are blobs in the batch (Ds-281 x2063 + Sea Accurate: 449 -> 5 draw calls, 33,323 -> 30 vertices); light-vs-Midnight gap was Cinematic's light-theme ring path; ui6c_ds281sea; GPU fps not tested.
- Tools (no app change): native/tools/autoinstall (download watcher that installs each new PhysicsLab-windows-x64 zip into C:\PhysicsLab, CleanOld.ps1, Install/Uninstall); also attached to the "latest" release as PhysicsLab-AutoInstall.zip; logic tested on Linux pwsh only (native/tests/autoinstall_test.ps1), Windows behaviour untested.
- 0.16 stage 4: the in-app Benchmark has 11 scenes (about 2 min); the 7 old-CPU twins (12 to 18) are behind the box "Compare old CPU paths"; stress and speed-risk boxes gone; texts in app/bench_scenes.cpp, tested by app_settings (scene numbers).
- 0.16 stage 3: nucleus view at full zoom makes nothing wholly off the picture (Accurate + Sea at x10,000,000: 311,076 -> 36 vertices, 843 -> 24 blobs, pixels identical; ui6c_fullzoom); benchmark scene 35 "Nucleus, full zoom, camera moving" and "Frames slower than 16.7 ms" per scene; outline compute warmed up at start. Speed NOT measured.
- 0.16 stage 2: About scrollbar (z-order after reopen), Benchmark window height (body scrolls, Start fixed), Contents rail 20 em wide, Atom Lab Nucleus section always shown (ui6b_scrollbar/bench/rail/nucleus). Next: the dead-code hunt.
- 0.16 stage 1b: the old interface is removed (side panels, header, Atom Lab Old panel button, Settings -> New UI, old fps counter, large table window); the New UI is always on; version 0.16. Tests converted / deleted: see native/REPORT.md. Next: the dead-code hunt.
- 0.16 stage 1a: the Atom Lab's four old-panel gaps (Dirac switch, lab spectrum card on the data sheet, Restart atom, draw-call line) have a New UI home (ui6a_atom_gaps); nothing removed yet; stage 1 (removal) is next.
- 0.15 stage 5t: Controls segments and check boxes are grey (ui5t_grey); generator drift documented in REPORT 5t; Kr-78 NOT changed (NUBASE2020 unreachable from the cloud).
- 0.15 stage 5s: index.html moved to native/archive/web/index.html (sha256 unchanged); only 6 script paths changed, no app change.
- (History) Version 0.15 "Full UI rework" ended with 0.16 stage 1b. Stages 1 to 5r are done (5r: pills are flat, no gradient block, no outline except Monochrome / High contrast; ui5r_pill; 5q: lit buttons in every theme, theme list below the top bar, constant About scrollbar, exact End, hovering Element picker, grey sliders, `ui5l_popups` inset 4; the full suite result is in native/REPORT.md) (cloud-tested only; Windows runs and what was not tested are in native/REPORT.md and OWNER-TODO.md). The look is M1 (New UI On + Dark theme). Stage 5p fixed the owner's six PC findings: the wheel died after About (a page-open flag never cleared), About scrollbar, Atom zoom-out twice as far, lit buttons (dark fill, white icon), and the Element picker (a layout loop flipped the picture, toolbar and FPS pill between two places every frame; the picker no longer overlaps the fact line, toolbar or FPS pill, and the info text is hidden while it is open). The full suite result of 5p is in native/REPORT.md. Next, as in ROADMAP.md: what the owner reports from the PC, then stage 6 (reading views, Achievements), stage 7 (all themes, full search, keyboard, final speed check), then the 0.16 list. The release title says 0.15; the app's own label still says 0.14 alpha.
- Workflow (0.15 stage 4.1, details in CLAUDE.md): `native/tools/check.sh quick` while working; ONE test with `ctest --test-dir /home/user/build -R '^name$'`; `check.sh full` once at the end.
- Test programs and their check counts, all measured in the 0.15 docs cleanup session on the tree of stage 5n (unit-test build, each program run once; `check.sh quick` 9 tests, 8 passed and glsl_shaders skipped because glslang-tools was not installed):
  - orbit_tests: 1,631
  - atom_tests: 21,183
  - app_tests: 202,232
  - nbody_tests: 37
  - jpl_tests: 55
  - moon_tests: 24
  - moon_real_tests: 59
  - Not re-measured there, so no count is given: glsl_shaders (needs glslang-tools), gl_phase_colours (needs -DPL_GL_TESTS=ON), font_tests and the picture tests (need the app build).

## How work flows
- The owner pastes a written task into a new Claude Code session (branch box `native`).
- The session works on its own side branch and pushes to `native` with `git push origin HEAD:refs/heads/native` (check with `git ls-remote origin refs/heads/native`). It is a plain push, never a PR.
- Each push is checked by a planning chat in a lean way: fresh clone, build, only the changed screens and REPORT.md (not every test program each time). After that come the GitHub build and the release tagged `latest`. The release is a Pre-release while the stage is alpha or beta.
- GitHub Actions builds the Windows zip only on pushes that change native/** or the workflow.

## Rules that matter most
- native/archive/web/index.html is frozen (sha256 starts e5501387ae64); moved there from the repo root in 0.15 stage 5s.
- The accuracy and honesty rules in CLAUDE.md.
- No helper agents unless the job says so.
- One task per session.

## Keeping this file up to date
- At the end of each version, replace (do not append to) the lines in "Current state" with the new version and test counts.
