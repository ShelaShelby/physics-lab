// 0.75.0: the Orbit Lab's 3D camera (orbit/camera.h), WRITTEN BEFORE the code. The references are pinhole geometry written here from the camera's place and
// its three axes (right, up, forward), not the app's own formula and not its earlier pictures: a point at angle a from the axis is focal tan(a) from the
// middle; a sphere's outline is where the tangent rays from the camera meet the picture; parallel lines meet at the vanishing point of their direction; the
// near plane cuts a segment where its depth is the near distance; a star (a direction) and a scene point very far away in that direction are at the same
// place; and the zoom's span at the target's distance is 0.74.0's (min(W, H) / 34 x 10^zoom points per sim length), so the scale bar holds there.
#include "orbit/sky.h"

static ViewCam testCam(double W, double H, double zoom, double yaw, double pitch, const Vec3& bc, double cdep, double ox, double oy, double ppdx, double ppdy) {
    ViewCam c;
    c.bc = bc; c.W = W; c.H = H; c.d3 = true;
    const double yawA = yaw + units::kPi, pitchA = -pitch;   // (orbit_view.cpp's turn)
    c.ca = std::cos(yawA); c.sa = std::sin(yawA); c.cb = std::cos(pitchA); c.sb = std::sin(pitchA);
    c.px = std::min(W, H) / 34 * std::pow(10.0, zoom) / units::kSimLength;
    c.cd = cdep; c.ox = ox; c.oy = oy; c.ppx = W / 2 + ppdx; c.ppy = H / 2 + ppdy;
    c.finish();
    return c;
}

static void testCamera075() {
    const double kDeg = units::kPi / 180;
    // the camera's axes in space, from its turn (x1 right, y2 up, the depth towards the viewer: the camera looks along minus it)
    const auto axes = [](const ViewCam& c, Vec3& right, Vec3& up, Vec3& fwd) {
        right = Vec3{c.ca, -c.sa, 0}; up = Vec3{c.sa * c.cb, c.ca * c.cb, -c.sb};
        fwd = Vec3{-c.sa * c.sb, -c.ca * c.sb, -c.cb};
    };
    const ViewCam cams[] = {testCam(1280, 800, 0.0, 0.6, 1.0, Vec3{1.1e9, -0.7e9, 2e7}, 0, 0, 0, 0, 0),
                            testCam(1280, 800, 3.0, 2.0, 0.3, Vec3{3e8, 2e8, -1e7}, 7.4e11, 120, -40, 0, 0),
                            testCam(880, 560, 5.5, -1.2, -0.9, Vec3{}, -2.5e11, -300, 75, 0, 0)};
    for (const ViewCam& c : cams) {
        Vec3 right, up, fwd;
        axes(c, right, up, fwd);
        const Vec3 P = c.position();
        // the field: the picture's height is 45 degrees: focal = (H / 2) / tan(22.5 degrees)
        near(c.F, c.H / 2 / std::tan(22.5 * kDeg), c.F, 1e-12, "camera: focal length for a 45-degree field");
        // 1) a point on the axis is at the middle, at any distance in front
        for (double s : {0.02, 0.5, 1.0, 7.0, 1e3, 1e6}) {
            const Vec3 p = P + fwd * (s * c.d);
            const Q q = c.proj(p.x, p.y, p.z);
            check(q.f > 0, "camera: a point on the axis in front is drawn");
            near(q.first, c.ppx, c.W, 1e-9, "camera: a point on the axis is at the middle (x), s = " + std::to_string(s));
            near(q.second, c.ppy, c.H, 1e-9, "camera: a point on the axis is at the middle (y), s = " + std::to_string(s));
        }
        // 2) a point at angle a above the axis is focal tan(a) above the middle; at 22.5 degrees it is at the picture's top edge (a 45-degree field)
        for (double a : {1.0, 10.0, 22.5, -15.0}) {
            const Vec3 p = P + (fwd * std::cos(a * kDeg) + up * std::sin(a * kDeg)) * (3.7 * c.d);
            const Q q = c.proj(p.x, p.y, p.z);
            near(q.second, c.ppy - c.F * std::tan(a * kDeg), c.H, 1e-9, "camera: a point " + std::to_string(a) + " degrees above the axis is focal tan(a) above the middle");
            near(q.first, c.ppx, c.W, 1e-9, "camera: ... and straight above it");
        }
        {
            const Vec3 p = P + (fwd * std::cos(22.5 * kDeg) + up * std::sin(22.5 * kDeg)) * (2 * c.d);
            near(c.proj(p.x, p.y, p.z).second, 0.0, c.H, 1e-9, "camera: 22.5 degrees above the axis is the picture's top edge");
        }
        // 3) a sphere of radius R at distance L: its outline is where the camera's tangent rays meet the sphere, at L - R^2 / L along the axis and
        //    R sqrt(1 - R^2 / L^2) from it; its radius on the picture is focal tan(asin(R / L))
        for (double k : {1.5, 3.0, 40.0}) {
            const double L = k * 0.1 * c.d, R = 0.1 * c.d;
            const Vec3 C = P + fwd * L;
            const Q qc = c.proj(C.x, C.y, C.z);
            const Vec3 T = P + fwd * (L - R * R / L) + right * (R * std::sqrt(1 - R * R / (L * L)));
            const Q qt = c.proj(T.x, T.y, T.z);
            const double want = std::hypot(qt.first - qc.first, qt.second - qc.second);
            near(sphereScreenRadius(c, qc, R), want, want, 1e-9, "camera: a sphere's outline from the tangent rays, L = " + std::to_string(k) + " R");
            near(sphereScreenRadius(c, qc, R), c.F * std::tan(std::asin(R / L)), want, 1e-9, "camera: a sphere's radius on the picture is focal tan(asin(R / L))");
        }
        // 4) two parallel lines going away meet at the vanishing point of their direction, pp + focal (u . right, -(u . up)) / (u . fwd)
        {
            Vec3 u = fwd * 0.8 + right * 0.35 - up * 0.25;
            u = u * (1 / length(u));
            const double vx = c.ppx + c.F * dot(u, right) / dot(u, fwd), vy = c.ppy - c.F * dot(u, up) / dot(u, fwd);
            const Vec3 a0 = P + fwd * c.d + right * (0.3 * c.d), b0 = P + fwd * (2 * c.d) - up * (0.4 * c.d);
            for (const Vec3& s0 : {a0, b0}) {
                const Vec3 far = s0 + u * (1e9 * c.d);
                const Q q = c.proj(far.x, far.y, far.z);
                check(std::hypot(q.first - vx, q.second - vy) <= 1e-6 * c.H, "camera: parallel lines meet at one vanishing point (within 1e-6 of the picture)");
                const Vec3 nearer = s0 + u * (1e3 * c.d);   // and they converge to it: a thousand times nearer, they are farther from it
                const Q q2 = c.proj(nearer.x, nearer.y, nearer.z);
                check(std::hypot(q2.first - vx, q2.second - vy) > std::hypot(q.first - vx, q.second - vy), "camera: a parallel line converges to the vanishing point");
            }
        }
        // 5) behind the camera: nothing; a segment from in front to behind is cut at the near plane, the cut end's depth the near distance
        {
            const Vec3 behind = P - fwd * (0.5 * c.d) + right * (0.2 * c.d);
            const Q qb = c.proj(behind.x, behind.y, behind.z);
            check(!(qb.f > 0), "camera: a point behind the camera is not drawn");
            const Vec3 front = P + fwd * (0.8 * c.d) - up * (0.3 * c.d);
            const Q qf = c.proj(front.x, front.y, front.z);
            Q a{0, 0, 0, 0}, b{0, 0, 0, 0};
            check(clipNear(c, qf, qb, a, b), "camera: a segment from in front to behind is drawn in part");
            near(b.Z, kNearFraction * c.d, kNearFraction * c.d, 1e-9, "camera: the cut end's depth is the near distance");
            // the cut end is on the segment, in space: its depth along the forward axis
            const double t = (dot(front - P, fwd) - kNearFraction * c.d) / (dot(front - P, fwd) - dot(behind - P, fwd));
            const Vec3 m = front + (behind - front) * t - P;   // (from the camera; on the picture by the pinhole written from the axes)
            near(b.first, c.ppx + c.F * dot(m, right) / dot(m, fwd), c.W, 1e-6, "camera: the cut end is the segment's own point at the near plane (x)");
            near(b.second, c.ppy - c.F * dot(m, up) / dot(m, fwd), c.H, 1e-6, "camera: the cut end is the segment's own point at the near plane (y)");
            check(!clipNear(c, qb, qb, a, b), "camera: a segment wholly behind the camera is not drawn");
        }
        // 6) THE SKY MATCHES THE SCENE: 20 directions on the picture: a star's place (orbit/sky.h skyScreenLens, the 3D lens) and a scene point 1e6 camera distances
        //    away in that direction from the camera agree within 0.5 point
        {
            SkyLens L;
            L.pinhole = true; L.ppx = c.ppx; L.ppy = c.ppy; L.F = c.F;
            int n = 0;
            double worst = 0;
            for (int i = 0; i < 5; i++)
                for (int j = 0; j < 4; j++) {
                    const double ax = (-0.9 + 1.8 * i / 4) * (c.W / 2) / c.F, ay = (-0.9 + 1.8 * j / 3) * (c.H / 2) / c.F;
                    Vec3 v = fwd + right * ax + up * ay;
                    v = v * (1 / length(v));
                    double X = 0, Y = 0;
                    if (!skyScreenLens(v, c.W, c.H, c.ca, c.sa, c.cb, c.sb, L, X, Y)) { worst = 1e9; continue; }
                    const Vec3 p = P + v * (1e6 * c.d);
                    const Q q = c.proj(p.x, p.y, p.z);
                    worst = std::max(worst, std::hypot(X - q.first, Y - q.second));
                    n++;
                }
            check(n == 20 && worst <= 0.5, "camera: the sky matches the scene: 20 directions, a star and a point 1e6 camera distances away agree within 0.5 point (worst " + std::to_string(worst) + ")");
            // and the pinhole's inverse (the Milky Way's per-pixel map) gives the direction back
            Vec3 v0 = fwd + right * 0.3 - up * 0.2;
            v0 = v0 * (1 / length(v0));
            double X = 0, Y = 0;
            Vec3 back;
            check(skyScreenLens(v0, c.W, c.H, c.ca, c.sa, c.cb, c.sb, L, X, Y) && skyDirectionLens(X, Y, c.W, c.H, c.ca, c.sa, c.cb, c.sb, L, back) && length(back - v0) < 1e-12,
                  "camera: the sky's pinhole and its inverse agree");
        }
    }
    // 7) the zoom's span at the target's distance is 0.74.0's: a length l there is l x min(W, H) / 34 x 10^zoom / kSimLength points; two points at the target's depth half
    //    the picture's height above and below it (in that measure) are at its top and bottom edges
    for (double zoom : {-3.0, 0.0, 3.0, 5.5}) {
        const ViewCam c = testCam(1280, 800, zoom, 0.9, 0.7, Vec3{2e8, -1e8, 0}, 0, 0, 0, 0, 0);
        Vec3 right, up, fwd;
        axes(c, right, up, fwd);
        const double px074 = std::min(c.W, c.H) / 34 * std::pow(10.0, zoom) / units::kSimLength;
        const Vec3 target = c.position() + fwd * c.d;   // (the look-at point)
        const Vec3 top = target + up * (c.H / 2 / px074), bottom = target - up * (c.H / 2 / px074), side = target + right * (100 / px074);
        near(c.proj(top.x, top.y, top.z).second, 0.0, c.H, 1e-9, "camera: the zoom's span at the target (top edge), zoom " + std::to_string(zoom));
        near(c.proj(bottom.x, bottom.y, bottom.z).second, c.H, c.H, 1e-9, "camera: the zoom's span at the target (bottom edge), zoom " + std::to_string(zoom));
        near(c.proj(side.x, side.y, side.z).first - c.ppx, 100.0, 100, 1e-9, "camera: 100 points at the target's distance are 0.74.0's 100 points (the scale bar), zoom " + std::to_string(zoom));
    }
    // 8) the look-at plane maps as 0.74.0's map, whatever the principal point (a slide moves the middle, not the scale): orbit_view's offsets keep their meaning
    {
        const ViewCam c = testCam(1280, 800, 2.0, 0.4, 1.2, Vec3{}, 3e10, 150, -60, 90, 40);
        double sx = 0, sy = 0;
        Vec3 right, up, fwd;
        axes(c, right, up, fwd);
        const Vec3 p = c.position() + fwd * c.d + right * (0.2 * c.d) + up * (0.1 * c.d);
        c.plane(p.x, p.y, p.z, sx, sy);
        const Q q = c.proj(p.x, p.y, p.z);
        near(q.first, sx + c.ox, c.W, 1e-9, "camera: the look-at plane is 0.74.0's map (x)");
        near(q.second, sy + c.oy, c.H, 1e-9, "camera: the look-at plane is 0.74.0's map (y)");
    }
}
