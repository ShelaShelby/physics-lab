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
    // 0.10.3: the default start is the JPL state (tests/test_jpl.cpp checks it against the table). The LEGACY elements start (the 0.10.2 start, tests only) is still
    // what it was: heliocentric positions are the elements' positions, velocities scaled by sqrt((GM_sun + GM_i) / kGMSun)
    const NBodyState legacy = nbodyElementsStartState();
    double dpos = 0, dvel = 0;
    for (int i = 1; i < kNBodies; i++) {
        const State st = planetStateAt(std::size_t(i - 1), kNBodyRefJD);
        dpos = std::max(dpos, length((legacy.x[i] - legacy.x[0]) - st.pos) / length(st.pos));
        dvel = std::max(dvel, length((legacy.v[i] - legacy.v[0]) - st.vel * std::sqrt((kRealGM[0] + kRealGM[i]) / units::kGMSun)) / length(st.vel));
    }
    std::printf("legacy elements start vs the app's orbital elements at the reference epoch: heliocentric positions differ by %.1e, velocities (after the GM scaling) by %.1e (relative)\n", dpos, dvel);
    check("clock: the legacy elements start is the elements' state (positions and scaled velocities), to rounding", dpos < 1e-12 && dvel < 1e-12);
    // the default start is not the elements' state: Mercury is thousands of km from where the mean elements put it (the JPL state, not the elements, is the app's start)
    check("clock: the default start is the JPL state, not the elements' (Mercury's heliocentric position differs by more than 1,000 km)", length((start.x[1] - start.x[0]) - (legacy.x[1] - legacy.x[0])) > 1e6);
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
    std::printf("clock timing here (software, one core): first jump to 1800 %.0f ms (%lld steps), then to 2100 %.0f ms, a second jump near 1800 %.1f ms, near 2100 %.1f ms; %zu checkpoints kept\n", ms1, static_cast<long long>(c2.stepsTaken()), ms2, ms3, ms4, c2.checkpoints());
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

// ================= General relativity (1PN) for the planets, 0.10.1 =================
// The option adds the Nobili & Roxburgh (1986) potential of the Sun, Phi(r) = -3 (GM_sun)^2 / (c^2 r^2), to the planets (the same method as REBOUNDx's
// "gr_potential", Tamayo et al. 2020, MNRAS 491, 2885). Everything below is checked against things that do not come from the code: the standard 1PN formula,
// a published number, the 0.10 results saved before the change, and conservation laws.
static const double kC = units::kLightSpeed;                 // speed of light, m/s: CODATA exact value 299,792,458 (units.h)
static const double kCenturyS = 36525.0 * 86400;             // Julian century, s
static const double kArcsecPerRad = 180.0 / units::kPi * 3600;

// The standard first-order (1PN) extra advance of the perihelion per orbit, in radians: delta_omega = 6 pi GM / (c^2 a (1 - e^2)) (the standard result; not read from a source in this session; the tests below check it against the published 42.98 arcseconds per century).
static double grAdvancePerOrbit(double gm, double a, double e) { return 6 * units::kPi * gm / (kC * kC * a * (1 - e * e)); }

static double lineSlope(const std::vector<double>& t, const std::vector<double>& y) {      // least-squares slope
    double mt = 0, my = 0; const double m = double(t.size());
    for (std::size_t i = 0; i < t.size(); i++) { mt += t[i]; my += y[i]; }
    mt /= m; my /= m;
    double sxy = 0, sxx = 0;
    for (std::size_t i = 0; i < t.size(); i++) { sxy += (t[i] - mt) * (y[i] - my); sxx += (t[i] - mt) * (t[i] - mt); }
    return sxy / sxx;
}

// A planet-sized test body (Mercury's GM) alone with the Sun on an orbit (a, e), started at perihelion, for `orbits` orbits with step hDays. Returns the
// advance of the perihelion per orbit (rad) as the slope of the eccentricity-vector direction, sampled every step.
static double measuredAdvancePerOrbit(bool gr, double a, double e, long orbits, double hDays) {
    const double gm[2] = {kRealGM[0], kRealGM[1]};
    const double mu = gm[0] + gm[1], T = 2 * units::kPi * std::sqrt(a * a * a / mu), h = hDays * 86400;
    Vec3 rv; const Vec3 r0 = keplerRel(a, e, mu, 0, &rv);
    Vec3 x[2], v[2];
    x[0] = r0 * (-gm[1] / mu); x[1] = r0 * (gm[0] / mu);
    v[0] = rv * (-gm[1] / mu); v[1] = rv * (gm[0] / mu);
    const long nSteps = long(std::llround(orbits * T / h));
    std::vector<double> ts, ws; ts.reserve(std::size_t(nSteps) + 1); ws.reserve(std::size_t(nSteps) + 1);
    double prev = 0, unwrap = 0;
    for (long i = 0; i <= nSteps; i++) {
        const Vec3 r = x[1] - x[0], vv = v[1] - v[0], hv = cross(r, vv);
        const Vec3 ev = cross(vv, hv) * (1.0 / mu) - r * (1.0 / length(r));                // points to the perihelion
        const double w = std::atan2(ev.y, ev.x);
        double d = w - prev; if (d < -units::kPi) d += 2 * units::kPi; else if (d > units::kPi) d -= 2 * units::kPi;
        unwrap += d; prev = w;
        ts.push_back(i * h); ws.push_back(unwrap);
        if (i < nSteps) nbodyStepN(2, gm, x, v, h, gr);
    }
    return lineSlope(ts, ws) * T;
}

static void testGRFormula() {
    struct Orbit { const char* name; double a, e; long orbits; } orbitsToTry[] = {
        {"Mercury (a = 0.38709927 AU, e = 0.20563593: the app's elements)", 0.38709927 * units::kAU, 0.20563593, 500},
        {"an eccentric orbit (a = 0.5 AU, e = 0.5)", 0.5 * units::kAU, 0.5, 400},
    };
    for (const Orbit& o : orbitsToTry) {
        const double want = grAdvancePerOrbit(kRealGM[0], o.a, o.e);
        double rel[2];
        const double steps[2] = {0.25, 1.0};                            // 0.25 d: close to the exact flow; 1 d: the app's step
        for (int k = 0; k < 2; k++) {
            const double on = measuredAdvancePerOrbit(true, o.a, o.e, o.orbits, steps[k]), off = measuredAdvancePerOrbit(false, o.a, o.e, o.orbits, steps[k]);
            rel[k] = (on - off) / want - 1;
            std::printf("GR advance, %s, %ld orbits, step %.2f d: GR on - GR off = %.5e rad/orbit, formula 6 pi GM/(c^2 a (1-e^2)) = %.5e (relative difference %+.2e; GR off alone drifts %.1e rad/orbit)\n",
                        o.name, o.orbits, steps[k], on - off, want, rel[k], off);
        }
        // Limit for the fine step: the fit sees the short-period wobble of the osculating perihelion (amplitude of order GM/(c^2 p) ~ 1e-7 rad) against a total
        // advance of order 1e-4 rad, so about 1e-3; terms of the next order in GM/(c^2 p) are ~1e-7 and negligible.
        check(std::string("GR: perihelion advance for ") + o.name + " matches 6 pi GM / (c^2 a (1-e^2)) to 1e-3 at a 0.25 d step", std::fabs(rel[0]) < 1e-3);
        // The app's own 1-day step, same limit and same reason. (My first limit here was 2 %, a guess for a splitting error; the measured difference is ~1e-5, no
        // different from the 0.25 d step, so I use the fit-noise limit.)
        check(std::string("GR: the 1-day step (the app's) reproduces the formula for ") + o.name + " to 1e-3", std::fabs(rel[1]) < 1e-3);
    }
}

// Published value: Will, C. M., "The Confrontation between General Relativity and Experiment", Living Reviews in Relativity 17 (2014) 4: Mercury's perihelion
// shift is 42.98 arcseconds per century times (2 + 2 gamma - beta)/3, i.e. 42.98 for general relativity (gamma = beta = 1). Quoted from a search-engine excerpt of
// that review (the paper itself, on arXiv, could not be opened from the session that wrote this).
static void testGRPublished() {
    const double a = 0.38709927 * units::kAU, e = 0.20563593;
    const double mu = kRealGM[0] + kRealGM[1], T = 2 * units::kPi * std::sqrt(a * a * a / mu);
    const double on = measuredAdvancePerOrbit(true, a, e, 500, 0.25), off = measuredAdvancePerOrbit(false, a, e, 500, 0.25);
    const double perCentury = (on - off) * (kCenturyS / T) * kArcsecPerRad;
    std::printf("GR advance of Mercury (Sun + Mercury, 500 orbits, step 0.25 d): %.3f arcseconds per century; published (Will 2014): 42.98\n", perCentury);
    // Limit 1e-3 (0.043 arcsec): the published number is rounded to 0.01 (1.2e-4), and it uses Mercury's real orbit while the app uses its own AU (149.6e9 m, 1.0e-4
    // below the IAU value; the advance goes as a^(-5/2): 2.5e-4) and J2000 mean elements; the fit adds up to 4e-4 (see above).
    check("GR: Mercury's advance agrees with the published 42.98 arcseconds per century to 1e-3", std::fabs(perCentury / 42.98 - 1) < 1e-3);
}

// ---- with the option off, nothing changes from 0.10 ----
// The Sun and planets' positions after 3,652 one-day steps from the start state, saved from version 0.10.0 (the code before this change; GCC, x86-64) as exact
// binary floating-point numbers. A saved result of our own earlier code, so it only shows that nothing changed.
static const double kSaved010[kNBodies][3] = {
    {-0x1.638f6fb9a1d14p+28, -0x1.ee3a0b8fe0f08p+29, 0x1.7fb0240dcebcap+23},
    {0x1.966afe98afa3cp+34, 0x1.10773d66b151p+35, 0x1.06ed1a08bbd4ep+29},
    {0x1.8f072c99051b7p+36, 0x1.8ea0604a855f4p+33, -0x1.650e805ae7bbdp+32},
    {-0x1.840f53424fe74p+34, 0x1.0bdd617c266bp+37, 0x1.1e8381ebb942p+19},
    {0x1.cab293c3e134p+36, 0x1.566fed0116907p+37, 0x1.9af765981a926p+29},
    {0x1.bf8a50fb2bbd8p+38, 0x1.09a679ee1da6cp+39, -0x1.8737ce4b5c018p+33},
    {-0x1.b9bc6fbf15bbbp+39, 0x1.c9c8147d84afcp+39, 0x1.348b9c64fe403p+34},
    {-0x1.1a544d638f698p+39, 0x1.3fc3ac025f8b1p+41, 0x1.0c8946c0a10c9p+34},
    {0x1.ddddbd265929ep+41, 0x1.97fad46569454p+40, -0x1.e6c6434004e1fp+36},
};

static void testGROffIsOld() {
    NBodyState a = nbodyElementsStartState(), b = nbodyElementsStartState();   // the saved 0.10.0 numbers are from the elements start (0.10.3: the app's default start is now JPL's)
    for (int i = 0; i < 3652; i++) { nbodyStep(a, kNBodyStep); nbodyStep(b, kNBodyStep, false); }
    bool same = true;
    for (int i = 0; i < kNBodies; i++) same = same && a.x[i].x == b.x[i].x && a.x[i].y == b.x[i].y && a.x[i].z == b.x[i].z && a.v[i].x == b.v[i].x;
    double worst = 0;
    for (int i = 1; i < kNBodies; i++)
        worst = std::max(worst, length(a.x[i] - Vec3{kSaved010[i][0], kSaved010[i][1], kSaved010[i][2]}) / length(a.x[i] - a.x[0]));
    std::printf("GR off: after 3,652 steps the positions differ from the saved 0.10.0 ones by at most %.1e of the planets' distances (default argument equals an explicit 'off': %s)\n", worst, same ? "yes" : "NO");
    // 1e-9: the same compiled code gives 0; another compiler or CPU may round differently, and 3,652 steps can grow that to about 1e-12 at most.
    check("GR off: the default (no argument) is the same as an explicit off, bit for bit", same);
    check("GR off: positions equal the ones saved from 0.10.0 (to 1e-9 of the distances, for other compilers' rounding)", worst < 1e-9);
    NBodyState c = nbodyElementsStartState(); for (int i = 0; i < 3652; i++) nbodyStep(c, kNBodyStep, true);
    check("GR on really changes the motion (Mercury's place after 10 years is not the 'off' place)", length(c.x[1] - a.x[1]) > 1e3);
}

// ---- conserved quantities with GR on: energy including the GR potential energy, angular momentum, momentum, centre of mass ----
static void testGRConservation() {
    NBodyState s = nbodyStartState();
    const double h = kNBodyStep, e0 = nbodyEnergyGR(s);
    const Vec3 l0 = nbodyAngularMomentum(s), p0 = nbodyMomentum(s);
    double scaleP = 0; for (int i = 0; i < kNBodies; i++) scaleP += kRealGM[i] * length(s.v[i]);
    double maxE = 0, maxL = 0, maxP = 0, maxCom = 0;
    const long n = long(1000 * kYearS / h);
    for (long i = 1; i <= n; i++) {
        nbodyStep(s, h, true);
        if (i % 50 == 0) {
            maxE = std::max(maxE, std::fabs(nbodyEnergyGR(s) / e0 - 1));
            maxL = std::max(maxL, length(nbodyAngularMomentum(s) - l0) / length(l0));
            maxP = std::max(maxP, length(nbodyMomentum(s)) / scaleP);
            maxCom = std::max(maxCom, length(nbodyCentreOfMass(s)));
        }
    }
    std::printf("GR on, eight planets, 1,000 years: max |dE/E| = %.3e (E includes the GR potential), max |dL|/|L| = %.3e, max |P| / sum(GM |v|) = %.3e (start %.1e), max |centre of mass| = %.3e m\n",
                maxE, maxL, maxP, length(p0) / scaleP, maxCom);
    // The limits are the ones used without GR (REPORT 0.10.0): the GR kick is a force from a potential that depends only on the distance to the Sun, so it is
    // time-symmetric, central (no torque) and momentum-conserving, and the energy it adds to the error is far below the planets' own splitting error.
    check("GR on: energy (with the GR potential) bounded below 1e-8 over 1,000 years", maxE < 1e-8);
    check("GR on: angular momentum to rounding level (1e-12)", maxL < 1e-12);
    check("GR on: momentum stays at rounding level (1e-13 of sum GM |v|)", maxP < 1e-13);
    check("GR on: the centre of mass stays within 1 km of the origin over 1,000 years", maxCom < 1e3);
}

static void testGRReversal() {
    NBodyState s = nbodyStartState();
    const NBodyState start = s;
    const double h = kNBodyStep;
    const long n = long(100 * kYearS / h);
    for (long i = 0; i < n; i++) nbodyStep(s, h, true);
    for (long i = 0; i < n; i++) nbodyStep(s, -h, true);
    const double back = maxRelDiff(s, start);
    std::printf("GR on, time reversal: 100 years forward then back: the planets are back to within %.2e of their distances\n", back);
    check("GR on: time reversal still works (back at the start to within 1e-9, as without GR)", back < 1e-9);
}

// Eight planets: Mercury's perihelion advance with GR on minus the same run with GR off, which removes the planets' pull (about 526 arcseconds per century here).
static double mercuryRateArcsecPerCentury(bool gr, double years) {
    NBodyState s = nbodyStartState();
    const double h = kNBodyStep;
    const long n = long(years * kYearS / h);
    std::vector<double> ts, ws;
    double unwrap = 0, prev = 0;
    for (long i = 0; i <= n; i++) {
        if (i % 40 == 0) {
            const Vec3 r = s.x[1] - s.x[0], vv = s.v[1] - s.v[0];
            const double mu = kRealGM[0] + kRealGM[1];
            const Vec3 hv = cross(r, vv), ev = cross(vv, hv) * (1.0 / mu) - r * (1.0 / length(r));
            const double w = std::atan2(ev.y, ev.x);
            double d = w - prev; if (d < -units::kPi) d += 2 * units::kPi; else if (d > units::kPi) d -= 2 * units::kPi;
            unwrap = ts.empty() ? w : unwrap + d; prev = w;
            ts.push_back(i * h / kCenturyS); ws.push_back(unwrap);
        }
        if (i < n) nbodyStep(s, h, gr);
    }
    return lineSlope(ts, ws) * kArcsecPerRad;
}

static void testGRMercuryEightPlanets() {
    const double on = mercuryRateArcsecPerCentury(true, 500), off = mercuryRateArcsecPerCentury(false, 500);
    std::printf("Mercury in the eight-planet model, 500 years: perihelion advance %.2f arcseconds per century with GR, %.2f without; difference %.2f (published GR value 42.98, Will 2014)\n", on, off, on - off);
    // Limit 2 %: the same fit in both runs (so the planets' wobble cancels); what is left is the fit's own noise and a tiny cross effect of GR and the planets.
    check("GR: in the eight-planet model GR adds 42.98 arcseconds per century to Mercury's advance (to 2 %)", std::fabs((on - off) / 42.98 - 1) < 0.02);
}

// ================= The Sun's motion round the barycentre (0.10.1) =================
// The Sun is not fixed: the integrator keeps the total momentum zero, so the Sun moves against the planets. Checked with exact relations, not with our own earlier output.

// (1) Through the clock (what the app uses, with the Hermite interpolation): the centre of mass stays at the origin and the total momentum at zero.
static void testBarycentreStays() {
    for (int gr = 0; gr < 2; gr++) {
        NBodyClock clock(kNBodyStep, gr == 1);
        Vec3 x[kNBodies], v[kNBodies];
        double maxCom = 0, maxP = 0, scaleP = 0;
        for (int i = 0; i < kNBodies; i++) scaleP += kRealGM[i] * length(nbodyStartState().v[i]);
        int n = 0;
        for (double years = -226; years <= 75; years += 0.37, n++) {            // 1800 to 2100 (the app's date range), at odd times between the grid steps
            clock.stateAt(years * kYearS, x, v);
            NBodyState s; for (int i = 0; i < kNBodies; i++) { s.x[i] = x[i]; s.v[i] = v[i]; }
            maxCom = std::max(maxCom, length(nbodyCentreOfMass(s)));
            maxP = std::max(maxP, length(nbodyMomentum(s)) / scaleP);
        }
        std::printf("barycentre through the clock (GR %s), %d times in 1800-2100: max |centre of mass| = %.2e m, max |P| / sum(GM |v|) = %.2e\n", gr ? "on" : "off", n, maxCom, maxP);
        // Limit 1 m (the Sun's own motion is ~1e9 m): the sum of GM_i x_i is a constant of the motion and the Hermite interpolation keeps a constant exactly; what is left
        // is rounding (measured 1.2 mm after 1,000 years of stepping, REPORT 0.10.0). Momentum: rounding level, 1e-12.
        check(std::string("barycentre: the centre of mass stays within 1 m of the origin over 1800-2100 (GR ") + (gr ? "on" : "off") + ")", maxCom < 1.0);
        check(std::string("barycentre: the total momentum stays zero (below 1e-12 of sum GM |v|), GR ") + (gr ? "on" : "off"), maxP < 1e-12);
    }
}

// (2) The Sun and Jupiter alone: the Sun's distance from the centre of mass is GM_J / (GM_S + GM_J) times the distance between them, at every moment (exact for two bodies:
// the centre of mass lies on the line between them at that ratio). The GM values are JPL's DE440 ones (nbody.cpp).
static void testSunJupiterBarycentre() {
    const double gm[2] = {kRealGM[0], kRealGM[5]};
    const double a = 5.2 * units::kAU, e = 0.3, mu = gm[0] + gm[1], T = 2 * units::kPi * std::sqrt(a * a * a / mu);
    Vec3 rv; const Vec3 r0 = keplerRel(a, e, mu, 0, &rv);
    Vec3 x[2], v[2];
    x[0] = r0 * (-gm[1] / mu); x[1] = r0 * (gm[0] / mu); v[0] = rv * (-gm[1] / mu); v[1] = rv * (gm[0] / mu);
    const double ratio = gm[1] / mu, h = kNBodyStep;
    double worst = 0, maxSun = 0, minSun = 1e30;
    const long n = long(5 * T / h);
    for (long i = 0; i < n; i++) {
        nbodyStepN(2, gm, x, v, h);
        const double dSun = length(x[0]), dSep = length(x[1] - x[0]);                         // the centre of mass is at the origin
        worst = std::max(worst, std::fabs(dSun / (ratio * dSep) - 1));
        maxSun = std::max(maxSun, dSun); minSun = std::min(minSun, dSun);
    }
    std::printf("Sun + Jupiter (a = 5.2 AU, e = 0.3), 5 orbits: |Sun - centre of mass| / (GM_J/(GM_S+GM_J) x separation) differs from 1 by at most %.1e; the Sun's distance runs from %.3e to %.3e m (formula: ratio x a(1-+e) = %.3e to %.3e)\n",
                worst, minSun, maxSun, ratio * a * (1 - e), ratio * a * (1 + e));
    // Limit 1e-9: rounding of ~1e11 m positions (1e-16 of them, about 1e-5 m) against the Sun's 1e9 m distance is 1e-14; the integrator's own error does not enter the ratio.
    check("Sun + Jupiter: the Sun's distance from the barycentre is GM_J/(GM_S+GM_J) times the separation, at every step (to 1e-9)", worst < 1e-9);
    check("Sun + Jupiter: the Sun's nearest and farthest distance are ratio x a(1-e) and ratio x a(1+e) (to 1e-6, from sampling every day)",
          std::fabs(minSun / (ratio * a * (1 - e)) - 1) < 1e-6 && std::fabs(maxSun / (ratio * a * (1 + e)) - 1) < 1e-6);
}

// (3) The eight planets, 1800 to 2100: how far the Sun gets from the barycentre. A sanity bound, not a published value (none could be reached: see REPORT.md):
// since sum GM_i x_i = 0, x_sun = -sum_{i>=1} GM_i (x_i - x_sun) / sum GM, so |x_sun| <= sum GM_i |x_i - x_sun| / sum GM <= sum GM_i a_i (1 + e_i) / sum GM
// (if every planet stayed inside its orbit's far point, with the app's elements). The planets' orbits change a little in 300 years, so the bound gets 5 % extra.
static void testSunWobbleRange() {
    NBodyClock clock;
    Vec3 x[kNBodies], v[kNBodies];
    double total = 0, bound = 0, maxD = 0, minD = 1e30, maxAt = 0;
    for (int i = 0; i < kNBodies; i++) total += kRealGM[i];
    for (int i = 1; i < kNBodies; i++) bound += kRealGM[i] * kPlanets[i - 1].el[0] * units::kAU * (1 + kPlanets[i - 1].el[2]) / total;
    for (double years = -226; years <= 75; years += 0.02) {
        clock.stateAt(years * kYearS, x, v);
        const double d = length(x[0]);
        if (d > maxD) { maxD = d; maxAt = years; }
        minD = std::min(minD, d);
    }
    std::printf("the Sun's distance from the barycentre, 1800-2100 (Real, no GR): from %.2f to %.2f Sun radii (largest at %.0f years from 2026); upper bound from the planets' far points %.2f Sun radii (Sun radius %.0f km, units.h)\n",
                minD / units::kSunRadius, maxD / units::kSunRadius, maxAt, bound / units::kSunRadius, units::kSunRadius / 1e3);
    check("Sun's wobble: always below the upper bound from the planets' far points (+5 %)", maxD < 1.05 * bound);
    check("Sun's wobble: it does reach more than one Sun radius in 300 years (Jupiter alone gives about 1.07 at its mean distance)", maxD > 1.0 * units::kSunRadius);
}

int main() {
    testTwoBody();
    testEightPlanets();
    testReversal();
    testConvergence();
    testClock();
    testMercuryPerihelion();
    testGRFormula();
    testGRPublished();
    testGROffIsOld();
    testGRConservation();
    testGRReversal();
    testGRMercuryEightPlanets();
    testBarycentreStays();
    testSunJupiterBarycentre();
    testSunWobbleRange();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
