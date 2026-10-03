// Tests for "Planet physics: Real" (orbit/nbody.*): independent checks of the integrator, the conserved quantities and the clock. Prints the real numbers.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "orbit/nbody.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"

using namespace orbit;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }

static const double kYearS = 365.25 * 86400;      // Julian year
static double msSince(std::chrono::steady_clock::time_point t0) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }

// ---- an independent exact two-body (Kepler) solution: the relative orbit with mu = GM1 + GM2, started at perihelion ----
static Vec3 keplerRel(double a, double e, double mu, double t, Vec3* vel) {
    const double n = std::sqrt(mu / (a * a * a)), M = n * t;
    double E = M;
    for (int i = 0; i < 100; i++) E -= (E - e * std::sin(E) - M) / (1 - e * std::cos(E));            // Newton's method on Kepler's equation
    const double b = a * std::sqrt(1 - e * e), Ed = n / (1 - e * std::cos(E));
    if (vel) *vel = Vec3{-a * std::sin(E) * Ed, b * std::cos(E) * Ed, 0};
    return Vec3{a * (std::cos(E) - e), b * std::sin(E), 0};
}

static void testTwoBody() {
    const double gm[2] = {kRealGM[0], kRealGM[5]};                  // the Sun and the Jupiter system
    const double a = 5.2 * units::kAU, e = 0.3, mu = gm[0] + gm[1], T = 2 * units::kPi * std::sqrt(a * a * a / mu);
    Vec3 rv;
    const Vec3 r0 = keplerRel(a, e, mu, 0, &rv);
    Vec3 x[2], v[2];
    x[0] = r0 * (-gm[1] / mu); x[1] = r0 * (gm[0] / mu);            // barycentric, centre of mass at the origin, zero momentum
    v[0] = rv * (-gm[1] / mu); v[1] = rv * (gm[0] / mu);
    const double h = kNBodyStep;
    const long orbits = 20;
    double t = 0, worst = 0, tCross = 0;
    double prevAng = 0, unwrapped = 0;
    int crossings = 0;
    const long nSteps = long(orbits * T / h) + 1;
    for (long i = 1; i <= nSteps; i++) {
        nbodyStepN(2, gm, x, v, h);
        t += h;
        const Vec3 rel = x[1] - x[0];
        if (i % 97 == 0) {                                          // compare with the exact solution now
            const Vec3 ex = keplerRel(a, e, mu, t, nullptr);
            worst = std::max(worst, length(rel - ex) / a);
        }
        double ang = std::atan2(rel.y, rel.x), d = ang - prevAng;
        if (d < -units::kPi) d += 2 * units::kPi; else if (d > units::kPi) d -= 2 * units::kPi;
        const double before = unwrapped; unwrapped += d; prevAng = ang;
        if (std::floor(unwrapped / (2 * units::kPi)) > std::floor(before / (2 * units::kPi)) && unwrapped > 0) {   // passed another full turn: time by interpolation
            crossings++;
            const double target = 2 * units::kPi * std::floor(unwrapped / (2 * units::kPi));
            tCross = t - h * (unwrapped - target) / d;
        }
    }
    const double Tmeasured = tCross / crossings;
    std::printf("two-body (Sun + Jupiter system, a = 5.2 AU, e = 0.3), %ld orbits (%.0f years): worst position error vs the exact Kepler solution %.2e of a; period measured %.6f d, exact 2*pi*sqrt(a^3/mu) = %.6f d (relative difference %.2e)\n",
                orbits, orbits * T / kYearS, worst, Tmeasured / 86400, T / 86400, std::fabs(Tmeasured / T - 1));
    check("two-body: positions match the exact Kepler solution to 1e-6 of a over 20 orbits", worst < 1e-6);
    check("two-body: the period matches 2 pi sqrt(a^3 / (GM_sun + GM_planet)) to 1e-7", crossings == orbits && std::fabs(Tmeasured / T - 1) < 1e-7);
}

static void testEightPlanets() {
    NBodyState s = nbodyStartState();
    const double h = kNBodyStep;
    const double e0 = nbodyEnergy(s);
    const Vec3 l0 = nbodyAngularMomentum(s), p0 = nbodyMomentum(s);
    double scaleP = 0; for (int i = 0; i < kNBodies; i++) scaleP += kRealGM[i] * length(s.v[i]);
    double maxE = 0, maxL = 0, maxP = 0, maxCom = 0;
    const long n = long(1000 * kYearS / h);
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 1; i <= n; i++) {
        nbodyStep(s, h);
        if (i % 50 == 0) {
            maxE = std::max(maxE, std::fabs(nbodyEnergy(s) / e0 - 1));
            maxL = std::max(maxL, length(nbodyAngularMomentum(s) - l0) / length(l0));
            maxP = std::max(maxP, length(nbodyMomentum(s)) / scaleP);
            maxCom = std::max(maxCom, length(nbodyCentreOfMass(s)));
        }
    }
    std::printf("eight planets, 1,000 years, step %.1f d (%ld steps, %.0f ms): max |dE/E| = %.3e, max |dL|/|L| = %.3e, max |P| / sum(GM |v|) = %.3e (start %.1e), max |centre of mass| = %.3e m (the Sun is %.2e m across)\n",
                h / 86400, n, msSince(t0), maxE, maxL, maxP, length(p0) / scaleP, maxCom, 2 * 6.957e8);
    check("energy: relative error bounded below 1e-8 over 1,000 years", maxE < 1e-8);
    check("angular momentum: relative error below 1e-12 (rounding level)", maxL < 1e-12);
    check("momentum: total momentum stays at rounding level (below 1e-13 of sum GM |v|)", maxP < 1e-13);
    check("centre of mass stays within 1 km of the origin over 1,000 years", maxCom < 1e3);
}

static double maxRelDiff(const NBodyState& a, const NBodyState& b) {                // the biggest position difference, as a fraction of the body's distance from the Sun
    double w = 0;
    for (int i = 1; i < kNBodies; i++) w = std::max(w, length(a.x[i] - b.x[i]) / length(a.x[i] - a.x[0]));
    return w;
}

static void testReversal() {
    NBodyState s = nbodyStartState();
    const NBodyState start = s;
    const double h = kNBodyStep;
    const long n = long(100 * kYearS / h);
    for (long i = 0; i < n; i++) nbodyStep(s, h);
    const double fwd = maxRelDiff(s, start);
    for (long i = 0; i < n; i++) nbodyStep(s, -h);
    const double back = maxRelDiff(s, start);
    std::printf("time reversal: 100 years forward then 100 years back (%ld + %ld steps): the planets moved %.2e (relative to their distance) and came back to within %.2e\n", n, n, fwd, back);
    check("time reversal: back at the start to within 1e-9 of the planets' distances (the planets had moved by order 1); rounding errors of 73,000 steps, mostly Mercury's phase, are about 1e-10", fwd > 0.1 && back < 1e-9);
}

static void testConvergence() {
    const double daysRun = 3652;                                                     // about 10 years; a whole number of steps for every step size below
    auto run = [&](double hDays) {
        NBodyState s = nbodyStartState();
        const double h = hDays * 86400;
        const long n = long(std::llround(daysRun / hDays));
        for (long i = 0; i < n; i++) nbodyStep(s, h);
        return s;
    };
    const NBodyState ref = run(0.125);                                               // the reference: a step 8 times smaller than the app's
    const double e4 = maxRelDiff(run(4), ref), e2 = maxRelDiff(run(2), ref), e1 = maxRelDiff(run(1), ref), e05 = maxRelDiff(run(0.5), ref);
    const double p1 = std::log2(e4 / e2), p2 = std::log2(e2 / e1), p3 = std::log2(e1 / e05);
    std::printf("convergence over 3,652 days vs a 0.125 d reference: errors %.3e (4 d), %.3e (2 d), %.3e (1 d), %.3e (0.5 d): orders %.2f, %.2f, %.2f (the scheme is 2nd order)\n", e4, e2, e1, e05, p1, p2, p3);
    check("convergence: halving the step reduces the error by about 2^2 (order between 1.8 and 2.2)", p1 > 1.8 && p1 < 2.2 && p2 > 1.8 && p2 < 2.2 && p3 > 1.8 && p3 < 2.2);
}

static void testClock() {
    NBodyClock clock;
    Vec3 x[kNBodies], v[kNBodies];
    const NBodyState start = nbodyStartState();
    clock.stateAt(0, x, v);
    double d0 = 0; for (int i = 0; i < kNBodies; i++) d0 = std::max(d0, length(x[i] - start.x[i]) + length(v[i] - start.v[i]));
    check("clock: at the reference epoch the state is the start state", d0 == 0);
    // the start state against the app's own elements: heliocentric positions are the elements' positions, velocities scaled by sqrt((GM_sun + GM_i) / kGMSun)
    double dpos = 0, dvel = 0;
    for (int i = 1; i < kNBodies; i++) {
        const State st = planetStateAt(std::size_t(i - 1), kNBodyRefJD);
        dpos = std::max(dpos, length((start.x[i] - start.x[0]) - st.pos) / length(st.pos));
        dvel = std::max(dvel, length((start.v[i] - start.v[0]) - st.vel * std::sqrt((kRealGM[0] + kRealGM[i]) / units::kGMSun)) / length(st.vel));
    }
    std::printf("start state vs the app's orbital elements at the reference epoch: heliocentric positions differ by %.1e, velocities (after the GM scaling) by %.1e (relative)\n", dpos, dvel);
    check("clock: the start state is the elements' state (positions and scaled velocities), to rounding", dpos < 1e-12 && dvel < 1e-12);
    // a jump 100 years on and 100 years back, against stepping directly
    const double h = kNBodyStep;
    NBodyState direct = start;
    const long n = long(std::llround(100 * kYearS / h));
    for (long i = 0; i < n; i++) nbodyStep(direct, h);
    clock.stateAt(n * h, x, v);
    NBodyState viaClock; for (int i = 0; i < kNBodies; i++) { viaClock.x[i] = x[i]; viaClock.v[i] = v[i]; }
    const double fw = maxRelDiff(viaClock, direct);
    NBodyState dback = start;
    for (long i = 0; i < n; i++) nbodyStep(dback, -h);
    clock.stateAt(-n * h, x, v);
    for (int i = 0; i < kNBodies; i++) { viaClock.x[i] = x[i]; viaClock.v[i] = v[i]; }
    const double bw = maxRelDiff(viaClock, dback);
    std::printf("clock: a jump 100 years forward differs from stepping directly by %.1e, 100 years back by %.1e (relative to the planets' distances)\n", fw, bw);
    check("clock: jumps forward and back agree with stepping directly (below 1e-10)", fw < 1e-10 && bw < 1e-10);
    // between grid steps: the interpolated position against a much finer integration of the same flow, started from the grid state
    {
        const long k = 2461;
        clock.stateAt(k * h, x, v);
        NBodyState g0; for (int i = 0; i < kNBodies; i++) { g0.x[i] = x[i]; g0.v[i] = v[i]; }
        const double frac = 0.31;
        NBodyState fine = g0;
        const int sub = 64, nf = int(std::llround(frac * sub));
        for (int i = 0; i < nf; i++) nbodyStep(fine, h / sub);
        clock.stateAt((k + double(nf) / sub) * h, x, v);
        NBodyState itp; for (int i = 0; i < kNBodies; i++) { itp.x[i] = x[i]; itp.v[i] = v[i]; }
        const double ie = maxRelDiff(itp, fine);
        std::printf("clock: the Hermite interpolation (0.31 of a step on) differs from a 64-times-finer integration of the same flow by %.1e\n", ie);
        check("clock: interpolated positions are good to 1e-6 of the planets' distances", ie < 1e-6);
    }
    // time of a jump in this machine: the first visit to the far ends of the app's date range (1800 and 2100), then again
    NBodyClock c2;
    const double j1800 = (-226.0 * kYearS), j2100 = 75.0 * kYearS;   // 1800-01-01 and 2100-12-31 are 226 years before and 75 years after the reference epoch (2026-01-01)
    auto t0 = std::chrono::steady_clock::now(); c2.stateAt(j1800, x, v); const double ms1 = msSince(t0);
    t0 = std::chrono::steady_clock::now(); c2.stateAt(j2100, x, v); const double ms2 = msSince(t0);
    t0 = std::chrono::steady_clock::now(); c2.stateAt(j1800 + 12345.0 * 86400, x, v); const double ms3 = msSince(t0);
    t0 = std::chrono::steady_clock::now(); c2.stateAt(j2100 - 5.0 * kYearS, x, v); const double ms4 = msSince(t0);
    std::printf("clock timing here (software, one core): first jump to 1800 %.0f ms (%ld steps), then to 2100 %.0f ms, a second jump near 1800 %.1f ms, near 2100 %.1f ms; %zu checkpoints kept\n", ms1, c2.stepsTaken(), ms2, ms3, ms4, c2.checkpoints());
    check("clock: checkpoints make a second jump cheap (a few thousand steps at most)", ms3 < 50 && ms4 < 50);
}

// Mercury's perihelion advance caused by the other planets in this model (no general relativity). Published value for the planetary perturbations:
// about 532 arcseconds per century (Clemence 1947, Reviews of Modern Physics 19, "The Relativity Effect in Planetary Motions"; the 532 is as quoted in secondary
// text, Clemence's own number was not read). The 43 arcseconds per century of general relativity are NOT in this model.
static void testMercuryPerihelion() {
    NBodyState s = nbodyStartState();
    const double h = kNBodyStep;
    const double years = 500;
    const long n = long(years * kYearS / h);
    std::vector<double> ts, ws;
    double unwrap = 0, prev = 0;
    for (long i = 0; i <= n; i++) {
        if (i % 40 == 0) {
            const Vec3 r = s.x[1] - s.x[0], vv = s.v[1] - s.v[0];
            const double mu = kRealGM[0] + kRealGM[1];
            const Vec3 hv = cross(r, vv), ev = cross(vv, hv) * (1.0 / mu) - r * (1.0 / length(r));       // eccentricity vector (points to perihelion)
            const double w = std::atan2(ev.y, ev.x);
            double d = w - prev; if (d < -units::kPi) d += 2 * units::kPi; else if (d > units::kPi) d -= 2 * units::kPi;
            unwrap = ts.empty() ? w : unwrap + d; prev = w;
            ts.push_back(i * h / (100 * kYearS)); ws.push_back(unwrap);
        }
        if (i < n) nbodyStep(s, h);
    }
    double sx = 0, sy = 0, sxx = 0, sxy = 0; const double m = double(ts.size());           // least-squares line: slope in rad per century
    for (std::size_t i = 0; i < ts.size(); i++) { sx += ts[i]; sy += ws[i]; sxx += ts[i] * ts[i]; sxy += ts[i] * ws[i]; }
    const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx), arcsec = slope * 180 / units::kPi * 3600;
    std::printf("Mercury's perihelion advance in this model over %.0f years: %.1f arcseconds per century (published, planetary perturbations: about 532; general relativity adds about 43 more and is not in this model)\n", years, arcsec);
    check("Mercury's perihelion advance from the planets is within 5 % of the published 532 arcseconds per century", std::fabs(arcsec / 532 - 1) < 0.05);
}

int main() {
    testTwoBody();
    testEightPlanets();
    testReversal();
    testConvergence();
    testClock();
    testMercuryPerihelion();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
