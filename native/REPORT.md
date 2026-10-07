# Native Physics Lab 0.15 — stage 5k (bug-fix stage 1 of 2)

`index.html` untouched. No helper agents. New UI Off = the old app. No physics, formula or value changed. No full suite (planned for stage 7).

## The cause of "About does not scroll" (found)
In the New UI the page's text started at a fixed SCREEN position (`SetCursorScreenPos` in `ui/reading.cpp`). So the scroll number changed (PageDown, the wheel, Contents all moved it) but the text never moved on the screen. The cloud tests (5g) only read that number, so they passed. Proof: before the fix, 0 of 29,987 sample pixels of the About column changed after PageDown; after it, thousands do (test `ui5k_pages` now compares pixels). The same fault made every Contents jump land at a wrong place, because the heading positions were measured relative to the scroll offset. Fixed: the content starts at a fixed place in the window's own coordinates and is clipped under the header (title and buttons stay in place).

## Changed (Part 1)
- **Robust scrolling at app level** (`readingFrameEnd`, after the Dear ImGui frame): wheel over the page or over the dimmed picture, and PageUp/PageDown/Home/End/Up/Down, scroll the open page whichever window has the focus or the hover; skipped when a popup is open, the Contents list or another scrolling panel is under the pointer, or a text box is active. Never twice for one event (nothing is added if the page already moved). With a page open the wheel no longer zooms the picture behind it and the arrow keys no longer walk the Chart of nuclides.
- **Slim scrollbar** (about 8 px, rounded, muted, wider and brighter on hover, draggable, none when the page fits).
- **Contents list:** a choice keeps aiming at its heading for 8 frames (the page's lines are measured as they are drawn, so headings shift about 1 %); the list is kept in front of the page; a press and release found by place also counts; the dropdown closes after a choice.
- **Hidden input log:** Ctrl+Shift+F12 writes `physics-lab-input.txt` into the settings folder (Windows: `%APPDATA%\PhysicsLab\PhysicsLab\`) and shows a toast with the full path. Plain text, about 25 lines: time, window size, theme, Motion, New UI, hovered and focused window, WantCapture flags, the open page's scroll / range / flags / rect, dimmed layer, Contents list, popups, the last 20 wheel and key events.
- Test hooks: script verbs `pscroll`, `key=ctrl+shift+f12`; logged rects for the scrollbar, Contents rows, windows.

## Changed (Part 2)
- **Handle** (the chevron pill) was drawn through open windows because it was brought to the front every frame; that call is gone, so windows opened later are above it.
- **Toggles:** the top-bar Settings, Theme and Search buttons now close on a second click (the Theme list needed the press found by place: Dear ImGui does not hand that click to the button).
- **New gear icon:** a ring with 8 teeth, same thin stroke as the other icons. Looked at 2x crops in Dark and Y2K Night: a clear cog.
- **Floating windows (New UI only; Off unchanged):** never taller than the room under the top bar, and on opening / after the app window changes size moved inside (8 px above the bottom). **Benchmark:** the scene list scrolls inside; the check boxes, Start and Cancel are always in view. Stop is in the small run box; the Result window (Save) was not changed.

## Tests (Linux, software OpenGL, scripted events)
- New `ui5k_pages` (225 checks: 5 pages, 3 sizes, Dark and Y2K Night; wheel 14 frames after the pointer arrived; focus in the Contents list, the Controls panel, the Legend in the Atom chart view; keys; dimmed picture; pixels move; scrollbar rect exists exactly when the page scrolls; thumb drag moves the page by the expected amount), `ui5k_contents` (104: every reachable entry, the same entry twice, after a wheel, dropdown and rail), `ui5k_log`, `ui5k_windows` (3 sizes: inside the window, Start in view, a click on Start starts the run, handle below windows, handle pixels), `ui5k_toggle`.
- Break on purpose: fallback off, keys and "focus elsewhere" and dimmed-wheel cases fail (stay at 0); Contents jump made a no-op: 87 of 104 fail; old handle order back: z-order and pixel checks fail; clamp off: the Settings window check fails at 880x560 (the Benchmark window stayed inside anyway, because its own list layout now caps it). All reverted.
- End runs (each part, once): `check.sh quick`-set, ui5k_*, ui5g_*, ui5h_*, ui5i_sheet, ui5j_look_m1, orbit_ui_card, orbit_ui_coverage, the five guards, shell_pictures. All green except `shell_pictures`, rewritten on purpose: part 1 the 8 About pictures (scrollbar), part 2 all 48 (the gear icon is in every top bar). Guards for New UI Off matched unchanged. `windows_decorated` needed one allowed-list entry for the scrollbar's transparent window (with reason, as that file prescribes).
- Windows run of part 1 (commit d54afc6): green.

## NOT tested
Windows run of part 2 (see below if added), a real mouse, a real GPU, speed, 125 / 150 % scale. Resizing the app window while a window is open (the clamp's resize path has no scripted resize). The Theme / Search toggles by a real mouse. The scrollbar was dragged by scripted events only. The wheel fallback rules were not seen with real hover states: the cause on the owner's PC was the layout fault above, which the cloud could now show.

## Honest notes
- The About page's scroll range grows a little (about 1 %) as it is scrolled, because lines are measured when drawn: `End` may need a second press to reach the very end.
- Not fixed (stage 5l, planned): settings search and the stuck Moon fact, Cloud / Cloud 3D double highlight, Nucleus button, toolbar centring, Labs dropdown, Legend closing on an outside click.

## For the owner
Press Ctrl+Shift+F12 in the app: it writes `physics-lab-input.txt` (path shown on screen). If About still does not scroll, send me that file.
