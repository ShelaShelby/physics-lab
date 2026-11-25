# 0.85.2: Fixes and diagnostics after the owner's PC run of 0.85.1 (build machine only)

## What changed
- Item 1: ui_orbit's "the same fields" failed on the new "light" field. It is True sunlight (0.85.0, s.trueLight). check_shell_state.py now drops it, as it does acen and moonoff. gr=1 against gr=0 is the 0.29 default change: old_gr_029 compares it, and that check passed.
- Item 4a: vk_compare_tests' stdout is unbuffered, and each check flushes. It prints "PART <name>" before each part. pc_tests.ps1 reports the last part begun.
- Item 3a: the PLBHTONE line's ok now includes the test's limit (at most 1 % over 8 levels). expok is the exposure alone. Before, the case close printed ok=1 at 1.99 %.
- Item 6a: the rest line now lists each Orbit redraw by its reason (a changed key part, or the key unchanged and why). Frames a benchmark scene moves are no longer "at rest". The owner's "camera 30 of 30" note came from a scene: reproduced here at 31.9 s, during the Orbit scenes.
- Item 2, diagnosis: the sky's first full picture counts as finished only when its timestamp pair is read back (VkBlackHole::times()). The first Hard scene, blackhole.real_move, starts at the same 1.3 s. Cause not found without the PC's log.

## Tests run
- check.sh job: quick 18 of 18 passed (vk_cards included), ui_smoke 1 of 1 passed.
- rest.orbit on the build machine (lavapipe), keep on: 0 redraws at 1280 x 700, 1 at 2560 x 1369, 0 after the seven Orbit Hard scenes. The PC's 765 does not reproduce here.
- check_shell_state.py by hand: 121 checks, 0 failed (it failed 1 of 121 in 0.85.0 and 0.85.1).

## Not done
- The owner's PC could not be reached: Remote Control lists the computer, but it offers no folder to cloud sessions (reason not given). So items 2 (cause), 3a (the shader), 3b, 4a (the crashing check), 4b-d, 5, 6a (the PC's cause) and 9 were not done. They stay on ROADMAP.
- Item 5: not split blind. A split could not be checked against the driver that crashes.
- Items 6b, 7, 8a, 8b: the build machine's GPU is a software renderer. atom.emit_all runs at 4.5 fps here, so the press frames cannot be told apart. 8a and 8b are new ROADMAP lines.
- No hardware run of 0.85.2: no pc_0.85.2.md or bench_0.85.2.txt.

## References changed on purpose
- check_shell_state.py: "light" left out of the field comparison (the old panel's saved run never had True sunlight).
