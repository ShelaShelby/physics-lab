# Native Physics Lab 0.5 — report

0.5 brings over the web app's own features: Settings, themes, saving, the header with About and
history, the "Did you know?" line, the Learn and Achievements tabs. It was done in four stages,
each committed to `native` and built by GitHub Actions. The plan, with the `index.html` line
numbers of every item, is in `APP_PLAN.md`.

The Black Hole Lab, the Spacetime Lab and the Slingshot game were not started, as asked. Orbit Lab
and Atom Lab behave as in 0.4, apart from the changes listed under stage 4.

The 0.4 report is in the git history (`git show 4e25469:native/REPORT.md`).

## What works (checked)

All three test programs pass on Linux:
- `app_tests`: 427 checks (new);
- `atom_tests`: 17,818 checks;
- `orbit_tests`: 1,370 checks.

Stages 1 and 2 also built and passed their tests on GitHub's Windows runner. For stages 3 and 4,
check the `latest` release.

Screens were checked on Linux (Xvfb) with real mouse clicks sent to the window, and compared with
the web page in headless Chromium where useful.

**Stage 1: Settings, saving, themes**
- **⚙ Settings** has the web's sections and texts, word for word:
  - **Text size:** Small, Normal, Large, Extra large (0.9 / 1 / 1.15 / 1.3), with the sample line.
    It changes the panels, the guides and the labels drawn in both pictures.
  - **Font:** the web's five choices.
    - Theme default: Segoe UI on Windows.
    - Serif: Georgia.
    - Easy to read: Atkinson Hyperlegible.
    - Rounded: Nunito.
    - Monospace: Consolas.
  - **Units:** as before.
  - **Motion:** Auto, Reduced, Full. Reduced does this:
    - the labs start paused, with a ▶ Play button;
    - the Atom Lab camera jumps instead of gliding;
    - Cloud 3D does not spin by itself;
    - the facts line changes only when you tap it (checked: it stays on 1/27);
    - the achievement pop-up fades instead of sliding.
  - **Progress:** Reset my progress with the web's two pop-ups and the Done line. Checked with clicks
    from start to end: the settings file then has no unlocks.
  - **🛠 Developer:** Unlock everything.
- **Fonts I bundled:** Atkinson Hyperlegible and Nunito, both under the SIL Open Font License.
  - They come from the @fontsource packages (v5.3.0). The latin and latin-ext parts were converted
    to TTF, 400 kB in all.
  - They are built into the .exe by `cmake/embed_fonts.cmake`.
  - The two licences are in `assets/fonts` and in the zip's `font-licences` folder.
  - Georgia, Consolas and Segoe UI are not shipped: they come with Windows.
- **🎨 Themes:** all eleven, with the web's colours read from its CSS by a tool:
  - Auto, Light, Dark, Black, Midnight, Solar, Paper, Sage, High contrast;
  - the secret Synthwave and Monochrome, shown as "???" with their hints until found.
- **Each theme colours** the ImGui panels, the cards, the pop-ups, all text and the pictures.
  - Paper switches to its book font (Georgia).
  - Monochrome uses monospace, square corners and a grey picture (a grey pass on the GPU).
  - Synthwave has its purple glow and the pink glow on the title.
  - All eleven were looked at side by side.
- **Saving:** everything is remembered between runs.
  - Where: `%APPDATA%\PhysicsLab\PhysicsLab\settings.txt`, one `key=value` per line.
  - What: theme, text size, font, motion, units, unlocks, achievements (with the time they were
    earned), the lab on screen, and every setting of both labs (view, zoom, sliders, isotope,
    switches, sky, constellations…).
  - When: it is written a quarter second after something changes.
  - Bad or unknown lines are ignored, and every value is checked when it is read.
  - Defaults still puts a lab back to the web's starting settings.

**Stage 2: header, About, history, language**
- **The header across the top:** Physics Lab, the lab's subtitle (word for word), then the web's
  buttons with their tooltips:
  - 🎨 Theme;
  - 🌐 English: Eesti keel is greyed out with "Coming soon";
  - 👤 About;
  - ⚙ Settings;
  - 🕰 history;
  - the lab's Update badge (Update 7.7.2 / 7.7.3), which opens that lab's web changelog word for
    word, with its note.
- **👤 About:** the web page word for word.
  - It has the card, the developer line, "Built with Claude" and "Where the facts come from" with
    every source and licence.
  - The links are clickable and open in the browser.
  - The version line lists only the native labs: "Labs: Orbit Lab 7.7.2 · Atom Lab 7.7.3."
  - Under it, a line gives the native version.
- **🕰 history:** the web's intro, all 42 APP_LOG entries word for word with their icons, and its
  closing note.
  - ▶ buttons appear only for Orbit Lab and Atom Lab.
  - Then come the native entries 0.5 → 0.1, with "you are here" on 0.5.
- **The panels and the picture** start under the header. Checked: a click on Neptune still selects
  Neptune.

**Stage 3: achievements, the facts line, the Learn tab**
- **💡 Did you know?:** all 27 web facts (the 🎁 is one of them) in a shuffled order. It has:
  - the counter (1/27);
  - a tap for the next fact, with a quarter-second fade;
  - a wait of 6 s plus 55 ms per character, as the web;
  - the accent border while the mouse is on it, and it waits then.
- **The 🎁 surprise:** opens the web's pop-up, with its little sun-and-grid picture.
  - It unlocks Synthwave, and "Use it now" switches to it.
  - Checked with clicks: the file then has `theme=synthwave`, `unlock=synthwave` and the
    achievement.
- **The "monochrome" word** in the Atom Lab guide (What is to scale) is clickable, with a dotted
  underline. It opens the Monochrome pop-up and unlocks the theme and Sharp Eyes (checked with
  clicks).
- **Tabs:** 🔬 Simulations · 📚 Learn · 🏆 Achievements. While Learn or Achievements is open, the lab
  is not drawn and does not run, as on the web.
- **🏆 Achievements:** the count ("2 of 4 unlocked"), the bar and the grid of cards. Locked cards
  are dimmed with 🔒 and the hint. Earned cards show "Unlocked 5 Sept 2026" (the web's en-GB date)
  or "Unlocked earlier".
- **The pop-up when one is earned:** it comes 0.7 s later at the bottom right, slides up, stays
  5.2 s and fades. Later ones queue.
- **The four achievements and their triggers**, each checked:
  - **Neon Nights:** the 🎁.
  - **Sharp Eyes:** the word.
  - **Half-Life:** any nucleus decaying on its own. Checked with polonium-212.
  - **Galactic Center:** Sagittarius A* selected in the Orbit Lab.
  - Point of No Return is skipped, and the native changelog says so.
- **📚 Learn:** the web's heading and intro, then all seven guides in two sections, Atom Lab and
  Orbit Lab: Atom guide, Scale, Weight, the atom timeline, the Orbit guide, Seasons, the Solar
  System timeline.
  - A click opens the guide in its lab.
  - If that is the other lab, it goes back to the lab you were in when you close the guide, as the
    web does (checked with clicks).

**Stage 4: polish and tests**
- **The Sun's glowing patches:** 16 soft bright and orange patches. They turn with the Sun's real
  axis and rate and are squashed towards the edge, as the web's `drawSun`. They look like the web's
  at the same zoom.
- **Antiquark rings in Cinematic** are now drawn with added-up light, the web's "lighter" mode
  (checked in the proton with + Sea).
- **Atom Lab readout:** the rows on their see-through panel no longer get the 8-copy halo. In light
  themes it left faint grey marks between the letters (found while checking the themes).
- **Tests** (`tests/test_app.cpp`, against runs of the web's own code):
  - **Settings file:** save and load every field, the defaults, broken and unknown lines, a locked
    secret theme, line breaks in values.
  - **Themes:** all 11 in the web's order, every colour present for the panels and the pictures,
    "light" matching the background, enough contrast between text and background.
  - **Texts:**
    - the reset pop-ups for all 18 counts;
    - "Unlock everything";
    - the header;
    - About;
    - the language menu;
    - the 42 history entries;
    - the 27 facts;
    - both secret pop-ups (first find and again);
    - the Achievements and Learn tabs, built from the same saved state as the web run;
    - the en-GB date for every month.
- **Version** 0.5 in CMake, the 🕰 history entry for 0.5, and the README.

## Not tested / be aware
- **On Windows:** I could not try these on a real Windows PC.
  - Auto theme following Windows' light/dark setting.
  - Auto motion following "Animation effects".
  - The settings file in `%APPDATA%`.
  - Links opening in the browser.
  - Segoe UI Emoji as a fallback: it should make the icons (🎨, 🏆, 💡, ⚛️ …) visible as plain
    outlines. On Linux most emoji are missing and are left out of the text.
  - The Windows build compiles and its tests pass on GitHub.
- **Flags:** 🇬🇧 and 🇪🇪 are left out of the language menu. No bundled font can draw them, and they
  would show as boxed letters.
- **Speed:** not judged here (no GPU). The Monochrome grey pass draws the picture once more
  offscreen.
- **Sizes:** layout checked at 1400 × 860 only, at every text size. The header wraps its subtitle,
  and puts its buttons under the title when the window is narrow. I did not check a very small
  window.
- **A lab running before its tab:** the lab does not run while a tab is open. A Journey in progress
  just waits.

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
- **Kept word for word although they name parts the native app does not have:**
  - About's card ("real orbits, atoms, black holes and a little space game");
  - its sources for black holes and spacetime;
  - the Units note ("the Black Hole readout");
  - the history entries about the other labs, which have no ▶ button.
  - None of the 27 facts names a lab.
- **History:** "you are here" is on native 0.5, not on the web's 8.6.2. The 🕰 button says "v0.5"
  (the native version) where the web says "v8.6.2".
- **Settings** is a window. On the web it is a page that slides over the lab.
- **No animation:** the theme and language menus and the secret pop-ups appear without the web's
  small grow animation.
- **Saving:** the native app writes one text file. The web uses the browser's storage, so the two
  do not share progress.
- **The facts' order** is shuffled with a different random generator, so it differs from the web's
  run by run, as the web's own order does.

## 0.6 stage 3: every spot in the native text that names the Black Hole Lab, the Spacetime Lab or the slingshot game
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
