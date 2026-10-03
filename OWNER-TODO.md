# Owner to-do (things only the owner can do)

## Try on the PC
- [x] Benchmark input lock: wheel, drag, click, Space and F11 do nothing while it runs; only Cancel and Esc work.
- [x] Milky Way band and star mass labels.
- [x] Quarks on light themes.
- [x] The 90% outline: Cloud side view, Slice on a dark theme, the 4f orbital.
- [x] The theme picker at Extra large text.
- [x] Planet physics Real: jump to 1800 and 2100, play at the fastest speed.

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
