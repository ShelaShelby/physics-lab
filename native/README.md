# Physics Lab — native Windows version

A native C++ version of Physics Lab for Windows (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
Version **0.1**: a pilot with **Orbit Lab only**. The web version (`../index.html`) is the
reference; this folder does not change it.

## Getting the .exe without building

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
- **Update 0.1** badge (top of the panel): what is new.

## How it is organised

| Folder | What is in it |
| --- | --- |
| `src/orbit/units.h` | SI units and every physical constant, with comments |
| `src/orbit/solar_data.*` | The real Solar System data (NASA/JPL), copied from the web version |
| `src/orbit/kepler.*` | Orbital elements → position and velocity (Kepler's equation) |
| `src/orbit/solar_system.*` | The simulation: leapfrog steps around the Sun, barycentre, orbit shape |
| `src/orbit/card_text.*`, `format.*` | The info card and how numbers are written |
| `src/orbit/orbit_view.*` | Drawing the picture, clicks, dragging, zoom |
| `src/render/` | A small OpenGL 3.3 loader and a batched 2D renderer |
| `src/ui/` | The ImGui panel, the changelog and fonts |
| `tests/` | Unit tests that compare the physics with the web version |

All physics is in SI units (metres, seconds, m/s). The web version uses "sim units"
(GM of the Sun = 100, 1 year = 2π); `units.h` converts them exactly, so both versions
follow the same orbits.

## Tests

`tests/test_orbit.cpp` compares the C++ physics with numbers produced by the web
version's **own** JavaScript: `tests/reference/make_reference.js` cuts the Orbit Lab
functions out of `index.html`, runs them in Node and writes `reference_values.h`.
It checks the starting positions of all planets, dwarf planets and comets, their orbit
shapes, the barycentre, and the positions after fixed runs of the leapfrog integrator at
three Time speeds. If the web physics ever changes, run
`node native/tests/reference/make_reference.js` from the repository root and rebuild.

## Command-line helpers for automatic checks

These do nothing in normal use: `PL_EXIT_AFTER_FRAMES=n` quits after n frames,
`PL_SCREENSHOT=file.bmp` saves the last frame, `PL_SELECT=Earth` selects and follows a
body, `PL_ZOOM=2.3` sets the zoom.
