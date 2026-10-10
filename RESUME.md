# Resume

## Current state
- Version 0.15 "Full UI rework" is IN PROGRESS. Stages 1 to 5n are done (cloud-tested only; the Windows runs and what was not tested are in native/REPORT.md and OWNER-TODO.md). The look is M1 (New UI On + Dark theme); Frutiger Aero, Y2K Night, Synthwave and the Theme effects setting are archived in `native/archive/themes/`. Next, as in ROADMAP.md: the four failing tests that the first full suite since 5k found (listed under "NEXT JOB" in ROADMAP.md), then stage 6 (reading view for Learn, Settings, About, history, Benchmark; Achievements) and stage 7 (all themes through the glass tokens, full search, keyboard, final speed check). The docs cleanup of 0.15 (this commit) changed only text, CLAUDE.md, the docs folders and `native/imgui.ini` (no app code).
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
