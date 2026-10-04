# Physics Lab — native Windows version

A native C++ version of Physics Lab for Windows (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
Version **0.10.6 alpha**. The app is in **alpha**: features are still being added (beta will follow when everything planned for 1.0 is in, then the plain release). It has the **Orbit Lab and Atom Lab**, each with everything of its web version except a few small items listed in `REPORT.md`, and the web app's own features: Settings, themes, the header with About and history, the "Did you know?" facts, the Learn and Achievements tabs, the **fps counter** in both labs, 📷 **Save image** (a PNG of the picture, saved in your Pictures folder), and everything remembered between runs (see 🕰 history in the app for what is new). The Black Hole Lab, the Spacetime Lab and the Slingshot game are not part of the native app. The web version (`../index.html`) is the
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

### If Windows warns you

The app is **not code-signed yet** (code signing is on the roadmap, `../ROADMAP.md`). Because of that Windows may
warn when you open it. This is a Windows reputation warning for an app it does not know; it does **not** mean a virus
was found. Either:

- **Unblock the zip before unzipping:** right-click `PhysicsLab-windows-x64.zip`, choose **Properties**, tick
  **Unblock**, press **OK**, then unzip; or
- when **"Windows protected your PC"** (SmartScreen) appears, click **More info**, then **Run anyway**.

The same help is in the text of the `latest` release.

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

The physics tests include `jpl_tests`, which compares the Real planets with JPL Horizons vector tables kept in `native/data/jpl/` (see the README.txt there; `native/tools/make_jpl_data.py` checks them and rewrites the generated `.inc` files). The Real planets' start state is the 2026 row of those tables (`src/orbit/jpl_epoch.inc`, made by the same tool).

`moon_tests` (0.10.4) checks the Moon from the lunar theory ELP2000-82B (`src/orbit/moon_elp.*`, terms in `src/orbit/elp_data.inc`, made by `tools/make_elp_data.py` from the published files) against a Horizons Moon table (`tests/reference/moon_table.inc`, made by `tools/make_moon_ref.py`). `moon_real_tests` (0.10.4 stage 2) checks the Real-mode Earth and Moon in `SolarSystem` against a Horizons Earth table (`tests/reference/earth_table.inc`, made by `tools/make_earth_ref.py`), the phases, the fast-speed rule and that the other planets are not worse than in 0.10.3. In the app, Planet physics Real draws Earth's Moon from ELP2000-82B (real direction and phase, exaggerated distance) and its card shows the phase.

## Using it

Pick the lab with the two buttons at the top of the panel: **Orbit Lab** or **Atom Lab**.
Each lab keeps its state while the other one is on screen.

### Across the app (as in the web version)

- **Header**: 🎨 Theme (a grid of eleven theme cards, three in a row, the description as a tooltip; Synthwave and Monochrome are secret until found), 🌐 language
  (English; Eesti keel is coming), 👤 About (the developer, and where the facts come from, with links),
  ⚙ Settings, 🕰 history (the web's history, then the native versions) and the lab's **Update** badge
  (that lab's changelog).
- **⚙ Settings**: text size, font, units (metric or imperial), motion (Auto follows Windows'
  "Animation effects"; Reduced starts the labs paused and stops gliding and spinning), Reset my
  progress, and Developer → Unlock everything.
- **Benchmark** (Performance card at the bottom of the panel, in both labs → **Benchmark...**): a test of about 40 seconds that gives one set of numbers to compare between runs, versions and PCs.
  Click **Start**. It runs 4 fixed scenes (the heaviest Orbit Lab view, the uranium-238 nucleus, Cloud 3D, one nucleon), each with 2 s of warm-up that is not counted and 8 s that are, with VSync off, at the window's size
  (so use the same window size, or full screen with F11, when you compare). **Esc** or **Cancel** stops it. Everything is put back as it was. At the end a table shows the average frame time and fps, the 1 % low, the worst frame,
  the CPU work and the GPU time of each scene; the same result is on the clipboard (**Copy result** puts it there again) and saved as `physics-lab-benchmark-DATE-TIME.txt` in your Pictures folder
  (Downloads if that fails). Two tick boxes add more scenes: **stress scenes** (3 heavier ones) and, new in 0.10.6 stage 7, **Atom Lab speed-risk scenes** (11 more, numbered 8 to 18: phase colours with Classic dots in 2D and 3D, a cloud jump held half way and jumps one after another, a superposition (orthogonal, overlapping, beating), the 90 % outline's first use and with the cut dragged, and the nucleus view; every setting is in the scene's name and the **Worst** column shows the stutter). On the first run (4 Oct 2026) scenes 11 and 12 (the cloud jump) were by far the slowest; 0.10.6 stage 8 changed the jump, and the same two scenes show whether it helped. **To send it:** paste the text into your message. Close other programs first. **Do not move or resize the window while it runs: that stops the test.**
  - **The GPU time** is the time the graphics card was busy with the whole drawing of a frame, from the first drawing command to just before the buffer swap: the picture, its soft (blurry) layer, the instanced draws and the panels and text.
    The result also gives the "span" (from the first to the last GPU command of the frame, waiting included). The timer comes late (a frame or two) and not every driver gives it right, so before the first scene the test draws a known heavy load
    and compares the timer with a clock (about a second). If they disagree, or fewer than half of the frames got a result, the table says **not reliable** instead of a number (the raw values are in the text file, marked as not to be trusted).
    The frame time and the CPU work are always real; when the GPU time is small next to them, the frame is limited by the CPU, not the graphics card.
  - **Include stress scenes** (a checkbox in the window before you start, off by default): two extra scenes after the four normal ones (20 seconds more) that load the PC much harder, to see where it becomes the limit:
    the biggest nucleus (oganesson-294, Accurate quarks, + Sea, every nucleon drawn large) and the heaviest Orbit Lab view with 8 times the belt rocks (48,000 instead of 6,000; the physics is unchanged).
    The four normal scenes never change, so their results stay comparable with earlier versions.
- **Moving or resizing the window** no longer freezes anything: the simulation and the drawing keep running while you hold or drag the title bar or an edge, and the picture and panels follow the new size live.
  (Windows pauses an app's own loop during a drag; the app draws from SDL's refresh signal for that time.) The Performance card shows how many frames were drawn during drags and the longest gap between them.
- **The window** opens where and how big it was last time (and maximised, if it was); if that place is no longer on a screen it opens centred. **F11** turns full screen on and off.
- **The icon** (an atom) is on `PhysicsLab.exe`, in the taskbar and in the window. If the window or OpenGL 3.3 cannot start, a message box says so in plain words and suggests updating the graphics driver.
- **💡 Did you know?**: a fact under the header; click for the next one. Somewhere in the list is a 🎁.
- **Tabs**: 🔬 Simulations (the labs), 📚 Learn (every guide of both labs; a click opens it in its lab)
  and 🏆 Achievements (four to find).
- **Remembered between runs**: everything above and every setting of both labs, in
  `%APPDATA%\PhysicsLab\PhysicsLab\settings.txt` (a plain text file; delete it to start fresh).
  **Defaults** still puts a lab back to the web's starting settings.
- **Fonts**: Atkinson Hyperlegible (Easy to read) and Nunito (Rounded) are built into the .exe,
  under the SIL Open Font License (`assets/fonts`, licences in the zip's `font-licences` folder).
  Georgia (Serif and the Paper theme), Consolas (Monospace) and Segoe UI come with Windows.

### Orbit Lab

- **Play / Pause**: the button in the Time card, or the Space bar.
- **Time speed**: the slider (from real time up to 1 s = 50 years).
- **Zoom**: the mouse wheel over the picture, or the Zoom slider.
- **Move the view**: drag with the mouse.
- **Facts**: click a planet, moon, comet, the Sun or the barycentre. The first click
  afterwards closes the box. **Go to** tiles fly to a body and keep it in the middle.
- **3D**: View card → 3D. Drag turns the view; right-drag or Shift-drag moves it.
- **Planet physics** (Orbit panel, Simple / Real, default Simple): Real lets the Sun and the eight planets pull on each other (Newtonian gravity, a Wisdom-Holman integrator with a one-day step, `src/orbit/nbody.*`). The start values are JPL Horizons' (DE441) positions and velocities of 1 January 2026 12:00 TDB (`data/jpl`, `src/orbit/jpl_epoch.inc`), moved to the lab's own centre of mass; dates are treated as TDB. Against Horizons' tables for 1800 to 2100 the worst planet errors are at most 29,231 km (Mercury, relativity off; the app's note lists all). Real is still an option until the owner has tried it. **Earth's Moon in Real (0.10.4):** its direction, distance and phase come from ELP2000-82B (`src/orbit/moon_elp.*`; at the dates checked within 5.2 km of Horizons at worst), Earth is drawn at the Earth-Moon barycentre minus the Moon's share, the Moon is drawn at the usual exaggerated moon distance, and outside 1800-2100 it falls back to the Kepler Moon. Every other moon, the dwarf planets, comets and belts move as in Simple, and there is no effect of moons or asteroids on the planets. **General relativity (1PN)** (tick box in Real, off by default since 0.10.3: measured against JPL it moves every planet away from JPL): the Sun's first-order potential (Nobili & Roxburgh 1986, as in REBOUNDx's `gr_potential`), so Mercury's perihelion turns about 43 arcseconds per century more; Sun's field only, no relativity between the planets, no higher orders. **The Sun's wobble** (Real): the picture is drawn round the barycentre from the integrator, the Sun's own path is drawn as a trail, and "Exaggerate the Sun's wobble (×50)" draws that path 50 times too big (labelled, not to scale).
- **Sky** card: stars, constellations, far-away things (zoom far out to find them). The Milky Way is one smooth band (labelled with the constellation names on); the far-away stars show a published mass under their name where one could be sourced (`src/orbit/far_masses.h` lists each value with its paper).
- **Journey and dates** card: the Journey (touch anything to stop it; your settings come back),
  and Jump to a date (1800–2100, Greenwich time).
- **Performance** card: frame time and a VSync switch (turn VSync off to see the real headroom).
- **Learn** card: What am I even looking at?, Why do we have seasons?, and the Solar System
  timeline (the web text, word for word; ▶ buttons take the lab there).
- **Defaults** (top of the panel): puts Orbit Lab's settings back to how they started, after asking.

### Atom Lab
- **Nucleus** card: click an element in the periodic table (⤢ Bigger table for a large one), or use the
  Protons and Neutrons sliders with − and +, or Famous isotopes. The card shows the element and its facts;
  🎲 Reshuffle nucleons draws a new arrangement; ☢ Build another appears after a fission.
- **Decay history**: unstable nuclei decay by themselves, with their real half-lives and odds; each step is listed.
- **Electron** card: View (Orbit, Cloud, Cloud 3D, Slice), Orbit style (Modern / Bohr 1913), and for the clouds **Cloud style** (Glow: soft dots that add up; Classic: the web's small hard dots) and **90% outline** (Off / On: a thin line round the smallest region where the electron is found 90% of the time, in the one-electron hydrogen-like model; `src/atom/outline.*`), and **Cloud density** (Normal 12,000 dots, High 24,000, Ultra 48,000; more dots show the same probability more smoothly, the physics is the same), Orbital and its **Phase colours** (Sign / Wheel, a colour ring legend): the wheel colours each dot by arg(ψ) with a cyclic constant-lightness CIELAB map; the **complex orbitals** m = ±1 … ±l of p, d and f are in the Orientation choice and always use the wheel (`atom_core.*`, `phase_wheel.h`). **Superposition** (Off / On, second orbital, Mix): ψ = a ψA e^(−iEA t/ħ) + b ψB e^(−iEB t/ħ) with the lab's hydrogen-like energies; the cloud changes at the beat frequency, slowed on screen (4 s per beat at Playback speed 5), the readout gives the real beat period; dots are fixed samples of the mixture density, each shown with probability |ψ|²/(2q), so no dot remembers the past. **Smooth photon jumps**: in the cloud views two fixed populations of dots (the old cloud and a new one) are shown with exact fractions while the mixing angle goes 0 → π/2 over 1.2 s; in the Orbit view the electron glides between the circles over 0.8 s; an emitted photon appears when the jump ends, an absorbed one starts the jump when it arrives; Reduced motion makes jumps instant. Textbook picture of a transition, not a calculation of photon absorption.
  orientation, the cut for Slice, Energy level and Playback speed for Orbit.
- **Photons** card: Emit, Absorb, Auto photons. The lines you make collect on the picture and in **Light spectrum**.
- **View & zoom**: Scale, Zoom (also the mouse wheel), and at deep zoom the quark style and nucleon detail. On the light themes the quarks, their glow and the gluon field are drawn in darker colours with ordinary blending (no dark backdrop); on dark themes they glow as before.
  Under the picture: ⌕ Atom / Nucleus / Nucleon, ⌖ Centre, ✥ Drag, ↺ Reset view, and the hint.
- **Mouse**: drag turns the atom (Orbit), rotates (Cloud 3D), slides the cut (Slice) or moves the picture (Cloud);
  right-drag or Shift-drag always moves it. Click a nucleon to dive into it; ⓘ shows more or less information
  (remembered between runs).
- **Temperature** card: the slider, solid / liquid / gas, the typical atom speed and a little box of atoms.
- **Learn**: What am I even looking at?, Scale, Weight and the timeline (the web text, word for word).
- **Defaults** (asks first) puts the Atom Lab settings back; **Reset** starts a fresh atom.

## How it is organised

| Folder | What is in it |
| --- | --- |
| `src/orbit/units.h` | SI units and every physical constant, with comments |
| `src/orbit/solar_data.*` | The real Solar System data (NASA/JPL), copied from the web version |
| `src/orbit/moon_elp.*` | The Moon's position from ELP2000-82B (0.10.4: used in Real mode only; Simple keeps the Kepler Moon) |
| `src/orbit/kepler.*` | Orbital elements → position and velocity (Kepler's equation) |
| `src/orbit/solar_system.*` | The simulation: leapfrog steps around the Sun, barycentre, orbit shape |
| `src/orbit/belts.*` | Asteroid belt, Trojans, Kuiper belt, Oort cloud (same random rocks as the web) |
| `src/orbit/sky.*`, `sky_data.inc`, `far_masses.h` | Night sky, constellations, far-away things, and their published masses |
| `src/orbit/spin.*` | IAU spin of the planets and the Sun, axis tilt, Earth's seasons |
| `src/orbit/card_text.*`, `format.*` | The info card and how numbers are written |
| `src/orbit/orbit_view.*` | Drawing the picture, clicks, dragging, zoom |
| `src/atom/atom_core.*`, `atom_data.inc` | Atom Lab physics: elements and nuclides, orbitals, cloud sampling, spectrum, nucleus, decay, mass, temperature |
| `src/atom/atom_sim.*` | The Atom Lab simulation: settings, the electron's orbit, photon jumps, the decay clock, readout and hint |
| `src/atom/atom_learn.*` | The Atom Lab's guide and panel texts (the web's, word for word) |
| `src/atom/atom_view.*` | Drawing the Atom Lab picture (instanced clouds, glows and gluon fields, a soft layer) |
| `src/render/` | OpenGL 3.3 loader, batched 2D renderer (instanced dots, planet shader), Earth map |
| `src/app/prefs.*`, `app_text.inc` | What is remembered (the settings file), themes, achievements; the app-wide web texts |
| `src/ui/` | The ImGui panels of both labs, Learn guides (rich-text and HTML layout), Settings, themes and the look (`look.*`), the header, About and history (`app_pages.*`), the facts line, tabs and achievements (`app_tabs.*`), Journey and dates, changelogs, fonts |
| `assets/fonts`, `cmake/embed_fonts.cmake` | The bundled free fonts, built into the .exe |
| `tests/` | Unit tests that compare the physics with the web version |
| `tools/make_sky_data.js` | Copies the sky, far-away and Earth-outline data out of `index.html` |
| `tools/make_learn_data.js` | Copies the Learn text out of `index.html` by running the web's own guide code |
| `tools/make_atom_data.js`, `tools/make_atom_learn.js` | Copy the Atom Lab data, timeline and history out of `index.html` |
| `tools/make_app_text.js` | Copies the app-wide texts (Settings, themes, header, About, history, facts, tabs, achievements) out of `index.html` by running the web's own code |

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

`tests/test_atom.cpp` does the same for Atom Lab (about 17,800 checks). `tests/reference/make_atom_reference.js`
loads the web Atom Lab code into Node (`atom_web.js`), with `Math.random` replaced by the same seeded generator the
native lab uses, and records: every nuclide's description, decay modes, binding energies, shapes, number formats,
ψ for every orbital, the dipole rule, spectral lines, seeded nuclei, and whole seeded runs of the lab (orbit, cloud
dots, photon jumps, decays, readout and hint lines). `make_atom_learn_reference.js` records the Learn guides and the
panel texts with a small fake page (`fake_dom.js`) for many atoms, in metric and imperial. Rerun both (from the
repository root) if the web Atom Lab changes.

`tests/test_app.cpp` checks the settings file (saving, loading, broken or unknown lines), every theme's colours,
and the app-wide texts against runs of the web's own code (`make_app_reference.js` with `app_web.js`): the reset
pop-ups for every count, the header, About, the language menu, the history, the 27 facts, the secret pop-ups, the
Achievements and Learn tabs, and the dates.

## Command-line helpers for automatic checks

These do nothing in normal use: `PL_EXIT_AFTER_FRAMES=n` quits after n frames,
`PL_SCREENSHOT=file.bmp` saves the last frame, `PL_SELECT=Earth` selects and follows a
body, `PL_ZOOM=2.3` sets the zoom, `PL_3D=1` starts in 3D, `PL_TILT=yaw,pitch` sets the 3D view's angles (radians), `PL_PHYSICS=Real` and `PL_GR=0|1` set Planet physics and General relativity, `PL_CONS=1` shows constellation
names, `PL_DATE=2003-08-27` jumps to a date (and prints the note), `PL_JOURNEY=1` starts a Journey,
`PL_LEARN=1|2|3` opens a Learn guide, `PL_IMPERIAL=1` shows imperial units, `PL_SKYOFF=1` turns the
stars off and `PL_LIGHT=1` pretends the system theme is light. For Atom Lab: `PL_ATOM=1` opens it,
`PL_ATOM_SET="Z=26;view=Cloud;zoom=4.5"` sets its settings, `PL_ATOM_DO=emit|absorb|auto|nucleus|nucleon` (with `PL_ATOM_DO_FRAME=n`: at frame n, after the cloud has filled) presses a
button, `PL_ATOM_FOCUS=n` follows nucleon n, `PL_ATOM_LEARN=1..4` (with `PL_ATOM_TAB=ten|miss|...`) opens a guide,
and `PL_SCROLL=y` scrolls the Atom Lab panel. App-wide: `PL_SETTINGS=file` uses another settings file,
`PL_THEME=paper`, `PL_TEXTSIZE=xl`, `PL_FONT=easy` and `PL_MOTION=on` set the look, `PL_SELECT=far:sgra` picks a
far-away thing, `PL_DYK_FIRST=26` puts that fact first (26 is the 🎁), and `PL_EXIT_AFTER_SECONDS=s` quits after s seconds.
