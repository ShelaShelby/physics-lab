// Defense Lab (0.17 stage 1): the factory part of a top-down factory game on an endless planet. No graphics in here (the drawing is defense_view.cpp).
//
// IMPORTANT: every speed, cost, size, ore amount, power and pollution number in the Defense Lab is a GAME DESIGN CHOICE, picked so the game plays well.
// None of them is a measured or physical value, and the app says so (About, "Where the facts come from"). Only the element names and symbols
// (iron Fe, copper Cu, carbon C for coal) come from real data: from the element table the Atom Lab already ships (atom::name / atom::symbol).
//
// Stage 1 builds only the factory: core, miner, belt (straight and corner), furnace, chest, coal generator, power pole. Stage 2 adds splitters and
// junctions, stage 3 aliens, turrets and ammo; the hooks for them are the Kind list, the entity list and the pollution value of each chunk.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace defense {

// ---- the map ----
constexpr int kChunk = 32;                                   // design: a chunk is 32 x 32 tiles (generated, drawn and polluted as one piece)
enum class Terrain : uint8_t { Deep, Shallow, Sand, Grass, Rock };
enum class Item : uint8_t { None, IronOre, CopperOre, Coal, Stone, IronPlate, CopperPlate, Brick, Count };
constexpr int kItemCount = static_cast<int>(Item::Count);
enum class Kind : uint8_t { None, Core, Belt, Miner, Furnace, Chest, Generator, Pole, Count };   // stage 2 / 3 add their kinds at the end (the save keeps the numbers)
constexpr int kKindCount = static_cast<int>(Kind::Count);

// ---- game design choices (NOT physics, NOT measured; see the note at the top) ----
constexpr int kTicksPerSecond = 60;                          // the simulation's fixed step: 1/60 s
constexpr int kBeltLen = 256;                                // one belt tile, in belt units
constexpr int kBeltGap = 64;                                 // items keep at least this far apart: 4 items a tile
constexpr int kBeltCap = kBeltLen / kBeltGap;
constexpr int kBeltSpeed = 8;                                // belt units a tick: 1.875 tiles/s, so at most 7.5 items/s pass a point
constexpr int kMinerTicks = 120;                             // a miner at full power digs one ore every 2 s
constexpr int kMinerWatts = 90000;                           // a miner draws 90 kW while it digs
constexpr int kGeneratorWatts = 900000;                      // a coal generator gives up to 900 kW
constexpr int64_t kCoalJoules = 4000000;                     // one coal in a generator: 4 MJ (a design number for one game "piece", not the energy of real coal)
constexpr int kSmeltTicks = 192;                             // a furnace makes one plate or brick in 3.2 s
constexpr int kFurnaceBurnTicks = 1920;                      // one coal keeps a furnace burning 32 s (10 smelts)
constexpr int kStoneForBrick = 2;                            // 2 stone make 1 brick; 1 ore makes 1 plate
constexpr int kSlotMax = 10;                                 // a building's input, fuel and output slots hold 10 each
constexpr int kChestMax = 400;                               // a chest holds 400 items
constexpr double kPoleSupply = 2.5;                          // a pole powers buildings that reach within 2.5 tiles of its centre (a 5 x 5 area)
constexpr double kPoleReach = 7.5;                           // poles up to 7.5 tiles apart are wired together
constexpr double kPollMiner = 10, kPollFurnace = 2, kPollGenerator = 6;   // pollution a minute while working (generator: at full load); units are the game's own
constexpr double kPollSpread = 0.02, kPollAbsorb = 0.01;     // each second a chunk gives 2 % of its pollution to each of its 4 neighbours and the ground absorbs 1 %
constexpr int kStartRadius = 40;                             // the start area: always land within 40 tiles of the core
constexpr int kOreCell = 48;                                 // one possible ore patch in each 48 x 48 tile cell outside the start area

struct Chunk {
    int cx = 0, cy = 0;
    uint32_t version = 0;                                    // goes up when an ore tile runs out (the picture redraws the chunk)
    uint8_t terrain[kChunk * kChunk];
    uint8_t ore[kChunk * kChunk];                            // an Item (IronOre, CopperOre, Coal, Stone) or None
    uint32_t amount[kChunk * kChunk];                        // ore left on the tile
};

struct BeltItem { uint8_t item = 0; uint16_t pos = 0; };     // pos 0 = where the item comes in, kBeltLen = the far end

struct Entity {
    uint32_t id = 0;
    Kind kind = Kind::None;
    int32_t x = 0, y = 0;                                    // top left tile
    uint8_t dir = 0;                                         // 0 north (up), 1 east, 2 south, 3 west: where a belt carries to and where a building puts its output
    // belt: the items on it, front (largest pos) first
    uint8_t n = 0;
    BeltItem items[kBeltCap];
    // buildings
    uint8_t inItem = 0, outItem = 0;
    uint16_t inCount = 0, fuel = 0, outCount = 0;
    int32_t progress = 0;                                    // miner: work done (1000 a tick at full power); furnace: ticks of smelting done
    int32_t burn = 0;                                        // furnace: ticks of fire left from the coal it burns
    int64_t burnJ = 0;                                       // generator: energy left from the coal it burns, J
    uint64_t store[kItemCount] = {};                         // chest and core: what they hold
    // worked out from the layout (not saved)
    int net = -1;                                            // power network (poles: their own; consumers and generators: the one they are on)
    int target = -1;                                         // belt: the entity its front tile is part of (index), -1 none
    int8_t shape = 0;                                        // belt: 0 straight, -1 corner coming in from the left, +1 from the right (seen along dir)
};

struct PowerNet { int64_t demandJ = 0, supplyJ = 0; int satPermille = 1000; };   // per tick

int footprint(Kind k);                                       // 1, 2 or 3 (square)
const char* kindName(Kind k);                                // "Miner"
std::string itemName(Item i);                                // "Iron ore (Fe)": the element symbols from the Atom Lab's element table
std::string itemShort(Item i);                               // "Fe ore"
uint32_t seedFromText(const std::string& s);                 // a typed seed: a number is used as it is, other text is hashed

// value noise of our own (no library): smooth, deterministic, about -1..1
double valueNoise(uint32_t seed, double x, double y);
double fbm(uint32_t seed, double x, double y, int octaves);

class World {
public:
    void newGame(uint32_t seed);
    uint32_t seed() const { return seed_; }
    uint64_t tick() const { return tick_; }

    // the map (made the first time it is asked for, the same every time for the same seed)
    const Chunk& chunk(int cx, int cy);
    Terrain terrainAt(int x, int y);
    Item oreAt(int x, int y);
    uint32_t oreAmountAt(int x, int y);
    static bool buildable(Terrain t) { return t != Terrain::Deep && t != Terrain::Shallow; }

    // building
    bool canPlace(Kind k, int x, int y, std::string* why = nullptr);
    uint32_t place(Kind k, int x, int y, int dir, bool ignoreTerrain = false);   // 0 = not placed; ignoreTerrain: test bases only
    bool remove(int x, int y);                               // what the building held goes into the core (the core itself cannot be removed)
    int entityAt(int x, int y) const;                        // index into entities(), -1 none
    const std::vector<Entity>& entities() const { return ents_; }
    Entity* find(uint32_t id);
    const Entity& core() const { return ents_[0]; }
    bool addToChest(uint32_t id, Item it, int count);       // test bases and the sample: fill a chest
    bool addToBelt(uint32_t id, Item it, int pos);           // test helper: puts an item on a belt if the spacing allows it

    // the simulation: one fixed step of 1/60 s
    void step();
    void layout() { if (dirty_) relayout(); }                 // belt shapes, targets and power networks up to date (the picture calls it while paused too)
    const std::vector<PowerNet>& nets() const { return nets_; }
    int totalSatPermille() const;                            // all networks together (for the status line), 1000 if nothing needs power

    // pollution: one number a chunk (no effect yet: stage 3 uses it to draw the aliens)
    double pollution(int cx, int cy) const;
    const std::map<int64_t, double>& pollutionMap() const { return pol_; }

    // items: how many are where (belt items, building slots, chests, core)
    uint64_t countItems(Item it) const;
    uint64_t countAllItems() const;

    // the camera is part of the saved game
    double camX = 0, camY = 0, zoom = 24;                    // tile at the picture's centre; points a tile

    // saving: a versioned text; load() checks it and says what is wrong
    std::string serialize() const;
    bool load(const std::string& text, std::string* error);
    uint64_t stateHash() const;                              // FNV-1a of serialize()

    static int64_t key(int x, int y) { return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y)); }   // x in the high half, y in the low
    static int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

private:
    void generate(Chunk& c);
    void relayout();                                         // belts' targets and shapes, the order belts move in, power networks
    bool accept(Entity& e, Item it, bool commit);            // a building takes an item from a belt (or a miner, chest, furnace)
    bool pushOut(Entity& from, Item it);                     // to the tile in front of a building
    bool beltCanTake(const Entity& b) const;
    void stepBelt(Entity& b);
    void addPollution(int x, int y, double amount);
    void spreadPollution();
    int frontTile(const Entity& e, int& tx, int& ty) const;
    Chunk& chunkMut(int cx, int cy);

    uint32_t seed_ = 1;
    uint64_t tick_ = 0;
    uint32_t nextId_ = 1;
    std::vector<Entity> ents_;                               // ents_[0] is the core
    std::unordered_map<int64_t, uint32_t> occ_;              // tile -> entity id
    std::unordered_map<uint32_t, int> index_;                // id -> index
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks_;
    Chunk* lastChunk_ = nullptr;
    std::map<int64_t, uint32_t> mined_;                      // ore tiles that were dug: what is left (the rest comes from the seed)
    std::map<int64_t, double> pol_;                          // chunk -> pollution
    std::vector<std::pair<int64_t, double>> polScratch_;
    std::vector<int> beltOrder_;
    std::vector<PowerNet> nets_;
    bool dirty_ = true;
};

// A small working base next to the core (iron miner -> furnace -> core, coal -> generator, poles, a stone chest), for the picture test and a first look.
void buildSampleBase(World& w);
// A big test base for counting (64 belt lines of 48 tiles between chests full of plates), far from the core; returns the entities placed.
int buildBigBase(World& w);

// Writes text to path atomically: a temporary file first, then renamed over the old one. False if it could not be written.
bool writeFileAtomic(const std::string& path, const std::string& text);
bool readFile(const std::string& path, std::string& out);

}  // namespace defense
