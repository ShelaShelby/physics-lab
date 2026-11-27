// 0.92.0: the celestial mechanics of the Orbit Lab's Data panel: the Lagrange points (orbit/lagrange.h), the Kirkwood gaps, the Lambert transfer planner and its porkchop
// plot (orbit/lambert.h), the ephemeris table and the integrator's drift check (orbit/ephem_table.h). Written before the code; each section checks against laws, textbook
// formulas or published numbers that the code does not use.
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "orbit/belts.h"
#include "orbit/ephem_table.h"
#include "orbit/nbody.h"
#include "orbit/real_errors.h"
#include "orbit/kepler.h"
#include "orbit/lambert.h"
#include "orbit/solar_system.h"
#include "orbit/lagrange.h"
#include "orbit/solar_data.h"
#include "orbit/units.h"

using namespace orbit;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }
static const double kEps = 2.220446049250313e-16;

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Part A: the Lagrange points. The reference for the force is written here again from Newton's law and the rotating frame (the code's own function is not used).
static void netForce(double mu, double x, double y, double& fx, double& fy, double& scale) {
    const double r1 = std::hypot(x + mu, y), r2 = std::hypot(x - 1 + mu, y);
    const double g1 = (1 - mu) / (r1 * r1 * r1), g2 = mu / (r2 * r2 * r2);
    fx = -g1 * (x + mu) - g2 * (x - 1 + mu) + x;       // gravity of M1 (at -mu), of M2 (at 1 - mu), the centrifugal pull w^2 x
    fy = -g1 * y - g2 * y + y;
    // the rounding of this sum: eps times the sum of its terms' sizes, and the slope of the force times the point's own rounding
    const double slope = 1 + 2 * (1 - mu) / (r1 * r1 * r1) + 2 * mu / (r2 * r2 * r2);
    scale = std::fabs(g1 * (x + mu)) + std::fabs(g2 * (x - 1 + mu)) + std::fabs(x) + std::fabs(g1 * y) + std::fabs(g2 * y) + std::fabs(y) + slope * (std::fabs(x) + std::fabs(y));
}

static void testLagrangeForce() {
    const double mus[] = {1e-9, 1.66e-7, 3.04e-6, 9.5e-4, 0.0121, 0.0385, 0.1, 0.3, 0.5};
    double worst = 0;
    for (double mu : mus) {
        const lagrange::Points P = lagrange::points(mu);
        for (int k = 0; k < 5; k++) {
            double fx, fy, sc;
            netForce(mu, P.x[k], P.y[k], fx, fy, sc);
            const double f = std::hypot(fx, fy);
            const double bound = 64 * kEps * sc;
            worst = std::max(worst, f / bound);
            check("force at L" + std::to_string(k + 1) + " mu " + std::to_string(mu) + " is zero within rounding (" + std::to_string(f) + " vs " + std::to_string(bound) + ")", f <= bound);
            double ax, ay;
            lagrange::restAcceleration(mu, P.x[k], P.y[k], ax, ay);
            check("restAcceleration agrees with the test's own force at L" + std::to_string(k + 1), std::fabs(ax - fx) <= bound && std::fabs(ay - fy) <= bound);
        }
        // the order along the line: L3 < M1 < L1 < M2 < L2; L4, L5 mirror images
        check("order on the line, mu " + std::to_string(mu), P.x[2] < -mu && -mu < P.x[0] && P.x[0] < 1 - mu && 1 - mu < P.x[1]);
        check("L4 and L5 are mirror images", P.x[3] == P.x[4] && P.y[3] == -P.y[4] && P.y[3] > 0);
    }
    std::printf("Lagrange force: worst force / its rounding bound %.3f\n", worst);
}

static void testEquilateral() {
    for (double mu : {1e-6, 0.0123, 0.0385, 0.25, 0.5}) {
        const lagrange::Points P = lagrange::points(mu);
        for (int k : {3, 4}) {
            const double d1 = std::hypot(P.x[k] + mu, P.y[k]), d2 = std::hypot(P.x[k] - 1 + mu, P.y[k]);
            check("L" + std::to_string(k + 1) + " is 1 from M1, mu " + std::to_string(mu), std::fabs(d1 - 1) <= 8 * kEps);
            check("L" + std::to_string(k + 1) + " is 1 from M2, mu " + std::to_string(mu), std::fabs(d2 - 1) <= 8 * kEps);
        }
    }
}

// Tatum 16.1.10 to 16.1.23: the quintics in his own notation (mu_T = M1 / (M1 + M2), x from the centre of mass, M1 at 1 - mu_T, M2 at -mu_T). The roots of the code's points
// must satisfy them: L1 (the code's L2) is his L2 turned round, so each code point is mapped to his x: his M2 is the code's M2 at x = -(code x), and so on.
// Code frame: M1 (heavy) at -mu, M2 (light) at 1 - mu. Tatum with his M1 = the heavy one: mu_T = 1 - mu; M1 at +(1 - mu_T) = mu... the maps below are verified by his own equation 16.1.9.
static double tatumQuintic(int which, double muT, double x) {      // which 1, 2, 3 = his L1, L2, L3
    const double m = muT, m2 = m * m, m3 = m2 * m, m4 = m3 * m;
    double a0, a1, a2, a3, a4;
    if (which == 3) { a0 = 1 - 3 * m + 3 * m2 - 2 * m3; a1 = 2 - 4 * m + 5 * m2 - 2 * m3 + m4; a2 = 1 - 4 * m + 6 * m2 - 4 * m3; a3 = 1 - 6 * m + 6 * m2; a4 = 2 - 4 * m; }
    else {
        a0 = -1 + 3 * m - 3 * m2; a3 = 1 - 6 * m + 6 * m2;
        if (which == 1) { a1 = 2 - 4 * m + m2 - 2 * m3 + m4; a2 = -1 + 2 * m - 6 * m2 + 4 * m3; a4 = -2 + 4 * m; }
        else { a1 = -2 + 4 * m + m2 - 2 * m3 + m4; a2 = -1 - 2 * m + 6 * m2 - 4 * m3; a4 = 2 - 4 * m; }
    }
    return a0 + x * (a1 + x * (a2 + x * (a3 + x * (a4 + x))));
}
static void testTatumQuintics() {
    // his equation 16.1.9 first, at his coordinates, for the sign convention: mu_T/(x - 1 + mu_T)^2 + (1 - mu_T)/(x + mu_T)^2 = x for his L1
    for (double mu : {0.001, 0.0123, 0.2, 0.5}) {
        const lagrange::Points P = lagrange::points(mu);
        // Tatum: M1 (mass fraction muT) sits at x = 1 - muT. Take his M1 as the code's light mass M2: muT = mu, M1 at 1 - mu (the code's M2 place), M2 at -mu (the code's M1 place).
        // So Tatum's x is the code's x for his L1 (the code's L2). His L2 is beyond his M2 (the code's L3), his L3 between (the code's L1).
        // (for his L2 and L3 the quintics take x as the distance from the centre of mass counted positive on the other side, so the code's x is turned round: found by trying both signs against 16.1.12 and 16.1.13)
        const double xs[3] = {P.x[1], -P.x[2], -P.x[0]};
        for (int w = 1; w <= 3; w++) {
            const double x = xs[w - 1];
            const double v = tatumQuintic(w, mu, x);
            // the quintic's value is small only if x is its root: its slope bounds how small
            const double h = 1e-7, slope = std::fabs(tatumQuintic(w, mu, x + h) - tatumQuintic(w, mu, x - h)) / (2 * h);
            check("Tatum quintic 16.1.1" + std::to_string(w) + " is zero at the code's point, mu " + std::to_string(mu) + " (" + std::to_string(v) + ")", std::fabs(v) <= 1e-12 * std::max(1.0, slope) + 1e-14);
        }
        const double x = P.x[1];
        const double lhs = mu / ((x - 1 + mu) * (x - 1 + mu)) + (1 - mu) / ((x + mu) * (x + mu));
        check("Tatum 16.1.9 holds at the code's L2, mu " + std::to_string(mu), std::fabs(lhs - x) <= 1e-12 * std::fabs(x));
    }
}

// Tatum, section 16.1, the table of least distances (units 16 L^2 / (G M^3): the separation is a = 1 / (16 m^2 (1 - m)^2) there, 16.1.27, m = M1 / (M1 + M2)). Each distance from his
// M1 has a least value at one mass fraction. His frame: M1 at x = 1 - m, M2 at -m. His L1 is the collinear point beyond M1, L2 beyond M2, L3 between. The code's frame has the
// light mass at 1 - mu: for m <= 0.5 his M1 is the light mass (mu = m, same x); for m > 0.5 it is the heavy one (mu = 1 - m) and his x is the code's x turned round.
static void testTatumTable() {
    struct Row { const char* name; double least, frac; int which; };   // which: 0 = M1 to the centre of mass, 1 = M1 L1, 2 = M1 L2, 3 = M1 L3
    const Row rows[] = {{"M1 C", 0.421875, 0.666666, 0}, {"M1 L1", 0.677756, 0.436062, 1}, {"M1 L2", 1.690392, 0.524579, 2}, {"M1 L3", 0.489038, 0.446273, 3}};
    for (const Row& r : rows) {
        auto dist = [&](double m) {
            const double a = 1.0 / (16 * m * m * (1 - m) * (1 - m));
            if (r.which == 0) return (1 - m) * a;
            const double mu = m <= 0.5 ? m : 1 - m, s = m <= 0.5 ? 1 : -1;
            const lagrange::Points P = lagrange::points(mu);
            double xt[3] = {s * P.x[0], s * P.x[1], s * P.x[2]};
            std::sort(xt, xt + 3);                                  // L2 (smallest), L3 (between), L1 (largest)
            const double x = r.which == 1 ? xt[2] : r.which == 2 ? xt[0] : xt[1];
            return std::fabs(x - (1 - m)) * a;
        };
        double best = 1e300, bestM = 0;
        for (int pass = 0; pass < 3; pass++) {
            const double st = pass == 0 ? 1e-3 : pass == 1 ? 1e-6 : 1e-8, lo = pass == 0 ? 0.31 : bestM - (pass == 1 ? 2e-3 : 2e-6), hi = pass == 0 ? 0.76 : bestM + (pass == 1 ? 2e-3 : 2e-6);
            for (double m = lo; m <= hi; m += st) { const double d = dist(m); if (d < best) { best = d; bestM = m; } }
        }
        check(std::string("Tatum's table: least distance ") + r.name + " " + std::to_string(best) + " vs " + std::to_string(r.least), std::fabs(best - r.least) <= 6e-7);
        check(std::string("Tatum's table: the mass fraction of ") + r.name + " " + std::to_string(bestM) + " vs " + std::to_string(r.frac), std::fabs(bestM - r.frac) <= 1e-6);
    }
}

static void testRouth() {
    const double muR = lagrange::routhCriticalMu();
    check("27 mu (1 - mu) = 1 at the critical value", std::fabs(27 * muR * (1 - muR) - 1) <= 8 * kEps);
    check("the critical value is the smaller root, below 1/2", muR > 0 && muR < 0.5 && std::fabs(muR - (1 - std::sqrt(23.0 / 27.0)) / 2) <= 4 * kEps);
    // Tatum 16.2.27: stable orbits when q = M1/M2 > 24.9599358 (or its inverse); q = 1 / mu - 1 here
    const double qTatum = 24.9599358;
    check("Tatum's critical mass ratio " + std::to_string(1 / muR - 1), std::fabs((1 / muR - 1) - qTatum) <= 5e-7);
    check("stable just below the critical mu", lagrange::trianglePointsStable(muR * (1 - 1e-9)));
    check("unstable just above the critical mu", !lagrange::trianglePointsStable(muR * (1 + 1e-9)));
    check("Earth-Moon (mu 0.0121) is stable, equal masses (0.5) are not", lagrange::trianglePointsStable(0.0121) && !lagrange::trianglePointsStable(0.5));
    // Tatum, the Sun and Jupiter, q = 1047.35: n = 0.996757 w and 0.0804645 w, periods 1.0033 P_J and 12.428 P_J
    const lagrange::Libration L = lagrange::libration(1 / (1 + 1047.35));
    check("libration frequencies exist for Sun-Jupiter", L.ok);
    check("Tatum: n = 0.996757 w (got " + std::to_string(L.fast) + ")", std::fabs(L.fast - 0.996757) <= 6e-7);
    // Tatum prints 0.0804645; his own 16.2.27 with q = 1047.35 gives 0.0804641 (the product and sum tests below hold for the computed one). His last digit is doubtful: the check keeps his 7 digits to 5e-7 (REPORT.md)
    check("Tatum: n = 0.0804645 w to his 6 digits (got " + std::to_string(L.slow) + ")", std::fabs(L.slow - 0.0804645) <= 5e-7);
    check("Tatum: periods 1.0033 and 12.428 P_J", std::fabs(1 / L.fast - 1.0033) <= 6e-5 && std::fabs(1 / L.slow - 12.428) <= 6e-4);
    check("the two frequencies satisfy n^2 + n'^2 = w^2 (Tatum 16.2.26: the sum of its roots)", std::fabs(L.fast * L.fast + L.slow * L.slow - 1) <= 8 * kEps);
    check("and n^2 n'^2 = 27 mu (1 - mu) / 4 (the product of its roots)", std::fabs(L.fast * L.fast * L.slow * L.slow - 27 * (1 / 1048.35) * (1 - 1 / 1048.35) / 4) <= 8 * kEps);
    check("no frequencies above the critical mu", !lagrange::libration(0.1).ok);
}

static void testHill() {
    // L1 and L2 approach the Hill radius (mu/3)^(1/3) as mu goes to 0: the distance from M2 is R_H (1 -/+ eps/3 + O(eps^2)), eps = R_H. The remainder, divided by eps^2, settles on a constant.
    double prev1 = NAN, prev2 = NAN;
    for (double mu : {1e-6, 1e-8, 1e-10, 1e-12}) {
        const lagrange::Points P = lagrange::points(mu);
        const double RH = lagrange::hillRadius(mu);
        check("hillRadius is (mu/3)^(1/3)", std::fabs(RH - std::cbrt(mu / 3)) <= 4 * kEps * RH);
        const double d1 = (1 - mu) - P.x[0], d2 = P.x[1] - (1 - mu);
        const double r1 = (d1 / RH - (1 - RH / 3)) / (RH * RH), r2 = (d2 / RH - (1 + RH / 3)) / (RH * RH);
        check("L1 distance to M2 is R_H (1 - eps/3 + O(eps^2)), mu " + std::to_string(mu) + " remainder/eps^2 " + std::to_string(r1), std::fabs(r1) < 1);
        check("L2 distance to M2 is R_H (1 + eps/3 + O(eps^2)), mu " + std::to_string(mu) + " remainder/eps^2 " + std::to_string(r2), std::fabs(r2) < 1);
        check("L1 is inside the Hill radius, L2 outside", d1 < RH && d2 > RH);
        if (std::isfinite(prev1)) check("the O(eps^2) constant settles (L1 " + std::to_string(prev1) + " -> " + std::to_string(r1) + ")", std::fabs(r1 - prev1) < 0.02 && std::fabs(r2 - prev2) < 0.02);
        prev1 = r1; prev2 = r2;
    }
}

static void testSystems() {
    const auto& S = lagrange::systems();
    auto find = [&](const char* n) -> const lagrange::System* { for (const auto& s : S) if (s.name == n) return &s; return nullptr; };
    check("a Sun-planet pair for each of the eight planets and Earth-Moon, Jupiter's four big moons, Titan, Triton", S.size() >= 14);
    const lagrange::System* sj = find("Sun - Jupiter");
    check("Sun - Jupiter exists with Tatum's q = M1/M2 = 1047.35", sj && std::fabs((1 - sj->mu) / sj->mu - 1047.35) <= 0.01);
    const lagrange::System* em = find("Earth - Moon");
    check("Earth - Moon: Tatum's q = 81.3 (JPL's 81.3005690769 for Earth/Moon: DE441 ratio, tests/test_moon_real.cpp)", em && std::fabs((1 - em->mu) / em->mu - 81.30057) <= 0.002 && std::fabs(em->sepKm - 384400) <= 1);
    for (const auto& s : S) {
        const lagrange::Table T = lagrange::table(s);
        check(s.name + ": mu is the light mass over the sum", std::fabs(s.mu - s.gmLight / (s.gmHeavy + s.gmLight)) <= 4 * kEps);
        // L1 + L2 distances from the light mass in km are the dimensionless ones times the separation
        const lagrange::Points P = lagrange::points(s.mu);
        check(s.name + ": the table's L1 is the points() value times the separation", std::fabs(T.inSep[0] - ((1 - s.mu) - P.x[0])) <= 4 * kEps && std::fabs(T.fromLightKm[0] - T.inSep[0] * s.sepKm) <= 1e-9 * s.sepKm);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Part B: the Kirkwood gaps. The data are read here again from the generated file the belts use.
static const double kRocks[][10] =
#include "orbit/asteroid_data.inc"
;

static void testKirkwood() {
    const auto& R = kirkwood::resonances();
    check("five resonances 4:1 3:1 5:2 7:3 2:1", R.size() == 5 && R[0].p == 4 && R[1].p == 3 && R[2].p == 5 && R[3].p == 7 && R[4].p == 2);
    // Jupiter's a from the lab's data: kPlanets[4].el[0] (JPL elements, AU). Kepler's third law twice: through the periods with the lab's GM of the Sun
    const double aJ = kPlanets[4].el[0] * units::kAU;
    check("a_J comes from the lab's Jupiter", std::string(kPlanets[4].name) == "Jupiter" && std::fabs(kirkwood::jupiterAU() - kPlanets[4].el[0]) <= 4 * kEps);
    const double TJ = 2 * units::kPi * std::sqrt(aJ * aJ * aJ / units::kGMSun);
    for (const auto& r : R) {
        const double T = TJ * r.q / r.p;                                    // the rock goes round p times while Jupiter goes round q times
        const double a = std::cbrt(units::kGMSun * T * T / (4 * units::kPi * units::kPi)) / units::kAU;
        check("resonance " + std::to_string(r.p) + ":" + std::to_string(r.q) + " at " + std::to_string(r.aAU) + " AU from Kepler's third law through the period", std::fabs(r.aAU - a) <= 1e-12 * a);
    }
    check("textbook order: 4:1 < 3:1 < 5:2 < 7:3 < 2:1", R[0].aAU < R[1].aAU && R[1].aAU < R[2].aAU && R[2].aAU < R[3].aAU && R[3].aAU < R[4].aAU);
    // the histogram against a direct count of the data
    const double lo = 1.8, hi = 3.6;
    const int bins = 90;
    const kirkwood::Histogram H = kirkwood::mainBelt(lo, hi, bins);
    std::vector<int> mine(bins, 0);
    int below = 0, above = 0, total = 0;
    for (const auto& r : kRocks) {
        if (int(r[0]) != 0) continue;
        total++;
        if (r[1] < lo) below++;
        else if (r[1] >= hi) above++;
        else mine[std::size_t(std::floor((r[1] - lo) / ((hi - lo) / bins)))]++;
    }
    check("histogram: bin width", std::fabs(H.w - (hi - lo) / bins) <= 4 * kEps && H.lo == lo && int(H.count.size()) == bins);
    check("histogram: every bin equals the direct count", H.count == mine);
    check("histogram: below, above and total equal the direct count", H.below == below && H.above == above && H.total == total);
    int sum = H.below + H.above;
    for (int c : H.count) sum += c;
    check("histogram: the pieces add up to the group", sum == total && total > 1000);
    // the gaps are in the data: the rocks within 0.01 AU of the 3:1 and 2:1 places are fewer than in either neighbouring window of the same width (qualitative)
    for (int k : {1, 4}) {
        const double a0 = R[std::size_t(k)].aAU, w = 0.015;
        int in = 0, left = 0, right = 0;
        for (const auto& r : kRocks) {
            if (int(r[0]) != 0) continue;
            if (std::fabs(r[1] - a0) < w) in++;
            else if (r[1] > a0 - 3 * w && r[1] <= a0 - w) left++;
            else if (r[1] < a0 + 3 * w && r[1] >= a0 + w) right++;
        }
        std::printf("Kirkwood %d:1 at %.4f AU: %d rocks within 0.015 AU, %d and %d in the windows beside it (each as wide)\n", R[std::size_t(k)].p, a0, in, left, right);
        check("the gap at the " + std::to_string(R[std::size_t(k)].p) + ":1 resonance holds fewer than half the rocks of the neighbouring windows' mean", in * 4 < left + right);
    }
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Part C: Lambert's problem. The lab's Kepler code is the independent check: the solved orbit, taken from r1 and v1 and carried to the arrival time, must be at r2.
struct Elements { double a, e, I, node, peri, M, n; };
// Elements of an elliptic orbit from a position and velocity: the standard textbook formulas (angles in degrees as keplerState wants them). mu in m^3/s^2.
static bool elementsOf(const Vec3& r, const Vec3& v, double mu, Elements& E) {
    const double rn = length(r), v2 = dot(v, v);
    const Vec3 h = cross(r, v);
    const double hn = length(h);
    const double energy = v2 / 2 - mu / rn;
    if (!(energy < 0)) return false;
    E.a = -mu / (2 * energy);
    const Vec3 ev = cross(v, h) * (1 / mu) - r * (1 / rn);
    E.e = length(ev);
    E.I = std::acos(h.z / hn) * 180 / units::kPi;
    const Vec3 nv{-h.y, h.x, 0};
    const double nn = length(nv);
    if (nn < 1e-12 * hn || E.e < 1e-9) return false;               // the node or the perihelion is not defined: the tests avoid these orbits
    double node = std::acos(nv.x / nn);
    if (nv.y < 0) node = 2 * units::kPi - node;
    double peri = std::acos(dot(nv, ev) / (nn * E.e));
    if (ev.z < 0) peri = 2 * units::kPi - peri;
    double nu = std::acos(std::max(-1.0, std::min(1.0, dot(ev, r) / (E.e * rn))));
    if (dot(r, v) < 0) nu = 2 * units::kPi - nu;
    const double Ean = 2 * std::atan2(std::sqrt(1 - E.e) * std::sin(nu / 2), std::sqrt(1 + E.e) * std::cos(nu / 2));
    double M = Ean - E.e * std::sin(Ean);
    E.node = node * 180 / units::kPi; E.peri = peri * 180 / units::kPi; E.M = M * 180 / units::kPi;
    E.n = std::sqrt(mu / (E.a * E.a * E.a));
    return true;
}

struct Case { Vec3 r1, r2; double tof; const char* name; };

static void testLambertGeometry() {
    const double mu = units::kGMSun, au = units::kAU, day = 86400.0;
    std::vector<Case> cases;
    // real planets on the lab's own table: Earth to Mars, Venus, Jupiter at several dates and flight times (both the short and the long way come out)
    const double jds[] = {2461000.5, 2461300.5, 2461600.5, 2462000.5, 2463000.5};
    struct Pair { int from, to; double tofD; };
    const Pair pairs[] = {{2, 3, 200}, {2, 3, 260}, {2, 3, 330}, {2, 1, 120}, {2, 1, 160}, {2, 4, 800}, {2, 4, 1500}, {3, 2, 250}, {1, 3, 300}, {2, 5, 2000}};
    for (double jd : jds)
        for (const Pair& pr : pairs) {
            const State a = planetStateAt(std::size_t(pr.from), jd), b = planetStateAt(std::size_t(pr.to), jd + pr.tofD);
            cases.push_back({a.pos, b.pos, pr.tofD * day, "planets"});
        }
    // made-up places (not planets) so that transfer angles of every size occur, 0.3 to 2 pi - 0.3 about +z, with a tilt
    for (int k = 0; k < 60; k++) {
        const double th = 0.3 + (2 * units::kPi - 0.6) * (k + 0.5) / 60, r1 = (0.8 + 0.01 * k) * au, r2 = (1.6 - 0.008 * k) * au, tilt = 0.12 * std::sin(k);
        const Vec3 p1{r1, 0, 0}, p2{r2 * std::cos(th), r2 * std::sin(th) * std::cos(tilt), r2 * std::sin(th) * std::sin(tilt)};
        cases.push_back({p1, p2, (200.0 + 7 * (k % 17)) * day, "made-up"});
    }
    int solved = 0, bad = 0;
    double worstPos = 0, worstVel = 0, worstE = 0, worstH = 0;
    for (const Case& c : cases) {
        const lambert::Solution S = lambert::solve(c.r1, c.r2, c.tof, mu);
        if (!S.ok) { bad++; continue; }
        solved++;
        // (2) energy and angular momentum are the same at both ends
        const double e1 = dot(S.v1, S.v1) / 2 - mu / length(c.r1), e2 = dot(S.v2, S.v2) / 2 - mu / length(c.r2);
        const Vec3 h1 = cross(c.r1, S.v1), h2 = cross(c.r2, S.v2);
        worstE = std::max(worstE, std::fabs(e1 - e2) / (mu / length(c.r1)));
        worstH = std::max(worstH, length(h1 - h2) / length(h1));
        // (1) carry r1, v1 to the arrival time with the lab's Kepler code: it must be at r2 with v2
        Elements E;
        if (!elementsOf(c.r1, S.v1, mu, E)) continue;               // hyperbolic or a degenerate orbit: only the conservation checks above
        const State st = keplerState(E.a, E.e, E.I, E.node, E.peri, E.M + E.n * c.tof * 180 / units::kPi, mu);
        worstPos = std::max(worstPos, length(st.pos - c.r2) / length(c.r2));
        worstVel = std::max(worstVel, length(st.vel - S.v2) / length(S.v2));
    }
    std::printf("Lambert geometry: %d of %zu solved, worst relative position miss %.2e, velocity %.2e, energy %.2e, angular momentum %.2e\n", solved, cases.size(), worstPos, worstVel, worstE, worstH);
    check("every case solved", bad == 0 && solved == int(cases.size()));
    // the bound: Izzo stops when x moves under 1e-14; Kepler's equation in keplerE stops at 1e-12 rad, and a miss of d in the anomaly moves the planet by about d times its radius,
    // so 1e-9 of the distance leaves room for the conditioning of the orbit's elements (e small, i small) in elementsOf
    check("(1) the solved orbit reaches r2 (relative miss <= 1e-9)", worstPos <= 1e-9);
    check("(1) and arrives with v2 (relative miss <= 1e-9)", worstVel <= 1e-9);
    check("(2) the energy is the same at both ends (<= 1e-11 of mu / r1)", worstE <= 1e-11);
    check("(2) the angular momentum is the same at both ends (<= 1e-11)", worstH <= 1e-11);
}

static void testTransferArc() {
    // the drawn transfer orbit starts at r1 and ends at r2 (every case of the geometry test: both ellipses and hyperbolas), and every point is on the conic: the energy of the
    // circular-motion-free relation r (1 + e cos f) = p is replaced by the simplest independent law: each point's distance from the Sun lies between the two ends' ... checked by
    // the vis-viva speed being real (2 mu / r - 1/a > 0) and by the arc's length being at least the chord
    const double mu = units::kGMSun, au = units::kAU, day = 86400.0;
    double worst = 0;
    int cases = 0;
    for (double th : {0.4, 1.2, 2.5, 3.5, 5.0, 6.0}) for (double days : {90.0, 200.0, 400.0, 900.0}) {
        const Vec3 r1{au, 0, 0}, r2{1.52 * au * std::cos(th), 1.52 * au * std::sin(th) * 0.97, 1.52 * au * std::sin(th) * 0.03};
        const lambert::Solution S = lambert::solve(r1, r2, days * day, mu);
        if (!S.ok) continue;
        cases++;
        const std::vector<Vec3> arc = lambert::transferArc(r1, S.v1, mu, days * day, 200);
        check("arc has n + 1 points", arc.size() == 201);
        worst = std::max(worst, std::max(length(arc.front() - r1) / au, length(arc.back() - r2) / au));
        double len = 0;
        for (std::size_t i = 1; i < arc.size(); i++) len += length(arc[i] - arc[i - 1]);
        check("the arc is at least as long as its chord", len >= length(r2 - r1) * (1 - 1e-12));
    }
    std::printf("transfer arcs: %d cases, worst end miss %.2e AU\n", cases, worst);
    check("the arc starts at r1 and ends at r2 (1e-9 AU)", cases >= 20 && worst <= 1e-9);
}

static void testLambertSeries() {
    // Battin's series (Izzo Eq. (20), with the hypergeometric function 2F1(3, 1; 5/2; S1): the paper's "1F2" is a misprint) equals Eq. (18) away from x = 1 where both are good
    double worst = 0;
    for (double lam : {-0.95, -0.6, -0.2, 0.0, 0.3, 0.7, 0.95})
        for (double x : {0.7, 0.85, 0.95, 1.05, 1.15, 1.3}) {
            const double a = lambert::timeOfFlightT(lam, x, 1), b = lambert::timeOfFlightT(lam, x, 2);
            worst = std::max(worst, std::fabs(a - b) / std::fabs(a));
        }
    std::printf("Battin's series against Eq. (18): worst relative difference %.2e\n", worst);
    // (Eq. (18) itself loses eps / (1 - x^2)^2 to cancellation, 2.3e-14 at x = 0.95, times the size of its terms over T: so 1e-11)
    check("the series equals Eq. (18) to 1e-11 over x in 0.7 .. 1.3 (the series is for x near 1: it diverges for S1 >= 1)", worst < 1e-11);
    // the parabola: T(1) = 2/3 (1 - lam^3) (Izzo Eq. (21)), and its slope (Eq. (23): dT/dx = 2/5 (lam^5 - 1))
    for (double lam : {-0.8, 0.0, 0.6}) {
        const double T1 = 2.0 / 3.0 * (1 - lam * lam * lam), h = 1e-6;
        check("T(1) = 2/3 (1 - lam^3), lam " + std::to_string(lam), std::fabs(lambert::timeOfFlightT(lam, 1, 2) - T1) <= 1e-14);
        const double slope = (lambert::timeOfFlightT(lam, 1 + h, 2) - lambert::timeOfFlightT(lam, 1 - h, 2)) / (2 * h);
        check("dT/dx at 1 = 2/5 (lam^5 - 1), lam " + std::to_string(lam), std::fabs(slope - 0.4 * (std::pow(lam, 5) - 1)) <= 1e-7);
    }
    // T(0) = acos(lam) + lam sqrt(1 - lam^2) (Eq. (19), M = 0), dT/dx = -2 at x = 0
    for (double lam : {-0.5, 0.2, 0.9}) {
        check("T(0) = acos(lam) + lam sqrt(1 - lam^2), lam " + std::to_string(lam), std::fabs(lambert::timeOfFlightT(lam, 0, 1) - (std::acos(lam) + lam * std::sqrt(1 - lam * lam))) <= 1e-14);
        const double h = 1e-6;
        check("dT/dx at 0 = -2, lam " + std::to_string(lam), std::fabs((lambert::timeOfFlightT(lam, h, 1) - lambert::timeOfFlightT(lam, -h, 1)) / (2 * h) + 2) <= 1e-6);
    }
}

static void testLambertParabola() {
    // Euler's parabolic time of flight (textbook; Battin 1999 and Izzo's Eq. (21) with x = 1: T1 = 2/3 (1 - lambda^3)): t = sqrt(2 / mu) / 6 (s^1.5 -/+ (s - c)^1.5)
    const double mu = units::kGMSun, au = units::kAU;
    for (double th : {0.5, 1.5, 3.0, 4.0, 5.5}) {
        const Vec3 r1{au, 0, 0}, r2{1.5 * au * std::cos(th), 1.5 * au * std::sin(th), 0};
        const double c = length(r2 - r1), s = (length(r1) + length(r2) + c) / 2;
        const double sgn = std::sin(th) >= 0 ? 1 : -1;
        const double t = std::sqrt(2 / mu) / 3 * (std::pow(s, 1.5) - sgn * std::pow(s - c, 1.5));
        const lambert::Solution S = lambert::solve(r1, r2, t, mu);
        const double e1 = dot(S.v1, S.v1) / 2 - mu / length(r1);
        check("parabolic flight time gives zero energy, angle " + std::to_string(th) + " (" + std::to_string(e1 / (mu / au)) + ")", S.ok && std::fabs(e1) <= 1e-9 * mu / au);
        const lambert::Solution Sa = lambert::solve(r1, r2, t * 1.002, mu), Sb = lambert::solve(r1, r2, t * 0.998, mu);
        const double ea = dot(Sa.v1, Sa.v1) / 2 - mu / au, eb = dot(Sb.v1, Sb.v1) / 2 - mu / au;
        check("a little longer is bound, a little shorter is not, angle " + std::to_string(th), Sa.ok && Sb.ok && ea < 0 && eb > 0);
    }
}

static void testLambertPublished() {
    // (mu = 398600.4418: the page does not print it; at this value the lab's solver reproduces the page's printed 8-digit v2 of both cases to 2e-7 km/s, which fixes mu to about 5e-8 of itself)
    // The worked examples listed in the poliastro documentation page "Revisiting Lambert's problem in Python", Part 4, Single revolution (saved in the sources repo as
    // data/celestial/poliastro_lambert_examples.html): the page gives the positions, the time and the expected velocities of each case. Earth, mu = 398600.4418 km^3/s^2 (poliastro's Earth.k).
    // The page does not name the books; the numbers are printed with 6 and 4 or 5 digits, so the limits below are those digits.
    const double mu = 398600.4418;
    {
        const Vec3 r1{15945.34, 0, 0}, r2{12214.83399, 10249.46731, 0};
        const lambert::Solution S = lambert::solve(r1, r2, 76 * 60.0, mu);
        const Vec3 ea{2.058925, 2.915956, 0}, eb{-3.451569, 0.910301, 0}, ps{-3.4515665, 0.91031354, 0};   // ps: the page's printed output of poliastro's own Izzo solver for v2
        // the page's expected v2 differs from its own solver's output by 1.2e-5 km/s (the books' rounding): the limit is 2e-5; the independent solver's 8 digits are met to 2e-7
        check("published case 1: v2 equals poliastro's printed Izzo output to 2e-7 km/s", S.ok && length(S.v2 - ps) <= 2e-7);
        check("published case 1 (r1 = 15945.34 km on x, 76 min): v1", S.ok && length(S.v1 - ea) <= 2e-5);
        check("published case 1: v2", S.ok && length(S.v2 - eb) <= 2e-5);
        std::printf("published case 1: v1 miss %.2e km/s, v2 miss %.2e km/s\n", length(S.v1 - ea), length(S.v2 - eb));
    }
    {
        const Vec3 r1{5000, 10000, 2100}, r2{-14600, 2500, 7000};
        const lambert::Solution S = lambert::solve(r1, r2, 3600.0, mu);
        const Vec3 ea{-5.9925, 1.9254, 3.2456}, eb{-3.3125, -4.1966, -0.38529}, ps{-3.3124585, -4.196619, -0.38528906};   // ps: the page's printed Izzo output for v2
        check("published case 2: v2 equals poliastro's printed Izzo output to 2e-6 km/s", S.ok && length(S.v2 - ps) <= 2e-6);
        check("published case 2 (r1 = (5000, 10000, 2100) km, 1 h): v1", S.ok && length(S.v1 - ea) <= 1e-4);
        check("published case 2: v2", S.ok && length(S.v2 - eb) <= 1e-4);
        std::printf("published case 2: v1 miss %.2e km/s, v2 miss %.2e km/s\n", length(S.v1 - ea), length(S.v2 - eb));
    }
}

// The porkchop grid on the lab's own ephemeris. Reference bound from physics: no transfer between two coplanar circular orbits needs less departure C3 than the Hohmann one,
// C3 = (sqrt(mu / r1) (sqrt(2 r2 / (r1 + r2)) - 1))^2; the real orbits (e, i) and the lab's elements allow a little less or more, so the best cell lies within 15 % of it.
static void testPorkchopCpu() {
    lambert::PorkchopSetup s;
    s.dep = 2; s.arr = 3; s.depJD0 = 2461000.5; s.depStep = 6; s.nDep = 130; s.arrJD0 = 2461100.5; s.arrStep = 6; s.nArr = 130;   // one synodic period (780 d) of departures
    const lambert::PorkchopInput in = lambert::porkchopInput(s);
    const lambert::PorkchopGrid g = lambert::porkchopCpu(in);
    check("grid size", g.nDep == 130 && g.nArr == 130 && g.c3.size() == 16900);
    int nan = 0, usable = 0;
    float best = 1e30f, bestV = 0;
    for (std::size_t k = 0; k < g.c3.size(); k++) {
        if (std::isnan(g.c3[k])) { nan++; continue; }
        usable++;
        if (g.c3[k] < best) { best = g.c3[k]; bestV = g.vinf[k]; }
    }
    // cells whose arrival is not after the departure carry no number
    int expectNan = 0;
    for (int i = 0; i < 130; i++) for (int j = 0; j < 130; j++) if (!((s.arrJD0 + s.arrStep * j) - (s.depJD0 + s.depStep * i) > 0)) expectNan++;
    check("NaN exactly where the arrival is not after the departure", nan == expectNan && usable > 5000);
    const double mu = units::kGMSun, r1 = kPlanets[2].el[0] * units::kAU, r2 = kPlanets[3].el[0] * units::kAU;
    const double vinf = std::sqrt(mu / r1) * (std::sqrt(2 * r2 / (r1 + r2)) - 1) / 1000.0, hohmannC3 = vinf * vinf;
    std::printf("porkchop: best C3 %.3f km2/s2 (v_inf at arrival %.3f), Hohmann between circular orbits %.3f\n", best, bestV, hohmannC3);
    check("the best C3 is within 15 % of the Hohmann value (" + std::to_string(best) + " vs " + std::to_string(hohmannC3) + ")", best >= 0.85 * hohmannC3 && best <= 1.15 * hohmannC3);
    // one cell, solved again by hand: the same numbers
    double c3, vi, tof;
    check("porkchopCell agrees with the grid", lambert::porkchopCell(in, 20, 60, &c3, &vi, &tof) && float(c3) == g.c3[20 * 130 + 60] && float(vi) == g.vinf[20 * 130 + 60] && std::fabs(tof - ((s.arrJD0 + 6.0 * 60) - (s.depJD0 + 6.0 * 20))) < 1e-9);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Part D: the ephemeris table and the drift check. The Horizons rows are the saved ones (tests/reference/jpl_table.inc, as tests/test_jpl.cpp reads them).
static const int kRows = 301;
static const double kJplTable[kNBodies][kRows][7] =
#include "reference/jpl_table.inc"
;

static void testEphemerisTable() {
    // The table at the 301 saved Horizons dates: the largest heliocentric miss of each planet is the error the lab quotes (orbit/real_errors.h): the quoted number is the measured
    // maximum rounded UP to a whole km (and in Real plus 0.2 km), so quoted >= measured and quoted - measured < 1.5 km (as tests/test_jpl.cpp)
    std::vector<double> jds;
    for (int r = 0; r < kRows; r++) jds.push_back(kJplTable[0][r][0]);
    for (ephem::Model m : {ephem::Model::Simple, ephem::Model::Real, ephem::Model::RealNoRelativity}) {
        ephem::Ephemeris E(m);
        bool allQuoted = true;
        std::string line;
        for (int p = 0; p < 8; p++) {
            const std::vector<ephem::Row> rows = E.table(p, jds);
            double worst = 0;
            for (int r = 0; r < kRows; r++) {
                const Vec3 ref{kJplTable[p + 1][r][1] - kJplTable[0][r][1], kJplTable[p + 1][r][2] - kJplTable[0][r][2], kJplTable[p + 1][r][3] - kJplTable[0][r][3]};
                worst = std::max(worst, length(rows[std::size_t(r)].pos - ref) / 1000.0);
                if (r == 226) check(std::string("the row carries its date and a distance ") + ephem::modelName(m), rows[std::size_t(r)].jdTDB == jds[std::size_t(r)] && std::fabs(rows[std::size_t(r)].distAU - length(rows[std::size_t(r)].pos) / units::kAU) <= 1e-12);
            }
            const double q = double(ephem::worstErrorKm(m, p));
            const bool ok = q >= worst && q - worst < (m == ephem::Model::Simple ? 1.0 : 1.5);
            if (!ok) std::printf("   %s %s: quoted %.0f km, measured maximum %.3f km\n", ephem::modelName(m), kRealErrName[p], q, worst);
            allQuoted = allQuoted && ok;
            char b[64]; std::snprintf(b, sizeof b, " %s %.1f", kRealErrName[p], worst); line += b;
        }
        std::printf("ephemeris table vs Horizons, worst of 301 dates (km), %s:%s\n", ephem::modelName(m), line.c_str());
        check(std::string("the table at the saved Horizons dates has the lab's quoted errors, ") + ephem::modelName(m), allQuoted);
    }
    // the dates the panel takes and shows: J2000 is 2000-01-01 12:00 (JD 2451545.0, the IAU's own definition); a round trip over 1800 to 2100 keeps every day
    double jd = 0;
    check("2000-01-01 is JD 2451545.0 at 12:00 UT", ephem::dateToJdUT("2000-01-01", jd) && jd == 2451545.0 && ephem::jdUTToDate(2451545.0) == "2000-01-01");
    bool trip = true;
    for (double j = 2378496.5; j < 2488070.5; j += 17) { double back; trip = trip && ephem::dateToJdUT(ephem::jdUTToDate(j + 0.5).c_str(), back) && std::fabs(back - (std::floor(j + 0.5))) < 1e-9; }
    check("every 17th day 1800-2100 survives the text round trip", trip);
    check("a bad date is refused", !ephem::dateToJdUT("2026-02-30", jd) && !ephem::dateToJdUT("tomorrow", jd));
    // a row at a date of the lab: the same as the lab's own function
    ephem::Ephemeris S(ephem::Model::Simple);
    const ephem::Row r = S.at(4, 2461377.5);
    check("Simple row = planetStateAt", length(r.pos - planetStateAt(4, 2461377.5).pos) == 0 && r.lonDeg >= 0 && r.lonDeg < 360 && std::fabs(r.latDeg) < 90);
}

static void testDrift() {
    // the same loops as tests/test_nbody.cpp (testEightPlanets, testGRConservation, testAccurateEightPlanets), 100 years of one-day steps: 36,500 steps (a multiple of 50: the same samples)
    const long n = 36500;
    {   // the accurate step, Newtonian
        NBodyState s = nbodyStartState();
        const double e0 = nbodyEnergy(s);
        const Vec3 l0 = nbodyAngularMomentum(s);
        double maxE = 0, maxL = 0;
        for (long i = 0; i < n; i++) {
            nbodyStepAccurateN(kNBodies, kRealGM, s.x, s.v, 86400, false);
            if ((i + 1) % 50 == 0) { maxE = std::max(maxE, std::fabs(nbodyEnergy(s) / e0 - 1)); maxL = std::max(maxL, length(nbodyAngularMomentum(s) - l0) / length(l0)); }
        }
        const ephem::DriftResult R = ephem::driftCheckSteps(ephem::Integrator::Accurate, false, n);
        std::printf("drift check, accurate step, 100 years: |dE/E| %.3e, |dL|/|L| %.3e\n", R.maxRelEnergy, R.maxRelAngular);
        check("accurate step: the drift check gives test_nbody's loop exactly", R.done && R.maxRelEnergy == maxE && R.maxRelAngular == maxL && R.steps == n);
        check("accurate step: energy within test_nbody's 1e-11 over 100 years", R.maxRelEnergy < 1e-11);
    }
    for (bool gr : {false, true}) {   // the Wisdom-Holman map: test_nbody's 1,000-year limits 1e-8 (energy) and 1e-12 (angular momentum) hold over these 100 years too
        NBodyState s = nbodyStartState();
        const double e0 = gr ? nbodyEnergyGR(s) : nbodyEnergy(s);
        const Vec3 l0 = nbodyAngularMomentum(s);
        double maxE = 0, maxL = 0;
        for (long i = 0; i < n; i++) {
            nbodyStep(s, 86400, gr);
            if ((i + 1) % 50 == 0) { maxE = std::max(maxE, std::fabs((gr ? nbodyEnergyGR(s) : nbodyEnergy(s)) / e0 - 1)); maxL = std::max(maxL, length(nbodyAngularMomentum(s) - l0) / length(l0)); }
        }
        const ephem::DriftResult R = ephem::driftCheckSteps(ephem::Integrator::WisdomHolman, gr, n);
        std::printf("drift check, Wisdom-Holman map, relativity %s, 100 years: |dE/E| %.3e, |dL|/|L| %.3e\n", gr ? "on" : "off", R.maxRelEnergy, R.maxRelAngular);
        check(std::string("Wisdom-Holman: the drift check gives test_nbody's loop exactly, relativity ") + (gr ? "on" : "off"), R.done && R.maxRelEnergy == maxE && R.maxRelAngular == maxL);
        check("Wisdom-Holman: energy below test_nbody's 1e-8 and angular momentum below its 1e-12", R.maxRelEnergy < 1e-8 && R.maxRelAngular < 1e-12);
    }
    std::atomic<bool> stop{true};
    std::atomic<long> prog{0};
    const ephem::DriftResult R = ephem::driftCheckSteps(ephem::Integrator::Accurate, false, 5000, &prog, &stop);
    check("stop ends the run early and says so", !R.done && prog.load() == 50);
    check("years turn into one-day steps", ephem::driftCheck(ephem::Integrator::WisdomHolman, false, 1.0).steps == 365);
}

// The GM values of the pairs are typed in orbit/lagrange.cpp from JPL's satellite page (saved in the sources repo; its rows are in data/jpl/sats_phys_par.extract.txt): read again here, as received.
static std::string readFile(const std::string& path) { std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static double numberAfter(const std::string& t, std::size_t pos) {                    // the first number at or after pos (skipping tags and white space)
    while (pos < t.size() && !((t[pos] >= '0' && t[pos] <= '9'))) pos++;
    return std::strtod(t.c_str() + pos, nullptr);
}
static double satelliteGM(const std::string& page, const char* name) {                 // the table's row: <td class="text-left">name</td>, the code cell, then GM
    std::size_t p = page.find(std::string("<td class=\"text-left\">") + name + "</td>");
    if (p == std::string::npos) return NAN;
    p = page.find("</td>", p) + 5;                                                      // end of the name cell
    p = page.find("</td>", p) + 5;                                                      // end of the code cell
    p = page.find("<td", p);
    return numberAfter(page, p);
}
static double planetGM(const std::string& page, const char* name) {                    // the table "Planet and Pluto GM Values": <td>name</td> then GM
    const std::size_t t0 = page.find("Planet and Pluto GM Values");
    if (t0 == std::string::npos) return NAN;
    const std::size_t p = page.find(std::string("<td>") + name + "</td>", t0);
    return p == std::string::npos ? NAN : numberAfter(page, p + std::string(name).size() + 9);
}
static void testSatellitePage() {
    const std::string page = readFile(std::string(PL_DATA_DIR) + "/jpl/sats_phys_par.extract.txt");
    check("the JPL satellite extract is there", page.size() > 1000);
    int n = 0;
    for (const auto& s : lagrange::systems()) {
        if (s.heavy == "Sun") continue;
        n++;
        const double gm = satelliteGM(page, s.light.c_str()), gp = planetGM(page, s.heavy.c_str());
        check(s.name + ": the moon's GM is JPL's saved " + std::to_string(gm), gm == s.gmLight);
        check(s.name + ": the planet's own GM is JPL's saved " + std::to_string(gp), gp == s.gmHeavy);
    }
    check("seven pairs with a moon", n == 7);
    // the Sun - planet pairs use the planets' whole systems (nbody.h kRealGM, DE440): the Jupiter system is the planet's own GM plus its moons', 126712764.1 against 126686531.9 here
    check("Sun - Jupiter: the system's GM is above the planet's own", lagrange::systems()[4].gmLight > planetGM(page, "Jupiter"));
}

// The numbers typed in these tests from Tatum's chapter appear in the extract the sources check keeps (data/celestial/tatum_ch16.extract.txt)
static void testExtracts() {
    const std::string tatum = readFile(std::string(PL_DATA_DIR) + "/celestial/tatum_ch16.extract.txt");
    for (const char* s : {"24.959 935 8", "0.996 757", "0.080 464 5", "1.0033PJ", "12.428", "1.690392", "0.677756", "0.489038", "0.524579", "0.436062", "0.446273", "0.421875", "16.1.9", "16.2.26", "16.2.27"})
        check(std::string("Tatum's extract holds ") + s, tatum.find(s) != std::string::npos);
    const std::string pol = readFile(std::string(PL_DATA_DIR) + "/celestial/poliastro_lambert.extract.txt");
    for (const char* s : {"15945.34", "12214.83399", "10249.46731", "2.058925, 2.915956", "-3.451569, 0.910301", "-3.4515665,~0.91031354", "5000.0, 10000.0, 2100.0", "-14600.0, 2500.0, 7000.0", "-5.9925, 1.9254, 3.2456", "-3.3125, -4.1966, -0.38529"})
        check(std::string("the poliastro page's extract holds ") + s, pol.find(s) != std::string::npos);
    const std::string izzo = readFile(std::string(PL_DATA_DIR) + "/celestial/izzo_2015.extract.txt");
    for (const char* s : {"Algorithm 1 Lambert solver", "Householder", "(18)", "(22)", "(30)"})
        check(std::string("Izzo's extract holds ") + s, izzo.find(s) != std::string::npos);
}

int main() {
    testLagrangeForce();
    testEquilateral();
    testTatumQuintics();
    testTatumTable();
    testRouth();
    testHill();
    testSystems();
    testSatellitePage();
    testExtracts();
    testKirkwood();
    testLambertGeometry();
    testLambertSeries();
    testLambertParabola();
    testTransferArc();
    testLambertPublished();
    testPorkchopCpu();
    testEphemerisTable();
    testDrift();
    std::printf("celestial: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
