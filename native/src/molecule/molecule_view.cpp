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
    if (L.realOn()) {                                       // 0.21: the computed electron density
        drawRealistic(r, L, theme);
        if (text && L.set.labels) drawLabels(text, uiScale, m);
        stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    const bool ball = L.set.style != Style::SpaceFill;      // (Realistic in Build mode: the estimated shape as ball-and-stick)
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
    if (text && L.set.labels) drawLabels(text, uiScale, m);   // labels: element and number, on the balls that are not hidden behind a nearer ball (text on top, ImGui's draw list)
    if (L.buildMode) drawNote(text, uiScale, L, theme);
    stats_.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}


void MoleculeView::drawLabels(ImDrawList* text, double uiScale, const Molecule& m) {
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

// ---- 0.21 stage 1: the Realistic style ----
bool RealCache::ensure(const std::string& id, int nDots, int isoIdx, const Molecule& m) {
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
        dotsId = id; dotsN = nDots;
        dotsMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    isoIdx = std::clamp(isoIdx, 0, 3);
    if (meshId != id || meshIso != isoIdx) {                 // the surface: marching tetrahedra on ln rho at every 2nd grid point (0.5 bohr)
        const auto t0 = std::chrono::steady_clock::now();
        dens::densitySurface(*data, kRealIso[isoIdx], 2, mesh);
        potRange = std::max(1e-6, dens::potentialRange(mesh));
        meshId = id; meshIso = isoIdx;
        revision++;
        meshMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
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
    const Molecule& m = L.mol();
    const double s = L.scale(), cx = L.cx(), cy = L.cy(), W = L.viewW, H = L.viewH;
    project(m, L.set.cam, Style::Realistic, cx, cy, s, proj_);
    if (!L.real.ensure(m.id, L.set.realDots, L.set.realIso, m)) return;
    const RealCache& rc = L.real;
    const double* R = L.set.cam.R;
    const double px = L.set.cam.panX, py = L.set.cam.panY;
    const render::Color accent{theme.accent.r, theme.accent.g, theme.accent.b, 1};
    const bool light = theme.light;
    // 1. the cloud: the Atom Lab's Cloud 3D Glow dots (atom/cloud_dots.h glowBlobs): far dots smaller and fainter, near ones larger and brighter; light adds up on a dark page
    if (L.set.realShow != 2) {
        const double ext = std::max(0.5, radiusA(m));
        glow_.clear();
        const std::size_t nd = rc.dots.size() / 3;
        glow_.reserve(nd);
        for (std::size_t i = 0; i < nd; i++) {
            const double x = rc.dots[3 * i], y = rc.dots[3 * i + 1], z = rc.dots[3 * i + 2];
            const double vx = R[0] * x + R[1] * y + R[2] * z + px, vy = R[3] * x + R[4] * y + R[5] * z + py, vz = R[6] * x + R[7] * y + R[8] * z;
            const double X = cx + vx * s, Y = cy - vy * s;
            if (X < 0 || X > W || Y < 0 || Y > H) continue;
            const float t = static_cast<float>(std::clamp(-vz / ext, -1.0, 1.0));   // -1 nearest, +1 farthest (as the Atom Lab's depth over the cloud's radius)
            const float near = 0.5f - 0.5f * t;
            glow_.push_back({static_cast<float>(X), static_cast<float>(Y), 4.6f * (0.8f + 0.4f * near), 0.3f + 0.9f * near, 0});
        }
        atom::DotColours dc = atom::makeDotColours(true, false, light, accent, accent);
        if (atom::glowBlobs(glow_, static_cast<long>(nd), light, dc, true, 1.0, W, H, expo_, blobs_)) {
            stats_.dots = static_cast<int>(blobs_.size());
            r.blobsPaletteTake(blobs_, !light, dc.palette());   // ONE instanced draw call for the cloud (the buffers are swapped, not copied)
            blobs_.clear();
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
        tz_.resize(nt); order_.resize(nt);
        for (std::size_t t = 0; t < nt; t++) { tz_[t] = sz_[M.tri[3 * t]] + sz_[M.tri[3 * t + 1]] + sz_[M.tri[3 * t + 2]]; order_[t] = static_cast<int>(t); }
        std::sort(order_.begin(), order_.end(), [&](int a, int b) { return tz_[a] < tz_[b]; });
        for (int i = 0; i < 14; i++) lastCam_[i] = cam[i];
        lastRev_ = rc.revision;
        stats_.resorted = true;
    }
    // colours: the vertex's base colour (potential or plain) times a light from the upper left front (a drawing choice), with a little white highlight
    vc_.resize(nv);
    const double lx = -0.38, ly = 0.50, lz = 0.78;            // towards the light, view space (normalised below)
    const double ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    const render::Color plain = light ? render::Color{0.62f, 0.68f, 0.78f, 1} : render::Color{0.80f, 0.84f, 0.90f, 1};
    for (std::size_t v = 0; v < nv; v++) {
        const double nx = M.n[3 * v], ny = M.n[3 * v + 1], nz = M.n[3 * v + 2];
        const double vx = R[0] * nx + R[1] * ny + R[2] * nz, vy = R[3] * nx + R[4] * ny + R[5] * nz, vz = R[6] * nx + R[7] * ny + R[8] * nz;
        const double d = std::fabs(vx * lx + vy * ly + vz * lz) / ll;   // (two-sided: the inner side is lit too)
        const double hl = std::pow(std::max(0.0, d), 24) * 0.35;
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
