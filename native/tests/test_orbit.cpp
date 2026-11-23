// Unit tests for the Orbit Lab physics: against NASA/JPL Horizons (the famous dates, the equinoxes, the comets), Kepler's equation and laws,
// conservation of energy, and the text rules.
//
// Run:  ctest --test-dir build   (or run the orbit_tests program directly)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <cstdio>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include "orbit/alphacen.h"
#include "orbit/belts.h"
#include "orbit/card_text.h"
#include "orbit/date_text.h"
#include "orbit/format.h"
#include "orbit/kepler.h"
#include "orbit/label_clip.h"
#include "orbit/real_errors.h"
#include "orbit/giant_rings.h"
#include "orbit/saturn_rings.h"
#include "orbit/saturn_profile.h"
#include "orbit/shapes.h"
#include "orbit/moon_sats.h"
#include "orbit/moon_orientation.h"
#include "orbit/iau_rotation.h"
#include "render/ellipsoid.h"
#include "render/shading.h"
#include "render/shaders.h"
#include "orbit/sky.h"
#include "orbit/spin.h"
#include "orbit/sun_facts.h"
#include "orbit/time_scale.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"
#include "render/planet_maps.h"
namespace ref { constexpr long long kEpochMs = 1767225600000LL; }   // 1 Jan 2026 00:00 UTC, the lab's starting date (the JPL Horizons epoch of data/jpl)
#include "reference/events_table.inc"   // 0.10.5 stage 5: Horizons vectors of the famous dates and the 2026 equinoxes (tools/make_events_ref.py)
#include "reference/comet_table.inc"   // 0.10.5 stage 3: the comets' records and Horizons vectors (made by tools/make_comet_ref.py)

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

static void runSteps(SolarSystem& sys, double tspeed, int steps) {
    for (int k = 0; k < steps; k++) sys.step(units::kPhysicsStep, tspeed);
}

#include "rock_reference.h"
#include "test_rocks.inc"   // 0.13 stage 4 part A: the belt rocks against Kepler's equation as the textbook has it
#include "test_camera.inc"   // 0.75.0: the 3D pinhole camera against pinhole geometry
#include "test_rings075.inc"   // 0.75.1: Saturn's rings, each part at its own viewing slant
#include "test_review0752.inc"   // 0.75.2: the outside review's corrections (gravity and spin, Simple running vs a jump, orbit numbers, rings of the other giants ...)

// 0.15 stage 5c, bug fix A: which labels the picture may draw under the bars. WRITTEN BEFORE the code. References: plain geometry (a rectangle is clear when it lies wholly inside the free area)
// and the owner's screenshot: at a far zoom-out the ring label "6,500,000,000 AU · 103,000 light years" sat on the breadcrumb in the top bar.
static void testLabelClear() {
    using namespace orbit;
    const double W = 1280, H = 800;
    const LabelInsets in{48, 60, 340};                                       // a top bar of 44 + 4, the toolbar band, the Controls panel
    const LabelBox b = labelBox(500, 300, 100, 10, 0.5);                      // a centred 100-wide, 10-high label
    near(b.x0, 450, 1, 1e-12, "label box: centred on x"); near(b.x1, 550, 1, 1e-12, "label box: 100 wide");
    check(b.y0 < 300 && b.y1 > 300 && b.y1 - b.y0 > 10 && b.y1 - b.y0 < 15, "label box: reaches above and a little below the baseline, about one size high");
    check(labelBox(500, 300, 100, 10, 0).x0 == 500, "label box: align 0 starts at x");
    check(labelClear(labelBox(500, 300, 100, 10, 0.5), W, H, in), "a label in the middle is drawn");
    check(!labelClear(labelBox(500, 50, 100, 10, 0.5), W, H, in), "a label whose top reaches into the top bar band is skipped");
    check(!labelClear(labelBox(500, 20, 100, 10, 0.5), W, H, in), "a label inside the top bar is skipped");
    check(labelClear(labelBox(500, 48 + 0.95 * 10 + 0.01, 100, 10, 0.5), W, H, in), "a label just under the band is drawn");
    check(!labelClear(labelBox(500, 745, 100, 10, 0.5), W, H, in), "a label reaching into the toolbar band is skipped");
    check(!labelClear(labelBox(W - 340 - 30, 300, 100, 10, 0.5), W, H, in), "a label reaching under the Controls panel is skipped");
    check(labelClear(labelBox(W - 340 - 60, 300, 100, 10, 0.5), W, H, in), "a label just left of the Controls panel is drawn");
    check(labelClear(labelBox(W - 20, 300, 100, 10, 0.5), W, H, LabelInsets{}), "no insets (the old layout): a label at the window's edge is drawn as before");
    check(labelClear(labelBox(500, 5, 100, 10, 0.5), W, H, LabelInsets{0, 0, 0}) && labelClear(labelBox(500, H, 100, 10, 0.5), W, H, LabelInsets{0, 0, 0}), "no insets: nothing is skipped, even half off the window");
    // the far zoom-out case: rings of 55 px steps out to the corner, labelled at the angle -0.8 rad about the Sun at the window's middle, the widest label ("6,500,000,000 AU · 103,000 light years", about 215 px at size 10)
    int drawn = 0, skipped = 0;
    for (double centreY : {400.0, 60.0, 30.0}) {                              // the Sun in the middle, near the top bar, and under it
        for (int k = 1; k * 55.0 <= std::hypot(W, H); k++) {
            const double R = k * 55.0, x = W / 2 + R * std::cos(-0.8), y = centreY + R * std::sin(-0.8) + 3;
            const LabelBox lb = labelBox(x, y, 215, 10, 0.5);
            if (labelClear(lb, W, H, in)) {
                drawn++;
                check(lb.y0 >= in.top && lb.y1 <= H - in.bottom && lb.x1 <= W - in.right, "far zoom-out: a ring label that is drawn is inside the free area");
            } else skipped++;
        }
    }
    check(drawn > 0 && skipped > 0, "far zoom-out: some ring labels are drawn and some are skipped (the ones in the bands)");
}


// 0.19 audit: the number-formatting functions with extreme and random input. No crash and no undefined behaviour (the unit tests also run under
// -fsanitize=address,undefined), and the standard words for NaN and Infinity (toFixed, toLocaleString('en'), toPrecision, toExponential; ECMA-262 / ECMA-402).
static void testFormatFuzz() {
    const double inf = std::numeric_limits<double>::infinity(), nan = std::numeric_limits<double>::quiet_NaN();
    check(fmt::fixed(nan, 2) == "NaN", "fixed(NaN, 2) = " + fmt::fixed(nan, 2));
    check(fmt::fixed(-inf, 1) == "-Infinity", "fixed(-Infinity, 1) = " + fmt::fixed(-inf, 1));
    check(fmt::localeNumber(inf) == "∞" && fmt::localeNumber(-inf) == "-∞", "localeNumber(±Infinity) = " + fmt::localeNumber(inf) + " " + fmt::localeNumber(-inf));
    check(fmt::toPrecision(inf, 3) == "Infinity" && fmt::toPrecision(nan, 3) == "NaN", "toPrecision(Infinity / NaN)");
    check(fmt::toExponential(-inf, 2) == "-Infinity" && fmt::toExponential(nan, 2) == "NaN", "toExponential(-Infinity / NaN)");
    check(fmt::commas(nan) == "NaN" && fmt::commas(inf) == "∞" && fmt::commas(-inf) == "-∞", "commas(NaN / ±Infinity) = " + fmt::commas(nan) + " " + fmt::commas(inf));
    check(fmt::commas(1e30) == "1,000,000,000,000,000,000,000,000,000,000", "commas(1e30) = " + fmt::commas(1e30));
    check(fmt::commas(-1e19) == "-10,000,000,000,000,000,000", "commas(-1e19) = " + fmt::commas(-1e19));
    check(fmt::plainNumber(1e300) == "1e+300", "plainNumber(1e300) = " + fmt::plainNumber(1e300));
    check(fmt::plainNumber(1e-100) == "0." + std::string(99, '0') + "1", "plainNumber(1e-100) = " + fmt::plainNumber(1e-100));
    check(fmt::kg(0) == "0 kg" && fmt::kg(nan) == "NaN kg", "kg(0) = " + fmt::kg(0) + ", kg(NaN) = " + fmt::kg(nan));
    check(fmt::superscript(std::numeric_limits<int>::min()) == "⁻²¹⁴⁷⁴⁸³⁶⁴⁸", "superscript(INT_MIN) = " + fmt::superscript(std::numeric_limits<int>::min()));
    check(fmt::simDate(ref::kEpochMs, -1e12) == "the year -999,999,997,974", "simDate 10^12 years back = " + fmt::simDate(ref::kEpochMs, -1e12));
    // random doubles from random bit patterns (every exponent, NaN and Infinity included): every function runs; the finite ones read back
    uint64_t st = 0x9E3779B97F4A7C15ull;
    const auto next = [&st] { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; };
    int bad = 0, shown = 0;
    for (int i = 0; i < 200000; i++) {
        const int bad0 = bad;
        double x; const uint64_t b = next(); std::memcpy(&x, &b, sizeof x);
        if (i % 2) x = std::ldexp(static_cast<double>(next() >> 11) / 9007199254740992.0 - 0.5, static_cast<int>(next() % 140) - 70);   // half of them in a useful range
        const int d = static_cast<int>(next() % 7);
        const std::string f = fmt::fixed(x, d), c = fmt::commas(x), l = fmt::localeNumber(x, d), p = fmt::toPrecision(x, d + 1), e = fmt::toExponential(x, d), j = fmt::shortestNumber(x), pn = fmt::plainNumber(x);
        (void)fmt::kg(x); (void)fmt::years(x); (void)fmt::au(x); (void)fmt::precision(x, d + 1);
        if (!std::isfinite(x)) continue;
        const auto num = [](std::string s) { s.erase(std::remove(s.begin(), s.end(), ','), s.end()); return std::strtod(s.c_str(), nullptr); };
        if (std::strtod(j.c_str(), nullptr) != x) bad++;                                                         // String(x) is exact
        if (std::fabs(x) < 1e21 && std::fabs(num(f) - x) > 0.5 * std::pow(10.0, -d) * (1 + 1e-9) + std::fabs(x) * 1e-15) bad++;   // toFixed: within half a unit of the last digit
        if (std::fabs(num(c) - x) > 0.5 + std::fabs(x) * 1e-15) bad++;                                         // rounded to a whole number
        if (std::fabs(num(l) - x) > 0.5 * std::pow(10.0, -d) * (1 + 1e-9) + std::fabs(x) * 1e-15) bad++;        // at most d decimals
        if (x != 0 && std::isfinite(num(p)) && std::fabs(num(p) - x) > std::fabs(x) * 0.5 * std::pow(10.0, -d) * (1 + 1e-9)) bad++;   // d + 1 significant digits (rounded up past
        if (x != 0 && std::isfinite(num(e)) && std::fabs(num(e) - x) > std::fabs(x) * 0.5 * std::pow(10.0, -d) * (1 + 1e-9)) bad++;   // the largest double, "2e+308" reads back as Infinity)
        if (num(pn) != x) bad++;                                                                                 // the shortest form reads back exactly
        if (bad > bad0 && shown++ < 3) std::printf("  format fuzz: x=%.17g d=%d fixed=%s commas=%s locale=%s prec=%s exp=%s str=%s plain=%s\n", x, d, f.c_str(), c.c_str(), l.c_str(), p.c_str(), e.c_str(), j.c_str(), pn.c_str());
    }
    check(bad == 0, "format fuzz: 200,000 random numbers read back within their rounding (" + std::to_string(bad) + " wrong)");
}

// 0.23 stage 2: the orbit paths drawn in the picture. References: the JPL "Keplerian elements for approximate positions of the major planets" table
// (a0 in AU and its rate per century, typed again here from the JPL page, not read from solar_data.cpp), and the ellipse's own geometry.
static void testOrbitPath() {
    SolarSystem s; s.loadAt(ref::kEpochMs);
    const double jd = ref::kEpochMs / 86400000.0 + 2440587.5, T = (jd - 2451545.0) / 36525;
    const struct { const char* name; double a0, a1; } jpl[] = {{"Mercury", 0.38709927, 0.00000037}, {"Earth", 1.00000261, 0.00000562}, {"Jupiter", 5.20288700, -0.00011607}, {"Neptune", 30.06992276, 0.00026291}};
    for (const auto& j : jpl) {
        const Body* b = nullptr;
        for (const Body& x : s.bodies) if (x.name == j.name) b = &x;
        check(b != nullptr, std::string("orbit path: ") + j.name + " found");
        if (!b) continue;
        std::vector<Vec3> path;
        orbitPath(*b, 360, path);
        check(path.size() == 361 && length(path.front() - path.back()) < 1e-6 * length(b->pos), std::string(j.name) + ": 361 points, closed");
        double rmin = 1e300, rmax = 0, dmin = 1e300, off = 0;
        const Vec3 h = cross(b->pos - b->drawShift, b->vel), w = h * (1 / length(h));   // (0.52.1: the orbit's point; Earth itself is drawn off it)
        for (std::size_t i = 0; i + 1 < path.size(); i++) {
            const Vec3 p = path[i], q = path[i + 1];
            rmin = std::min(rmin, length(p)); rmax = std::max(rmax, length(p)); off = std::max(off, std::fabs(dot(p, w)));
            const Vec3 bp = b->pos - b->drawShift;                                                // 0.52.1: the orbit's point (Earth: the barycentre)
            const Vec3 d = q - p; const double t = std::clamp(dot(bp - p, d) / dot(d, d), 0.0, 1.0);   // the distance of the body from this piece
            dmin = std::min(dmin, length(p + d * t - bp));
        }
        const double aWant = (j.a0 + j.a1 * T) * units::kAU, aPath = (rmin + rmax) / 2;
        check(std::fabs(aPath / aWant - 1) < 2e-5, std::string(j.name) + ": the path's semi-major axis is JPL's a(t) (" + std::to_string(aPath / aWant - 1) + ")");
        check(dmin < 2e-3 * aWant, std::string(j.name) + ": the path goes through the planet (" + std::to_string(dmin / aWant) + " a)");
        check(off < 1e-9 * aWant, std::string(j.name) + ": the path lies in the orbit plane");
        const OrbitShape o = orbitOf(*b);
        check(std::fabs(rmin / o.rPeri - 1) < 1e-4 && std::fabs(rmax / o.rApo - 1) < 1e-4, std::string(j.name) + ": from perihelion to aphelion");
    }
    Body escape; escape.pos = {units::kAU, 0, 0}; escape.vel = {0, 1e5, 0};
    std::vector<Vec3> none;
    orbitPath(escape, 90, none);
    check(none.empty(), "an unbound path is not drawn");
}

// 0.26: Saturn's rings against NASA's Saturnian Rings Fact Sheet (19 April 2022). The km edges divided by Saturn's
// 60,268 km must give the sheet's own "Radius/Eq. radius" column (a second column of the source, to 3 decimals);
// the rings must touch (C|B|Cassini|A); and the light a ring stops is Beer-Lambert: 1 - exp(-tau / mu).
static void testSaturnRings() {
    using namespace orbit::rings;
    const double S = kSaturnEqRadiusKm;
    const struct { double km, ratio; } col[] = {{kRings[0].innerKm, 1.239}, {kRings[0].outerKm, 1.526}, {kRings[1].outerKm, 1.950},
                                                {kRings[3].innerKm, 2.030}, {kRings[3].outerKm, 2.270}, {kFRingKm, 2.320}};
    for (const auto& c : col) check(std::fabs(c.km / S - c.ratio) <= 0.0006, "Saturn ring edge " + std::to_string(c.km) + " km / 60,268 km = the fact sheet's " + std::to_string(c.ratio));
    for (int i = 0; i + 1 < 4; i++) check(kRings[i].outerKm == kRings[i + 1].innerKm, std::string("Saturn: ") + kRings[i].name + " ends where " + kRings[i + 1].name + " begins");
    check(std::fabs(blocked(1, 1) - (1 - std::exp(-1.0))) < 1e-12, "a ring of optical depth 1 seen face-on stops 1 - 1/e of the light");
    check(std::fabs(blocked(0.5, 0.5) - blocked(1, 1)) < 1e-12, "seen at 30 degrees (mu 0.5) the path is twice as deep");
    check(blocked(0, 0.3) == 0 && blocked(2.5, 1e-6) > 0.999, "no ring stops nothing; edge-on a ring is opaque");
    // the ring light (single scattering in a slab): an opaque ring face-on in full sunlight is 1 (the reference); a thin ring's light is
    // tau / mu for small tau on both faces (the slab law's first order); an opaque ring sends nothing through to its dark face; and
    // with the Sun in the ring plane (mu0 -> 0, the equinox) a ring sends almost nothing
    check(std::fabs(shine(50, 1, 1, true) - 1) < 1e-9, "an opaque ring face-on in full sunlight is the full tint");
    check(std::fabs(shine(1e-4, 0.5, 0.3, true) / (1e-4 / 0.5 / 0.5) - 1) < 1e-3 && std::fabs(shine(1e-4, 0.5, 0.3, false) / (1e-4 / 0.5 / 0.5) - 1) < 1e-3,
          "a thin ring: light = tau / mu (over the reference 0.5) on both faces, the slab law's first order");
    check(shine(50, 0.5, 0.3, false) < 1e-9 && shine(1.45, 0.5, 1e-3, true) < 0.01, "an opaque ring is dark from behind, and every ring is dark at equinox");
    // the other giants' rings against NASA's ring fact sheets' own "Radius/Eq. radius" column (Jupiter's main ring 1.72-1.80, Uranus's
    // alpha and lambda rings 1.750 and 1.957, Neptune's Adams ring 2.541)
    const GiantRings J = giantRings("Jupiter"), U = giantRings("Uranus"), N = giantRings("Neptune");
    check(J.bands.size() == 1 && std::fabs(J.bands[0].innerKm / J.eqRadiusKm - 1.72) < 0.006 && std::fabs(J.bands[0].outerKm / J.eqRadiusKm - 1.80) < 0.006, "Jupiter's main ring: 1.72 to 1.80 Jupiter radii");
    // (Uranus: the sheet's ratio column disagrees with its own km column for eta, delta and epsilon (1.834, 1.900, 2.006 against 47,176,
    // 48,300 and 51,149 km / 25,559 km = 1.846, 1.890, 2.001); the km values are the ones used, so the rings that agree are checked)
    check(U.bands.size() == 10 && std::fabs(0.5 * (U.bands[3].innerKm + U.bands[3].outerKm) / U.eqRadiusKm - 1.750) < 0.0006
          && std::fabs(0.5 * (U.bands[8].innerKm + U.bands[8].outerKm) / U.eqRadiusKm - 1.957) < 0.0006, "Uranus: 10 rings, alpha at 1.750 and lambda at 1.957 radii");
    check(N.bands.size() == 4 && std::fabs(0.5 * (N.bands[3].innerKm + N.bands[3].outerKm) / N.eqRadiusKm - 2.541) < 0.0006, "Neptune: the Adams ring at 2.541 radii");
    // (2026-11-05 audit) the Lassell ring against the same sheet's ratio column: from Lassell's 2.148 to Arago's 2.310 (Arago, the same 1989N4R*, is its outer edge)
    check(std::fabs(N.bands[2].innerKm / N.eqRadiusKm - 2.148) < 0.001 && std::fabs(N.bands[2].outerKm / N.eqRadiusKm - 2.310) < 0.001, "Neptune: the Lassell ring from 2.148 to 2.310 radii");
    check(giantRings("Saturn").bands.empty() && giantRings("Mars").bands.empty(), "only Jupiter, Uranus and Neptune in the giants' table");
    // 0.48 (review): one radius per planet: the rings use the planet table's equatorial radius (NSSDC fact sheets: Jupiter 71,492, Uranus 25,559, Neptune 24,764 km)
    check(J.eqRadiusKm == 71492 && U.eqRadiusKm == 25559 && N.eqRadiusKm == 24764, "the giants' rings use the NSSDC equatorial radii (Neptune 24,764 km, as the planet table)");
}

// 0.27: the belts are real asteroids (orbit/asteroid_data.inc, JPL Small-Body Database). Every rock is a record; Vesta and Pallas are where JPL Horizons
// puts them on the start date; the 3:1 Kirkwood gap at 2.50 AU (Jupiter's resonance: 2.5 = 5.2 / 3^(2/3)) shows in the data itself (the old random rocks
// had it cut in by hand); and the Trojans sit round Jupiter's L4 and L5 points, 60 degrees ahead and behind (swaying tens of degrees either way).
static void testRealRocks() {
    const auto belts = makeBelts(ref::kEpochMs);
    check(belts.size() == 4 && belts[0].key == "main" && belts[0].n == 1800 && belts[1].key == "trojan" && belts[1].n == 300 && belts[2].key == "kuiper" && belts[2].n == 900,
          "1,800 main-belt asteroids, 300 Trojans, 900 trans-Neptunian objects");
    check(std::fabs(belts[0].a[0] - 2.361365965127599) < 1e-6 && std::fabs(belts[0].e[0] - 0.09020374382834395) < 1e-7, "the first main-belt rock is Vesta (its SBDB record)");
    // Horizons, heliocentric, ecliptic J2000, 2026-01-01 00:00 TDB (API, COMMAND='4;' and '2;', CENTER='500@10', TLIST='2461041.5', read 5 Nov 2026), AU
    const Vec3 vesta{1.100732664743188, -1.905243132346087, -7.747073586010435E-02}, pallas{2.898022066032923, -1.412616234615699, 7.283539046626709E-01};
    const double dv = length(beltPoint(belts[0], 0, 0) * (1 / units::kAU) - vesta), dp = length(beltPoint(belts[0], 1, 0) * (1 / units::kAU) - pallas);
    std::printf("  Vesta %.6f AU and Pallas %.6f AU from Horizons on the start date\n", dv, dp);
    check(dv < 1e-3 && dp < 1e-3, "Vesta and Pallas within 0.001 AU (150,000 km) of Horizons on the start date");
    int gap = 0, below = 0, above = 0;
    for (int k = 0; k < belts[0].n; k++) {
        const double a = belts[0].a[k];
        if (std::fabs(a - 2.50) < 0.02) gap++;
        else if (a > 2.40 && a < 2.44) below++;
        else if (a > 2.56 && a < 2.60) above++;
    }
    std::printf("  main belt: %d rocks at 2.48-2.52 AU, %d at 2.40-2.44, %d at 2.56-2.60\n", gap, below, above);
    check(gap * 4 < std::min(below, above), "the 3:1 Kirkwood gap: under a quarter as many rocks at 2.48-2.52 AU as in either 0.04 AU band beside it");
    SolarSystem sys; sys.loadAt(ref::kEpochMs);
    const Body* jup = nullptr;
    for (const Body& b : sys.bodies) if (b.name == "Jupiter") jup = &b;
    int l4 = 0, l5 = 0, far = 0;
    double lo = 180, hi = 0;
    const double lj = std::atan2(jup->pos.y, jup->pos.x);
    for (int k = 0; k < belts[1].n; k++) {
        const Vec3 p = beltPoint(belts[1], k, 0);
        double d = (std::atan2(p.y, p.x) - lj) * 180 / units::kPi;
        d -= 360 * std::floor((d + 180) / 360);
        if (d > 10 && d < 120) l4++; else if (d < -10 && d > -120) l5++; else far++;
        lo = std::min(lo, std::fabs(d)); hi = std::max(hi, std::fabs(d));
    }
    std::printf("  Trojans: %d near L4 (+60 deg), %d near L5 (-60 deg), %d elsewhere; %.1f to %.1f deg from Jupiter\n", l4, l5, far, lo, hi);
    check(far == 0 && l4 > 50 && l5 > 50, "every Trojan is 10-120 degrees ahead of or behind Jupiter (their tadpole orbits round L4 and L5 never reach Jupiter or the far side), both camps used");
}

// 0.27: the 13 big moons at their real places. Each against JPL Horizons at 90 dates the fit never used (tests/reference/moon_sats_table.inc:
// 1 January 12:00 TDB of every 5th year 1805-2100 and daily in January 2026). The worst distance per moon must be the number the app quotes
// (moon_sats_worst.inc, rounded up), and under 6 % of the orbit's size: before 0.27 a moon could be anywhere on its orbit (up to 2 a off).
static void testMoonSats() {
    static const double kRef[][5] =
#include "reference/moon_sats_table.inc"
    ;
    const char* names[13] = {"Phobos", "Deimos", "Io", "Europa", "Ganymede", "Callisto", "Enceladus", "Rhea", "Titan", "Titania", "Oberon", "Triton", "Charon"};
    double worst[13] = {}, aKm[13] = {};
    int count[13] = {};
    for (const auto& r : kRef) {
        const int k = static_cast<int>(r[0]);
        Vec3 p, v;
        if (!satState(names[k], r[1], p, v)) { check(false, std::string(names[k]) + ": a place at JD " + std::to_string(r[1])); continue; }
        worst[k] = std::max(worst[k], length(p - Vec3{r[2], r[3], r[4]}));
        aKm[k] = std::max(aKm[k], length(Vec3{r[2], r[3], r[4]}));
        count[k]++;
    }
    for (int k = 0; k < 13; k++) {
        const double q = satWorstErrKm(names[k]);
        std::printf("  %-9s worst %9.1f km of %d dates (%.2f %% of its distance)\n", names[k], worst[k], count[k], 100 * worst[k] / aKm[k]);
        check(count[k] == 90, std::string(names[k]) + ": 90 Horizons dates");
        check(q >= worst[k] && q < worst[k] + 1, std::string(names[k]) + ": the quoted worst error (" + std::to_string(q) + " km) is the measured one, rounded up");
        check(worst[k] < 0.06 * aKm[k], std::string(names[k]) + ": within 6 % of its distance from the planet");
    }
    Vec3 p, v;
    check(!satState("Io", kSatFirstJD - 1, p, v) && !satState("Io", kSatLastJD + 1, p, v) && !satState("Moon", 2451545, p, v), "outside 1800-2100, or for our Moon, no real place");
}

// 0.27: the real shapes. The radii against the NASA fact sheets (typed again here from the sheets, read 5 Nov 2026), each sheet's own flattening
// column against (eq - polar) / eq, and the drawn outline (render/ellipsoid.h, the shader's twin) against the textbook projection of a spheroid:
// seen with its pole at angle theta from the line of sight, the outline is an ellipse with axes 1 and sqrt(cos^2 theta + q^2 sin^2 theta), q = polar / eq.
static void testShapes() {
    struct Sheet { const char* name; double eq, polar, flat, flatTol; };
    const Sheet sheets[] = {{"Mercury", 2440.5, 2438.3, 0.0009, 0.00005}, {"Venus", 6051.8, 6051.8, 0.000, 0.0005}, {"Earth", 6378.137, 6356.752, 0.003353, 0.0000005},
                            {"Mars", 3396.2, 3376.2, 0.00589, 0.000005}, {"Jupiter", 71492, 66854, 0.06487, 0.000005}, {"Saturn", 60268, 54364, 0.09796, 0.000005},
                            {"Uranus", 25559, 24973, 0.02293, 0.000005}, {"Neptune", 24764, 24341, 0.01708, 0.000005}, {"Pluto", 1188, 1188, 0.0000, 0.00005},
                            {"Moon", 1738.1, 1736.0, 0.0012, 0.00005}};
    for (const Sheet& sh : sheets) {
        const orbit::BodyShape* b = orbit::bodyShape(sh.name);
        check(b && b->eqKm == sh.eq && b->polarKm == sh.polar, std::string(sh.name) + ": equatorial and polar radius = the NASA sheet");
        check(b && std::fabs((b->eqKm - b->polarKm) / b->eqKm - sh.flat) <= sh.flatTol, std::string(sh.name) + ": (eq - polar) / eq = the sheet's flattening");
    }
    check(orbit::bodyShape("Ceres") == nullptr && orbit::polarRatio("Ceres") == 1.0, "a body without a measured shape here stays a ball");
    // K rows: the body's meridian, east, pole axes in screen coordinates (right, up, towards you). theta: the pole's angle from the line of sight.
    const auto frame = [](double theta, float K[9]) {
        const double P[3] = {0, std::sin(theta), std::cos(theta)}, E[3] = {1, 0, 0}, M[3] = {0, -std::cos(theta), std::sin(theta)};   // M = E x P
        for (int j = 0; j < 3; j++) { K[j] = float(M[j]); K[3 + j] = float(E[j]); K[6 + j] = float(P[j]); }
    };
    const auto extent = [](const float K[9], double sz, bool vertical) {   // the outline's half size along screen up or right, scanned
        double last = 0;
        for (double s = 0; s <= 1.2; s += 1e-5)
            if (render::ellipsoidHit(K, 1.0, sz, vertical ? 0 : s, vertical ? -s : 0).hit) last = s;
        return last;
    };
    for (const char* name : {"Saturn", "Jupiter", "Earth"}) {
        const double q = orbit::polarRatio(name);
        for (double thDeg : {90.0, 60.0, 30.0, 0.0}) {
            float K[9]; frame(thDeg * units::kPi / 180, K);
            const double th = thDeg * units::kPi / 180, want = std::sqrt(std::cos(th) * std::cos(th) + q * q * std::sin(th) * std::sin(th));
            near(extent(K, q, true), want, 1, 2e-4, std::string(name) + ": the drawn outline's polar half size, pole " + std::to_string(int(thDeg)) + " deg from the line of sight");
            near(extent(K, q, false), 1.0, 1, 2e-4, std::string(name) + ": and its equatorial half size is the equatorial radius");
        }
    }
    {   // Saturn side on: the middle of the disc is on the equator, the top of the outline at the north pole, and the normal there points up
        float K[9]; frame(units::kPi / 2, K);
        const double q = orbit::polarRatio("Saturn");
        const render::EllipsoidHit mid = render::ellipsoidHit(K, 1, q, 0, 0), top = render::ellipsoidHit(K, 1, q, 0, -(q - 1e-9));
        check(mid.hit && std::fabs(mid.latRad) < 1e-9 && std::fabs(mid.n[2] - 1) < 1e-9, "Saturn side on: the middle of the disc is on the equator, facing you");
        check(top.hit && std::fabs(top.latRad - units::kPi / 2) < 1e-3 && top.n[1] > 0.999, "the top of the outline is the north pole, its normal pointing up");
        // planetocentric latitude at a point halfway up: atan(z / x) of the surface point, not the normal's angle (planetographic)
        const render::EllipsoidHit h = render::ellipsoidHit(K, 1, q, 0, -0.5 * q);
        const double z = 0.5 * q, x = std::sqrt(1 - 0.25);
        check(h.hit && std::fabs(h.latRad - std::atan2(z, x)) < 1e-9, "the map latitude is planetocentric (the maps' own)");
    }
    {   // a ball (1, 1) gives the old formulas exactly: d = the distance from the middle, normal (u, -v, sqrt(1 - d^2))
        float K[9]; frame(0.7, K);
        double worst = 0;
        for (double u = -0.9; u <= 0.9; u += 0.1)
            for (double v = -0.9; v <= 0.9; v += 0.1) {
                const double d = std::hypot(u, v);
                const render::EllipsoidHit h = render::ellipsoidHit(K, 1, 1, u, v);
                if (d > 0.999) continue;
                worst = std::max({worst, std::fabs(h.d - d), std::fabs(h.n[0] - u), std::fabs(h.n[1] + v), std::fabs(h.n[2] - std::sqrt(1 - d * d))});
            }
        check(worst < 1e-6, "a ball is drawn as before (worst difference " + std::to_string(worst) + ")");
    }
}

// 0.26: the picture maps and the Gaia sky photo. Every built-in map decodes at its size; the sky's inverse map undoes skyScreen;
// the view -> galactic matrix agrees with the galactic frame of the sky data; and the photo is the right way round: ESA draws galactic
// longitude growing to the left, so the app's own directions of the Pleiades and the Orion Nebula must fall on brighter spots than in
// the mirror image, and the galactic plane (|b| < 5°) must be far brighter than the sky near the poles (|b| > 60°), as in any star count.
static void testSkyMaps() {
    using namespace render;
    constexpr double kPi = units::kPi;
    for (int m = 1; m < static_cast<int>(Map::Count); m++) {
        MapImage img;
        check(decodeMap(static_cast<Map>(m), img) && img.w == mapInfo(static_cast<Map>(m)).w, std::string("map decodes at its size: ") + mapInfo(static_cast<Map>(m)).file);
    }
    check(mapForBody("Mars") == Map::Mars && mapForBody("Moon") == Map::Moon && mapForBody("Venus") == Map::None && mapForBody("Saturn") == Map::Saturn && mapForBody("Uranus") == Map::Uranus && mapForBody("Neptune") == Map::Neptune, "maps by body name");
    double u, v;
    planetUV(Map::Pluto, 180 * kPi / 180, 0, u, v);
    check(std::fabs(u - 0.5) < 1e-12 && std::fabs(v - 0.5) < 1e-12, "Pluto's map is centred on 180° east (USGS metadata)");
    // the measured colours (Mallama et al. 2017 albedos -> sRGB) against an independent picture: Jupiter's Cassini map, averaged over the
    // sphere in linear light, must have the same red/green and blue/green balance within 0.1 (the map 1.06 / 0.92, the albedos 1.03 / 0.87)
    {
        int c[3];
        MapImage jm;
        check(measuredColour("Jupiter", c) && measuredColour("Venus", c) && !measuredColour("Pluto", c), "measured colours: the eight planets only");
        if (decodeMap(Map::Jupiter, jm)) {
            const auto lin = [](double v) { v /= 255; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
            double m[3] = {0, 0, 0}, wsum = 0;
            for (int y = 0; y < jm.h; y++) {
                const double w = std::cos((90 - (y + 0.5) * 180.0 / jm.h) * kPi / 180);
                for (int x = 0; x < jm.w; x += 2) { const uint8_t* p = &jm.rgba[(static_cast<std::size_t>(y) * jm.w + x) * 4]; for (int k = 0; k < 3; k++) m[k] += w * lin(p[k]); wsum += w; }
            }
            measuredColour("Jupiter", c);
            const double rg = m[0] / m[1], bg = m[2] / m[1], crg = lin(c[0]) / lin(c[1]), cbg = lin(c[2]) / lin(c[1]);
            std::printf("  Jupiter: Cassini map r/g %.2f b/g %.2f; measured albedos r/g %.2f b/g %.2f\n", rg, bg, crg, cbg);
            check(std::fabs(rg - crg) < 0.1 && std::fabs(bg - cbg) < 0.1, "Jupiter's measured colour matches its Cassini map's colour balance");
        }
    }
    const Sky& S = skyData();
    const double W = 1600, H = 1000;
    int bad = 0, n = 0;
    double worstM = 0;
    for (int yaw = 0; yaw < 12; yaw++)
        for (int pit = 0; pit <= 6; pit++) {
            const double a = yaw * kPi / 6, b = pit * kPi / 6, ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b);
            double M[9];
            skyViewToGalactic(S, ca, sa, cb, sb, M);
            for (std::size_t i = 0; i < S.stars.size(); i += 37) {
                const Vec3 d = S.stars[i].dir;
                double X, Y; Vec3 back;
                if (!skyScreen(d, W, H, ca, sa, cb, sb, X, Y)) continue;
                n++;
                if (!skyDirection(X, Y, W, H, ca, sa, cb, sb, back) || length(back - d) > 1e-9) bad++;
                const double x1 = d.x * ca - d.y * sa, y1 = d.x * sa + d.y * ca, vv[3] = {x1, y1 * cb - d.z * sb, y1 * sb + d.z * cb};
                const Vec3 G[3] = {S.galCentre, cross(S.galPole, S.galCentre), S.galPole};
                for (int r = 0; r < 3; r++) worstM = std::max(worstM, std::fabs(M[3 * r] * vv[0] + M[3 * r + 1] * vv[1] + M[3 * r + 2] * vv[2] - dot(G[r], d)));
            }
        }
    check(n > 1000 && bad == 0, "sky: skyDirection undoes skyScreen (" + std::to_string(n) + " star directions in 84 views)");
    check(worstM < 1e-12, "sky: the view -> galactic matrix gives the same galactic coordinates as the sky's centre and pole");
    MapImage g;
    if (!decodeMap(Map::Sky, g)) return;
    const auto lum = [&](int x, int y) { x = ((x % g.w) + g.w) % g.w; y = std::clamp(y, 0, g.h - 1); const uint8_t* p = &g.rgba[(static_cast<std::size_t>(y) * g.w + x) * 4]; return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]; };
    const auto box = [&](int x, int y, int r) { double t = 0; int c = 0; for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) { t += lum(x + i, y + j); c++; } return t / c; };
    const auto contrast = [&](double l, double b, bool mirror) {
        skyUV(mirror ? -l : l, b, u, v);
        const int x = static_cast<int>(u * g.w), y = static_cast<int>(v * g.h);
        return box(x, y, 6) - box(x, y, 30);
    };
    SolarSystem sys; sys.loadAt(ref::kEpochMs);
    FarItem f;
    for (int i = 0; farItem(sys, i, f); i++) {
        if (f.key != "pleia" && f.key != "m42") continue;
        const double l = std::atan2(dot(f.dir, cross(S.galPole, S.galCentre)), dot(f.dir, S.galCentre)), b = std::asin(dot(f.dir, S.galPole));
        const double right = contrast(l, b, false), mirrored = contrast(l, b, true);
        std::printf("  Gaia map at the %s (l %.1f, b %.1f): %.1f brighter than around it; mirrored %.1f\n", f.name.c_str(), l * 180 / kPi, b * 180 / kPi, right, mirrored);
        check(right > mirrored + 5, "Gaia sky map: the " + f.name + " is on a bright spot only with galactic longitude growing to the left");
    }
    double plane = 0, poles = 0; int np = 0, nq = 0;
    for (int y = 0; y < g.h; y++) {
        const double b = 90 - (y + 0.5) * 180.0 / g.h;
        for (int x = 0; x < g.w; x += 4) { if (std::fabs(b) < 5) { plane += lum(x, y); np++; } else if (std::fabs(b) > 60) { poles += lum(x, y); nq++; } }
    }
    std::printf("  Gaia map: mean brightness of the galactic plane %.1f, of the sky near the poles %.1f\n", plane / np, poles / nq);
    check(plane / np > 5 * (poles / nq), "Gaia sky map: the galactic plane is far brighter than the poles (the map is in galactic coordinates)");
}

// 0.46.3: the clock is UT, the JPL data TDB (outside review: the lab used UT as TDB, 69 s off in 2026). References: the IERS leap-second table
// (data/time/iers-leap-second.dat: TAI - UTC = 37 s from MJD 57754, 1 Jan 2017; 32 s from MJD 51179, 1 Jan 1999) with TT - TAI = 32.184 s; NASA's Delta T tables
// (data/time/nasa-deltat2004-tables.html: Table 1, 1800 14, 1850 7, 1900 -3, 1950 29 s, whole seconds; Table 2 from the Astronomical Almanac 2006, 1960.0 33.2, 1970.0 40.2 s).
static void testTimeScale() {
    using timescale::deltaTSeconds;
    const auto jdOfMjd = [](double mjd) { return mjd + 2400000.5; };
    const auto jdOfYear = [](double y) { return 2451545.0 + (y - 2000.0) * 365.25; };
    near(deltaTSeconds(jdOfMjd(61041.0)), 69.184, 1.0, 1e-9, "TT - UTC on 1 Jan 2026 = 32.184 + 37 s (IERS)");
    near(deltaTSeconds(jdOfMjd(57754.0)), 69.184, 1.0, 1e-9, "TT - UTC from 1 Jan 2017 = 69.184 s");
    near(deltaTSeconds(jdOfMjd(57753.9)), 68.184, 1.0, 1e-9, "TT - UTC on 31 Dec 2016 = 68.184 s");
    near(deltaTSeconds(jdOfMjd(51179.0)), 64.184, 1.0, 1e-9, "TT - UTC on 1 Jan 1999 = 32.184 + 32 s");
    near(deltaTSeconds(jdOfMjd(41317.0)), 42.184, 1.0, 1e-9, "TT - UTC on 1 Jan 1972 = 32.184 + 10 s");
    const struct { double y, dt, tol; } tab[] = {{1800, 14, 1.0}, {1850, 7, 1.0}, {1900, -3, 1.0}, {1950, 29, 1.0}, {1960, 33.2, 0.5}, {1970, 40.2, 0.5}};
    for (const auto& r : tab) {
        const double v = deltaTSeconds(jdOfYear(r.y));
        check(std::fabs(v - r.dt) <= r.tol, "Delta T in " + std::to_string(int(r.y)) + " = " + std::to_string(v) + " s, NASA table " + std::to_string(r.dt));
    }
    check(std::fabs(deltaTSeconds(jdOfMjd(41316.99)) - 42.184) < 1.0, "Delta T: the polynomial meets the leap-second table at 1972 within 1 s");
    // the lab's date (UT) to the JPL time (TDB): the Julian day the app works with is the date bar's plus Delta T
    SolarSystem s; s.loadAt(ref::kEpochMs);
    near((s.julianDayTDB() - s.julianDay()) * 86400.0, 69.184, 1.0, 1e-4, "SolarSystem: TDB - UT at 1 Jan 2026 = 69.184 s");
}

// 0.49.4: the Sun's colour and the habitable zone, against their sources (data/sun/README.txt, data/hz/README.txt).
static void testSun0494() {
    // habitable zone: Kopparapu et al. 2014 (ApJL 787, L29) Table 1, 1 Earth mass: runaway greenhouse Seff 1.107, maximum greenhouse 0.356; Eq. (5) d = (L / Seff)^0.5 AU, L = 1
    near(sunfacts::hzInnerAU(), std::sqrt(1 / 1.107), 1.0, 1e-12, "habitable zone inner edge = (1 / 1.107)^0.5 AU (Kopparapu 2014, runaway greenhouse)");
    near(sunfacts::hzOuterAU(), std::sqrt(1 / 0.356), 1.0, 1e-12, "habitable zone outer edge = (1 / 0.356)^0.5 AU (Kopparapu 2014, maximum greenhouse)");
    SolarSystem s; s.loadAt(ref::kEpochMs);
    Card sc; CardOptions so; cardText(s, Selection{Selection::Star}, so, sc);
    bool hz = false;
    for (const auto& l : sc.lines) hz = hz || l == "Habitable zone (Kopparapu et al. 2014): 0.95 – 1.68 AU";
    check(hz, "Sun card: the habitable zone 0.95 – 1.68 AU with its source (1.676 rounds to 1.68; until 0.49.3 it said 1.67, no source)");
    // the Sun's colour: decode the drawn sRGB bytes (IEC 61966-2-1: the transfer function and the D65 matrix, written here) back to CIE xy and compare with the
    // chromaticity of the ASTM G173-03 extraterrestrial spectrum under the CIE 1931 2-degree observer, x = 0.3234, y = 0.3327 (tools/sun_colour.py); 8-bit rounding moves xy by < 0.002
    const auto lin = [](int b) { const double c = b / 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    const double r = lin(sunfacts::kSunSrgb[0]), g = lin(sunfacts::kSunSrgb[1]), b = lin(sunfacts::kSunSrgb[2]);
    const double X = 0.4124 * r + 0.3576 * g + 0.1805 * b, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b, Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
    near(X / (X + Y + Z), 0.3234, 1.0, 0.002, "the Sun's drawn colour: CIE x of the measured solar spectrum (ASTM G173-03)");
    near(Y / (X + Y + Z), 0.3327, 1.0, 0.002, "the Sun's drawn colour: CIE y of the measured solar spectrum (ASTM G173-03)");
}

// 0.50 (review item 3): lighting in linear light. The twin (render/shading.h) decodes the sRGB colour (IEC 61966-2-1, the transfer function written again here),
// multiplies by cos i and encodes; decoding what is drawn must give cos i times the day value (1e-3), and 0 on the night side. The shaders must use the same rule:

// 0.51.2: eclipse shadows, Earth's air and Cassini's ring profile (render/shading.h, orbit/saturn_profile.h), against geometry, a second implementation of
// Bodhaine et al. (1999) and the NSSDC ring fact sheet.
static void testVisuals0512() {
    const double PI = 3.14159265358979323846;
    // the share of the Sun's disc a round body hides: the overlap of two circles (textbook: two unit circles 1 apart overlap 2 pi / 3 - sqrt(3) / 2)
    near(render::sunCovered(1, 1, 0), 1.0, 1.0, 1e-12, "eclipse: equal discs, same centre: total");
    near(render::sunCovered(1, 0.5, 0), 0.25, 1.0, 1e-12, "eclipse: a disc half the Sun's size in the middle: annular, a quarter hidden");
    check(render::sunCovered(1, 1, 2.0) == 0 && render::sunCovered(1, 0.3, 1.31) == 0, "eclipse: discs apart: no eclipse");
    near(render::sunCovered(1, 1, 1), (2 * PI / 3 - std::sqrt(3.0) / 2) / PI, 1.0, 1e-12, "eclipse: equal discs one radius apart: the lens area");
    near(render::sunCovered(1e-3, 1e-3, 1e-3), (2 * PI / 3 - std::sqrt(3.0) / 2) / PI, 1.0, 1e-9, "eclipse: the same for discs of 1 mrad (the Sun's size)");
    // the length of Earth's umbra, by similar triangles: x = R_E d / (R_sun - R_E) (IAU: au 149,597,870.7 km exact, nominal solar radius 695,700 km; Earth 6,371 km)
    {
        const double RE = 6371, d = 149597870.7, Rs = 695700, x = RE * d / (Rs - RE);
        near(x, 1.383e6, 1.383e6, 2e-3, "Earth's umbra is about 1.38 million km long");
        const double L[3] = {1, 0, 0}, o[3] = {0, 0, 0};
        for (double f : {0.5, 0.99, 1.01, 3.0}) {
            const double s[3] = {-f * x / RE, 0, 0}, sunAng = std::asin(Rs / (d + f * x));
            const double v = render::sunlightPast(s, o, 1.0, L, sunAng);
            if (f < 1) near(v, 0.0, 1.0, 1e-12, "inside Earth's umbra: no sunlight (" + std::to_string(f) + " of its length)");
            else check(v > 0 && v < 1, "beyond the umbra's tip: an annular eclipse, part of the Sun shows (" + std::to_string(f) + ")");
        }
        const double s2[3] = {2, 0, 0};
        check(render::sunlightPast(s2, o, 1.0, L, 0.00465) == 1, "a body on the far side of the Sun's direction casts no shadow");
    }
    // Earth's air: tau(555 nm) at the equator from the app's table (latitude 45 deg, so times g(45) / g(0) of List 1968) = colour-science 0.4.7's own example 0.0936290...
    {
        const int i555 = (555 - 380) / 5;
        const double g45 = 980.6160, g0 = 980.6160 * (1 - 0.0026373 + 0.0000059);
        near(render::air::kAirTau[i555] * g45 / g0, 0.0936290, 1.0, 2e-6, "Rayleigh optical depth at 555 nm (Bodhaine 1999 via colour-science)");
        check(render::air::kAirTau[0] > render::air::kAirTau[i555] && render::air::kAirTau[i555] > render::air::kAirTau[render::air::kAirN - 1], "Rayleigh: blue light scatters most");
        const double r = render::air::kAirTau[(400 - 380) / 5] / render::air::kAirTau[(700 - 380) / 5];
        check(r > std::pow(7.0 / 4, 4) && r < std::pow(7.0 / 4, 4.2), "Rayleigh: tau(400) / tau(700) between lambda^-4 and lambda^-4.2 (air's dispersion)");
        for (int c = 0; c < 3; c++) near(render::airTransmit(c, 0), 1.0, 1.0, 1e-6, "no air: sunlight stays white (the table's 7 digits)");
        check(render::airTransmit(2, 2) < render::airTransmit(1, 2) && render::airTransmit(1, 2) < render::airTransmit(0, 2), "through the air, red gets through best");
        const double k = render::kEarthAirK;
        near(render::airMass(1, k), 1.0, 1.0, 1e-12, "air mass overhead: 1");
        near(render::airMass(0.5, k), 2.0, 1.0, 5e-3, "air mass at 60 deg: about 1 / cos");
        near(render::airMass(0, k), std::sqrt(2 * k + 1), 1.0, 1e-12, "air mass at the horizon: the shell's chord, finite");
        // straight down at full phase (scattering angle 180 deg, P = 3/2): rho T + 3/8 x 1/2 x (1 - T), m = 2
        for (int c = 0; c < 3; c++) {
            const double T = render::airTransmit(c, 2.0);
            near(render::earthAirLight(c, 0.3, 1, 1, -1), 0.3 * T + 0.375 * 0.5 * (1 - T), 1.0, 1e-12, "Earth's air, centre of the full disc: the single-scattering formula");
        }
        check(render::earthAirLight(2, 0, 1, 1, -1) > render::earthAirLight(1, 0, 1, 1, -1) && render::earthAirLight(1, 0, 1, 1, -1) > render::earthAirLight(0, 0, 1, 1, -1),
              "a black sea under the air looks blue (Rayleigh)");
        check(render::earthAirLight(0, 0.3, 0, 1, -1) == 0, "Earth's night side: no sunlight to scatter");
        double tab[3 * render::kAirTableN];
        for (int i = 0; i < render::kAirTableN; i++) for (int c = 0; c < 3; c++) tab[3 * i + c] = render::airTransmit(c, render::airTableM(i));
        double worst = 0;
        for (double m = 2; m < 2 * render::airMass(0, k); m *= 1.013) for (int c = 0; c < 3; c++) worst = std::max(worst, std::fabs(render::airTableLookup(tab, c, m) - render::airTransmit(c, m)));
        check(worst < 2e-3, "the shader's air table: within 0.002 of the exact sum at every path length (" + std::to_string(worst) + ")");
    }
    // Cassini's ring profile against the NSSDC Saturnian rings fact sheet (saturn_rings.h): the mean optical depth of the C and A rings within the sheet's ranges,
    // the A ring's outer edge within 50 km of the sheet's 136,780 km
    {
        namespace rg = orbit::rings;
        check(rg::saturnProfileCount() == 7884, "ring profile: the 7,884 rows of the PDS label");
        const auto meanTau = [&](double a, double b) { double s = 0; int n = 0; for (int i = 0; i < rg::saturnProfileCount(); i++) if (rg::saturnProfileKm(i) >= a && rg::saturnProfileKm(i) < b) { s += rg::saturnProfileTau(i); n++; } return s / std::max(n, 1); };
        const double tc = meanTau(rg::kRings[0].innerKm, rg::kRings[0].outerKm), ta = meanTau(rg::kRings[3].innerKm, rg::kRings[3].outerKm);
        check(tc >= rg::kRings[0].tauLo && tc <= rg::kRings[0].tauHi, "Cassini's C ring: mean optical depth in the fact sheet's range (" + std::to_string(tc) + ")");
        check(ta >= rg::kRings[3].tauLo && ta <= rg::kRings[3].tauHi, "Cassini's A ring: mean optical depth in the fact sheet's range (" + std::to_string(ta) + ")");
        double edge = 0;
        for (int i = 0; i < rg::saturnProfileCount(); i++) if (rg::saturnProfileKm(i) < 137500 && rg::saturnProfileTau(i) > 0.3) edge = rg::saturnProfileKm(i);
        check(std::fabs(edge - rg::kRings[3].outerKm) < 50, "Cassini's A ring ends within 50 km of the fact sheet's edge (" + std::to_string(edge) + ")");
        // the drawn bins keep the area average: the blocked share and the light over the whole profile, the same at 10 km and at 800 km bins
        for (bool lit : {true, false}) {
            const auto tot = [&](double w, int which) { double s = 0; for (const auto& b : rg::saturnProfileBins(w, 0.3, 0.2, lit)) s += (b.outerKm - b.innerKm) * (which ? b.shine : b.blocked); return s; };
            near(tot(800, 0), tot(10, 0), tot(10, 0), 1e-6, "ring bins: the blocked light is kept when bins are merged");
            near(tot(800, 1), tot(10, 1), tot(10, 1), 1e-6, "ring bins: the rings' light is kept when bins are merged");
        }
        float sb[96];
        const int nb = rg::saturnShadowBands(0.4, rg::kSaturnEqRadiusKm, 32, sb);
        check(nb == 32 && sb[0] > 1.2 && sb[3 * 31 + 1] < 2.3, "ring shadow: 32 bands from the C ring to the A ring (planet radii)");
        bool opaque = false;
        for (int k = 0; k < 32; k++) if (sb[3 * k] * rg::kSaturnEqRadiusKm > 100000 && sb[3 * k + 1] * rg::kSaturnEqRadiusKm < 115000 && sb[3 * k + 2] > 2) opaque = true;
        check(opaque, "ring shadow: the B ring's middle is opaque to the Sun");
    }
}

// no ambient floor and no highlight left in their text, the decode / encode present.
static void testShading050() {
    const auto dec = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    const double kD = 3.14159265358979323846 / 180;
    for (double base : {0.2, 0.5, 0.8, 1.0}) {
        const double day = dec(render::shadeEncoded(base, 1.0));
        near(day, dec(base), 1.0, 1e-9, "lit straight on: the drawn value decodes to the colour's own light");
        for (double iDeg : {0.0, 60.0, 90.0}) {
            const double L[3] = {std::sin(iDeg * kD), 0, std::cos(iDeg * kD)}, n[3] = {0, 0, 1};
            const double v = dec(render::shadeEncoded(base, render::lambertLit(n, L)));
            near(v, std::cos(iDeg * kD) * day, 1.0, 1e-3, "incidence " + std::to_string(int(iDeg)) + " deg: decoded intensity = cos i x day value (base " + std::to_string(base) + ")");
        }
        const double Ln[3] = {0, 0, -1}, n[3] = {0, 0, 1};
        near(render::shadeEncoded(base, render::lambertLit(n, Ln)), 0.0, 1.0, 1e-12, "night side: drawn black (no ambient light)");
    }
    // the Moon: McEwen's lunar function (arXiv:2504.08252 Table 1 Eq. 23, typed again here): at full Moon (phase 0) the disc is flat to the limb; at phase 60 deg g = 1/e
    for (double ci : {1.0, 0.5, 0.1}) near(render::mcewenLunar(ci, ci, 0.0), 1.0, 1.0, 1e-12, "full Moon: the same light at cos i = cos e = " + std::to_string(ci) + " (flat to the limb)");
    { const double g = std::exp(-1.0), ci = 0.5, ce = 0.8; near(render::mcewenLunar(ci, ce, 60.0), (1 - g) * ci + g * 2 * ci / (ci + ce), 1.0, 1e-12, "phase 60 deg: McEwen's weight exp(-1)"); }
    check(render::mcewenLunar(-0.1, 0.5, 30) == 0 && render::mcewenLunar(0.5, -0.1, 30) == 0, "lunar law: 0 on the night side and on the far side");
    check(std::string(render::kPlanetFS).find("exp(-phi / 60.0)") != std::string::npos, "planet shader: the Moon uses McEwen's lunar function");
    const std::string fs = render::kFragmentShader, pfs = render::kPlanetFS;
    for (const std::string* t : {&fs, &pfs}) {
        check(t->find("0.12 + 0.88") == std::string::npos && t->find("pow(lit, 12.0)") == std::string::npos, "shader: no ambient floor and no white highlight");
        check(t->find("srgbDecode") != std::string::npos && t->find("srgbEncode") != std::string::npos, "shader: decodes to linear light and encodes back");
    }
}

// 0.50 (review item 4): a star's drawn colour, decoded back to CIE xy (IEC 61966-2-1 transfer function and matrix, written here), equals the chromaticity of a
// black body at the Ballesteros (2012, eq. 14) temperature of its B-V, within 0.002. No published Planckian-locus table could be saved (CIE's site was refused),
// so the reference is computed here from the saved CIE 1931 2-degree table (data/stars/colour-science_v0.4.7_cmfs.py, parsed here) and Planck's law.
static void testStarColours050() {
    std::FILE* fp = std::fopen(PL_DATA_DIR "/stars/colour-science_v0.4.7_cmfs.py", "rb");
    check(fp != nullptr, "the saved CIE 1931 table is there");
    if (!fp) return;
    std::string t; char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) t.append(buf, n);
    std::fclose(fp);
    const size_t a = t.find("\"CIE 1931 2 Degree Standard Observer\": {", t.find("DATA_CMFS_STANDARD_OBSERVER")), b = t.find('}', a);
    std::vector<std::array<double, 4>> cmf;
    for (size_t p = a; p < b;) {
        const size_t q = t.find(": (", p);
        if (q == std::string::npos || q > b) break;
        size_t s0 = t.rfind(' ', q) + 1;
        double l = std::atof(t.substr(s0, q - s0).c_str()), x = 0, y = 0, z = 0;
        if (std::sscanf(t.c_str() + q + 3, "%lf, %lf, %lf", &x, &y, &z) == 3) cmf.push_back({l, x, y, z});
        p = q + 3;
    }
    check(cmf.size() == 471 && cmf.front()[0] == 360 && cmf.back()[0] == 830, "CIE 1931 table: 360-830 nm in 1 nm steps (" + std::to_string(cmf.size()) + " rows)");
    const double h = 6.62607015e-34, c = 299792458.0, k = 1.380649e-23;
    const auto dec = [](int byte) { const double v = byte / 255.0; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    for (double bv : {-0.3, 0.0, 0.4, 0.65, 1.0, 1.5}) {
        const double T = 4600 * (1 / (0.92 * bv + 1.7) + 1 / (0.92 * bv + 0.62));   // Ballesteros 2012, eq. (14)
        double X = 0, Y = 0, Z = 0;
        for (const auto& r : cmf) { const double l = r[0] * 1e-9, p = 2 * h * c * c / std::pow(l, 5) / std::expm1(h * c / (l * k * T)); X += p * r[1]; Y += p * r[2]; Z += p * r[3]; }
        const auto rgb = bvRGB(bv);
        const double R = dec(rgb[0]), G = dec(rgb[1]), B = dec(rgb[2]);
        const double dX = 0.4124 * R + 0.3576 * G + 0.1805 * B, dY = 0.2126 * R + 0.7152 * G + 0.0722 * B, dZ = 0.0193 * R + 0.1192 * G + 0.9505 * B;
        const std::string what = "star B-V " + std::to_string(bv) + " (" + std::to_string(int(T)) + " K): drawn colour's CIE ";
        near(dX / (dX + dY + dZ), X / (X + Y + Z), 1.0, 0.002, what + "x = the black body's");
        near(dY / (dX + dY + dZ), Y / (X + Y + Z), 1.0, 0.002, what + "y = the black body's");
    }
}

// 0.50 (review item 5): one brightness rule for every star. Light ∝ 10^(-0.4 m) (Pogson); drawn integrated light (opacity x area, in pixels) ∝ that light to the power 0.5.
// Two stars 5 magnitudes apart: light ratio 100, so the drawn ratio is 100^0.5 = 10; and every star's drawn light is exactly opacity x pi r^2 of its disc.
static void testStarBrightness050() {
    for (double m : {-1.0, 1.0, 3.0}) {
        const StarDraw a = starDraw(m), b = starDraw(m + 5);
        const double la = a.alpha * 3.14159265358979323846 * a.radius * a.radius, lb = b.alpha * 3.14159265358979323846 * b.radius * b.radius;
        near(la / lb, std::pow(std::pow(10.0, -0.4 * -5.0), 0.5), 1.0, 1e-9, "stars 5 magnitudes apart (m " + std::to_string(m) + "): drawn light ratio = (Pogson ratio 100)^0.5 = 10");
        check(a.alpha <= 1 && b.alpha <= 1 && b.alpha > 0, "opacity within 0..1");
    }
    check(starDraw(6.6).alpha > 0.3, "the faintest catalogue star (m 6.6) stays visible");
}

// 0.50 (review items 8 and 10): a map's colour against a measured disc colour. Both as LINEAR sRGB ratios r/g and b/g (the check the OPAL maps used).
// The map: every pixel decoded (IEC 61966-2-1), averaged over the sphere (weight cos latitude). The reference: the measured geometric albedos (typed here
// from the paper) -> linear between the band wavelengths, flat outside -> CIE 1931 XYZ under D65, 360-780 nm in 5 nm steps (the saved tables, parsed here)
// -> linear sRGB. Tolerance 0.02: the JPEG's 8-bit rounding and compression.
static std::string readAll(const std::string& path) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    std::string t; if (!fp) return t;
    char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) t.append(buf, n);
    std::fclose(fp);
    return t;
}
static std::map<int, std::array<double, 3>> savedCmf() {
    std::map<int, std::array<double, 3>> out;
    const std::string t = readAll(PL_DATA_DIR "/stars/colour-science_v0.4.7_cmfs.py");
    const size_t a = t.find("\"CIE 1931 2 Degree Standard Observer\": {", t.find("DATA_CMFS_STANDARD_OBSERVER")), b = t.find('}', a);
    for (size_t p = a; p != std::string::npos && p < b;) {
        const size_t q = t.find(": (", p); if (q == std::string::npos || q > b) break;
        double x, y, z; if (std::sscanf(t.c_str() + q + 3, "%lf, %lf, %lf", &x, &y, &z) == 3) out[std::atoi(t.c_str() + t.rfind(' ', q) + 1)] = {x, y, z};
        p = q + 3;
    }
    return out;
}
static std::map<int, double> savedD65() {
    std::map<int, double> out;
    const std::string t = readAll(PL_DATA_DIR "/planets/colour-science_v0.4.7_illuminant_sds.py");
    const size_t a = t.find("\"D65\": {"), b = t.find('}', a);
    for (size_t p = t.find('\n', a); p != std::string::npos && p < b; p = t.find('\n', p + 1)) {
        double l, v; if (std::sscanf(t.c_str() + p + 1, " %lf: %lf,", &l, &v) == 2) out[int(l)] = v;
    }
    return out;
}
static std::array<double, 2> albedoRatios(const double wl[], const double alb[], int n) {
    std::vector<std::pair<double, double>> pts; for (int i = 0; i < n; i++) pts.push_back({wl[i], alb[i]});
    std::sort(pts.begin(), pts.end());
    const auto at = [&](double l) {
        if (l <= pts.front().first) return pts.front().second;
        if (l >= pts.back().first) return pts.back().second;
        for (size_t i = 0; i + 1 < pts.size(); i++) if (l <= pts[i + 1].first) return pts[i].second + (pts[i + 1].second - pts[i].second) * (l - pts[i].first) / (pts[i + 1].first - pts[i].first);
        return pts.back().second;
    };
    const auto cmf = savedCmf(); const auto d65 = savedD65();
    double X = 0, Y = 0, Z = 0;
    for (int l = 360; l <= 780; l += 5) { const double a = at(l), e = d65.at(l); const auto& c = cmf.at(l); X += a * e * c[0]; Y += a * e * c[1]; Z += a * e * c[2]; }
    const double r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z, g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z, b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    return {r / g, b / g};
}
static std::array<double, 2> mapRatios(render::Map m) {
    render::MapImage im;
    if (!render::decodeMap(m, im)) return {-1, -1};
    double lut[256]; for (int i = 0; i < 256; i++) { const double c = i / 255.0; lut[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
    double R = 0, G = 0, B = 0;
    for (int y = 0; y < im.h; y++) {
        const double w = std::cos(((y + 0.5) / im.h - 0.5) * 3.14159265358979323846);
        for (int x = 0; x < im.w; x++) { const uint8_t* p = &im.rgba[4 * (std::size_t(y) * im.w + x)]; R += w * lut[p[0]]; G += w * lut[p[1]]; B += w * lut[p[2]]; }
    }
    return {R / G, B / G};
}
static void testMapColours050() {
    const double wl[12] = {360, 436, 549, 700, 900, 641, 798, 355, 469, 616, 748, 893};   // U B V R I Rc Ic u' g' r' i' z' (nm), as tools/map_albedo_colour.py
    const double mercury[12] = {0.087, 0.105, 0.142, 0.172, 0.208, 0.158, 0.180, 0.095, 0.130, 0.169, 0.200, 0.237};   // Mallama et al. 2017, Table 7
    const auto ref = albedoRatios(wl, mercury, 12), got = mapRatios(render::Map::Mercury);
    near(got[0], ref[0], 1.0, 0.02, "Mercury's map: r/g (linear, over the sphere) = Mallama 2017's disc colour " + std::to_string(ref[0]));
    near(got[1], ref[1], 1.0, 0.02, "Mercury's map: b/g (linear, over the sphere) = Mallama 2017's disc colour " + std::to_string(ref[1]));
}

// 0.50 (review item 9): the Moon's axis and libration. The drawn sub-Earth point (the app's Moon place and orientation) against JPL Horizons, target Moon (301),
// observer Earth's centre, "ObsSub-LON ObsSub-LAT" (MOON_ME frame, east longitude), saved as received: data/jpl/horizons-moon-subearth.txt (7 dates, 1800-2100, TT).
// Tolerance 0.05 degrees: Horizons gives the APPARENT point (light time and aberration, about 0.006 deg) in the DE ephemeris' mean-Earth frame; the app uses the
// geometric ELP2000-82B place (its direction error is under 0.001 deg) and the IAU rotation model, which follows the mean-Earth frame to a few hundredths of a degree
// at most. The libration itself reaches about 8 deg in longitude and 7 deg in latitude, so the old rule (sub-Earth point always 0, 0) fails by degrees.
static void testMoonOrientation050() {
    const std::string t = readAll(PL_DATA_DIR "/jpl/horizons-moon-subearth.txt");
    const size_t a = t.find("$$SOE"), b = t.find("$$EOE");
    check(a != std::string::npos && b != std::string::npos, "the saved Horizons sub-Earth table is there");
    if (a == std::string::npos || b == std::string::npos) return;
    static const char* const kMon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int rows = 0;
    for (size_t p = t.find('\n', a) + 1; p < b; p = t.find('\n', p) + 1) {
        int y, d, hh, mi; double ss, lon, lat; char mon[4] = {};
        if (std::sscanf(t.c_str() + p, " %d-%3c-%d %d:%d:%lf %lf %lf", &y, mon, &d, &hh, &mi, &ss, &lon, &lat) != 8) continue;
        int m = 0; for (int k = 0; k < 12; k++) if (std::strncmp(mon, kMon[k], 3) == 0) m = k + 1;
        // Julian date of the TT calendar date (Meeus, Astronomical Algorithms, ch. 7; Gregorian); TDB - TT is under 2 ms
        int Y = y, M = m; if (M <= 2) { Y -= 1; M += 12; }
        const int A = Y / 100, B = 2 - A + A / 4;
        const double jd = std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + d + B - 1524.5 + (hh + mi / 60.0 + ss / 3600.0) / 24.0;
        double gl, gb;
        moonSubEarthDeg(jd, gl, gb);
        double dl = std::fmod(gl - lon + 540.0, 360.0) - 180.0;
        near(dl, 0.0, 1.0, 0.05, "Moon's drawn sub-Earth longitude at JD " + std::to_string(jd) + ": app " + std::to_string(gl) + ", Horizons " + std::to_string(lon));
        near(gb, lat, 1.0, 0.05, "Moon's drawn sub-Earth latitude at JD " + std::to_string(jd) + ": app " + std::to_string(gb) + ", Horizons " + std::to_string(lat));
        rows++;
    }
    check(rows == 7, "7 Horizons dates read (" + std::to_string(rows) + ")");
}

// 0.50 (review item 10): the moons drawn with a map are turned by their IAU model. Io's drawn sub-Jupiter point (the app's Io place, orbit/moon_sats.h, and its
// orientation) against JPL Horizons, target Io (501), observer Jupiter's centre, "ObsSub-LON ObsSub-LAT" (IAU_IO, WEST longitude), saved as received:
// data/jpl/horizons-io-subjupiter.txt (7 dates, 1800-2100). Tolerance 0.62 degree: the app's Io place is a fit to Horizons (moon_sats.h) whose worst error is
// 4,514 km (moon_sats_worst.inc), atan(4514 / 421,800 km) = 0.61 deg seen from Jupiter; Horizons' point is apparent (1.4 s of light time: under 0.001 deg).
// The orientation model itself adds far less. Io's real sub-Jupiter point swings by about 1.2 deg, so the old rule (always 0, 0) fails at 4 of the 7 dates.
static void testIauMoons050() {
    const std::string t = readAll(PL_DATA_DIR "/jpl/horizons-io-subjupiter.txt");
    const size_t a = t.find("$$SOE"), b = t.find("$$EOE");
    check(a != std::string::npos && b != std::string::npos, "the saved Horizons Io table is there");
    if (a == std::string::npos || b == std::string::npos) return;
    static const char* const kMon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int rows = 0;
    for (size_t p = t.find('\n', a) + 1; p < b; p = t.find('\n', p) + 1) {
        int y, d, hh, mi; double ss, lonW, lat; char mon[4] = {};
        if (std::sscanf(t.c_str() + p, " %d-%3c-%d %d:%d:%lf %lf %lf", &y, mon, &d, &hh, &mi, &ss, &lonW, &lat) != 8) continue;
        int m = 0; for (int k = 0; k < 12; k++) if (std::strncmp(mon, kMon[k], 3) == 0) m = k + 1;
        int Y = y, M = m; if (M <= 2) { Y -= 1; M += 12; }
        const int A = Y / 100, B = 2 - A + A / 4;
        const double jd = std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + d + B - 1524.5 + (hh + mi / 60.0 + ss / 3600.0) / 24.0;
        Vec3 pos, vel, ax[3];
        if (!satState("Io", jd, pos, vel)) { check(false, "Io's place at JD " + std::to_string(jd)); continue; }
        satDrawnAxes("Io", jd, pos, ax);
        const Vec3 u = pos * (-1 / length(pos));
        const double kD = 180 / 3.14159265358979323846;
        const double lonE = std::atan2(dot(u, ax[1]), dot(u, ax[0])) * kD, la = std::asin(dot(u, ax[2])) * kD;
        const double dl = std::fmod(lonE + lonW + 540.0, 360.0) - 180.0;   // east = -west
        near(dl, 0.0, 1.0, 0.62, "Io's drawn sub-Jupiter longitude at JD " + std::to_string(jd) + ": app " + std::to_string(-lonE) + " W, Horizons " + std::to_string(lonW) + " W");
        near(la, lat, 1.0, 0.62, "Io's drawn sub-Jupiter latitude at JD " + std::to_string(jd));
        rows++;
    }
    check(rows == 7, "7 Horizons dates read for Io (" + std::to_string(rows) + ")");
}

// 0.52.1: a planet smaller than a pixel is drawn in its measured colour (render::pointColour = render::measuredColour for every body that has one), else its marker
static void testPointColour0521() {
    int n = 0;
    bool same = true;
    auto check1 = [&](const char* name, const char* hex) {
        int m[3], p[3];
        render::pointColour(name, hex, p);
        if (render::measuredColour(name, m)) { n++; same = same && p[0] == m[0] && p[1] == m[1] && p[2] == m[2]; }
        else { unsigned r = 0, g = 0, b = 0; std::sscanf(hex, "#%02x%02x%02x", &r, &g, &b); same = same && p[0] == int(r) && p[1] == int(g) && p[2] == int(b); }
    };
    for (std::size_t i = 0; i < kPlanetCount; i++) check1(kPlanets[i].name, kPlanets[i].color);
    for (std::size_t i = 0; i < kDwarfCount; i++) check1(kDwarfs[i].name, kDwarfs[i].color);
    check(n == 8 && same, "a sub-pixel body's colour is its measured colour for all 8 planets that have one (the ball's), its marker colour for the rest (" + std::to_string(n) + " measured)");
}

// 0.73.0: the Alpha Centauri system (PLAN_ALPHACEN.md sect. 12, the eight tests, limits as the plan wrote them before any result was looked at).
// WRITTEN BEFORE the code (on 0.72.1 none of it builds: orbit/alphacen.h did not exist). The references are typed HERE from the saved papers (sources repo,
// data/alphacen/, native/data/alphacen/README.txt), not read from the app's own tables: Akeson et al. 2021 (A21) Tables 6, 8, 9, 10; Pourbaix & Boffin 2016
// (PB16) Table 1; Kervella et al. 2016 (K16) as A21 Table 8 quotes it; Suarez Mascareno et al. 2025 (N25) Tables 2 and 3; Faria et al. 2022 Table 1;
// Kopparapu et al. 2014 eq. 4-5; Willmer 2018 Table 3.
static void testAlphaCen073() {
    using namespace orbit;
    const double PC_AU = 648000.0 / units::kPi, d2r = units::kPi / 180;
    // 1. Kepler's third law: a^3 / P^2 (a = a" / parallax in au, P in Julian years, GM_sun DE440) against each paper's own mA + mB, within 3 x the propagated error
    {
        struct O { const char* n; double a, sa, P, sP, plx, splx, mA, mB; } os[] = {
            {"A21", 17.4930, 0.0096, 79.762, 0.019, 750.81, 0.38, 1.0788, 0.9092},
            {"K16", 17.592, 0.013, 79.929, 0.013, 747.17, 0.61, 1.1055, 0.9373},
            {"PB16", 17.66, 0.026, 79.91, 0.013, 743.0, 1.3, 1.133, 0.972}};
        for (const O& o : os) {
            const double aAU = o.a / (o.plx / 1000), M = aAU * aAU * aAU / (o.P * o.P);   // (with GM_sun the unit: 4 pi^2 au^3 / yr^2 = GM_sun to 1e-4; the exact form below)
            const double Mx = 4 * units::kPi * units::kPi * std::pow(aAU * units::kAU, 3) / (units::kGMSun * std::pow(o.P * units::kYear, 2));
            const double rel = std::sqrt(std::pow(3 * o.sa / o.a, 2) + std::pow(3 * o.splx / o.plx, 2) + std::pow(2 * o.sP / o.P, 2));
            (void)M;
            check(std::fabs(Mx - (o.mA + o.mB)) <= 3 * rel * Mx, std::string("alpha Cen: Kepler's third law with ") + o.n + "'s own a, P, parallax gives its mA + mB within 3 sigma");
        }
        // the app's A-B orbit: GM = G (mA + mB) with A21's masses, so Kepler's law holds by construction
        const double aM = acen::abSemiMajorAU() * units::kAU, P = acen::abPeriodYears() * units::kYear;
        near(4 * units::kPi * units::kPi * aM * aM * aM / (P * P), acen::abGM(), acen::abGM(), 1e-12, "alpha Cen: the app's A-B orbit keeps Kepler's law (a, P from the app, GM from A21's masses)");
        near(acen::abGM() / units::kGMSun, 1.0788 + 0.9092, 1, 1e-12, "alpha Cen: the app's GM of A + B is A21's 1.0788 + 0.9092 solar masses");
        near(acen::abPeriodYears(), 79.762, 79.762, 3 * 0.019 / 79.762, "alpha Cen: the app's A-B period (from Kepler's law) is A21's 79.762 +- 0.019 yr within 3 sigma");
        near(acen::abSemiMajorAU(), 17.4930 / 0.75081, 1, 1e-12, "alpha Cen: a = 17.4930 arcsec / 750.81 mas (A21)");
    }
    // 2. the orbit against ALMA (A21 Table 6): position angle and separation of B about A within 5 mas
    {
        const double alma[][3] = {{2018.7846, 336.2542, 4.80434}, {2019.5358, 342.8634, 5.21125}, {2019.6505, 343.7729, 5.27869}};
        for (const auto& r : alma) {
            const double jd = 2451545.0 + (r[0] - 2000.0) * 365.25;     // decimal Julian year -> JD (A21 use Julian years)
            const Vec3 rel = acen::bAboutA(jd);                          // scene frame: x west, y north, z towards the Sun (m)
            const double east = -rel.x, north = rel.y, sep = std::hypot(east, north) / units::kAU * 0.75081;   // arcsec at A21's parallax
            const double pa = std::fmod(std::atan2(east, north) / d2r + 360, 360);
            const double dpa = std::remainder(pa - r[1], 360.0);
            check(std::fabs(dpa * d2r * r[2] * 1000) <= 5, "alpha Cen: B's position angle at the ALMA epoch " + std::to_string(r[0]) + " within 5 mas across (A21 Table 6)");
            check(std::fabs((sep - r[2]) * 1000) <= 5, "alpha Cen: B's separation at the ALMA epoch " + std::to_string(r[0]) + " within 5 mas (A21 Table 6)");
        }
        // A and B about their centre of mass: A at -mB/(mA+mB), B at +mA/(mA+mB) of B's relative orbit
        const double jd = 2461000.5;
        const acen::Stars s = acen::stars(jd);
        const Vec3 rel = acen::bAboutA(jd);
        near(length(s.B - s.A - rel), 0, length(rel), 1e-12, "alpha Cen: B - A is the relative orbit");
        near(length(s.A * 1.0788 + s.B * 0.9092), 0, length(rel), 1e-12, "alpha Cen: the centre of mass of A and B is the scene's origin");
    }
    // 3. RV semi-amplitude K = (2 pi G / P)^(1/3) m sin i / (M + m)^(2/3), M = 0.1221 (N25 Table 2): within 1 % of N25's K (Table 3)
    {
        const double Pb = 11.18465 * units::kDay, Pd = 5.12338 * units::kDay, mE = 398600.435507e9 / units::kGMSun;   // GM_Earth DE440 (nbody.h)
        near(acen::rvSemiAmplitude(Pb, 1.055 * mE, 0.1221), 1.226, 1.226, 0.01, "Proxima b: K from m sin i within 1 % of N25's 1.226 m/s");
        near(acen::rvSemiAmplitude(Pd, 0.260 * mE, 0.1221), 0.392, 0.392, 0.01, "Proxima d: K from m sin i within 1 % of N25's 0.392 m/s");
        // 4. planet orbit size from Kepler's law with M = 0.1221 within 1 sigma of N25's a
        check(std::fabs(acen::keplerAAU(Pb, 0.1221 + 1.055 * mE) - 0.04848) <= 0.00029, "Proxima b: a from Kepler's law within 1 sigma of N25's 0.04848 +- 0.00029 au");
        check(std::fabs(acen::keplerAAU(Pd, 0.1221 + 0.260 * mE) - 0.02881) <= 0.00017, "Proxima d: a from Kepler's law within 1 sigma of N25's 0.02881 +- 0.00017 au");
    }
    // 5. habitable zone (Kopparapu 2014 eq. 4-5 with the Teff term): the Sun's edges unchanged; Proxima's early-Mars edge with F22's L = 0.0016 and 2900 K within 0.0005 au of N25's 0.088
    {
        near(sunfacts::hzEdgeAU(1.0, 5780, sunfacts::Hz::Runaway), 0.9504, 1, 1e-4, "habitable zone at 5780 K, L = 1: runaway greenhouse 0.9504 au");
        near(sunfacts::hzEdgeAU(1.0, 5780, sunfacts::Hz::MaxGreenhouse), 1.6760, 1, 1e-4, "habitable zone at 5780 K, L = 1: maximum greenhouse 1.6760 au");
        near(sunfacts::hzEdgeAU(1.0, 5780, sunfacts::Hz::Runaway), sunfacts::hzInnerAU(), 1, 1e-15, "habitable zone: the Teff form at 5780 K is the Sun's old one");
        check(std::fabs(sunfacts::hzEdgeAU(0.0016, 2900, sunfacts::Hz::EarlyMars) - 0.088) <= 0.0005, "Proxima: early-Mars edge with L = 0.0016, 2900 K within 0.0005 au of N25's 0.088");
        // and by hand from Table 1 for A (5795 K, 1.5059 L_sun): runaway greenhouse Seff = 1.107 + 1.332e-4 T + 1.58e-8 T^2 - 8.308e-12 T^3 - 1.931e-15 T^4, T = 15 K
        const double T = 15, se = 1.107 + 1.332e-4 * T + 1.58e-8 * T * T - 8.308e-12 * T * T * T - 1.931e-15 * T * T * T * T;
        near(sunfacts::hzEdgeAU(1.5059, 5795, sunfacts::Hz::Runaway), std::sqrt(1.5059 / se), 1, 1e-12, "alpha Cen A: runaway-greenhouse edge from Kopparapu's Table 1 by hand");
    }
    // 6. Proxima is bound: the drawn orbit's energy is negative; its periastron and apastron are A21's 4.1 and 12.3 kau within the table's rounding (0.05 kau)
    {
        const acen::Ellipse o = acen::proximaOrbit();
        const State st = acen::proximaState(2461000.5);
        const Vec3 r = st.pos - o.F;                                         // about the drawn orbit's focus
        const double E = 0.5 * dot(st.vel, st.vel) - acen::proximaGM() / length(r);
        check(E < 0, "Proxima: the orbit's energy about A-B is negative (bound)");
        check(std::fabs(o.a * (1 - o.e) / units::kAU - 4100) <= 50 && std::fabs(o.a * (1 + o.e) / units::kAU - 12300) <= 50, "Proxima: periastron 4.1 and apastron 12.3 kau (A21 Table 10) within the rounding");
        near(-acen::proximaGM() / (2 * E), o.a, o.a, 1e-9, "Proxima: the state's energy gives the drawn orbit's a");
        near(acen::proximaGM() / units::kGMSun, 1.0788 + 0.9092 + 0.1221, 1, 1e-12, "Proxima's orbit: GM of all three stars (A21 masses, Proxima 0.1221)");
        near(o.periodYears / 1000, 511, 511, 41.0 / 511, "Proxima's period from Kepler's law within A21's 511 +41 -30 kyr");
        // its place: today's measured offset (A21 Table 9 barycentre at 2016.0, Gaia DR3 Proxima as Libralato 2025 Table 2, both parallaxes): 12,052 au (computed in
        // tools/alphacen_numbers.py) at the epoch 2016.0
        const Vec3 p16 = acen::proximaState(2457388.5).pos;   // 2016-01-01 0h (epoch 2016.0 = JD 2457389.0: half a day, 0.03 au)
        near(length(p16) / units::kAU, 12052, 12052, 5.0 / 12052, "Proxima at its measured distance from A-B, 12,052 au (within 5 au)");
        check(p16.z / units::kAU > 6300 && p16.z / units::kAU < 6440, "Proxima is about 6,368 au nearer to us than A-B (z towards the Sun)");
    }
    // 7. phase: at its N25 T0 (inferior conjunction) each planet is nearest us (towards the Sun: z largest); one period later it is back within 1e-9 of the orbit
    {
        const double T0[2] = {2460548.59, 2460557.55}, P[2] = {11.18465, 5.12338};
        for (int k = 0; k < 2; k++) {
            const Vec3 p0 = acen::planetOffset(k, T0[k]);
            double zmax = -1e300;
            for (int j = 0; j < 400; j++) zmax = std::max(zmax, acen::planetOffset(k, T0[k] + P[k] * j / 400.0).z);
            check(p0.z >= zmax - 1e-9 * length(p0), std::string("Proxima ") + (k ? "d" : "b") + ": at its T0 (N25) it is at inferior conjunction (nearest us)");
            near(length(acen::planetOffset(k, T0[k] + P[k]) - p0), 0, length(p0), 1e-9, std::string("Proxima ") + (k ? "d" : "b") + ": back after one period");
            near(length(p0) / units::kAU, k ? 0.02881 : 0.04848, 1, 1e-12, std::string("Proxima ") + (k ? "d" : "b") + ": on N25's circle");
        }
    }
    // 8. the sky: the Sun's V from A-B 0.43 +- 0.01 (Willmer 2018 M_V 4.81, A21's distance); its direction within 0.01 deg of the antipode of A21 Table 9
    {
        near(acen::sunVFromAB(), 0.43, 1, 0.01, "the Sun seen from Alpha Centauri: V = 0.43 +- 0.01");
        const double ra = (219.85892215 + 180) * d2r, de = 60.83163195 * d2r;   // the antipode of A21 Table 9
        const Vec3 want{std::cos(de) * std::cos(ra), std::cos(de) * std::sin(ra), std::sin(de)};
        const Vec3 got = acen::sunDirectionEquatorial();                          // from the app's sky as seen from Alpha Centauri (the Sun's star)
        check(std::acos(std::clamp(dot(got, want) / length(got), -1.0, 1.0)) / d2r <= 0.01, "the Sun in Alpha Centauri's sky: within 0.01 deg of the antipode of A21 Table 9");
        (void)PC_AU;
        // the sky's Sun star has that V; Sirius seen from there (HYG v4.1 row, data/stars/hyg_v41_far_stars_rows.csv: RA 6.752481 h, Dec -16.716116, 2.6371 pc, V -1.44)
        // is where plain vector arithmetic puts it (the A-B barycentre at A21 Table 9's direction and 1000 / 750.81 pc), within the catalogue's 0.01 deg cut of its
        // places (0.02 deg here), and its V is -1.44 + 5 log10(d' / d) within the catalogue's 0.1 cut of V (0.11 here)
        const Sky& S = acen::acenSky();
        double sunV = 99;
        for (const auto& nm : S.names) if (nm.second == "Sun") sunV = S.stars[std::size_t(nm.first)].mag;
        near(sunV, 0.43, 1, 0.01, "the Sun's star in Alpha Centauri's sky: V 0.43 +- 0.01");
        const double ra2 = 6.752481 * 15 * d2r, de2 = -16.716116 * d2r, dS = 2.6371, dAB = 1000.0 / 750.81, raB = 219.85892215 * d2r, deB = -60.83163195 * d2r;
        const Vec3 sir = Vec3{std::cos(de2) * std::cos(ra2), std::cos(de2) * std::sin(ra2), std::sin(de2)} * dS - Vec3{std::cos(deB) * std::cos(raB), std::cos(deB) * std::sin(raB), std::sin(deB)} * dAB;
        const Vec3 sirDir = acen::equatorialToScene(sir * (1 / length(sir)));
        double best = 1e9; float bestV = 99;
        for (const SkyStar& st : S.stars) { const double a = std::acos(std::clamp(dot(st.dir, sirDir), -1.0, 1.0)) / d2r; if (a < best) { best = a; bestV = st.mag; } }
        check(best <= 0.02, "Sirius seen from Alpha Centauri: in its parallax-shifted direction (within 0.02 deg; " + std::to_string(best) + ")");
        near(bestV, -1.44 + 5 * std::log10(length(sir) / dS), 1, 0.11, "Sirius seen from Alpha Centauri: V by the inverse square of its new distance");
        int bright = 0;
        for (const SkyStar& st : S.stars) if (st.mag < 0.5 && st.mag > -30) bright++;
        check(bright >= 1, "Alpha Centauri's sky: A and B are not in it, the Sun and the bright stars are");
    }
    // keplerState with GM = kGMSun is today's (0.72.1) to the last bit: the same call both ways, and five orbits against 0.72.1's numbers (printed with %a by
    // 0.72.1's own build, GCC -O3 on x86-64; another compiler may round a last bit of its own differently, so there 4 units in the last place)
    {
        const double c[][6] = {{1.495978707e11, 0.0167, 0.0, -11.26, 114.2, 10.0}, {7.7834e11, 0.0484, 1.30, 100.46, 273.87, -170.0}, {5.9e12, 0.2488, 17.16, 110.30, 113.83, 33.3},
                               {2.67e12, 0.967, 162.3, 58.42, 111.33, 179.0}, {5.79e10, 0.2056, 7.0, 48.33, 29.12, -45.0}};
        const double ref[][6] = {
            {-0x1.b142a33bf41p+35, 0x1.f782c8a10abf2p+36, 0x0p+0, -0x1.b3223c3b17f8cp+14, -0x1.7362c44a76b2cp+13, -0x0p+0},
            {-0x1.5c5c5f4c2b69p+39, -0x1.2dd9eaadcf54ep+38, 0x1.0caabd3f984e2p+34, 0x1.3af142b54b883p+12, -0x1.63b8db190a7b7p+13, -0x1.063826237a964p+6},
            {0x1.4d47d6f54606cp+39, -0x1.154bd95bdd20ep+42, 0x1.1a43541073efdp+38, 0x1.564e7060fd50ep+12, -0x1.11633e1939bp+8, -0x1.85376771c4e05p+10},
            {-0x1.5a4ac8926b04bp+41, 0x1.d9263254d396ap+41, -0x1.5a74507554275p+40, 0x1.5c84b7e9e195fp+9, 0x1.23c887455d478p+9, 0x1.6fe988faf7d14p+6},
            {0x1.71b3332198b4ap+35, 0x1.4ea25d4e0d7ddp+33, -0x1.b143e451491b8p+31, -0x1.3b3fdc3fe070dp+14, 0x1.8443d5322118ap+15, 0x1.713729c7993edp+12}};
#if defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__)
        const double ulps = 0;
#else
        const double ulps = 4;
#endif
        for (int k = 0; k < 5; k++) {
            const State a = keplerState(c[k][0], c[k][1], c[k][2], c[k][3], c[k][4], c[k][5]);
            const State b = keplerState(c[k][0], c[k][1], c[k][2], c[k][3], c[k][4], c[k][5], units::kGMSun);
            const double ga[6] = {a.pos.x, a.pos.y, a.pos.z, a.vel.x, a.vel.y, a.vel.z}, gb[6] = {b.pos.x, b.pos.y, b.pos.z, b.vel.x, b.vel.y, b.vel.z};
            bool same = true, old = true;
            for (int j = 0; j < 6; j++) {
                same = same && std::memcmp(&ga[j], &gb[j], sizeof(double)) == 0;
                old = old && std::fabs(ga[j] - ref[k][j]) <= ulps * std::fabs(std::nextafter(ref[k][j], 2 * ref[k][j] + 1) - ref[k][j]);
            }
            check(same, "keplerState with GM = kGMSun is the Sun's keplerState, bit for bit (orbit " + std::to_string(k) + ")");
            check(old, "keplerState gives 0.72.1's numbers (orbit " + std::to_string(k) + ")");
        }
    }
}

int main() {
    testCamera075();
    testRings075();
    testReview0752();
    testAlphaCen073();
    testPointColour0521();
    testIauMoons050();
    testMoonOrientation050();
    testMapColours050();
    testStarBrightness050();
    testStarColours050();
    testShading050();
    testVisuals0512();
    testSun0494();
    testTimeScale();
    testOrbitPath();
    testSaturnRings();
    testShapes();
    testMoonSats();
    testRealRocks();
    testSkyMaps();
    testLabelClear();
    testFormatFuzz();
    testRockKepler();
    // 1) Kepler's equation, checked by putting E back in: E - e sin E must give M
    for (double e : {0.0, 0.0167, 0.2056, 0.6, 0.967})
        for (double Mdeg : {-170.0, -30.0, 0.0, 1.0, 45.0, 120.0, 179.0}) {
            const double E = keplerE(e, Mdeg), M = Mdeg * 3.14159265358979323846 / 180;
            near(E - e * std::sin(E), M, 1.0, 1e-12, "keplerE e=" + std::to_string(e) + " M=" + std::to_string(Mdeg));
        }

    // 6) physics sanity: after one Earth year Earth is back
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
        // (0.10.5 stage 5: the old "Earth back at its start after one Julian year, within 1e-4 AU = 15,000 km" is replaced by the check against Kepler's third law in section 16; the energy check stays)
        (void)moved;
        const double e1 = orbitOf(s.bodies[2]).energy;
        near(e1, e0, e0, 1e-6, "Earth orbital energy conserved over one year");
    }

    // 7) belts: four groups of rocks
    check(makeBelts(ref::kEpochMs).size() == 4, "4 belt groups");

    // 9) (0.10.5 stage 5) the two famous dates of the Jump tiles are now checked in section 16 against JPL Horizons vectors, in Real mode; the old checks here (Mars-Earth within 0.3 million km, Jupiter-Saturn 0.1 +- 0.05 degrees, Simple mode) could not see an error under about 100,000 km.

    // 10) number and date wording (fixed rules, ECMA-262)
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
    //     with fixed rounding rules
    {
        check(fmt::celsius(15) == "15 °C", "15 °C metric");
        check(fmt::kmPlain(2575) == "2,575 km" && fmt::kmPlain(1560.8) == "1,560.8 km" && fmt::kmPlain(11.1) == "11.1 km" && fmt::kmPlain(606) == "606 km",
              "kmPlain groups thousands: " + fmt::kmPlain(2575) + " / " + fmt::kmPlain(1560.8));
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

    // 12) the info cards (printed so they can be read)
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
        // row 7: Oort travel times come from Voyager 1's speed (16.882 km/s x 0.21095 AU per year per km/s = 3.5613 AU/yr; 2026-11-05 audit: was 16.94, now Horizons) and the distances in the same text
        const double auPerYear = 16.882 * 0.21095;
        near(voyagerYearsFromSun(2000), 2000 / auPerYear, 560, 1e-9, "Oort: years to 2,000 AU (by hand 561.6)");
        near(voyagerYearsFromSun(200000), 200000 / auPerYear, 56000, 1e-9, "Oort: years to 200,000 AU (by hand 56,160)");
        near(voyagerYearsFromSun(200000) / voyagerYearsFromSun(2000), 100, 100, 1e-12, "Oort: 100 times the distance, 100 times the time");
        const std::string oort = joined(beltLines("oort"));
        // 0.49 (outside review): the edges are NASA's (Oort Cloud Facts: inner edge 2,000 to 5,000 AU, outer edge 10,000 to 100,000 AU), no longer 200,000 AU (no source);
        // 100,000 AU / 3.5613 AU per year = 28,079 years, two figures: 28,000
        check(has(oort, "about 560 years") && has(oort, "about 28,000 years") && has(oort, "2,000 AU") && has(oort, "100,000 AU") && !has(oort, "200,000"), "Oort card: travel times 560 and 28,000 years next to 2,000 and 100,000 AU");
        check(has(oort, "Inner edge about 2,000 – 5,000 AU, outer edge about 10,000 – 100,000 AU (NASA)"), "Oort card: NASA's edges");
        check(has(oortCaption(), "2,000 to 100,000 AU") && has(oortCaption(), "about 560 years") && !has(oortCaption(), "300 years"), "Journey Oort caption: 560 years from the same distances");
        check(!has(oort, "300 years") && !has(oort, "30,000"), "Oort card: the old 300 and 30,000 are gone");
        check(has(oort, "4.25 light years") && has(oort, "269,000 AU"), "Oort card: Proxima 4.25 light years = 269,000 AU (Gaia DR3 4.2465 x 63,241.077 = 268,553; 2026-11-05 audit: was 4.24)");
        check(probeLaunchYear("v1") == 1977 && probeLaunchYear("v2") == 1977, "probes: launch year 1977");
        // row 1, 5, 10: the notes after a jump
        const std::string simple2060 = dateNotes(true, false, false), real2060 = dateNotes(true, true, false), real2026 = dateNotes(false, true, true);
        check(has(simple2060, "After 2050") && has(simple2060, "extrapolated"), "jump note: Simple after 2050 says extrapolated");
        check(!has(real2060, "After 2050") && !has(real2060, "extrapolated") && !has(real2026, "extrapolated"), "jump note: Real never says extrapolated");
        check(has(real2060, "measured against JPL for 1800 to 1 Jan 2100 only") && has(real2026, "measured against JPL for 1800 to 1 Jan 2100 only"), "jump note: Real says the errors were measured for 1800 to 1 Jan 2100 only");
        check(!has(simple2060, "measured against JPL"), "jump note: Simple does not quote the Real errors");
        for (const std::string& n : {simple2060, real2060, real2026}) check(has(n, "simple orbit fixed at one date") && !has(n, "where they really"), "jump note: dwarf planets and comets are on a simple orbit");
        check(has(simple2060, "Pluto is about 1 AU off in 1800") && !has(real2060, "Pluto is about") && has(real2026, "The dwarf planets move with the planets") && !has(real2026, "dwarf planets and comets"), "jump note: 0.32, Pluto (0.36: every dwarf planet) is on the fixed orbit only in Simple");
        check(has(real2026, "Earth’s Moon and the 13 other big moons are at their real places") && has(real2026, "1800 to 1 Jan 2100"), "jump note: real Moon, with its range");
        check(has(real2060, "only from 1800 to 1 Jan 2100") && !has(real2060, "big moons are at their real places"), "jump note: past the Moon's range says so");
        check(has(dateNotes(false, false, true), "Earth’s Moon and the 13 other big moons are at their real places"), "jump note: 0.46.3, Simple has the real Moon too");
        for (const std::string& n : {simple2060, real2060, real2026}) check(has(n, "real asteroids") && !has(n, "belt rocks start at made-up"), "jump note: 0.46.3, the belt rocks are real asteroids (belts.h)");
        check(has(datePickHint(), "simple orbit") && !has(datePickHint(), "where they really"), "date hint: no 'where they really were'");
        // 0.46.3 (outside review): Haumea's size is the equal-volume diameter of the axes on its card (Proudfoot et al. 2026, PSJ 7, Table 1: semi-axes 1061, 844, 514 km, r_vol 772 km)
        for (std::size_t i = 0; i < kDwarfCount; i++) if (std::string(kDwarfs[i].name) == "Haumea") {
            const double dVol = 2 * std::cbrt(1061.0 * 844.0 * 514.0);
            check(std::fabs(kDwarfs[i].diameterKm - dVol) < 1.0 && std::fabs(kDwarfs[i].diameterKm - 2 * 772) < 1.0, "Haumea: size = equal-volume diameter of its axes (" + std::to_string(dVol) + " km) = 2 x 772 km");
        }
        // 0.46.3 (outside review): the guide's physics paragraph follows the setting: Real does not say "only the Sun's pull"
        check(has(physicsGuideText(true), "and of each other") && !has(physicsGuideText(true), "only the Sun"), "guide: Real says the planets pull on each other");
        check(has(physicsGuideText(false), "do not pull on each other") && has(physicsGuideText(false), "table of approximate orbits") && has(physicsGuideText(false), "Simple"), "guide: Simple says the planets are put by the table and do not pull on each other (0.75.2, on purpose: it no longer says they feel only the Sun, because they are no longer integrated)");
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
        // 0.39.3: an axis tilted past 90 degrees turns backwards; only near 180 (Venus, 177.4 degrees, NASA fact sheet) is it upside down: Uranus (97.8) and Pluto (119.5) lie tipped past their sides
        check(has(cardOf(s, "Venus"), "Upside down, so it turns backwards") && !has(cardOf(s, "Uranus"), "Upside down") && has(cardOf(s, "Uranus"), "turns backwards")
              && !has(cardOf(s, "Pluto"), "Upside down") && has(cardOf(s, "Pluto"), "turns backwards") && !has(cardOf(s, "Earth"), "backwards"),
              "cards: Venus is upside down; Uranus and Pluto are tipped past their sides; all three turn backwards");
        {   // 0.39.3: the card wraps keep a short number with the word after it ("12 Dec 1992", "26 km"): no line of any body's card ends in a 1- or 2-digit number
            for (const Body& b : s.bodies) {
                for (int mi = -1; mi < static_cast<int>(b.moons.size()); mi++) {
                    const std::string cn = mi < 0 ? b.name : b.name + " moon " + std::to_string(mi);
                    Card c; CardOptions o; cardText(s, mi < 0 ? Selection{Selection::Body, b.id} : Selection{Selection::Moon, b.id, mi}, o, c);
                    for (const std::string& ln : c.lines) {
                        const std::size_t sp = ln.rfind(' ');
                        const std::string last = sp == std::string::npos ? ln : ln.substr(sp + 1);
                        const bool shortNumber = !last.empty() && last.size() <= 2 && std::all_of(last.begin(), last.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
                        check(!shortNumber, cn + ": no card line ends in a lone short number (" + ln + ")");
                    }
                }
                }
        }
        {   // 0.32: Pluto's card says which model moves it
            SolarSystem sr; sr.loadAt(ref::kEpochMs); sr.setRealPlanets(false); const std::string simplePluto = cardOf(sr, "Pluto");
            sr.setRealPlanets(true); sr.setRelativity(true); const std::string realPluto = cardOf(sr, "Pluto");
            check(has(simplePluto, "simple orbit fixed at one date") && !has(simplePluto, "In Real") && has(realPluto, "In Real") && has(realPluto, "1,908 km") && !has(realPluto, "simple orbit fixed"), "cards: 0.32, Pluto in Real is on the N-body clock, in Simple on its fixed orbit");
            const std::string realCeres = cardOf(sr, "Ceres"), realEris = cardOf(sr, "Eris");   // 0.36: the other dwarf planets are test particles on the clock
            sr.setRealPlanets(false); const std::string simpleCeres = cardOf(sr, "Ceres");
            check(has(simpleCeres, "simple orbit fixed at one date") && has(realCeres, "In Real") && has(realCeres, "9,324 km") && has(realEris, "78 km") && !has(realCeres, "simple orbit fixed"),
                  "cards: 0.36, Ceres and Eris in Real are on the N-body clock (quoted errors), in Simple on their fixed orbits");
        }
        check(has(cardOf(s, "Ceres"), "an estimate, not measured: a bare ball that reflects 30 % of the sunlight") && !has(cardOf(s, "Earth"), "reflects 30 %"), "cards: the no-atmosphere temperature says it assumes 30 % reflected (Earth's is the real one)");
        Card sc; CardOptions so; cardText(s, Selection{Selection::Star}, so, sc);
        const std::string sun = joined(sc.lines);
        check(has(sun, "Habitable zone (Kopparapu et al. 2014)") && has(sun, "Frost line (rough)") && has(sun, "Lifetime (rough)"), "Sun card: frost line and lifetime are marked rough; the habitable zone names its source (0.49.4, was rough)");
        check(!has(cardOf(s, "Earth", 0), "× our Moon") && has(cardOf(s, "Earth", 0), "Radius: 1,737.4 km"), "Moon card: no comparison with itself");
        check(has(cardOf(s, "Jupiter", 0), "× our Moon"), "Io card: still compared with our Moon");
    }

    // 15) 0.10.5 stage 3, Orbit Lab data fixes (audit rows 3, 4, 12). Source numbers typed from the pages named next to them.
    {
        // row 3: planet surface gravity, JPL "Planetary Physical Parameters" (https://ssd.jpl.nasa.gov/planets/phys_par.html, read 2026-10-03), column "Equatorial Gravity" (m s-2),
        // defined on that page as "the gravitational acceleration on the planet's surface at the equator". Mercury .. Neptune:
        const double kJplGravity[8] = {3.70, 8.87, 9.80, 3.71, 24.79, 10.44, 8.87, 11.15};
        for (int i = 0; i < 8; i++) {
            check(kPlanets[i].gravityMS2 == kJplGravity[i], std::string("gravity of ") + kPlanets[i].name + " is JPL's value");
            SolarSystem g; g.loadAt(ref::kEpochMs);
            near(g.bodies[i].gravityG, kJplGravity[i] / 9.80665, 1.0, 1e-15, std::string("gravity in g of ") + kPlanets[i].name + " = its m/s2 over standard gravity 9.80665 (0.46.3; was Earth's 9.80)");
        }
        // 0.46.3 (outside review): what you would feel at the equator, gravity minus the spin's push. NASA NSSDC planetary fact sheets (data/nssdc/<planet>fact.html, read 2026-11-08),
        // "Surface acceleration (eq.)" (giants: "Acceleration (eq., 1 bar)"), m/s2, Mercury .. Neptune:
        const double kNasaFelt[8] = {3.70, 8.87, 9.780, 3.69, 23.12, 8.96, 8.69, 11.00};
        for (int i = 0; i < 8; i++) check(kPlanets[i].feltMS2 == kNasaFelt[i], std::string("felt acceleration of ") + kPlanets[i].name + " is NASA's value");
        // and a physics check of NASA's numbers: to first order in J2, felt = GM/R^2 (1 + 1.5 J2) - omega^2 R at the equator (textbook rotating-oblate-planet formula), with GM/R^2 the JPL
        // gravity above and the sidereal day and J2 from the same NASA fact sheets: Saturn 10.656 h, J2 16,298e-6, R 60,268 km; Jupiter 9.9250 h, J2 14,736e-6, R 71,492 km
        for (const auto& r : {std::make_tuple(5, 10.656, 60268.0, 16298e-6), std::make_tuple(4, 9.9250, 71492.0, 14736e-6)}) {
            const int i = std::get<0>(r); const double w = 2 * units::kPi / (std::get<1>(r) * 3600), cf = w * w * std::get<2>(r) * 1000;
            const double est = kJplGravity[i] * (1 + 1.5 * std::get<3>(r)) - cf;
            std::printf("  felt at the equator, %s: GM/R2 (1 + 1.5 J2) - w2 R = %.3f m/s2, NASA %.2f\n", kPlanets[i].name, est, kNasaFelt[i]);
            check(std::fabs(est - kNasaFelt[i]) < 0.15, std::string("felt = gravity (with J2) - centrifugal within 0.15 m/s2 for ") + kPlanets[i].name);
        }
        SolarSystem g; g.loadAt(ref::kEpochMs);
        const auto massLine = [&](const char* n) { for (const Body& b : g.bodies) if (b.name == n) { Card c; CardOptions o; cardText(g, Selection{Selection::Body, b.id}, o, c); return c.lines[1]; } return std::string(); };
        check(massLine("Jupiter").find("you would feel 2.36 g at the equator") != std::string::npos, "Jupiter card: " + massLine("Jupiter"));
        check(massLine("Saturn").find("you would feel 0.91 g at the equator") != std::string::npos, "Saturn card (0.46.3: lighter than Earth): " + massLine("Saturn"));
        check(massLine("Earth").find("you would feel 1.00 g at the equator") != std::string::npos && massLine("Mars").find("you would feel 0.38 g at the equator") != std::string::npos, "Earth and Mars cards say the felt gravity is at the equator");
        // row 4: the Sun's radius, JPL Horizons' Sun header (API, COMMAND='10', OBJ_DATA='YES', read 2026-10-03):
        //   "Vol. mean radius, km  = 695700" and "Solar radius (IAU2015)= 695700 km"
        near(units::kSunRadius, 695700.0 * 1000, 695700.0 * 1000, 0, "Sun radius = 695,700 km (IAU 2015 nominal)");
        Card sc; CardOptions so; cardText(g, Selection{Selection::Star}, so, sc);
        bool radius = false; for (const auto& l : sc.lines) if (l.find("Radius: 1.00 Suns (695,700 km)") != std::string::npos) radius = true;
        check(radius, "Sun card: Radius: 1.00 Suns (695,700 km)");
        // row 9: the Simple note's list of worst errors (kSimpleErrKm in orbit/real_errors.h; tests/test_jpl.cpp checks them against the measured maxima), in million km
        check(simpleWorstErrorList() == "Saturn 8.0, Uranus 2.9, Neptune 2.3, Jupiter 1.8, Mars 0.09, Earth 0.016, Venus 0.014, Mercury 0.0076", "Simple note: worst errors, million km: " + simpleWorstErrorList());
        // row 12: each comet's elements are exactly the record named in tests/reference/comet_table.inc, and its place is measured against Horizons at two dates
        // (the record's perihelion time and 2026-01-01 12:00 TDB). The guards are regression guards: about 1 % above the error measured when they were set, NOT physical bounds.
        // Measured on Linux GCC when set (0.10.5 stage 4, with the IAU au; km, at tp / at 2026-01-01): Halley 5,175,018 / 26,983,131; Hale-Bopp 9,704,149 / 1,161,789; NEOWISE 1 / 1,226,153;
        // Encke 8,947 / 273,680; 67P 0 / 1,712,840; Swift-Tuttle 1,228,869 / 8,993,217. Where the error is
        // about 0 the guard is a floor of 100 km, for rounding differences between compilers. They are large elsewhere because each comet is ONE fixed two-body orbit from the record's epoch
        // (Halley's is 1968). The old elements (stage 2) gave 4.6 / 48.8, 9.7 / 1.7, 0.05 / 2.8, 0.18 / 2.8, 14.4 / 61.8, 1.2 / 17.8 million km.
        const double kGuardKm[6][2] = {{5226769, 27252963}, {9801191, 1173407}, {100, 1238415}, {9037, 276417}, {100, 1729969}, {1241158, 9083150}};
        int ci = 0;
        for (const CometRecord& r : kCometRecords) {
            const CometData* c = nullptr;
            for (std::size_t k = 0; k < kCometCount; k++) if (std::string(kComets[k].name) == r.name) c = &kComets[k];
            if (!c) { check(false, std::string("comet in the lab: ") + r.name); ci++; continue; }
            const std::string tag = std::string("comet ") + r.name + " (record " + r.record + ")";
            check(c->qAU == r.q && c->e == r.e && c->inclDeg == r.i && c->nodeDeg == r.om && c->periDeg == r.w && c->perihelionJD == r.tp && c->periodDays == r.per, tag + ": the lab's elements are exactly the record's");
            for (int k = 0; k < 2; k++) {
                const State st = cometState(*c, r.jd[k]);
                const double err = length(st.pos - Vec3{r.x[k] * 1000, r.y[k] * 1000, r.z[k] * 1000}) / 1000;   // km
                std::printf("  %s vs Horizons at JD %.3f: %.0f km (%.4f au; guard %.0f)\n", tag.c_str(), r.jd[k], err, err / 149597870.7, kGuardKm[ci][k]);
                check(err <= kGuardKm[ci][k], tag + ": position at JD " + std::to_string(r.jd[k]) + " within the regression guard");
            }
            ci++;
        }
        check(ci == 6, "comets: all six records tested");
    }

    // 16) 0.10.5 stage 5: Orbit Lab tests against independent references (audit rows 20-23) and the message logic that moved out of the app target into orbit/.
    {
        const auto ms = [](double jd) { return static_cast<int64_t>(timescale::unixMsOfTDB(jd)); };   // Horizons' dates are TDB; 0.46.3: the lab's clock is UT (orbit/time_scale.h)
        const auto has = [](const std::string& hay, const char* w) { return hay.find(w) != std::string::npos; };
        const auto vecOf = [](const char* body, const EventVector*& out) { for (const EventVector& e : kEventVectors) if (std::string(e.body) == body) { out = &e; return true; } return false; };
        const EventVector *mars = nullptr, *jup = nullptr, *sat = nullptr;
        check(vecOf("Mars", mars) && vecOf("Jupiter", jup) && vecOf("Saturn", sat), "Horizons event vectors present");
        // row 21: Real mode against Horizons (events_table.inc). These replace the old checks that allowed 0.3 million km (Mars-Earth 27 Aug 2003) and 0.05 degrees (Jupiter-Saturn 21 Dec 2020).
        // The limits are regression guards, about 1 % above the error measured when they were set (Linux GCC), NOT physical bounds.
        {
            SolarSystem r; r.setRealPlanets(true); r.loadAt(ms(mars->jd));
            const auto pos = [](const SolarSystem& sys, const char* n) { for (const Body& b : sys.bodies) if (b.name == n) return b.pos; return Vec3{}; };
            const Vec3 d = pos(r, "Mars") - pos(r, "Earth");
            const double errKm = length(d - Vec3{mars->x * 1000, mars->y * 1000, mars->z * 1000}) / 1000, distMkm = length(d) / 1e9;
            std::printf("  Real, Mars from Earth, 27 Aug 2003 10:00: %.3f million km (Horizons %.3f), error %.0f km\n", distMkm, length(Vec3{mars->x, mars->y, mars->z}) / 1e6, errKm);
            // measured 326 km (0.29, the accurate step, relativity off as SolarSystem's own default; 0.28: 1,219 km) (Mars-Earth distance 55.758 million km)
            check(errKm <= 330, "Real: Mars as seen from Earth on 27 Aug 2003 within the regression guard of Horizons' vector");
            SolarSystem q; q.setRealPlanets(true); q.loadAt(ms(jup->jd));
            const Vec3 j = pos(q, "Jupiter") - pos(q, "Earth"), sa = pos(q, "Saturn") - pos(q, "Earth");
            const Vec3 hj{jup->x, jup->y, jup->z}, hs{sat->x, sat->y, sat->z};
            const double deg = 180 / units::kPi, sep = std::acos(dot(j, sa) / (length(j) * length(sa))) * deg, hsep = std::acos(dot(hj, hs) / (length(hj) * length(hs))) * deg;
            std::printf("  Real, Jupiter-Saturn, 21 Dec 2020 12:00: %.5f degrees (Horizons %.5f), error %.5f arcsec\n", sep, hsep, std::fabs(sep - hsep) * 3600);
            // measured 0.01348 arcsec (0.29, the accurate step, relativity off; 0.28: 0.00244, its integrator error happening to cancel); guard about 1 % above
            check(std::fabs(sep - hsep) * 3600 <= 0.0137, "Real: the Jupiter-Saturn angle on 21 Dec 2020 within the regression guard of Horizons'");
        }
        // row 21, "Earth back after one year". 0.75.2 (on purpose): Simple no longer integrates the planets round the Sun (running time places each planet from JPL's table at every date, as a jump does),
        // so the old reference, the leapfrog returning after the Kepler period P = 2 pi sqrt(a^3 / GM), no longer describes what the code does. The new reference is the table's own: Earth's mean longitude
        // comes back after 360 degrees / (its rate per century) = 36525 x 360 / 35999.37244981 days (JPL's table, data/jpl/approx_pos_2026-11-23.html), and the other elements move by a few thousandths of a
        // degree a year, so the planet must be within a few hundred km of its start (measured below). The table's own mean motion and its a differ by 7e-6 (the old check measured that as 6,958 km).
        {
            SolarSystem s; s.loadAt(ref::kEpochMs);
            const Vec3 p0 = s.bodies[2].pos - s.bodies[2].drawShift;                             // 0.52.1: Earth's orbit point, the barycentre (Earth itself is drawn off it)
            const double P = 36525.0 * 360.0 / 35999.37244981 * 86400.0;                         // s, the table's period of the mean longitude
            const double realSeconds = P / units::kSimTime;                                       // at Time speed 0: 1 real second = kSimTime
            const int steps = static_cast<int>(realSeconds / units::kPhysicsStep);
            for (int k = 0; k < steps; k++) s.step(units::kPhysicsStep, 0.0);
            s.step(realSeconds - steps * units::kPhysicsStep, 0.0);
            const double backKm = length(s.bodies[2].pos - s.bodies[2].drawShift - p0) / 1000;
            std::printf("  Simple: Earth after one period of the table's mean longitude (%.4f years) is %.2f km from its start\n", P / units::kYear, backKm);
            check(backKm <= 1000, "Simple: Earth is back at its start after one period of the table's mean longitude (the table's own reference)");
        }
        // row 22: the 2026 equinoxes. "Next season starts in N days" must land on Horizons' equinox within 0.2 hours, in Simple and in Real, from five start dates.
        {
            const struct { double jd; int which; } starts[] = {{2461042.0, 0}, {2461087.0, 0}, {2461101.0, 0}, {2461254.0, 1}, {2461294.0, 1}};
            double worst = 0;
            for (int real = 0; real < 2; real++)
                for (const auto& st : starts) {
                    SolarSystem s; s.setRealPlanets(real == 1); s.loadAt(ms(st.jd));
                    for (const Body& b : s.bodies) if (b.name == "Earth") {
                        EarthSeason q;
                        check(earthSeason(b, s, q), "equinox: season known");
                        const double hours = (st.jd + q.until - kEquinoxJD[st.which]) * 24;
                        worst = std::fmax(worst, std::fabs(hours));
                        check(std::fabs(hours) <= 0.2, std::string(real ? "Real" : "Simple") + ": next season from JD " + std::to_string(st.jd) + " lands on the equinox within 0.2 h (" + std::to_string(hours) + " h)");
                    }
                }
            std::printf("  equinoxes 2026: worst difference from Horizons %.3f hours (limit 0.2)\n", worst);
        }
        // row 23, moved into orbit/ (no ImGui) so they can be tested: dates, jump message, Time speed text, the date line, the hidden-selection rule, the Voyager caption
        int64_t t = 0;
        check(parseDateTime("2003-08-27", "12:00", t) && t == 1061985600000LL, "parseDateTime: 27 Aug 2003 12:00 UT = 1061985600000 ms (Mars-Earth test above: 1061942400000 is 00:00)");
        check(parseDateTime("2003-08-27", "", t) && t == 1061985600000LL, "parseDateTime: no time means 12:00");
        check(!parseDateTime("2003-02-30", "12:00", t) && !parseDateTime("2003-13-01", "12:00", t) && !parseDateTime("2003-08-27", "24:00", t) && !parseDateTime("2003-08-27", "12:00:30", t) && !parseDateTime("27 Aug 2003", "", t) && !parseDateTime("2003-08-27x", "", t), "parseDateTime: 30 February, month 13, 24:00, seconds and other shapes are refused");
        check(parseDateTime("2024-02-29", "00:00", t) && !parseDateTime("2023-02-29", "00:00", t), "parseDateTime: 29 February only in leap years");
        // 0.19 audit: a year so far away that its milliseconds do not fit in 64 bits (the box holds 15 characters) used to overflow (undefined; it came out as a date
        // before 1800, so the note said "before 1800"); a far but representable year still reads (and the jump is then clamped to 2100 with its note)
        check(!parseDateTime("999999999-12-31", "", t), "parseDateTime: the year 999,999,999 is not read (it would overflow)");
        check(parseDateTime("100000-01-01", "", t) && t > kDateMaxMs, "parseDateTime: the year 100,000 reads, after the lab's last date");
        std::string clampNote;
        check(clampJumpDate(1061985600000LL, clampNote) == 1061985600000LL && clampNote.empty(), "clampJumpDate: a date inside the range is kept, no note");
        const int64_t before = clampJumpDate(-5364662400001LL, clampNote);
        check(before == kDateMinMs && has(clampNote, "outside 1800–2100") && has(clampNote, "Showing the nearest date it can: 1 Jan 1800"), "clampJumpDate: 1799 is moved to 1 Jan 1800, with a note: " + clampNote);
        clampNote.clear();
        check(clampJumpDate(4133894400000LL + 86400000LL, clampNote) == kDateMaxMs && has(clampNote, "31 Dec 2100"), "clampJumpDate: 2101 is moved to the end of 2100");
        check(jumpMessage("", 1061985600000LL, false, false).rfind("Showing 27 Aug 2003. ", 0) == 0, "jump message: starts with Showing <date>");
        check(has(jumpMessage("", kTableEndMs + 86400000LL, false, false), "After 2050") && !has(jumpMessage("", kTableEndMs, false, false), "After 2050"), "jump message: the 2050 note starts the day after 31 Dec 2050");
        check(tspeedText(0.0) == "1 s = 58 days" && tspeedText(units::kTspeedMin) == "1 s = 1 second (real time)" && tspeedText(units::kTspeedMax) == "1 s = 50 years" && tspeedText(-3.145) == "1 s = 60 minutes",
              "Time speed text: 58.13 days at 0, real time at the slowest, 18,383 days = 50 years at the fastest, 3,597 s = 59.95 minutes at the Journey's: " + tspeedText(0.0) + " / " + tspeedText(units::kTspeedMin) + " / " + tspeedText(units::kTspeedMax) + " / " + tspeedText(-3.145));
        {
            SolarSystem d; d.loadAt(ref::kEpochMs);
            check(dateLine(d) == "Solar System today, 1 Jan 2026", "date line: today: " + dateLine(d));
            d.jumped = true; d.loadAt(static_cast<int64_t>(std::llround((2459205.0 - 2440587.5) * 86400000.0)));   // 21 Dec 2020 12:00 UT (the date line shows UT)
            check(dateLine(d) == "Solar System on 21 Dec 2020, 12:00 UT", "date line: after a jump it also shows the time of day: " + dateLine(d));
            SolarSystem e; e.loadAt(ref::kEpochMs); e.t = units::kYear;
            check(dateLine(e) == "Solar System on 1 Jan 2027  ·  1.00 years after 1 Jan 2026", "date line: one Julian year later: " + dateLine(e));
        }
        {
            SolarSystem v; v.loadAt(ref::kEpochMs);
            int ceres = 0, halley = 0, earth = 0; for (const Body& b : v.bodies) { if (b.name == "Ceres") ceres = b.id; if (b.name == "Halley") halley = b.id; if (b.name == "Earth") earth = b.id; }
            SelectionSwitches all;
            check(selectionStillValid(v, Selection{Selection::Body, ceres}, all) && selectionStillValid(v, Selection{Selection::Star}, all) && selectionStillValid(v, Selection{Selection::Bary}, all), "selection: a visible body, the Sun and the barycentre stay selected");
            SelectionSwitches noDwarf = all; noDwarf.dwarfs = false;
            check(!selectionStillValid(v, Selection{Selection::Body, ceres}, noDwarf) && selectionStillValid(v, Selection{Selection::Body, earth}, noDwarf), "selection: Dwarf planets off drops Ceres, not Earth");
            SelectionSwitches noComet = all; noComet.comets = false;
            check(!selectionStillValid(v, Selection{Selection::Body, halley}, noComet) && selectionStillValid(v, Selection{Selection::Body, halley}, all), "selection: Comets off drops Halley");
            SelectionSwitches noMoon = all; noMoon.moons = false;
            check(selectionStillValid(v, Selection{Selection::Moon, earth, 0}, all) && !selectionStillValid(v, Selection{Selection::Moon, earth, 0}, noMoon) && !selectionStillValid(v, Selection{Selection::Moon, earth, 1}, all) && !selectionStillValid(v, Selection{Selection::Moon, earth, -1}, all), "selection: Moons off, or a moon number the planet does not have, drops the moon (Earth has one)");
            check(!selectionStillValid(v, Selection{Selection::Body, 9999}, all) && !selectionStillValid(v, Selection{Selection::Moon, 9999, 0}, all), "selection: a body that is gone is dropped");
            SelectionSwitches noBelt = all; noBelt.belts = false;
            check(selectionStillValid(v, Selection{Selection::Belt, 0, 3}, all) && !selectionStillValid(v, Selection{Selection::Belt, 0, 4}, all) && !selectionStillValid(v, Selection{Selection::Belt, 0, 3}, noBelt), "selection: a belt needs Belts on and a belt number below 4");
            SelectionSwitches noFar = all; noFar.far = false;
            const int v1 = farIndexOf("v1");
            check(selectionStillValid(v, Selection{Selection::Far, 0, v1}, all) && !selectionStillValid(v, Selection{Selection::Far, 0, v1}, noFar) && !selectionStillValid(v, Selection{Selection::Far, 0, 9999}, all), "selection: a far-away thing needs Far-away on and a real number");
            SolarSystem old; old.loadAt(-5364662400000LL);   // 1800: the Voyagers are not shown before 1990
            check(!selectionStillValid(old, Selection{Selection::Far, 0, v1}, all), "selection: Voyager 1 is dropped on a date before 1990");
        }
        {
            SolarSystem old; old.loadAt(ms(2444239.5));   // 1 Jan 1980
            check(has(voyagerCaption(old), "launched in 1977") && has(voyagerCaption(old), "from 1990 on"), "Voyager caption in 1980: " + voyagerCaption(old));
            SolarSystem old2; old2.loadAt(ms(2430000.5));  // 1941
            check(has(voyagerCaption(old2), "not launched yet"), "Voyager caption in 1941: not launched yet");
            SolarSystem now; now.loadAt(ref::kEpochMs);
            check(has(voyagerCaption(now), "Voyager 1 is the most distant spacecraft") && has(voyagerCaption(now), "121 AU in 2012"), "Voyager caption today: " + voyagerCaption(now));
        }
        {   // 0.49 (outside review): the Voyagers before 30 Sep 2026 are JPL Horizons' places, not the 2026 straight line run backwards. Independent references:
            // NASA (data/jpl/voyager/nasa_voyager1_read_2026-11-11.html): on 17 Feb 1998 Voyager 1 passed Pioneer 10 at 69.4 AU; Voyager 2 flew past Neptune (30 AU) on
            // 25 Aug 1989 in the planets' plane, so on 1 Jan 1990 it is beyond 30 AU and close to the ecliptic (before 0.49: 27.4 AU and 38.5° south).
            const int v1 = farIndexOf("v1"), v2 = farIndexOf("v2");
            FarItem f1, f2;
            SolarSystem pass; pass.loadAt(887673600000LL);                        // 17 Feb 1998 00:00 UTC
            check(farItem(pass, v1, f1) && std::fabs(f1.au - 69.4) < 0.1, "Voyager 1 on 17 Feb 1998 is 69.4 AU from the Sun (NASA: where it passed Pioneer 10): " + std::to_string(f1.au));
            SolarSystem y90; y90.loadAt(631152000000LL);                          // 1 Jan 1990 00:00 UTC
            const bool ok2 = farItem(y90, v2, f2);
            check(ok2 && f2.au > 30.5 && f2.au < 32 && std::fabs(f2.dir.z) < std::sin(2 * 3.14159265358979 / 180),
                  "Voyager 2 on 1 Jan 1990 is past Neptune (31 AU) and within 2° of the ecliptic: " + std::to_string(f2.au) + " AU, z/r " + std::to_string(f2.dir.z));
            FarItem a, b;
            SolarSystem t0a, t0b; t0a.loadAt(static_cast<int64_t>(kSkyT0Ms) - 60000); t0b.loadAt(static_cast<int64_t>(kSkyT0Ms) + 60000);
            check(farItem(t0a, v1, a) && farItem(t0b, v1, b) && length(a.pos - b.pos) < 1e-4 * units::kAU, "Voyager 1 does not jump on 30 Sep 2026, where the table ends (2 minutes apart: under 0.0001 AU)");
            SolarSystem y95; y95.loadAt(788918400000LL);                         // 1 Jan 1995
            const std::string cap95 = voyagerCaption(y95);
            check(has(cap95, "Pioneer 10 is still farther away") && !has(cap95, "most distant") && has(cap95, "will leave the Sun’s wind"), "Voyager caption in 1995: not yet the most distant, not yet out: " + cap95);
            Card card;
            FarItem f95;
            check(farItem(y95, v1, f95), "Voyager 1 is shown in 1995");
            farCard(f95, card);
            bool will = false;
            for (const std::string& l : card.lines) will = will || has(l, "it will leave the Sun’s wind");
            check(will, "Voyager 1's card in 1995 says it WILL leave the Sun's wind (in 2012)");
        }
        // the two Planet physics notes (the text moved to orbit/real_errors.h; its length is kept inside what showed as 10 lines on 2026-10-03, 1280 x 800, normal text size)
        const std::string realOff = realNoteText(false), realOn = realNoteText(true), simple = simpleNoteText();
        check(has(realOff, "relativity off: Mercury 40,962, Venus 20,309, Earth 13,726, Mars 9,286, Jupiter 728, Saturn 128, Uranus 298, Neptune 1,506 km.") && has(realOff, "Pluto 1,889 km") && has(realOff, "Moon 5.2 km") && has(realOff, "1800 to 1 Jan 2100"), "Real note: the measured errors without relativity: " + realOff);
        check(has(realOn, "relativity on: Mercury 25, Venus 5, Earth 14, Mars 125, Jupiter 378, Saturn 360, Uranus 134, Neptune 1,436 km.") && has(realOn, "Pluto 1,908 km"), "Real note: the measured errors with relativity: " + realOn);
        std::printf("  Real note lengths: %zu / %zu characters (off / on), Simple note %zu\n", realOff.size(), realOn.size(), simple.size());
        // 444 characters (relativity off) showed as 10 lines on 2026-10-03 (1280 x 800, normal text size, about 43 characters a line); 11 lines would be about 470. Guard: 460.
        check(realOff.size() <= 460 && realOn.size() <= 460, "Real note: no longer than the text that showed as 10 lines, plus a little (guard in characters)");
        // (0.75.2, on purpose: Simple no longer calls the orbits fixed: each planet is put where the table says at the date; the note says so)
        check(has(simple, "table of approximate elements") && has(simple, "Saturn 8.0") && has(simple, "1800-2050"), "Simple note: the table and the measured errors: " + simple);
    }

    // 17) 0.10.5 stage 5, row 18: numbers with a named source, and the ones marked as having none
    {
        const auto has = [](const std::string& hay, const char* w) { return hay.find(w) != std::string::npos; };
        // c: JPL astro_par page, "speed of light in vacuum c 299792458 m/s 2018 CODATA recommended values" (read 2026-10-03)
        check(units::kLightSpeed == 299792458.0, "c = 299,792,458 m/s (JPL astro_par page)");
        // a light year = c x one Julian year (365.25 d = 31,557,600 s) = 9,460,730,472,580,800 m (by hand: 299,792,458 x 31,557,600), over the au of 149,597,870,700 m
        near(units::kLightYearAU, 9460730472580800.0 / 149597870700.0, 63241, 1e-14, "a light year in AU = c x 31,557,600 s / au = 63,241.077");
        // the Sun's turning: Horizons' Sun header, "Adopted sid. rot. per.= 25.38 d" (API, COMMAND='10', read 2026-10-03); the lab's rate is the IAU's 14.1844 degrees a day
        near(360.0 / spinRateDegPerDay("Sun"), 25.38, 25.38, 2e-4, "Sun's standard turning period 25.38 days (Horizons' header)");
        // comets' nucleus sizes: JPL SBDB "diameter" (read 2026-10-03): Halley 11.0, Hale-Bopp 60, Encke 4.8, 67P 3.4, Swift-Tuttle 26; none for NEOWISE
        const struct { const char* name; double km; bool rough; } sizes[] = {{"Halley", 11.0, false}, {"Hale–Bopp", 60, false}, {"Encke", 4.8, false}, {"67P", 3.4, false}, {"Swift–Tuttle", 26, false}, {"NEOWISE", 5, true}};
        SolarSystem s; s.loadAt(ref::kEpochMs);
        for (const auto& z : sizes) {
            const CometData* c = nullptr; for (std::size_t k = 0; k < kCometCount; k++) if (std::string(kComets[k].name) == z.name) c = &kComets[k];
            check(c && c->nucleusKm == z.km && c->nucleusRough == z.rough, std::string("comet ") + z.name + ": nucleus " + std::to_string(z.km) + " km from SBDB" + (z.rough ? " (none: marked rough)" : ""));
            for (const Body& b : s.bodies) if (b.name == z.name) {
                Card cd; CardOptions o; cardText(s, Selection{Selection::Body, b.id}, o, cd);
                const bool marked = cd.lines[0].find("(rough: JPL gives no size)") != std::string::npos;
                check(marked == z.rough, std::string("comet card ") + z.name + ": the nucleus line " + (z.rough ? "says rough" : "does not say rough") + ": " + cd.lines[0]);
            }
        }
        Card sc; CardOptions so; cardText(s, Selection{Selection::Star}, so, sc);
        std::string sun; for (const auto& l : sc.lines) sun += l + "\n";
        check(has(sun, "Surface (rough): about 5,770 K") && has(sun, "The 25.38 days is JPL's standard rate; the 25 and 34 days are rough"), "Sun card: surface temperature rough; 25.38 days is JPL's, 25 and 34 rough");
        bool beltsOk = true;
        for (const char* k : {"main", "trojan", "kuiper", "oort"}) beltsOk = beltsOk && beltLines(k).back() == "The counts and masses above are rough figures (no named source)";
        check(beltsOk, "belt cards (asteroid, Trojans, Kuiper, Oort): the last line says the counts and masses are rough");
    }

    std::printf("%d checks, %d failed.\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
