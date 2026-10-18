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
    L.refresh();
    if (L.sketchOn()) {                                     // 0.20: Build mode, the Sketch
        drawSketch(r, text, uiScale, L, theme);
        stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    if (L.buildMode && !L.builtOk) {                        // 0.20: Build mode, 3D, but no shape: say why
        stats_.mode = "noshape";
        drawNote(text, uiScale, L, theme);
        stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    if (L.buildMode) stats_.mode = "shape";
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
    if (L.buildMode) drawNote(text, uiScale, L, theme);
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

namespace {
// a note on a dark plate (the picture's own text: ImGui's draw list, as the atom labels); centred on (x, y) when centre, else its top left at (x, y). Points in, pixels out.
void plate(ImDrawList* dl, double uiScale, double x, double y, double maxW, const char* s, bool centre, ImU32 fg, ImU32 bg) {
    const float wrap = static_cast<float>(maxW * uiScale);
    const ImVec2 ts = ImGui::CalcTextSize(s, nullptr, false, wrap);
    const float pad = static_cast<float>(8 * uiScale);
    ImVec2 p(static_cast<float>(x * uiScale), static_cast<float>(y * uiScale));
    if (centre) { p.x -= ts.x / 2; p.y -= ts.y / 2; }
    dl->AddRectFilled(ImVec2(p.x - pad, p.y - pad * 0.6f), ImVec2(p.x + ts.x + pad, p.y + ts.y + pad * 0.6f), bg, 8 * static_cast<float>(uiScale));
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), p, fg, s, nullptr, wrap);
}
}  // namespace

void MoleculeView::drawNote(ImDrawList* text, double uiScale, MoleculeLab& L, const app::ThemeColors& c) {
    if (!text) return;
    const ImU32 fg = c.light ? IM_COL32(24, 26, 30, 255) : IM_COL32(236, 238, 242, 255), bg = c.light ? IM_COL32(255, 255, 255, 225) : IM_COL32(16, 18, 22, 215);
    const double x0 = L.freeW > 0 ? L.freeX : 0, y0 = L.freeH > 0 ? L.freeY : 0, w = L.freeW > 0 ? L.freeW : L.viewW;
    if (L.builtOk) {
        plate(text, uiScale, x0 + 20, y0 + 16, w - 40, "Estimated shape, not measured (ideal VSEPR angles, one CCCBDB bond length per bond type)", false, fg, bg);
        return;
    }
    static std::string msg;                                  // (grows once: no allocation in a normal frame)
    msg.assign("No 3D shape yet. ");
    msg.append(L.builtWhy);
    plate(text, uiScale, L.cx(), L.cy(), std::min(420.0, w - 60), msg.c_str(), true, fg, bg);
}

void MoleculeView::drawSketch(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& L, const app::ThemeColors& theme) {
    stats_.mode = "sketch";
    const build::Graph& g = L.ed.graph();
    const double S = MoleculeLab::kSketchPt, cx = L.cx(), cy = L.cy(), rad = build::kAtomRadius * S;
    const auto X = [&](double x) { return cx + x * S; };
    const auto Y = [&](double y) { return cy + y * S; };
    const render::Color ink = theme.light ? render::Color{0.20f, 0.21f, 0.24f, 1} : render::Color{0.80f, 0.82f, 0.86f, 1};
    const render::Color stubC = theme.light ? render::Color{0.45f, 0.47f, 0.52f, 1} : render::Color{0.58f, 0.61f, 0.66f, 1};
    const render::Color accent{theme.accent.r, theme.accent.g, theme.accent.b, 1};
    const render::Color outline = theme.light ? render::Color{0.12f, 0.12f, 0.14f, 0.85f} : render::Color{0, 0, 0, 0.7f};
    blobs_.clear();
    // bonds: one line, two parallel lines (double) or three (triple), from disc edge to disc edge
    for (const build::SBond& b : g.bonds) {
        const double ax = X(g.atoms[b.a].x), ay = Y(g.atoms[b.a].y), bx = X(g.atoms[b.b].x), by = Y(g.atoms[b.b].y), L2 = std::hypot(bx - ax, by - ay);
        if (L2 < 1e-6) continue;
        const double ux = (bx - ax) / L2, uy = (by - ay) / L2, nx = -uy, ny = ux, gap = 4.5;
        for (int k = 0; k < b.order; k++) {
            const double off = b.order == 1 ? 0 : b.order == 2 ? (k ? gap : -gap) : (k - 1) * 2 * gap * 0.85;
            blobs_.push_back(line(ax + ux * rad * 0.8 + nx * off, ay + uy * rad * 0.8 + ny * off, bx - ux * rad * 0.8 + nx * off, by - uy * rad * 0.8 + ny * off, 1.7, ink));
            stats_.sticks++;
        }
    }
    // open valences: short open stubs, spread into the free directions round the atom
    for (int i = 0; i < static_cast<int>(g.atoms.size()); i++) {
        const int open = build::valence(g.atoms[i].el) - build::bondOrderSum(g, i);
        if (open <= 0) continue;
        double ang[8];
        int m = 0;
        for (const build::SBond& b : g.bonds) {
            if ((b.a != i && b.b != i) || m >= 4) continue;
            const int o = b.a == i ? b.b : b.a;
            ang[m++] = std::atan2(g.atoms[o].y - g.atoms[i].y, g.atoms[o].x - g.atoms[i].x);
        }
        const double twoPi = 6.283185307179586;
        int placed = 0;
        double stub[4];
        if (m == 0) for (int k = 0; k < open && k < 4; k++) stub[placed++] = -twoPi / 4 + k * twoPi / open + (open == 4 ? twoPi / 8 : 0);
        else if (m == 1) for (int k = 1; k <= open && placed < 4; k++) stub[placed++] = ang[0] + k * twoPi / (open + 1);
        else
            for (int k = 0; k < open && placed < 4; k++) {           // into the largest gap left, one at a time
                double all[8];
                int n = 0;
                for (int j = 0; j < m; j++) all[n++] = ang[j];
                for (int j = 0; j < placed; j++) all[n++] = stub[j];
                for (int a = 1; a < n; a++) for (int b = a; b > 0 && all[b - 1] > all[b]; b--) std::swap(all[b - 1], all[b]);   // (n <= 8)
                double best = -1, at = 0;
                for (int j = 0; j < n; j++) {
                    const double a0 = all[j], a1 = j + 1 < n ? all[j + 1] : all[0] + twoPi;
                    if (a1 - a0 > best + 1e-9) { best = a1 - a0; at = (a0 + a1) / 2; }
                }
                stub[placed++] = at;
            }
        for (int k = 0; k < placed; k++) {
            const double ux = std::cos(stub[k]), uy = std::sin(stub[k]), x = X(g.atoms[i].x), y = Y(g.atoms[i].y);
            blobs_.push_back(line(x + ux * (rad + 1), y + uy * (rad + 1), x + ux * (rad + 9), y + uy * (rad + 9), 1.3, stubC));
            Blob o{};                                                  // the open end: a small ring
            o.x = static_cast<float>(x + ux * (rad + 12.5)); o.y = static_cast<float>(y + uy * (rad + 12.5)); o.r = 5; o.kind = 7; o.fx = 3.3f / 5; o.f0 = 1.1f / 5; o.c0 = stubC;
            blobs_.push_back(o);
            stats_.stubs++;
        }
    }
    // atoms: a flat CPK disc with an outline; the picked one has an accent ring
    for (int i = 0; i < static_cast<int>(g.atoms.size()); i++) {
        const double x = X(g.atoms[i].x), y = Y(g.atoms[i].y);
        Blob o{};
        o.x = static_cast<float>(x); o.y = static_cast<float>(y); o.r = static_cast<float>(rad + 1.2); o.kind = 3; o.c0 = outline;
        blobs_.push_back(o);
        Blob b = o;
        b.r = static_cast<float>(rad); b.c0 = cpk(g.atoms[i].el);
        blobs_.push_back(b);
        if (L.ed.picked == i) {
            Blob gl{};
            gl.x = b.x; gl.y = b.y; gl.r = static_cast<float>(rad + 6); gl.kind = 7; gl.fx = static_cast<float>((rad + 3.5) / (rad + 6)); gl.f0 = static_cast<float>(1.6 / (rad + 6)); gl.c0 = accent;
            blobs_.push_back(gl);
        }
        stats_.atoms++;
    }
    stats_.blobs = static_cast<int>(blobs_.size());
    if (!blobs_.empty()) r.blobs(blobs_.data(), static_cast<int>(blobs_.size()));   // ONE instanced draw call for the whole sketch
    if (!text) return;
    for (int i = 0; i < static_cast<int>(g.atoms.size()); i++) {        // the element letters
        const char* sym = element(g.atoms[i].el).sym;
        const ImVec2 ts = ImGui::CalcTextSize(sym);
        const unsigned v = element(g.atoms[i].el).rgb;
        const double lum = 0.2126 * ((v >> 16) & 255) + 0.7152 * ((v >> 8) & 255) + 0.0722 * (v & 255);
        text->AddText(ImVec2(static_cast<float>(X(g.atoms[i].x) * uiScale - ts.x / 2), static_cast<float>(Y(g.atoms[i].y) * uiScale - ts.y / 2)),
                      lum > 140 ? IM_COL32(16, 18, 22, 255) : IM_COL32(245, 247, 250, 255), sym);
        stats_.labels++;
    }
    const ImU32 fg = theme.light ? IM_COL32(24, 26, 30, 255) : IM_COL32(236, 238, 242, 255), bg = theme.light ? IM_COL32(255, 255, 255, 225) : IM_COL32(16, 18, 22, 215);
    const double w = L.freeW > 0 ? L.freeW : L.viewW, y0 = L.freeH > 0 ? L.freeY : 0;
    if (g.atoms.empty()) plate(text, uiScale, cx, cy, std::min(420.0, w - 60), "Pick an element in the panel, then click here to add an atom.", true, fg, bg);
    if (!L.ed.message.empty()) plate(text, uiScale, cx, y0 + 34, std::min(560.0, w - 60), L.ed.message.c_str(), true, fg, bg);   // the last refusal, at the top of the picture
}

}  // namespace molecule
