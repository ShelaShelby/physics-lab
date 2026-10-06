# Resume

## Current state
- Version 0.15 "Full UI rework" is IN PROGRESS: stages 1 (audit, rejected layouts), 2 (control inventory, final spec, four small fixes), 3 (the new shell) and 4 (the whole Orbit Lab in the new UI) are done; stage 5 (the Atom Lab: element chip with the periodic table, toolbar, subject card and data sheet, legend, and its fact line) is next, see ROADMAP "BUILD PLAN". The owner's chosen design (5 Oct 2026): a full-screen 3D-viewer look, native/docs/ui-0.15/DESIGN.md (sections 9 and 10 say what stages 3 and 4 built); every control's new home: INVENTORY.md. All of it is behind Settings → New UI (preview), Off by default (`PL_NEWUI=on|off` overrides it for checks). Stage 4 code: `src/orbit/subject_card.*` (the card's big number, key values and data sheet, made from the old card's text), `src/orbit/marker_colors.h` (the picture's marker colours, shared with the Legend), `src/app/shell.*` (pure logic, unit-tested in `tests/test_shell.inc`: the fact line's timing, place and shadow colour, search with recent choices, panels stepping aside), `src/ui/shell.cpp` (frame, menu, card, data sheet, fact line, search), `shell_orbit.cpp` (toolbar, chip, Time panel, Show / Pick / Scale / View panels, legend, search entries), `orbit_actions.*` (the actions the old panel and the new UI SHARE, now also "Slow time to ..." and the GR / wobble tooltips). Test hooks added: `PL_SCRIPT` verbs move=, drag=, wheel=, key=down/up; `PL_FACT_INTERVAL`, `PL_FACTS`, `PL_LOG_CARD` (the old layout's card text) and the log lines PLSHEET, PLLINK, panelneed, searchhit, factline, factalpha. Tests: `tests/check_orbit_ui.py` (ctests orbit_ui_coverage, _card, _time, _facts, _facts_long, _readable, _search, _links), `check_shell.py` (shell_ui), `check_shell_state.py` (shell_state: now also Sky, Constellations, Dwarf planets, Far-away, wobble, Follow, Slow time, Reset view, Journey), `check_shell_pictures.py` (48 guards in `tests/reference/shell_pictures_0.15s4.sha256`). Not tested on Windows: everything about the real PC, Segoe UI, scale changes, speed. Stage 3 and 2 code: see the stage 3 and 2 lines of ROADMAP (`src/app/page_cache.*`, `power.*`, `html_entities.*`, `ui/reading.*` and the rest).
- Test programs and their last known check counts:
  - orbit_tests: 1,612 (72 added in 0.13 stage 4)
  - atom_tests: 21,066 (84 added in 0.13 stage 4)
  - app_tests: 177,190 (9,291 added in 0.15 stage 4: the subject card for every real card, search with recent choices, the fact line's clock, place and shadow colour, panels stepping aside, the Facts and recent prefs; 167,899 before it, of which 164,443 added in 0.15 stage 3: the new shell; 3,456 before it, with 420 added in 0.15 stage 2: entities, power request, benchmark focus flag, display scale, page cache logic); 3,036 before it (1,844 added in 0.14: the WCAG formula, theme data, gloss, Aero and Y2K contrast over the page textures, title bars, the Light accent, fonts and licence, floating windows hide what is behind them, the Theme effects setting and what Off removes). Also `tests/check_windows_decorated.py` (ctest windows_decorated) and, with -DPL_PICTURE_TESTS=ON, `theme_pictures`, `window_pictures`, `y2k_pictures`, `effects_pictures` and (0.15 stage 2) `page_cache_pictures`
  - glsl_shaders (glslang, needs `glslang-tools`; skipped if absent; 15 shaders, 4 compute) and gl_phase_colours (only with -DPL_GL_TESTS=ON: 503 checks on llvmpipe)
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
