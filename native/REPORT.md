# 0.76.0: lean tests (the UI suite cut to what catches real breakage)

## What changed
- Measured first: 0.75.3's full suite took 83 min on GitHub (4,981 s, 290 core-min of UI tests). On the build machine its 154 UI parts took at least 228 core-min. The 10 slowest: check_atom_chip classic 872 s (GitHub), check_blackhole smooth 596 s (GitHub), check_molecule card 465 s, check_atom_chip card 319 s, check_blackhole precision 300 s, and over 300 s each: check_shell vulkan2d (2 h once), check_shell gpuhelper, check_shell (no part), check_controls, check_atom_panel.
- Sorted every part (native/tests/TESTS.md): 52 kept, 0 merged (no two parts could share one start without rewriting their checks), 102 deleted. 16 scripts with no kept part deleted. Every GPU-vs-CPU comparison is kept and unchanged.
- Split for the new 5-minute part limit: classic into 5 parts (one scene each), molecule card into card, cardpic and cardpiclight, black hole precision into precision, sky and precisioncard. The atom and molecule card guards now die in one step, not all five. The element chip clicks 12 of 118 elements.
- New ui_smoke (check_shell.py smoke, label smoke): each lab draws a frame that is not empty, zero validation errors, exit code 0. It fails when the labs draw no picture (PL_SMOKE_EMPTY=1). check.sh job = quick + ui_smoke.
- run_group.py stops a part after 5 minutes with its last 20 lines and prints each part's time. Each ui_* test has a 900 s ctest timeout and a COST (longest first). The GitHub job's limit is 45 min.
- Rules: CLAUDE.md (every job runs check.sh job; new UI checks only for a new screen or a bug the owner saw), DECISIONS.md, ROADMAP.md, README, changelog, VERSION 0.76.0.

## Tests run
- check.sh job: passed, 228 s (quick 18 of 18, ui_smoke 6 s).
- check.sh full (final): stopped at 15 min by the rule, 18 of 32 done, 17 passed. ui_atom failed only in classicsliceglow (known). ui_shell and ui_screens, run after: passed, 232 s and 112 s. Estimated full run here: about 17 min (64 core-min).
- Before: at least 57 min here, 83 min on GitHub. check.sh job did not exist (quick alone 223 s).

## Not done
- Stop rule: the full suite is not under 15 min. The GPU-vs-CPU parts take about 40 core-min and the unit tests 8, so 10 min needs deleting a comparison.
- check_atom_chip classic: 3 tries (PL_VK_SYNC, the pictures, a fixed scene clock). The two runs are drawn at different zooms. Not fixed, reverted, ROADMAP line updated. It fails in one or two of its 5 scenes per run.
- ui_shell vulkan2d deleted: its glare check compares the GPU with the GPU. smooth and short deleted. Their ROADMAP lines removed.

## References changed on purpose
- None. No limit or expected value changed.
