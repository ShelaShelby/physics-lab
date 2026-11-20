// 0.68.0: the Density surface's processor work (molecule_view.cpp's code of 0.67.0, moved here unchanged so the card's start-up check compares with the
// very code that draws on the processor): the vertices on the picture and their depths, the triangles far to near (molecule/depth_order.h), and the
// vertices' colours (lit from the viewer, two-sided, by the potential or plain). Header only.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "molecule/density.h"
#include "molecule/depth_order.h"
#include "render/renderer2d.h"

namespace molecule {

// the potential's colours: red = negative, white = zero, blue = positive (t = V / range, -1..1). The ends are drawing choices.
inline render::Color surfacePotColour(double t) {
    t = std::clamp(t, -1.0, 1.0);
    const render::Color red{0.86f, 0.16f, 0.13f, 1}, blue{0.15f, 0.36f, 0.90f, 1}, white{0.97f, 0.97f, 0.97f, 1};
    const render::Color& e = t < 0 ? red : blue;
    const float a = static_cast<float>(std::fabs(t));
    return {white.r + (e.r - white.r) * a, white.g + (e.g - white.g) * a, white.b + (e.b - white.b) * a, 1};
}

struct SurfaceCpu {
    std::vector<float> sx, sy, sz, tz;                   // the vertices on the picture (points) and their depth; each triangle's depth (the sum of its three)
    std::vector<int> order;                              // the triangles far to near
    std::vector<std::uint32_t> key, key2;
    std::vector<int> tmp;
    std::vector<render::Color> vc;                       // the vertices' colours
};

// R: the camera's turn (rows); (px, py) its pan; (cx, cy) the picture's centre; s points per bohr
inline void surfacePlace(const dens::Mesh& M, const double R[9], double px, double py, double cx, double cy, double s, SurfaceCpu& o) {
    const std::size_t nv = M.p.size() / 3, nt = M.tri.size() / 3;
    o.sx.resize(nv); o.sy.resize(nv); o.sz.resize(nv);
    for (std::size_t v = 0; v < nv; v++) {
        const double x = M.p[3 * v], y = M.p[3 * v + 1], z = M.p[3 * v + 2];
        o.sx[v] = static_cast<float>(cx + (R[0] * x + R[1] * y + R[2] * z + px) * s);
        o.sy[v] = static_cast<float>(cy - (R[3] * x + R[4] * y + R[5] * z + py) * s);
        o.sz[v] = static_cast<float>(R[6] * x + R[7] * y + R[8] * z);
    }
    o.tz.resize(nt);
    for (std::size_t t = 0; t < nt; t++) o.tz[t] = o.sz[std::size_t(M.tri[3 * t])] + o.sz[std::size_t(M.tri[3 * t + 1])] + o.sz[std::size_t(M.tri[3 * t + 2])];
    depthOrder(o.tz, o.order, o.key, o.key2, o.tmp);     // 0.66.0: a radix sort, equal depths by triangle number (molecule/depth_order.h)
}

// colours: the vertex's base colour (potential or plain) times a light, with a little white highlight. 0.46.6 (outside review: CO2's two oxygens and water's
// two hydrogens came out in different colours): the light comes from the viewer (it was from the upper left), so two parts that are mirror images by symmetry
// are always lit the same, and the highlight no longer bleaches one side of the potential's colours
inline void surfaceColours(const dens::Mesh& M, const double R[9], bool pot, double potRange, render::Color plain, SurfaceCpu& o) {
    const std::size_t nv = M.p.size() / 3;
    o.vc.resize(nv);
    const double lx = 0, ly = 0, lz = 1;                      // towards the light, view space
    const double ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    for (std::size_t v = 0; v < nv; v++) {
        const double nx = M.n[3 * v], ny = M.n[3 * v + 1], nz = M.n[3 * v + 2];
        const double vx = R[0] * nx + R[1] * ny + R[2] * nz, vy = R[3] * nx + R[4] * ny + R[5] * nz, vz = R[6] * nx + R[7] * ny + R[8] * nz;
        const double d = std::fabs(vx * lx + vy * ly + vz * lz) / ll;   // (two-sided: the inner side is lit too)
        const double hl = std::pow(std::max(0.0, d), 24) * 0.15;
        const render::Color base = pot ? surfacePotColour(M.value[v] / potRange) : plain;
        const float k = static_cast<float>(0.62 + 0.38 * d);   // (a soft light: the colours stay readable against the scale)
        o.vc[v] = {std::min(1.0f, base.r * k + static_cast<float>(hl)), std::min(1.0f, base.g * k + static_cast<float>(hl)), std::min(1.0f, base.b * k + static_cast<float>(hl)), 1};
    }
}

// one triangle's opacity: its front (facing the viewer: < 0 with screen y down) or its back
inline float surfaceAlpha(const SurfaceCpu& o, int a, int b, int c, float aFront, float aBack) {
    const std::size_t A = std::size_t(a), B = std::size_t(b), C = std::size_t(c);
    const double ex = (o.sx[B] - o.sx[A]) * (o.sy[C] - o.sy[A]) - (o.sy[B] - o.sy[A]) * (o.sx[C] - o.sx[A]);   // screen y points down: < 0 means the triangle faces the viewer
    return ex < 0 ? aFront : aBack;
}

}  // namespace molecule
