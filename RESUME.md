# Resume

## Current state
- Version 0.14 alpha, "Themes", stage 1 of 5 is done (richer theme styles + the Frutiger Aero theme). Stages 2-5 (Y2K futurism, glass / chrome planets and nucleons, glossy chart and Cloud dots, animated backgrounds and a full contrast check) are in ROADMAP. What stage 1 is: a theme can have a gradient page, glass panels, gloss, rounding, a shadow and a system font, all as data (`src/app/theme_types.h`, `theme_style.*`, drawn by `src/ui/style_draw.*`); the 11 web themes draw the same pixels as in 0.13 (`tools/theme_pictures.sh`, `tests/check_theme_pictures.sh`, reference hashes in `tests/reference/theme_pictures_*.sha256`; the real app builds and runs headless on Linux with SDL's offscreen driver and llvmpipe, see REPORT.md); `PL_FIXED_DT` (main.cpp) makes two runs draw the same pixels. Science colours and the lab pictures are NOT themed yet. 0.13 (GPU update) is complete before it.
- Test programs and their last known check counts:
  - orbit_tests: 1,612 (72 added in 0.13 stage 4)
  - atom_tests: 21,066 (84 added in 0.13 stage 4)
  - app_tests: 2,014 (398 added in 0.14 stage 1: the WCAG formula, theme data, gloss, Aero contrast, font fallback)
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
