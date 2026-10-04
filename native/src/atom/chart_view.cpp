#include "chart_view.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>

#include "atom_core.h"
#include "nubase.h"

namespace atom {

// ---------------------------------------------------------------- the height of a tile
double chartHeightFromHalfLife(double seconds) {
    if (!(seconds > 0)) return 0;
    return std::max(0.0, std::min(1.0, (std::log10(seconds) - kHeightLog10Lo) / (kHeightLog10Hi - kHeightLog10Lo)));
}

namespace {
// ">4.6 zs" -> 4.6e-21 s. The units NUBASE2020 uses in its half-life column: SI prefixes on the second (y yocto, z zepto, a atto, p pico, n nano, u micro, m milli), m h d (minute, hour, day: exact), y the year
// (the lab's kYearS, 365.25 days: the file's header does not define its year; a different year moves a height by far less than a pixel) and its multiples ky My Gy Ty Py Ey Zy Yy.
double secondsOf(const char* text) {
    const char* p = text;
    while (*p == '<' || *p == '>' || *p == '~' || *p == ' ') p++;
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p) return NAN;
    while (*end == ' ') end++;
    static const struct { const char* u; double s; } unit[] = {
        {"ys", 1e-24}, {"zs", 1e-21}, {"as", 1e-18}, {"ps", 1e-12}, {"ns", 1e-9}, {"us", 1e-6}, {"ms", 1e-3}, {"s", 1}, {"m", 60}, {"h", 3600}, {"d", 86400}, {"y", kYearS},
        {"ky", 1e3 * kYearS}, {"My", 1e6 * kYearS}, {"Gy", 1e9 * kYearS}, {"Ty", 1e12 * kYearS}, {"Py", 1e15 * kYearS}, {"Ey", 1e18 * kYearS}, {"Zy", 1e21 * kYearS}, {"Yy", 1e24 * kYearS}};
    for (const auto& u : unit) if (std::strcmp(end, u.u) == 0) return v * u.s;
    return NAN;
}

ChartHeight computeHeight(int Z, int N) {
    const std::string* c = nucCode(Z, N);
    const NubaseRow* nb = nubase(Z, N);
    if (!c) return {0, HeightKind::Unknown};
    if (*c == "S") return {kStableHeight, nb && (*nb->lim || *nb->modes) ? HeightKind::StableNoted : HeightKind::Stable};
    if (*c == "?") {
        if (nb && (nb->kind == 'B' || nb->kind == 'A')) {
            const double s = secondsOf(nb->lim);
            if (s > 0) return {chartHeightFromHalfLife(s), nb->kind == 'B' ? HeightKind::Limit : HeightKind::Approx};
        }
        return {0, nb && nb->kind == 'E' ? HeightKind::Extrapolated : HeightKind::Unknown};
    }
    const auto d = decInfo(Z, N);
    if (!d || !d->T) return {0, HeightKind::Unknown};
    return {chartHeightFromHalfLife(*d->T), (*c)[0] == '~' ? HeightKind::Approx : HeightKind::Measured};
}

const std::vector<ChartHeight>& heights() {            // aligned with chartTiles()
    static const std::vector<ChartHeight> v = [] { std::vector<ChartHeight> r; for (const ChartTile& t : chartTiles()) r.push_back(computeHeight(t.Z, t.N)); return r; }();
    return v;
}
}  // namespace

ChartHeight chartHeightOf(int Z, int N) {
    const std::vector<ChartTile>& t = chartTiles();
    const auto it = std::lower_bound(t.begin(), t.end(), std::make_pair(Z, N), [](const ChartTile& a, const std::pair<int, int>& k) { return a.Z != k.first ? a.Z < k.first : a.N < k.second; });
    if (it == t.end() || it->Z != Z || it->N != N) return {0, HeightKind::Unknown};
    return heights()[std::size_t(it - t.begin())];
}
int chartHeightCount(HeightKind k) {
    static const std::vector<int> n = [] { std::vector<int> a(7, 0); for (const ChartHeight& h : heights()) a[int(h.kind)]++; return a; }();
    return n[int(k)];
}
const char* chartHeightWords() {
    return "Height: the log of the half-life, a scale chosen for this picture, not a measurement. Each step up is ten times longer, from 10^-23 s (flat) to 10^32 s (the top). "
           "Stable nuclides have no half-life: all stand at one fixed height above the top, so they read as no decay seen, not as a long number. A pale block is only a limit or an approximate value. "
           "A pale flat square has only an extrapolated half-life and a hollow one has none: no height is invented for them.";
}
const char* chartHeightShort() { return "Height: log10 of the half-life, 10^-23 s to 10^32 s; stable = one fixed height above; pale = limit or approximate; flat pale or hollow = unknown."; }

// ---------------------------------------------------------------- the projection
void ChartProjection::map(double a, double b, double z, double& sx, double& sy, double& depth) const {
    if (identity) { sx = a; sy = b; depth = z; return; }
    double rx, ry, rd;
    if (flat) { rx = a; ry = b; rd = z; }
    else {
        const double u = a - cx, v = b - cy, x = u * cosY - v * sinY, y = u * sinY + v * cosY;
        rx = cx + x; ry = cy + (y * cosP - z * sinP); rd = y * sinP + z * cosP;
    }
    sx = fx + K * (rx - cx); sy = fy + K * (ry - cy); depth = K * rd;
}
void ChartProjection::unmap(double sx, double sy, double depth, double& a, double& b, double& z) const {
    const double x = identity ? sx - cx : (sx - fx) / K, ry = identity ? sy - cy : (sy - fy) / K, rd = identity ? depth : depth / K;
    const double y = ry * cosP + rd * sinP;
    z = -ry * sinP + rd * cosP;
    a = cx + x * cosY + y * sinY;
    b = cy - x * sinY + y * cosY;
}

const std::vector<int>& chartPaintOrder() {
    static const std::vector<int> o = [] {
        const std::vector<ChartTile>& t = chartTiles();
        std::vector<int> v(t.size());
        for (std::size_t i = 0; i < v.size(); i++) v[i] = int(i);
        const int zMax = chartMaxZ();
        std::sort(v.begin(), v.end(), [&](int i, int j) {
            const int ki = t[std::size_t(i)].N + zMax - t[std::size_t(i)].Z, kj = t[std::size_t(j)].N + zMax - t[std::size_t(j)].Z;
            return ki != kj ? ki < kj : t[std::size_t(i)].N < t[std::size_t(j)].N;
        });
        return v;
    }();
    return o;
}

// ---------------------------------------------------------------- the scene
namespace {
bool inQuad(const float* q, double px, double py) {
    bool pos = false, neg = false;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) & 3;
        const double c = (q[2 * j] - q[2 * i]) * (py - q[2 * i + 1]) - (q[2 * j + 1] - q[2 * i + 1]) * (px - q[2 * i]);
        if (c > 1e-9) pos = true; else if (c < -1e-9) neg = true;
    }
    return !(pos && neg);
}
}  // namespace

void ChartScene::build(double w, double h, const ChartViewState& in) {
    ChartViewState st = in;
    if (w == w_ && h == h_ && st == st_ && !blocks_.empty()) return;
    w_ = w; h_ = h; st_ = st;
    L_ = chartLayout(w, h);
    const std::vector<ChartTile>& tiles = chartTiles();
    const std::vector<int>& order = chartPaintOrder();
    ChartProjection P;
    P.flat = st.tilt == 0;
    P.identity = st.flat();
    const double yaw = st.tilt * kChartYawDeg * kPi / 180, pitch = st.tilt * kChartPitchDeg * kPi / 180;
    P.cosY = std::cos(yaw); P.sinY = std::sin(yaw); P.cosP = std::cos(pitch); P.sinP = std::sin(pitch);
    P.cx = L_.x0 + (L_.nMax + 1) * L_.cell / 2; P.cy = L_.y0 + L_.zMax * L_.cell / 2;
    // pass 1: the blocks projected with K = 1 about the grid's centre (no fitting yet)
    ChartProjection raw = P; raw.identity = false; raw.K = 1; raw.fx = P.cx; raw.fy = P.cy;
    blocks_.assign(tiles.size(), ChartBlock{});
    double minx = DBL_MAX, miny = DBL_MAX, maxx = -DBL_MAX, maxy = -DBL_MAX;
    const auto corner = [&](const ChartProjection& pr, double a, double b, double z, float* out) { double sx, sy, d; pr.map(a, b, z, sx, sy, d); out[0] = float(sx); out[1] = float(sy); };
    for (std::size_t k = 0; k < order.size(); k++) {
        const ChartTile& t = tiles[std::size_t(order[k])];
        const ChartHeight ch = heights()[std::size_t(order[k])];
        ChartBlock& B = blocks_[k];
        B.Z = t.Z; B.N = t.N; B.cls = t.cls; B.kind = ch.kind; B.h = ch.h;
        const double a0 = L_.x(t.N), a1 = a0 + L_.cell, b0 = L_.y(t.Z), b1 = b0 + L_.cell, H = heightPoints(ch.h);
        const double tp[4][3] = {{a0, b0, H}, {a1, b0, H}, {a1, b1, H}, {a0, b1, H}};
        for (int i = 0; i < 4; i++) corner(raw, tp[i][0], tp[i][1], tp[i][2], B.top + 2 * i);
        const double fb[4][3] = {{a0, b1, 0}, {a1, b1, 0}, {a1, b1, H}, {a0, b1, H}}, fa[4][3] = {{a1, b0, 0}, {a1, b1, 0}, {a1, b1, H}, {a1, b0, H}};
        for (int i = 0; i < 4; i++) { corner(raw, fb[i][0], fb[i][1], fb[i][2], B.faceB + 2 * i); corner(raw, fa[i][0], fa[i][1], fa[i][2], B.faceA + 2 * i); }
        B.sides = !P.flat && H * P.sinP > 0.05;
        const int nq = B.sides ? 3 : 1;
        const float* qs[3] = {B.top, B.faceB, B.faceA};
        for (int q = 0; q < nq; q++) for (int i = 0; i < 4; i++) { minx = std::min<double>(minx, qs[q][2 * i]); maxx = std::max<double>(maxx, qs[q][2 * i]); miny = std::min<double>(miny, qs[q][2 * i + 1]); maxy = std::max<double>(maxy, qs[q][2 * i + 1]); }
    }
    // fit: the whole landscape inside the room the layout leaves (the flat chart already fits, so s = 1 at tilt 0), centred more and more as the tilt grows
    const double aw = std::max(10.0, w - L_.left - L_.right), ah = std::max(10.0, h - L_.top - L_.bottom);
    double s = 1, shx = 0, shy = 0;
    if (st.tilt > 0) {
        s = std::min(1.0, std::min(aw / (maxx - minx), ah / (maxy - miny)));
        const double ccx = P.cx + s * ((minx + maxx) / 2 - P.cx), ccy = P.cy + s * ((miny + maxy) / 2 - P.cy);
        shx = st.tilt * (L_.left + aw / 2 - ccx); shy = st.tilt * (L_.top + ah / 2 - ccy);
    }
    const double qx = w / 2, qy = h / 2;
    P.K = st.zoom * s;
    P.fx = qx + st.zoom * (P.cx - qx + shx) + st.panX;
    P.fy = qy + st.zoom * (P.cy - qy + shy) + st.panY;
    proj_ = P;
    // pass 2: into the picture (nothing to do for the flat chart at zoom 1: the points are already stage 2's)
    bx0_ = DBL_MAX; by0_ = DBL_MAX; bx1_ = -DBL_MAX; by1_ = -DBL_MAX;
    for (ChartBlock& B : blocks_) {
        float* all[3] = {B.top, B.faceB, B.faceA};
        const int nq = B.sides ? 3 : 1;
        B.x0 = B.y0 = FLT_MAX; B.x1 = B.y1 = -FLT_MAX;
        for (int q = 0; q < nq; q++) for (int i = 0; i < 4; i++) {
            float* pt = all[q] + 2 * i;
            if (!P.identity) { pt[0] = float(P.fx + P.K * (double(pt[0]) - P.cx)); pt[1] = float(P.fy + P.K * (double(pt[1]) - P.cy)); }
            B.x0 = std::min(B.x0, pt[0]); B.x1 = std::max(B.x1, pt[0]); B.y0 = std::min(B.y0, pt[1]); B.y1 = std::max(B.y1, pt[1]);
        }
        bx0_ = std::min<double>(bx0_, B.x0); bx1_ = std::max<double>(bx1_, B.x1); by0_ = std::min<double>(by0_, B.y0); by1_ = std::max<double>(by1_, B.y1);
    }
    // room per picture column
    const int cols = int(std::ceil(w)) + 1;
    colTop_.assign(std::size_t(cols), FLT_MAX); colBottom_.assign(std::size_t(cols), -FLT_MAX);
    for (const ChartBlock& B : blocks_) {
        const int c0 = std::max(0, int(std::floor(B.x0))), c1 = std::min(cols - 1, int(std::floor(B.x1)));
        for (int c = c0; c <= c1; c++) { colTop_[std::size_t(c)] = std::min(colTop_[std::size_t(c)], B.y0); colBottom_[std::size_t(c)] = std::max(colBottom_[std::size_t(c)], B.y1); }
    }
}

bool ChartScene::pick(double px, double py, int& Z, int& N) const {
    for (std::size_t i = blocks_.size(); i-- > 0;) {
        const ChartBlock& B = blocks_[i];
        if (px < B.x0 || px > B.x1 || py < B.y0 || py > B.y1) continue;
        if (inQuad(B.top, px, py) || (B.sides && (inQuad(B.faceB, px, py) || inQuad(B.faceA, px, py)))) { Z = B.Z; N = B.N; return true; }
    }
    return false;
}

double ChartScene::freeHeight(bool fromTop, double x0, double x1) const {
    const double edgeTop = L_.top, edgeBottom = h_ - L_.bottom;
    double free = edgeBottom - edgeTop;
    const int cols = int(colTop_.size());
    for (int c = std::max(0, int(std::floor(x0))); c <= std::min(cols - 1, int(std::floor(x1))); c++) {
        if (colTop_[std::size_t(c)] == FLT_MAX) continue;
        free = std::min(free, fromTop ? double(colTop_[std::size_t(c)]) - edgeTop : edgeBottom - double(colBottom_[std::size_t(c)]));
    }
    return std::max(0.0, free);
}

ChartViewState ChartScene::clamped(const ChartViewState& in) {
    ChartViewState s = in;
    s.tilt = std::max(0.0, std::min(1.0, s.tilt));
    s.zoom = std::max(1.0, std::min(kChartZoomMax, s.zoom));
    if (s.zoom == 1) { s.panX = s.panY = 0; return s; }
    const double lim = 3 * std::max(w_, h_) * s.zoom;            // a pan beyond this is out of the picture anyway; cutting it first keeps the float corners exact enough below
    s.panX = std::max(-lim, std::min(lim, s.panX)); s.panY = std::max(-lim, std::min(lim, s.panY));
    build(w_, h_, s);
    const double bw = bx1_ - bx0_, bh = by1_ - by0_, needX = std::min(std::min(bw, w_), 120.0), needY = std::min(std::min(bh, h_), 90.0);
    double dx = 0, dy = 0;
    if (bx1_ < needX) dx = needX - bx1_; else if (bx0_ > w_ - needX) dx = (w_ - needX) - bx0_;
    if (by1_ < needY) dy = needY - by1_; else if (by0_ > h_ - needY) dy = (h_ - needY) - by0_;
    s.panX += dx; s.panY += dy;
    return s;
}

}  // namespace atom
