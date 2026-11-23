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
    // 0.75.0: the 3D view's pinhole camera (orbit/camera.h). A ring point q (planet radii, screen frame) is at depth Z = Zc (1 - zr qz) for Zc the
    // middle's distance in front of the camera and zr = (radius, m) / Zc, so it is on the screen at pp + ((x, y) - pp + r (qx, -qy)) / (1 - zr qz); nearer
    // than the near plane (1 - zr qz < wn = near / Zc) it is not drawn. zr = 0: the flat map of 0.74.0 (2D, and the tests' old views).
    double ppx = 0, ppy = 0, zr = 0, wn = 0;
    // 0.75.1: each ring point's own slant. The pole (unit, screen frame) and the Sun's elevation: mu0 = |sin| of it, and whether the face you see is the one the
    // Sun lights (the same for every point: the camera is on one side of the plane, the Sun far away). ringMu(f, q) is the point's mu.
    double pole[3] = {0, 0, 1}, mu0 = 0.5;
    bool sunlit = true;
};
constexpr double kRingMuMin = 0.004;                      // the smallest slant drawn (edge-on: opaque, no width); the card's shader has the same number
// 0.75.1: the sine of the angle between the line of sight to a ring point q (planet radii, in the ring plane, screen frame) and the ring plane:
// mu = |n . (P - C)| / |P - C| for the pole n, the camera C and the point P. In 3D (the pinhole) the camera is at one height h over the plane, so mu = h / |P - C|
// = h / sqrt(h^2 + D^2) for D the distance along the plane from the camera's foot; worked out in the units of the middle's distance Zc (zr = radius / Zc), the vector
// from the camera to P is (ux + zr qx, uy + zr qy, 1 - zr qz) for (ux, uy) the middle's place on the screen over the focal length, and its dot product with the pole,
// (px ux + py uy - pz) + zr (n . q), loses the last term in the plane. In 2D (the flat map) the slant is |pz| at every point, as before.
inline double ringHeight(const RingFrame& f) {              // signed: n . (the middle - the camera) / Zc (3D); the pole's z (2D, the map's sign)
    if (f.zr == 0) return -f.pole[2];
    const double F = f.r / f.zr, ux = (f.x - f.ppx) / F, uy = -(f.y - f.ppy) / F;
    return f.pole[0] * ux + f.pole[1] * uy - f.pole[2];
}
inline double ringMu(const RingFrame& f, const double q[3]) {
    double mu;
    if (f.zr == 0) mu = std::fabs(f.pole[2]);
    else {
        const double F = f.r / f.zr, ux = (f.x - f.ppx) / F, uy = -(f.y - f.ppy) / F;
        const double vx = ux + f.zr * q[0], vy = uy + f.zr * q[1], vz = 1 - f.zr * q[2];
        mu = std::fabs(ringHeight(f)) / std::sqrt(vx * vx + vy * vy + vz * vz);
    }
    return std::max(mu, kRingMuMin);
}
// the camera is on the pole's side of the plane (it sees the face the pole points from): in 3D, n . (the camera - the middle) > 0
inline bool ringCameraOnPoleSide(const RingFrame& f) { return ringHeight(f) <= 0; }
// a ring point q on the screen; false: nearer than the near plane (or behind the camera)
inline bool ringOnScreen(const RingFrame& f, const double q[3], double& X, double& Y) {
    const double w = 1 - f.zr * q[2];
    if (f.zr != 0 && !(w >= f.wn)) return false;
    if (f.zr == 0) { X = f.x + f.r * q[0]; Y = f.y - f.r * q[1]; return true; }
    X = f.ppx + (f.x - f.ppx + f.r * q[0]) / w; Y = f.ppy + (f.y - f.ppy - f.r * q[1]) / w;
    return true;
}
// 0.75.1: the ring point a pixel's line of sight meets (the inverse of ringOnScreen, as the card's ringAt does it): p = (px, py) in points; q in planet radii, in
// the ring plane; false when the line is parallel to the plane or the point is nearer than the near plane
inline bool ringPointAtPixel(const RingFrame& f, double px, double py, double q[3]) {
    const double u = (px - f.ppx) / f.r, v = -(py - f.ppy) / f.r, cu = (f.x - f.ppx) / f.r, cv = -(f.y - f.ppy) / f.r;
    const double m00 = f.e1[0] + u * f.zr * f.e1[2], m01 = f.e2[0] + u * f.zr * f.e2[2], m10 = f.e1[1] + v * f.zr * f.e1[2], m11 = f.e2[1] + v * f.zr * f.e2[2];
    const double det = m00 * m11 - m01 * m10;
    if (std::fabs(det) < 1e-6) return false;
    const double A = ((u - cu) * m11 - m01 * (v - cv)) / det, B = (m00 * (v - cv) - (u - cu) * m10) / det;
    for (int k = 0; k < 3; k++) q[k] = A * f.e1[k] + B * f.e2[k];
    return !(f.zr != 0 && !(1 - f.zr * q[2] >= f.wn));
}
// a drawn band: inner and outer edge in planet radii (a narrow ring widened to a pixel), its opacity (coverage folded in) and its light k (the tint's factor)
// 0.75.1: a band of Saturn's profile (i0 >= 0) has no fixed opacity and light: they are the means over its measurements i0 .. i0 + n - 1 at each point's own slant
// (orbit/saturn_profile.h saturnGroupMeans), times cover (the share of the drawn width that is the band's own); alpha and k are then the values at the middle's slant.
// 0.75.2: a band of Jupiter's, Uranus's or Neptune's rings (giant_rings.h; tauBand) has its optical depth tau and albedo here and, like Saturn's, takes each point's own slant (ringBandValue).
struct RingBand { double a0 = 0, a1 = 0; float alpha = 0, k = 0; int i0 = -1, n = 0; double cover = 1; bool tauBand = false; double tau = 0, albedo = 0; };

// The ring plane and the Sun for a pole (px, py, pz on the screen, not necessarily unit), the Sun's direction (Lx, Ly, Lz: right, up, towards you).
// K: nullptr without a spin model (the Sun's direction is then used as it is).
inline RingFrame ringFrame(double x, double y, double r, double px, double py, double pz, const float* K, double sz, double Lx, double Ly, double Lz) {
    RingFrame f;
    f.x = x; f.y = y; f.r = r;
    const double pn = std::sqrt(px * px + py * py + pz * pz); px /= pn; py /= pn; pz /= pn;
    double e1x = py, e1y = -px, e1z = 0; { const double n = std::hypot(e1x, e1y); if (n < 1e-6) { e1x = 1; e1y = 0; } else { e1x /= n; e1y /= n; } }
    f.e1[0] = e1x; f.e1[1] = e1y; f.e1[2] = e1z;
    f.e2[0] = py * e1z - pz * e1y; f.e2[1] = pz * e1x - px * e1z; f.e2[2] = px * e1y - py * e1x;   // P x E1
    f.pole[0] = px; f.pole[1] = py; f.pole[2] = pz;
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
    // 0.75.0: a corner keeps its place on the ring plane (q, planet radii) and its shadow function; the cuts (along F = 0, then at the near plane) are made
    // there, in 3D, and only then put on the screen (ringOnScreen): with the pinhole a straight edge in space is a straight edge on the screen
    struct RP { double q[3]; double F; };
    const auto at = [&](double rr, double tt, double& X, double& Y, double& F, RP& p) {   // a point of the ring: screen position (in front or not), shadow function, place
        const double ct = std::cos(tt), st = std::sin(tt);
        p.q[0] = rr * (ct * f.e1[0] + st * f.e2[0]); p.q[1] = rr * (ct * f.e1[1] + st * f.e2[1]); p.q[2] = rr * (ct * f.e1[2] + st * f.e2[2]);
        F = p.F = ringShadowF(f, p.q[0], p.q[1], p.q[2]);
        if (!ringOnScreen(f, p.q, X, Y)) { X = std::nan(""); Y = X; }
    };
    const auto mix = [](const RP& a, const RP& b, double u) { RP m; for (int k = 0; k < 3; k++) m.q[k] = a.q[k] + u * (b.q[k] - a.q[k]); m.F = a.F + u * (b.F - a.F); return m; };
    const auto emit = [&](const RP* P, int n, bool lit) {   // a convex polygon: cut at the near plane (in 3D), then a fan of triangles on the screen
        RP c[8]; int m = 0;
        if (f.zr == 0) { for (int k = 0; k < n; k++) c[m++] = P[k]; }
        else for (int k = 0; k < n; k++) {
            const RP& a = P[k]; const RP& b = P[(k + 1) % n];
            const double wa = 1 - f.zr * a.q[2] - f.wn, wb = 1 - f.zr * b.q[2] - f.wn;
            if (wa >= 0) c[m++] = a;
            if ((wa >= 0) != (wb >= 0)) { RP e = mix(a, b, wa / (wa - wb)); e.q[2] = (1 - f.wn) / f.zr; c[m++] = e; }
        }
        if (m < 3) return;
        double X[8], Y[8];
        for (int k = 0; k < m; k++) if (!ringOnScreen(f, c[k].q, X[k], Y[k])) { const double w = 1 - f.zr * c[k].q[2], s = 1 / std::max(w, f.wn);
            X[k] = f.ppx + (f.x - f.ppx + f.r * c[k].q[0]) * s; Y[k] = f.ppy + (f.y - f.ppy - f.r * c[k].q[1]) * s; }   // (a cut corner exactly at the plane)
        for (int k = 1; k + 1 < m; k++) tri(X[0], Y[0], X[k], Y[k], X[k + 1], Y[k + 1], lit, c[0].q, c[k].q, c[k + 1].q);   // (0.75.1: with each corner's place on the ring plane)
    };
    const auto splitTri = [&](const RP* P) {             // a triangle cut along F = 0 (linear between its corners, in the ring plane)
        RP L[4], S[4]; int nl = 0, ns = 0;
        for (int a = 0; a < 3; a++) {
            const int b2 = (a + 1) % 3;
            if (P[a].F >= 0) L[nl++] = P[a]; else S[ns++] = P[a];
            if ((P[a].F >= 0) != (P[b2].F >= 0)) { const RP e = mix(P[a], P[b2], P[a].F / (P[a].F - P[b2].F)); L[nl++] = e; S[ns++] = e; }
        }
        if (nl >= 3) emit(L, nl, true);
        if (ns >= 3) emit(S, ns, false);
    };
    double fe[2][9], xe[2][9], ye[2][9];
    RP pe[2][9];
    for (int i = 0; i < kRingSegments; i++) {
        const double t0 = 2 * pi * i / kRingSegments, t1 = 2 * pi * (i + 1) / kRingSegments;
        const double zm = std::cos(0.5 * (t0 + t1)) * f.e1[2] + std::sin(0.5 * (t0 + t1)) * f.e2[2];
        if ((zm < 0) != back) continue;                   // back: the half away from you
        for (int j = 0; j <= nr; j++) {
            const double rj = b.a0 + (b.a1 - b.a0) * j / nr;
            at(rj, t0, xe[0][j], ye[0][j], fe[0][j], pe[0][j]);
            at(rj, t1, xe[1][j], ye[1][j], fe[1][j], pe[1][j]);
        }
        {   // the whole step beyond one side of the picture: nothing of it is made (0.75.0: only when all four corners are in front of the near plane)
            double mnx = 1e300, mxx = -1e300, mny = 1e300, mxy = -1e300;
            bool front = true;
            for (int e = 0; e < 2; e++) for (int j = 0; j <= nr; j += nr) { if (std::isnan(xe[e][j])) front = false; else { mnx = std::min(mnx, xe[e][j]); mxx = std::max(mxx, xe[e][j]); mny = std::min(mny, ye[e][j]); mxy = std::max(mxy, ye[e][j]); } }
            if (front && (mxx < -2 || mnx > W + 2 || mxy < -2 || mny > H + 2)) continue;
        }
        const auto mixedAt = [&](int j) { const bool s0 = fe[0][j] >= 0; return !(s0 == (fe[0][j + 1] >= 0) && s0 == (fe[1][j + 1] >= 0) && s0 == (fe[1][j] >= 0)); };
        int j = 0;
        while (j < nr) {
            if (!mixedAt(j)) {                            // a run of whole steps on one side of the shadow's edge: one quad
                const bool lit = fe[0][j] >= 0;
                int k = j + 1;
                while (k < nr && !mixedAt(k) && (fe[0][k] >= 0) == lit) k++;
                const RP a3[3] = {pe[0][j], pe[0][k], pe[1][k]}, b3[3] = {pe[0][j], pe[1][k], pe[1][j]};
                splitTri(a3);
                splitTri(b3);
                j = k;
                continue;
            }
            const double r0 = b.a0 + (b.a1 - b.a0) * j / nr, r1 = b.a0 + (b.a1 - b.a0) * (j + 1) / nr;
            constexpr int S = 4;                          // the shadow's edge crosses this step: 4 x 4 smaller quads, each cut along it
            for (int u = 0; u < S; u++)
                for (int w = 0; w < S; w++) {
                    double sX[4], sY[4], sF[4];
                    RP sp[4];
                    const double ra = r0 + (r1 - r0) * u / S, rb = r0 + (r1 - r0) * (u + 1) / S, ta = t0 + (t1 - t0) * w / S, tb = t0 + (t1 - t0) * (w + 1) / S;
                    at(ra, ta, sX[0], sY[0], sF[0], sp[0]); at(rb, ta, sX[1], sY[1], sF[1], sp[1]); at(rb, tb, sX[2], sY[2], sF[2], sp[2]); at(ra, tb, sX[3], sY[3], sF[3], sp[3]);
                    const RP a3[3] = {sp[0], sp[1], sp[2]}, b3[3] = {sp[0], sp[2], sp[3]};
                    splitTri(a3);
                    splitTri(b3);
                }
            j++;
        }
    }
}

}  // namespace orbit
