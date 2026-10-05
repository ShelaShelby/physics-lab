# 0.15 stage 1 — audit (Part A text size, Part B screens)

Method: the real app (0.14, Release), drawn headless (SDL offscreen driver, Mesa llvmpipe), window 1280 × 800, display scale 1.0, theme Dark, Text size Normal, DejaVu Sans
(the Linux fallback: it is WIDER than Windows' Segoe UI, so on Windows lines wrap a little less). The numbers are written down by a throwaway probe in a scratch copy of Dear ImGui
(`tools/`, not in the app). ImGui does not draw what is scrolled out of sight, so every screen was drawn again scrolled down in steps of 400 px and the results were merged.
NOT tested: a real Windows PC, Windows' own DPI scaling, Segoe UI, other themes (Aero adds decoration on top of these numbers), any speed.

## Part A — the text-size problem

**What the code does** (read in `src/main.cpp`, `src/ui/look.cpp`, and SDL 3.4.16's `SDL_windowsvideo.c` / `SDL_windowsmodes.c`, the version the build fetches):
- **DPI awareness: yes, by SDL, not by the exe.** SDL declares the process per-monitor-v2 DPI aware when video starts (hint `SDL_WINDOWS_DPI_AWARENESS` unset → `permonitorv2`), so Windows does not stretch the window.
  The exe has no manifest (`assets/physics_lab.rc` is only the icon); it relies on that runtime call. If that call fails (a compatibility override set by the owner on the .exe) Windows may bitmap-scale: not testable here.
- **Display scale:** `SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay())` (= monitor DPI / 96: 1.0 at 100 %, 1.25 at 125 %, 1.5 at 150 %) is read ONCE at start (`main.cpp:112`). It sets the first window size
  (1400 × 860 × scale), the spacing (`ScaleAllSizes`), the text (`FontScaleDpi`), the side panel (360 × scale) and the mouse mapping.
  Gaps: (1) it is the PRIMARY monitor's scale even if the saved window is on another monitor; (2) it is never read again (no handler for SDL's display-scale-changed events): moving the window to a monitor with another scale, or changing Windows' scale while the app runs, keeps the old text size until restart; (3) no minimum window size is set.
- **Does text follow the window? No.** Measured at 1000 × 640, 1280 × 800 and 1920 × 1200: identical text sizes and an identical 360 px side panel. What does change is the header: 163 px high at 1280 wide, 211 px at 1000 (33 % of a 640 px window, because the subtitle wraps), and in the Atom Lab the bar under the picture (87 → 116 px). At 1000 × 640 the Atom Lab picture gets 313 px of 640 (49 %).
- **Does text follow the Text size setting? Yes, but the panel does not.** Small / Normal / Large / Extra large = × 0.9 / 1 / 1.15 / 1.3 on every size. The side panel stays 360 px at all four.

**What "normal" text is, in pixels at 100 % scale (Normal setting)** — characters counted per size in the whole side column:

| where | main text | share of characters | other sizes in the column |
|---|---|---|---|
| Orbit Lab panel | 16 px | 93.5 % | 13, 13.5, 14, 20 |
| Atom Lab panel (all views) | **12 px** | 81–83 % | 9.5 (the periodic-table symbols, in cells 16 × 26 px), 11 (spectrum labels), 12.5, 13, 13.5, 14, 16 (5.8 %), 17, 20, 26 |
| Settings window | 16 px | 85.6 % | 13, 14, 15.5, 17, 18, 19.5, 20, 22 |
| labels drawn on the lab pictures | 12–13 px | — | — |

At the four Text size settings the Orbit panel text is 14 / 16 / 18 / 21 px and the Atom Lab panel text 10.5 / 12 / 13.5 / 15.75 px.

**What Windows' own UI uses at the same setting.** Windows 11's type ramp (Fluent, Segoe UI Variable) is Body 14/20 and Caption 12/16 effective pixels; 1 epx = 1 px at 100 %, 1.25 px at 125 %, 1.5 px at 150 %
(source: the Fluent / Windows typography pages, read here only as a web-search summary: learn.microsoft.com itself was blocked from this session, so the exact page text was NOT read).
So at 100 %: Windows body 14 px, caption 12 px; at 150 %: 21 and 18 px. The Atom Lab's main text (12 px) is Windows' caption size and its symbols (9.5 px) and labels (11 px) are smaller than anything Windows' ramp has. The Orbit Lab panel (16 px) is larger than Windows' body, and DejaVu at 16 px looks bigger than Segoe UI at 16 px (Segoe UI has the smaller x-height): on the owner's PC the same pixel size will look a little smaller than in these pictures.

**What is NOT known.** The owner's real scale % and resolution. If the owner LOWERS the screen resolution below the monitor's own, the picture gets bigger, not smaller; so "impossibly small" more likely means a high-resolution screen at 100 %, or the Atom Lab's 12 px text, or a small window. OWNER-TODO asks for: Windows scale %, resolution, screen size, window size, and which lab.

**Proposed rule (not applied; details in DESIGN.md):**
1. One unit u = 1 px at 100 %. Everything is in u × (Windows scale) × (Text size). Body text 16 u, nothing in the controls below 13 u; the 12 u and 11 u are kept only for labels drawn ON the picture.
2. Text never shrinks when the window is smaller (the layout changes instead: panel becomes a drawer, see the mockups). A new Text size "Auto" may grow text on big windows: u × clamp(window height / 900, 1, 1.4).
3. The panel width is a number of text widths (≈ 24 × body size), not a fixed 360.
4. Read the scale again when SDL reports it changed and take it from the window's own monitor; set a minimum window size.
5. Add a line in Settings that prints "Windows scale 125 % · window 1280 × 800 · body text 20 px" so the owner can tell me the numbers.

## Part B — screens (1280 × 800, Dark, Normal)

Definitions. **Controls**: distinct clickable items (buttons, tick boxes, sliders, combo boxes, tabs, text boxes, list rows); "seen" = on the first screen and not under a floating window; "in all" = including those scrolled away or under a floating window. **Outside the main control area**: seen controls that are not in the screen's own control window (the side panel, or for a floating window that window): header, tabs, the Atom Lab bar, and for floating windows the panel behind. **Bordered boxes**: distinct closed outlines at least 20 × 14 px. **Text sizes**: distinct pixel sizes of interface text (not the picture's labels). **Scroll**: how many pixels the side column (or the floating window) can be scrolled. **Paragraphs over 3 lines**: wrapped text blocks of 4+ lines shown without anyone opening anything.

| Screen | Controls seen / in all | of those outside the main control area | Bordered boxes seen / in all | Text sizes (px) | Scroll of the side column or window, px | Paragraphs over 3 lines, first screen / all | Text characters |
|---|---|---|---|---|---|---|---|
| Orbit Lab | 29 / 96 | 10 | 28 / 100 | 5: 13, 13.5, 14, 16, 20 | 1,919 | 1 / 2 | 2,203 |
| Orbit Lab, Earth selected | 29 / 99 | 10 | 28 / 103 | 5: 13, 13.5, 14, 16, 20 | 1,997 | 1 / 2 | 2,233 |
| Atom Lab: orbit view | 140 / 202 | 14 | 27 / 117 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 4,857 | 0 / 19 | 17,381 |
| Atom Lab: nucleus | 141 / 211 | 15 | 28 / 127 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 5,055 | 1 / 20 | 15,981 |
| Atom Lab: Cloud 3D | 141 / 233 | 15 | 28 / 152 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 5,936 | 0 / 23 | 17,966 |
| Atom Lab: Slice | 140 / 229 | 14 | 27 / 146 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 5,836 | 1 / 23 | 18,533 |
| Atom Lab: chart | 139 / 209 | 13 | 27 / 128 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 5,717 | 1 / 25 | 16,915 |
| Atom Lab: landscape | 139 / 209 | 13 | 27 / 128 | 11: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 20, 26 | 5,717 | 1 / 25 | 16,919 |
| Learn tab | 17 / 17 | 10 | 15 / 15 | 6: 13, 13.5, 14, 15, 16, 20 | 0 (window) | 2 / 2 | 1,423 |
| Achievements tab | 10 / 10 | 10 | 12 / 12 | 6: 13, 13.5, 14, 15, 16, 20 | 0 (window) | 0 / 0 | 638 |
| Settings | 48 / 120 | 28 | 42 / 122 | 10: 13, 13.5, 14, 15.5, 16, 17, 18, 19.5, 20, 22 | 446 (window) | 1 / 2 | 3,871 |
| Theme picker | 40 / 110 | 26 | 42 / 114 | 3: 13, 13.5, 16 | 0 (window) | 1 / 2 | 1,994 |
| About | 33 / 103 | 26 | 26 / 116 | 6: 10.5, 13, 13.5, 14, 16, 20 | 9,212 (window) | 1 / 2 | 77,886 |
| History (app changelog) | 33 / 109 | 26 | 26 / 142 | 7: 13, 13.5, 14, 15, 16, 18, 20 | 17,530 (window) | 3 / 81 | 87,220 |
| Lab changelog | 35 / 103 | 28 | 28 / 106 | 5: 13, 13.5, 14, 16, 20 | 3,654 (window) | 6 / 26 | 21,979 |
| Guide (Orbit Lab) | 51 / 129 | 27 | 42 / 122 | 8: 13, 13.5, 14, 14.5, 16, 17.5, 18, 20 | 2,847 (window) | 1 / 2 | 19,530 |
| Guide (Atom Lab) | 163 / 235 | 136 | 39 / 135 | 12: 9.5, 11, 12, 12.5, 13, 13.5, 14, 16, 17, 18, 20, 26 | 2,080 (window) | 0 / 19 | 29,057 |

Reading the table:
- The Atom Lab side column is **4,857–5,936 px of scrolling in a 637 px window**: its content is 8.6–10.3 windows tall. 118 of its ~200 controls are the periodic-table cells (16 × 26 px each); the other ~85 are real controls.
- The picture starts 163 px down (20 % of 800); in the Atom Lab 87–102 px more are taken at the bottom, so the picture is 535–550 px of 800 (67–69 %).
- "Paragraphs over 3 lines" are counted where the app draws them in the default state. I did not check which of them sit behind a button such as "Less info"; the first-screen / all split only says how many need scrolling to reach.
- On the chart and landscape pictures the legend and note boxes are drawn over the picture itself (seen in the renders; their area was not measured).

## The 5 worst problems, in plain words
1. **The Atom Lab side column is a 9-screen-tall wall**: about 200 controls (118 of them the periodic table), 19–25 long paragraphs and 15,000–18,500 characters of text, all in one 360 px column that scrolls ~5,000 px. The picture is the product, and it gets the leftovers.
2. **Text is too small exactly where the most is written**: the Atom Lab's main text is 12 px, its table symbols 9.5 px, and it uses 11 different text sizes (the Orbit Lab: 5, Settings: 10). It does not grow with the window or follow a monitor change, and the panel does not widen when the text size is raised.
3. **Controls are scattered**: the header, tabs and "Did you know?" bar (10 controls), the side panel, and under the Atom picture another bar (4); nothing says which are "choose" and which are "view". 139–141 controls are seen at once in the Atom Lab (29 in the Orbit Lab).
4. **Everything has a box**: 27–28 outlined boxes are on screen at once on the lab screens, 100–152 in the full column; boxes sit inside boxes; no space-based grouping. The changelog and About windows hold 78,000–87,000 characters.
5. **The picture is squeezed**: a header whose subtitle wraps to 3 lines (DejaVu; Segoe UI may need 2), the always-on "Did you know?" bar and the tab bar take 20 % of the height (33 % in a 1000 × 640 window); text overlays (a 4-line readout) and big legend boxes then cover the picture itself.

Also found (reported, not changed): **About and History show raw HTML entities** (`&rsquo;`, `&ndash;`, `&ldquo;` … appear as typed; 236 entities in `app_text.inc`); `html_view.cpp` has no entity decoding. Seen in the About render ("the planets&rsquo; gravity"). A fix is a small task for a later stage.
