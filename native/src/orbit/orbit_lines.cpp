// 0.68.0: the Orbit Lab's lines for both paths (orbit/orbit_lines.h). The processor's functions are orbit_view.cpp's code of 0.67.0, moved here unchanged.
#include "orbit/orbit_lines.h"

#include <algorithm>
#include <cmath>

#include "orbit/solar_system.h"
#include "orbit/units.h"

namespace orbit {

using namespace units;
using render::Color;

namespace {
std::vector<double>* recNext(std::vector<std::vector<double>>* rec) { if (!rec) return nullptr; rec->emplace_back(); return &rec->back(); }
// a point to the pen; the start-up check keeps it instead (on the picture, and where it is in space: x, y, X, Y, Z)
inline void go(LinePen& pen, const Q& q, double x, double y, double z) {
    if (pen.rec) { for (double v : {q.first, q.second, x, y, z}) pen.rec->push_back(v); return; }
    pen.to(q);                                        // 0.75.0: cut at the near plane (3D)
}
}  // namespace

// ---- the processor's ----
void ringPointsXYZ(double Rm, std::vector<double>& xyz) {
    xyz.clear();
    for (int i = 0; i <= 96; i++) { const double t = i / 96.0 * 2 * kPi; xyz.push_back(Rm * std::cos(t)); xyz.push_back(Rm * std::sin(t)); xyz.push_back(0); }
}

void drawRingCpu(render::Renderer2D& R, const LineCam& cam, const std::vector<double>& xyz, Color col, std::vector<std::vector<double>>* rec) {
    LinePen pen{R, cam.W, cam.H, 1.0, col};
    pen.rec = recNext(rec); pen.cam = &cam;
    for (std::size_t i = 0; i + 2 < xyz.size(); i += 3) { const Q q = cam.proj(xyz[i], xyz[i + 1], xyz[i + 2]); go(pen, q, xyz[i], xyz[i + 1], xyz[i + 2]); }
}

void drawPathCpu(render::Renderer2D& R, const LineCam& cam, const std::vector<Vec3>& pts, const std::vector<PathPart>& parts, double width, double dashOn, double dashOff,
                 std::vector<std::vector<double>>* rec) {
    const int n = static_cast<int>(pts.size());
    for (const PathPart& pp : parts) {
        const int from = std::max(1, pp.from), to = std::min(n - 1, pp.to);
        if (to < from) continue;
        LinePen pen{R, cam.W, cam.H, width, pp.col, dashOn, dashOff};
        pen.rec = recNext(rec); pen.cam = &cam;
        for (int i = from - 1; i <= to; i++) { const Vec3& v = pts[std::size_t(i)]; go(pen, cam.proj(v.x, v.y, v.z), v.x, v.y, v.z); }
    }
}

int trailStart(const std::deque<Vec3>& tr, const Vec3& pos, bool full) {
    const int n = static_cast<int>(tr.size());
    int st0 = 0;
    if (!full && n > 1) {                                 // only the last stretch, about 0.6 of the distance to the Sun
        const double lim = 0.6 * std::max(length(pos), kSimLength);
        double acc = length(pos - tr[std::size_t(n - 1)]);
        st0 = n - 1;
        while (st0 > 0 && acc < lim) { acc += length(tr[std::size_t(st0)] - tr[std::size_t(st0 - 1)]); st0--; }
    }
    return st0;
}

void drawTrailCpu(render::Renderer2D& R, const LineCam& cam, const std::deque<Vec3>& tr, const Vec3& pos, int st0, double width, const Color cols[4],
                  std::vector<std::vector<double>>* rec) {
    const int n = static_cast<int>(tr.size());
    if (n == 0) return;
    const double W = cam.W, H = cam.H;
    const int nn = n - st0;
    // 0.46.6: the trail's points are about 2 % of the distance to the Sun apart, so straight pieces cut inside the curve by up to r (0.02)^2 / 8: 7,500 km at 1 AU,
    // plain to see when zoomed in. A long piece that reaches the picture is drawn as a Catmull-Rom curve through its neighbours (off by about r (0.02)^4: tens of km).
    const auto P = [&](int i) { return i >= n ? pos : tr[std::size_t(std::max(0, i))]; };   // (point n is the body itself: the trail always ends there)
    const auto piece = [&](LinePen& pen, int i) {    // the curve from point i - 1 to point i, without its first point
        const Vec3 a = P(i - 1), c = P(i);
        const Q qa = cam.proj(a.x, a.y, a.z), qc = cam.proj(c.x, c.y, c.z);
        double L = 0;
        const bool near = chordNear(cam, qa, qc, L);      // 0.75.0: the chord cut at the near plane
        if (near && L > 6 && i >= 2 && i <= n) {
            const Vec3 p0 = P(i - 2), p3 = i + 1 <= n ? P(i + 1) : c * 2.0 - a;
            const int k = static_cast<int>(std::min(4096.0, std::ceil(L / 4)));
            for (int j = 1; j < k; j++) {
                const double t = double(j) / k, t2 = t * t, t3 = t2 * t;
                const Vec3 v = (a * 2.0 + (c - p0) * t + (p0 * 2.0 - a * 5.0 + c * 4.0 - p3) * t2 + (a * 3.0 - p0 - c * 3.0 + p3) * t3) * 0.5;
                const Q f = cam.proj(v.x, v.y, v.z); go(pen, f, v.x, v.y, v.z);
            }
        }
        go(pen, qc, c.x, c.y, c.z);
    };
    for (int part = 0; part < 4; part++) {           // fades in 4 steps, oldest faintest
        const int i0 = st0 + part * nn / 4, i1 = std::min(n, st0 + (part + 1) * nn / 4 + 1);
        LinePen pen{R, W, H, width, cols[part]};
        pen.rec = recNext(rec); pen.cam = &cam;
        for (int i = i0; i < i1; i++) {
            if (i == i0) { const Vec3& w = tr[std::size_t(i)]; const Q q = cam.proj(w.x, w.y, w.z); go(pen, q, w.x, w.y, w.z); }
            else piece(pen, i);
        }
        if (part == 3) piece(pen, n);                // always end at the body itself
    }
}

void drawOrbitCpu(render::Renderer2D& R, const LineCam& cam, const Body& b, double width, Color col, std::vector<Vec3>& cpts, std::vector<std::vector<double>>* rec) {
    const double W = cam.W, H = cam.H;
    orbitPath(b, 256, cpts);
    LinePen pen{R, W, H, width, col};
    pen.rec = recNext(rec); pen.cam = &cam;
    // 0.46.6 (outside review: Earth zoomed in had two paths about 10,000 km apart): a 256-sided polygon of a 1 AU orbit is up to a(1 - cos(pi/256)) = 11,000 km
    // inside the real ellipse. A piece that reaches the picture is drawn again from the ellipse itself, finely enough that it is off by under half a point.
    std::vector<Vec3> fine;
    for (std::size_t i = 0; i < cpts.size(); i++) {
        const Q q = cam.proj(cpts[i].x, cpts[i].y, cpts[i].z);
        if (i > 0) {
            const Q q0 = cam.proj(cpts[i - 1].x, cpts[i - 1].y, cpts[i - 1].z);
            double L = 0;
            const bool near = chordNear(cam, q0, q, L);   // (a piece's bulge is far under a quarter of its length; 0.75.0: the chord cut at the near plane)
            if (near && L > 6) {
                const int k = static_cast<int>(std::min(4096.0, std::ceil(L / 4)));
                orbitPath(b, 2 * kPi * (i - 1) / 256.0, 2 * kPi * i / 256.0, k, fine);
                for (std::size_t j = 1; j + 1 < fine.size(); j++) { const Q f = cam.proj(fine[j].x, fine[j].y, fine[j].z); go(pen, f, fine[j].x, fine[j].y, fine[j].z); }
            }
        }
        go(pen, q, cpts[i].x, cpts[i].y, cpts[i].z);
    }
}

// 0.73.0: an ellipse given directly (the Alpha Centauri scene), the same corners and fine pieces as drawOrbitCpu
void ellipsePath(const LineEllipse& el, double E0, double E1, int n, std::vector<Vec3>& out) {
    out.clear();
    for (int i = 0; i <= n; i++) {
        const double E = E0 + (E1 - E0) * i / n;
        out.push_back(el.F + el.P * (el.a * (std::cos(E) - el.e)) + el.Q * (el.b * std::sin(E)));
    }
}
void ellipseCorners(const LineEllipse& el, std::vector<Vec3>& out) { ellipsePath(el, 0, 2 * kPi, 256, out); }

void drawEllipseCpu(render::Renderer2D& R, const LineCam& cam, const LineEllipse& el, double width, Color col, std::vector<Vec3>& cpts, std::vector<std::vector<double>>* rec) {
    const double W = cam.W, H = cam.H;
    ellipseCorners(el, cpts);
    std::vector<Vec3> fine;
    // piece i: from corner i - 1 to corner i, drawn again from the ellipse finely enough where it reaches the picture (drawOrbitCpu's rule), without its first point
    const auto piece = [&](LinePen& pen, std::size_t i) {
        const Q q0 = cam.proj(cpts[i - 1].x, cpts[i - 1].y, cpts[i - 1].z), q = cam.proj(cpts[i].x, cpts[i].y, cpts[i].z);
        double L = 0;
        const bool near = chordNear(cam, q0, q, L);
        if (near && L > 6) {
            const int k = static_cast<int>(std::min(4096.0, std::ceil(L / 4)));
            ellipsePath(el, 2 * kPi * (i - 1) / 256.0, 2 * kPi * i / 256.0, k, fine);
            for (std::size_t j = 1; j + 1 < fine.size(); j++) { const Q f = cam.proj(fine[j].x, fine[j].y, fine[j].z); go(pen, f, fine[j].x, fine[j].y, fine[j].z); }
        }
        go(pen, q, cpts[i].x, cpts[i].y, cpts[i].z);
    };
    const auto start = [&](LinePen& pen, std::size_t i) { const Q q = cam.proj(cpts[i].x, cpts[i].y, cpts[i].z); go(pen, q, cpts[i].x, cpts[i].y, cpts[i].z); };
    if (!el.dashed) {
        LinePen pen{R, W, H, width, col};
        pen.rec = recNext(rec); pen.cam = &cam;
        start(pen, 0);
        for (std::size_t i = 1; i < cpts.size(); i++) piece(pen, i);
        return;
    }
    for (std::size_t i = 1; i < cpts.size(); i += 2) {    // dashed: the odd pieces, each its own line
        LinePen pen{R, W, H, width, col};
        pen.rec = recNext(rec); pen.cam = &cam;
        start(pen, i - 1);
        piece(pen, i);
    }
}

// ---- the card's ----
bool LinesPoints::ring(std::uint64_t key, const std::deque<Vec3>& tr, render::LinesRun& r) {
    const int n = static_cast<int>(tr.size());
    if (card) return card->ring(key, n, [&](int i, double* p) { const Vec3& v = tr[std::size_t(i)]; p[0] = v.x; p[1] = v.y; p[2] = v.z; }, r);
    if (!check || n <= 0) return false;
    r.slot = static_cast<std::uint32_t>(check->checkPts.size() / 3); r.cap = static_cast<std::uint32_t>(n); r.start = 0; r.n = static_cast<std::uint32_t>(n);
    for (const Vec3& v : tr) { check->checkPts.push_back(v.x); check->checkPts.push_back(v.y); check->checkPts.push_back(v.z); }
    return true;
}

bool LinesPoints::list(std::uint64_t key, const std::vector<double>& xyz, render::LinesRun& r) {
    const int n = static_cast<int>(xyz.size() / 3);
    if (card) return card->list(key, xyz.data(), n, r);
    if (!check || n <= 0) return false;
    r.slot = static_cast<std::uint32_t>(check->checkPts.size() / 3); r.cap = static_cast<std::uint32_t>(n); r.start = 0; r.n = static_cast<std::uint32_t>(n);
    check->checkPts.insert(check->checkPts.end(), xyz.begin(), xyz.begin() + 3 * n);
    return true;
}

bool LinesPoints::list(std::uint64_t key, const std::vector<Vec3>& pts, render::LinesRun& r) {
    static_assert(sizeof(Vec3) == 3 * sizeof(double), "Vec3 is three doubles");
    const int n = static_cast<int>(pts.size());
    if (card) return card->list(key, pts.empty() ? nullptr : &pts[0].x, n, r);
    if (!check || n <= 0) return false;
    r.slot = static_cast<std::uint32_t>(check->checkPts.size() / 3); r.cap = static_cast<std::uint32_t>(n); r.start = 0; r.n = static_cast<std::uint32_t>(n);
    for (const Vec3& v : pts) { check->checkPts.push_back(v.x); check->checkPts.push_back(v.y); check->checkPts.push_back(v.z); }
    return true;
}

std::int32_t LinesPoints::circle() {
    if (circleAt >= 0) return circleAt;
    if (card) return circleAt = static_cast<std::int32_t>(card->circleSlot());
    if (!check) return 0;
    circleAt = static_cast<std::int32_t>(check->checkPts.size() / 3);
    for (int i = 0; i <= 256; i++) { const double E = 0 + (2 * kPi - 0) * i / 256; check->checkPts.push_back(std::cos(E)); check->checkPts.push_back(std::sin(E)); check->checkPts.push_back(0); }
    return circleAt;
}

namespace {
void hiLo3(const Vec3& v, float hi[4], float lo[4]) {
    const double p[3] = {v.x, v.y, v.z};
    for (int k = 0; k < 3; k++) { hi[k] = static_cast<float>(p[k]); lo[k] = static_cast<float>(p[k] - static_cast<double>(hi[k])); }
    hi[3] = lo[3] = 0;
}
void colourOf(render::LinesRun& r, Color c) { r.colour[0] = c.r; r.colour[1] = c.g; r.colour[2] = c.b; r.colour[3] = c.a; }
}  // namespace

render::LinesCamera linesCamera(const LineCam& c) {
    render::LinesCamera k;
    // C: the reference point, so the points near the view are small numbers. 2D: the point at the picture's middle at the view's middle depth (orbit_view's
    // map turned back); 0.75.0, 3D: the look-at point, on the camera's axis at the look-at plane's depth (orbit/camera.h)
    const double x1 = c.d3 ? c.lx : -(c.fx + c.ox) / c.px, y2 = c.d3 ? c.ly : c.oy / c.px, dep = c.cd;
    const double y1 = y2 * c.cb + dep * c.sb, z = -y2 * c.sb + dep * c.cb, x = x1 * c.ca + y1 * c.sa, y = -x1 * c.sa + y1 * c.ca;
    const Vec3 C{x + c.bc.x, y + c.bc.y, z + c.bc.z};
    // 0.73.0 (a bug since 0.68.0, found by the Alpha Centauri case of the start-up check): hiLo3 writes four floats, and cHi and cLo have three, so its hi[3] = 0
    // was cLo[0]: the low part of C's x was always lost (an x error up to half a float step of C's x: 0.5 point at 1 AU zoomed fully in, 7 points 12,000 au out)
    { float h4[4], l4[4]; hiLo3(C, h4, l4); for (int q = 0; q < 3; q++) { k.cHi[q] = h4[q]; k.cLo[q] = l4[q]; } }
    // C's own numbers as the camera works them out, about the barycentre
    double X1 = 0, Y2 = 0, DEP = 0;
    c.turn(C.x, C.y, C.z, X1, Y2, DEP);
    k.rot[0] = static_cast<float>(c.ca); k.rot[1] = static_cast<float>(c.sa); k.rot[2] = static_cast<float>(c.cb); k.rot[3] = static_cast<float>(c.sb);
    k.proj[0] = static_cast<float>(c.px); k.proj[1] = c.d3 ? static_cast<float>(c.d) : 0.0f; k.proj[2] = static_cast<float>(DEP - c.cd); k.proj[3] = static_cast<float>(c.zNear);
    if (c.d3) {   // 0.75.0: the pinhole: the principal point, C's offset from the camera's axis (m: rounding only), the focal length (points)
        k.scr[0] = static_cast<float>(c.ppx); k.scr[1] = static_cast<float>(c.ppy); k.scr[2] = static_cast<float>(X1 - c.lx); k.scr[3] = static_cast<float>(Y2 - c.ly);
    } else {
        k.scr[0] = static_cast<float>(c.W / 2 + c.fx + c.ox + c.px * X1); k.scr[1] = static_cast<float>(c.H / 2 + c.oy - c.px * Y2); k.scr[2] = 0; k.scr[3] = 0;
    }
    k.view[0] = static_cast<float>(c.W); k.view[1] = static_cast<float>(c.H); k.view[2] = 20; k.view[3] = static_cast<float>(c.F);
    return k;
}

bool addRingRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const std::vector<double>& xyz, Color col) {
    render::LinesRun r;
    if (!pts.list(key, xyz, r)) return false;
    r.from = 1; r.to = static_cast<std::int32_t>(r.n) - 1; r.kind = 0;
    colourOf(r, col); r.style[0] = 1.0f;
    d.add(r);
    return true;
}

bool addTrailRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const std::deque<Vec3>& tr, const Vec3& pos, int st0, double width, const Color cols[4]) {
    const int n = static_cast<int>(tr.size());
    if (n == 0) return true;
    render::LinesRun r;
    if (!pts.ring(key, tr, r)) return false;
    r.kind = 1;
    r.style[0] = static_cast<float>(width);
    hiLo3(pos, r.tailHi, r.tailLo);
    const int nn = n - st0;
    for (int part = 0; part < 4; part++) {           // drawTrailCpu's parts: piece i joins point i - 1 to point i; the last part ends at the body (piece n)
        const int i0 = st0 + part * nn / 4, i1 = std::min(n, st0 + (part + 1) * nn / 4 + 1);
        r.from = i0 + 1; r.to = part == 3 ? n : i1 - 1;
        colourOf(r, cols[part]);
        d.add(r);
    }
    return true;
}

bool addOrbitRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const Body& b, double width, Color col, std::vector<Vec3>& cpts) {
    orbitPath(b, 256, cpts);
    if (cpts.size() != 257) return true;             // (not bound: no orbit, as drawOrbitCpu)
    Vec3 P, Q;
    double a = 0, e = 0, bAx = 0;
    if (!orbitAxes(b, P, Q, a, e, bAx)) return true;
    render::LinesRun r;
    if (!pts.list(key, cpts, r)) return false;
    r.from = 1; r.to = 256; r.kind = 2;
    r.circle = pts.circle();
    colourOf(r, col); r.style[0] = static_cast<float>(width);
    const Vec3 pa = P * a, qb = Q * bAx;
    r.pa[0] = static_cast<float>(pa.x); r.pa[1] = static_cast<float>(pa.y); r.pa[2] = static_cast<float>(pa.z);
    r.qb[0] = static_cast<float>(qb.x); r.qb[1] = static_cast<float>(qb.y); r.qb[2] = static_cast<float>(qb.z);
    r.qb[3] = static_cast<float>(2 * kPi / 256);
    d.add(r);
    return true;
}

bool addPathRuns(LinesPoints& lp, render::LinesDraw& d, std::uint64_t key, const std::vector<Vec3>& pts, const std::vector<PathPart>& parts, double width, double dashOn, double dashOff) {
    const int n = static_cast<int>(pts.size());
    if (n < 2) return true;
    render::LinesRun r;
    if (!lp.list(key, pts, r)) return false;
    r.kind = 0;
    r.style[0] = static_cast<float>(width); r.style[1] = static_cast<float>(dashOn); r.style[2] = static_cast<float>(dashOff);
    for (const PathPart& pp : parts) {
        r.from = std::max(1, pp.from); r.to = std::min(n - 1, pp.to);
        colourOf(r, pp.col);
        d.add(r);                                        // (a part with no pieces adds nothing)
    }
    return true;
}

bool addEllipseRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const LineEllipse& el, double width, Color col, std::vector<Vec3>& cpts) {
    ellipseCorners(el, cpts);
    render::LinesRun r;
    if (!pts.list(key, cpts, r)) return false;
    r.kind = 2;
    r.circle = pts.circle();
    colourOf(r, col); r.style[0] = static_cast<float>(width);
    const Vec3 pa = el.P * el.a, qb = el.Q * el.b;
    r.pa[0] = static_cast<float>(pa.x); r.pa[1] = static_cast<float>(pa.y); r.pa[2] = static_cast<float>(pa.z);
    r.qb[0] = static_cast<float>(qb.x); r.qb[1] = static_cast<float>(qb.y); r.qb[2] = static_cast<float>(qb.z);
    r.qb[3] = static_cast<float>(2 * kPi / 256);
    if (!el.dashed) { r.from = 1; r.to = 256; d.add(r); return true; }
    for (int i = 1; i <= 256; i += 2) { r.from = i; r.to = i; d.add(r); }   // drawEllipseCpu's dashes: one run a piece
    return true;
}

}  // namespace orbit
