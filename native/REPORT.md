# Native Physics Lab 0.1 — pilot report (Orbit Lab)

## What works (checked)

**Physics, ported from the web Orbit Lab (index.html, Orbit Lab update 7.7.2)**
- Same data: the 8 planets (NASA/JPL Keplerian elements 1800–2050 with rates per century),
  5 dwarf planets (JPL Small-Body Database), 14 moons, 6 famous comets. Only the real
  Solar System; no custom star or planets.
- Same method: leapfrog (kick-drift-kick) around the Sun, sub-steps of at most
  0.004 sim units (about 5.6 h), fixed physics step of 1/240 s of real time, frames capped
  at 0.05 s, the same softening, crash and escape radii, trail spacing, comets set from
  their exact Kepler orbit each step, moons on fixed Kepler ellipses.
- All in SI units (m, s, m/s); `units.h` converts the web's "sim units" exactly.
- **Unit tests (643 checks, all pass)** against numbers produced by running the web
  version's **own JavaScript**, cut out of index.html by a script, not retyped:
  starting positions/velocities of all 19 bodies, orbit shapes (a, e, tilt, period),
  barycentre, positions after 10 s at the default Time speed, 1 s at the fastest
  (≈50 years) and 1 s at real time. Worst difference: **2.6 × 10⁻¹¹** of the distance
  from the Sun (rounding only). A deliberately broken step size makes 43 checks fail,
  so the tests do catch mistakes. Also checked: Earth returns to its start after one
  year and its orbital energy stays constant to 1 part in a million.
- Info-card numbers (distance, speed, escape speed, lap, perihelion/aphelion, barycentre)
  were compared with the web formulas for Earth, Pluto and Halley: identical.

**App (checked on Linux with software OpenGL, Mesa llvmpipe, under a virtual screen)**
- Window opens, OpenGL 3.3 core renderer works, the picture draws: Sun with glow, lit
  planets, dwarf planets, moons (shown as in the web: when zoomed in or their planet is
  picked; fast moons as a lit orbit), comets with tails and dotted orbits, short or full
  trails, habitable zone, distance rings, barycentre marker, labels, scale bar with light
  time, x/y/z arrows, date line, fps.
- Play/pause (button and Space), Time speed slider, zoom (wheel and slider), drag to pan,
  click to select with the info card (first click closes it), Follow, Reset view,
  Reset to today, Sizes (True scale / Easy to see), Trails, Moons/Dwarf planets/Comets
  switches, Go to tiles, "Slow time to watch moons", Update 0.1 badge with changelog
  (native history + the web Orbit Lab history for reference).
- Real mouse input was sent with XTest: wheel zoom, click on the Sun (card appears), drag.
- Deep zoom (Earth filling the picture) draws correctly; the maths stays in double
  precision until points are on screen.

## Not tested / not known yet (be aware)

- **The Windows build itself has not been run by me.** It was only compiled and run on
  Linux. The GitHub Actions workflow is written for Windows (MSVC) but, at the time of
  writing, I could not confirm a finished run (see the end of this file). Windows-only
  code paths (Segoe UI font loading, static MSVC runtime with static SDL3, WinMain via
  SDL) are untested.
- **Speed on a real PC is unknown.** The cloud machine has no GPU; its frame rate says
  nothing. The design is light (one draw call for the picture; physics is ~1 million
  simple steps per second at the fastest Time speed), but it needs checking on Alex's PC.
- High-DPI: the app scales with the Windows display scale at start-up; moving the window
  to a screen with a different scale is not handled yet.
- Emoji used in the web (🚀, ⏱ in some fonts) may show as boxes if a font lacks them.

## Small differences from the web version (on purpose, or not ported)

- Speeds are worked out in exact SI. The web multiplies by a rounded factor
  (29.78 km/s based), so a speed can differ in the last shown digit (≈0.02 %).
- Dragging moves the view at any zoom (the web only allows it once zoomed in, because on
  a phone a drag must scroll the page).
- The card says "click anywhere to close this box" instead of "tap".
- Background is the dark night-sky colour, but **without the stars**.
- **Not ported in this pilot:** 3D view, night sky / constellations, far-away things
  (Voyagers, nearest stars, galactic centre), asteroid belt / Trojans / Kuiper belt /
  Oort cloud, spinning planets with axes and Earth's continents, Saturn's rings, seasons
  and Sun spin lines on the cards, Journey, Jump to a date, Learn (timeline and guides),
  "What am I looking at?", achievements, metric/imperial switch, the other labs.

## What to do next (suggested order)

1. Download the Actions artifact, run it on Windows, check it opens and feels smooth.
   Report anything odd (fonts, scaling, speed).
2. 3D view (the camera code is small and already understood).
3. Belts and Oort cloud, then the night sky and far-away things (large data tables).
4. Spinning planets, Saturn's rings, seasons, Jump to a date and Journey.
5. Learn content with custom text rendering, then the metric/imperial switch.
6. Then the next lab (Atom Lab, Black Hole Lab, Spacetime Lab, Slingshot).

Each of these is an update: the version number will be asked before starting.
