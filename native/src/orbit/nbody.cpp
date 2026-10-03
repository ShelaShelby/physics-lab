#include "nbody.h"

#include <cmath>

#include "solar_system.h"
#include "units.h"

namespace orbit {

using namespace units;

// JPL "Astrodynamic Parameters" page, DE440 (km^3/s^2 there, times 1e9 here to m^3/s^2). The Earth + Moon system is the sum of the page's Earth
// (398600.435507) and Moon (4902.800118). Mercury and Venus have no moons; the others are the page's "system" values.
const double kRealGM[kNBodies] = {
    1.32712440041279419e20,                   // Sun (the page: 1.32712440041279419e11 km^3/s^2)
    22031.868551e9,                           // Mercury
    324858.592000e9,                          // Venus
    (398600.435507 + 4902.800118) * 1e9,      // Earth + Moon
    42828.375816e9,                           // Mars system
    126712764.100000e9,                       // Jupiter system
    37940584.841800e9,                        // Saturn system
    5794556.400000e9,                         // Uranus system
    6836527.100580e9,                         // Neptune system
};

void nbodyAccelN(int n, const double* gm, const Vec3* x, Vec3* a) {
    for (int i = 0; i < n; i++) a[i] = Vec3{};
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            const Vec3 d = x[j] - x[i];
            const double r2 = dot(d, d), inv3 = 1.0 / (r2 * std::sqrt(r2));
            a[i] = a[i] + d * (gm[j] * inv3);
            a[j] = a[j] - d * (gm[i] * inv3);
        }
}

// ---- the Wisdom-Holman step in democratic heliocentric variables ----
// Stumpff functions C(z) = (1 - cos sqrt z) / z and S(z) = (sqrt z - sin sqrt z) / sqrt z^3 (z < 0: with cosh and sinh); a series for small |z|.
static void stumpff(double z, double& C, double& S) {
    if (std::fabs(z) < 1.0) {
        C = 0; S = 0;
        double term = 1;                                   // (-z)^k
        double fC = 1.0 / 2, fS = 1.0 / 6;                 // 1/(2k+2)!  and 1/(2k+3)!
        for (int k = 0; k < 14; k++) {
            C += term * fC; S += term * fS;
            term *= -z;
            fC /= (2.0 * k + 3) * (2.0 * k + 4); fS /= (2.0 * k + 4) * (2.0 * k + 5);
        }
    } else if (z > 0) {
        const double w = std::sqrt(z);
        C = (1 - std::cos(w)) / z; S = (w - std::sin(w)) / (w * w * w);
    } else {
        const double w = std::sqrt(-z);
        C = (std::cosh(w) - 1) / (-z); S = (std::sinh(w) - w) / (w * w * w);
    }
}

// Advance one body on its Kepler orbit round a centre with GM = mu for time dt (negative: backwards) by the universal-variable method with Gauss's f and g
// functions (Stumpff functions as in Bate, Mueller & White, "Fundamentals of Astrodynamics", 1971). q, v: position and velocity relative to the centre.
static void keplerDrift(Vec3& q, Vec3& v, double mu, double dt) {
    const double sm = std::sqrt(mu), r0 = length(q), vr0 = dot(q, v) / r0, alpha = 2.0 / r0 - dot(v, v) / mu;
    double chi = sm * std::fabs(alpha) * dt, C = 0, S = 0;
    if (alpha < 1e-12 && alpha > -1e-12) chi = sm * dt / r0;
    for (int it = 0; it < 60; it++) {
        const double z = alpha * chi * chi;
        stumpff(z, C, S);
        const double F = r0 * vr0 / sm * chi * chi * C + (1 - alpha * r0) * chi * chi * chi * S + r0 * chi - sm * dt;
        const double dF = r0 * vr0 / sm * chi * (1 - z * S) + (1 - alpha * r0) * chi * chi * C + r0;
        const double d = F / dF;
        chi -= d;
        if (std::fabs(d) < 1e-15 * std::fabs(chi) + 1e-300) break;
    }
    const double z = alpha * chi * chi;
    stumpff(z, C, S);
    const double f = 1 - chi * chi * C / r0, g = dt - chi * chi * chi * S / sm;
    const Vec3 qn = q * f + v * g;
    const double r = length(qn);
    const double fd = sm / (r * r0) * (z * S * chi - chi), gd = 1 - chi * chi * C / r;
    v = q * fd + v * gd;
    q = qn;
}

// Democratic heliocentric variables (Duncan, Levison & Lee 1998): Q_i = x_i - x_0 and V_i = v_i - v_cm for the planets i >= 1. The Hamiltonian splits into
//   H = H_kepler (each planet alone round the Sun, GM_0) + H_sun (the Sun's kinetic energy, |sum_i GM_i V_i|^2 / (2 GM_0)) + H_interaction (planet-planet).
// One step (Wisdom & Holman 1991, Strang splitting, symmetric so time-reversible): half interaction kick, half Sun jump, full Kepler drift, half jump, half kick.
// The Kepler part is solved exactly, so for planets far smaller than the Sun the error is of order (planet mass / Sun mass) * h^2, not h^4 times a big
// constant: that is why a day-scale step is enough and the orbits do not creep (see REPORT.md for the measured numbers).
void nbodyStepN(int n, const double* gm, Vec3* x, Vec3* v, double h, bool gr) {
    double mt = 0; Vec3 com, vcm;
    for (int i = 0; i < n; i++) { mt += gm[i]; com = com + x[i] * gm[i]; vcm = vcm + v[i] * gm[i]; }
    com = com * (1.0 / mt); vcm = vcm * (1.0 / mt);
    Vec3 Q[kNBodies], V[kNBodies];
    for (int i = 1; i < n; i++) { Q[i] = x[i] - x[0]; V[i] = v[i] - vcm; }
    const auto kick = [&](double dt) {                                           // planet-planet pulls only (the Sun's pull is in the Kepler part)
        Vec3 a[kNBodies];
        for (int i = 1; i < n; i++) a[i] = Vec3{};
        for (int i = 1; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                const Vec3 d = Q[j] - Q[i];
                const double r2 = dot(d, d), inv3 = 1.0 / (r2 * std::sqrt(r2));
                a[i] = a[i] + d * (gm[j] * inv3);
                a[j] = a[j] - d * (gm[i] * inv3);
            }
        if (gr) {                                                                // the Sun's 1PN potential (Nobili & Roxburgh 1986, see nbody.h): a = -6 GM^2 Q / (c^2 |Q|^4)
            const double pre = 6.0 * gm[0] * gm[0] / (kLightSpeed * kLightSpeed);
            for (int i = 1; i < n; i++) { const double r2 = dot(Q[i], Q[i]); a[i] = a[i] - Q[i] * (pre / (r2 * r2)); }
        }
        for (int i = 1; i < n; i++) V[i] = V[i] + a[i] * dt;
    };
    const auto jump = [&](double dt) {                                           // every planet moves with the Sun's reflex velocity
        Vec3 p;
        for (int i = 1; i < n; i++) p = p + V[i] * gm[i];
        const Vec3 d = p * (dt / gm[0]);
        for (int i = 1; i < n; i++) Q[i] = Q[i] + d;
    };
    kick(0.5 * h); jump(0.5 * h);
    for (int i = 1; i < n; i++) keplerDrift(Q[i], V[i], gm[0], h);
    jump(0.5 * h); kick(0.5 * h);
    com = com + vcm * h;
    Vec3 sq, sv;                                                                  // the Sun from the balance of the planets
    for (int i = 1; i < n; i++) { sq = sq + Q[i] * gm[i]; sv = sv + V[i] * gm[i]; }
    x[0] = com - sq * (1.0 / mt);
    v[0] = vcm - sv * (1.0 / gm[0]);
    for (int i = 1; i < n; i++) { x[i] = x[0] + Q[i]; v[i] = V[i] + vcm; }
}

void nbodyAccel(const Vec3* x, Vec3* a) { nbodyAccelN(kNBodies, kRealGM, x, a); }
void nbodyStep(NBodyState& s, double h, bool gr) { nbodyStepN(kNBodies, kRealGM, s.x, s.v, h, gr); }

double nbodyEnergy(const NBodyState& s) {
    double e = 0;
    for (int i = 0; i < kNBodies; i++) {
        e += 0.5 * kRealGM[i] * dot(s.v[i], s.v[i]);
        for (int j = i + 1; j < kNBodies; j++) e -= kRealGM[i] * kRealGM[j] / length(s.x[j] - s.x[i]);
    }
    return e;
}
double nbodyEnergyGR(const NBodyState& s) {
    double e = nbodyEnergy(s);
    const double pre = 3.0 * kRealGM[0] * kRealGM[0] / (kLightSpeed * kLightSpeed);       // potential energy per G: GM_i * (-3 GM_sun^2 / (c^2 r^2))
    for (int i = 1; i < kNBodies; i++) { const Vec3 d = s.x[i] - s.x[0]; e -= kRealGM[i] * pre / dot(d, d); }
    return e;
}
Vec3 nbodyAngularMomentum(const NBodyState& s) {
    Vec3 l;
    for (int i = 0; i < kNBodies; i++) l = l + cross(s.x[i], s.v[i]) * kRealGM[i];
    return l;
}
Vec3 nbodyMomentum(const NBodyState& s) {
    Vec3 p;
    for (int i = 0; i < kNBodies; i++) p = p + s.v[i] * kRealGM[i];
    return p;
}
Vec3 nbodyCentreOfMass(const NBodyState& s) {
    Vec3 c; double m = 0;
    for (int i = 0; i < kNBodies; i++) { c = c + s.x[i] * kRealGM[i]; m += kRealGM[i]; }
    return c * (1.0 / m);
}

NBodyState nbodyStartState(double jd) {
    NBodyState s;
    Vec3 r[kNBodies], v[kNBodies];                                      // heliocentric
    double total = 0;
    for (int i = 0; i < kNBodies; i++) total += kRealGM[i];
    for (int i = 1; i < kNBodies; i++) {
        const State st = planetStateAt(std::size_t(i - 1), jd);
        r[i] = st.pos;
        v[i] = st.vel * std::sqrt((kRealGM[0] + kRealGM[i]) / kGMSun);
    }
    Vec3 rs, vs;                                                        // the Sun's barycentric position and velocity: minus the mass-weighted mean of the planets'
    for (int i = 1; i < kNBodies; i++) { rs = rs - r[i] * (kRealGM[i] / total); vs = vs - v[i] * (kRealGM[i] / total); }
    s.x[0] = rs; s.v[0] = vs;
    for (int i = 1; i < kNBodies; i++) { s.x[i] = r[i] + rs; s.v[i] = v[i] + vs; }
    return s;
}

NBodyClock::NBodyClock(double h, bool gr, const NBodyState* start) : h_(h), gr_(gr), start_(start ? *start : nbodyStartState()) {}

const NBodyState& NBodyClock::checkpoint(long j) {
    if (j == 0) return start_;
    auto it = cp_.find(j);
    if (it != cp_.end()) return it->second;
    // walk from the nearest known checkpoint towards j (stored ones are kept while |index| <= kKeep)
    const long dir = j > 0 ? 1 : -1;
    long from = 0;
    NBodyState s = start_;
    for (long q = j - dir; q != 0; q -= dir) { auto f = cp_.find(q); if (f != cp_.end()) { from = q; s = f->second; break; } }
    for (long q = from + dir; ; q += dir) {
        for (long n = 0; n < kEvery; n++) nbodyStep(s, dir * h_, gr_);
        steps_ += kEvery;
        if (std::labs(q) <= kKeep) cp_[q] = s;
        if (q == j) { if (std::labs(q) > kKeep) { last_ = s; return last_; } break; }
    }
    return cp_[j];
}

void NBodyClock::ensure(long k) {
    if (have_ && k == k_) return;
    if (have_ && k == k_ + 1) { s0_ = s1_; k_ = k; s1_ = s0_; nbodyStep(s1_, h_, gr_); steps_++; return; }
    if (have_ && k > k_ && k - k_ < kEvery) {                          // a short way on: just step
        while (k_ < k) { s0_ = s1_; k_++; s1_ = s0_; nbodyStep(s1_, h_, gr_); steps_++; }
        return;
    }
    long j = k >= 0 ? k / kEvery : -((-k + kEvery - 1) / kEvery);       // floor(k / kEvery)
    NBodyState s = checkpoint(j);
    long at = j * kEvery;
    for (; at < k; at++) { nbodyStep(s, h_, gr_); steps_++; }
    s0_ = s; s1_ = s; nbodyStep(s1_, h_, gr_); steps_++;
    k_ = k; have_ = true;
}

void NBodyClock::stateAt(double tau, Vec3* x, Vec3* v) {
    const double q = tau / h_;
    const long k = long(std::floor(q));
    const double u = q - k;
    ensure(k);
    // cubic Hermite between the grid states (positions), and its derivative (velocities)
    const double u2 = u * u, u3 = u2 * u;
    const double h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
    const double d00 = (6 * u2 - 6 * u) / h_, d10 = (3 * u2 - 4 * u + 1), d01 = (-6 * u2 + 6 * u) / h_, d11 = (3 * u2 - 2 * u);
    for (int i = 0; i < kNBodies; i++) {
        x[i] = s0_.x[i] * h00 + s0_.v[i] * (h10 * h_) + s1_.x[i] * h01 + s1_.v[i] * (h11 * h_);
        v[i] = s0_.x[i] * d00 + s0_.v[i] * d10 + s1_.x[i] * d01 + s1_.v[i] * d11;
    }
}

}  // namespace orbit
