# 0.19 stage 1 (checkpoint): Defense Lab removed, dead code removed, first audit fixes (Windows, a GPU and a real mouse NOT tested)

Stage 2 of this job replaces this file with the full 0.19 report (audit results in native/AUDIT.md).

## What changed
- Defense Lab removed: src/defense, ui/shell_defense.cpp, tests defense_logic + defense_pictures, CMake targets, the lab picker row, main.cpp input / save code, About source block, shell ternaries. Labs: Orbit 0, Atom 1, Molecule 2 (app::labName / app::labIndex).
- A 0.18 settings file still loads: lab=Defense Lab (or any unknown lab) -> Orbit Lab; unknown lines ignored; every other setting kept (new test testSettingsFrom018, failed before).
- Dead code removed (compiler -Wunused-function, linker --gc-sections, grep): about 30 functions left over from the old interface, unused constants and web texts.
- Fixed: the Atom guide's "monochrome" word did nothing since 0.16 (Monochrome theme + Sharp Eyes unreachable); new picture test ui19_monokey.
- Fixed: number formatting with NaN / Infinity / huge numbers (crash, wrong text), a cut search result could end in a broken character, a far-away year typed in Jump to a date overflowed.
- Version 0.19.

## Tests changed on purpose
- app_settings: native edit count 38 -> 37 (the Defense About block removed); shell_pictures: shell_about_dark/light_1280x800 hashes updated (shorter About page). Lab picker and About looked at once each.
