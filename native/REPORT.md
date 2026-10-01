# Native Physics Lab 0.3 — report

0.3 finishes Orbit Lab. It was done in four steps, each committed to `native` and built by
GitHub Actions. Earlier reports are in the git history (0.2: `git show 1e3eb34:native/REPORT.md`).

## What works (checked)

**Step 1: "Slow time to watch it turn"**
- In the View card, when a planet with spin data is selected (Mercury to Neptune, and Pluto).
- Sets the Time speed so one turn takes 24 seconds, follows the planet, and at True scale
  zooms in on it (the web's zoomFor(radius, 80)).

**Step 2: Learn**
- What am I even looking at?, Why do we have seasons? and the Solar System timeline, with
  the web text **word for word**. `tools/make_learn_data.js` runs the web's own guide code
  in Node with a pretend page. It records the HTML for every case the text depends on
  (2D/3D, True scale/Easy to see, something selected or not). The live values (date,
  selected thing, Time speed, barycentre) are filled in by the app.
- The text is drawn by a small rich-text layout in ImGui: paragraphs, bold, italic, fact
  cards, headings, word wrapping.
  - "What am I even looking at?": one long page with a row of headings to jump to, a ↑ on
    each heading to go back, and the View/Sizes chips.
  - Seasons guide:
    - the four 2026 equinox/solstice buttons plus Today;
    - the live numbers from the lab's own Earth (daylight and noon Sun height from 80° N to
      80° S);
    - each planet's lean, measured from its real axis;
    - the orbit diagram, redrawn natively.
  - Timeline: all 29 steps. The ▶ buttons fly the lab to the place or jump to the date, as in
    the web. The Slingshot step's button is greyed out with a note, since that game is not
    in the native app.
- Emoji in headings: ImGui now uses 32-bit characters, so emoji that Segoe UI Symbol has are
  drawn. Any the font lacks are left out rather than shown as boxes.

**Step 3: Settings, metric/imperial, Defaults**
- ⚙ Settings → Metric (km · m/s · °C) or Imperial (miles · mph · °F). Everything inside
  stays SI; only the words on screen change.
  - What converts: facts cards (planets, moons, comets, Sun, far-away things), the scale bar
    (round numbers of miles), and the live line in the seasons guide.
  - The km/h and °C written inside the data's notes are converted with the web's rules
    (e.g. Neptune "Winds over 1,240 mph").
  - The choice is saved in your app-data folder (`%APPDATA%\PhysicsLab\PhysicsLab\settings.txt`).
- About the "facts line stays in °C" bug: I found the web's "Did you know?" ticker. It is
  not part of Orbit Lab and is not in the native app, so I did not add it. Instead, a test
  checks all 60 Orbit Lab facts cards in imperial mode for any leftover "km" or "°C" (none
  left). I also checked that this test does catch metric lines (103 caught with imperial
  switched off). If you meant a different line, tell me which.
- Defaults (top of the panel, and in Settings) now asks "Put Orbit Lab back?" before it
  resets anything. Checked with real clicks:
  - Cancel keeps everything as it is.
  - "Yes, put them back" restores: 2D view, ×0.06 zoom, nothing selected, the Solar System
    back to today.
  - Your units choice stays.

**Step 4: the two differences from 0.2**
- With Stars set to Off, the picture follows Windows' light or dark theme (the web's colours
  for each). Text loses its dark edge, as on the web. Checked in both themes.
- The Journey's last caption is the web's again: "… Tap Reset view to come back." The
  Journey still puts your settings back when it ends, so that sentence is now a little
  redundant.

**Tests:** 1,370 checks, all passing: physics, belts, spin/season wording, dates, number
formats, and the imperial cards.

## Not tested / be aware
- **On Windows:** each step was pushed and Actions builds it. Steps 1 and 2 were pushed
  before I finished; check the Actions tab, or that the `latest` release has commit
  "native 0.3 step 4". The `IMGUI_USE_WCHAR32` change is new for the Windows build.
- **Emoji:** which ones show depends on Segoe UI Symbol. On Linux, DejaVu dropped most of
  them (🔎, 🌍, 🕰); the guides read fine without them.
- **Theme:** read once per frame from Windows (`SDL_GetSystemTheme`). It was only checked by
  forcing it on Linux, not by switching Windows' theme. Only the picture follows the theme;
  the ImGui panels stay dark. The web's theme picker (🎨) is not ported.
- **Learn:** layout checked at one window size on Linux. Long guides on small windows were
  not tried.
- **Speed:** not judged here (no GPU).

## Still different from the web Orbit Lab
- The timeline's small picture icons are not drawn. The text and buttons are all there.
- The Sun's decorative glowing patches are not drawn. Lighting is Lambert shading, not the
  web's exact gradient.
- App-wide web features outside Orbit Lab are not ported: the "Did you know?" ticker, the
  theme picker, text size and font settings, motion settings, achievements.

Orbit Lab is done. I will not start another lab until you say so.
