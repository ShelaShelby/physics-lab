HISTORICAL plan (native 0.4/0.5). ROADMAP.md is the current plan.
# App-wide features plan (native 0.5)

Reference: `index.html` (web Physics Lab 8.6.2). Line numbers are from the current file, which is
not edited. Text is copied out of it by tool scripts, never retyped. The Black Hole Lab, the
Spacetime Lab and the Slingshot game are **not** ported.

Stages (each committed and pushed on its own, so Actions builds it):
1. **Settings, saving, themes**
2. **Header, About, history, language**
3. **Achievements, the facts line, the Learn tab**
4. **Polish, tests, report**

## How the text is checked
As for the labs, a Node script loads the web's app code with the small fake page
(`tests/reference/fake_dom.js`):
- the settings, About, history, theme, achievement, Learn-tab and fact-line code (lines 4933–5366);
- the text-size and motion scripts (lines 202–214).

It calls the web's own handlers (open Settings, open About, the reset pop-up with different
counts, the theme menu with secrets locked or unlocked, `paintAch`, `paintLearn`, `appTimeline`)
and records the HTML they build. The C++ builds the same HTML pieces, and `tests/test_app.cpp`
compares them word for word. The data lists are copied into `src/app/app_text.inc` by
`tools/make_app_text.js`:
- `APP_LOG` and `APP_PLAN`;
- `DYK`;
- the achievement list and `THEMES`;
- the language list and `PL_FONTS`;
- the text sizes.

## Stage 1: Settings, saving, themes

| Item | index.html | Native version |
| --- | --- | --- |
| Text size (Small / Normal / Large / Extra large; factors 0.9 / 1 / 1.15 / 1.3) | `PL_SIZES` line 203; `TEXT_SIZES` 5057; `setTextSize` 5059; Settings section 5086–5096 | The four buttons with their "Aa" samples, the note and the sample sentence. The size scales every panel, guide and picture label (ImGui font scale and picture text). |
| Font (Theme default, Serif, Easy to read, Rounded, Monospace) | `PL_FONTS` line 206; `setFont` 5066; Settings 5098–5108 | The five buttons, each "Aa" in its own font. **Easy to read** (Atkinson Hyperlegible) and **Rounded** (Nunito) are both SIL Open Font License: bundled inside the .exe with their licence text. **Serif** (Georgia), **Monospace** (Consolas) and **Theme default** (Segoe UI) come with Windows, so they are loaded from the system and not shipped. Letters a font lacks (×, ⁻, ☢ …) fall back to Segoe UI / Segoe UI Symbol. |
| Units (Metric / Imperial) | `setUnits` 5080; Settings 5110–5120 | Already done in 0.3. It moves into the new Settings window with the web's note, word for word (the note names the Black Hole readout; see the report). |
| Motion (Auto / Reduced / Full) | `PL_MOTION`, `plRM` 211–214; `setMotion` 5073; Settings 5121–5131 | The three buttons and the note. **Auto** follows Windows' "Animation effects" setting (`SPI_GETCLIENTAREAANIMATION`). **Reduced** means: the labs start paused (and the pause button says ▶ Play, as `setPaused` 4754); the Atom Lab camera jumps instead of gliding (3269); Cloud 3D does not spin by itself (3129, 5077); the facts line changes only on tap (5337); pop-ups appear without sliding. |
| Progress: Reset my progress | Settings 5133–5165 | The row with its small line. The first pop-up says "Nothing to reset", or "Reset your progress?" with the counts of secret themes and achievements; the second says "Progress reset"; then the Done line. |
| Developer: Unlock everything | Settings 5167–5183 | The row, and the "✅ Unlocked …" line with the web's wording and plurals. |
| Themes (🎨) | `THEMES` 5225–5237; colours 10–27 (`:root[data-pl=…]`); menu 5238–5280 | The menu: a swatch, name and small line for each theme, ✓ on the one in use, and the two secrets shown as "???" with their hints until unlocked. Every theme sets the colours of the ImGui panels, cards, buttons and text, and of both pictures (the Atom Lab picture, and the Orbit Lab picture with Stars Off, as the web). **Auto** follows Windows' light/dark setting. **Paper** uses its book font (Georgia, `--ffd` line 25). **Monochrome** uses the monospace font, square corners (line 27) and a grey picture (`canvas{filter:grayscale(1)}`, line 28), done as a grey pass over the picture. Synthwave gets its purple glow at the top of the page (line 23). |
| Remember everything | `STORE_KEY` / `persist` / `remember` 4733–4744; `plTheme`, `plTextSize`, `plFont`, `plMotion`, `plUnits`, `plUnlock`, `plAch`, `plAtomMore`, `plLang` | One text file `settings.txt` in the app-data folder (`%APPDATA%\PhysicsLab\PhysicsLab\`). It holds the theme, text size, font, motion, units, language, unlocked themes, achievements (with the date earned), More/Less info, the lab on screen, and every setting of both labs: Orbit's view, zoom, time speed, sizes, trails, extras, sky and constellations; Atom's element, isotope, view, orbital, orientations, zoom, sliders and quark choices. Saved shortly after a change, like `remember()`. Values are checked on loading, like `restored()` (4740), and anything unknown is ignored. **Defaults** still puts a lab back to the web's defaults and forgets its saved settings (`load(name, true)`, 4758). |

## Stage 2: Header, About, history, language

| Item | index.html | Native version |
| --- | --- | --- |
| Header buttons | line 219 | A header row at the top of the panel, shared by both labs: 🎨 Theme, 🌐 English, 👤 About, ⚙ Settings, 🕰 history, and the lab's own "Update …" button. Each has the web's tooltip, word for word. |
| Language | 5014–5043 | A menu with 🇬🇧 English ✓ and 🇪🇪 Eesti keel greyed out with "Coming soon". The choice is saved (`plLang`). |
| About | 5045–5052 | The card, the developer line, "Built with Claude" and "Where the facts come from" with every source group and link (links open in the browser), word for word. The version line names only the native labs: "Labs: Orbit Lab 7.7.2 · Atom Lab 7.7.3", plus the native version. The card's first sentence mentions black holes and a game: it is kept word for word (see the report). |
| App history (🕰) | `APP_LOG` 4933–4974; `APP_PLAN` 4976–4983; `appTimeline` 4985–4997 | The intro, the "Planned next" card and every web entry, word for word. An entry's ▶ button is kept only when its lab exists natively (Atom Lab, Orbit Lab). Then the native versions 0.1 to 0.5 (the native changelog), and the web's closing note. |
| The lab's changelog button | `openLog` 4998–5009 | "Update 7.7.2" (Orbit) or "Update 7.7.3" (Atom) opens that lab's web changelog with its note, as the web does. The native changelog moves into 🕰 history. |

## Stage 3: Achievements, the facts line, the Learn tab

| Item | index.html | Native version |
| --- | --- | --- |
| Achievements tab | `paintAch` 4888–4899; `PLAch` 5188–5222; tabs line 225 | A tab row above the panel: 🔬 Simulations · 📚 Learn · 🏆 Achievements (no Games tab, as there is no game). The 🏆 page shows "N of 4 unlocked", the progress bar, and a grid of cards locked until earned (🔒, how to get it), then the icon, text and "Unlocked 1 Oct 2026" date. The pop-up slides in at the corner for 5.2 s, queued, and does not slide with reduced motion. |
| Neon Nights | 5190; granted by the 🎁 fact (5272) | As the web. |
| Sharp Eyes | 5191; granted by the guide's "monochrome" word (2650, 5281–5298) | The word in "What is to scale" becomes clickable. It unlocks Monochrome and grants the achievement, with the web's pop-up ("You found a secret!" / "Use it now"). |
| Half-Life | 5193; granted on a decay (1973) | Granted in `decTick` when a nucleus decays. |
| Galactic Center | 5194; granted when Sagittarius A* is selected (1340) | Granted when Sgr A* is selected, by a click or from the Far away tiles. |
| Point of No Return | 5192, 3908 | **Skipped** (Black Hole Lab). The changelog says so. |
| "Did you know?" line | `DYK` 5301–5329; logic 5330–5366; button line 222 | A line across the top of the picture: 💡 **Did you know?**, the fact and the counter (e.g. 3/27), in a shuffled order. It moves on after 6 s + 55 ms per letter, and on a click. It waits while the mouse is over it or a pop-up is open, and only changes on click with reduced motion. "@gift" shows the 🎁 line and opens the Synthwave pop-up (unlock, "Use it now"). None of the 25 facts names a lab. |
| Learn tab | `paintLearn` 4900–4918; `LEARN` 249–262 | The page heading and line, then ⚛️ Atom Lab (4 cards) and 🪐 Orbit Lab (3 cards: the guide, Why do we have seasons?, the timeline), each with its icon, title and description from `LEARN`. Clicking a card opens that guide in its lab; closing it brings back the lab you were in (`lrn.after`, 4911). The Spacetime Lab section is left out, and so is the note naming the Black Hole Lab and Slingshot, as those are not in the native app. |

## Stage 4: Polish
- Small visual gaps, if cheap:
  - the Sun's glowing patches (Orbit, web `sunDraw`);
  - the Atom Lab labels and u/d letters in depth order (drawn into the picture);
  - antiquark rings with "lighter" blending in Cinematic.
  I'll skip any that are not cheap.
- Tests:
  - the new texts against the web, as above;
  - saving and loading `settings.txt`, including bad and missing values;
  - the themes: every theme has every colour for the panels and the picture.
- `REPORT.md` and the 0.5 changelog. Version 0.5.
