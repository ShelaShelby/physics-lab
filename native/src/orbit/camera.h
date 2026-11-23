// 0.75.0: the Orbit Lab's camera, ONE place for every point the picture puts on the screen (orbit_view.cpp's bodies, moons, labels and hits, the lines of
// orbit/orbit_lines.h on both paths, the belts' rocks, the night sky, the Alpha Centauri scene). 2D: the top-down map, as before (no perspective). 3D: one
// pinhole camera with a vertical field of view of kFieldOfViewDeg. It looks at the look-at point (the followed thing, else the view's middle) from the
// distance d chosen so that the look-at plane shows the zoom's span: d = (span across the picture's height) / (2 tan(fov / 2)) = focal / px. So the zoom,
// the wheel, the Journey and the scale bar keep their meaning at the target's distance, and zooming in moves the camera closer.
//   camera space: X right, Y up, Z the distance in front of the camera (m); screen = principal point + focal (X / Z, -Y / Z) (points, y down).
//   A point nearer than the near plane (kNearFraction d) or behind the camera is not drawn; a line is cut at the near plane in 3D (clipNear).
#pragma once

#include <algorithm>
#include <cmath>

#include "orbit/vec3.h"

namespace orbit {

// the 3D view's vertical field of view (degrees): one constant (a later setting may change it). 45 degrees is a phone camera's main lens held upright.
constexpr double kFieldOfViewDeg = 45.0;
// the near plane, as a fraction of the camera's distance to the look-at plane. Something there is drawn 100 times bigger than at the target's distance;
// nearer than that nothing useful is left on the picture (a line or a moon fills it), and 1 / 0.01 keeps the card's single-precision division well away
// from its limits (the lines' check). The closest zoom keeps the camera at least 1.5 radii from a followed body's middle (OrbitView), so the body itself
// (its near side at 0.33 d or more) is always in front of the near plane.
constexpr double kNearFraction = 0.01;
constexpr double kCamPi = 3.14159265358979323846;
inline double focalFor(double H) { return H / 2 / std::tan(kFieldOfViewDeg / 2 * kCamPi / 180); }   // points from the middle to the edge at half the field

// A point on screen: x, y (points), depth (m, the turned frame's third axis, > 0 towards the viewer), the perspective scale (3D: d / Z, the size of a metre
// here over its size at the look-at plane; 0: not drawn: behind the camera or nearer than the near plane; 2D: 1) and the camera-space place (3D; 2D: 0, 0, 1)
struct Q { double first, second, dep, f; double X = 0, Y = 0, Z = 1; };

struct ViewCam {
    Vec3 bc;                                             // everything about the barycentre
    double ca = 1, sa = 0, cb = 1, sb = 0;               // the turn: the yaw about z, then the pitch (orbit_view.cpp)
    double px = 1;                                       // points per metre at the look-at plane (the zoom)
    double W = 1, H = 1, fx = 0, ox = 0, oy = 0;         // the picture (points), the scene's middle shift, the view's offset at the look-at plane
    bool d3 = false;
    // 3D: the look-at plane's depth (the turned frame), the principal point (where the camera's axis meets the picture); finish() sets the rest
    double cd = 0, ppx = 0, ppy = 0;
    double F = 1, d = 1, lx = 0, ly = 0, zNear = 0;      // focal (points), the camera's distance from the look-at plane (m), the axis's place, the near plane (m)
    // the derived numbers: focal from the picture's height, d = F / px, and the camera's axis put so that the look-at plane maps as the 2D map
    // (W / 2 + fx + ox + px x1, H / 2 + oy - px y2) does: a point there is where it was before 0.75.0, whatever the principal point
    void finish() {
        F = focalFor(H); d = F / px; zNear = kNearFraction * d;
        lx = (ppx - W / 2 - fx - ox) / px; ly = (H / 2 + oy - ppy) / px;
    }
    void turn(double x, double y, double z, double& x1, double& y2, double& dep) const {
        x -= bc.x; y -= bc.y; z -= bc.z;
        x1 = x * ca - y * sa; const double y1 = x * sa + y * ca; y2 = y1 * cb - z * sb; dep = y1 * sb + z * cb;
    }
    double depth(double x, double y, double z) const { double x1, y2, dep; turn(x, y, z, x1, y2, dep); return dep; }
    // the look-at plane's map without the offset (OrbitView::holdAt): where a point would be with ox = oy = 0, ignoring its depth
    void plane(double x, double y, double z, double& sx, double& sy) const { double x1, y2, dep; turn(x, y, z, x1, y2, dep); sx = W / 2 + fx + x1 * px; sy = H / 2 - y2 * px; }
    Q fromCam(double X, double Y, double Z, double dep) const {   // a camera-space place (3D) on the screen
        if (!(Z >= zNear)) return Q{-1e9, -1e9, dep, 0, X, Y, Z};
        return Q{ppx + F * X / Z, ppy - F * Y / Z, dep, d / Z, X, Y, Z};
    }
    Q proj(double x, double y, double z) const {
        double x1, y2, dep; turn(x, y, z, x1, y2, dep);
        if (!d3) return Q{W / 2 + fx + ox + x1 * px, H / 2 + oy - y2 * px, dep, 1};
        return fromCam(x1 - lx, y2 - ly, cd + d - dep, dep);
    }
    // a direction (world, unit) as seen from the camera: the sky's stars, anything too far for the camera's place to matter. False: not in front.
    bool dirScreen(const Vec3& v, double& X, double& Y) const;
    // where the camera is (world, m), 3D
    Vec3 position() const {
        const double x1 = lx, y2 = ly, dep = cd + d;
        const double y1 = y2 * cb + dep * sb, z = -y2 * sb + dep * cb;
        return Vec3{x1 * ca + y1 * sa + bc.x, -x1 * sa + y1 * ca + bc.y, z + bc.z};
    }
};

// THE pinhole for a direction (the sky, orbit/sky.cpp skyScreenLens, and ViewCam::dirScreen): the turned direction (x1, y2, depth), seen towards -depth
inline bool pinholeDirection(const Vec3& v, double ca, double sa, double cb, double sb, double ppx, double ppy, double F, double& X, double& Y) {
    const double x1 = v.x * ca - v.y * sa, y1 = v.x * sa + v.y * ca, y2 = y1 * cb - v.z * sb, dep = y1 * sb + v.z * cb;
    const double Z = -dep;                               // the camera looks towards -depth
    if (!(Z > 1e-9 * std::sqrt(x1 * x1 + y2 * y2 + dep * dep))) return false;
    X = ppx + F * x1 / Z; Y = ppy - F * y2 / Z;
    return true;
}
inline bool ViewCam::dirScreen(const Vec3& v, double& X, double& Y) const { return pinholeDirection(v, ca, sa, cb, sb, ppx, ppy, F, X, Y); }

// a sphere of radius R whose middle is at q (3D): its outline's radius on the screen, focal tan(asin(R / L)) for L the distance from the camera (exact for a
// sphere seen straight on; off the axis its outline is a slightly longer ellipse, drawn round). Inside it: a huge number.
inline double sphereScreenRadius(const ViewCam& c, const Q& q, double R) {
    const double L2 = q.X * q.X + q.Y * q.Y + q.Z * q.Z, R2 = R * R;
    if (L2 <= R2 * (1 + 1e-12)) return 1e9;
    return c.F * R / std::sqrt(L2 - R2);
}

// the part of the segment a-b in front of the near plane, cut there in 3D (camera space, then projected): false when none of it is in front.
inline bool clipNear(const ViewCam& c, const Q& a, const Q& b, Q& a2, Q& b2) {
    a2 = a; b2 = b;
    if (!c.d3) return true;
    const bool ia = a.Z >= c.zNear, ib = b.Z >= c.zNear;
    if (ia && ib) return true;
    if (!ia && !ib) return false;
    const double t = (c.zNear - a.Z) / (b.Z - a.Z);      // where it meets the plane
    const double X = a.X + t * (b.X - a.X), Y = a.Y + t * (b.Y - a.Y);
    const Q m = Q{c.ppx + c.F * X / c.zNear, c.ppy - c.F * Y / c.zNear, a.dep + t * (b.dep - a.dep), c.d / c.zNear, X, Y, c.zNear};
    if (ia) b2 = m; else a2 = m;
    return true;
}

// a flat convex polygon (its corners on the screen, in order): the part in front of the near plane, cut there in 3D, as a fan of triangles tri(a, b, c)
template <class Tri>
inline void clipPolygonNear(const ViewCam& c, const Q* p, int n, Tri&& tri) {
    Q o[16]{};
    int m = 0;
    for (int k = 0; k < n && m < 15; k++) {
        if (!c.d3) { o[m++] = p[k]; continue; }
        const Q& a = p[k];
        const Q& b = p[(k + 1) % n];
        const bool ia = a.Z >= c.zNear, ib = b.Z >= c.zNear;
        if (ia) o[m++] = a;
        if (ia != ib) { Q x{0, 0, 0, 0}, y{0, 0, 0, 0}; clipNear(c, a, b, x, y); o[m++] = ia ? y : x; }
    }
    for (int k = 1; k + 1 < m; k++) tri(o[0], o[k], o[k + 1]);
}

}  // namespace orbit
