# Native Physics Lab 0.9.8 (alpha) — report

An optional 90% probability outline for the electron cloud (Atom Lab: Cloud, Cloud 3D, Slice). Older reports are in git history. `index.html` was not touched.

## What changed
- **The outline** (`atom/outline.h/.cpp`, no graphics, so it is testable). The region is `{|psi|^2 >= c}` with `c` chosen so that it holds exactly 90% of the probability: the smallest-volume region with 90% (the textbook definition), not a sphere except for s orbitals. It uses the cloud's own `psi()` (no second copy of the wave functions), so it is the same one-electron, non-relativistic, hydrogen-like model. Orbitals with nodes give all their surfaces.
- **How:** `|psi|^2` on a 64 x 64 x 64 grid over `1.15 x kExt[n]` (voxel centres); the voxels are added densest first until they hold 0.9 of the probability (`psi` is not normalised, so the total comes from a separate radial-times-angular integral, `orbitalNorm`). Worked out **once per orbital** (with its orientation) at Z = 1, kept, and scaled by 1/Z (psi at Z is psi at Z = 1 with lengths divided by Z). Not done in the frame loop. Time here (software, one core): at most 39 ms for the biggest orbital, the Slice's cut-plane contour (200 x 200 points, again when the cut slider moves) 2 ms.
- **Drawing** (`atom_view.cpp`), thin lines (about 1 pt) in the theme's text colour (light on dark pages, dark on light ones), so readable on both: **Slice**: marching-squares contour of `|psi|^2 = c` in the cut plane. **Cloud** (side view): the outline of the region's shadow along y (contour of the maximum over y), so it says "its shadow seen from the side" and is not a cross-section. **Cloud 3D**: a faint wireframe, the contour lines of the surface in 5 planes across the region for each of x, y, z, projected like the dots (turns and zooms with the cloud), nearer lines a little stronger. No mesh or surface was needed, so the 3D part did not grow beyond the plan.
- **Setting "90% outline: Off / On"** in the Electron card (shown for the three cloud views), default Off, with a one-line note ("Shows where the electron is found 90% of the time (one-electron model)." ; the On note adds that atoms with more electrons differ). Saved in the settings file with the same validity check as the other choices; put back by Defaults. The readout (and More info) say what the line is and the limits (one electron, non-relativistic, atoms with more electrons differ) when it is on.
- **Benchmark:** `fresh()` sets the outline Off in every scene (tested).
- Version 0.9.8, 🕰 entry, README; `ROADMAP.md`: 0.9.5 to 0.9.8 marked done (and moved to Done).

## Tests (full suite once before the commit: `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,304; all pass; no saved reference changed)
- **1s against the closed form**: root of 1 - e^(-2r)(1 + 2r + 2r^2) = 0.9 gives 2.6612 a0; the outline's level gives 2.6708 a0 and the slice contour averages 2.6710 a0 (+0.4%, the 64-point grid's resolution; tolerance 0.03 a0). The grid says it holds 0.9025.
- **All 30 orbitals and orientations (1s ... 4f)**: the probability inside `|psi|^2 >= c`, from a separate fine integration (spherical midpoint in r and phi, Gauss-Legendre in cos(theta), 0.1 a0 radial steps), is 0.90 within 0.0053 at worst (limit 0.01); the app's normalising integral agrees with the fine one within 0.5%. (A first version of the test used a midpoint rule in cos(theta); it disagreed by 1% for the f orbitals with z^3-type parts. That was the test's own error, which Gauss-Legendre removed; the app's code was unchanged.)
- **The sampled dots** (Ultra, fixed seeds, batch means): the fraction inside the outline is 0.9104 (1s), 0.9010 (2s), 0.9128 (2p), 0.9078 (3d), 0.9004 (4f), each within 4 standard errors (0.0071 to 0.0089; the largest is 1.8).
- **Z scaling**: psi at Z = 2 on the halved contour points equals psi at Z = 1 (checked point by point); at Z = 2 the same level holds 0.90 and reaches half as far (e.g. 4f|xyz: 29.63 a0 at Z = 1, 14.95 at Z = 2, half would be 14.81, within the grid tolerance).
- Settings: default Off, good/bad values, file round trip, setParam; benchmark scenes 1 to 5 have the outline Off.
- The tests do **not** prove how it looks.

## Looked at (Linux, Xvfb, software OpenGL; 4 pictures, one draft pass first)
A first set (denser wireframe, 7 planes) was a draft that I reduced to 5 planes and fainter lines; the 4 below are the final ones.
- Hydrogen 1s, Cloud 3D, Dark: a light, thin wireframe sphere round the glowing core; the dots show through. Good.
- Hydrogen 2p, Cloud 3D, Paper: two lobes in the wireframe (the dumbbell); readable, the node plane between the lobes is clear.
- Hydrogen 3d (dz2), Cloud 3D, Dark: two lobes and a ring in the wireframe, the dark gap at the nodes stays visible.
- Hydrogen 3s, Slice, Paper: the contour is four rings (an inner core and two shells, as the nodes make); the gaps between the rings, where the dots thin out at the nodes, are not covered by the line.
- Cloud (side view) was not looked at, and Dark Slice was not looked at. The wireframe is drawn over the dots at constant brightness, so on a crowded orbital (4f) it may look busy; I did not look at 4f.

## Not tested
- No Windows PC and no GPU: nothing about speed or the real-screen look. Only the time to compute the outline here (above).
