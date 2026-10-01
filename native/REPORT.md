# Native Physics Lab 0.7 — report

0.7 puts the frames-per-second counter in the Atom Lab (drawn in one place for both labs), makes the window remember
itself, gives the exe an icon and a clear start-up error, and looks at drawing the Atom Lab labels in depth order (not
done: estimate below). Done in four stages, each committed to `native` ("native 0.7 stage N"). `index.html` was not
touched (sha256 starts e5501387ae64). The 0.6 report is in the git history (`git show e339a68:native/REPORT.md`).

## What works (checked)
All three test programs pass on Linux:
- `app_tests`: 926 checks (was 888; new: the fps text, the window settings and the off-screen fallback);
- `atom_tests`: 17,818 checks; `orbit_tests`: 1,369 checks (both unchanged).
Screens were checked on Linux (Xvfb, software OpenGL) with real mouse clicks (xdotool).

**Part 1: fps counter.** It was only in the Orbit Lab, drawn by the Orbit view as bare text; main.cpp never gave the number to the Atom view.
- Now `ui/fps_counter.cpp` draws it, called once from main.cpp after whichever lab was drawn; the Orbit view's copy is removed
  (so a third lab gets it too). Look as the web's CSS: 9 px from the top, 12 px from the right, 11 px bold monospace, muted colour,
  the card colour at 70 % behind it with 2 × 5 px padding and 5 px rounded corners (square in Monochrome). The Orbit Lab's counter
  therefore now has the box too. It follows the Text size. It shows nothing for the first half second, like the web, then "N fps"
  every 0.5 s. The text is the web's `Math.round(...) + ' fps'`; a unit test covers it (rounding halves up, nothing before the first measurement).
- Not drawn in a saved 📷 picture (checked: the saved PNG has no counter while the screen has) and not drawn while Learn or Achievements covers the lab.
- Looked at both labs at 1000 × 640 in Light, Paper, Monochrome, Synthwave, High contrast and Dark; and the Atom Lab at 1400 × 860 and at
  1000 × 640 with Extra large text, with uranium-238 (the longest readout). Nothing overlaps: the counter sits at the top right, the readout's
  first row ends before it (about 540 of 640 px at Extra large) and the ⓘ button is bottom left. I did not have to move it. A very long first readout row
  in a narrower picture could still run under it; the web has the same corner.

**Part 2: the window remembers itself.** The settings file has a new line `window=w,h,x,y,max` (`x,y` are `-` when unknown).
- It is written when the window changes (size and place of the normal window; "maximised" is a separate flag, so un-maximising returns to the old size) and read at start.
- On start: the saved size is raised to at least 800 × 500 and at most the biggest screen; the saved place is dropped (window centred) if the title bar
  is not on any screen now; a bad, missing or half line is ignored like the other settings. The unit tests cover save and load, 13 bad lines, the minimum,
  one and two screens, a second monitor unplugged, a sliver on screen, a title bar above or below the screens.
- F11 turns full screen on and off (README). Full screen is not remembered; maximised is.
- Checked on Linux: size and place come back (900 × 600 at 40, 50); a place on a missing screen opens centred; a tiny saved size opens 800 × 500.
  Not checked: maximised and F11 on a real desktop (the test X server has no window manager, so nothing maximises). The maximised start waits up to 1.5 s for the
  window system, so the old state is not saved over it.

**Part 3: icon and start-up error.**
- Icon: an atom (orange nucleus, three blue orbits, white electrons on a dark rounded square), drawn by `tools/make_icon.py` into
  `assets/physics_lab.ico` (16, 24, 32, 48, 64, 128, 256 px) and into the window icon (`ui/icon_data.inc`). On Windows `assets/physics_lab.rc` puts it in the exe
  (CMake, only on Windows); the window and taskbar icon is also set by SDL from the same picture. **Untested on a real PC**: the GitHub Windows build
  is the test. I checked that the .ico opens with all 7 sizes in Pillow and looked at the picture; the .rc was not compiled here.
- Error message: `main.cpp` already showed a message box (with the SDL error text only). It now says in plain words what is wrong and what to try (update the graphics
  driver, restart, do not use Remote Desktop or a virtual machine), with the technical detail on the last line, for the four start-up failures (window system, OpenGL window,
  OpenGL 3.3 functions, shaders). Looked at the box on Linux with a forced failure (`PL_FAKE_GLFAIL=1`). The Windows version of the box is Windows' own and was not seen.

**Part 4: Atom Lab labels in depth order: not done.** Estimate: the u/d letters (up to 3 per quark, on the 45 nearest nucleons, so about 135) and the
other labels are ImGui text, drawn after the renderer has drawn the whole picture in one go; the quarks themselves go through the soft layer (an offscreen picture drawn
half-size) and the instanced blob draws. To put a letter between two nucleons, the picture would have to be drawn in slices between the letters: each slice is its own renderer pass
(and soft-layer pass) with an ImGui text pass in between, up to a hundred or more passes per frame, or else the letters would have to be drawn by the renderer itself, with a glyph atlas and its own text shader (and Windows fonts, Segoe UI Emoji and so on). Either is a rewrite of the renderer's drawing
order, not a small change, and the extra offscreen passes would certainly cost more than 5 % of the frame time on a big nucleus. So it was left as is. The visible effect is small: a letter can sit over a nucleon that is a little nearer; the letters
fade with depth fog as before.

## Not tested / be aware
- **No Windows PC, no GPU.** Everything above ran on Linux with software OpenGL. The Windows build (with the icon resource) is compiled and unit-tested by GitHub: check the `latest` release.
- Window position and maximised state on Windows (title bar height, scaling above 100 %): the saved place uses SDL's units and the "title bar on a screen" test uses 40 units; not tried with a scaled screen or two monitors.
- F11 and maximise: not run on a real desktop.
- The icon in the taskbar and the exe: not seen. The 0.5 and 0.6 items not tested (Auto theme and motion following Windows, the settings file in %APPDATA%, the Pictures folder for Save image, links opening the browser) are still not tested.
- Speed: not judged (no GPU). The counter draws one box and a few letters.

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
- **Native edits to the web's text (0.6):** the words that name the Black Hole Lab, the Spacetime Lab and the slingshot game are taken out of About, the Units note, the Sagittarius A* card, the Orbit guide and the timeline (list below). The 🕰 history keeps the web's entries word for word, with one added native line above them.
- **History:** "you are here" is on native 0.7, not on the web's 8.6.2. The 🕰 button says "v0.7"
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
