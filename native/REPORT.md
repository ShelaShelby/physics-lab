# 0.17 stage 2: Defense Lab fixes and new look (speed NOT tested on a GPU; Windows and a real mouse NOT tested)

## What changed (the owner's five findings + handling)
- Power: the core has a free emergency generator (kCoreWatts 180 kW = 2 miners, always on, joins a network through a pole in range like any generator; coal generators burn only for demand beyond it). New game: core holds 50 coal. A left click on a coal generator moves core coal into it (up to 10); holding repeats every 0.25 s. How to play explains it in 3 lines.
- Map: fixed 512 x 512 tiles (16 x 16 chunks), 8-tile dark wasteland band, all constants in defense_world.h (kMapTiles, kMapMin/Max, kBorder, inMap, chunkInMap). Nothing generated or buildable outside; camera clamped to map + band, zoom-out limited (centred) when the window is bigger. Pollution only spreads to chunks inside the map. Big test base moved to (-230, -200).
- Terrain: water and sand removed; one invented red-rock biome with Dust / Pits / Cracks / Stones looks (all buildable) + Waste outside. Ore infinite (amounts, richness and the "m" save lines removed); patches grow with distance (r = 3.5 + min(7, d/40) + up to 2.5 tiles).
- New look, all code-drawn: panel floor with soft seams and vignette, soft dust blend, outlined bevelled rounded machines with own accents (miner yellow + spinning drill, furnace copper + orange glow/bloom only when working, generator red-orange + hazard stripes + pulse when burning, chest lid + latch, pole mast + glowing node, core 3 x 3 with hazard stripes and pulsing teal ring), charcoal belts with raised edges and moving chevrons (phase from the tick), outlined item chips (colours named in defense_view.cpp). LOD below 14 pt/tile skips details, outlines on belts and bloom.
- Find the core: arrow + "Core N tiles" at the edge of the free picture (not under Controls or the bar), hidden while any core tile is on screen; Home (H) button and key H (also 8).
- Handling: left drag with Belt lays a line (longer axis first, auto corners, ghost line green/red); other buildings place along the drag; right drag removes along the way; right click on ground or Esc drops the tool; WASD / arrows pan; 1-6 build bar, 7 turn, 8 home; Q picks the building under the pointer; the tool stays chosen. Pure functions: beltLine, canPlace, hotkeyFor, coreArrow, clampCamera, minZoomFor.
- Save version 2. A version-1 save: "saved with an older version. Start a new planet.", copy kept as .old, nothing loaded, no crash.
- About (native edit 37 reworded, count unchanged): invented red planet, map/ore/core numbers are design choices. Changelog 0.17 part 2 entry. OWNER-TODO: try handling on a real PC.

## Tests changed on purpose (features removed)
- defense_logic: "chunk 10,000 tiles out identical" -> "the map's corner chunk identical" + "outside the map only unbuildable wasteland" (endless map removed). "no water within 24 tiles" -> "every tile within 24 tiles buildable" (water removed). "all 5 terrains occur" -> "all 5 red-rock looks occur inside the map, no Waste inside" (biomes replaced). "ore far out holds 2x more" -> "ore covers > 1.4x more ground at 180-250 tiles than at 70-140" (richness removed, patches bigger). Merge test: item total + the core's 50 start coal (the rule "no item made or lost" unchanged).
- defense_pictures: save header "PhysicsLabDefense 2" (version bump); the hand-made damaged file now says version 2 so it still tests a checksum failure.
- Unchanged and passing: determinism, belt conservation / spacing / back-pressure / speed, save -> load hash, 7 damaged files.

## Tested (Linux, software GL)
- defense_logic 91 checks (was 62), all pass. New: core power alone runs a miner, coal reaches the generator, generator adds 900 kW, mined tiles still coal, 3 miners get 666 per mille (no deadlock from a fresh core-only world); click-feed moves exactly 7/10/10/10/10/3/0 and conserves coal; furnace in the map corner pollutes its chunks, nothing leaves the map; camera clamp (far out, inside, huge window centred); belt line shapes + the placed turn is a corner; placement (free, last tile, occupied, outside, band, half off, miner off ore, second core); hotkeys; core arrow (on core, partly visible, edge exactly on the screen edge, far diagonal, straight below); old save -> .old + message.
- Broken on purpose (then restored): no core power -> 4 checks fail; pollution to off-map chunks -> 1 fails; arrow visibility with <= -> the exact-edge check fails.
- defense_pictures (now with: arrow shown far from the core, not on it) passed. check.sh quick: 11/11 passed. UI tests touching About / changelog / the lab, run once: shell_ui, ui5g_pages, ui5k_pages, ui5k_log passed; shell_pictures: shell_about_dark/light_1280x800 changed on purpose (longer About text: a render with the old text differs only in a 12 x 5 px box at the scrollbar thumb), hashes updated, re-run passed; app_settings passed. Full suite NOT run (job rule).
- Pictures looked at (4): sample base at zoom 24, zoom 64 crop (core, belt, cracks), big base zoomed out (seams were too strong and dust blocky: fixed in the one correction pass), big base after the fix with the core arrow.

## Speed guard (counts; Linux, software GL; CPU noisy; NOT a speed test)
- Big test base (3,232 entities, 12,224 items on screen, 40 chunks, zoom 6): 44 draw calls (stage 1: 44; limit 88). Draw-list CPU median 0.85 ms (min 0.78, max 1.13; stage 1 0.34-0.38 ms): more vertices per belt and building. Simulation (unit test): 0.035 ms a tick for 3,201 entities.
- Sample base: 13 draw calls at zoom 24 and 64 (stage 1: 12), draw-list CPU median 0.11 ms.
- No per-frame allocation in steady state (belt line and item buffers reused, a fixed dust grid per chunk build, animation from tick/clock).

## Not tested / not done
- fps on a real GPU, Windows, a real mouse and keyboard (drag-belts, ghost, WASD, hotkeys, held feed click): in OWNER-TODO. The input wiring in main.cpp was not driven by a script; only the pure logic is tested.
- Not built (by design): aliens, turrets, splitters, junctions, building costs.
