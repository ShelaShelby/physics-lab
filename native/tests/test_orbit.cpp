// Unit tests for the Orbit Lab physics.
//
// The reference numbers in reference/reference_values.h were made by running the
// web version's own JavaScript (cut straight out of index.html) in Node; see
// reference/make_reference.js. These tests check that the C++ port gives the same
// positions, velocities and orbit shapes.
//
// Run:  ctest --test-dir build   (or run the orbit_tests program directly)

#include <cmath>
#include <regex>
#include <cstdio>
#include <cstring>
#include <string>

#include "orbit/belts.h"
#include "orbit/card_text.h"
#include "orbit/date_text.h"
#include "orbit/format.h"
#include "orbit/kepler.h"
#include "orbit/sky.h"
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

    // 11) metric / imperial: in imperial every facts line must be converted (no km or °C left anywhere),
    //     with the web version's rounding rules
    {
        check(fmt::celsius(15) == "15 °C", "15 °C metric");
        fmt::setImperial(true);
        check(fmt::celsius(15) == "59 °F", "15 °C -> " + fmt::celsius(15));
        check(fmt::celsius(-65) == "-85 °F", "-65 °C -> " + fmt::celsius(-65));
        check(fmt::km(6371) == "3,959 mi", "6,371 km -> " + fmt::km(6371));
        check(fmt::kmPlain(11.1) == "6.9 mi", "11.1 km -> " + fmt::kmPlain(11.1));
        check(fmt::millionKm(149.6) == "93 million mi", "149.6 million km -> " + fmt::millionKm(149.6));
        check(fmt::kms(29.78) == "18.5 mi/s", "29.78 km/s -> " + fmt::kms(29.78));
        check(fmt::unitText("Winds over 2,000 km/h, the fastest known") == "Winds over 1,240 mph, the fastest known", fmt::unitText("Winds over 2,000 km/h, the fastest known"));
        check(fmt::unitText("Only about 12 km across") == "Only about 7.5 mi across", fmt::unitText("Only about 12 km across"));
        check(fmt::unitText("It is −65 °C there") == "It is −85 °F there", fmt::unitText("It is −65 °C there"));
        SolarSystem s; s.loadAt(ref::kEpochMs);
        CardOptions opt;
        std::vector<Selection> all = {{Selection::Star}, {Selection::Bary}};
        for (const Body& b : s.bodies) {
            all.push_back({Selection::Body, b.id});
            for (std::size_t j = 0; j < b.moons.size(); j++) all.push_back({Selection::Moon, b.id, static_cast<int>(j)});
        }
        for (std::size_t g = 0; g < s.belts.size(); g++) all.push_back({Selection::Belt, 0, static_cast<int>(g)});
        for (int f = 0; f < farCount(); f++) all.push_back({Selection::Far, 0, f});
        int cards = 0;
        for (const Selection& sel : all) {
            Card c;
            if (!cardText(s, sel, opt, c)) continue;
            cards++;
            for (const std::string& l : std::vector<std::string>{c.title} ) (void)l;
            std::vector<std::string> lines = c.lines; lines.push_back(c.title);
            for (const auto& l : lines) {
                const bool metricLeft = std::regex_search(l, std::regex(R"(\d km|°C| km/)"));
                if (metricLeft) std::printf("   still metric: [%s] %s\n", c.title.c_str(), l.c_str());
                check(!metricLeft, "imperial card line converted");
            }
        }
        std::printf("  imperial: %d cards checked\n", cards);
        fmt::setImperial(false);
    }

    // 12) the info cards (printed so they can be read and compared with the web version)
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

    // 13) the Milky Way band (0.10.1): it must run smoothly across the sky and fade only at the edge of the map. Checked for 312 view directions (24 turns x 13 tilts, from
    //     looking straight down to straight up) on two picture shapes, against what follows from the geometry (not against the old drawing, which dropped most of the band:
    //     its neighbour test read flags that the same loop had just cleared, so one gap in the visible stretch removed everything after it).
    {
        const Sky& S = skyData();
        const Vec3 A = S.galCentre, P = S.galPole, Bv = cross(P, A);
        const double pic[2][2] = {{840, 620}, {400, 900}};
        const int n = kBandPieces;
        const double arc = 2 * units::kPi / n;                       // angle between two neighbouring samples on the sky
        int views = 0, wrongSet = 0, manyRuns = 0, wrongPlace = 0, noBand = 0;
        double worstEndFade = 0, worstFadeStep = 0, worstStep = 0;
        for (const auto& wh : pic) {
            const double W = wh[0], H = wh[1], K = std::hypot(W, H) / 2 / 1.75;
            for (int ky = 0; ky < 24; ky++)
                for (int kp = -6; kp <= 6; kp++) {
                    const double yaw = ky * 2 * units::kPi / 24, pitch = kp * 0.25;
                    const double ca = std::cos(yaw), sa = std::sin(yaw), cb = std::cos(pitch), sb = std::sin(pitch);
                    std::vector<BandPoint> band;
                    milkyWayBand(S, W, H, ca, sa, cb, sb, band);
                    views++;
                    if (int(band.size()) != n + 1) { wrongSet++; continue; }
                    std::vector<char> vis(n + 1, 0);
                    int drawn = 0;
                    for (int i = 0; i <= n; i++) {                   // which samples of the great circle are on the map (100 degrees round the middle)?
                        const double l = i * arc;
                        double X = 0, Y = 0;
                        vis[i] = skyScreen(A * std::cos(l) + Bv * std::sin(l), W, H, ca, sa, cb, sb, X, Y);
                        if (vis[i] && (std::fabs(band[i].X - X) > 1e-9 || std::fabs(band[i].Y - Y) > 1e-9)) wrongPlace++;   // the band is still on the same great circle
                    }
                    for (int i = 0; i <= n; i++) {
                        const int pv = i == 0 ? n - 1 : i - 1, nx = i == n ? 1 : i + 1;   // the circle is closed: sample n is sample 0
                        const bool want = vis[i] && (vis[pv] || vis[nx]);              // on the map and not alone
                        if (band[i].ok != want) wrongSet++;
                        if (band[i].ok) drawn++;
                    }
                    if (drawn < 100) noBand++;                       // a great circle always has an arc of at least 160 degrees within 100 degrees of any direction
                    int runs = 0;                                    // separate drawn stretches round the closed circle
                    for (int i = 0; i < n; i++) {
                        const int pv = i == 0 ? n - 1 : i - 1;
                        if (band[i].ok && !band[pv].ok) {
                            runs++;
                            worstEndFade = std::max(worstEndFade, band[i].fade);   // where a stretch begins it must already be nearly faded out
                        }
                        const int nx = i + 1 == n ? 0 : i + 1;
                        if (band[i].ok && !band[nx].ok) worstEndFade = std::max(worstEndFade, band[i].fade);
                    }
                    if (runs > 1) manyRuns++;
                    for (int i = 0; i < n; i++) {
                        const BandPoint &p = band[i], &q = band[i + 1];
                        if (!p.ok || !q.ok) continue;
                        worstFadeStep = std::max(worstFadeStep, std::fabs(q.fade - p.fade));
                        worstStep = std::max(worstStep, std::hypot(q.X - p.X, q.Y - p.Y) / (K * arc));
                    }
                }
        }
        // limits from the geometry: (a) fade = (1.75 - theta) / 0.2 near the edge and neighbouring samples are `arc` apart on the sky, so the fade changes by at most
        // arc / 0.2 = 0.131 per step; (b) a stretch ends where the map ends, where the fade is at most that same step above zero (one sample inside the edge), plus
        // one more step if an end sample has to be left out: 2 steps = 0.262; (c) in this projection a step of `arc` on the sky is at most arc x K x theta / sin(theta)
        // long on the picture (1.777 at the 100 degrees edge), so 1.8 x arc x K.
        const double fadeStep = arc / 0.2;
        std::printf("  Milky Way band, %d views: wrong set of drawn points %d, views with more than one stretch %d, views with almost no band %d, off the great circle %d; "
                    "largest fade at the end of a stretch %.3f (limit %.3f), largest fade change per step %.3f (limit %.3f), longest step %.3f x arc x K (limit 1.8)\n",
                    views, wrongSet, manyRuns, noBand, wrongPlace, worstEndFade, 2 * fadeStep, worstFadeStep, fadeStep, worstStep);
        check(views == 624, "Milky Way band: all views tried");
        check(wrongSet == 0, "Milky Way band: every point on the map that has a neighbour on the map is drawn, and no other (no dropped pieces, no stubs)");
        check(manyRuns == 0 && noBand == 0, "Milky Way band: one connected run across the sky in every view");
        check(wrongPlace == 0, "Milky Way band: still on the same great circle (the galactic centre and pole are unchanged)");
        check(worstEndFade <= 2 * fadeStep + 1e-9, "Milky Way band: a run ends only where the map ends (nearly faded out there)");
        check(worstFadeStep <= fadeStep + 1e-9, "Milky Way band: the fade changes smoothly along the band");
        check(worstStep <= 1.8, "Milky Way band: no jumps in position along the band");
        // the middle of the sky: looking straight down, the band must be drawn right through the middle region of the map, not only at its edge
        {
            std::vector<BandPoint> band;
            milkyWayBand(S, 840, 620, 1, 0, 1, 0, band);
            int inner = 0;
            for (const BandPoint& p : band) if (p.ok && std::hypot(p.X - 420, p.Y - 310) < 0.6 * std::hypot(840, 620) / 2) inner++;
            check(inner > 10, "Milky Way band: drawn inside the inner part of the map too (looking straight down)");
        }
    }

    // 14) 0.10.5 stage 2, Orbit Lab text fixes from the audit (REPORT of stage 1). The expected numbers are worked out by hand from the lab's own figures.
    {
        const auto has = [](const std::string& s, const char* w) { return s.find(w) != std::string::npos; };
        const auto joined = [](const std::vector<std::string>& v) { std::string s; for (const auto& l : v) s += l + "\n"; return s; };
        // row 7: Oort travel times come from Voyager 1's speed (16.94 km/s x 0.21095 AU per year per km/s = 3.5735 AU/yr) and the distances in the same text
        const double auPerYear = 16.94 * 0.21095;
        near(voyagerYearsFromSun(2000), 2000 / auPerYear, 560, 1e-9, "Oort: years to 2,000 AU (by hand 559.7)");
        near(voyagerYearsFromSun(200000), 200000 / auPerYear, 56000, 1e-9, "Oort: years to 200,000 AU (by hand 55,968)");
        near(voyagerYearsFromSun(200000) / voyagerYearsFromSun(2000), 100, 100, 1e-12, "Oort: 100 times the distance, 100 times the time");
        const std::string oort = joined(beltLines("oort"));
        check(has(oort, "about 560 years") && has(oort, "about 56,000 years") && has(oort, "2,000 AU") && has(oort, "200,000 AU"), "Oort card: travel times 560 and 56,000 years next to 2,000 and 200,000 AU");
        check(has(oortCaption(), "2,000 to 200,000 AU") && has(oortCaption(), "about 560 years") && !has(oortCaption(), "300 years"), "Journey Oort caption: 560 years from the same distances");
        check(!has(oort, "300 years") && !has(oort, "30,000"), "Oort card: the old 300 and 30,000 are gone");
        check(has(oort, "4.24 light years") && has(oort, "268,000 AU"), "Oort card: Proxima 4.24 light years = 268,000 AU (4.24 x 63,241.077 = 268,142)");
        check(probeLaunchYear("v1") == 1977 && probeLaunchYear("v2") == 1977, "probes: launch year 1977");
        // row 1, 5, 10: the notes after a jump
        const std::string simple2060 = dateNotes(true, false, false), real2060 = dateNotes(true, true, false), real2026 = dateNotes(false, true, true);
        check(has(simple2060, "After 2050") && has(simple2060, "extrapolated"), "jump note: Simple after 2050 says extrapolated");
        check(!has(real2060, "After 2050") && !has(real2060, "extrapolated") && !has(real2026, "extrapolated"), "jump note: Real never says extrapolated");
        check(has(real2060, "measured against JPL for 1800 to 1 Jan 2100 only") && has(real2026, "measured against JPL for 1800 to 1 Jan 2100 only"), "jump note: Real says the errors were measured for 1800 to 1 Jan 2100 only");
        check(!has(simple2060, "measured against JPL"), "jump note: Simple does not quote the Real errors");
        for (const std::string& n : {simple2060, real2060, real2026}) check(has(n, "simple orbit fixed at one date") && has(n, "Pluto is about 1 AU off in 1800") && !has(n, "where they really"), "jump note: dwarf planets and comets are on a simple orbit");
        check(has(real2026, "Earth’s Moon is the real one") && has(real2026, "1800 to 1 Jan 2100"), "jump note: real Moon, with its range");
        check(has(real2060, "only from 1800 to 1 Jan 2100") && !has(real2060, "is the real one"), "jump note: Real but past the Moon's range says so");
        check(has(simple2060, "The moons") && !has(simple2060, "Earth’s Moon is the real one"), "jump note: Simple has no real Moon");
        check(has(datePickHint(), "simple orbit") && !has(datePickHint(), "where they really"), "date hint: no 'where they really were'");
        // row 6: the Voyager caption before it can be shown
        check(has(voyagerFallbackCaption(1960, 1977, 1990), "not launched yet"), "Voyager caption 1960: not launched yet");
        check(has(voyagerFallbackCaption(1980, 1977, 1990), "launched in 1977") && has(voyagerFallbackCaption(1980, 1977, 1990), "from 1990 on") && !has(voyagerFallbackCaption(1980, 1977, 1990), "not launched yet"), "Voyager caption 1980: launched in 1977, shown from 1990 on");
        // cards: rows 1, 8, 13, 19
        SolarSystem s; s.loadAt(ref::kEpochMs);
        const auto cardOf = [&](const SolarSystem& sys, const char* name, int moon = -1) {
            for (const Body& b : sys.bodies) if (b.name == name) { Card c; CardOptions o; cardText(sys, moon < 0 ? Selection{Selection::Body, b.id} : Selection{Selection::Moon, b.id, moon}, o, c); return joined(c.lines); }
            return std::string("(no such body)");
        };
        check(has(cardOf(s, "Ceres"), "simple orbit fixed at one date") && has(cardOf(s, "Halley"), "simple orbit fixed at one date"), "cards: dwarf planet and comet say their orbit is fixed at one date");
        check(!has(cardOf(s, "Mars"), "habitable zone") && !has(cardOf(s, "Earth"), "habitable zone") && !has(cardOf(s, "Ceres"), "habitable zone"), "cards: no 'In the habitable zone' verdict on planets");
        check(!has(cardOf(s, "Earth"), "simple orbit fixed"), "cards: planets do not carry the dwarf/comet line");
        check(has(cardOf(s, "Ceres"), "assumes 30 % reflected") && !has(cardOf(s, "Earth"), "assumes 30 %"), "cards: the no-atmosphere temperature says it assumes 30 % reflected (Earth's is the real one)");
        Card sc; CardOptions so; cardText(s, Selection{Selection::Star}, so, sc);
        const std::string sun = joined(sc.lines);
        check(has(sun, "Habitable zone (rough)") && has(sun, "Frost line (rough)") && has(sun, "Lifetime (rough)"), "Sun card: habitable zone, frost line and lifetime are marked rough");
        check(!has(cardOf(s, "Earth", 0), "× our Moon") && has(cardOf(s, "Earth", 0), "Radius: 1737.4 km"), "Moon card: no comparison with itself");
        check(has(cardOf(s, "Jupiter", 0), "× our Moon"), "Io card: still compared with our Moon");
    }

    std::printf("%d checks, %d failed. Worst position difference vs the web version: %.3g of the distance from the Sun (belt rocks: %.3g).\n",
                g_checks, g_failed, worstRel, worstBelt);
    return g_failed ? 1 : 0;
}
