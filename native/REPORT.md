# Native Physics Lab 0.9.9 (alpha) — report

Small UI fixes. No physics changes. Older reports are in git history. `index.html` was not touched.

## What changed
- **A. Theme picker as a grid** (`ui/settings.cpp`, `app/grid.h`). The 🎨 Theme menu is now cards in 3 columns, rows added as needed. Each card: the colour dot (Auto keeps its half-and-half dot, the two secret themes show "?" and "???"), the bold name, a tick on the current theme. The description line is a tooltip (also on the locked cards, where it is their hint). Same Theme button, one click switches and closes, as before. The cards are as wide as the longest name needs; the popup is exactly as wide and tall as the grid (cursor-sized, no empty space). If three cards would not fit in the window, the cards get narrower and a long name wraps onto a second line, so a name is never cut off (this narrow-window path was written but **not seen**). The grid arithmetic is in `app/grid.h` (`gridRows`, `gridCell`).
- **B. History button** (`ui/app_pages.cpp`, `ui/changelog.cpp`, `app/app_text.inc`). In the Atom Lab the header button "Update 7.7.3" is now "Web Atom Lab history 7.7.3", with the tooltip "History of the web Atom Lab that this Atom Lab follows"; the window it opens is titled the same. The number still comes from `kLabInfo` (nothing typed twice). The full name fitted at Extra large text on the 1,280-pixel-wide window, so the short form "Web history" was not needed. The word-for-word references (`kHeaderRef`) are untouched and still pass: the web's own "Update" text and tip are still in the data, only the Atom Lab's header uses the new words. **Only the Atom Lab was changed, as asked: the Orbit Lab's button still says "Update 7.7.2"**, which has the same naming problem if you want it changed too.
- **C. Nucleus label** (`atom/atom_view.cpp`). The label under the nucleus ("Cadmium-114 (¹¹⁴Cd) · 48p, 66n") is drawn with the existing text-edge rule (8 copies a point and a half away, here in the page's card colour at 90%), the same rule the mass label next to it already used. Not moved. It is one line of code common to the Orbit, Cloud, Cloud 3D and Slice views (the Bohr 1913 label is a different text and was left alone).
- A test hook: `PL_OPEN=themes` opens the picker at frame 12. Version 0.9.9, 🕰 entry, README, ROADMAP (0.9.9 done).

## Tests (full suite once before the commit: `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,321; all pass; no saved reference changed)
- New: the grid arithmetic for 1, 3, 4, 11 and 13 themes (rows 1, 1, 2, 4, 5; the last card at row 0 col 0, row 0 col 2, row 1 col 0, row 3 col 1, row 4 col 0), every card in its own place in order, 0 cards, and that the app has 11 themes in 4 rows.
- **The panels and drawing are not linked into the test programs.** The popup, the tooltip, the button text, the title and the label edge are therefore **not tested**; only the grid arithmetic is. Nothing here claims the look as tested.

## Looked at (Linux, Xvfb, software OpenGL)
Two attempts at the grid were blank mistakes of mine (the test popup opened at the window middle and I looked in the wrong place), so I fixed the hook and the cropping; the following were looked at once each:
- Theme grid at normal text size (Dark): 3 columns by 4 rows, Dark ticked, locked cards "???", names in full. Good.
- Theme grid at Extra large text with the Atom Lab header: the cards are wider, "High contrast" is in full, the popup stays in the window; the header shows "Web Atom Lab history 7.7.3" in full on one line next to the other buttons (the shot still showed "v0.9.8 alpha" for the native history button because it was taken before the version bump).
- Cadmium-114 over a dense 4f Cloud 3D, Dark and Paper: the label is readable on both. The edge is thin; the label sits mostly in the gap between the lobes, so it was not tested over the very densest part.
- Not looked at: the tooltip (it needs a mouse hover), a narrow window, Orbit/Slice views with the label, other themes' cards, Extra large with the Orbit Lab.

## Not tested
- No Windows PC and no GPU: nothing about speed or the real-screen look.

## Stage 2: sources added to the About page (same version, 0.9.9; no code or physics change)
- **Added** to "Where the facts come from" (`app/app_text.inc`), in the same link style:
  - Stars and far-away things: Pourbaix & Boffin 2016 (Alpha Centauri A, B; arXiv:1601.01636), Zurlo et al. 2018 (Proxima; arXiv:1807.01318), Bond et al. 2017 (Sirius A, B; arXiv:1703.10625), Bond et al. 2015 (Procyon A, B; arXiv:1510.00485), Mamajek 2012 (Fomalhaut, from stellar models; ApJ 754, L20; arXiv:1206.6353), Ramirez & Allende Prieto 2011 (Arcturus, from stellar models; ApJ 743, 135; arXiv:1109.4425). Each links to `https://arxiv.org/abs/<id>`; the ids are the ones written in `far_masses.h`.
  - The existing GRAVITY Collaboration 2022 entry was **not** added again; its words now say it is the source of the Sagittarius A* mass (4.3 million Suns).
  - A muted note: the masses are published estimates in solar masses, some uncertain, Fomalhaut and Arcturus come from stellar models, **the numbers were read from the papers' summaries and not checked against their tables** (as the 0.9.5 report says), and stars without a source show none.
  - Atoms and particles: the two textbooks named in the 0.9.4 test comments, by author and title only: Griffiths, *Introduction to Quantum Mechanics*, and Bethe & Salpeter, *Quantum Mechanics of One- and Two-Electron Atoms*. No edition, chapter or equation numbers (they were never checked), and no link.
- **Left out, and why:**
  - **The galactic centre and north galactic pole** used for the Milky Way band (`kGalCentre`, `kGalPole` in `sky_data.inc`): the comment there names no source (the file is generated from the web version, which names none either). In 0.9.5 I checked them against the standard J2000 values quoted from memory, which is not a source I can name, so nothing was added.
  - **The 90% region** (0.9.8): the code (`outline.h`) calls it "the standard textbook definition" but names no book, so no source was invented for it.
  - Sources the code names only for the Orbit/Atom physics before 0.9.4 were not looked at (outside this task).
- **Test reference changed on purpose:** "About page (the web's, with the native edits)" in `app_tests` builds the expected About from the web text plus a list of native edits. The existing GRAVITY edit now has the new wording and the six star entries after it, and two edits were added (the masses note after the "Distances of Polaris..." line, the textbooks after the Bohr 1913 entry). The counter "every native edit was used" went from 3 to 5 About edits. No check was loosened: the expected text is word for word the new About text.
- `CLAUDE.md`: the rule that every source must also be listed in the About page. The 🕰 0.9.9 entry has one more line ("Sources added to About...").
- **Tests** (full suite once, before the commit): `orbit_tests` 1,369, `atom_tests` 17,818, `app_tests` 1,321, all pass. The look of the About page was not checked on screen (no new screenshot).
