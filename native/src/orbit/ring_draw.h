// 0.64.0: a ring system's picture on the screen, shared by the Orbit Lab's drawing (orbit_view.cpp), the rings' card program (render: kRingsFS, the same
// geometry per pixel) and its start-up check and tests (orbit/ring_check.h): the ring plane on the screen, the planet's shadow function, the drawn bands
// and the processor's triangles (the exact reference the card is checked against and the fallback where the card cannot draw them).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace orbit {

constexpr int kRingSegments = 128;                       // angular steps of the processor's rings (chords: the card follows the same chords)

// One ring system on the screen for one frame. Screen frame: right, up, towards you; positions in points (x right, y down, as the renderer's).
struct RingFrame {
    double x = 0, y = 0, r = 1;                          // the planet's middle and its equatorial radius on the screen
    double e1[3] = {1, 0, 0}, e2[3] = {0, 1, 0};         // the ring plane's two axes (unit vectors, screen frame)
    bool hasK = false;                                   // K: the screen frame to the planet's own frame (orbit_view.cpp SpinCtx), the pole axis divided by sz
    double K[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double sz = 1;                                       // equatorial / polar radius (the ball of radius 1 in the stretched frame)
    double Lb[3] = {0, 0, 1};                            // the direction to the Sun in that frame, and its square length
    double LL = 1;
};
// a drawn band: inner and outer edge in planet radii (a narrow ring widened to a pixel), its opacity (coverage folded in) and its light k (the tint's factor)
struct RingBand { double a0 = 0, a1 = 0; float alpha = 0, k = 0; };

// The ring plane and the Sun for a pole (px, py, pz on the screen, not necessarily unit), the Sun's direction (Lx, Ly, Lz: right, up, towards you).
// K: nullptr without a spin model (the Sun's direction is then used as it is).
inline RingFrame ringFrame(double x, double y, double r, double px, double py, double pz, const float* K, double sz, double Lx, double Ly, double Lz) {
    RingFrame f;
    f.x = x; f.y = y; f.r = r;
    const double pn = std::sqrt(px * px + py * py + pz * pz); px /= pn; py /= pn; pz /= pn;
    double e1x = py, e1y = -px, e1z = 0; { const double n = std::hypot(e1x, e1y); if (n < 1e-6) { e1x = 1; e1y = 0; } else { e1x /= n; e1y /= n; } }
    f.e1[0] = e1x; f.e1[1] = e1y; f.e1[2] = e1z;
    f.e2[0] = py * e1z - pz * e1y; f.e2[1] = pz * e1x - px * e1z; f.e2[2] = px * e1y - py * e1x;   // P x E1
    f.hasK = K != nullptr; f.sz = K ? sz : 1.0;
    if (K) for (int i = 0; i < 9; i++) f.K[i] = K[i];
    double Lb[3];
    if (!K) { Lb[0] = Lx; Lb[1] = Ly; Lb[2] = Lz; }
    else { for (int i = 0; i < 3; i++) Lb[i] = f.K[3 * i] * Lx + f.K[3 * i + 1] * Ly + f.K[3 * i + 2] * Lz; Lb[2] /= f.sz; }
    for (int i = 0; i < 3; i++) f.Lb[i] = Lb[i];
    f.LL = Lb[0] * Lb[0] + Lb[1] * Lb[1] + Lb[2] * Lb[2];
    return f;
}

// 0.46.7: the shadow test as a smooth function, negative in the planet's shadow: (closest approach of the ray to the Sun to the body's middle)^2 - 1, in
// the frame where the body is a ball of radius 1. On the rings (all outside the ball) the ray's half away from the Sun never comes within 1 of the middle,
// so its value stays positive there. q: a point of the ring plane in planet radii (screen frame).
inline double ringShadowF(const RingFrame& f, double qx, double qy, double qz) {
    double qb[3];
    if (!f.hasK) { qb[0] = qx; qb[1] = qy; qb[2] = qz; }
    else { for (int i = 0; i < 3; i++) qb[i] = f.K[3 * i] * qx + f.K[3 * i + 1] * qy + f.K[3 * i + 2] * qz; qb[2] /= f.sz; }
    const double t = -(qb[0] * f.Lb[0] + qb[1] * f.Lb[1] + qb[2] * f.Lb[2]) / f.LL;
    const double dx = qb[0] + t * f.Lb[0], dy = qb[1] + t * f.Lb[1], dz = qb[2] + t * f.Lb[2], v = dx * dx + dy * dy + dz * dz - 1;
    return t <= 0 ? std::max(v, 1e-3) : v;
}

// The processor's triangles of one band, the half away from you (back) or the half towards you: kRingSegments angular steps (chords) of up to 8 radial
// steps (so the shadow's edge is smooth); a step the shadow's edge crosses is cut into 4 x 4 smaller quads, each triangle cut along F = 0 (linear between
// its corners): the lit part and the shadowed part. tri(x0, y0, x1, y1, x2, y2, lit) gets each triangle. W, H: the picture (a part of a band wholly beyond
// one side of it is not made). 0.64.0: a run of radial steps the shadow's edge does not cross, on one side of it, is ONE quad (the steps lie on the same two
// straight lines from the middle, so their union is that quad: the same pixels as 0.63.1's separate steps).
template <class Tri>
void ringTriangles(const RingFrame& f, const RingBand& b, bool back, double W, double H, Tri&& tri) {
    constexpr double pi = 3.14159265358979323846;
    const double drawPx = (b.a1 - b.a0) * f.r;
    const int nr = std::max(1, std::min(8, static_cast<int>(drawPx / 6)));
    const auto at = [&](double rr, double tt, double& X, double& Y, double& F) {   // a point of the ring: screen position and shadow function
        const double ct = std::cos(tt), st = std::sin(tt);
        const double qx = rr * (ct * f.e1[0] + st * f.e2[0]), qy = rr * (ct * f.e1[1] + st * f.e2[1]), qz = rr * (ct * f.e1[2] + st * f.e2[2]);
        X = f.x + f.r * qx; Y = f.y - f.r * qy; F = ringShadowF(f, qx, qy, qz);
    };
    const auto splitTri = [&](const double* X, const double* Y, const double* F) {   // a triangle cut along F = 0
        double lx[4], ly[4], sx[4], sy[4]; int nl = 0, ns = 0;
        for (int a = 0; a < 3; a++) {
            const int b2 = (a + 1) % 3;
            if (F[a] >= 0) { lx[nl] = X[a]; ly[nl++] = Y[a]; } else { sx[ns] = X[a]; sy[ns++] = Y[a]; }
            if ((F[a] >= 0) != (F[b2] >= 0)) {
                const double u = F[a] / (F[a] - F[b2]), ex = X[a] + u * (X[b2] - X[a]), ey = Y[a] + u * (Y[b2] - Y[a]);
                lx[nl] = ex; ly[nl++] = ey; sx[ns] = ex; sy[ns++] = ey;
            }
        }
        for (int k = 1; k + 1 < nl; k++) tri(lx[0], ly[0], lx[k], ly[k], lx[k + 1], ly[k + 1], true);
        for (int k = 1; k + 1 < ns; k++) tri(sx[0], sy[0], sx[k], sy[k], sx[k + 1], sy[k + 1], false);
    };
    double fe[2][9], xe[2][9], ye[2][9];
    for (int i = 0; i < kRingSegments; i++) {
        const double t0 = 2 * pi * i / kRingSegments, t1 = 2 * pi * (i + 1) / kRingSegments;
        const double zm = std::cos(0.5 * (t0 + t1)) * f.e1[2] + std::sin(0.5 * (t0 + t1)) * f.e2[2];
        if ((zm < 0) != back) continue;                   // back: the half away from you
        for (int j = 0; j <= nr; j++) {
            const double rj = b.a0 + (b.a1 - b.a0) * j / nr;
            at(rj, t0, xe[0][j], ye[0][j], fe[0][j]);
            at(rj, t1, xe[1][j], ye[1][j], fe[1][j]);
        }
        {   // the whole step beyond one side of the picture: nothing of it is made
            double mnx = 1e300, mxx = -1e300, mny = 1e300, mxy = -1e300;
            for (int e = 0; e < 2; e++) for (int j = 0; j <= nr; j += nr) { mnx = std::min(mnx, xe[e][j]); mxx = std::max(mxx, xe[e][j]); mny = std::min(mny, ye[e][j]); mxy = std::max(mxy, ye[e][j]); }
            if (mxx < -2 || mnx > W + 2 || mxy < -2 || mny > H + 2) continue;
        }
        const auto mixedAt = [&](int j) { const bool s0 = fe[0][j] >= 0; return !(s0 == (fe[0][j + 1] >= 0) && s0 == (fe[1][j + 1] >= 0) && s0 == (fe[1][j] >= 0)); };
        int j = 0;
        while (j < nr) {
            if (!mixedAt(j)) {                            // a run of whole steps on one side of the shadow's edge: one quad
                const bool lit = fe[0][j] >= 0;
                int k = j + 1;
                while (k < nr && !mixedAt(k) && (fe[0][k] >= 0) == lit) k++;
                const double aX[3] = {xe[0][j], xe[0][k], xe[1][k]}, aY[3] = {ye[0][j], ye[0][k], ye[1][k]}, aF[3] = {fe[0][j], fe[0][k], fe[1][k]};
                const double bX[3] = {xe[0][j], xe[1][k], xe[1][j]}, bY[3] = {ye[0][j], ye[1][k], ye[1][j]}, bF[3] = {fe[0][j], fe[1][k], fe[1][j]};
                splitTri(aX, aY, aF);
                splitTri(bX, bY, bF);
                j = k;
                continue;
            }
            const double r0 = b.a0 + (b.a1 - b.a0) * j / nr, r1 = b.a0 + (b.a1 - b.a0) * (j + 1) / nr;
            constexpr int S = 4;                          // the shadow's edge crosses this step: 4 x 4 smaller quads, each cut along it
            for (int u = 0; u < S; u++)
                for (int w = 0; w < S; w++) {
                    double sX[4], sY[4], sF[4];
                    const double ra = r0 + (r1 - r0) * u / S, rb = r0 + (r1 - r0) * (u + 1) / S, ta = t0 + (t1 - t0) * w / S, tb = t0 + (t1 - t0) * (w + 1) / S;
                    at(ra, ta, sX[0], sY[0], sF[0]); at(rb, ta, sX[1], sY[1], sF[1]); at(rb, tb, sX[2], sY[2], sF[2]); at(ra, tb, sX[3], sY[3], sF[3]);
                    const double aX[3] = {sX[0], sX[1], sX[2]}, aY[3] = {sY[0], sY[1], sY[2]}, aF[3] = {sF[0], sF[1], sF[2]};
                    const double bX[3] = {sX[0], sX[2], sX[3]}, bY[3] = {sY[0], sY[2], sY[3]}, bF[3] = {sF[0], sF[2], sF[3]};
                    splitTri(aX, aY, aF);
                    splitTri(bX, bY, bF);
                }
            j++;
        }
    }
}

}  // namespace orbit
