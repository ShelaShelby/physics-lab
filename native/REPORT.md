# Native Physics Lab 0.2 — report

0.2 was built in four stages, each committed and pushed to `native` as it was finished.
The 0.1 pilot report is in the git history (`git show a71e6d6:native/REPORT.md`).

## What works (checked)

**Stage 1: frame timing, VSync, fixed download page**
- A Performance card shows the last frame time, the average and worst of the last 240 frames
  (with a graph), and the CPU work per frame (physics plus building the frame, before
  waiting for the screen). A VSync switch turns waiting for the monitor on and off.
- Every Windows build is also published to the GitHub Release tagged `latest`
  (`…/releases/tag/latest`, file `PhysicsLab-windows-x64.zip`). The run for Stage 2 passed
  on GitHub's Windows runner, including this release step.

**Stage 2: asteroid belt, Jupiter's Trojans, Kuiper belt, Oort cloud**
- Same data and behaviour as the web: the same random generator (mulberry32), seed, order of
  draws, 32-bit storage, orbits and 150-year Trojan sway. Belt cards, clickable belt names,
  Go to tiles and the on/off switch work.
- Tested against the web's own JS output for 18 sample rocks, at the start and 100 years
  later. They match to 4×10⁻⁸ (32-bit float rounding).
- Drawing: each belt is **one instanced draw call** (the web used thousands of canvas
  operations). Rock positions are computed on the CPU in double precision, and only when
  simulated time moves (paused or just panning: nothing is recomputed). On this cloud CPU
  that is about 0.7 ms per frame for all 6,000 rocks.
  - I did not move the Kepler maths onto the GPU on purpose. OpenGL 3.3 shaders only have
    32-bit floats, and the mean anomaly n·t would drift by thousands of km after a few
    decades, so the rocks would stop matching the web physics.

**Stage 3: 3D view, night sky, far-away things**
- 3D view with the web's exact camera maths (yaw, pitch, perspective, follow depth).
  Drag turns it, right-drag or Shift-drag moves it, plus the Drag-mode and Spin buttons.
  Drop lines, perspective sizes, ≈ on the scale bar and the turning x/y/z arrows are there.
- Night sky: 2,874 stars at true directions and colours, the Milky Way band, and
  constellation lines and names. Stars are drawn as instanced dots grouped by colour and
  size (36 draw calls).
- Far-away things: nearest stars, the Pleiades, the Orion Nebula, the galactic centre and
  the Voyagers. The Voyagers move on with time and are hidden before 1990. Each has a card
  and a Go to tile.

**Stage 4: spinning planets, Jump to a date, Journey**
- IAU spin data (with the Mars and Neptune wobble terms). Earth's globe uses the same
  Natural Earth coastlines as the web; the other planets get a latitude/longitude grid and
  the 0° meridian. Each has a dashed axis with N, Saturn's rings tilt with its real axis,
  and Earth has its thin blue air.
- The card lines (turn time, axis tilt, "upside down", too fast to see, the Sun's spin,
  Earth's season and where the Sun is overhead) match the web **word for word** on three
  test dates. The only exception is the "turns N times a second" number: the web uses a
  rounded 58.13 days/s, the native app the exact 58.1302, so that number can differ in its
  last digit.
- Jump to a date: 1800–2100, Greenwich time, with the 7 quick tiles. The selected body
  stays selected. Checks the web quotes: Mars to Earth on 27 Aug 2003 is 55.80 million km
  (NASA: 55.76), and Jupiter–Saturn on 21 Dec 2020 is 0.13° apart (about 0.1° in reality,
  given the approximate JPL elements).
- **Web bug fixed:** a date outside 1800–2100 gets a note saying it is out of range and
  which date is shown instead. An impossible date (30 Feb) or bad input gets a note too.
  Before, the web jumped silently.
- **Web bug fixed:** Journey saves all your settings and your view (zoom, time speed,
  2D/3D, sizes, belts, moons, far things, selection, follow, pan, 3D angles, pause) and
  puts them all back when it ends or is interrupted. The web only restored the Time speed.
  Checked: a Journey started in 3D and stopped by a click came back to the same 3D view,
  zoom and time speed.

**Tests:** 829 checks, all passing (physics, belts, spin and season wording, dates and
number formats). Run with `ctest`. GitHub Actions runs the same tests on Windows.

## Not tested / be aware
- **Speed on a real PC:** the cloud machine has no GPU, so I could not judge it. Please
  use the new Performance card with VSync off on your PC. Places that may cost the most:
  - the planet shader when a planet fills the screen;
  - the 2,874-star sky (recomputed every frame, cheap on a desktop CPU);
  - Earth's map is painted once at start-up (about 0.3 s here), so the window may open a
    moment later.
- **The Stage 3 and Stage 4 builds on Windows:** pushed, but I only saw the Stage 1 and
  Stage 2 runs pass. Check the Actions tab, or that the `latest` release has the newest
  commit.
- Fonts on Windows: Segoe UI and Segoe UI Symbol. I removed the 🚀 and ⏱ emoji from
  buttons because the fonts have no emoji; other symbols (☉, ⟲, ✥, ◉) were checked on
  Linux only.
- Real-mouse 3D rotation was not driven by a test. Rotation uses the web's formulas, but
  please try right-drag and Shift-drag on your PC.

## Still different from the web (not ported, or on purpose)
- Not ported: the Sun's decorative glowing patches; the planets' sprite caching (not needed
  with a shader); Learn / timeline / "What am I looking at?"; achievements; the
  metric/imperial switch; the other labs.
- Lighting: plain Lambert shading from the Sun's direction, not the web's exact radial
  gradient. It looks similar but is not identical pixel for pixel.
- With Stars set to Off, the background stays dark (the web uses the light/dark page theme).
- The last Journey caption says your view comes back, instead of "Tap Reset view to come back".
- A click that stops a Journey only stops it. It does not also select or move anything.

## Suggested next steps
1. Measure on your PC with VSync off, and tell me any slow spot.
2. Learn content (the timeline and the guides) with custom text rendering.
3. The metric/imperial switch.
4. The next lab.
