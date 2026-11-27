# 0.94.1: Black Hole Lab picture and camera, moons under their planets, two benchmark faults

## What changed
- A (dotted curve): not reproduced as described. Found near the plane: one thin dotted curve along the shadow's edge, ending on the disc's straight edge. It is the disc's order 1 and 2 images, thinner than a pixel (Real spin 0 at 15 GM/c², 0.7° below: 5,752 and 246 pixels, the same in the CPU lookup and the GPU). Real physics: kept, one sentence in the guide. No shader change.
- B: the camera stays outside the disc's thickness (DECISIONS 27 Nov 2026). lab.h camDiscPitch / keepOutsideDisc: |z| >= h(R) + 0.1 GM/c² over the disc (Real: DiscPhys::height, every spin; Film: margin only). In turn, glide, wheel, loadSettings (typed and saved), presets, the paper's view and each cameraTick. The Camera section says why it stops.
- C: Pick a body lists each body's moons under it behind "▸ n moons" (orbit/pick_list.h, moved from ui/orbit_actions.cpp so tests read it). No Moons group. A picked body opens; a selected moon keeps its planet open. The filter shows a matching moon under its planet.
- D1 cause confirmed: the porkchop worker's finished grid was taken only while the Data panel drew, so celestialBusy() stayed true. Now celestialTick polls it every frame. A real user was affected too: a step's time limit could run out while the panel was shut and switch the card path off.
- D2: the card's hottest-ring search (560 flux evaluations, about 7 calls a frame since 0.87.0) is kept per spin. readout 0.034 -> 0.022 ms a call here. The rest of the owner's 58-62 % is not found (Not done).

## Tests run
- orbit_tests 3110 checks, 0 failed (testPickList094 new; it does not build on 0.94.0). blackhole_tests 1223, 0 failed (testCameraOutsideDisc094 new, same). app_tests 202,492, 0 failed.
- check_6b.py benchporkchop (new, ui_screens, 17 s): the two waits take 2-110 ms. With the new poll switched off by hand: 15000 ms (capped), as on the owner's PC.
- check.sh job: quick 18 of 18 passed, ui_smoke passed. Pick list checked on screen (Jupiter opened, Io picked).

## Not done
- A: views tried on the build machine (GPU pictures, feeding disc, black sky): Real spin 0 at 8, 15, 30 GM/c², Real spin 0.5 at 30, spin 0.9 at 15 and 30, Film at 15, 30, 74; pitch 0.3-3° either side of the plane. Lookup against card at 15, 30, 57.5 GM/c², ±0.7° and ±2°: at most 6 pixels differ. ROADMAP asks the owner for his settings if his curve is another one.
- D2: on the build machine 0.85.2 and 0.94.1 use the same frame-thread time at rest (shell 0.6-0.75 ms, draw 0.2 ms) and the same software-renderer threads. The cause is on the owner's PC (driver or GPU path). ROADMAP line.
- Needs pc_tests.ps1 on the owner's PC: the Black Hole picture checks on the Radeon, rest.blackhole and rest.blackhole_spin, the two waits.

## References changed on purpose
- testPickList094: two expectations corrected before they first passed: "titan" also finds Titania (under Uranus), "jupiter" also finds the belt Jupiter's Trojans.
- Benchmark: blackhole.all_light keeps the camera outside the disc (it now jumps the thickness at the plane). blackhole.edge_on's camera stops just under the surface. No UI check clicked a moon, so none needed updating.
