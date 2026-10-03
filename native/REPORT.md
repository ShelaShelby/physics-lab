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
