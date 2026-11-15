#include "blackhole/schwarzschild.h"

#include <algorithm>
#include <cmath>

#include "blackhole/elliptic.h"
#include "blackhole/kerr.h"
#include "orbit/sky.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
const double kBc = 3 * std::sqrt(3.0);
constexpr double kXMin = 1e-9;          // the table's first point: x = edge - psi (rad)

// delta = P - 3 from e = b^2 - 27 > 0, by Newton on delta^2 (9 + delta) = e (1 + delta) (the cubic P^3 - b^2 P + 2 b^2 = 0 written about P = 3, so that
// a ray close to the critical one keeps its digits), started above the root from Berens et al. (14d)
double periastronOffset(double e) {
    const double b = std::sqrt(27 + e);
    const double P0 = 2 * b / std::sqrt(3.0) * std::cos(std::acos(-kBc / b) / 3);
    double d = std::max(P0 - 3, 0.0) + std::sqrt(e / 9);
    for (int i = 0; i < 100; i++) {
        const double g = d * d * d + 9 * d * d - e * d - e, dg = 3 * d * d + 18 * d - e;
        const double nd = d - g / dg;
        if (!(nd < d)) break;                                    // monotone from above; stop when it no longer falls
        d = nd;
    }
    return d;
}

struct Luminet { double pref, kc2, Finf, K, P, Qm6; };   // Qm6 = Q - P + 6
Luminet luminet(double e) {
    Luminet L;
    const double d = periastronOffset(e), P = 3 + d, Q = std::sqrt((1 + d) * (9 + d));
    const double QmP = 4 * d / (Q + P);                          // Q^2 - P^2 = 4P - 12
    const double Qm3 = (d * d + 10 * d) / (Q + 3);               // Q^2 - 9 = P^2 + 4P - 21
    L.P = P; L.Qm6 = QmP + 6;
    L.kc2 = (Qm3 + d) / (2 * Q);                                 // 1 - k^2 = (Q + P - 6) / 2Q
    L.pref = 2 * std::sqrt(P / Q);
    L.K = ellKc(L.kc2);
    L.Finf = ellFsc(std::sqrt((QmP + 2) / L.Qm6), std::sqrt(4 / L.Qm6), L.kc2);   // sin^2 zeta_inf = (Q - P + 2) / (Q - P + 6), cos^2 = 4 / (Q - P + 6)
    return L;
}
double luminetFr(const Luminet& L, double r) {                    // F(zeta_r, k): cos^2 zeta_r = 4 (r - P) / (r (Q - P + 6))
    const double c2 = std::max(0.0, 4 * (r - L.P) / (r * L.Qm6));
    return ellFsc(std::sqrt(std::max(0.0, 1 - c2)), std::sqrt(c2), L.kc2);
}

// b < b_c: the swept angle from r out to infinity, b [F3(inf) - F3(r)], Gralla & Lupsasca 2020a case (3) with r2 = 0
double sweepOutBelow(double b, double r) {
    if (b <= 0) return 0;
    const double b2 = b * b, s = std::sqrt(1 - b2 / 27);
    double r1 = -(std::cbrt(b2 * (b2 / 27) / (1 + s)) + std::cbrt(b2 * (1 + s)));   // Cardano (1 - s = (b^2/27) / (1 + s))
    const double e = b2 - 27;
    double d1 = r1 + 6;                                          // refine r1 = -6 + d1 on d1^3 - 18 d1^2 + (81 - e) d1 + 8e = 0 (simple root)
    for (int i = 0; i < 4; i++) {
        const double g = ((d1 - 18) * d1 + (81 - e)) * d1 + 8 * e, dg = (3 * d1 - 36) * d1 + (81 - e);
        d1 -= g / dg;
    }
    r1 = -6 + d1;
    const double b1 = -r1 / 2, a12 = -b2 * d1 / (4 * r1);     // r3 + r4 = -r1; |r3|^2 = -2 b^2 / r1, so a1^2 = -b^2 (6 + r1) / (4 r1)
    const double A = std::sqrt(a12 + b1 * b1), B = std::sqrt(a12 + 9 * b1 * b1);   // (B56) with r2 = 0, b1 - r1 = 3 b1
    const double t = a12 / (b1 * b1), X = (1 + t) * (1 + t / 9);
    const double kc2 = b1 * b1 * (6 * (10 * t / 9 + t * t / 9) / (std::sqrt(X) + 1) - 2 * t) / (4 * A * B);   // 1 - k3 of (B59), without rounding
    const auto F3 = [&](double x3) { return ellFc(std::acos(std::clamp(x3, -1.0, 1.0)), kc2); };
    const double xr = (A * (r - r1) - B * r) / (A * (r - r1) + B * r), xinf = (A - B) / (A + B);   // (B55)
    return b * (F3(xinf) - F3(xr)) / std::sqrt(A * B);           // (B71)
}
}  // namespace

double criticalImpact() { return kBc; }
double periastron(double b) { return 3 + periastronOffset(b * b - 27); }
double deflectionFar(double b) {
    const Luminet L = luminet((b - kBc) * (b + kBc));
    return 2 * L.pref * (L.K - L.Finf) - kPi;
}
double cameraImpact(double rO, double psi) { return rO * std::sin(psi) / std::sqrt(1 - 2 / rO); }
double shadowAngle(double rO) {
    // Synge's sin alpha_sh = 3 sqrt(3) (1 - 2/rO)^(1/2) / rO, on the branch of (7) (alpha_sh > pi/2 inside 3M), taken as atan2(sin, cos) with
    // cos^2 = 1 - sin^2 = (rO^2 - 27 + 54/rO) / rO^2 (the same formula; written so that it keeps its digits at rO = 3M, where asin(1 - tiny) would not)
    const double c = std::sqrt(std::max(0.0, rO * rO - 27 + 54 / rO));
    return std::atan2(kBc * std::sqrt(1 - 2 / rO), rO < 3 ? -c : c);
}
double shadowAngleZN(double rO) {
    const double t = std::atan(std::sqrt(1 - 2 / rO) / std::sqrt(std::max(0.0, 2 / rO - 1 + rO * rO / 27)));
    return rO < 3 ? kPi - t : t;
}

Bend bendFromCameraEdge(double rO, double x) {
    Bend out;
    if (!(x > 0)) return out;                                    // on or past the edge: shadow
    const double edge = kPi - shadowAngle(rO), psi = edge - x, sf = std::sqrt(1 - 2 / rO);
    if (psi < 0) return out;
    out.captured = false;
    if (psi == 0) return out;
    const double b = rO * std::sin(psi) / sf;
    const double dbc = -2 * rO * std::cos(edge - x / 2) * std::sin(x / 2) / sf;   // b - b_c = rO (sin psi - sin edge) / sf
    const bool outward = psi <= kPi / 2;
    if (dbc < 0 || (dbc == 0 && !(rO > 3))) {
        if (outward && dbc < 0) { out.dphi = sweepOutBelow(b, rO); return out; }
        out.captured = true; return out;
    }
    if (!(rO > 3)) { out.captured = true; return out; }
    const Luminet L = luminet(dbc * (b + kBc));
    const double Fr = luminetFr(L, rO);
    out.dphi = outward ? L.pref * (Fr - L.Finf) : L.pref * (2 * L.K - L.Finf - Fr);
    return out;
}
Bend bendFromCamera(double rO, double psi) { return bendFromCameraEdge(rO, kPi - shadowAngle(rO) - psi); }

Vec3 bentDirection(const Vec3& er, const Vec3& look, double cosPsi, double dphi) {
    Vec3 t = look - er * cosPsi;
    const double L = length(t);
    if (!(L > 1e-300)) return dphi == 0 ? er : er * std::cos(dphi);   // straight along the axis: only psi = 0 (dphi = 0) reaches here outside the shadow
    t = t * (1 / L);
    return er * std::cos(dphi) + t * std::sin(dphi);
}

LensTable::LensTable(double rO, int n) : rO_(rO) {
    if (!(rO < 1e300)) { edge_ = kPi; return; }
    edge_ = kPi - shadowAngle(rO);
    s0_ = std::log(kXMin);
    ds_ = (std::log(edge_) - s0_) / (n - 1);
    v_.resize(n);
    for (int i = 0; i < n; i++) v_[i] = bendFromCameraEdge(rO, i == n - 1 ? edge_ : std::exp(s0_ + i * ds_)).dphi;
}

bool LensTable::lookup(double psi, double& dphi) const {
    if (v_.empty()) { dphi = psi; return true; }
    const double x = edge_ - psi;
    if (!(x > 0)) return false;
    const double t = (std::log(x) - s0_) / ds_;
    const int n = size();
    if (t <= 0) { dphi = v_[0] + t * (v_[1] - v_[0]); return true; }
    const int i = std::clamp(static_cast<int>(t), 1, n - 3);    // points i-1 .. i+2
    const double u = t - i, p0 = v_[i - 1], p1 = v_[i], p2 = v_[i + 1], p3 = v_[i + 2];
    // Lagrange through u = -1, 0, 1, 2
    dphi = p0 * (-u * (u - 1) * (u - 2) / 6) + p1 * ((u + 1) * (u - 1) * (u - 2) / 2) + p2 * (-(u + 1) * u * (u - 2) / 2) + p3 * ((u + 1) * u * (u - 1) / 6);
    return true;
}

bool LensTable::lookupF(float psi, float& dphi) const {
    if (v_.empty()) { dphi = psi; return true; }
    const float x = static_cast<float>(edge_) - psi;
    if (!(x > 0)) return false;
    const float t = (std::log(x) - static_cast<float>(s0_)) / static_cast<float>(ds_);
    const int n = size();
    const auto V = [&](int k) { return static_cast<float>(v_[k]); };
    if (t <= 0) { dphi = V(0) + t * (V(1) - V(0)); return true; }
    const int i = std::clamp(static_cast<int>(t), 1, n - 3);
    const float u = t - static_cast<float>(i);
    dphi = V(i - 1) * (-u * (u - 1) * (u - 2) / 6) + V(i) * ((u + 1) * (u - 1) * (u - 2) / 2) + V(i + 1) * (-(u + 1) * u * (u - 2) / 2) + V(i + 2) * ((u + 1) * u * (u - 1) / 6);
    return true;
}

LensView::LensView(const LensTable& t, const Vec3& hole, double W, double H, double ca, double sa, double cb, double sb)
    : T_(t), h_(hole), W_(W), H_(H), ca_(ca), sa_(sa), cb_(cb), sb_(sb) {}

int LensView::pixel(double X, double Y, Vec3& dir) const {
    orbit::Vec3 v;
    if (!orbit::skyDirection(X, Y, W_, H_, ca_, sa_, cb_, sb_, v)) return -1;
    if (!(T_.rO() < 1e300)) { dir = v; return 0; }
    const Vec3 er = h_ * -1.0;
    const double c = dot(v, er), s = length(cross(v, er)), psi = std::atan2(s, c);
    double dphi;
    if (!T_.lookup(psi, dphi)) return 1;
    dir = bentDirection(er, v, c, dphi);
    return 0;
}

double LensView::shift() const { return T_.rO() < 1e300 ? hoverShift(0, T_.rO(), kPi / 2, 0) : 1.0; }

static double slopeAtX(double rO, double edge, double x) {
    const double psi = edge - x;
    if (x < 0.5 * edge) {                                        // near the edge: d dphi / d psi = -(1/x) d dphi / d ln x (smooth in ln x there)
        const double h = 1e-4;
        const double fp = bendFromCameraEdge(rO, x * std::exp(h)).dphi, fm = bendFromCameraEdge(rO, x * std::exp(-h)).dphi;
        return -(fp - fm) / (2 * h) / x;
    }
    // far from it, in psi; dphi is odd in psi (a ray at -psi is the mirror image), which covers psi close to 0
    const double h = 1e-5;
    const auto at = [&](double p) { return p >= 0 ? bendFromCamera(rO, p).dphi : -bendFromCamera(rO, -p).dphi; };
    return (at(psi + h) - at(psi - h)) / (2 * h);
}
double bendSlope(double rO, double psi) { const double edge = kPi - shadowAngle(rO); return slopeAtX(rO, edge, edge - psi); }
double magnification(double rO, double psi, double dphi) {
    return std::sin(psi) / (std::fabs(std::sin(dphi)) * std::fabs(bendSlope(rO, psi)));
}

int starImages(const LensTable& T, const Vec3& hole, const Vec3& s, double mag, double magLimit, std::vector<StarImage>& out) {
    const double rO = T.rO();
    if (!(rO < 1e300)) { out.push_back({s, 1.0, mag, 0, 0}); return 1; }
    const Vec3 er = hole * -1.0;
    const double cs = dot(s, er), sn = length(cross(s, er)), chi = std::atan2(sn, cs);
    if (sn < 1e-9) return 0;
    const Vec3 u = (s - er * cs) * (1 / sn);
    const double edge = T.edge();
    int count = 0;
    for (int j = 0; j < 64; j++) {
        const double target = (j % 2 == 0) ? chi + kPi * j : kPi * (j + 1) - chi;
        // dphi falls as x = edge - psi grows; bracket in ln x between 1e-12 and edge
        double lo = std::log(kXMin), hi = std::log(edge);         // (closer than 1e-9 rad the closed form loses its digits; such images are e^-20 faint)
        if (bendFromCameraEdge(rO, std::exp(lo)).dphi < target) break;    // too close to the edge to reach (and far below any cut-off)
        for (int it = 0; it < 80; it++) {
            const double mid = 0.5 * (lo + hi);
            if (bendFromCameraEdge(rO, std::exp(mid)).dphi > target) lo = mid; else hi = mid;
        }
        const double x = std::exp(0.5 * (lo + hi)), psi = edge - x;
        const double f = std::sin(psi) / (std::fabs(std::sin(target)) * std::fabs(slopeAtX(rO, edge, x)));
        StarImage im;
        im.psi = psi;
        im.order = j;
        im.magnification = f;
        im.mag = mag - 2.5 * std::log10(f);
        im.dir = er * std::cos(psi) + u * ((j % 2 == 0 ? 1.0 : -1.0) * std::sin(psi));
        if (j > 0 && im.mag > magLimit) break;
        out.push_back(im);
        count++;
    }
    return count;
}

}  // namespace blackhole
