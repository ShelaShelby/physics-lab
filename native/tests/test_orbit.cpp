// Unit tests for the Orbit Lab physics.
//
// The reference numbers in reference/reference_values.h were made by running the
// web version's own JavaScript (cut straight out of index.html) in Node; see
// reference/make_reference.js. These tests check that the C++ port gives the same
// positions, velocities and orbit shapes.
//
// Run:  ctest --test-dir build   (or run the orbit_tests program directly)

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "orbit/belts.h"
#include "orbit/card_text.h"
#include "orbit/format.h"
#include "orbit/kepler.h"
#include "orbit/spin.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"
#include "reference/reference_values.h"

using namespace orbit;

static int g_failed = 0, g_checks = 0;

static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}

// |got − want| must be within relTol of the size of `scale` (e.g. the body's distance from the Sun)
static void near(double got, double want, double scale, double relTol, const std::string& what) {
    const double err = std::fabs(got - want);
    const bool ok = err <= relTol * std::fabs(scale) || (want == got);
    if (!ok) std::printf("   %s: got %.17g want %.17g (error %.3g, allowed %.3g)\n", what.c_str(), got, want, err, relTol * std::fabs(scale));
    check(ok, what);
}

static double worstRel = 0;
static double worstBelt = 0;   // the same for belt rocks

static void compareStates(const SolarSystem& sys, const ref::BodyState* want, std::size_t n, double relTol, const char* label) {
    check(sys.bodies.size() == n, std::string(label) + ": number of bodies");
    for (std::size_t i = 0; i < n && i < sys.bodies.size(); i++) {
        const Body& b = sys.bodies[i];
        const ref::BodyState& w = want[i];
        const std::string tag = std::string(label) + " " + w.name;
        check(b.name == w.name, tag + ": name/order");
        const double r = std::sqrt(w.x * w.x + w.y * w.y + w.z * w.z), v = std::sqrt(w.vx * w.vx + w.vy * w.vy + w.vz * w.vz);
        near(b.pos.x, w.x, r, relTol, tag + " x");
        near(b.pos.y, w.y, r, relTol, tag + " y");
        near(b.pos.z, w.z, r, relTol, tag + " z");
        near(b.vel.x, w.vx, v, relTol, tag + " vx");
        near(b.vel.y, w.vy, v, relTol, tag + " vy");
        near(b.vel.z, w.vz, v, relTol, tag + " vz");
        const double dr = std::sqrt((b.pos.x - w.x) * (b.pos.x - w.x) + (b.pos.y - w.y) * (b.pos.y - w.y) + (b.pos.z - w.z) * (b.pos.z - w.z));
        worstRel = std::fmax(worstRel, dr / r);
    }
}

static void runSteps(SolarSystem& sys, double tspeed, int steps) {
    for (int k = 0; k < steps; k++) sys.step(units::kPhysicsStep, tspeed);
}

int main() {
    // 1) Kepler's equation
    for (const auto& k : ref::kKepler) {
        const double E = keplerE(k.e, k.Mdeg);
        near(E, k.E, 1.0, 1e-12, "keplerE e=" + std::to_string(k.e) + " M=" + std::to_string(k.Mdeg));
    }

    // 2) starting positions on 1 Jan 2026 from the orbital elements
    SolarSystem sys;
    sys.loadAt(ref::kEpochMs);
    compareStates(sys, ref::kInitial, sizeof(ref::kInitial) / sizeof(ref::kInitial[0]), 1e-12, "initial");

    // 3) orbit shapes
    for (std::size_t i = 0; i < sizeof(ref::kOrbits) / sizeof(ref::kOrbits[0]); i++) {
        const OrbitShape o = orbitOf(sys.bodies[i]);
        const auto& w = ref::kOrbits[i];
        const std::string tag = std::string("orbit ") + w.name;
        near(o.a, w.a_m, w.a_m, 1e-9, tag + " a");
        near(o.e, w.e, 1.0, 1e-9, tag + " e");
        near(o.inclDeg, w.inclDeg, 180.0, 1e-9, tag + " incl");
        near(o.periodYears, w.periodYears, w.periodYears, 1e-9, tag + " period");
    }

    // 4) barycentre
    {
        const Vec3 bc = sys.barycentre();
        near(bc.x, ref::kBary[0], units::kSunRadius, 1e-9, "barycentre x");
        near(bc.y, ref::kBary[1], units::kSunRadius, 1e-9, "barycentre y");
        near(bc.z, ref::kBary[2], units::kSunRadius, 1e-9, "barycentre z");
    }

    // 5) leapfrog runs, same fixed steps as the web (1/240 s of real time each)
    const double tol = 1e-9;   // relative to the body's distance from the Sun (1e-9 of 1 AU is 150 m)
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        runSteps(s, ref::kRunDefaultTspeed, ref::kRunDefaultSteps);
        near(s.t, ref::kRunDefaultSimTime, ref::kRunDefaultSimTime, 1e-12, "default run: simulated time");
        compareStates(s, ref::kRunDefault, sizeof(ref::kRunDefault) / sizeof(ref::kRunDefault[0]), tol, "default run");
    }
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        runSteps(s, ref::kRunFastTspeed, ref::kRunFastSteps);
        near(s.t, ref::kRunFastSimTime, ref::kRunFastSimTime, 1e-12, "fast run: simulated time");
        compareStates(s, ref::kRunFast, sizeof(ref::kRunFast) / sizeof(ref::kRunFast[0]), tol, "fast run");
    }
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        runSteps(s, ref::kRunRealTimeTspeed, ref::kRunRealTimeSteps);
        near(s.t, ref::kRunRealTimeSimTime, ref::kRunRealTimeSimTime, 1e-12, "real-time run: simulated time");
        compareStates(s, ref::kRunRealTime, sizeof(ref::kRunRealTime) / sizeof(ref::kRunRealTime[0]), tol, "real-time run");
    }

    // 6) physics sanity, independent of the web version: after one Earth year Earth is back
    //    near where it started (the JPL year is 365.256 days; our clock year is 365.25 days),
    //    and leapfrog keeps the orbital energy nearly constant.
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        const Vec3 p0 = s.bodies[2].pos;
        const double e0 = orbitOf(s.bodies[2]).energy;
        const double tspeed = 0.0;                            // 1 s = 58.13 days
        const double realSeconds = units::kYear / units::kSimTime;   // real seconds for one simulated year at tspeed 0 (= 2π)
        const int steps = static_cast<int>(realSeconds / units::kPhysicsStep);
        runSteps(s, tspeed, steps);
        s.step(realSeconds - steps * units::kPhysicsStep, tspeed);    // the last part of a step, to land exactly on one year
        const double moved = length(s.bodies[2].pos - p0);
        // Earth's orbital period here is 1.000006 years (0.002 days more, about 6,000 km of travel); allow 1e-4 AU (15,000 km)
        check(moved < 1e-4 * units::kAU, "Earth back at its start after one year (moved " + std::to_string(moved / units::kKm) + " km)");
        const double e1 = orbitOf(s.bodies[2]).energy;
        near(e1, e0, e0, 1e-6, "Earth orbital energy conserved over one year");
    }

    // 7) belts: same random numbers, same rocks in the same places as the web version
    {
        Mulberry32 r(20260929);
        for (double want : ref::kRng) near(r.next(), want, 1.0, 0, "rng");
        const auto belts = makeBelts(ref::kEpochMs);
        check(belts.size() == 4, "4 belt groups");
        for (std::size_t i = 0; i < belts.size() && i < 4; i++) check(belts[i].n == ref::kBeltCounts[i], "belt count " + belts[i].key);
        for (const auto& w : ref::kBeltRocks) {
            const BeltGroup* g = nullptr;
            for (const auto& b : belts) if (b.key == w.group) g = &b;
            if (!g) { check(false, std::string("belt group ") + w.group); continue; }
            const Vec3 p = beltPoint(*g, w.k, w.t);
            const double r = std::sqrt(w.x * w.x + w.y * w.y + w.z * w.z);
            const std::string tag = std::string("belt ") + w.group + " rock " + std::to_string(w.k) + (w.t > 0 ? " +100 y" : "");
            // 1e-6: the web stores the Oort cloud in sim units as 32-bit floats, we store metres as 32-bit floats
            near(p.x, w.x, r, 1e-6, tag + " x"); near(p.y, w.y, r, 1e-6, tag + " y"); near(p.z, w.z, r, 1e-6, tag + " z");
            worstBelt = std::fmax(worstBelt, length(p - Vec3{w.x, w.y, w.z}) / r);
        }
    }

    // 8) spin, axis tilt and seasons: the same card lines as the web version
    {
        SpinFrame f;
        spinFrame("Mars", 2461041.5, f);
        const double* w = ref::kMarsFrame;
        near(f.P.x, w[0], 1, 1e-12, "Mars pole x"); near(f.P.y, w[1], 1, 1e-12, "Mars pole y"); near(f.P.z, w[2], 1, 1e-12, "Mars pole z");
        near(f.A.x, w[3], 1, 1e-9, "Mars meridian x"); near(f.B.z, w[8], 1, 1e-9, "Mars east z"); near(f.W, w[9], 360, 1e-12, "Mars W");
        for (const auto& c : ref::kSpinCases) {
            SolarSystem s; s.loadAt(c.ms);
            std::vector<std::string> got;
            if (std::string(c.body) == "Sun") got = sunSpinLines(s, c.tspeed, false);
            else
                for (const Body& b : s.bodies)
                    if (b.name == c.body) { got = spinLines(b, s, c.tspeed, false); for (auto& l : seasonLines(b, s)) got.push_back(l); }
            // The web writes "Turns N times a second" with its rounded 58.13 days per second; we use the exact
            // 58.1302, so N may differ in its last digit. Everything else must be word for word the same.
            const auto norm = [](std::vector<std::string> v) {
                for (auto& l : v)
                    if (l.rfind("Turns ", 0) == 0 && l.find("times a second") != std::string::npos) l = "Turns N times a second" + l.substr(l.find(" times a second") + 15);
                return v;
            };
            const bool same = norm(got) == norm(c.lines);
            if (!same) {
                std::printf("   %s (%lld, tspeed %g):\n", c.body, c.ms, c.tspeed);
                for (auto& l : got) std::printf("     got  %s\n", l.c_str());
                for (auto& l : c.lines) std::printf("     want %s\n", l.c_str());
            }
            check(same, std::string("spin/season lines ") + c.body);
        }
    }

    // 9) famous dates the web version quotes (Jump to a date): Mars 55.76 million km from Earth on
    //    27 Aug 2003 (NASA), and Jupiter and Saturn 0.1° apart as seen from Earth on 21 Dec 2020
    {
        const auto find = [](const SolarSystem& s, const char* n) { for (const Body& b : s.bodies) if (b.name == n) return b.pos; return Vec3{}; };
        SolarSystem s; s.loadAt(1061942400000LL + 10 * 3600000LL);   // 27 Aug 2003, 10:00 UT (closest at 09:51 UT)
        const double mkm = length(find(s, "Mars") - find(s, "Earth")) / 1e9;
        std::printf("  Mars–Earth on 27 Aug 2003: %.2f million km (NASA: 55.76)\n", mkm);
        check(std::fabs(mkm - 55.76) < 0.3, "Mars–Earth distance on 27 Aug 2003");
        s.loadAt(1608552000000LL);                                    // 21 Dec 2020, 12:00 UT
        const Vec3 e = find(s, "Earth"), j = find(s, "Jupiter") - e, sa = find(s, "Saturn") - e;
        const double sep = std::acos(dot(j, sa) / (length(j) * length(sa))) * 180 / units::kPi;
        std::printf("  Jupiter–Saturn on 21 Dec 2020: %.3f° apart (real: 0.10°)\n", sep);
        check(std::fabs(sep - 0.1) < 0.05, "Jupiter–Saturn great conjunction");
    }

    // 10) number and date wording (as JavaScript would write them)
    check(fmt::plainNumber(-110) == "-110", "plainNumber(-110) = " + fmt::plainNumber(-110));
    check(fmt::plainNumber(2439.7) == "2439.7", "plainNumber(2439.7) = " + fmt::plainNumber(2439.7));
    check(fmt::plainNumber(0.0553) == "0.0553", "plainNumber(0.0553) = " + fmt::plainNumber(0.0553));
    check(fmt::plainNumber(149600000) == "149600000", "plainNumber(149600000) = " + fmt::plainNumber(149600000));
    check(fmt::commas(1234567.4) == "1,234,567", "commas");
    check(fmt::fixed(-0.001, 2) == "0.00", "fixed(-0.001, 2) = " + fmt::fixed(-0.001, 2));
    check(fmt::dateUTC(ref::kEpochMs) == "1 Jan 2026", "dateUTC = " + fmt::dateUTC(ref::kEpochMs));
    check(fmt::dateUTC(-5364662400000LL) == "1 Jan 1800", "dateUTC 1800 = " + fmt::dateUTC(-5364662400000LL));
    check(fmt::kg(9.384e20) == "9.4×10²⁰ kg", "kg = " + fmt::kg(9.384e20));
    check(fmt::years(0.05) == "18 days", "years(0.05) = " + fmt::years(0.05));
    check(fmt::years(0.2408) == "0.24 years", "years(0.2408) = " + fmt::years(0.2408));

    // 11) the info cards (printed so they can be read and compared with the web version)
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        CardOptions opt; opt.tspeed = units::kTspeedDefault;
        const Selection sels[] = {{Selection::Star}, {Selection::Bary}, {Selection::Body, 3}, {Selection::Body, 10}, {Selection::Body, 14}, {Selection::Moon, 3, 0}};
        for (const Selection& sel : sels) {
            Card c;
            check(cardText(s, sel, opt, c), "card text exists");
            std::printf("  [%s]\n", c.title.c_str());
            for (const auto& l : c.lines) std::printf("    %s\n", l.c_str());
        }
    }

    std::printf("%d checks, %d failed. Worst position difference vs the web version: %.3g of the distance from the Sun (belt rocks: %.3g).\n",
                g_checks, g_failed, worstRel, worstBelt);
    return g_failed ? 1 : 0;
}
