# Archived themes (0.15 stage 5m)

Three themes were taken out of the app on 2026-10-07 at the owner's request. Nothing in this folder is compiled, none of it is in CMake, and none of it is in the tests.
A saved settings file that names one of them opens as **Dark** (once, no message).

## What each one was

| id | Name | What it was |
| --- | --- | --- |
| `aero` | Frutiger Aero | A light "Windows Vista" style: sky-blue to leaf-green page with soft clouds and bubbles, frosted glass panels, glossy aqua and lime "jelly" buttons, blue glossy title bars, rounder corners. Used Segoe UI when the PC had it. |
| `y2knight` | Y2K Night | A dark "cyber / techno print" style: night-blue page with film grain, halftone dots, grid lines and a violet light streak, thin HUD rules and corner brackets, chrome buttons, window titles and the app name in the wide Michroma font. (The owner called it "midnight"; it is **not** the web theme `midnight`, which stays.) |
| `synthwave` | Synthwave | The web version's secret theme: neon purple and hot pink, a purple glow behind the top of the page, a pink glow on the app name. It was unlocked by the 🎁 surprise in the "Did you know?" facts (achievement "Neon Nights"). |

The **Theme effects** switch in Settings (page textures, glints, brackets, HUD marks on or off) only changed Aero and Y2K Night, so it was archived with them (`archived_theme_code.cpp.txt`, last section).

## What is here

- `archived_theme_code.cpp.txt`: the exact source text taken out, section by section, with the file it came from (theme definitions, the three `makeStyle` branches, Synthwave's glow drawing, the Settings "Theme effects" row, `withoutEffects`). Copied from commit `383d6ad`.
- `fonts/Michroma-Regular-latin.ttf`, `fonts/OFL-Michroma.txt`: the Y2K title font (SIL OFL 1.1).
- `tests/check_y2k_pictures.py`, `tests/check_effects_pictures.py`, `tests/check_page_cache.py`, `tests/theme_pictures_0.14s2_native.sha256`: the picture tests and saved hashes that rendered these themes.

## What stayed in the app (not archived)

- The generic style engine (`ThemeStyle`, `style_draw.cpp`, the page cache, `hasStage2Decor`): no shipped theme uses it now, but it is general, every window draws through it, and its maths still has unit tests (on a made-up test style, not on a theme). Stage 7 (all themes through tokens) replaces it.
- The web's data (`app_text.inc`, generated from the frozen `index.html`) still holds the Synthwave row, the Neon Nights achievement and the 🎁 texts, because the word-for-word tests compare it with the web. The app skips them (`themeArchived`, `achievementInApp`, the facts list).

## How to put one back (steps)

1. **Theme definition.** In `src/app/theme_style.cpp` paste the `kNativeThemes` block from the archive above `allThemes()` and add `for (const ThemeDef& t : kNativeThemes) if (!t.secret) v.push_back(&t);` to `allThemes()` (non-secret themes, before the secret ones). Synthwave needs no definition: it is in the web's `kThemes`.
2. **Style.** Paste the theme's `else if (id == "...") { ... }` branch into `makeStyle()` (same file). For Synthwave also add back `OptRgb glowTop, titleGlow;` to `ThemeStyle` (`src/app/theme_types.h`), `bool synthGlow` and `drawSynthGlow` to `src/ui/look.h` / `look.cpp`, and the glow calls (`if (look().synthGlow) drawSynthGlow(...)`) in `app_pages.cpp`, `app_tabs.cpp`, `orbit_panel.cpp`, `atom_panel.cpp`.
3. **Remove the id from `themeArchived()`** (`theme_style.cpp`) and from `achievementInApp` (Synthwave: `prefs.cpp`), and for Synthwave put back the `"synthwave"` entries in `loadPrefs` (`unlock=`), `unlockedThemeCount`, `resetProgress`, `unlockEverything`, the secret pop-up in `app_tabs.cpp` and the "@gift" fact (`isGiftEntry`).
4. **Aero / Y2K Night only:** copy `fonts/Michroma-Regular-latin.ttf` and `OFL-Michroma.txt` back to `assets/fonts/` (the font is picked up by the CMake glob) and re-add the Michroma loading block to `src/ui/fonts.cpp` (see git history of stage 5m). Put the "Theme effects" row and the `effects` preference back (the archived Settings section above, `Prefs::effects`, `Look::effects`, `withoutEffects`).
5. **Tests.** Move the tests from `tests/` here back to `native/tests/`, add their `add_test` lines back to `CMakeLists.txt`, restore the deleted theme tests from git history (`git show 383d6ad:native/tests/test_theme_stage2.inc` and `test_theme_style.inc`), and regenerate the picker / Settings hashes in `tests/reference/theme_pictures_*.sha256` and `shell_pictures_0.15s4.sha256` with the old code first, then the new.
6. **About / sources.** The About page's "Where the facts come from" entries for these themes are in git history (`git show 383d6ad:native/tests/test_app.cpp`, the `NativeEdit` rows for 0.14 stage 1 and 2); put them back in `app_text.inc` and `test_app.cpp` together.

Everything here is also in git history before the commit "native 0.15 stage 5m part 1".
