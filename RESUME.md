# Resume

## Current state
- Version 0.15 "Full UI rework" is IN PROGRESS: stage 1 (audit, design system, mockups) is finished and changed NO app code (docs only, `native/docs/ui-0.15/`: AUDIT.md, DESIGN.md, design_tokens.json, mockups/ A-B-C, tools/ = throwaway probe patches and scripts). Findings: the Atom Lab panel's text is 12 px and its column scrolls 5,000+ px; text does not follow the window; the scale is read once from the primary monitor; About/History show raw HTML entities (&rsquo; ...). Next stages wait for the owner's pick (OWNER-TODO). 0.14 "Themes" is complete before it. How themes work: a theme's look is data (`src/app/theme_types.h`, `theme_style.*`), drawn by `src/ui/style_draw.*`; Settings → Theme effects Off strips the decoration; plain themes draw the same pixels as in 0.13 (`tools/theme_pictures.sh`, `tests/check_theme_pictures.sh`, `tests/reference/`); the real app builds and runs headless on Linux (SDL offscreen, llvmpipe; needs apt packages libegl-dev glslang-tools libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxkbcommon-dev libgbm-dev). Test switches: `PL_FIXED_DT`, `PL_MOUSE`, `PL_OPEN_COMBO`, `PL_FX`, `PL_DECOR`, `PL_TITLEFONT`, `PL_EFFECTS`, `PL_LOG_WINDOWS`, `PL_SAVE_IMAGE` + `PL_SHOT_DIR` (a clean picture of the lab area).
- Test programs and their last known check counts:
  - orbit_tests: 1,612 (72 added in 0.13 stage 4)
  - atom_tests: 21,066 (84 added in 0.13 stage 4)
  - app_tests: 3,036 (1,844 added in 0.14: the WCAG formula, theme data, gloss, Aero and Y2K contrast over the page textures, title bars, the Light accent, fonts and licence, floating windows hide what is behind them, the Theme effects setting and what Off removes). Also `tests/check_windows_decorated.py` (ctest windows_decorated) and, with -DPL_PICTURE_TESTS=ON, `theme_pictures`, `window_pictures`, `y2k_pictures` and `effects_pictures`
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
