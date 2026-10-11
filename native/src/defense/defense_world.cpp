// Defense Lab (0.17 stage 1): the world. Every number in here is a GAME DESIGN CHOICE (see defense_world.h), not a measured value.
#include "defense/defense_world.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "atom/atom_core.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace defense {

// ---------------------------------------------------------------- names
int footprint(Kind k) { return k == Kind::Core ? 3 : k == Kind::Miner || k == Kind::Furnace || k == Kind::Generator ? 2 : 1; }
const char* kindName(Kind k) {
    switch (k) {
    case Kind::Core: return "Core"; case Kind::Belt: return "Belt"; case Kind::Miner: return "Miner"; case Kind::Furnace: return "Furnace";
    case Kind::Chest: return "Chest"; case Kind::Generator: return "Coal generator"; case Kind::Pole: return "Power pole"; default: return "";
    }
}
// The element symbols come from the Atom Lab's element table (atom::symbol: Fe = 26, Cu = 29, C = 6), not typed again here. Stone and brick are not elements: no symbol.
std::string itemName(Item i) {
    switch (i) {
    case Item::IronOre: return std::string("Iron ore (") + atom::symbol(26) + ")";
    case Item::CopperOre: return std::string("Copper ore (") + atom::symbol(29) + ")";
    case Item::Coal: return std::string("Coal (") + atom::symbol(6) + ")";
    case Item::Stone: return "Stone";
    case Item::IronPlate: return std::string("Iron plate (") + atom::symbol(26) + ")";
    case Item::CopperPlate: return std::string("Copper plate (") + atom::symbol(29) + ")";
    case Item::Brick: return "Brick";
    default: return "";
    }
}
std::string itemShort(Item i) {
    switch (i) {
    case Item::IronOre: return std::string(atom::symbol(26)) + " ore";
    case Item::CopperOre: return std::string(atom::symbol(29)) + " ore";
    case Item::Coal: return "Coal";
    case Item::Stone: return "Stone";
    case Item::IronPlate: return std::string(atom::symbol(26)) + " plate";
    case Item::CopperPlate: return std::string(atom::symbol(29)) + " plate";
    case Item::Brick: return "Brick";
    default: return "";
    }
}
uint32_t seedFromText(const std::string& s) {
    char* end = nullptr;
    errno = 0;
    const unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (!s.empty() && end && *end == 0 && errno == 0 && v <= 0xFFFFFFFFull && s[0] != '-') return static_cast<uint32_t>(v);
    uint32_t h = 2166136261u;                                    // FNV-1a of the text
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}

// ---------------------------------------------------------------- noise (our own: an integer hash and smooth value noise)
namespace {
uint32_t hash3(uint32_t seed, int32_t x, int32_t y) {
    uint32_t h = seed * 0x9E3779B1u ^ static_cast<uint32_t>(x) * 0x85EBCA77u ^ static_cast<uint32_t>(y) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
double u01(uint32_t h) { return h / 4294967296.0; }
}  // namespace

double valueNoise(uint32_t seed, double x, double y) {
    const double fx0 = std::floor(x), fy0 = std::floor(y);
    const int32_t x0 = static_cast<int32_t>(fx0), y0 = static_cast<int32_t>(fy0);
    const double tx = x - fx0, ty = y - fy0;
    const double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const double a = u01(hash3(seed, x0, y0)), b = u01(hash3(seed, x0 + 1, y0)), c = u01(hash3(seed, x0, y0 + 1)), d = u01(hash3(seed, x0 + 1, y0 + 1));
    const double top = a + (b - a) * sx, bot = c + (d - c) * sx;
    return (top + (bot - top) * sy) * 2 - 1;
}
double fbm(uint32_t seed, double x, double y, int octaves) {
    double sum = 0, amp = 1, norm = 0, f = 1;
    for (int i = 0; i < octaves; i++) {
        sum += amp * valueNoise(seed + 1013u * static_cast<uint32_t>(i), x * f, y * f);
        norm += amp; amp *= 0.5; f *= 2;
    }
    return sum / norm;
}

// ---------------------------------------------------------------- the map
namespace {
struct Patch { Item ore; double cx, cy, r, rich; };
// design: the start area's four patches, about 13 tiles from the core (always the same places, so a new player always finds them)
const Patch kStart[4] = {{Item::IronOre, 10, -9, 4.5, 600}, {Item::CopperOre, -11, -8, 4.5, 600}, {Item::Coal, -9, 10, 4.5, 600}, {Item::Stone, 11, 9, 4.5, 600}};
// The random patch of an ore cell, if it has one. Richer and bigger the farther from the start (design).
bool cellPatch(uint32_t seed, int ci, int cj, Patch& p) {
    const uint32_t h = hash3(seed ^ 0xA5A5A5A5u, ci, cj), h2 = hash3(seed ^ 0x5A5A5A5Au, ci, cj);
    if (u01(h) >= 0.55) return false;
    p.cx = ci * kOreCell + 8 + (h2 % 32u) + 0.5;
    p.cy = cj * kOreCell + 8 + ((h2 >> 8) % 32u) + 0.5;
    const double dist = std::sqrt(p.cx * p.cx + p.cy * p.cy);
    if (dist < kStartRadius + 24) return false;                  // the start area has its own four
    p.ore = static_cast<Item>(1 + (h >> 4) % 4u);
    p.r = 3.5 + std::min(10.0, dist / 150.0) + u01(h2 ^ 0x1234567u) * 2.5;
    p.rich = 350 + 1.2 * dist;
    return true;
}
}  // namespace

void World::generate(Chunk& c) {
    for (int j = 0; j < kChunk; j++)
        for (int i = 0; i < kChunk; i++) {
            const int x = c.cx * kChunk + i, y = c.cy * kChunk + j;
            double h = fbm(seed_, x / 110.0, y / 110.0, 5);
            const double d = std::sqrt(double(x) * x + double(y) * y);
            if (d <= kStartRadius) h = std::max(h, -0.05);       // the start area is always land
            else if (d < kStartRadius + 16) h = std::max(h, -0.05 - (d - kStartRadius) / 16 * 0.6);
            const Terrain t = h < -0.30 ? Terrain::Deep : h < -0.20 ? Terrain::Shallow : h < -0.13 ? Terrain::Sand : h < 0.30 ? Terrain::Grass : Terrain::Rock;
            const int k = j * kChunk + i;
            c.terrain[k] = static_cast<uint8_t>(t);
            c.ore[k] = 0; c.amount[k] = 0;
            if (!buildable(t)) continue;
            const double wob = 0.85 + 0.3 * (valueNoise(seed_ + 99u, x / 3.0, y / 3.0) + 1) / 2;
            const auto tryPatch = [&](const Patch& p) {
                const double dx = x + 0.5 - p.cx, dy = y + 0.5 - p.cy, r = std::sqrt(dx * dx + dy * dy), R = p.r * wob;
                if (r > R) return false;
                c.ore[k] = static_cast<uint8_t>(p.ore);
                c.amount[k] = static_cast<uint32_t>(std::lround(p.rich * (1.25 - 0.75 * r / R)));
                return true;
            };
            bool found = false;
            if (d < kStartRadius) for (const Patch& p : kStart) if (!found) found = tryPatch(p);
            const int ci = floorDiv(x, kOreCell), cj = floorDiv(y, kOreCell);
            for (int b = -1; b <= 1 && !found; b++)
                for (int a = -1; a <= 1 && !found; a++) { Patch p; if (cellPatch(seed_, ci + a, cj + b, p)) found = tryPatch(p); }
            if (found) {
                const auto m = mined_.find(key(x, y));
                if (m != mined_.end()) { c.amount[k] = m->second; if (m->second == 0) c.ore[k] = 0; }
            }
        }
}

Chunk& World::chunkMut(int cx, int cy) {
    if (lastChunk_ && lastChunk_->cx == cx && lastChunk_->cy == cy) return *lastChunk_;
    auto it = chunks_.find(key(cx, cy));
    if (it == chunks_.end()) {
        auto c = std::make_unique<Chunk>();
        c->cx = cx; c->cy = cy;
        generate(*c);
        it = chunks_.emplace(key(cx, cy), std::move(c)).first;
    }
    lastChunk_ = it->second.get();
    return *lastChunk_;
}
const Chunk& World::chunk(int cx, int cy) { return chunkMut(cx, cy); }
Terrain World::terrainAt(int x, int y) {
    const Chunk& c = chunkMut(floorDiv(x, kChunk), floorDiv(y, kChunk));
    return static_cast<Terrain>(c.terrain[(y - c.cy * kChunk) * kChunk + (x - c.cx * kChunk)]);
}
Item World::oreAt(int x, int y) {
    const Chunk& c = chunkMut(floorDiv(x, kChunk), floorDiv(y, kChunk));
    return static_cast<Item>(c.ore[(y - c.cy * kChunk) * kChunk + (x - c.cx * kChunk)]);
}
uint32_t World::oreAmountAt(int x, int y) {
    const Chunk& c = chunkMut(floorDiv(x, kChunk), floorDiv(y, kChunk));
    return c.amount[(y - c.cy * kChunk) * kChunk + (x - c.cx * kChunk)];
}

// ---------------------------------------------------------------- building
void World::newGame(uint32_t seed) {
    *this = World();
    seed_ = seed;
    Entity core;
    core.id = nextId_++; core.kind = Kind::Core; core.x = -1; core.y = -1;
    ents_.push_back(core);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) occ_[key(-1 + i, -1 + j)] = core.id;
    index_[core.id] = 0;
    dirty_ = true;
}

bool World::canPlace(Kind k, int x, int y, std::string* why) {
    const auto no = [&](const char* w) { if (why) *why = w; return false; };
    if (k == Kind::None || k == Kind::Core || k >= Kind::Count) return no("This cannot be built.");
    const int s = footprint(k);
    bool ore = false;
    for (int j = 0; j < s; j++)
        for (int i = 0; i < s; i++) {
            if (occ_.count(key(x + i, y + j))) return no("Something is already there.");
            if (!buildable(terrainAt(x + i, y + j))) return no("Nothing can be built on water.");
            if (oreAt(x + i, y + j) != Item::None) ore = true;
        }
    if (k == Kind::Miner && !ore) return no("A miner must stand on ore.");
    return true;
}

uint32_t World::place(Kind k, int x, int y, int dir, bool ignoreTerrain) {
    if (ignoreTerrain) {
        if (k == Kind::None || k == Kind::Core || k >= Kind::Count) return 0;
        for (int j = 0; j < footprint(k); j++) for (int i = 0; i < footprint(k); i++) if (occ_.count(key(x + i, y + j))) return 0;
    } else if (!canPlace(k, x, y)) return 0;
    Entity e;
    e.id = nextId_++; e.kind = k; e.x = x; e.y = y; e.dir = static_cast<uint8_t>(dir & 3);
    for (int j = 0; j < footprint(k); j++) for (int i = 0; i < footprint(k); i++) occ_[key(x + i, y + j)] = e.id;
    index_[e.id] = static_cast<int>(ents_.size());
    ents_.push_back(e);
    dirty_ = true;
    return e.id;
}

bool World::remove(int x, int y) {
    const int idx = entityAt(x, y);
    if (idx <= 0) return false;                                   // nothing there, or the core (index 0): the core stays
    Entity& e = ents_[std::size_t(idx)];
    uint64_t* inv = ents_[0].store;                               // what it held goes to the core: no item is made or lost
    for (int k = 0; k < e.n; k++) inv[e.items[k].item]++;
    if (e.inCount) inv[e.inItem] += e.inCount;
    if (e.outCount) inv[e.outItem] += e.outCount;
    if (e.fuel) inv[static_cast<int>(Item::Coal)] += e.fuel;
    for (int i = 0; i < kItemCount; i++) inv[i] += e.store[i];
    for (int j = 0; j < footprint(e.kind); j++) for (int i = 0; i < footprint(e.kind); i++) occ_.erase(key(e.x + i, e.y + j));
    ents_.erase(ents_.begin() + idx);
    index_.clear();
    for (int i = 0; i < int(ents_.size()); i++) index_[ents_[std::size_t(i)].id] = i;
    dirty_ = true;
    return true;
}

int World::entityAt(int x, int y) const {
    const auto it = occ_.find(key(x, y));
    if (it == occ_.end()) return -1;
    const auto ix = index_.find(it->second);
    return ix == index_.end() ? -1 : ix->second;
}
Entity* World::find(uint32_t id) { const auto ix = index_.find(id); return ix == index_.end() ? nullptr : &ents_[std::size_t(ix->second)]; }

bool World::addToChest(uint32_t id, Item it, int count) {
    Entity* e = find(id);
    if (!e || e->kind != Kind::Chest || it == Item::None || it >= Item::Count || count < 0) return false;
    e->store[static_cast<int>(it)] += static_cast<uint64_t>(count);
    return true;
}
bool World::addToBelt(uint32_t id, Item it, int pos) {
    Entity* b = find(id);
    if (!b || b->kind != Kind::Belt || b->n >= kBeltCap || pos < 0 || pos > kBeltLen || it == Item::None || it >= Item::Count) return false;
    int at = 0;
    while (at < b->n && b->items[at].pos > pos) at++;
    if (at > 0 && b->items[at - 1].pos - pos < kBeltGap) return false;
    if (at < b->n && pos - b->items[at].pos < kBeltGap) return false;
    for (int k = b->n; k > at; k--) b->items[k] = b->items[k - 1];
    b->items[at] = {static_cast<uint8_t>(it), static_cast<uint16_t>(pos)};
    b->n++;
    return true;
}

int World::frontTile(const Entity& e, int& tx, int& ty) const {
    const int s = footprint(e.kind), a = s == 3 ? 1 : 0;
    switch (e.dir & 3) {
    case 0: tx = e.x + a; ty = e.y - 1; break;
    case 1: tx = e.x + s; ty = e.y + a; break;
    case 2: tx = e.x + s - 1 - a; ty = e.y + s; break;
    default: tx = e.x - 1; ty = e.y + s - 1 - a; break;
    }
    return entityAt(tx, ty);
}

// ---------------------------------------------------------------- the layout: belts' targets, shapes and order; power networks
void World::relayout() {
    index_.clear();
    for (int i = 0; i < int(ents_.size()); i++) index_[ents_[std::size_t(i)].id] = i;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    for (Entity& e : ents_) { e.net = -1; e.target = -1; e.shape = 0; }
    for (Entity& e : ents_) if (e.kind == Kind::Belt) { int tx, ty; e.target = frontTile(e, tx, ty); }
    for (int bi = 0; bi < int(ents_.size()); bi++) {                // a corner: nothing feeds it from behind and exactly one belt from one side
        Entity& b = ents_[std::size_t(bi)];
        if (b.kind != Kind::Belt) continue;
        const int d = b.dir & 3;
        const auto feeds = [&](int x, int y) { const int i = entityAt(x, y); return i >= 0 && ents_[std::size_t(i)].kind == Kind::Belt && ents_[std::size_t(i)].target == bi; };
        const bool back = feeds(b.x - DX[d], b.y - DY[d]);
        const bool left = feeds(b.x + DY[d], b.y - DX[d]), right = feeds(b.x - DY[d], b.y + DX[d]);
        if (!back && left != right) b.shape = left ? -1 : 1;
    }
    // the order belts move in: the one in front first, so a belt always sees where its target's last item already is (no gap is ever squeezed)
    beltOrder_.clear();
    std::vector<char> seen(ents_.size(), 0);
    std::vector<int> path;
    for (int i = 0; i < int(ents_.size()); i++) {
        if (ents_[std::size_t(i)].kind != Kind::Belt || seen[std::size_t(i)]) continue;
        path.clear();
        int c = i;
        while (c >= 0 && ents_[std::size_t(c)].kind == Kind::Belt && !seen[std::size_t(c)]) { seen[std::size_t(c)] = 1; path.push_back(c); c = ents_[std::size_t(c)].target; }
        for (auto it = path.rbegin(); it != path.rend(); ++it) beltOrder_.push_back(*it);
    }
    // power: poles wired within kPoleReach form a network; a building is on the network of the first pole whose 5 x 5 area it reaches
    std::vector<int> poles;
    for (int i = 0; i < int(ents_.size()); i++) if (ents_[std::size_t(i)].kind == Kind::Pole) poles.push_back(i);
    std::vector<int> parent(poles.size());
    for (std::size_t i = 0; i < poles.size(); i++) parent[i] = int(i);
    const auto root = [&](int a) { while (parent[std::size_t(a)] != a) { parent[std::size_t(a)] = parent[std::size_t(parent[std::size_t(a)])]; a = parent[std::size_t(a)]; } return a; };
    for (std::size_t a = 0; a < poles.size(); a++)
        for (std::size_t b = a + 1; b < poles.size(); b++) {
            const Entity &p = ents_[std::size_t(poles[a])], &q = ents_[std::size_t(poles[b])];
            const double dx = p.x - q.x, dy = p.y - q.y;
            if (dx * dx + dy * dy <= kPoleReach * kPoleReach) parent[std::size_t(root(int(a)))] = root(int(b));
        }
    std::vector<int> netOf(poles.size(), -1);
    int nn = 0;
    for (std::size_t a = 0; a < poles.size(); a++) { const int r = root(int(a)); if (netOf[std::size_t(r)] < 0) netOf[std::size_t(r)] = nn++; ents_[std::size_t(poles[a])].net = netOf[std::size_t(r)]; }
    nets_.assign(std::size_t(nn), PowerNet{});
    const int reach = static_cast<int>(std::floor(kPoleSupply));
    for (Entity& e : ents_) {
        if (e.kind != Kind::Miner && e.kind != Kind::Generator) continue;
        const int s = footprint(e.kind);
        for (int pi : poles) {
            const Entity& p = ents_[std::size_t(pi)];
            if (e.x <= p.x + reach && e.x + s - 1 >= p.x - reach && e.y <= p.y + reach && e.y + s - 1 >= p.y - reach) { e.net = p.net; break; }
        }
    }
    dirty_ = false;
}

// ---------------------------------------------------------------- the simulation
namespace {
bool smeltable(Item i) { return i == Item::IronOre || i == Item::CopperOre || i == Item::Stone; }
Item product(Item i) { return i == Item::IronOre ? Item::IronPlate : i == Item::CopperOre ? Item::CopperPlate : i == Item::Stone ? Item::Brick : Item::None; }
int need(Item i) { return i == Item::Stone ? kStoneForBrick : 1; }
}  // namespace

bool World::beltCanTake(const Entity& b) const { return b.n < kBeltCap && (b.n == 0 || b.items[b.n - 1].pos >= kBeltGap); }

bool World::accept(Entity& e, Item it, bool commit) {
    const int ii = static_cast<int>(it);
    switch (e.kind) {
    case Kind::Core: if (commit) e.store[ii]++; return true;
    case Kind::Chest: {
        uint64_t total = 0;
        for (int i = 0; i < kItemCount; i++) total += e.store[i];
        if (total >= kChestMax) return false;
        if (commit) e.store[ii]++;
        return true;
    }
    case Kind::Furnace:
        if (it == Item::Coal) { if (e.fuel >= kSlotMax) return false; if (commit) e.fuel++; return true; }
        if (!smeltable(it) || (e.inItem && e.inItem != ii) || e.inCount >= kSlotMax) return false;
        if (commit) { e.inItem = static_cast<uint8_t>(ii); e.inCount++; }
        return true;
    case Kind::Generator:
        if (it != Item::Coal || e.fuel >= kSlotMax) return false;
        if (commit) e.fuel++;
        return true;
    default: return false;
    }
}

bool World::pushOut(Entity& from, Item it) {
    int tx, ty;
    const int ti = frontTile(from, tx, ty);
    if (ti < 0) return false;
    Entity& t = ents_[std::size_t(ti)];
    if (&t == &from) return false;
    if (t.kind == Kind::Belt) {
        if (!beltCanTake(t)) return false;
        t.items[t.n++] = {static_cast<uint8_t>(it), 0};
        return true;
    }
    return accept(t, it, true);
}

void World::stepBelt(Entity& b) {
    if (b.n == 0) return;
    Entity* t = b.target >= 0 ? &ents_[std::size_t(b.target)] : nullptr;
    const bool tBelt = t && t->kind == Kind::Belt && t != &b;
    int limit = kBeltLen;                                          // how far the front item may go, in this belt's units (past kBeltLen = onto the next belt)
    if (tBelt) {
        limit = t->n == 0 ? INT_MAX / 2 : kBeltLen - kBeltGap + t->items[t->n - 1].pos;
        if (t->n >= kBeltCap) limit = std::min(limit, kBeltLen);
    }
    BeltItem keep[kBeltCap];
    int kept = 0, prev = 0;
    for (int k = 0; k < b.n; k++) {
        int p = b.items[k].pos + kBeltSpeed;
        p = std::min(p, k == 0 ? limit : prev - kBeltGap);
        if (k == 0 && p >= kBeltLen) {
            if (tBelt && t->n < kBeltCap) { t->items[t->n++] = {b.items[k].item, static_cast<uint16_t>(p - kBeltLen)}; prev = p; continue; }
            p = kBeltLen;
            if (t && !tBelt && accept(*t, static_cast<Item>(b.items[k].item), true)) { prev = p; continue; }
        }
        p = std::max(p, static_cast<int>(b.items[k].pos));          // never backwards
        keep[kept++] = {b.items[k].item, static_cast<uint16_t>(p)};
        prev = p;
    }
    for (int k = 0; k < kept; k++) b.items[k] = keep[k];
    b.n = static_cast<uint8_t>(kept);
}

void World::addPollution(int x, int y, double amount) { pol_[key(floorDiv(x, kChunk), floorDiv(y, kChunk))] += amount; }
double World::pollution(int cx, int cy) const { const auto it = pol_.find(key(cx, cy)); return it == pol_.end() ? 0.0 : it->second; }

void World::spreadPollution() {
    polScratch_.clear();
    for (const auto& kv : pol_) polScratch_.push_back(kv);
    for (const auto& [k, v] : polScratch_) {
        const int cx = static_cast<int>(k >> 32), cy = static_cast<int>(static_cast<int32_t>(static_cast<uint32_t>(k & 0xFFFFFFFF)));
        pol_[k] -= v * (4 * kPollSpread + kPollAbsorb);
        const double out = v * kPollSpread;
        pol_[key(cx + 1, cy)] += out; pol_[key(cx - 1, cy)] += out; pol_[key(cx, cy + 1)] += out; pol_[key(cx, cy - 1)] += out;
    }
    for (auto it = pol_.begin(); it != pol_.end();) it = it->second < 1e-4 ? pol_.erase(it) : std::next(it);
}

void World::step() {
    if (dirty_) relayout();
    tick_++;
    const int64_t minerJ = kMinerWatts / kTicksPerSecond, genJ = kGeneratorWatts / kTicksPerSecond;
    for (PowerNet& n : nets_) n = PowerNet{};
    // 1. power: what the miners that can dig want, and what the generators with coal can give
    const auto minerCanDig = [&](const Entity& m) {
        if (m.outCount) return false;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) if (oreAmountAt(m.x + i, m.y + j) > 0 && oreAt(m.x + i, m.y + j) != Item::None) return true;
        return false;
    };
    int gensIn[64] = {};
    for (const Entity& e : ents_) {
        if (e.net < 0) continue;
        if (e.kind == Kind::Miner && minerCanDig(e)) nets_[std::size_t(e.net)].demandJ += minerJ;
        if (e.kind == Kind::Generator && (e.burnJ > 0 || e.fuel > 0)) { nets_[std::size_t(e.net)].supplyJ += genJ; if (e.net < 64) gensIn[e.net]++; }
    }
    for (PowerNet& n : nets_) n.satPermille = n.demandJ == 0 ? 1000 : static_cast<int>(std::min<int64_t>(1000, n.supplyJ * 1000 / n.demandJ));
    // 2. generators burn coal for what was used, shared evenly
    for (Entity& e : ents_) {
        if (e.kind != Kind::Generator || e.net < 0 || !(e.burnJ > 0 || e.fuel > 0)) continue;
        const PowerNet& n = nets_[std::size_t(e.net)];
        const int gens = e.net < 64 ? std::max(1, gensIn[e.net]) : static_cast<int>(std::max<int64_t>(1, n.supplyJ / genJ));
        const int64_t share = n.demandJ * n.satPermille / 1000 / gens;
        while (e.burnJ < share && e.fuel > 0) { e.fuel--; e.burnJ += kCoalJoules; }
        e.burnJ -= std::min(share, e.burnJ);
        if (share > 0) addPollution(e.x, e.y, kPollGenerator / 3600.0 * double(share) / double(genJ));
    }
    // 3. buildings
    for (Entity& e : ents_) {
        switch (e.kind) {
        case Kind::Miner: {
            if (e.net >= 0 && minerCanDig(e)) {
                const int sat = nets_[std::size_t(e.net)].satPermille;
                e.progress += sat;
                if (sat > 0) addPollution(e.x, e.y, kPollMiner / 3600.0 * sat / 1000.0);
                if (e.progress >= kMinerTicks * 1000) {
                    for (int j = 0; j < 2 && e.progress >= kMinerTicks * 1000; j++)
                        for (int i = 0; i < 2 && e.progress >= kMinerTicks * 1000; i++) {
                            const int x = e.x + i, y = e.y + j;
                            Chunk& c = chunkMut(floorDiv(x, kChunk), floorDiv(y, kChunk));
                            const int k = (y - c.cy * kChunk) * kChunk + (x - c.cx * kChunk);
                            if (!c.ore[k] || !c.amount[k]) continue;
                            e.outItem = c.ore[k]; e.outCount = 1;
                            c.amount[k]--;
                            mined_[key(x, y)] = c.amount[k];
                            if (c.amount[k] == 0) { c.ore[k] = 0; c.version++; }
                            e.progress -= kMinerTicks * 1000;
                        }
                }
            }
            if (e.outCount && pushOut(e, static_cast<Item>(e.outItem))) { e.outCount = 0; e.outItem = 0; }
            break;
        }
        case Kind::Furnace: {
            const Item in = static_cast<Item>(e.inItem);
            const bool canSmelt = e.inCount >= need(in) && in != Item::None && (e.outCount == 0 || (e.outItem == static_cast<int>(product(in)) && e.outCount < kSlotMax));
            if (canSmelt && e.burn == 0 && e.fuel > 0) { e.fuel--; e.burn = kFurnaceBurnTicks; }
            if (canSmelt && e.burn > 0) {
                e.burn--; e.progress++;
                addPollution(e.x, e.y, kPollFurnace / 3600.0);
                if (e.progress >= kSmeltTicks) {
                    e.inCount = static_cast<uint16_t>(e.inCount - need(in));
                    if (e.inCount == 0) e.inItem = 0;
                    e.outItem = static_cast<uint8_t>(product(in)); e.outCount++;
                    e.progress = 0;
                }
            }
            if (e.outCount && pushOut(e, static_cast<Item>(e.outItem))) { e.outCount--; if (!e.outCount) e.outItem = 0; }
            break;
        }
        case Kind::Chest:
            for (int i = 1; i < kItemCount; i++)
                if (e.store[i]) { if (pushOut(e, static_cast<Item>(i))) e.store[i]--; break; }
            break;
        default: break;
        }
    }
    // 4. belts, front first
    for (int i : beltOrder_) stepBelt(ents_[std::size_t(i)]);
    // 5. pollution spreads once a second
    if (tick_ % kTicksPerSecond == 0) spreadPollution();
}

int World::totalSatPermille() const {
    int64_t d = 0, got = 0;
    for (const PowerNet& n : nets_) { d += n.demandJ; got += n.demandJ * n.satPermille / 1000; }
    return d == 0 ? 1000 : static_cast<int>(got * 1000 / d);
}

uint64_t World::countItems(Item it) const {
    const int ii = static_cast<int>(it);
    uint64_t n = 0;
    for (const Entity& e : ents_) {
        for (int k = 0; k < e.n; k++) n += e.items[k].item == ii;
        if (e.inItem == ii) n += e.inCount;
        if (e.outItem == ii) n += e.outCount;
        if (it == Item::Coal) n += e.fuel;
        n += e.store[ii];
    }
    return n;
}
uint64_t World::countAllItems() const { uint64_t n = 0; for (int i = 1; i < kItemCount; i++) n += countItems(static_cast<Item>(i)); return n; }

// ---------------------------------------------------------------- saving
namespace {
uint64_t fnv(const char* s, std::size_t n) { uint64_t h = 1469598103934665603ull; for (std::size_t i = 0; i < n; i++) { h ^= static_cast<unsigned char>(s[i]); h *= 1099511628211ull; } return h; }
const char* const kMagic = "PhysicsLabDefense";
constexpr int kVersion = 1;
}  // namespace

std::string World::serialize() const {
    std::string s;
    char b[256];
    std::snprintf(b, sizeof b, "%s %d\nseed %u\ntick %llu\ncamera %.17g %.17g %.17g\nnextid %u\n", kMagic, kVersion, seed_, static_cast<unsigned long long>(tick_), camX, camY, zoom, nextId_);
    s += b;
    for (const Entity& e : ents_) {
        std::snprintf(b, sizeof b, "e %u %d %d %d %d %d %d %d %d %d %d %d %lld", e.id, static_cast<int>(e.kind), e.x, e.y, e.dir, e.inItem, e.inCount, e.fuel, e.outItem, e.outCount,
                      e.progress, e.burn, static_cast<long long>(e.burnJ));
        s += b;
        for (int i = 0; i < kItemCount; i++) { std::snprintf(b, sizeof b, " %llu", static_cast<unsigned long long>(e.store[i])); s += b; }
        std::snprintf(b, sizeof b, " %d", e.n); s += b;
        for (int k = 0; k < e.n; k++) { std::snprintf(b, sizeof b, " %d %d", e.items[k].item, e.items[k].pos); s += b; }
        s += "\n";
    }
    for (const auto& [k, v] : mined_) { std::snprintf(b, sizeof b, "m %d %d %u\n", static_cast<int>(k >> 32), static_cast<int>(static_cast<int32_t>(static_cast<uint32_t>(k & 0xFFFFFFFF))), v); s += b; }
    for (const auto& [k, v] : pol_) { std::snprintf(b, sizeof b, "p %d %d %.17g\n", static_cast<int>(k >> 32), static_cast<int>(static_cast<int32_t>(static_cast<uint32_t>(k & 0xFFFFFFFF))), v); s += b; }
    std::snprintf(b, sizeof b, "end %016llx\n", static_cast<unsigned long long>(fnv(s.data(), s.size())));
    s += b;
    return s;
}

uint64_t World::stateHash() const { const std::string s = serialize(); return fnv(s.data(), s.size()); }

namespace {
// a strict reader of whitespace-separated whole numbers and finite decimals on one line
struct Line {
    const char* p;
    bool ok = true;
    explicit Line(const char* s) : p(s) {}
    long long i(long long lo, long long hi) {
        if (!ok) return 0;
        while (*p == ' ') p++;
        char* end = nullptr;
        errno = 0;
        const long long v = std::strtoll(p, &end, 10);
        if (end == p || errno == ERANGE || v < lo || v > hi || (*end != ' ' && *end != 0)) { ok = false; return 0; }
        p = end;
        return v;
    }
    unsigned long long u(unsigned long long hi) {
        if (!ok) return 0;
        while (*p == ' ') p++;
        if (*p == '-') { ok = false; return 0; }
        char* end = nullptr;
        errno = 0;
        const unsigned long long v = std::strtoull(p, &end, 10);
        if (end == p || errno == ERANGE || v > hi || (*end != ' ' && *end != 0)) { ok = false; return 0; }
        p = end;
        return v;
    }
    double d() {
        if (!ok) return 0;
        while (*p == ' ') p++;
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p || !std::isfinite(v) || (*end != ' ' && *end != 0)) { ok = false; return 0; }
        p = end;
        return v;
    }
    bool done() { while (*p == ' ') p++; return ok && *p == 0; }
};
}  // namespace

bool World::load(const std::string& text, std::string* error) {
    const auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    if (text.empty()) return fail("the file is empty");
    // the last line holds a checksum of everything before it: a cut or changed file is found here
    const std::size_t endAt = text.rfind("end ");
    if (endAt == std::string::npos || (endAt > 0 && text[endAt - 1] != '\n')) return fail("the file is cut short (no end line)");
    {
        std::string hex = text.substr(endAt + 4);
        while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r')) hex.pop_back();
        char* e = nullptr;
        errno = 0;
        const unsigned long long sum = std::strtoull(hex.c_str(), &e, 16);
        if (hex.size() != 16 || !e || *e != 0 || errno == ERANGE) return fail("the end line is damaged");
        if (sum != fnv(text.data(), endAt)) return fail("the checksum does not match (the file was changed or damaged)");
    }
    World w;
    std::size_t pos = 0;
    int lineNo = 0;
    bool haveSeed = false, haveTick = false, haveCam = false, haveNext = false;
    while (pos < endAt) {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string::npos || nl > endAt) nl = endAt;
        const std::string ln = text.substr(pos, nl - pos);
        pos = nl + 1;
        lineNo++;
        const std::string where = " (line " + std::to_string(lineNo) + ")";
        if (lineNo == 1) {
            const std::string head = std::string(kMagic) + " ";
            if (ln.compare(0, head.size(), head) != 0) return fail("this is not a Defense Lab save");
            Line l(ln.c_str() + head.size());
            const long long v = l.i(0, 1000000);
            if (!l.done()) return fail("the version line is damaged");
            if (v != kVersion) return fail("the save is version " + std::to_string(v) + "; this app reads version " + std::to_string(kVersion));
            continue;
        }
        const std::size_t sp = ln.find(' ');
        const std::string tag = ln.substr(0, sp);
        Line l(sp == std::string::npos ? "" : ln.c_str() + sp + 1);
        if (tag == "seed") { w.seed_ = static_cast<uint32_t>(l.u(0xFFFFFFFFull)); haveSeed = true; }
        else if (tag == "tick") { w.tick_ = l.u(ULLONG_MAX); haveTick = true; }
        else if (tag == "camera") { w.camX = l.d(); w.camY = l.d(); w.zoom = l.d(); haveCam = true; if (std::fabs(w.camX) > 1e9 || std::fabs(w.camY) > 1e9 || w.zoom <= 0) return fail("the camera is damaged" + where); }
        else if (tag == "nextid") { w.nextId_ = static_cast<uint32_t>(l.u(0xFFFFFFFFull)); haveNext = true; }
        else if (tag == "e") {
            Entity e;
            e.id = static_cast<uint32_t>(l.u(0xFFFFFFFFull));
            e.kind = static_cast<Kind>(l.i(1, kKindCount - 1));
            e.x = static_cast<int32_t>(l.i(-1000000000, 1000000000)); e.y = static_cast<int32_t>(l.i(-1000000000, 1000000000));
            e.dir = static_cast<uint8_t>(l.i(0, 3));
            e.inItem = static_cast<uint8_t>(l.i(0, kItemCount - 1)); e.inCount = static_cast<uint16_t>(l.i(0, 65535)); e.fuel = static_cast<uint16_t>(l.i(0, 65535));
            e.outItem = static_cast<uint8_t>(l.i(0, kItemCount - 1)); e.outCount = static_cast<uint16_t>(l.i(0, 65535));
            e.progress = static_cast<int32_t>(l.i(INT_MIN, INT_MAX)); e.burn = static_cast<int32_t>(l.i(0, INT_MAX)); e.burnJ = l.i(0, LLONG_MAX);
            for (int i = 0; i < kItemCount; i++) e.store[i] = l.u(ULLONG_MAX);
            e.n = static_cast<uint8_t>(l.i(0, kBeltCap));
            for (int k = 0; k < e.n; k++) { e.items[k].item = static_cast<uint8_t>(l.i(1, kItemCount - 1)); e.items[k].pos = static_cast<uint16_t>(l.i(0, kBeltLen)); }
            if (!l.done()) return fail("a building is damaged" + where);
            if (e.n && e.kind != Kind::Belt) return fail("a building is damaged" + where);
            for (int k = 1; k < e.n; k++) if (e.items[k - 1].pos - e.items[k].pos < kBeltGap) return fail("a belt is damaged" + where);
            if ((w.ents_.empty()) != (e.kind == Kind::Core)) return fail("the core must come first, and only once" + where);
            if (e.id == 0 || w.index_.count(e.id)) return fail("two buildings have the same number" + where);
            const int s = footprint(e.kind);
            for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) { if (w.occ_.count(key(e.x + i, e.y + j))) return fail("two buildings overlap" + where); w.occ_[key(e.x + i, e.y + j)] = e.id; }
            w.index_[e.id] = static_cast<int>(w.ents_.size());
            w.ents_.push_back(e);
            continue;
        } else if (tag == "m") {
            const int x = static_cast<int>(l.i(INT_MIN, INT_MAX)), y = static_cast<int>(l.i(INT_MIN, INT_MAX));
            w.mined_[key(x, y)] = static_cast<uint32_t>(l.u(0xFFFFFFFFull));
        } else if (tag == "p") {
            const int x = static_cast<int>(l.i(INT_MIN, INT_MAX)), y = static_cast<int>(l.i(INT_MIN, INT_MAX));
            const double v = l.d();
            if (v < 0) return fail("the pollution is damaged" + where);
            w.pol_[key(x, y)] = v;
        } else return fail("unknown line '" + tag.substr(0, 12) + "'" + where);
        if (!l.done()) return fail("a line is damaged" + where);
    }
    if (lineNo == 0) return fail("this is not a Defense Lab save");
    if (!haveSeed || !haveTick || !haveCam || !haveNext) return fail("a part of the save is missing");
    if (w.ents_.empty()) return fail("the core is missing");
    for (const Entity& e : w.ents_) if (e.id >= w.nextId_) return fail("the building numbers are damaged");
    w.dirty_ = true;
    *this = std::move(w);
    if (error) error->clear();
    return true;
}

// ---------------------------------------------------------------- files
bool readFile(const std::string& path, std::string& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    const bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}
bool writeFileAtomic(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool wrote = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool flushed = std::fflush(f) == 0;
    const bool closed = std::fclose(f) == 0;
    if (!wrote || !flushed || !closed) { std::remove(tmp.c_str()); return false; }
#ifdef _WIN32
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { std::remove(tmp.c_str()); return false; }
#else
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::remove(tmp.c_str()); return false; }
#endif
    return true;
}

// ---------------------------------------------------------------- sample and test bases
void buildSampleBase(World& w) {
    // iron: miner -> belt -> furnace (coal from a chest) -> belt with a corner -> core
    w.place(Kind::Miner, 9, -10, 3);
    for (int x = 8; x >= 5; x--) w.place(Kind::Belt, x, -9, 3);
    w.place(Kind::Belt, 4, -9, 2);
    w.place(Kind::Belt, 4, -8, 2);
    const uint32_t coalChest = w.place(Kind::Chest, 4, -10, 2);
    w.addToChest(coalChest, Item::Coal, kSlotMax);
    w.place(Kind::Furnace, 4, -7, 2);
    for (int y = -5; y <= -1; y++) w.place(Kind::Belt, 5, y, 2);
    for (int x = 5; x >= 2; x--) w.place(Kind::Belt, x, 0, 3);
    // coal: miner -> belt -> generator; poles carry the power to the miners
    w.place(Kind::Miner, -10, 9, 1);
    w.place(Kind::Belt, -8, 9, 1);
    w.place(Kind::Belt, -7, 9, 1);
    const uint32_t gen = w.place(Kind::Generator, -6, 8, 1);
    if (Entity* g = w.find(gen)) g->fuel = 5;
    const int poles[7][2] = {{-8, 7}, {-4, 7}, {-1, 3}, {3, 3}, {7, -2}, {8, -7}, {9, 7}};
    for (const auto& p : poles) w.place(Kind::Pole, p[0], p[1], 0);
    // stone: miner -> chest
    w.place(Kind::Miner, 11, 9, 3);
    w.place(Kind::Chest, 10, 10, 2);
}

int buildBigBase(World& w) {
    const int x0 = 2000, y0 = 2000;
    int n = 0;
    for (int i = 0; i < 64; i++) {
        const int y = y0 + 2 * i;
        const uint32_t src = w.place(Kind::Chest, x0, y, 1, true);
        n += src != 0;
        w.addToChest(src, Item::IronPlate, kChestMax);
        for (int x = 1; x <= 48; x++) n += w.place(Kind::Belt, x0 + x, y, 1, true) != 0;
        n += w.place(Kind::Chest, x0 + 49, y, 1, true) != 0;
    }
    return n;
}

}  // namespace defense
