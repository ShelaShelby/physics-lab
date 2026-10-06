# Resume

## Current state
- Version 0.15 "Full UI rework" is IN PROGRESS: stages 1 (audit, rejected layouts), 2 (control inventory, final spec, four small fixes) and 3 (the new shell) are done; stage 4 (Orbit Lab: the rest of its controls into the chip, toolbar and subject card, and the side column goes) is next, see ROADMAP "BUILD PLAN". The owner's chosen design (5 Oct 2026): a full-screen 3D-viewer look, native/docs/ui-0.15/DESIGN.md; every control's new home: INVENTORY.md. Stage 3 code (all behind Settings → New UI (preview), Off by default; `PL_NEWUI=on|off` overrides it for checks): `src/app/shell.*` (pure logic, unit-tested in `tests/test_shell.inc`: type scale, layout, centre shift, glass opacity, keys, reading column, search), `src/ui/shell.cpp` (the frame: top bar, toolbar, readout, cards, dropdowns, search, Old panel), `shell_widgets.*` (the hand-drawn controls), `shell_orbit.cpp` (what the Orbit Lab puts in the chip, toolbar and panels), `orbit_actions.*` (the actions the old panel and the new UI SHARE, so the state is the same), `reading.*` (the reading view: `pageBegin` / `pageEnd` replace the old floating windows), `shell_test.*` (test support: `PL_SCRIPT` presses things through real SDL events, `PL_LOG_SHELL` prints every text size and rectangle, `PL_SCALE` stands in for the Windows scale, `PL_SHIFT=off`). Tests: `tests/check_shell.py` (ctest shell_ui), `check_shell_state.py` (shell_state), `check_shell_pictures.py` (shell_pictures; guards in `tests/reference/shell_pictures_0.15s3.sha256`). Not tested on Windows: everything about the real PC, Segoe UI, scale changes, speed. Stage 2 code: `src/app/html_entities.*` (About's entities), `src/app/page_cache.*` + `style_draw.cpp pageCacheDraw` (the page effects drawn once into a texture; test switch `PL_PAGE_CACHE=off`; log line `PLPAGECACHE rebuild` with `PL_LOG_WINDOWS`), `src/app/power.*` + `ui/power.cpp` (Windows power throttling asked off), `BenchRun::noteWindow` (the benchmark's focus flag), `src/app/display_scale.*` + `main.cpp rereadScale/applyScale` (display scale per monitor, smallest window 880 x 560 units). 0.14 "Themes" is complete before it. How themes work: a theme's look is data (`src/app/theme_types.h`, `theme_style.*`), drawn by `src/ui/style_draw.*`; Settings → Theme effects Off strips the decoration; plain themes draw the same pixels as in 0.13 (`tools/theme_pictures.sh`, `tests/check_theme_pictures.sh`, `tests/reference/`); the real app builds and runs headless on Linux (SDL offscreen, llvmpipe; needs apt packages libegl-dev glslang-tools libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxkbcommon-dev libgbm-dev). Test switches: `PL_FIXED_DT`, `PL_MOUSE`, `PL_OPEN_COMBO`, `PL_FX`, `PL_DECOR`, `PL_TITLEFONT`, `PL_EFFECTS`, `PL_LOG_WINDOWS`, `PL_PAGE_CACHE`, `PL_NEWUI`, `PL_SCALE`, `PL_SCRIPT`, `PL_LOG_SHELL`, `PL_SHIFT`, `PL_SAVE_IMAGE` + `PL_SHOT_DIR` (a clean picture of the lab area).
- Test programs and their last known check counts:
  - orbit_tests: 1,612 (72 added in 0.13 stage 4)
  - atom_tests: 21,066 (84 added in 0.13 stage 4)
  - app_tests: 167,899 (164,443 added in 0.15 stage 3: the new shell; 3,456 before it, with 420 added in 0.15 stage 2: entities, power request, benchmark focus flag, display scale, page cache logic); 3,036 before it (1,844 added in 0.14: the WCAG formula, theme data, gloss, Aero and Y2K contrast over the page textures, title bars, the Light accent, fonts and licence, floating windows hide what is behind them, the Theme effects setting and what Off removes). Also `tests/check_windows_decorated.py` (ctest windows_decorated) and, with -DPL_PICTURE_TESTS=ON, `theme_pictures`, `window_pictures`, `y2k_pictures`, `effects_pictures` and (0.15 stage 2) `page_cache_pictures`
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
