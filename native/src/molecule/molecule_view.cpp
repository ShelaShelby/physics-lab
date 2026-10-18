// Molecule Lab (0.18 stage 1): the picture (see molecule_view.h). Colours: CPK (PubChem periodic table, molecule.cpp); shading, outline and selection colours are drawing choices.
#include "molecule/molecule_view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "imgui.h"

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
    const Molecule& m = L.mol();
    const double s = L.scale(), cx = L.cx(), cy = L.cy();
    const bool ball = L.set.style == Style::BallStick;
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
    for (const Item& it : items_) {
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
        const double nx = -uy / L2, ny = ux / L2;
        const int el = m.atoms[it.half ? bd.b : bd.a].el;
        const bool chosen = L.sel.bond == it.index;
        const int n = bd.order == 2 ? 2 : bd.order == 3 ? 3 : 1;   // aromatic (4): one stick (the panel says "aromatic")
        const double hw = n == 1 ? stickHW : stickHW * 0.62, gap = n == 2 ? stickHW * 1.3 : stickHW * 1.45;
        for (int k = 0; k < n; k++) {
            const double off = n == 1 ? 0 : n == 2 ? (k ? gap : -gap) : (k - 1) * 2 * gap;
            const double ox = nx * off, oy = ny * off;
            blobs_.push_back(line(sx + ox, sy + oy, mx + ox, my + oy, hw + 1.0, chosen ? accent : outline));
            blobs_.push_back(line(sx + ox, sy + oy, mx + ox, my + oy, hw, cpk(el, 0.8f)));
            blobs_.push_back(line(sx + ox - nx * hw * 0.35, sy + oy - ny * hw * 0.35, mx + ox - nx * hw * 0.35, my + oy - ny * hw * 0.35, hw * 0.28, mixWhite(cpk(el), 0.3f)));
            stats_.sticks++;
        }
    }
    stats_.blobs = static_cast<int>(blobs_.size());
    if (!blobs_.empty()) r.blobs(blobs_.data(), static_cast<int>(blobs_.size()));   // ONE instanced draw call for the whole molecule
    // labels: element and number, on the balls that are not hidden behind a nearer ball (text on top, ImGui's draw list)
    if (text && L.set.labels) {
        for (std::size_t i = 0; i < m.atoms.size(); i++) {
            bool hidden = false;
            for (std::size_t j = 0; j < m.atoms.size() && !hidden; j++)
                hidden = j != i && proj_[j].depth > proj_[i].depth && std::hypot(proj_[j].x - proj_[i].x, proj_[j].y - proj_[i].y) < proj_[j].r * 0.8;
            if (hidden) continue;
            char b[16];
            std::snprintf(b, sizeof b, "%s", atomLabel(m, static_cast<int>(i)).c_str());
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
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace molecule
