#include "chart_draw.h"
#include "chart_view.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace atom {

namespace {

using render::Color;

std::vector<std::string> wrapText(const ChartText& T, const std::string& text, double maxW, double size, bool bold = false) {
    std::vector<std::string> lines;
    std::string cur;
    std::size_t i = 0;
    while (i <= text.size()) {
        const std::size_t sp = text.find(' ', i);
        const std::string word = text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
        const std::string next = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && T.width(next, size, bold) > maxW) { lines.push_back(cur); cur = word; } else cur = next;
        if (sp == std::string::npos) break;
        i = sp + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

void box(render::Renderer2D& R, double x, double y, double w, double h, double t, Color c) {   // an outline t points thick
    R.rect(x, y, w, t, c); R.rect(x, y + h - t, w, t, c); R.rect(x, y, t, h, c); R.rect(x + w - t, y, t, h, c);
}

// The status line in the top margin and the legend, shared by the flat chart and the landscape. room(fromTop, x0, x1): how tall a box can be in the top-left or bottom-right corner before it covers the chart.
struct LegendBox { double x = 0, y = 0, w = 0, h = 0; };
// dry = true: only work out where the legend goes (the box is returned), draw nothing
LegendBox drawStatusAndLegend(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, const ChartLayout& L, const std::function<double(bool, double, double)>& room, const Color* col,
                              int selZ, int selN, bool landscape, bool dry = false) {
    // the status line in the top margin: which nuclide is chosen (so it is never lost) and what the mouse and keys do. The details of a hovered square are a tooltip (atom_view.cpp).
    if (!dry) {
        const std::vector<std::string> info = chartHover(selZ, selN);
        const std::string name = info.empty() ? std::string() : info[0].substr(0, info[0].find("  ·  "));
        T.draw("Chosen: " + name + "   ·   hover a square for details, click to choose it, arrow keys walk one proton or neutron" + (landscape ? "   ·   wheel zooms, drag pans" : ""), 112, L.top - 6, 11, C.fg, 0, false);
    }

    // the legend, in the corner of the picture where the chart leaves more room (top left or bottom right): every colour in words with how many tiles have it, then the magic numbers (and in the
    // landscape the height). The full wording is used when it fits in that room; otherwise the colour names and one-line sentences; a tiny picture may still be covered a little.
    const double lw = std::min(w < 700 ? 250.0 : 400.0, 0.5 * w), ls = 10, llh = 12;
    const double xL = L.left + 8, xR = w - L.right - lw;
    const double freeTL = room(true, xL, xL + lw), freeBR = room(false, xR, xR + lw);
    bool top = freeTL > freeBR, right = !top;                  // the flat chart: top left or bottom right (stage 2's rule, unchanged)
    double best = std::max(freeTL, freeBR);
    if (landscape) {                                           // the landscape: any of the four corners, the one with the most room (ties: bottom right, top left, bottom left, top right)
        const double freeTR = room(true, xR, xR + lw), freeBL = room(false, xL, xL + lw);
        top = false; right = true; best = freeBR;
        if (freeTL > best) { top = true; right = false; best = freeTL; }
        if (freeBL > best) { top = false; right = false; best = freeBL; }
        if (freeTR > best) { top = true; right = true; best = freeTR; }
    }
    struct Block { std::vector<std::string> rows; int cls; };
    const auto build = [&](bool full, std::vector<Block>& bl, std::vector<std::string>& magic) {
        bl.clear();
        bl.push_back({wrapText(T, "Colour: how it decays (an empty square: no such nuclide seen)", lw - 12, ls), -1});
        for (int c = 0; c < kChartClasses; c++) {
            if (chartClassCount(ChartClass(c)) == 0) continue;
            const ChartClassInfo& ci = chartClassInfo(ChartClass(c));
            bl.push_back({wrapText(T, (full ? std::string(ci.words) : std::string(ci.name)) + "  (" + std::to_string(chartClassCount(ChartClass(c))) + ")", lw - 24, ls), c});
        }
        magic = wrapText(T, full ? chartMagicWords() : chartMagicShort(), lw - 12, ls);
        if (landscape) for (const std::string& r : wrapText(T, full ? chartHeightWords() : chartHeightShort(), lw - 12, ls)) magic.push_back(r);
        double total = 12 + magic.size() * llh;
        for (const Block& b : bl) total += b.rows.size() * llh + 3;
        return total;
    };
    std::vector<Block> blocks; std::vector<std::string> magic;
    double total = build(true, blocks, magic);
    if (total > best - 4) total = build(false, blocks, magic);
    const double lx = right ? xR : xL, legTop = top ? L.top + 2 : h - L.bottom - total - 2;
    if (dry) return {lx, legTop, lw, total};
    R.rect(lx, legTop, lw, total, C.card.withAlpha(0.9f)); box(R, lx, legTop, lw, total, 1, C.line.withAlpha(0.8f));
    double ly = legTop + 6;
    for (const Block& b : blocks) {
        if (b.cls >= 0) R.rect(lx + 6, ly + 1, 11, 11, col[b.cls]);
        for (std::size_t k = 0; k < b.rows.size(); k++) T.draw(b.rows[k], lx + (b.cls >= 0 ? 22 : 6), ly + (k + 1) * llh - 3, ls, b.cls >= 0 ? C.fg : C.muted, 0, false);
        ly += b.rows.size() * llh + 3;
    }
    for (std::size_t k = 0; k < magic.size(); k++) T.draw(magic[k], lx + 6, ly + 3 + (k + 1) * llh - 3, ls, C.muted, 0, false);
    return {lx, legTop, lw, total};
}

}  // namespace

void drawChart(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, int selZ, int selN, int hovZ, int hovN) {
    const ChartLayout L = chartLayout(w, h);
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) { const char* hex = chartClassInfo(ChartClass(c)).hex; col[c] = *hex ? render::hexColor(hex) : C.fg; }

    // the magic-number rows and columns: a faint band under the tiles, so it shows in the empty squares too, and thin edges over them
    for (int m : kChartMagic) {
        R.rect(L.x(m), gridT, L.cell, gridB - gridT, C.line.withAlpha(0.22f));
        if (m <= L.zMax) R.rect(gridL, L.y(m), gridR - gridL, L.cell, C.line.withAlpha(0.22f));
    }
    // the tiles: one rectangle each, a hair smaller than the square so neighbours stay apart when the squares are big enough
    const double gap = L.cell >= 3 ? 0.6 : 0;
    for (const ChartTile& t : chartTiles()) R.rect(L.x(t.N) + gap / 2, L.y(t.Z) + gap / 2, L.cell - gap, L.cell - gap, col[int(t.cls)]);
    for (int m : kChartMagic) {
        const double lw = 1;
        R.rect(L.x(m) - lw / 2, gridT, lw, gridB - gridT, C.muted.withAlpha(0.7f)); R.rect(L.x(m) + L.cell - lw / 2, gridT, lw, gridB - gridT, C.muted.withAlpha(0.7f));
        if (m <= L.zMax) { R.rect(gridL, L.y(m) - lw / 2, gridR - gridL, lw, C.muted.withAlpha(0.7f)); R.rect(gridL, L.y(m) + L.cell - lw / 2, gridR - gridL, lw, C.muted.withAlpha(0.7f)); }
    }

    // axis numbers and titles (every 10; every 20 when the squares are very small)
    const int step = L.cell >= 3 ? 10 : 20;
    for (int n = 0; n <= L.nMax; n += step) T.draw(std::to_string(n), L.x(n) + L.cell / 2, h - L.bottom + 12, 10, C.muted, 0.5, false);
    for (int z = step; z <= L.zMax; z += step) T.draw(std::to_string(z), L.x(0) - 4, L.y(z) + L.cell / 2 + 3.5, 10, C.muted, 1, false);
    T.draw("neutrons N →", (gridL + gridR) / 2, h - 5, 11, C.fg, 0.5, true);
    T.draw("↑ protons Z", 2, L.top - 3, 11, C.fg, 0, true);

    // the chosen nuclide: cross hairs along its row and column and a box round it, so it is found at a glance
    if (selZ >= 1 && selZ <= L.zMax && selN >= 0 && selN <= L.nMax) {
        const double cx = L.x(selN) + L.cell / 2, cy = L.y(selZ) + L.cell / 2, half = std::max(6.0, 1.5 * L.cell);
        R.rect(gridL, cy - 0.5, gridR - gridL, 1, C.accent.withAlpha(0.45f));
        R.rect(cx - 0.5, gridT, 1, gridB - gridT, C.accent.withAlpha(0.45f));
        box(R, cx - half, cy - half, 2 * half, 2 * half, 2.5, C.accent);
        box(R, cx - half - 1.5, cy - half - 1.5, 2 * half + 3, 2 * half + 3, 1, C.card);
    }
    if (hovZ >= 1 && (hovZ != selZ || hovN != selN)) box(R, L.x(hovN) - 1, L.y(hovZ) - 1, L.cell + 2, L.cell + 2, 1.5, C.fg);

    drawStatusAndLegend(R, T, C, w, h, L, [&](bool fromTop, double x0, double x1) { return chartFreeHeight(L, h, fromTop, x0, x1); }, col, selZ, selN, false);
}

// ---------------------------------------------------------------- the landscape
namespace {

Color shade(Color c, float f) {                        // a side face: darker than the top; a dark colour (stable, in the light theme) is lightened instead so the faces still differ
    const float lum = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
    if (lum > 0.45f) return {c.r * f, c.g * f, c.b * f, c.a};
    const float m = 1 - f;
    return {c.r + (1 - c.r) * m * 0.9f, c.g + (1 - c.g) * m * 0.9f, c.b + (1 - c.b) * m * 0.9f, c.a};
}
void quad(render::Renderer2D& R, const float* q, Color c) {
    R.triangle(q[0], q[1], c, q[2], q[3], c, q[4], q[5], c);
    R.triangle(q[0], q[1], c, q[4], q[5], c, q[6], q[7], c);
}
void outline(render::Renderer2D& R, const float* q, double wd, Color c) { for (int i = 0; i < 4; i++) { const int j = (i + 1) & 3; R.line(q[2 * i], q[2 * i + 1], q[2 * j], q[2 * j + 1], wd, c); } }
void groundQuad(render::Renderer2D& R, const ChartProjection& P, double a0, double b0, double a1, double b1, Color c) {
    float q[8]; double sx, sy, d;
    const double pt[4][2] = {{a0, b0}, {a1, b0}, {a1, b1}, {a0, b1}};
    for (int i = 0; i < 4; i++) { P.map(pt[i][0], pt[i][1], 0, sx, sy, d); q[2 * i] = float(sx); q[2 * i + 1] = float(sy); }
    quad(R, q, c);
}
void groundOutline(render::Renderer2D& R, const ChartProjection& P, double a0, double b0, double a1, double b1, double wd, Color c) {
    float q[8]; double sx, sy, d;
    const double pt[4][2] = {{a0, b0}, {a1, b0}, {a1, b1}, {a0, b1}};
    for (int i = 0; i < 4; i++) { P.map(pt[i][0], pt[i][1], 0, sx, sy, d); q[2 * i] = float(sx); q[2 * i + 1] = float(sy); }
    outline(R, q, wd, c);
}

}  // namespace

void drawChartScene(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChartScene& S, double w, double h, int selZ, int selN, int hovZ, int hovN) {
    const ChartLayout& L = S.layout();
    const ChartProjection& P = S.proj();
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) { const char* hex = chartClassInfo(ChartClass(c)).hex; col[c] = *hex ? render::hexColor(hex) : C.fg; }

    // the ground: a faint plate under the whole chart, the magic-number rows and columns on it (they run under the blocks), and the chosen nuclide's footprint
    groundQuad(R, P, gridL, gridT, gridR, gridB, C.line.withAlpha(0.12f));
    for (int m : kChartMagic) {
        groundQuad(R, P, L.x(m), gridT, L.x(m) + L.cell, gridB, C.line.withAlpha(0.26f));
        groundOutline(R, P, L.x(m), gridT, L.x(m) + L.cell, gridB, 1, C.muted.withAlpha(0.7f));
        if (m <= L.zMax) {
            groundQuad(R, P, gridL, L.y(m), gridR, L.y(m) + L.cell, C.line.withAlpha(0.26f));
            groundOutline(R, P, gridL, L.y(m), gridR, L.y(m) + L.cell, 1, C.muted.withAlpha(0.7f));
        }
    }
    groundOutline(R, P, gridL, gridT, gridR, gridB, 1, C.muted.withAlpha(0.8f));
    if (selZ >= 1 && selZ <= L.zMax && selN >= 0 && selN <= L.nMax) groundOutline(R, P, L.x(selN), L.y(selZ), L.x(selN) + L.cell, L.y(selZ) + L.cell, 2, C.accent);

    // the blocks, far to near (the order is fixed: see chartPaintOrder), each a top and the two faces that face the viewer
    for (const ChartBlock& B : S.blocks()) {
        const Color c = col[int(B.cls)];
        switch (B.kind) {
        case HeightKind::Measured: case HeightKind::Stable: case HeightKind::StableNoted:
            if (B.sides) { quad(R, B.faceA, shade(c, 0.58f)); quad(R, B.faceB, shade(c, 0.76f)); }
            quad(R, B.top, c);
            if (B.kind == HeightKind::StableNoted) {                         // NUBASE2020 has a limit and/or a "?" entry for it: a small mark on the top
                float m[8];
                for (int i = 0; i < 4; i++) { const int j = (i + 2) & 3; m[2 * i] = float(0.7 * B.top[2 * i] + 0.3 * B.top[2 * j]); m[2 * i + 1] = float(0.7 * B.top[2 * i + 1] + 0.3 * B.top[2 * j + 1]); }
                quad(R, m, C.card);
            }
            break;
        case HeightKind::Approx: case HeightKind::Limit:                       // pale: a limit or an approximate value
            if (B.sides) { quad(R, B.faceA, shade(c, 0.58f).withAlpha(0.5f)); quad(R, B.faceB, shade(c, 0.76f).withAlpha(0.5f)); }
            quad(R, B.top, c.withAlpha(0.5f));
            outline(R, B.top, 1, c.withAlpha(0.9f));
            break;
        case HeightKind::Extrapolated:                                          // no height: flat, pale fill and an outline
            quad(R, B.top, c.withAlpha(0.28f));
            outline(R, B.top, 1, c.withAlpha(0.9f));
            break;
        case HeightKind::Unknown:                                               // no height: flat and hollow
            outline(R, B.top, 1, c.withAlpha(0.9f));
            break;
        }
    }

    // where the legend will go (a dry run), so no axis number is drawn under it
    const std::function<double(bool, double, double)> roomFn = [&](bool fromTop, double x0, double x1) { return S.freeHeight(fromTop, x0, x1); };
    const LegendBox lg = drawStatusAndLegend(R, T, C, w, h, L, roomFn, col, selZ, selN, true, true);
    const auto underLegend = [&](double x, double y) { return x > lg.x - 14 && x < lg.x + lg.w + 14 && y > lg.y - 8 && y < lg.y + lg.h + 8; };
    // axis numbers along the two near edges of the ground, and the titles, pushed out from the edge in the direction that edge's outside points on the screen (so they do not run into each other when
    // the edge slants)
    {
        const double step = L.cell * P.K >= 3 ? 20 : 40;
        double sx, sy, d, ex, ey, ed;
        const auto outward = [&](double a, double b, double da, double db, double& dx, double& dy) {      // screen direction of one step (da, db) on the ground, as a unit vector
            double x0, y0, d0, x1, y1, d1; P.map(a, b, 0, x0, y0, d0); P.map(a + da, b + db, 0, x1, y1, d1);
            const double l = std::hypot(x1 - x0, y1 - y0); dx = l > 0 ? (x1 - x0) / l : 0; dy = l > 0 ? (y1 - y0) / l : 1;
        };
        double nx, ny, zx, zy;
        outward(gridL, gridB, 0, 1, nx, ny);                  // beyond the bottom edge of the flat chart (smaller Z)
        outward(gridL, gridB, -1, 0, zx, zy);                 // beyond the left edge (smaller N)
        for (int n = 0; n <= L.nMax; n += int(step)) { P.map(L.x(n) + L.cell / 2, gridB, 0, sx, sy, d); if (!underLegend(sx + nx * 12, sy + ny * 12)) T.draw(std::to_string(n), sx + nx * 12, sy + ny * 12 + 3.5, 10, C.muted, 0.5, false); }
        for (int z = int(step); z <= L.zMax; z += int(step)) { P.map(gridL, L.y(z) + L.cell / 2, 0, sx, sy, d); if (!underLegend(sx + zx * 12, sy + zy * 12)) T.draw(std::to_string(z), sx + zx * 12, sy + zy * 12 + 3.5, 10, C.muted, 0.5, false); }
        P.map((gridL + gridR) / 2, gridB, 0, sx, sy, d); if (!underLegend(sx + nx * 46, sy + ny * 46)) T.draw("neutrons N →", sx + nx * 46, sy + ny * 46 + 4, 11, C.fg, 0.5, true);
        P.map(gridL, (gridT + gridB) / 2, 0, ex, ey, ed); if (!underLegend(ex + zx * 46, ey + zy * 46)) T.draw("↑ protons Z", ex + zx * 46, ey + zy * 46 + 4, 11, C.fg, 0.5, true);
    }

    // the hovered block and the chosen one, over everything: the chosen one has a ring round its top and a pin standing on it, so it is found even behind taller blocks
    const auto find = [&](int z, int n) -> const ChartBlock* { for (const ChartBlock& B : S.blocks()) if (B.Z == z && B.N == n) return &B; return nullptr; };
    if (hovZ >= 1 && (hovZ != selZ || hovN != selN)) if (const ChartBlock* B = find(hovZ, hovN)) outline(R, B->top, 1.5, C.fg);
    if (const ChartBlock* B = find(selZ, selN)) {
        outline(R, B->top, 3.5, C.card); outline(R, B->top, 2, C.accent);
        const double cx = (B->top[0] + B->top[4]) / 2, cy = (B->top[1] + B->top[5]) / 2;
        R.line(cx, cy, cx, cy - 26, 2.5, C.card); R.line(cx, cy, cx, cy - 26, 1.5, C.accent);
        R.disc(cx, cy - 27, 3.5, C.accent);
    }
    drawStatusAndLegend(R, T, C, w, h, L, roomFn, col, selZ, selN, true);
}

}  // namespace atom
