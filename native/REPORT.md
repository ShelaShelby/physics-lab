# Native Physics Lab 0.15 "Full UI rework" — stage 1: audit, design system, mockups (no screen rebuilt)

**Docs, tools and mockup pictures only.** Not one line of app code, test, asset, data or workflow changed (`git diff origin/native` outside `native/docs`, REPORT, README, RESUME, ROADMAP, OWNER-TODO: empty). `index.html` untouched (sha256 e5501387ae64…). No helper agents. Everything is in `native/docs/ui-0.15/`: `AUDIT.md` (Part A and B in full), `DESIGN.md` + `design_tokens.json` (Part C), `mockups/` (Part D), `tools/` (throwaway probe and scripts, `README.md`).
**NOT tested: a real PC, Windows' own DPI scaling, Segoe UI, any speed.** Numbers are from the real app (Release) on Linux, SDL offscreen + Mesa llvmpipe, 1280 × 800, scale 1.0, Dark, Text size Normal, DejaVu Sans (wider than Segoe UI); other themes were not audited.

## Part A — text size (numbers; rule only proposed)
- **DPI:** SDL 3.4.16 itself declares per-monitor-v2 awareness on Windows (the .exe has no manifest). The app reads `SDL_GetDisplayContentScale` of the PRIMARY monitor ONCE at start and uses it for fonts, spacing, panel width (360 × scale) and window size. It never reads it again (no scale-changed handler: moving the window to another monitor keeps the old size) and sets no minimum window size.
- **Text does not follow the window** (same sizes at 1000 × 640, 1280 × 800, 1920 × 1200); it follows Text size (× 0.9 / 1 / 1.15 / 1.3) but the panel stays 360 px at all four. The header grows in small windows (163 px at 1280 wide, 211 px at 1000: 33 % of a 640 px window).
- **"Normal" text:** Orbit Lab panel **16 px** (94 % of its characters); Atom Lab panel **12 px** (82 %), table symbols 9.5 px in 16 × 26 px cells, labels 11 px. Windows' own type ramp: body 14 / caption 12 epx (1 epx = 1 px at 100 %). I could only read this as a web-search summary (learn.microsoft.com was blocked), not the page. At 150 %: 21 / 18 px.
- **Unknown:** the owner's real scale and resolution (OWNER-TODO). Proposed rule (AUDIT.md): unit u × Windows scale × Text size, body 16 u, nothing under 13 u, text never shrinks with the window, panel width in text widths, re-read the scale, Settings shows the numbers.

## Part B — audit table (full definitions and all text sizes: AUDIT.md)
| Screen | Controls seen / all | Outside main area | Boxes seen / all | Text sizes | Scroll px | Paragraphs >3 lines (first screen / all) |
|---|---|---|---|---|---|---|
| Orbit Lab | 29 / 96 | 10 | 28 / 100 | 5 | 1,919 | 1 / 2 |
| Orbit Lab, Earth selected | 29 / 99 | 10 | 28 / 103 | 5 | 1,997 | 1 / 2 |
| Atom Lab: orbit view | 140 / 202 | 14 | 27 / 117 | 11 | 4,857 | 0 / 19 |
| Atom Lab: nucleus | 141 / 211 | 15 | 28 / 127 | 11 | 5,055 | 1 / 20 |
| Atom Lab: Cloud 3D | 141 / 233 | 15 | 28 / 152 | 11 | 5,936 | 0 / 23 |
| Atom Lab: Slice | 140 / 229 | 14 | 27 / 146 | 11 | 5,836 | 1 / 23 |
| Atom Lab: chart | 139 / 209 | 13 | 27 / 128 | 11 | 5,717 | 1 / 25 |
| Atom Lab: landscape | 139 / 209 | 13 | 27 / 128 | 11 | 5,717 | 1 / 25 |
| Learn tab | 17 / 17 | 10 | 15 / 15 | 6 | 0 (window) | 2 / 2 |
| Achievements tab | 10 / 10 | 10 | 12 / 12 | 6 | 0 (window) | 0 / 0 |
| Settings | 48 / 120 | 28 | 42 / 122 | 10 | 446 (window) | 1 / 2 |
| Theme picker | 40 / 110 | 26 | 42 / 114 | 3 | 0 (window) | 1 / 2 |
| About | 33 / 103 | 26 | 26 / 116 | 6 | 9,212 (window) | 1 / 2 |
| History (app changelog) | 33 / 109 | 26 | 26 / 142 | 7 | 17,530 (window) | 3 / 81 |
| Lab changelog | 35 / 103 | 28 | 28 / 106 | 5 | 3,654 (window) | 6 / 26 |
| Guide (Orbit Lab) | 51 / 129 | 27 | 42 / 122 | 8 | 2,847 (window) | 1 / 2 |
| Guide (Atom Lab) | 163 / 235 | 136 | 39 / 135 | 12 | 2,080 (window) | 0 / 19 |
**The 5 worst problems:** (1) the Atom Lab side column is 5,000+ px of scrolling (9–10 windows tall): ~200 controls, 118 of them periodic-table cells, 19–25 long paragraphs; (2) the Atom Lab's main text is 12 px, it uses 11 text sizes, and neither window size nor panel width follows the text; (3) controls are scattered: header, tabs, "Did you know?" bar, side panel, the bar under the Atom picture; (4) a box around everything: 27–28 outlined boxes at once, 100–150 in a full column; About and History hold 78,000–87,000 characters; (5) the picture gets what is left (48–57 % of the window, 43 % at 1000 × 640) and the legend / readout text is drawn over it.

## Parts C and D
- **DESIGN.md:** spacing 4/8/12/16/24/32 u; text Caption 13 / **Body 16** / Title 20 / Display 28 u (minimums per Windows scale in the file); one button shape, Primary / Secondary / Quiet; icons **Lucide** (ISC, licence in `mockups/assets/`); 3 surface levels, no outlines (edges of text-less controls computed to 3:1); grid; summary ≤ 140 characters + More / ⓘ; groups Choose / View / Info; tips: the bar is replaced by an icon + one card + an optional toast; all 14 themes map onto the 7 flat colours they already have (WCAG table computed by `tools/theme_contrast.py`: muted text ≥ 4.99:1 everywhere; the theme line colour is only 1.3–1.8:1 in most themes, so control edges are computed).
- **Mockups** (`native/docs/ui-0.15/mockups/`, 24 PNGs, largest 187 KB, 2.9 MB in all): `A_*` rail + panel with tabs, `B_*` top bar + bottom drawer, `C_*` picture first with a subject card and popovers; each as `<layout>_<orbit|chart>_<dark|aero>_<1280x800|1000x640>.png`, e.g. `A_orbit_dark_1280x800.png`. Lab pictures in them are the app's real pictures; words and numbers are the app's own. Per-layout paragraph, honest comparison and a suggestion (A as base + C's card on click, NOT a decision) are in `mockups/README.md`. Limits: one tab and 14–26 sample controls per picture, other screens and the element picker not drawn, Aero glass approximated, Inter font (free) not Segoe UI.

## Found, not fixed (for later stages)
- **About and History show raw HTML entities** (`&rsquo;`, `&ndash;`, `&ldquo;` …; 236 in `app_text.inc`): `html_view.cpp` has no entity decoding. Seen in the About render.
- The scale is read once from the primary monitor; no minimum window size (Part A). Monochrome's line colour is 2.10:1 on its page (needs the computed edge in 0.15).

## Tests
Linux GCC Release, Ninja, `-DPL_BUILD_APP=ON -DPL_GL_TESTS=ON -DPL_PICTURE_TESTS=ON`, run on the unchanged code before and after the docs were added: **ctest 14 / 14 pass** (orbit, atom, app, nbody, jpl, moon, moon_real, glsl_shaders, windows_decorated, gl_phase_colours, theme_pictures, window_pictures, y2k_pictures, effects_pictures; the picture tests keep 0 differing pixels). Test counts are those of 0.14. The audit probe and mockup code live only in a scratch copy / in `docs`; none is reachable from the app.
