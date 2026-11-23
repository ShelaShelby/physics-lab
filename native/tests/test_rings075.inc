// 0.75.1: Saturn's rings, each part at its own viewing slant, WRITTEN BEFORE the code. The references are written here, not taken from the app:
//  1) the geometry by hand: a camera at one height h over the ring plane sees a ring point at distance D along the plane (from the camera's foot) at
//     mu = h / sqrt(h^2 + D^2); and in the camera's own vectors mu = |n . (P - C)| / |P - C| (n the plane's normal, C the camera, P the point), with the points
//     put in space from the screen the way a pinhole does (depth away from the camera), not through the app's ring formulas;
//  2) a pixel's ring point and slant: the pixel's ray from the camera meets the ring plane where the vectors say;
//  3) a band's opacity at a pixel is the area mean of 1 - exp(-tau / mu) over the measurements of the saved Cassini profile (the data the app reads) at THAT
//     pixel's mu, and its light the mean of albedo / 0.5 x the single-scattering formulas (Chandrasekhar 1960) written out here; limit: the area weights of the
//     profile's 10 km steps (9.96 to 10.04 km) against the equal weights the card uses, at most 0.4 % of a term's range: 0.004 (blocked, range 1) and 0.012 (light);
//  4) the far-camera limit: from 400 radii away (mu nearly the same everywhere) each pixel's colour is 0.75.0's (one mu for the whole system) within 8 levels
//     in at least 99 % of the ring pixels (the card check's limit).
// 0.75.0 used one slant for every point (|pz|, the line to the middle): the first test shows how far that is from the formula close up.
#include "orbit/ring_bands.h"

namespace rings075 {
struct Cam { RingFrame f; double S[3]; double nd[3]; double F; };   // the frame, the middle in camera space (right, up, depth), the normal in the same, the focal length

// a ring system seen by a pinhole at the origin looking along +depth: the middle at S (radii; R = 1), the pole's screen-frame components (px, py, pz: right, up, towards the viewer)
Cam makeCam(double Sx, double Sy, double Sz, double px, double py, double pz, double F) {
    Cam c;
    const double ppx = 640, ppy = 400, n = std::sqrt(px * px + py * py + pz * pz);
    px /= n; py /= n; pz /= n;
    const double x = ppx + F * Sx / Sz, y = ppy - F * Sy / Sz, r = F * 1.0 / Sz;       // the middle on the screen, a radius on the screen
    c.f = ringFrame(x, y, r, px, py, pz, nullptr, 1.0, 0.5, 0.45, 0.74);
    c.f.ppx = ppx; c.f.ppy = ppy; c.f.zr = 1.0 / Sz; c.f.wn = 0.01 / Sz;
    c.S[0] = Sx; c.S[1] = Sy; c.S[2] = Sz;
    c.nd[0] = px; c.nd[1] = py; c.nd[2] = -pz;                                         // (the depth runs away from the camera: the screen frame's z is towards it)
    c.F = F;
    return c;
}
// the point of the ring plane a ring-plane vector q (screen frame) names, in camera space: the middle plus (qx, qy, -qz)
void spacePoint(const Cam& c, const double q[3], double P[3]) { P[0] = c.S[0] + q[0]; P[1] = c.S[1] + q[1]; P[2] = c.S[2] - q[2]; }
double muByVectors(const Cam& c, const double q[3]) {
    double P[3]; spacePoint(c, q, P);
    const double nP = c.nd[0] * P[0] + c.nd[1] * P[1] + c.nd[2] * P[2], L = std::sqrt(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]);
    return std::max(std::fabs(nP) / L, kRingMuMin);
}
}  // namespace rings075

static void testRings075() {
    using namespace rings075;
    namespace rg = orbit::rings;
    // ---- 1) the geometry by hand ----
    {   // looking straight down on the plane from a height h = 4 radii: the pole points at the camera, the middle on the axis; a point D radii away along the plane: h / sqrt(h^2 + D^2)
        const Cam c = makeCam(0, 0, 4, 0, 0, 1, 900);
        for (double D : {0.3, 1.0, 2.2}) {
            for (double ang : {0.0, 1.1, 3.0}) {
                const double q[3] = {D * std::cos(ang), D * std::sin(ang), 0};
                near(ringMu(c.f, q), 4 / std::sqrt(16 + D * D), 1, 1e-9, "rings: straight down from 4 radii, a point " + std::to_string(D) + " away: mu = h / sqrt(h^2 + D^2)");
            }
        }
    }
    // a general view: the middle off the axis, the pole tilted, a near point and a far point; the reference is the vectors from the camera to the point
    const double ppoles[3][3] = {{0.12, 0.99, 0.07}, {-0.3, 0.6, -0.5}, {0.5, -0.2, 0.8}};
    for (const auto& pp : ppoles) {
        const Cam c = makeCam(0.9, -0.3, 4.2, pp[0], pp[1], pp[2], 900);
        double mus[2] = {0, 0};
        for (int k = 0; k < 2; k++) {
            const double A = k ? -2.0 : 0.25, B = k ? 1.5 : -0.2;
            double q[3];
            for (int i = 0; i < 3; i++) q[i] = A * c.f.e1[i] + B * c.f.e2[i];
            mus[k] = ringMu(c.f, q);
            near(mus[k], muByVectors(c, q), 1, 1e-9, std::string("rings: the app's mu at a ") + (k ? "far" : "near") + " ring point, against |n . (P - C)| / |P - C| from the camera's vectors");
            // the camera's height over the plane is one number: mu = h / sqrt(h^2 + D^2) with D along the plane from the camera's foot
            const double h = std::fabs(c.nd[0] * c.S[0] + c.nd[1] * c.S[1] + c.nd[2] * c.S[2]);
            double P[3]; spacePoint(c, q, P);
            const double D2 = P[0] * P[0] + P[1] * P[1] + P[2] * P[2] - h * h;
            near(mus[k], std::max(h / std::sqrt(h * h + D2), kRingMuMin), 1, 1e-9, "rings: ... and h / sqrt(h^2 + D^2)");
        }
        // 0.75.0 gave every point |pz| (the slant along the line to the middle); the parts differ a lot (printed for the report)
        std::printf("   rings, pole (%.2f, %.2f, %.2f): near point mu %.4f, far point mu %.4f; 0.75.0 used |pz| = %.4f for both\n", pp[0], pp[1], pp[2], mus[0], mus[1], c.f.pole[2] < 0 ? -c.f.pole[2] : c.f.pole[2]);
        check(std::max(mus[0], mus[1]) > 1.2 * std::min(mus[0], mus[1]), "rings: near and far parts are at slants over 20 % apart close up (0.75.0 had one for both)");
    }
    // ---- 2) a pixel's ring point: the inverse of the screen projection, and the ray's meeting with the plane ----
    {
        const Cam c = makeCam(0.5, 0.2, 3.0, 0.2, 0.9, 0.35, 900);
        for (double sx : {120.0, 400.0, 700.0, 1100.0}) for (double sy : {90.0, 300.0, 520.0, 760.0}) {
            double q[3];
            if (!ringPointAtPixel(c.f, sx, sy, q)) continue;
            double P[3]; spacePoint(c, q, P);
            // the pixel's ray through the pinhole: depth d, (x, y) = (pp + F (X / d, -Y / d)); the point is on it
            near(640 + c.F * P[0] / P[2], sx, 1000, 1e-9, "rings: the pixel's ring point is on the pixel's ray (x)");
            near(400 - c.F * P[1] / P[2], sy, 1000, 1e-9, "rings: the pixel's ring point is on the pixel's ray (y)");
            const double nq = c.f.pole[0] * q[0] + c.f.pole[1] * q[1] + c.f.pole[2] * q[2];
            near(nq, 0, 1, 1e-9, "rings: the pixel's ring point is in the ring plane");
        }
    }
    // ---- 3) a band's opacity and light at a pixel: the area mean at that pixel's own mu ----
    {
        const auto albedo = [&](double km) { return rg::saturnAlbedoAt(km); };
        const auto edgeLo = [&](int i) { return i > 0 ? 0.5 * (rg::saturnProfileKm(i - 1) + rg::saturnProfileKm(i)) : rg::saturnProfileKm(0) - 0.5 * (rg::saturnProfileKm(1) - rg::saturnProfileKm(0)); };
        const auto edgeHi = [&](int i) { const int n = rg::saturnProfileCount(); return i + 1 < n ? edgeLo(i + 1) : rg::saturnProfileKm(i) + 0.5 * (rg::saturnProfileKm(i) - rg::saturnProfileKm(i - 1)); };
        const std::vector<RingBand>& bands = saturnDrawnBands(rg::kSaturnEqRadiusKm, 400.0);   // (a planet of 400 points: bands of 150 km, 15 measurements)
        check(bands.size() > 400 && bands.size() <= 768, "rings: the drawn bands fit the card's 768 (" + std::to_string(bands.size()) + ")");
        long compared = 0;
        double worstB = 0, worstS = 0;
        const double poles[2][3] = {{0.12, 0.99, 0.07}, {0.5, -0.2, 0.8}};
        for (int vi = 0; vi < 2; vi++) {
            Cam c = makeCam(0.9, -0.3, 4.2, poles[vi][0], poles[vi][1], poles[vi][2], 900);
            for (int sun = 0; sun < 2; sun++) {
                const double mu0 = sun ? 0.35 : 0.08, sunElev = sun ? 0.35 : -0.08;   // the Sun above, then below the plane (the pole's side)
                c.f.mu0 = mu0; c.f.sunlit = ringCameraOnPoleSide(c.f) == (sunElev >= 0);
                for (double sx = 20; sx < 1260; sx += 17) for (double sy = 10; sy < 790; sy += 13) {
                    double q[3];
                    if (!ringPointAtPixel(c.f, sx, sy, q)) continue;
                    const double rad = std::hypot(q[0], q[1], q[2]), km = rad * rg::kSaturnEqRadiusKm;
                    const RingBand* band = nullptr;
                    for (const RingBand& b : bands) if (rad >= b.a0 && rad < b.a1) { band = &b; break; }
                    if (!band) continue;
                    // the mean over the band's measurements at this pixel's mu, by the formulas as the textbook has them, weighted by each measurement's own width
                    const double mu = muByVectors(c, q);
                    double sb = 0, ss = 0, ws = 0;
                    for (int j = band->i0; j < band->i0 + band->n; j++) {
                        const double tau = rg::saturnProfileTau(j), w = edgeHi(j) - edgeLo(j);
                        sb += w * (1 - std::exp(-tau / mu));
                        const double sh = c.f.sunlit ? mu0 / (mu0 + mu) * (1 - std::exp(-tau * (1 / mu + 1 / mu0))) / 0.5
                                                     : (std::fabs(mu0 - mu) < 1e-9 ? tau / mu * std::exp(-tau / mu) / 0.5 : mu0 / (mu0 - mu) * (std::exp(-tau / mu0) - std::exp(-tau / mu)) / 0.5);
                        ss += w * albedo(rg::saturnProfileKm(j)) / 0.5 * sh;
                        ws += w;
                    }
                    sb /= ws; ss /= ws;
                    const RingValue v = ringBandValue(c.f, *band, ringMu(c.f, q));
                    const double aWant = sb * band->cover, sWant = ss * band->cover, aGot = v.alpha, sGot = double(v.alpha) * v.k;
                    // the light is alpha x k, k capped at 1 (the full tint): compare where it is not capped
                    worstB = std::max(worstB, std::fabs(aGot - aWant));
                    if (aWant > 1e-3 && sWant / aWant < 1) worstS = std::max(worstS, std::fabs(sGot - sWant));
                    compared++;
                    (void)km;
                }
            }
        }
        std::printf("   rings: %ld pixels compared; the largest difference from the area mean: opacity %.5f, light %.5f\n", compared, worstB, worstS);
        check(compared > 500, "rings: enough pixels fell on the bands (" + std::to_string(compared) + ")");
        check(worstB <= 0.004, "rings: a band's opacity at a pixel is the area mean of 1 - exp(-tau / mu) at that pixel's mu (limit 0.004, got " + std::to_string(worstB) + ")");
        check(worstS <= 0.012, "rings: a band's light at a pixel is the mean of albedo / 0.5 x shine at that pixel's mu (limit 0.012, got " + std::to_string(worstS) + ")");
    }
    // ---- 4) the far-camera limit: from 400 radii, 0.75.0's one slant gives the same colours within the card check's limit ----
    {
        const double Zc = 400;
        const double mu0 = 0.4, sunElev = 0.4;
        Cam c = makeCam(0, 0, Zc, 0.25, 0.85, 0.46, 400.0 * 260);                  // (the middle on the axis: the slant along the line to it is |pz|)
        const double mu1 = std::max(std::fabs(c.f.pole[2]), kRingMuMin);
        // a focal length 400 times larger puts the rings across the picture (a telephoto lens: the slant geometry is the same)
        c.f.mu0 = mu0; c.f.sunlit = ringCameraOnPoleSide(c.f) == (sunElev >= 0);
        const std::vector<RingBand>& bands = saturnDrawnBands(rg::kSaturnEqRadiusKm, c.f.r);
        const std::vector<RingBand>& flat = saturnDrawnBandsFlat(rg::kSaturnEqRadiusKm, c.f.r, mu1, mu0, c.f.sunlit);
        long ring = 0, over = 0;
        int worst = 0;
        for (double sx = 0; sx < 1280; sx += 4) for (double sy = 0; sy < 800; sy += 4) {
            double q[3];
            if (!ringPointAtPixel(c.f, sx, sy, q)) continue;
            const double rad = std::hypot(q[0], q[1], q[2]);
            const RingBand* nb = nullptr; const RingBand* ob = nullptr;
            for (const RingBand& b : bands) if (rad >= b.a0 && rad < b.a1) { nb = &b; break; }
            for (const RingBand& b : flat) if (rad >= b.a0 && rad < b.a1) { ob = &b; break; }
            if (!nb && !ob) continue;
            const RingValue nv = nb ? ringBandValue(c.f, *nb, ringMu(c.f, q)) : RingValue{0, 0, true};
            const double oa = ob ? ob->alpha : 0, ok = ob ? ob->k : 0;
            const auto level = [](double a, double k, int ch) { return int(std::lround(std::clamp(a * render::shadeEncoded(kRingTint[ch], k), 0.0, 1.0) * 255)); };
            int d = 0; bool any = false;
            for (int ch = 0; ch < 3; ch++) { const int a = level(nv.alpha, nv.k, ch), b = level(oa, ok, ch); d = std::max(d, std::abs(a - b)); any = any || a || b; }
            if (!any) continue;
            ring++; over += d > 8; worst = std::max(worst, d);
        }
        std::printf("   rings, far camera (400 radii): %ld ring pixels, %ld over 8 levels from 0.75.0's one slant, the largest %d\n", ring, over, worst);
        check(ring > 5000 && over * 100 <= ring, "rings: from 400 radii away the picture is 0.75.0's within the card check's limit (at most 1 % over 8 levels)");
    }
}
