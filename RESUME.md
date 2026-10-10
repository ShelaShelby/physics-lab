# Resume

## Current state
- Version 0.15 "Full UI rework" is IN PROGRESS. Stages 1 to 5r are done (5r: pills are flat, no gradient block, no outline except Monochrome / High contrast; ui5r_pill; 5q: lit buttons in every theme, theme list below the top bar, constant About scrollbar, exact End, hovering Element picker, grey sliders, `ui5l_popups` inset 4; the full suite result is in native/REPORT.md) (cloud-tested only; Windows runs and what was not tested are in native/REPORT.md and OWNER-TODO.md). The look is M1 (New UI On + Dark theme). Stage 5p fixed the owner's six PC findings: the wheel died after About (a page-open flag never cleared), About scrollbar, Atom zoom-out twice as far, lit buttons (dark fill, white icon), and the Element picker (a layout loop flipped the picture, toolbar and FPS pill between two places every frame; the picker no longer overlaps the fact line, toolbar or FPS pill, and the info text is hidden while it is open). The full suite result of 5p is in native/REPORT.md. Next, as in ROADMAP.md: what the owner reports from the PC, then stage 6 (reading views, Achievements), stage 7 (all themes, full search, keyboard, final speed check), then the 0.16 list. The release title says 0.15; the app's own label still says 0.14 alpha.
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
- The owner pastes a written task into a new Claude Code session (Sonnet 5.5, branch box `native`).
- The session works on its own side branch and pushes to `native` with `git push origin HEAD:refs/heads/native` (check with `git ls-remote origin refs/heads/native`). It is a plain push, never a PR.
- Each push is checked by a planning chat in a lean way: fresh clone, build, only the changed screens and REPORT.md (not every test program each time). After that come the GitHub build and the release tagged `latest`. The release is a Pre-release while the stage is alpha or beta.
- GitHub Actions builds the Windows zip only on pushes that change native/** or the workflow.

## Rules that matter most
- index.html is frozen (sha256 starts e5501387ae64).
- The accuracy and honesty rules in CLAUDE.md.
- No helper agents unless the job says so.
- One task per session.

## Keeping this file up to date
- At the end of each version, replace (do not append to) the lines in "Current state" with the new version and test counts.
