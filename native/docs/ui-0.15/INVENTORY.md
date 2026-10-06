# 0.15 stage 2 — control inventory: every control, readout, text block and window, and its new home

Made by reading the source (`orbit_panel.cpp`, `atom_panel.cpp`, `journey_dates.cpp`, `app_pages.cpp`, `app_tabs.cpp`, `settings.cpp`, `learn.cpp`, `atom_learn.cpp`, `changelog.cpp`, `benchmark.cpp`, `main.cpp`) and checked against the renders of stage 1 (re-run in stage 2, see "Audit corrections" at the end). The new design is the owner's (DESIGN.md). **The owner judges this list**; nothing in the app has changed.

## How to read it
**Homes** (the short names used in the tables):
- **TB** top bar (about 40 px): menu ☰, breadcrumbs *Physics Lab › Orbit Lab ▾ › Solar System › Earth*, the centre chip, search, small icons on the right (other lab, Learn, Achievements, 💡 facts, Theme, Settings).
- **CHIP** the centre chip and its dropdown panel (Orbit: the date/time chip → Time panel; Atom: the element chip → periodic table).
- **SEARCH** Ctrl+K: any body, element, isotope, view, setting, guide. Every control below also has a search entry, which is the second way in.
- **CARD** the subject card (top right: name, one-line kind, one big key number, 4–6 key values, one action link). **DATA** = its "All values" button, which opens the *data sheet*: a large floating panel with tabs (Values · Spectrum · Lifetimes · Nucleus · Temperature · Decay …). The data sheet is how nothing is lost from the Atom Lab's many cards.
- **TOOL** a button on the floating toolbar (bottom centre); **TOOL▾** a control inside the small panel that opens above a toolbar button's caret.
- **LEG** the Legend (top left, collapsed by default). **RO** the tiny readout (bottom right). **PIC** stays in the picture (mouse, wheel, keys, labels). **MENU** ☰ menu. **SET** the Settings window. **READ** the reading view (a centred column ≤ 70 characters over a dimmed picture, with headings and a table of contents).
- **Reach** = clicks to make the control usable: 0 = already on screen, 1 = one click opens it (a chip, a caret panel, a window), 2 = two clicks. **Nothing is more than 2.** "One click away" means reach ≤ 1. Panels open on click and close on a click outside, Esc, or when another opens.
- **Default**: Y = visible without any click; ✓ = after one click; — = only when relevant (a selection, a view, a state).

**Stage 5c (6 Oct 2026, owner's decision, DECISIONS.md): a new home, CTRL = a section of the Controls panel** (one docked panel on the right, both labs, open by default, a Controls button on the toolbar opens / closes it; reach 0 while it is open, 1 when closed; the Pick a body section starts folded: reach 1). Wherever the tables below say **TOOL speed ▾ / show ▾ / pick a body ▾ / scale ▾ / the 2D/3D caret (View options)** read **CTRL Time speed / Show / Pick a body / Zoom, sizes and trails / View options**; the toolbar keeps 2D / 3D and gains a one-click **Reset view**. In the Atom Lab **TOOL Nucleus ▾ (Nucleon level, Build another, Reshuffle) and Cloud ▾ (Cloud, Cloud 3D, Slice)** read **CTRL View**; the Nucleus and Cloud buttons stay one click. Reach never gets worse (a reach-1 caret panel is reach 0 or 1 now).

## 1. Orbit Lab (today: the side column "Controls", the picture and its overlays)
| Today (where) | New home | Reach | Default |
|---|---|---|---|
| Orbit Lab / Atom Lab buttons (top of column) | TB breadcrumb *Orbit Lab ▾* dropdown; TB "other lab" icon | 0 / 1 | Y |
| Defaults (puts the lab's settings back; asks first) | MENU › Put this lab's settings back | 2 | ✓ |
| 📷 Save image (+ its note) | **MENU › Save picture** (stage 4: owner's note of 6 Oct 2026, a camera button on the toolbar felt like a phone's media bar); also SEARCH; the note shows as a toast above the toolbar | 1 | ✓ |
| ☉ Reset to today | CHIP ↺ "Today" button inside the chip (and in the Time panel) | 0 | Y |
| Journey: Earth to the galactic centre / ■ Stop the journey | CHIP Time panel › Jump to › Journey; while it runs, a ■ Stop button replaces Today in the chip and the chip says "Journey in progress" (stage 4) | 2 | ✓ |
| Date box (YYYY-MM-DD), time box (HH:MM), Jump | CHIP Time panel: **Live / Set manually** switch; Set manually shows the date field with Go, the 24-hour dial and hour / minute steppers (stage 4) | 1 | ✓ |
| "Today" button (journey card) | CHIP Today button; Time panel › Live (which also runs the lab) | 0 / 1 | Y |
| 6 history buttons (Halley 1986, Neptune found 1846, Mars closest 2003, Venus crosses the Sun 2012, Great conjunction 2020, Halley returns 2061) | CHIP Time panel › Jump to: the first 4, then "More dates" (stage 4: 3 clicks for the last two); also SEARCH | 2 / 3 | ✓ |
| Paragraph "Pick any date from 1800 to 2100 …" (6 lines) | CHIP Time panel: behind the (i) button (stage 4) | 2 | ✓ |
| Learn: ❓ What am I even looking at? · 🌍 Why do we have seasons? · 🕰 Timeline of the Solar System | TB Learn icon (the Learn page, READ) and CARD "Learn about this" link | 1–2 | ✓ |
| ▶ Play / ❚❚ Pause | **CHIP: a small play / pause icon at the chip's left** (stage 4, same owner's note; Space stays; also SEARCH) | 0 | Y |
| Time speed readout ("1 s = 29 days") and slider | CHIP second line shows it always; TOOL speed ▾ slider (one click on the speed button opens it) | 0 / 1 | Y |
| "Slow time to watch moons", "Slow time to watch it turn" | CARD action link (only when a body with moons / spin is selected; built in stage 4) | 1 | — |
| Performance card: VSync tick box; GPU rocks tick box; "Frame … ms", "= fps", "Work per frame (CPU)", the frame-time plot; "Window drag / resize" line | SET › Performance (all of it, the same code and wording; in Settings from stage 4 when the New UI is on); the fps number stays in RO | 2 | ✓ |
| Benchmark… button (and the Benchmark window, its start / cancel / result / Copy result) | SET › Performance › Benchmark… (window unchanged) | 2 | ✓ |
| Planet physics: Simple / Real; General relativity (1PN) tick box; Exaggerate the Sun's wobble (×50); the Real / relativity notes (long) | TOOL show ▾ › Planet physics (segmented Simple · Real; the two tick boxes appear under Real; notes: one line + More) | 1 | ✓ |
| View: 2D (top-down) / 3D (rotatable) combo | TOOL 2D/3D button | 0 | Y |
| ✥ Drag moves the view / ⟲ Drag rotates; ⟳ Spin on/off; ◉ Following / ○ Follow; ↺ Reset view | TOOL 2D/3D caret ▾ › View options (the same four) | 1 | ✓ |
| Zoom slider ("Zoom: ×0.06") | RO zoom (click opens a zoom slider); wheel and pinch stay; TOOL scale ▾ also has it | 0 / 1 | Y |
| Sizes: True scale / Easy to see | TOOL scale ▾ | 1 | ✓ |
| Trails: Short tail / Full orbit trails | TOOL scale ▾ | 1 | ✓ |
| Sky: Soft / Full / Off; Constellations Off / Lines / Lines and names; Far-away things (zoom out to find them) tick box | TOOL show ▾ › Sky | 1 | ✓ |
| Solar System extras: Moons · Dwarf planets · Famous comets · Belts and Oort cloud | TOOL show ▾ (check boxes) | 1 | ✓ |
| Go to: tiles — Star and planets: Sun, Barycentre, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune; Dwarf planets: Ceres, Pluto, Haumea, Makemake, Eris; Famous comets: Halley, Hale–Bopp, NEOWISE, Encke, 67P, Swift–Tuttle; Moons: Moon, Phobos, Deimos, Io, Europa, Ganymede, Callisto, Enceladus, Rhea, Titan, Titania, Oberon, Triton, Charon; Belts and the far edge: Asteroid belt, Jupiter’s Trojans, Kuiper belt, Oort cloud; Far away: Voyager 1, Voyager 2, Alpha Centauri, Sirius, Vega, Pleiades, Orion Nebula, Galactic centre (47 tiles: stage 2 said 45, a miscount found by the stage 4 test) | TOOL pick a body ▾ (grouped list with a filter box; a click flies there; the chosen one is ticked and a click on it lets go); SEARCH; a click on the thing in the picture | 1 | ✓ |
| Hints "Click a tile to fly there …" and "Real orbits from NASA/JPL data. Moons show when you zoom in …" | pick-a-body panel: the first on top, the second at the foot (stage 4: the NASA/JPL source stays in view) | 1 | ✓ |
| Date label in the picture ("Solar System on 5 Oct 2026, 12:00 UT") | CHIP (the same words; the label leaves the picture) | 0 | Y |
| Scale bar and "light takes 2 hours" | RO (scale, light time) | 0 | Y |
| fps counter | RO | 0 | Y |
| Axes gizmo (x y z) | PIC (stays, bottom right above RO) | 0 | Y |
| Selection card in the picture (name, size, mass, distance, speed, orbit, one lap, temperature, day, tilt, season lines, "1 moon shown", "click anywhere to close") | CARD (name, kind, the big number = distance from the Sun, 4–6 values) and DATA (the rest, same wording) | 0 / 1 | — |
| Labels, orbit rings, trails, belts, stars in the picture; click to select; drag (rotate / move); wheel zoom; right/shift drag; Space | PIC (unchanged) | 0 | Y |
| Tooltips on tiles, tick boxes, header buttons | kept (same wording) | — | — |

## 2. Atom Lab (today: the side column "Atom controls", the bar under the picture, the picture and its overlays)
| Today (where) | New home | Reach | Default |
|---|---|---|---|
| Orbit Lab / Atom Lab buttons | as in 1 | 0 / 1 | Y |
| Defaults · Reset (a fresh atom, same settings) · 📷 Save image | MENU › Put this lab's settings back (asks first) · TOOL reset (↺, with "Reset view") · TOOL save picture | 2 · 0 · 0 | ✓ · Y · Y |
| ▶ Play / ❚❚ Pause, "Space: play / pause" | TOOL play (added to the Atom toolbar: it exists today and is used) | 0 | Y |
| ❓ What am I even looking at? (Atom guide) and the other 3 Atom guides (Scale, Weight, Timeline of the atom) | TB Learn icon (Learn page) and CARD link | 1–2 | ✓ |
| **Nucleus** — the periodic table (118 cells, 9.5 px symbols) | CHIP dropdown: the periodic table (cells ≥ 28 px) | 1 | ✓ |
| ⤢ Bigger table | gone as a button: the chip's table IS the big table (feature kept) | — | — |
| Protons (Z) stepper ◀ ▶, slider; the element card (symbol, name, class, period, group, state at room temperature, "Where you meet it", melting / boiling point, fact lines) | CHIP dropdown (Z stepper and slider under the table); CARD (name, kind, the big number = Z or atomic mass); DATA › Values (everyday use, facts) | 1 | ✓ |
| Neutrons: − slider +, "usual" | CHIP dropdown › Neutrons (−, slider, +, usual); the isotope list "famous isotopes" next to it | 1 | ✓ |
| Famous isotopes combo ("Pick one…") | CHIP dropdown › famous isotopes list; SEARCH | 1 | ✓ |
| ☢ Build another · 🎲 Reshuffle nucleons | TOOL Nucleus view ▾ (shown only in the nucleus view, as today) | 1 | — |
| Decay history list (steps so far) | DATA › Decay | 2 | ✓ |
| Decay chain: Show the chain on the chart; Walk one step; Auto-walk / Pause; Restart walk; the chain text blocks | CARD action link "Walk its decay chain" opens the walk controls in a small panel on the chart; TOOL marks ▾ › Show the chain; the long chain text: DATA › Decay with More | 1 | — |
| **View & zoom** — Scale Auto / Fixed; Zoom (log scale) slider | TOOL view options ▾ › Scale, Zoom; RO zoom | 1 | ✓ |
| Quark style (Cinematic / Accurate / Classic); Nucleon detail (Valence / + Gluons / + Sea); Nucleon edge (Fuzzy / Hard balls) | TOOL view options ▾ › Nucleus section (shown at the zoom levels where they apply, as today) | 1 | — |
| ⟳ Spin on/off | TOOL view options ▾ | 1 | ✓ |
| View: Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides | TOOL view buttons **Atom** (Orbit view) · **Nucleus** (zoom to the nucleus; Nucleon one level further, a segmented choice inside the Nucleus button's caret) · **Cloud** (Cloud · Cloud 3D · Slice, chosen in its caret) · **Chart** (the chart of nuclides) | 0 / 1 | Y |
| The bar under the picture: ⌕ Atom · ⌕ Nucleus · ⌕ Nucleon · Drag: rotate (✥) · Reset view; its hint line; "Less info" | TOOL (the same actions as the view buttons and reset view); hint → RO; "Less info" → CARD collapse | 0 | Y |
| Tilt (flat chart to landscape) slider; Height means (Half-life / Binding energy per nucleon); Reset tilt, zoom and position | TOOL view options ▾ (chart section) | 1 | — |
| Orbit style (Modern / Bohr 1913) | TOOL view options ▾ (orbit section) | 1 | — |
| Orbital 1s … 4f (10 choices); Orientation (p, d, f variants); Energy level n slider; Playback speed | TOOL view options ▾ › Orbital (first row; also SEARCH "4f") | 1 | — |
| Cloud style (Classic / Glow); Cloud density (Normal / High / Ultra, with the 12,000-dots note); 90 % outline Off / On; Phase colours Sign / Wheel | TOOL marks ▾ (outline, phase colours) and view options ▾ (style, density) | 1 | — |
| Superposition Off / On; Second orbital; Mix slider; Playback speed (beats) | TOOL view options ▾ › Superposition | 1 | — |
| Slice: Cut direction; Cut position | TOOL view options ▾ (slice section) | 1 | — |
| Where the electron is found (radial graph and its marks; the explanation) | DATA › Values (graph) with the explanation behind More; the three marks also in LEG | 2 | ✓ |
| **Photons**: ↓ Emit photon; ↑ Absorb photon; Auto photons (on / off, its notes) | TOOL photon / jump ▾ | 1 | ✓ |
| Check a jump: from (shell) to (shell), the result lines | TOOL photon / jump ▾ › Check a jump | 1 | — |
| **Light spectrum** card (lines, strip, visible 380–780 nm, the non-relativistic note) | DATA › Spectrum (note: one line + More; the honesty line stays) | 2 | ✓ |
| Relativity option (Off / On) | DATA › Spectrum (with its note) | 2 | ✓ |
| **Real spectrum (NIST)** card (ion and neutral atom, two separate comparisons) | DATA › Spectrum | 2 | ✓ |
| **Nuclear size and shape (measured)** card | DATA › Nucleus | 2 | ✓ |
| **Lifetimes and jumps (NIST)** card | DATA › Lifetimes | 2 | ✓ |
| **Temperature** slider (0–1000, K) and preset jumps (e.g. room, boiling); state and speed lines; details | DATA › Temperature | 2 | ✓ |
| The readout above the picture (4 lines: state, layer, "model: one electron, non-relativistic …", e.g. average radius) | CARD (kind line, key number, values); the model-limits line stays visible as a one-line RO item ("model: one electron, non-relativistic ⓘ", full words behind ⓘ) — honesty rule | 0 | Y |
| Picture: nucleus, electron, labels, hover tooltips, click a square on the chart, arrow keys, wheel, drag, shift-drag; the chart's status line, legend (8 colours with counts), notes box, annotation labels | PIC for the picture and its labels; status line → CHIP/CARD ("Chosen: Uranium-238"); legend → LEG (collapsed by default; one line says "Colour: how it decays"); notes box → LEG › Notes (More) | 0 | Y |
| "Picture: n draw calls, m instanced shapes" (debug line) | SET › Performance | 2 | ✓ |

## 3. App-wide: header, tabs, facts, pop-ups, windows
| Today | New home | Reach | Default |
|---|---|---|---|
| Header block: title "Physics Lab", subtitle sentence | TB breadcrumbs (the subtitle becomes the lab's tooltip and the Learn page's first line) | 0 | Y |
| Header buttons: Theme (13 theme cards, locked ones, tooltips) | TB Theme icon (the same popup; unlocks unchanged) | 1 | ✓ |
| 🌐 English (language list; "soon" entries) | SET › Language (or MENU › Language) | 2 | ✓ |
| About (page) | MENU › About (READ) | 2 | ✓ |
| ⚙ Settings (window) | TB Settings icon (window; its content in section 4) | 1 | ✓ |
| "v0.14 alpha · history" (the app's history page) | MENU › What's new (READ) | 2 | ✓ |
| "Update 7.7.2" / "Web Atom Lab history 7.7.3" (the lab's changelog window) | MENU › Orbit Lab / Atom Lab changelog (READ) | 2 | ✓ |
| Tabs Simulations · Learn · Achievements | the picture (Simulations); TB Learn icon; TB Achievements icon | 0 / 1 | Y |
| "Did you know?" bar (27 facts, ›, the 💡 secret-theme unlock) | **the FACT LINE** (stage 4, owner's note: facts should be seen without asking): one short muted line at the bottom left of the picture, fading to the next fact every ~20 s, still while the pointer is on it; a click opens the whole fact in a small panel (the secret-theme unlock stays inside it); ‹ › on hover; Settings › Facts On / Off; also SEARCH. No 💡 button. | 0 | Y |
| Achievement toast (bottom), the secret pop-up ("##secret") | toast stays (bottom centre, above the toolbar); pop-up unchanged | — | — |
| Fullscreen F11, Space, arrow keys, Esc closing windows | unchanged; listed in MENU › Keys (a short help card, new) | — | — |
| Windows: Settings, About, History, lab changelog, Learn guides (3 Orbit, 4 Atom), Benchmark (ask, run, result), "Put … back?" dialogs, the reset-progress dialogs | Settings and the dialogs stay small windows; About, History, changelogs and guides become READ | — | — |

## 4. Settings (today: one window, 48 controls seen, scroll 446 px)
| Today | New home | Reach |
|---|---|---|
| Text size: Small · Normal · Large · Extra large (+ sample text) | SET › Display (and a text-size stepper "A− A+" in the TB menu so it can be fixed fast) | 1–2 |
| Font: Theme default · Serif · Easy to read · Rounded · Monospace | SET › Display | 2 |
| Units: Metric / Imperial (+ note) | SET › Display | 2 |
| Motion: Auto · Reduced · Full (+ note) | SET › Display | 2 |
| Theme effects: On / Off (+ note) | SET › Display | 2 |
| New: Display line "Windows scale 125 % · window 1280 × 800 · body text 15 px" | SET › Display (stage 3) | 2 |
| Progress: reset progress (and its two dialogs); the unlock code row | SET › Progress | 2 |
| Language | SET › Display | 2 |
| Performance (see section 1) and Benchmark | SET › Performance | 2 |

## 5. Learn, Achievements and the reading pages
| Today | New home | Reach | Default |
|---|---|---|---|
| Learn tab: 7 guide cards (4 Atom, 3 Orbit) with icons and 2-line blurbs | the Learn page: READ with a card list (TB Learn icon) | 1 | ✓ |
| Orbit guides: "What am I even looking at?" (sections with a "jump to" row), "Why do we have seasons?" (season buttons, "Today"), "Timeline: how we understood the Solar System" (22 steps with ▶ buttons that set the lab) | READ with a table of contents; the ▶ buttons stay and close the page to show the lab | 2 | ✓ |
| Atom guides: "What am I even looking at?" (4 sections), Scale (object tabs), Weight (tabs), Timeline of the atom (22 steps, ▶ buttons); the element stepper ◀ ▶ − + usual inside Scale / Weight | READ with a table of contents; the steppers stay inside the page | 2 | ✓ |
| Achievements tab: 10 cards (locked ones show "???"), the progress bar | TB Achievements icon → a floating panel (list, progress); secret ones unchanged | 1 | ✓ |
| About page (254 entities now decoded; long, 78,000 characters with sources) | READ: headings (Who made it, Built with Claude, Where the facts come from, …) and a table of contents | 2 | ✓ |
| History page (app changelog, 87,000 characters), lab changelogs | READ: one heading per version, older versions collapsed, table of contents | 2 | ✓ |
| Long paragraphs in the Atom panel (everyday facts, model notes, spectrum and temperature notes, chain texts, chart notes) | one line + More inside DATA / LEG; the full wording is kept, nothing is dropped | 2 | ✓ |

## 6. The 10 most-used controls per lab (one click away, reach 0 or 1)
No usage data exists (the app does not record clicks). The choice is from what each lab is for, what the guides send a first-time user to, and the web version's layout; **the owner should correct it**.
- **Orbit Lab:** (1) Play / Pause — **the chip's play icon**, reach 0 (stage 4; was TOOL); (2) Time speed — chip line + TOOL speed ▾; (3) Date and time — CHIP; (4) Today — the Today button inside the chip, reach 0; (5) Pick a body — click in the picture (0) or TOOL pick ▾ (1); (6) Moons / Dwarfs / Comets / Belts — TOOL show ▾; (7) Zoom — wheel, RO, TOOL scale ▾; (8) 2D / 3D — TOOL, reach 0; (9) Planet physics Simple / Real — TOOL show ▾; (10) Save picture — **MENU, reach 1** (stage 4; was TOOL). Why: they are what every Orbit session starts with (watch, set a date, look at one body, change what is drawn); Journey, history jumps, sky and trails are one click deeper inside the same panels.
- **Atom Lab:** (1) Element — CHIP periodic table; (2) Neutrons / isotope — CHIP; (3) View buttons Atom · Nucleus · Cloud · Chart — TOOL, reach 0 (counted as one: they are the lab's main switch); (4) Orbital — TOOL view options ▾ top row; (5) Cloud style and density, outline, phase colours — TOOL marks ▾ / view options ▾; (6) Emit / Absorb photon — TOOL photon ▾; (7) Play / Pause — TOOL; (8) Reset view — TOOL; (9) Zoom — wheel, RO; (10) Save picture — TOOL. Why: the first thing anyone does is pick an element and a view; orbitals and photons are what the guides teach next.
- Not one click away but two (acceptable, each used less often): Journey, history jumps, Sky, Trails, Superposition, Check a jump, spectrum cards, temperature, Settings content, Defaults, Benchmark.

## 7. Count: controls visible by default, old vs new
"Old seen" = on the first screen at 1280 × 800 (stage 1, corrected); "old in all" = including those below the fold. New numbers are counted from the design (DESIGN.md): TB 12 (☰, *Physics Lab*, lab ▾, *Solar System*, chip, search, other-lab icon, Learn, Achievements, 💡, Theme, Settings) + LEG 1 + CARD 3 (collapse, action link, All values) + TOOL.
| Screen | Old seen / in all | New by default | Reach of everything else |
|---|---|---|---|
| Orbit Lab | 29 / 96 | 12 + 1 + 3 + 11 (play, speed▾, show▾, pick▾, scale▾, 2D/3D▾, save: 7 buttons, 4 carets) = **27** (a selection adds its crumb: 28) | ≤ 2 |
| Atom Lab, any picture view | 139–141 / 202–233 | 12 + 1 + 3 + 10 (play, Atom, Nucleus, Cloud, Chart, view options ▾, marks ▾, photon ▾, reset, save) = **26** | ≤ 2 |
| Settings | 48 / 120 | one section (Display) = about 16, others one click deeper | 2 |
| Theme picker | 40 / 110 | 13 cards (as now) | 1 |
| Learn | 17 / 17 | a READ list of 7 cards + top bar | 1 |
| Achievements | 10 / 10 | a floating list of 10 + top bar | 1 |
| About, History, changelogs, guides | 33–51 / 103–129 seen incl. the panel behind (163 in the Atom guide) | **0 controls in the page itself** except the ▶ buttons of timelines and a table of contents; the picture is dimmed, the panels behind are not drawn | 2 |
Honest reading: on the Orbit Lab the default count barely changes (29 → 27); the gain is that nothing needs scrolling (the column of 96 controls and 1,919 px is gone: every control is ≤ 2 clicks), that the 6 text sizes and 28 outlined boxes go, and that the Atom Lab falls from 139 to 26.

## 8. Rules check
- **Nothing without a home:** every row above has one; the 47 body tiles, the 118 element cells, the 22 + 22 timeline steps and the 7 guides keep their content (moved, not shortened).
- **Nothing important more than 2 clicks away:** the deepest are DATA tabs and SET sections at reach 2.
- **Three things that need the owner's eye:** (a) the Atom toolbar gets a Play button the brief did not list; (b) "Orbital" has no chip of its own (it is the first row of view options ▾); an orbital chip beside the element chip is possible if the owner wants it one click closer; (c) the data sheet (DATA) is my addition so the 7 Atom info cards have a home that is not the side column.

## Audit corrections (stage 1 numbers)
The stage 1 probe skipped text drawn in ImGui's default font (a bug: `AddText` without a font). Re-run in stage 2 (same screens, 1280 × 800, Dark): see `AUDIT.md` "Corrections in stage 2" for what changed. Stage 1 also said "236 entities … About and History": the right figure is **254, in the About page only** (the pattern missed names with digits and capitals; the History page has none).


## Stage 4 (6 Oct 2026): what changed in the Orbit Lab's homes
Play / pause moved into the chip, Save picture into the menu (the toolbar now holds only view and lab controls: speed, show, pick, scale, 2D / 3D); the 💡 button became the fact line; the Time panel got Live / Set manually, 4 + More jumps and the (i); Show gained Sky (stars, constellations) and Far-away things; the Pick list got a filter and the NASA/JPL words; the subject card has one big number, 4 to 6 values, an action link and All values (the data sheet: every line of the old card, word for word); the Legend lists what the picture really draws; Performance and Benchmark are in Settings. **After this stage the Orbit Lab has no Old panel in the new UI**; the old side column is drawn only with New UI Off. The Atom Lab's rows above are unchanged (stage 5); its toolbar is empty until then (its play button is in its chip, Save picture in the menu).

## Stage 5a (6 Oct 2026): the Atom Lab's toolbar
Built: TOOL Atom · Nucleus ▾ (Nucleus / Nucleon) · Cloud ▾ (Cloud / Cloud 3D / Slice) · Chart · separator · Reset view (on the chart: "Reset tilt, zoom and position"); Centre and the Drag mode button appear only when the old bar showed them. Play stays in the CHIP. The bar under the picture, its hint line and the picture's "Less info" button are gone in the new UI: the hint line and More / Less info are at the top of the Old panel until 5b (hint → RO, More / Less → CARD collapse). Still in the Old panel: Build another, Reshuffle nucleons, view options, marks, photons, everything in the table above that says "5b".

## Stage 5b (6 Oct 2026): the Atom Lab's element chip
Built: CHIP "U · Uranium-238 / 92 protons · 146 neutrons" (singular for 1; play stays inside it) and its dropdown (38 em, under the chip): the periodic table (18 columns, cells of at least 28 u, chosen element ringed, dashed edge = no stable isotope, lanthanides and actinides below), Protons ◀ slider ▶, Neutrons ◀ slider ▶ and "usual (N = n)", the famous isotopes. The dropdown's table IS the big table; the Old panel's "Bigger table" window still works. TOOL Nucleus ▾ now holds Build another and Reshuffle nucleons (only while zoomed to the nucleus; no longer in the Old panel with New UI On). Still in the Old panel: the hint line and More / Less info (5d), the element card, view options, marks, photons.
