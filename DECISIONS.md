# Decisions (made by the owner)

Only the owner's decisions still in force, one or two lines each. Older wording, superseded entries and the session's own choices are in git history
(0.52.0 cut this file to that; the dropped titles are listed in native/REPORT.md of 0.52.0).

## How the work is done
- Three roles (14 Nov 2026): a planning chat leads (architecture, design, what comes next) and writes each job; it writes no code and does not push. An Opus
  session builds the job and leads its Haiku helpers (CLAUDE.md, "Two models", 4 Nov 2026); Haiku helps, and Opus spot-checks what Haiku reports.
- Quiet chat (14 Nov 2026): a working session does not report progress. It writes in chat only when it cannot go on without the owner's answer, and once at
  the end (the outcome first); everything else goes in REPORT.md. Every message costs the owner's usage.
- No time limit for a job; a problem that fails 3 times is skipped and written in REPORT.md (18 Oct 2026).
- Tests in batches (6 Nov 2026): every job runs the quick tests and the UI check it adds or changes; the full UI suite only at a new version X.Y or when the owner asks.
- Versions are named X.Y.Z only; commits are "native X.Y.Z"; no "stage" names (8 Nov 2026). No 1.0 milestone: the app keeps growing (7 Nov 2026).
- No connectors, plugins or extra skills unless a job asks for one (they cost usage). No code signing (it costs money); the README and release text explain the warning.

## Project
- The app (native/) is C++ for Windows and stays C++; Rust only for separate apps, never in this repo (18 Oct 2026). Four labs: Orbit, Atom, Molecule and the
  Black Hole Lab (15 Nov 2026; this replaces "three labs"; the fourth is planned, not built yet).
- The Defense Lab is removed (18 Oct 2026); an old settings file still loads and opens the Orbit Lab.
- There is no other version of the app (3 Nov 2026): no copy, no tests against one, no mentions. The old web page (branch `main`) is deleted (14 Nov 2026; the session may not delete a branch, so the owner does it on GitHub).
- This repo is public and stays public (15 Nov 2026). Papers, preprints, journal scans, saved web pages and downloads of unclear terms live only in the private repo ShelaShelby/physics-lab-sources (same relative paths under `data/`); this repo keeps the extracts the tests read and native/data/SOURCES.md. The old repo was renamed ShelaShelby/physics-lab-archive and made private (it keeps the full history); ShelaShelby/physics-lab is a fresh public repo with one commit and one branch `native` (15 Nov 2026).
- Serious work only, no games (13 Nov 2026). No game parts (14 Nov 2026): achievements, the surprise and the secret unlocking are removed; Journey (a scale tour) stays.

## Orbit Lab
- Real planets start from the JPL Horizons state of 1 January 2026 12:00 TDB; the IAU astronomical unit, 149,597,870,700 m exactly (3 Oct 2026).
- Planet physics Real stays an option (default Simple) until the owner has tried it on the PC (3 Oct 2026). In Real, relativity (1PN) is on by default (5 Nov 2026).
- The Moon in Real mode comes from ELP2000-82B, checked against Horizons (3 Oct 2026).
- "Reset to today" keeps the zoom, the Moons/Dwarfs/Comets/Belts switches and Time speed; it resets the selection and Follow.
- The Milky Way is ESA's Gaia all-sky colour map (5 Nov 2026). Planet maps: NASA/USGS where one exists, Hubble OPAL for Saturn, Uranus and Neptune (the owner
  downloads them), Venus a smooth ball in its measured colour; true colour, no grid, no made-up bands (5 Nov 2026).
- The Orbit Lab opens fitted to the whole Solar System on every start (4 Nov 2026).

## Atom Lab and chart of nuclides
- Electron cloud defaults: style Glow, density High. The whole atom opens at True scale (Shells enlarged is a choice).
- More electrons (6 Nov 2026): Hartree-Fock-based orbitals for every electron, shells as layers with one orbital to pick, only measured (NIST) energies;
  every electron in every view, the Orbit view as shell rings labelled as a picture, not real orbits (0.39.1).
- Relativity: the one-electron cloud has a Cloud model setting (Auto: Dirac from Z = 27; Dirac; Non-relativistic); the spectrum's relativity is on by default
  (Dirac energy plus the Lamb shift of n = 1, 2 from Yerokhin & Shabaev 2015) (11 Nov 2026).
- A photon jump is drawn as the exact one-photon process, slowed down (τ = 0.2 s on screen, cut at τ ln 48,000); placing an electron by hand is a 1.2 s cross-fade (11 Nov 2026).
- Lifetimes and jump rates come only from NIST ASD's A values; where ASD has none the card says so (4 Oct 2026). The nucleus is drawn at its measured size
  (IAEA charge radii) and measured shape where one can be used (IAEA moments, ENSDF B(E2), else FRDM(2012)), and the card says which (4 Oct 2026).
- The CODATA 2022 constants (4 Oct 2026). The real NIST ASD spectrum is shown as two comparisons, the hydrogen-like ion and the neutral atom, never one list (5 Oct 2026).
- Quarks (8 Nov 2026): styles Accurate or Simple; Accurate draws no colour charge and its quarks are clouds from measured form factors. Nucleus looks Density or Nucleons.
- Element jumps (9 Nov 2026): D1 a jump is measured only when both its levels are; D2 30-50 levels with a cloud per atom; D3 the clouds may add 10-20 MB;
  D4 where nobody measured a rate, the app's own GRASP value, marked computed; D5 atoms with no jumps: grey buttons with the reason; D6 the lined-up (M_J = J) shape by default.
- Element jumps are paused after 0.51.3 (14 Nov 2026; replaces D7, "a group of atoms per update"): no further parts are planned; what is done stays.

## Black Hole Lab (15 Nov 2026; the plan is native/PLAN_BLACKHOLE.md)
- A fourth lab. The Orbit Lab's Sagittarius A* dot gets a way to open it.
- The goal: a black hole that looks as realistic as possible, everything deeply researched and traceable. It is shown as a calculated model and the app says
  so; where the real look is not known, the app says that too.
- One black hole with mass and spin controls, and presets for the two imaged ones (Sagittarius A*, M87*) with their measured masses.
- Moving: first turning around the hole and moving closer or farther; later orbiting at speed and falling in. The foundation must not block those.
- Looks as options, like the other labs: "Real" (the closest to reality, with the dot) and "Film" (labelled as the film's choices).
- It must run on low-end graphics cards too, not only the owner's (a Radeon RX 9060 XT); how low is proposed by the research (PLAN_BLACKHOLE.md, section 6).
- The skin (corrected 15 Nov 2026; replaces the earlier "white curved surface" line): the whole picture becomes white paper in place of black space and stars;
  black handwritten-style calculations, the real equations the lab uses, cover it and run across it; near the hole the writing stretches, curls into arcs
  and wraps around it, bent by the same light-path calculation as the real view. It comes after the real view works. No embedding diagram.
- The plan's eight questions (15 Nov 2026), answered as recommended: our own sky behind the hole with one line saying so, and a switch for a black background;
  the Sagittarius A* and M87* presets open with no disc and a sentence about their real faint gas, the disc one click away and labelled, plus a third preset
  "A feeding black hole (model)"; automatic exposure with a slider and the readout "shown N times dimmer than real"; the Film look gets a simple soft glow
  labelled as the app's own; presets set mass and distance only, spin starts at 0 with "spin not measured"; no new minimum graphics card; the radio-picture
  look stays a roadmap idea; build order: the lab, the disc, spin, the looks.

## Molecule Lab
- The Molecule Lab is the 15 measured molecules; the builder is removed (4 Nov 2026). Density shows a DFT electron density at the measured shape (18 Oct 2026).
- Space-filling shows true, overlapping van der Waals spheres; heavier atoms get more cloud dots (7 Nov 2026).

## Tests and precision
- Tests never compare raw floating-point bytes from another compiler; Atom Lab references use double-double arithmetic, never `long double` (4 Oct 2026).
- The GPU (32-bit) may differ from the 64-bit CPU path only within float rounding of a threshold, counted and bounded by the tests (0.13).
- Orbital normalisation is the exact closed form, not a numerical sum (0.13). Belt rocks are Kepler ellipses; anything that grows without limit stays in double (0.13).

## Graphics and speed
- Repeated per-item work runs on the GPU so the app runs on old PCs (target a 2013 Core i7-4770K): OpenGL 4.3 with compute shaders, falling back to 3.3 and the
  CPU paths, which are kept as the reference and as switches in Settings, Performance (5 Oct 2026).

## Look and interface
- The 0.38 look (6 Nov 2026): sharp corners, flat surfaces with a 1 px edge, a chosen option as a soft accent tint, the labs as tabs in the top bar; in every theme.
- The 0.15 layout (5 Oct 2026): the picture fills the window; a slim top bar, floating cards, a toolbar bottom centre, a reading view for pages; body text never
  under 14 px at 100 % scale; desktop-sized controls; every feature reachable. The toolbar is centred on its scene.
- All simulation options live in one Controls panel on the right, opened from a handle on the picture's edge, sliding in and out (6 Oct 2026).
- Panels are nearly solid (opacity found so text passes WCAG 4.5:1 over the worst picture); pages of text are solid; the picture dims behind Settings, About and the guides.
- Science colours (phase wheel, decay modes, elements, planets, spectra) do not follow the theme. The theme picker is a 3-column grid. Inter is the font of the look.
- The choice closest to reality is marked with a dot, and each lab's Controls has a "Most realistic" button (5 Nov 2026).
- The app asks Windows not to power-throttle it; the smallest window is 880 x 560 units.
