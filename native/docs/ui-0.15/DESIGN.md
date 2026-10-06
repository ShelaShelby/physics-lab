# 0.15 design — final spec (stage 2). Chosen by the owner on 5 Oct 2026; nothing here is applied to the app yet

Stage 1's layouts A, B and C were **rejected** ("looks like a mobile app"): see `DESIGN_stage1_rejected.md` and `mockups/` (kept for the record). Every control has a home in `INVENTORY.md`. Numbers are in `design_tokens.json`.

## 0. The design in one paragraph, and the owner's hard rules
A **full-screen 3D-viewer ("digital twin") look**: the lab picture fills the whole window. Over it float: a slim **top bar** (menu, breadcrumbs, a centre **chip**, search, small icons), small **floating panels** that collapse to their title (**Legend** top left, collapsed; **subject card** top right), a compact **toolbar** bottom centre whose buttons have small ▾ carets that open a panel above them, and a tiny **readout** bottom right. Panels are dark translucent glass with a thin light edge and ~10 px corners; one accent colour. Checkboxes and segmented buttons, **no phone-style switches**. Long text lives in a **reading view** (centred column over a dimmed picture). No side column, no header block, no "Did you know?" bar.
Hard rules: (1) text readable on the owner's screen: **body never under 14 px at 100 % Windows scale**, scaled by Windows scale and Text size, panels grow with the text; (2) **clean and DESKTOP**: mouse-sized controls 28–32 px, not touch-sized; (3) **nothing lost**: every feature stays reachable (INVENTORY.md: nothing more than 2 clicks away).

## 1. Units, text and size
**u** = 1 px at Windows scale 100 % and Text size Normal. A size in the app = `max(floor, round(size_u × Windows scale × Text size factor))`, Text size factor 0.9 / 1 / 1.15 / 1.3 (as today). The Windows scale is re-read when it changes (stage 3).
| name | u | floor (px) | used for |
|---|---|---|---|
| Caption | 13 | 12 | units, hints, the readout, legend entries, tooltips, labels drawn on the picture |
| **Body** | **15** | **14** | every control label, value, list row and paragraph; the chip text |
| Title | 17 (semibold) | 15 | panel and card titles, the chip's value, breadcrumbs' last item |
| Key number | 28 (semibold) | 22 | the ONE big number of the subject card |
At **100 % + Text size Small** Body is 14 px (13.5 floored to 14): the owner's rule holds in every setting. At 125 % / 150 % + Normal: Body 19 / 23 px (rounded). Reading view headings: H1 28, H2 20, H3 17, body 15 with line height 1.6.
Fonts: the app's existing choices stay (Segoe UI on Windows by default, Atkinson Hyperlegible, Nunito, Michroma for Y2K titles). Numbers use tabular figures where the font has them. Two weights: regular and semibold. Icons: **Lucide** (ISC), 20 u at 1.75 stroke, one colour; emoji leave the interface.
**Sizes follow the text.** Every width below is in em of Body (1 em = the Body size in px), so a panel grows with the text: nothing is a fixed pixel width. Heights are in u × the same factor.

## 2. Controls: desktop-sized
| thing | size (u at Normal) | notes |
|---|---|---|
| Button (text) / field / combo | height **30**, padding 12, radius 6 | Primary = accent fill; Secondary = a lighter glass tone; Quiet = text only |
| Icon button | **30 × 30**, icon 20; toolbar buttons 32 × 32 | tooltip with the name and the key |
| Caret ▾ | 16 × 32, attached to its button | opens the small panel above (toolbar) or below (top bar) |
| Check box | 16 × 16 box, 2 u check, label 8 u away; row height 28 | never a switch |
| Segmented button | height 28, one track, the chosen segment accent-filled, equal widths | for Simple / Real, Flat / Landscape, Metric / Imperial |
| Slider | track 4, thumb 14; value in Caption on the right; row height 28 | wheel and arrow keys work |
| Stepper (− ▮ +) | buttons 28 × 28 | neutrons, protons, hour, minute |
| Chip | height 38 (two lines: value in Title, sub-line in Caption) | the centre of the top bar |
| Top bar | height **44** (about 40; the chip needs two lines) | translucent gradient, hairline below |
Spacing scale: **4 · 8 · 12 · 16 · 24** u. 8 between related controls, 12 inside panels' sections, 16 panel padding, 24 between a panel and the window edge (12 for the top bar and toolbar). Radius 10 for panels, 6 for controls, full round for the toolbar and chips' ends. Focus ring 2 px accent, always visible. No outline around groups: sections are separated by space and a Caption heading; a hairline only between list rows.

## 3. Glass panel tokens, and the contrast finding
A glass panel = the theme's panel colour at opacity **a** over the picture, a 1 px light edge (the text colour at 20 %), a faint top-lit gradient (the text colour at 6 % fading out over 40 u), and for floating panels one soft shadow. **No real blur**: Dear ImGui has none and a blur pass costs GPU time (a later option, measured first).
**Contrast rule (owner):** body text on a panel passes **WCAG 2.x 4.5:1 over the WORST picture that can be behind it** (white behind a dark panel, black behind a light one: the lab pictures do contain both). Computed by `tools/glass_alpha.py` from each theme's own flat colours (the least opacity at which the text colour, the muted colour and the accent colour all pass; then +0.05, at most 0.97):
| theme | panel | opacity to use | the least for text / muted / accent |
|---|---|---|---|
| Light | #ffffff | 0.97 | 0.52 / 0.93 / 0.97 |
| Dark | #1e1e21 | 0.96 | 0.66 / 0.91 / 0.90 |
| Black | #0b0b0c | 0.93 | 0.61 / 0.88 / 0.83 |
| Midnight | #111c38 | 0.95 | 0.66 / 0.90 / 0.84 |
| Solar | #271d15 | 0.97 | 0.68 / 0.92 / 0.83 |
| Paper | #fbf6ea | 0.97 | 0.57 / 0.92 / 0.85 |
| Sage | #16211e | 0.95 | 0.67 / 0.90 / 0.82 |
| High contrast | #000000 | 0.70 | 0.54 / 0.65 / 0.63 |
| Synthwave | #1b1035 | 0.94 | 0.63 / 0.85 / 0.89 |
| Monochrome | #ffffff | 0.87 | 0.46 / 0.82 / 0.46 |
| Frutiger Aero | #ffffff (light glass) | 0.75 | 0.52 / 0.60 / 0.70 |
| Y2K Ice | #f4f9ff | 0.80 | 0.51 / 0.63 / 0.75 |
| Y2K Night | #0c1428 | 0.78 | 0.62 / 0.66 / 0.73 |
**Finding for the owner:** the plain themes' muted text and accent text (grey and blue on a dark panel) only reach 4.5:1 over a white picture when the panel is **93–97 % opaque**: the picture then shows through very little. Two honest ways to make panels see through more: (a) keep that opacity (looks nearly solid, safest); (b) lower to about **0.70** (the least for the main text colour is 0.46–0.68) and give the muted and accent text their own solid backing or the main colour on glass — decided in stage 3 with the owner looking at it. The spec default is (a). The 0.70 row for High contrast shows what is possible with a bright text colour.
Where it is guaranteed: tests (stage 3) compute the contrast of every text colour on every panel kind for all 13 themes against pure white and pure black behind the panel, with the exact opacity, as the 0.14 contrast tests do.

## 4. How all 14 themes map onto it
Tokens come from the 7 flat colours every theme already has: **panel** = card, **text** = fg, **muted** = muted, **hairline** = line (dividers only), **accent** = accent (text, marks), **accent-fill** = accentFill (primary button, chosen segment, check mark), **on-accent** = onacc, **edge** = fg at 20 %, **opacity** = the table above. The page colour (bg) is used by the reading view's dim and by the picture behind transparent areas.
- *Plain dark* (Dark, Black, Midnight, Solar, Sage, Synthwave) and *plain light* (Light, Paper, Monochrome): tokens only. Monochrome and **High contrast** also get a full-strength 1 px outline on every control (the "outline mode"): their line colours are 2.10 and 10.24:1 on the page; Monochrome's is below the 3:1 interface-part rule, so the control edge is computed to 3:1, not taken from the theme.
- **Frutiger Aero** = light, frosted glass (white at 0.75, a bright rim, the glossy lime Primary button, aqua Secondary, the Segoe UI font when present) over the lab picture; its sky gradient, clouds and bubbles are drawn **inside** the glass panels and the top bar (the picture covers the rest of the window), so the page effects stay cheap and under the stage 2 cache (C2). **Y2K Ice** = its chrome and ice-white glass (0.80), silver chrome Primary, HUD rules and corner brackets on the cards; **Y2K Night** = dark glass (0.78), gunmetal chrome buttons, grain and the light streak inside the panels, Michroma for card titles. **Theme effects Off** strips textures, glints and brackets as today; colours, gloss and fonts stay.
- *Auto*: Dark or Light as today. Science colours (phase wheel, decay modes, elements, planets) stay fixed in every theme.
- The old 0.14 contrast tests (panels, buttons, title bars over textures) keep applying and are extended to the new surfaces in stage 7.

## 5. The pieces
- **Top bar.** Left to right: ☰ MENU (what's new, About, changelogs, language, put this lab's settings back, keys, text size A− A+); breadcrumbs *Physics Lab › Orbit Lab ▾ › Solar System › Earth* (each a link; the lab name is a dropdown switching labs; the last crumb is the selection); the **centre chip**; a **search** button (Ctrl+K); on the right icons: other lab, Learn, Achievements, 💡 facts, Theme, Settings. At narrow widths the crumbs shorten to the last two, then to icons.
- **Centre chip + dropdown.** Orbit: date and time (Title) and the speed under it (Caption), a ↺ Today button inside it; opens the **Time panel** (24 em wide): Live / Set manually, date field, time-of-day **dial** with hour and minute fields (a 120 u circle, a hand per field; the fields are the exact input), speed slider, Jump to (Journey and the six history jumps). Atom: element chip "U · Uranium-238 · 92 protons · 146 neutrons"; opens the **periodic table** (18 columns, cells ≥ 28 u, the chosen one ringed), protons stepper and slider, neutrons stepper, slider, "usual", famous isotopes. One dropdown open at a time; click outside or Esc closes it; it never covers the chip.
- **Search (Ctrl+K).** A centred palette 36 em wide: one field, up to 8 results grouped (Bodies · Elements and isotopes · Views · Settings · Guides), arrows and Enter, Esc closes. It indexes every control of INVENTORY.md by its label (the second way in).
- **Subject card** (top right, 20 em wide). Name (Title), one-line kind (Caption), the **one big key number** (Key number) with a short caption, 4–6 key values (stat lines: label left, value right), one action link ("Walk its decay chain", "Slow time to watch it turn"), **All values** (opens the data sheet: a large floating panel with tabs, about 36 em wide, max 80 % of the window). Collapses to its title; remembers its state per lab.
- **Legend** (top left): a chip "Legend ▸", collapsed by default; open it lists the colour swatches with words and counts. The chart's legend and notes live here. The model-limits line ("model: one electron, non-relativistic ⓘ") is NOT in the legend: it stays visible in the readout.
- **Toolbar** (bottom centre): a floating pill, height 40, buttons 32 × 32 with 4 u gaps, thin separators between groups, carets on the buttons that have a panel. Orbit: play · speed ▾ · show ▾ · pick a body ▾ · scale ▾ · 2D/3D ▾ · save picture. Atom: play · Atom · Nucleus ▾ · Cloud ▾ · Chart · view options ▾ · marks ▾ · photon / jump ▾ · reset · save picture. A panel opens **above** its caret, max 22 em wide, scrolls inside if it must; narrow windows move the last buttons into a "…" button (never a second row).
- **Readout** (bottom right, Caption, no box, a 1 u dark outline on the text so it reads over anything): Orbit: zoom, scale, light-travel time, fps; hover hint; Atom: zoom, scale, the model-limits line, hover hint.
- **Reading view.** The picture dims to 60 % black (not blurred); a centred column about **70 characters wide (about 38 em)**, a table of contents in a rail on the left when the window is wider than 60 em plus the column (else a "Contents ▾" menu), headings H1–H3, short paragraphs, "More" folds long blocks, a ✕ and Esc to close, and "Back to the lab" at the foot. About, History, changelogs and the 7 guides use it; the guides' ▶ buttons that set the lab close it. The Learn page is the same view with cards.
- **Facts (💡).** One click opens a small card (fact, previous / next, "n of 27"); the secret-theme unlock stays inside it. No timer, no bar. The mascot (ROADMAP "Later") would use the same slot.
- **Toasts** (achievements, saved image) appear above the toolbar, 4 s, never over the chip.
- **Collapsing.** Every floating panel has a ▾ in its title; the state is kept per lab. In a window narrower than 880 u or lower than 560 u the card and the legend start collapsed and the toolbar uses "…"; below **880 × 560 u** the window is not allowed to shrink (minimum window size, in u × scale).

## 6. How the picture's centre shifts (so the selected body is not under a card)
The picture is always drawn full-window. Let **free** = the window minus the open panels (top bar, subject card, open dropdown or panel, toolbar). When something is selected (or flown to) and its projected centre falls inside an occluded rectangle (plus 24 u), the **view offset** is moved, eased over 0.3 s (instant with Motion Reduced), so the centre lands in the middle of the largest free rectangle; zoom does not change. Follow keeps the body at that point. Closing the panel eases back unless the user has dragged the view meanwhile. In the Atom Lab the nucleus is the subject: the same rule. Nothing here moves the physics: only the camera offset (the existing `ox, oy`).

## 7. Build plan (each stage shippable: the app is fully usable after each)
- **Stage 3 — shell.** Windows scale re-read + text-size rule (already begun in stage 2 C4) and the Settings display line; the glass token code (theme → panel fill, edge, opacity; tests of the contrast table); the top bar (☰, breadcrumbs, chip placeholder = today's date text, icons) **replacing the header block, the tabs and the "Did you know?" bar** (💡 facts card, Learn and Achievements icons open today's pages as windows); the floating-panel framework (collapse, remember, free-area computation, centre shift); the reading-view framework; search skeleton. **The old side column stays, now drawn as a glass floating panel over the picture** (collapsible), so nothing is lost while labs are moved one by one. Tests: contrast, layout (free area, min window), picture tests changed on purpose and listed.
- **Stage 4 — Orbit Lab.** Chip + Time panel (dial), toolbar and its panels, pick-a-body list, subject card + data sheet (the picture's card moves), RO; the Orbit side column is removed. All Orbit rows of INVENTORY.md; tests: every Orbit control reachable (a test walks INVENTORY.md's Orbit home list), physics tests unchanged.
- **Stage 5 — Atom Lab.** Element chip + periodic table dropdown, toolbar with view buttons, view options / marks / photon panels, subject card + data sheet (spectrum, lifetimes, nucleus, temperature, decay), legend; the Atom column and its bar are removed. Same reachability test; atom tests unchanged.
- **Stage 6 — Learn, Settings, About, history, changelogs, guides, Benchmark.** Reading view for all long text, the Learn page, the Achievements panel, Settings in sections, dialogs; the About / History tables of contents.
- **Stage 7 — themes, search, polish.** All 13 themes through the glass tokens (Aero and Y2K style data on the panels), the contrast tests extended, full search index, keyboard (Tab order, focus ring), the final speed check and cleanup (remove the old panel code, update README and the history).
Each stage ends with: ctest all green, the pictures listed, REPORT.md, an OWNER-TODO of what to look at on the real PC. Order is by risk: shell first because both labs sit on it; Orbit before Atom because it has fewer controls; the reading pages late because they are text moves.

## 8. Not decided, for the owner
(1) Glass opacity: nearly solid (the table) or see-through with solid backing for muted text; (2) an orbital chip next to the element chip; (3) the Play button in the Atom toolbar (INVENTORY.md); (4) Body 15 (this spec) or 16 (stage 1) as the default, given Windows' own body is 14 and the owner's complaint was size.

## 9. Stage 3 as built (6 Oct 2026): what is exactly as above, what differs
The shell is in the app behind Settings → New UI (preview) (Off by default). Owner decisions of 6 Oct: glass nearly solid (section 3's opacities), the INVENTORY section 6 "most-used" lists accepted. Differences from the text above, all listed in DECISIONS.md:
- **Reading column 34 em** (section 5 says about 38 em): in DejaVu Sans 38 em gave Body lines of up to 80 characters; 34 em gives at most 75 here, and about 70 in Segoe UI, which is narrower. The contents rail appears only when the window is wider than 60 em plus the column, as written, so at 1280 px wide the "Contents" menu is used.
- **Text on accent fills:** Dark and Black's own on-accent colour (white on #6c9cff) is 2.68:1, so the new UI uses the page colour there (`onAccent`); the old layout keeps it.
- **Icons** are drawn by code (no Lucide yet); the Time panel has Today, a date field, a 24-hour dial with steppers, Go, the speed slider and the jump list, without the "Live / Set manually" switch; the subject card shows the card's own lines (no big key number or data sheet yet: stage 4); the Legend lists the pick groups with their counts.
- **Glass:** a plain theme's panel is the card colour at the found opacity with a faint top-lit shine, a 1 px edge (the text colour at 20 %; a full-strength edge for Monochrome and High contrast) and one soft shadow; Aero and Y2K use their own panel decoration (the top bar is a docked panel, so the page behind it comes from the stage 2 page cache); stage 7 refines them.
- **Sizes** are exactly `max(floor, round(u x Windows scale x Text size))` with the factor 1.3 held as a double (as a float 15 x 1.3 rounded to 19 instead of 20: found by the stage 3 test).
- **Atom Lab:** its picture starts under the top bar and the bar under it stays until stage 5; its chip shows the element and is not yet a dropdown.

