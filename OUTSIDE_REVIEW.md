# Physics Lab: harsh outside review
Reviewed: `native` at **0.60.0** (commit `c3ae816`), 18 Nov 2026. Reviewer: a third, independent agent (no code changed, nothing pushed).
Screenshots referenced below are in `review_shots/` (Linux, Mesa llvmpipe, 1280x800 unless noted).

## What I actually did (so you can weigh the verdict)
- Fresh build (OpenGL + Vulkan), `ctest -L quick`: **18/18 pass**.
- AddressSanitizer + UndefinedBehaviorSanitizer build of all 17 physics/logic test programs: **clean**.
- The full app under ASan/UBSan, every lab, OpenGL and Vulkan: **clean**. Vulkan with validation layers (4 scenes): **0 errors, 0 warnings**.
- About 40 screenshots of all four labs, Settings, About, Learn, small window (880x560), Vulkan.
- Read the physics modules and recomputed numbers by hand: Page-Thorne flux and roots, Bardeen-Press-Teukolsky orbit formulas, Luminet's printed √3/3 typo (the app found and handles it), ISCO and efficiencies (5.72 %, 26.4 % at a = 0.99), the hover clock, blueshift and g⁴ brightening, shadow size (28.54° at 20 M, 53.3 µas for Sgr A*), ZAMO lapse at a = 0.99, frame-dragging period (145 d), Morse closed-form motion, U-238 decay odds, H 1s ⟨r⟩, CODATA 2022 constants in the calculator (13 checked), CIE glare equation, disc colour against my own black-body → sRGB integration (it matches about 40,000 K), all 3,557 nuclide half-lives in `atom_data.inc` against the NUBASE2020 table (**3557/3557 agree**), every "Did you know" fact, planet data.

## Verdict
The **physics core is honest and mostly right**. I went looking for made-up math and mostly did not find it: formulas are cited to the equation, simplifications are labelled, made-up dots (Oort cloud) say so. That is rare and deserves credit.
The app's real problems are elsewhere: **(1) the Black Hole Lab's pictures are physically defensible but visually bad and badly framed by default, and its readout card covers the picture; (2) the UI is drowning in text and duplicated controls; (3) several default views show almost nothing; (4) the project burns effort on speed targets, API migration and process instead of finishing labs.** Details and evidence below, worst first.

---

## A. Bugs (concrete, reproducible)
1. **"Brightest star: 0 × fainter than the disc's brightest part"** (Black Hole Lab, Sky beside the disc: True brightness). `blackhole_view.cpp:466-470`: on the graphics-card exposure path `st_.look` is reset to `DiscLook{}` (brightY = 0), then `trueNumbers(v)` at line 488 divides with it, so `fainter = 0`. The log shows `bhtrue fainter=0`. This is the default path on any PC whose card passes the start-up check. Shot 04.
2. **The readout card sits on top of the picture and hides the subject.** At 1280x800 it covers the right edge of the shadow, and with spin it covers the **approaching, bright side of the disc**, the one thing the spin/Doppler physics is there to show (shot 05). At 880x560 it covers **the whole black hole** (shot 01). The hole is centred in the full width instead of the free area.
3. **The lab tabs disappear from the header** when the Black Hole subtitle gets long (feeding preset, Film, spin): "Orbit Lab / Atom Lab / Molecule Lab / Black Hole" become one icon. Navigation depends on how long a sentence is (shots 02, 03).
4. **Vulkan: the Black Hole Lab's note "The black hole's picture comes to Vul…"** is cut off behind the same card (shot 09).
5. **Film look on Sagittarius A\* contradicts itself.** The picture shows a bright 4,500 K disc, the header says "Sagittarius A\*", and the panel right next to it says its gas "is not a bright disc, and is not shown here". `kWhatIf` ("this black hole does not have one") is shown in Real but explicitly skipped for Film (`shell_blackhole.cpp:163, 392`: `!film`). Shot 02.
6. **Two UI tests have been failing since 0.58/0.59** (`ui_popups`, `ui_molecule`), and RESUME.md normalises it ("32 of 34 … as in 0.59.0"). A failing suite carried for versions stops being a safety net.
7. **The owner's own Radeon crashes on the Black Hole graphics-card path** (since 0.55.6); it is switched off by the guard. So the person who sets the 255 Hz target is running the processor fallback of the lab being tuned.
8. Dead code: Orbit Simple mode deletes bodies inside `kCrashRadius` = 0.15 AU and counts `crashed`/`escaped`, but nothing can get there (no user-launched bodies) and nothing reads the counters.

## B. "Fake physics / made-up visuals": what is actually wrong or misleading
Nothing I found is invented math. These are the places where the **picture tells a learner something false by default**, even if a caption somewhere says otherwise:
1. **Two exposures composited by default (Black Hole Lab, Real look, disc on).** "Sky beside the disc: Shown" is the default: the Milky Way is drawn on its own exposure, about 10¹³ times too bright next to the disc. Result: the lensed sky is **brighter than a 40,000-70,000 K accretion disc** (shots 03, 05). The only label is a caption under a sub-heading. A look called **"Real"** should not default to this; make True brightness the default in Real, or rename the default.
2. **"Real" ignores what the camera's own motion does to the sky.** The readout says "1.054 × bluer, 1.235 × brighter (not drawn)". At the closest distance (2.2 M) that is 3.3 × bluer and about 120 × brighter: the sky's colours would change completely. Labelled "not drawn", but then the look should not be called Real without a qualifier.
3. **Atom Lab, Orbit view, style "Modern"** draws an electron going round a circle (shot 08). The small print says it is "Bohr's 1913 model beside a modern nucleus", but the button says **Modern**. A learner will read "modern orbit". Rename (e.g. "Bohr orbit, modern nucleus") or drop the circle in that style.
4. **Nucleon view gives a dot two meanings on the same screen**: the top text says "the dots show where its quarks are found", the legend says "each dot: a piece of the momentum" (shot 10). They are different quantities (the legend is the correct one for the tomography data). Pick one.
5. **Quark colours.** Colour charge drawn as red/green/blue is a convention, not a colour. Check that the Simple style says so as clearly as it labels the flux tubes.
6. **Orbit fact "Neptune: winds over 2,000 km/h, the fastest known"** is false: exoplanet winds are measured far faster (hot Jupiters, several km/s). Say "fastest in the Solar System".
7. Belt asteroids and Trojans in Simple mode move on fixed Kepler orbits (no Jupiter), so over centuries of sim time the Trojans leave L4/L5. ROADMAP knows ("far from 2026"); the picture does not say it.
8. Stale wording in `realest.cpp:60`: Bending "calculated exactly for a hole that does not spin". Spin has been calculated since 0.55.2.

What I checked and found **correct** (so nobody "fixes" it): the disc's pale blue (true colour of 25-70 kK black bodies), the auto exposure (the white is the brightest *channel*, about 2 × the brightest Y, so nothing clips), Titan e = 0.0292 and Enceladus 0.0045 (NASA fact sheet, I fetched it), all the "Did you know" facts except Neptune's, U-238's per-minute decay chance, the shadow and clock numbers.

## C. Default views that show (almost) nothing
- **Atom Lab, whole atom at True scale** (the "Most realistic" setting): iron's Cloud is a white blob about 20 px wide in a 900 px picture at a 5 Å scale (shot 06). Sodium's Slice is a dot (shot 07). The auto-fit frames the far tail, so the atom fills about 10 % of the view and the core saturates. "Realistic" should not mean "invisible": fit to the 90 % radius and tone-map the core (ROADMAP's "owner decides" tone curve; decide it).
- **Black Hole Lab, feeding preset**: the camera at 20 M sits inside a disc that reaches 2,000 M, so the disc is a flat lavender wall over half the sky, washed further by the glare (shot 03). Physically defensible, useless as a first picture. The preset should open at the classic view (about 30-75 M, nearly edge-on, outer edge about 30 M), where the lensed far side and the Doppler asymmetry are visible.
- **Film look at its default 20 M**: a blown-out cream wash (shot 02). Selecting Film should jump to the paper's view (74.1 M, 86.56°), which looks right (I rendered it).
- The Atom Lab's Cloud 3D is the default "Picture"; the first thing a user sees of hydrogen is a blue fuzz. Fine for H, but there is no "hero" view per lab.

## D. UI / UX
- **Wall of text.** Every control has one to four sentences of caption permanently visible; the Black Hole panel is mostly prose. Readout cards are 15-20 lines. Put explanations behind an (i) or in the guide, keep one line under a control.
- **The header subtitle is a paragraph** ("A calculated model: no spin; the standard thin disc, real for quasars and small black holes feeding from a star; dimmed, an average eye's glare · our sky, seen from Earth"). It pushes out the navigation (bug A3).
- **Duplicate controls**: the bottom toolbar repeats the panel's Picture row (Atom) and Style row (Molecule), and "Most realistic" plus blue "realistic" dots on every option is a third way to say the same thing.
- **Random facts strip** is unrelated to the lab ("Diamond and graphite…" on hydrogen, "Lightning…" in the nucleon view, "Escape velocity of Earth" under a black hole). Either tie facts to the lab or drop the strip; it also overlaps the picture.
- **Language.** The app's English is consistently odd and machine-flavoured: "the card's light paths", "the processor's path", "Glare draws the veil an average eye would see", "Starlight reaching you … (not drawn)". Learners will stumble. Get one pass by a human editor.
- **Imbalance in Learn**: Orbit has 3 guides, Atom 4, Molecule 1, Black Hole 1.
- Time keeps running behind the Settings window. Minor, but surprising.
- Legend text in the nucleon view sits on top of the dots.

## E. Features: too little here, too much there
- **Black Hole Lab has too many knobs for one static camera**: Look (Real/Film), Light (eye/all), Sky beside (Shown/True), Glare, Exposure auto/manual, Bending, Sky ours/black, disc, feed, outer edge, spin, mass, three presets. Yet it **lacks the basics a learner expects**: the camera cannot orbit or fall in, the gas does not move (planned), and the special radii (photon orbit, ISCO, horizon) cannot be drawn on the picture. Cut options (All light, the manual exposure) until those exist.
- **Molecule Lab is thin**: 15 fixed molecules, no way to build or pick another, no reactions, no orbitals (HOMO/LUMO), no symmetry elements; one guide. It is the weakest lab for "university and PhD level".
- **Orbit Lab** has no user action beyond time, zoom and picking: no launch or transfer orbit (a Lambert or porkchop tool is on ROADMAP), no Lagrange points.
- **Atom Lab is the opposite**: 5 pictures × 3 zooms × Hydrogen-like/All × Relativistic/Non × Shells enlarged/True × Classic/Density glow × orbitals × photons × Zeeman × chart. Powerful, but no guided path; a newcomer cannot tell what matters.
- The calculator and Data panel (fits checked against NIST StRD) are genuinely good; they are hidden behind small top-right icons.

## F. Vulkan: was it the right call?
My honest view: **right destination, wrong moment.**
- **For it**: AMD's Windows OpenGL drivers are notoriously fragile, and the owner's Radeon already dies linking the big GL shader (A7). Vulkan (or D3D) is the realistic fix for that, and the work so far is clean (0 validation errors, guarded, OpenGL untouched).
- **Against the timing**: it started on 18 Nov with the Black Hole Lab unfinished (about 12 ROADMAP items, several marked **fix**), two UI tests red, and the moving gas promised. Steps 3-5 mean re-writing every compute path, so every card program is maintained twice until step 5. Most slow scenes on the owner's list are **processor-bound** (tables per frame, triangle sorting, grid read-backs, the 132-azimuth edge on the frame thread): a new graphics API does not touch those.
- **Recommendation**: freeze at step 2. Do step 4 (Black Hole programs) only, since that is where the owner's crash is. Fix the processor-bound frames first. **Never remove OpenGL** (step 5): older Intel/VM/remote-desktop machines lack usable Vulkan, and for an education app "it starts everywhere" beats 255 Hz.
- The **255 Hz target is the wrong north star** for a physics-teaching app. "Never below 60 Hz on a mid-range PC, and smooth while dragging" is a target users feel; 255 Hz on one 255 Hz monitor is not.

## G. Process and code health (harsh, but you asked)
- **Priorities flip daily** (16 Nov speed first; 17 Nov look first; 17 Nov evening the order again; 18 Nov Vulkan). The Black Hole Lab went 0.55.0 → 0.58.0 in three days with about 20 commits, and its ROADMAP section still has many **fix** items. Finish one lab's look before the next initiative.
- **Comments are a changelog**: 3,139 lines of `src` mention a version ("0.46.6 (outside review: …)"); 1,584 source lines are longer than 200 characters. Version history belongs in git; comments should say what the code does now.
- **135 distinct `PL_*` test hooks** (environment variables) live in the shipped binary, most of them in `main.cpp`. Move them behind a test build flag or one parser.
- **Generated data with no generator**: `atom_data.inc` and `learn_data.inc` say "the tool that first wrote it is gone". `learn_data.inc` holds the Orbit guide **copied 8 times** (one per view variant). I checked: the copies agree today, but every edit must be made 8 times.
- The rules (CLAUDE.md, quiet chat, three-word status replies, Haiku delegation, at most 4 screenshots) optimise token cost, not quality. A fix job allowed only 4 looks will miss exactly the framing problems in C.
- The tests are good physics tests. There are **no tests of what a user sees by default** (is the subject visible? is it covered? is it framed?). Bugs A2, A3, A4 and section C would be caught by one "subject's bounding box is inside the free area and at least N % of it" check per lab.

## H. What I would do next, in order
1. Fix A1 (0 × fainter), A5 (Film on a real hole), and the stale/misleading labels in B3, B4, B6, B8. Small and quick.
2. Black Hole layout: centre the hole in the free area, card collapsed by default on narrow windows, header subtitle to one short line, tabs never hidden (A2-A4).
3. Better defaults (C): feeding and Film open at a readable view; Real defaults to True brightness; whole-atom True scale framed and tone-mapped.
4. Cut text by half (D) and remove duplicate controls; tie facts to labs.
5. Get the two red UI tests green, and add a "subject visible and framed" check per lab.
6. Vulkan: step 4 only for now; keep OpenGL permanently; re-state the speed target as 60 Hz minimum on a mid-range PC.
7. Then features: moving camera and moving gas in the Black Hole Lab; more molecules and orbitals in the Molecule Lab.
