#include "comet_tail.h"

#include <algorithm>

#include <cmath>

#include "kepler.h"
#include "solar_data.h"
#include "units.h"

namespace orbit {
namespace tail {

double gmSunAuDay() { return units::kGMSun * units::kDay * units::kDay / (units::kAU * units::kAU * units::kAU); }

// Stumpff's functions C(z) = (1 - cos sqrt z) / z and S(z) = (sqrt z - sin sqrt z) / sqrt(z)^3 (cosh and sinh for z < 0); near 0 their series
// (C = sum (-z)^k / (2k + 2)!, S = sum (-z)^k / (2k + 3)!), where the closed forms lose digits.
static void stumpff(double z, double& C, double& S) {
    if (std::fabs(z) < 0.1) {
        double c = 0.5, s = 1.0 / 6.0, tc = 0.5, ts = 1.0 / 6.0;
        for (int k = 1; k <= 8; k++) {
            tc *= -z / ((2.0 * k + 1) * (2.0 * k + 2));
            ts *= -z / ((2.0 * k + 2) * (2.0 * k + 3));
            c += tc; s += ts;
        }
        C = c; S = s;
    } else if (z > 0) {
        const double q = std::sqrt(z);
        C = (1 - std::cos(q)) / z; S = (q - std::sin(q)) / (q * q * q);
    } else {
        const double q = std::sqrt(-z);
        C = (std::cosh(q) - 1) / -z; S = (std::sinh(q) - q) / (q * q * q);
    }
}

void propagate(const Vec3& r0, const Vec3& v0, double mu, double dt, Vec3& r, Vec3& v) {
    if (!(mu > 0)) { r = r0 + v0 * dt; v = v0; return; }   // no net force on the grain (beta = 1): a straight line
    const double R0 = length(r0), vr0 = dot(r0, v0) / R0, al = 2 / R0 - dot(v0, v0) / mu, sm = std::sqrt(mu);
    double x = sm * dt / R0;                                   // the universal anomaly chi; this start is good while dt is short next to the orbit
    double C = 0.5, S = 1.0 / 6.0;
    for (int it = 0; it < 100; it++) {                         // Newton on F(chi) = 0, the universal Kepler equation
        const double z = al * x * x;
        stumpff(z, C, S);
        const double F = R0 * vr0 / sm * x * x * C + (1 - al * R0) * x * x * x * S + R0 * x - sm * dt;
        const double Fp = R0 * vr0 / sm * x * (1 - z * S) + (1 - al * R0) * x * x * C + R0;
        const double d = F / Fp;
        x -= d;
        if (std::fabs(d) <= 1e-15 * std::max(1.0, std::fabs(x))) break;
    }
    stumpff(al * x * x, C, S);
    const double f = 1 - x * x / R0 * C, g = dt - x * x * x * S / sm;
    r = r0 * f + v0 * g;
    const double Rn = length(r);
    const double fd = sm / (Rn * R0) * (al * x * x * x * S - x), gd = 1 - x * x / Rn * C;
    v = r0 * fd + v0 * gd;
}

Vec3 ionTailDirection(const Vec3& pos, const Vec3& vel, double windKmS) {
    const double r = length(pos);
    const Vec3 w = pos * (windKmS * 1e3 / r) - vel;            // the wind as the comet meets it
    return w * (1 / length(w));
}

void CometGrains::update(const CometData& c, double jdTDB) {
    const double step = kReleaseStepDays, mu = gmSunAuDay();
    now_ = jdTDB - kJ2000;
    const std::int64_t kNow = static_cast<std::int64_t>(std::floor(now_ / step));
    live_ = 0;
    int runFirst = -1, runEnd = -1;
    for (std::int64_t k = kNow - kReleases + 1; k <= kNow; k++) {
        const int row = static_cast<int>(((k % kReleases) + kReleases) % kReleases);
        Row& R = rows_[std::size_t(row)];
        if (R.k != k) {
            R.k = k;
            const State st = cometState(c, kJ2000 + double(k) * step);
            R.r0 = st.pos * (1 / units::kAU);
            R.v0 = st.vel * (units::kDay / units::kAU);
            R.on = length(R.r0) <= kSwitchOnAU;
            for (int i = 0; i < kBetas; i++) {
                const std::size_t g = std::size_t(row) * kBetas + std::size_t(i);
                float* e = &el_[g * kStride];
                e[0] = static_cast<float>(R.r0.x); e[1] = static_cast<float>(R.r0.y); e[2] = static_cast<float>(R.r0.z);
                e[3] = static_cast<float>(R.v0.x); e[4] = static_cast<float>(R.v0.y); e[5] = static_cast<float>(R.v0.z);
                e[6] = R.on ? static_cast<float>((1 - betaOf(i)) * mu) : -1.f;
                e[7] = 0.f;
                t0_[g] = double(k) * step;
            }
            const int a = row * kBetas, b = a + kBetas;       // (rows next to each other join one range)
            if (runFirst >= 0 && a == runEnd) runEnd = b;
            else { if (runFirst >= 0) dirty_.push_back({runFirst, runEnd - runFirst}); runFirst = a; runEnd = b; }
        }
        if (R.on) live_ += kBetas;
    }
    if (runFirst >= 0) dirty_.push_back({runFirst, runEnd - runFirst});
}

bool CometGrains::position(int g, Vec3& r) const {
    if (g < 0 || g >= kGrains) return false;
    const Row& R = rows_[std::size_t(g / kBetas)];
    if (!R.on || R.k == INT64_MIN) return false;
    Vec3 v;
    propagate(R.r0, R.v0, (1 - betaOf(g % kBetas)) * gmSunAuDay(), now_ - double(R.k) * kReleaseStepDays, r, v);
    return true;
}

bool CometGrains::farthest(Vec3& r) const {
    double oldest = 1e300;
    int best = -1;
    for (int row = 0; row < kReleases; row++) {
        const Row& R = rows_[std::size_t(row)];
        if (R.on && R.k != INT64_MIN && double(R.k) < oldest) { oldest = double(R.k); best = row; }
    }
    return best >= 0 && position(best * kBetas + kBetas - 1, r);
}

}  // namespace tail
}  // namespace orbit
