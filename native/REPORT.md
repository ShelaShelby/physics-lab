# Native Physics Lab 0.8 — report

0.8 puts the native updates on top of the 🕰 history and adds an automatic benchmark. Done in three commits ("native 0.8 stage 1, 2, 3").
`index.html` was not touched (sha256 starts e5501387ae64). The 0.7 report is in the git history (`git show 3c763cb:native/REPORT.md`).

## What works (checked)
All three test programs pass on Linux:
- `app_tests`: 973 checks (was 926; new: the history notes, the benchmark statistics, run, text and restoring);
- `atom_tests`: 17,818 checks; `orbit_tests`: 1,369 checks (both unchanged).
Screens were checked on Linux (Xvfb, software OpenGL) with real mouse clicks (xdotool).

**Part 1: 🕰 history.**
- Order now: the web's intro (unchanged), the heading "Physics Lab (native app)", the native entries newest first (0.8 → 0.1) with "you are here" on the newest,
  then the heading "The web app's history", the native note, the web's 42 entries word for word, and the web's closing note. ▶ buttons are only where they were (Orbit Lab and Atom Lab entries).
- The note above the web group now ends after "...it has the Orbit Lab and the Atom Lab." (the sentence "Its own updates are listed after them." is gone). The texts of the two headings and the note are in `app/history.h`; a test checks the note and that nothing says "listed after".
  The web's entries are still compared word for word with the web's run (that test was not touched).
- Also: the 0.5 native entry said the history was "the web's history, then this list"; it now says "and this list".
- The stale "list below" pointer: the list of the 0.6 text edits is copied (short) under "Part 1 of 0.6" below, so the pointer in "Still different" is right.

**Part 2: benchmark.** Performance card (both labs) → **Benchmark...** with a one-line note. It opens a small window (what it does, about 40 seconds, Start, Cancel; Esc cancels); nothing runs without the click.
- **What is measured:** four scenes, VSync off, at the current window size (the result prints the window and picture size in pixels), 2 s warm-up (not counted) and 8 s counted each (40 s in all). Per scene: frames counted, average frame time and fps, the **1 % low**
  (1000 / the average of the slowest 1 % of the frames, at least one frame), the **worst frame**, the **CPU work per frame** (the same number as the card's "Work per frame": building and submitting the frame, before the buffer swap) and the **GPU time per frame**
  (OpenGL 3.3 `GL_TIME_ELAPSED` query around the whole draw, read a frame or two later through a ring of 4 queries; if the driver gives no result the text says "not available"). The frame time is the time from one frame to the next, including the buffer swap.
- **The scenes** (chosen by measuring a list of 22 candidates, below):
  1. Orbit Lab, heaviest view: 3D, belts and Oort cloud, moons, dwarf planets, comets and far-away things on, full night sky with constellation lines and names, full orbit trails, true scale, fastest time speed (1 s = about 50 years), zoom ×1, fixed camera angle, the Solar System reset to the same day.
  2. Atom Lab, uranium-238 at the Nucleus zoom (the Nucleus button's zoom), Cinematic quarks, + Sea, Fuzzy nucleons.
  3. Atom Lab, Cloud 3D: uranium-238, the 4f orbital (the biggest cloud), all 12,000 dots (filled by running the physics steps first, same every time), fixed camera, no spin.
  4. Atom Lab, one nucleon: uranium-238, Accurate quarks, + Sea, zoomed all the way into the first nucleon.
  The candidate list I measured on this Linux machine (software OpenGL, so only the ranking is of any use, in ms per frame): Orbit zoom ×1 / ×0.06 (default) / 2D / soft sky / no belts / no constellations: 23.0 / 23.3 / 21.5 / 23.4 / 22.2 / 23.0, zoomed in ×30: 12.8 (so zoom ×1, 3D, full stars and trails won);
  uranium nucleus whole / zoom 4 / zoom 5.5: 14.6 / 25.4 / 26.0; whole-nucleus Cinematic / Accurate / Gluons / Classic: 14.6 / 13.5 / 13.7 / 13.8; Cloud 3D 4f for Fe / H / U: 16.8 / 16.5 / 17.6, Cloud 3D 1s 15.8, Cloud (2D) 13.8, Slice 11.8; one nucleon Cinematic / Accurate / hydrogen: 24.9 / 27.5 / 14.2.
  Scene 2 stays at the Nucleus zoom as asked, though a deeper zoom costs more; scene 4 covers the deep zoom.
- **Repeatable:** fixed random seed, fixed camera, no camera glide (the zoom is set directly), the Solar System reset to the same day, no mouse needed. The physics runs as always (fixed 1/240 s steps; on a slow machine the app's usual limit on the time step applies and the simulation then runs slower, as in normal use). A unit test checks that the cloud and nucleus scenes give identical dots and readouts twice.
- **Result:** a table in the window; the same text on the clipboard (SDL; **Copy result** puts it there again) and in `physics-lab-benchmark-DATE-TIME.txt` in the Pictures folder (Downloads if that fails; the path is shown). The text starts with: native version, date and time, GPU (GL_RENDERER), OpenGL version, driver/vendor (GL_VENDOR), window and picture size in pixels,
  full screen or not, the display's refresh rate (if SDL gives it), Text size and theme, whether the GPU timer worked.
- **Giving everything back:** the Solar System, the Orbit view, the Atom Lab and its view, all settings, the lab on screen, VSync, paused, and the open guide windows are copied before and put back after, also when you cancel. While it runs nothing is written to the settings file (checked: a hand-made settings file with Atom Lab, Cloud view, oxygen-16, imperial units, Paper theme,
  a 3D Orbit view and a fast Time speed was byte-for-byte the same after a full run). Checked on screen: after a run the Atom Lab was the same oxygen-16 Cloud view; after Esc the Orbit Lab was back at 1 s = 29 days in the 2D view.
- **Cannot run when it should not:** the button is disabled, with a tooltip that says why, while Learn or Achievements is open, during a Journey and during a 📷 save; if one of these starts during a run, the run stops and gives everything back.
- **Tests:** statistics with known inputs (a 202-frame case with two slow frames, a single frame, 100 frames, unsorted input), the run's timing (3 scenes of 10 s, set-up order, warm-up not counted, a cancelled run), the result text (every header line in order, "not available" instead of guesses, the cancelled line), the file name, and restoring (the Atom Lab and the Solar System are copied, put through the benchmark's scenes and put back exactly).
  A real run of the whole benchmark on Linux (Xvfb) with a click on Start, the result file and the clipboard text were checked; another with Esc.
- **The numbers from Linux mean nothing for real speed:** the graphics there are software OpenGL (Mesa llvmpipe, the CPU draws everything), so "GPU time" is really CPU rasterising and the whole table is about how fast this container's CPU is. For example: 45 / 74 / 59 / 37 fps for the four scenes, 1 % lows 28 / 45 / 44 / 28 fps. Do not compare them with a real PC.
- **Not measured / not tested:** no GPU and no Windows PC here. Not checked on Windows: the timer query on real drivers (some drivers return the GPU time late or not at all; then the text says "not available"), the display refresh rate, full screen, the clipboard and the Pictures folder. The benchmark does not measure start-up, loading, memory or power, only frames. GPU time covers drawing the frame, not the swap (waiting for the screen is part of the frame time).
  The 1 % low uses about 80 slowest-frame samples at 100 fps, so it is rough; compare it only between runs of the same length.

## Part 1 of 0.6 (kept here): the native edits to the web's text
The words that name the Black Hole Lab, the Spacetime Lab and the slingshot game were taken out of: the About card line ("…real orbits, atoms, black holes and a little space game." → "…real orbits and atoms."); the About sources "Black holes" and "Spacetime and real objects" (one of them, GRAVITY Collaboration 2022 for Sagittarius A*, is kept under the other Sagittarius A* source);
the Settings → Units note ("the Black Hole readout"); the Sagittarius A* card ("(see the Spacetime Lab)"); the Orbit guide section "The stars and the far-away things" ("(the Spacetime Lab shows what a black hole like it does)"); and the timeline's "Try the slingshot game" button (the button and its code are gone).
`tests/test_app.cpp` has the list of these 6 "native edits" and compares each text with the web's own text after exactly those edits. The 🕰 history entries (web, word for word) that name the other labs are kept; the note above them says the native app does not have them.

## Not tested / be aware
- **No Windows PC, no GPU.** Everything ran on Linux with software OpenGL. The Windows build is compiled and unit-tested by GitHub: check the `latest` release.
- Not tested from earlier versions (still): Auto theme and motion following Windows, the settings file in %APPDATA%, the Pictures folder for 📷 and now the benchmark file, links opening the browser, F11 and maximise on a real desktop, the icon in the taskbar and on the exe, window place with scaling above 100 %.
- The benchmark's window covers a corner of the picture while it runs (a small progress box); it costs a few draw calls and is the same in every run.

## Still different from the web
- **Not done, estimated in 0.7 (see "Part 4" above):** Atom Lab labels and u/d letters are still drawn on top of the picture,
  not in depth order. All text is drawn by ImGui after the whole picture.
- **Not in the native app, on purpose:**
  - the 🎮 Games tab;
  - the Spacetime Lab section of the Learn tab, and its note "The Black Hole Lab and the Slingshot
    game do not have guides yet.";
  - the "Planned next" card in the history;
  - Point of No Return.
  - So the native app counts 4 achievements, and "Unlock everything" and the reset pop-ups count 4
    where the web counts 5. The texts themselves are the web's, and the tests check them for every
    count.
- **Native edits to the web's text (0.6):** the words that name the Black Hole Lab, the Spacetime Lab and the slingshot game are taken out of About, the Units note, the Sagittarius A* card, the Orbit guide and the timeline (the list is under "Part 1 of 0.6" in this report). The 🕰 history keeps the web's entries word for word; since 0.8 the native updates are listed first, and a native note stands directly above the web's group.
- **History:** "you are here" is on native 0.8, not on the web's 8.6.2. The 🕰 button says "v0.8"
  (the native version) where the web says "v8.6.2".
- **Settings** is a window. On the web it is a page that slides over the lab.
- **No animation:** the theme and language menus and the secret pop-ups appear without the web's
  small grow animation.
- **Saving:** the native app writes one text file. The web uses the browser's storage, so the two
  do not share progress.
- **The facts' order** is shuffled with a different random generator, so it differs from the web's
  run by run, as the web's own order does.
- **The window:** the web page has no window to remember. Full screen with F11 is native only; fullscreen is not remembered (maximised is).
- **The fps counter** is drawn by the app (not a page label): same place, colours and box as the web's, and it follows the Text size.
