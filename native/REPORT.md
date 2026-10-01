# Native Physics Lab 0.9.1 — report

A small update: the sliders look like sliders, and one cut-off button is fixed. The 0.9 report (freeze fix, GPU timer, stress scenes, Alex's 0.8 benchmark numbers) is in the git history: `git show 44f563a:native/REPORT.md`.
`index.html` was not touched.

## What changed
- **Sliders (both labs, all themes)**, in `ui/look.cpp`:
  - the track (frame colour) is stronger: 16 % of the text colour mixed into the card, was 7 %;
  - the borders are stronger (the line colour mixed 25 % toward the text colour), so the track has an outline on white panels;
  - the thumb is at least 16 px, scaled with the window's DPI scale and the Text size (`GrabMinSize`); its colour is the accent, slightly darker.
- **Value text no longer under the thumb** (`ui/orbit_panel.cpp`): the Time speed and Zoom values are written on the line above the slider ("Time speed: 1 s = 60 minutes", "Zoom: x20067"), and the slider's own text is empty. The Atom Lab's sliders already had no text inside; they get the new track and thumb.
- **"Drag rotates the view"** is now a full-width button (it did not fit half a row at big Text sizes); "Spin" is full width on the next line.
- Version 0.9.1 in CMake and a 0.9.1 entry in the 🕰 history.

## Checked (Linux, Xvfb, software OpenGL)
- `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,006 checks: all pass.
- One screenshot, Orbit Lab, 3D, Paper (light) theme, Extra large text: both sliders have a clearly visible grey track with an outline and a big dark-red thumb; the values are readable above them; "Drag rotates the view" is complete. (A first screenshot only showed the top of the panel and was used to find where the sliders are.)

## Not tested / still to do
- No Windows PC: not seen on the real PC or with Windows' own scaling (the thumb size uses the DPI scale; checked only at 1.0). Other light themes and the Atom Lab sliders were not looked at (same style code, no separate screenshot: two screenshots were the limit).
- Seen but not changed (not in the same row): the dropdowns "Sizes", "Trails" and "Stars in the background" cut their long selected text at Extra large ("True scale (planets and Sun as b").
