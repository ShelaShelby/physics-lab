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

// the ground's colours (a look, not data)
Color terrainColour(Terrain t) {
    switch (t) {
    case Terrain::Deep: return rgb(31, 78, 121);
    case Terrain::Shallow: return rgb(58, 124, 165);
    case Terrain::Sand: return rgb(216, 196, 138);
    case Terrain::Grass: return rgb(95, 143, 62);
    default: return rgb(125, 117, 107);
    }
}
Color itemColour(Item i) {
    switch (i) {
    case Item::IronOre: return rgb(143, 163, 184);
    case Item::CopperOre: return rgb(200, 116, 58);
    case Item::Coal: return rgb(43, 43, 43);
    case Item::Stone: return rgb(185, 169, 138);
    case Item::IronPlate: return rgb(205, 214, 224);
    case Item::CopperPlate: return rgb(232, 146, 84);
    case Item::Brick: return rgb(168, 72, 52);
    default: return rgb(255, 0, 255);
    }
}
Color kindColour(Kind k) {
    switch (k) {
    case Kind::Core: return rgb(88, 110, 140);
    case Kind::Miner: return rgb(196, 160, 60);
    case Kind::Furnace: return rgb(150, 140, 130);
    case Kind::Chest: return rgb(150, 105, 60);
    case Kind::Generator: return rgb(110, 125, 110);
    case Kind::Pole: return rgb(120, 90, 60);
    default: return rgb(60, 60, 64);
    }
}
double heightOf(Kind k) { return k == Kind::Core ? 1.2 : k == Kind::Pole ? 1.6 : k == Kind::Chest ? 0.4 : k == Kind::Furnace ? 0.7 : 0.8; }   // tiles (a look: the shadow's length)
uint32_t h2(int x, int y, uint32_t s) { uint32_t h = s * 0x9E3779B1u ^ uint32_t(x) * 0x85EBCA77u ^ uint32_t(y) * 0xC2B2AE3Du; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13; return h; }
const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

void quadV(std::vector<render::MeshVertex>& v, float x0, float y0, float x1, float y1, Color c) {
    const float px[6] = {x0, x1, x1, x0, x1, x0}, py[6] = {y0, y0, y1, y0, y1, y1};
    for (int i = 0; i < 6; i++) {
        render::MeshVertex m{};
        m.p0[0] = px[i]; m.p0[1] = py[i]; m.p0[2] = 0;
        m.rgba[0] = c.r; m.rgba[1] = c.g; m.rgba[2] = c.b; m.rgba[3] = c.a;
        m.corner = -1; m.height = -1;
        v.push_back(m);
    }
}
}  // namespace

void DefenseView::buildChunk(World& w, int cx, int cy) {
    const Chunk& c = w.chunk(cx, cy);
    build_.clear();
    for (int j = 0; j < kChunk; j++)
        for (int i = 0; i < kChunk; i++) {
            const int k = j * kChunk + i, x = cx * kChunk + i, y = cy * kChunk + j;
            const Terrain t = static_cast<Terrain>(c.terrain[k]);
            const float var = 1.0f + 0.07f * static_cast<float>(valueNoise(w.seed() + 7u, x / 5.0, y / 5.0)) + 0.025f * (float(h2(x, y, w.seed()) & 255) / 255.0f - 0.5f);
            Color col = shade(terrainColour(t), var);
            const Item ore = static_cast<Item>(c.ore[k]);
            if (ore != Item::None) col = mix(col, itemColour(ore), 0.3f);
            quadV(build_, float(x), float(y), float(x + 1), float(y + 1), col);
            if (ore != Item::None) {                                    // three small lumps of ore on the tile
                const uint32_t h = h2(x, y, w.seed() ^ 0xABCDu);
                for (int s = 0; s < 3; s++) {
                    const float ox = 0.15f + 0.6f * float((h >> (s * 8)) & 255) / 255.0f, oy = 0.15f + 0.6f * float((h >> (s * 8 + 4)) & 255) / 255.0f, r = 0.11f + 0.04f * s;
                    quadV(build_, x + ox - r, y + oy - r, x + ox + r, y + oy + r, shade(itemColour(ore), 0.8f + 0.12f * s));
                }
            }
        }
}

void DefenseView::draw(render::Renderer2D& r, ImDrawList* text, double uiScale, DefenseLab& L, double W, double H, const app::ThemeColors& theme) {
    const auto t0 = std::chrono::steady_clock::now();
    frame_++;
    stats_ = ViewStats{};
    L.viewW = W; L.viewH = H;
    if (!L.loaded) return;
    World& w = L.world;
    w.layout();
    const double z = w.zoom;
    const auto sx = [&](double tx) { return (tx - w.camX) * z + W / 2; };
    const auto sy = [&](double ty) { return (ty - w.camY) * z + H / 2; };
    const int tx0 = static_cast<int>(std::floor(L.tileX(0))) - 1, tx1 = static_cast<int>(std::floor(L.tileX(W))) + 1;
    const int ty0 = static_cast<int>(std::floor(L.tileY(0))) - 1, ty1 = static_cast<int>(std::floor(L.tileY(H))) + 1;

    // ---- the ground: one retained mesh a chunk ----
    render::MeshProjection P;
    P.identity = false; P.flat = true; P.K = z; P.cx = w.camX; P.cy = w.camY; P.fx = W / 2; P.fy = H / 2;
    int builds = 0;
    for (int cy = World::floorDiv(ty0, kChunk); cy <= World::floorDiv(ty1, kChunk); cy++)
        for (int cx = World::floorDiv(tx0, kChunk); cx <= World::floorDiv(tx1, kChunk); cx++) {
            ChunkMesh& m = meshes_[World::key(cx, cy)];
            const uint32_t ver = w.chunk(cx, cy).version;
            if (m.id < 0 || m.version != ver || m.seed != w.seed()) {
                if (builds >= 8 && m.id >= 0) { /* an old picture of it is still right enough for this frame */ }
                else if (builds < 8) {
                    if (m.id >= 0) r.meshDelete(m.id);
                    buildChunk(w, cx, cy);
                    m.id = r.meshCreate(build_); m.count = static_cast<int>(build_.size()); m.version = ver; m.seed = w.seed();
                    builds++;
                }
            }
            m.lastFrame = frame_;
            if (m.id >= 0) { r.drawMesh(m.id, 0, m.count, P); stats_.chunks++; }
            else r.rect(sx(cx * kChunk), sy(cy * kChunk), kChunk * z, kChunk * z, shade(terrainColour(Terrain::Grass), 0.6f));
        }
    stats_.meshesBuilt = builds;
    if (meshes_.size() > 160)                                           // forget the ground far from the camera
        for (auto it = meshes_.begin(); it != meshes_.end();) {
            if (frame_ - it->second.lastFrame > 120) { r.meshDelete(it->second.id); it = meshes_.erase(it); } else ++it;
        }
    stats_.meshesAlive = static_cast<int>(meshes_.size());

    const auto visible = [&](const Entity& e) { const int s = footprint(e.kind); return e.x + s + 2 >= tx0 && e.x - 2 <= tx1 && e.y + s + 2 >= ty0 && e.y - 2 <= ty1; };
    const bool detail = z >= 12;
    const float lw = static_cast<float>(std::max(1.0, z * 0.05));

    // ---- belts and their items ----
    for (std::vector<float>& v : itemXY_) v.clear();
    const double anim = std::fmod(double(w.tick()) * kBeltSpeed / kBeltLen, 1.0);
    const Color beltBase = rgb(58, 58, 63), rail = rgb(92, 92, 100), chev = rgb(201, 162, 39);
    for (const Entity& e : w.entities()) {
        if (e.kind != Kind::Belt || !visible(e)) continue;
        stats_.entitiesDrawn++;
        const double x = sx(e.x), y = sy(e.y), cxp = x + z / 2, cyp = y + z / 2;
        const int d = e.dir & 3;
        r.rect(x + z * 0.06, y + z * 0.06, z * 0.88, z * 0.88, beltBase);
        // where items come in: behind, or from the side for a corner
        const double inX = e.shape == 0 ? -DX[d] : e.shape < 0 ? DY[d] : -DY[d];
        const double inY = e.shape == 0 ? -DY[d] : e.shape < 0 ? -DX[d] : DX[d];
        const auto at = [&](double t, double& px, double& py) {          // the path: the middle of the entry edge, the tile's middle, the middle of the exit edge
            const double ex = cxp + inX * z / 2, ey = cyp + inY * z / 2, xx = cxp + DX[d] * z / 2, xy = cyp + DY[d] * z / 2;
            const double a = (1 - t) * (1 - t), b = 2 * t * (1 - t), c = t * t;
            px = a * ex + b * cxp + c * xx; py = a * ey + b * cyp + c * xy;
        };
        if (detail) {
            // rails along the two sides the items do not cross
            for (int s = 0; s < 4; s++) {
                const bool entry = DX[s] == inX && DY[s] == inY, exit = s == d;
                if (entry || exit) continue;
                const double rx = cxp + DX[s] * z * 0.42, ry = cyp + DY[s] * z * 0.42;
                r.line(rx - DY[s] * z * 0.44, ry - DX[s] * z * 0.44, rx + DY[s] * z * 0.44, ry + DX[s] * z * 0.44, lw * 1.6, rail);
            }
            for (int k = 0; k < 2; k++) {                                // two chevrons moving along the path
                const double t = std::fmod(anim + k * 0.5, 1.0);
                double px, py, qx, qy;
                at(t, px, py); at(std::min(1.0, t + 0.05), qx, qy);
                double fx = qx - px, fy = qy - py; const double l = std::hypot(fx, fy); if (l < 1e-9) continue; fx /= l; fy /= l;
                const double s = z * 0.16;
                r.line(px - fx * s - fy * s, py - fy * s + fx * s, px, py, lw, chev.withAlpha(0.55f));
                r.line(px - fx * s + fy * s, py - fy * s - fx * s, px, py, lw, chev.withAlpha(0.55f));
            }
        }
        for (int k = 0; k < e.n; k++) {
            double px, py;
            at(e.items[k].pos / double(kBeltLen), px, py);
            std::vector<float>& v = itemXY_[e.items[k].item];
            v.push_back(static_cast<float>(px)); v.push_back(static_cast<float>(py));
            stats_.items++;
        }
    }
    for (int i = 1; i < kItemCount; i++) {
        if (itemXY_[i].empty()) continue;
        r.squares(itemXY_[i].data(), static_cast<int>(itemXY_[i].size() / 2), z * 0.34 + 1.5, rgb(20, 20, 20, 0.55f));   // a dark rim
        r.squares(itemXY_[i].data(), static_cast<int>(itemXY_[i].size() / 2), z * 0.30, itemColour(static_cast<Item>(i)));
    }

    // ---- buildings: soft shadow first (light from the top left), then the body: lit top, darker front face ----
    for (const Entity& e : w.entities()) {
        if (e.kind == Kind::Belt || !visible(e)) continue;
        const double s = footprint(e.kind), hgt = heightOf(e.kind);
        const double x = sx(e.x), y = sy(e.y), in = e.kind == Kind::Pole ? z * 0.36 : z * 0.08;
        const double bw = s * z - 2 * in;
        const double off = hgt * z * 0.35;
        for (int k = 0; k < 3; k++) {
            const double g = (k + 1) * z * 0.06;
            r.rect(x + in + off * 0.6 - g + z * 0.05, y + in + off * 0.6 - g + z * 0.05, bw + 2 * g, bw + 2 * g, rgb(0, 0, 0, 0.09f));
        }
        stats_.entitiesDrawn++;
    }
    for (const Entity& e : w.entities()) {
        if (e.kind == Kind::Belt || !visible(e)) continue;
        const double s = footprint(e.kind);
        const double x = sx(e.x), y = sy(e.y), in = e.kind == Kind::Pole ? z * 0.36 : z * 0.08;
        const double bw = s * z - 2 * in, front = std::min(bw * 0.28, heightOf(e.kind) * z * 0.22);
        const Color base = kindColour(e.kind);
        r.rect(x + in, y + in, bw, bw, shade(base, 0.62f));                       // the front face (the lower part, in shade)
        r.rect(x + in, y + in, bw, bw - front, base);                             // the top
        r.rect(x + in, y + in, bw, std::max(1.0, z * 0.05), shade(base, 1.3f));  // the lit top edge
        r.rect(x + in, y + in, std::max(1.0, z * 0.05), bw - front, shade(base, 1.15f));
        const double cx = x + s * z / 2, cy = y + (s * z - front) / 2;
        const bool hasDir = e.kind == Kind::Miner || e.kind == Kind::Furnace || e.kind == Kind::Chest;
        switch (e.kind) {
        case Kind::Core:
            r.disc(cx, cy, z * 0.85, shade(base, 0.8f));
            r.ring(cx, cy, z * 0.85, std::max(1.0, z * 0.08), Color{theme.accent.r, theme.accent.g, theme.accent.b, 1});
            r.disc(cx, cy, z * 0.35, rgb(170, 220, 255));
            break;
        case Kind::Miner: {
            r.disc(cx, cy, z * 0.55, shade(base, 0.75f));
            const double a = L.clock * (e.progress > 0 ? 4.0 : 0.0);
            r.line(cx - std::cos(a) * z * 0.5, cy - std::sin(a) * z * 0.5, cx + std::cos(a) * z * 0.5, cy + std::sin(a) * z * 0.5, lw * 2, shade(base, 0.45f));
            if (e.net < 0 || (e.net < int(w.nets().size()) && w.nets()[std::size_t(e.net)].satPermille == 0)) r.disc(x + in + z * 0.2, y + in + z * 0.2, z * 0.14, rgb(230, 60, 50));   // no power
            break;
        }
        case Kind::Furnace:
            r.rect(cx - z * 0.4, cy - z * 0.25, z * 0.8, z * 0.5, rgb(40, 35, 32));
            if (e.burn > 0) r.fadeDisc(cx, cy, z * 0.55, rgb(255, 140, 40, 0.85f));
            break;
        case Kind::Chest:
            r.rect(x + in, cy - z * 0.04, bw, std::max(1.0, z * 0.06), shade(base, 0.6f));
            break;
        case Kind::Generator:
            r.disc(cx - z * 0.4, cy, z * 0.3, shade(base, 0.7f));
            r.disc(cx + z * 0.4, cy, z * 0.3, shade(base, 0.7f));
            if (e.burnJ > 0 || e.fuel > 0) r.disc(cx, cy, z * 0.14, rgb(255, 214, 70));
            break;
        case Kind::Pole:
            r.rect(x + z * 0.2, y + z * 0.25, z * 0.6, std::max(1.0, z * 0.08), shade(base, 0.8f));   // the cross-arm
            break;
        default: break;
        }
        if (hasDir && detail) {                                                   // the output: a small arrow on the front edge
            int ox, oy;
            const int d = e.dir & 3, a = s == 3 ? 1 : 0;
            switch (d) { case 0: ox = e.x + a; oy = e.y - 1; break; case 1: ox = e.x + int(s); oy = e.y + a; break; case 2: ox = e.x + int(s) - 1 - a; oy = e.y + int(s); break; default: ox = e.x - 1; oy = e.y + int(s) - 1 - a; }
            const double ax = sx(ox + 0.5) - DX[d] * z * 0.62, ay = sy(oy + 0.5) - DY[d] * z * 0.62, q = z * 0.16;
            r.triangle(ax + DX[d] * q, ay + DY[d] * q, rgb(250, 250, 250), ax - DX[d] * q - DY[d] * q, ay - DY[d] * q + DX[d] * q, rgb(250, 250, 250), ax - DX[d] * q + DY[d] * q, ay - DY[d] * q - DX[d] * q, rgb(250, 250, 250));
        }
    }
    // ---- power wires between poles ----
    poles_.clear();
    for (int i = 0; i < int(w.entities().size()); i++) if (w.entities()[std::size_t(i)].kind == Kind::Pole) poles_.push_back(i);
    for (std::size_t a = 0; a < poles_.size(); a++)
        for (std::size_t b = a + 1; b < poles_.size(); b++) {
            const Entity &p = w.entities()[std::size_t(poles_[a])], &q = w.entities()[std::size_t(poles_[b])];
            const double dx = p.x - q.x, dy = p.y - q.y;
            if (dx * dx + dy * dy > kPoleReach * kPoleReach || (!visible(p) && !visible(q))) continue;
            r.line(sx(p.x + 0.5), sy(p.y + 0.3), sx(q.x + 0.5), sy(q.y + 0.3), std::max(1.0, z * 0.04), rgb(40, 30, 20, 0.8f));
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

    // ---- the building being placed, under the pointer ----
    if (L.tool != Kind::None && L.hoverOn) {
        int ax, ay;
        L.anchor(L.tool, L.hoverX, L.hoverY, ax, ay);
        const bool ok = w.canPlace(L.tool, ax, ay);
        const double s = footprint(L.tool);
        if (L.tool == Kind::Pole) {
            const int rr = static_cast<int>(std::floor(kPoleSupply));
            r.rect(sx(ax - rr), sy(ay - rr), (2 * rr + 1) * z, (2 * rr + 1) * z, rgb(90, 160, 255, 0.12f));
        }
        r.rect(sx(ax), sy(ay), s * z, s * z, ok ? rgb(80, 220, 120, 0.38f) : rgb(235, 70, 60, 0.38f));
        if (L.tool != Kind::Pole && L.tool != Kind::Generator) {
            const int d = L.dir & 3;
            const double cx = sx(ax + s / 2), cy = sy(ay + s / 2), q = z * 0.3 * std::min(2.0, s);
            r.triangle(cx + DX[d] * q, cy + DY[d] * q, rgb(255, 255, 255, 0.9f), cx - DX[d] * q * 0.6 - DY[d] * q * 0.7, cy - DY[d] * q * 0.6 + DX[d] * q * 0.7, rgb(255, 255, 255, 0.9f),
                       cx - DX[d] * q * 0.6 + DY[d] * q * 0.7, cy - DY[d] * q * 0.6 - DX[d] * q * 0.7, rgb(255, 255, 255, 0.9f));
        }
    }
    // the core's name, when there is room for it
    if (text && z >= 16) {
        const Entity& c = w.core();
        const char* name = "Core";
        const ImVec2 ts = ImGui::CalcTextSize(name);
        text->AddText(ImVec2(static_cast<float>(sx(c.x + 1.5) * uiScale - ts.x / 2), static_cast<float>(sy(c.y + 3.1) * uiScale)), IM_COL32(255, 255, 255, 230), name);
    }
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace defense
