#include "chart_draw.h"

#include <algorithm>
#include <cmath>

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

    // the status line in the top margin: which nuclide is chosen (so it is never lost) and what the mouse and keys do. The details of a hovered square are a tooltip (atom_view.cpp).
    {
        const std::vector<std::string> info = chartHover(selZ, selN);
        const std::string name = info.empty() ? std::string() : info[0].substr(0, info[0].find("  ·  "));
        T.draw("Chosen: " + name + "   ·   hover a square for details, click to choose it, arrow keys walk one proton or neutron", 112, L.top - 6, 11, C.fg, 0, false);
    }

    // the legend, in the corner of the picture where the chart leaves more room (top left or bottom right): every colour in words with how many tiles have it, then the magic numbers.
    // The full wording is used when it fits in that room; otherwise the colour names and a one-line magic sentence; a tiny picture may still be covered a little.
    const double lw = std::min(w < 700 ? 250.0 : 400.0, 0.5 * w), ls = 10, llh = 12;
    const double freeTL = chartFreeHeight(L, h, true, L.left + 8, L.left + 8 + lw), freeBR = chartFreeHeight(L, h, false, w - L.right - lw, w - L.right);
    const bool top = freeTL > freeBR;
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
        double total = 12 + magic.size() * llh;
        for (const Block& b : bl) total += b.rows.size() * llh + 3;
        return total;
    };
    std::vector<Block> blocks; std::vector<std::string> magic;
    double total = build(true, blocks, magic);
    if (total > std::max(freeTL, freeBR) - 4) total = build(false, blocks, magic);
    const double lx = top ? L.left + 8 : w - L.right - lw, legTop = top ? L.top + 2 : h - L.bottom - total - 2;
    R.rect(lx, legTop, lw, total, C.card.withAlpha(0.9f)); box(R, lx, legTop, lw, total, 1, C.line.withAlpha(0.8f));
    double ly = legTop + 6;
    for (const Block& b : blocks) {
        if (b.cls >= 0) R.rect(lx + 6, ly + 1, 11, 11, col[b.cls]);
        for (std::size_t k = 0; k < b.rows.size(); k++) T.draw(b.rows[k], lx + (b.cls >= 0 ? 22 : 6), ly + (k + 1) * llh - 3, ls, b.cls >= 0 ? C.fg : C.muted, 0, false);
        ly += b.rows.size() * llh + 3;
    }
    for (std::size_t k = 0; k < magic.size(); k++) T.draw(magic[k], lx + 6, ly + 3 + (k + 1) * llh - 3, ls, C.muted, 0, false);
}

}  // namespace atom
