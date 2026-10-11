// Defense Lab (0.17 stage 1, stage 2) logic tests: the map from the seed, belts (item conservation, back-pressure, a corner), saving and loading;
// stage 2: the fixed map and its border, infinite ore, the core's emergency power (no deadlock), feeding a generator by click, pollution kept on the map,
// the pure handling functions (belt drag line, placement, hotkeys, the core arrow, the camera clamp) and an older save.
// The numbers the game uses are design choices (src/defense/defense_world.h); what is checked here is that the rules hold: the same seed gives the same map,
// belts never make or lose an item and never squeeze items closer than kBeltGap, a save gives back the identical world and a damaged file is refused.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <chrono>

#include "atom/atom_core.h"
#include "defense/defense_lab.h"
#include "defense/defense_world.h"

using namespace defense;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}

static bool sameChunk(const Chunk& a, const Chunk& b) {
    return std::memcmp(a.terrain, b.terrain, sizeof a.terrain) == 0 && std::memcmp(a.ore, b.ore, sizeof a.ore) == 0;
}

// 1. The same seed gives the same map (chunk by chunk, generated in a different order too); a different seed gives a different one.
static void testSeedMap() {
    World a, b, c;
    a.newGame(1234); b.newGame(1234); c.newGame(1235);
    for (int cy = 3; cy >= -3; cy--) for (int cx = 3; cx >= -3; cx--) b.chunk(cx, cy);   // b makes its chunks in the opposite order
    int same = 0, diffSeed = 0, n = 0;
    for (int cy = -3; cy <= 3; cy++)
        for (int cx = -3; cx <= 3; cx++) {
            const Chunk& ca = a.chunk(cx, cy);
            const Chunk& cb = b.chunk(cx, cy);
            same += sameChunk(ca, cb);
            diffSeed += !sameChunk(ca, c.chunk(cx, cy));
            n++;
        }
    check(same == n, "same seed: every chunk identical (" + std::to_string(same) + " of " + std::to_string(n) + ")");
    check(diffSeed >= n / 2, "different seed: most chunks differ (" + std::to_string(diffSeed) + " of " + std::to_string(n) + ")");
    // stage 2 (changed on purpose: the map is fixed, 512 x 512): the same seed again at the map's far corner; outside the map only wasteland, no ore
    World d; d.newGame(1234);
    check(sameChunk(a.chunk(kChunkMax, kChunkMin), d.chunk(kChunkMax, kChunkMin)), "same seed: the map's corner chunk is identical");
    bool waste = true;
    for (int y = kMapMin - kBorder; y <= kMapMax + kBorder; y += 3)
        for (int x = kMapMin - kBorder; x <= kMapMax + kBorder; x++) {
            if (inMap(x, y)) continue;
            if (a.terrainAt(x, y) != Terrain::Waste || a.oreAt(x, y) != Item::None || World::buildable(a.terrainAt(x, y))) waste = false;
        }
    check(waste, "outside the map (the border band): only unbuildable wasteland, no ore");
    // the start area always has buildable ground and all four ores within a short walk (24 tiles) of the core
    for (uint32_t s = 1; s <= 12; s++) {
        World w; w.newGame(s * 7919u);
        bool ore[kItemCount] = {};
        bool land = true;
        for (int y = -24; y <= 24; y++)
            for (int x = -24; x <= 24; x++) {
                if (x * x + y * y > 24 * 24) continue;
                ore[static_cast<int>(w.oreAt(x, y))] = true;
                if (!World::buildable(w.terrainAt(x, y))) land = false;
            }
        check(land, "seed " + std::to_string(s * 7919u) + ": every tile within 24 tiles of the core is buildable");
        check(ore[1] && ore[2] && ore[3] && ore[4], "seed " + std::to_string(s * 7919u) + ": iron, copper, coal and stone within 24 tiles");
    }
    // stage 2 (changed on purpose: one red-rock biome, no water or sand): inside the map all 5 looks occur (rock, dust, pits, cracks, stones), all buildable
    int kinds[kTerrainCount] = {};
    for (int y = kMapMin; y <= kMapMax; y += 2) for (int x = kMapMin; x <= kMapMax; x += 2) kinds[static_cast<int>(a.terrainAt(x, y))]++;
    for (int k = 0; k < 5; k++) check(kinds[k] > 0 && World::buildable(static_cast<Terrain>(k)), "red rock look " + std::to_string(k) + " occurs and is buildable");
    check(kinds[static_cast<int>(Terrain::Waste)] == 0, "no wasteland inside the map");
    // stage 2 (changed on purpose: ore is infinite, no amounts): patches are bigger farther out, so ore covers more of the ground (4 seeds)
    int nearOre = 0, nearAll = 0, farOre = 0, farAll = 0;
    for (uint32_t sd = 1; sd <= 4; sd++) {
        World o; o.newGame(sd * 104729u);
        for (int y = kMapMin; y <= kMapMax; y += 2)
            for (int x = kMapMin; x <= kMapMax; x += 2) {
                const double r = std::sqrt(double(x) * x + double(y) * y);
                const bool ore = o.oreAt(x, y) != Item::None;
                if (r >= 70 && r < 140) { nearAll++; nearOre += ore; } else if (r >= 180 && r < 250) { farAll++; farOre += ore; }
            }
    }
    const double nearF = double(nearOre) / nearAll, farF = double(farOre) / farAll;
    check(farF > 1.4 * nearF, "bigger patches far out: ore covers " + std::to_string(farF) + " of the ground 180-250 tiles out, " + std::to_string(nearF) + " at 70-140");
    check(itemName(Item::IronOre) == std::string("Iron ore (") + atom::symbol(26) + ")" && itemName(Item::CopperPlate) == std::string("Copper plate (") + atom::symbol(29) + ")" &&
          itemName(Item::Coal) == std::string("Coal (") + atom::symbol(6) + ")", "item names use the Atom Lab's element symbols");
}

// 2. Belts: a line of 5 tiles east, then a corner and 3 tiles south, ending on nothing. Items are never made or lost, they queue up kBeltGap apart (back-pressure),
// and when a chest is put at the end they all arrive in it.
static void testBelts() {
    World w; w.newGame(42);
    const int x0 = 5000, y0 = 5000;
    std::vector<uint32_t> line;
    for (int i = 0; i < 5; i++) line.push_back(w.place(Kind::Belt, x0 + i, y0, 1, true));
    for (int i = 0; i < 3; i++) line.push_back(w.place(Kind::Belt, x0 + 5, y0 + i, 2, true));
    bool placed = true;
    for (uint32_t id : line) placed = placed && id != 0;
    check(placed, "belts placed");
    int put = 0;
    for (int i = 0; i < 5; i++) for (int p = 0; p < kBeltLen; p += kBeltGap) put += w.addToBelt(line[std::size_t(i)], Item::IronPlate, p);
    check(put == 20, "20 items put on the first five belts");
    w.layout();
    check(w.find(line[5])->shape != 0 && w.find(line[4])->shape == 0, "the belt where the line turns is a corner, the one before it straight");
    const auto onBelts = [&] { int n = 0; for (uint32_t id : line) n += w.find(id)->n; return n; };
    bool conserved = true, spaced = true;
    for (int t = 0; t < 1500; t++) {
        w.step();
        if (onBelts() != 20 || w.countItems(Item::IronPlate) != 20) conserved = false;
        std::vector<int> along;                                    // every item's distance along the whole line, front first
        for (int i = int(line.size()) - 1; i >= 0; i--) { const Entity* b = w.find(line[std::size_t(i)]); for (int k = 0; k < b->n; k++) along.push_back(i * kBeltLen + b->items[k].pos); }
        for (std::size_t k = 1; k < along.size(); k++) if (along[k - 1] - along[k] < kBeltGap) spaced = false;
    }
    check(conserved, "no item made or lost on the belts in 1,500 ticks");
    check(spaced, "items never closer than kBeltGap, across belt joins and the corner too");
    // back-pressure: the line ends on nothing, so the 20 items stand packed at its end: the last 5 tiles full (4 each), kBeltGap apart
    bool packed = true;
    for (int i = 3; i < 8; i++) { const Entity* b = w.find(line[std::size_t(i)]); if (b->n != kBeltCap) packed = false; for (int k = 0; k < b->n; k++) if (b->items[k].pos != kBeltLen - k * kBeltGap) packed = false; }
    for (int i = 0; i < 3; i++) if (w.find(line[std::size_t(i)])->n != 0) packed = false;
    check(packed, "back-pressure: the items queue at the blocked end, 4 a tile, kBeltGap apart, the first 3 tiles empty");
    // a chest at the end: they all go in, none lost
    const uint32_t chest = w.place(Kind::Chest, x0 + 5, y0 + 3, 2, true);
    for (int t = 0; t < 1500; t++) w.step();
    check(chest != 0 && w.find(chest)->store[static_cast<int>(Item::IronPlate)] == 20 && onBelts() == 0, "all 20 items arrive in the chest at the end");
    // a chest feeding a line that ends on nothing: the line fills (4 a tile) and the chest stops; chest + belts = what it started with
    World v; v.newGame(42);
    const uint32_t src = v.place(Kind::Chest, x0 - 1, y0, 1, true);
    std::vector<uint32_t> l2;
    for (int i = 0; i < 6; i++) l2.push_back(v.place(Kind::Belt, x0 + i, y0, 1, true));
    v.addToChest(src, Item::Coal, 100);
    for (int t = 0; t < 3000; t++) v.step();
    int n2 = 0; bool full = true;
    for (uint32_t id : l2) { n2 += v.find(id)->n; if (v.find(id)->n != kBeltCap) full = false; }
    check(full && v.find(src)->store[static_cast<int>(Item::Coal)] + uint64_t(n2) == 100, "a chest fills a blocked line to 4 a tile and keeps the rest (100 in all)");
    // a side line feeding into the middle of a line (two belts feed one): the gap and the count still hold where the two streams merge
    World u; u.newGame(42);
    std::vector<uint32_t> mainL, side;
    for (int i = 0; i < 6; i++) mainL.push_back(u.place(Kind::Belt, x0 + i, y0, 1, true));
    for (int i = 0; i < 3; i++) side.push_back(u.place(Kind::Belt, x0 + 3, y0 - 3 + i, 2, true));
    int put2 = 0;
    for (uint32_t id : mainL) for (int p = 0; p < kBeltLen; p += kBeltGap) put2 += u.addToBelt(id, Item::Coal, p);
    for (uint32_t id : side) for (int p = 0; p < kBeltLen; p += kBeltGap) put2 += u.addToBelt(id, Item::Stone, p);
    const uint32_t sink = u.place(Kind::Chest, x0 + 6, y0, 1, true);
    bool mergeSpaced = true, mergeKept = true;
    for (int t = 0; t < 2000; t++) {
        u.step();
        if (u.countAllItems() != uint64_t(put2) + kStartCoal) mergeKept = false;   // (stage 2: + the core's 50 start coal)
        std::vector<int> along;
        for (int i = 5; i >= 0; i--) { const Entity* b = u.find(mainL[std::size_t(i)]); for (int k = 0; k < b->n; k++) along.push_back(i * kBeltLen + b->items[k].pos); }
        for (std::size_t k = 1; k < along.size(); k++) if (along[k - 1] - along[k] < kBeltGap) mergeSpaced = false;
    }
    check(mergeKept && u.find(sink)->store[static_cast<int>(Item::Coal)] + u.find(sink)->store[static_cast<int>(Item::Stone)] == uint64_t(put2), "a merge: no item made or lost, all " + std::to_string(put2) + " reach the chest");
    check(mergeSpaced, "a merge: items never closer than kBeltGap where a side belt feeds in");
    // speed: one item on an empty line of 8 tiles reaches the chest at the end after 8 x 256 / 8 = 256 ticks (design speed, kBeltSpeed)
    World s; s.newGame(42);
    std::vector<uint32_t> l3;
    for (int i = 0; i < 8; i++) l3.push_back(s.place(Kind::Belt, x0 + i, y0, 1, true));
    const uint32_t end = s.place(Kind::Chest, x0 + 8, y0, 1, true);
    s.addToBelt(l3[0], Item::Brick, 0);
    int arrived = -1;
    for (int t = 1; t <= 400 && arrived < 0; t++) { s.step(); if (s.find(end)->store[static_cast<int>(Item::Brick)] == 1) arrived = t; }
    check(arrived == 8 * kBeltLen / kBeltSpeed, "one item crosses 8 tiles in " + std::to_string(8 * kBeltLen / kBeltSpeed) + " ticks (took " + std::to_string(arrived) + ")");
}

// 3. Save and load give the identical world (hash of the whole state), and it goes on identically; damaged files are refused without a crash.
static void testSaveLoad() {
    World a; a.newGame(20261011);
    buildSampleBase(a);
    a.camX = 3.25; a.camY = -1.5; a.zoom = 31;
    for (int t = 0; t < 2400; t++) a.step();
    check(a.core().store[static_cast<int>(Item::IronPlate)] > 0, "the sample base works: iron plates reached the core (" + std::to_string(a.core().store[static_cast<int>(Item::IronPlate)]) + ")");
    check(a.pollution(0, -1) > 0 || a.pollution(0, 0) > 0, "the sample base pollutes the chunks next to it");
    check(a.totalSatPermille() > 0, "the sample base has power");
    const std::string text = a.serialize();
    World b;
    std::string err;
    check(b.load(text, &err), "the save loads (" + err + ")");
    check(b.stateHash() == a.stateHash() && b.serialize() == text, "loaded world identical (state hash)");
    check(b.camX == 3.25 && b.camY == -1.5 && b.zoom == 31 && b.seed() == 20261011u && b.tick() == a.tick(), "camera, seed and time come back");
    for (int t = 0; t < 1200; t++) { a.step(); b.step(); }
    check(b.stateHash() == a.stateHash(), "the loaded world goes on exactly as the saved one");
    // the file: written atomically and read back
    const char* tmp = std::getenv("TMPDIR");
    if (!tmp) tmp = std::getenv("TEMP");                          // (Windows)
    const std::string path = std::string(tmp ? tmp : ".") + "/pl_defense_test.sav";
    std::string back;
    check(writeFileAtomic(path, text) && readFile(path, back) && back == text, "the save file is written (temp file + rename) and reads back the same");
    std::remove(path.c_str());
    // damaged files: refused with a message, the world untouched, no crash
    const uint64_t before = b.stateHash();
    std::vector<std::string> bad = {"", "garbage", text.substr(0, text.size() / 2), "PhysicsLabDefense 999\n" + text.substr(text.find('\n') + 1)};
    std::string flipped = text; flipped[flipped.size() / 3] = flipped[flipped.size() / 3] == '7' ? '8' : '7'; bad.push_back(flipped);
    std::string noEnd = text.substr(0, text.rfind("end"));
    bad.push_back(noEnd);
    std::string huge = text; huge.replace(huge.find("\ne "), 3, "\ne 99999999999999999999 "); bad.push_back(huge);
    for (std::size_t i = 0; i < bad.size(); i++) {
        std::string e2;
        const bool ok = b.load(bad[i], &e2);
        check(!ok && !e2.empty(), "damaged save " + std::to_string(i) + " refused with a message (" + e2 + ")");
    }
    check(b.stateHash() == before, "a refused file leaves the world as it was");
}

// 4. Stage 2: power without a character. A fresh world (only the core) + 3 poles + a miner on coal + 2 belts to a coal generator: the miner runs on the core's
// emergency power alone, coal reaches the generator, the generator then adds its power; the ore never runs out. This is the "no deadlock" proof.
static void testCorePower() {
    World w; w.newGame(777);
    check(w.entities().size() == 1 && w.core().store[static_cast<int>(Item::Coal)] == uint64_t(kStartCoal), "a new game: only the core, holding 50 coal");
    const uint32_t miner = w.place(Kind::Miner, -10, 9, 1);
    const uint32_t b1 = w.place(Kind::Belt, -8, 9, 1), b2 = w.place(Kind::Belt, -7, 9, 1);
    const uint32_t gen = w.place(Kind::Generator, -6, 8, 1);
    const uint32_t p1 = w.place(Kind::Pole, -1, 3, 0), p2 = w.place(Kind::Pole, -4, 7, 0), p3 = w.place(Kind::Pole, -8, 7, 0);
    check(miner && b1 && b2 && gen && p1 && p2 && p3, "the coal chain can be built on a fresh planet");
    w.layout();
    check(w.core().net >= 0 && w.find(miner)->net == w.core().net && w.find(gen)->net == w.core().net, "core, miner and generator are on one network (through the poles)");
    const int net = w.core().net;
    for (int t = 0; t < 30; t++) w.step();
    const int64_t coreJ = kCoreWatts / kTicksPerSecond, genJ = kGeneratorWatts / kTicksPerSecond;
    check(w.nets()[std::size_t(net)].supplyJ == coreJ && w.nets()[std::size_t(net)].satPermille == 1000 && w.find(gen)->fuel == 0,
          "before any coal arrives: the miner runs at full power on the core alone (180 kW for 90 kW)");
    for (int t = 0; t < 900; t++) w.step();
    const Entity* g = w.find(gen);
    check(g->fuel > 0, "coal reached the generator by belt (" + std::to_string(g->fuel) + " in it)");
    check(w.nets()[std::size_t(net)].supplyJ == coreJ + genJ, "the generator now adds its power (core + generator on the network)");
    check(w.oreAt(-10, 9) == Item::Coal && w.oreAt(-9, 10) == Item::Coal, "ore is infinite: the mined tiles are still coal");
    // three miners on the core alone: the core's 180 kW is shared, they dig at 2/3 speed (enough for about 2 miners, as designed)
    World v; v.newGame(777);
    int poles = 0, miners = 0;
    for (const auto& q : {std::pair<int, int>{-1, 3}, {4, -1}, {7, -5}, {10, -8}}) poles += v.place(Kind::Pole, q.first, q.second, 0) != 0;
    for (int k = 0; k < 3; k++) miners += v.place(Kind::Miner, 8 + 2 * k, -10, 3) != 0;
    v.step();
    check(poles == 4 && miners == 3 && v.totalSatPermille() == 666, "3 miners on the core's power alone get 2/3 of what they need (" + std::to_string(v.totalSatPermille()) + " per mille)");
}

// 5. Stage 2: a click on a coal generator moves coal from the core into it: exactly what fits, no item made or lost.
static void testFeed() {
    World w; w.newGame(9);
    std::vector<uint32_t> gens;
    for (int k = 0; k < 7; k++) gens.push_back(w.place(Kind::Generator, -30 + 3 * k, 30, 1));
    w.find(gens[0])->fuel = 3;
    const uint64_t coal0 = w.countItems(Item::Coal);
    const int moved[7] = {w.feedGenerator(-30, 30), w.feedGenerator(-27, 31), w.feedGenerator(-23, 30), w.feedGenerator(-21, 30), w.feedGenerator(-18, 31), w.feedGenerator(-15, 30), w.feedGenerator(-12, 30)};
    check(moved[0] == kSlotMax - 3 && moved[1] == kSlotMax && moved[4] == kSlotMax && moved[5] == 50 - 7 - 4 * kSlotMax && moved[6] == 0,
          "feeding: 7 fill the half-full one, 10 each next, the last 3 of the 50, then nothing (" + std::to_string(moved[5]) + ")");
    check(w.countItems(Item::Coal) == coal0 && w.core().store[static_cast<int>(Item::Coal)] == 0 && w.find(gens[0])->fuel == kSlotMax, "feeding conserves coal (core + generators the same)");
    check(w.feedGenerator(-30, 30) == 0 && w.feedGenerator(0, 0) == 0 && w.feedGenerator(-100, -100) == 0, "a full generator, the core, empty ground: nothing moves");
}

// 6. Stage 2: the fixed map. Pollution spreads only inside it; the camera never shows more than map + band.
static void testMapEdge() {
    World w; w.newGame(3);
    const uint32_t f = w.place(Kind::Furnace, kMapMax - 1, kMapMax - 1, 3, true);
    const uint32_t a = w.place(Kind::Chest, kMapMax - 2, kMapMax - 1, 1, true), b = w.place(Kind::Chest, kMapMax - 2, kMapMax, 1, true);
    w.addToChest(a, Item::IronOre, 60); w.addToChest(b, Item::Coal, 20);
    for (int t = 0; t < 60 * 120; t++) w.step();
    bool inside = true;
    for (const auto& kv : w.pollutionMap()) if (!chunkInMap(static_cast<int>(kv.first >> 32), static_cast<int>(static_cast<int32_t>(static_cast<uint32_t>(kv.first & 0xFFFFFFFF))))) inside = false;
    check(f && w.pollution(kChunkMax, kChunkMax) > 0 && w.pollution(kChunkMax - 1, kChunkMax) > 0, "a furnace in the map's corner pollutes its chunk and the next one");
    check(inside && w.pollution(kChunkMax + 1, kChunkMax) == 0 && w.pollution(kChunkMax, kChunkMax + 1) == 0, "pollution never leaves the map");
    // the camera: map + band = 528 tiles; 1280 x 800 points at the lowest zoom (6) shows 213 x 133 tiles
    double cx = 10000, cy = -10000, z = 1;
    clampCamera(cx, cy, z, 1280, 800);
    const double lo = kMapMin - kBorder, hi = kMapMax + 1 + kBorder;
    check(z == kZoomMin && std::fabs((cx + 640 / z) - hi) < 1e-9 && std::fabs((cy - 400 / z) - lo) < 1e-9, "camera far out: zoom kept to 6, the view's edge on the band's outer edge");
    cx = 3; cy = -4; z = 24; clampCamera(cx, cy, z, 1280, 800);
    check(cx == 3 && cy == -4 && z == 24, "camera inside: unchanged");
    cx = 50; cy = 50; z = 1; clampCamera(cx, cy, z, 6000, 3000);         // a window wider than map + band at zoom 6 (6000 / 6 = 1000 tiles)
    check(std::fabs(z - 6000.0 / (hi - lo)) < 1e-9 && std::fabs(cx - 0) < 1e-9 && cy - 1500 / z >= lo - 1e-9, "a huge window: the zoom-out is limited so map + band fill it (centred)");
}

// 7. Stage 2 handling, pure functions: the belt drag line, placement validity, the hotkeys, the core arrow.
static void testHandling() {
    std::vector<BeltStep> l;
    beltLine(0, 0, 4, 2, 0, l);                                       // 4 east, then a corner and 2 south
    bool shape = l.size() == 7;
    const int want[7][3] = {{0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {4, 0, 2}, {4, 1, 2}, {4, 2, 2}};
    for (int i = 0; shape && i < 7; i++) shape = l[std::size_t(i)].x == want[i][0] && l[std::size_t(i)].y == want[i][1] && l[std::size_t(i)].dir == want[i][2];
    beltLine(5, 5, 5, 2, 0, l);
    shape = shape && l.size() == 4 && l[0].dir == 0 && l[3].y == 2 && l[3].dir == 0;
    beltLine(1, 1, 1, 1, 3, l);
    shape = shape && l.size() == 1 && l[0].dir == 3;
    beltLine(0, 0, -1, 3, 0, l);                                      // the longer axis first: 3 south, then 1 west
    shape = shape && l.size() == 5 && l[0].dir == 2 && l[2].dir == 2 && l[3].dir == 3 && l[3].x == 0 && l[3].y == 3 && l[4].x == -1 && l[4].dir == 3;
    check(shape, "belt drag line: along the longer axis, a corner, then the other; each tile points to the next");
    // the line placed: the tile where it turns becomes a corner by itself
    World w; w.newGame(11);
    beltLine(20, 20, 24, 22, 0, l);
    for (const BeltStep& s : l) w.place(Kind::Belt, s.x, s.y, s.dir);
    w.layout();
    check(w.entities().size() == 8 && w.entities()[std::size_t(w.entityAt(24, 20))].shape != 0 && w.entities()[std::size_t(w.entityAt(23, 20))].shape == 0, "the dragged line's turn is a corner");
    // placement validity: free rock yes; occupied, outside the map, the band, a miner off ore: no
    std::string why;
    const bool okFree = w.canPlace(Kind::Chest, 30, 30), okEdge = w.canPlace(Kind::Chest, kMapMax, kMapMax);
    const bool noOcc = !w.canPlace(Kind::Chest, 0, 0, &why), noOut = !w.canPlace(Kind::Chest, kMapMax + 1, 0), noBand = !w.canPlace(Kind::Pole, kMapMin - 3, 0);
    const bool noEdge2 = !w.canPlace(Kind::Furnace, kMapMax, 0), noMiner = !w.canPlace(Kind::Miner, 30, 30), okMiner = w.canPlace(Kind::Miner, -10, 9), noCore = !w.canPlace(Kind::Core, 40, 40);
    check(okFree && okEdge && noOcc && noOut && noBand && noEdge2 && noMiner && okMiner && noCore, "placement: free tile and last map tile yes; occupied, outside, band, half off the map, miner off ore, a second core: no");
    // hotkeys: 1-6 the build bar, 7 turn, 8 home, others nothing
    bool keys = true;
    for (int d = 1; d <= 6; d++) keys = keys && hotkeyFor(d).action == HotkeyAction::Pick && hotkeyFor(d).kind == kBuildBar[d - 1];
    keys = keys && hotkeyFor(1).kind == Kind::Belt && hotkeyFor(6).kind == Kind::Pole && hotkeyFor(7).action == HotkeyAction::Turn && hotkeyFor(8).action == HotkeyAction::Home;
    keys = keys && hotkeyFor(0).action == HotkeyAction::None && hotkeyFor(9).action == HotkeyAction::None;
    check(keys, "hotkeys 1-6 pick Belt, Miner, Furnace, Chest, Generator, Pole; 7 turns; 8 home; 0 and 9 nothing");
    // the core arrow (core middle at tile 0.5, 0.5; a 1000 x 600 view at 20 points a tile; margin 40)
    const CoreArrow on = coreArrow(0.5, 0.5, 20, 1000, 600, 0.5, 0.5, 40);
    check(!on.show && on.dist == 0, "arrow: hidden when the camera is on the core");
    const CoreArrow part = coreArrow(-23.5 + 0.01, 0.5, 20, 1000, 600, 0.5, 0.5, 40);   // the core's left edge just inside the right screen edge
    check(!part.show, "arrow: hidden while any part of the core is on screen");
    const CoreArrow edge = coreArrow(-26, 0.5, 20, 1000, 600, 0.5, 0.5, 40);           // the core's left edge exactly on the right screen edge: nothing of it on screen
    check(edge.show && std::fabs(edge.angle) < 1e-12 && std::fabs(edge.x - 960) < 1e-9 && std::fabs(edge.y - 300) < 1e-9 && std::fabs(edge.dist - 26.5) < 1e-9,
          "arrow: the core's edge exactly on the right screen edge: shown, points right (0), at x = 960, mid-height, 26.5 tiles");
    const CoreArrow far = coreArrow(200.5, 200.5, 6, 1000, 600, 0.5, 0.5, 40);           // far away: the camera down-right of the core, the arrow points up-left
    check(far.show && std::fabs(far.angle + 3 * 3.14159265358979 / 4) < 1e-9 && std::fabs(far.y - 40) < 1e-9 && std::fabs(far.x - (500 - 260)) < 1e-9 && std::fabs(far.dist - 200 * std::sqrt(2.0)) < 1e-9,
          "arrow: far away diagonally: points up-left (-135 degrees), on the top edge, 283 tiles");
    const CoreArrow below = coreArrow(0.5, -100, 10, 1000, 600, 0.5, 0.5, 40);
    check(below.show && std::fabs(below.angle - 3.14159265358979 / 2) < 1e-9 && std::fabs(below.x - 500) < 1e-9 && std::fabs(below.y - 560) < 1e-9, "arrow: core straight below: points down, bottom edge");
}

// 8. Stage 2: an older (stage 1) save: a short message, a copy kept as .old, no crash, nothing loaded.
static void testOldSave() {
    World a; a.newGame(5);
    std::string text = a.serialize();
    text.replace(0, std::strlen("PhysicsLabDefense 2"), "PhysicsLabDefense 1");
    const char* tmp = std::getenv("TMPDIR");
    if (!tmp) tmp = std::getenv("TEMP");
    const std::string path = std::string(tmp ? tmp : ".") + "/pl_defense_old.sav";
    std::remove((path + ".old").c_str());
    writeFileAtomic(path, text);
    DefenseLab L;
    L.open(path, 1);
    std::string back;
    check(!L.loaded && L.oldSave && L.loadError.find("saved with an older version") != std::string::npos && readFile(path + ".old", back) && back == text,
          "an older save: not loaded, the message says so, the file is kept as .old (" + L.loadError + ")");
    L.startNew(8);
    check(L.loaded && !L.oldSave && L.world.entities().size() == 1, "then a new planet starts");
    std::remove(path.c_str()); std::remove((path + ".old").c_str());
}

// Counts for the REPORT (no pass / fail on time: the cloud's CPU is not the owner's PC)
static void bigBaseCounts() {
    World w; w.newGame(5);
    const int placed = buildBigBase(w);
    for (int t = 0; t < 600; t++) w.step();
    const auto t0 = std::chrono::steady_clock::now();
    for (int t = 0; t < 600; t++) w.step();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 600;
    std::printf("big test base: %d entities placed, %zu in all, %llu items, %.3f ms CPU a simulation tick (this machine)\n", placed, w.entities().size(),
                static_cast<unsigned long long>(w.countAllItems()), ms);
    check(placed > 3000, "the big test base has over 3,000 entities");
}

int main() {
    testSeedMap();
    testBelts();
    testSaveLoad();
    testCorePower();
    testFeed();
    testMapEdge();
    testHandling();
    testOldSave();
    bigBaseCounts();
    std::printf("defense_tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
