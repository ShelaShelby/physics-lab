#include "nbody.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "moon_elp.h"
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

// Earth alone and the Moon alone: https://ssd.jpl.nasa.gov/astro_par.html, "Planetary Masses" (JPL planetary ephemeris DE440), GM in km^3/s^2, read from the page itself on 2026-10-03.
// (kRealGM[3] above is their sum, written in the same way as before so that nothing in the planets' numbers changes.)
const double kRealGMEarthAlone = 398600.435507e9;
const double kRealGMPluto = 975.5e9;            // Pluto system, the same page (DE440): 975.500000 km^3/s^2 (nbody.h)
const double kRealGMMoon = 4902.800118e9;

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

void nbodyStep(NBodyState& s, double h, bool gr) { nbodyStepN(kNBodies, kRealGM, s.x, s.v, h, gr); }

// ---- 0.29: the accurate step (see nbody.h) ----
namespace {

// The pulls on every body: Newtonian point masses, the Earth-Moon split, the Sun's 1PN field. moon: the Moon seen from the Earth's centre at this instant (used when emb >= 0).
void accurateAccel(int n, const double* gm, const Vec3* x, const Vec3* v, Vec3* a, bool gr, int emb, const Vec3& moon) {
    // (0.36: two test particles, GM 0, do not pull on each other; the pair is skipped)
    for (int i = 0; i < n; i++) a[i] = Vec3{};
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            if (gm[i] == 0 && gm[j] == 0) continue;
            const Vec3 d = x[j] - x[i];
            const double r2 = dot(d, d), inv3 = 1.0 / (r2 * std::sqrt(r2));
            a[i] = a[i] + d * (gm[j] * inv3);
            a[j] = a[j] - d * (gm[i] * inv3);
        }
    if (emb >= 0) {
        // The Earth and the Moon are pulled separately: the barycentre's acceleration is the GM-weighted mean of the pulls at the Earth's and the Moon's real places,
        // not the pull at the barycentre (the difference is the Sun's and planets' tide on the Earth-Moon pair). Every other body feels the Earth and the Moon at their
        // own places in return, so the total momentum stays exactly as in the point-mass sum.
        const double gE = kRealGMEarthAlone, gMoon = kRealGMMoon, gSum = gE + gMoon;
        const Vec3 xe = x[emb] - moon * (gMoon / gSum), xm = x[emb] + moon * (gE / gSum);
        for (int j = 0; j < n; j++) {
            if (j == emb) continue;
            const Vec3 de = x[j] - xe, dm = x[j] - xm, db = x[j] - x[emb];
            const double re = dot(de, de), rm = dot(dm, dm), rb = dot(db, db);
            const Vec3 pe = de * (1.0 / (re * std::sqrt(re))), pm = dm * (1.0 / (rm * std::sqrt(rm))), pb = db * (1.0 / (rb * std::sqrt(rb)));   // per unit GM of j
            const Vec3 diff = (pe * gE + pm * gMoon) * (1.0 / gSum) - pb;
            a[emb] = a[emb] + diff * gm[j];
            a[j] = a[j] - diff * gSum;
        }
    }
    if (gr) {
        // The Sun's first-order (1PN) Schwarzschild field on each planet, beta = gamma = 1 (IERS Conventions 2010, IERS Technical Note 36, eq. 10.12, written there for a
        // satellite of the Earth; its first, Schwarzschild, term; data/iers holds the chapter): a = GM / (c^2 r^3) [ (4 GM / r - v^2) r_vec + 4 (r_vec . v_vec) v_vec ], r_vec and v_vec the planet's position and velocity relative to the Sun.
        // The Sun gets the opposite pull (weighted by GM), so the total momentum does not change.
        const double mu = gm[0], c2 = kLightSpeed * kLightSpeed;
        for (int i = 1; i < n; i++) {
            const Vec3 r = x[i] - x[0], w = v[i] - v[0];
            const double rr = std::sqrt(dot(r, r));
            const Vec3 g = (r * (4.0 * mu / rr - dot(w, w)) + w * (4.0 * dot(r, w))) * (mu / (c2 * rr * rr * rr));
            a[i] = a[i] + g;
            a[0] = a[0] - g * (gm[i] / mu);
        }
    }
}

// The Moon at a fraction u (0..1) of the step: the quadratic through its places at the start, the middle and the end.
Vec3 moonAt(const Vec3* m3, double u) {
    const double l0 = 2 * (u - 0.5) * (u - 1), l1 = -4 * u * (u - 1), l2 = 2 * u * (u - 0.5);
    return m3[0] * l0 + m3[1] * l1 + m3[2] * l2;
}

}  // namespace

void nbodyStepAccurateN(int n, const double* gm, Vec3* x, Vec3* v, double h, bool gr, int emb, const Vec3* moon3) {
    // Gragg's modified midpoint rule with n_q = 2, 4, ..., 2 kGbsStages substeps, extrapolated to substep 0 in powers of the substep squared (Aitken-Neville):
    // E. Hairer, S. P. Norsett & G. Wanner, Solving Ordinary Differential Equations I, 2nd ed. (Springer 1993), section II.9.
    Vec3 tx[kGbsStages][kNBodyMax], tv[kGbsStages][kNBodyMax];
    const Vec3 noMoon{};
    for (int q = 0; q < kGbsStages; q++) {
        const int steps = 2 * (q + 1);
        const double sub = h / steps;
        Vec3 x0[kNBodyMax], v0[kNBodyMax], x1[kNBodyMax], v1[kNBodyMax], a[kNBodyMax];
        for (int i = 0; i < n; i++) { x0[i] = x[i]; v0[i] = v[i]; }
        accurateAccel(n, gm, x0, v0, a, gr, emb, moon3 ? moonAt(moon3, 0.0) : noMoon);
        for (int i = 0; i < n; i++) { x1[i] = x0[i] + v0[i] * sub; v1[i] = v0[i] + a[i] * sub; }
        for (int m = 1; m < steps; m++) {
            accurateAccel(n, gm, x1, v1, a, gr, emb, moon3 ? moonAt(moon3, double(m) / steps) : noMoon);
            for (int i = 0; i < n; i++) {
                const Vec3 nx = x0[i] + v1[i] * (2 * sub), nv = v0[i] + a[i] * (2 * sub);
                x0[i] = x1[i]; v0[i] = v1[i]; x1[i] = nx; v1[i] = nv;
            }
        }
        accurateAccel(n, gm, x1, v1, a, gr, emb, moon3 ? moonAt(moon3, 1.0) : noMoon);
        for (int i = 0; i < n; i++) { tx[q][i] = (x0[i] + x1[i] + v1[i] * sub) * 0.5; tv[q][i] = (v0[i] + v1[i] + a[i] * sub) * 0.5; }
        for (int p = q - 1; p >= 0; p--) {
            const double ratio = double(q + 1) / double(p + 1), f = 1.0 / (ratio * ratio - 1);
            for (int i = 0; i < n; i++) { tx[p][i] = tx[p + 1][i] + (tx[p + 1][i] - tx[p][i]) * f; tv[p][i] = tv[p + 1][i] + (tv[p + 1][i] - tv[p][i]) * f; }
        }
    }
    // The pulls are equal and opposite, so the total momentum and the motion of the centre of mass are exact properties of the flow; the extrapolation keeps them only to
    // rounding, which adds up over a long run (2e-12 of sum GM |v| in 300 years, measured). As in the Wisdom-Holman step, the Sun is put where the balance says.
    Vec3 p0, c0;
    double mt = 0;
    for (int i = 0; i < n; i++) { p0 = p0 + v[i] * gm[i]; c0 = c0 + x[i] * gm[i]; mt += gm[i]; }
    for (int i = 0; i < n; i++) { x[i] = tx[0][i]; v[i] = tv[0][i]; }
    Vec3 p1, c1;
    for (int i = 1; i < n; i++) { p1 = p1 + v[i] * gm[i]; c1 = c1 + x[i] * gm[i]; }
    v[0] = (p0 - p1) * (1.0 / gm[0]);
    x[0] = (c0 + p0 * h - c1) * (1.0 / gm[0]);
}

// 0.36: the GMs of the app's bodies in the accurate step's order: the nine, the Pluto system, and the four dwarf planets with GM 0 (test particles; nbody.h)
static const double kAccurateGM[kNBodyMax] = {kRealGM[0], kRealGM[1], kRealGM[2], kRealGM[3], kRealGM[4], kRealGM[5], kRealGM[6], kRealGM[7], kRealGM[8], kRealGMPluto, 0, 0, 0, 0};
static_assert(kNBodyMax == kNBodies + 1 + 4, "the GM list above has four test particles");

// 0.36 stage 4: the accelerations of a state at JD (TDB), exactly as the accurate step works them out (for the interpolation of NBodyClock::stateAt)
static void stateAccel(const NBodyState& s, bool gr, double jdTDB, Vec3* a) {
    Vec3 x[kNBodyMax], v[kNBodyMax];
    for (int i = 0; i < kNBodies; i++) { x[i] = s.x[i]; v[i] = s.v[i]; }
    x[kNBodies] = s.px; v[kNBodies] = s.pv;
    for (int i = 0; i < kNBodyTest && s.dwarfs; i++) { x[kNBodies + 1 + i] = s.dx[i]; v[kNBodies + 1 + i] = s.dv[i]; }
    const int n = !s.pluto ? kNBodies : s.dwarfs ? kNBodyMax : kNBodies + 1;
    for (int i = 0; i < kNBodyMax; i++) a[i] = Vec3{};
    accurateAccel(n, s.pluto ? kAccurateGM : kRealGM, x, v, a, gr, 3, moonGeocentricElp(jdTDB, kMoonSplitTerms));
}

void nbodyStepAccurate(NBodyState& s, double h, bool gr, double jdTDB) {
    // the Moon at the start, the middle and the end of the step, from the largest terms of ELP2000-82B; the last few are kept (a step's end is the next one's start)
    struct Cached { double jd; Vec3 m; };
    static thread_local Cached cache[6] = {{-1e300, {}}, {-1e300, {}}, {-1e300, {}}, {-1e300, {}}, {-1e300, {}}, {-1e300, {}}};
    static thread_local int next = 0;
    auto moon = [&](double jd) {
        for (const Cached& c : cache) if (c.jd == jd) return c.m;
        const Vec3 m = moonGeocentricElp(jd, kMoonSplitTerms);
        cache[next] = {jd, m}; next = (next + 1) % 6;
        return m;
    };
    const double days = h / 86400.0;
    const Vec3 m3[3] = {moon(jdTDB), moon(jdTDB + 0.5 * days), moon(jdTDB + days)};
    if (!s.pluto) { nbodyStepAccurateN(kNBodies, kRealGM, s.x, s.v, h, gr, 3, m3); return; }
    // 0.36: the four dwarf planets after Pluto with GM 0 (test particles; nbody.h). A pull of GM 0 adds an exact zero, so the others move bit for bit as without them.
    const double* gm = kAccurateGM;
    Vec3 x[kNBodyMax], v[kNBodyMax];
    for (int i = 0; i < kNBodies; i++) { x[i] = s.x[i]; v[i] = s.v[i]; }
    x[kNBodies] = s.px; v[kNBodies] = s.pv;
    const int n = s.dwarfs ? kNBodyMax : kNBodies + 1;
    for (int i = 0; i < kNBodyTest && s.dwarfs; i++) { x[kNBodies + 1 + i] = s.dx[i]; v[kNBodies + 1 + i] = s.dv[i]; }
    nbodyStepAccurateN(n, gm, x, v, h, gr, 3, m3);
    for (int i = 0; i < kNBodies; i++) { s.x[i] = x[i]; s.v[i] = v[i]; }
    s.px = x[kNBodies]; s.pv = v[kNBodies];
    for (int i = 0; i < kNBodyTest && s.dwarfs; i++) { s.dx[i] = x[kNBodies + 1 + i]; s.dv[i] = v[kNBodies + 1 + i]; }
}

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
    if (s.pluto) p = p + s.pv * kRealGMPluto;          // 0.32
    return p;
}
Vec3 nbodyCentreOfMass(const NBodyState& s) {
    Vec3 c; double m = 0;
    for (int i = 0; i < kNBodies; i++) { c = c + s.x[i] * kRealGM[i]; m += kRealGM[i]; }
    if (s.pluto) { c = c + s.px * kRealGMPluto; m += kRealGMPluto; }   // 0.32
    return c * (1.0 / m);
}

// JPL Horizons (DE441, downloaded 2026-10-03; native/data/jpl) at 2026-01-01 12:00 TDB, barycentric (JPL's Solar System Barycenter), ecliptic and mean equinox of J2000:
// { {x, y, z} (m), {vx, vy, vz} (m/s) } for the Sun, Mercury, Venus, Earth-Moon barycentre, Mars ... Neptune (system barycentres). Made by tools/make_jpl_data.py.
static const double kJplEpoch[kNBodies][2][3] =
#include "jpl_epoch.inc"
;

NBodyState nbodyStartState(bool withPluto) {
    NBodyState s;
    for (int i = 0; i < kNBodies; i++) {
        s.x[i] = Vec3{kJplEpoch[i][0][0], kJplEpoch[i][0][1], kJplEpoch[i][0][2]};
        s.v[i] = Vec3{kJplEpoch[i][1][0], kJplEpoch[i][1][1], kJplEpoch[i][1][2]};
    }
    // 0.32: the Pluto system, Horizons' Pluto Barycenter (9) at 2026-01-01 12:00 TDB (data/jpl/horizons-pluto.txt, the 2026 row; km and km/s, ecliptic J2000, from the barycentre)
    s.pluto = withPluto;
    s.px = Vec3{2.876200408189997E+09, -4.436685205826147E+09, -3.572159698157532E+08} * 1e3;
    s.pv = Vec3{4.711860617340981E+00, 1.747366704852745E+00, -1.549925488214824E+00} * 1e3;
    const double gp = withPluto ? kRealGMPluto : 0.0;
    double total = gp; Vec3 c = s.px * gp, w = s.pv * gp;                 // the GM-weighted mean position and velocity (the lab's own centre of mass)
    for (int i = 0; i < kNBodies; i++) { total += kRealGM[i]; c = c + s.x[i] * kRealGM[i]; w = w + s.v[i] * kRealGM[i]; }
    c = c * (1.0 / total); w = w * (1.0 / total);
    for (int i = 0; i < kNBodies; i++) { s.x[i] = s.x[i] - c; s.v[i] = s.v[i] - w; }
    if (withPluto) { s.px = s.px - c; s.pv = s.pv - w; } else s.px = s.pv = Vec3{};
    // 0.36: Ceres, Haumea, Makemake, Eris (test particles, nbody.h): Horizons' 2026-01-01 12:00 TDB rows of data/jpl/horizons-ceres.txt, -haumea, -makemake, -eris
    // (small-body solutions JPL#48, JPL#132, JPL#130, JPL#103; km and km/s, ecliptic J2000, from the Solar System Barycenter), moved to the lab's centre of mass like the rest
    static const double kDwarfEpoch[kNBodyTest][2][3] = {
        {{3.803498785233887E+08, 1.924888470820131E+08, -6.401516857199971E+07}, {-8.440922238208920E+00, 1.477523747984418E+01, 2.024854733395276E+00}},
        {{-5.538891352256805E+09, -3.532632762718422E+09, 3.523084736201016E+09}, {2.404685058980316E+00, -3.034291335176685E+00, -2.332680796719875E-01}},
        {{-6.879725037845993E+09, -1.354951910854890E+09, 3.611609291323755E+09}, {3.374102414782658E-01, -3.707437660140037E+00, -5.604809326498228E-01}},
        {{1.274735305282195E+10, 5.902892759274123E+09, -2.617828320966299E+09}, {-8.158217497691310E-01, 1.484943393472427E+00, 1.622885087809584E+00}}};
    s.dwarfs = withPluto;
    for (int i = 0; i < kNBodyTest; i++) {
        s.dx[i] = withPluto ? Vec3{kDwarfEpoch[i][0][0], kDwarfEpoch[i][0][1], kDwarfEpoch[i][0][2]} * 1e3 - c : Vec3{};
        s.dv[i] = withPluto ? Vec3{kDwarfEpoch[i][1][0], kDwarfEpoch[i][1][1], kDwarfEpoch[i][1][2]} * 1e3 - w : Vec3{};
    }
    return s;
}

NBodyState nbodyElementsStartState(double jd) {
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

NBodyClock::NBodyClock(double h, bool gr, const NBodyState* start) : h_(h), gr_(gr), start_(start ? *start : nbodyStartState(true)) {}

void NBodyClock::step(NBodyState& s, long k, int dir) {
    nbodyStepAccurate(s, dir * h_, gr_, kNBodyRefJD + double(k) * h_ / 86400.0);
    steps_++;
}

const NBodyState& NBodyClock::checkpoint(long j) {
    if (j == 0) return start_;
    auto it = cp_.find(j);
    if (it != cp_.end()) return it->second;
    // walk from the nearest known checkpoint towards j (stored ones are kept while |index| <= kKeep); always away from the epoch, so a checkpoint is the same whatever was asked first
    const long dir = j > 0 ? 1 : -1;
    long from = 0;
    NBodyState s = start_;
    if (dir > 0) { auto f = cp_.lower_bound(j); if (f != cp_.begin()) { --f; if (f->first > 0) { from = f->first; s = f->second; } } }   // the nearest kept one between the epoch and j
    else { auto f = cp_.upper_bound(j); if (f != cp_.end() && f->first < 0) { from = f->first; s = f->second; } }
    for (long q = from + dir; ; q += dir) {
        const long k0 = (q - dir) * kEvery;
        for (long n = 0; n < kEvery; n++) step(s, k0 + dir * n, int(dir));
        keep(q, s);
        if (q == j) break;
    }
    return cp_.at(j);
}

// 0.40: a checkpoint is kept for good while |index| <= kKeep; farther out only the kFar most recent are kept, so playing on past the year ~3126 walks from the newest
// one (one stretch of steps), not again and again from the last kept one (until 0.40 the walk there grew with every stretch, and the Orbit Lab fell to 1 frame a second).
void NBodyClock::keep(long q, const NBodyState& s) {
    if (q == 0 || cp_.count(q)) return;
    cp_[q] = s;
    if (std::labs(q) <= kKeep) return;
    farOrder_.push_back(q);
    if (farOrder_.size() > kFar) { cp_.erase(farOrder_.front()); farOrder_.pop_front(); }
}

namespace {
// Stretch j made from its anchor: after the epoch (j >= 0) forwards from its first state (checkpoint j), before it backwards from its last (checkpoint j + 1), with the same
// steps as NBodyClock::step and the checkpoint walk (so every state is bit for bit what walking there gives). No clock members: it may run on a background thread.
std::vector<NBodyGridPoint> makeStretch(const NBodyState& anchor, long j, long every, double h, bool gr) {
    std::vector<NBodyGridPoint> sg(std::size_t(every) + 1);
    if (j >= 0) {
        sg[0].s = anchor;
        for (long n = 1; n <= every; n++) { sg[std::size_t(n)].s = sg[std::size_t(n) - 1].s; nbodyStepAccurate(sg[std::size_t(n)].s, h, gr, kNBodyRefJD + double(j * every + n - 1) * h / 86400.0); }
    } else {
        sg[std::size_t(every)].s = anchor;
        for (long n = every; n > 0; n--) { sg[std::size_t(n) - 1].s = sg[std::size_t(n)].s; nbodyStepAccurate(sg[std::size_t(n) - 1].s, -h, gr, kNBodyRefJD + double(j * every + n) * h / 86400.0); }
    }
    // 0.36 stage 6: each state's pulls, for the interpolation (made here, in the background, not on the drawing thread: about 7 us each)
    for (long n = 0; n <= every; n++) stateAccel(sg[std::size_t(n)].s, gr, kNBodyRefJD + double(j * every + n) * h / 86400.0, sg[std::size_t(n)].a);
    return sg;
}
}  // namespace

const NBodyState* NBodyClock::knownCheckpoint(long j) const {
    if (j == 0) return &start_;
    auto it = cp_.find(j);
    return it == cp_.end() ? nullptr : &it->second;
}

void NBodyClock::addStretch(long j, Stretch&& sg) {
    if (seg_.count(j)) return;
    if (seg_.size() >= kStretches) {                                         // forget the stretch used longest ago
        long oldest = seg_.begin()->first;
        for (const auto& [q, t] : segUse_) if (t < segUse_[oldest]) oldest = q;
        seg_.erase(oldest); segUse_.erase(oldest);
    }
    // both ends are checkpoints (made by the same steps as the walk): keep them, so the next stretch does not walk the same steps again
    keep(j, sg.front().s);
    keep(j + 1, sg.back().s);
    segUse_[j] = ++useClock_;
    seg_.emplace(j, std::move(sg));
}

bool NBodyClock::needsWalk(double tau) const {
    const long j = stretchOf(stepOf(tau));
    if (seg_.count(j)) return false;
    for (const Ahead& e : ahead_) if (e.j == j) return false;
    const long a = j >= 0 ? j : j + 1;                                       // the stretch's anchor (checkpoint() walks to it)
    if (knownCheckpoint(a)) return false;
    // the walk from the nearest known checkpoint: at most one stretch's steps is not a walk worth a worker
    const long dir = a > 0 ? 1 : -1;
    long from = 0;
    if (dir > 0) { auto f = cp_.lower_bound(a); if (f != cp_.begin()) { --f; if (f->first > 0) from = f->first; } }
    else { auto f = cp_.upper_bound(a); if (f != cp_.end() && f->first < 0) from = f->first; }
    return std::labs(a - from) > 1;
}

void NBodyClock::prepare(double tau) {
    wantWalk_ = true; wantTau_ = tau;
    if (walk_.valid()) return;                                              // (one at a time: started when the one under way is taken in)
    wantWalk_ = false;
    if (!needsWalk(tau)) return;
    const long j = stretchOf(stepOf(tau)), a = j >= 0 ? j : j + 1, dir = a > 0 ? 1 : -1;
    long from = 0;
    NBodyState s = start_;
    if (dir > 0) { auto f = cp_.lower_bound(a); if (f != cp_.begin()) { --f; if (f->first > 0) { from = f->first; s = f->second; } } }   // as checkpoint() chooses
    else { auto f = cp_.upper_bound(a); if (f != cp_.end() && f->first < 0) { from = f->first; s = f->second; } }
    const double h = h_; const bool gr = gr_;
    walkJ_ = j;
    walk_ = std::async(std::launch::async, [s, from, a, dir, j, h, gr]() mutable {
        Walk w;
        w.j = j;
        for (long q = from + dir; ; q += dir) {                              // the same steps as checkpoint()'s walk (NBodyClock::step)
            const long k0 = (q - dir) * kEvery;
            for (long n = 0; n < kEvery; n++) nbodyStepAccurate(s, double(dir) * h, gr, kNBodyRefJD + double(k0 + dir * n) * h / 86400.0);
            w.steps += kEvery;
            w.cps.emplace_back(q, s);
            if (q == a) break;
        }
        w.sg = makeStretch(s, j, kEvery, h, gr);
        w.steps += kEvery;
        return w;
    }).share();
}

bool NBodyClock::prepared() {
    if (walk_.valid()) {
        if (walk_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
        Walk w = walk_.get();
        walk_ = {};
        for (const auto& [q, st] : w.cps) keep(q, st);
        steps_ += w.steps;
        addStretch(w.j, std::move(w.sg));
    }
    if (wantWalk_) { prepare(wantTau_); if (walk_.valid()) return false; }
    return true;
}

void NBodyClock::ensure(long k) {
    if (have_ && k == k_) return;
    const int dir = !have_ || k >= k_ ? 1 : -1;                              // the direction time is going
    const long j = k >= 0 ? k / kEvery : -((-k + kEvery - 1) / kEvery);       // floor(k / kEvery): the stretch that holds steps k and k + 1
    const long i = k - j * kEvery;                                           // 0 .. kEvery - 1
    auto it = seg_.find(j);
    if (it == seg_.end()) {
        Stretch sg;
        auto e = std::find_if(ahead_.begin(), ahead_.end(), [j](const Ahead& a) { return a.j == j; });
        if (e != ahead_.end()) {                                               // made ahead (waits if it is not finished yet)
            if (e->f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) waits_++;
            sg = e->f.get();
            ahead_.erase(e);
        } else sg = makeStretch(j >= 0 ? checkpoint(j) : checkpoint(j + 1), j, kEvery, h_, gr_);
        steps_ += kEvery;
        addStretch(j, std::move(sg));
        it = seg_.find(j);
    }
    segUse_[j] = ++useClock_;
    const Stretch& sg = it->second;
    const NBodyGridPoint &g0 = sg[std::size_t(i)], &g1 = sg[std::size_t(i) + 1];
    s0_ = g0.s; s1_ = g1.s;
    for (int b = 0; b < kNBodyMax; b++) { a0_[b] = g0.a[b]; a1_[b] = g1.a[b]; }
    k_ = k; have_ = true;
    if (prefetch_) topUp(j, dir);                                              // 0.90.0: the stretches ahead in the direction of travel
}

// 0.90.0: keeps the kAhead stretches after j in the direction dir being made: each from a kept checkpoint where there is one, else from the end of the
// stretch before it as it comes out of its worker (after the epoch going forwards its last state, before it going backwards its first: the checkpoint
// addStretch keeps, so the states are those of a stretch made from that checkpoint, bit for bit). Stretches no longer ahead are dropped.
void NBodyClock::topUp(long j, int dir) {
    for (auto e = ahead_.begin(); e != ahead_.end();) {
        const long m = (e->j - j) * dir;
        if (m < 1 || m > kAhead) { dropped_.push_back(e->f); e = ahead_.erase(e); } else ++e;
    }
    dropped_.erase(std::remove_if(dropped_.begin(), dropped_.end(), [](const std::shared_future<Stretch>& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }), dropped_.end());
    std::shared_future<Stretch> prev;                                         // the stretch before J being made ahead (none: made, or not chained)
    const double h = h_; const bool gr = gr_;
    for (int m = 1; m <= kAhead; m++) {
        const long J = j + m * dir;
        if (seg_.count(J)) { prev = {}; continue; }
        auto e = std::find_if(ahead_.begin(), ahead_.end(), [J](const Ahead& a) { return a.j == J; });
        if (e != ahead_.end()) { prev = e->f; continue; }
        std::shared_future<Stretch> f;
        if (const NBodyState* anchor = knownCheckpoint(J >= 0 ? J : J + 1)) f = std::async(std::launch::async, makeStretch, *anchor, J, kEvery, h, gr).share();
        else if (prev.valid() && ((dir > 0 && J >= 0) || (dir < 0 && J < 0)))
            f = std::async(std::launch::async, [prev, J, h, gr, dir] { const Stretch& p = prev.get(); return makeStretch(dir > 0 ? p.back().s : p.front().s, J, kEvery, h, gr); }).share();
        else break;                                                            // (its anchor is not known yet: the rest wait for a later frame)
        ahead_.push_back({J, f});
        prev = f;
    }
}

bool NBodyClock::aheadReady() const {
    for (const Ahead& e : ahead_) if (e.f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
    return true;
}

void NBodyClock::stateAt(double tau, Vec3* x, Vec3* v, Vec3* plutoX, Vec3* plutoV, Vec3* dwarfX, Vec3* dwarfV) {
    const double q = tau / h_;
    const long k = long(std::floor(q));
    const double u = q - k;
    ensure(k);
    // 0.36 stage 4: the quintic Hermite between the grid states (positions, velocities, accelerations at both ends), and its derivative (velocities).
    // x(u) = H0 x0 + H1 h v0 + H2 h^2 a0 + H3 h^2 a1 + H4 h v1 + H5 x1, u in [0, 1]; the basis is the standard one (each Hi is 1 for its own value or derivative at its end and 0
    // for all the others); tests/test_jpl.cpp checks it against Horizons every 7 hours. At u = 0 it gives the grid state itself, bit for bit.
    const double u2 = u * u, u3 = u2 * u, u4 = u3 * u, u5 = u4 * u, hh = h_ * h_;
    const double H0 = 1 - 10 * u3 + 15 * u4 - 6 * u5, H1 = u - 6 * u3 + 8 * u4 - 3 * u5, H2 = 0.5 * u2 - 1.5 * u3 + 1.5 * u4 - 0.5 * u5;
    const double H3 = 0.5 * u3 - u4 + 0.5 * u5, H4 = -4 * u3 + 7 * u4 - 3 * u5, H5 = 10 * u3 - 15 * u4 + 6 * u5;
    const double D0 = (-30 * u2 + 60 * u3 - 30 * u4) / h_, D1 = 1 - 18 * u2 + 32 * u3 - 15 * u4, D2 = u - 4.5 * u2 + 6 * u3 - 2.5 * u4;
    const double D3 = 1.5 * u2 - 4 * u3 + 2.5 * u4, D4 = -12 * u2 + 28 * u3 - 15 * u4, D5 = (30 * u2 - 60 * u3 + 30 * u4) / h_;
    auto pos = [&](const Vec3& x0, const Vec3& v0, const Vec3& a0, const Vec3& x1, const Vec3& v1, const Vec3& a1) {
        return x0 * H0 + v0 * (H1 * h_) + a0 * (H2 * hh) + a1 * (H3 * hh) + v1 * (H4 * h_) + x1 * H5;
    };
    auto vel = [&](const Vec3& x0, const Vec3& v0, const Vec3& a0, const Vec3& x1, const Vec3& v1, const Vec3& a1) {
        return v0 * D1 + x0 * D0 + a0 * (D2 * h_) + a1 * (D3 * h_) + v1 * D4 + x1 * D5;
    };
    for (int i = 0; i < kNBodies; i++) {
        x[i] = pos(s0_.x[i], s0_.v[i], a0_[i], s1_.x[i], s1_.v[i], a1_[i]);
        v[i] = vel(s0_.x[i], s0_.v[i], a0_[i], s1_.x[i], s1_.v[i], a1_[i]);
    }
    if (s0_.pluto && plutoX) *plutoX = pos(s0_.px, s0_.pv, a0_[kNBodies], s1_.px, s1_.pv, a1_[kNBodies]);
    if (s0_.pluto && plutoV) *plutoV = vel(s0_.px, s0_.pv, a0_[kNBodies], s1_.px, s1_.pv, a1_[kNBodies]);
    for (int i = 0; i < kNBodyTest && s0_.dwarfs; i++) {
        const int j = kNBodies + 1 + i;
        if (dwarfX) dwarfX[i] = pos(s0_.dx[i], s0_.dv[i], a0_[j], s1_.dx[i], s1_.dv[i], a1_[j]);
        if (dwarfV) dwarfV[i] = vel(s0_.dx[i], s0_.dv[i], a0_[j], s1_.dx[i], s1_.dv[i], a1_[j]);
    }
}

}  // namespace orbit
