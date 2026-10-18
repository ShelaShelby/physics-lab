# 0.19: Defense Lab removed, clean-up, full audit (Windows, a GPU and a real mouse NOT tested)

## What changed
- Defense Lab removed (code, tests, CMake, lab picker, main.cpp, About block, scripts). Labs: Orbit 0, Atom 1, Molecule 2 (`app::labName` / `app::labIndex`, `app::kLabCount`). A 0.18 settings file loads: a removed or unknown lab opens the Orbit Lab, unknown lines are ignored, all other settings are kept.
- Dead code removed: 32 unused functions, 12 unused web texts (also in tools/make_app_text.js), 5 unused constants, 2 dead stores; no unreferenced source file, no commented-out code, no stale TODO found.
- Audit fixes (details and tests in native/AUDIT.md): the Atom guide's "monochrome" word (Sharp Eyes) works again; number formatting with NaN / Infinity / huge values; far-away years in Jump to a date; UTF-8-safe cutting of search results and the fact line; Molecule Lab search and bottom bar; settings file re-save stability and an atomic, checked write.
- NH3: CCCBDB reachable this time; its page itself lists 9.4443 next to a geometry giving 9.97 (page saved in data/molecules); still marked doubtful, the panel note now says so. No second source reachable.
- Docs: CLAUDE.md replaced (owner text), DECISIONS (new entry, superseded marks), ROADMAP, RESUME, OWNER-TODO, native/README.md, plan headers, data/molecules/README.txt; changelog 0.19; version 0.19.

## Tests
- New (each failed before the fix, passes after): app_settings testSettingsFrom018, testPrefsFuzz, testSettingsWrite, testDropLastChar; orbit_physics testFormatFuzz and two parseDateTime checks; molecule_logic testSettingsFuzz (a fuzz probe; it found no bug, only that re-reading a view moves its last bits); picture tests ui19_monokey, ui19_molsearch, ui19_molbar (tests/check_audit19.py).
- Removed with the lab (not loosened): defense_logic, defense_pictures.
- Changed on purpose: app_settings native edit count 38 -> 37 (Defense About block gone); shell_pictures shell_about_dark/light_1280x800 hashes (About page shorter). About and the lab picker looked at once each.
- Sanitizers (ASan + UBSan) on all unit tests: clean. check.sh full once: 79 / 80; ui5l_search failed (second time under the parallel load) -> its highlight clock now follows the frames under PL_FIXED_DT (test switch only); re-run alone with 3 CPU-burning processes: passed.
- Pictures looked at: 12 looks (7 grouped sheets of 26 screens, lab picker, About, Molecule bar x3). Found: U-238 info text over the nucleus, the Orbit sheet over the Legend chip (both listed, not changed).

## Draw counts
- Unchanged: no drawing code changed except the Molecule bar's button fill (same number of draws). molecule_pictures passes (one instanced draw call).

## Not tested / not done
- Windows, a real GPU (fps), a real mouse (drag / pan / wheel feel), display scales 125 / 150 %: in OWNER-TODO.
- Not fixed (AUDIT.md "Not fixed, why"): two layout overlaps, unlimited Molecule pan, the theme-style code of archived themes, misc-include-cleaner's include list.
