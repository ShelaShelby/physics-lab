#include "cloud_dots.h"

#include "cloud_light.h"

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

GroupsBy fillGroups(AtomLab& L, render::IGroupCompute* gc, long cnt, unsigned char* grp) {
    const bool gpu = L.gpuCompute && gc && gc->ok();
    if (L.jumpDrawn()) {
        if (gpu && L.jumpGroupsGpu(*gc, grp)) return GroupsBy::Gpu;
        L.jumpGroups(grp);
        return GroupsBy::Cpu;
    }
    if (L.superOn()) {
        const double th = L.beatTheta();
        if (gpu && L.superGroupsGpu(*gc, cnt, th, grp)) return GroupsBy::Gpu;
        for (long i = 0; i < cnt; i++) grp[i] = (unsigned char)L.superGroup(i, th);
        return GroupsBy::Cpu;
    }
    plainGroups(L.usesWheel(), L.s.ph.data(), L.s.sg.data(), cnt, grp);
    return GroupsBy::Cpu;
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

// ---- Glow: soft dots whose light adds up ----
// 0.46: the exposure from the grid of 8 x 8-point cells (each the summed footprint of its dots), shared by the CPU path below and the GPU cloud (render: Renderer2D::cloudGpu),
// so both pick the same exposure from the same grid. False (expo unchanged) when no cell has light.
bool glowExposure(const std::vector<float>& grid, long cap, bool light, double base, double& expo) {
    constexpr int kCell = 8;
    static std::vector<float> used;                                        // the brightest places, ignoring the top 0.5 % of cells (single random clumps)
    used.clear();
    for (float g : grid) if (g > 0) used.push_back(g);
    if (used.empty()) return false;
    const std::size_t top = used.size() - 1 - std::min<std::size_t>(used.size() - 1, used.size() / 200);
    std::nth_element(used.begin(), used.begin() + top, used.end());
    const float peak = used[top];
    const double summed = double(peak) / (kCell * kCell) * cloudDotWeight(cap);   // per unit of dot light, as if the cloud had 12,000 dots
    const double e = cloudExposure(summed, light ? 2.4 : 1.0, base);
    expo = expo <= 0 || e < expo ? e : std::exp(0.85 * std::log(expo) + 0.15 * std::log(e));   // never brighter than the picture can show; slowly brighter again
    return true;
}
void darkenForLightPage(DotColours& dc) {                                // the Sign colours on a light page: darker, more saturated (the wheel has its own light-page colours)
    const Color cP = dc.c[0], cN = dc.c[1];
    dc.c[0] = Color{cP.r * 0.85f, cP.g * 0.85f, cP.b * 0.85f, 1}; dc.c[1] = Color{cN.r * 0.85f, cN.g * 0.62f, cN.b * 0.45f, 1};
}
double glowSumAt(const std::vector<GlowDot>& D, long cap, double expo, double base, double X, double Y) {
    double s = 0;
    for (const GlowDot& d : D) {
        const double dx = d.x - X, dy = d.y - Y, q = (dx * dx + dy * dy) / (double(d.r) * d.r);
        if (q < 1) s += cloudDotLight(cap, expo, base, d.wt) * (std::exp(-4 * q) - 0.0183156) * 1.018657;
    }
    return s;
}
void glowCapExposure(const std::vector<GlowDot>& D, long cap, double base, const std::vector<std::array<double, 2>>& at, double& expo) {
    for (int it = 0; it < 4 && expo > 0; it++) {                           // (a few rounds: a dot's light is capped at 1, so the sum is not quite linear in expo)
        double top = 0;
        for (const auto& p : at) top = std::max(top, glowSumAt(D, cap, expo, base, p[0], p[1]));
        if (top <= 1) return;
        expo /= top;
    }
}
void glowDotsNear(const float* pts, long cnt, const unsigned char* grp, const CloudCamera& cam, double X, double Y, std::vector<GlowDot>& out, std::vector<int>* indices) {
    out.clear();
    if (indices) indices->clear();
    // 0.57.0: a quick pass in single precision first, which can only leave out dots at least a point farther from (X, Y) than the largest disc (5.52 points):
    // its rounding is about 1e-3 of a point on any picture, so the dots kept below are exactly those of the full pass (tested); a NaN goes on to the full pass
    const float ca = float(cam.ca), sa = float(cam.sa), cb = float(cam.cb), sb = float(cam.sb), k4 = float(1 / (4 * cam.ext)), pxm = float(cam.pxm);
    const float dx0 = float(cam.cx - X), dy0 = float(cam.cy - Y), lim = (4.6f * 1.2f + 1.f) * (4.6f * 1.2f + 1.f);
    static std::vector<int> near;
    near.clear();
    constexpr long kBlock = 1024;
    unsigned char keep[kBlock];
    for (long b0 = 0; b0 < cnt; b0 += kBlock) {
        const long nb = std::min(kBlock, cnt - b0);
        const float* q = pts + 3 * b0;
        for (long j = 0; j < nb; j++) {
            const float x = q[3 * j], y = q[3 * j + 1], z = q[3 * j + 2];
            const float x1 = x * ca - y * sa, y1 = x * sa + y * ca, dep = y1 * cb - z * sb, up = y1 * sb + z * cb;
            const float f = pxm / (1.f + dep * k4);
            const float ex = dx0 + x1 * f, ey = dy0 - up * f;
            keep[j] = !(ex * ex + ey * ey >= lim);
        }
        for (long j = 0; j < nb; j++) if (keep[j]) near.push_back(int(b0 + j));
    }
    for (const int i : near) {
        if (grp && grp[i] == 255) continue;
        const double x = pts[3 * i], y = pts[3 * i + 1], z = pts[3 * i + 2];
        const double x1 = x * cam.ca - y * cam.sa, y1 = x * cam.sa + y * cam.ca, dep = y1 * cam.cb - z * cam.sb, up = y1 * cam.sb + z * cam.cb;   // as projectCloud
        const double f = 1 / (1 + dep / (4 * cam.ext));
        const float px = float(cam.cx + x1 * cam.pxm * f), py = float(cam.cy - up * cam.pxm * f);
        const float t = std::max(-1.f, std::min(1.f, float(dep / cam.ext))), near = 0.5f - 0.5f * t;   // as the atom view's Cloud 3D glow
        const float r = 4.6f * (0.8f + 0.4f * near);
        if ((px - X) * (px - X) + (py - Y) * (py - Y) < double(r) * r) { out.push_back({px, py, r, 0.3f + 0.9f * near, grp ? grp[i] : (unsigned char)0}); if (indices) indices->push_back(i); }
    }
}
bool glowBlobs(const std::vector<GlowDot>& D, long cap, bool light, DotColours& dc, bool darken, double base, double w, double h, double& expo, std::vector<Blob>& B,
               const std::vector<std::array<double, 2>>* cores) {
    B.clear();
    if (D.empty()) return false;
    constexpr int kCell = 8;
    const int gw = int(w / kCell) + 2, gh = int(h / kCell) + 2;
    static std::vector<float> grid;
    grid.assign(std::size_t(gw) * gh, 0.f);
    for (const GlowDot& d : D) {                                           // the light one dot puts into its cell: its depth weight × footprint (pi r² / 4 of a Gaussian)
        const int cx = std::max(0, std::min(gw - 1, int(d.x / kCell))), cy = std::max(0, std::min(gh - 1, int(d.y / kCell)));
        float& g = grid[std::size_t(cy) * gw + cx];
        g += d.wt * d.r * d.r * 0.785f;
    }
    glowExposure(grid, cap, light, base, expo);
    if (cores && !light) glowCapExposure(D, cap, base, *cores, expo);
    if (light && darken) darkenForLightPage(dc);
    B.reserve(D.size());
    for (const GlowDot& d : D) B.push_back(softDot(dc, d.x, d.y, d.r, d.g, cloudDotLight(cap, expo, base, d.wt)));   // (GPU colours: the dot carries its group, the shader picks the colour)
    return true;
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

void sideBlobs(const std::vector<SideDot>& side, const DotColours& dc, double weight, std::vector<Blob>& out) {
    out.clear();
    out.reserve(side.size());
    for (const SideDot& d : side) out.push_back(squareDot(dc, d.x, d.y, 1, d.g, weight));
}

}  // namespace atom
