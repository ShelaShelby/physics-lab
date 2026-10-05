# Physics Lab — native Windows version

A native C++ version of Physics Lab for Windows (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
Version **0.14 alpha** (Themes: complete). The app is in **alpha**: features are still being added (beta will follow when everything planned for 1.0 is in, then the plain release). It has the **Orbit Lab and Atom Lab**, each with everything of its web version except a few small items listed in `REPORT.md`, and the web app's own features: Settings, themes, the header with About and history, the "Did you know?" facts, the Learn and Achievements tabs, the **fps counter** in both labs, 📷 **Save image** (a PNG of the picture, saved in your Pictures folder), and everything remembered between runs (see 🕰 history in the app for what is new). The Black Hole Lab, the Spacetime Lab and the Slingshot game are not part of the native app. The web version (`../index.html`) is the
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
  (Downloads if that fails). Two tick boxes add more scenes: **stress scenes** (3 heavier ones) and, new in 0.10.6 stage 7, **Atom Lab speed-risk scenes** (15 more, numbered 8 to 22, and from 0.13 eight more at the end, 23 and 24 (stage 1), 25 to 28 (stage 2) and 29 and 30 (stage 3), that run scenes 8 and 10, 11, 13, 14, 15, and 16 and 17, again with the old CPU ways, and from 0.13 stage 4 four more at the very end: 31 and 32 (scenes 1 and 6 with GPU rocks Off) and 33 and 34 (scenes 19 and 20 with the GPU chart cache Off); the scenes of 0.11 and 0.12 are the chart and chain ones, 19 to 22. Scenes 8 to 18 are: phase colours with Classic dots in 2D and 3D, a cloud jump held half way and jumps one after another, a superposition (orthogonal, overlapping, beating), the 90 % outline's first use and with the cut dragged, and the nucleus view; every setting is in the scene's name and the **Worst** column shows the stutter). On the first run (4 Oct 2026) scenes 11 and 12 (the cloud jump) were by far the slowest; 0.10.6 stage 8 changed the jump, and the same two scenes show whether it helped. **To send it:** paste the text into your message. Close other programs first. **Do not move or resize the window while it runs: that stops the test.**
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
- **Where the electron is found** card (new in 0.10.7, cloud views only): the radial probability graph, P(r) = r²|R(r)|² against the distance from the nucleus for the orbital on screen (area 1; P(r) dr is the chance of finding the electron between r and r + dr), with three marks: the most likely distance (the peak), the average distance and how far the 90 % outline reaches (the farthest point of the outline, which for p, d and f orbitals is not the radius holding 90 % of P). The axis is in Bohr radii, each mark also gives Å, and the scale follows the orbital and Z. Every orientation (m = 0, ±1, …) of a shell has the same curve, and the card says so. While a superposition is shown the card says why there is no graph; during a photon jump it waits for the landing. One electron, non-relativistic, point nucleus (`src/atom/radial_graph.*`, drawn in `src/ui/atom_panel.cpp`).
- **Real spectrum (NIST)** card (new in 0.10.7 stage 2), under Light spectrum: the real measured spectrum beside the lab's, as two separate comparisons. **(a) The ion the lab models** (H I, He II, … Fe XXVI: one electron round a nucleus of charge Z): the lab's lines (top row) against NIST's lines for that ion (bottom row), with a line-by-line check of the first line of each series; they should nearly coincide, the rest is fine structure and relativity, which the lab leaves out, so this checks the lab. **(b) The neutral atom of the element** (Fe I, Ne I, …), what "the spectrum of iron" means: it has Z electrons and the lab has one, so its lines are not expected to match; this shows the lab's limit. If NIST has no data (the ions of Z 111–118, the neutral atoms of Fm and heavier) the card says so. Data: NIST Atomic Spectra Database lines, vacuum wavelengths, fetched 2026-10-04 and stored in `data/asd` (a selection: the ion's lines between n ≤ 6; the neutral atom's 30 strongest observed lines with an intensity in 100–2500 nm); `src/atom/asd.*`; the data tool is `tools/make_asd_data.py` (fetch needs the network, build does not). NIST intensities are qualitative and only mean something within one spectrum, so they are drawn within one spectrum only, and a line with no intensity is drawn short and equal.
- **Relativity (Dirac levels)** setting (new in 0.10.8, under Light spectrum, **off by default**): switched on, the Light spectrum and Real spectrum cards use the exact Dirac energy of a one-electron atom with a point nucleus (the Sommerfeld fine-structure formula; `src/atom/dirac.*`), so a level depends on n and j and the lines split (the split grows with Z; a short list of the levels is shown). The Real spectrum card then compares the split lines with NIST's components one by one (matched by n and j). **Only energies and lines change**: clouds, orbitals, outlines and the radial graph stay non-relativistic, and so do the readouts, the photon of a jump and the quantum beats. No Lamb shift, no other QED and no nuclear size (2s and 2p 1/2 are exactly equal in Dirac theory), so NIST's lines still differ a little. The main energy keeps the reduced mass; the relativistic correction is that of an infinitely heavy nucleus. The setting is saved only when on. Its arithmetic is written without subtracting nearly equal numbers (0.10.8 stage 2) and checked against a double-double reference; the tests never use `long double`, which is not the same size on Windows.
- **Phase colours on the graphics card** (0.13 stage 1, the first of four "GPU update" stages that move repeated work from the processor to the graphics card so the app runs better on old PCs): every cloud dot now carries only the number of its colour (the Wheel's 24 phase colours or the 2 Sign colours) and the graphics card picks the colour in the vertex shader, instead of the processor going over all 48,000 dots once for each colour in each of three depth bands. The picture is meant to be the same pixel for pixel: the dots are drawn in the same order (a counting sort gives the old order) and the shader is a direct port of `atom::phaseRgb` (`phase_wheel.h`). Tested here: the colour of every dot and the draw order against the old scan, in one run, for 4f, 2p and 3d clouds; every shader with glslang (`tests/check_shaders.py`); and old-way against new-way pictures drawn with Mesa's software OpenGL (llvmpipe) by the real renderer (`tests/gl_compare.cpp`, build with `-DPL_GL_TESTS=ON`; largest pixel difference 0 of 255). **Not tested: speed** (no graphics card here): the speed-up is expected, not measured, until the benchmark runs on a real PC. The Performance card has a switch, **GPU colours (Atom Lab cloud dots)**, default On, to compare with the old way; the benchmark's scenes 23 and 24 run scenes 8 and 10 with it Off. This stage stays on OpenGL 3.3; stages 2 to 4 plan OpenGL 4.3 (compute shaders), which needs a graphics chip of about 2013 or newer. Tests: `tests/test_gpu_colours.inc`.
- **Richer theme styles and the Frutiger Aero theme** (0.14 stage 1 of 5, "Themes"). A theme can now have a gradient page, glass panels (see-through, a shine line, a soft shadow under floating windows), glossy buttons / tabs / check boxes / sliders, its own corner roundness and a system font by file name; all as data (`src/app/theme_style.*`, drawn by `src/ui/style_draw.*`). The 11 older themes draw the same pixels as in 0.13 (six screens x 12 versions, `tests/check_theme_pictures.sh`). New light theme **Frutiger Aero**: sky blue, frosted glass, glossy aqua and green, Segoe UI when Windows has it (Microsoft's font, never shipped). Text contrast follows the W3C's WCAG 2.x formula (tests worked by hand). Not yet seen on a real PC; the pictures of the labs keep their colours until stages 3 and 4. Later stages: Y2K futurism, glass / chrome planets, glossy chart, animated backgrounds.
- **Theme effects switch and the end of 0.14** (0.14 stage 3, the last stage: the owner ended 0.14 here). Settings → **Theme effects: On / Off** (saved, default On). Off removes the decoration of Frutiger Aero, Y2K Ice and Y2K Night (clouds, bubbles, grain, halftone dots, grid lines, light streaks, glints, reflections, corner brackets, HUD marks); colours, gloss and fonts stay. The benchmark report says which it was. Next: 0.15, a full rework of the screens to make the app clean (see the roadmap).
- **Aero's real look, Light's accent and two Y2K themes** (0.14 stage 2 of 5). Frutiger Aero is now saturated (aqua and leaf green, jelly buttons, blue glossy title bars, a bright rim and glow, soft clouds and glass bubbles on the page, glossy combo boxes and tooltips). Two new themes, **Y2K Ice** (light) and **Y2K Night** (dark): the cold techno-print look with chrome buttons, fine grain, halftone dots, grid lines, HUD rules, corner brackets and a tiny "SYS" label, window titles in the free Michroma font (SIL OFL, built in). Light's blue text is a little darker so it reads (4.57:1). Style only, no logos or artwork. Pictures of the labs are not themed yet. Not tested: a real PC, a real graphics card, speed.
- **The Orbit Lab's rocks and the chart of nuclides on the graphics card, and the Cloud 2D cost** (0.13 stage 4, the last "GPU update" stage; 0.13 is complete). Measured first (CPU, `tests/timing_stage4.cpp`, `-DPL_TIMING=ON`, plus a whole-frame run with a headless Dear ImGui): the rocks are about 0.7 of 4.1 ms in scene 1 and about 6 of 9.8 ms in scene 6 (the Kepler position is 90 % of it); the rest of scene 1 is the sky and trails, not rocks (put on the ROADMAP); the chart sends 1 MB (flat) to 3.5 MB (landscape) of vertices every frame; Cloud 2D was slower than Cloud 3D because its 48,000-blob list was rebuilt from nothing and regrown about 17 times every frame (fixed: a kept vector and a renderer that takes it without copying; scene 10 from about 3.9 to 2.6 ms here). **Rocks:** a compute shader (`kRocksCS`, `src/render/rock_compute.*`, `src/orbit/belt_layer.*`) places every rock; time and the mean anomaly stay in double precision on the GPU, only a small angle goes to float; Kepler's equation is solved by `orbit::solveKepler` (converged; the old 3-step solver was wrong for e >= 0.8, no app rock is) and its float twin; every rock of scenes 1 and 6 is within 0.003 point of the CPU's, and the physics is checked against Kepler's equation by bisection to ten million years. **Chart:** the squares/blocks are made once into a GPU buffer (`src/atom/chart_mesh.*`, `src/render/mesh.h`, `kChartVS`) and tilt, zoom and pan are uniforms; rebuilt only when the picture size, the theme colours or the height meaning changes; pictures, triangles and the hit test checked against the per-frame way. Switches in the Performance card: **GPU rocks** (Orbit Lab) and **GPU chart cache** (Atom Lab), not saved; the benchmark report prints "Orbit Lab rocks: OpenGL 4.3: GPU rocks on". The speed-ups are EXPECTED, not measured: there was no real GPU or Windows PC.
- **The 90 % outline on the graphics card, and exact orbital normalisation** (0.13 stage 3). Measured first (CPU, `tests/timing_stage3.cpp`, `-DPL_TIMING=ON`): the first use of an outline (the 64³ pass) is about 30 ms: 13 ms for the 262,144 density values and 17 ms for the sort that finds the 90 % level; the norm and the line building are about 1 ms. The Slice frame is 1.86 ms, 1.69 of it density values. Now (OpenGL 4.3, switch "GPU outline" in the Performance card, default On): `kOutlineDensityCS` (a port of `atom::density`) fills the grid, `kOutlineLevelCS` finds the 90 % level in ONE work group by bisection over the float bit patterns (31 passes over the grid, no sort), in `src/render/outline_compute.*`; the CPU does not wait: a job and a fence, each frame it only asks "done?" (`outlineReady`, `SliceContour` in `src/atom/outline.*`), reads the grid back once per orbital (1 MB) and builds the lines with the same code as before; the outline appears a frame or two later (the Slice contour trails the cut by a frame). The radial graph's 90 % mark is pending the same way. The CPU functions (`outlineGridCpu`, `outlineLevelCpu`, `outlinePlaneCpu`, `outlineFromGrid`) are the reference and the 3.3 fallback. **Part B, a physics fix on purpose:** `orbitalNorm` is now exact (closed form: radial (n/2)^(2l+3) 2n (n+l)!/(n-l-1)!, angular sphere integrals of the polynomials squared); it was a midpoint sum that left amplitudes too big by 3e-6 to 4.4e-5; every orbital the app offers (real and complex, Z = 1, 2, 5) now integrates to 1 within 1e-10 by an independent quadrature; the stage 2 tolerances widened for that error are tightened. Tests: `tests/test_outline_norm.inc` (the normalisation, the 90 % level against textbook formulas, the logic with a stand-in GPU), `tests/gl_compare.cpp` (llvmpipe: grids, levels, planes, pipeline, pictures, 3.3 fallback). **Speed not measured** (no GPU here).
- **Superposition and photon jumps on the graphics card** (0.13 stage 2): measured first (CPU, `tests/timing_stage2.cpp`, build with `-DPL_TIMING=ON`): for the jump and the three superpositions the biggest part of a frame is the colour group of every dot (thinning against its fixed random number, the phase of the superposed wave, its colour bin): 5 of 10 ms in the jump scene, 2.2 to 4.6 ms of 4.4 to 6.8 ms in the superposition scenes, against 0.0 to 0.6 ms for a plain orbital. That now runs in a compute shader (`kGroupsCS` in `src/render/shaders.h`, GLSL 430; the class is `src/render/group_compute.*`) on a PC with OpenGL 4.3: each dot's two amplitudes are stored on the GPU (sent once, and only the dots the lab changed after that), each frame the CPU sends the beat phase and the weights and reads back one byte a dot, and the dots are projected, ordered and drawn by the code of stage 1. The processor's functions (`AtomLab::superGroup`, `jumpGroups`) are the reference and the fallback: the app asks for a 4.3 context first and creates 3.3 if it cannot get it; the Performance card says which way runs and has a switch (`PL_NO_COMPUTE=1` in the environment also forces the CPU path). The GPU result may differ from the CPU one only for a dot within float rounding of a threshold; the tests count and bound every such dot. The jump's own building (a chain of random steps) stays on the CPU. Tests: `tests/test_gpu_compute.inc` (the textbook physics check, the logic with a stand-in GPU) and `tests/gl_compare.cpp` (llvmpipe, OpenGL 4.5 and a 3.3 context). **Speed not measured** (no GPU here).
- **Accuracy clean-up** (0.12 stage 2): nuclides with no half-life number are described by what NUBASE2020 says (369 extrapolated, 84 none, 11 only a limit, 6 approximate, 3 p-unst), not "seen only fleetingly"; the one-line text and the decay readout never show an extrapolation or a limit as a half-life; the decay clock times only measured or approximate half-lives (the same rule as the chain), so those nuclei stay as chosen and the readout says why; krypton-78 and xenon-124 are stable as in NUBASE2020 (their old half-lives had no source); 113 superheavy "~" half-lives are NUBASE2020's own measured values and 16 stale ones are corrected (`tools/fix_halflives_0_12.py`, which rewrites `atom_data.inc` from the NUBASE file and is idempotent); the hover, panel line and chain say when NUBASE2020 lists decay modes with no intensity (644 nuclides) so the odds are not complete; the unsourced magic numbers Z = 114 and N = 184 are kept and labelled in the nucleus card. Tests: `tests/test_clean.inc`.
- **Decay chains walk the chart** (0.12 stage 1, in the Chart of nuclides view): the chosen nuclide's chain is drawn as lines from square to square, in the colour of the mode: the likeliest way thick and solid, other measured ways thinner, ways below 0.1 % faint, a way whose odds are not measured labelled with a ?; a cross marks a nuclide whose half-life is only a limit, only an extrapolation or not given (the chain stops there and says why; no time is made up). The panel summarises the chain (uranium-238: 14 steps along the likeliest path to lead-206, which NUBASE2020 lists as only observationally stable) and can **walk** it, one step per press or automatically (one step per 1.6 s of screen time): each step says what happened, uses the lab's real random draws (a lifetime from the exponential law, a branch from the listed odds) and says how many times faster than real time it was shown; the walk is never real time. The listed odds are added up to 1 (also in the other views' decay clock, which before could never take a rare way listed after a certain one). Odds NUBASE2020 marks as not measured are listed by the lab's table with equal shares: the walk uses them only as a marked stand-in. A fourth chart benchmark scene measures the chain drawn on the landscape. Code: `src/atom/decay_chain.*`, drawing in `chart_draw.cpp`, tests in `tests/test_chain.inc`.
- **What the height means, the valley and the island** (0.11 stage 4, the chart view's "Height means" choice): the half-life (stage 3) or the **binding energy per nucleon** (AME2020, a linear scale 0 to 9,000 keV; the peak, nickel-62 in the file, is read from the table), 1,008 estimated values flat and pale (never a height). Switching it changes nothing else. The chart names the **valley of stability** (measured), marks the heaviest known nuclides (Z = 118) and says the **island of stability** is a prediction, not seen and not drawn: no source for it could be read, so no region and no predicted number appear. A third benchmark scene measures the binding landscape.
- **Chart of nuclides as a landscape** (0.11 stage 3): the view's Tilt slider turns the flat chart into an isometric landscape; a block's height is the log of the half-life (a chosen scale, 10^-23 s to 10^32 s, stated in the legend), all stable nuclides stand at one fixed height above the top, and a nuclide with no measured half-life or only an extrapolation has no height (flat, hollow or pale). The wheel zooms about the pointer, a drag pans when zoomed in; hover and click pick the block drawn on top. Tilt 0 is exactly the flat chart. A second benchmark scene measures it.
- **Chart of nuclides** (0.11 stage 2, a view in the Atom Lab's View list): one square for each of the 3,557 nuclides the lab knows, neutrons across and protons up, coloured by how it decays, with a legend in words, the magic-number lines (2, 8, 20, 28, 50, 82, 126), the chosen nuclide marked, a tooltip with the half-life, the decay modes and odds, the spin and whether a charge radius and a quadrupole moment were measured, click to choose a nuclide (the other views then show it) and the arrow keys to move one proton or neutron. Spins and the kind of half-life come from NUBASE2020 (data/nubase). While the chart is shown the chosen nucleus does not decay by itself. No heights or 3D yet (stages 3 and 4). A benchmark scene measures it.
- **Auto photons** (0.11 stage 1): with Auto photons on, when the atom cannot jump at all (a complex orientation, m = ±1 to ±3, or a superposition) the lab now says why, once per reason, with the same words as the Emit and Absorb buttons; Auto stays on and resumes by itself when you pick a real orientation. The lab still does not do photon jumps from complex states or superpositions (it does not model light's polarisation).
- **Nuclear size and shape (measured)** card (new in 0.10.9 stages 1 and 2, always on, no setting). **Size (stage 1):** every nucleus with a measured **rms charge radius** in the IAEA Nuclear Data Section's table (908 isotopes of 90 elements, `data/radii`) is drawn at that size, as a uniform ball of radius sqrt(5/3) x the rms (an assumption, stated in the app), next to the usual rule's (1.2 A^(1/3) fm) with the difference as a number; where there is no value (and for the 48 isotopes with only a preliminary one, and for hydrogen-1) the old rule stays and the card says "no measured radius for this isotope; size from the usual rule". **Shape (stage 2):** a nucleus's shape comes from a **measured electric quadrupole moment** where one can be used (the IAEA recommended values, `data/quadrupole`: 598 ground-state rows; 498 of the slider's 4,252 isotopes), else from the lab's **B(E2)** table (11 isotopes), else from an **estimate**; the card and the "shape:" readout line name which of the three, say whether the nucleus is prolate (stretched) or oblate (flattened), and explain that the table's spectroscopic moment is converted to the intrinsic one with the strong-coupling relation K = I (a model assumption, not a measurement) and that a nucleus of spin 0 or 1/2 (every even-even nucleus) has no spectroscopic moment whatever its shape, so a missing moment never means round. A deformed nucleus keeps its volume and still gives the measured charge radius. It is a static size and shape only: not a matter radius, so no halo nuclei yet (they wait for data the owner downloads; see `ROADMAP.md`).
- **Lifetimes and jumps (NIST)** card (new in 0.10.8 stage 3, under Real spectrum): how long the state on screen lives (every j of its shell) and how likely each jump is, **from NIST ASD's transition probabilities A** (the lab only adds them: lifetime = 1/ΣA over every allowed jump down to a lower n, branching share = A/ΣA), each with the accuracy rating ASD gives it (AAA = at most 0.3 % … E = more than 50 %). "Check a jump" asks about any two shells: allowed (with A, rating and share) or not allowed, with the rule that forbids it named (parity, l, j) and, where ASD lists it as forbidden (M1, E2 …), ASD's rate under its own type. Where ASD has no A for an allowed jump the card says so and shows no number (nothing is calculated); a state with a partial set gets only an upper limit. 2s is metastable: no allowed way down, no lifetime number, and the card says a real 2s state still decays by two photons, which ASD does not list and the lab does not model. Jumps between levels of one n are not counted. Hydrogen: complete for 34 of its 35 states n = 2–6.
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
| `src/atom/asd.*`, `asd_data.inc`, `data/asd/` | The real NIST ASD lines (generated tables, the data files and README with the query), the lab-against-ion comparison and the card's words; no drawing |
| `tools/make_asd_data.py` | Fetches the ASD lines (needs `physics.nist.gov`) and builds `asd_data.inc` |
| `src/atom/radii.*`, `radii_data.inc`, `data/radii/` | The measured nuclear charge radii (generated table, the CSV as fetched and its README with the URL, date and credit), the drawn size, the solver for the nucleon cloud's start radius and the card's words; no drawing |
| `tools/make_radii_data.py` | Fetches the IAEA charge-radii file (needs `www-nds.iaea.org`, the one static file) and builds `radii_data.inc` |
| `src/app/theme_style.*`, `theme_types.h` | (0.14 stage 1) The data and maths of theme styles: gradients, glass, gloss colours, the WCAG contrast formula, the theme list, theme fonts. No graphics, tested |
| `src/ui/style_draw.*` | (0.14 stages 1 and 2) Draws them: page and its textures, panels, title bars, cards, jelly / chrome buttons, check boxes, sliders, combo boxes, tooltips, brackets and HUD rules. Does nothing for plain themes, so old themes draw as before |
| `src/atom/cloud_dots.*` | (0.13 stage 1) How the cloud dots are put together for drawing: the colour groups, the three depth bands, the draw order made by the old scan or by a counting sort, the blobs of both ways; no graphics calls. The GLSL (phase colours, and from stage 2 the compute kernel) is in `src/render/shaders.h` (all shader sources, checked by `tests/check_shaders.py`: 330 core and 430 core); pictures old way vs new way: `tests/gl_compare.cpp` |
| `src/atom/quadrupole.*`, `quadrupole_data.inc`, `data/quadrupole/` | The measured quadrupole moments (generated table, the CSV as fetched and its README with the URL, date, checksum, what the second column is and what is not used), the spectroscopic-to-intrinsic conversion, the volume-keeping body, `nucShape` (the three sources) and the card's words; no drawing |
| `tools/make_quadrupole_data.py` | Fetches the IAEA quadrupole-moment file (needs `www-nds.iaea.org`, the one static file), checks that its second column is the mass number and builds `quadrupole_data.inc` |
| `src/atom/radial_graph.*` | The radial probability graph's numbers (curve, peak, average distance, 90 % reach, axis step and the card's words); no drawing |
| `src/atom/atom_learn.*` | The Atom Lab's guide and panel texts (the web's, word for word) |
| `src/atom/atom_view.*` | Drawing the Atom Lab picture (instanced clouds, glows and gluon fields, a soft layer) |
| `src/render/rock_compute.*`, `mesh.h` | (0.13 stage 4) The OpenGL 4.3 compute object that places the belt rocks, and the retained-mesh types of the chart of nuclides |
| `src/orbit/belt_layer.*` | (0.13 stage 4) The belts' rocks on the Orbit Lab picture: the CPU way or the GPU way, no Dear ImGui |
| `src/atom/chart_mesh.*` | (0.13 stage 4) The chart's tiles and blocks as a retained mesh, and the CPU twin of its vertex shader |
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

`tests/test_atom.cpp` does the same for Atom Lab (about 21,000 checks; the decay-chain tests of 0.12 are in `tests/test_chain.inc`, which `test_atom.cpp` includes, and check against textbook series, conservation rules and statistics, not against the app's own earlier output). `tests/reference/make_atom_reference.js`
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
