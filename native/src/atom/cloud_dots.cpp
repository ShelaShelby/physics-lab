#include "cloud_dots.h"

#include <algorithm>
#include <cmath>

namespace atom {

using render::Color;
using Blob = render::Renderer2D::Blob;

namespace {
Color withAlpha(Color c, double a) { c.a = static_cast<float>(std::max(0.0, std::min(1.0, a)) * c.a); return c; }   // as the picture's withA: the light, clamped, times the colour's own alpha
}

// ---- colours ----
DotColours makeDotColours(bool gpu, bool wheel, bool light, Color plus, Color minus) {
    DotColours d;
    d.gpu = gpu; d.wheel = wheel; d.light = light;
    if (wheel) {
        d.n = kPhaseBins;
        for (int g = 0; g < kPhaseBins; g++) { const PhaseRgb c = phaseRgb(phaseOfBin(g), light); d.c[g] = Color{c.r, c.g, c.b, 1}; }
    } else {
        d.n = 2;
        d.c[0] = plus; d.c[1] = minus;
    }
    return d;
}

render::Renderer2D::PhasePalette DotColours::palette() const {
    render::Renderer2D::PhasePalette p;
    p.wheel = wheel; p.light = light;
    p.sign[0] = c[0]; p.sign[1] = c[1];
    return p;
}

Blob squareDot(const DotColours& dc, double x, double y, double half, int g, double alpha) {
    Blob b{};
    b.x = float(x); b.y = float(y); b.r = float(half);
    if (dc.gpu) { b.kind = render::kBlobPalSquare; b.fx = float(g); b.c0 = Color{0, 0, 0, withAlpha(dc.c[g], alpha).a}; }
    else { b.kind = 2; b.c0 = withAlpha(dc.c[g], alpha); }
    return b;
}

Blob softDot(const DotColours& dc, double x, double y, double r, int g, double alpha) {
    Blob b{};
    b.x = float(x); b.y = float(y); b.r = float(r);
    if (dc.gpu) { b.kind = render::kBlobPalSoft; b.fx = float(g); b.c0 = Color{0, 0, 0, withAlpha(dc.c[g], alpha).a}; }
    else { b.kind = 4; b.c0 = withAlpha(dc.c[g], alpha); }
    return b;
}

void plainGroups(bool wheel, const float* ph, const signed char* sg, long cnt, unsigned char* grp) {
    for (long i = 0; i < cnt; i++) grp[i] = wheel ? (unsigned char)phaseBin(ph[i]) : (unsigned char)(sg[i] > 0 ? 0 : 1);
}

// ---- the picture of the cloud ----
void projectCloud(const float* pts, long cnt, const CloudCamera& cam, double w, double h, std::vector<float>& PX, std::vector<float>& PY, std::vector<float>& PD) {
    PX.resize(std::size_t(cnt)); PY.resize(std::size_t(cnt)); PD.resize(std::size_t(cnt));
    for (long i = 0; i < cnt; i++) {
        const double x = pts[3 * i], y = pts[3 * i + 1], z = pts[3 * i + 2];
        const double x1 = x * cam.ca - y * cam.sa, y1 = x * cam.sa + y * cam.ca, dep = y1 * cam.cb - z * cam.sb, up = y1 * cam.sb + z * cam.cb;   // dep > 0: farther away
        const double f = 1 / (1 + dep / (4 * cam.ext));                    // mild perspective
        PX[i] = float(cam.cx + x1 * cam.pxm * f); PY[i] = float(cam.cy - up * cam.pxm * f); PD[i] = float(dep / cam.ext);
        if (PX[i] < 0 || PX[i] > w || PY[i] < 0 || PY[i] > h) PD[i] = NAN;   // off screen
    }
}

// ---- the order the dots are drawn in ----
void drawOrderByScan(const unsigned char* band, const unsigned char* grp, long cnt, int nBands, int nGroups, std::vector<int>& out) {
    out.clear();
    for (int b = 0; b < nBands; b++)
        for (int g = 0; g < nGroups; g++)
            for (long i = 0; i < cnt; i++)
                if (band[i] == b && grp[i] == g) out.push_back(int(i));
}

void drawOrderByCounting(const unsigned char* band, const unsigned char* grp, long cnt, int nBands, int nGroups, std::vector<int>& out) {
    const int nKeys = nBands * nGroups;
    static std::vector<int> start;                                         // where each (band, group) bucket starts in the output
    start.assign(std::size_t(nKeys) + 1, 0);
    for (long i = 0; i < cnt; i++)
        if (band[i] < nBands && grp[i] < nGroups) start[std::size_t(band[i] * nGroups + grp[i]) + 1]++;
    for (int k = 0; k < nKeys; k++) start[std::size_t(k) + 1] += start[std::size_t(k)];
    out.assign(std::size_t(start[std::size_t(nKeys)]), 0);
    for (long i = 0; i < cnt; i++)
        if (band[i] < nBands && grp[i] < nGroups) out[std::size_t(start[std::size_t(band[i] * nGroups + grp[i])]++)] = int(i);   // dots go in in dot order, so each bucket keeps it
}

void buildClassic3D(const float* PX, const float* PY, const float* PD, const unsigned char* grp, long cnt, const DotColours& dc, double weight, std::vector<Blob>& out) {
    static std::vector<unsigned char> band;
    static std::vector<int> order;
    band.resize(std::size_t(cnt));
    for (long i = 0; i < cnt; i++) band[std::size_t(i)] = depthBandOf(PD[i]);
    if (dc.gpu) drawOrderByCounting(band.data(), grp, cnt, kDepthBands, dc.n, order);
    else drawOrderByScan(band.data(), grp, cnt, kDepthBands, dc.n, order);
    out.clear();
    out.reserve(order.size());
    for (const int i : order) {
        const DepthBand& db = kDepthBand[band[std::size_t(i)]];
        out.push_back(squareDot(dc, PX[i], PY[i], db.size / 2, grp[i], db.alpha * weight));
    }
}

void sideDots(const float* pts, long cnt, const unsigned char* grp, int nGroups, double cx, double cy, double pxm, double w, double h, bool counting, std::vector<SideDot>& out) {
    out.clear();
    if (!counting) {                                                       // the v0.12 way: a pass over all dots for each colour group
        for (int g = 0; g < nGroups; g++)
            for (long i = 0; i < cnt; i++) {
                if (grp[std::size_t(i)] != g) continue;
                const double X = cx + pts[3 * i] * pxm - 1, Y = cy - pts[3 * i + 2] * pxm - 1;
                if (!(X > -2 && X < w && Y > -2 && Y < h)) continue;
                out.push_back({float(X + 1), float(Y + 1), (unsigned char)g});
            }
        return;
    }
    static std::vector<SideDot> dots;
    static std::vector<unsigned char> band;
    static std::vector<int> order;
    dots.resize(std::size_t(cnt)); band.resize(std::size_t(cnt));
    for (long i = 0; i < cnt; i++) {
        const double X = cx + pts[3 * i] * pxm - 1, Y = cy - pts[3 * i + 2] * pxm - 1;
        band[std::size_t(i)] = X > -2 && X < w && Y > -2 && Y < h ? 0 : kNoBand;
        dots[std::size_t(i)] = {float(X + 1), float(Y + 1), grp[std::size_t(i)]};
    }
    drawOrderByCounting(band.data(), grp, cnt, 1, nGroups, order);
    out.reserve(order.size());
    for (const int i : order) out.push_back(dots[std::size_t(i)]);
}

}  // namespace atom
