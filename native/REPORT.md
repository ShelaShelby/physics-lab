# Native Physics Lab 0.4 — report

0.4 adds the **Atom Lab**, ported from the web version (web Atom Lab update 7.7.3). It was done in
four stages, each committed to `native` and built by GitHub Actions. The plan is in `ATOM_PLAN.md`.
The 0.3 report is in the git history (`git show e30a67a:native/REPORT.md`).

## What works (checked)

**Stage 1: physics core and tests**
- Everything the Atom Lab calculates is in `src/atom/`. It uses SI units inside, the web's own
  rounded constants (0.529 Å, 27.211 eV, 24.19 as, FM_AU) and its steps (fixed 1/240 s steps;
  h = dt·speed/Z²).
- Data is generated from `index.html`, never retyped: 3,557 nuclides, NUBASE2020 decays, binding
  energies, temperatures, categories, famous isotopes and nuclear shapes.
- `tests/test_atom.cpp` has about 17,800 checks, all passing on Linux and on the Windows build.
  The reference values come from the web's own JavaScript run in Node, with `Math.random` replaced
  by the same seeded generator the app uses. Even the random parts compare step by step:
  - the cloud dots;
  - photon jumps;
  - nucleon positions;
  - decays (which way, after how long) and the decay history.
- The tests check every nuclide's one-line description, all decay-mode words, ψ for all 30
  orbitals, the dipole rule for all 900 pairs, spectral lines, binding energies, and the readout
  and hint lines along whole scripted runs.
- To get JavaScript's exact number wording, `format.cpp` gained helpers for `String`,
  `toPrecision`, `toExponential` and `toLocaleString`. `toFixed` now rounds exact halves up, as
  JavaScript does. The Orbit tests still pass unchanged.

**Stage 2: picture and panel**
- **Views:** Orbit (Modern, and Bohr 1913 with its circles), Cloud, Cloud 3D (three depth bands,
  auto spin) and Slice.
- **The nucleus at its real size:**
  - fuzzy nucleons (the web's charge-density profile) or hard balls;
  - jitter and depth order;
  - click a nucleon to dive into it.
- **Quarks:** Classic, Accurate and Cinematic, with:
  - colour-charge swaps and the travelling gluon;
  - the gluon field and sea pairs;
  - depth fog and focus, and the 45-nucleon limit with fade in and out;
  - the dark backdrop and the mass-budget bar.
- **Drawing:**
  - cloud dots, glows and gluon particles are instanced draws (a new "blob" primitive);
  - blurry things go through a half-resolution soft layer, like the web's;
  - the Performance card shows the picture's draw calls and instanced shapes.
- **Also drawn:**
  - photons as wave packets with their labels;
  - the spectral-line strip;
  - decay flashes, flying particles and the decay log;
  - the scale bar, the readout on its plate, the ⓘ More/Less info button and the nuclide label.
- **Panel:** a lab picker at the top (Orbit Lab / Atom Lab), then:
  - the periodic table (hues, dashed edges for no stable isotope, tooltips, * and ** marks);
  - the bigger table window with its legend;
  - the isotope line, element card and facts with the temperature bar;
  - Protons and Neutrons with − and +, and Famous isotopes;
  - Build another and Reshuffle;
  - the Decay history;
  - View & zoom, Electron and Photons, with every web setting as a row of buttons with its note;
  - the Light spectrum picture and note;
  - Temperature, with the slider, state line, atom speed, particle box, four jump buttons and
    "What is temperature, really?".
- **Under the picture:** ⌕ Atom / Nucleus / Nucleon, ⌖ Centre, ✥ Drag, ↺ Reset view, and the hint.
- **Visual check:** the web page was screenshotted in headless Chromium and compared with the
  native app.
  - The fuzzy-nucleon brightness profile matches within a few levels (peak 138 native, 135 web).
  - The Cinematic and Classic quark views look the same, though not pixel for pixel, as the
    nucleon placement is random on both.

**Stage 3: Learn and texts, word for word**
- The four guides:
  - **What am I even looking at?** with the element bar, view chips, jump bar and 8 sections,
    including the live readout;
  - **Scale**, with both tabs and all five objects;
  - **Weight**, with five tabs;
  - **the timeline**: 22 steps, with the web's small icons drawn from their SVG. Its 11 ▶ buttons
    set up the lab and close the guide (checked with a real click: radium-226).
- The guides' text is made by C++ functions. The tests compare them with the web's own `lrnLook`,
  `lrnScale` and `lrnMass` run in Node with a small fake page, for 17 atoms and all views
  (imperial too).
- The panel's texts are compared with what the web's own `buildUI` writes for 20 isotopes:
  - isotope line, element card, facts and spectrum note;
  - temperature and atom-speed lines;
  - decay history.
- For all 118 elements, the card, the facts and "Where you meet it" are compared in both units.
- Metric/imperial follows ⚙ Settings wherever the web converts. This covers °F, mph, miles, feet
  and inches in the Scale guide, "20 °C / 68 °F", and the km/h and °C inside the element notes.

**Stage 4: polish**
- **Defaults** asks "Put Atom Lab back?" first. Checked with real clicks:
  - "Yes" gives hydrogen-1, the Orbit view and a fresh atom;
  - "Cancel" keeps everything.
- **Reset** (the web's top-bar button) starts a fresh atom with the same settings.
- **More/Less info** is remembered between runs. Checked: saved on click, restored on the next start.
- The **Performance** card is on the Atom Lab panel too.
- **Changelog:** the 0.4 entry, plus the web Atom Lab history (27 entries and its note) for reference.
  The version is 0.4.
- **Orbit Lab:** the same as 0.3. Its Performance card was moved into a shared function, unchanged,
  and the lab picker sits above its title. Its tests still pass.

**Also checked with real mouse input:**
- switching labs and clicking an element;
- dragging to turn the orbit (the axes and ↺ Reset view appear);
- the wheel zoom (0.4 per notch, as the web's 0.004 per wheel unit);
- the ⓘ button.

## Not tested / be aware
- **On Windows:** stages 1–3 built and passed their tests on GitHub's Windows runner (check that
  the `latest` release says "native 0.4 stage 4"). But I have not run the Atom Lab on a real
  Windows PC with a graphics card.
  - The new parts that only a real GPU will prove are the blob shader, the offscreen soft layer
    and additive blending.
  - Please try the deep zoom (⌕ Nucleon with Cinematic quarks) and a big nucleus (uranium) at
    Nucleus zoom.
- **Speed:** not judged here (no GPU). The Performance card now shows draw calls, so you can see
  the instancing at work on your PC.
- **One rounding tie:** the uranium-233 orbit radius is exactly 0.575 pm. MSVC's sin/cos round
  its last bit differently, so the Windows build shows 0.58 pm where the web shows 0.57. The test
  allows exactly this case (off by one in the last digit) and prints a note when it happens.
- **Emoji** in buttons (🎲, 🏠, 🔥, 💨, 🌡, 🔟 …) show only if Segoe UI Symbol has them. Any that
  are missing are left out, so the words still read correctly.
- **Layout:** checked at one window size on Linux, not on a small screen or a 4K screen.
- **Mouse wheel:** the zoom step assumes the usual 100 units per notch in the browser.
  Touchpads and pinch were not tried.

## Still different from the web Atom Lab
- **Text layering:** labels and the u/d letters are drawn on top of the picture, so a letter can
  sit over a nucleon nearer to you. On the web they are drawn in depth order.
- **Rings:** antiquark rings in Cinematic are drawn with normal blending, not "lighter".
- **Timeline icons:** their gradients are drawn as rings of colour.
- **Line breaks:** the readout wraps by the native font's widths, so it may break at slightly
  different places.
- **Settings memory:** the lab's settings are not remembered between runs; only units and
  More/Less info are. The web remembers every setting of every lab. (Orbit Lab has the same
  difference.)
- **App-wide web features not ported:**
  - achievements (the decay one);
  - the theme picker, so the guide's "monochrome" word is plain bold text and not the hidden
    theme switch;
  - reduced-motion mode;
  - text size settings;
  - touch pinch-zoom.
- **Theme:** the picture follows Windows' light or dark theme. The panels stay dark, as in Orbit Lab.
- **Temperature box:** the particle box uses its own random numbers. It looks the same, but the
  particles do not match the web's.

The Atom Lab is done. I have not started another lab.
