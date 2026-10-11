// Defense Lab (0.17 stage 1) logic tests: the map from the seed, belts (item conservation, back-pressure, a corner), saving and loading.
// The numbers the game uses are design choices (src/defense/defense_world.h); what is checked here is that the rules hold: the same seed gives the same map,
// belts never make or lose an item and never squeeze items closer than kBeltGap, a save gives back the identical world and a damaged file is refused.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

#include "atom/atom_core.h"
#include "defense/defense_world.h"

using namespace defense;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}

static bool sameChunk(const Chunk& a, const Chunk& b) {
    return std::memcmp(a.terrain, b.terrain, sizeof a.terrain) == 0 && std::memcmp(a.ore, b.ore, sizeof a.ore) == 0 && std::memcmp(a.amount, b.amount, sizeof a.amount) == 0;
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
    // far away too (the map is endless): the same seed again, a chunk 10,000 tiles out
    World d; d.newGame(1234);
    check(sameChunk(a.chunk(312, -280), d.chunk(312, -280)), "same seed: a chunk 10,000 tiles out is identical");
    // every terrain kind exists somewhere, and the start area always has land and all four ores within a short walk (24 tiles) of the core
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
        check(land, "seed " + std::to_string(s * 7919u) + ": no water within 24 tiles of the core");
        check(ore[1] && ore[2] && ore[3] && ore[4], "seed " + std::to_string(s * 7919u) + ": iron, copper, coal and stone within 24 tiles");
    }
    int kinds[5] = {};
    for (int y = -600; y < 600; y += 6) for (int x = -600; x < 600; x += 6) kinds[static_cast<int>(a.terrainAt(x, y))]++;
    for (int k = 0; k < 5; k++) check(kinds[k] > 0, "terrain kind " + std::to_string(k) + " occurs");
    // ore patches are richer farther out
    uint64_t nearSum = 0, farSum = 0; int nearN = 0, farN = 0;
    for (int y = -2000; y < 2000; y += 3)
        for (int x = -2000; x < 2000; x += 3) {
            const double r = std::sqrt(double(x) * x + double(y) * y);
            if (a.oreAt(x, y) == Item::None) continue;
            if (r < 400) { nearSum += a.oreAmountAt(x, y); nearN++; } else if (r > 1500) { farSum += a.oreAmountAt(x, y); farN++; }
        }
    check(nearN > 0 && farN > 0 && farSum / farN > 2 * (nearSum / nearN), "ore tiles farther than 1,500 tiles hold more than twice as much as those within 400");
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
        if (u.countAllItems() != uint64_t(put2)) mergeKept = false;
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
    bigBaseCounts();
    std::printf("defense_tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
