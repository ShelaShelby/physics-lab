// 0.68.0: the Orbit Lab's lines (the trails, the planets' and dwarf planets' orbits, the distance rings) in one place for both paths: the processor's
// LinePen (the reference, and the path on OpenGL or when the card's is off) and the runs the card draws them from (render/lines_card.h). OrbitView and
// the start-up check (orbit/lines_check.h) both call these, so the check compares the card with the very code that draws on the processor.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

#include "orbit/vec3.h"
#include "render/renderer2d.h"

namespace orbit {

struct Body;

// A point on screen: x, y (points), depth (m, > 0 towards the viewer) and perspective scale
struct Q { double first, second, dep, f; };

// Liang-Barsky: the part of a segment inside a rectangle. Zoomed far in, pieces of a trail or an orbit are millions of points long, so only the visible
// part is drawn.
bool clipSeg(double x0, double y0, double x1, double y1, double xa, double ya, double xb, double yb, double out[4]);   // (inline, below)

// Draws a polyline clipped to the picture (with a 20-point margin), optionally dashed. rec (the start-up check): the line functions below keep each
// point there (x, y on the picture, then X, Y, Z in space) and draw nothing.
struct LinePen {
    render::Renderer2D& r;
    double W, H, width;
    render::Color c;
    double dashOn = 0, dashOff = 0;   // dash lengths in points (0 = solid)
    bool havePrev = false;
    double px = 0, py = 0, dashPos = 0;
    std::vector<double>* rec = nullptr;
    void to(double x, double y);
    void dashed(double x0, double y0, double x1, double y1);
};

// The picture's camera (orbit_view.cpp): turn by yaw about z, tilt by pitch towards the viewer, then a mild perspective; everything about the barycentre bc
struct LineCam {
    Vec3 bc;
    double ca = 1, sa = 0, cb = 1, sb = 0, px = 1, D = 1, cd = 0, W = 1, H = 1, fx = 0, ox = 0, oy = 0;
    bool d3 = false;
    Q base(double x, double y, double z) const {
        x -= bc.x; y -= bc.y; z -= bc.z;
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb, dep = y1 * sb + z * cb;
        const double f = d3 ? D / std::max(D - (dep - cd), 0.05 * D) : 1;
        return Q{W / 2 + fx + x1 * px * f, H / 2 - y2 * px * f, dep, f};
    }
    Q proj(double x, double y, double z) const { Q q = base(x, y, z); q.first += ox; q.second += oy; return q; }
};

// ---- the processor's lines (rec: the start-up check's record of each pen's points, one list a pen) ----
void ringPointsXYZ(double Rm, std::vector<double>& xyz);   // a distance ring of Rm metres round the Sun: 97 points (x, y, z)
void drawRingCpu(render::Renderer2D& R, const LineCam& cam, const std::vector<double>& xyz, render::Color col, std::vector<std::vector<double>>* rec = nullptr);
// a trail: its points tr, the body itself at pos (the trail always ends there), drawn from point st0 in four parts, faintest first
int trailStart(const std::deque<Vec3>& tr, const Vec3& pos, bool full);
void drawTrailCpu(render::Renderer2D& R, const LineCam& cam, const std::deque<Vec3>& tr, const Vec3& pos, int st0, double width, const render::Color cols[4],
                  std::vector<std::vector<double>>* rec = nullptr);
// an orbit: the body's whole orbit (orbitPath, 256 pieces), each piece that reaches the picture drawn again from the ellipse finely enough
void drawOrbitCpu(render::Renderer2D& R, const LineCam& cam, const Body& b, double width, render::Color col, std::vector<Vec3>& cpts,
                  std::vector<std::vector<double>>* rec = nullptr);

// ---- the card's runs ----
// where a run's points go: the card's store (the frame) or the check's own list (LinesDraw::checkPts)
struct LinesPoints {
    render::LinesCard* card = nullptr;
    render::LinesDraw* check = nullptr;
    bool ring(std::uint64_t key, const std::deque<Vec3>& tr, render::LinesRun& r);
    bool list(std::uint64_t key, const std::vector<double>& xyz, render::LinesRun& r);
    bool list(std::uint64_t key, const std::vector<Vec3>& pts, render::LinesRun& r);
    std::int32_t circle();
    std::int32_t circleAt = -1;
};
render::LinesCamera linesCamera(const LineCam& cam);
// the same lines as the processor's functions above, as runs of the draw d (false: the points could not be kept: the caller draws them itself)
bool addRingRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const std::vector<double>& xyz, render::Color col);
bool addTrailRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const std::deque<Vec3>& tr, const Vec3& pos, int st0, double width, const render::Color cols[4]);
bool addOrbitRuns(LinesPoints& pts, render::LinesDraw& d, std::uint64_t key, const Body& b, double width, render::Color col, std::vector<Vec3>& cpts);

// ---- inline: the processor's per-point work ----
inline bool clipSeg(double x0, double y0, double x1, double y1, double xa, double ya, double xb, double yb, double out[4]) {
    const double dx = x1 - x0, dy = y1 - y0;
    if (x0 >= xa && x0 <= xb && x1 >= xa && x1 <= xb && y0 >= ya && y0 <= yb && y1 >= ya && y1 <= yb) {
        out[0] = x0; out[1] = y0; out[2] = x1; out[3] = y1;
        return true;
    }
    double t0 = 0, t1 = 1;
    for (int k = 0; k < 4; k++) {
        const double p = k == 0 ? -dx : k == 1 ? dx : k == 2 ? -dy : dy;
        const double q = k == 0 ? x0 - xa : k == 1 ? xb - x0 : k == 2 ? y0 - ya : yb - y0;
        if (p == 0) { if (q < 0) return false; }
        else {
            const double r = q / p;
            if (p < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
        }
    }
    out[0] = x0 + t0 * dx; out[1] = y0 + t0 * dy; out[2] = x0 + t1 * dx; out[3] = y0 + t1 * dy;
    return true;
}

inline void LinePen::to(double x, double y) {
    if (havePrev) {
        double cl[4];
        if (clipSeg(px, py, x, y, -20, -20, W + 20, H + 20, cl)) {
            if (dashOn <= 0) r.line(cl[0], cl[1], cl[2], cl[3], width, c);
            else dashed(cl[0], cl[1], cl[2], cl[3]);
        }
    }
    px = x; py = y; havePrev = true;
}

inline void LinePen::dashed(double x0, double y0, double x1, double y1) {
    const double len = std::hypot(x1 - x0, y1 - y0), period = dashOn + dashOff;
    double s = 0;
    while (s < len) {
        const double inPeriod = std::fmod(dashPos, period);
        const double step = std::min(len - s, inPeriod < dashOn ? dashOn - inPeriod : period - inPeriod);
        if (inPeriod < dashOn) r.line(x0 + (x1 - x0) * s / len, y0 + (y1 - y0) * s / len, x0 + (x1 - x0) * (s + step) / len, y0 + (y1 - y0) * (s + step) / len, width, c);
        s += step; dashPos += step;
    }
}

}  // namespace orbit
