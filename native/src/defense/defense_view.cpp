// Defense Lab (0.17 stage 1, new look in stage 2): the picture. Original art drawn in code (no art files, no other game's sprites): a dark red rocky planet seen from
// above with a slight front-face depth; chunky machines with thick dark outlines, bevelled bodies, rounded corners and one accent colour each.
// Speed guard: the ground of each chunk is a mesh made once and kept on the GPU; belts, buildings, wires and glow go into the renderer's one triangle batch, belt
// items into square batches per item kind. Animation comes from the game tick / the lab clock only; nothing is allocated per frame once the buffers have grown.
// Level of detail: below kLodZoom points a tile the small parts (blades, latches, stripes, chevrons) and the glow bloom are skipped.
#include "defense/defense_view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "render/renderer2d.h"

namespace defense {

namespace {
using render::Color;
Color rgb(int r, int g, int b, float a = 1) { return render::rgba(r, g, b, a); }
Color shade(Color c, float k) { return {std::min(1.0f, c.r * k), std::min(1.0f, c.g * k), std::min(1.0f, c.b * k), c.a}; }
Color mix(Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

// ---- the palette (a look, not data) ----
const Color kOutline = rgb(20, 16, 16);                   // the thick dark outline of every machine
const Color kHazardYellow = rgb(246, 196, 38), kHazardBlack = rgb(28, 26, 24);
const Color kTeal = rgb(54, 214, 196);                    // the core's ring light
const Color kGlowOrange = rgb(255, 150, 40);              // a working furnace / a burning generator
Color terrainColour(Terrain t) {
    switch (t) {
    case Terrain::Dust: return rgb(150, 86, 58);          // dust: a lighter orange-tan
    case Terrain::Pits: return rgb(112, 55, 39);          // pits, cracks, stones: the red-brown rock with marks on it
    case Terrain::Cracks: return rgb(116, 57, 40);
    case Terrain::Stones: return rgb(120, 60, 42);
    case Terrain::Waste: return rgb(34, 24, 23);          // the border band: dark wasteland
    default: return rgb(118, 58, 40);                     // rock: red-brown
    }
}
Color itemColour(Item i) {
    switch (i) {
    case Item::IronOre: return rgb(84, 102, 128);         // iron ore: dark slate blue (darker and rougher than the plate)
    case Item::CopperOre: return rgb(150, 82, 40);        // copper ore: dark rust orange
    case Item::Coal: return rgb(30, 30, 33);              // coal: black (with a grey highlight)
    case Item::Stone: return rgb(204, 188, 150);          // stone: beige
    case Item::IronPlate: return rgb(124, 164, 214);      // iron plate: steel blue
    case Item::CopperPlate: return rgb(244, 140, 58);     // copper plate: bright orange
    case Item::Brick: return rgb(164, 72, 50);            // brick: red-brown
    default: return rgb(255, 0, 255);
    }
}
Color oreGroundColour(Item i) { return i == Item::Stone ? rgb(170, 150, 112) : shade(itemColour(i), 0.85f); }   // ore lumps on the ground: rougher, darker
Color kindColour(Kind k) {
    switch (k) {
    case Kind::Core: return rgb(70, 80, 96);              // core: dark steel (teal ring light)
    case Kind::Miner: return rgb(236, 184, 40);           // miner: yellow
    case Kind::Furnace: return rgb(196, 104, 52);         // furnace: copper orange
    case Kind::Chest: return rgb(134, 120, 84);           // chest: khaki metal (yellow latch)
    case Kind::Generator: return rgb(210, 70, 48);        // generator: red-orange (hazard stripes)
    case Kind::Pole: return rgb(84, 84, 92);              // pole: grey mast (glowing node)
    default: return rgb(44, 44, 48);                      // belt: charcoal
    }
}
double heightOf(Kind k) { return k == Kind::Core ? 1.2 : k == Kind::Pole ? 1.6 : k == Kind::Chest ? 0.45 : k == Kind::Furnace ? 0.8 : 0.8; }   // tiles (a look: the shadow's length)
uint32_t h2(int x, int y, uint32_t s) { uint32_t h = s * 0x9E3779B1u ^ uint32_t(x) * 0x85EBCA77u ^ uint32_t(y) * 0xC2B2AE3Du; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13; return h; }
float hf(uint32_t h, int shift) { return float((h >> shift) & 255) / 255.0f; }
const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

// ---- the ground mesh's pieces ----
render::MeshVertex mv(float x, float y, Color c) {
    render::MeshVertex m{};
    m.p0[0] = x; m.p0[1] = y; m.p0[2] = 0;
    m.rgba[0] = c.r; m.rgba[1] = c.g; m.rgba[2] = c.b; m.rgba[3] = c.a;
    m.corner = -1; m.height = -1;
    return m;
}
void quad4V(std::vector<render::MeshVertex>& v, float x0, float y0, float x1, float y1, Color c00, Color c10, Color c11, Color c01) {
    v.push_back(mv(x0, y0, c00)); v.push_back(mv(x1, y0, c10)); v.push_back(mv(x1, y1, c11));
    v.push_back(mv(x0, y0, c00)); v.push_back(mv(x1, y1, c11)); v.push_back(mv(x0, y1, c01));
}
void quadV(std::vector<render::MeshVertex>& v, float x0, float y0, float x1, float y1, Color c) { quad4V(v, x0, y0, x1, y1, c, c, c, c); }
void ngonV(std::vector<render::MeshVertex>& v, float cx, float cy, float rx, float ry, int n, float rot, Color c) {
    for (int i = 0; i < n; i++) {
        const float a0 = rot + 6.2831853f * i / n, a1 = rot + 6.2831853f * (i + 1) / n;
        v.push_back(mv(cx, cy, c)); v.push_back(mv(cx + rx * std::cos(a0), cy + ry * std::sin(a0), c)); v.push_back(mv(cx + rx * std::cos(a1), cy + ry * std::sin(a1), c));
    }
}
void segV(std::vector<render::MeshVertex>& v, float x0, float y0, float x1, float y1, float w, Color c) {
    const float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-6f) return;
    const float nx = -dy / l * w / 2, ny = dx / l * w / 2;
    v.push_back(mv(x0 + nx, y0 + ny, c)); v.push_back(mv(x1 + nx, y1 + ny, c)); v.push_back(mv(x1 - nx, y1 - ny, c));
    v.push_back(mv(x0 + nx, y0 + ny, c)); v.push_back(mv(x1 - nx, y1 - ny, c)); v.push_back(mv(x0 - nx, y0 - ny, c));
}

// ---- the machines' pieces (immediate, into the renderer's triangle batch) ----
void roundRect(render::Renderer2D& r, double x, double y, double w, double h, double rad, Color c) {
    rad = std::max(0.0, std::min(rad, std::min(w, h) / 2));
    if (rad < 1.0) { r.rect(x, y, w, h, c); return; }
    r.rect(x + rad, y, w - 2 * rad, h, c);
    r.rect(x, y + rad, rad, h - 2 * rad, c);
    r.rect(x + w - rad, y + rad, rad, h - 2 * rad, c);
    // the corners: a quarter fan each (6 triangles), the same flat colour as the rects
    const double cxs[4] = {x + rad, x + w - rad, x + w - rad, x + rad}, cys[4] = {y + rad, y + rad, y + h - rad, y + h - rad};
    for (int k = 0; k < 4; k++) {
        const double a0 = 3.14159265 + k * 1.5707963;
        for (int i = 0; i < 4; i++) {
            const double t0 = a0 + i * 0.39269908, t1 = t0 + 0.39269908;
            r.triangle(cxs[k], cys[k], c, cxs[k] + rad * std::cos(t0), cys[k] + rad * std::sin(t0), c, cxs[k] + rad * std::cos(t1), cys[k] + rad * std::sin(t1), c);
        }
    }
}
// yellow / black hazard stripes in a strip (slanted bars; the ends are clipped to the strip)
void hazard(render::Renderer2D& r, double x, double y, double w, double h) {
    r.rect(x, y, w, h, kHazardBlack);
    const double step = std::max(4.0, h * 1.6);
    for (double s = x - h; s < x + w; s += step) {
        const double a = std::clamp(s, x, x + w), b = std::clamp(s + step / 2, x, x + w), c = std::clamp(s + h, x, x + w), d = std::clamp(s + h + step / 2, x, x + w);
        r.triangle(a, y + h, kHazardYellow, b, y + h, kHazardYellow, d, y, kHazardYellow);
        r.triangle(a, y + h, kHazardYellow, d, y, kHazardYellow, c, y, kHazardYellow);
    }
}
// a machine's body: outline, darker front face (depth), the top with a bevel (light top / left edge, darker inner panel)
struct Body { double x, y, w, h, front, rad; };
Body body(render::Renderer2D& r, double x, double y, double w, double h, double z, double frontTiles, Color base, bool detail) {
    const double o = std::max(1.0, z * 0.07), rad = z * 0.16, front = std::min(h * 0.3, frontTiles * z);
    roundRect(r, x - o, y - o, w + 2 * o, h + 2 * o, rad + o, kOutline);
    roundRect(r, x, y, w, h, rad, shade(base, 0.52f));                  // the front face, in shade
    roundRect(r, x, y, w, h - front, rad, base);                        // the top
    if (detail) {
        const double b = std::max(1.0, z * 0.07);
        r.rect(x + rad, y + b * 0.4, w - 2 * rad, b, shade(base, 1.35f));                 // the lit top edge
        r.rect(x + b * 0.4, y + rad, b, h - front - 2 * rad, shade(base, 1.18f));         // the lit left edge
        roundRect(r, x + 2.2 * b, y + 2.2 * b, w - 4.4 * b, h - front - 4.4 * b, rad * 0.6, shade(base, 0.9f));   // the inner panel
    }
    return {x, y, w, h, front, rad};
}
}  // namespace

void DefenseView::buildChunk(World& w, int cx, int cy) {
    const Chunk& c = w.chunk(cx, cy);
    build_.clear();
    // the colour a tile's corner gets: the panel vignette (4 x 4 tile panels, darker toward their corners), and outside the map darker toward the far edge
    const auto vign = [&](int x, int y) {
        const float px = float(((x % 4) + 4) % 4) - 2.0f, py = float(((y % 4) + 4) % 4) - 2.0f;
        return 1.0f - 0.05f * (px * px + py * py) / 8.0f;
    };
    // dust patches: a smooth blend from the noise at each tile corner (the edges are soft, not tile steps)
    for (int j = 0; j <= kChunk; j++)
        for (int i = 0; i <= kChunk; i++) {
            const double n = fbm(w.seed() + 31u, (cx * kChunk + i) / 36.0, (cy * kChunk + j) / 36.0, 3);
            const double t = std::clamp((n - 0.12) / 0.2, 0.0, 1.0);
            dust_[std::size_t(j * (kChunk + 1) + i)] = static_cast<float>(t * t * (3 - 2 * t));
        }
    const auto corner = [&](Color rock, int i, int j, int x, int y) { return shade(mix(rock, terrainColour(Terrain::Dust), 0.8f * dust_[std::size_t(j * (kChunk + 1) + i)]), vign(x, y)); };
    for (int j = 0; j < kChunk; j++)
        for (int i = 0; i < kChunk; i++) {
            const int k = j * kChunk + i, x = cx * kChunk + i, y = cy * kChunk + j;
            const Terrain t = static_cast<Terrain>(c.terrain[k]);
            const uint32_t h = h2(x, y, w.seed());
            if (t == Terrain::Waste) {
                const int out = std::max({kMapMin - x, x - kMapMax, kMapMin - y, y - kMapMax, 0});
                const float f = 1.0f - 0.5f * std::min(1.0f, float(out) / kBorder) + 0.06f * (hf(h, 0) - 0.5f);
                quadV(build_, float(x), float(y), float(x + 1), float(y + 1), shade(terrainColour(t), f));
                continue;
            }
            const float var = 1.0f + 0.06f * static_cast<float>(valueNoise(w.seed() + 7u, x / 6.0, y / 6.0)) + 0.03f * (hf(h, 0) - 0.5f);
            Color col = shade(terrainColour(t == Terrain::Dust ? Terrain::Rock : t), var);
            const Item ore = static_cast<Item>(c.ore[k]);
            if (ore != Item::None) col = mix(col, oreGroundColour(ore), 0.22f);
            quad4V(build_, float(x), float(y), float(x + 1), float(y + 1), corner(col, i, j, x, y), corner(col, i + 1, j, x + 1, y), corner(col, i + 1, j + 1, x + 1, y + 1), corner(col, i, j + 1, x, y + 1));
            col = mix(col, terrainColour(Terrain::Dust), 0.8f * dust_[std::size_t(j * (kChunk + 1) + i)]);   // (the marks below take the tile's blended colour)
            // the looks of the variants (all buildable)
            const float ox = x + 0.2f + 0.6f * hf(h, 8), oy = y + 0.2f + 0.6f * hf(h, 16);
            switch (t) {
            case Terrain::Dust: ngonV(build_, ox, oy, 0.38f, 0.26f, 7, hf(h, 24) * 3, shade(col, 1.08f)); break;
            case Terrain::Pits:
                ngonV(build_, ox + 0.04f, oy + 0.05f, 0.24f, 0.2f, 8, 0, shade(col, 1.18f));     // the lit lower rim
                ngonV(build_, ox, oy, 0.22f, 0.18f, 8, 0, shade(col, 0.58f));
                break;
            case Terrain::Cracks: {
                float px = x + 0.1f + 0.3f * hf(h, 8), py = y + 0.15f + 0.7f * hf(h, 16);
                for (int s = 0; s < 3; s++) {
                    const float qx = px + 0.28f, qy = py + (hf(h, 4 + s * 7) - 0.5f) * 0.4f;
                    segV(build_, px, py, qx, qy, 0.045f, shade(col, 0.55f));
                    px = qx; py = qy;
                }
                break;
            }
            case Terrain::Stones:
                for (int s = 0; s < 3; s++) {
                    const float sx = x + 0.2f + 0.6f * hf(h, s * 9), sy = y + 0.2f + 0.6f * hf(h, s * 9 + 5), rr = 0.06f + 0.04f * hf(h, s * 3 + 20);
                    ngonV(build_, sx + 0.025f, sy + 0.03f, rr, rr * 0.8f, 6, 0, shade(col, 0.6f));   // its shadow
                    ngonV(build_, sx, sy, rr, rr * 0.8f, 6, 0.4f, rgb(176, 120, 92));
                }
                break;
            default: break;
            }
            if (ore != Item::None) {                                    // ore lumps: rough outlined chunks with a highlight
                const uint32_t g = h2(x, y, w.seed() ^ 0xABCDu);
                const Color oc = oreGroundColour(ore);
                for (int s = 0; s < 3; s++) {
                    const float lx = x + 0.2f + 0.6f * hf(g, s * 8), ly = y + 0.2f + 0.6f * hf(g, s * 8 + 4), r = 0.12f + 0.04f * s;
                    ngonV(build_, lx, ly, r + 0.035f, (r + 0.035f) * 0.85f, 6, hf(g, s + 3), shade(kOutline, 1.0f));
                    ngonV(build_, lx, ly, r, r * 0.85f, 6, hf(g, s + 3), oc);
                    ngonV(build_, lx - r * 0.3f, ly - r * 0.3f, r * 0.35f, r * 0.3f, 5, 0, shade(oc, ore == Item::Coal ? 3.2f : 1.3f));
                }
            }
        }
    // the panels' dark seams (every 4 tiles; only inside the map)
    const Color seam = rgb(40, 18, 14, 0.3f);
    for (int s = 0; s < kChunk; s += 4) {                              // (the chunk on the other side draws the last one)
        const int gx = cx * kChunk + s, gy = cy * kChunk + s;
        const float y0 = float(std::max(cy * kChunk, kMapMin)), y1 = float(std::min(cy * kChunk + kChunk, kMapMax + 1));
        const float x0 = float(std::max(cx * kChunk, kMapMin)), x1 = float(std::min(cx * kChunk + kChunk, kMapMax + 1));
        if (gx >= kMapMin && gx <= kMapMax + 1 && y1 > y0) quadV(build_, gx - 0.02f, y0, gx + 0.02f, y1, seam);
        if (gy >= kMapMin && gy <= kMapMax + 1 && x1 > x0) quadV(build_, x0, gy - 0.02f, x1, gy + 0.02f, seam);
    }
}

void DefenseView::draw(render::Renderer2D& r, ImDrawList* text, double uiScale, DefenseLab& L, double W, double H, const app::ThemeColors& theme) {
    (void)theme;
    const auto t0 = std::chrono::steady_clock::now();
    frame_++;
    stats_ = ViewStats{};
    L.viewW = W; L.viewH = H;
    if (!L.loaded) return;
    L.clampView();                                                       // the window may have changed: the view stays on map + band
    World& w = L.world;
    w.layout();
    const double z = w.zoom;
    const auto sx = [&](double tx) { return (tx - w.camX) * z + W / 2; };
    const auto sy = [&](double ty) { return (ty - w.camY) * z + H / 2; };
    const int tx0 = static_cast<int>(std::floor(L.tileX(0))) - 1, tx1 = static_cast<int>(std::floor(L.tileX(W))) + 1;
    const int ty0 = static_cast<int>(std::floor(L.tileY(0))) - 1, ty1 = static_cast<int>(std::floor(L.tileY(H))) + 1;
    const bool detail = z >= kLodZoom;                                   // level of detail
    const double clk = L.clock;

    // ---- the ground: one retained mesh a chunk ----
    render::MeshProjection P;
    P.identity = false; P.flat = true; P.K = z; P.cx = w.camX; P.cy = w.camY; P.fx = W / 2; P.fy = H / 2;
    int builds = 0;
    for (int cy = World::floorDiv(ty0, kChunk); cy <= World::floorDiv(ty1, kChunk); cy++)
        for (int cx = World::floorDiv(tx0, kChunk); cx <= World::floorDiv(tx1, kChunk); cx++) {
            ChunkMesh& m = meshes_[World::key(cx, cy)];
            if ((m.id < 0 || m.seed != w.seed()) && builds < 8) {
                if (m.id >= 0) r.meshDelete(m.id);
                buildChunk(w, cx, cy);
                m.id = r.meshCreate(build_); m.count = static_cast<int>(build_.size()); m.seed = w.seed();
                builds++;
            }
            m.lastFrame = frame_;
            if (m.id >= 0) { r.drawMesh(m.id, 0, m.count, P); stats_.chunks++; }
            else r.rect(sx(cx * kChunk), sy(cy * kChunk), kChunk * z, kChunk * z, terrainColour(Terrain::Rock));
        }
    stats_.meshesBuilt = builds;
    if (meshes_.size() > 160)                                           // forget the ground far from the camera
        for (auto it = meshes_.begin(); it != meshes_.end();) {
            if (frame_ - it->second.lastFrame > 120) { r.meshDelete(it->second.id); it = meshes_.erase(it); } else ++it;
        }
    stats_.meshesAlive = static_cast<int>(meshes_.size());

    const auto visible = [&](const Entity& e) { const int s = footprint(e.kind); return e.x + s + 2 >= tx0 && e.x - 2 <= tx1 && e.y + s + 2 >= ty0 && e.y - 2 <= ty1; };
    const float lw = static_cast<float>(std::max(1.0, z * 0.05));

    // ---- belts: dark charcoal with a raised lighter edge and bright chevrons moving along (phase from the tick); items on top ----
    for (std::vector<float>& v : itemXY_) v.clear();
    const double anim = std::fmod(double(w.tick()) * kBeltSpeed / kBeltLen, 1.0);
    const Color beltBase = kindColour(Kind::Belt), rail = rgb(92, 92, 102), chev = rgb(255, 198, 44);
    for (const Entity& e : w.entities()) {
        if (e.kind != Kind::Belt || !visible(e)) continue;
        stats_.entitiesDrawn++;
        const double x = sx(e.x), y = sy(e.y), cxp = x + z / 2, cyp = y + z / 2;
        const int d = e.dir & 3;
        if (detail) { r.rect(x, y, z, z, kOutline.withAlpha(0.85f)); r.rect(x + z * 0.05, y + z * 0.05, z * 0.9, z * 0.9, beltBase); }
        else r.rect(x, y, z, z, beltBase);
        const double inX = e.shape == 0 ? -DX[d] : e.shape < 0 ? DY[d] : -DY[d];
        const double inY = e.shape == 0 ? -DY[d] : e.shape < 0 ? -DX[d] : DX[d];
        const auto at = [&](double t, double& px, double& py) {          // the path: the middle of the entry edge, the tile's middle, the middle of the exit edge
            const double ex = cxp + inX * z / 2, ey = cyp + inY * z / 2, xx = cxp + DX[d] * z / 2, xy = cyp + DY[d] * z / 2;
            const double a = (1 - t) * (1 - t), b = 2 * t * (1 - t), c = t * t;
            px = a * ex + b * cxp + c * xx; py = a * ey + b * cyp + c * xy;
        };
        for (int s = 0; s < 4; s++) {                                    // the raised edges along the two sides the items do not cross
            const bool entry = DX[s] == inX && DY[s] == inY, exit = s == d;
            if (entry || exit) continue;
            const double rx = cxp + DX[s] * z * 0.4, ry = cyp + DY[s] * z * 0.4;   // (axis-aligned: a rect)
            if (DX[s] != 0) r.rect(rx - z * 0.05, y + z * 0.05, z * 0.1, z * 0.9, rail); else r.rect(x + z * 0.05, ry - z * 0.05, z * 0.9, z * 0.1, rail);
        }
        if (detail)
            for (int k = 0; k < 2; k++) {                                // two chevrons moving along the path
                const double t = std::fmod(anim + k * 0.5, 1.0);
                double px, py, qx, qy;
                at(t, px, py); at(std::min(1.0, t + 0.05), qx, qy);
                double fx = qx - px, fy = qy - py; const double l = std::hypot(fx, fy); if (l < 1e-9) continue; fx /= l; fy /= l;
                const double s = z * 0.17;
                r.line(px - fx * s - fy * s, py - fy * s + fx * s, px, py, lw * 1.6, chev.withAlpha(0.85f));
                r.line(px - fx * s + fy * s, py - fy * s - fx * s, px, py, lw * 1.6, chev.withAlpha(0.85f));
            }
        for (int k = 0; k < e.n; k++) {
            double px, py;
            at(e.items[k].pos / double(kBeltLen), px, py);
            std::vector<float>& v = itemXY_[e.items[k].item];
            v.push_back(static_cast<float>(px)); v.push_back(static_cast<float>(py));
            stats_.items++;
        }
    }
    for (int i = 1; i < kItemCount; i++) {                               // items: small bright chips with a dark outline
        if (itemXY_[i].empty()) continue;
        const int n = static_cast<int>(itemXY_[i].size() / 2);
        r.squares(itemXY_[i].data(), n, z * 0.36 + 1.5, kOutline.withAlpha(0.9f));
        r.squares(itemXY_[i].data(), n, z * 0.28, itemColour(static_cast<Item>(i)));
        if (static_cast<Item>(i) == Item::Coal && detail) r.squares(itemXY_[i].data(), n, z * 0.1, rgb(120, 120, 130));   // coal's highlight
    }

    // ---- buildings: soft shadow first (light from the top left), then the body and its parts ----
    for (const Entity& e : w.entities()) {
        if (e.kind == Kind::Belt || !visible(e)) continue;
        const double s = footprint(e.kind), hgt = heightOf(e.kind);
        const double x = sx(e.x), y = sy(e.y), in = e.kind == Kind::Pole ? z * 0.36 : z * 0.08;
        const double bw = s * z - 2 * in;
        const double off = hgt * z * 0.35;
        if (!detail) { r.rect(x + in + off * 0.6, y + in + off * 0.6, bw, bw, rgb(0, 0, 0, 0.22f)); stats_.entitiesDrawn++; continue; }
        for (int k = 0; k < 3; k++) {
            const double g = (k + 1) * z * 0.06;
            roundRect(r, x + in + off * 0.6 - g + z * 0.05, y + in + off * 0.6 - g + z * 0.05, bw + 2 * g, bw + 2 * g, z * 0.2 + g, rgb(0, 0, 0, 0.09f));
        }
        stats_.entitiesDrawn++;
    }
    const auto pulse = [&](double speed, double phase) { return 0.5 + 0.5 * std::sin(clk * speed + phase); };
    for (const Entity& e : w.entities()) {
        if (e.kind == Kind::Belt || !visible(e)) continue;
        const double s = footprint(e.kind);
        const double x = sx(e.x), y = sy(e.y), in = z * 0.1;
        const Color base = kindColour(e.kind);
        const double ph = double(e.id % 97) * 0.37;                       // each machine its own phase
        if (e.kind == Kind::Pole) {                                      // a mast with a cross-arm and a glowing node
            const double mx = x + z * 0.5, mw = std::max(2.0, z * 0.2);
            roundRect(r, x + z * 0.18, y + z * 0.62, z * 0.64, z * 0.3, z * 0.08, kOutline);   // the foot
            roundRect(r, x + z * 0.22, y + z * 0.65, z * 0.56, z * 0.22, z * 0.06, shade(base, 0.8f));
            r.rect(mx - mw / 2 - 1, y + z * 0.1, mw + 2, z * 0.62, kOutline);
            r.rect(mx - mw / 2, y + z * 0.12, mw, z * 0.58, base);
            if (detail) r.rect(mx - mw / 2, y + z * 0.12, mw * 0.35, z * 0.58, shade(base, 1.4f));
            r.rect(x + z * 0.16, y + z * 0.18, z * 0.68, std::max(2.0, z * 0.1), kOutline);
            const double nodeY = y + z * 0.16;
            if (detail) r.fadeDisc(mx, nodeY, z * 0.42, rgb(255, 220, 120, 0.35f + 0.15f * float(pulse(3, ph))));
            r.disc(mx, nodeY, z * 0.13, kOutline);
            r.disc(mx, nodeY, z * 0.09, rgb(255, 232, 150));
            continue;
        }
        const Body b = body(r, x + in, y + in, s * z - 2 * in, s * z - 2 * in, z, heightOf(e.kind) * 0.25, base, detail);
        const double cx = b.x + b.w / 2, cy = b.y + (b.h - b.front) / 2, top = b.h - b.front;
        switch (e.kind) {
        case Kind::Core: {                                               // the landmark: hazard stripes on the front, a ring light that pulses
            if (detail) hazard(r, b.x + b.rad, b.y + b.h - b.front * 0.85, b.w - 2 * b.rad, b.front * 0.6);
            const float p = static_cast<float>(pulse(2.5, 0));
            r.disc(cx, cy, z * 1.0, kOutline);
            r.disc(cx, cy, z * 0.9, shade(base, 0.75f));
            if (detail) r.fadeDisc(cx, cy, z * 1.9, kTeal.withAlpha(0.18f + 0.22f * p));
            r.ring(cx, cy, z * 0.72, std::max(1.5, z * 0.13), mix(shade(kTeal, 0.6f), kTeal, p));
            r.disc(cx, cy, z * 0.38, rgb(36, 40, 48));
            r.disc(cx, cy, z * 0.24, mix(rgb(120, 220, 210), rgb(220, 255, 250), p));
            break;
        }
        case Kind::Miner: {                                              // a drill head that spins while it digs
            const bool powered = e.net >= 0 && e.net < int(w.nets().size()) && w.nets()[std::size_t(e.net)].satPermille > 0;
            const bool working = powered && e.outCount == 0;
            r.disc(cx, cy, top * 0.4, kOutline);
            r.disc(cx, cy, top * 0.34, rgb(60, 58, 56));
            const double a = working ? double(w.tick()) * 0.12 : ph;
            for (int k = 0; k < 3; k++) {
                const double t = a + k * 2.0943951, rr = top * 0.32;
                const Color bl = rgb(186, 186, 196);
                r.triangle(cx, cy, bl, cx + std::cos(t) * rr, cy + std::sin(t) * rr, bl, cx + std::cos(t + 0.7) * rr * 0.7, cy + std::sin(t + 0.7) * rr * 0.7, bl);
            }
            r.disc(cx, cy, top * 0.09, kOutline);
            if (detail) { r.rect(b.x + b.w * 0.12, b.y + top * 0.08, b.w * 0.12, top * 0.12, shade(base, 0.6f)); r.rect(b.x + b.w * 0.76, b.y + top * 0.08, b.w * 0.12, top * 0.12, shade(base, 0.6f)); }
            if (!powered) { r.disc(b.x + z * 0.22, b.y + z * 0.22, z * 0.17, kOutline); r.disc(b.x + z * 0.22, b.y + z * 0.22, z * 0.12, rgb(235, 60, 50)); }   // no power
            break;
        }
        case Kind::Furnace: {                                            // round copper furnace: glows bright orange only while it works
            const bool working = e.burn > 0 && e.inCount > 0;
            r.disc(cx, cy, top * 0.46, kOutline);
            r.disc(cx, cy, top * 0.41, shade(base, 1.1f));
            if (detail) r.ring(cx, cy, top * 0.33, std::max(1.0, z * 0.05), shade(base, 0.7f));
            const float p = static_cast<float>(pulse(5, ph));
            if (working && detail) r.fadeDisc(cx, cy, top * 1.1, kGlowOrange.withAlpha(0.45f + 0.15f * p));   // the bloom
            r.disc(cx, cy, top * 0.24, working ? mix(rgb(255, 170, 60), rgb(255, 236, 150), p) : rgb(44, 34, 30));
            break;
        }
        case Kind::Chest:                                                // a lid with a seam and a latch
            r.rect(b.x + b.rad, b.y + top * 0.42, b.w - 2 * b.rad, std::max(1.0, z * 0.06), shade(base, 0.55f));
            if (detail) {
                r.rect(cx - z * 0.11, b.y + top * 0.3, z * 0.22, z * 0.26, kOutline);
                r.rect(cx - z * 0.07, b.y + top * 0.33, z * 0.14, z * 0.18, kHazardYellow);
            }
            break;
        case Kind::Generator: {                                          // hazard stripes; a core that pulses while it burns, dark when idle
            const bool burning = e.burnJ > 0 || e.fuel > 0;
            if (detail) hazard(r, b.x + b.rad, b.y + b.h - b.front * 0.85, b.w - 2 * b.rad, b.front * 0.6);
            r.disc(cx - b.w * 0.27, b.y + top * 0.3, top * 0.13, kOutline);   // exhausts
            r.disc(cx + b.w * 0.27, b.y + top * 0.3, top * 0.13, kOutline);
            const float p = static_cast<float>(pulse(4, ph));
            r.disc(cx, cy + top * 0.06, top * 0.27, kOutline);
            if (burning && detail) r.fadeDisc(cx, cy + top * 0.06, top * 0.8, kGlowOrange.withAlpha(0.3f + 0.3f * p));
            r.disc(cx, cy + top * 0.06, top * 0.21, burning ? mix(rgb(255, 150, 40), rgb(255, 230, 120), p) : rgb(52, 40, 36));
            break;
        }
        default: break;
        }
        const bool hasDir = e.kind == Kind::Miner || e.kind == Kind::Furnace || e.kind == Kind::Chest;
        if (hasDir && detail) {                                          // the output: a small arrow on the front edge
            int ox, oy;
            const int d = e.dir & 3, a = s == 3 ? 1 : 0;
            switch (d) { case 0: ox = e.x + a; oy = e.y - 1; break; case 1: ox = e.x + int(s); oy = e.y + a; break; case 2: ox = e.x + int(s) - 1 - a; oy = e.y + int(s); break; default: ox = e.x - 1; oy = e.y + int(s) - 1 - a; }
            const double ax = sx(ox + 0.5) - DX[d] * z * 0.66, ay = sy(oy + 0.5) - DY[d] * z * 0.66, q = z * 0.17;
            const Color wc = rgb(250, 250, 250);
            r.triangle(ax + DX[d] * q * 1.3, ay + DY[d] * q * 1.3, kOutline, ax - DX[d] * q * 1.2 - DY[d] * q * 1.3, ay - DY[d] * q * 1.2 + DX[d] * q * 1.3, kOutline, ax - DX[d] * q * 1.2 + DY[d] * q * 1.3, ay - DY[d] * q * 1.2 - DX[d] * q * 1.3, kOutline);
            r.triangle(ax + DX[d] * q, ay + DY[d] * q, wc, ax - DX[d] * q - DY[d] * q, ay - DY[d] * q + DX[d] * q, wc, ax - DX[d] * q + DY[d] * q, ay - DY[d] * q - DX[d] * q, wc);
        }
    }
    // ---- power wires between poles: thin, from node to node ----
    poles_.clear();
    for (int i = 0; i < int(w.entities().size()); i++) if (w.entities()[std::size_t(i)].kind == Kind::Pole) poles_.push_back(i);
    for (std::size_t a = 0; a < poles_.size(); a++)
        for (std::size_t b = a + 1; b < poles_.size(); b++) {
            const Entity &p = w.entities()[std::size_t(poles_[a])], &q = w.entities()[std::size_t(poles_[b])];
            const double dx = p.x - q.x, dy = p.y - q.y;
            if (dx * dx + dy * dy > kPoleReach * kPoleReach || (!visible(p) && !visible(q))) continue;
            r.line(sx(p.x + 0.5), sy(p.y + 0.16), sx(q.x + 0.5), sy(q.y + 0.16), std::max(1.0, z * 0.045), rgb(24, 20, 18, 0.85f));
            if (detail) r.line(sx(p.x + 0.5), sy(p.y + 0.16), sx(q.x + 0.5), sy(q.y + 0.16), std::max(0.6, z * 0.015), rgb(255, 210, 120, 0.35f));
        }

    // ---- the pollution overlay: one tint a chunk ----
    if (L.overlay)
        for (int cy = World::floorDiv(ty0, kChunk); cy <= World::floorDiv(ty1, kChunk); cy++)
            for (int cx = World::floorDiv(tx0, kChunk); cx <= World::floorDiv(tx1, kChunk); cx++) {
                const double p = w.pollution(cx, cy);
                if (p <= 0.001) continue;
                const float a = static_cast<float>(std::min(0.6, 0.12 * std::log10(1 + p * 10)));
                r.rect(sx(cx * kChunk), sy(cy * kChunk), kChunk * z, kChunk * z, rgb(150, 40, 20, a));
            }

    // ---- the ghost: the building (or the belt line being dragged) under the pointer, green where it can go, red where not ----
    const Color okC = rgb(80, 230, 120, 0.42f), noC = rgb(240, 70, 60, 0.42f), white = rgb(255, 255, 255, 0.9f);
    const auto ghostArrow = [&](double cx, double cy, int d, double q) {
        r.triangle(cx + DX[d] * q, cy + DY[d] * q, white, cx - DX[d] * q * 0.6 - DY[d] * q * 0.7, cy - DY[d] * q * 0.6 + DX[d] * q * 0.7, white,
                   cx - DX[d] * q * 0.6 + DY[d] * q * 0.7, cy - DY[d] * q * 0.6 - DX[d] * q * 0.7, white);
    };
    if (L.dragBuild && L.tool == Kind::Belt) {
        beltLine(L.dragX0, L.dragY0, L.dragX1, L.dragY1, L.dir, L.line);
        for (const BeltStep& st : L.line) {
            r.rect(sx(st.x), sy(st.y), z, z, w.canPlace(Kind::Belt, st.x, st.y) ? okC : noC);
            ghostArrow(sx(st.x + 0.5), sy(st.y + 0.5), st.dir, z * 0.3);
        }
    } else if (L.tool != Kind::None && L.hoverOn) {
        int ax, ay;
        L.anchor(L.tool, L.hoverX, L.hoverY, ax, ay);
        const bool ok = w.canPlace(L.tool, ax, ay);
        const double s = footprint(L.tool);
        if (L.tool == Kind::Pole) {
            const int rr = static_cast<int>(std::floor(kPoleSupply));
            r.rect(sx(ax - rr), sy(ay - rr), (2 * rr + 1) * z, (2 * rr + 1) * z, rgb(90, 160, 255, 0.12f));
        }
        roundRect(r, sx(ax) + z * 0.06, sy(ay) + z * 0.06, s * z - z * 0.12, s * z - z * 0.12, z * 0.16, kindColour(L.tool).withAlpha(0.45f));
        r.rect(sx(ax), sy(ay), s * z, s * z, ok ? okC : noC);
        if (L.tool != Kind::Pole && L.tool != Kind::Generator) ghostArrow(sx(ax + s / 2), sy(ay + s / 2), L.dir & 3, z * 0.3 * std::min(2.0, s));
    }

    // ---- a slight vignette over the whole picture ----
    {
        const double v = std::min(W, H) * 0.18;
        const Color a = rgb(0, 0, 0, 0.28f), o = rgb(0, 0, 0, 0);
        r.triangle(0, 0, a, W, 0, a, W, v, o); r.triangle(0, 0, a, W, v, o, 0, v, o);
        r.triangle(0, H - v, o, W, H - v, o, W, H, a); r.triangle(0, H - v, o, W, H, a, 0, H, a);
        r.triangle(0, 0, a, v, 0, o, v, H, o); r.triangle(0, 0, a, v, H, o, 0, H, a);
        r.triangle(W - v, 0, o, W, 0, a, W, H, a); r.triangle(W - v, 0, o, W, H, a, W - v, H, o);
    }

    // ---- the arrow to the core when it is off screen, with the distance in tiles ----
    const double aW = L.freeW > 0 ? std::min(W, L.freeW) : W, aH = std::max(100.0, H - L.barH);   // the part of the picture not under the Controls panel or the build bar
    const CoreArrow arrow = coreArrow(w.camX + (aW - W) / 2 / z, w.camY + (aH - H) / 2 / z, z, aW, aH, w.coreCX(), w.coreCY(), 40);
    stats_.arrow = arrow.show;
    if (arrow.show) {
        const double ca = std::cos(arrow.angle), sa = std::sin(arrow.angle), q = 16;
        r.disc(arrow.x, arrow.y, 22, kOutline.withAlpha(0.8f));
        r.triangle(arrow.x + ca * q * 1.25, arrow.y + sa * q * 1.25, kOutline, arrow.x - ca * q * 0.8 - sa * q * 1.0, arrow.y - sa * q * 0.8 + ca * q * 1.0, kOutline,
                   arrow.x - ca * q * 0.8 + sa * q * 1.0, arrow.y - sa * q * 0.8 - ca * q * 1.0, kOutline);
        r.triangle(arrow.x + ca * q, arrow.y + sa * q, kTeal, arrow.x - ca * q * 0.6 - sa * q * 0.75, arrow.y - sa * q * 0.6 + ca * q * 0.75, kTeal,
                   arrow.x - ca * q * 0.6 + sa * q * 0.75, arrow.y - sa * q * 0.6 - ca * q * 0.75, kTeal);
        if (text) {
            char b[48];
            std::snprintf(b, sizeof b, "Core %.0f tiles", arrow.dist);
            const ImVec2 ts = ImGui::CalcTextSize(b);
            const double lx = std::clamp(arrow.x - ca * 34, 4.0 + ts.x / 2 / uiScale, W - 4.0 - ts.x / 2 / uiScale), ly = std::clamp(arrow.y - sa * 34, 4.0 + ts.y / 2 / uiScale, H - 4.0 - ts.y / 2 / uiScale);
            const ImVec2 p(static_cast<float>(lx * uiScale - ts.x / 2), static_cast<float>(ly * uiScale - ts.y / 2));
            text->AddRectFilled(ImVec2(p.x - 6, p.y - 3), ImVec2(p.x + ts.x + 6, p.y + ts.y + 3), IM_COL32(20, 16, 16, 200), 6);
            text->AddText(p, IM_COL32(230, 255, 250, 255), b);
        }
    }
    // the core's name, when there is room for it
    if (text && z >= 16) {
        const Entity& c = w.core();
        const char* name = "Core";
        const ImVec2 ts = ImGui::CalcTextSize(name);
        text->AddText(ImVec2(static_cast<float>(sx(c.x + 1.5) * uiScale - ts.x / 2), static_cast<float>(sy(c.y + 3.15) * uiScale)), IM_COL32(255, 255, 255, 230), name);
    }
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace defense
