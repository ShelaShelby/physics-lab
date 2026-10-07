# Native Physics Lab 0.15 — stage 5j.1: fix the Windows run of 5j + full-colour look pictures

`index.html` untouched. No helper agents. New UI Off = the old app. No physics, formula or value changed.

## What was wrong (read from the log, job `build` of run 37635452775, commit 5ef6cf9)
`font_coverage` failed with exactly 3 failures: "at least DejaVu Sans is available as the fallback", and "IBM Plex Sans (M3): text character U+207A / U+207B is drawable". The Windows runner has none of the Linux fallback fonts (the log says "not on this machine" for all four). The step failed, so "Publish to the latest release" was skipped. The log also showed that Plex Sans itself lacks U+207A / U+207B (Inter has them); on Linux DejaVu covered them.

## Changed
- **One fallback list:** new `src/ui/font_chain.h` (`ui::fallbackFontChain()`: Windows Segoe UI, Segoe UI Symbol, Segoe UI Emoji; Linux DejaVu Sans, Noto Sans Symbols, FreeSerif). `ui/fonts.cpp` merges exactly this list behind every face, and `tests/test_fonts.cpp` checks exactly this list, so they cannot drift apart. (The 5j test had also counted Liberation Sans as a fallback; the loader does not merge it as one, so it is out now.) The "at least one fallback available" check now looks for this platform's chain.
- **Not loosened:** every text character (below U+2100) must still be drawable by the new font or the platform's real chain, on both systems; symbols / emoji (U+2100 up) are listed, not failed, as before.
- **Workflow:** one extra step `ctest -R font_coverage -V` in `native-windows.yml`, so the Windows lists are printed in the log (ctest hides output of a passing test).
- **Pictures:** `tools/render_look.py` now saves full-colour PNGs (no palette). All 15 `m*_*.png` and `look_compare.png` re-rendered. Looked once at `m2_1`: Sun yellow, Earth blue, Mars red, Jupiter orange, Neptune blue: the dots are coloured. Sizes: largest picture `m2_5.png` 300 KB (limit 600 KB), `look_compare.png` 515 KB (limit 3 MB).

## Results on Windows (GitHub runner, real Segoe UI files)
- Run 37637583953 (commit 41da3ae): `font_coverage` **Passed** (case a: no text character missing; Segoe UI has U+207A / U+207B), build green, **published** (release `latest`, target 41da3ae). So no extra Inter fallback behind Plex was needed.
- Run 37638990629 (commit b5473dd), printed: "132 different non-ASCII characters", "3 of the platform's fallback fonts found", Plex M3: **0** characters in neither, Inter M1/M2: **0** in neither. So on Windows even the 36 emoji that are missing on Linux are drawn (by Segoe UI Emoji), on the runner's Segoe files.
- Linux (cloud session, same test): text characters all covered; **36 emoji in neither** (U+1F306 1F30C 1F30D 1F319 1F321 1F381 1F393 1F3A8 1F3AF 1F3B2 1F3C6 1F3E0 1F440 1F464 1F4A8 1F4CF 1F4DA 1F4F7 1F50D 1F50E 1F518 1F522 1F52C 1F534 1F570 1F573 1F5BC 1F680 1F6E0 1F9CA 1F9E9 1F9EA 1F9ED 1F9F9 1FA90 1FA9C), same for Inter and Plex.
- The first Windows run's lists (before the fix) counted only the Linux chain, so its "80 / 72 missing" are not meaningful.

## Zip size (Actions artifact `PhysicsLab-windows-x64`, bytes)
- 3624084 (5j part 1, no look fonts): 3,545,665. 41da3ae (with the fonts): 4,216,286; b5473dd: 4,216,287.
- **Increase: 670,621 bytes (about 0.64 MiB).** This replaces the estimate "about 653 KB" (gzip of the four fonts). The release asset itself (made by Compress-Archive) is 4,235,495 bytes; I have no earlier release asset to compare with.

## Tested (Linux, software OpenGL, and the Windows runner)
- End run (once): `check.sh quick` 10 of 10 (font_coverage included); `ui5j_look_m1/m2/m3`, `theme_pictures`, `window_pictures`, `y2k_pictures`, `page_cache_pictures`, `effects_pictures`, `shell_pictures`: 9 of 9. With PL_LOOK unset every guard matched its saved reference; no reference rewritten.
- Windows runner: build, all 10 unit tests (glsl_shaders skipped there: no glslang), packaging, publish: green.

## NOT tested
Running the exe on a real PC (looks M1-M3 on Windows, the font rendering, scale 125 / 150 %), a real mouse, a real GPU, speed. The runner proves the fonts cover the characters, not how they look.

## Try it on the PC
Download `PhysicsLab-windows-x64.zip` from the release `latest`, unzip it. Start menu -> `cmd` -> Enter, type `set PL_LOOK=m1` Enter, then the full path of `PhysicsLab.exe` Enter. Settings -> New UI = On, theme Dark. `m2`, `m3` for the others. Close the window to go back.
