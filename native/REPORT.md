# Native Physics Lab 0.6 — report

0.6 adds the one web feature native lacked (📷 Save image), the Solar System timeline pictures, a clean-up of the
text that named parts the native app does not have, and a check of small and very large windows. It was done in
four stages, each committed to `native` and built by GitHub Actions ("native 0.6 stage N").
`index.html` was not touched (sha256 starts e5501387ae64). The 0.5 report is in the git history
(`git show 122e16e:native/REPORT.md`).

## What works (checked)

All three test programs pass on Linux:
- `app_tests`: 888 checks (was 427; new: file name, PNG writing, native edits);
- `atom_tests`: 17,818 checks (unchanged);
- `orbit_tests`: 1,369 checks (was 1,370: the Sagittarius A* card text is shorter and wraps into one line fewer).

Screens were checked on Linux (Xvfb, software OpenGL) with real mouse clicks sent to the window (xdotool).

**Part 1: 📷 Save image**
- The button is next to Defaults in both labs (and on the next line when the panel is too narrow), with the web's label
  "📷 Save image" and tooltip "Save what you see as a PNG picture". Both texts, "Saved ✓" and "Could not save" are read
  from the web page by the generator, and a test compares them with the web's.
- It saves exactly the picture area, readout and labels included, nothing else (no panels, windows or pop-ups). It draws
  the frame again into an offscreen image (the renderer's shapes, then the lab's text), so it does not depend on what lies over
  the picture. The file has the size of the picture area in pixels (checked: 1040 × 713 and 1040 × 523 for a 1400 × 860 window;
  the pixels are the screen's). The frame-rate counter is left out, as on the web (there it is a separate label, not part of the canvas).
- Monochrome gives a grey picture: checked that all 543,920 pixels of a saved Atom Lab image have R = G = B. Checked: Orbit Lab (Dark, Normal text), Atom Lab (Monochrome, Extra large text); the failure message with Paper at Large text.
- File: `orbit-lab-2026-10-01-183834.png`, or `atom-lab-Z6-N6-…png` for the Atom Lab (the web's name plus the local date and time).
  Folder: Pictures; if the file cannot be written there, Downloads. After saving the button says "Saved ✓" for 1.8 s (the web's
  flash) and "Saved to <full path>" stays under the buttons for 20 s. If saving fails the button says "Could not save"
  and a red line gives the reason and the folders tried (checked with a folder that cannot be written).
- The PNG writer is my own (no library): row filters (none, Sub, Up) and LZ77 with the fixed Huffman code. Checked: valid in a
  separate decoder (Pillow) and by the unit test, which decodes it with a small inflate written from RFC 1951, checks every
  chunk CRC, the zlib check sum and gets the same pixels back for 7 pictures (1 × 1 up to 1040 × 713).
- I looked at the PNGs: the Orbit Lab picture matches the screen (date line, labels, scale bar, axes); the Monochrome Atom Lab one too.

**Part 2: Solar System timeline pictures.** The Orbit Lab timeline did not have them (the Atom Lab's did). Added: all 29 steps now
have the web's small picture, drawn in the same box and layout as the Atom Lab timeline. The SVG painter moved from the Atom guide
into `ui/svg_icon.cpp` and learned what the Orbit icons need: ellipses (with rotate), closed and filled paths, several lines in one
path, dashed lines. The Atom Lab icons are pixel-identical to 0.5 on the screen I compared. (Also fixed on the way: a path in the
Atom timeline with two separate lines had been drawn joined.) The pictures are generated from the web and a test compares all 29 with the web's.

**Part 3: text that named parts the native app does not have.** Done as asked: the list is below, the edits are minimal, the
tests have an explicit "native edits" list (6 edits, 14 uses: one for the Units note, three for About, one for the Sagittarius A* card,
one for the guide in all 8 cases, one for the timeline button), and the dead button and its code are removed. A stray extra change
fails the test (checked by changing one edit).

**Part 4: small and large windows.** Checked 1000 × 640 and 3840 × 2160, at Normal and Extra large, in both labs, plus Settings, About,
the Orbit timeline and the Atom Scale guide at 1000 × 640 Extra large. Found and fixed:
- long button labels were cut off ("Journey: Earth to the galactic centre", "Timeline: how we understood the Solar System", "Weight: what does an atom weigh?" at Extra large): they now wrap, as on the web;
- two grey hint lines and two check boxes in the Orbit panel were cut off: they wrap;
- the Settings window (40 lines tall) was taller than a 640-pixel window, so its title bar and ✕ could not be reached: it is now at most 92 % of the window; the big periodic table the same.
Found and left: at 1000 × 640 with Extra large text the header and tabs take about a third of the height (the Atom Lab's bottom bar
another 170 px), so the picture is small, but nothing overlaps or is lost and the panel scrolls; the drop-down boxes show a shortened
current choice when it is too long (the list itself is complete). At 3840 × 2160 the layout is correct; under Xvfb there is no
Windows DPI scaling, so the text looks small there, on a real 4K screen at 150 % it is bigger.

## Not tested / be aware
- **No Windows PC, no GPU.** Everything above was run on Linux with software OpenGL. The Windows build is compiled and unit-tested by GitHub; check the `latest` release.
- **Save image on Windows:** not run there. Untested: the Pictures folder (it uses SDL's own folder lookup, so a OneDrive-moved Pictures folder should be found), a user name with non-English letters in the path (SDL writes the file with a UTF-8 path), and a screen at 150 % scaling (the image is made at the window's pixel size; the maths is the same as the screen drawing, but I could not run a scaled screen). Saving is synchronous: a 4K picture is encoded in about a second on a slow machine (I did not time it on Windows).
- The 📷 and ✓ symbols: on Linux the 📷 is not in the font and is left out ("Save image"); on Windows Segoe UI Emoji should draw it.
- Not tested: the 0.5 items listed in its report (Auto theme and Auto motion following Windows, the settings file in %APPDATA%, links opening the browser).
- Not tested: speed (no GPU). The save draws the picture once more offscreen, once, when the button is pressed.

## Part 3 in detail: every spot in the native text that names the Black Hole Lab, the Spacetime Lab or the slingshot game
(Found by searching all native text and code for: black hole, Black Hole Lab, spacetime, Spacetime Lab, slingshot, game, Games, space game, Point of No Return, "other lab".)

Changed (the words that name the missing part are taken out, nothing else):
1. **About, the card line:** "…real orbits, atoms, black holes and a little space game." → "…real orbits and atoms."
2. **About, sources "🕳️ Black holes"** (3 sources of the Black Hole Lab): removed.
3. **About, sources "🌀 Spacetime and real objects"** (10 sources of the Spacetime Lab) and the neutron-star note under them: removed. One of the 10, GRAVITY Collaboration 2022 (Sagittarius A*, 4.3 million Suns), is kept and moved under the Sagittarius A* source in "Stars and far-away things", because the Orbit Lab still says Sagittarius A* is 4.3 million Suns.
4. **Settings → Units note:** "…the scale bar, the Black Hole readout, element melting points…" → "…the scale bar, element melting points…".
5. **Sagittarius A* card:** "…about 4.3 million Suns (see the Spacetime Lab)." → "…about 4.3 million Suns."
6. **Orbit guide "What am I even looking at?", section "The stars and the far-away things"** (found by the search; it is in all 8 cases of the guide): " (the Spacetime Lab shows what a black hole like it does)" removed.
7. **Timeline, step 1977–1989 "Voyager's Grand Tour":** the button "Try the slingshot game" is gone, not only its text. The code that drew it as a greyed-out button with "(the Slingshot game is not in the native app yet)" is deleted too.

Looked at and kept (these name the missing parts, but are history, or never shown):
8. **🕰 history, the web's 42 entries, word for word.** These entries name the missing parts: 8.5 Unit switch (Black Hole readout), 8.4.4 (Black Hole Lab 3.3, Spacetime Lab 2.6.1), 8.4.3 (Spacetime Lab guide), 8.1 and 8 (Spacetime Lab), 7.6 and 6.9 (Black Hole Lab Quality setting), 7.5 and 7 (the Games tab, Point of No Return), 5.1 (the ▶ button to "the lab or game"), 4 (Slingshot), 3 (Black Hole Lab); 8.4.1 and 8.4 are about the black hole in the sky, not the lab. Their ▶ buttons for the other labs and the game are not shown in the native app (as in 0.5). Because some entries are present tense ("A new Learn tab sits next to Simulations, Games and Achievements"), one short line was **added** above the list: "Native app: the entries below are the web app's history, word for word. Some of them are about the Black Hole Lab, the Spacetime Lab, the Slingshot game and the Games tab. The native app does not have those…". No web text was changed.
9. **Achievements data:** the card of Point of No Return ("Zoom in on the black hole in the Black Hole Lab as far as you can.") is still in the data table, but the native app never shows it (the count is 4).
10. **Native history 0.5 and 0.1:** 0.5 says Point of No Return is left out because the Black Hole Lab is not in the native app; 0.1 says "Not here yet: … the other labs". Both are true statements about those versions.
11. **Sagittarius A*, "black hole" as an object:** the Orbit Lab guide, the Journey caption, the timeline step "Weighing the black hole" and the Orbit changelog talk about the black hole at the centre of the galaxy. That is a thing in the sky, not a lab, so it stays.

The tests: `tests/test_app.cpp` has a list "native edits" (6 edits, 14 uses). Every changed text (About, Units note, all 19 far-away notes, the guide's sky section in all 8 cases, all 29 timeline steps) is compared with the web's own text, run by `tests/reference/make_app_reference.js`, after exactly those edits. An edit must match exactly once, so a stray extra change fails.

## Still different from the web
- **Skipped (not cheap):** Atom Lab labels and u/d letters are still drawn on top of the picture,
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
- **History:** "you are here" is on native 0.6, not on the web's 8.6.2. The 🕰 button says "v0.6"
  (the native version) where the web says "v8.6.2".
- **Settings** is a window. On the web it is a page that slides over the lab.
- **No animation:** the theme and language menus and the secret pop-ups appear without the web's
  small grow animation.
- **Saving:** the native app writes one text file. The web uses the browser's storage, so the two
  do not share progress.
- **The facts' order** is shuffled with a different random generator, so it differs from the web's
  run by run, as the web's own order does.

