#include "chart_draw.h"
#include "ame.h"
#include "chart_view.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace atom {

namespace {

bool g_legendInPicture = true;                       // (stage 5f) see chart_draw.h
int g_legendDrawn = -1;

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
                              int selZ, int selN, bool landscape, ChartHeightMode mode, bool dry = false) {
    // the status line in the top margin: which nuclide is chosen (so it is never lost) and what the mouse and keys do. The details of a hovered square are a tooltip (atom_view.cpp).
    if (!dry) {
        const std::vector<std::string> info = chartHover(selZ, selN);
        const std::string name = info.empty() ? std::string() : info[0].substr(0, info[0].find("  ·  "));
        T.draw("Chosen: " + name + "   ·   hover a square for details, click to choose it, arrow keys walk one proton or neutron" + (landscape ? "   ·   wheel zooms, drag pans" : ""), 112, L.top - 6, 11, C.fg, 0, false);
    }

    if (!g_legendInPicture) { if (!dry) g_legendDrawn = 0; return {}; }       // (stage 5f: the new UI lists the legend in its Legend chip)
    if (!dry) g_legendDrawn = 1;
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
        if (landscape) for (const std::string& r : wrapText(T, full ? chartHeightWords(mode) : chartHeightShort(mode), lw - 12, ls)) magic.push_back(r);
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


// The notes on the chart itself (0.11 stage 4): the valley's name at the median stable tile, a mark on the heaviest known nuclides (where the measured chart ends) and, in the binding landscape, the
// peak of the data. Each label goes where it covers no tile (or block) and not the legend, found by trying a few places; if none is free the label is left out (the legend says the same in words).
struct Notes {
    std::function<bool(int, int, double&, double&)> centre;                 // screen place of a tile (its top face's middle)
    std::function<bool(double, double, double, double)> covers;             // does this rectangle cover a tile / block?
    std::function<void(int, int, Color)> ring;                              // a ring round a tile
};
// The notes box: what the valley is (measured) and what the island is (a prediction, not drawn). It is a separate box from the legend, in the corner with the most room that the legend does not
// take; full wording if it fits, else the short lines, else (a tiny picture) nothing: the labels on the chart say the same in short.
LegendBox drawNotesBox(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, const ChartLayout& L, const LegendBox& lg, const std::function<double(bool, double, double)>& room, bool dry) {
    if (!g_legendInPicture) return {};                                       // (stage 5f: the notes are in the Legend chip too)
    const double nw = std::min(w < 700 ? 250.0 : 380.0, 0.45 * w), ls = 10, llh = 12;
    const double xL = L.left + 8, xR = w - L.right - nw;
    struct Cand { bool top, right; double x, free; };
    Cand cand[4] = {{false, true, xR, room(false, xR, xR + nw)}, {true, false, xL, room(true, xL, xL + nw)}, {false, false, xL, room(false, xL, xL + nw)}, {true, true, xR, room(true, xR, xR + nw)}};
    for (int tier = 0; tier < 2; tier++) {
        std::vector<std::string> rows;
        for (const std::string& r : wrapText(T, tier == 0 ? chartValleyWords() : chartValleyShort(), nw - 12, ls)) rows.push_back(r);
        const std::size_t valleyRows = rows.size();
        for (const std::string& r : wrapText(T, tier == 0 ? chartIslandWords() : chartIslandShort(), nw - 12, ls)) rows.push_back(r);
        const double total = 12 + rows.size() * llh + 6;
        int best = -1;
        for (int i = 0; i < 4; i++) {
            const double y = cand[i].top ? L.top + 2 : h - L.bottom - total - 2;
            const bool hitsLegend = cand[i].x < lg.x + lg.w && cand[i].x + nw > lg.x && y < lg.y + lg.h && y + total > lg.y;
            if (hitsLegend || total > cand[i].free - 4) continue;
            if (best < 0 || cand[i].free > cand[best].free) best = i;
        }
        if (best < 0) continue;
        const double x = cand[best].x, y = cand[best].top ? L.top + 2 : h - L.bottom - total - 2;
        if (dry) return {x, y, nw, total};
        R.rect(x, y, nw, total, C.card.withAlpha(0.88f)); box(R, x, y, nw, total, 1, C.line.withAlpha(0.8f));
        for (std::size_t k = 0; k < rows.size(); k++) T.draw(rows[k], x + 6, y + 6 + (k + 1) * llh - 3 + (k >= valleyRows ? 4 : 0), ls, k < valleyRows ? C.fg : C.muted, 0, false);
        return {x, y, nw, total};
    }
    return {};                                                    // no room: no box (w and h are 0)
}

struct AvoidRect { double x0, y0, x1, y1; };
void drawNotes(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChartLayout& L, double w, double h, const LegendBox& lg, const LegendBox& nb, const Notes& N, bool bindingPeak,
               const std::vector<AvoidRect>& avoid = {}) {
    struct Rect { double x0, y0, x1, y1; };
    std::vector<Rect> taken;
    for (const AvoidRect& a : avoid) taken.push_back({a.x0, a.y0, a.x1, a.y1});
    const auto free = [&](const Rect& r) {
        if (r.x0 < 4 || r.x1 > w - 4 || r.y0 < L.top || r.y1 > h - L.bottom) return false;
        if (r.x0 < lg.x + lg.w && r.x1 > lg.x && r.y0 < lg.y + lg.h && r.y1 > lg.y) return false;
        if (nb.w > 0 && r.x0 < nb.x + nb.w && r.x1 > nb.x && r.y0 < nb.y + nb.h && r.y1 > nb.y) return false;
        for (const Rect& t : taken) if (r.x0 < t.x1 && r.x1 > t.x0 && r.y0 < t.y1 && r.y1 > t.y0) return false;
        return !N.covers(r.x0, r.y0, r.x1, r.y1);
    };
    // label: draws it at the first free place among the offsets (dx, dy of the label's middle from (ax, ay)); the leader line goes from the tile to the label's nearest edge
    const auto label = [&](const std::string& text, double ax, double ay, const std::vector<std::pair<double, double>>& offsets, Color c) {
        const double tw = T.width(text, 11, true);
        for (const auto& o : offsets) {
            const double cx = ax + o.first, cy = ay + o.second;
            const Rect r{cx - tw / 2 - 3, cy - 11, cx + tw / 2 + 3, cy + 4};
            if (!free(r)) continue;
            taken.push_back(r);
            const double nx = std::max(r.x0, std::min(ax, r.x1)), ny = std::max(r.y0, std::min(ay, r.y1));
            R.line(ax, ay, nx, ny, 1, c.withAlpha(0.8f));
            T.draw(text, cx, cy, 11, c, 0.5, true);
            return;
        }
    };
    const ChartTile v = chartValleyAnchor();
    double ax, ay;
    if (N.centre(v.Z, v.N, ax, ay)) {
        R.rect(ax - 2, ay - 2, 4, 4, C.fg.withAlpha(0.85f));
        std::vector<std::pair<double, double>> off;                     // up-left and down-right of the tile, then further out: the first place that is free is used
        for (const double r : {150.0, 230.0, 310.0, 400.0, 500.0}) for (const auto& d : {std::pair<double, double>{-1, -0.7}, {1, 0.7}, {-1, -0.3}, {1, 0.3}, {-0.6, -1}, {0.6, 1}}) off.push_back({d.first * r, d.second * r});
        label("valley of stability (measured)", ax, ay, off, C.fg);
    }
    double sx = 0, sy = 0; int n = 0;
    for (const ChartTile& t : chartHeaviestTiles()) if (N.centre(t.Z, t.N, ax, ay)) { N.ring(t.Z, t.N, C.fg); sx += ax; sy += ay; n++; }
    if (n) {
        const std::string text = chartHeaviestLabel();
        const double tw = T.width(text, 11, true), d = tw / 2 + 18;
        std::vector<std::pair<double, double>> off;                     // to the left of the end of the chart, nearer first, then further out and lower
        for (const double k : {0.0, 40.0, 90.0, 150.0, 230.0, 330.0}) for (const double dy : {6.0, 36.0, 66.0, 96.0, 126.0}) off.push_back({-d - k, dy});
        label(text, sx / n, sy / n, off, C.fg);
    }
    if (bindingPeak) {
        const AmeRow& pk = ameHighest();
        if (N.centre(pk.Z, pk.N, ax, ay)) {
            N.ring(pk.Z, pk.N, C.accent);
            const std::string text = chartBindingPeakLabel();
            const double tw = T.width(text, 11, true);
            std::vector<std::pair<double, double>> off;
            for (const double dy : {-46.0, -80.0, -114.0, -160.0, 44.0, 80.0, 120.0}) for (const double dx : {0.0, -tw / 2 - 10, tw / 2 + 10, -tw / 2 - 80, tw / 2 + 80}) off.push_back({dx, dy});
            label(text, ax, ay, off, C.accent);
        }
    }
}


// ---------------------------------------------------------------- the decay chain (0.12 stage 1)
// How a way is drawn (the rule, also in the panel's words): the LIKELIEST way at each nuclide is solid and thick; the other ways whose measured share is at least 0.1 % are thinner; the rarer ones
// (fission, double beta, cluster decays: below 0.1 %) are very faint hairlines; where NUBASE2020 marks the odds as not measured no way is likeliest and each is drawn thin, its label ending in " ?".
// The colour is the kind of the mode (alpha yellow, beta minus blue, beta plus red, ...), the same as the tiles. Every line runs from the middle of one square to the middle of the next (a block's top
// face in the landscape), with a small arrowhead short of the end. The walker is a disc in the accent colour; the squares it has been through are small discs, and the path it took is drawn over the trail.
// Cost: a few dozen to about 120 segments (plus labels), computed from a graph built once per chosen nuclide: no per-frame search (the landscape finds a block by a lookup).
using CentreFn = std::function<bool(int, int, double&, double&)>;
void drawChainOverlay(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChainOverlay& ov, double cell, const Color* col, const CentreFn& centre) {
    if (!ov.graph || ov.graph->nodes.empty()) return;
    const ChainGraph& g = *ov.graph;
    const ChainNode& start = g.nodes[0];
    if (start.stop == ChainStop::Stable) return;                         // a stable nuclide has no chain
    struct Seg { double x0, y0, x1, y1; const ChainEdge* e; const ChainNode* from; };
    std::vector<Seg> segs;
    for (const ChainNode& a : g.nodes) {
        double x0, y0;
        if (!centre(a.Z, a.N, x0, y0)) continue;
        for (const ChainEdge& e : a.edges) {
            if (e.end != ChainEnd::Node) continue;
            const ChainNode& b = g.nodes[std::size_t(e.to)];
            double x1, y1;
            if (centre(b.Z, b.N, x1, y1)) segs.push_back({x0, y0, x1, y1, &e, &a});
        }
    }
    const auto style = [](const ChainEdge& e, double& wd, float& al) {      // thick and solid / thinner / faint hairline / thin (odds not measured)
        if (e.likeliest) { wd = 4.2; al = 1.0f; }
        else if (!e.measured) { wd = 1.8; al = 0.95f; }
        else if (e.p >= 1e-3) { wd = 1.8; al = 0.9f; }
        else { wd = 1.0; al = 0.45f; }
    };
    std::stable_sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.e->likeliest < b.e->likeliest; });   // the likeliest on top
    const double head = std::max(5.0, std::min(9.0, 0.9 * cell));
    for (const Seg& sg : segs) {
        double wd; float al; style(*sg.e, wd, al);
        const Color c = col[int(chartClassOfMode(sg.e->mode))];
        if (sg.e->likeliest || !sg.e->measured || sg.e->p >= 1e-3) R.line(sg.x0, sg.y0, sg.x1, sg.y1, wd + 3, C.card.withAlpha(0.85f));   // a pale outline so the line shows over any tile colour, a blue beta-minus line over blue tiles too (not for the faint hairlines: it would show as a pale band)
        R.line(sg.x0, sg.y0, sg.x1, sg.y1, wd, c.withAlpha(al));
        const double len = std::hypot(sg.x1 - sg.x0, sg.y1 - sg.y0);
        if (len > 2.2 * head) {                                            // an arrowhead short of the destination, so the walker's disc does not hide it
            const double ux = (sg.x1 - sg.x0) / len, uy = (sg.y1 - sg.y0) / len, tx = sg.x1 - ux * 0.55 * cell, ty = sg.y1 - uy * 0.55 * cell;
            for (const double side : {-1.0, 1.0}) { const double ca = std::cos(0.45), sa = side * std::sin(0.45); R.line(tx, ty, tx - head * (ux * ca - uy * sa), ty - head * (uy * ca + ux * sa), wd, c.withAlpha(al)); }
        }
    }
    // a split (fission) that is likeliest or at least 0.1 %: three short rays from the square, since it has no single daughter
    for (const ChainNode& a : g.nodes) {
        double x, y;
        if (!centre(a.Z, a.N, x, y)) continue;
        for (const ChainEdge& e : a.edges) if (e.end == ChainEnd::Fission && (e.likeliest || e.p >= 1e-3)) {
            const Color c = col[int(chartClassOfMode(e.mode))];
            for (int k = 0; k < 3; k++) { const double an = -1.57 + k * 2.09, r0 = std::min(0.6 * cell, 16.0), r1 = r0 + 9; R.line(x + r0 * std::cos(an), y + r0 * std::sin(an), x + r1 * std::cos(an), y + r1 * std::sin(an), 2, c); }
        }
    }
    // mode labels at the middle of the likeliest steps (and of the other measured ways of at least 1 %), where they do not run into a label already placed
    {
        struct Rc { double x0, y0, x1, y1; };
        std::vector<Rc> placed;
        for (const Seg& sg : segs) {
            if (!(sg.e->likeliest || !sg.e->measured || sg.e->p >= 0.01)) continue;
            const double len = std::hypot(sg.x1 - sg.x0, sg.y1 - sg.y0);
            if (len < 6) continue;
            const std::string t = chainModeShort(sg.e->mode) + (sg.e->measured ? "" : " ?");                 // a way whose odds are not measured says so on its label
            const double tw = T.width(t, 10, true), mx = (sg.x0 + sg.x1) / 2, my = (sg.y0 + sg.y1) / 2, nx = -(sg.y1 - sg.y0) / len, ny = (sg.x1 - sg.x0) / len;
            for (const double side : {1.0, -1.0}) {
                const double cx = mx + side * nx * (5 + tw / 2), cy = my + side * ny * 8 + 3;
                const Rc r{cx - tw / 2 - 2, cy - 10, cx + tw / 2 + 2, cy + 3};
                bool clash = false;
                for (const Rc& q : placed) if (r.x0 < q.x1 && r.x1 > q.x0 && r.y0 < q.y1 && r.y1 > q.y0) clash = true;
                if (clash) continue;
                placed.push_back(r);
                R.rect(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, C.card.withAlpha(0.8f));
                T.draw(t, cx, cy, 10, C.fg, 0.5, true);
                break;
            }
        }
    }
    // the walk: the squares it has been through, the path it took over the trail, the walker
    if (ov.walk) {
        const ChainWalk& w = *ov.walk;
        double x0, y0, x1, y1;
        for (const WalkStep& st : w.steps) {
            if (st.end != ChainEnd::Node || !centre(st.fromZ, st.fromN, x0, y0) || !centre(st.toZ, st.toN, x1, y1)) continue;
            R.line(x0, y0, x1, y1, 6.4, C.card.withAlpha(0.9f)); R.line(x0, y0, x1, y1, 4.0, C.accent);
        }
        const double rd = std::max(2.5, std::min(7.0, 0.45 * cell));            // (the discs grow with the squares only up to a size that never hides the trail)
        for (const WalkStep& st : w.steps) if (centre(st.fromZ, st.fromN, x0, y0)) R.disc(x0, y0, rd, C.accent.withAlpha(0.9f));
        if (centre(w.Z, w.N, x0, y0)) { const double r = std::max(4.0, std::min(12.0, 0.9 * cell)); R.disc(x0, y0, r + 2.2, C.card); R.disc(x0, y0, r, C.accent); }
    }
    // a cross on every nuclide where the chain stops without a time (a half-life that is only a limit, only an extrapolation or not given), over the walker's disc too, with a pale outline
    for (const ChainNode& a : g.nodes) {
        double x, y;
        if (a.stop != ChainStop::NoHalfLife || !centre(a.Z, a.N, x, y)) continue;
        const double d = std::max(3.0, std::min(8.0, 0.7 * cell));
        for (const double wd : {5.0, 2.2}) { const Color k = wd > 3 ? C.card : C.fg; R.line(x - d, y - d, x + d, y + d, wd, k); R.line(x - d, y + d, x + d, y - d, wd, k); }
    }
}

}  // namespace

namespace {
Color classColour(const ChartColours& C, int c) { const char* hex = chartClassInfo(ChartClass(c)).hex; return *hex ? render::hexColor(hex) : C.fg; }
bool sameColours(const ChartColours& a, const ChartColours& b) {
    const auto same = [](const Color& x, const Color& y) { return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a; };
    return same(a.fg, b.fg) && same(a.muted, b.muted) && same(a.accent, b.accent) && same(a.card, b.card) && same(a.line, b.line);
}
// a slot of the cache is up to date for this picture, theme and height meaning, or it is made again (its old buffer freed first)
template <class Build>
void ensureSlot(render::Renderer2D& R, ChartCache::Slot& s, double w, double h, const ChartColours& C, int mode, Build build) {
    if (s.id >= 0 && s.w == w && s.h == h && s.mode == mode && sameColours(s.colours, C)) return;
    if (s.id >= 0) R.meshDelete(s.id);
    s.mesh = build();
    s.id = R.meshCreate(s.mesh.v);
    s.w = w; s.h = h; s.mode = mode; s.colours = C;
    s.builds++;
}
}  // namespace

void ChartCache::release(render::Renderer2D& R) {
    for (Slot* s : {&flat, &land}) { if (s->id >= 0) R.meshDelete(s->id); *s = Slot(); }
}

// The static part of the flat chart, made every frame (the reference of the retained mesh, and the path with no cache)
void drawChartStaticFlat(render::Renderer2D& R, const ChartColours& C, const ChartLayout& L) {
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) col[c] = classColour(C, c);
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
}

void drawChart(render::Renderer2D& R, const ChartText& T, const ChartColours& C, double w, double h, int selZ, int selN, int hovZ, int hovN, const ChainOverlay* chain, ChartCache* cache) {
    const ChartLayout L = chartLayout(w, h);
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) col[c] = classColour(C, c);

    if (cache) {                                                       // the tiles and the magic-number rows and columns from the GPU buffer (one draw call, no vertices made or sent)
        ensureSlot(R, cache->flat, w, h, C, 0, [&] { return chartFlatMesh(L, C); });
        R.drawMesh(cache->flat.id, cache->flat.mesh.partB, cache->flat.mesh.partBCount, render::MeshProjection());   // (the default projection is the identity: the picture's own points)
    } else drawChartStaticFlat(R, C, L);

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

    if (chain) drawChainOverlay(R, T, C, *chain, L.cell, col, [&](int z, int n, double& x, double& y) { x = L.x(n) + L.cell / 2; y = L.y(z) + L.cell / 2; return true; });

    const std::function<double(bool, double, double)> roomFlat = [&](bool fromTop, double x0, double x1) { return chartFreeHeight(L, h, fromTop, x0, x1); };
    const LegendBox lgFlat = drawStatusAndLegend(R, T, C, w, h, L, roomFlat, col, selZ, selN, false, ChartHeightMode::HalfLife, true);
    Notes notes;
    notes.centre = [&](int z, int n, double& x, double& y) { x = L.x(n) + L.cell / 2; y = L.y(z) + L.cell / 2; return true; };
    notes.covers = [&](double x0, double y0, double x1, double y1) { for (const ChartTile& t : chartTiles()) { const double tx = L.x(t.N), ty = L.y(t.Z); if (tx < x1 && tx + L.cell > x0 && ty < y1 && ty + L.cell > y0) return true; } return false; };
    notes.ring = [&](int z, int n, Color c) { box(R, L.x(n) - 1.5, L.y(z) - 1.5, L.cell + 3, L.cell + 3, 1.2, c); };
    const LegendBox nbFlat = drawNotesBox(R, T, C, w, h, L, lgFlat, roomFlat, true);
    drawNotes(R, T, C, L, w, h, lgFlat, nbFlat, notes, false);
    drawStatusAndLegend(R, T, C, w, h, L, roomFlat, col, selZ, selN, false, ChartHeightMode::HalfLife);
    drawNotesBox(R, T, C, w, h, L, lgFlat, roomFlat, false);
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

// the ground of the landscape: a faint plate under the whole chart and the magic-number rows and columns on it (they run under the blocks)
void drawChartStaticGround(render::Renderer2D& R, const ChartColours& C, const ChartLayout& L, const ChartProjection& P) {
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
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
}

// the blocks, far to near (the order is fixed: see chartPaintOrder), each a top and the two faces that face the viewer
void drawChartStaticBlocks(render::Renderer2D& R, const ChartColours& C, const ChartScene& S) {
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) col[c] = classColour(C, c);
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
}

void drawChartScene(render::Renderer2D& R, const ChartText& T, const ChartColours& C, const ChartScene& S, double w, double h, int selZ, int selN, int hovZ, int hovN, const ChainOverlay* chain, ChartCache* cache) {
    const ChartLayout& L = S.layout();
    const ChartProjection& P = S.proj();
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    Color col[kChartClasses];
    for (int c = 0; c < kChartClasses; c++) col[c] = classColour(C, c);
    const render::MeshProjection mp = chartMeshProjection(P);
    if (cache) ensureSlot(R, cache->land, w, h, C, int(S.mode()), [&] { return chartLandscapeMesh(L, C, S.mode()); });

    // the ground, from the GPU buffer or made now
    if (cache) R.drawMesh(cache->land.id, cache->land.mesh.partA, cache->land.mesh.partACount, mp); else drawChartStaticGround(R, C, L, P);
    // the chosen nuclide's footprint (it follows the choice, so it is never in the mesh)
    if (selZ >= 1 && selZ <= L.zMax && selN >= 0 && selN <= L.nMax) groundOutline(R, P, L.x(selN), L.y(selZ), L.x(selN) + L.cell, L.y(selZ) + L.cell, 2, C.accent);
    // the blocks, far to near
    if (cache) R.drawMesh(cache->land.id, cache->land.mesh.partB, cache->land.mesh.partBCount, mp); else drawChartStaticBlocks(R, C, S);

    // where the legend will go (a dry run), so no axis number is drawn under it
    const std::function<double(bool, double, double)> roomFn = [&](bool fromTop, double x0, double x1) { return S.freeHeight(fromTop, x0, x1); };
    const ChartHeightMode mode = S.mode();
    const LegendBox lg = drawStatusAndLegend(R, T, C, w, h, L, roomFn, col, selZ, selN, true, mode, true);
    const LegendBox nb = drawNotesBox(R, T, C, w, h, L, lg, roomFn, true);
    const auto underLegend = [&](double x, double y) { return (x > lg.x - 14 && x < lg.x + lg.w + 14 && y > lg.y - 8 && y < lg.y + lg.h + 8) || (nb.w > 0 && x > nb.x - 14 && x < nb.x + nb.w + 14 && y > nb.y - 8 && y < nb.y + nb.h + 8); };
    std::vector<AvoidRect> axisRects;                          // where the axis numbers and titles are, so the notes keep clear of them
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
        for (int n = 0; n <= L.nMax; n += int(step)) { P.map(L.x(n) + L.cell / 2, gridB, 0, sx, sy, d); if (!underLegend(sx + nx * 12, sy + ny * 12)) { T.draw(std::to_string(n), sx + nx * 12, sy + ny * 12 + 3.5, 10, C.muted, 0.5, false); axisRects.push_back({sx + nx * 12 - 12, sy + ny * 12 - 8, sx + nx * 12 + 12, sy + ny * 12 + 8}); } }
        for (int z = int(step); z <= L.zMax; z += int(step)) { P.map(gridL, L.y(z) + L.cell / 2, 0, sx, sy, d); if (!underLegend(sx + zx * 12, sy + zy * 12)) { T.draw(std::to_string(z), sx + zx * 12, sy + zy * 12 + 3.5, 10, C.muted, 0.5, false); axisRects.push_back({sx + zx * 12 - 12, sy + zy * 12 - 8, sx + zx * 12 + 12, sy + zy * 12 + 8}); } }
        P.map((gridL + gridR) / 2, gridB, 0, sx, sy, d); if (!underLegend(sx + nx * 46, sy + ny * 46)) { T.draw("neutrons N →", sx + nx * 46, sy + ny * 46 + 4, 11, C.fg, 0.5, true); axisRects.push_back({sx + nx * 46 - 45, sy + ny * 46 - 8, sx + nx * 46 + 45, sy + ny * 46 + 8}); }
        P.map(gridL, (gridT + gridB) / 2, 0, ex, ey, ed); if (!underLegend(ex + zx * 46, ey + zy * 46)) { T.draw("↑ protons Z", ex + zx * 46, ey + zy * 46 + 4, 11, C.fg, 0.5, true); axisRects.push_back({ex + zx * 46 - 40, ey + zy * 46 - 8, ex + zx * 46 + 40, ey + zy * 46 + 8}); }
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
    if (chain) drawChainOverlay(R, T, C, *chain, L.cell * P.K, col, [&](int z, int n, double& x, double& y) { const ChartBlock* B = S.block(z, n); if (!B) return false; x = (B->top[0] + B->top[4]) / 2; y = (B->top[1] + B->top[5]) / 2; return true; });
    Notes notes;
    notes.centre = [&](int z, int n, double& x, double& y) { const ChartBlock* B = find(z, n); if (!B) return false; x = (B->top[0] + B->top[4]) / 2; y = (B->top[1] + B->top[5]) / 2; return true; };
    notes.covers = [&](double x0, double y0, double x1, double y1) { for (const ChartBlock& B : S.blocks()) if (B.x0 < x1 && B.x1 > x0 && B.y0 < y1 && B.y1 > y0) return true; return false; };
    notes.ring = [&](int z, int n, Color c) { if (const ChartBlock* B = find(z, n)) { outline(R, B->top, 3.5, C.card); outline(R, B->top, 1.8, c); } };
    drawNotes(R, T, C, L, w, h, lg, nb, notes, mode == ChartHeightMode::Binding, axisRects);
    drawStatusAndLegend(R, T, C, w, h, L, roomFn, col, selZ, selN, true, mode);
    drawNotesBox(R, T, C, w, h, L, lg, roomFn, false);
}

void setChartLegendInPicture(bool on) { g_legendInPicture = on; }
int chartLegendDrawn() { return g_legendDrawn; }

}  // namespace atom
