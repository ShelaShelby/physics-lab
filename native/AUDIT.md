# Audit 0.19 (18 Oct 2026): Orbit Lab, Atom Lab, Molecule Lab, shell, UI, settings, renderer

Replaced by the next audit. Cloud only: Linux, Mesa llvmpipe (software OpenGL), no GPU, no Windows, no real mouse.

## How it was searched
- Sanitizers: all unit tests (10 programs) built with `-fsanitize=address,undefined -fno-sanitize-recover=undefined`: clean before and after the fixes (also with the new fuzz tests).
- Warnings: whole app with `-Wall -Wextra -Wunused -Wunused-macros -Wshadow=local`: 10 unused functions (deleted), 2 narrowing warnings in a test (fixed), 17 shadowed locals (read one by one: all deliberate inner scopes, no bug).
- Linker: `-ffunction-sections -Wl,--gc-sections,--print-gc-sections`, then every dropped project function grepped in src and tests: 22 more functions used nowhere (deleted); the rest are used by tests or inlined.
- Static analysis: clang-tidy 18 (bugprone-*, clang-analyzer-*, performance-*, misc-include-cleaner) and cppcheck 2.13 on all sources (installed with apt). Findings read by hand: 2 dead stores / unused locals (removed); "unchecked optional" (6) and a null-pointer path (2) are guarded by the callers (false positives); rounding casts and enum sizes are style. misc-include-cleaner lists about 1,500 include changes, mostly ImGui / std headers reached through other headers: NOT applied (too broad for the gain; a later job could do it file by file).
- Fuzzing in the unit tests: number formatting (200,000 random doubles, every exponent, NaN, Infinity), the settings file (20,000 random files), the Molecule Lab settings text (20,000 random texts).
- Reading by area: settings load / save, date entry, formatting, Molecule Lab picking / camera / panel, search, lab switching, window size and minimise, GL objects (no per-frame creation found; the one page texture is reused), benchmark statistics.
- Pictures: every screen of every lab in Dark 1280 x 800 and each lab's main screen in Light, 26 screens in 7 grouped looks, plus the changed lab picker, About and Molecule bar (12 looks in all).

## Docs audit (Phase 1; fixed in Phase 5)
- CLAUDE.md: two labs, time limit, "stop and ask" -> replaced by the owner's text (three labs, C++ only, stop rule).
- There is no root README.md; native/README.md is the README (said "0.16", "two buttons at the top of the panel", no Molecule Lab, 20 paragraphs of 0.15 stage history) -> rewritten intro, Labs dropdown, Molecule Lab section, folders, tests, PL_ switches.
- RESUME.md: Defense Lab lines, "lab index 3", "0.15 IN PROGRESS", a model name for coding sessions -> fixed; one 0.19 line added.
- ROADMAP.md: Done list stopped at 0.16 stage 1b; "lab 3"; 1.0 still promised the archived Aero / Y2K themes; 0.15 "NEXT JOB" stale; a duplicated 0.16 item; the outline warm-up already done; the pre-1.0 bug audit -> all updated.
- DECISIONS.md: "Orbit Lab and Atom Lab only", the 0.14 theme decisions (themes archived in 0.15 stage 5m), "New UI behind a setting" (gone in 0.16), the 5r working rules (time limit), "lab 3" -> marked "superseded in 0.19"; new owner entry of 18 Oct 2026.
- OWNER-TODO.md: both Defense Lab sections, the done ui5l_popups item, the done 0.14 -> 0.15 release label, three "New UI Off" requests (the setting is gone) -> removed; the NH3 item rewritten (CCCBDB now read); Molecule / Orbit / Atom PC tests added.
- APP_PLAN.md, ATOM_PLAN.md: historical, header lines now say what replaced them. data/molecules/README.txt: CCCBDB reachability updated. docs/ui-0.15/*.md: 0.15 history, already marked; kept.

## Found and fixed (each with a test that failed before and passes now)
1. The Atom guide's clickable word "monochrome" did nothing since 0.16 stage 1b (its action was set by the old header's fact line, removed with the old interface): the Monochrome theme and the Sharp Eyes achievement could not be found. Test `ui19_monokey`.
2. Number formatting (`orbit/format.cpp`), test `orbit_physics` (testFormatFuzz; it crashed before): `jsLocale`, `jsToPrecision`, `jsToExponential` of Infinity read past a string (crash); `fixed(NaN)` gave "nao"; `commas` of NaN, Infinity or above 9.2e18 was undefined behaviour; `plainNumber` cut numbers of 1e21 and up and very small ones at 63 characters; `kg(0)` took log10(0); `superscript(INT_MIN)` overflowed; `simDate` overflowed far in the past. Now as JavaScript writes them (NaN, Infinity, ∞, 1e+300, exact digits).
3. Jump to a date: a year such as 999999999 overflowed the milliseconds (undefined; the note then said "before 1800"), and long numbers were cut to int. Now read as long long, years beyond ±100,000,000 are "not a date the lab can read". Test `orbit_physics` (parseDateTime).
4. Search results and the fact line cut long text one byte or two characters at a time and could leave half a UTF-8 character ("°", "×"); now whole characters (`app::shell::dropLastChar`). Test `app_settings` (testDropLastChar; it crashed before).
5. Molecule Lab: search offered "Orbit Lab changelog", "Put this lab's settings back…" and "Legend", which do nothing there (the menu already hid them). Test `ui19_molsearch`.
6. Molecule Lab bottom bar: lit buttons used the accent fill (blue in Light), unlike every other toolbar (5q rule: quiet fill towards the text colour); Reset view looked lit. Test `ui19_molbar`.
7. Settings file: a stray CR inside a value made the file change on every second save. Test `app_settings` (testPrefsFuzz).
8. Settings file: written in place with no error check (a crash or full disk left a cut file and the app believed it was saved; a failed open was retried every frame). Now a side file, renamed over the old one only when complete; a failure keeps the old file and retries after 5 s. Test `app_settings` (testSettingsWrite).
10. Test robustness: `ui5l_search` (Settings row highlight) failed twice in the parallel full run, never alone: the highlight's 1.6 s ran on the wall clock while the test counts frames. With PL_FIXED_DT (test switch only) the highlight now follows the frames, like the labs' clocks; it then passed with three CPU-burning processes beside it. Normal use is unchanged.
9. A 0.18 settings file left on the Defense Lab must open the Orbit Lab with every other setting kept. Test `app_settings` (testSettingsFrom018).

## Removed (Phase 2 and 3)
- Defense Lab: src/defense (6 files), ui/shell_defense.cpp, tests defense_logic + defense_pictures (test_defense.cpp, check_defense.py), CMake targets, lab picker row, main.cpp input / timing / save code, About block, shell ternaries. No saved pictures or hashes existed for it.
- 32 functions used nowhere (old panel cards, old header buttons, language menu, Learn buttons, journey card, `nbodyAccel`, `categoryIndex`, `dragText`, ...), 12 unused web texts in app_text.inc (and their lines in tools/make_app_text.js), 5 unused constants (`kSunMassKg` 1.989e30 among them), 2 dead stores. Picture hashes unchanged except the two About pictures (Defense block gone).

## Not fixed, why
- **FIXED in 0.20:** Atom Lab, Nucleus view of a heavy atom (U-238): the long info text at the top left was drawn over the nucleus. It now folds above the nucleus (a click shows the rest; test molbuild_u238, also Th-232 and Pb-208).
- **FIXED in 0.20:** Orbit Lab: the "All values" sheet covered the Legend chip at 1280 x 800. It now starts below the chip (test molbuild_sheet, 1280 x 800 and 1920 x 1080).
- **FIXED in 0.20:** Molecule Lab: panning had no limit. It is now limited to 1.5 molecule radii from the centre, for a drag and for the stored pan (test molbuild_molpan).
- NH3 rotational constants: CCCBDB was reachable this time; its page lists A = B = 9.44430, C = 6.19600 cm-1 (1966Herzberg) next to rNH 1.012 / aHNH 106.67, which give A = B = 9.974, C = 6.346; CCCBDB's own product of moments (8.668394 amu^3 A^6) follows the listed constants, its Cartesians give 7.588. The page is inconsistent with itself. NIST WebBook, PubChem, the JPL and CDMS catalogues and Wikipedia were NOT reachable, so no second source: the value is unchanged and still marked doubtful (page saved: data/molecules/cccbdb_nh3_exp2x_2026-10-18.html).
- Earth's card says "radius 6,378 km": that is the equatorial radius (NASA fact sheet: equatorial 6,378.1 km, volumetric mean 6,371.0 km). Not wrong, but the word "radius" alone is ambiguous; the card text is the web's, word for word. Doubtful wording only.
- The About intro (web text, word for word) says "real orbits and atoms" and does not mention molecules.
- Theme-style code for the archived themes (page textures, glass, gloss in src/ui/style_draw.cpp and src/app/theme_style.*) is still compiled and tested but no current theme uses most of it ("dead by data"). Removing it is a larger refactor; left for the owner to decide.

## Owner decides (kept, not deleted)
| What | Size | Referenced by |
| --- | --- | --- |
| native/archive/web/index.html (frozen web reference) | 800 KB | tools/*.js, tests/reference/*.js |
| native/archive/themes/ (Frutiger Aero, Y2K, Synthwave code as text) | 136 KB | README there; nothing compiled |
| native/docs/ui-0.15/ (AUDIT, DESIGN, INVENTORY, mockups, tools) | 368 KB | DESIGN.md is linked from README.md and code comments; the rest is 0.15 history |
| tests/reference/*.inc, *.js, old_ui_runs.json, shell_pictures_0.15s4.sha256 | | the unit and picture tests |
| native/data/* (asd, ame, nubase, quadrupole, radii, jpl, molecules) | | tools/make_*_data.py, the generated .inc files, the tests |
| native/APP_PLAN.md, native/ATOM_PLAN.md | small | marked HISTORICAL |

## Complicated functions left alone (over 150 lines or nesting 5 or more)
AtomView::draw (735 lines, nesting 8), shellFrame (657, 7), main (477, 5), loadFonts (181, 5), Renderer2D::end (180), Renderer2D::init (157), molecule buildAll (151, a data table), drawSky (120, 5), svgIcon (119, 5), runScript (118, 6, test support), html flow (75, 5), historyWindow, decayChain, seasonsGuide, richText / parse, diracLines (nesting 5). None was refactored: picture hashes cover their output, but a split of AtomView::draw or shellFrame is a large change with no bug to fix; a later job can split one at a time.
