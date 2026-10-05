#include "chart_mesh.h"

#include <cmath>
#include <cstdlib>

namespace atom {

namespace {

using render::Color;
using render::MeshVertex;

// the colour of "#rrggbb" exactly as render::hexColor makes it (that function is in renderer2d.cpp, which atom_core does not have)
Color hexOf(const char* hex) {
    const unsigned long v = std::strtoul(hex + 1, nullptr, 16);
    return Color{static_cast<float>((v >> 16) & 255) / 255.f, static_cast<float>((v >> 8) & 255) / 255.f, static_cast<float>(v & 255) / 255.f, 1.f};
}

struct Builder {
    std::vector<MeshVertex>& v;
    void vert(double a, double b, double z, Color c, float corner = -1, const double* q = nullptr, float height = -1) {
        MeshVertex m{};
        m.p0[0] = float(a); m.p0[1] = float(b); m.p0[2] = float(z);
        if (q) { m.p1[0] = float(q[0]); m.p1[1] = float(q[1]); m.p1[2] = float(q[2]); }
        m.rgba[0] = c.r; m.rgba[1] = c.g; m.rgba[2] = c.b; m.rgba[3] = c.a;
        m.corner = corner; m.height = height;
        v.push_back(m);
    }
    // a rectangle in the ground plane at height z: (x, y), (x + w, y), (x + w, y + h) and (x, y), (x + w, y + h), (x, y + h), as Renderer2D::rect pushes it
    void rect(double x, double y, double w, double h, Color c) {
        vert(x, y, 0, c); vert(x + w, y, 0, c); vert(x + w, y + h, 0, c);
        vert(x, y, 0, c); vert(x + w, y + h, 0, c); vert(x, y + h, 0, c);
    }
    // a quadrilateral q[4][3] as two triangles (0 1 2) and (0 2 3), the way chart_draw.cpp's quad() does; height >= 0 marks a side face
    void quad(const double q[4][3], Color c, float height = -1) {
        for (int i : {0, 1, 2, 0, 2, 3}) vert(q[i][0], q[i][1], q[i][2], c, -1, nullptr, height);
    }
    // a hairline 1 point wide: six vertices in Renderer2D::line's order, P0 + n, P1 + n, P1 - n, P0 + n, P1 - n, P0 - n
    void line(const double* p0, const double* p1, Color c) {
        for (float k : {0.f, 1.f, 3.f, 0.f, 3.f, 2.f}) vert(p0[0], p0[1], p0[2], c, k, p1);
    }
    void outline(const double q[4][3], Color c) { for (int i = 0; i < 4; i++) line(q[i], q[(i + 1) & 3], c); }
};

Color shade(Color c, float f) {                        // as chart_draw.cpp: a side face is darker than the top; a dark colour is lightened instead so the faces still differ
    const float lum = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
    if (lum > 0.45f) return {c.r * f, c.g * f, c.b * f, c.a};
    const float m = 1 - f;
    return {c.r + (1 - c.r) * m * 0.9f, c.g + (1 - c.g) * m * 0.9f, c.b + (1 - c.b) * m * 0.9f, c.a};
}

void classColours(const ChartColours& C, Color* col) {
    for (int c = 0; c < kChartClasses; c++) { const char* hex = chartClassInfo(ChartClass(c)).hex; col[c] = *hex ? hexOf(hex) : C.fg; }
}
Color withA(Color c, float a) { return Color{c.r, c.g, c.b, a}; }

}  // namespace

ChartMesh chartFlatMesh(const ChartLayout& L, const ChartColours& C) {
    ChartMesh M;
    Builder B{M.v};
    Color col[kChartClasses];
    classColours(C, col);
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    for (int m : kChartMagic) {                                              // drawChart: the faint bands under the tiles
        B.rect(L.x(m), gridT, L.cell, gridB - gridT, withA(C.line, 0.22f));
        if (m <= L.zMax) B.rect(gridL, L.y(m), gridR - gridL, L.cell, withA(C.line, 0.22f));
    }
    const double gap = L.cell >= 3 ? 0.6 : 0;                                // the tiles, a hair smaller than the square
    for (const ChartTile& t : chartTiles()) B.rect(L.x(t.N) + gap / 2, L.y(t.Z) + gap / 2, L.cell - gap, L.cell - gap, col[int(t.cls)]);
    for (int m : kChartMagic) {                                              // the thin edges over them
        const double lw = 1;
        B.rect(L.x(m) - lw / 2, gridT, lw, gridB - gridT, withA(C.muted, 0.7f)); B.rect(L.x(m) + L.cell - lw / 2, gridT, lw, gridB - gridT, withA(C.muted, 0.7f));
        if (m <= L.zMax) { B.rect(gridL, L.y(m) - lw / 2, gridR - gridL, lw, withA(C.muted, 0.7f)); B.rect(gridL, L.y(m) + L.cell - lw / 2, gridR - gridL, lw, withA(C.muted, 0.7f)); }
    }
    M.partB = 0; M.partBCount = int(M.v.size());
    return M;
}

ChartMesh chartLandscapeMesh(const ChartLayout& L, const ChartColours& C, ChartHeightMode mode) {
    ChartMesh M;
    Builder B{M.v};
    Color col[kChartClasses];
    classColours(C, col);
    const double gridL = L.x(0), gridR = L.x(L.nMax) + L.cell, gridT = L.y(L.zMax), gridB = L.y(1) + L.cell;
    const auto ground = [&](double a0, double b0, double a1, double b1, Color c) { const double q[4][3] = {{a0, b0, 0}, {a1, b0, 0}, {a1, b1, 0}, {a0, b1, 0}}; B.quad(q, c); };
    const auto groundLine = [&](double a0, double b0, double a1, double b1, Color c) { const double q[4][3] = {{a0, b0, 0}, {a1, b0, 0}, {a1, b1, 0}, {a0, b1, 0}}; B.outline(q, c); };
    // part A: drawChartScene's ground (the chosen nuclide's footprint, which follows it, is drawn per frame)
    M.partA = 0;
    ground(gridL, gridT, gridR, gridB, withA(C.line, 0.12f));
    for (int m : kChartMagic) {
        ground(L.x(m), gridT, L.x(m) + L.cell, gridB, withA(C.line, 0.26f));
        groundLine(L.x(m), gridT, L.x(m) + L.cell, gridB, withA(C.muted, 0.7f));
        if (m <= L.zMax) {
            ground(gridL, L.y(m), gridR, L.y(m) + L.cell, withA(C.line, 0.26f));
            groundLine(gridL, L.y(m), gridR, L.y(m) + L.cell, withA(C.muted, 0.7f));
        }
    }
    groundLine(gridL, gridT, gridR, gridB, withA(C.muted, 0.8f));
    M.partACount = int(M.v.size());
    // part B: the blocks, far to near
    M.partB = int(M.v.size());
    const std::vector<ChartTile>& tiles = chartTiles();
    for (const int idx : chartPaintOrder()) {
        const ChartTile& t = tiles[std::size_t(idx)];
        const ChartHeight ch = chartHeightOf(t.Z, t.N, mode);
        const double a0 = L.x(t.N), a1 = a0 + L.cell, b0 = L.y(t.Z), b1 = b0 + L.cell, H = ch.h * chartHeightTiles(mode) * L.cell;    // ChartScene::heightPoints
        const double top[4][3] = {{a0, b0, H}, {a1, b0, H}, {a1, b1, H}, {a0, b1, H}};
        const double faceB[4][3] = {{a0, b1, 0}, {a1, b1, 0}, {a1, b1, H}, {a0, b1, H}}, faceA[4][3] = {{a1, b0, 0}, {a1, b1, 0}, {a1, b1, H}, {a1, b0, H}};
        const Color c = col[int(t.cls)];
        switch (ch.kind) {
        case HeightKind::Measured: case HeightKind::Stable: case HeightKind::StableNoted:
            B.quad(faceA, shade(c, 0.58f), float(H)); B.quad(faceB, shade(c, 0.76f), float(H));
            B.quad(top, c);
            if (ch.kind == HeightKind::StableNoted) {                         // a small mark on the top: 0.7 of the corner and 0.3 of the opposite one (an affine mix, so it commutes with the projection)
                double m[4][3];
                for (int i = 0; i < 4; i++) { const int j = (i + 2) & 3; for (int k = 0; k < 3; k++) m[i][k] = 0.7 * top[i][k] + 0.3 * top[j][k]; }
                B.quad(m, C.card);
            }
            break;
        case HeightKind::Approx: case HeightKind::Limit:
            B.quad(faceA, shade(c, 0.58f).withAlpha(0.5f), float(H)); B.quad(faceB, shade(c, 0.76f).withAlpha(0.5f), float(H));
            B.quad(top, c.withAlpha(0.5f));
            B.outline(top, c.withAlpha(0.9f));
            break;
        case HeightKind::Extrapolated:
            B.quad(top, c.withAlpha(0.28f));
            B.outline(top, c.withAlpha(0.9f));
            break;
        case HeightKind::Unknown:
            B.outline(top, c.withAlpha(0.9f));
            break;
        }
    }
    M.partBCount = int(M.v.size()) - M.partB;
    return M;
}

render::MeshProjection chartMeshProjection(const ChartProjection& P) {
    render::MeshProjection m;
    m.cosY = P.cosY; m.sinY = P.sinY; m.cosP = P.cosP; m.sinP = P.sinP; m.cx = P.cx; m.cy = P.cy; m.K = P.K; m.fx = P.fx; m.fy = P.fy; m.identity = P.identity; m.flat = P.flat;
    return m;
}

void chartMeshReference(const std::vector<render::MeshVertex>& v, int first, int count, const ChartProjection& P, std::vector<float>& xy, std::vector<float>& rgba, std::vector<int>* tri) {
    xy.clear(); rgba.clear();
    if (tri) tri->clear();
    const auto proj = [&](const float* p, double& sx, double& sy) { double d; P.map(p[0], p[1], p[2], sx, sy, d); };
    for (int t = 0; t + 2 < count; t += 3) {
        bool cull = false;
        double sx[3], sy[3];
        for (int k = 0; k < 3; k++) {
            const render::MeshVertex& m = v[std::size_t(first + t + k)];
            if (m.height >= 0 && (P.flat || double(m.height) * P.sinP <= 0.05)) cull = true;
            if (m.corner < 0) { proj(m.p0, sx[k], sy[k]); continue; }
            double x0, y0, x1, y1;
            proj(m.p0, x0, y0); proj(m.p1, x1, y1);
            const double dx = x1 - x0, dy = y1 - y0, len = std::hypot(dx, dy);
            if (len < 1e-9) { cull = true; sx[k] = sy[k] = 0; continue; }
            const int c = int(m.corner + 0.5);
            const double side = c >= 2 ? -1 : 1, nx = -dy / len * 0.5 * side, ny = dx / len * 0.5 * side;
            sx[k] = ((c & 1) ? x1 : x0) + nx; sy[k] = ((c & 1) ? y1 : y0) + ny;
        }
        if (cull) continue;
        if (tri) tri->push_back(first + t);
        for (int k = 0; k < 3; k++) {
            const render::MeshVertex& m = v[std::size_t(first + t + k)];
            xy.push_back(float(sx[k])); xy.push_back(float(sy[k]));
            for (int c = 0; c < 4; c++) rgba.push_back(m.rgba[c]);
        }
    }
}

}  // namespace atom
