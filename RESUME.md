# Resume

## Current state
- Version 0.14 alpha, "Themes", is COMPLETE (3 stages: 1 + 1.1 richer theme styles, Frutiger Aero, solid windows; 2 Aero's real look, Light's accent fix, Y2K Ice and Y2K Night; 3 the Theme effects switch and the wrap-up). The owner ended 0.14 here: glass / chrome planets, glossy chart tiles, theme-aware Cloud dots and animated backgrounds are DROPPED. Next is 0.15 "Full UI rework" (ROADMAP: design system and mockups first; not started). How themes work: a theme's look is data (`src/app/theme_types.h`, `theme_style.*`: page gradient and textures, glass, gloss, rim, glow, title bar, brackets, fonts), drawn by `src/ui/style_draw.*`; Settings → Theme effects Off strips the decoration (`app::withoutEffects`, applied in `ui/look.cpp`); plain themes (the 10 web themes besides Light) draw the same pixels as in 0.13 (`tools/theme_pictures.sh`, `tests/check_theme_pictures.sh`, `tests/reference/theme_pictures_*.sha256`; the real app builds and runs headless on Linux with SDL's offscreen driver and llvmpipe, see REPORT.md). Test switches: `PL_FIXED_DT`, `PL_MOUSE`, `PL_OPEN_COMBO`, `PL_FX`, `PL_DECOR`, `PL_TITLEFONT`, `PL_EFFECTS`, `PL_LOG_WINDOWS`. Science colours and the lab pictures are NOT themed (and will not be in 0.14). 0.13 (GPU update) is complete before it.
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
