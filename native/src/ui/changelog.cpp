#include "changelog.h"
#include "ui/reading.h"

#include <algorithm>
#include <string>

#include "app/app_data.h"
#include "fonts.h"
#include "look.h"
#include "style_draw.h"

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The native app's own history. Newest first.
const Entry kNative[] = {
    {"0.16", "Atom Lab: the nucleus with sea quarks draws in far fewer steps",
     "With \"+ Sea\" each antiquark ring used to be drawn as 24 or more separate line pieces and broke the graphics card's batch; the Classic quark style's flux tubes did the same. "
     "Both are now drawn in the same batch as the dots round them: darmstadtium-281 with \"+ Sea\" in Accurate, turning, went from 449 draw calls and 33,323 corners a frame to 5 and 30. "
     "This was also why light themes were slower than dark ones with Cinematic quarks: on a light theme the rings took the slow way. The picture is the same (only the edge pixels of the rings differ slightly). "
     "Not measured: speed on a real graphics card (the cloud has none)."},
    {"0.16", "Benchmark: 11 scenes instead of 34, about 2 minutes",
     "The Benchmark now runs 11 scenes of 10 seconds each (about 2 minutes instead of about 6): the heaviest Orbit Lab view, 8 times the rocks, the oganesson-294 nucleus, the nucleus at full zoom with the camera moving, one nucleon, "
     "phase colours with 48,000 dots, a cloud jump in flight, a superposition that beats, an outline's first use, the outline with the cut dragged, and the chart with the decay chain. "
     "The scenes that only repeated another one were removed. The two old boxes (stress scenes, speed-risk scenes) are replaced by one: \"Compare old CPU paths\" adds 7 scenes (12 to 18) that run scenes of the list again with the processor doing the work, the way older versions did. "
     "Every scene number in the texts and in the report is the new one."},
    {"0.16", "Atom Lab: less work at full zoom on the nucleus; benchmark counts slow frames",
     "At full zoom on a nucleus (x10,000,000) the Atom Lab no longer makes the parts of nearby nucleons that are wholly outside the picture: their gluon cloud, sea quarks, rings and lines. "
     "The picture is the same pixel for pixel. With \"+ Sea\" the antiquark rings alone were up to 311,076 corners a frame in Accurate, Classic and every light theme; now 36. "
     "The benchmark has a new last scene, \"Nucleus, full zoom, camera moving\" (lawrencium-266, Cinematic, + Sea, the camera turning), and every scene now says how many frames took longer than 16.7 ms, "
     "so a one-time hitch can be told from a repeating one. The outline's graphics-card programs now run once at start, so a driver's first-use work does not fall on the first outline. "
     "Not measured: speed (the cloud has no graphics card)."},
    {"0.16", "Four fixes: About scrollbar, Benchmark window, Contents rail, Atom Lab Nucleus",
     "The About page keeps its scrollbar after you close and reopen it (it was drawn behind the reopened page). The Benchmark window with both extra-scene boxes ticked never grows taller than the app window: its text scrolls and Start and Cancel stay in view. The Contents list beside a page is wider, so labels like \"Stars and far-away things\" are shown in full. In the Atom Lab, Controls now always has a Nucleus section with Quark style, Nucleon detail and Nucleon edge, at any zoom; they show in the picture when zoomed in close."},
    {"0.16", "The old interface is gone: the New UI is the only one",
     "The old side panels, the old header and the Atom Lab's Old panel button were removed, and so was Settings -> New UI (preview): the new layout is always on. Nothing it showed was lost: every old control, card and note has a home in the new layout. A settings file that still holds the old New UI choice opens without a problem (the choice is ignored). The benchmark's result no longer has a New UI line. The old frames-per-second counter is gone; the fps is in the readout at the bottom right and in Settings -> Performance. No new facts or numbers; no physics changed. Not tested: Windows, a real mouse, a real GPU, speed."},
    {"0.15", "Full UI rework, stage 1a of the 0.16 clean-up: the Atom Lab's last old-panel features",
     "With New UI On, the Atom Lab's Controls panel now has the Dirac levels switch (Relativity, with the same words as before) and a Restart atom button (a fresh atom, the same settings). The data sheet's Spectrum tab shows the lab's own light spectrum, with the Dirac level rows when Dirac is On, below the NIST spectrum. Settings, Performance shows the line \"Picture: N draw calls, M instanced shapes\" in the Atom Lab. Nothing was removed."},
    {"0.15", "Full UI rework, stage 5t: grey segments and check boxes",
     "In the Controls panel the chosen part of a segmented button and a ticked check box are now grey, like the sliders, in every theme. The label and the tick are white or black, whichever reads better. The bottom toolbar and the Settings tiles are unchanged."},
    {"0.15", "Full UI rework, stage 5r: the toolbar pills are flat",
     "The thin lighter rectangle behind the buttons of the bottom toolbar (and the 2D / 3D pill and the small chips) is gone: those pills now have one flat fill and no thin outline. Monochrome and High contrast keep their outline."},
    {"0.15", "Full UI rework, stage 5q: lit buttons in every theme, theme list, About scrollbar, End key, Element picker, grey sliders",
     "The button that is switched on in the bottom toolbars and in the top bar looked light blue, pink, green or brown in every theme except Dark. Now it is the same quiet look everywhere: a fill a little different from the bar, a strong icon (white on dark themes, the text colour on light ones), no outline and no glow; icons that are off are dimmer so the difference also shows without colour. The hover colour of those buttons is grey in every theme, not tinted with the accent. In Contrast and Monochrome the switched-on button no longer has a box edge. "
     "The theme list under the palette button now opens below the top bar instead of covering its icons. The scrollbar of About, Learn and the other pages keeps its 12 px width when you point at it or drag it (it used to grow). Pressing End on a page now goes all the way to the bottom in one press (the page grows a little while its bottom is first drawn, so it stopped about 1 % short). "
     "In the Atom Lab the Element picker now just floats over the picture: opening it no longer moves the atom, the toolbar, the FPS number or the scale bar (they stay exactly where they were); the picker stops above the scale bar's text, and the subject card is left out while the picker would cover it. The sliders in the Controls panel are grey in every theme, no longer blue. "
     "Not tested: Windows, a real mouse, a real graphics card, speed."},
    {"0.15", "Full UI rework, stage 5p: wheel after About, About scrollbar, zoom out further, lit buttons, Element picker",
     "Fixed: after you opened About (or History, or the log) and closed it again, the mouse wheel no longer zoomed the Orbit Lab or the Atom Lab until you restarted the app. The app kept thinking a page was still open. "
     "The scrollbar of About, Learn and the other pages is now 12 px wide with a faint track, and still drags; the wheel, PageUp, PageDown, Home and End already scrolled them. In the Atom Lab you can now zoom out twice as far in every view (Atom, Nucleus, Nucleon, Cloud, Cloud 3D, Slice and the Chart of nuclides): the picture can be half its old smallest size. "
     "The button that is switched on in the bottom toolbars and in the top bar (for example the gear while Settings is open) is no longer a light grey box: it is a dark fill a little lighter than the bar with a white icon, with no outline and no glow; the icons that are off are a little dimmer so the difference shows without colour. "
     "With the Element picker open in the Atom Lab: the picture, the toolbar, the FPS number and the scale bar no longer jump between two places from one frame to the next (the toolbar was centred on the free space, and the free space was worked out with the toolbar in it; I could reproduce the jump in the test at 880 x 560, not the exact look on your PC, so please test it). The Atom info text is not shown while the picker is open, and the picker no longer reaches the fact line, the toolbar or the FPS number. "
     "Not tested: Windows, a real mouse, a real graphics card, speed, 125 and 150 % scaling as a picture."},
    {"0.15", "Full UI rework, stage 5o: three small layout fixes",
     "With New UI (preview) On and the Dark theme: in a small window (880 x 560) the Orbit Lab's subject card no longer runs into the readout at the bottom right; it stops above it and scrolls inside itself (use the mouse wheel to reach \"All values\"). In the Atom Lab a long Did-you-know fact that did not fit beside the toolbar (for example with the Controls panel open) is no longer cut with \"...\"; it moves to the row above the toolbar and the scale bar moves up with it. Three paragraphs of the About page are worded a little shorter so that no line of text is too long to read comfortably. Not tested: Windows, a real mouse, a real graphics card, speed, 125 and 150 % scaling."},
    {"0.15", "Full UI rework, stage 5m part 2 and 5n: bigger handle, grey instead of blue, M1 windows, chart tilt slider",
     "With New UI (preview) On and the Dark theme: the handle on the picture's edge is bigger (32 x 72 points); hovered and pressed buttons are light grey, not blue; a lit toolbar button is the same light-grey pill as a chosen segment; the selected element in the periodic table has a light ring; the subject card steps aside while the Legend is open, so it never covers it; Settings, About, Learn, the history pages, the Benchmark windows and the pop-up lists use the same flat dark cards, rounded corners and grey selected pills. In the Atom Lab's Chart view a slider \"flat <-> 3D\" above the toolbar tilts the chart (the same tilt as the Controls panel). Not tested: Windows, a real mouse, a real GPU, how the slide feels."},
    {"0.15", "Full UI rework, stage 5m: three themes archived, the M1 look is the look",
     "Frutiger Aero, Y2K Night and Synthwave were taken out of the app (their code is kept in native/archive/themes). A saved choice of one of them now opens as Dark, with no message. The Settings row Theme effects went with them (only those themes used it), and so did the 🎁 surprise in the facts, which only unlocked Synthwave. With New UI (preview) On and the Dark theme, the look M1 (Inter font, flat dark cards, a light-grey selected pill) is now always on; looks M2 and M3 and the PL_LOOK switch were removed. With New UI Off nothing else changed. Not tested: Windows, a real mouse, a real graphics card, speed."},
    {"0.15", "Full UI rework, stage 5l: search finds every setting, Legend closes, toolbar centred",
     "With New UI (preview) On, search now finds every row of Settings (Benchmark, Font, Units, Motion, Theme effects, Did you know facts and the rest) and opens Settings at that row. An empty search box lists nothing, and Ctrl+K no longer brings back the old words. The Legend folds when you click anywhere else or press Esc. The bottom toolbar is centred on the picture in each lab, lights only one of Cloud, Cloud 3D and Slice, and has no Nucleus button (Controls, View has it). Small dropdowns fit their content without a scrollbar, and the Labs dropdown is opaque and in front. Not tested: Windows, a real mouse, speed."},
    {"0.15", "Full UI rework, stage 5k: pages scroll, Settings toggles, new gear icon",
     "With New UI (preview) On, About, Learn and the other pages now really scroll: the text used to stay in place while the scroll position changed. The mouse wheel and PageUp, PageDown, Home, End and the arrow keys work whichever window has the focus, and a slim scrollbar shows on the page. The Contents list jumps to each section. The Settings button closes Settings on a second click and has a new gear icon. Floating windows stay inside the app window, and the Benchmark Start button is always in view. Ctrl+Shift+F12 writes a small text file about the mouse and keys for finding problems. Not tested: Windows, a real mouse, speed."},
    {"0.15", "Full UI rework, stage 5j: look options M1-M3 for testing",
     "Three modern looks, M1, M2 and M3, were added for testing; they are not visible by default and nothing in the normal app changed. They keep the layout and change the details and the text: a clean modern font, flat dark cards with a thin edge, one solid selected pill, quiet muted section titles and thin icons. The old looks A, B and C were removed. "
     "To try one on a PC: set PL_LOOK to m1, m2 or m3 before starting the app, and turn New UI on with the Dark theme. M1 and M2 use the Inter font and M3 uses IBM Plex Sans (both SIL Open Font License 1.1, built into the app, listed in About). Not tested: Windows, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5i: the data sheet stays above the toolbar",
     "With New UI (preview) On, the data sheet (All values) now ends above the bottom toolbar and scrolls inside itself when it is taller. Look options added for testing (not visible by default). No new facts or sources. Not tested: a real PC, a real mouse."},
    {"0.15", "Full UI rework, stage 5h: the Atom data sheet gets the last cards, and the atom is centred in the free space",
     "New UI (preview) On: in the Atom Lab, All values now opens a data sheet with a second row of tabs: Spectrum (the real NIST spectrum), Lifetimes (lifetimes and jumps), Nuclear size, Temperature and, in the cloud views, Radial (the radial graph). Each tab is the Old panel's own card, drawn by the same code with the same words and the same numbers, and a tab only appears when the Old panel shows that card for the same atom and view. The sheet scrolls inside itself when a tab is taller than the window (mouse wheel, Page Up, Page Down). "
     "The atom and its label now sit in the middle of the space the subject card, the Controls panel and the bars leave free, so the card no longer covers them; the atom's info text at the top left is 14 px (it was 13). The Old panel button stays. No new facts or sources. Not tested: a real PC, a real mouse or wheel."},
    {"0.15", "Full UI rework, stage 5g part 2: panel sections scroll into view, Atom text and data sheet",
     "With New UI (preview) On: a Controls panel section you open now scrolls up so you can see what is in it (Pick a body used to open below the edge, out of sight). "
     "The Atom Lab's info text at the top left now stops before the subject card and the phase ring, and goes under the ring in a narrow window. "
     "The Atom data sheet has a solid background in the plain themes, and nothing in it is cut off with an ellipsis; the one \"…\" you may see in the isotope line (for example Uranium-238) is part of that line's own words: it means the nuclide has more decay modes than the two listed. "
     "Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5g part 1: scrolling, Contents, a fixed toolbar, Cloud 3D, a flat handle",
     "Fixed: the Contents list in a Learn or About page worked once and then did nothing (the page had come in front of the list); it now works every time. The Page Up, Page Down, Home, End and arrow keys now scroll a Learn, About or history page, and in the new UI the wheel also scrolls it when the pointer is over the dimmed picture beside the page. "
     "I could not reproduce \"the wheel does nothing\" with a scripted wheel (it scrolled in every window, at three sizes, with the new UI On and Off), so a real mouse on a real PC still has to confirm it. "
     "With New UI (preview) On: the toolbar's left edge now stays in one place in every view and in both labs, every button keeps its slot and the optional ones (Drag, Centre) join at the right end; the Atom toolbar has Cloud 3D and Slice buttons beside Cloud; "
     "the first time the Atom Lab opens (no saved view) it shows Cloud 3D, which can be turned (the old layout still starts in Orbit); the colour ring of the phase Wheel no longer sits under the subject card; the Controls panel's handle is a flat, slim pill with one chevron in every theme. "
     "The light Y2K Ice theme was removed (a saved setting that names it now gives Dark); Y2K Night stays. Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5f part 2: the Atom Lab's subject card, data sheet and legend",
     "With New UI (preview) On, the Atom Lab now has a card at the top right for the chosen isotope: its name, kind, the proton number, neutrons, nucleons, the isotope line, a More info / Less info button and All values. All values opens a data sheet with three tabs: Values (where you meet it, melting and boiling point, density), Decay (the decay history) and This view (the hint line of the picture). They are the very words the old panel showed, moved, not rewritten; the hint line and the More / Less info button are no longer at the top of the Old panel. "
     "On the Chart of nuclides the colour legend and the notes about the valley and the island now live in the Legend chip at the top left (closed at first); the picture no longer draws its own copy, which the Controls panel used to cover. "
     "The small text in the Orbit and Atom cards and in the data sheets is now at least 14 pixels (it was 13). Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5f part 1: a tidier Atom Controls panel",
     "With New UI (preview) On, the Atom Lab's View section is now one compact choice (Atom, Nucleus, Nucleon, Chart, Cloud, Cloud 3D, Slice) with the current view highlighted, doing what the old rows and the toolbar buttons do. "
     "The long notes (the energy level, the cloud, the phase colours, the chart, the decay chain, the photon jump) now sit behind a row \"Notes about this choice (show)\", closed at first, with the same words. "
     "On the Chart of nuclides the Cloud section is left out, because its settings do nothing there (the old panel still shows them). Nothing about the physics changed. Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stages 5d and 5e: the Atom Lab's options, marks and photons in the Controls panel",
     "With New UI (preview) On, the Atom Lab's Controls panel now has, after View: Orbital (the orbital, its orientation, the energy level n), Cloud (style, density, superposition, the slice), Orbit (Modern or Bohr 1913), Scale and motion (spin, scale, zoom, playback speed), Nucleus (quark style, nucleon detail and edge), Chart (tilt, what the height means), Marks (the 90 % outline, the phase colours, the decay chain and its walk) and Photons (emit, absorb, auto photons, check a jump). "
     "Each section is shown only in the views where the old panel showed those controls; every control changes the same setting, with the same words and notes (one electron, non-relativistic: as before). "
     "The fact line at the bottom left now wraps to two lines before it is cut, and the Atom Lab shows it too (the picture's scale bar moved above it). The benchmark's result now says whether the New UI was on."},
    {"0.15", "Full UI rework, stage 5c.1: the Controls panel opens from a handle, and tidier panels",
     "With New UI (preview) On: the Controls button is gone from the bottom toolbar. A small tab with an arrow on the right edge of the picture opens the Controls panel (arrow pointing left) and closes it (arrow pointing right); the panel slides in and out in about a fifth of a second, or at once when Settings, Motion is reduced. The panel's own close button still works. "
     "The glow and shadow round the new panels, the Legend and the toolbar now hug their edge (at most 2 pixels; in Aero the glow used to spread over whatever was next to them). The Controls panel is solid (the picture's labels showed through it), its section headers, rows and controls line up on one left and one right edge, its sections are evenly spaced and its small grey text is now normal text size. Not tested: a real PC, a real graphics card, speed, how smooth the slide looks. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5c of about 7: the Controls panel on the right, and two picture fixes",
     "With New UI (preview) On, in both labs, all the simulation options now live in ONE panel docked on the right side, called Controls (the owner's decision of 6 Oct 2026). It floats over the full-window picture under the top bar, about 22 em wide, has sections that fold (click a section's name) and scrolls inside. It is open at first; the Controls button at the right end of the bottom toolbar, or its own cross, closes and opens it, and the choice is kept like the cards'. The toolbar and the readout move left of it, the selected body is kept out from under it, and the Old panel (Atom Lab) opens left of it.\n"
     "Orbit Lab: Time speed, Show, Pick a body, Zoom, sizes and trails, and View options (Drag moves the view, Spin, Follow, Reset view) are sections of the panel, with the same controls, words and tooltips as before; the toolbar keeps the 2D / 3D switch, a Reset view button and the Controls button, and no carets. Atom Lab: the Nucleus level (Nucleus or Nucleon), Cloud, Cloud 3D and Slice, Build another and Reshuffle nucleons are in a View section; the Nucleus and Cloud buttons stay one click each. The rest of the Atom controls are still in the Old panel (next stages).\n"
     "Two fixes you reported: the distance-ring labels (for example \"6,500,000,000 AU · 103,000 light years\") are no longer drawn over the top bar, the toolbar or the Controls panel at a far zoom-out (the rings are still drawn); and the readout at the bottom right (zoom, scale, light travel time, frames per second) is now the size of the normal text on a soft dark rounded backing, so it reads over lines and stars in every theme. Not done yet: the Atom view options, marks and photon panels, the subject card, the legend and the data sheet. Not tested: a real PC, Segoe UI, Windows scale 125 / 150 %, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5b of about 7: the Atom Lab's element chip and periodic table",
     "With New UI (preview) On, the Atom Lab's chip at the top centre now reads, for example, \"U · Uranium-238\" over \"92 protons · 146 neutrons\" (with the right singular: 1 proton, 1 neutron, 0 neutrons). A click on it opens a panel under the chip with the periodic table (18 columns, every cell at least 28 units wide, the chosen element ringed, dashed edges for the elements with no stable isotope, the lanthanides and actinides in two rows below), the Protons arrows and slider, the Neutrons arrows and slider with a \"usual\" button for the element's most common isotope, and the list of famous isotopes. Click outside, press Esc or click the chip again to close it. This table is the big table: the old Bigger table button still works in the Old panel.\n"
     "Build another and Reshuffle nucleons moved from the Old panel into the toolbar's Nucleus panel (the small arrow next to Nucleus); they show only while the picture is zoomed to the nucleus. The hint line and More / Less info stay at the top of the Old panel for now. Still to come in stage 5c / 5d: view options, marks, photon panels, the subject card, the data sheet and the legend. Not tested: a real PC, Segoe UI, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5a of about 7: the Atom Lab's toolbar and view buttons",
     "With New UI (preview) On, the Atom Lab now has the same floating toolbar as the Orbit Lab, at the bottom centre: Atom, Nucleus, Cloud, Chart of nuclides, a thin separator and Reset view. Each does exactly what the old control did. Atom shows the whole atom in the Orbit view; Nucleus zooms to the nucleus, and its small arrow lets you choose Nucleus or Nucleon (one proton or neutron); Cloud shows the electron as a cloud, and its arrow chooses Cloud, Cloud 3D or Slice; Chart shows the chart of nuclides; Reset view turns the picture back (on the chart: tilt, zoom and position). Centre and the Drag button (rotate or move) appear in the toolbar only when the old bar showed them. The old bar under the picture and the picture's own More / Less info button are gone in the new layout, and the picture now fills the whole window like the Orbit Lab, with its text lines and the chart kept clear of the top bar and the toolbar. Play and pause stays in the element chip (Space too), not on the toolbar.\n"
     "Not done yet (stage 5b): the element chip's periodic table, the view options, marks and photon panels, the subject card and the legend. Until then the Atom Lab keeps its Old panel button, and the Old panel now also holds the More / Less info button and the hint line that used to sit under the picture. Not tested: a real PC, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 4 of about 7: the whole Orbit Lab in the new layout",
     "With New UI (preview) On, the Orbit Lab no longer needs the Old panel. Everything has a home: play and pause is a small button at the left of the date chip (Space still does it), Save picture is in the menu (and in search), the toolbar holds only what changes the view: speed, show, pick a body, zoom and sizes, 2D / 3D. The Time panel has Live (from the real date and time now, running) and Set manually (the date, the 24-hour dial, Go), the first four history jumps with More dates, and an (i) that explains the date range. Show now also holds the stars, the constellations and the far-away things. The Pick list has a filter box. The card for the selected body shows its name, what kind of thing it is, one big number (for a planet, its distance from the Sun) and four to six key values, with All values opening a sheet of every line the old card had, word for word. The Legend lists what the picture really draws, in the colours it draws them.\n"
     "The Did you know? button is gone: a fact now sits at the bottom left of the picture, fades to the next about every 20 seconds, stands still while the pointer is on it, and a click opens the whole fact (Settings, Facts: On or Off). Search (Ctrl+K) now finds bodies, dates, every Orbit Lab setting, menu actions, guides and the facts, takes Enter and the arrow keys, and lists your recent choices first. Links in the reading view are underlined as one line. The readout's outline is now black round light text and white round dark text in every theme. Performance and Benchmark are in Settings (with New UI On).\n"
     "Found on the way: the plan said 45 body tiles, the list has 47; none of the 27 facts has a named source in the app yet (the full fact says so). Not tested: a real PC, a real graphics card, Segoe UI, speed on Windows. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 3 of about 7: the new shell (a preview you can switch on)",
     "Settings has a new last section, New UI (preview), Off by default. Off is exactly the layout you know. On, the lab picture fills the whole window and a new frame floats over it: a slim top bar (menu, breadcrumbs with a Labs drop-down, the date and speed in a centre chip with a Today button, search, and small icons for the other lab, Learn, Achievements, Did you know?, Theme and Settings), a toolbar at the bottom (play, speed, show, pick a body, zoom and sizes, 2D / 3D, save picture, each with a small panel above it), the zoom, scale and frame rate in a small readout at the bottom right, a Legend that starts collapsed, and a card for the selected body that collapses to its title. If a card or panel would cover the body you picked, the picture slides so the body stays in the clear. Keys: Esc closes the open panel, Ctrl+K opens search, Space plays and pauses.\n"
     "About, History, the changelogs and the guides open in a reading view: a centred column about 70 characters wide over the dimmed picture, with a contents list. Text sizes follow one scale (13, 15, 17 and 28 units, never under 12, 14, 15 and 22 pixels, times Windows' scale and your Text size). Panels are nearly solid so their text passes the WCAG 4.5:1 rule over a white or a black picture behind them. Only the Orbit Lab's main controls are moved so far; everything else, in both labs, is behind the Old panel button at the top right until stages 4 and 5 move it.\n"
     "Found on the way: in Dark and Black the text on a chosen button (white on #6c9cff) is only 2.7:1; the new UI uses the page colour there, the old layout is unchanged. Text size Extra large was 19 pixels instead of 20 in the first build of this stage; the test caught it. Not tested: a real PC, a real graphics card, Segoe UI, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 2 of about 7: the plan, and four small fixes",
     "0.15 is the rework of every screen. Stage 2 is mostly a plan: a list of every control in the app and where each one will live (INVENTORY.md in the project), and the final design, chosen by the owner: the lab picture fills the whole window, with a slim top bar, a centre chip, small floating panels, a toolbar at the bottom and a reading view for long text. The earlier layouts A, B and C were rejected. "
     "Four fixes came with it. (1) The About page showed raw codes like \"&rsquo;\" instead of the characters; they are now decoded (all 254 of them, and the whole HTML list is understood). (2) The page effects of Frutiger Aero, Y2K Ice and Y2K Night (clouds, bubbles, grain, grid, halftone, light streak) are drawn once into a picture and reused, instead of being rebuilt every frame; the pictures stay the same within a few levels of rounding, and the saving is expected to be about 0.7 ms per frame (measured by the owner's PC, not here). "
     "(3) Windows is asked not to slow the app down when its window loses focus, and the benchmark report now flags any test during which the window lost focus or was minimised. (4) The app now notices when the window moves to another monitor or when Windows' display scale changes, and resizes its text; there is also a smallest window size. "
     "Not tested: anything on a real Windows PC (power behaviour, scale changes, speed): the project's tests use stand-ins for those. Nothing about the physics changed."},
    {"0.14", "Themes (complete): richer styles, Frutiger Aero, Y2K Ice, Y2K Night, a Theme effects switch (all four were taken out again, see 0.15 stage 5g and 5m)",
     "0.14 is finished (it was planned in five stages; the owner ended it after three, and the stage entries below say what each did). What you get: themes are no longer only seven flat colours but can have a gradient page, glass, gloss, a rim, a glow, glossy title bars, their own fonts and decoration. Frutiger Aero (light) is glossy and saturated, aqua and leaf green with jelly buttons. Y2K Ice (light) and Y2K Night (dark) are the cold techno-print looks with chrome buttons, grain, halftone dots, grid lines, corner brackets and the free Michroma font for titles. Light's blue text is a little darker so it reads (4.57:1). Windows and pop-ups are solid, so nothing behind them shows through the text. "
     "New in the last stage: Settings has Theme effects On / Off (saved). Off removes the clouds, bubbles, grain, halftone dots, grid lines, light streaks, glints, reflections, corner brackets and HUD marks and keeps the colours, the gloss and the fonts; the benchmark report says which it was. The other themes have none of this decoration, so nothing changes for them. "
     "Dropped by the owner, not postponed: glass and chrome planets and nucleons, glossy chart tiles, theme-aware Cloud dots and animated backgrounds. The pictures of the labs keep their own colours in every theme. Next is 0.15, a full rework of the screens to make the app clean. Not tested: a real PC, a real graphics card, speed."},
    {"0.14", "Themes, stage 2 of 5: Frutiger Aero's real look, a readable Light, and two Y2K themes",
     "Frutiger Aero is more saturated now: the page goes from sky blue through aqua to leaf green all the way down, buttons are aqua jelly (a strong white highlight over the top, a deep colour below, a thin darker rim, a glow when you point at one) and chosen buttons and tabs are lime jelly, windows have glossy deep-blue title bars, a bright rim and a soft glow so they read over the dark space picture, a faint reflection sits under windows and cards, and the page has soft clouds and glass bubbles (still, not animated yet). Combo boxes, their lists and the tooltips follow the same style. "
     "Two new themes, from one style: Y2K Ice (light) and Y2K Night (dark). The cold techno-print look: icy blue, cold white, black and brushed silver (violet only as a light streak in Night), fine film grain, halftone dots and thin grid lines on the page, thin HUD rules, corner brackets and a tiny SYS label on windows, chrome buttons with a cold highlight line and a glint when you point at them, a soft blue glow on the chosen tab, and window titles and the app name in the free wide font Michroma (SIL Open Font License, built in; the body text keeps its normal font). Night's buttons are dark gunmetal on purpose: text must stay readable at the brightest point of the gloss. "
     "Light: its blue text was 4.24:1 on its page, under the 4.5 that text needs; it is a little darker now (4.57:1, the same hue), so the pictures of Light changed on purpose. "
     "Tests: every text pair of Aero and Y2K passes WCAG 2.x over the page's textures at their strongest, over every button state and over every title bar; decoration stays in a 7 point band at the window edges (picture test); the title font is only on titles; the 50 test pictures of the unchanged themes are identical to 0.13. Not tested: a real PC, a real graphics card, speed."},
    {"0.14", "Themes, stage 1.1: a fix, Frutiger Aero's floating windows are solid now",
     "The owner found on the real app that in Frutiger Aero the guides (\"What am I even looking at?\"), the changelog and some other windows were see-through: the Learn cards and the Solar System readout showed through their text. Cause: stage 1 made window backgrounds see-through for glass themes but did not paint every window. Now every window and pop-up that holds text is solid (the glass look is the sky gradient, a bright line along the top, the rim and a soft shadow), only the docked panels stay see-through, and the scroll bars show again. The older themes are unchanged (the 72 test pictures are identical). New tests: every window opened by the app must be painted by the theme; the inside of a window must be the same picture over two different backdrops (Aero, Dark, Light); every theme's text passes 4.5:1 over any backdrop colour. Found, not changed: the Light theme's accent text on its page colour is 4.24:1, a little under 4.5. NOT TESTED: a real PC and a real GPU."},
    {"0.14", "Themes, stage 1 of 5: richer theme styles and the Frutiger Aero theme",
     "A theme is no longer only seven flat colours. It can now also have a page that fades from one colour to another, glass panels (see-through, with a bright line along the top and a soft shadow under floating windows), glossy buttons, tabs, check boxes and sliders (a bright top half, a tinted lower half, a pressed look), its own corner roundness and its own font. All of this is data in the theme, not code for one theme, and every theme that existed before draws exactly the same pixels as in 0.13 (checked by pictures of six screens in all 12 versions, dark and light). "
     "New theme: Frutiger Aero (light): a sky-blue page, frosted glass, glossy aqua and fresh-green buttons, rounder corners. It uses Segoe UI when Windows has it; the font is Microsoft's and is not shipped. Every text and background pair in it passes the W3C's WCAG 2.x contrast rule (4.5:1 for text, 3:1 for large text and interface parts), glass over the lightest and darkest part of the page included; the tests check the formula against values worked by hand. "
     "Colours that used to be written inside drawing code (error red, the red button, the faint illustration tint, the grey of tracks, the fill colour of chosen buttons) now come from the theme with the same values. The pictures of the labs (space, cloud, chart) keep their colours in this stage. "
     "NOT TESTED: the look on a real PC with Segoe UI and a real graphics card, and speed. Combo boxes keep the flat frame. The owner judges the look."},
    {"0.13", "GPU update, finished: the Orbit Lab's rocks and the chart of nuclides on the graphics card, and what 0.13 is in all (stage 4 of 4)",
     "0.13 \"GPU update\" in one place. WHAT MOVED to the graphics card (OpenGL 4.3, with OpenGL 3.3 as the fallback that keeps the old processor code): the phase colours of the electron cloud's dots (stage 1); the colour groups of a superposition and of a photon jump (stage 2); the 90 % outline's density grid and level, and the Slice contour (stage 3); and now the places of the Orbit Lab's belt rocks and the chart of nuclides' tiles and blocks (stage 4). Stage 4: "
     "(1) The 6,000 rocks (48,000 in the stress scene) used to be placed one by one by the processor every frame, Kepler's equation included; now a compute shader does it, with the time kept in double precision on the graphics card so a rock is right after 10,000 years at the fastest speed too (tested against Kepler's equation solved separately, at times up to ten million years, to 0.003 of a point). While checking it I found that the belt code solved Kepler's equation with only three Newton steps: exact for every rock the app makes, wrong for orbits with e of 0.8 and more (none in the app); it now iterates until converged, and the app's rocks do not move. "
     "(2) The chart's 3,557 squares or blocks are made once and kept in a graphics-card buffer instead of being rebuilt and sent every frame (a landscape frame sent 3.2 MB); tilt, zoom and pan are applied on the graphics card; a rebuild happens only when the picture size, the theme colours or the height meaning changes. The hover and click still answer exactly as before (tested against the drawn blocks). "
     "(3) The Atom Lab's Cloud 2D view cost more than Cloud 3D for a reason that was not the colours: its list of 48,000 dots was built again from nothing every frame, grown about 17 times; it is now kept, and the renderer takes it without copying (measured here: scene 10 from about 3.9 to 2.6 ms of processor time). "
     "THE SWITCHES (none is saved; all start On): GPU colours, GPU superposition and jumps, GPU outline, GPU rocks (Orbit Lab) and GPU chart cache, in the Performance card; the benchmark report says which way each ran, and scenes 23 to 34 run the matching scenes again with the old way (31 and 32: the Orbit Lab; 33 and 34: the chart). "
     "ALSO IN 0.13: the orbitals' normalisation is exact now (stage 3), a physics fix of up to 4.4e-5 in amplitude. "
     "MEASURED BY YOU (5 Oct 2026, stage 2 build): scene 1 2.24 ms, scene 6 5.34 ms, scene 10 4.43 ms, scenes 19 to 22 1.3 to 2.1 ms. NOT MEASURED: every speed-up in 0.13 is EXPECTED, not measured, because there was no graphics card or Windows PC in the sessions that wrote it; the tests compare the graphics card's results with the processor's on a software OpenGL (Mesa llvmpipe), where a real driver may round 32-bit numbers differently. Run the benchmark with the extra scenes and compare each scene with its Off twin."},
    {"0.13", "GPU update: the 90 % outline on the graphics card, and exact orbital normalisation (stage 3 of 4)",
     "The first time a 90 % outline is needed for an orbital, the processor used to make a 64 x 64 x 64 grid of densities and sort it to find the 90 % level: about 30 ms in a measurement here (13 ms for the densities, 17 ms for the sort), the only real hitch left in the app. "
     "Now, on a PC with OpenGL 4.3, compute shaders on the graphics card make the grid and find the level (by a search over the values, no sort), and the picture does not wait for them: the outline appears a frame or two after it is switched on. In the Slice view the cut's contour values are made the same way, so the contour is a frame behind the cut while you drag it. "
     "The 90 % mark of the radial graph no longer makes the panel wait either. A PC with only OpenGL 3.3 keeps the old way. The Performance card has a switch (GPU outline) and says which way your PC runs; the benchmark report prints it; scenes 29 and 30 run scenes 16 and 17 again with the old way. "
     "The outline is the same as before: the graphics card's level agrees with the processor's to 1e-4 and the cells to float rounding, and the tests compare them cell by cell. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. "
     "A physics fix on purpose: the orbitals' normalisation was a numerical sum that left the wave function too big by 3e-6 (2p) up to 4.4e-5 (4f z^3) in amplitude; it is now exact (closed form), checked for every orbital the app offers to 1e-10. The only numbers that move are the recorded jump amplitude sums in the tests; nothing you see changes."},
    {"0.13", "GPU update: superposition and photon jumps on the graphics card (stage 2 of 4)",
     "In a superposition (and while a photon jump is in flight) the processor used to work out, for every one of the 48,000 dots and every frame, whether the dot is shown, and its colour. This was the slowest part of the app (the overlapping superposition was the slowest scene of 0.12). "
     "Now, on a PC with OpenGL 4.3, a compute shader on the graphics card does that from the amplitudes stored with each dot, and the processor only sends the beat phase and the weights. A PC with only OpenGL 3.3 keeps the old way and nothing else changes: the app asks for 4.3 first and falls back to 3.3. "
     "The Performance card says which way your PC runs (\"OpenGL 4.3: GPU superposition on\" or \"OpenGL 3.3: CPU path\"), has a switch to compare the two, and the benchmark report prints it; scenes 25 to 28 run the jump and the three superpositions again with the old way. "
     "The physics is the same: the formulas are ported line by line and tested against the processor's for every dot. The graphics card works in 32-bit numbers, so a dot within rounding of a threshold (a few in a million) can come out differently; each such dot is tested to be that close. "
     "The tests also check the superposition against the textbook hydrogen formulas written out separately, and found that the lab's orbital normalisation is off by 3e-6 to 1.6e-5 in amplitude (not changed, reported). "
     "Not changed: the building of the dots of a jump (the random sampling is a chain that cannot run in parallel). "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. Scene 14 should drop from about 10 ms towards the 4 ms of one plain orbital."},
    {"0.13", "GPU update: phase colours on the graphics card (stage 1 of 4)",
     "The electron cloud's dots used to be coloured by the processor: for the 24 Wheel colours it went over all 48,000 dots once for each colour, in each of three depth bands. Now each dot carries only the number of its colour and the graphics card picks the colour. "
     "The picture is meant to be exactly the same (the same colours, the same depth bands, the same drawing order); the tests compare the old way and the new way dot by dot and, with a software OpenGL, pixel by pixel, and found no difference. "
     "This applies to every cloud view (Cloud, Cloud 3D, Slice), Classic and Glow dots, Sign and Wheel, every orbital and density. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. The benchmark's scene 8 should drop towards scene 9, and scene 10 likewise; scenes 23 and 24 run them again with the old way for comparison. "
     "A switch in the Performance card, GPU colours (Atom Lab cloud dots), goes back to the old way (it is On again at every start). "
     "Two texts in the benchmark's scene list pointed at the wrong scene (scene 19 said to compare with scene 8, it is scene 2; scene 22 said scenes 16 to 18, it is 19 to 21) and the tick box said 11 extra scenes (it is 15 now): fixed. "
     "The next stages plan OpenGL 4.3, which needs a graphics chip of about 2013 or newer (Intel HD 4600, GeForce 600, Radeon HD 7000); older PCs will not run those versions. The app's version is now 0.13."},
    {"0.12", "Atom Lab: the accuracy clean-up (stage 2 of 2)",
     "This stage fixes data and wording that earlier versions found doubtful and only reported. Nothing about how the atom or the chart works has changed; some numbers have, and each is listed here. "
     "Nuclides with no half-life number were all described as \"seen only fleetingly\". The data does not say that. They are now described by what NUBASE2020 says: 369 only have an extrapolation from systematics, 84 have none, 11 only a limit, 6 an approximate value, 3 are only known to be unstable to emitting a particle. "
     "The one-line text and the decay readout no longer show an extrapolation or a limit as if it were a half-life (fluorine-31 used to say \"half-life 2 ms\", boron-16 \"half-life 4.6e-21 s\" for a limit), and the decay clock no longer makes such a nucleus decay on a timer: it stays as you chose it, and the readout says why. "
     "Krypton-78 and xenon-124 had half-lives that are in neither NUBASE2020 nor the mendeleev package, which both call them stable; the numbers had no source, so they are now shown as NUBASE2020 shows them (observationally stable, with its limits). "
     "113 superheavy half-lives had been rounded and marked \"approximate\" although NUBASE2020 gives plain measured values: they are now NUBASE2020's own. 16 more superheavy values came from an older evaluation and were up to 2.8 times off (seaborgium-269: 14 minutes before, 5 minutes now). "
     "For 644 nuclides NUBASE2020 lists a decay mode with no intensity (marked with a question mark); the odds leave it out, and the hover, the line in the panel and the chain now say so. "
     "The nucleus's shape estimate counts Z = 114 and N = 184 as magic numbers; they come from the web version with no source, so they stay but are now labelled in the nucleus card (286 nuclides depend on them). "
     "This finishes version 0.12."},
    {"0.12", "Atom Lab: decay chains walk the chart (stage 1 of 2)",
     "On the Chart of nuclides, the chosen nuclide now shows where it goes next. Every way it can decay is drawn as a line from square to square, in the colour of the mode: an alpha jumps two squares down and two left, a beta minus moves one square up and one left, a beta plus the other way. "
     "The likeliest way is thick and solid, the other ways whose odds are measured are thinner, ways below 0.1 % are faint hairlines, and a way whose odds are not measured is marked with a question mark. Uranium-238 reaches lead-206 in 14 steps along its likeliest path; bismuth-212 splits in two paths (beta minus 64 %, alpha 36 %) that meet again at lead-208. "
     "A nuclide whose half-life is only a limit, only an extrapolation or not given is marked with a cross: the chain stops there and says why, and no time is made up for it. A chain that reaches a stable nuclide names it, and says when it is only observationally stable (nothing seen, but a limit is listed). "
     "You can also walk the chain: one step at a time, or automatically. Each step says what happened (for example: alpha, two protons and two neutrons leave; uranium-238 to thorium-234) and uses the lab's real random draws, a lifetime from the exponential law with the real half-life and a branch from the real odds, so it is different every time. "
     "The walk is NOT real time. Uranium-238 really lives billions of years; the screen shows a step every 1.6 seconds, and each step says how many times faster than real time it was shown. The nuclide you chose stays where you put it. "
     "The decay clock that runs in the other views now adds up the listed odds to 1 (before, a rare way listed after a certain one, such as fission for uranium-238, could never happen). "
     "A fourth benchmark scene measures the chain drawn on the landscape. The app's version is now 0.12."},
    {"0.11", "Atom Lab: what the height means, and the valley and the island of stability (stage 4 of 4)",
     "The Chart of nuclides has a new choice, Height means. Besides the half-life it can now be the binding energy per nucleon, the quantity behind the whole chart: it rises steeply from hydrogen, reaches its highest value in the data at nickel-62 (8,794.56 keV per nucleon, with iron-58 and iron-56 next; the app reads this from the table, it is not typed in), and falls slowly towards uranium. That is why fusing light nuclei releases energy and splitting heavy ones does. The scale is linear from 0 to 9,000 keV per nucleon, and the legend says which height is in use. "
     "The numbers are from the 2020 atomic mass evaluation (AME2020). It marks 1,008 of the 3,557 nuclides as estimates, not measurements, including every nuclide above Z = 110, so those are drawn flat and pale and never as a height. "
     "Switching the height changes nothing else: the colours, the magic-number bands, the hover text and the chosen nuclide stay exactly as they were. "
     "The chart now names the valley of stability, the band of stable nuclides that the data already shows, and says it is measured. It also marks the heaviest known nuclides (Z = 118) and says that nothing has been observed beyond them. "
     "The island of stability is a prediction: theory expects a region of superheavy nuclides beyond the heaviest known that might live much longer than their neighbours. The app says so plainly and draws nothing there, because no source for where it lies could be reached from the project's tools; it says only that where it lies is disputed. "
     "This finishes the chart of nuclides (version 0.11)."},
    {"0.11", "Atom Lab: the chart of nuclides as a landscape (stage 3 of 4)",
     "The Chart of nuclides view has a new Tilt slider. At 0 it is exactly the flat chart of the last version; as you move it up, the chart turns into a landscape seen from a fixed isometric angle, and every nuclide becomes a block whose height is its half-life. The stable nuclides stand in a ridge along the middle, and the short-lived ones fall away on both sides like a shore. "
     "The height is a chosen scale, not a measurement: the logarithm of the half-life, from 10^-23 seconds (flat) to 10^32 seconds (the top), where each step up is ten times longer; the legend says so. Stable nuclides have no half-life, so they all stand at one fixed height above the top of the scale, meaning no decay seen, not a long number. "
     "A nuclide with no measured half-life, or only an extrapolated one, has no height at all: it is drawn flat, hollow or pale. A pale block is only a limit or an approximate value. "
     "The wheel zooms about the pointer, a drag moves the picture when zoomed in, and a reset button brings back the flat chart. Hover and click work on the block you see on top, including its sides. "
     "A benchmark scene measures the landscape. The next stage is about what the height can mean."},
    {"0.11", "Atom Lab: the chart of nuclides (stage 2 of 4)",
     "A new view, \"Chart of nuclides\", is a map of every nuclide the lab knows: 3,557 squares, neutrons across and protons up. Each square is coloured by how that nuclide decays (stable, beta minus, beta plus or electron capture, alpha, fission, proton or neutron emission), and the legend says in words what each colour means. The stable nuclides run along the middle like a valley, with neutron-rich nuclei on one side and proton-rich ones on the other. "
     "The lines are the magic numbers 2, 8, 20, 28, 50, 82 and 126, where a shell of nucleons is full. The nuclide you have chosen is marked with a box and cross-hairs, so you do not lose your place. Hover a square for its half-life, how it decays and with what odds, its spin, and whether the lab has a measured charge radius and a measured quadrupole moment; where the lab has no value, it says so. "
     "Click a square to choose that nuclide: the other views then show it. The arrow keys move one proton or one neutron, which is the natural way to walk a decay chain. While the chart is shown the chosen nucleus does not decay on its own, so it stays where you put it. "
     "The spins and the kind of each half-life (a measurement, a limit, only an extrapolation, or none) come from NUBASE2020, which is now listed under Where the facts come from. The colours are a convention of this lab, not a measurement; where a decay has several branches, the colour is the kind with the largest total odds. The chart shows what has been observed: it says nothing about the island of stability or about heights, which come in the next stages. "
     "The app's version is now 0.11."},
    {"0.11", "Atom Lab: Auto photons now say why they cannot jump (stage 1 of 4)",
     "With Auto photons on, a complex orientation (m = ±1, ±2, ±3) or a superposition used to leave the atom doing nothing, with no word about it. Now the lab says why, in the same words the Emit and Absorb buttons use, once for each reason (not again at every look), and again if you change the state or switch Auto off and on. Auto stays on and starts jumping by itself as soon as you pick a real orientation (px, pz, ...). "
     "Nothing else changed: real orientations jump exactly as before (the same photons in the same test run). "
     "Still true: the lab does not do photon jumps from a complex state or a superposition. Light can in fact take such a state to another level (the rule l changes by 1 still holds; the light's polarisation decides which m it ends in); the lab refuses because it does not model the polarisation, and whether to allow it is left to the owner's decision. The next stages of 0.11 are the chart of nuclides."},
    {"0.10.9", "Atom Lab: the nucleus's shape from measured quadrupole moments (stage 2)",
     "The shape of a nucleus now comes from a measured quadrupole moment wherever one can be used: the IAEA's recommended values (fetched 2026-10-04, 598 ground-state rows). A positive moment means stretched (like a rugby ball), a negative one flattened (like a pancake), and the card and the readout line say which the nucleus on screen is. "
     "Where there is no usable moment the lab keeps what it had, and now says which: a B(E2) value (11 isotopes) or an estimate; of the neutron slider's 4,252 isotopes, 498 now take their shape from a measured moment, 11 from B(E2) and 3,743 from the estimate. "
     "The table gives the spectroscopic moment, the one a laboratory measures in the nucleus's spin state; the shape needs the intrinsic moment in the nucleus's own frame. The two are turned into each other with the strong-coupling relation (K = I), which the card calls a model assumption, not a measurement; it is poor for light nuclei and near closed shells, and where it would need |β₂| above 0.6 the moment is shown and not used. "
     "A missing moment never means round: a nucleus of spin 0 (every even-even nucleus) or spin 1/2 has no spectroscopic moment whatever its shape, and the card says so. Values without a sign, with a tentative sign or spin, or typed wrongly in the file are listed and not used. A deformed nucleus keeps its volume, and its protons still give the measured charge radius and have exactly the measured quadrupole. "
     "Doubly magic nuclei, drawn round by expectation, are now labelled an estimate. The shape is static: real nuclei also vibrate and rotate. Halo nuclei are not in this stage; they wait for data."},
    {"0.10.9", "Atom Lab: every nucleus at its measured size (stage 1)",
     "The nucleus is now drawn at its measured size wherever there is a measurement, always on (there is no setting). The numbers are the IAEA Nuclear Data Section's table of measured nuclear charge radii (fetched 2026-10-04): 908 isotopes of 90 elements. A new card, \"Nuclear size (measured)\", shows the measured radius with its uncertainty next to the usual rule's (r = 1.2 A^(1/3) fm) and the difference as a number. "
     "A charge radius is the rms size of the protons' electric charge, and the nucleus is drawn as a uniform ball of the radius that has that rms size (an assumption: real nuclei have soft edges). The nucleon cloud is scaled so its protons have exactly the measured rms radius. Light nuclei change most: helium-3 is 47 % bigger than the rule's ball, lithium-6 53 %, the deuteron 83 %; lead-208 changes by 0.1 %. "
     "Helium-4 is smaller than helium-3 and lithium's radius falls to lithium-9 and rises at lithium-11: the card shows those measured numbers and claims no explanation. Where there is no measurement (3,345 of the 4,252 isotopes the neutron slider offers) the old rule stays and the card says \"no measured radius for this isotope; size from the usual rule\"; 48 isotopes have only a preliminary value in the file, which is named and not used. Hydrogen-1 keeps the CODATA proton radius. "
     "Limits, said on the card: a charge radius is a size, not a shape, and not a matter radius (neutrons carry no charge), so halo nuclei and measured nucleus shapes are not in this stage; they wait for data. The picture is static: real nuclei also vibrate and rotate. The text \"The nucleus is ~N× smaller\" and the chance of the electron being inside the nucleus use the new size."},
    {"0.10.8", "Atom Lab: real lifetimes and selection rules, from NIST (stage 3)",
     "A new card, \"Lifetimes and jumps (NIST)\", says how long the state on screen lives and how likely each jump is. The numbers are NIST's: the rate A of each jump (the chance per second that the state makes it by itself) with the accuracy rating NIST gives it. "
     "The lifetime is 1 divided by the sum of the rates of every allowed way down, and the share of a jump is its rate over that sum. \"Check a jump\" asks about any two shells: if the jump is allowed you see its rate, rating and share; if not, the card names the rule that forbids it (parity, l or j) and, where NIST lists it as a forbidden jump, shows NIST's rate under that name. "
     "Where NIST has no rate the card says so and shows no number; the lab never makes one up. The 2s state of hydrogen has no allowed way down: the card gives no lifetime for it and says that a real 2s state still decays by emitting two photons, which NIST does not list and the lab does not model. "
     "The numbers and the rules are tested against NIST's own data (every allowed line in it is allowed in the lab, none of the forbidden ones is, rates scale as Z\u2074 across 109 ions); the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.8", "Atom Lab: the Dirac formula's arithmetic made exact, and a Windows test fixed (stage 2)",
     "The Windows build of the previous stage failed in its new tests. The cause was in the tests, not in the physics: a reference value was computed in a way that throws away about 11 of 16 digits (it subtracts 1 from 0.99997), and only passed on Linux because the Linux compiler's extra-long numbers hid it; the lab's own value was the accurate one. "
     "The tests now use a reference that is exact on every computer. The lab's own arithmetic was also looked at for the same weakness: the small relativistic correction to each level was a difference of two nearly equal energies and lost up to 5 digits for hydrogen; it is now worked out directly, as are the percentage and the 1s energy in the heavy-atom readout. "
     "Nothing you can see changes: every line, level, note and number shown is the same text as before (checked for all 118 elements), and with the setting off nothing at all changed."},
    {"0.10.8", "Atom Lab: relativity as a real option, the Dirac levels (stage 1)",
     "New setting under Light spectrum, \"Relativity (Dirac levels)\", off by default. Switched on, the two spectrum cards use the energy levels of the Dirac equation (one electron, point nucleus) instead of the non-relativistic formula: a level then depends on j, the total angular momentum, as well as n, so 2p splits into 2p 1/2 and 2p 3/2 and every line splits, more and more as Z grows. "
     "The strip shows the split lines, a short list gives the split levels, and the Real spectrum card now compares the lines the lab draws, component by component, with NIST's measured lines: the lab lands 8 times closer for hydrogen, 10 times for He\u207a and 32 times for Fe\u00b2\u2075\u207a. "
     "What it does not do, and the app says so: the pictures (clouds, orbitals, outlines, radial graph) are still non-relativistic, and so are the readouts, the photon of a jump and the quantum beats; there is no Lamb shift, no other QED effect and no size of the nucleus, so NIST's lines still differ a little (in Dirac theory 2s and 2p 1/2 have exactly the same energy). "
     "With the setting off, nothing changes (compared byte for byte with the previous version). The numbers are tested against hand arithmetic and NIST's data; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: the real spectrum from NIST next to the lab's (stage 2)",
     "A new card, \"Real spectrum (NIST)\", puts the measured spectrum from the NIST Atomic Spectra Database next to the lines the lab predicts, as two separate comparisons. "
     "(a) The ion the lab actually models (one electron round a nucleus of charge Z: H I, He II, ... Fe XXVI): the lab's lines should land almost on the measured ones, and what is left is mostly fine structure and relativity, which the lab leaves out. This checks the lab. "
     "(b) The neutral atom of the element (Fe I, Ne I, ...), which is what \"the spectrum of iron\" means: it has Z electrons and the lab has one, so the lines are not expected to match. This shows the lab's limit. "
     "Where NIST has no data the card says so. The lines were fetched on 4 October 2026 and are stored in the app (a selection, stated in About); intensities are NIST's rough relative values and are drawn only within one spectrum. "
     "The numbers are tested; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: how far from the nucleus the electron is found (stage 1)",
     "The Atom Lab's cloud views have a new card, \"Where the electron is found\": a graph of how likely the electron is to be at each distance from the nucleus, for the orbital on screen. "
     "It marks the most likely distance, the average distance (the same number as the readout's \"theory\") and how far the 90 % outline reaches, with the distance in Bohr radii and in Å, scaled for the orbital and the nucleus. "
     "Every orientation of a shell has the same curve, and the card says so. It is not drawn while a superposition is shown, and it waits for the landing during a photon jump. One electron, non-relativistic, point nucleus. "
     "The numbers are tested against textbook formulas, exact peaks and the dots themselves; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.6", "A test that did not hold on Windows, fixed (stage 9)",
     "A test of the faster cloud jump compared the results byte for byte with numbers recorded on another compiler; the last bits of cos, log and exp may differ between compilers, so it failed on the Windows build although nothing was wrong. "
     "It now runs the old way and the new way side by side on the same computer and compares them exactly, and compares the recorded numbers with a stated tolerance. Nothing in the app changed."},
    {"0.10.6", "Atom Lab: the cloud jump made cheaper (stage 8)",
     "The benchmark on a real PC showed the Atom Lab's cloud jump (an electron jumping between orbitals in the cloud views) was the slowest thing in the app: about 33 frames a second for the 1.2 seconds it lasts, with a long hitch at its start. "
     "The jump now does the work that is the same for every dot once per frame instead of once per dot, and builds its new dots over the first four frames instead of in one. The pictures and the random dots are exactly the same as before (tested bit for bit). "
     "It was not timed in the cloud session: the speed has to be confirmed by running the benchmark again."},
    {"0.10.6", "Atom Lab: speed scenes added to the benchmark (stage 7)",
     "The benchmark has a new tick box, \"Include Atom Lab speed-risk scenes\": 11 scenes for the Atom Lab's heaviest paths (phase colours with Classic dots, a cloud jump, a superposition, the 90 % outline's first use and with the cut dragged, the nucleus view). "
     "They were not timed yet (the cloud session has no graphics card); the numbers will come from a real PC, and nothing was made faster in this stage."},
    {"0.10.6", "Atom Lab: the nucleus's real mass, and spectrum checked against NIST (stage 6)",
     "The nucleus's mass in the Atom Lab now comes from the measured binding energies (AME2020), not from the mass number times a mass unit, so the light of deuterium and tritium is worked out with the right mass (deuterium's red line moves by 1.3 trillionths of a metre; nothing else visible changes). "
     "The tests now check the spectrum against the Rydberg formula worked out by hand and against the NIST Atomic Spectra Database, and check more basic facts about the orbitals."},
    {"0.10.6", "Atom Lab: clearer, more honest words (stage 5)",
     "The Atom Lab now says \"non-relativistic\" for every atom and warns the same way, from the same atom (cobalt, Z = 27) on, in the readout, the hint and the spectrum note. Visible light is 380 to 780 nm everywhere. "
     "The average radius from the dots appears only once the cloud is full, and says it is an estimate. Two Learn steps no longer claim more than the lab can show (the neon button, and the 2013 electron-wave picture)."},
    {"0.10.6", "Atom Lab: the right outline, a correct superposition (stage 4)",
     "The thin 90% outline of a complex state (m = +1, +2 ...) now has that state's own shape, a doughnut for 2p with m = +1 (it used to show the dumbbell of the real orbital). A superposition of two states that overlap, such as the same state twice, "
     "is now properly normalised, so its cloud is the right density and all its dots are shown; the line over the picture says when the mixture is renormalised. Other superpositions are unchanged."},
    {"0.10.6", "Atom Lab: one energy, a steadier orbit, Fixed scale (stage 3)",
     "Hydrogen's energy now reads -13.60 eV in every view (it said -13.61 in two of them): the readout uses the same level as the spectrum, with the nucleus's real mass. In the Orbit view the electron no longer breathes at the fastest playback speed "
     "(the orbit is calculated in smaller steps; the speed itself is the same). With Scale set to Fixed the biggest clouds (n = 4) now fit in the picture. A new note says the energy is the one-electron, non-relativistic level."},
    {"0.10.6", "Atom Lab: exact constants (stage 2)",
     "The Atom Lab now uses the official CODATA 2022 values (read from NIST) for the Bohr radius, the Hartree energy, the atomic unit of time, the Rydberg energy, the masses and more, instead of the web version's rounded ones. "
     "The Bohr radius is 0.03 % larger than before, so the electron's circle is a hair bigger; everything else looks the same. The About page lists the constants and where they come from."},
    {"0.10.6", "Atom Lab checked for errors", "Atom Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.5", "Orbit Lab: more checks and clearer sources (stage 5)",
     "Orbit Lab: tests now compare the lab with NASA/JPL Horizons for the famous dates (Mars closest, 2003; Jupiter and Saturn, 2020) and for the 2026 equinoxes (the Earth card's \"next season\" is within 12 minutes), and the texts of the jump notes, the Real and Simple notes, the date line and the hidden-selection rule are tested too. Sources: the speed of light is JPL's; the Sun's 25.38-day turning period is Horizons'; comets' nucleus sizes are the Small-Body Database's (Encke 4.9 → 4.8 km). Numbers that have no named source are now marked rough on the cards (the Sun's surface temperature and its 25- and 34-day turning, the belts' counts and masses, NEOWISE's size), and About lists them. Nothing else changed."},
    {"0.10.5", "The official astronomical unit (stage 4)",
     "The Orbit Lab now uses the official astronomical unit, 149,597,870,700 m (the IAU value, as on NASA/JPL's astrodynamic parameters page). Before, it used the web version's rounded 149.6 million km, which is 2,129 km too long at 1 AU (64,000 km at Neptune). Every distance in the lab, the same orbits in Simple, the comets, the belts and the Oort cloud, is now 0.0014 % shorter; the times and the orbits' shapes do not change. Against NASA/JPL Horizons no planet got worse by more than that change can explain (Uranus and Neptune about 30,000 and 39,000 km farther off in their worst year, the others within 1,300 km)."},
    {"0.10.5", "Orbit Lab data fixes (stage 3)",
     "Some of the Orbit Lab's numbers were replaced by JPL's. Planet surface gravity is now JPL's value at the equator (Jupiter 2.5 g, Saturn 1.1 g, Uranus 0.9 g, Neptune 1.1 g; the old values were up to 14 % lower for Saturn) and the cards say \"at the equator\". The Sun's radius is the IAU 2015 value, 695,700 km (it was 696,000). Each of the six famous comets now takes all its elements from one JPL record (Halley's, Hale-Bopp's, NEOWISE's, Encke's and Swift-Tuttle's from the Small-Body Database, 67P's from Horizons for the same orbit at 2021) instead of a mixture; against Horizons on 1 January 2026 they are closer to the real comet (67P 62 million km before, 1.7 million now). A comet is still one fixed orbit, so it drifts from the real one away from its record's date, and Halley's next return comes out in January 2062 on this orbit (JPL's prediction is 28 July 2061). The Simple note now says how far Simple is from JPL (worst: Saturn 8 million km by 2100). About lists the new sources."},
    {"0.10.5", "Orbit Lab text fixes (stage 2)",
     "The Orbit Lab's texts were corrected after the check. Dwarf planets and comets now say that they move on a simple orbit fixed at one date, so they drift from their real places the further the date is from it; the jump note after 2050 about extrapolation is only for Simple (Real says its errors were measured for 1800 to 1 Jan 2100); the Voyager caption is right for 1977 to 1989; the Oort cloud travel times are worked out from the distances next to them; the habitable zone, frost line and lifetime are marked rough and planet cards no longer say whether they are in the habitable zone; the real Moon's range is written 1800 to 1 Jan 2100; the Venus 2012 tile jumps to the middle of the transit; the no-atmosphere temperature says it assumes 30 % reflected; the Moon card says the phase names between the four main phases are this app's own; the Real note is shorter. No number or orbit changed."},
    {"0.10.5", "Orbit Lab checked for errors", "Orbit Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.4", "The real Moon (Orbit Lab, Real planet physics)",
     "With Planet physics set to Real, Earth's Moon is now the real one: its direction, distance and phase on the date shown come from the lunar theory ELP2000-82B (a 1988 theory), checked against NASA/JPL "
     "Horizons (DE441): at the dates checked between 1800 and 2100 it is within 5.2 km (2.9 arcseconds as seen from the Earth) at worst. Select the Moon to see its phase (new moon, waxing crescent, first quarter and so on) "
     "and how much of it is lit. Earth is now drawn where it really is, a few thousand km off the Earth–Moon balance point, which is what moves smoothly round the Sun. The Moon is still drawn at the "
     "exaggerated distance used for moon orbits, so you can see it. The other moons, and Simple, are unchanged; outside the years 1800 to 2100 the Moon goes back to its simple ellipse. At very fast Time "
     "speeds the Moon's orbit is lit instead of the Moon (as for every moon) and Earth is held at the balance point so it does not flicker. Real is still an option, and Simple is still the default."},
    {"0.10.3", "Real planets start from JPL's data (Orbit Lab)",
     "With Planet physics set to Real, the planets now start from NASA/JPL Horizons' positions and velocities (the DE441 ephemeris) for 1 January 2026 12:00 TDB, instead of the lab's own simplified orbital "
     "elements. Measured against Horizons' tables for 1 January of every year from 1800 to 2100, the worst error of any planet's position is now under 30,000 km (Mercury) with relativity off, and under 8,000 km "
     "for the others; before, the worst errors were millions of km for the inner planets and Jupiter, and over a hundred million km for Saturn, Uranus and Neptune. The lab's calendar dates are treated as TDB (the time scale "
     "of the JPL data) with no conversion from UTC; the note under Planet physics says so, and gives the worst errors. The General relativity (1PN) tick box is now off by default: measured against JPL, the Sun-potential "
     "term gets Mercury's extra turn right (about 43 arcseconds per century) but the motion along the orbit slightly wrong, so it moves every planet's position away from JPL (Mercury, worst case 1800 to 2100: "
     "70,045 km with it, 29,231 km without). The box is still there, and a setting you saved earlier is kept (press Defaults once to put it off). Real is still an option, and Simple is still the default, "
     "until it has been tried on a PC. Still left out: moons, dwarf planets (Pluto is not in) and asteroids pulling on the planets. Simple is unchanged."},
    {"0.10.2", "Phase colours, quantum beats and smooth photon jumps (Atom Lab)",
     "The Atom Lab can now show the phase of the electron's wave. New orientations m = +1, −1 (and ±2, ±3 for d and f) are complex waves, and a new setting, \"Phase colours: Sign / Wheel\", "
     "colours each dot by the phase of the wave with a colour ring (Sign is the old two-colour look and is still the default). New: \"Superposition\", two orbitals at once, such as 1s + 2p. "
     "The cloud then changes in time at the beat frequency (E₂ − E₁)/h: 1s + 2p sloshes back and forth, 1s + 2s breathes, 2s + 2p does not change at all (same energy here; fine structure and the Lamb shift are not modelled). "
     "Real beats are far too fast to see, so the picture is slowed down and the line under it says by how much. Photon jumps are now smooth: in the cloud views the cloud slides from one shape to the other, "
     "and in the Orbit view the electron glides between the circles (only to help the eye: Bohr's model has no path between circles). An emitted photon appears when the jump ends, and an absorbed photon starts the jump when it arrives. "
     "This is the textbook picture of a transition as a mixture of two states, not a calculation of how a photon is absorbed. With Reduced motion on, jumps are instant."},
    {"0.10.1", "General relativity, the Sun's wobble, and two fixes",
     "With Planet physics set to Real there is a new tick box, \"General relativity (1PN)\" (on by default), that adds Einstein's first-order correction to the Sun's pull, so Mercury's "
     "orbit turns about 43 arcseconds per century more, as measured. It is an approximation with the Sun's field only: it leaves out relativity between the planets, the Sun's spin and "
     "shape, and higher orders. Also in Real: the picture is drawn round the true balance point (the barycentre) from the planets' motion, the Sun's own path round it is drawn as a "
     "trail, and \"Exaggerate the Sun's wobble (x50)\" (on by default) draws that path 50 times too big so you can see it (it is labelled exaggerated, not to scale). "
     "Fixed: the Milky Way band no longer stops in the middle of the sky or goes missing when you turn the 3D view (most of it had been dropped), and the two secret themes show "
     "their names once unlocked instead of \"???\"."},
    {"0.10.0", "Planet physics: Simple or Real",
     "A new setting in the Orbit Lab, \"Planet physics\", has two choices. Simple (the default) is what the lab always did: every planet moves alone round the Sun and the planets do "
     "not pull on each other. Real lets the Sun and the eight planets pull on each other with Newtonian gravity, moved by a Wisdom-Holman integrator (a method made for planets). "
     "It leaves out relativity and the pull of moons, dwarf planets and asteroids on the planets, and the picture stays centred on the Sun. The start values are the lab's own simplified "
     "orbital elements for 1 January 2026, not NASA's ephemeris, so far from that date the planets drift away from their true positions (Mercury by about 12 degrees in 1800). "
     "Dates and Time speed work in both. The benchmark always uses Simple."},
    {"0.9.9", "Theme grid, clearer history button, readable nucleus label",
     "The 🎨 Theme menu is now a grid of cards, three in a row, each with its colour dot, its name and a tick on the one in use; the short description of a theme shows when "
     "you hold the mouse over its card, and names are never cut off, even at Extra large text. In the Atom Lab the header button that used to say \"Update 7.7.3\" now says "
     "\"Web Atom Lab history 7.7.3\" (it shows the history of the web Atom Lab that this Atom Lab follows), and the window it opens has the same name. The nucleus label in the "
     "Atom Lab (for example \"Cadmium-114 · 48p, 66n\") has a thin edge in the page colour, so it can be read over the electron cloud on dark and light themes. "
     "Sources added to About: the papers behind the star masses and the textbooks for the hydrogen-like orbitals."},
    {"0.9.8", "A 90% outline for the electron cloud",
     "A new setting in the Atom Lab's Cloud, Cloud 3D and Slice views, \"90% outline\" (Off by default), draws a thin line round the smallest region where the electron is found "
     "90% of the time. It is not a sphere: for p, d and f orbitals it follows the real shape (dumbbells, rings, clovers), and an orbital with nodes can have several surfaces. "
     "In Slice it is the line where the cut plane crosses that surface, in Cloud it is the region's shadow seen from the side, and in Cloud 3D it is a faint wireframe that turns "
     "with the cloud. It uses the same model as the cloud: one electron, no relativity, hydrogen-like orbitals; atoms with more electrons have different shapes. "
     "The benchmark always has it off."},
    {"0.9.7", "Reset to today keeps your view choices",
     "\"Reset to today\" (and the Today buttons in the Journey card and the Learn window) now sets only the date and time back to today, and clears the selection and Follow. "
     "It no longer changes your zoom or the Moons, Dwarf planets, Comets and Belts switches (it already kept your Time speed). Defaults still puts everything back. "
     "There is also a new roadmap file, ROADMAP.md, in the project, and the Windows download page is set up to say what the version is and how to get past Windows' warning for an app that is not code-signed yet."},
    {"0.9.6", "Quarks readable on light themes",
     "In the Atom Lab's quark zoom, the dark patch that was drawn behind the glowing quarks is gone on the light themes (Light and Paper), where it looked like a dark "
     "smudge. There the quarks, their halos, the gluon field and the sea pairs are drawn in darker, more saturated colours that read on a light page, and the red, green "
     "and blue colour charges are still easy to tell apart. On the dark themes nothing changes: the quarks glow as before. Only the drawing changed, not the physics."},
    {"0.9.5", "A smooth Milky Way, and star masses",
     "Orbit Lab: the Milky Way band in the night sky is now one smooth band with soft edges instead of three overlapping stripes, so there are no joints or "
     "stray grey streaks when you zoom far out; it is in the same place on the sky as before, and with the constellation names on it has its own label, "
     "\"The Milky Way (the plane of our galaxy, simplified)\". The far-away stars now show their mass under the name, in solar masses, taken from published "
     "papers: Alpha Centauri, Sirius, Procyon (each star of the pair), Proxima Centauri, Fomalhaut, Arcturus and Sagittarius A*. Masses worked out "
     "from stellar models say \"from models\", and a very uncertain one (Proxima) shows its range. Stars whose mass could not be checked against a "
     "published source are left without one. A line near the bottom says the masses are published estimates and some are uncertain."},
    {"0.9.4", "A glowing, denser electron cloud",
     "In the Atom Lab's Cloud, Cloud 3D and Slice views the electron cloud is drawn as soft glowing dots instead of tiny hard squares: where the electron is most likely to be "
     "the light adds up and glows, and thin places fade. In Cloud 3D, size and brightness change smoothly with depth. The nodes, where the wave is zero, stay as clear gaps, "
     "and the blue and orange phase colours are kept (on light themes the dots are darker so they show on white). Two new settings, Cloud style (Classic is the old look, "
     "Glow is new) and Cloud density (Normal 12,000 dots, High 24,000, Ultra 48,000), are remembered and put back by Defaults. More dots show the same probability more "
     "smoothly; they do not change the physics: the dots are found in exactly the same way, and the total light of the cloud is the same at every density. The benchmark's "
     "Cloud 3D scene uses Classic and Normal, so its results stay comparable with older ones, and the stress scenes got a Glow, Ultra Cloud 3D scene."},
    {"0.9.3", "Four fixes: benchmark scenes, guide buttons, benchmark input lock, Time speed on Today",
     "1) The Atom Lab benchmark scenes no longer pick up your own Atom Lab settings: they start from the lab's defaults, so \"Bohr 1913\" can no "
     "longer hide the nucleons (earlier results for the nucleus, one-nucleon and biggest-nucleus scenes taken with Bohr 1913 on are not valid). "
     "The result now says which settings each Atom Lab scene used. "
     "2) The two \"What am I even looking at?\" buttons in the Atom Lab panel no longer clash, so the red \"conflicting ID\" message is gone. "
     "3) While the benchmark runs, nothing but Cancel and Esc works: not the panels, the buttons at the top, the keys, nor the picture. "
     "The result says so. The window title also follows the lab that is on screen. "
     "4) \"Reset to today\" and the other Today buttons keep your Time speed; only Defaults puts it back."},
    {"0.9.2", "Alpha label, drop-down text fits",
     "The Orbit Lab's drop-downs (Sizes, Trails, Stars in the background, Constellations) show only the short name in the closed box, so "
     "it is never cut off at big Text sizes; the explanation is under each choice in the open list. "
     "The app is now labelled alpha: features are still being added. Beta will follow when everything planned for 1.0 is in, then the plain release."},
    {"0.9.1", "Sliders that look like sliders",
     "The Time speed and Zoom sliders have a clearly visible track and a bigger, easier-to-grab thumb in every theme, and their values "
     "are written above them instead of under the thumb. The \"Drag rotates the view\" button is no longer cut off."},
    {"0.9", "Keeps running while the window is moved or resized, a checked GPU timer, stress scenes",
     "The simulation and the drawing no longer freeze while you drag the window by its title bar or an edge (Windows pauses the app's own "
     "loop then; the app now draws from the window system's own refresh signal instead). The picture and the panels follow the new size "
     "while you drag, and after you let go nothing jumps or catches up. The Performance card counts the frames drawn during a drag. "
     "The benchmark's GPU time now covers the whole frame's drawing, and the timer is checked with a known load before the test: if it "
     "cannot be trusted, the result says \"not reliable\" instead of a number. A checkbox \"Include stress scenes\" adds two much "
     "heavier scenes (the biggest nucleus, and eight times the belt rocks) after the four normal ones."},
    {"0.8", "Benchmark, history with the native updates on top",
     "A Benchmark button in the Performance card (both labs): it runs four fixed scenes (the heaviest Orbit Lab view, the uranium-238 "
     "nucleus, Cloud 3D, one nucleon) with VSync off at the window's size, 2 s of warm-up and 8 s counted each, and shows the average "
     "frame time and fps, the 1 % low, the worst frame, the CPU work and the GPU time (where the driver gives it). The result is "
     "copied to the clipboard and saved as a text file next to the saved pictures, with the graphics card, the OpenGL version and the "
     "window size on top. It gives all your settings back, and Esc stops it. The 🕰 history now lists the native updates first, then "
     "the web's own history with its note."},
    {"0.7", "fps counter in both labs, the window remembers itself, icon",
     "The frames-per-second counter is now in the Atom Lab too (it was only in the Orbit Lab): top right of the picture, in the "
     "web's small see-through rounded box, in every theme and text size, and it is drawn in one place for every lab. It is not in "
     "a saved picture. The window opens with the size and place it had last time, and maximised if it was; if the place is no "
     "longer on a screen it opens centred, and it never starts smaller than 800 x 500. F11 turns full screen on and off. "
     "PhysicsLab.exe has an icon (an atom) on the file, in the taskbar and in the window. If the window or OpenGL 3.3 cannot "
     "start, a message box now says what is wrong and suggests updating the graphics driver."},
    {"0.6", "Save image, timeline pictures, cleaner text, small windows",
     "📷 Save image, next to Defaults in both labs, saves exactly the picture you see (the readout included, in every theme, "
     "grey in Monochrome) as a PNG in your Pictures folder (Downloads if that fails), named with the lab and the date and "
     "time. The button says Saved ✓ and the path is shown under it; if saving fails it says so. The Orbit Lab's timeline "
     "now has the web's small step pictures, like the Atom Lab's. The words that name the Black Hole Lab, the Spacetime Lab "
     "and the slingshot game are taken out of About, the Units note, the Sagittarius A* card and the guides, and the dead "
     "\"Try the slingshot game\" button is gone. Small windows: long button labels wrap instead of being cut off, and the "
     "Settings window and the big periodic table never get bigger than the window."},
    {"0.5", "The web app's own features: settings, themes, header, history",
     "Settings as in the web version: text size (Small, Normal, Large, Extra large), font (Theme default, Serif, Easy to read, "
     "Rounded, Monospace; Atkinson Hyperlegible and Nunito are built in, under the SIL Open Font License), units, motion (Auto "
     "follows Windows' animation setting; Reduced starts the labs paused, makes camera moves jump, stops Cloud 3D spinning), "
     "Reset my progress with the web's two pop-ups, and Developer > Unlock everything. The 🎨 Theme menu has the web's eleven "
     "themes, including the secret Synthwave and Monochrome; the panels, cards, text and pictures all follow it. Everything is "
     "remembered between runs in the app-data folder: the look, units, unlocks and every setting of both labs. A header across "
     "the top has the web's buttons: 🎨 Theme, 🌐 language (English; Eesti keel is coming), 👤 About (the web's page with its "
     "sources), ⚙ Settings, 🕰 history (the web's history, and this list) and the lab's Update badge with its changelog. "
     "Under it, the 💡 Did you know? line with all the web's facts (tap for the next one; it waits while the mouse is on it) "
     "and its hidden 🎁 surprise, and the tabs: 📚 Learn lists every guide and timeline of both labs and opens them in their "
     "lab; 🏆 Achievements shows Neon Nights, Sharp Eyes, Half-Life and Galactic Center, locked until earned, with the "
     "web's pop-up when one is earned. Point of No Return is left out: it belongs to the Black Hole Lab, which is not in "
     "the native app. The \"monochrome\" word in the Atom Lab guide unlocks the Monochrome theme and Sharp Eyes. "
     "Also: the Sun has the web's soft glowing patches that turn with it, antiquark rings in Cinematic add up their light "
     "as on the web, and the Atom Lab readout lost faint grey marks it had in light themes."},
    {"0.4", "Atom Lab",
     "The second lab: pick Orbit Lab or Atom Lab at the top of the panel (each keeps its state while the other is shown). "
     "Atom Lab is the web version's, feature for feature: any nucleus from hydrogen to oganesson with one electron, in four "
     "pictures (Orbit, as Bohr's 1913 circles or with the real nucleus; Cloud; Cloud 3D; Slice), photons that make the electron "
     "jump and build a spectrum, a zoom from the whole atom down to the protons and neutrons and the quarks inside them "
     "(Cinematic, Accurate or Classic quarks), every known nuclide with its real half-life and decay odds, decays happening on "
     "their own with a decay history, the periodic table with element cards and facts, the light spectrum, and the "
     "Temperature card with its little box of atoms. The four Learn guides are here too: What am I even looking at?, Scale, "
     "Weight and the 22-step timeline, whose buttons set the lab up. "
     "The physics uses the web's constants and steps (SI units inside), and the tests compare it with the web's own code: "
     "the electron's orbit, the sampled cloud, photon jumps and decays step by step with the same random numbers, and every "
     "text word for word. Cloud dots, glows and gluon particles are drawn as instanced shapes, a few draw calls for thousands. "
     "Units follow Settings (°F, mph, miles where the web converts them). Defaults asks first; Reset starts a fresh atom; "
     "More / Less info is remembered. The Performance card is on the Atom Lab panel too, with the picture's draw calls."},
    {"0.3", "Orbit Lab complete: Learn guides, Settings with metric or imperial, watch a planet turn",
     "New ⚙ Settings: show distances, speeds and temperatures in metric (km, km/s, °C) or imperial (miles, mi/s, °F). "
     "Every line of every facts card follows it, including the km/h and °C inside the notes, and so does the scale bar; "
     "the choice is remembered. The lab still works in metres and seconds inside. The Defaults button now asks before it "
     "puts Orbit Lab's settings back to how they started. "
     "With Stars set to Off, the picture now follows Windows' light or dark theme, as the web version follows the page "
     "theme. The Journey's last caption is now the same as in the web version. "
     "New Learn card with the three guides of the web version, word for word: What am I even looking at? (one page "
     "with a row of headings to jump to, following your 2D/3D and Sizes choice and the live date), Why do we have "
     "seasons? (with the lab's own Earth: day length and noon Sun height from pole to pole, and each planet's lean), "
     "and the Solar System timeline, whose ▶ buttons take the lab there. "
     "New button in the View card: pick a planet and press Slow time to watch it turn; time slows down so one turn "
     "takes about 24 seconds, and the view follows and zooms in on it."},
    {"0.2", "3D view, night sky, belts, spinning planets, Journey and Jump to a date",
     "The planets now really spin, with the real IAU axes and rates: Earth shows its real coastlines, the others a grid of "
     "latitude and longitude, each with a dashed axis and N; Saturn's rings tilt with its axis. The cards say how long a turn "
     "takes and how far the axis leans, the Sun's card how it spins, and Earth's card which season it is. "
     "Journey and dates card: the Journey zooms out from Earth to the centre of our galaxy, and when it ends (or you touch "
     "anything) all your settings and your view come back. Jump to a date puts everything where it really was or will be "
     "from 1800 to 2100 (Greenwich time); a date outside that range, or one that does not exist, now gets a note instead of "
     "a silent jump. "
     "New 3D view (View card): drag to turn it, right-drag or Shift-drag (or the Drag button) to move it, Spin to turn it "
     "slowly. Nearer things look bigger, and dashed lines drop from each planet to the flat plane of the orbits. "
     "Behind everything is the real night sky: the 2,874 brightest stars at their true directions and colours, the band "
     "of the Milky Way, and (Sky card) constellation lines and names. Zoom far out to find the far-away things at their "
     "real distance: the nearest stars, the Pleiades, the Orion Nebula, the galactic centre and the two Voyagers, all "
     "clickable and in Go to. "
     "The asteroid belt (with its Kirkwood gaps), Jupiter's Trojans, the Kuiper belt and the Oort cloud are here, "
     "with the same 6,000 stand-in rocks as the web version (same random seed, same orbits; the tests check they sit in "
     "the same places). Each belt is drawn with a single draw call, and the rocks are only moved when time moves. "
     "Click a belt's name or its Go to tile for its facts; a Belts and Oort cloud switch is under Solar System extras. "
     "A new Performance card shows how long each frame takes (the last one, the average and the worst of the last "
     "240 frames, with a small graph) and how much of it is the app's own work. A VSync switch turns off waiting for "
     "the screen, so you can see the real headroom instead of the monitor's refresh rate. Each Windows build is now "
     "also published on one fixed GitHub Release page, tagged \"latest\"."},
    {"0.1", "First native version: Orbit Lab (pilot)",
     "A native Windows version of Physics Lab, written in C++ with SDL3, OpenGL 3.3 and Dear ImGui. "
     "It has Orbit Lab only: the real Solar System where everything is today (the eight planets, the five dwarf planets, "
     "fourteen moons and six famous comets), with the same physics, numbers and method as the web version (leapfrog steps "
     "around the Sun, NASA/JPL orbital elements). Play and pause, Time speed, zoom with the mouse wheel or the slider, drag "
     "to move the view, click anything for its facts, Go to tiles, the habitable zone, distance rings, trails, the scale bar "
     "and the barycentre. Not here yet: the 3D view, the night sky and far-away things, the belts and Oort cloud, spinning "
     "planets, Journey, Jump to a date, Learn and the other labs."},
};

// The web Orbit Lab's changelog (the native Orbit Lab follows web Orbit Lab 7.7.2).
const Entry kWeb[] = {
#include "web_orbit_changelog.inc"
};
// The web Atom Lab's history (the native Atom Lab follows web Atom Lab 7.7.3).
const Entry kWebAtom[] = {
#include "web_atom_changelog.inc"
};
const char* const kWebAtomNote =
#include "web_atom_changelog_note.inc"
    ;

}  // namespace

// The stage word shown after the version number (change this one word to move from alpha to beta, then "" for 1.0)
constexpr const char* kStage = "alpha";

const char* appVersion() { return PL_VERSION; }
const char* appStage() { return kStage; }
std::string appVersionLabel() { return *kStage ? std::string(PL_VERSION) + " " + kStage : std::string(PL_VERSION); }

const NativeEntry* nativeHistory(int& count) {
    count = int(sizeof kNative / sizeof kNative[0]);
    static NativeEntry out[sizeof kNative / sizeof kNative[0]];
    for (int i = 0; i < count; i++) out[i] = {kNative[i].version, kNative[i].title, kNative[i].text};
    return out;
}

// The web's changelog pop-up for the lab on screen ("Orbit Lab · changelog"): each update's number, title and text
void labLogWindow(bool* open, int lab) {
    if (!*open) return;
    const app::LabInfo& L = app::kLabInfo[lab];
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = ImGui::GetFontSize() / 16;
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x - 32, 620 * s), vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string title = (lab == 1 ? std::string(app::kAtomHistoryName) + " " + L.update : std::string(L.name) + app::kLogTitleTail) + "###labLog";
    const bool logShown = pageBegin(title, open);                // the floating window as before, or (new UI) the reading view
    if (logShown) {
        const Entry* rows = lab == 1 ? kWebAtom : kWeb;
        const int n = lab == 1 ? int(sizeof kWebAtom / sizeof kWebAtom[0]) : int(sizeof kWeb / sizeof kWeb[0]);
        const ImU32 accent = col(look().w.accent), line = col(look().w.line);
        for (int i = 0; i < n; i++) {
            // .logRow: the number in a 52-point column (accent, bold), then the title and the text (muted, smaller)
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float x1 = p.x + 62 * s, w = ImGui::GetContentRegionAvail().x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x, p.y + 10 * s), accent, rows[i].version);
            ImGui::SetCursorScreenPos(ImVec2(x1, p.y + 10 * s));
            ImGui::PushTextWrapPos(p.x + w - ImGui::GetWindowPos().x);   // window-relative
            ImGui::PushFont(fonts().bold, 0);
            ImGui::TextUnformatted(displayable(rows[i].title, fonts().bold).c_str());
            ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(x1, ImGui::GetCursorScreenPos().y + 2 * s));
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 13 / 16);
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().w.muted));
            ImGui::TextUnformatted(displayable(rows[i].text).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
            const float y = ImGui::GetCursorScreenPos().y + 10 * s;
            if (i + 1 < n) dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + w, y), line);
            ImGui::SetCursorScreenPos(ImVec2(p.x, y));
            ImGui::Dummy(ImVec2(w, 0));
        }
        const char* note = lab == 1 ? kWebAtomNote : app::kOrbitLogNote;
        if (*note) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().w.muted));
            ImGui::TextWrapped("%s", displayable(note).c_str());
            ImGui::PopStyleColor();
        }
    }
    pageEnd();
}

}  // namespace ui
