# Native Physics Lab 0.9 — report

0.9 stops the app from freezing while the window is moved or resized, checks and fixes the benchmark's GPU timer, and adds optional stress scenes to the benchmark. Done in three commits ("native 0.9 stage 1, 2, 3").
`index.html` was not touched (sha256 starts e5501387ae64). The 0.8 report is in the git history (`git show dc88bea:native/REPORT.md`).

**What was NOT tested: no Windows PC and no GPU were available.** Everything below that says "checked" ran on Linux (Xvfb, software OpenGL, Mesa llvmpipe). The freeze fix itself, the GPU timer on a real GPU and the stress scenes' real cost are **untested on Windows / on a GPU**. What Alex should check is under "Not tested" below.

## What works (checked)
All three test programs pass on Linux:
- `app_tests`: 1,006 checks (was 973; new: the time step, the live-frame counters, the GPU query ring, the timer verdict, the result text when the timer is not reliable, the stress belts and the stress Atom Lab scene; the old benchmark tests were changed only where the API changed, see Part 2);
- `atom_tests`: 17,818 checks; `orbit_tests`: 1,369 checks (both unchanged).
The GitHub Windows build compiles and runs the same tests: check the `latest` release.

## The numbers Alex measured for 0.8 (real, from his PC, not Linux)
Window 1400 x 860, VSync off, Windows, AMD Radeon RX 9060 XT, driver 26.8.1.260810:

| Scene | fps | 1 % low (fps) |
|---|---|---|
| Orbit Lab (scene 1) | 611.9 | 418.4 |
| Atom nucleus (scene 2) | 1715.9 | 1109.2 |
| Atom Cloud 3D (scene 3) | 936.2 | 697.6 |
| Atom one nucleon (scene 4) | 563.6 | 372.7 |

The 0.8 result printed "GPU time per frame: 0.09 ms" for scenes 1, 2 and 3 and 0.30 ms for scene 4, while the CPU work was 0.5 to 1.7 ms. Part 2 below is about that. The four normal scenes are unchanged in 0.9, so these numbers can be compared with 0.9 runs (the 0.9 timer queries add a little work per frame, see "Not tested").

**Part 1: the freeze while the window is moved or resized.**
- **Cause** (read in the source of the SDL 3.4.16 that is built in, `src/video/windows/SDL_windowsevents.c`): on Windows a drag by the title bar or an edge runs Windows' own loop inside the window call (`WM_ENTERSIZEMOVE` ... `WM_EXITSIZEMOVE`), so the loop in `main.cpp` does not run. SDL sets a timer for that time (`SetTimer`, `USER_TIMER_MINIMUM`, about 10 ms); on each tick it sends `SDL_EVENT_WINDOW_EXPOSED` with `data1 = 1` ("live-resize expose") straight to the event watchers, from the main thread. `SDL_events.h` says: this event "can be redrawn directly from event watchers" and "is guaranteed to be sent from the OS on the main thread and you are expected to redraw your window in response".
- **Chosen fix: `SDL_AddEventWatch` + draw one whole frame from the watcher** (the way SDL documents it). Why this and not the others:
  - A second thread for the simulation and drawing would need the OpenGL context moved or shared, and Dear ImGui is not thread-safe (it would also have to be the only thread to touch it). Two threads is the riskiest option and I did not take it.
  - `SDL_MAIN_USE_CALLBACKS` (SDL calls the app's iterate function from that same timer) also works, but means rewriting `main()` into four callbacks with all its state in a struct; the watcher gives the same result for a much smaller change.
  - The watcher keeps everything (physics, OpenGL, ImGui) on the one main thread, so nothing needs a lock.
- **How:** the body of the main loop became one function, `runFrame(live)`. The normal loop calls `runFrame(false)`. The watcher (`onLiveExpose` in `main.cpp`) calls `runFrame(true)` for `SDL_EVENT_WINDOW_EXPOSED` with `data1 == 1`, only on the main thread (`SDL_IsMainThread()`), and only if no frame is being built (a flag; no frame starts inside another). A live frame reads no events (SDL does not deliver any in the window system's loop; reading events from inside the watcher would re-enter SDL's own event loop), but does everything else: physics, picture, panels, buffer swap, 📷 Save image if it was asked.
- **No jump after the drag:** the time step now comes from `app::stepSeconds` (`app/frame_time.h`): a slow frame counts as at most 0.05 s as before; a gap of more than 1 s (a stall: the window held by the system, the PC asleep) counts as **nothing**, so there is no giant step and nothing to catch up. Live frames and normal frames share the same clock, so after the drag the first normal frame is a normal 16 ms frame. (A frame slower than 1 s is not a slow frame any more: that is a stall. Before, a 1 s frame counted as 0.05 s.)
- **Following the new size:** the window size is read again after the events and in every live frame, so the picture area, the panels (which are laid out from the window size every frame) and the OpenGL viewport follow while you drag. Checked on Linux with a simulated live resize (below): the screenshot after it shows the picture and the panels filling the new size, nothing stretched or black.
- **Benchmark:** a drag or resize during a run cannot give true numbers, so the first live frame **stops** the run (prints "Benchmark stopped: the window was moved or resized") and gives everything back, as Esc does. The benchmark's window already said "do not resize"; it now also says that moving stops the test. 📷 Save image takes the picture of the frame being drawn, live or not: checked after a live resize, the PNG has the new picture size.
- **To see whether it works on Alex's PC:** the Performance card (both labs) has a new grey line, "Window drag / resize: ...": it counts the frames drawn during drags, the number of drags and the longest gap between two of those frames; the tooltip also gives the longest pause before the first one. If the picture still freezes while dragging and the line stays at "no frames drawn", the watcher does not get the events on that PC and the numbers say why. If it counts frames but the picture still looks frozen, the picture is not reaching the screen during the drag.
- **Checked on Linux (simulated):** Linux has no such freeze, so a test hook mimics it. `PL_FAKE_MODAL=2` blocks the main loop for 2 s inside the event reading, and every 10 ms pushes `SDL_EVENT_WINDOW_EXPOSED` with `data1 = 1` with `SDL_PushEvent`, which calls the watchers at once, as SDL's own timer does in Windows; `PL_FAKE_MODAL_RESIZE=1` also changes the window size every time. Result with software OpenGL (frames take about 30-50 ms here): "Live frames: 48 in 1 drags, worst gap 52 ms", biggest physics step 0.05 s (the usual limit), no crash, no frame inside a frame, the picture at the new size. With a drag during a benchmark: the run stopped as described. `PL_LIVE_REPORT=1` prints this line when the app ends; `PL_SAVE_IMAGE=n` presses 📷 at frame n (both only for automatic checks).
- **Unit tests:** `stepSeconds` (normal, slow, stall, zero, negative, not-a-number; a run with a 5 s freeze in the middle gives the same number of physics steps as one without), the live-frame counters (frames, drags, worst gap, lead-in).
- **Known limit:** if Windows blocks the app *before* its move loop starts (the half second or so after pressing the title bar before the window starts to move), nothing can draw then: no event arrives from SDL in that time. That pause is not covered, and the "longest pause before the first live frame" in the tooltip will show it.

**Part 2: the GPU timer.**
- **What the 0.8 query really covered** (read from the code): one `GL_TIME_ELAPSED` query that began just before `glViewport`/`glClear` and ended just after the ImGui draw, so on paper it already contained everything (the picture, the soft layer's offscreen passes, the instanced draws, the ImGui panels), not the buffer swap. I found no part left out. So the cause of "0.09 ms in three different scenes" is **not found**, and cannot be found without the PC. Candidates: (a) the GPU really is that fast and the frame is limited by the CPU (1.6 ms of CPU work against 0.09 ms is plausible, but the same 0.09 ms for scenes of very different content is not); (b) the driver reports only part of the work when the drawing switches between render targets (the soft layer is drawn to its own offscreen picture inside the query), or a clock resolution/idle floor; (c) the query results were collected badly: 0.8 read the ring in slot order, and a frame whose slot was still busy was silently not measured.
- **What 0.9 does** (`ui/benchmark.cpp`, `app/gpu_timer.h`):
  - the query still covers the whole drawing from the first GL call of the frame (clear) to just before the buffer swap: picture with the soft layer and the instanced draws, then ImGui; with a ring of 8 slots (was 4) read oldest first, and every frame that finds no free slot is counted as skipped;
  - each frame now has two measurements: **busy** (`GL_TIME_ELAPSED`) and **span** (two `GL_TIMESTAMP`s, first to last GPU command, waiting included). A span shorter than the busy time inside it is impossible, so that contradiction is flagged;
  - **a self-check before the first scene** (about a second): a known load (many see-through full-picture rectangles, doubled until the clock says at least 2 ms) is drawn 9 times; the median of what `GL_TIME_ELAPSED` says is compared with a clock running up to `glFinish()`. The timer is "reliable" only if it says 60 % to 130 % of the clock. If not, every scene says **"GPU time per frame: not reliable (...)"** and the raw numbers are kept, marked "do not trust them", in the text file;
  - a scene is also "not reliable" if fewer than half of its frames got a result, if the GPU time is more than 1.5 times the frame time, or if span < 0.8 x busy;
  - the result text says what the time covers, and prints the self-check line ("GPU timer check: reliable, self-check with a known load: timer X ms, clock Y ms" or "NOT RELIABLE, ...").
  - the result also prints "GPU span per frame" next to the GPU time (new line).
- **Tests:** the query ring (order, a younger result does not jump the queue, all slots busy, reset), the verdict (agree, far too low as in the 0.09 ms case with both numbers in the text, too high, zero, a load too small, the edges), the scene check, and the result text (a number only when reliable; "not reliable (...)" with the raw values otherwise; "not available" when no result). The old benchmark tests were changed where the API changed: `BenchRun::frame` takes two numbers now and GPU results come with `gpuSample`; the result-text test now also sets "reliable" for its GPU number. No expectation was loosened.
- **Linux run (software OpenGL):** the self-check says "NOT RELIABLE: the timer said 0.5 ms but the clock said 40 ms for the same known load" and the result text shows "not reliable" for all scenes. That is correct (llvmpipe draws on the CPU: its timer measures nearly nothing of that work) and shows the check can tell a wrong timer from a right one. What it says on the RX 9060 XT is **not known**.
- README's benchmark note updated.

**Part 3: stress scenes.** A checkbox "Include stress scenes (2 extra, much heavier)" in the benchmark's first window (off by default; the run is then 60 s instead of 40 s). The four normal scenes are not changed (same set-up code, same order), the stress scenes run after them, so older results stay comparable.
- **5. Stress: Atom Lab, the biggest nucleus:** oganesson-294 (118 protons, 176 neutrons, the largest the app has), Accurate quarks, + Sea detail, Fuzzy nucleons, zoomed 0.6 (about 4 times) closer than the whole nucleus so that every nucleon is drawn large. Same fixed seed, camera still, no glide. I chose the zoom by measuring four values on Linux (software OpenGL; ranking only): whole nucleus 29.5 ms, +0.3 29.9 ms, **+0.6 73.6 ms**, +1.0 59.1 ms per frame (scene 2, uranium, was about 24 ms in the same runs).
- **6. Stress: Orbit Lab, 8 times the rocks:** scene 1's view with 8 times as many rocks in the asteroid belt, the Trojans, the Kuiper belt and the Oort cloud (48,000 instead of 6,000), made by `makeBelts(date, 8)` (the default is 1; a test checks that scale 1 is exactly the old belts and that the first rocks of the big main belt are the normal ones). The rocks do not interact, so the physics is unchanged. About 47 ms per frame on Linux against about 27 ms for scene 1.
- **What these scenes tell, honestly:** I do not know where an RX 9060 XT limits on them. Scene 6 adds mostly **CPU** work (every rock's Kepler position is worked out every frame) and one instanced draw per belt, so it may show the CPU limit before the GPU's. Scene 5 is mostly **fill rate** (big overlapping glowing nucleons), the most likely one to show the GPU limit. There is no way here to scale the load of the Cloud 3D scene: its dot count is the physics' fixed cap of 12,000 (changing it would change the physics and its tests), so I did not touch it. If both stress scenes still run above 300 fps on Alex's PC, the next step is a bigger window/full screen (more pixels), not more scenes.

## Kept from 0.8

**0.8 Part 1: 🕰 history.**
- Order now: the web's intro (unchanged), the heading "Physics Lab (native app)", the native entries newest first (0.9 → 0.1) with "you are here" on the newest,
  then the heading "The web app's history", the native note, the web's 42 entries word for word, and the web's closing note. ▶ buttons are only where they were (Orbit Lab and Atom Lab entries).
- The note above the web group now ends after "...it has the Orbit Lab and the Atom Lab." (the sentence "Its own updates are listed after them." is gone). The texts of the two headings and the note are in `app/history.h`; a test checks the note and that nothing says "listed after".
  The web's entries are still compared word for word with the web's run (that test was not touched).
- Also: the 0.5 native entry said the history was "the web's history, then this list"; it now says "and this list".
- The stale "list below" pointer: the list of the 0.6 text edits is copied (short) under "Part 1 of 0.6" below, so the pointer in "Still different" is right.

**0.8 Part 2: benchmark** (the GPU time lines are changed by 0.9, see Part 2 above). Performance card (both labs) → **Benchmark...** with a one-line note. It opens a small window (what it does, about 40 seconds, Start, Cancel; Esc cancels); nothing runs without the click.
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
- **Not tested in 0.9 (needs Alex's PC):**
  1. **The freeze fix.** Start the Orbit Lab (any speed), make the window smaller than the screen (not full screen, not maximised) and drag it around by the title bar; then drag an edge to resize it. Look at the planets and the fps counter: they must keep moving, the picture must follow the new size, and when you let go nothing may jump. Then open the Performance card: "Window drag / resize: N frames drawn during D drags, longest gap G ms". Please send what it says. If it says "no frames drawn", or the picture still froze, tell Alex (send the line, and whether it froze while holding without moving, or only while moving).
  2. **Holding the title bar without moving** may still pause for a moment (see "Known limit" in Part 1).
  3. **The GPU timer on the RX 9060 XT:** run the benchmark and send the result text. Its "GPU timer check" line says whether the timer agreed with the clock; if it says "NOT RELIABLE" the GPU numbers are marked "not reliable" and the line tells why. If it says reliable, the new GPU time should be near the CPU work or above it for the heavy scenes; the old 0.09 ms for three scenes would show up as "NOT RELIABLE" or as real.
  4. **The stress scenes** on the real GPU (their real cost is not known), and whether the 9 timer self-check frames before the first scene work on that driver.
  5. The timer queries cost a little work per frame (two timestamps next to the old query, 8 slots): a 0.9 run may be a little different from 0.8 for that reason alone, even with the same scenes.
  6. Moving the window while the Orbit Lab's Journeys run was not tried on Windows (checked on Linux that live frames run the same code, including `journeyTick`, since a live frame is a whole frame).
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
- **History:** "you are here" is on native 0.9, not on the web's 8.6.2. The 🕰 button says "v0.9"
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
