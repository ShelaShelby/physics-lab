# Native Physics Lab 0.11 (alpha) — stage 2 of 4: the chart of nuclides, flat and clickable

New Atom Lab view **"Chart of nuclides"** (View list). `index.html` untouched (sha256 e5501387ae64…). No helper agents. Stages 3 and 4 not started. The 3 deferred optimisation items stay deferred.

## The version bump
`CMakeLists.txt` `project(... VERSION 0.10.9)` → **0.11**. Nothing else carried the number as a version: the window title, About line and benchmark result read `PL_VERSION`; every other "0.10.9" is history text or a comment; no test asserts a version string. README says "0.11 alpha (stage 2 of 4)".

## The data: what the lab had, what was fetched
- **Had** (`atom_data.inc`, NUBASE2020 through mendeleev): **3,557 nuclides** (Z 1-118, N 0-177), each with a code: 251 stable, 32 unbound (no time given), 473 "?" (no number), 113 "~" (approximate), 2,688 a half-life; a decay entry (modes, odds, 469 with odds "not measured") for every unstable one. **Lacked:** spins, and whether a missing half-life is an extrapolation, a limit or nothing. The slider's **4,252** is its range (3 either side of the data), not the data: the chart draws **3,557**, no more.
- **Fetched** (`curl`, same GET as `tools/make_nubase_data.py fetch`): `https://www-nds.iaea.org/amdc/ame2020/nubase_4.mas20.txt`, **2026-10-04**, HTTP 200, **761,906 bytes**, 5,868 lines, Last-Modified 11 Nov 2022, **sha256 1585a5ee…9df9859**. Saved as received in `data/nubase/` with a README. Citation (the file's own first line): Kondev, Wang, Huang, Naimi, Audi, Chin. Phys. C45, 030001 (2021). **The page that states citation terms could not be read** (bot challenge, 403; amdc.in2p3.fr no answer); OWNER-TODO asks. Table `nubase_data.inc` (126 KB): spin text, kind of half-life, and for stable nuclides the limit and the "?" entries.
- **The two sets are identical:** NUBASE's 3,558 ground-state lines = the free neutron + the lab's 3,557 (the tool refuses to build if one differs).

## Findings in existing data (reported, not changed)
- The lab's "?" (473) hides: 369 NUBASE extrapolations (#), 84 with no half-life, 11 limits, 6 approximate, 3 "p-unst". `isoInfo` says of all "seen only fleetingly": an over-claim; the chart's hover uses NUBASE's kinds instead.
- The 113 "~" cannot be reproduced from NUBASE (most are plain values there). Kr-78 and Xe-124: the lab has numbers, NUBASE says stable with a limit. In 29 nuclides the lab's odds hold an unquantified remainder that NUBASE marks "?".

## The colour rule (stated in `chart.h`, the legend, About)
Stable = the lab's table says stable (251). Else the **kind with the largest total odds** (beta-delayed branches add to their beta; the first kind wins a tie); odds not measured → the kind if all modes agree, else grey "several ways". Counts: stable 251, β⁻ 1,401, β⁺/EC 1,112, α 479, fission 59, proton 104, neutron 30, grey 121. My first draft (single most likely mode) was wrong for Te-108 (51 % β⁺ split in 3 branches vs 49 % α); the cross-check below found it.
**Stable vs observationally stable:** the data does distinguish, so the hover shows it: 253 "stbl" in NUBASE, **71** with a half-life limit and/or decay entries marked "?" (shown as written; the file's header does not define "?"). Ca-48 and Bi-209 have measured half-lives and are coloured by their decay.

## What is on the chart
Tiles, colours, 14 magic bands (7 columns + **6 rows: Z = 126 has no row**, the chart ends at 118), axis numbers, legend (words and counts) in the corner with more room (full words if they fit, else names), a status line naming the chosen nuclide, box + cross-hairs on it. Hover = **tooltip** (name, Z/N, stable or half-life, modes with odds or "odds not measured", spin with NUBASE's mark, charge radius, quadrupole moment, "no value" lines). Click = `chooseNuclide(Z, N)`. Arrows (`main.cpp`) move one proton/neutron **only onto an observed nuclide** (from H-1 "up" does nothing: He-2 does not exist). In this view `step` returns early, the **decay clock waits** (a short-lived nucleus would walk away from the marker), Emit/Absorb say why, Auto waits silently.

## Tests (Linux GCC Release): orbit 1,540; **atom 18,987 (+98)**; **app 1,526 (+6)**; nbody 37; jpl 55; moon 24; moon_real 59 — all pass
Typed from the NUBASE file by a separate script: 3,558/3,557, 118 per-element counts, spin and half-life text of 15 nuclides, the kind counts. Colour counts from a separate Python implementation; cross-check done once against NUBASE's own decay column: **3,054 of 3,083 agree**. Tile set both ways (0 cells differ); 16 chosen colours; layout at 4 sizes (inside, no gaps, round trips, 1 %/99 % corners); free room measured, tight to one square; click on 10 nuclides sets exactly Z and N; arrows (edges, H-1, Fe-56, Z 118); decay clock and Emit; hover for 15 nuclides incl. missing data and a limit, every tile has spin, half-life and decay lines. Drawing through a **recording stand-in for the renderer**: 3,557 squares at their places and colours, the 26 magic edges, the marker, the legend's words, no legend box over a tile in 5 window sizes. About lists NUBASE (edit count 20 → 21) and the benchmark count 16 → 17: both changed on purpose. No `long double`, no byte hashes. Nothing existing moved or was loosened.

## Perturbations (each reverted, files compared)
β⁻/β⁺ swapped: 9 failures. Single-mode rule: 2. Click transposed (Z/N swapped): 4. Arrows without the edge check: 5. **Magic line off by one: first try 0 failures** (the left edge drawn where the right edge is passed "is it on an edge"); the test now needs both distinct edges: 1 failure. All caught.

## Speed (in principle, not timed)
The tile table is built once; per frame about 3,600 `rect` calls into the renderer's one triangle batch (about 22,000 vertices) and about 100 strings of text; hover words are built only for the hovered tile; the chart view runs no cloud sampling. **Benchmark scene 19 "Chart of nuclides: the whole chart, 3,557 squares"**: compare CPU frame time with GPU time (expected near idle); keep the pointer off the picture.

## Not compiled, not seen
`atom_view.cpp`, `atom_panel.cpp`, `main.cpp`, `benchmark.cpp`, `changelog.cpp` do not build here, but were **syntax-checked against the real Dear ImGui v1.92.9b and SDL 3.4.16 headers** (clean; `benchmark.cpp` has old missing-initializer warnings): not linked, not run. Never seen: the real app. The pictures I looked at are my own software replay of the same rectangles and text (DejaVu fonts, not the app's) at 900×700, 640×480 (before the legend was made to fit) and 1280×720. Unverified: the tooltip's wrapping and glyphs, the arrow-key handler, windows below 640×480 (the legend may still cover a little), the panel with the view's controls hidden.

## Windows CI result
Run 37218607916 on commit 76f727f (Native Windows build, windows-latest, Visual Studio 64-bit): **success**. Configure, Build (the whole app: `atom_view.cpp`, `atom_panel.cpp`, `main.cpp`, `benchmark.cpp`, `changelog.cpp`, `chart_draw.cpp` **compiled on MSVC**) and "Physics unit tests" (all seven programs, including the chart tests with their recording stand-in) passed. I did not read the printed counts from the log, so "18,987 on Windows" is not claimed; the step's success is. Compiling is not seeing: the chart has never been looked at in the real app (OWNER-TODO).
