# Physics Lab — Windows app

Physics Lab for Windows, in C++ (C++17, SDL3, Vulkan, Dear ImGui). Version **0.69.0 alpha**. The app draws everything with Vulkan (1.3 or 1.4, or 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2); a PC whose Vulkan cannot start gets a message naming what is needed (OpenGL was removed in 0.69.0). Four labs:
- **Orbit Lab**: the real Solar System from JPL Horizons (Simple or Real physics, relativity, the Moon from ELP2000-82B, eclipse shadows, Earth's air, Saturn's rings from Cassini, drawn per pixel by the graphics card on Vulkan with the processor's triangles as reference).
- **Atom Lab**: any nucleus, down to quarks; one electron (exact, with Dirac's relativity) or every electron of the neutral atoms (Dirac-Hartree-Fock orbitals computed for the app);
  photon jumps along NIST's measured lines: one electron for every element, every electron shown for 16 atoms (Li, Na, K, Rb, Cs, Fr, He, Be, Mg, Ca, Zn, Sr, Cd, Ba, Hg, Ra;
  GRASP2018 clouds per level). The other atoms say why they have no jumps. Atoms in a magnetic field: the Zeeman effect from weak to strong field (Paschen-Back), the split lines
  with their polarisations, Landé g against NIST's measured g. The chart of nuclides (NUBASE2020, AME2020) with decay chains.
- **Molecule Lab**: 15 small molecules in their measured shape, a calculated electron density, vibrations and an infrared spectrum, the diatomics' anharmonic bond that breaks (Morse curve against measured levels and dissociation energies), and the straight molecules' rotation (levels, populations, HCl's rotation-vibration lines).
- **Black Hole Lab**: a black hole, spinning up to 0.99 of the largest spin or not at all, in front of our own sky (Gaia's map and the catalogue stars), as a hovering camera sees it: the exact shadow, the bent sky, every star's images; Sagittarius A* and M87* with their measured mass and distance (spin not measured: they start at 0); a model thin gas disc (Page & Thorne) orbiting with the spin (round the spinning hole the graphics card works out every pixel's light path from the closed formulas, the processor's table as reference and fallback), in true colour, dimmed by a stated exposure, with a third preset "A feeding black hole (model)"; two looks, Real (the model) and Film (the film Interstellar's choices as James et al. 2015 state them, labelled, with the app's own glow from the disc); in Real, the stars at their true brightness beside the disc (black) and the glare an average eye would see (the CIE's glare equation). A calculated model.

App-wide: a units-aware calculator (CODATA 2022), the Data panel (each lab's tables with units and sources, plots, fits, CSV), search (Ctrl+K), themes, the Learn guides,
Journey (a scale tour), Save picture and a Benchmark. About lists every data source (*Where the facts come from*); 🕰 What's new has every version (`src/ui/changelog.cpp`).
The app is in **alpha**: features are still being added. The design of the interface is in `docs/DESIGN-1.0.md`.

The repository is public. `native/data` holds the project's own outputs, small tables with their reference, raw US-government datasets (NIST, NASA/JPL) and, for each test that needs numbers from a paper or saved page, a small extract (`*.extract.txt`). The papers, scans, preprints, saved web pages and downloads of unclear terms themselves are in the private repo `ShelaShelby/physics-lab-sources` (same paths under `data/`); `native/data/SOURCES.md` lists each with its reference, URL, date and sha256, and `native/tools/check_sources.py` checks them against a checkout of that repo.

## Getting the .exe without building

The newest build is always on the GitHub **Release tagged `latest`**
(`https://github.com/ShelaShelby/physics-lab/releases/tag/latest`). Download `PhysicsLab-windows-x64.zip`, unzip it and run `PhysicsLab.exe`.
No installer and no extra DLLs are needed. The zip also holds the licences of the bundled fonts (`font-licences`) and of SDL3 and Dear ImGui (`licences`).
Each run of the **Native Windows build** workflow (Actions tab; it runs on every push to `native` that changes `native/`) also attaches the zip.

### If Windows warns you

The app is **not code-signed** (signing costs money), so Windows may warn when you open it. This is a reputation warning for an app it does not know;
it does **not** mean a virus was found. Either right-click the zip, **Properties**, tick **Unblock**, **OK**, then unzip; or, at
**"Windows protected your PC"** (SmartScreen), click **More info**, then **Run anyway**. The same help is in the text of the `latest` release.

## Building on your own Windows PC

You need **Visual Studio 2022** (Community is free) with *Desktop development with C++* (it includes CMake), and **Git** (CMake downloads SDL3 and Dear ImGui
at fixed versions the first time). In a *Developer PowerShell for VS 2022*, from the repository root:

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release --parallel
ctest --test-dir native/build -C Release --output-on-failure   # the physics tests
native/build/Release/PhysicsLab.exe
```

Or open the `native` folder in Visual Studio (File → Open → Folder) and press F5.

## How it is organised

| Folder or file | What is in it |
| --- | --- |
| `src/orbit/` | The Orbit Lab: units and constants (`units.h`), Solar System data, Kepler, the simulation, belts, moons, spin, sky, rings, the picture (`orbit_view.*`) |
| `src/atom/` | The Atom Lab: elements and nuclides (`atom_core.*`), the simulation (`atom_sim.*`), NIST ASD lines (`asd.*`), Dirac (`dirac*.*`), every electron (`hf.*`), the outer electron (`alkali.*`), level jumps (`levels.*`), the magnetic field (`zeeman.*`, with the exact 3j symbols `threej.*`), nucleus size and shape (`radii.*`, `quadrupole.*`), the chart of nuclides (`chart*.*`, `decay_chain.*`), the picture (`atom_view.*`) |
| `src/molecule/` | The Molecule Lab: molecules (`molecule.*`), density (`density.*`), vibrations (`vibration.*`), rotation (`rotation.*`), the anharmonic bond (`morse.*`), the picture (`molecule_view.*`) |
| `src/analysis/` | The Data panel's tables (`lab_tables.*`), text in and out (`table.*`) and fitting (`fit.*`); no graphics |
| `src/app/` | App-wide logic without graphics: settings file (`prefs.*`), texts and About (`app_text.inc`), themes (`theme_style.*`), calculator (`calc*.*`), benchmark (`bench*.*`), the graphics card's light paths' guard and log (`gpu_guard.*`), the crash-safe helper process that makes the card programs' risky first pipelines and first small runs (`gpu_helper.*`; its Vulkan side `src/render/vk_helper.cpp`) |
| `src/render/` | The Vulkan start and frames (`vk_backend.*`, shaders in `vk/`), 2D renderer (on Vulkan: `vk_draw2d.*`, `renderer2d_vk.cpp`; the card programs on Vulkan: `vk_card.*`; the Black Hole Lab's: `vk_blackhole.*`), shaders (`shaders.h`), the spinning hole's per-pixel shader's steps (`kerr_sky.*`), compute objects, planet maps, the CIE 1931 table (`cie1931.h`) |
| `src/ui/` | The panels and pages: shell, Controls, Settings, Learn tab and facts (`app_tabs.*`), About and history (`app_pages.*`), guides, calculator and Data panels |
| `data/` | The saved sources: data files and papers, most folders with a README (where it came from, when) |
| `tools/` | The scripts that build the `.inc` tables from `data/`, the DHF and GRASP2018 pipelines, `check.sh` |
| `assets/` | Fonts and planet maps built into the .exe |
| `tests/` | Unit tests against independent references, and the picture / UI checks |

All physics is in SI units. The Orbit Lab's Time speed and zoom are in "sim units" (GM of the Sun = 100, 1 year = 2π); `units.h` converts them exactly.

## Tests

`native/tools/check.sh quick` runs the physics and logic programs (`ctest -L quick`): the Orbit Lab against JPL Horizons and Kepler's laws (`orbit_physics`, `jpl_validation`,
`moon_validation`), the Atom Lab against NIST, NUBASE2020, AME2020, the IAEA tables and textbook formulas (`atom_physics`), the Molecule Lab against CCCBDB
(`molecule_logic`, `vibration_logic`, `rotation_logic`, `density_logic`), the calculator against CODATA 2022 (`calc_logic`), the fits against NIST's StRD (`analysis_logic`), and the settings
file, themes and About page (`app_settings`). `check.sh full` adds the 15 `ui_*` tests, one per screen, which start the real app on Linux (Mesa); `ctest -N` lists them.

The `PL_*` environment variables read in `src/main.cpp` are for the automatic checks only (e.g. `PL_EXIT_AFTER_FRAMES=n`, `PL_SCREENSHOT=file.bmp`, `PL_ATOM=1`, `PL_THEME=paper`).
