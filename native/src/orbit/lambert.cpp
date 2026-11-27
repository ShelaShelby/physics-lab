#include "lambert.h"

#include <algorithm>
#include <cmath>

#include "solar_system.h"
#include "units.h"

namespace orbit {
namespace lambert {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Eq. (18) with the derivatives of Eq. (22). lam in (-1, 1), x > -1. Elliptic below x = 1, hyperbolic above. M = 0.
struct Tof { double T, d1, d2, d3; };
double tofSeries(double lam, double x) {
    // Battin's series, Eq. (20): eta = y - lam x, S1 = (1 - lam - x eta) / 2, Q = 4/3 2F1(3, 1; 5/2; S1), 2T = eta^3 Q + 4 lam eta
    const double y = std::sqrt(1 - lam * lam * (1 - x * x));
    const double eta = y - lam * x, S1 = 0.5 * (1 - lam - x * eta);
    double term = 1, sum = 1;                                       // 2F1(3, 1; 5/2; S) = sum (3)_k (1)_k / ((5/2)_k k!) S^k = sum (3)_k / (5/2)_k S^k
    for (int k = 0; k < 200; k++) {
        term *= (3.0 + k) / (2.5 + k) * S1;
        sum += term;
        if (std::fabs(term) < 1e-17 * std::fabs(sum)) break;
    }
    const double Q = 4.0 / 3.0 * sum;
    return 0.5 * (eta * eta * eta * Q + 4 * lam * eta);
}
double tofDirect(double lam, double x) {                           // Eq. (18) with psi from Eq. (17)
    const double y = std::sqrt(1 - lam * lam * (1 - x * x));
    double psi;
    if (x < 1) psi = std::acos(std::max(-1.0, std::min(1.0, x * y + lam * (1 - x * x))));            // elliptic
    else psi = std::acosh(std::max(1.0, x * y - lam * (x * x - 1)));                                   // hyperbolic
    return (psi / std::sqrt(std::fabs(1 - x * x)) - x + lam * y) / (1 - x * x);
}
Tof timeOfFlight(double lam, double x) {
    const double lam2 = lam * lam, lam3 = lam2 * lam;
    const double y = std::sqrt(1 - lam2 * (1 - x * x));
    const double T = std::fabs(x - 1) < 1e-2 ? tofSeries(lam, x) : tofDirect(lam, x);
    const double om = 1 - x * x;
    Tof r;
    r.T = T;
    r.d1 = (3 * T * x - 2 + 2 * lam3 * x / y) / om;
    r.d2 = (3 * T + 5 * x * r.d1 + 2 * (1 - lam2) * lam3 / (y * y * y)) / om;
    r.d3 = (7 * x * r.d2 + 8 * r.d1 - 6 * (1 - lam2) * lam2 * lam3 * x / (y * y * y * y * y)) / om;
    return r;
}
}  // namespace

double timeOfFlightT(double lam, double x, int mode) { return mode == 2 ? tofSeries(lam, x) : mode == 1 ? tofDirect(lam, x) : (std::fabs(x - 1) < 1e-2 ? tofSeries(lam, x) : tofDirect(lam, x)); }

Solution solve(const Vec3& r1, const Vec3& r2, double tof, double mu) {
    Solution S;
    const double n1 = length(r1), n2 = length(r2);
    if (!(tof > 0) || !(n1 > 0) || !(n2 > 0) || !(mu > 0)) return S;
    const Vec3 cv = r2 - r1;
    const double c = length(cv), s = 0.5 * (n1 + n2 + c);
    const Vec3 i1 = r1 * (1 / n1), i2 = r2 * (1 / n2);
    Vec3 ih = cross(i1, i2);
    const double nh = length(ih);
    if (nh < 1e-12) return S;                                          // the two places on one line through the Sun: the plane is not defined
    ih = ih * (1 / nh);
    double lam = std::sqrt(std::max(0.0, 1 - c / s));
    Vec3 it1, it2;
    if (r1.x * r2.y - r1.y * r2.x < 0) { lam = -lam; it1 = cross(i1, ih); it2 = cross(i2, ih); }
    else { it1 = cross(ih, i1); it2 = cross(ih, i2); }
    const double T = std::sqrt(2 * mu / (s * s * s)) * tof;
    // the start, Eq. (30) as improved after it: the parabola's T1 = 2/3 (1 - lam^3), the minimum-energy ellipse's T00 = acos(lam) + lam sqrt(1 - lam^2)
    const double T00 = std::acos(lam) + lam * std::sqrt(1 - lam * lam), T1 = 2.0 / 3.0 * (1 - lam * lam * lam);
    double x;
    if (T >= T00) x = std::pow(T00 / T, 2.0 / 3.0) - 1;
    else if (T < T1) x = 2.5 * T1 * (T1 - T) / (T * (1 - std::pow(lam, 5))) + 1;
    else x = std::pow(T / T00, std::log(T1 / T00) / std::log(2.0)) - 1;
    int it = 0;
    for (; it < 40; it++) {
        const Tof f = timeOfFlight(lam, x);
        const double d = f.T - T;
        const double num = f.d1 * f.d1 - d * f.d2 / 2, den = f.d1 * (f.d1 * f.d1 - d * f.d2) + f.d3 * d * d / 6;
        double dx = d * num / den;
        if (!std::isfinite(dx)) return S;
        double xn = x - dx;
        if (xn <= -1) xn = 0.5 * (x - 1);                             // x is above -1 (alpha at most 2 pi)
        const double step = std::fabs(xn - x);
        x = xn;
        if (step < 1e-14 * (1 + std::fabs(x))) { it++; break; }
    }
    S.iterations = it;
    S.x = x;
    { const Tof f = timeOfFlight(lam, x); S.T = f.T; S.dT = f.d1; }
    const double y = std::sqrt(1 - lam * lam * (1 - x * x));
    const double gam = std::sqrt(mu * s / 2), rho = (n1 - n2) / c, sig = std::sqrt(std::max(0.0, 1 - rho * rho));
    const double Vr1 = gam * ((lam * y - x) - rho * (lam * y + x)) / n1;
    const double Vr2 = -gam * ((lam * y - x) + rho * (lam * y + x)) / n2;
    const double Vt1 = gam * sig * (y + lam * x) / n1, Vt2 = gam * sig * (y + lam * x) / n2;
    S.v1 = i1 * Vr1 + it1 * Vt1;
    S.v2 = i2 * Vr2 + it2 * Vt2;
    S.ok = std::isfinite(S.v1.x + S.v1.y + S.v1.z + S.v2.x + S.v2.y + S.v2.z);
    return S;
}

std::vector<Vec3> transferArc(const Vec3& r1, const Vec3& v1, double mu, double tof, int n) {
    std::vector<Vec3> out;
    const double rn = length(r1);
    const Vec3 h = cross(r1, v1);
    const double hn = length(h);
    if (!(rn > 0) || !(hn > 0) || !(tof > 0) || n < 1) return out;
    const Vec3 hh = h * (1 / hn);
    const Vec3 ev = cross(v1, h) * (1 / mu) - r1 * (1 / rn);
    const double e = length(ev), energy = dot(v1, v1) / 2 - mu / rn;
    const Vec3 P = e > 1e-12 ? ev * (1 / e) : r1 * (1 / rn), Q = cross(hh, P);
    if (energy < 0) {
        const double a = -mu / (2 * energy), b = a * std::sqrt(1 - e * e), nm = std::sqrt(mu / (a * a * a));
        const double E0 = std::atan2(dot(r1, Q) / b, dot(r1, P) / a + e);
        const double M1 = E0 - e * std::sin(E0) + nm * tof;
        double E1 = M1;
        for (int k = 0; k < 60; k++) { const double d = (E1 - e * std::sin(E1) - M1) / (1 - e * std::cos(E1)); E1 -= d; if (std::fabs(d) < 1e-15) break; }
        for (int k = 0; k <= n; k++) { const double E = E0 + (E1 - E0) * k / n; out.push_back(P * (a * (std::cos(E) - e)) + Q * (b * std::sin(E))); }
    } else {
        const double A = mu / (2 * energy), b = A * std::sqrt(e * e - 1), nm = std::sqrt(mu / (A * A * A));   // energy > 0: A = |a|
        const double H0 = std::asinh(dot(r1, Q) / b);
        const double M1 = e * std::sinh(H0) - H0 + nm * tof;
        double H1 = std::asinh(M1 / e);
        for (int k = 0; k < 80; k++) { const double d = (e * std::sinh(H1) - H1 - M1) / (e * std::cosh(H1) - 1); H1 -= d; if (std::fabs(d) < 1e-15) break; }
        for (int k = 0; k <= n; k++) { const double H = H0 + (H1 - H0) * k / n; out.push_back(P * (A * (e - std::cosh(H))) + Q * (b * std::sinh(H))); }
    }
    return out;
}

PorkchopInput porkchopInput(const PorkchopSetup& s) {
    PorkchopInput in;
    in.s = s;
    const double au = units::kAU, gm = units::kGMSun;
    const double tu = std::sqrt(au * au * au / gm);                    // s
    in.timeUnitDays = tu / 86400.0;
    in.velUnitKmS = au / tu / 1000.0;
    auto fill = [&](int body, double jd0, double step, int n, std::vector<double>& out) {
        out.assign(std::size_t(6) * std::size_t(n), 0.0);
        for (int k = 0; k < n; k++) {
            const State st = planetStateAt(std::size_t(body), jd0 + step * k);
            double* o = &out[std::size_t(6) * std::size_t(k)];
            o[0] = st.pos.x / au; o[1] = st.pos.y / au; o[2] = st.pos.z / au;
            o[3] = st.vel.x * tu / au; o[4] = st.vel.y * tu / au; o[5] = st.vel.z * tu / au;
        }
    };
    fill(s.dep, s.depJD0, s.depStep, s.nDep, in.dep);
    fill(s.arr, s.arrJD0, s.arrStep, s.nArr, in.arr);
    return in;
}

bool porkchopCell(const PorkchopInput& in, int i, int j, double* c3, double* vinf, double* tofDays) {
    const double* d = &in.dep[std::size_t(6) * std::size_t(i)];
    const double* a = &in.arr[std::size_t(6) * std::size_t(j)];
    const double tofD = (in.s.arrJD0 + in.s.arrStep * j) - (in.s.depJD0 + in.s.depStep * i);
    if (tofDays) *tofDays = tofD;
    if (!(tofD > 0)) return false;
    const Solution S = solve({d[0], d[1], d[2]}, {a[0], a[1], a[2]}, tofD / in.timeUnitDays, 1.0);
    if (!S.ok) return false;
    const double k = in.velUnitKmS;
    const Vec3 dv1 = S.v1 - Vec3{d[3], d[4], d[5]}, dv2 = S.v2 - Vec3{a[3], a[4], a[5]};
    if (c3) *c3 = dot(dv1, dv1) * k * k;
    if (vinf) *vinf = length(dv2) * k;
    return true;
}

PorkchopGrid porkchopCpu(const PorkchopInput& in) {
    PorkchopGrid g;
    g.nDep = in.s.nDep; g.nArr = in.s.nArr;
    const float nan = std::nanf("");
    g.c3.assign(std::size_t(g.nDep) * std::size_t(g.nArr), nan);
    g.vinf = g.c3;
    for (int i = 0; i < g.nDep; i++)
        for (int j = 0; j < g.nArr; j++) {
            double c3, vi;
            if (porkchopCell(in, i, j, &c3, &vi, nullptr)) { g.c3[std::size_t(i) * std::size_t(g.nArr) + std::size_t(j)] = float(c3); g.vinf[std::size_t(i) * std::size_t(g.nArr) + std::size_t(j)] = float(vi); }
        }
    return g;
}

void rampRGB(double t, unsigned char rgb[3]) {
    t = std::max(0.0, std::min(1.0, t));
    static const double s[5][3] = {{0.10, 0.06, 0.35}, {0.15, 0.30, 0.65}, {0.12, 0.60, 0.60}, {0.55, 0.80, 0.30}, {1.0, 0.92, 0.35}};
    const double u = t * 4;
    const int k = std::min(3, int(u));
    const double f = u - k;
    for (int c = 0; c < 3; c++) rgb[c] = static_cast<unsigned char>(std::lround(255 * (s[k][c] * (1 - f) + s[k + 1][c] * f)));
}

long plotPixelsOver(const PorkchopGrid& ref, const PorkchopGrid& other, int levels, bool vinf) {
    const std::vector<float>& A = vinf ? ref.vinf : ref.c3;
    const std::vector<float>& B = vinf ? other.vinf : other.c3;
    double vmin = 1e300;
    for (float v : A) if (std::isfinite(v) && v < vmin) vmin = v;
    const double span = 2 * vmin;
    long over = 0;
    for (std::size_t k = 0; k < A.size() && k < B.size(); k++) {
        unsigned char a[3], b[3];
        if (std::isfinite(A[k])) rampRGB((A[k] - vmin) / span, a); else { a[0] = kNoTransferRGB[0]; a[1] = kNoTransferRGB[1]; a[2] = kNoTransferRGB[2]; }
        if (std::isfinite(B[k])) rampRGB((B[k] - vmin) / span, b); else { b[0] = kNoTransferRGB[0]; b[1] = kNoTransferRGB[1]; b[2] = kNoTransferRGB[2]; }
        if (std::isfinite(A[k]) != std::isfinite(B[k]) || std::abs(int(a[0]) - int(b[0])) > levels || std::abs(int(a[1]) - int(b[1])) > levels || std::abs(int(a[2]) - int(b[2])) > levels) over++;
    }
    return over;
}

namespace {
constexpr double kEps24 = 5.9604644775390625e-8;        // 2^-24
}
double cardBoundVinf(double kappa, double sigma2, double velUnitKmS) { return velUnitKmS * kEps24 * kBoundK * (1 + kappa) * (1 + 1 / sigma2); }
double cardBoundC3(double c3Ref, double dv) { const double v = std::sqrt(std::max(0.0, c3Ref)); return 2 * v * dv + dv * dv; }

GridCompare compareGrids(const PorkchopInput& in, const PorkchopGrid& ref, const PorkchopGrid& card) {
    GridCompare r;
    r.cells = long(ref.c3.size());
    for (std::size_t k = 0; k < ref.c3.size() && k < card.c3.size(); k++) {
        const bool a = std::isfinite(ref.c3[k]), b = std::isfinite(card.c3[k]);
        if (a != b) { r.flips++; if ((a ? ref.c3[k] : card.c3[k]) < 100) r.flipLow++; continue; }
        if (!a) continue;
        r.both++;
        const int i = int(k / std::size_t(ref.nArr)), j = int(k % std::size_t(ref.nArr));
        const double* d = &in.dep[std::size_t(6) * std::size_t(i)];
        const double* ar = &in.arr[std::size_t(6) * std::size_t(j)];
        const double tof = (in.s.arrJD0 + in.s.arrStep * j) - (in.s.depJD0 + in.s.depStep * i);
        const Solution S = solve({d[0], d[1], d[2]}, {ar[0], ar[1], ar[2]}, tof / in.timeUnitDays, 1.0);
        const double kappa = S.ok && S.dT != 0 ? S.T / std::fabs(S.dT) : 1e6;
        const double n1 = length({d[0], d[1], d[2]}), n2 = length({ar[0], ar[1], ar[2]}), cc = length(Vec3{ar[0] - d[0], ar[1] - d[1], ar[2] - d[2]});
        const double rho = (n1 - n2) / cc, sig2 = std::max(1e-12, 1 - rho * rho);          // sigma^2 of Algorithm 1
        r.worstKappa = std::max(r.worstKappa, kappa);
        const double dv = cardBoundVinf(kappa, sig2, in.velUnitKmS);
        const double dc = std::fabs(double(card.c3[k]) - ref.c3[k]), bound = cardBoundC3(ref.c3[k], dv);
        const double dvi = std::fabs(double(card.vinf[k]) - ref.vinf[k]);
        if (dc / bound > r.worstC3OfBound) { r.worstC3OfBound = dc / bound; r.worstC3 = dc; r.worstC3At = ref.c3[k]; }
        r.worstVinfOfBound = std::max(r.worstVinfOfBound, dvi / dv);
        r.worstVinf = std::max(r.worstVinf, dvi);
        if (dc > bound || dvi > dv) r.over++;
    }
    return r;
}

}  // namespace lambert
}  // namespace orbit
