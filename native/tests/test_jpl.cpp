// 0.10.3: the Real planets (orbit/nbody.*) checked against the JPL Horizons vector tables in native/data/jpl (the independent reference).
// Stage 1 measured it with the JPL state passed to the clock by the test; since stage 2 the JPL state IS the clock's (and the app's) default start, so the
// "JPL" runs below use the default clock. The old orbital-elements start is the legacy function nbodyElementsStartState, kept for the before-and-after column.
// The tables say {source: DE441}: Horizons' default ephemeris (spans -13200 to +17191; JPL says it is a little less accurate than DE440 for the present
// century, mainly for the Moon). The lab's GM values are DE440's. So this is a comparison with Horizons' DE441 output, NOT "against DE440".
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "au_legacy.h"
#include "orbit/time_scale.h"
#include "orbit/nbody.h"
#include "orbit/real_errors.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"

using namespace orbit;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }

// All rows of the 9 bodies (Sun, Mercury ... Neptune; Earth = Earth-Moon barycentre): { JD (TDB), x, y, z (m), vx, vy, vz (m/s) }, 1 Jan 12:00 of 1800 .. 2100.
// Barycentric (JPL's Solar System Barycenter), ecliptic and mean equinox of J2000. Made by tools/make_jpl_data.py from the files in data/jpl.
static const int kRows = 301, kFirstYear = 1800, kEpochRow = 2026 - kFirstYear;
static const double kJplTable[kNBodies][kRows][7] =
#include "reference/jpl_table.inc"
;
// Only the 2026-01-01 12:00 TDB row (the epoch kNBodyRefJD), { {x, y, z}, {vx, vy, vz} }: made by the same tool, for the app to use in stage 2.
static const double kJplEpoch[kNBodies][2][3] =
#include "orbit/jpl_epoch.inc"
;

static const double kAuIAU = 149597870700.0;                       // m, IAU 2012 Resolution B2 (exact)
static const double kArcsecPerRad = 180.0 / units::kPi * 3600;
static const char* kNames[kNBodies] = {"Sun", "Mercury", "Venus", "Earth(EMB)", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
static const int kShow[5] = {1800, 1900, 2000, 2050, 2100};

static Vec3 tv(int b, int row, int off) { return Vec3{kJplTable[b][row][off], kJplTable[b][row][off + 1], kJplTable[b][row][off + 2]}; }
static bool isLeapGregorian(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// ---- (a) the data ----
static void testData() {
    bool epochSame = true;
    for (int b = 0; b < kNBodies; b++) {
        const Vec3 p = tv(b, kEpochRow, 1), v = tv(b, kEpochRow, 4);
        epochSame = epochSame && kJplTable[b][kEpochRow][0] == kNBodyRefJD && p.x == kJplEpoch[b][0][0] && p.y == kJplEpoch[b][0][1] && p.z == kJplEpoch[b][0][2] &&
                    v.x == kJplEpoch[b][1][0] && v.y == kJplEpoch[b][1][1] && v.z == kJplEpoch[b][1][2];
    }
    check("data: jpl_epoch.inc equals the 2026 row of the table exactly, and that row's Julian date is kNBodyRefJD = 2461042.0", epochSame);

    bool gaps = true, gapsLeap = true, same = true, up = true;
    for (int b = 0; b < kNBodies; b++)
        for (int r = 0; r + 1 < kRows; r++) {
            const double gap = kJplTable[b][r + 1][0] - kJplTable[b][r][0];
            gaps = gaps && (gap == 365.0 || gap == 366.0);
            gapsLeap = gapsLeap && gap == (isLeapGregorian(kFirstYear + r) ? 366.0 : 365.0);   // 1 Jan y to 1 Jan y+1 holds 29 Feb of y only if y is a leap year
            up = up && gap > 0;
            same = same && kJplTable[b][r][0] == kJplTable[0][r][0];
        }
    check("data: every Julian-date gap is 365 or 366 days", gaps);
    check("data: the gap is 366 days exactly when the year is a Gregorian leap year (1900 not, 2000 yes)", gapsLeap);
    check("data: all 9 files have the same Julian dates, in increasing order", same && up);

    double mercMin = 1e99, mercMax = 0, earthMin = 1e99, earthMax = 0;
    for (int r = 0; r < kRows; r++) {
        const double dm = length(tv(1, r, 1) - tv(0, r, 1)) / kAuIAU, de = length(tv(3, r, 1) - tv(0, r, 1)) / kAuIAU;
        mercMin = std::min(mercMin, dm); mercMax = std::max(mercMax, dm); earthMin = std::min(earthMin, de); earthMax = std::max(earthMax, de);
    }
    std::printf("data: Mercury is %.4f to %.4f au from the Sun and the Earth-Moon barycentre %.4f to %.4f au in the 301 rows (au = 149,597,870,700 m)\n", mercMin, mercMax, earthMin, earthMax);
    check("data: Mercury 0.30 to 0.47 au from the Sun in every row", mercMin > 0.30 && mercMax < 0.47);
    check("data: Earth-Moon barycentre 0.98 to 1.02 au from the Sun in every row", earthMin > 0.98 && earthMax < 1.02);
}

// ---- the JPL epoch state moved to the lab's own 9-body centre of mass (test path only) ----
static NBodyState g_jplStart;
static void makeJplStart() {
    NBodyState s;                                                             // from the 2026 row of the TABLE (not from jpl_epoch.inc, which the app uses)
    for (int i = 0; i < kNBodies; i++) { s.x[i] = tv(i, kEpochRow, 1); s.v[i] = tv(i, kEpochRow, 4); }
    const Vec3 c = nbodyCentreOfMass(s);                                      // sum GM_i x_i / sum GM_i with the lab's GMs
    double total = 0; for (int i = 0; i < kNBodies; i++) total += kRealGM[i];
    const Vec3 w = nbodyMomentum(s) * (1.0 / total);                          // sum GM_i v_i / sum GM_i
    std::printf("shift: JPL's barycentre -> the lab's 9-body centre of mass (GM-weighted mean of the Sun and 8 planet systems, lab GMs): %.1f km and %.3f mm/s "
                "(JPL's barycentre includes Pluto, the asteroids and so on, which the lab has not)\n", length(c) / 1e3, length(w) * 1e3);
    for (int i = 0; i < kNBodies; i++) { s.x[i] = s.x[i] - c; s.v[i] = s.v[i] - w; }
    const Vec3 c2 = nbodyCentreOfMass(s), p2 = nbodyMomentum(s);
    check("shift: afterwards the lab's centre of mass is at the origin (< 1 m) and its momentum zero (< 1e-6 m/s per unit GM)", length(c2) < 1.0 && length(p2) / total < 1e-6);
    // the heliocentric differences, which is what is compared below, do not change
    double dh = 0; for (int i = 1; i < kNBodies; i++) dh = std::max(dh, length((s.x[i] - s.x[0]) - (tv(i, kEpochRow, 1) - tv(0, kEpochRow, 1))));
    check("shift: heliocentric positions are unchanged by the shift (< 1e-3 m, rounding only)", dh < 1e-3);
    g_jplStart = s;

    // (a) 0.10.3 stage 2: the app's default start (what NBodyClock() and the app use) is this state: JPL's epoch row minus the shift
    const NBodyState d = nbodyStartState();
    double dpos = 0, dvel = 0, dhel = 0, dhelv = 0;
    for (int i = 0; i < kNBodies; i++) { dpos = std::max(dpos, length(d.x[i] - s.x[i])); dvel = std::max(dvel, length(d.v[i] - s.v[i])); }
    for (int i = 1; i < kNBodies; i++) {
        dhel = std::max(dhel, length((d.x[i] - d.x[0]) - (tv(i, kEpochRow, 1) - tv(0, kEpochRow, 1))));
        dhelv = std::max(dhelv, length((d.v[i] - d.v[0]) - (tv(i, kEpochRow, 4) - tv(0, kEpochRow, 4))));
    }
    std::printf("default start: differs from the JPL row minus the shift by %.1e m and %.1e m/s; heliocentric positions differ from the row by %.1e m, velocities by %.1e m/s\n", dpos, dvel, dhel, dhelv);
    check("default start: heliocentric positions equal the JPL 2026 row to 1e-3 m", dhel < 1e-3);
    check("default start: heliocentric velocities equal the JPL 2026 row to 1e-9 m/s", dhelv < 1e-9);
    check("default start: equals the JPL row minus the shift (positions 1e-3 m, velocities 1e-9 m/s)", dpos < 1e-3 && dvel < 1e-9);
    const Vec3 cm = nbodyCentreOfMass(d), pm = nbodyMomentum(d);
    check("default start: the centre of mass is at the origin (< 1 m) and the momentum is zero (< 1e-6 m/s per unit GM)", length(cm) < 1.0 && length(pm) / total < 1e-6);
}

// ---- (b) nothing changed: the 0.10.2 values (saved from the code before this stage; GCC x86-64), exact binary numbers ----
// A saved result of our own earlier code, so it only shows that nothing changed. Dates: days after kNBodyRefJD.
#include "test_jpl_saved.inc"

static void testNothingChanged() {
    // The integrator and the clock are unchanged: Real from the LEGACY elements start (the 0.10.2 start) still gives the 0.10.2 numbers. (The app's default Real start
    // is now the JPL state, which changes Real on purpose; that is checked against the JPL table below.)
    // 0.29: the clock now uses the accurate step (changed on purpose, checked against JPL below), so the Wisdom-Holman map itself is stepped here directly to the same whole days
    // (the clock's grid states at whole days were exactly these steps, without interpolation)
    const NBodyState elemStart = legacyStartRoundedAu();
    double worstReal = 0, worstSimple = 0;
    for (int d = 0; d < 5; d++) {
        Vec3 x[kNBodies], v[kNBodies];
        const long target = long(kSavedDays[d]);
        // the old clock reached a date from the checkpoint at or below it (every 2,000 steps, walked outwards from the epoch), then stepped forwards
        const long j = target >= 0 ? target / 2000 : -((-target + 1999) / 2000);
        NBodyState s = elemStart;
        for (long q = 0; q != j * 2000; q += (j > 0 ? 1 : -1)) nbodyStep(s, j > 0 ? kNBodyStep : -kNBodyStep, false);
        for (long q = j * 2000; q < target; q++) nbodyStep(s, kNBodyStep, false);
        for (int i = 0; i < kNBodies; i++) { x[i] = s.x[i]; v[i] = s.v[i]; }
        for (int i = 0; i < kNBodies; i++) {
            const double scale = length(x[i == 0 ? 5 : i] - x[0]);            // the planet's distance from the Sun (the Sun: Jupiter's)
            worstReal = std::max(worstReal, length(x[i] - Vec3{kSavedReal[d][i][0], kSavedReal[d][i][1], kSavedReal[d][i][2]}) / scale);
        }
        for (int i = 0; i < 8; i++) {
            const State s = planetStateAt(std::size_t(i), kNBodyRefJD + kSavedDays[d]);
            worstSimple = std::max(worstSimple, length(s.pos - Vec3{kSavedSimple[d][i][0], kSavedSimple[d][i][1], kSavedSimple[d][i][2]}) / length(s.pos));
        }
    }
    std::printf("nothing changed: Real (5 dates) differs from the 0.10.2 values by at most %.1e, Simple (5 dates) by at most %.1e (relative)\n", worstReal, worstSimple);
    // 1e-9: the same compiled code gives 0; another compiler (the Windows build) may round differently, which cannot grow to 1e-9 in 74 years of 1-day steps.
    check("nothing changed: the Wisdom-Holman map from the legacy elements start, 5 dates, equals the 0.10.2 values within 1e-9 relative (the old integrator did not change)", worstReal < 1e-9);
    check("nothing changed: Simple planets at 5 dates equal the 0.10.2 values within 1e-9 relative", worstSimple < 1e-9);
    NBodyClock viaNull(kNBodyStep, false, nullptr), plain;
    const NBodyState def = nbodyStartState(true);        // 0.32: with the Pluto system
    NBodyClock viaDef(kNBodyStep, false, &def);
    Vec3 x[kNBodies], v[kNBodies], y[kNBodies], w[kNBodies];
    viaNull.stateAt(3000 * 86400.0, x, v); plain.stateAt(3000 * 86400.0, y, w);
    bool same = true; for (int i = 0; i < kNBodies; i++) same = same && x[i].x == y[i].x && x[i].y == y[i].y && x[i].z == y[i].z;
    viaDef.stateAt(3000 * 86400.0, y, w);
    for (int i = 0; i < kNBodies; i++) same = same && x[i].x == y[i].x && x[i].y == y[i].y && x[i].z == y[i].z;
    check("default start: a clock given no start state is the clock started from nbodyStartState(true) (the JPL state with the Pluto system), bit for bit", same);
}

// ---- (c) validation ----
enum { kSimple, kElemGR, kJplGR, kJplNoGR, kModels };
static const char* kModelNames[kModels] = {"Simple", "Real from the legacy elements start, GR on (0.10.2)", "Real, default JPL start, GR on", "Real, default JPL start, GR off"};
static double g_km[kModels][8][kRows], g_as[kModels][8][kRows];             // planet 0 = Mercury ... 7 = Neptune
static double g_sunDiff[kRows];                                             // |lab Sun - JPL Sun| (barycentric, km), the JPL-start GR-on run

// The measured maxima over 1800..2100 (km, arcsec) with a margin above them: regression guards only (not physical bounds).
// Regression guards: the largest error 1800..2100 of each model and planet (km; arcsec) with a margin of about 1 % above what was measured in 0.10.3 stage 1
// (GCC, x86-64), rounded up to 3 digits. They only notice a change; they are NOT physical bounds, and the big ones (Real from the elements) are the known drift of the elements start.
static const double kGuardKm[kModels][8] = {
    {   // Simple
        7700,   // regression guard (measured 7617.7 km)
        14000,   // regression guard (measured 13857.2 km)
        16300,   // regression guard (measured 16111.6 km)
        91400,   // regression guard (measured 90492.6 km)
        1.86e+06,   // regression guard (measured 1.83993e+06 km)
        8.09e+06,   // regression guard (measured 8.00076e+06 km)
        2.98e+06,   // regression guard (measured 2.94287e+06 km)
        2.37e+06,   // regression guard (measured 2.34148e+06 km)
    },
    {   // Real from elements, GR on (0.29: the accurate step; re-measured)
        1.95e+06,   // regression guard (measured 1.92226e+06 km)
        1.15e+06,   // regression guard (measured 1.13623e+06 km)
        6.86e+05,   // regression guard (measured 678280 km)
        3.7e+06,   // regression guard (measured 3.65885e+06 km)
        1.05e+07,   // regression guard (measured 1.0386e+07 km)
        1.13e+08,   // regression guard (measured 1.10924e+08 km)
        4.09e+08,   // regression guard (measured 4.04882e+08 km)
        1.13e+08,   // regression guard (measured 1.11452e+08 km)
    },
    {   // Real from JPL, GR on (0.32: with the Pluto system; re-measured; guards 1 % or 0.5 km, the larger)
        24.5,   // regression guard (measured 23.928 km)
        4.78,   // regression guard (measured 4.275 km)
        13.7,   // regression guard (0.35: measured 13.157 km, every Moon term in the split; 0.32: 74.298, guard 75.1)
        126,   // regression guard (measured 123.963 km)
        381,   // regression guard (measured 377.059 km)
        363,   // regression guard (measured 358.883 km)
        135,   // regression guard (measured 133.236 km)
        1460,   // regression guard (measured 1435.713 km)
    },
    {   // Real from JPL, GR off (0.32: with the Pluto system; re-measured; guards 1 % or 0.5 km, the larger)
        41400,   // regression guard (measured 40961.39 km)
        20600,   // regression guard (0.35: measured 20308.749 km; 0.32: 20309.058)
        13800,   // regression guard (0.35: measured 13725.171 km; 0.32: 13654.116)
        9380,   // regression guard (measured 9285.36 km)
        735,   // regression guard (measured 727.176 km)
        129,   // regression guard (measured 126.826 km)
        301,   // regression guard (measured 297.709 km)
        1530,   // regression guard (measured 1505.165 km)
    },
};
static const double kGuardArcsec[kModels][8] = {
    {   // Simple
        34.4,   // regression guard (measured 34.008 arcsec)
        26.8,   // regression guard (measured 26.454 arcsec)
        22.9,   // regression guard (measured 22.582 arcsec)
        86.3,   // regression guard (measured 85.42 arcsec)
        512,   // regression guard (measured 505.949 arcsec)
        1200,   // regression guard (measured 1186.37 arcsec)
        203,   // regression guard (measured 200.206 arcsec)
        91.8,   // regression guard (measured 90.817 arcsec)
    },
    {   // Real from elements, GR on (0.29: the accurate step; re-measured)
        8690,   // regression guard (measured 8603.36 arcsec)
        2210,   // regression guard (measured 2179.99 arcsec)
        961,   // regression guard (measured 951.177 arcsec)
        3690,   // regression guard (measured 3645.56 arcsec)
        2720,   // regression guard (measured 2683.68 arcsec)
        17000,   // regression guard (measured 16815.1 arcsec)
        31000,   // regression guard (measured 30598.6 arcsec)
        5070,   // regression guard (measured 5011.66 arcsec)
    },
    {   // Real from JPL, GR on (0.32: with the Pluto system; re-measured; guards 1 % or the 0.5 km room, the larger)
        0.0851,   // regression guard (measured 0.083306 arcsec)
        0.00917,   // regression guard (measured 0.008201 arcsec)
        0.0195,   // regression guard (0.35: measured 0.018447 arcsec, about 0.5 km of room; 0.32: 0.104191, guard 0.106)
        0.12,   // regression guard (measured 0.117992 arcsec)
        0.104,   // regression guard (measured 0.102698 arcsec)
        0.0549,   // regression guard (measured 0.05429 arcsec)
        0.0102,   // regression guard (measured 0.010039 arcsec)
        0.066,   // regression guard (measured 0.065268 arcsec)
    },
    {   // Real from JPL, GR off (0.32: with the Pluto system; re-measured; guards 1 % or the 0.5 km room, the larger)
        186,   // regression guard (measured 183.375889 arcsec)
        39.4,   // regression guard (measured 38.965819 arcsec)
        19.4,   // regression guard (0.35: measured 19.247335 arcsec; 0.32: 19.147691)
        9.35,   // regression guard (measured 9.251611 arcsec)
        0.205,   // regression guard (measured 0.202047 arcsec)
        0.0194,   // regression guard (measured 0.019119 arcsec)
        0.0227,   // regression guard (measured 0.022425 arcsec)
        0.0692,   // regression guard (measured 0.068417 arcsec)
    },
};
static const double kGuardSunKm = 175;   // regression guard (measured 172.5 km)

static double angleArcsec(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)) * kArcsecPerRad; }

// 0.32: the Pluto system (Horizons' Pluto Barycenter, data/jpl/horizons-pluto.txt): {JD, x, y, z} km
static const double kPlutoTable[kRows][4] = {
#include "jpl_pluto_table.inc"
};
static double g_pluto[3];
// 0.36: Ceres, Haumea, Makemake, Eris (test particles in Real; data/jpl/horizons-ceres.txt and so on): {JD, x, y, z} km
static const double kDwarfTable[kNBodyTest][kRows][4] = {
#include "jpl_dwarfs_table.inc"
};
static double g_dwarf[3][kNBodyTest];                                       // their worst heliocentric errors, km: [0] Simple (the app's fixed orbits), [1] Real GR on, [2] off                                                    // the worst heliocentric error of Pluto, km: [0] Simple (the app's own, a fixed orbit), [1] Real GR on, [2] off
#include "jpl_7h_table.inc"

// 0.36 stage 4: the places BETWEEN whole days. The clock steps whole days and interpolates in between; Mercury, the fastest, against Horizons every 7 hours (so most
// times fall between days) in early 2026, where the integration itself is about 0.02 km off. The cubic interpolation of 0.29-0.36 was 19.5 km and 688 mm/s off there.
static void testBetweenDays() {
    NBodyClock c(kNBodyStep, true);
    c.setPrefetch(false);
    double wx = 0, wv = 0;
    int between = 0;
    for (std::size_t r = 0; r < sizeof kSun7h / sizeof kSun7h[0]; r++) {
        const double jd = kSun7h[r][0];
        Vec3 x[kNBodies], v[kNBodies];
        c.stateAt((jd - kNBodyRefJD) * 86400.0, x, v);
        const Vec3 rx = Vec3{kMercury7h[r][1] - kSun7h[r][1], kMercury7h[r][2] - kSun7h[r][2], kMercury7h[r][3] - kSun7h[r][3]} * 1e3;
        const Vec3 rv = Vec3{kMercury7h[r][4] - kSun7h[r][4], kMercury7h[r][5] - kSun7h[r][5], kMercury7h[r][6] - kSun7h[r][6]} * 1e3;
        if (std::fabs(jd - kNBodyRefJD - std::round(jd - kNBodyRefJD)) > 1e-6) between++;
        wx = std::max(wx, length(x[1] - x[0] - rx) / 1e3);
        wv = std::max(wv, length(v[1] - v[0] - rv));
    }
    std::printf("between whole days: Mercury vs Horizons every 7 h, Jan-Apr 2026 (%d of the times between days): worst %.4f km, %.3f mm/s\n", between, wx, wv * 1e3);
    check("between whole days the interpolation adds almost nothing: Mercury within 0.1 km and 10 mm/s of Horizons every 7 hours, Jan-Apr 2026 (the cubic one was 19.5 km off)", wx < 0.1 && wv < 0.01);
}

static void testValidation() {
    const NBodyState elemStart = nbodyElementsStartState();                   // LEGACY start, the "before"
    NBodyClock elem(kNBodyStep, true, &elemStart), jplGR(kNBodyStep, true), jplOff(kNBodyStep, false);     // the last two: the app's own default clock
    NBodyClock* clocks[3] = {&elem, &jplGR, &jplOff};
    double worstApp = 0;                                                       // 0.10.5 stage 5: the app's own path (SolarSystem::loadAt in Simple) against planetStateAt, km
    for (int r = 0; r < kRows; r++) {
        const double jd = kJplTable[0][r][0], tau = (jd - kNBodyRefJD) * 86400.0;     // a whole number of days from the epoch
        for (int m = 0; m < kModels; m++) {
            Vec3 x[kNBodies], v[kNBodies], h[8];
            if (m == kSimple) {
                for (int i = 0; i < 8; i++) h[i] = planetStateAt(std::size_t(i), jd).pos;
                SolarSystem app; app.loadAt(static_cast<int64_t>(timescale::unixMsOfTDB(jd)));   // 0.46.3: the table's JD is TDB, the lab's clock UT       // what the app shows after "Jump to a date" in Simple
                for (int i = 0; i < 8; i++) worstApp = std::max(worstApp, length(app.bodies[i].pos - app.bodies[i].drawShift - h[i]) / 1e3);   // (0.52.1: Earth's orbit point, the barycentre; Earth itself is drawn off it)
                for (const auto& bd : app.bodies)
                    for (int d = 0; d < kNBodyTest; d++)
                        if (bd.name == kNBodyTestName[d]) g_dwarf[0][d] = std::max(g_dwarf[0][d], length(bd.pos - (Vec3{kDwarfTable[d][r][1], kDwarfTable[d][r][2], kDwarfTable[d][r][3]} * 1e3 - tv(0, r, 1))) / 1e3);
                for (const auto& bd : app.bodies) if (bd.name == "Pluto") g_pluto[0] = std::max(g_pluto[0], length(bd.pos - (Vec3{kPlutoTable[r][1], kPlutoTable[r][2], kPlutoTable[r][3]} * 1e3 - tv(0, r, 1))) / 1e3);
            }
            else {
                Vec3 px, dx[kNBodyTest];
                clocks[m - 1]->stateAt(tau, x, v, &px, nullptr, dx);
                for (int i = 0; i < 8; i++) h[i] = x[i + 1] - x[0];
                if (m == kJplGR || m == kJplNoGR) {
                    const Vec3 ref = Vec3{kPlutoTable[r][1], kPlutoTable[r][2], kPlutoTable[r][3]} * 1e3 - tv(0, r, 1);
                    g_pluto[m == kJplGR ? 1 : 2] = std::max(g_pluto[m == kJplGR ? 1 : 2], length((px - x[0]) - ref) / 1e3);
                    for (int d = 0; d < kNBodyTest; d++) {
                        const Vec3 dref = Vec3{kDwarfTable[d][r][1], kDwarfTable[d][r][2], kDwarfTable[d][r][3]} * 1e3 - tv(0, r, 1);
                        g_dwarf[m == kJplGR ? 1 : 2][d] = std::max(g_dwarf[m == kJplGR ? 1 : 2][d], length((dx[d] - x[0]) - dref) / 1e3);
                    }
                }
            }
            for (int i = 0; i < 8; i++) {
                const Vec3 ref = tv(i + 1, r, 1) - tv(0, r, 1);                       // JPL: planet minus Sun, so the barycentre shift cancels
                g_km[m][i][r] = length(h[i] - ref) / 1e3;
                g_as[m][i][r] = angleArcsec(h[i], ref);
            }
            if (m == kJplGR) g_sunDiff[r] = length(x[0] - tv(0, r, 1)) / 1e3;
        }
    }
    std::printf("\nvalidation against the Horizons DE441 tables (not DE440): heliocentric error of each planet (km) and the angle between the two heliocentric position vectors (arcsec)\n");
    std::printf("  after integrating through the clock from 2026-01-01 12:00 TDB with a 1-day step to 1 January 12:00 of each year 1800 .. 2100\n");
    for (int m = 0; m < kModels; m++) {
        std::printf("\n%s\n  %-8s %12s %5s | km at 1800 / 1900 / 2000 / 2050 / 2100                       | %10s | arcsec at 1800 / 1900 / 2000 / 2050 / 2100\n", kModelNames[m], "planet", "max km", "year", "max arcsec");
        for (int i = 0; i < 8; i++) {
            int at = 0; for (int r = 0; r < kRows; r++) if (g_km[m][i][r] > g_km[m][i][at]) at = r;
            double mxA = 0; for (int r = 0; r < kRows; r++) mxA = std::max(mxA, g_as[m][i][r]);
            std::printf("  %-8s %12.1f %5d |", kNames[i + 1], g_km[m][i][at], kFirstYear + at);
            for (int k = 0; k < 5; k++) std::printf(" %11.1f", g_km[m][i][kShow[k] - kFirstYear]);
            std::printf(" | %10.3f |", mxA);
            for (int k = 0; k < 5; k++) std::printf(" %9.3f", g_as[m][i][kShow[k] - kFirstYear]);
            std::printf("\n");
            check(std::string("validation: ") + kModelNames[m] + ", " + kNames[i + 1] + ": largest error 1800-2100 within the regression guard",
                  g_km[m][i][at] <= kGuardKm[m][i] && mxA <= kGuardArcsec[m][i]);
        }
    }
    for (int m = kJplGR; m <= kJplNoGR; m++) {
        std::printf("exact maxima, %s (km):", kModelNames[m]);
        for (int i = 0; i < 8; i++) { double mx = 0; for (int r = 0; r < kRows; r++) mx = std::max(mx, g_km[m][i][r]); std::printf(" %.3f", mx); }
        std::printf("\n");
        std::printf("exact maxima, %s (arcsec):", kModelNames[m]);
        for (int i = 0; i < 8; i++) { double mx = 0; for (int r = 0; r < kRows; r++) mx = std::max(mx, g_as[m][i][r]); std::printf(" %.6f", mx); }
        std::printf("\n");
    }
    // (b) the numbers quoted in the app's Real note (orbit/real_errors.h) are the measured maxima through the app's own default clock, rounded UP to a whole km
    bool quoted = true;
    for (int i = 0; i < 8; i++) {
        double mOff = 0, mOn = 0;
        for (int r = 0; r < kRows; r++) { mOff = std::max(mOff, g_km[kJplNoGR][i][r]); mOn = std::max(mOn, g_km[kJplGR][i][r]); }
        // (0.29: rounded up on every platform: the Linux and Windows maths libraries give maxima up to 0.17 km apart (Uranus, off: 621.99 and 622.01 km), so a
        // quoted number may be up to 1.5 km above one platform's maximum)
        quoted = quoted && double(kRealErrKmRelOff[i]) >= mOff && double(kRealErrKmRelOff[i]) - mOff < 1.5 && double(kRealErrKmRelOn[i]) >= mOn && double(kRealErrKmRelOn[i]) - mOn < 1.5;
    }
    check("the worst errors quoted in the app's Real note are the measured maxima (default clock, GR off and on) rounded up to a whole km", quoted);
    // 0.32: Pluto (the Pluto system's centre of mass) through the clock, and the app's own Simple Pluto (its fixed orbit) for comparison
    std::printf("Pluto system vs Horizons 1800-2100: Real GR on %.1f km, off %.1f km; Simple (a fixed orbit, as Real drew it before 0.32) %.0f km\n", g_pluto[1], g_pluto[2], g_pluto[0]);
    check("Pluto: the quoted worst errors are the measured maxima (+0.2 km, rounded up)", double(kRealErrKmPluto[1]) >= g_pluto[1] && double(kRealErrKmPluto[1]) - g_pluto[1] < 1.5 && double(kRealErrKmPluto[0]) >= g_pluto[2] && double(kRealErrKmPluto[0]) - g_pluto[2] < 1.5);
    check("Pluto: in Real it is far closer to Horizons than its fixed orbit (at least 100 times)", g_pluto[1] * 100 < g_pluto[0] && g_pluto[2] * 100 < g_pluto[0]);
    // 0.36: the four test-particle dwarf planets the same way
    bool dq = true, dbetter = true;
    for (int d = 0; d < kNBodyTest; d++) {
        std::printf("%s vs Horizons 1800-2100: Real GR on %.3f km, off %.3f km; Simple (its fixed orbit) %.0f km\n", kNBodyTestName[d], g_dwarf[1][d], g_dwarf[2][d], g_dwarf[0][d]);
        dq = dq && double(kRealErrKmDwarf[1][d]) >= g_dwarf[1][d] && double(kRealErrKmDwarf[1][d]) - g_dwarf[1][d] < 1.5 && double(kRealErrKmDwarf[0][d]) >= g_dwarf[2][d] && double(kRealErrKmDwarf[0][d]) - g_dwarf[2][d] < 1.5;
        dbetter = dbetter && g_dwarf[1][d] * 10 < g_dwarf[0][d] && g_dwarf[2][d] * 10 < g_dwarf[0][d];
    }
    check("dwarf planets (Ceres, Haumea, Makemake, Eris): the quoted worst errors are the measured maxima (+0.2 km, rounded up)", dq);
    check("dwarf planets: in Real each is far closer to Horizons than its fixed orbit (at least 10 times)", dbetter);
    // (b2) 0.29, physics: Horizons' DE441 is a relativistic ephemeris. For Mercury to Mars relativity is the largest thing the model would leave out (its effect falls off as 1 / a;
    // the outer planets' errors come mostly from the masses the model has not: Pluto, the asteroids, the Kuiper belt), so there relativity on must bring every planet closer.
    // (Saturn is NOT closer: relativity moves it about 200 km over 1800-2100 and its "off" error, 150 km, is partly that cancelling the missing masses; REPORT.md 0.29.)
    // And each setting must beat 0.28's numbers for the same setting (0.28 quoted them in real_errors.h; its Wisdom-Holman step was itself thousands of km off).
    {
        const double old028Off[8] = {29231, 4762, 3960, 7293, 728, 151, 620, 2653}, old028On[8] = {70045, 45430, 22081, 22341, 2823, 1068, 876, 2791};
        bool grBetterInner = true, beatsOldOn = true;
        for (int i = 0; i < 8; i++) {
            double mOff = 0, mOn = 0;
            for (int r = 0; r < kRows; r++) { mOff = std::max(mOff, g_km[kJplNoGR][i][r]); mOn = std::max(mOn, g_km[kJplGR][i][r]); }
            std::printf("  0.29 vs 0.28, %s: relativity on %.1f km (0.28: %.0f), off %.1f km (0.28: %.0f)\n", kNames[i + 1], mOn, old028On[i], mOff, old028Off[i]);
            if (i < 4) grBetterInner = grBetterInner && mOn < mOff;
            beatsOldOn = beatsOldOn && mOn < old028On[i];
        }
        check("physics: Mercury to Mars are closer to Horizons (a relativistic ephemeris) with relativity on than off", grBetterInner);
        check("physics: with relativity on every planet is closer to Horizons than 0.28 with relativity on", beatsOldOn);
    }
    // (c) 0.10.5 stage 3: the same for Simple's worst errors, quoted in the app's Simple note (orbit/real_errors.h, kSimpleErrKm)
    bool quotedSimple = true;
    for (int i = 0; i < 8; i++) {
        double mx = 0;
        for (int r = 0; r < kRows; r++) mx = std::max(mx, g_km[kSimple][i][r]);
        const bool ok = double(kSimpleErrKm[i]) >= mx && double(kSimpleErrKm[i]) - mx < 1.0;
        if (!ok) std::printf("   Simple %s: quoted %ld km, measured maximum %.3f km\n", kNames[i + 1], kSimpleErrKm[i], mx);
        quotedSimple = quotedSimple && ok;
    }
    check("the worst errors quoted in the app's Simple note are the measured maxima (planets from the elements, 301 dates 1800-2100) rounded up to a whole km", quotedSimple);
    // (c2) 0.10.5 stage 5 (audit row 20): the measured Simple errors above are those of planetStateAt; the app (loadAt, Simple) must show the same positions, so the measured errors, the quoted
    // maxima and the guards are the errors a user sees. (The comparison with JPL Horizons itself, 301 dates, every planet, km and arcsec, was already here: kGuardKm / kGuardArcsec for "Simple".)
    std::printf("the app's Simple positions (loadAt) differ from planetStateAt by at most %.6f km over 301 dates and 8 planets\n", worstApp);
    check("the app's Simple positions after a jump (SolarSystem::loadAt) are the positions measured against Horizons (within 1 km)", worstApp < 1.0);
    // (d) 0.10.5 stage 4, the independent check of the au change: no planet moved (from the old
    // au of 149.6e9 m) by more than the change itself can explain: the au moved every length by 1 - 149597870700 / 149.6e9 = 1.4233e-5, so a position error can grow by at most that times the planet's
    // greatest distance from the Sun, a (1 + e) (JPL elements at J2000).
    {
        const long before[8] = {7641, 13959, 16314, 90248, 1840647, 7999511, 2912393, 2302277};
        bool within = true;
        for (int i = 0; i < 8; i++) {
            double mx = 0;
            for (int r = 0; r < kRows; r++) mx = std::max(mx, g_km[kSimple][i][r]);
            const double rmax = kPlanets[i].el[0] * (1 + kPlanets[i].el[2]) * units::kAU / 1000.0;     // km
            const double allowed = (1 - units::kAU / 149.6e9) * rmax;
            std::printf("  au change, Simple %s: before %ld km, now %.0f km (%+.0f), at most +%.0f explainable\n", kNames[i + 1], before[i], mx, mx - double(before[i]), allowed);
            within = within && mx - double(before[i]) <= allowed;
        }
        check("the au change: no planet's worst Simple error grew by more than 1.42e-5 of its greatest distance from the Sun", within);
    }
    // Mercury: does the GR term move it towards JPL?
    std::printf("\nMercury, from the JPL state, error (km) with GR on / off:\n");
    for (int k = 0; k < 5; k++) std::printf("  %d: %.1f / %.1f\n", kShow[k], g_km[kJplGR][0][kShow[k] - kFirstYear], g_km[kJplNoGR][0][kShow[k] - kFirstYear]);
    // The Sun's own barycentric difference (includes the shift and everything the lab leaves out: Pluto, asteroids ...)
    double mx = 0; for (int r = 0; r < kRows; r++) mx = std::max(mx, g_sunDiff[r]);
    std::printf("\nSun's barycentric position, lab (JPL start, GR on, moved to the lab's centre of mass) minus JPL: largest %.1f km; at 1800 / 1900 / 2000 / 2050 / 2100:", mx);
    for (int k = 0; k < 5; k++) std::printf(" %.1f", g_sunDiff[kShow[k] - kFirstYear]);
    std::printf(" km (Sun radius 695,700 km)\n");
    check("validation: the Sun's barycentric difference is within its regression guard", mx <= kGuardSunKm);
}

// ---- (e) the GMs in the headers ----
static void testGM() {
    // From the headers of horizons-sun.txt ("GM, km^3/s^2 = 132712440041.93938", physical properties updated 2024-Oct-30), horizons-mercury.txt (22031.86855)
    // and horizons-venus.txt (324858.592, 1-sigma +-0.006). The other 6 headers (barycentres) give no GM. The header does not name a source for these values.
    const double hdr[3] = {132712440041.93938, 22031.86855, 324858.592};
    const int idx[3] = {0, 1, 2};
    const char* nm[3] = {"Sun", "Mercury", "Venus"};
    const double lim[3] = {5.5e-12, 5.0e-11, 1e-11};
    for (int k = 0; k < 3; k++) {
        const double lab = kRealGM[idx[k]] / 1e9, rel = (hdr[k] - lab) / lab;
        std::printf("GM %-7s header %.5f km^3/s^2, lab (DE440 from the JPL page excerpt) %.6f, difference %+.6f (relative %+.2e)\n", nm[k], hdr[k], lab, hdr[k] - lab, rel);
        // regression guards (measured: Sun 4.97e-12, Mercury 4.54e-11 = the 10 digits the header prints, Venus 0)
        check(std::string("GM: ") + nm[k] + ": the header's value agrees with the lab's within the regression guard", std::fabs(rel) < lim[k]);
    }
}

int main() {
    testData();
    makeJplStart();
    testNothingChanged();
    testValidation();
    testBetweenDays();
    testGM();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
