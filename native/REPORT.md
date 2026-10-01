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

## Checked (Linux, Xvfb, software OpenGL)
- `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,006 checks: all pass.
- Screenshot 1, Orbit Lab, Paper (light) theme, Extra large text: "True scale", "Short tail" and "Soft" show in full, with the arrow visible; the 0.9.1 slider and the full-width Drag button still look right. The open list was not looked at (no mouse clicks).
- Screenshot 2, Atom Lab, dark theme, Extra large text: shows the top of the panel only (buttons, periodic table, Hydrogen card), nothing cut off there. The sliders are further down the panel and **were not seen**: the limit of two screenshots was used up.

## Not tested / still to do
- No Windows PC: not seen on the real PC or at Windows scaling above 100 %.
- The open drop-down list (the wrapped explanations, the width of the popup) was not looked at on screen.
- The 0.9.1 slider look in the Atom Lab (dark theme) is still unchecked; it uses the same style code as the Orbit Lab sliders that were checked.
