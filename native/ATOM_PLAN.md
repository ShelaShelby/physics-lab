HISTORICAL plan (native 0.4/0.5). ROADMAP.md is the current plan.
# Atom Lab port plan (native 0.4)

Reference: `archive/web/index.html`, web Atom Lab update 7.7.3. The Atom Lab is `SIMS['Atom Lab']`
(lines 3072–3862) plus its helpers and data (lines 1645–3071): about 320 KB of
JavaScript, the largest part of the web app. `index.html` is not edited.

Stages (each committed and pushed on its own, so Actions builds it):
1. **Physics core + tests**: no drawing.
2. **Drawing + panel**: main view, all controls, play/pause, speed, cards.
3. **Learn and text**: guides, element texts, all checked word for word.
4. **Polish**: Defaults with confirmation, changelog, Performance card, report.

Lab switching: the app gets a lab picker (Orbit Lab / Atom Lab) at the top of the panel.
Orbit Lab code and behaviour stay as they are.

## Units

The web works in atomic units: length a0, energy Hartree, time 24.19 as. Its display
constants are rounded: A0 = 0.529 Å, HARTREE = 27.211 eV, AU_T = 24.19 as.

The native lab keeps SI inside:
- electron positions in m and velocities in m/s;
- cloud dots in m, nucleon centres in m;
- wavelengths in m, energies in J (shown in eV);
- half-lives in s.

It converts with the web's own constants, so every number on screen matches the web.
The Bohr-orbit leapfrog uses the same step (h = dt·speed/Z² atomic time units), written in seconds.

## Data (generated from index.html by `tools/make_atom_data.js`, never retyped)
| Web name | What | Stage |
| --- | --- | --- |
| `NAME`, `SYMBOL` | 118 element names and symbols | 1 |
| `NUC_DATA` → `NUC` | every known nuclide (3,557): stable / unbound / ? / half-life | 1 |
| `DEFAULT_N`, `N_RANGE` | usual isotope per element; neutron slider range | 1 |
| `DECAY_DATA` → `decInfo` | NUBASE2020 decay modes and odds | 1 |
| `BEA_DATA` → `massBinding` | binding energy per nucleon (AME) | 1 |
| `TEMPD` | melting, boiling, flag, density for each element | 1 |
| `CATS`, `CAT`, `ptPos` | periodic-table categories, hues, positions | 1 |
| `FAMOUS` | 20 famous isotopes | 1 (data), 2 (picker) |
| `EVERYDAY` | 56 "where you meet it" texts + fallbacks | 3 |
| `MAGIC_N`, `MAGIC_Z`, `BETA_KNOWN` | nuclear shapes | 1 |
| `LRN_OBJ`, `LRN_LAND`, `LRN_TEN`, `LRN_MASS`, `LRN_TL` | Learn tables, 22-step timeline | 3 |
| changelog | 27 Atom Lab entries + note | 4 |

## Physics core (stage 1, `src/atom/`)
- **Orbitals**:
  - `lagCoef` (Laguerre coefficients); radial part `rad`; the 16 angular polynomials `ANG` (s, p, d, f);
  - `ORB` table for n ≤ 4; `psi`, `psi2`; `fullOrb`; `EXT` (view radius per n).
- **Cloud sampling**: Metropolis `walk` (Gaussian steps, σ = 0.4·n·(1.8 if n = 4)/Z, phase sign
  recorded), burn-in 800, 12,000-dot ring buffer (`CAP`). Random numbers come from one
  generator, so a test can seed the web's `Math.random` and the native one identically and
  compare dot for dot.
- **Selection rules**: `dipoleOK` (numerical angular integral over a 1,500-point Fibonacci
  sphere); `targets` (emit/absorb lists); `photonInfo` (reduced-mass Rydberg `rydM`, series
  names); `specLines` (Lyman–Pfund, up to +20, series limits); `lamColor`.
- **Bohr orbit**: leapfrog with h = dt·speed/Z²; `beginTransition` (instant leap in Orbit,
  cloud morph in the cloud views); `advance` (photon flight 1.6 s, morph 1.2 s, flash,
  boost, auto photons with random waits); `manualJump` messages.
- **Nucleus**:
  - sizes: `nuclearR` (1.2·A^⅓ fm; 0.84 fm for A = 1), `nucR`;
  - shape: `nucBeta` (measured B(E2) or the Casten P-factor), `nucShapeWord`, `nucMeasured`;
  - `buildNucleus` (Woods–Saxon or Gaussian sampling, hard core, neutron skin, deformation, tilt; deuteron pair);
  - `zoomForNucleus` / `zoomForNucleon`.
- **Decay**: `decInfo`, `decMove`, `decWord`, `decShots`, `DEC_SHORT`, `decTick` (random
  lifetime from the exponential law; paused/hold logic; chain of up to 40 steps; log of 6),
  `decLine`, `isoLine`, `isoInfo`, `fmtHalf`, `fmtLife`, `hasStable`.
- **Mass**: `massBinding` (measured or Bethe–Weizsäcker); constants M_U, M_P, M_N, M_E, U_*, U_MEV.
- **Temperature**: `tempState`, `tempLine`, the particle-box model of `tempPicture` (32 atoms,
  solid / liquid / gas rules), `TEMP_K` slider mapping (T_MAX·(v/1000)²).
- **Formatting**: `sci`, `metres`, `fmtLam`, `fmtLen`, `fmtT`, `fmtEV`, `supDigits`, `ionLabel`,
  `nuclideLabel`, `lrnSci`, `lrnSci3`, `lrnLen` (metric/imperial).
- **Readout**: `info` (Bohr 1913 and modern), `info1`, `hint`.

**Tests (stage 1)** compare against `tests/reference/make_atom_reference.js`, which cuts the
web functions out of `index.html` and runs them in Node:
- psi values at fixed points for all 30 orbitals; `lagCoef`; the `dipoleOK` table; `targets`
  for every level;
- `photonInfo` / `specLines` for several Z, N; `rydM`; `nucBeta` for many nuclei;
- `decInfo` / `decLine` / `isoLine` / `isoInfo` / `fmtHalf` texts; `massBinding`; `tempLine`;
- seeded MCMC dots (the same 2,000 walk steps, exact); a seeded `buildNucleus` (exact
  positions); the Bohr orbit after fixed steps;
- `info1` / `info` / `hint` lines for a set of states.

## Controls (stage 2, `src/ui/atom_panel.cpp`)
Cards in the web's order: Nucleus, Decay history, Learn, View & zoom, Electron, Photons, Light spectrum, Temperature.

**Nucleus**
- periodic table: 118 buttons in their rows and columns; category hues; dashed edge = no stable isotope; tooltip with `isoLine`; * / ** markers;
- ⤢ Bigger table (large table in a window, with the category legend and its note);
- ☢/● isotope line;
- element card (tile, name, chips) with the click note;
- facts (melting, boiling, room-temperature state, density, bar, notes);
- Protons (Z) slider with −/+ (label "Z · Name");
- Neutrons slider with −/+ (range `N_RANGE`);
- Famous isotopes picker;
- actions: ☢ Build another (after a fission or an untrackable decay), 🎲 Reshuffle nucleons (A > 3).

**Decay history**: chain list (start nucleus, each step "↓ how, after time", result, total line, or the "No decays yet." variants).

**Learn**: 4 buttons (stage 3).

**View & zoom**
- Scale: Auto / Fixed;
- Zoom (log, 0–7, "×N");
- Quark style: Cinematic / Accurate / Classic (shown when zoom ≥ 4);
- Nucleon detail: Valence / + Gluons / + Sea (zoom ≥ 4);
- Nucleon edge: Fuzzy / Hard balls (zoom ≥ 3);
- ⌕ Atom, ⌕ Nucleus, ⌕ Nucleon, ⌖ Centre (when zoomed or panned);
- ✥ Drag: rotate / move / slide the cut;
- ↺ Reset view (Cloud 3D, or a turned Orbit);
- ⟳ Spin on/off (Cloud 3D).

**Electron**
- View: Orbit / Cloud / Cloud 3D / Slice;
- Orbit style: Modern / Bohr 1913;
- Orbital: 10 shells; Orientation for p (3), d (5), f (7);
- Cut direction, Cut position (Slice);
- Energy level n (1–4) and Playback speed (1–30) (Orbit).

**Photons**: ↓ Emit photon, ↑ Absorb photon, ▶ Auto photons / ⏸ Auto photons: on.

**Light spectrum**: the whole-range log strip and the visible strip (`drawSpectrum`),
series names, ▼ marks for lines seen, range labels, the hydrogen or ion note.

**Temperature**
- slider; state line (K · °C · °F → state); typical atom speed line (imperial: mph);
- particle box; ❄ Cold, 🏠 Room, 🔥 Just melted, 💨 Just boiled;
- "🌡 What is temperature, really?" (collapsible); the note.

**Time**: play/pause (Space), shared with the app.

**Mouse**
- wheel zoom (rate 0.004 per web wheel unit);
- drag by view: Orbit turns the atom, Cloud 3D rotates, Slice slides the cut, Cloud moves;
  Shift, right or middle drag always moves the picture;
- click ⓘ More/Less info; click a nucleon to dive into it.

## Drawing (stage 2, `src/atom/atom_view.cpp`)
- **Orbit (Modern)**: dashed circle, trail (500 points), electron with label, turnable view with x/y/z axes when turned.
- **Orbit (Bohr 1913)**: allowed circles n = 1–4 with "n = k" labels; the electron with a minus sign; landing flash; the nucleus as one ball with + and "nucleus +Z".
- **Cloud**: side view, phase colours (accent / orange), one instanced draw per colour.
- **Cloud 3D**: three depth bands × two phases (instanced), axis gizmo, auto spin.
- **Slice**: dots inside the slab, fading with distance from the cut, cut-plane axes.
- **Nucleus**:
  - a speck at atom scale; Fuzzy protons and neutrons (the K₁ column-density profile, as a radial shader) or Hard balls; flat dots when tiny;
  - jitter, depth order, hit targets for the nucleon dive.
- **Quarks**:
  - Classic: balls with flux tubes, gluon sparks, sea pairs;
  - Accurate and Cinematic: colour charge with gluon exchange every 1.1 s, a gluon field
    of up to about 700 particles drawn instanced, quark halos, the dark backdrop in Cinematic;
  - depth fog and focus; the 45-nucleon limit with fade in and out; the soft layer as one
    low-resolution pass or the same blur look; the u/d letters.
- **Overlays**:
  - the mass budget bar at the Quarks layer; the nuclide label ("Helium-4 (⁴He) · 2p, 2n");
  - the ⓘ More/Less info button; the photon wave packet with its label (X-ray/UV/IR tags);
  - the spectral-lines strip; decay flash, flying particles with labels, and the decay log
    (14 s fade); the scale bar (1-2-5 in Å, pm or fm, "= 10⁻¹⁵ m");
  - readout lines at the top, the hint at the bottom.

## Learn and text (stage 3, `src/ui/atom_learn.cpp`)
- **❓ What am I even looking at?** (`lrnLook`):
  - the element bar (◀ select ▶, neutrons −/+ "usual") and the view chips;
  - 9 sections: the big picture, the nucleus, quarks, the electron, why only one electron, the picture on screen (per view and Bohr style), reading the numbers (live readout), what is to scale.
- **📏 Scale: how empty is an atom?** (`lrnScale`):
  - two tabs: "If the nucleus were big" (5 objects, landmark comparison, 3 fact cards, note) and "Powers of ten" (9 rows, log bars).
- **⚖ Weight: what does an atom weigh?** (`lrnMass`):
  - five tabs: The parts, Missing mass, How many atoms?, Inside a proton, Mass ladder.
- **🕰 Timeline: how we pictured the atom** (`lrnTimeline`, `LRN_TL`):
  - 22 steps; 11 have ▶ buttons that set Z, N, view, orbital and zoom.
- **Element texts**: the `EVERYDAY` card and its category fallbacks; the facts card; the spectrum note; the periodic-table notes; decay log and history wording; manualJump messages; info, hint and temperature lines.
- **Method**: as for Orbit Lab, `tools/make_atom_learn.js` runs the web's own functions in Node for a list of cases (several elements and isotopes, all four views, both orbit styles, both units), and `tests/test_atom.cpp` checks the native text word for word.
- **Units**: metric/imperial follows Settings wherever the web uses `imp()`:
  - `lrnLen`, the Temperature speed line, the facts' 20 °C / 68 °F, `tempLine`;
  - the fixed science units (Å, pm, fm, eV, K, kg, u) stay as on the web.

## Polish (stage 4)
- Defaults for Atom Lab, with the same "are you sure?" question (Orbit Lab's Defaults unchanged).
- The 0.4 changelog entry; the Atom Lab web history shown for reference.
- Performance card on the Atom Lab panel too (frame time, VSync).
- The More/Less info choice remembered between runs (the web saves it).
- `REPORT.md`.

## Known risks
- **Size**: about 320 KB of web code. I port everything, but the stage-2 visuals (quark field,
  soft blur layer) are the hardest to match pixel for pixel.
- **Randomness**: the web uses `Math.random`, so live behaviour differs run to run on both
  versions. The tests use seeded runs on both sides for exact comparison.
