# Native Physics Lab 0.16 stage 1a: the Atom Lab's old-panel gaps get a New UI home (nothing removed)

## What changed
- Dirac levels switch: Controls panel, Photons section (shell_atom.cpp), same key "dirac", Off/On, same words. The choice table moved to atomChoices("dirac") (atom_panel.cpp); the old row uses it too.
- The lab's own spectrum (explanation, picture, note, Dirac level rows only when On): one function labSpectrumCard() in atom_panel.cpp, called by the old panel and by the data sheet's Spectrum tab (below the NIST card, under a "Light spectrum" heading). No new text.
- "Restart atom" in the Controls panel View section, next to Build another / Reshuffle: calls the same L.restart() as the old Reset. (It is in the Controls panel, not a menu: that is where those two live in the code.)
- Draw-call line: now printed by performanceCard() when the Atom Lab is open (Settings and the old panel); the old panel's own copy was removed so it is not shown twice.
- Test hooks: PLASTATE now also prints dirac; a "drawcalls shown" audit note.

## The three extra old-panel items (home found, nothing added)
- Decay history: the data sheet's Decay tab (shell.cpp:223, from atomSubject().decayTab).
- Atom Learn buttons (4 guides): the Learn tab's Atom Lab cards (app_tabs.cpp:219-226, app_text.inc:227-230); menu "Learn" and the top-bar Learn icon open it.
- "What am I even looking at?": the first Atom Lab card in the same Learn tab (guide 1).

## Tested (Linux, software OpenGL; not Windows, not a real GPU)
- New test ui6a_atom_gaps (tests/check_6a.py): 3 themes, 1280x1800. FAILED on the old code (18 of 18 checks), passes now (24 checks).
- check.sh quick exit 0. ctest: atom_toolbar, atom_chip, controls_panel, atom_panel, cards_atom, ui5h_cards, ui5i_sheet, app_settings, ui6a_atom_gaps (see final result below).
- Looked at: Spectrum tab with Dirac On (picture, note, level rows fine), Controls panel (Restart atom, Dirac switch fine).

## Not tested
- Windows run, speed, real mouse. Not looked at: the Settings performance card (it is below the Settings window's scroll; only the audit note proves it is drawn). No full suite.
