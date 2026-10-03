# Native Physics Lab 0.9.3 (alpha) — report

Four bug fixes found by Alex on his PC. Older reports are in git history. `index.html` was not touched.

## What changed
1. **Benchmark, Atom Lab scenes** (`atom/atom_bench.cpp`). Cause confirmed: `benchSetup` never set `ostyle`, so a saved "Bohr 1913" drew the nucleus as one small ball (scenes 2, 4, 5). Now every Atom Lab scene starts from `AtomParams{}` (what Defaults gives), also clears the ⓘ More/Less choice, the decay history and the pause flags, and sets Orbit style "Modern" explicitly; the Temperature slider is set to 293.15 K for the run (given back after). The old lab state is still given back.
   - The result text has a new line "Atom Lab settings used: scene 2: view Orbit, Orbit style Modern, quark style ...".
   - **Any earlier result for scenes 2, 4 and 5 taken with Bohr 1913 on is not valid** (scene 3, Cloud 3D, was not affected by the style). Scenes 1–4 are otherwise unchanged, so results with Modern stay comparable.
2. **"conflicting ID"** (`ui/fonts.*`, `ui/atom_learn.cpp`). `wideButton(label, id)` has an optional ID; the two "❓ What am I even looking at?" buttons now have IDs `atomHelp` and `atomLearn1`..`4` (text and tooltip unchanged). I searched the `wideButton`/`Button`/`Selectable`/`Checkbox` calls of both labs: the other repeated labels ("##s", "##card", "##tab", "##l") are inside `PushID` scopes, and no other pair shares a screen. `io.ConfigDebugHighlightIdConflicts = false` in `main.cpp`, after the fix.
3. **Benchmark input lock** (`ui/benchmark.cpp`, `main.cpp`, `app/bench.h`). The running box is now an ImGui modal pop-up (clear dim, so the picture is unchanged): panels, header, tabs, settings get no input. In `main.cpp` the wheel, drag, click and key handling (Space, F11) of the labs asks `app::labInputAllowed(app.benchRunning)` first. Cancel button and Esc stop the run; moving/resizing still stops it; the window's X still closes the app. The pop-up is closed whichever way the run ends. The result has "Input during the test: blocked".
   - Optional: the window title now follows the lab on screen ("... — Atom Lab").
4. **Time speed on Today** (`ui/orbit_panel.cpp`, `ui/settings.cpp`). `resetToToday()` no longer sets the Time speed, so "☉ Reset to today", the Journey "Today" jump and the Learn "Today" button keep it. Defaults sets `kTspeedDefault` itself. Decision on the Journey: its own speed (`-3.145`, 1 s ≈ 1 hour) is part of the journey (the Earth is seen turning) and is given back with all other settings when it ends, so it is left alone; ordinary date jumps (`jumpToDate`) never touched the speed. The benchmark sets its own speed.
   - Other things `resetToToday()` still resets, for Alex to decide: zoom, the moons / dwarfs / comets / belts switches (all on), the selection, Follow, and the view offset.
- Version 0.9.3 in CMake, a 0.9.3 entry in the 🕰 history, README. `kStage` is still "alpha". No "word for word" test or saved reference changed.

## Checked (Linux, Xvfb, software OpenGL)
- `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,049 checks (43 new): all pass.
- New tests (`app_tests`): for each of the 4 Atom Lab scenes, a lab set to odd values everywhere (Bohr 1913, Slice, 3d, Classic, Fixed, ...) gives exactly the same settings after `benchSetup` as a default lab, with Orbit style Modern, the nucleus the scene names, and (Orbit scenes) all Z+N nucleons in the lab's list. The new check fails (13 checks) if the reset line is removed, so it does test the fix. Also: `labInputAllowed`, and the two new result lines.
- Screenshot (Atom Lab, benchmark scene 2 running, started with `ostyle=Bohr 1913` set): the nucleus is drawn with its nucleons, the running box is at the top left, the picture is not dimmed, the header says "v0.9.3 alpha · history".

## Not tested / not seen
- No Windows PC and no GPU: nothing about speed or Windows behaviour. The GPU timer check said "NOT reliable" (software OpenGL).
- The input lock was not tried with a mouse or keys (no way to click here): wheel, drag, clicks, Space, F11, panel buttons and Esc during a run are untested on screen. Only the allow/deny function is unit-tested. Alex should try them on his PC, also the Cancel button and Esc.
- The red "conflicting ID" box: not looked for on screen; it is off in the build anyway, and the real conflict was fixed by reading the code.
- No test for `resetToToday` (it lives in the ImGui part of the app, which the test programs do not link). Not seen on screen: "Reset to today" with a changed Time speed; the window title change.
