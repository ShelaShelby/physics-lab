# 0.17 stage 1: new Defense Lab, the factory part (speed NOT tested on a GPU; Windows NOT tested)

## What changed
- A third lab, "Defense Lab" (Labs menu; app.lab = 2; saved as "Defense Lab" in the settings). Top-down 2D with fake depth (soft 3-layer shadows from the top left, lit top face, darker front face, lit edges). Not isometric, not 3D.
- src/defense (new library defense_core, no graphics): defense_world.* (map, buildings, belts, power, pollution, save), defense_lab.* (camera, placing, steps, autosave), defense_view.* (drawing, app only). UI: ui/shell_defense.cpp (build bar at the bottom: Belt, Miner, Furnace, Chest, Generator, Pole, Turn (R), Save; Controls panel: seed, seed field + New game, Save now, power, core inventory, pollution overlay, how to play, game numbers).
- Map: endless 32 x 32 chunks made on demand from the seed with our own integer hash + value noise (fbm, no new dependency). Deep / shallow water, sand, grass, rock; water is not buildable. Start area (40 tiles) always land with iron, copper, coal and stone about 13 tiles from the core; other patches in 48-tile cells, richer and bigger with distance.
- Buildings: core (3 x 3, cannot be removed, counts what is belted in), miner (2 x 2, on ore, needs power, outputs in front), belt (4 items a tile, fixed speed, back-pressure; a belt fed from one side only becomes a corner), furnace (ore -> plate, 2 stone -> brick, burns coal), chest (400), coal generator, pole (5 x 5 area, wires 7.5 tiles). Low power slows miners proportionally. Removing a building puts what it held into the core (no item lost).
- Pollution: one number a chunk from miners, furnaces and generators, spreading 2 % a second to each neighbour, 1 % absorbed; overlay in Controls > View; no effect yet.
- Save: defense_save.txt next to settings.txt, versioned text with a checksum, written to .tmp then renamed (MoveFileEx on Windows). Saved by the button, every 60 s and when the app closes; no offline progress. A damaged file: a message, a copy kept as .damaged, "Start a new game".
- Element names/symbols (Fe, Cu, C) come from atom::symbol. All speeds, costs, sizes, amounts, power and pollution numbers are labelled game design choices in defense_world.h, the Controls panel and About ("Defense Lab (a game)", native edit 37). Changelog 0.17 entry; CMake VERSION 0.17.
- Stage 2 / 3 hooks: Kind list (new kinds append; the save keeps numbers), entity list, World::pollution per chunk.

## Speed guard (counts; Linux, software GL; CPU noisy)
- Big test base (PL_DEFENSE_SAMPLE=big: sample + 64 lines of 48 belts between chests, zoom 6): 3,232 entities, 9,920 items on screen, 40 chunks, 44 draw calls a frame (one retained mesh a chunk + belts/buildings in one triangle batch + 2 square batches an item kind). CPU: simulation about 0.10 ms a frame, building the draw lists 0.34-0.38 ms. Unit test: 0.037 ms a tick for 3,201 entities.
- Small sample (zoom 24): 12 draw calls, 32 entities, 4 chunks, 0.023 ms draw-list CPU.
- No per-frame allocation in steady state (item and pole buffers reused, chunk meshes cached). Allocations still happen when a new chunk comes into view, when the layout changes, and at most once a second when pollution reaches a new chunk.

## Tested (Linux)
- New defense_logic (quick label, 62 checks): same seed = identical chunks (also in reverse order and 10,000 tiles out), other seed differs, all 5 terrains occur, no water and all 4 ores within 24 tiles for 12 seeds, ore richer far out, item names use atom::symbol; belts: 20 items on a line with a corner, 1,500 ticks: none made or lost, never closer than 64 units, packed 4 a tile at the blocked end, all reach a chest; chest-fed line fills and stops; a side-belt merge keeps count and gap; one item crosses 8 tiles in exactly 256 ticks; save -> load: identical state hash and text, identical after 1,200 more ticks; atomic write/read; 7 damaged files refused with a message, world unchanged. Broken on purpose: no target-gap rule -> the merge gap check fails; no checksum -> 2 damaged-file checks fail.
- New defense_pictures (real app, 22 checks): dark and light pictures of the sample base, draw counts, build bar on screen, core drawn in the middle, saved on close, the next start continues from the saved tick, damaged save -> message, .damaged copy, and a scripted "Start a new game".
- check.sh quick: 11 of 12 the first time (windows_decorated: tooltips must use ui::SetTooltip; fixed, re-run: passed). app_settings: About native edits 36 -> 37 (updated on purpose).
- Existing UI tests touching the Labs menu, pages and About, run once: shell_ui, shell_state, ui5g_pages, ui5k_pages, ui5k_log, ui5l_popups, ui5p_lit, ui5m2_windows passed. shell_pictures: shell_about_dark/light_1280x800 changed on purpose (the About page is longer: only the scrollbar thumb differs in view; looked at once), hashes updated, re-run passed. Full suite NOT run (job rule).
- Pictures looked at: sample base dark (with Controls) and light + the damaged-save panel.

## Not tested / not done
- fps on a real GPU, Windows behaviour, a real mouse (OWNER-TODO). Placing by real clicks was not driven by a script (only the logic).
- Not built (by design of this stage): splitters, junctions, aliens, turrets, ammo, building costs (placing is free in stage 1).
