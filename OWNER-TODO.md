# Owner to-do (things only the owner can do)

## Try on the PC
- [x] Benchmark input lock: wheel, drag, click, Space and F11 do nothing while it runs; only Cancel and Esc work.
- [x] Milky Way band and star mass labels.
- [x] Quarks on light themes.
- [x] The 90% outline: Cloud side view, Slice on a dark theme, the 4f orbital.
- [x] The theme picker at Extra large text.
- [x] Planet physics Real: jump to 1800 and 2100, play at the fastest speed.

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
