# Native Physics Lab 0.16 stage 1: AUDIT ONLY - STOPPED, gaps found. Nothing removed, nothing pushed.

Rule (CLAUDE.md / job): any old-UI control with no New UI home => change nothing, list the gaps.
Flag: newUiOn() in ui/shell.cpp:464 (PL_NEWUI env or prefs.newUi). Old code behind it: orbitPanel/orbitPanelContents
(ui/orbit_panel.cpp), atomPanel/atomPanelContents (ui/atom_panel.cpp), header/atomBottomBar, FPS counter (main.cpp ~486-507).
The New UI itself still shows the Atom Lab's "Old panel" (top.old button, menu:old, shell.cpp:545/980/1083-1090).

## Orbit Lab: no gap found
Every control of orbitPanelContents has a New UI home (shell_orbit.cpp / shell.cpp / settings.cpp): play, time speed, presets,
Journey and dates, Learn, Real/Simple + 1PN + wobble, 2D/3D, drag/spin, zoom, sizes, trails, follow, slow moons/spin, reset view,
sky, constellations, far things, extras, go-to tiles, Save image, Defaults, lab switch, Performance (Settings).

## Atom Lab: GAPS (found by grep of shell*.cpp / settings.cpp; not run in the app)
1. Dirac levels switch ("Light spectrum" section, atom_panel.cpp:1027, param "dirac" Off/On). New UI: no control anywhere (settings.cpp
   only saves / loads the value). Without it the Dirac level split cannot be turned on.
2. The lab's own simulated light spectrum (atom_panel.cpp:1030-1037): spectrumPicture(), spectrumNote(), diracExplain/diracOnNote and
   the Dirac level rows (diracLevelRows). The data sheet has only the NIST card (realSpectrumCard); this picture and text are not in it.
3. "Reset" button (atom_panel.cpp:870, L.restart(): a fresh atom, same settings). New UI: only Defaults (which also resets the
   settings); no restart-only action.
4. Performance line "Picture: N draw calls, M instanced shapes" (atom_panel.cpp:1055). Not found in the New UI (performanceCard is in Settings).
Not checked in detail (please confirm): Decay history list (atom_panel.cpp:916; the subject card may cover it), Atom Learn buttons
and the "What am I even looking at?" help button (atom_learn.cpp:184/189; menu "Learn" exists), Where the electron is found (radial: in the data sheet, OK).

## Proposed next step (needs the owner)
Give gaps 1-4 a New UI home (Dirac switch + lab spectrum card in the data sheet or View panel, a Restart action, draw-call line in
the readout / Settings Performance), or accept their loss. Then run this job again.

## Not tested
Nothing was built or run. No tests, no screenshots, no Windows run. apt packages installed only.
