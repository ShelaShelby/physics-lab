// Black Hole Lab (0.55.4): one true exposure and the eye's glare (light.h says where every number and formula comes from).
#include "blackhole/light.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

#include "blackhole/low_priority.h"
#include "orbit/sky.h"
#include "orbit/units.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;
}  // namespace

// ---- part B ----
double sunSolidAngle() { const double a = units::kSunRadius / units::kAU; return kPi * a * a; }
double starLight(double m) { return sunSolidAngle() * std::pow(10.0, -0.4 * (m - kSunV)); }
double starSurface(double m, double omegaSr) { return starLight(m) / omegaSr; }
double starDotArea() { return kPi * orbit::kStarMinRadius * orbit::kStarMinRadius; }
double starTrueLight(double m, double white, double ptSr) { return starLight(m) / (white * ptSr); }
bool starVisible(double m, double white, double ptSr) {
    // the dot's light per square point (its opacity, for a dot no larger than the smallest) reaches the screen's faintest step; a larger dot is at opacity 1
    return orbit::starDrawLight(starTrueLight(m, white, ptSr)).alpha >= kScreenStep;
}
double pointSolidAngle(double K, double cx, double cy, double X, double Y) {
    const double th = std::hypot(X - cx, Y - cy) / K;
    return (th > 1e-9 ? std::sin(th) / th : 1.0) / (K * K);
}
TrueSkyNumbers trueSkyNumbers(double brightMag, double discY, double white, double ptSr) {
    TrueSkyNumbers t;
    t.starY = starSurface(brightMag, starDotArea() * ptSr);
    t.fainter = t.starY > 0 ? discY / t.starY : 0;
    t.belowStep = kScreenStep * white / t.starY;
    t.showsAtWhite = t.starY / kScreenStep;
    return t;
}

// ---- part C ----
double glarePsf(double th, double age, double p) {
    if (!(th >= kGlareMinDeg && th <= kGlareMaxDeg)) return 0;
    const double a = age / 62.5, a4 = a * a * a * a;
    return 10 / (th * th * th) + (5 / (th * th) + 0.1 * p / th) * (1 + a4) + 2.5e-3 * p;
}

namespace {
// Simpson's rule in u = ln(theta) (the PSF falls like theta^-3 near 0.1 degrees): Int Psi(theta) 2 pi sin(theta rad) (theta rad) du
double shareSimpson(double age, double p) {
    const int n = 200000;
    const double u0 = std::log(kGlareMinDeg), u1 = std::log(kGlareMaxDeg), h = (u1 - u0) / n;
    double s = 0;
    for (int i = 0; i <= n; i++) {
        const double th = std::exp(u0 + i * h), f = glarePsf(std::min(std::max(th, kGlareMinDeg), kGlareMaxDeg), age, p) * 2 * kPi * std::sin(th * kDeg) * th * kDeg;
        s += f * (i == 0 || i == n ? 1 : (i % 2 ? 4 : 2));
    }
    return s * h / 3;
}
}  // namespace

double glareShare(double age, double p) {
    if (age == kEyeAge && p == kEyePigment) { static const double s = shareSimpson(kEyeAge, kEyePigment); return s; }
    return shareSimpson(age, p);
}

double glareShareGauss(double age, double p) {
    // 64-point Gauss-Legendre on each of 400 pieces of theta (degrees), the pieces even in ln(theta); nodes by Newton on P_64
    static std::vector<double> x, w;
    if (x.empty()) {
        const int n = 64;
        for (int i = 0; i < n; i++) {
            double z = std::cos(kPi * (i + 0.75) / (n + 0.5)), pp = 0;
            for (int it = 0; it < 100; it++) {
                double p0 = 1, p1 = 0;
                for (int j = 1; j <= n; j++) { const double p2 = p1; p1 = p0; p0 = ((2.0 * j - 1) * z * p1 - (j - 1.0) * p2) / j; }
                pp = n * (z * p0 - p1) / (z * z - 1);
                const double dz = p0 / pp;
                z -= dz;
                if (std::fabs(dz) < 1e-16) break;
            }
            x.push_back(z); w.push_back(2 / ((1 - z * z) * pp * pp));
        }
    }
    const int pieces = 400;
    const double l0 = std::log(kGlareMinDeg), l1 = std::log(kGlareMaxDeg);
    double s = 0;
    for (int k = 0; k < pieces; k++) {
        const double a = std::exp(l0 + (l1 - l0) * k / pieces), b = std::exp(l0 + (l1 - l0) * (k + 1) / pieces), m = (a + b) / 2, r = (b - a) / 2;
        for (std::size_t i = 0; i < x.size(); i++) { const double th = m + r * x[i]; s += w[i] * r * glarePsf(th, age, p) * 2 * kPi * std::sin(th * kDeg) * kDeg; }
    }
    return s;
}

GlareGrid glareGrid(int W, int H, double cx, double cy, double K) {
    GlareGrid g;
    g.W = W; g.H = H; g.cx = cx; g.cy = cy; g.K = K;
    g.b = std::max(1, static_cast<int>(std::lround(kGlareBlockDeg * kDeg * K)));
    g.nx = (W + g.b - 1) / g.b; g.ny = (H + g.b - 1) / g.b;
    return g;
}

namespace {
struct V3 { double x, y, z; };
// the direction (camera frame) of the point (X, Y) of the picture (pixel coordinates, the middle at (cx, cy)): schwarzschild.h viewDirection's map
V3 dirAt(const GlareGrid& g, double X, double Y) {
    const double dx = (X - g.cx) / g.K, dy = (Y - g.cy) / g.K, th = std::min(std::hypot(dx, dy), kPi), s = th > 1e-12 ? std::sin(th) / th : 1;
    return {s * dx, s * dy, -std::cos(th)};
}
double angleDeg(const V3& a, const V3& b) {
    const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z, c = std::sqrt(dx * dx + dy * dy + dz * dz) / 2;
    return 2 * std::asin(std::min(1.0, c)) / kDeg;
}
// the pixel range of block k of a level whose blocks are f finest blocks of b pixels
void span(int k, int f, int b, int n, int& a0, int& a1) { a0 = k * f * b; a1 = std::min((k + 1) * f * b, n); }

template <class F>
void runRows(int n, const F& fn) {
    const int threads = std::max(1, std::min(8, static_cast<int>(std::thread::hardware_concurrency())));
    if (threads == 1 || n < 16) { fn(0, n); return; }
    std::vector<std::thread> t;
    const bool low = thisThreadLow();                                   // 0.55.5 (low_priority.h)
    for (int k = 0; k < threads; k++) t.emplace_back([&fn, n, k, threads, low] { if (low) lowerThisThread(); fn(n * k / threads, n * (k + 1) / threads); });
    for (std::thread& x : t) x.join();
}

// THE NEAR WEIGHTS. Between blocks within kGlareNear of each other (the block itself too) Psi changes fast, so its mean over the two blocks is used, on the
// picture's own local map: at angle th from the middle the map stretches nothing along the radius and shrinks the across direction by f = sin(th) / th
// (schwarzschild.h viewDirection). Tabulated against f (kNf values) and the radius's direction phi (kNp values over half a turn), for the finest block of a
// degrees; looked up bilinearly. The mean over two unit squares (di, dj) apart is Psi weighted by their overlap, the tent L(dx - di) L(dy - dj),
// L(t) = max(0, 1 - |t|), integrated in polar coordinates round the source on the local map's angles (u = M d, d = M^-1 u, dd = du / f), even in ln(rho)
// from the model's 0.1 degrees out (kNearR x kNearA points). A pair farther apart: Psi at the exact angle between the source's light and the target's
// centre, times the ratio of Psi's mean over the target block (and, at the finest level, the source block) to its value at the centre, on a flat patch.
constexpr int kNf = 9, kNp = 16, kNearR = 64, kNearA = 64, kNW = 2 * kGlareNear + 1;
constexpr double kFmin = 0.05;
double nearMean(double a, double f, double phi, int di, int dj, double age, double p) {
    const double c = std::cos(phi), s = std::sin(phi), r0 = kGlareMinDeg / a, r1 = 1.41421356237 * (std::max(std::abs(di), std::abs(dj)) + 1);
    if (!(r1 > r0)) return 0;
    const double l0 = std::log(r0), dl = (std::log(r1) - l0) / kNearR, da = 2 * kPi / kNearA;
    double sum = 0;
    for (int ir = 0; ir < kNearR; ir++) {
        const double rho = std::exp(l0 + (ir + 0.5) * dl), wr = glarePsf(rho * a, age, p) * rho * rho * dl * da;   // rho drho = rho^2 dln(rho)
        for (int ia = 0; ia < kNearA; ia++) {
            const double al = (ia + 0.5) * da, ur = rho * std::cos(al), ut = rho * std::sin(al) / f;   // back to blocks: the across part stretched by 1 / f
            const double dx = ur * c - ut * s, dy = ur * s + ut * c;
            sum += wr * std::max(0.0, 1 - std::fabs(dx - di)) * std::max(0.0, 1 - std::fabs(dy - dj));
        }
    }
    return sum / f;
}
struct NearTable { double a = -1, age = -1, p = -1; std::vector<double> w; };
const NearTable& nearTable(double a, double age, double p) {
    static NearTable T;
    static std::mutex mx;
    std::lock_guard<std::mutex> lk(mx);
    if (T.a == a && T.age == age && T.p == p) return T;
    T.a = a; T.age = age; T.p = p;
    T.w.assign(static_cast<std::size_t>(kNf) * kNp * kNW * kNW, 0.0);
    runRows(kNf * kNp, [&](int k0, int k1) {
        for (int k = k0; k < k1; k++) {
            const double f = kFmin + (1 - kFmin) * (k / kNp) / (kNf - 1), phi = kPi * (k % kNp) / kNp;
            for (int dj = -kGlareNear; dj <= kGlareNear; dj++)
                for (int di = -kGlareNear; di <= kGlareNear; di++)
                    T.w[(static_cast<std::size_t>(k) * kNW + (dj + kGlareNear)) * kNW + (di + kGlareNear)] = nearMean(a, f, phi, di, dj, age, p);
        }
    });
    return T;
}
// the near weight for a source at angle th (radians) from the middle in the direction phi, the target (di, dj) blocks away
double nearWeight(const NearTable& T, double th, double phi, int di, int dj) {
    const double f = th > 1e-9 ? std::sin(th) / th : 1.0;
    double u = std::clamp((f - kFmin) / (1 - kFmin) * (kNf - 1), 0.0, kNf - 1.0), v = phi / kPi * kNp;
    v -= kNp * std::floor(v / kNp);
    const int i0 = std::min(static_cast<int>(u), kNf - 2), j0 = static_cast<int>(v) % kNp, j1 = (j0 + 1) % kNp;
    const double fu = u - i0, fv = v - std::floor(v);
    const std::size_t o = static_cast<std::size_t>(dj + kGlareNear) * kNW + (di + kGlareNear);
    const auto at = [&](int a, int b) { return T.w[(static_cast<std::size_t>(a) * kNp + b) * kNW * kNW + o]; };
    return (1 - fu) * ((1 - fv) * at(i0, j0) + fv * at(i0, j1)) + fu * ((1 - fv) * at(i0 + 1, j0) + fv * at(i0 + 1, j1));
}
// the flat ratio of Psi's mean over the target block (n x n points; with `both`, over the source block too) to its value between the centres
double flatRatio(double a, int di, int dj, bool both, double age, double p) {
    const double c = glarePsf(std::hypot(di, dj) * a, age, p);
    if (!(c > 0)) return 1.0;
    const int n = 4;
    double s = 0;
    const int ns = both ? n : 1;
    for (int sy = 0; sy < ns; sy++)
        for (int sx = 0; sx < ns; sx++)
            for (int ty = 0; ty < n; ty++)
                for (int tx = 0; tx < n; tx++) {
                    const double ox = both ? (sx + 0.5) / n : 0.5, oy = both ? (sy + 0.5) / n : 0.5;
                    s += glarePsf(std::hypot(di + (tx + 0.5) / n - ox, dj + (ty + 0.5) / n - oy) * a, age, p);
                }
    return s / (static_cast<double>(n) * n * ns * ns) / c;
}
constexpr int kCorrR = 4 * kGlareNear + 3;   // the largest offset a level handles (its parents' neighbours' children)

struct Level {
    int nx = 0, ny = 0, f = 1, np = 1;  // np: sample points per block (1 at the finest level, its centre; 2 x 2 above, its corner children's centres)
    std::vector<double> E;              // 3 per block: Sum of mean light x solid angle (white x sr)
    std::vector<V3> sdir;               // as a source: the light-weighted direction of its light
    std::vector<V3> pdir;               // np per block: the sample points' directions
    std::vector<double> px, py;         // np per block: the sample points (pixels)
    std::vector<unsigned char> any;     // the block has light
    std::vector<double> S;              // 3 per sample point: the veil there from this level's pairs and the coarser levels'
    std::vector<double> corr;           // per offset (di, dj): flatRatio
    std::vector<double> om, th, phi;    // the finest level: each block's solid angle, its angle from the middle and the radius's direction
    std::vector<double> M;              // 6 per block: the spread of its light about sdir (light-weighted second moment of the directions; xx yy zz xy xz yz, rad^2)
};
// Psi's first and second derivatives (per degree) for the far pairs' spread correction
void psiDerivs(double th, double age, double p, double& d1, double& d2) {
    const double a = age / 62.5, a4 = 1 + a * a * a * a;
    d1 = -30 / std::pow(th, 4) + (-10 / std::pow(th, 3) - 0.1 * p / (th * th)) * a4;
    d2 = 120 / std::pow(th, 5) + (30 / std::pow(th, 4) + 0.2 * p / std::pow(th, 3)) * a4;
}
// A far source block of spread M seen from the direction t: Psi at its light's centre c plus the second-order term of its spread,
// (1/2)[Psi'' m_rr + Psi' / theta m_tt] (m_rr: the spread along the arc from c towards t, m_tt: across it; degrees)
double psiSpread(const V3& t, const V3& c, const double* M, double age, double p) {
    const double th = angleDeg(t, c), w = glarePsf(th, age, p);
    if (!(w > 0) || !(M[0] + M[1] + M[2] > 0)) return w;
    const double tc = t.x * c.x + t.y * c.y + t.z * c.z;
    double rx = t.x - tc * c.x, ry = t.y - tc * c.y, rz = t.z - tc * c.z;
    const double rn = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (!(rn > 0)) return w;
    rx /= rn; ry /= rn; rz /= rn;
    const double d2r = 1 / (kDeg * kDeg);
    const double mrr = (M[0] * rx * rx + M[1] * ry * ry + M[2] * rz * rz + 2 * (M[3] * rx * ry + M[4] * rx * rz + M[5] * ry * rz)) * d2r, mtt = (M[0] + M[1] + M[2]) * d2r - mrr;
    double d1, d2;
    psiDerivs(th, age, p, d1, d2);
    return std::max(0.0, w + 0.5 * (d2 * mrr + d1 / th * mtt));
}
}  // namespace

double glareBlockSolidAngle(const GlareGrid& g, int i, int j) {
    int x0, x1, y0, y1;
    span(i, 1, g.b, g.W, x0, x1); span(j, 1, g.b, g.H, y0, y1);
    if (x1 <= x0 || y1 <= y0) return 0;
    return (x1 - x0) * (y1 - y0) * pointSolidAngle(g.K, g.cx, g.cy, (x0 + x1) / 2.0, (y0 + y1) / 2.0);
}

void glareVeil(const GlareGrid& g, const std::vector<float>& src, std::vector<float>& veil, GlareSums* sums, double age, double p) {
    const auto t0 = std::chrono::steady_clock::now();
    const double s = glareShare(age, p);
    const int D = kGlareNear, CW = 2 * kCorrR + 1;
    const double a0 = g.b / g.K / kDeg;                                 // the finest block, degrees (at the middle of the picture)
    const NearTable& NT = nearTable(a0, age, p);
    std::vector<Level> L(1);
    L[0].nx = g.nx; L[0].ny = g.ny; L[0].f = 1; L[0].np = 1;
    double total = 0;
    while (true) {                                                      // the levels: each 4 x 4 blocks of the one below, up to a grid of a few blocks
        const bool fine = L.size() == 1;
        Level& v = L.back();
        const std::size_t n = static_cast<std::size_t>(v.nx) * v.ny;
        v.E.assign(3 * n, 0); v.sdir.resize(n); v.any.assign(n, 0);
        v.pdir.resize(n * v.np); v.px.resize(n * v.np); v.py.resize(n * v.np); v.S.assign(3 * n * v.np, 0);
        if (fine) { v.om.assign(n, 0); v.th.assign(n, 0); v.phi.assign(n, 0); }
        v.M.assign(6 * n, 0);
        for (int j = 0; j < v.ny; j++)
            for (int i = 0; i < v.nx; i++) {
                int x0, x1, y0, y1;
                span(i, v.f, g.b, g.W, x0, x1); span(j, v.f, g.b, g.H, y0, y1);
                const std::size_t q = static_cast<std::size_t>(j) * v.nx + i;
                const double X = (x0 + x1) / 2.0, Y = (y0 + y1) / 2.0;
                v.sdir[q] = dirAt(g, X, Y);
                if (fine) {
                    v.px[q] = X; v.py[q] = Y; v.pdir[q] = v.sdir[q];
                    const double om = glareBlockSolidAngle(g, i, j);
                    v.om[q] = om; v.th[q] = std::hypot(X - g.cx, Y - g.cy) / g.K; v.phi[q] = std::atan2(Y - g.cy, X - g.cx);
                    for (int k = 0; k < 3; k++) { v.E[3 * q + k] = static_cast<double>(src[3 * q + k]) * om; total += v.E[3 * q + k]; }
                    const double s2 = om / 12;                          // a square of solid angle om: om / 12 along each of its two sides
                    const V3 c = v.sdir[q];
                    v.M[6 * q] = s2 * (1 - c.x * c.x); v.M[6 * q + 1] = s2 * (1 - c.y * c.y); v.M[6 * q + 2] = s2 * (1 - c.z * c.z);
                    v.M[6 * q + 3] = -s2 * c.x * c.y; v.M[6 * q + 4] = -s2 * c.x * c.z; v.M[6 * q + 5] = -s2 * c.y * c.z;
                } else {                                                // a source's direction: the light-weighted mean of its parts' (a point stays a point)
                    for (int k = 0; k < 9; k++) {                       // 3 x 3 points: its corner children's centres and its middle
                        const double fr[3] = {0.125, 0.5, 0.875};
                        const double sx = x0 + (x1 - x0) * fr[k % 3], sy = y0 + (y1 - y0) * fr[k / 3];
                        v.px[9 * q + k] = sx; v.py[9 * q + k] = sy; v.pdir[9 * q + k] = dirAt(g, sx, sy);
                    }
                    const Level& c = L[L.size() - 2];
                    double sx = 0, sy = 0, sz = 0;
                    for (int jj = 4 * j; jj < std::min(4 * j + 4, c.ny); jj++)
                        for (int ii = 4 * i; ii < std::min(4 * i + 4, c.nx); ii++) {
                            const std::size_t r = static_cast<std::size_t>(jj) * c.nx + ii;
                            for (int k = 0; k < 3; k++) v.E[3 * q + k] += c.E[3 * r + k];
                            const double w = c.E[3 * r] + c.E[3 * r + 1] + c.E[3 * r + 2];
                            sx += w * c.sdir[r].x; sy += w * c.sdir[r].y; sz += w * c.sdir[r].z;
                        }
                    const double m = std::sqrt(sx * sx + sy * sy + sz * sz);
                    if (m > 0) {
                        v.sdir[q] = {sx / m, sy / m, sz / m};
                        const V3 c0 = v.sdir[q];
                        double W = 0, Mq[6] = {0, 0, 0, 0, 0, 0};       // the parts' spreads and their places about the centre, light-weighted
                        for (int jj = 4 * j; jj < std::min(4 * j + 4, c.ny); jj++)
                            for (int ii = 4 * i; ii < std::min(4 * i + 4, c.nx); ii++) {
                                const std::size_t r = static_cast<std::size_t>(jj) * c.nx + ii;
                                const double w = c.E[3 * r] + c.E[3 * r + 1] + c.E[3 * r + 2];
                                if (!(w > 0)) continue;
                                const double ex = c.sdir[r].x - c0.x, ey = c.sdir[r].y - c0.y, ez = c.sdir[r].z - c0.z;
                                const double o[6] = {ex * ex, ey * ey, ez * ez, ex * ey, ex * ez, ey * ez};
                                for (int k = 0; k < 6; k++) Mq[k] += w * (c.M[6 * r + k] + o[k]);
                                W += w;
                            }
                        for (int k = 0; k < 6; k++) v.M[6 * q + k] = Mq[k] / W;
                    }
                }
                v.any[q] = (v.E[3 * q] > 0 || v.E[3 * q + 1] > 0 || v.E[3 * q + 2] > 0) ? 1 : 0;
            }
        if (v.nx <= 2 * D + 2 && v.ny <= 2 * D + 2) break;
        Level up;
        up.nx = (v.nx + 3) / 4; up.ny = (v.ny + 3) / 4; up.f = v.f * 4; up.np = 9;
        L.push_back(std::move(up));
    }
    const int top = static_cast<int>(L.size()) - 1;
    for (int l = 0; l <= top; l++) {                                    // the block-size ratios of the finest level (the coarser ones lay the veil at points)
        Level& v = L[static_cast<std::size_t>(l)];
        v.corr.assign(static_cast<std::size_t>(CW) * CW, 1.0);
        for (int dj = -kCorrR; dj <= kCorrR; dj++)
            for (int di = -kCorrR; di <= kCorrR; di++)
                if (l == 0 && std::max(std::abs(di), std::abs(dj)) > D) v.corr[static_cast<std::size_t>(dj + kCorrR) * CW + (di + kCorrR)] = flatRatio(a0, di, dj, true, age, p);
    }
    for (int l = top; l >= 0; l--) {                                    // each pair once: separated at this level, their parents not (the top: all separated)
        Level& v = L[static_cast<std::size_t>(l)];
        if (l < top) {                                                  // the coarser levels' veil, by quadratic interpolation inside each parent between its 3 x 3 points
            const Level& c = L[static_cast<std::size_t>(l + 1)];
            for (int j = 0; j < v.ny; j++)
                for (int i = 0; i < v.nx; i++) {
                    const std::size_t pq = static_cast<std::size_t>(j / 4) * c.nx + (i / 4), q = static_cast<std::size_t>(j) * v.nx + i;
                    const double xs[3] = {c.px[9 * pq], c.px[9 * pq + 1], c.px[9 * pq + 2]}, ys[3] = {c.py[9 * pq], c.py[9 * pq + 3], c.py[9 * pq + 6]};
                    for (int k = 0; k < v.np; k++) {
                        const std::size_t t = q * v.np + k;
                        double wx[3], wy[3];
                        for (int m = 0; m < 3; m++) {                   // Lagrange's weights on the three nodes
                            wx[m] = 1; wy[m] = 1;
                            for (int o = 0; o < 3; o++) if (o != m) { wx[m] *= (v.px[t] - xs[o]) / (xs[m] - xs[o]); wy[m] *= (v.py[t] - ys[o]) / (ys[m] - ys[o]); }
                        }
                        for (int ch = 0; ch < 3; ch++) {
                            double sum = 0;
                            for (int my = 0; my < 3; my++)
                                for (int mx = 0; mx < 3; mx++) sum += wx[mx] * wy[my] * c.S[3 * (9 * pq + 3 * my + mx) + ch];
                            v.S[3 * t + ch] = std::max(0.0, sum);
                        }
                    }
                }
        }
        runRows(v.ny, [&](int j0, int j1) {
            for (int j = j0; j < j1; j++)
                for (int i = 0; i < v.nx; i++) {
                    int ia, ib, ja, jb;
                    if (l == top) { ia = 0; ib = v.nx - 1; ja = 0; jb = v.ny - 1; }
                    else {
                        ia = std::max(0, (i / 4 - D) * 4); ib = std::min(v.nx - 1, (i / 4 + D) * 4 + 3);
                        ja = std::max(0, (j / 4 - D) * 4); jb = std::min(v.ny - 1, (j / 4 + D) * 4 + 3);
                    }
                    const std::size_t q = static_cast<std::size_t>(j) * v.nx + i;
                    for (int k = 0; k < v.np; k++) {
                        const V3 d = v.pdir[q * v.np + k];
                        double a0s = 0, a1s = 0, a2s = 0;
                        for (int jj = ja; jj <= jb; jj++)
                            for (int ii = ia; ii <= ib; ii++) {
                                const int di = i - ii, dj = j - jj, cd = std::max(std::abs(di), std::abs(dj));
                                if (cd <= D && l > 0) continue;           // near pairs belong to a finer level (the finest keeps them, the block itself too)
                                const std::size_t r = static_cast<std::size_t>(jj) * v.nx + ii;
                                if (!v.any[r]) continue;
                                double w;
                                if (cd <= D) w = nearWeight(NT, v.th[r], v.phi[r], di, dj);   // the source's own local map
                                else if (l == 0) w = glarePsf(angleDeg(d, v.sdir[r]), age, p) *
                                                     (cd <= kCorrR ? v.corr[static_cast<std::size_t>(dj + kCorrR) * CW + (di + kCorrR)] : 1.0);   // (a one-level picture's far pairs: 1)
                                else w = psiSpread(d, v.sdir[r], &v.M[6 * r], age, p);   // a coarse source: its centre and its spread
                                if (w <= 0) continue;
                                a0s += w * v.E[3 * r]; a1s += w * v.E[3 * r + 1]; a2s += w * v.E[3 * r + 2];
                            }
                        const std::size_t t = q * v.np + k;
                        v.S[3 * t] += a0s; v.S[3 * t + 1] += a1s; v.S[3 * t + 2] += a2s;
                    }
                }
        });
    }
    const Level& f = L[0];
    veil.assign(f.S.size(), 0);
    double laid = 0;
    for (std::size_t q = 0; q < f.S.size(); q++) {
        veil[q] = static_cast<float>(f.S[q]);
        laid += f.S[q] * f.om[q / 3];
    }
    if (sums) {
        sums->spread = s * total;
        sums->laid = laid;
        sums->ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
}

void glareTables(const GlareGrid& g, std::vector<float>& nearW, std::vector<float>& corr, double age, double p) {
    const double a0 = g.b / g.K / kDeg;                                 // the finest block, degrees (as glareVeil)
    const NearTable& NT = nearTable(a0, age, p);
    nearW.resize(NT.w.size());
    for (std::size_t k = 0; k < NT.w.size(); k++) nearW[k] = static_cast<float>(NT.w[k]);
    const int CW = 2 * kCorrR + 1;
    corr.assign(static_cast<std::size_t>(CW) * CW, 1.0f);
    for (int dj = -kCorrR; dj <= kCorrR; dj++)
        for (int di = -kCorrR; di <= kCorrR; di++)
            if (std::max(std::abs(di), std::abs(dj)) > kGlareNear) corr[static_cast<std::size_t>(dj + kCorrR) * CW + (di + kCorrR)] = static_cast<float>(flatRatio(a0, di, dj, true, age, p));
}

void glareVeilDirect(const GlareGrid& g, const std::vector<float>& src, std::vector<float>& veil, double age, double p) {
    const std::size_t n = static_cast<std::size_t>(g.nx) * g.ny;
    std::vector<V3> dir(n);
    std::vector<double> E(3 * n), th(n), phi(n);
    for (int j = 0; j < g.ny; j++)
        for (int i = 0; i < g.nx; i++) {
            int x0, x1, y0, y1;
            span(i, 1, g.b, g.W, x0, x1); span(j, 1, g.b, g.H, y0, y1);
            const std::size_t q = static_cast<std::size_t>(j) * g.nx + i;
            const double X = (x0 + x1) / 2.0, Y = (y0 + y1) / 2.0;
            dir[q] = dirAt(g, X, Y);
            th[q] = std::hypot(X - g.cx, Y - g.cy) / g.K; phi[q] = std::atan2(Y - g.cy, X - g.cx);
            for (int k = 0; k < 3; k++) E[3 * q + k] = src[3 * q + k] * glareBlockSolidAngle(g, i, j);
        }
    // the finest level's weights for every pair (near: the local-map table; the rest: Psi at the centres' exact angle x the flat ratio of both blocks)
    const double a = g.b / g.K / kDeg;
    const NearTable& NT = nearTable(a, age, p);
    const int CX = g.nx, CY = g.ny, CW = 2 * CX + 1;
    std::vector<double> corr(static_cast<std::size_t>(CW) * (2 * CY + 1), -1.0);
    veil.assign(3 * n, 0);
    for (std::size_t q = 0; q < n; q++)
        for (std::size_t r = 0; r < n; r++) {
            if (!(E[3 * r] > 0 || E[3 * r + 1] > 0 || E[3 * r + 2] > 0)) continue;
            const int di = static_cast<int>(q % g.nx) - static_cast<int>(r % g.nx), dj = static_cast<int>(q / g.nx) - static_cast<int>(r / g.nx);
            double w;
            if (std::max(std::abs(di), std::abs(dj)) <= kGlareNear) w = nearWeight(NT, th[r], phi[r], di, dj);
            else {
                double& c = corr[static_cast<std::size_t>(dj + CY) * CW + (di + CX)];
                if (c < 0) c = flatRatio(a, di, dj, true, age, p);
                w = glarePsf(angleDeg(dir[q], dir[r]), age, p) * c;
            }
            for (int k = 0; k < 3; k++) veil[3 * q + k] += static_cast<float>(w * E[3 * r + k]);
        }
}

}  // namespace blackhole
