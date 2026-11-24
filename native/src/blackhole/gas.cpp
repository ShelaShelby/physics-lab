#include "blackhole/gas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include "blackhole/disc.h"
#include "blackhole/low_priority.h"
#include "blackhole/kerr.h"
#include "orbit/units.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

double gasOmega(double r, double a) { return orbitOmega(r, a); }
double gasPeriod(double r, double a) { return 2 * kPi / gasOmega(r, a); }
double gasSpeed(double r, double a) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    return m.varpi / m.alpha * (gasOmega(r, a) - m.omega);
}
double gasClockRate(double r, double a) {
    const KerrMetric m = kerrMetric(a, r, kPi / 2);
    const double V = gasSpeed(r, a);
    return m.alpha * std::sqrt(1 - V * V);
}
double cameraClockRate(double a, double r, double theta) { return kerrMetric(a, r, theta).alpha; }
double massTimeSeconds(double massSun) { return massSun * units::kGMSun / std::pow(units::kLightSpeed, 3); }

bool KerrBLt::launch(const Camera& cam, const Vec3& look, Ray& ray) const {
    if (!KerrBL::launch(cam, look, ray)) return false;
    ray.n = kTState;
    ray.y[5] = 0;
    ray.y[6] = 0;
    return true;
}
void KerrBLt::rhs(const Ray& ray, const double* y, double* dy) const {
    KerrBL::rhs(ray, y, dy);
    const double r = y[0], s = std::sin(y[1]), c = std::cos(y[1]), a = a_, b = ray.c[0];
    const double rho2 = r * r + a * a * c * c, D = r * r - 2 * r + a * a, P = r * r + a * a - a * b;
    dy[5] = ((r * r + a * a) * P / D + a * (b - a * s * s)) / rho2;
    dy[6] = 1 / rho2;
}

void Clump::init() {
    const KerrMetric m = kerrMetric(a, rc, kPi / 2);
    const double uph = orbitAngMom(rc, a);
    grr = rc * rc / m.Delta;                       // g_rr = rho^2 / Delta at the equator
    gpp = m.varpi * m.varpi + uph * uph;           // g_phph + u_phi^2
    omega = gasOmega(rc, a);
}
double Clump::density(double r, double phi, double tEmit) const {
    const double dr = r - rc, dp = std::remainder(phi - phi0 - omega * tEmit, 2 * kPi);
    return std::exp(-(grr * dr * dr + gpp * dp * dp) / (2 * RS * RS));
}
double Clump::period() const { return 2 * kPi / omega; }

std::uint32_t gasHash(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
double gasU01(std::uint32_t h) { return (h & 0xFFFFFF) / 16777216.0; }

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.79.0: the streaks (gas.h)
namespace {
// radix-2 FFT of one line of a (n values, stride apart from off), in place
void fft1(std::vector<std::complex<double>>& a, int n, int stride, int off, bool inv) {
    std::vector<std::complex<double>> t(static_cast<std::size_t>(n));
    for (int i = 0; i < n; i++) t[static_cast<std::size_t>(i)] = a[static_cast<std::size_t>(off + i * stride)];
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(t[static_cast<std::size_t>(i)], t[static_cast<std::size_t>(j)]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = 2 * kPi / len * (inv ? 1 : -1);
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < len / 2; k++) {
                const std::complex<double> w(std::cos(ang * k), std::sin(ang * k));
                const std::complex<double> u = t[static_cast<std::size_t>(i + k)], v = t[static_cast<std::size_t>(i + k + len / 2)] * w;
                t[static_cast<std::size_t>(i + k)] = u + v;
                t[static_cast<std::size_t>(i + k + len / 2)] = u - v;
            }
    }
    for (int i = 0; i < n; i++) a[static_cast<std::size_t>(off + i * stride)] = t[static_cast<std::size_t>(i)];
}
// the standard normal quantile (Acklam's rational start, then two Newton steps on erfc: to double rounding)
double normQuantile(double p) {
    static const double a[6] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02, 1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[5] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02, 6.680131188771972e+01, -1.328068155288572e+01};
    static const double c[6] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00, -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[4] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00};
    double x;
    if (p < 0.02425) { const double q = std::sqrt(-2 * std::log(p)); x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1); }
    else if (p > 1 - 0.02425) { const double q = std::sqrt(-2 * std::log(1 - p)); x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1); }
    else { const double q = p - 0.5, r = q * q; x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1); }
    for (int it = 0; it < 2; it++) {                                        // Newton on Phi(x) - p, Phi(x) = erfc(-x / sqrt 2) / 2
        const double e = 0.5 * std::erfc(-x / std::sqrt(2.0)) - p;
        x -= e * std::sqrt(2 * kPi) * std::exp(x * x / 2);
    }
    return x;
}
inline double frac(double v) { return v - std::floor(v); }
}  // namespace

void StreakField::make(std::uint32_t seed) {
    const auto t0 = std::chrono::steady_clock::now();
    const int NX = kNx, NY = kNy;
    std::vector<std::complex<double>> a(static_cast<std::size_t>(NX) * NY);
    for (std::size_t i = 0; i < a.size(); i++) {                           // white noise: Box-Muller from the hash (the same on every machine)
        const std::uint32_t h = gasHash(seed, static_cast<std::uint32_t>(i), 0x5EEDu);
        const double u1 = (gasU01(h) + 0.5 / 16777216.0), u2 = gasU01(h * 747796405u + 1u);
        a[i] = std::sqrt(-2 * std::log(u1)) * std::cos(2 * kPi * u2);
    }
    for (int y = 0; y < NY; y++) fft1(a, NX, 1, y * NX, false);
    for (int x = 0; x < NX; x++) fft1(a, NY, NX, x, false);
    const double dx = kLnSpan / NX, dy = 2 * kPi / NY;
    // u1 at kTilt from the orbit (the y axis), trailing: the inner gas ahead, so the streak's outer end lags: along u1, phi grows as ln R falls
    const double u1x = -std::sin(kTilt), u1y = std::cos(kTilt), u2x = u1y, u2y = -u1x, l1 = kLam1, l2 = kLam1 * kRatio;
    for (int y = 0; y < NY; y++)
        for (int x = 0; x < NX; x++) {
            const double kx = 2 * kPi * (x <= NX / 2 ? x : x - NX) / (NX * dx), ky = 2 * kPi * (y <= NY / 2 ? y : y - NY) / (NY * dy);
            const double k1 = kx * u1x + ky * u1y, k2 = kx * u2x + ky * u2y;
            a[static_cast<std::size_t>(y) * NX + x] *= 1 / (1 + l1 * l1 * k1 * k1 + l2 * l2 * k2 * k2);   // the square root of P (LG21 (13))
        }
    for (int x = 0; x < NX; x++) fft1(a, NY, NX, x, true);
    for (int y = 0; y < NY; y++) fft1(a, NX, 1, y * NX, true);
    // each value replaced by the normal quantile of its rank: the field's values are exactly standard normal (the mean of S is then exactly 1)
    const std::size_t N = a.size();
    std::vector<std::uint32_t> order(N);
    for (std::size_t i = 0; i < N; i++) order[i] = static_cast<std::uint32_t>(i);
    std::sort(order.begin(), order.end(), [&](std::uint32_t p, std::uint32_t q) { return a[p].real() < a[q].real() || (a[p].real() == a[q].real() && p < q); });
    lev[0].assign(N, 0.0f);
    for (std::size_t k = 0; k < N; k++) lev[0][order[k]] = static_cast<float>(normQuantile((static_cast<double>(k) + 0.5) / static_cast<double>(N)));
    for (int L = 1; L < kLevels; L++) {                                      // 2 x 2 means
        const int w = nx(L), h = ny(L), pw = nx(L - 1);
        lev[L].assign(static_cast<std::size_t>(w) * h, 0.0f);
        const std::vector<float>& P = lev[L - 1];
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                const std::size_t q = static_cast<std::size_t>(2 * j) * pw + 2 * i;
                lev[L][static_cast<std::size_t>(j) * w + i] = static_cast<float>(0.25 * (static_cast<double>(P[q]) + P[q + 1] + P[q + pw] + P[q + pw + 1]));
            }
    }
    // each level's texel covariances at offsets (0, 0), (1, 0), (0, 1), and (1, 1) + (1, -1): the variance read between texels follows from them exactly
    // (varAt); the covariance of level L with L + 1 read at the same point, averaged over 2^18 points evenly spread (the hash: only the blend uses it)
    for (int L = 0; L < kLevels; L++) {
        const int w = nx(L), h = ny(L);
        double a0 = 0, ax = 0, ay = 0, ad = 0;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                const double v = texel(L, i, j);
                a0 += v * v; ax += v * texel(L, i + 1, j); ay += v * texel(L, i, j + 1); ad += v * (texel(L, i + 1, j + 1) + texel(L, i + 1, j - 1));
            }
        const double n = static_cast<double>(w) * h;
        c00[L] = a0 / n; cx[L] = ax / n; cy[L] = ay / n; cd[L] = ad / n;
        double v = 0, c = 0;
        const int m = 1 << 18;
        for (int i = 0; i < m; i++) {
            const std::uint32_t q = gasHash(0xC0FFEEu, static_cast<std::uint32_t>(L), static_cast<std::uint32_t>(i));
            const double X = gasU01(q), Y = gasU01(q * 2891336453u + 7u), f = sample(L, X, Y);
            v += f * f;
            if (L + 1 < kLevels) c += f * sample(L + 1, X, Y);
        }
        var[L] = v / m; cov[L] = c / m;
    }
    makeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

double StreakField::texel(int L, int i, int j) const {
    const int w = nx(L), h = ny(L);
    i %= w; if (i < 0) i += w;
    j %= h; if (j < 0) j += h;
    return lev[L][static_cast<std::size_t>(j) * w + i];
}
double StreakField::sample(int L, double X, double Y, double* v) const {
    const double x = frac(X) * nx(L) - 0.5, y = frac(Y) * ny(L) - 0.5;
    const double fx = std::floor(x), fy = std::floor(y), ax = x - fx, ay = y - fy;
    const int i = static_cast<int>(fx), j = static_cast<int>(fy);
    if (v) *v = varAt(L, ax, ay);
    return (1 - ay) * ((1 - ax) * texel(L, i, j) + ax * texel(L, i + 1, j)) + ay * ((1 - ax) * texel(L, i, j + 1) + ax * texel(L, i + 1, j + 1));
}
// the variance of the bilinear mix at (ax, ay) from the texel covariances: with A = w0^2 + w1^2 and B = 2 w0 w1 along each axis,
// A_x A_y c00 + B_x A_y cx + A_x B_y cy + B_x B_y (c(1, 1) + c(1, -1)) / 2
double StreakField::varAt(int L, double ax, double ay) const {
    const double Ax = (1 - ax) * (1 - ax) + ax * ax, Bx = 2 * ax * (1 - ax), Ay = (1 - ay) * (1 - ay) + ay * ay, By = 2 * ay * (1 - ay);
    return Ax * Ay * c00[L] + Bx * Ay * cx[L] + Ax * By * cy[L] + 0.5 * Bx * By * cd[L];
}
double StreakField::lodOf(double fe, double fp) const {
    const double m = std::max({fe * kNx / kLnSpan, fp * kNy / (2 * kPi), 1.0});
    return std::min(std::log2(m), static_cast<double>(kLevels - 1));
}
double streakLife(int k, double a) { const double R = std::exp((k + 0.5) * kStreakBinW); return StreakField::kLife * 2 * kPi * (R * std::sqrt(R) + a); }
void streakClock(double epoch, double life, double& f, std::uint32_t& K) {
    const double q = epoch / life, Kd = std::floor(q);
    f = q - Kd;
    K = static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(Kd)));
}
double StreakField::field(double R, double phi, double T, double a, double fe, double fp, double* s2) const {
    const double lod = lodOf(fe, fp);
    const int L = std::min(static_cast<int>(lod), kLevels - 2);
    const double w = std::min(lod - L, 1.0);
    const double om = 1 / (R * std::sqrt(R) + a), X0 = std::log(R) / kLnSpan, E = gasEpoch(T), tau = T - E;
    const double q = std::clamp(std::log(R) / kStreakBinW - 0.5, 0.0, kStreakBins - 1.0);
    const int k0 = std::min(static_cast<int>(q), kStreakBins - 2);
    const double th = 0.5 * kPi * (q - k0);
    double f = 0, sv = 0;
    for (int b = 0; b < 2; b++) {                                           // the two nearest clock bins
        const int k = k0 + b;
        const double wb = b == 0 ? std::cos(th) : std::sin(th), life = streakLife(k, a);
        double fk;
        std::uint32_t K;
        streakClock(E, life, fk, K);
        for (int j = 0; j < 2; j++) {                                       // the two copies: independent (their offsets differ), so their variances add
            const double v = fk + tau / life + 0.5 * j, nn = std::floor(v), u = v - nn, wt = wb * std::sin(kPi * u);
            const std::uint32_t n = K + static_cast<std::uint32_t>(static_cast<std::int64_t>(nn));
            const std::uint32_t h = gasHash(n, static_cast<std::uint32_t>(2 * k + j + 7), 0x51u);
            const double X = X0 + gasU01(h), Y = (phi - om * u * life) / (2 * kPi) + gasU01(h * 747796405u + 1u);
            double e0 = 0, e1 = 0;
            const double v0 = sample(L, X, Y, &e0), v1 = w > 0 ? sample(L + 1, X, Y, &e1) : 0;
            f += wt * ((1 - w) * v0 + w * v1);
            sv += wt * wt * ((1 - w) * (1 - w) * e0 + w * w * e1 + 2 * w * (1 - w) * cov[L]);
        }
    }
    if (s2) *s2 = sv;
    return f;
}
double StreakField::factor(double R, double phi, double t, double a, double fe, double fp, double c) const {
    double s2 = 1;
    const double f = field(R, phi, t, a, fe, fp, &s2);
    return std::exp(c * f - 0.5 * c * c * s2);
}
std::vector<float> StreakField::atlas(int& w, int& h) const {
    w = kNx + kNx / 2; h = kNy;
    std::vector<float> o(static_cast<std::size_t>(w) * h, 0.0f);
    for (int L = 0; L < kLevels; L++)
        for (int j = 0; j < ny(L); j++)
            for (int i = 0; i < nx(L); i++) o[static_cast<std::size_t>(atlasY(L) + j) * w + atlasX(L) + i] = lev[L][static_cast<std::size_t>(j) * nx(L) + i];
    return o;
}
namespace {
struct StreakStore {
    std::mutex mx;
    std::vector<std::unique_ptr<StreakField>> kept;                        // every field made (the last is the one in use)
    std::future<std::unique_ptr<StreakField>> job;
    std::atomic<const StreakField*> ready{nullptr};
    void start() {                                                          // (under mx)
        if (job.valid()) return;
        job = std::async(std::launch::async, [] { lowerThisThread(); auto f = std::make_unique<StreakField>(); f->make(); return f; });
    }
    void collect(bool wait) {                                               // (under mx)
        if (!job.valid()) return;
        if (!wait && job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        kept.push_back(job.get());
        ready.store(kept.back().get());
    }
};
StreakStore& store() { static StreakStore s; return s; }
}  // namespace
const StreakField& streakField() {
    StreakStore& S = store();
    if (const StreakField* f = S.ready.load()) return *f;
    std::lock_guard<std::mutex> lk(S.mx);
    S.start();
    S.collect(true);
    return *S.ready.load();
}
const StreakField* streakFieldReady() {
    StreakStore& S = store();
    if (const StreakField* f = S.ready.load()) return f;
    std::lock_guard<std::mutex> lk(S.mx);
    S.start();
    S.collect(false);
    return S.ready.load();
}
void streakFieldRemake() {
    StreakStore& S = store();
    std::lock_guard<std::mutex> lk(S.mx);
    S.collect(true);
    S.ready.store(nullptr);
    S.start();
}

double gasEpoch(double T) { return kGasEpochM * std::floor(T / kGasEpochM); }

void gasCard(bool streaks, double a, const Clump* c, double T, GasCard& o) {
    o = GasCard{};
    o.epoch = gasEpoch(T);
    o.tau = static_cast<float>(T - o.epoch);
    o.G[2] = static_cast<float>(a);
    if (c) {
        o.T[3] = static_cast<float>(c->A);
        o.C[0] = static_cast<float>(c->rc);
        o.C[1] = static_cast<float>(std::remainder(c->phi0 + c->omega * o.epoch, 2 * kPi));
        o.C[2] = static_cast<float>(c->omega);
        o.C[3] = static_cast<float>(1 / (2 * c->RS * c->RS));
        o.G[0] = static_cast<float>(c->grr);
        o.G[1] = static_cast<float>(c->gpp);
    }
    if (streaks) {
        const StreakField& f = streakField();
        o.T[1] = static_cast<float>(kStreakC);
        o.T[2] = 1;
        for (int k = 0; k < kStreakBins; k++) {
            double fk;
            std::uint32_t K;
            streakClock(o.epoch, streakLife(k, a), fk, K);
            o.B[4 * k] = static_cast<float>(fk); o.B[4 * k + 1] = static_cast<float>(K >> 16); o.B[4 * k + 2] = static_cast<float>(K & 0xFFFFu);
        }
        for (int L = 0; L < StreakField::kLevels; L++) {
            o.V[4 * L] = static_cast<float>(f.c00[L]); o.V[4 * L + 1] = static_cast<float>(f.cx[L]); o.V[4 * L + 2] = static_cast<float>(f.cy[L]); o.V[4 * L + 3] = static_cast<float>(f.cd[L]);
            o.W[L] = static_cast<float>(f.cov[L]);
        }
    }
}

}  // namespace blackhole
