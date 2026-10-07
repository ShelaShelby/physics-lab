# Owner to-do (things only the owner can do)

## 0.15 stage 5i: choose the look (on your phone or PC)
- [ ] Look at `native/docs/look/` on GitHub (a_1.png ... c_5.png and `look_compare.png`) and tell me: **A, B, C or a mix** (for example "B's surfaces with C's accent").
- [ ] Optional, on the PC: start the app with `PL_LOOK=a` (steps in REPORT.md) and try it live; Settings -> New UI On, theme Dark.
- [ ] Atom Lab, All values, a tall tab (Spectrum): the sheet should end above the bottom toolbar and scroll inside itself.
- [ ] Keep: the Learn / About wheel bug was not reproduced in the cloud; test with a real mouse (see stage 5g below).

## 0.15 stage 5h: please try on the real PC (New UI On)
- [ ] Atom Lab, All values: click the five new tabs (Spectrum, Lifetimes, Nuclear size, Temperature, Radial) for a few atoms and views; scroll a tall one with the real mouse wheel and with Page Up / Page Down; is the text big enough, and are the graph labels (smaller than the other text) readable?
- [ ] The atom and its label should sit clear of the subject card, also with the Controls panel closed; the info text at the top left should be a little bigger.

## 0.15 stage 5g: please try on the real PC (New UI On, and once with it Off)
- [ ] **Scroll with the real mouse wheel** in Learn (a guide and the timeline), About and What's new. The scripted wheel scrolled all of them here (3 sizes, New UI On and Off), so I could not reproduce "cannot scroll at all". If it still fails on your PC, tell me which window, the New UI setting, the theme and the pointer place. Also try Page Down / Page Up / arrows / Home / End, and dragging the scroll bar.
- [ ] **Contents** button in Learn and About (narrow window): open it and choose three sections in a row.
- [ ] **Toolbar position**: it should stay in the same place in the Orbit Lab and in every Atom view; Cloud 3D and Slice have buttons now.
- [ ] **Cloud 3D as the first Atom view**: your saved settings keep the last view you used. To see the new default, use Defaults in the Atom Lab (or delete the settings file).
- [ ] The Controls handle (flat pill), the phase ring beside the card, and the theme picker without Ice, in the themes you use.

## 0.15 stage 3 (the new shell): please look at it on the real PC
- [ ] Settings (last section) → **New UI (preview): On**. Look at the **Orbit Lab**: the picture fills the window; a slim top bar; the date chip in the middle; the toolbar at the bottom (play, speed, show, pick a body, zoom/sizes, 2D/3D, save); the small readout at the bottom right. Click the chip (date, dial, jump list), each toolbar button, the Legend, and pick a body (a card appears at the top right). Is it clean, desktop-sized and readable, picture first?
- [ ] **Text size** on BOTH monitors (and with Windows scale 100 / 125 / 150 %): is Body text (15 units, never under 14 px) big enough? Try Settings → Text size Small ... Extra large, or the menu's A− / A+. The old layout's text is untouched.
- [ ] Glass: panels are nearly solid (93-97 % opaque) as you decided. Does the glass look (edge, shine) still read as glass in Dark, Aero and Y2K Night? Say if you want them more see-through (the contrast rule then needs solid backing for grey text).
- [ ] Keys: **Esc** closes the open panel, **Ctrl+K** opens search (try a planet's name), **Space** plays / pauses.
- [ ] Open About, History, a changelog and a guide: they open in the reading view (centred column, contents list, ✕ and Esc). Is the line length comfortable (it is about 70 characters in Segoe UI; 34 em wide)?
- [ ] Everything not moved yet is behind **Old panel** (top right): check that nothing is missing in either lab. The Atom Lab keeps its bar under the picture and its picture starts under the top bar until stage 5.
- [ ] Not tested here: Windows, a real GPU, Segoe UI, speed. If the app feels slower with New UI On, tell me.
- [ ] Decide for stage 4: the Atom toolbar's Play button, an orbital chip next to the element chip, the data sheet (DESIGN.md section 8 / INVENTORY.md end).

## 0.15 stage 2 (inventory, final spec, small fixes): please check
- [ ] **Read `native/docs/ui-0.15/INVENTORY.md`** (every control and where it goes in your design). Tell me anything missing, in the wrong place, more than 2 clicks away, or a "most used" list that is wrong. Three questions are at its end (Play button in the Atom toolbar, an orbital chip, the data sheet).
- [ ] Read `DESIGN.md` section 3 and decide the glass: nearly solid panels (93-97 % opaque, always readable) or more see-through (about 70 %, muted text needs solid backing). Also section 8 (Body 15 px or 16).
- [ ] **Benchmark effects On vs Off** on your PC (Y2K Night, focused, main monitor, like last time): the page effects are now drawn once. Expected: the extra 0.7 ms per frame with effects On is gone (below 0.05 ms). Tell me the two numbers. (Here, on a software renderer, 300 frames took 13.2 s with the cache and 22.6 s without: that says nothing about your GPU.) Look at Frutiger Aero, Y2K Ice and Y2K Night once: they should look the same as before (a few levels of rounding differ); switch the Theme effects Off and On and resize the window to see the effects redrawn.
- [ ] **Focus / power:** with the benchmark's window on the second monitor, click the other monitor so the app loses focus and watch the frame rate: did the big drop go away? Run the benchmark and click away during it: the report must say "WARNING: … lost focus or was minimised". The report now has a "Windows power throttling" line: tell me what it says.
- [ ] **DPI:** drag the window between two monitors with different Windows scale (and change Windows' scale while the app runs): the text and the panel should follow without a restart. Try making the window small: it should stop at a smallest size (880 x 560 at 100 %).
- [ ] The About page: the quotes and dashes (the "planets' gravity" lines) should show as real characters now, not "&rsquo;".

## 0.15 stage 1 (audit, design system, mockups): pick a layout
- [ ] Look at the mockups in `native/docs/ui-0.15/mockups/` (24 PNGs; start with `A_orbit_dark_1280x800.png`, `B_orbit_dark_1280x800.png`, `C_orbit_dark_1280x800.png` and the `chart` ones) and read the paragraph for each in `mockups/README.md`. **Pick A, B or C** (or a mix, e.g. "A with C's card on click") and say what to change (colours, icons, sizes, what feels wrong).
- [ ] Tell me your screen numbers so the text-size rule fits you: Windows Settings → System → Display: **Scale (%)** and **Resolution**; the screen size in inches; whether the app window is usually maximised. And say which text is "impossibly small" (the Atom Lab side panel, which is 12 px, is my main suspect).
- [ ] Read `native/docs/ui-0.15/AUDIT.md` (Part B table and "the 5 worst problems") and say if anything important is missing or wrong.

## 0.14 stage 3 (final: Theme effects switch): look at all three new looks in the real app on the PC
- [ ] Look at Frutiger Aero, Y2K Ice and Y2K Night one more time on your screen (the stage 2 items below are still open): glossy and "wet" enough? the Y2K themes cold and techno enough? Say what you want changed; 0.14 is finished, so changes would be small fixes in 0.15 or later.
- [ ] Settings → Theme effects: switch Off in each of them. The textures, clouds, bubbles, glints, reflections and corner brackets should go; colours, gloss and fonts stay. Switch On again. Tell me if anything stays that should go, or goes that should stay.
- [ ] Speed: if an old PC feels slower in Aero or the Y2K themes, try Theme effects Off and tell me the difference. Speed was never measured here.
- [ ] 0.15 is the UI rework (ROADMAP): when you are ready, say so and the first task is the design system and mockups. Useful to bring: your screen size, Windows' display scaling (Settings → System → Display → Scale) and a screenshot of the worst screen.

## 0.14 stage 2 (Aero's real look, Light's accent, Y2K Ice and Y2K Night): look at them in the real app on the PC — no Windows, no GPU, no Segoe UI in the cloud session
- [ ] Open 🎨 Theme → Frutiger Aero. Is it now glossy, saturated and "wet" (aqua and lime jelly buttons, blue title bars, clouds and bubbles), or still pale? Check the Learn guide, Settings, the changelog and a combo box (Orbit Lab → View). Say what is off.
- [ ] Open 🎨 Theme → Y2K Ice and Y2K Night. Do they read as the cold blue-white / night-city print look with chrome buttons, grid, halftone, HUD rules and corner brackets, or just "grey with blue"? Is the title font (Michroma) too wide anywhere? Night's buttons are dark gunmetal on purpose (see DECISIONS); say if you want bright silver there.
- [ ] Light: its blue text (the accent) is a little darker now. Look at "Did you know?" and the chosen tab. Say if it feels too dark.
- [ ] Speed was NOT measured: the page textures (grain, halftone, grid, clouds) are drawn as many small shapes every frame. If an old PC feels slower in the Y2K themes, tell me; stage 5 has an off switch for background effects.
- [ ] Not seen here: the benchmark windows and the two "put back" pop-ups in the new themes (only checked by a source scan), and a tooltip in every screen.

## 0.14 stage 1.1 (Aero floating windows were see-through): look again on the PC
- [ ] Frutiger Aero: open Orbit Lab → Learn → "What am I even looking at?", the changelog (v0.14 alpha · history), Settings, 🎨 Theme and the Atom Lab guides and periodic table. Text must read cleanly with nothing showing through, and the window must sit on top of the Solar System card and the tip bar. Say what still feels off (the windows are now solid white-blue; the glass look is the gradient, shine and shadow).
- [ ] Not checked here: the benchmark windows and the two "put back" pop-ups in Aero were painted by the same call but only found by a source scan, not by a picture.

## 0.14 stage 1 (richer theme styles, Frutiger Aero): look at it on the PC — the cloud session has no Windows and no Segoe UI
- [ ] Open the real app, 🎨 Theme → Frutiger Aero. Does it read as Aero (glossy, glassy, bright) or just "light blue"? Say what feels off: the sky gradient, the glass, the button gloss, the green, the rounding, the shadow. The look was only seen as llvmpipe pictures here.
- [ ] Check the font on Windows: with Segoe UI present Aero should use it (the default font on Windows already is Segoe UI unless you picked another in Settings). Tell me if the text looks wrong.
- [ ] Look at all 11 older themes once: they should look exactly as in 0.13 (the cloud session compared pictures of six screens and found 0 different pixels, but only on Mesa's software OpenGL).
- [ ] Not changed on purpose in this stage: the space / cloud / chart picture areas, combo boxes (flat frame), progress bars. Say if any of them looks wrong next to Aero.
- [ ] The W3C's WCAG page could not be opened from the cloud session: if you want, check the contrast formula quoted in About against https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio.

## 0.13 stage 4 (rocks and chart on the GPU, the Cloud 2D fix) and the whole of 0.13: run on the PC — nothing here could be run in the cloud (no real GPU, no Windows)
- [ ] Run the FULL benchmark (tick both extra boxes: stress scenes and the speed-risk scenes, now 27 extra) and send the whole result text. Compare every GPU scene with its Off twin: scene 1 with 31 and 6 with 32 (rocks), 19 with 33 and 20 with 34 (chart cache), 8 with 23, 10 with 24, 11 with 25, 13 with 26, 14 with 27, 15 with 28, 16 with 29, 17 with 30. Your 5 Oct 2026 numbers (stage 2 build): scene 1 2.24 ms, 6 5.34, 10 4.43 (scene 8 2.97), 19 1.34, 20 2.06, 21 2.12, 22 2.14. Expected, NOT measured: scene 6 falls most (the rocks were about 6 of 9.8 ms on the cloud CPU), scene 1 a little (the sky and trails are most of it, see ROADMAP Later), scene 10 falls to about scene 8, the chart scenes by a few tenths of a ms. The report should say "Orbit Lab rocks: OpenGL 4.3: GPU rocks on" and the Performance card the same. If any scene is NOT better, send the text: that is a finding.
- [ ] Look at the real app: Orbit Lab, 3D view, belts on, at the fastest Time speed for a minute or two (about 1 s = 50 years: the main belt, the Trojans' clumps and the Kuiper belt should stay where they are, no jitter or drift of single rocks), and zoomed in a lot (Follow a planet): switch "GPU rocks" Off and On; the rocks should look the same.
- [ ] Look at the real app: Atom Lab, Chart of nuclides, flat and as a landscape (drag the tilt slider, zoom with the wheel, drag to pan, switch the height to binding energy, change theme and window size): it should look and feel as before; the hover tooltip and click on a block must pick the block under the pointer, also on hollow squares. Switch "GPU chart cache" Off and On to compare.
- [ ] Look at the outlines (Cloud 3D and Slice, 4f, 2p z, 3d z2), dragging the Slice cut (the contour trails the cut by a frame on the GPU path), the radial graph, a superposition and a photon jump: all as in the stage 1 to 3 items below.
- [ ] If the Performance card says "OpenGL 3.3: CPU path" or "OpenGL 4.3: CPU path" for the rocks (the shader needs double precision), send the GPU name and driver.

## 0.13 stage 3 (the outline on the GPU, exact normalisation): run on the PC — nothing here could be run in the cloud (no real GPU, no Windows)
- [ ] Run the benchmark with the speed-risk scenes. Compare scene 16 (first use of an outline) with 29 (CPU path): the WORST FRAME and the 1 % low are what matter (0.12 gave 19.90 ms worst, 58.8 fps 1 % low on your RX 9060 XT). Expected, NOT measured: no hitch on 16. Compare 17 with 30 (the cut dragged; 2.02 ms before): expected faster. The Performance card line should say "OpenGL 4.3: GPU outline on" (or a higher number).
- [ ] Look at the real app: Atom Lab, Cloud 3D and Slice with the 90 % outline On for the 4f, 2p z and 3d z2 orbitals and a complex one (m = +1), on a dark and a light theme: the outline should appear within a frame or two of switching it on (or of changing the orbital) and look the same as before. Switch "GPU outline" Off and On to compare. Watch the Slice while dragging the cut: on the GPU path the contour trails the dots by about a frame; tell me if that is visible or annoying.
- [ ] Open the radial probability graph (Atom Lab) and change orbital with the outline Off: the 90 % mark may appear a frame late; nothing should stall.
- [ ] Nothing you see should change from the exact normalisation (amplitudes move by 3e-6 to 4.4e-5): the dots, colours and outlines look the same.

## 0.13 stage 2 (superposition and jumps on the GPU): run on the PC — nothing here could be run in the cloud (no real GPU, no Windows)
- [ ] Start the app and open the Performance card in the Atom Lab: it should say "OpenGL 4.3: GPU superposition on" (or a higher version number such as 4.6). If it says "OpenGL 3.3: CPU path", send the GPU name and driver (the benchmark report has them): that PC cannot run stages 3 and 4 on the GPU either.
- [ ] Run the benchmark with the speed-risk scenes. Compare scene 11 with 25, 13 with 26, 14 with 27 and 15 with 28 (CPU path), and 14 against scene 9 (one plain orbital, about 4 ms). Expected, NOT measured: 13, 14 and 15 drop towards scene 9 (from 8.15, 10.33 and 9.55 ms on your RX 9060 XT) and 11 towards about 5 ms. One thing to watch: the CPU waits for the GPU each frame to read the colour groups back; if the numbers are not better, send the result text.
- [ ] Look at the real app: Atom Lab, Cloud 3D, Superposition On with 2p z + 2p x, 2p x + 2p m+1 and 2p z + 3d z2 (watch it beat), and a photon jump (Emit): switch "GPU superposition and jumps" Off and On, and on a dark and a light theme. The picture should look the same; a rare single dot may differ.
- [x] The orbital normalisation (off by 3e-6 to 1.6e-5 in amplitude) was fixed in 0.13 stage 3, as you decided.

## 0.13 stage 1 (phase colours on the GPU): run on the PC — nothing here could be run in the cloud (no real GPU, no Windows)
- [ ] Run the benchmark with "Atom Lab speed-risk scenes" ticked (15 + 2 more scenes now). Compare scene 8 (GPU colours On) with scene 23 (Off, the 0.12 way), and scene 10 with scene 24; also scene 8 against scene 9. Expected, NOT measured: scene 8 drops towards scene 9 (about 4 ms from 8.08 ms on your RX 9060 XT) and scene 10 from 4.24 ms. If it does not, send the result text: that would be a finding.
- [ ] Look at the real app: Atom Lab, 4f orbital with a complex orientation (m = +3), Phase colours Wheel and Sign, Cloud and Cloud 3D, Classic and Glow, on a dark and a light theme. Switch "GPU colours (Atom Lab cloud dots)" in the Performance card Off and On: the picture should look the same (pixel for pixel with the software OpenGL here; your driver may differ in a rare pixel by one level).
- [ ] If you can: try it on an old PC (the i7-4770K) and tell which Intel/NVIDIA graphics it has; stages 2 to 4 need OpenGL 4.3.

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
- [x] (Resolved in 0.12 stage 2) The lab's 113 "~" half-lives: NUBASE2020 gives each as a plain measured value; the lab's older table had rounded them and flagged them approximate. They now carry NUBASE2020's values.

## 0.11 stage 3 (the landscape): what only the owner can do
- [ ] On the PC, in the Chart of nuclides view, move the Tilt slider from 0 to 100 %: at 0 it must look exactly like the flat chart of stage 2; at 100 % check the ridge of stable nuclides, the shore of hollow squares, the legend (it moves to the emptiest corner; does it cover a block or an axis number at your window sizes?), the pin on the chosen nuclide. My images are a software replay with other fonts.
- [ ] Hover and click blocks that are partly hidden behind taller ones (a short block behind the stable ridge): the one you point at, the one drawn on top, must be the one that is chosen. Try the side faces of a tall block too.
- [ ] Zoom with the wheel (about the pointer) and drag to pan; zoom out to 1 and check the chart is back and centred. Try Reset.
- [ ] Judge the height scale: 12 tiles per scale height, the plateau of stable nuclides. If the relief is too flat or too tall for you, say so; it is one number (`kHeightCells`).
- [ ] Run the new benchmark scene (number 20, "Chart of nuclides, tilted landscape") next to the flat chart scene (19), pointer off the picture, and send both results.
- [ ] Decide whether the view from the neutron-rich side (the proton-rich side hides behind the ridge at full tilt; at tilt 50 % both show) is the right one, or whether the other side should face the viewer.

## 0.11 stage 4 (the second height, the valley and the island): what only the owner can do
- [ ] **The island of stability needs a source from you.** From these sessions every host I tried (Wikipedia, arXiv, INSPIRE, Crossref, OpenAlex, Semantic Scholar, journal sites, JINR, GSI, LBL, OSTI, search engines: about 30 in all) answered "403", the proxy's policy denial. If you want a predicted region drawn, send a review of the predicted magic numbers and region of superheavy nuclei that you can open (text, PDF or a pasted section) and say how it should be drawn; until then the chart only says it is a prediction and draws nothing.
- [ ] In the Chart of nuclides view, set "Height means" to "Binding energy per nucleon", tilt up, and look: the steep rise from hydrogen, the highest block at nickel-62 (ringed and labelled, with the pin), the slow fall towards uranium, the flat pale squares (estimates), the labels "valley of stability (measured)" and "heaviest known". Switch back and forth: nothing but the heights may change, and the picture must not jump (the fit no longer depends on the mode).
- [ ] Look at the notes box (valley and island words) at your window sizes: it sits in a free corner, or is left out in a very small window. Does anything cover a block, a tick number or the legend? My images are a software replay with other fonts; at 640 x 480 the legend still covers part of the island.
- [ ] Judge the binding scale: 28 tiles per scale height, linear from zero. It shows the peak and the fall clearly but most nuclides stand at 75-98 % of the top (heavy ones 80-90 %); a different baseline would exaggerate the fall, so I left it at zero.
- [ ] Run benchmark scene 21 ("... BINDING-ENERGY height ...") with scenes 19 and 20, pointer off the picture, and send the three results. It should cost about the same as scene 20.
- [ ] The lab's older web data holds the numbers N = 184 and Z = 114 (kMagicN / kMagicZ in atom_data.inc, with no source). They are not used for the island. If you know where they came from, say so. (0.12 stage 2: they set the shape estimate of 286 nuclides; the nucleus card now says they have no source.)

## 0.12 stage 1 (decay chains): what only the owner can do
- [ ] Open the Chart of nuclides view with uranium-238 chosen (the default benchmark nuclide) and look at the trail at your usual window size, flat and tilted: the likeliest path should be a clear thick line from the ringed square to lead-206, the faint hairlines the rare ways. Are the mode labels (α, β⁻) readable, or too many? Zoom in with the wheel and look again. My images are a software replay with other fonts.
- [ ] Press "Walk one step" and "Auto-walk" in the panel: each step must say what happened and how many times faster than real time it was shown. Is 1.6 s per step a pace you like (it is one number, `kWalkPaceS`)? Does the walker's disc stay visible on the landscape?
- [ ] Choose bismuth-212 (a branch of 64 % and 36 %), then europium-131 (the chain runs into a half-life that is only an extrapolation: a cross, and the walk stops with the reason) and holmium-140 (odds not measured: labels end in ?, the steps say an equal chance was used as a stand-in). Do the words read plainly?
- [ ] Decide the policy for odds that are not measured: the walk now uses equal shares as a marked stand-in; the alternative is to stop the walk at such a branch.
- [ ] Lead-206 and lead-208, the ends of two classic series, are 'observationally stable' in NUBASE2020 (a half-life limit and an 'A ?' entry). The app says so in those words. Tell me if you want 'stable' alone there.
- [ ] Run benchmark scene 22 ("... with the DECAY CHAIN of uranium-238 drawn and walked ...") next to scenes 19 to 21, pointer off the picture, and send the results. Scenes 19 to 21 no longer draw a chain, so they are comparable with the 0.11 results.
- [ ] The reference series (uranium-238, uranium-235, thorium-232, neptunium-237) were typed from textbook tables, because no online source could be reached. If you have a source you trust (a PDF or text), send it and I will check the lists against it.

## 0.12 stage 2 (the accuracy clean-up): what only the owner can do
- [ ] **Krypton-78 and xenon-124.** The half-lives the lab used to show (9.2e21 years and 1.8e22 years) are in neither NUBASE2020 nor the mendeleev package, which both call the nuclides stable, so I removed them. If you have a source for them (a decay measurement, as a PDF or text), send it and I will restore them with the source named.
- [ ] **Magic numbers 114 and 184.** They are kept and labelled in the nucleus card's Shape source line (286 nuclides' estimated shapes depend on them). Decide: keep labelled, remove (their shape estimates would change), or send a source.
- [ ] On the PC look at: the Atom panel's isotope line and decay readout for fluorine-31 (an extrapolated half-life: no number, "the lab does not time it"), boron-16 (only a limit), europium-162 ("about 10 s"), beryllium-11 ("odds not complete"); the hover of beryllium-11 and of titanium-39 in the Chart of nuclides; the nucleus card's Shape source line of oganesson-294 (it mentions Z = 114 or N = 184).
- [ ] Decide whether a nucleus the lab does not time (369 extrapolated, 11 limit-only, 87 with none) should be allowed to decay by itself with a clearly labelled invented time. I chose no.
- [ ] NUBASE2020's file does not define the question mark after a decay mode. If the AMDC pages (not reachable from the cloud session) say what it means, send the text; the app only says the mode is listed "with no intensity (marked with a question mark)".

## 0.15 stage 4 (the Orbit Lab complete in the New UI): what only the owner can do
- [ ] Settings → New UI (preview) → On, then use the WHOLE Orbit Lab for a while on your PC (Windows, your real screen, Segoe UI). Tell me what is missing, off, or still "mobile-y". Things to try: the play / pause icon at the left of the date chip (and Space); Save picture in the ☰ menu and through Ctrl+K; the Time panel's Live / Set manually and More dates; the subject card (big number, All values); Show (Sky, Far-away things); Pick a body with its filter; the Legend; Settings → Performance and Facts.
- [ ] **The fact line.** Is the place (bottom left, beside the toolbar), the size and the soft shadow right over the real picture? Does it fade too fast or too slow (20 s on screen, 0.5 s fade)? Does hovering stop it, does a click open the whole fact, do the ‹ › arrows show on hover? Facts On or Off by default? Decide: should the next fact overlap the old one while fading (a true cross-fade) instead of out and then in? In the Atom Lab it is NOT shown yet (its bottom left holds its own scale bar and "Less info"): stage 5 will place it.
- [ ] **Sources for the facts.** None of the 27 "Did you know?" facts has a named source in the app (they came with the web version), and the full-fact panel says so. If you want, send sources (or tell me to look for them in a later stage); I did not invent any.
- [ ] Search (Ctrl+K): is the order of results right? Are there things you would type that find nothing? (It lists your recent choices first when the box is empty.)
- [ ] Not tested here: Windows scale 125 / 150 %, Segoe UI widths (the "Lines and names" segment, the card's rows), the speed on your PC, the real graphics card. The look was judged from software-OpenGL pictures only.

## 0.15 stage 5a (the Atom Lab's toolbar, New UI On): what only the owner can do
- [ ] Settings → New UI (preview) → On, go to the Atom Lab and use the bottom toolbar on your PC: Atom, Nucleus (and its ▾: Nucleus / Nucleon), Cloud (and its ▾: Cloud 3D, Slice), Chart, Reset view; Centre and Drag appear when the old bar showed them. Are the little pictures (atom, nucleus, cloud, chart, reset, move) clear without text? Is it right that Nucleus / Cloud act on the click and the ▾ opens the choice?
- [ ] The Atom picture now fills the window. Check on your real screen that the top text lines and the chart's notes box are clear of the top bar, and that nothing important hides behind the toolbar (the chart's "neutrons N →" label sits just under it).
- [ ] The hint line and More / Less info are at the top of the Old panel for now (5b gives them a home). Tell me if you miss the bar's "hint" more than that.
- [ ] Not tested here: Windows, Segoe UI widths, Windows scale 125 / 150 %, speed.

## 0.15 stage 5b (the Atom Lab's element chip, New UI On): what only the owner can do
- [ ] Settings → New UI (preview) → On, Atom Lab: click the chip at the top (it reads e.g. "U · Uranium-238"). Is the table big and clear enough (cells at least 28 u), are the dashed edges (no stable isotope) and the colours right, is the ringed element easy to see? Try the Protons and Neutrons arrows and sliders, "usual", the famous isotopes, Esc, a click outside.
- [ ] The dropdown is tall (table, two sliders, the famous list, which scrolls inside). Would you rather have the famous isotopes in a second column, or fewer controls shown at once?
- [ ] Zoom to the nucleus (the Nucleus button), open its small arrow: Reshuffle nucleons is there (Build another appears after a decay). Is "only in the nucleus view" what you wanted, or should they show in the atom view too?
- [ ] The Old panel button is still there, with the hint line and More / Less info at its top (5d).
- [ ] Not tested here: Windows, Segoe UI widths, Windows scale 125 / 150 %, speed on your PC and graphics card.

## 0.15 stage 5c (the Controls panel, New UI On, both labs): what only the owner can do
- [ ] On your PC at Windows scale 125 % and 150 %: is the Controls panel (22 em of Body) a good width, do the long rows ("Far-away things (zoom out to find them)", "Lines and names") fit, does the Show section still read well? Try an ultrawide window: does a panel that is only 330 px wide look lost at the right edge of a very wide picture (a wider or resizable panel can be planned)?
- [ ] Open and close the panel with the Controls button and its ✕; does the picture feel cramped with it open at 1280 x 720? The toolbar and the readout move left of it: is that fine? (Closed it stays closed next time you start the app.)
- [ ] Far zoom-out in the Orbit Lab (zoom slider to the far left): distance-ring labels are skipped where they would sit under the top bar, the toolbar or the panel. Is that enough, or should they slide to a free place instead?
- [ ] The readout (bottom right) is now text-size on a dark pill. In the light themes and Aero its text is white (the muted colour cannot reach 4.5:1 on a pill that is only 0.6 opaque over a white picture). Do you like it, or do you want a darker pill (0.7) so the softer grey text works?
- [ ] Atom Lab: the Controls panel only holds the View section for now; the Old panel opens left of it (the two together are wide). The rest moves in 5d and 5e.
- [ ] Not tested here: Windows, Segoe UI widths, Windows scale 125 / 150 %, a real GPU, speed.
- [ ] (0.15 stage 5c.1) On the PC, New UI On, in EVERY theme: the handle on the picture's right edge (opens / closes the panel, never hidden behind the toolbar, top bar or a card), the slide (smooth? 180 ms feels right?), and the hover outline / glow round the Legend, the toolbar buttons and the panel's check boxes and buttons (nothing spreading over neighbours). Also at 880 x 560, 1280 x 800, 1920 x 1080 and Windows scale 125 / 150 %.

## 0.15 stages 5d + 5e (Atom view options, marks and photons in the Controls panel, New UI On)
- [ ] With a real mouse on the PC (New UI On, Atom Lab): open the Controls panel in the Orbit, Cloud, Cloud 3D, Slice and Chart views and try every control of Orbital, Cloud, Orbit, Scale and motion, Nucleus, Chart, Marks and Photons; each should do what the same control in the Old panel does. Do the orbital and orientation buttons (rows of 5 and 3) read well?
- [ ] Marks: 90 % outline On and Phase colours Wheel in the Cloud and Slice views; on the Chart (e.g. Uranium-238) Show the chain, Walk one step, Auto-walk, Restart walk.
- [ ] Photons: Emit (from n = 2 or more), Absorb, Auto photons, and Check a jump (2p → 1s, 2s → 1s).
- [ ] The fact line at the bottom left now wraps to two lines (and is shown in the Atom Lab, with the picture's scale bar above it). Readable on your screen? The Cloud options are also shown on the Chart, as in the old panel, where they do nothing: keep or hide?
- [ ] Not tested here: Windows, a real GPU, speed, Segoe UI widths, Windows scale 125 / 150 %.

## 0.15 stage 5f (Atom panel tidy, subject card, data sheet, legend; New UI On)
- [ ] With a real mouse on the PC: the Atom View selector (Atom, Nucleus, Nucleon, Chart, Cloud, Cloud 3D, Slice) and the "Notes about this choice (show)" rows.
- [ ] The subject card (top right), All values (tabs Values, Decay, This view; Esc and the cross close it), More / Less info, in both labs; the Legend chip on the Atom chart (it scrolls inside when tall).
- [ ] Atom Lab: check on the PC that the Atom cloud dots draw (the short software frames here did not show them; not investigated).
- [ ] Atom picture: its info text at the top left (More info) runs under the card on a narrow window: tell me if you want it moved.
- [ ] Old app (New UI Off), not changed: the "Restart walk" row looks cut off in the old 360 u Atom column.
- [ ] Not tested here: Windows, a real GPU, speed, Segoe UI, Windows scale 125 / 150 %.
- [ ] Orbit Lab (found in 5f, older than this stage): click a body in the Pick list in a 1280 x 800 window: does it fly there and select it? The scripted tests `orbit_ui_card` and `orbit_ui_coverage` fail at that size (same failures at the stage 5e commit).
