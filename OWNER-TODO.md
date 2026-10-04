# Owner to-do (things only the owner can do)

## Try on the PC
- [x] Benchmark input lock: wheel, drag, click, Space and F11 do nothing while it runs; only Cancel and Esc work.
- [x] Milky Way band and star mass labels.
- [x] Quarks on light themes.
- [x] The 90% outline: Cloud side view, Slice on a dark theme, the 4f orbital.
- [x] The theme picker at Extra large text.
- [x] Planet physics Real: jump to 1800 and 2100, play at the fastest speed.

## Look at on the PC (0.10.8 stage 3: lifetimes and jumps) — the drawing could not be compiled or seen in the cloud
- [ ] Atom Lab, under "Real spectrum (NIST)": the new card "Lifetimes and jumps (NIST)". Look at it in every theme and at every text size, for hydrogen (Z = 1), iron (Z = 26), a mid element (Z = 12 or 13, where ASD lacks many rates) and Z = 118 (no ASD data): the explanation, the state lines, the two "Check a jump" rows of buttons (ten shells each) and the result lines must be readable and not run past the panel edge; the glyphs (→, −, ⁻¹, ×10ⁿ, µ) must draw.
- [ ] Try: 2p → 1s (allowed, A about 6.3×10⁸ s⁻¹, share 100 %), 3p → 2s and 3p → 1s (two ways, two shares), 2s → 1s (not allowed: the parity and l rules named, and ASD's M1 rate shown as a different process), 3d → 1s (not allowed), 3s → 2p; and the state 2s on screen (metastable note, no lifetime number). Read the words: do they say plainly what a lifetime and a branching share are?
- [ ] Question (not a defect): the card lists the lifetime of every j of the shell on screen because the lab's orbitals have no j. Fine, or should it show one number per shell?
- [ ] ASD's own typing looks wrong for Cs LV 1s-2s (blank Type = allowed, with A = 7.591e11 /s, though l does not change); the lab ignores it and says so. If you can, report it to NIST.

## 0.10.8 stage 2 (the Dirac arithmetic): nothing new to look at
- [x] Nothing the app shows changed (every text is identical, checked for all 118 elements), so the stage 1 screen checks below are still the only ones. The Windows build is what stage 2 fixed: its result is in REPORT.md.

## Look at on the PC (0.10.8 stage 1: the Relativity / Dirac setting) — the drawing could not be compiled or seen in the cloud
- [ ] Atom Lab, under "Light spectrum": the new setting "Relativity (Dirac levels)" (Off / On, off by default). Switch it on for hydrogen, iron (Z = 26) and a heavy element (Z = 92): the strip should show split lines (more lines, farther apart as Z grows), the note under the picture, the list of Dirac levels, and under "Real spectrum (NIST)" a row of text per series with the lab's lines against NIST's components and the miss with and without Dirac. Look at every theme and at every text size: nothing may overlap or run past the panel edge, and every glyph (−, →, ▼) must draw.
- [ ] Read the words as a reader would: does the one-line explanation say what changes (levels and lines in the spectrum cards) and what does not (pictures, readouts, photon jumps, Lamb shift, nuclear size)? Does the long note under the setting, when on, make clear the pictures did NOT become relativistic?
- [ ] Switch it off again: the cards must look exactly as before. Quit and restart with it on: it should come back on; with it off the settings file should have no "dirac" line.
- [ ] Decide (a question, not a defect): the photon of a jump, the energy readout and the quantum beats still use the non-relativistic levels, because the lab's orbitals have no spin and so no j. Should a later stage give them Dirac values (for example by picking j by statistical weight)? It is not on the roadmap.
- [ ] Not looked up in the cloud session: the Dirac formula is textbook material written from knowledge (physics.nist.gov was the only reachable site). The tests check it against hand arithmetic, its expansion and NIST's measured lines, but the book (Bethe & Salpeter) was not re-read; please check the formula in the About page against it if you can.

## Look at on the PC (0.10.7 stage 2: the real spectrum) — the drawing could not be compiled or seen in the cloud
- [ ] Atom Lab, under "Light spectrum": the new card "Real spectrum (NIST)". Look at it in every theme and at every text size, for hydrogen (Z = 1), helium, iron (Z = 26), a heavy element (Z = 92) and one NIST does not have (Z = 118; Fm I, Z = 100, for the neutral atom): the two rows of each strip, the tick colours, the line-by-line text under (a), and the long notes at the bottom must be readable and not run past the panel's edge.
- [ ] Check that (a) and (b) read as two different things (the titles and notes), and that the words are right to you.
- [ ] Check NIST's terms of use: the session could not read NIST's general statement on copyright and licensing of its data (www.nist.gov is not reachable from it); only the citation request on ASD's own pages was read, and the app and data README follow it. Please read NIST's statement before the app is shared widely.
- [ ] The app grew by about 0.65 MB of C++ tables (the NIST lines): check the Windows build still compiles and the .exe starts at its usual speed (not measured).

## Look at on the PC (0.10.7 stage 1: the radial probability graph) — the drawing could not be compiled or seen in the cloud
- [ ] Atom Lab, a cloud view: the new card "Where the electron is found" (above Photons). Look at it in every theme and at every text size (Settings), for 1s, 2s, 2p, 3d, 4f, and Z = 1 and a large Z: curve, the three marks and their lines, the tick numbers, and the three legend lines must be readable and not overlap or run past the panel's edge.
- [ ] Check it against the cloud: the dots should be thick where the curve is high; the peak line near the densest ring of dots; the readout's "theory" average distance equals the dashed "average distance" line's value.
- [ ] Change the orbital with the 90 % outline Off and watch for a one-off hitch (the graph asks for the outline's numbers once per new orbital; it was not timed).
- [ ] Turn Superposition on (the card should give a one-line explanation, no graph) and press Emit (the card keeps its height and shows "changing orbital" until the landing).
- [ ] Run the speed-risk benchmark scenes again only if you want the deferred items' numbers refreshed; nothing was optimised.

## Nothing new for stage 9
- The Windows build was red after stage 8 because of one test (not the app). Stage 9 fixes the test only; the stage 8 items below still apply.

## Run on the PC (0.10.6 stage 8: the cloud jump) — please do this first
- [ ] Run the benchmark again with "Include Atom Lab speed-risk scenes" ticked (same window size as on 4 Oct) and paste the result. I need scenes **11** (jump held in flight) and **12** (jumps one after another): average and **worst** frame, to compare with before: scene 11 average 29.93 ms (33.4 fps), worst 33.28 ms; scene 12 average 8.06 ms, worst 36.35 ms, 1 % low 32.1 fps; scene 9 (no jump) 3.80 ms.
- [ ] Look at it too: Atom Lab, Cloud 3D, 4f, density Ultra, press Emit: the jump should look as before (the dots swap over 1.2 s) and start without a hitch. If the first moment looks like a small pop, say so.
- [ ] Not changed on purpose (later stages): phase colours (scenes 8 and 10), the outline (16, 17) and the superpositions (13 to 15).

## Run on the PC (0.10.6 stage 7: Atom Lab speed scenes) — the numbers are what the next stage is decided from
- [ ] Performance card → Benchmark…, tick **both** "Include stress scenes" and "Include Atom Lab speed-risk scenes", click Start, leave the window alone (about 3 minutes). Use the same window size as in your earlier benchmark.
- [ ] Paste the whole result text into your message to Claude. If you only look: note the **Worst** column of the scenes numbered 8 to 18 (and the Average). Compare: 8 and 10 (phase colours) with 9 (two colours, same dots); 11 and 12 (jumps) and 13 to 15 (superpositions) with 9; 16 and 17 (outline) with scene 3; 18 (Og-294 nucleus) with scene 2.
- [ ] What a bad number looks like: a worst frame above about 16 ms (a visible hitch at 60 Hz) in any of 8 to 18, or an average far above scene 9. Scenes 12 (jumps one after another) and 16 (first use of an outline) should have a worst frame well above their average: that is the stutter, and it is what is being measured.
- [ ] Nothing else changes in the app. If the benchmark window looks wrong (the new tick box, long scene names wrapping, the result table), say so: that code could not be compiled in the cloud session.

## Try on the PC (0.10.6 stage 6: nuclear mass)
- [ ] Atom Lab, hydrogen with 1 neutron (deuterium): the light spectrum card shows Hα at 656.29 nm as before (the real change is 1.3 pm, far below what the card shows). Nothing else should look different.
- [ ] Optional: the hydrogen-1 atom mass in the Learn texts is still 1.00782503223 u (AME2020's file says 1.007825031898 u); say if you want it updated.

## Try on the PC (0.10.6 stage 5: Atom Lab words)
- [ ] Atom Lab, Cloud view, hydrogen: the line over the picture shows "avg r: sampled …" for the first seconds, then the number with "(an estimate, usually within 10 %)"; the new "model: one electron, non-relativistic …" line under ⓘ More reads well and wraps at Extra large text.
- [ ] Spectrum card: "Visible light, 380 to 780 nm (to scale)"; Learn, Timeline: the Pauli step's button says "Open a neon nucleus with one electron" and opens one electron around neon; the 2013 step reads well and its button opens a hydrogen cloud.
- [ ] Decide: should the timeline's 2013 step get a real source (the paper could not be reached from the cloud session; you could look it up and tell what it measured)?

## Try on the PC (0.10.6 stage 4: outline of complex states, superposition norm)
- [ ] 90% outline On, Cloud 3D, 2p with Orientation "m = +1": the thin outline is a doughnut round the z axis (not a dumbbell). Try 3d "m = +2" and 4f "m = +3" in Cloud, Cloud 3D and Slice too.
- [ ] Superposition: 2p (px) + second orbital 2p "m = +1": the cloud is one steady shape, all dots there, the line over the picture says "amplitudes … the states overlap by 71 %, so the mixture is renormalised"; moving Mix redraws the cloud. 1s + 2p (pz) must look and behave exactly as before. The longer Superposition note reads well at Extra large text.

## Try on the PC (0.10.6 stage 3: energy, Orbit accuracy, Fixed scale)
- [ ] Atom Lab, hydrogen: the line over the picture says E −13.60 eV in the Orbit view and in every Cloud view (it said −13.61); the new grey note under the View buttons (non-relativistic, one electron) reads well and fits at Extra large text.
- [ ] Orbit view, n = 1, Playback speed 30: the electron goes round smoothly on a steady circle (no breathing); speed 5 looks exactly as before. Is it still smooth (no stutter) at speed 30 on your PC?
- [ ] Scale: Fixed, Cloud views: the 4s, 4p, 4d, 4f clouds now fit in the picture. The cloud views are drawn 1.67 times smaller in Fixed than before (the n = 4 size): is that fine?

## Try on the PC (0.10.6 stage 2: Atom Lab constants)
- [ ] Atom Lab, Orbit view, hydrogen: the picture looks the same (the circle is 0.03 % larger); the readout says r 0.53 Å, E −13.61 eV, period 152 as. Try Cloud and Slice too: nothing should look different.
- [ ] About, "Where the facts come from", Atoms and particles: the new CODATA 2022 line reads well (it is long) and the two old CODATA 2018 lines now say the values were read from NIST.
- [ ] Decide: keep the proton radius at the web's rounded 0.84 fm, or use CODATA 2022's 0.84075 fm (it changes the nucleon size by 0.09 %).

## Try on the PC (0.10.5)
- [ ] Planet physics Real: the note under it reads well and is 11 lines or fewer at normal text size, and also at Extra large text.
- [ ] Select a comet and a dwarf planet (Ceres, Pluto): the card says its orbit is a simple one fixed at one date. Select Mars: no "In the habitable zone" line. Select the Sun: the habitable zone, frost line and lifetime say "(rough)".
- [ ] Jump to a date in 1980 and select Voyager 1's tile / run the Journey: the caption says Voyager 1 launched in 1977 and is shown from 1990 on (not "not launched yet").
- [ ] Press the "Venus crosses the Sun, 2012" tile: the date box shows 01:26 (the middle of the transit).
- [ ] Select Jupiter (and Saturn): the Mass line says "gravity 2.5 g at the equator" (Saturn 1.1 g). Select Halley: the card shows its orbit numbers; it says the orbit is a simple one fixed at one date; next closest approach January 2062 (the note says JPL predicts 28 July 2061); the Sun card says 695,700 km.
- [ ] Switch Planet physics to Simple: the note under it lists the worst errors against JPL (Saturn 8.0 million km first) and reads well.
- [ ] Select the Oort cloud: the travel times (about 560 and 56,000 years) fit the distances above them; the Moon's own card no longer compares it with itself.

## Try on the PC (0.10.4)
- [ ] Planet physics Real, select the Moon (Earth's Moon): its card shows the phase and the lit percent, and they match the real Moon of that day (look at the sky, or an almanac). Try today, and a date with a full moon or a new moon.
- [ ] Jump to 1800 and to 2100 with the Moon selected: the Moon is still real and the card still shows a phase; jump before 1800 or after 2100: the card says the real Moon is not used there.
- [ ] Play at the fastest Time speed with the Moon selected: nothing flickers; the Moon is not drawn and its orbit is lit (the card says so), and Earth does not shake.
- [ ] Simple mode: the Moon looks and moves as before (no phase on its card).
- [ ] The Real note under Planet physics (11 lines or fewer at normal text size) reads well, also at Extra large text.

## Try on the PC (0.10.1)
- [ ] Planet physics Real: the "General relativity (1PN)" and "Exaggerate the Sun's wobble (×50)" tick boxes appear only in Real, their tooltips read well (hover), and they are still as you left them after closing and reopening the app. (Mercury's extra turn is only about 0.01 degrees per century: too small to see in the picture; the tests check it, not the eye.)
- [ ] The Sun's wobble: in Real, run time at a fast Time speed and zoom in to the inner system: gold loops of the Sun's path round the Barycentre marker, labelled exaggerated.
- [ ] The Milky Way band at far zoom (about 9,200 and 13,000 AU across), 3D view turned to several angles: one stripe running right across, fading only at the edges.
- [ ] The two secret themes, once unlocked, show their names in the Theme picker (not "???") and the cards still look right.

## Try on the PC (0.10.3: Real planets start from JPL)
- [ ] Planet physics: **Real**. Jump the date to **1800** and to **2100**: the planets should be where Simple has them to the eye (the Real note gives the real errors) and nothing should jump, freeze or look broken.
- [ ] The **fastest Time speed** in Real: smooth, no stalls when the date moves far; **trails at fast speeds** look sensible.
- [ ] The **Journey** in Real mode (it should work as in Simple).
- [ ] The **planet cards** in Real mode: numbers sensible.
- [ ] **Extra large text** and a **narrow window**: the longer Real note under "Planet physics" still reads and scrolls well.
- [ ] The relativity tick box is now **off** by default. If your saved settings still have it on, press **Defaults** once (Defaults also puts everything else back).
- [ ] Then **decide whether Real becomes the default** (it stays an option until you do).

## Try on the PC (0.10.2: phase colours, quantum beats, smooth jumps)
- [ ] **Phase colours:** Atom Lab, Electron card: "Phase colours: Sign / Wheel". Sign must look as before. Wheel on a dark AND a light theme, in Cloud, Cloud 3D and Slice, Classic and Glow; the colour ring (0, π/2, π, 3π/2) top right. Orientation of 2p, 3d, 4f has "m = +1", "m = −1" … (4f up to ±3): the colours go round the z axis m times, opposite ways for + and −. Defaults puts everything back.
- [ ] **Superposition:** 1s + 2p (pz): the cloud's centre sloshes up and down once per 4 s at Playback speed 5; the line under the picture gives the real beat period. 1s + 2s: the cloud breathes. 2s + 2p: nothing moves, and the text says why. **1s + 4f:** try it too (the dipole is zero, so the cloud only changes shape, and the text should say so). Mix slider (0 and 100 % = one still orbital), Playback speed (faster/slower beats), **Space pauses it**.
- [ ] **Smooth photon jumps:** Emit and Absorb in Cloud, Cloud 3D and **Slice**, on a dark and a **light theme**, Classic and Glow dots: the cloud should slide from one shape to the other without any pop or reshuffle; the emitted photon should appear exactly when the cloud has landed, the absorbed one should start the jump exactly when it arrives. **Space in the middle of a jump** should freeze it; changing the orbital or element in the middle should give a clean new cloud. Auto photons for a minute: no pops. With the 90% outline on it should fade out and back in.
- [ ] **Orbit view** (Modern and Bohr 1913): the electron glides between the circles (0.8 s) with the landing flash; Superposition is greyed out with its message. **Settings → Motion → Reduced motion:** jumps should be instant.
- [ ] Is it smooth on your PC at Normal, High and Ultra density (per-frame work during a jump: more dots are handled than in a still cloud)? I could only measure on a cloud machine.

## Still untested on a real PC from earlier versions
- [x] Auto theme and motion following Windows.
- [x] The settings file in %APPDATA%\PhysicsLab\PhysicsLab\settings.txt.
- [x] The Pictures folder for Save image and the benchmark file.
- [x] The icon on the exe and the taskbar.
- [x] F11 and maximise.
- [x] Window position under Windows scaling.
- [x] Links opening the browser.

## Decisions and other tasks
- [x] Download a data file from JPL Horizons for 0.10.3 (done 2026-10-03: nine files, now in `native/data/jpl/`; used by the new `jpl_tests`).
- [ ] Decide the 1.0 style direction: clean and scientific, neon and cinematic, or both. The planning chat recommended both: clean light themes, cinematic dark themes.
- [ ] Decide whether to add more electrons (see DECISIONS.md).
- [ ] The free "Full reset" on the usage page expires on 22 Oct: keep it for when the weekly limit runs out.
- [ ] Decide whether the repository should stay private or become public (this decides which free code-signing options exist).

## 0.10.9 stage 1 (the nucleus at its measured size): what only the owner can do
- [ ] Download, in a normal browser (the IAEA pages and the INDC PDFs are behind a bot challenge no script can pass), the data for the halo stage, and put the file in `native/data/matter_radii/`, then say so in the task. (Stone's quadrupole table is NO LONGER needed from you: stage 2 fetched its CSV by a plain request.)
  - a table of measured matter radii of the light halo isotopes (He-6, He-8, Li-11, Be-11, B-17, C-19 ...), for example in Tanihata, Savajols and Kanungo, Prog. Part. Nucl. Phys. 68 (2013) 215, or Ozawa, Suzuki and Tanihata, Nucl. Phys. A693 (2001) 32 (both named from a search result, not checked): a file you can open.
- [ ] Open `www-nds.iaea.org/radii/` and check what it says about citing the charge-radii file and which edition it is; send it. The app credits Angeli and Marinova, At. Data Nucl. Data Tables 99 (2013) 69, with its volume and page unchecked (data/radii/README.txt).
- [ ] Look at the new card "Nuclear size (measured)" on the PC (under the Atom Lab's settings, above "Lifetimes and jumps"): it was never drawn or seen. Check: helium-4 (measured numbers, "smaller than helium-3", MeV per nucleon), lithium-9 (the lithium list, the Li-11 line), lithium-10 or a heavy isotope (the "no measured radius for this isotope; size from the usual rule" line), beryllium-12 (the preliminary value is named and not used), hydrogen-1 (one proton, the 0.8783 value named and not used); that the long explanation wraps and reads well at large text; the glyphs (± × √ ² ⁻ ·).
- [ ] Look at the nucleus itself on the PC: zoom into helium-3, helium-4, carbon-12 and lead-208. The cloud is scaled to the measured size, so light nuclei are now clearly bigger than before (He-3 by 47 % of the rule's rms, Li-6 by 53 %); check that nucleons overlap acceptably (the closest two are never nearer than 0.68 fm; each is drawn 0.84 fm in radius, as before). The frame-time of the nucleus scenes (benchmark scene 18 and the other nucleus scenes) should be measured again: the size now comes from a table look-up every frame (a handful of comparisons) and the nucleon cloud is built once per isotope.

## 0.10.9 stage 2 (the nucleus's shape from measured quadrupole moments): what only the owner can do
- [ ] Open `www-nds.iaea.org/nuclearmoments/` in a browser and send what it says about (a) how to cite the quadrupole table (the app credits the file and the INDC report named in its rows, indc-nds-0833, and says the citation terms could not be read), (b) what an UNSIGNED value means (41 ground-state values have no sign; the app does not use them for a shape and says the file gives no sign), (c) what a value in brackets or "x or y" means.
- [ ] Decide the limit: the app does not use a measured moment where the strong-coupling conversion (K = I) would need |beta2| above 0.6 (14 isotopes, e.g. Li-7, Li-9, Be-9, B-8, B-10, B-11, Na-28, S-33, K-39, K-40, K-41; the card says so). The 0.6 is the lab's own choice, not from a source. It is kShapeBetaLimit in quadrupole.h; say if you want a different value.
- [ ] The 11 B(E2) values the lab has (Ne-20 ... U-238) come from the web version; their source is not named anywhere in the code, so they cannot be traced. If you know it, send it; otherwise they should be replaced by a fetched table (Raman et al.) in a later stage.
- [ ] Look at the card "Nuclear size and shape (measured)" on the PC (above "Lifetimes and jumps"): holmium-165 (measured moment, prolate), caesium-126 (oblate), lithium-7 (a real moment that is not used, and why), boron-12 (|Q| without a sign), gadolinium-158 (B(E2)), lead-208 (an estimate: doubly magic), tin-120 (even-even: "spin 0 ... cannot say whether it is round"), lithium-10 (no row). It was never drawn or seen. Check the wrapping of the long explanation at large text and the glyphs: beta2 and Q0 are written with subscripts (the panel already uses subscripts) and the line ends with a long credit.
- [ ] Look at the nucleus itself: zoom into holmium-165 (the stretched football, beta2 = +0.32), caesium-126 (flattened), and an even-even rare earth. The readout line "shape: ... (from a measured quadrupole moment / from B(E2) / an estimate)" shows at the Nucleus layer. Every deformed nucleus changed a little: it keeps its volume now, and its protons have the measured quadrupole exactly.
- [ ] The nucleus scenes of the benchmark (scene 18 and the other nucleus scenes) were already on your re-measure list from stage 1. Stage 2 adds NO per-frame work: the shape is solved once per isotope change (about a microsecond) and the readout line asks again only when the Nucleus layer text is built; nothing else needs measuring.

## 0.11 stage 1 (Auto photons say why): what only the owner can do
- [ ] Decide what the lab should do with photon jumps from a complex state (m = ±1, ±2, ±3): the options and a recommendation are in native/REPORT.md ("The complex-state question"). Until you decide, the lab refuses (now with a message from Auto too).
- [ ] On the PC: switch Auto photons on, choose a complex orientation (for example 2p m = +1) and check the message appears once, reads well and is not cut off; then pick px and check Auto starts by itself. The message's layout has never been seen on a screen.

## 0.11 stage 2 (the chart of nuclides): what only the owner can do
- [ ] Open the new view (View: Chart of nuclides) on the PC and look: the whole chart at your usual window size and at a small one, the legend (does it cover any tile, is the text readable and not cut off), the magic lines, the box and cross-hairs round the chosen nuclide, the axis numbers. I replayed the drawing into a picture with my own fonts, never the app's real ones.
- [ ] Hover a few squares (a stable one, an alpha emitter, a fissioning one such as fermium-258, one with a tentative spin, one far from stability): the tooltip must wrap and stay on screen. Click a square, switch to Cloud or Orbit and check it shows that nuclide. Use the arrow keys to walk along a row and a column (hold one down).
- [ ] Run the benchmark scene "Chart of nuclides: the whole chart, 3,557 squares" with the pointer off the picture and send the result (frame time, GPU time).
- [ ] Open www-nds.iaea.org/amdc/ (a browser gets past the bot challenge) and send what it says about citing NUBASE2020; the app credits only the citation line the file itself gives.
- [ ] Decide whether Auto photons waiting silently while the chart is shown is fine (the chart shows no electron) or should say so.
- [ ] The lab's table marks 113 half-lives with "~" (approximate); I could not reproduce that mark from NUBASE2020 (most of those have plain values there). If you know where it came from, say so; the chart's hover repeats the lab's existing wording.
