# 0.15 design system (stage 1: a spec, NOT applied; the app looks and behaves as in 0.14)

The same numbers are in `design_tokens.json` (data, not read by the app yet). Goal: AniList-like calm: big picture, few text sizes, space instead of lines, details on request. Nothing here changes physics, data or features.
Unit **u** = 1 px at Windows scale 100 % and Text size Normal. Every size below is multiplied by (Windows scale × Text size 0.9 / 1 / 1.15 / 1.3). Rules for scale and text size: AUDIT.md, Part A.

## 1. Spacing — 4 · 8 · 12 · 16 · 24 · 32 u (nothing else)
8 between related controls · 16 between a title and its content · **24 between groups** (this replaces the outline around a group) · 16 inside a panel · 24 page margin · 32 between big blocks.
Control height 36 u (a 16 u text line + 10 u above and below; chosen, not copied). Click targets never below 32 × 32 u (the periodic table's 16 × 26 px cells go: the element is chosen from a big picker, see §8).

## 2. Text — four sizes, two weights
| name | size / line | weight | role | at 100 % · 125 % · 150 % |
|---|---|---|---|---|
| Caption | 13 / 18 | 400 | units, hints, tooltips, labels drawn on the picture | 13 · 16.3 · 19.5 px |
| **Body** | **16 / 24** | 400 | every control label, readout line, paragraph | **16 · 20 · 24 px** |
| Title | 20 / 26 | 600 | section and card titles | 20 · 25 · 30 px |
| Display | 28 / 34 | 600 | the one name of the screen (element, planet, nuclide) | 28 · 35 · 42 px |
Minimum anywhere in the interface: **13 px at Normal, 12 px at Small** (today the Atom Lab uses 12 px for its main text and 9.5 px in the table; the audit has 11 sizes there). Numbers use tabular figures. Body is bigger than Windows' own body (14 epx; AUDIT.md says how that was sourced) on purpose: the owner finds today's text too small.
Font: **Inter** (SIL OFL 1.1, free, bundled; the mockups use it) with Segoe UI (Windows' own, used from the PC, never shipped) and the existing Atkinson Hyperlegible / Nunito choices kept in Settings → Font. Text size and Windows scale multiply as today; the panel width follows the text (§5).

## 3. Buttons — one shape, three looks
Height 36 u, radius 8 u, 16 u side padding, label Body semibold, **no outline**.
- **Primary**: accent-fill with on-accent text. At most one per group and one in view (Play, Emit, Jump).
- **Secondary** (the default): face colour (mix of card and text, 12 %), text colour.
- **Quiet** (text): no fill, accent-coloured text, a 10 % accent fill when hovered. For "More", "Reset", links and icon buttons (36 × 36, 20 u icon).
States: hover +8 % tint, pressed +16 %, focus = 2 px accent ring (always visible for keyboard use), disabled = 40 % text. Segmented control (view mode, Metric / Imperial): one face-coloured track, the chosen segment accent-filled. Switch 36 × 20 for on / off. Slider: 4 u track, 16 u thumb, value in Caption at the right.

## 4. Icons — Lucide (ISC licence)
One set: **Lucide** v1.52.0 (lucide.dev, ISC licence; the licence text is `mockups/assets/LICENSE-lucide.txt`), 24 × 24 grid, round 2 px stroke drawn at 20 u / 1.75 stroke, always one colour (text, muted or accent). Emoji (📷 ⚙ 💡 ❓) leave the interface. Icons used in the mockups: atom, orbit, graduation-cap, trophy, settings, palette, info, play, pause, rotate-ccw, camera, search, layers, eye, sliders-horizontal, circle-help, lightbulb, chevron-down, x, menu, scale, clock.
Cost to apply: the lines are plain SVG strokes; the app's `svg_icon.cpp` draws filled Learn art, so a small stroke path drawer (M L C A H V Z, circle, line, polyline) is needed, or the icons are baked into an icon font at build time.

## 5. Panels and surfaces — space and tone instead of outlines
- Levels: **page** (bg) → **panel** (card colour) → **inset** (card mixed 6 % with text; one level only, for a readout block or a segmented track). No box inside an inset.
- A group is not boxed: it has a Title or a Caption label, 24 u above it and 8 u between its rows. A hairline (1 px, the theme's line colour) is allowed only between the rows of one list.
- Controls with no text (tick box, switch, slider track, text field) keep an edge computed to **3:1** against what is behind (the existing CONTRAST RULE: 4.5:1 for text, 3:1 for interface parts); a text button may rely on its fill and label.
- Radius: 12 u panels, 8 u controls. One soft shadow, only under floating things (popover, drawer, dialog). The picture is full-bleed: no frame, no title bar.
- Side panel width = **24 text widths of Body (384 u at Normal)**, minimum 320 u, so it grows with Text size; below that the panel becomes a drawer (layout B/C).

## 6. Layout grid
4 u baseline; page margin 24; a 12-column grid inside a panel with 8 u gutters (a slider row = 12, two buttons = 6 + 6, label + value = 8 + 4). The picture takes everything left over: **never less than 60 % of the window height** (today 49 % at 1000 × 640). Top bar 56 u holds only navigation (labs, Learn, Achievements, search, menu); everything that acts on the picture lives in the panel; the time controls (play, speed, date) are one bar docked under the picture. Nothing else lives outside these three places.

## 7. Long text: summary first, details on request
- Every explanation = a **summary of at most 140 characters / 2 lines**, then a quiet **More** (opens in place, one at a time) or an **ⓘ** icon (a popover, for definitions and for sources).
- A paragraph over 3 lines is never shown by default, except on reading pages (Learn guides, About, changelog: 66-em wide, line height 1.6, a heading at least every 4 paragraphs, the changelog collapsed per version).
- Numbers are shown as **stat lines** (label left, value right, six at most per card), not as sentences; "All values" opens the rest. Sources sit behind a **Sources** link at the end of a card, never in the middle.
- Honesty text (model limits: "one electron, non-relativistic …") keeps one visible short line (Caption, muted, with ⓘ for the full wording). It is not hidden.

## 8. Grouping: Choose · View · Info
The panel has three tabs with icons, always in this order:
- **Choose** — what to look at: element and orbital (a "Hydrogen · 1s ▾" card; the periodic table opens in a big picker with search, 40 u cells), planet, date, journey, presets.
- **View** — how it is drawn: view mode, style, density, scale, labels, units, zoom, quality.
- **Info** — the readout (stat lines), the facts, spectrum / lifetime cards, guides, sources.
Outside the panel only: the lab switch, Learn, Achievements, search and the menu (Settings, Theme, Language, About, History) in the top bar; and the one time bar under the picture. Defaults / Reset / Save image are in the panel's footer and the menu.

## 9. Tips and facts (replacing the always-on "Did you know?" bar)
- The bar (about 60 px of every screen) is removed. A **lightbulb icon** in the top bar opens one **facts card** (popover: fact, previous / next, "n of 27").
- Optional **toast**: after 8 s in a lab, once per session per lab, one fact slides in bottom-left, closes by itself after 10 s, never over the picture's centre, never with Motion Reduced (it just appears). A Settings switch "Show tips" (default On) turns it off.
- The secret-theme unlock that lives in the facts (the locked Synthwave theme says "A surprise is hiding in the 💡 facts") stays in the facts card; the Learn page shows the fact of the day. A later mascot (ROADMAP "Later") would use the same card slot.

## 10. How every existing theme maps onto it
Every theme already has seven flat colours; the design needs no new data. Tokens: **page** = bg · **panel** = card · **inset** = mix(card, text, 6 %) · **face** = mix(card, text, 12 %) · **text** = fg · **muted** = muted · **hairline** = line · **accent** (text, marks) = accent · **accent-fill** (the primary button, the chosen segment) = accentFill · **on-accent** = onacc · **danger** = danger. The derived colours use the app's `mix`.
Measured contrast of the flat colours (`tools/theme_contrast.py`, W3C WCAG 2.x formula; Light's accent is the native #2d6ae3):

| theme | text/page | muted/page | muted/card | accent/card | line/page |
|---|---|---|---|---|---|
| light | 15.91 | 4.99 | 5.36 | 4.90 | 1.28 |
| dark | 15.72 | 6.45 | 5.88 | 6.21 | 1.45 |
| black | 17.78 | 6.46 | 6.06 | 7.34 | 1.43 |
| midnight | 15.74 | 6.70 | 6.06 | 7.66 | 1.57 |
| solar | 14.60 | 6.29 | 5.75 | 7.93 | 1.58 |
| paper | 13.03 | 4.99 | 5.44 | 6.45 | 1.33 |
| sage | 15.05 | 6.81 | 6.19 | 8.01 | 1.54 |
| contrast | 21.00 | 13.89 | 13.89 | 14.88 | 10.24 |
| synthwave | 16.74 | 7.45 | 6.97 | 6.28 | 1.79 |
| monochrome | 21.00 | 7.00 | 7.00 | 21.00 | 2.10 |
| aero | 10.26 | 8.05 | 13.29 | 9.66 | 1.82 |
| y2kice | 15.97 | 11.03 | 11.94 | 8.26 | 3.02 |
| y2knight | 17.77 | 15.10 | 14.00 | 11.16 | 4.51 |

- **Muted text** passes 4.5:1 on page and panel in all 13 themes (lowest 4.99, Light and Paper) → muted text may be used for Captions. **Accent as text** passes 4.5:1 on the panel in all 13 (lowest 4.90). Text on accent-fill is already tested in 0.14 (accentFill rule).
- **The theme's line colour is 1.3–1.8:1 against the page in most themes (Monochrome 2.1, High contrast 10.2).** It is fine for a divider between list rows, but it is NOT enough for the edge of a text-less control, so those edges are computed (3:1) rather than taken from the theme. Only High contrast (10.2), Y2K Night (4.5) and Y2K Ice (3.0) reach 3:1 with their own line colour; every other theme (Monochrome 2.1 included) needs the computed edge.
- **Groups of themes.** *Plain dark* (Dark, Black, Midnight, Solar, Sage, Synthwave): tokens only. *Plain light* (Light, Paper, Monochrome): tokens only; Paper keeps its serif font option. *High contrast* (and Monochrome): outlines come back at full strength on every control (the "outline mode": the hairline = text colour at 70 %), because there the rule "space instead of lines" would hurt. *Auto*: picks Dark or Light as today.
- **Styled themes (Frutiger Aero, Y2K Ice, Y2K Night)** keep their style data. Mapping: glass = the side panel and the top bar surfaces; gloss = the Primary button only (Secondary and Quiet stay flat so the page does not look busy); rim, glow, title bar and font stay on dialogs and popovers; textures stay behind the picture and the page only, never under text (the 0.14 rule) and **Theme effects Off** still strips them. The 0.14 contrast tests (panel, buttons, title bars over the page textures) keep applying to the new surfaces.
- Science colours (phase wheel, decay modes, elements, planets) stay fixed in every theme (0.14 decision).

## 11. What this costs and does not decide
The rebuild touches every panel (`orbit_panel`, `atom_panel`, `app_tabs`, `app_pages`, `settings`, `learn`, `atom_learn`, `changelog`, `look`) and the ImGui style code; physics and renderers are not touched. The layout (rail / drawer / bottom sheet) is the owner's pick between the mockups A, B and C (mockups/); this spec holds for all three. Not done in stage 1: any code, the Inter font and Lucide files in the app, the Windows scale re-read, the Settings display line.
