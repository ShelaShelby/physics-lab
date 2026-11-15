# The 0.38 look (chosen by the owner on 6 Nov 2026)

The owner chose this look from a clickable mock-up ("mission look, every option": https://claude.ai/artifact/Nss62XnDYtd92UvmLVpogH, private to the owner). It is drawn in every theme since 0.38. It replaces the look parts of `docs/ui-0.15/DESIGN.md` (M1's radii, the grey pill, the grey sliders); the 0.15 layout, the type scale, the text-size floors and INVENTORY.md still hold.

## Why (what looked dated in 0.37)
Rendered in Dark, Light, Midnight and Paper (10 screens each), the 0.37 look read as a default toolkit. These were the problems:
- **Shapes:** round pills everywhere (chip, toolbar, handle, the reality dots).
- **Choices:** a chosen option was a heavy filled block (light grey in dark themes, near-black in light ones).
- **Controls:** check boxes and sliders were grey and did not read as "on".
- **Text:** section headers were a repeated "Title … ▲" with a divider under each, and the Settings headings had emoji drawn as tiny glyphs.
- **Labs:** the lab switcher was hidden in a breadcrumb dropdown.

## The look
- **Corners:** at most 6 px anywhere (`ui::corner` caps every radius). Panels 6, controls 4, parts inside a group 3, check boxes 3. Monochrome stays square. No pills, no round knobs.
- **Surfaces:** flat. The card colour is at the theme's found opacity (Dark solid), with a 1 px edge of the text colour at 10 %. No shine, no halo, no shadow. The toolbar has the same edge.
- **Chosen option** (segments, list rows, Settings tiles, guide chips): the control face moved 18 % towards the accent, a 1 px accent edge at 85 % (60 % failed 3:1 in Light), and the text colour on it. The parts not chosen are muted. In Monochrome and High contrast (outline mode) it is the solid accent fill with its on-fill text.
- **Check box:** 16 u. When ticked it is the accent fill with an on-fill tick.
- **Slider:** a 3 u track, the filled part in the accent fill, and an upright 12 × 18 u thumb with a 1 px dark edge (light on dark themes).
- **"Closest to reality" mark:** a small accent square. An unticked box that is best left unticked gets an outlined square.
- **Buttons:** Secondary is the face with a 1 px edge. Primary is the accent fill with on-fill text. Quiet is text only. Hover and press move towards the text colour in every theme.
- **Top bar:**
  - On the left, the **labs as tabs**: the open one in the text colour with a 2 px accent line, the others muted.
  - Then a thin rule and where you are (Orbit Lab: Solar System › body).
  - A narrow window shows the tabs only, then the Labs icon.
  - The chip is a 4 px box. In the Atom Lab it starts with the **element's tile** in its periodic-table colour (hsl(hue, 70 %, 55 %) at 22 %, edge hsl(hue, 60 %, 50 %) at 50 %, as in `ui/atom_panel.cpp`).
- **Controls sections:** the name in capitals, muted, with a thin rule after it up to the arrow. Sections are separated by space, not by a divider.
- **Atom Lab readout:** a 4 px plate with a 1 px edge. Its first row (the state and its energy) is in the text colour, the other rows muted.
- **Theme button:** a painter's palette (outline dented for the thumb, three paint dots). The owner chose style C of six (it was a circle with three dots, which read as a face).
- **Font:** Inter in every theme with Font = Theme default. Paper keeps its book font, Monochrome its monospace.

## Colours
- **Dark** has its own palette:

  | role | colour |
  |---|---|
  | bg | #0b0c0e |
  | card | #16171a |
  | text | #ecedef |
  | muted | #a3a6ae |
  | accent | #6b9ff7 |
  | accent fill | #3a6fd4 (white text 4.8:1) |
  | line | #2c2e33 |

- **Every other theme** uses its own seven colours unchanged.
- **Tests:** every theme's pairs are tested with WCAG 2.x in `tests/test_look.inc`:
  - text, muted and on-fill text at 4.5:1
  - the tint at 4.5:1 for text
  - the accent edge and the fills at 3:1
- **Science colours stay fixed.** 0.38 also fixes the + half of an orbital's wave, which followed the theme's accent until 0.37 (Paper drew both halves red-brown). It is now a fixed blue: #6c9cff on dark pictures, #2d6ae3 on light ones.

## Not done (on purpose or for later)
- **The dim (0.38.1, the owner: "fix all"):** the picture dims to 60 % black behind Settings, as behind About and the guides, and a click on it closes the window. The Controls panel and the bars stay as they are and keep working (no modal lock). Pages of text (the reading view, its contents, search) are solid, not glass.
- **The Atom Lab readout keeps its rows** (with their fold and fit logic). The big key number stays on the element card.
- **Emoji inside the guide and card texts** (⏱ ⚖ ☁ ●) are content and were left as they are. Only the Settings headings lost theirs.
