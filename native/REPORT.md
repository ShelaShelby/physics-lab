# Native Physics Lab 0.9.7 (alpha) — report

Housekeeping. No physics changes; the only code change is Part D. Older reports are in git history. `index.html` was not touched.

## What changed
- **A. `ROADMAP.md`** (repo root) with the list you gave, in your order, each item marked planned or done, no dates, nothing added. One line in `CLAUDE.md` points to it. Two notes: it lists 0.9.5, 0.9.6 and 0.9.7 as "planned" because that is how you worded the order ("Done: 0.9.3, 0.9.4"), although 0.9.5 and 0.9.6 are already built and pushed; and the "code signing" item says only "planned".
- **B. Release text** (`.github/workflows/native-windows.yml`, the "Publish to the latest release" step only). The title is now "Physics Lab native 0.9.7 alpha (latest build)". The version is read in the step from `native/CMakeLists.txt` (`project(... VERSION ...)`, the source of `PL_VERSION`) and the stage from `kStage` in `native/src/ui/changelog.cpp`: no second copy. The text names the version, commit and branch and has the Windows help. `--prerelease` is added only when the stage is `alpha` or `beta`; an empty stage gives the title without a stage and a normal release. The title, tag `latest` and link are otherwise as before.
- **C. Windows help.** New section "If Windows warns you" in `native/README.md` and the same text in the release. It says plainly the app is not code-signed yet, that this is a reputation warning and not a virus found, how to Unblock the zip (Properties, Unblock, OK), and More info, Run anyway. The only thing said about signing is that it is on the roadmap.
- **D. "Reset to today"** (`orbit/today.h`, `ui/orbit_panel.cpp`). It now sets only the date and time to now, and still clears selection, Follow, the moved view and the 3D depth. It no longer resets the **zoom** or the **Moons / Dwarfs / Comets / Belts** switches (the Time speed was already left alone since 0.9.3). The same function is used by the Journey "Today" jump and the Learn "Today" button. Defaults is unaffected: it first puts every setting back to `ViewSettings{}` (zoom -1.2, switches on, default Time speed), then calls it. The benchmark sets all of these itself. The app's start-up call changes nothing (fresh settings are already the defaults, and the saved ones are loaded after it).
- Version 0.9.7, 🕰 history entry, README.

## Tests (full suite run once before the commit: `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,187; all pass; no saved reference changed)
- New (7 checks): after a jump to another date with a changed zoom, the four switches off, a changed Time speed, 3D, a selection, Follow and a moved view, `backToToday` gives today's date (not "jumped", time 0), planets at the same positions as a fresh load at that date, zoom / switches / Time speed / 3D / far-away things unchanged, selection and Follow reset. The settings are passed as a const reference, so the function cannot change them; the test mostly guards against someone adding a reset later.
- Not covered by a test: the buttons themselves (the panel code is not linked into the test programs).

## Not tested
- **The workflow was NOT run.** I could only check that the YAML parses and that the two regular expressions find "0.9.6" and "alpha" in the files (before the bump to 0.9.7). PowerShell is not installed here, so the PowerShell code (here-string, argument list, Pre-release flag) has never been executed. The owner's push is the test: check that the release is titled "Physics Lab native 0.9.7 alpha (latest build)", shows **Pre-release**, and that its text has the version, commit, branch and the Windows help.
- Windows behaviour, SmartScreen and the Unblock steps are written from the common Windows procedure and were not tried here.
- Nothing about speed.
