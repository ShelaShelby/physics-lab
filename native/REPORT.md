# Native Physics Lab 0.15 — stage 5b (of 7): the Atom Lab's element chip and periodic table dropdown (New UI On)

`index.html` untouched. No helper agents. One commit, one push to `native`, no PR. New UI Off = the old app (hash guards unchanged). No physics value, formula or source changed, so the About page needs no new source. Time used: about 45 minutes (the full run is 24 of them).

## What changed (New UI On, Atom Lab only)
- **Chip**: "U · Uranium-238" over "92 protons · 146 neutrons"; "1 proton", "1 neutron", "0 neutrons" are singular / zero correctly. Play / pause stays inside it; Space works.
- **Dropdown** (click the chip; 38 em wide; opens under the chip, never over it; Esc, a click outside or a second click on the chip closes it; one dropdown at a time):
  - the **periodic table**: 18 columns, cells at least 28 u wide, lanthanides and actinides in two rows below, the chosen element ringed, **the same colours and dashed edges** as the old table (it is the same drawing code: the loop moved into `periodicTableCells`, used by the old panel, the large window and the dropdown; old pictures unchanged);
  - **Protons** ◀ slider ▶; **Neutrons** ◀ slider ▶ and **usual (N = n)**; the **famous isotopes** list (scrolls inside the panel). Two caption lines: how to use the table, and the isotope line (half-life) of the chosen nuclide.
  - Every element is reachable (tested). **Bigger table**: the dropdown's table *is* the big table (INVENTORY); the Old panel's "⤢ Bigger table" button and window still work (tested).
- **Build another** and **Reshuffle nucleons** moved from the Old panel into the toolbar's **Nucleus ▾** panel; shown only while zoomed to the nucleus / a nucleon in the Orbit view and only when the old panel offered them (Reshuffle for more than 3 nucleons; Build another after a decay held a nucleus). Gone from the Old panel with New UI On (still there with it Off). Their emoji are dropped (the font has none).
- The hint line and More / Less info stay at the top of the Old panel (5d). Not in this stage: view options, marks, photon panels, card, legend, data sheet.
- Code: `atom/atom_tools.*` (chip words, pick / step / usual / famous, Build another / Reshuffle rules), `ui/shell_atom.cpp` (the dropdown), `ui/atom_panel.*` (`periodicTableCells`, `PtGeom`), `ui/shell.cpp` / `shell_orbit.cpp` (the chip belongs to both labs), two caret icons, an Atom `state` line with Z, N, usual N, seed, big table.

## Tests (written first; none loosened)
- `atom_tests` (unit), `tests/test_atom_chip.inc`, 35 new checks, from independent references: the spec's words and singulars; the usual isotope's textbook mass numbers (H-1, He-4, C-12, O-16, Fe-56, Pb-208, U-238); all 118 elements picked = the old `setParam("Z")` result; the steppers, clamps at Z 1 and 118 and at the neutron slider's ends; "usual" = the guide window's old `usual` rule; every famous entry sets its own Z and N; Build another / Reshuffle rules. 21,183 atom checks, 0 failed.
- `atom_chip` (new picture test, 51 checks, 119 s): the real app. Dropdown below the chip and inside the window; **all 118 cells drawn, no overlaps, each at least 28 u (28 u = 28 px here)**; 14 elements in their standard places (H, He, B, C, Na, Fe, Kr, La, Ce, Lu, Ac, Pa, Lr, Og; the lanthanide row under group 3-17); **all 118 elements clicked in the real app**: Z is the clicked one and N the usual one for every element; Protons / Neutrons arrows and sliders, usual, famous (Helium-3 from its own label); Esc, outside click and a second chip click close it; the words for 1 proton / 1 neutron / 2 protons; Reshuffle works in the Nucleus panel, is absent in the atom view and in the Old panel, and is still in the Old panel with New UI Off; Bigger table opens. `atom_toolbar` (5a) now reads the longer state line.
- **Break it on purpose:** "usual" set to N + 1: `atom_tests` failed 2 checks ("usual gives carbon-12 again", "usual from the far end"); reverted, 0 failed.
- Full suite (`check.sh full`, run once, 28 tests with the new `atom_chip`): **28 of 28 passed**, 1,305 s.

## Cost (measured, software OpenGL, 1 llvmpipe thread, 200 frames, 1280x800; not a real GPU)
- Closed: nothing added (no per-frame work). Open: the table is drawn every frame (it has hover and a ring, so a texture would need rebuilding on hover); wall time 8.6 s closed → 18.1 s open for 200 frames, i.e. **about +47 ms per frame in software rendering while the dropdown is open** (table about 21 ms, famous list 5 ms, isotope line 3.5 ms, the rest the big glass panel). For comparison the Old panel (which has the same table and more) costs +37 ms per frame the same way, and the Orbit menu panel +3 ms. gdb samples showed the time inside the OpenGL software rasteriser, not in our code; the dashed edges (about 35 elements) are many small lines. On a real GPU this is expected to be small, **but that is not measured**. If it matters on the owner's PC: cache the table in a texture (rebuilt on hover / selection change).

## Looked at (software OpenGL, 1280x800, once each)
The open dropdown in Dark and in Aero, and the nucleus view with the Nucleus ▾ panel open. Fixed what I saw: the two buttons' emoji did not draw (dropped). The long famous list is cut by the window's bottom and scrolls (as intended); a one-column list is long, see OWNER-TODO.

## Not tested / limits
- Windows, Segoe UI widths, Windows scale 125 / 150 %, a real GPU, speed: not tested. At scale or text sizes where the panel does not fit the window the cells would shrink below 28 u (the app's minimum window 880 u keeps it at 28+ at Normal text size; larger text sizes make the panel and cells larger, so a small window may clip it; not looked at).
- The Neutrons slider spans the isotopes with data ±3 (the old slider's range, unchanged); "usual" is the most abundant (or longest-lived) isotope from the app's own list.
- Environment: a fresh container needs the apt line in CLAUDE.md before CMake works; this session's container already had a build.
