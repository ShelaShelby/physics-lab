# 0.85.4: no freezes from a slow card fill; the benchmark on the owner's PC

Built and tested on the owner's PC (RX 9060 XT, AMD driver 26.11.2, VS 2026 Build Tools).

## What changed
- With the light paths working on the Radeon (0.85.3), a card fill took 1,964 ms and held the GPU's queue: blackhole.close_spin ran at 3.6 fps with a 1,780 ms frame. A first fill over 1,000 ms (kCardFirstFillSlowMs) or a later one over 250 ms (kCardFillSlowMs) now keeps the light paths "slow" for this shader and driver. The processor's table is used. Both limits are choices (spin_card.h says so).
- pc_tests.ps1 3e.rest.orbit: the check is now the rest line (quiet, no redraws). The log's "drawn anew" line came from the lab's start, before the rest is counted. It is shown as information.
- ROADMAP: the measured results below.

## Tests run
- Benchmark (Hard scenes, local build, 2560 x 1369, 255 Hz): close_spin 132.7 fps, 1 % low 63.8 (0.85.3 before this fix: 3.6 fps). The spinning scenes 110-135 fps with a new picture each frame (0.85.2: 900+ fps, 31-45 new pictures a second). orbit.real_fast 1128 fps, orbit.heaviest 1127, ui.settings_open 1059 (0.85.2: 616, 663, 620: the kept picture is used now). The run warned that the window lost focus in 24 scenes.
- pc_tests.ps1 -Parts items: rest.orbit quiet (0.9 % CPU, no redraws; 0.85.2: busy, 765 redraws), PASS.
- The 0.85.3 tests (unit tests, vk_compare_tests, pc_tests smoke and bh) still hold; this change touches only the fill's timing.

## Not done
- The sky per pixel (kKerrSkyFS) takes about 6 ms at 2560 x 1369 on the Radeon. The spinning scenes are below 255 Hz. The owner chose no lower resolution while moving (DECISIONS, 17 Nov 2026): the shader must get faster (ROADMAP).
- rest.blackhole_glare is still busy at rest (14 % CPU with the gas off). Its cause is not found yet.
- check.sh job: its UI tests are Linux only.

## References changed on purpose
- None.
