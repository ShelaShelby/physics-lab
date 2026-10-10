# Native Physics Lab 0.15 stage 5q part 1: buttons, theme list, About scrollbar, End key

Bug fixes only, no physics or value changed, `index.html` untouched (hash e5501387ae64 at start). New test: `tests/check_5q.py` (ui5q_lit, _theme, _scrollbar, _endkey). The app's own label stays 0.14.

## Changed
- #1 Lit buttons, ALL themes. LOOK FIRST (11 themes rendered before any change): the coloured rectangle came from the WIDGET CODE, not the theme tokens. Stage 5p switched the new look on only when `g_lo` was set (Dark); every other theme drew a lit button with `cFacePress()` = the face colour 30 % towards the ACCENT (light blue in Light / Auto-light, pink in Paper, green in Sage, brown in Solar, blue in Midnight), Contrast and Monochrome also drew an outline round every button, and hover was accent-tinted too. Now `iconButton`, the 2D / 3D segment and the open chip use one rule from tokens: fill = card 14 % towards the text colour, icon = white on a dark theme / the theme's text colour on a light theme, no outline, no glow, idle icon = muted towards the card until it is 3.3:1 under the lit icon, hover / press = face towards the text colour (never the accent). Also the caret (panel) buttons (Cloud, Labs...) are lit the same way when their panel is open.
- #2 Theme list: now hangs under the top bar, right edge under the palette button (`themePicker(app, &hang)`); the old-UI call is unchanged.
- #3 About scrollbar: the thumb was 1.4 x wider (12 -> 16.8 px) on hover and drag; now 12 px in every state (only the colour changes).
- #4 End: the page grows ~1 % while its bottom lines are first drawn, so one End stopped short. End now keeps aiming at the maximum for up to 24 frames (any other scroll, Home or a scrollbar grab lets go).
- Looks on purpose: Contrast and Monochrome lit buttons lose their box edge (your rule "no outline in every theme"); the idle buttons keep theirs. Easy to revert, see DECISIONS.

## Tested (cloud, Linux, Mesa llvmpipe, scripted events, pixels)
- Before the fix on the same tests: lit 72 of 330 checks failed (every theme but Dark), theme list 21 of 30, scrollbar 10 of 20, End 12 of 28. After: 0 failed in all four.
- ui5q_lit: 11 themes x (Atom toolbar button, top-bar gear, gear under the pointer, plain icon under the pointer, Orbit 2D/3D segment): fill differs 10..80 levels from the bar, not a coloured pill, edge pixels between fill and bar (no outline), 2..4 px outside = bar (no glow), icon on fill >= 4.5:1, lit against idle icon >= 3:1, idle icon on bar >= 3:1. WCAG formula written in the test.
- Break on purpose (outline back on in Light, restored): 14 checks failed (Light, Auto-light, toolbar and gear, hover); my first ring probe missed the toolbar outline (it sits ON the edge pixel), so I fixed the probe and ran the break again.
- Theme list at 880x560, 1280x800, 2560x1369: touches no top-bar icon, starts below the top bar, inside the window. Scrollbar: width idle, hover, drag (3 frames) all 12.00 px. End: one press, frames 20 / 30 / 45 after it, offset == max exactly at 3 sizes; Home exactly 0.
- Looked at: all 11 themes before and after (lit toolbar + gear), the theme list at 1280x800.
- Existing: `check.sh quick` 10/10; ui5p_lit, ui5p_scroll, ui5j_look_m1, ui5l_toolbar passed; ui5l_popups failed with the same single corner pixel as before (fixed in part 2 by the approved inset).

## NOT tested
Windows behaviour, real mouse, real GPU, speed, 125 / 150 % scale as a picture. Full suite (`shell_pictures` will change: hover / lit pixels of other themes) runs once at the end of part 2.
