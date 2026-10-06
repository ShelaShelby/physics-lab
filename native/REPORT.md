# Native Physics Lab 0.15 — stage 5a (of 7): the Atom Lab's toolbar and view buttons (New UI On)

`index.html` untouched. No helper agents. One commit, one push to `native`, no PR. New UI Off = the old app (hash guards unchanged). No physics value, formula or source changed, so the About page needs no new source.

## What changed (New UI On, Atom Lab only)
- **Toolbar**, bottom centre, same pill, 32 px buttons, 4 u gaps and carets as the Orbit one: **Atom · Nucleus ▾ · Cloud ▾ · Chart | Reset view**. Six new icons (atom, nucleus, cloud, chart, reset, move) drawn like the others.
  - Atom = View Orbit + zoom out (old "View: Orbit" and the bar's ⌕ Atom). Nucleus = the bar's ⌕ Nucleus; its ▾ holds Nucleus / Nucleon (the bar's ⌕ Nucleon). Cloud = View Cloud; its ▾ holds Cloud / Cloud 3D / Slice. Chart = View Chart of nuclides. Reset view = the bar's ↺ Reset view; on the chart it is "Reset tilt, zoom and position".
  - **Centre** and the **Drag rotate / move** button (also in the old bar) are kept: they appear only when the old bar showed them, so the toolbar changes width with the view. Nothing dropped.
  - Nucleus / Nucleon from the chart also go to the Orbit view (the chart has no zoom; nothing would be seen). In the cloud views they only zoom, as the old bar did.
- **Play / pause** stays in the element chip (stage 4 rule; Space works). This **replaces** DESIGN.md's "Play in the toolbar" idea for the Atom Lab (DECISIONS.md).
- The **bar under the picture** and the picture's **"Less info"** button are gone in the New UI; the picture fills the window (header 0, no bar). Its own text lines and the chart keep clear of the top bar and the toolbar by an inset (new `AtomLab::insetTop/insetBottom`, `setChartInsets`; both 0 with New UI Off).
- Because the hint line and More / Less info have no home until 5b, **both are at the top of the Old panel** (nothing unreachable). Build another and Reshuffle nucleons are also still only in the Old panel.
- Code: `atom/atom_tools.*` (what each button does, no drawing), `ui/shell_atom.cpp` (toolbar items, two panels), small edits in `ui/shell.cpp` (split button kind, panel ids), `main.cpp`, `atom_view.cpp`, `chart.*`, `shell_test.cpp` (an Atom `state` line).

## Tests (written first; none loosened)
- `atom_tests` (unit, in `check.sh quick`), new file `tests/test_atom_tools.inc`: for the five view words of INVENTORY ("Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides") from every start view the button gives that word and the same state as the old View row; Nucleus / Nucleon / Atom against the old bar's functions called directly (and a zoom-in / ordering sanity check); Reset view in Cloud 3D (start angles 0.6 / 0.35), Orbit, Cloud (untouched), chart (= chartReset); Centre, Drag toggle and when each is shown; which button is lit. 21,148 atom checks, 0 failed.
- `atom_toolbar` (new picture test, 44 checks, 130 s): the real app twice, old controls (New UI Off) against the toolbar (New UI On), same scripted clicks: view after each step, the levels Nucleus / Nucleon / Atom (carbon-12; hydrogen's nucleus is one nucleon, so its "Nucleus" level already reads as the nucleon level), Reset view, Drag toggle, the chart's Reset, chip play and Space, toolbar one row, inside the window, centred, not touching the top bar. Zoom numbers are not compared (the picture size differs).
- **Break it on purpose:** the Slice button set to Cloud 3D: `atom_tests` failed 8 checks (Slice from each of 4 views, view and state); reverted, 0 failed.
- Full suite (`check.sh full`, run once, with the new `atom_toolbar` in it): **27 of 27 passed**, 1,447 s.

## Looked at (software OpenGL, 1280x800, once each)
Atom, Nucleus, Cloud 3D, Chart in Dark; Cloud 3D in Aero. Found and fixed: the Atom picture's first two text lines and the chart's top-left notes were under the top bar (the inset above), and the chart's axis row sat under the toolbar. Left as it is: the chart's "neutrons N →" title is just below the toolbar's bottom edge (5b arranges the chart's legend and notes).

## Not tested / limits
- Windows, Segoe UI, Windows scale 125 / 150 %, a real GPU, speed: not tested (cloud session, software rendering). **No frame cost measured**; the stage adds no per-frame work except one tool list of 5-8 items and two number copies.
- Not in this stage (5b): the element chip's periodic table, view options, marks, photon panels, subject card, legend, the Atom fact line. The Atom Lab keeps its Old panel button.
- The toolbar's icons are small drawings without text; whether they are clear is for the owner (OWNER-TODO.md).
- Found on the way: the picture's `layer` is chosen from the drawn size, so a one-nucleon nucleus (hydrogen) shows "Quarks / Nucleon" at the Nucleus zoom; existing behaviour, unchanged.
- Environment: a fresh container needs X11 dev headers before CMake works (SDL stops on XCURSOR); the apt line is now in CLAUDE.md.
