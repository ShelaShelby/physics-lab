# Physics Lab — native Windows version

A native C++ version of Physics Lab for Windows (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
Version **0.2**: **Orbit Lab only** (see the Update badge in the app for what is new). The web version (`../index.html`) is the
reference; this folder does not change it.

## Getting the .exe without building

The newest build is always on one fixed page: the GitHub **Release tagged `latest`**
(`https://github.com/ShelaShelby/physics-lab/releases/tag/latest`). Download
`PhysicsLab-windows-x64.zip`, unzip it and run `PhysicsLab.exe`.

It is also attached to each workflow run:

1. On GitHub, open the **Actions** tab and pick **Native Windows build**.
2. Either open the latest run (it runs on every push to the `native` branch that changes
   `native/`), or press **Run workflow** to start one by hand.
3. When it is green, download **PhysicsLab-windows-x64** at the bottom of the run page,
   unzip it and run `PhysicsLab.exe`. No installer and no extra DLLs are needed.

One build takes roughly 5–10 minutes. GitHub counts minutes on Windows runners double for
private repositories, so a build uses about 10–20 of the 2,000 free minutes a month.

## Building on your own Windows PC

You need:
- **Visual Studio 2022** (Community is free) with the *Desktop development with C++* workload
  (it includes CMake), or Visual Studio Build Tools + CMake 3.24 or newer.
- **Git** (CMake uses it to download SDL3 and Dear ImGui at fixed versions the first time).

In a *Developer PowerShell for VS 2022*, from the repository root:

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release --parallel
ctest --test-dir native/build -C Release --output-on-failure   # the physics tests
native/build/Release/PhysicsLab.exe
```

Or open the `native` folder in Visual Studio (File → Open → Folder) and press F5.

## Using it

- **Play / Pause**: the button in the Time card, or the Space bar.
- **Time speed**: the slider (from real time up to 1 s = 50 years).
- **Zoom**: the mouse wheel over the picture, or the Zoom slider.
- **Move the view**: drag with the mouse.
- **Facts**: click a planet, moon, comet, the Sun or the barycentre. The first click
  afterwards closes the box. **Go to** tiles fly to a body and keep it in the middle.
- **3D**: View card → 3D. Drag turns the view; right-drag or Shift-drag moves it.
- **Sky** card: stars, constellations, far-away things (zoom far out to find them).
- **Journey and dates** card: the Journey (touch anything to stop it; your settings come back),
  and Jump to a date (1800–2100, Greenwich time).
- **Performance** card: frame time and a VSync switch (turn VSync off to see the real headroom).
- **Update** badge (top of the panel): what is new.

## How it is organised

| Folder | What is in it |
| --- | --- |
| `src/orbit/units.h` | SI units and every physical constant, with comments |
| `src/orbit/solar_data.*` | The real Solar System data (NASA/JPL), copied from the web version |
| `src/orbit/kepler.*` | Orbital elements → position and velocity (Kepler's equation) |
| `src/orbit/solar_system.*` | The simulation: leapfrog steps around the Sun, barycentre, orbit shape |
| `src/orbit/belts.*` | Asteroid belt, Trojans, Kuiper belt, Oort cloud (same random rocks as the web) |
| `src/orbit/sky.*`, `sky_data.inc` | Night sky, constellations, far-away things |
| `src/orbit/spin.*` | IAU spin of the planets and the Sun, axis tilt, Earth's seasons |
| `src/orbit/card_text.*`, `format.*` | The info card and how numbers are written |
| `src/orbit/orbit_view.*` | Drawing the picture, clicks, dragging, zoom |
| `src/render/` | OpenGL 3.3 loader, batched 2D renderer (instanced dots, planet shader), Earth map |
| `src/ui/` | The ImGui panel, the changelog and fonts |
| `tests/` | Unit tests that compare the physics with the web version |
| `tools/make_sky_data.js` | Copies the sky, far-away and Earth-outline data out of `index.html` |

All physics is in SI units (metres, seconds, m/s). The web version uses "sim units"
(GM of the Sun = 100, 1 year = 2π); `units.h` converts them exactly, so both versions
follow the same orbits.

## Tests

`tests/test_orbit.cpp` compares the C++ physics with numbers produced by the web
version's **own** JavaScript: `tests/reference/make_reference.js` cuts the Orbit Lab
functions out of `index.html`, runs them in Node and writes `reference_values.h`.
It checks the starting positions of all planets, dwarf planets and comets, their orbit
shapes, the barycentre, the positions after fixed runs of the leapfrog integrator at
three Time speeds, the belt rocks, and the spin / axis / season lines on the cards
(word for word). It also checks two dates the web quotes: Mars 55.8 million km away on
27 Aug 2003, and Jupiter and Saturn 0.1° apart on 21 Dec 2020. If the web physics ever changes, run
`node native/tests/reference/make_reference.js` from the repository root and rebuild.

## Command-line helpers for automatic checks

These do nothing in normal use: `PL_EXIT_AFTER_FRAMES=n` quits after n frames,
`PL_SCREENSHOT=file.bmp` saves the last frame, `PL_SELECT=Earth` selects and follows a
body, `PL_ZOOM=2.3` sets the zoom, `PL_3D=1` starts in 3D, `PL_CONS=1` shows constellation
names, `PL_DATE=2003-08-27` jumps to a date (and prints the note), `PL_JOURNEY=1` starts a Journey.
