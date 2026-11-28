// 0.10.4 stage 2: the real Moon in the Orbit Lab (Real planet physics only), written from independent references BEFORE the code:
//  * JPL Horizons Earth (399) seen from the Sun, tests/reference/earth_table.inc (tools/make_earth_ref.py): the Real-mode Earth against it. In Real the N-body body "Earth" is the
//    Earth-Moon barycentre (EMB); the app now draws Earth at EMB minus the Moon's share of the Earth-Moon vector, so the Earth error is the EMB error plus a tiny Moon-offset error.
//  * the Moon's vector seen from Earth equals the ELP2000-82B vector, and Earth and Moon balance on the N-body EMB;
//  * phases (new, first quarter, full, last quarter) of known geometry and of the Horizons full moon of 2026-01-03 (JD 2461043.91990 TDB, tests/reference/moon_table.inc);
//  * Simple mode, the other planets and the other moons are unchanged; the 0.10.3 worst errors (real_errors.h) hold for every planet but Earth.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "orbit/time_scale.h"
#include "orbit/card_text.h"
#include "orbit/moon_elp.h"
#include "orbit/nbody.h"
#include "orbit/real_errors.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"

using namespace orbit;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }

// { Julian date (TDB), x, y, z (m) }: Horizons Earth (399) seen from the Sun (10), 1 January 12:00 TDB of 1800 .. 2100, ecliptic and equinox of J2000, geometric.
static const int kRows = 301;
static const double kEarthTable[kRows][4] =
#include "reference/earth_table.inc"
;
// the nine bodies' rows of 0.10.3 (the Sun, Mercury ... Neptune; Earth = Earth-Moon barycentre): { JD, x, y, z, vx, vy, vz }, barycentric.
static const double kJplTable[kNBodies][kRows][7] =
#include "reference/jpl_table.inc"
;
// { group, JD (TDB), Moon x, y, z (m), Sun x, y, z (m) } seen from the Earth's centre (stage 1): group 2 = hourly 2026-01-02 .. 04
static const double kMoonTable[170][8] =
#include "reference/moon_table.inc"
;

// Regression guards (NOT physics limits), about 1 % above the measurement printed by this program (km, Earth against Horizons' Earth over the 301 dates); filled in after the first run.
static const double kGuardEarthKmOff = 13800, kGuardEarthKmOn = 13.7;   // 0.35 (every Moon term in the split): measured 13725.1 and 13.1 km; 0.29: 13654.1 km (GR off) and 74.6 km (GR on), guard 75.4; 0.28 (Wisdom-Holman): 3959.9 and 22080.1 km

static Vec3 v3(const double* p) { return {p[0], p[1], p[2]}; }
static int64_t unixMsOf(double jd) { return static_cast<int64_t>(timescale::unixMsOfTDB(jd)); }   // the table's JDs are TDB; 0.46.3: the lab's clock is UT, so TDB - UT is taken off (orbit/time_scale.h)

static void testConstants() {
    // GM of the Earth alone and of the Moon: JPL "Astrodynamic Parameters" page (DE440 values), read from the page itself on 2026-10-03: Earth 398600.435507, Moon 4902.800118 km^3/s^2.
    check("constants: Earth GM is the page's 398600.435507 km^3/s^2", kRealGMEarthAlone == 398600.435507e9);
    check("constants: Moon GM is the page's 4902.800118 km^3/s^2", kRealGMMoon == 4902.800118e9);
    // the lab's Earth-system GM (nbody) is the sum
    check("constants: the N-body Earth GM is Earth + Moon (relative 1e-15)", std::fabs((kRealGMEarthAlone + kRealGMMoon) / kRealGM[3] - 1) < 1e-15);
    // independent check: Horizons' own header of the Moon (reply of 2026-10-03, ephemeris DE441): "Earth/Moon mass ratio = 81.3005690769"
    const double ratio = kRealGMEarthAlone / kRealGMMoon, hz = 81.3005690769;
    std::printf("constants: Earth/Moon from the astrodynamic page %.10f, Horizons header %.10f: relative difference %.2e\n", ratio, hz, ratio / hz - 1);
    // measured 1.04e-8. My first guess of the limit was 1e-8, too tight: the header's physical data are dated 2018-Aug-15 and the page is DE440 (2021), so a difference of this size is expected. The limit is 1e-7:
    // this check only catches a wrong or swapped value; a relative 1e-7 in the ratio moves Earth's 4,700 km offset from the barycentre by 0.5 mm, far below the 4 km error of the Moon itself.
    check("constants: the page's Earth/Moon ratio agrees with the Horizons header within 1e-7 (measured value above)", std::fabs(ratio / hz - 1) < 1e-7);
}

// 0.75.2: the exact lit share, written here from the triangle Earth - Moon - Sun alone (law of cosines for the angle at the Moon, the phase angle i; Meeus, Astronomical Algorithms, ch. 48:
// k = (1 + cos i) / 2), independent of the app's vectors: EM, ES and MS are the three side lengths.
static double exactLitShare(double EM, double ES, double MS) { return (1.0 + (EM * EM + MS * MS - ES * ES) / (2.0 * EM * MS)) / 2.0; }

static void testPhaseGeometry() {
    // Known geometry, Sun along +x seen from Earth, the Moon at 0, 45, 90 ... degrees of longitude from the Sun. Longitude difference (Moon minus Sun) grows from new moon.
    static const char* names[8] = {"new moon", "waxing crescent", "first quarter", "waxing gibbous", "full moon", "waning gibbous", "last quarter", "waning crescent"};
    const Vec3 sun{1.496e11, 0, 0};
    bool namesOk = true, fracOk = true;
    for (int k = 0; k < 8; k++) {
        const double a = k * 45.0 * units::kPi / 180;
        const MoonPhase p = moonPhase(Vec3{3.844e8 * std::cos(a), 3.844e8 * std::sin(a), 0}, sun);
        namesOk = namesOk && std::string(p.name) == names[k] && p.index == k;
        const double EM = 3.844e8, ES = 1.496e11, MS = std::sqrt(EM * EM + ES * ES - 2 * EM * ES * std::cos(a));
        fracOk = fracOk && std::fabs(p.fraction - exactLitShare(EM, ES, MS)) < 1e-12 && std::fabs(p.elongationDeg - (k <= 4 ? 45.0 * k : 360 - 45.0 * k)) < 1e-9;
    }
    check("phase: the 8 names at 0, 45 ... 315 degrees from the Sun, in order, starting at new moon", namesOk);
    // (0.75.2: the reference was (1 - cos elongation) / 2, the first-order share; the deliberate change to the exact (1 + cos i) / 2 made it the exact triangle formula above. At the quarters the two differ by 0.0013.)
    check("phase: fraction = (1 + cos i) / 2 from the Earth-Moon-Sun triangle (exact) and the elongation is the angle at the Earth (0 .. 180)", fracOk);
    {
        const double a = 90.0 * units::kPi / 180, EM = 3.844e8, ES = 1.496e11;
        const MoonPhase p = moonPhase(Vec3{EM * std::cos(a), EM * std::sin(a), 0}, sun);
        check("phase: at 90 degrees of elongation the exact share is 0.5013, not the first-order 0.5000 (the Sun is not at infinity)", std::fabs(p.fraction - 0.5) > 0.0010 && std::fabs(p.fraction - 0.5) < 0.0016 && p.fraction > 0.5);
        (void)ES;
    }
    // the names follow the lit fraction: new moon under 2 %, crescent to 40 %, quarter to 60 %, gibbous to 98 %, full above. In elongation: acos(1 - 2 f) = 16.26, 78.46, 101.54, 163.74 degrees.
    auto nameAt = [&](double deg) { const double a = deg * units::kPi / 180; return std::string(moonPhase(Vec3{std::cos(a), std::sin(a), 0}, sun).name); };
    check("phase: new moon below 16.26 degrees (2 % lit), waxing crescent above", nameAt(16.0) == "new moon" && nameAt(16.5) == "waxing crescent" && nameAt(-16.0) == "new moon" && nameAt(-16.5) == "waning crescent");
    check("phase: crescent to 78.46 degrees (40 % lit), first quarter above", nameAt(78.0) == "waxing crescent" && nameAt(79.0) == "first quarter" && nameAt(-78.0) == "waning crescent" && nameAt(-79.0) == "last quarter");
    check("phase: quarter to 101.54 degrees (60 % lit), waxing gibbous above", nameAt(101.0) == "first quarter" && nameAt(102.0) == "waxing gibbous" && nameAt(-101.0) == "last quarter" && nameAt(-102.0) == "waning gibbous");
    check("phase: gibbous to 163.74 degrees (98 % lit), full moon above", nameAt(163.0) == "waxing gibbous" && nameAt(164.5) == "full moon" && nameAt(-163.0) == "waning gibbous" && nameAt(-164.5) == "full moon");
    // so the name never disagrees with the percent: "quarter" only between 40 and 60 % lit
    // out of the ecliptic: 5 degrees above the Sun's direction is still "new" but the angle at the Earth is 5 degrees, so a sliver (not 0) is lit
    const MoonPhase q = moonPhase(Vec3{3.844e8 * std::cos(5 * units::kPi / 180), 0, 3.844e8 * std::sin(5 * units::kPi / 180)}, sun);
    const double qa = 5 * units::kPi / 180, qEM = 3.844e8, qES = 1.496e11, qMS = std::sqrt(qEM * qEM + qES * qES - 2 * qEM * qES * std::cos(qa));   // (the Moon's vector there has the Moon's length)
    check("phase: 5 degrees above the Sun's direction: new moon, elongation 5 degrees, the exact share", std::string(q.name) == "new moon" && std::fabs(q.elongationDeg - 5) < 1e-9 && std::fabs(q.fraction - exactLitShare(qEM, qES, qMS)) < 1e-9);
}

// A system in Real mode at a Julian date (TDB, used as the lab does).
static void loadReal(SolarSystem& s, double jd) { s.loadAt(unixMsOf(jd)); }

static int earthIndex(const SolarSystem& s) {
    for (std::size_t i = 0; i < s.bodies.size(); i++) if (s.bodies[i].name == "Earth") return static_cast<int>(i);
    return -1;
}

static void testEarthAndMoonAgainstHorizons() {
    for (int gr = 0; gr < 2; gr++) {
        SolarSystem sys;
        sys.setRelativity(gr == 1);
        sys.setRealPlanets(true);
        NBodyClock clk(kNBodyStep, gr == 1);
        double worstEarth = 0, worstPlanet[8] = {0}, worstMoonVec = 0, worstBary = 0, worstEmb = 0, sumEarth = 0;
        double offMin = 1e99, offMax = 0;
        bool realMoonEverywhere = true;
        for (int r = 0; r < kRows; r++) {
            const double jd = kEarthTable[r][0];
            loadReal(sys, jd);
            const int ie = earthIndex(sys);
            realMoonEverywhere = realMoonEverywhere && sys.realMoon();
            // Earth against Horizons' Earth
            const double eEarth = length(sys.bodies[ie].pos - v3(&kEarthTable[r][1]));
            worstEarth = std::max(worstEarth, eEarth); sumEarth += eEarth;
            // the other seven planets against the 0.10.3 table (planet minus Sun)
            for (int i = 0; i < 8; i++) {
                if (i == 2) continue;
                int bi = -1;
                for (std::size_t k = 0; k < sys.bodies.size(); k++) if (sys.bodies[k].name == kPlanets[i].name) bi = static_cast<int>(k);
                const Vec3 ref = v3(&kJplTable[i + 1][r][1]) - v3(&kJplTable[0][r][1]);
                worstPlanet[i] = std::max(worstPlanet[i], length(sys.bodies[bi].pos - ref));
            }
            // the Moon seen from Earth is the ELP vector; Earth and Moon balance on the N-body Earth-Moon barycentre
            const Vec3 g = moonGeocentricElp(jd);
            const Vec3 moonSeen = sys.moonPosition() - sys.bodies[ie].pos;
            worstMoonVec = std::max(worstMoonVec, length(moonSeen - g));
            Vec3 x[kNBodies], v[kNBodies];
            clk.stateAt((jd - kNBodyRefJD) * 86400.0, x, v);
            const Vec3 emb = x[3] - x[0];
            const double mu = kRealGMMoon / kRealGMEarthAlone;
            worstBary = std::max(worstBary, length((sys.bodies[ie].pos + sys.moonPosition() * mu) * (1 / (1 + mu)) - emb));
            worstEmb = std::max(worstEmb, length(sys.embPosition() - emb));
            const double off = length(sys.bodies[ie].pos - emb);
            offMin = std::min(offMin, off); offMax = std::max(offMax, off);
        }
        const char* tag = gr ? "GR on" : "GR off";
        std::printf("earth (%s): worst %.1f km, mean %.1f km against Horizons' Earth over 1800..2100 (301 dates)\n", tag, worstEarth / 1e3, sumEarth / kRows / 1e3);
        std::printf("earth (%s): Earth is %.0f to %.0f km from the Earth-Moon barycentre; Moon vector difference %.2e m; barycentre difference %.2e m; EMB difference %.2e m\n",
                    tag, offMin / 1e3, offMax / 1e3, worstMoonVec, worstBary, worstEmb);
        check(std::string("moon: real Moon at all 301 dates (1800..2100) (") + tag + ")", realMoonEverywhere);
        // (b) the Moon seen from Earth is the ELP vector: heliocentric numbers are about 1.5e11 m, so double rounding is about 3e-5 m; limit 1e-3 m
        check(std::string("moon: the Moon seen from Earth equals the ELP vector within 1e-3 m (") + tag + ")", worstMoonVec < 1e-3);
        check(std::string("moon: Earth and Moon balance on the N-body Earth-Moon barycentre within 1e-3 m (") + tag + ")", worstBary < 1e-3);
        check(std::string("moon: the stored barycentre is the N-body one within 1e-6 m (") + tag + ")", worstEmb < 1e-6);
        // Earth is off the barycentre by mu / (1 + mu) of the Earth-Moon distance: with 356,000 .. 407,000 km (the known extremes) that is 4,300 .. 5,000 km
        check(std::string("moon: Earth is 4,300 to 5,000 km from the barycentre (") + tag + ")", offMin > 4.3e6 && offMax < 5.0e6);
        // (a) Earth against Horizons: regression guard about 1 % above the measurement (GR off / on), set after the first run
        const double guard = gr ? kGuardEarthKmOn : kGuardEarthKmOff;
        check(std::string("earth: worst error against Horizons' Earth within the regression guard (") + tag + ")", worstEarth / 1e3 < guard);
        // the number the app's Real note quotes for Earth (real_errors.h) is Earth's own measured maximum + 0.2 km, rounded up: the same rule and check as tests/test_jpl.cpp
        // (0.35: was an exact ceil, which the Windows maths library, up to 0.17 km different, could break on a value just above a whole km)
        {
            const double q = double(gr ? kRealErrKmRelOn[2] : kRealErrKmRelOff[2]), w = worstEarth / 1e3;
            check(std::string("earth: the quoted worst error for Earth in real_errors.h is Earth's measured maximum (+0.2 km, rounded up) (") + tag + ")", q >= w && q - w < 1.5);
        }
        // (e) the other seven planets: the 0.10.3 measured worst errors (real_errors.h, rounded up) still hold
        bool same = true;
        for (int i = 0; i < 8; i++) {
            if (i == 2) continue;
            const long lim = gr ? kRealErrKmRelOn[i] : kRealErrKmRelOff[i];
            std::printf("planet (%s): %-8s worst %.1f km (0.10.3 limit %ld km)\n", tag, kPlanets[i].name, worstPlanet[i] / 1e3, lim);
            same = same && worstPlanet[i] / 1e3 <= lim;
        }
        check(std::string("planets: no planet but Earth is worse than its 0.10.3 worst error (") + tag + ")", same);
    }
}

static void testPhasesInSystem() {
    SolarSystem sys;
    sys.setRealPlanets(true);
    // the Horizons full moon (stage 1: JD 2461043.91990 TDB), new moon 2461029.57308 and first quarter 2461037.29970 of the same lunar cycle (stage 1 ELP crossings, equal to Horizons' to 0.02 min)
    struct Case { double jd; const char* name; double lo, hi; } cases[] = {
        {2461043.91990, "full moon", 0.99, 1.0}, {2461029.57308, "new moon", 0.0, 0.01}, {2461037.29970, "first quarter", 0.45, 0.55}, {2461051.15990, "last quarter", 0.45, 0.55}};
    for (const Case& c : cases) {
        loadReal(sys, c.jd);
        const int ie = earthIndex(sys);
        const MoonPhase p = moonPhase(sys.moonGeocentric(), sys.bodies[ie].pos * -1.0);
        std::printf("phase: JD %.5f: %s, %.2f%% lit, elongation %.2f degrees\n", c.jd, p.name, 100 * p.fraction, p.elongationDeg);
        check(std::string("phase: ") + c.name + " at its Horizons time: right name", std::string(p.name) == c.name);
        check(std::string("phase: ") + c.name + ": fraction in its range", p.fraction >= c.lo && p.fraction <= c.hi);
    }
    // the same full moon from Horizons' own Moon and Sun vectors (hourly rows of 2026-01-03, nearest hour 10:00 = JD 2461043.91667 is within 0.05 days): full moon, fraction near 1
    {
        const double* best = nullptr;
        for (int r = 121; r < 170; r++) if (!best || std::fabs(kMoonTable[r][1] - 2461043.91990) < std::fabs(best[1] - 2461043.91990)) best = kMoonTable[r];
        const MoonPhase p = moonPhase(v3(best + 2), v3(best + 5));
        std::printf("phase: Horizons' own vectors at JD %.5f: %s, %.2f%% lit\n", best[1], p.name, 100 * p.fraction);
        check("phase: Horizons' own Moon and Sun at the full moon also give 'full moon' and a fraction above 0.99", std::string(p.name) == "full moon" && p.fraction > 0.99);
    }
    // one whole cycle, sampled daily from the new moon: every name appears, in order, none skipped, and the lit fraction rises to near 1 and falls
    int idx = 0, steps = 0, last = -1, jumps = 0;
    double maxF = 0, fAtEnd = 1;
    bool seen[8] = {false}, backwards = false;
    for (double jd = 2461029.57308; jd < 2461029.57308 + 29.5; jd += 0.25) {
        loadReal(sys, jd);
        const int ie = earthIndex(sys);
        const MoonPhase p = moonPhase(sys.moonGeocentric(), sys.bodies[ie].pos * -1.0);
        idx = p.index; seen[idx] = true;
        if (last >= 0 && idx != last) { jumps++; if (idx != (last + 1) % 8) backwards = true; }
        last = idx; maxF = std::max(maxF, p.fraction); fAtEnd = p.fraction; steps++;
    }
    bool all = true;
    for (bool s : seen) all = all && s;
    std::printf("phase: one cycle from the new moon in %d samples: %d changes of name, brightest %.3f, last sample %.3f lit\n", steps, jumps, maxF, fAtEnd);
    check("phase: over one lunar cycle all 8 names appear", all);
    check("phase: the names come in order, each followed by the next, with none skipped", !backwards && jumps >= 8 && jumps <= 9);
    check("phase: the lit fraction rises above 0.99 and the cycle ends dark again (below 0.05)", maxF > 0.99 && fAtEnd < 0.05);
}

static void testFallbackAndSimple() {
    // Simple (0.46.3, outside review: the Moon's real place does not depend on the planet physics): the real Moon round Earth, and Earth stays the Kepler planet (the barycentre)
    SolarSystem simple;
    simple.loadAt(unixMsOf(2461042.0));
    check("simple: the real Moon (ELP2000-82B at the date shown, in TDB)", simple.realMoon() && length(simple.moonGeocentric() - moonGeocentricElp(2461042.0)) < 1e-3);
    // 0.52.1 (deliberate change; until 0.52.0 Simple drew Earth at the barycentre itself, and this check asked for that): Earth and the Moon on the barycentre's two sides,
    // by the lab's own mass ratio mu = GM_Moon / GM_Earth (nbody.h): the barycentre of the drawn pair is the orbit point, Earth's offset d mu / (1 + mu)
    {
        const double mu = kRealGMMoon / kRealGMEarthAlone;
        const Vec3 E = simple.bodies[earthIndex(simple)].pos, M = simple.moonPosition(), B = simple.embPosition();
        const double d = length(M - E);
        check("simple: Earth is drawn off the barycentre (earthOffBary)", simple.earthOffBary());
        check("simple: the barycentre of the drawn Earth and Moon is the orbit point (within 1 m)", length((E + M * mu) * (1 / (1 + mu)) - B) < 1);
        check("simple: Earth's offset from the orbit point is d m_Moon / (m_Earth + m_Moon) (within 1 m; about 4,700 km)", std::fabs(length(E - B) - d * mu / (1 + mu)) < 1 && length(E - B) > 4.0e6);
        // the shift is only drawn: after about 10 days of Simple steps the barycentre is where a fresh load at that date puts it (the leapfrog's state was not moved)
        for (int k = 0; k < 10; k++) simple.step(1.0 / 60, 0);
        SolarSystem fresh; fresh.loadAt(simple.currentUnixMs());
        const double drift = length(simple.embPosition() - fresh.embPosition());
        double marsDrift = 0;
        for (std::size_t i = 0; i < simple.bodies.size(); i++) if (simple.bodies[i].name == "Mars") marsDrift = length(simple.bodies[i].pos - fresh.bodies[i].pos);
        std::printf("simple: after 10 steps (about 10 days) against a fresh load at that date: the barycentre %.1f km apart (Mars, with no shift: %.1f km)\n", drift / 1000, marsDrift / 1000);
        // (the leapfrog's own error against the elements is about 100 km here; a drawn shift left in the state would put Earth's orbit some 4,700 km off at once)
        check("simple: stepping keeps the barycentre's own orbit (within 1,000 km of the elements after 10 days; Earth's offset is 4,700 km)", drift < 1.0e6);
        check("simple: after stepping, Earth is again d mu / (1 + mu) from the barycentre", std::fabs(length(simple.bodies[earthIndex(simple)].pos - simple.embPosition()) -
                                                                                                   length(simple.moonPosition() - simple.bodies[earthIndex(simple)].pos) * mu / (1 + mu)) < 1);
    }
    // Real, outside the span of ELP (before 1800 / after 2100): no ELP Moon, the card says so. 0.96.0 (part E): the Moon is on the ellipse fitted to ELP (moon_elp.h), so Earth is
    // drawn off the barycentre by mu / (1 + mu) of that vector and a phase is shown (until 0.95.0 these two checks said Earth is exactly the barycentre and no phase is shown)
    SolarSystem sys;
    sys.setRealPlanets(true);
    loadReal(sys, 2378497.0 - 400);
    const int ie = earthIndex(sys);
    const double muAll = kRealGMMoon / kRealGMEarthAlone;
    check("fallback: before 1800-01-01 12:00 the Moon is not the real one", !sys.realMoon());
    check("fallback: ... the Moon is on the fitted ellipse and Earth is mu / (1 + mu) of its vector from the N-body barycentre, on the far side",
          sys.moonPlaced() && muAll > 0 && length(sys.moonGeocentric() - moonGeocentricEllipse(sys.julianDayTDB())) < 1.0 &&
          length(sys.bodies[ie].pos - (sys.embPosition() - sys.moonGeocentric() * (muAll / (1 + muAll)))) < 1.0);
    CardOptions opt;
    Card c;
    Selection sel{Selection::Moon, sys.bodies[ie].id, 0};
    check("fallback: the Moon card exists", cardText(sys, sel, opt, c));
    std::string all;
    for (auto& l : c.lines) all += l + "\n";
    check("fallback: the card says the real Moon is not used outside 1800 to 1 Jan 2100", all.find("Outside 1800 to 1 Jan 2100") != std::string::npos);
    check("fallback: a phase is shown, from the fitted ellipse, and the card says it is only roughly right", all.find("Phase now") != std::string::npos && all.find("roughly right") != std::string::npos);
    loadReal(sys, 2461043.91990);
    Card c2;
    check("card: real Moon card exists", cardText(sys, Selection{Selection::Moon, sys.bodies[earthIndex(sys)].id, 0}, opt, c2));
    all.clear();
    for (auto& l : c2.lines) all += l + "\n";
    check("card: the real Moon card names the phase (full moon at the Horizons full moon) and the percent lit", all.find("Phase now: full moon, 100% lit") != std::string::npos || all.find("Phase now: full moon, 99% lit") != std::string::npos);
    check("card: ... names ELP2000-82B", all.find("ELP2000-82B") != std::string::npos);
    {   // the lines are wrapped, so look at them as one text
        std::string one = all; for (char& ch : one) if (ch == '\n') ch = ' ';
        check("card: ... says the four main phases are exact and the in-between phase names use this app's own cut-offs (2, 40, 60, 98 %)", one.find("The four main phases (new, first quarter, full, last quarter) are exact") != std::string::npos && one.find("(crescent, gibbous) use this app’s own cut-offs: 2%, 40%, 60% and 98% lit") != std::string::npos);
    }
    // (0.75.2, on purpose: the card gives the exact lit share from the phase angle at the Moon, no longer the first-order one)
    check("card: ... says the lit part is (1 + cos i) / 2 with the phase angle at the Moon (exact)", all.find("(1 + cos i) / 2") != std::string::npos && all.find("first order") == std::string::npos);
    // the card of another moon (Jupiter's Io) is not touched
    int ij = -1;
    for (std::size_t i = 0; i < sys.bodies.size(); i++) if (sys.bodies[i].name == "Jupiter") ij = static_cast<int>(i);
    Card c3;
    cardText(sys, Selection{Selection::Moon, sys.bodies[ij].id, 0}, opt, c3);
    std::string io;
    for (auto& l : c3.lines) io += l + "\n";
    check("other moons: Io's card has no phase and no ELP", io.find("Phase now") == std::string::npos && io.find("ELP2000") == std::string::npos);
    // Simple: Earth's Moon card is the old one
    Card c4;
    simple.loadAt(unixMsOf(2461042.0));
    cardText(simple, Selection{Selection::Moon, simple.bodies[earthIndex(simple)].id, 0}, opt, c4);
    std::string sm;
    for (auto& l : c4.lines) sm += l + "\n";
    check("simple: Earth's Moon card has the real phase and names ELP2000-82B (0.46.3)", sm.find("Phase now") != std::string::npos && sm.find("ELP2000") != std::string::npos && sm.find("made-up angle") == std::string::npos);
}

static void testFastSpeed() {
    // The Moon goes round 13 times a year. The Time speed slider: 1 real second = kSimTime x 10^tspeed simulated seconds (kSimTime = 58.13 days). The drawing lights the whole Moon orbit instead of
    // drawing the Moon when it goes round more than 8 times a second (as for every moon); then Earth is held at the Earth-Moon barycentre too (its 4,300-5,000 km wobble would only flicker).
    SolarSystem sys;
    sys.setRealPlanets(true);
    loadReal(sys, 2461042.0);
    const int ie = earthIndex(sys);
    // slow: 1 lap per second at tspeed = -0.3 (58.13 d x 0.501 / 27.32 d = 1.07 laps a second)
    sys.step(units::kPhysicsStep, -0.3);
    double off = length(sys.bodies[ie].pos - sys.embPosition());
    check("speed: slow (about 1 lap a second): Earth is off the barycentre by 4,300-5,000 km", off > 4.3e6 && off < 5.0e6 && !sys.moonFast());
    // fast: tspeed = 1 -> 58.13 x 10 / 27.32 = 21 laps a second
    sys.step(units::kPhysicsStep, 1.0);
    off = length(sys.bodies[ie].pos - sys.embPosition());
    check("speed: fast (21 laps a second): the Moon is flagged fast and Earth sits at the barycentre", sys.moonFast() && off == 0);
    // the real Moon vector is still there (the view hides the Moon and lights the orbit)
    check("speed: fast: the real Moon vector is still the ELP one at the date shown", sys.realMoon() && length(sys.moonGeocentric() - moonGeocentricElp(sys.julianDayTDB())) < 1e-3);
    // slowing down again brings the wobble back
    sys.step(units::kPhysicsStep, -0.3);
    off = length(sys.bodies[ie].pos - sys.embPosition());
    check("speed: slow again: the offset is back", off > 4.3e6 && off < 5.0e6 && !sys.moonFast());
    // exactly at the limit (8 laps a second) is not yet 'fast': the same rule as the drawing's (laps a second > 8)
    const double tsLimit = std::log10(8.0 * 27.3217 / (units::kSimTime / units::kDay));
    sys.step(units::kPhysicsStep, tsLimit - 1e-6);
    check("speed: just under 8 laps a second is not fast", !sys.moonFast());
    sys.step(units::kPhysicsStep, tsLimit + 1e-6);
    check("speed: just over 8 laps a second is fast", sys.moonFast());
    // the date moves with the speed: after a large step the Moon vector follows it (a month at 1 lap a second -> another place)
    const Vec3 g0 = sys.moonGeocentric();
    sys.step(units::kPhysicsStep * 30, -0.3);
    check("speed: the Moon vector follows the date", length(sys.moonGeocentric() - g0) > 1e6 && length(sys.moonGeocentric() - moonGeocentricElp(sys.julianDayTDB())) < 1e-3);
}

int main() {
    testConstants();
    testPhaseGeometry();
    testEarthAndMoonAgainstHorizons();
    testPhasesInSystem();
    testFallbackAndSimple();
    testFastSpeed();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
