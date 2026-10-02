# Native Physics Lab 0.9.2 — report

Small update: the Orbit Lab's drop-down boxes are no longer cut off at big Text sizes. Older reports are in git history.
`index.html` was not touched.

## What changed
- `ui/orbit_panel.cpp`: a small helper `shortCombo` replaces the four drop-downs that had long closed-box text:
  - **Sizes:** "True scale" / "Easy to see";
  - **Trails:** "Short tail" / "Full orbit trails";
  - **Stars in the background:** "Soft" / "Full" / "Off";
  - **Constellations:** "Off" / "Lines" / "Lines and names".
  The closed box shows only the short name. The open list shows each short name with its explanation under it in muted, wrapped text ("planets and Sun as big as they really are"). All the words are the ones that were there; none of the texts the tests compare were changed.
- The "3D (rotatable)" box was already short. The Atom Lab panel has only one drop-down ("Pick one…" for a famous nucleus); I did not change it.
- Version 0.9.2 in CMake and a 0.9.2 entry in the 🕰 history.

### Stage 2: "alpha" label
- `kStage = "alpha"` in `ui/changelog.cpp` (one word; `""` gives the plain release). The number in CMake is unchanged (0.9.2). New `appStage()` and `appVersionLabel()` ("0.9.2 alpha").
- The 🕰 button reads "🕰 v0.9.2 alpha · history"; About's native line and the "you are here" history entry use the same label. The 0.9.2 history entry's title is now "Alpha label, drop-down text fits" and ends with the line about alpha, beta and the plain release.
- README says the app is in alpha.
- Workflow `native-windows.yml`: `gh release create latest ... --prerelease` (the `--latest` flag is dropped, since GitHub does not give a pre-release the Latest badge), title "Physics Lab native (latest build, alpha)". Tag `latest` and its link are unchanged.
- No test compared the button text or the version string (the benchmark test uses its own "0.9"), so no test was changed.

## Checked (Linux, Xvfb, software OpenGL)
- `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,006 checks: all pass.
- Screenshot 1, Orbit Lab, Paper (light) theme, Extra large text: "True scale", "Short tail" and "Soft" show in full, with the arrow visible; the 0.9.1 slider and the full-width Drag button still look right. The open list was not looked at (no mouse clicks).
- Screenshot 2, Atom Lab, dark theme, Extra large text: shows the top of the panel only (buttons, periodic table, Hydrogen card), nothing cut off there. The sliders are further down the panel and **were not seen**: the limit of two screenshots was used up.

## Not tested / still to do
- **Workflow change is untested until GitHub runs it**: after the next build, check the "latest" release page shows "Pre-release" and the new title.
- Stage 2 on screen: **no screenshot**. This container's SDL has no OpenGL/GLX (no X11 dev headers), so the app window could not open. The label code was built and the 3 test programs pass (1,369 / 17,818 / 1,006 checks), but the button, About and history text were not seen.
- No Windows PC: not seen on the real PC or at Windows scaling above 100 %.
- The open drop-down list (the wrapped explanations, the width of the popup) was not looked at on screen.
- The 0.9.1 slider look in the Atom Lab (dark theme) is still unchecked; it uses the same style code as the Orbit Lab sliders that were checked.
