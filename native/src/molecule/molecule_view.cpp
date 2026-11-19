// Molecule Lab (0.18 stage 1): the picture (see molecule_view.h). Colours: CPK (PubChem periodic table, molecule.cpp); shading, outline and selection colours are drawing choices.
#include "molecule/molecule_view.h"
#include "molecule/depth_order.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "atom/cloud_light.h"
#include "molecule/mol_cloud.h"
#include "molecule/molecule_card_paths.h"

namespace molecule {
namespace {

using Blob = render::Renderer2D::Blob;

render::Color cpk(int el, float k = 1) {
    const unsigned v = element(el).rgb;
    return {std::min(1.0f, ((v >> 16) & 255) / 255.0f * k), std::min(1.0f, ((v >> 8) & 255) / 255.0f * k), std::min(1.0f, (v & 255) / 255.0f * k), 1};
}
render::Color mixWhite(render::Color c, float t) { return {c.r + (1 - c.r) * t, c.g + (1 - c.g) * t, c.b + (1 - c.b) * t, c.a}; }
Blob line(double x0, double y0, double x1, double y1, double halfW, render::Color c) {
    const double mx = (x0 + x1) / 2, my = (y0 + y1) / 2, hx = (x1 - x0) / 2, hy = (y1 - y0) / 2, r = std::max(1.0, std::hypot(hx, hy) + halfW);
    Blob b{};
    b.x = static_cast<float>(mx); b.y = static_cast<float>(my); b.r = static_cast<float>(r); b.kind = 8;
    b.fx = static_cast<float>(hx / r); b.fy = static_cast<float>(hy / r); b.f0 = static_cast<float>(halfW / r); b.s1 = 0;
    b.c0 = c; b.c1 = c; b.c2 = c;
    return b;
}

}  // namespace

void MoleculeView::draw(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& L, double W, double H, const app::ThemeColors& theme) {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = ViewStats{};
    L.viewW = W; L.viewH = H;
    const Molecule& m = L.shown();   // (0.23: the atoms where a vibration has moved them)
    const double s = L.scale(), cx = L.cx(), cy = L.cy();
    if (L.realOn()) {                                       // 0.21: the computed electron density
        drawRealistic(r, L, theme);
        if (text && L.set.labels) drawLabels(text, uiScale, m);
        stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    const bool ball = L.set.style != Style::SpaceFill;
    project(m, L.set.cam, L.set.style, cx, cy, s, proj_);
    // back to front: atoms at their centre's depth; a stick half (centre of atom A to the bond's middle) at the depth of its own middle, so a half that points
    // towards the viewer is drawn after (over) its ball and one that points away before (under) it
    items_.clear();
    for (std::size_t i = 0; i < m.atoms.size(); i++) items_.push_back({proj_[i].depth, 0, static_cast<int>(i), 0});
    if (ball)
        for (std::size_t b = 0; b < m.bonds.size(); b++) {
            const double za = proj_[m.bonds[b].a].depth, zb = proj_[m.bonds[b].b].depth, zm = (za + zb) / 2;
            items_.push_back({(za + zm) / 2, 1, static_cast<int>(b), 0});
            items_.push_back({(zb + zm) / 2, 1, static_cast<int>(b), 1});
        }
    std::stable_sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    const render::Color accent{theme.accent.r, theme.accent.g, theme.accent.b, 1};
    const render::Color outline = theme.light ? render::Color{0.12f, 0.12f, 0.14f, 0.85f} : render::Color{0, 0, 0, 0.7f};
    const double stickHW = kStickRadiusA * s;
    blobs_.clear();
    spheres_.clear();
    double ringX = 0, ringY = 0; int ringN = 0;            // 0.46.6: the middle of the aromatic ring on screen (its dashed lines go on the inside)
    for (const Bond& bd : m.bonds) if (bd.order == 4) { ringX += proj_[bd.a].x + proj_[bd.b].x; ringY += proj_[bd.a].y + proj_[bd.b].y; ringN += 2; }
    if (ringN) { ringX /= ringN; ringY /= ringN; }
    for (const Item& it : items_) {
        if (it.kind == 0 && !ball) {                       // 0.40: Space-filling: a true sphere (each pixel knows its depth, so overlapping balls meet along their real curve
            const Proj& p = proj_[it.index];               // and never jump in front of each other as the molecule turns); the outline is drawn by the sphere itself
            const int el = m.atoms[it.index].el;
            const bool chosen = L.sel.atom == it.index;
            Blob b{};
            b.x = static_cast<float>(p.x); b.y = static_cast<float>(p.y); b.r = static_cast<float>(p.r); b.kind = 9;
            b.fx = static_cast<float>(spheres_.size()); b.fy = static_cast<float>(p.depth * s); b.f0 = chosen ? 2.0f : 1.2f;
            b.c0 = chosen ? accent : outline; b.c1 = cpk(el); b.c2 = cpk(el, 0.42f);
            spheres_.push_back(b);
            if (chosen) {                                  // the clicked atom: the accent ring as before, drawn after all the balls
                Blob g{};
                g.x = b.x; g.y = b.y; g.r = static_cast<float>(p.r + 5); g.kind = 7; g.fx = static_cast<float>((p.r + 3) / (p.r + 5)); g.f0 = static_cast<float>(1.5 / (p.r + 5)); g.c0 = accent;
                blobs_.push_back(g);
            }
            stats_.atoms++;
            continue;
        }
        if (it.kind == 0) {
            const Proj& p = proj_[it.index];
            const int el = m.atoms[it.index].el;
            const bool chosen = L.sel.atom == it.index;
            Blob o{};                                  // the outline: a flat disc 1 point bigger (a white H on a light page stays visible)
            o.x = static_cast<float>(p.x); o.y = static_cast<float>(p.y); o.r = static_cast<float>(p.r + 1.2); o.kind = 3;
            o.c0 = chosen ? accent : outline;
            blobs_.push_back(o);
            Blob b{};                                  // the ball: a radial gradient lit from the upper left (a drawing choice)
            b.x = static_cast<float>(p.x); b.y = static_cast<float>(p.y); b.r = static_cast<float>(p.r); b.kind = 0;
            b.fx = -0.35f; b.fy = -0.4f; b.f0 = 0.04f; b.s1 = 0.45f;
            b.c0 = mixWhite(cpk(el), 0.6f); b.c1 = cpk(el); b.c2 = cpk(el, 0.42f);
            blobs_.push_back(b);
            if (chosen) {                                      // the clicked atom: an accent ring
                Blob g{};
                g.x = b.x; g.y = b.y; g.r = static_cast<float>(p.r + 5); g.kind = 7; g.fx = static_cast<float>((p.r + 3) / (p.r + 5)); g.f0 = static_cast<float>(1.5 / (p.r + 5)); g.c0 = accent;
                blobs_.push_back(g);
            }
            stats_.atoms++;
            continue;
        }
        const Bond& bd = m.bonds[it.index];
        const Proj& A = proj_[it.half ? bd.b : bd.a];
        const Proj& B = proj_[it.half ? bd.a : bd.b];
        const double mx = (A.x + B.x) / 2, my = (A.y + B.y) / 2, ux = B.x - A.x, uy = B.y - A.y, L2 = std::hypot(ux, uy);
        if (L2 < 1e-6) continue;                              // seen exactly end-on: the ball covers it
        // the stick leaves its ball where the bond's line crosses the ball's surface: a fraction rBall / (bond length) of the way, the same fraction on screen (orthographic)
        const double t0 = std::min(0.5, A.r / (dist(m.atoms[bd.a], m.atoms[bd.b]) * s));
        const double sx = A.x + ux * t0, sy = A.y + uy * t0;
        // 0.46.6: the side is fixed by the bond (a to b), not by which half is drawn: the two halves' highlights (and a double bond's two sticks) were on opposite
        // sides, so every bond looked striped (outside review, benzene)
        double nx = -uy / L2, ny = ux / L2;
        if (it.half) { nx = -nx; ny = -ny; }
        const int el = m.atoms[it.half ? bd.b : bd.a].el;
        const bool chosen = L.sel.bond == it.index;
        const bool arom = bd.order == 4;                      // order 1.5 (benzene's ring, ozone): one solid stick and one dashed, the usual drawing of a bond between single and double
        const int n = bd.order == 2 || arom ? 2 : bd.order == 3 ? 3 : 1;
        const double hw = n == 1 ? stickHW : stickHW * 0.62, gap = n == 2 ? stickHW * 1.3 : stickHW * 1.45;
        const double hx = nx * (nx * -0.6 + ny * -0.8 < 0 ? 1 : -1), hy = ny * (nx * -0.6 + ny * -0.8 < 0 ? 1 : -1);   // the highlight: on the side towards the light (upper left), as on the balls
        const double inSide = arom && ringN && (ringX - mx) * nx + (ringY - my) * ny < 0 ? -1 : 1;   // the dashed stick on the ring's inside
        for (int k = 0; k < n; k++) {
            const double off = n == 1 ? 0 : n == 2 ? (k ? gap : -gap) * (arom ? inSide : 1) : (k - 1) * 2 * gap;
            const double ox = nx * off, oy = ny * off;
            const auto stick = [&](double x0, double y0, double x1, double y1) {
                blobs_.push_back(line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, hw + 1.0, chosen ? accent : outline));
                blobs_.push_back(line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, hw, cpk(el, 0.8f)));
                blobs_.push_back(line(x0 + ox - hx * hw * 0.35, y0 + oy - hy * hw * 0.35, x1 + ox - hx * hw * 0.35, y1 + oy - hy * hw * 0.35, hw * 0.28, mixWhite(cpk(el), 0.3f)));
            };
            if (arom && k == 1) {                             // dashes from the middle of the bond outwards, so both halves meet in a gap
                const double len = std::hypot(sx - mx, sy - my), dash = std::max(2.0, 2.2 * hw), space = std::max(1.5, 1.6 * hw);
                if (len > 1e-6)
                    for (double d0 = space / 2; d0 < len; d0 += dash + space) {
                        const double d1 = std::min(len, d0 + dash), ux2 = (sx - mx) / len, uy2 = (sy - my) / len;
                        stick(mx + ux2 * d0, my + uy2 * d0, mx + ux2 * d1, my + uy2 * d1);
                    }
            } else stick(sx, sy, mx, my);
            stats_.sticks++;
        }
    }
    stats_.blobs = static_cast<int>(blobs_.size() + spheres_.size());
    if (!spheres_.empty()) r.spheres(spheres_.data(), static_cast<int>(spheres_.size()));   // (0.40: Space-filling: one call for the spheres, then the ring)
    if (!blobs_.empty()) r.blobs(blobs_.data(), static_cast<int>(blobs_.size()));   // ONE instanced draw call for the whole molecule
    if (text && L.set.labels) drawLabels(text, uiScale, m);   // labels: element and number, on the balls that are not hidden behind a nearer ball (text on top, ImGui's draw list)
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}


void MoleculeView::drawLabels(ImDrawList* text, double uiScale, const Molecule& m) {
    for (std::size_t i = 0; i < m.atoms.size(); i++) {
        bool hidden = false;
        for (std::size_t j = 0; j < m.atoms.size() && !hidden; j++)
            hidden = j != i && proj_[j].depth > proj_[i].depth && std::hypot(proj_[j].x - proj_[i].x, proj_[j].y - proj_[i].y) < proj_[j].r * 0.8;
        if (hidden) continue;
        char b[16];
        std::snprintf(b, sizeof b, "%s", element(m.atoms[i].el).sym);   // (0.22 stage 6: the element only, as in a textbook; the numbered name, e.g. "H1", is in the clicked atom's card)
        const ImVec2 ts = ImGui::CalcTextSize(b);
        const ImVec2 p(static_cast<float>(proj_[i].x * uiScale - ts.x / 2), static_cast<float>(proj_[i].y * uiScale - ts.y / 2));
        const unsigned v = element(m.atoms[i].el).rgb;
        const double lum = 0.2126 * ((v >> 16) & 255) + 0.7152 * ((v >> 8) & 255) + 0.0722 * (v & 255);
        const bool small = proj_[i].r * 2 < ts.x / uiScale + 2;   // too small a ball: the label sits on a dark pill
        if (small) text->AddRectFilled(ImVec2(p.x - 3, p.y - 1), ImVec2(p.x + ts.x + 3, p.y + ts.y + 1), IM_COL32(16, 18, 22, 200), 4);
        text->AddText(p, !small && lum > 140 ? IM_COL32(16, 18, 22, 255) : IM_COL32(245, 247, 250, 255), b);
        stats_.labels++;
    }
}



// ---- 0.21 stage 1: the Realistic style ----
namespace { std::uint64_t epochs = 0; }   // 0.57.1: the sets of dots made so far (RealCache::activeEpoch)
bool RealCache::ensure(const std::string& id, int nDots, int isoIdx, const Molecule& m, int vibMode, double vibS) {
    if (!data || data->id != id) {
        data = dens::get(id);
        if (data) {
            gridElectrons = dens::electronCount(*data);
            dotsId.clear(); meshId.clear();
        }
    }
    if (!data || !m.storedFrame) return false;              // (the data is in the stored frame: a molecule drawn in another frame gets no cloud rather than a wrong one)
    if (dotsId != id || dotsN != nDots) {                    // the Monte Carlo dots: fixed seed, so the same molecule always shows the same cloud
        const auto t0 = std::chrono::steady_clock::now();
        dens::sampleDots(*data, nDots, dens::kDotSeed, dots, cdf);
        dotsId = id; dotsN = nDots; dotsEpoch = ++epochs;
        dotsMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    isoIdx = std::clamp(isoIdx, 0, 3);
    if (meshId != id || meshIso != isoIdx) {                 // the surface: marching tetrahedra on ln rho at every 2nd grid point (0.5 bohr)
        const auto t0 = std::chrono::steady_clock::now();
        dens::densitySurface(*data, kRealIso[isoIdx], 2, mesh);
        potRange = std::max(1e-6, dens::potentialRange(mesh));
        meshRadius = 0;
        for (std::size_t v = 0; v + 2 < mesh.p.size(); v += 3) meshRadius = std::max(meshRadius, std::sqrt(double(mesh.p[v]) * mesh.p[v] + double(mesh.p[v + 1]) * mesh.p[v + 1] + double(mesh.p[v + 2]) * mesh.p[v + 2]));
        meshId = id; meshIso = isoIdx; meshVib = false;
        revision++;
        meshMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    if (const dens::VibDensityMode* vm = vibMode >= 0 ? dens::vibDensity(id, vibMode) : nullptr) {   // 0.24: the density vibrating
        if (vibKeyId != id || vibKeyMode != vibMode || vibKeyDots != nDots) { vibDots.assign(kVibLevels, {}); vibEpoch.assign(kVibLevels, 0); vibKeyId = id; vibKeyMode = vibMode; vibKeyDots = nDots; vibKeyS = 2; }
        // 0.57.1: meshVib in place of clearing meshId here: the still surface was made again in every frame of the swing only to be replaced (plain waste; the
        // same surfaces, colour scale and zoom as before)
        if (vibS != vibKeyS || vibKeyIso != isoIdx || !meshVib) {
            dens::blendVib(*data, *vm, vibS, vibNow, false);   // (0.57.1: without the fine cubes, which the surface does not read)
            const double keepRange = potRange;
            dens::densitySurface(vibNow, kRealIso[isoIdx], 2, mesh);
            potRange = keepRange;                                  // (the still surface's scale: the colours do not jump as the cloud moves)
            vibKeyS = vibS; vibKeyIso = isoIdx; meshVib = true;    // (back to still: the still surface is made again)
            revision++;
        }
        const int lv = std::clamp(static_cast<int>(std::lround((std::clamp(vibS, -1.0, 1.0) + 1) / 2 * (kVibLevels - 1))), 0, kVibLevels - 1);
        if (vibDots[lv].empty()) { dens::Data at; dens::blendVib(*data, *vm, -1 + 2.0 * lv / (kVibLevels - 1), at); dens::sampleDots(at, nDots, dens::kDotSeed, vibDots[lv], cdf); vibEpoch[lv] = ++epochs; }
        activeDots = &vibDots[lv]; activeEpoch = vibEpoch[lv];
    } else {
        if (vibKeyMode >= 0 || meshVib) { vibKeyMode = -1; vibKeyId.clear(); meshId.clear(); meshVib = false; return ensure(id, nDots, isoIdx, m); }   // (a swing just stopped: the still surface again)
        activeDots = &dots; activeEpoch = dotsEpoch;
    }
    return true;
}

namespace {
// the potential's colours: red = negative, white = zero, blue = positive (t = V / range, -1..1). The ends are drawing choices.
render::Color potColour(double t) {
    t = std::clamp(t, -1.0, 1.0);
    const render::Color red{0.86f, 0.16f, 0.13f, 1}, blue{0.15f, 0.36f, 0.90f, 1}, white{0.97f, 0.97f, 0.97f, 1};
    const render::Color& e = t < 0 ? red : blue;
    const float a = static_cast<float>(std::fabs(t));
    return {white.r + (e.r - white.r) * a, white.g + (e.g - white.g) * a, white.b + (e.b - white.b) * a, 1};
}
}  // namespace

render::Color realPotColour(double t) { return potColour(t); }

void MoleculeView::drawRealistic(render::Renderer2D& r, MoleculeLab& L, const app::ThemeColors& theme) {
    stats_.mode = "realistic";
    const Molecule& m = L.shown();                         // (0.24: the nuclei where a vibration has moved them)
    const double s = L.scale(), cx = L.cx(), cy = L.cy(), W = L.viewW, H = L.viewH;
    project(m, L.set.cam, Style::Realistic, cx, cy, s, proj_);
    if (!L.ensureReal()) return;
    const RealCache& rc = L.real;
    const double* R = L.set.cam.R;
    const double px = L.set.cam.panX, py = L.set.cam.panY;
    const render::Color accent{theme.accent.r, theme.accent.g, theme.accent.b, 1};
    const bool light = theme.light;
    // 1. the cloud: the Atom Lab's Cloud 3D Glow dots (atom/cloud_dots.h glowBlobs): far dots smaller and fainter, near ones larger and brighter; light adds up on a dark page
    if (L.set.realShow != 2) {
        const std::vector<float>& dots = *rc.activeDots;
        const std::size_t nd = dots.size() / 3;
        CloudCam cam;                                      // (0.57.1: the per-dot formulas in molecule/mol_cloud.h cloudDot, unchanged, shared with the card's check)
        for (int k = 0; k < 9; k++) cam.R[k] = R[k];
        cam.px = px; cam.py = py; cam.cx = cx; cam.cy = cy; cam.s = s; cam.ext = std::max(0.5, radiusA(m)); cam.w = W; cam.h = H;
        atom::DotColours dc = atom::makeDotColours(true, false, light, accent, accent);
        // 0.52.1: under the surface (Show: both) the cores at the nuclei are kept inside the displayable range (atom::glowCapExposure): water's O core was clipped
        // white and glared through the surface, as if on top of it. The cloud alone keeps the brighter exposure, which shows its thin outer parts
        cores_.clear();
        if (L.set.realShow == 0) for (std::size_t i = 0; i < m.atoms.size(); i++) cores_.push_back({proj_[i].x, proj_[i].y});
        MolCardPaths& card = molCardPaths();
        card.frame(r, dots.data(), static_cast<long>(nd));
        if (card.use(r, gpuCloud)) {
            // 0.57.1: the card makes, sums and draws every dot (render/molecule_card.h); here only the dots that can reach a nucleus (the exposure's cap)
            const long onPicture = cloudNear(dots.data(), static_cast<long>(nd), cam, light ? std::vector<std::array<double, 2>>() : cores_, near_);
            if (light) atom::darkenForLightPage(dc);              // (as glowBlobs does with darken)
            render::MolCloudDraw d;
            d.pts = dots.data(); d.cnt = static_cast<long>(nd); d.epoch = rc.activeEpoch; d.cam = cardCam(cam); d.additive = !light;
            d.colour[0] = dc.c[0].r; d.colour[1] = dc.c[0].g; d.colour[2] = dc.c[0].b; d.colour[3] = dc.c[0].a;
            d.fenceAfter = card.fenceWanted();
            const long cap = static_cast<long>(nd);
            double* expo = &expo_;
            const std::vector<std::vector<atom::GlowDot>>* nearDots = &near_;
            const std::vector<std::array<double, 2>>* at = &cores_;
            d.exposure = [expo, cap, light, nearDots, at](const std::vector<float>& g) {
                if (!atom::glowExposure(g, cap, light, 1.0, *expo)) return -1.0;
                if (!light && !at->empty()) cloudCapExposure(*nearDots, cap, *at, *expo);   // (glowCapExposure's sums, each nucleus's dots only)
                return 1.0 * *expo * atom::cloudDotWeight(cap);
            };
            if (r.molCloud(d)) { stats_.dots = static_cast<int>(onPicture); stats_.cloudCard = true; }
        } else {
            // 1. the cloud: the Atom Lab's Cloud 3D Glow dots (atom/cloud_dots.h glowBlobs): far dots smaller and fainter, near ones larger and brighter; light adds up on a dark page
            glow_.clear();
            glow_.reserve(nd);
            for (std::size_t i = 0; i < nd; i++) {
                atom::GlowDot g{};
                if (cloudDot(cam, &dots[3 * i], g)) glow_.push_back(g);
            }
            if (atom::glowBlobs(glow_, static_cast<long>(nd), light, dc, true, 1.0, W, H, expo_, blobs_, cores_.empty() ? nullptr : &cores_)) {
                stats_.dots = static_cast<int>(blobs_.size());
                r.blobsPaletteTake(blobs_, !light, dc.palette());   // ONE instanced draw call for the cloud (the buffers are swapped, not copied)
                blobs_.clear();
            }
        }
    }
    // 2. the nuclei: small bright points (CPK tint); the clicked one has an accent ring. Drawn before the surface, so they show through it
    blobs_.clear();
    for (std::size_t i = 0; i < m.atoms.size(); i++) {
        const Proj& p = proj_[i];
        Blob o{};
        o.x = static_cast<float>(p.x); o.y = static_cast<float>(p.y); o.r = 3.4f; o.kind = 3;
        o.c0 = light ? render::Color{0.10f, 0.10f, 0.12f, 0.9f} : render::Color{0, 0, 0, 0.75f};
        blobs_.push_back(o);
        Blob b = o;
        b.r = 2.4f; b.c0 = mixWhite(cpk(m.atoms[i].el), light ? 0.15f : 0.55f);
        blobs_.push_back(b);
        if (L.sel.atom == static_cast<int>(i)) {
            Blob g{};
            g.x = b.x; g.y = b.y; g.r = 9; g.kind = 7; g.fx = 6.5f / 9; g.f0 = 1.5f / 9; g.c0 = accent;
            blobs_.push_back(g);
        }
        stats_.atoms++;
    }
    stats_.blobs = static_cast<int>(blobs_.size()) + stats_.dots;
    r.blobs(blobs_.data(), static_cast<int>(blobs_.size()));
    if (L.set.realShow == 1) return;
    // 3. the density surface: translucent, lit, coloured by the potential; triangles far to near (painter's order: the renderer has no depth buffer). The order is
    // worked out again only when the view or the surface changed. Approximation: the surface as a whole is drawn over the cloud and the nuclei (they are not sorted together)
    const dens::Mesh& M = rc.mesh;
    const std::size_t nv = M.p.size() / 3, nt = M.tri.size() / 3;
    const double cam[14] = {R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7], R[8], px, py, cx, cy, s};
    bool same = lastRev_ == rc.revision && sx_.size() == nv;
    for (int i = 0; i < 14 && same; i++) same = cam[i] == lastCam_[i];
    if (!same) {
        sx_.resize(nv); sy_.resize(nv); sz_.resize(nv);
        for (std::size_t v = 0; v < nv; v++) {
            const double x = M.p[3 * v], y = M.p[3 * v + 1], z = M.p[3 * v + 2];
            sx_[v] = static_cast<float>(cx + (R[0] * x + R[1] * y + R[2] * z + px) * s);
            sy_[v] = static_cast<float>(cy - (R[3] * x + R[4] * y + R[5] * z + py) * s);
            sz_[v] = static_cast<float>(R[6] * x + R[7] * y + R[8] * z);
        }
        tz_.resize(nt);
        for (std::size_t t = 0; t < nt; t++) tz_[t] = sz_[M.tri[3 * t]] + sz_[M.tri[3 * t + 1]] + sz_[M.tri[3 * t + 2]];
        depthOrder(tz_, order_, rkey_, rkey2_, rtmp_);         // 0.66.0: a radix sort, equal depths by triangle number (molecule/depth_order.h)
        for (int i = 0; i < 14; i++) lastCam_[i] = cam[i];
        lastRev_ = rc.revision;
        stats_.resorted = true;
    }
    // colours: the vertex's base colour (potential or plain) times a light, with a little white highlight. 0.46.6 (outside review: CO2's two oxygens and water's two
    // hydrogens came out in different colours): the light comes from the viewer (it was from the upper left), so two parts that are mirror images by symmetry are
    // always lit the same, and the highlight no longer bleaches one side of the potential's colours
    vc_.resize(nv);
    const double lx = 0, ly = 0, lz = 1;                      // towards the light, view space
    const double ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    const render::Color plain = light ? render::Color{0.62f, 0.68f, 0.78f, 1} : render::Color{0.80f, 0.84f, 0.90f, 1};
    for (std::size_t v = 0; v < nv; v++) {
        const double nx = M.n[3 * v], ny = M.n[3 * v + 1], nz = M.n[3 * v + 2];
        const double vx = R[0] * nx + R[1] * ny + R[2] * nz, vy = R[3] * nx + R[4] * ny + R[5] * nz, vz = R[6] * nx + R[7] * ny + R[8] * nz;
        const double d = std::fabs(vx * lx + vy * ly + vz * lz) / ll;   // (two-sided: the inner side is lit too)
        const double hl = std::pow(std::max(0.0, d), 24) * 0.15;
        const render::Color base = L.set.realPot ? potColour(M.value[v] / rc.potRange) : plain;
        const float k = static_cast<float>(0.62 + 0.38 * d);   // (a soft light: the colours stay readable against the scale)
        vc_[v] = {std::min(1.0f, base.r * k + static_cast<float>(hl)), std::min(1.0f, base.g * k + static_cast<float>(hl)), std::min(1.0f, base.b * k + static_cast<float>(hl)), 1};
    }
    const float aFront = light ? 0.62f : 0.60f, aBack = light ? 0.15f : 0.15f;   // translucent, so the cloud shows through; the far (inner) side fainter, so the near side reads as the surface
    for (const int t : order_) {
        const int a = M.tri[3 * static_cast<std::size_t>(t)], b = M.tri[3 * static_cast<std::size_t>(t) + 1], c = M.tri[3 * static_cast<std::size_t>(t) + 2];
        const double ex = (sx_[b] - sx_[a]) * (sy_[c] - sy_[a]) - (sy_[b] - sy_[a]) * (sx_[c] - sx_[a]);   // screen y points down: < 0 means the triangle faces the viewer
        const float al = ex < 0 ? aFront : aBack;
        render::Color ca = vc_[a], cb = vc_[b], cc = vc_[c];
        ca.a = cb.a = cc.a = al;
        r.triangle(sx_[a], sy_[a], ca, sx_[b], sy_[b], cb, sx_[c], sy_[c], cc);
    }
    stats_.triangles = static_cast<int>(nt);
}

}  // namespace molecule
