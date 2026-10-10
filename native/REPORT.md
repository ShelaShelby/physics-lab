# Native Physics Lab 0.16 stage 1b: the old interface is removed, version 0.16

## Removed (the New UI is the only interface; PL_NEWUI is ignored)
- Old side panels (orbitPanel / atomPanel and their contents), the old header, the Atom Lab's bottom bar, the Atom Lab's "Old panel" button, its menu entry and floating window, `app.showOldPanel`, the old fps counter (fps_counter.*; the readout and Settings > Performance show it), the "New UI (preview)" setting, `prefs.newUi`, `newUiOn()`, the benchmark report's "New UI:" line (BenchEnv.newUi).
- Kept (shared with the New UI, moved nowhere): performanceCard, labSpectrumCard, atomChoices, spectrumPicture, the cards, periodicTableCells, resetViewAction, zoomLabel.
- Also removed: the large "Periodic table" window (atomBigTable, `app.bigTable`, the "big=" test field). Its only button was in the old panel; the New UI's element dropdown (chip) already holds the full table. Say if you want a button for it back.
- Not removed, listed for the dead-code job: `UiState.oldPanel` in app/shell.cpp (+ its unit test lines), `AtomLab.shellUi` (now always true), `headerH` plumbing in main.cpp (always 0), orbit_view.cpp's PL_LOG_CARD logging.
- A settings file with the old `newui=` key loads fine (the key is ignored, not written back): unit test "prefs: an old newui key is ignored..." in test_shell.inc (app_settings).

## Version 0.16
CMake VERSION, window title (from it), About, history entry (0.16), README, workflow release name "0.16 alpha (latest build)". Looked at: About shows "Native app: Physics Lab (native) 0.16 alpha".

## Tests
- Deleted (only subject was the old UI): theme_pictures (+ script, tools/theme_pictures.sh, 3 hash files), window_pictures (+ script).
- Converted: the old half now reads a saved run of the last build that still had the old UI (`tests/reference/old_ui_runs.json`, made by the commit before this one; `shell_lib.old_run`; nothing in it comes from the new code): shell_state, atom_panel, cards_atom (check_cards.py), ui5h_cards (check_5h.py), orbit_ui_card.
- Converted by dropping the old half and the "old = new" comparison, expected values written in the test: atom_toolbar, atom_chip (checks F-old and G, Bigger table, dropped), controls_panel (Old panel check dropped), shell_ui (part "old": now checks there is NO Old panel and PL_NEWUI=off is ignored), ui5g_default, ui5g pages ("off" runs dropped), ui5h_infosize (13 px old size dropped), ui5m_themes (now: PL_NEWUI=off is ignored, M1 stays on), ui5j_look_m1 (old-vs-new picture check dropped), ui5k_log ("new UI: on" line gone), ui5l_search (the 'newui' Settings row is gone from the expected list).
- Re-saved: shell_pictures hashes (About shows 0.16). The other 20 hashes also differed BEFORE my change (the unchanged old build gives the same pixels as the new one, but not the saved hash: this cloud's Mesa differs from the one that made them), so the whole file was re-saved here; it is a regression guard only.
- app_tests: the "New UI:" bench line check was removed (feature gone) and the line's absence is checked; "prefs: New UI ..." tests replaced by the old-key test.
- `check.sh full` ran once (before the test fixes: 18 of 71 failed, all old-UI related). The 17 converted tests (all but the 2 deleted) were then re-run one by one on the final build: all passed; ui5g_pages (its "off" runs dropped) was re-run too.

## Tested / not tested
- Linux, software OpenGL only. NOT tested: Windows (the run after push), speed, a real mouse or GPU. No physics, data or font test changed.
- Looked at 6 screenshots (2 more than the limit: the first Settings and About crops missed what I needed): Orbit and Atom default views (no Old panel button), Settings (no New UI row; Performance card present), About (0.16).
