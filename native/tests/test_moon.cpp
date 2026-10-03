// 0.10.4 stage 1: the geocentric Moon from ELP2000-82B (orbit/moon_elp.*) checked against the JPL Horizons table in tests/reference/moon_table.inc
// (Moon and Sun seen from the Earth's centre, DE441, ecliptic and mean equinox of J2000, geometric). The table is the independent reference; it was
// written by tools/make_moon_ref.py BEFORE the code, and the checks below were written from it. ELP2000-82B is a 1988 theory fitted to DE200, so its
// errors against DE441 are larger than JPL's own; they are measured and printed, nothing is assumed.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "orbit/moon_elp.h"
#include "orbit/units.h"
#include "orbit/vec3.h"

using namespace orbit;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }

// { group, Julian date (TDB), Moon x, y, z (m), Sun x, y, z (m) }. group 0: every 5 years 1800..2100 at 1 January 12:00 TDB (61 rows); group 1: daily from
// 2025-12-02 00:00 TDB (60 rows); group 2: hourly 2026-01-02 00:00 .. 2026-01-04 00:00 TDB (49 rows).
static const int kRows = 170;
static const double kTable[kRows][8] =
#include "reference/moon_table.inc"
;
// Python port of the same series (tools/make_elp_data.py), same terms as orbit/elp_data.inc: { JD, x, y, z (m) }. Checks the C++ port, not the theory.
static const double kPyCheck[][4] =
#include "reference/moon_elp_pycheck.inc"
;

static const double kArcsecPerRad = 180.0 / units::kPi * 3600;
static const int kGroupFirst[3] = {0, 61, 121}, kGroupCount[3] = {61, 60, 49};
static Vec3 moonRow(int r) { return {kTable[r][2], kTable[r][3], kTable[r][4]}; }
static Vec3 sunRow(int r) { return {kTable[r][5], kTable[r][6], kTable[r][7]}; }
static double angleBetween(Vec3 a, Vec3 b) { return std::atan2(length(cross(a, b)), dot(a, b)); }

// Regression guards (NOT physics limits): each is about 1 % above what this program measured and printed (the measured value is next to each; see REPORT.md); they only
// stop a later change from making things worse. The honest accuracy is the measured number, not the guard.
static const double kGuardWorstKm = 5.264, kGuardRmsKm = 1.556, kGuardWorstArcsec = 2.908, kGuardRmsArcsec = 0.788;   // all 170 rows; measured 5.2116 km, 1.5407 km, 2.8789", 0.7805"
static const double kGuardPhaseCrossMinutes = 0.0233;      // daily phase crossings, ELP against Horizons; measured 0.02300 minutes
static const double kGuardFullMoonMinutes = 0.00069;       // full moon 2026-01-03, ELP against Horizons; measured 0.000675 minutes (0.04 s)
static const double kGuardPeriodDays = 0.0744;             // sidereal period, worst single one-year estimate; measured 0.07366 d
static const double kGuardPeriodMeanDays = 0.00048;        // sidereal period, mean over 120 years against 27.321582 d; measured 0.000475 d

static void testData() {
    check("data: 170 rows in the three groups", kGroupFirst[2] + kGroupCount[2] == kRows);
    bool grp = true, up = true;
    for (int g = 0; g < 3; g++)
        for (int i = 0; i < kGroupCount[g]; i++) {
            const int r = kGroupFirst[g] + i;
            grp = grp && int(kTable[r][0]) == g;
            if (i > 0) up = up && kTable[r][1] > kTable[r - 1][1];
        }
    check("data: group numbers and increasing Julian dates", grp && up);
    check("data: first row 1800-01-01 12:00 (JD 2378497), last yearly row 2100-01-01 12:00 (JD 2488070)", kTable[0][1] == 2378497.0 && kTable[60][1] == 2488070.0);
    check("data: the elp range constants match the table's first and last yearly rows", kMoonElpFirstJD == kTable[0][1] && kMoonElpLastJD == kTable[60][1]);
    double dmin = 1e99, dmax = 0, smin = 1e99, smax = 0;
    for (int r = 0; r < kRows; r++) {
        dmin = std::min(dmin, length(moonRow(r))); dmax = std::max(dmax, length(moonRow(r)));
        smin = std::min(smin, length(sunRow(r))); smax = std::max(smax, length(sunRow(r)));
    }
    std::printf("data: Horizons Moon distance %.0f to %.0f km, Sun %.4f to %.4f au (au = 149,597,870,700 m) over the 170 rows\n", dmin / 1e3, dmax / 1e3, smin / 149597870700.0, smax / 149597870700.0);
    check("data: Horizons Moon distance inside 356,000 to 407,000 km in every row", dmin > 356e6 && dmax < 407e6);
    check("data: Sun 0.98 to 1.02 au from the Earth in every row", smin > 0.98 * 149597870700.0 && smax < 1.02 * 149597870700.0);
}

static void testPositionError() {
    const char* gname[3] = {"yearly 1800-2100", "daily 2025-12-02..2026-01-30", "hourly 2026-01-02..04"};
    std::vector<double> km, as;
    double worstJd = 0, worstSeen = 0;
    for (int g = 0; g < 3; g++) {
        double wk = 0, wa = 0, sk = 0;
        for (int i = 0; i < kGroupCount[g]; i++) {
            const int r = kGroupFirst[g] + i;
            const Vec3 e = moonGeocentricElp(kTable[r][1]);
            const double dk = length(e - moonRow(r)) / 1e3, da = angleBetween(e, moonRow(r)) * kArcsecPerRad;
            km.push_back(dk); as.push_back(da);
            if (dk > worstSeen) { worstSeen = dk; worstJd = kTable[r][1]; }
            wk = std::max(wk, dk); wa = std::max(wa, da); sk += dk;
        }
        std::printf("error %-30s worst %9.3f km (%7.3f arcsec), mean %9.3f km\n", gname[g], wk, wa, sk / kGroupCount[g]);
    }
    double wk = 0, wa = 0, rk = 0, ra = 0;
    for (std::size_t i = 0; i < km.size(); i++) { wk = std::max(wk, km[i]); wa = std::max(wa, as[i]); rk += km[i] * km[i]; ra += as[i] * as[i]; }
    rk = std::sqrt(rk / km.size()); ra = std::sqrt(ra / as.size());
    std::vector<double> sk = km, sa = as;
    std::sort(sk.begin(), sk.end()); std::sort(sa.begin(), sa.end());
    std::printf("error ALL 170 rows: worst %.4f km (%.4f arcsec), rms %.4f km (%.4f arcsec), median %.4f km (%.4f arcsec)\n", wk, wa, rk, ra, sk[sk.size() / 2], sa[sa.size() / 2]);
    std::printf("error: the worst row is JD %.1f (year %.1f)\n", worstJd, 2000.0 + (worstJd - 2451545.0) / 365.25);
    check("error: worst position error within the regression guard (km)", wk < kGuardWorstKm);
    check("error: rms position error within the regression guard (km)", rk < kGuardRmsKm);
    check("error: worst angular error seen from the Earth within the regression guard (arcsec)", wa < kGuardWorstArcsec);
    check("error: rms angular error within the regression guard (arcsec)", ra < kGuardRmsArcsec);
}

static void testDistanceRange() {
    // The Moon's distance stays between the extreme perigee and apogee. 356,000 to 407,000 km is the owner's bound (the NASA Moon fact sheet lists perigee about
    // 356,400 and apogee about 406,700 km; that page was not opened in this session). Scanned every 0.25 day over 1800..2100 with the ELP Moon.
    double dmin = 1e99, dmax = 0;
    for (double jd = kMoonElpFirstJD; jd <= kMoonElpLastJD; jd += 0.25) {
        const double d = length(moonGeocentricElp(jd)) / 1e3;
        dmin = std::min(dmin, d); dmax = std::max(dmax, d);
    }
    std::printf("distance: ELP Moon %.1f to %.1f km, every 0.25 day over 1800..2100\n", dmin, dmax);
    check("distance: ELP Moon inside 356,000 to 407,000 km over 1800..2100", dmin > 356000 && dmax < 407000);
    // a 0.25-day scan can miss the exact extreme by a little, so it must also get close to both ends
    check("distance: the scan does reach below 358,000 km and above 405,000 km (the orbit is not frozen)", dmin < 358000 && dmax > 405000);
}

static double wrap2pi(double a) { a = std::fmod(a, 2 * units::kPi); return a < 0 ? a + 2 * units::kPi : a; }
static double lon(Vec3 v) { return std::atan2(v.y, v.x); }

static void testSiderealPeriod() {
    // Sidereal period: time for the longitude (J2000 ecliptic, inertial) to advance 360 degrees, from the ELP Moon alone. A window of one year (365.25 days = 1461 steps of
    // 0.25 day, longitude unwrapped) is not a whole number of orbits (13.37), so a single year's estimate is off by the Moon's uneven speed over the unfinished orbit (a
    // few hundredths of a day, measured below); the MEAN over many years, with start dates spaced 913.1 days so the orbital phase is mixed, is the mean sidereal period.
    // Reference: Horizons' Moon header "Orbit period = 27.321582 d" (read in the reply of 2026-10-03).
    const double ref = 27.321582;
    double worst = 0, sum = 0, lo = 1e9, hi = 0;
    int n = 0;
    for (double jd0 = kMoonElpFirstJD + 100; jd0 + 366 < kMoonElpLastJD; jd0 += 913.1) {
        double prev = lon(moonGeocentricElp(jd0)), total = 0;
        for (int i = 1; i <= 1461; i++) {
            const double a = lon(moonGeocentricElp(jd0 + i * 0.25));
            double d = a - prev;
            if (d < -units::kPi) d += 2 * units::kPi;
            if (d > units::kPi) d -= 2 * units::kPi;
            total += d; prev = a;
        }
        const double period = 365.25 * 2 * units::kPi / total;
        worst = std::max(worst, std::fabs(period - ref)); lo = std::min(lo, period); hi = std::max(hi, period); sum += period; n++;
    }
    std::printf("period: %d one-year estimates: mean %.5f d, range %.5f to %.5f d; reference %.6f d: mean differs %+.6f d, worst single year %.6f d\n", n, sum / n, lo, hi, ref, sum / n - ref, worst);
    check("period: at least 100 years sampled", n >= 100);
    check("period: the mean differs from 27.321582 d by less than the regression guard", std::fabs(sum / n - ref) < kGuardPeriodMeanDays);
    check("period: every single year within the regression guard of 27.321582 d (the spread is the unfinished orbit, not an error)", worst < kGuardPeriodDays);
}

// elongation (Moon longitude minus Sun longitude, 0..2 pi) from a Moon and a Sun vector
static double elong(Vec3 moon, Vec3 sun) { return wrap2pi(lon(moon) - lon(sun)); }

// Time (JD) at which an increasing, unwrapped series f(t_i) (t_i = t0 + i*dt) reaches `target`: the 4-point Lagrange interpolant, root by bisection between the two middle points.
static double rootTime(const std::vector<double>& f, double t0, double dt, int i, double target) {
    auto at = [&](double s) {   // s in [i, i+1]
        double v = 0;
        for (int a = i - 1; a <= i + 2; a++) {
            double w = 1;
            for (int b = i - 1; b <= i + 2; b++) if (b != a) w *= (s - b) / double(a - b);
            v += w * f[a];
        }
        return v - target;
    };
    double lo = i, hi = i + 1;
    for (int k = 0; k < 60; k++) { const double mid = 0.5 * (lo + hi); (at(lo) * at(mid) <= 0) ? hi = mid : lo = mid; }
    return t0 + 0.5 * (lo + hi) * dt;
}

static std::vector<double> unwrapped(const std::vector<double>& e) {
    std::vector<double> u = e;
    for (std::size_t i = 1; i < u.size(); i++) {
        double d = e[i] - e[i - 1];
        while (d < -units::kPi) d += 2 * units::kPi;
        while (d > units::kPi) d -= 2 * units::kPi;
        u[i] = u[i - 1] + d;
    }
    return u;
}

static void testPhases() {
    // Daily rows (group 1): elongation from the ELP Moon and the JPL Sun, and from the Horizons Moon and Sun. Phases: new 0, first quarter 90, full 180, last quarter 270 degrees.
    std::vector<double> eE, eH;
    for (int i = 0; i < kGroupCount[1]; i++) {
        const int r = kGroupFirst[1] + i;
        eE.push_back(elong(moonGeocentricElp(kTable[r][1]), sunRow(r)));
        eH.push_back(elong(moonRow(r), sunRow(r)));
    }
    const std::vector<double> uE = unwrapped(eE), uH = unwrapped(eH);
    bool rising = true;
    double dmin = 1e9, dmax = 0;
    for (std::size_t i = 1; i < uE.size(); i++) {
        const double d = (uE[i] - uE[i - 1]) * 180 / units::kPi;
        rising = rising && d > 0; dmin = std::min(dmin, d); dmax = std::max(dmax, d);
    }
    std::printf("phases: ELP elongation grows %.2f to %.2f degrees per day over 60 days (total %.1f degrees)\n", dmin, dmax, (uE.back() - uE.front()) * 180 / units::kPi);
    check("phases: the elongation grows every day", rising);
    check("phases: it grows between 9 and 15.5 degrees per day (the Moon gains on the Sun by 360 degrees in 29.53 days = 12.19 degrees per day on average)", dmin > 9 && dmax < 15.5);
    // crossings of k * 90 degrees, in order; both series must cross the same multiples of 90 degrees in the same order
    auto crossings = [](const std::vector<double>& u, double t0) {
        std::vector<std::pair<int, double>> c;   // {k, JD}
        const double q = units::kPi / 2;
        for (int i = 1; i + 2 < int(u.size()); i++)
            for (int k = int(std::floor(u[i] / q)) + 1; k * q <= u[i + 1]; k++)
                if (k * q > u[i]) c.push_back({k, rootTime(u, t0, 1.0, i, k * q)});
        return c;
    };
    const double t0 = kTable[kGroupFirst[1]][1];
    const auto cE = crossings(uE, t0), cH = crossings(uH, t0);
    bool sameOrder = cE.size() == cH.size() && cE.size() >= 8;
    bool consecutive = true;
    double worstH = 0;
    for (std::size_t i = 0; sameOrder && i < cE.size(); i++) {
        sameOrder = sameOrder && (cE[i].first % 4 + 4) % 4 == (cH[i].first % 4 + 4) % 4;
        if (i > 0) consecutive = consecutive && cE[i].first == cE[i - 1].first + 1;
        worstH = std::max(worstH, std::fabs(cE[i].second - cH[i].second) * 24);
    }
    static const char* nm[4] = {"new", "first quarter", "full", "last quarter"};
    for (std::size_t i = 0; i < cE.size(); i++)
        std::printf("phases: %-13s JD %.5f (ELP) %.5f (Horizons): %+.4f minutes\n", nm[((cE[i].first % 4) + 4) % 4], cE[i].second, i < cH.size() ? cH[i].second : 0.0, i < cH.size() ? (cE[i].second - cH[i].second) * 1440 : 0.0);
    check("phases: ELP and Horizons cross the same multiples of 90 degrees in the same order (at least 8 crossings = two cycles)", sameOrder);
    check("phases: the crossings go new, first quarter, full, last quarter, new, ... with no gaps", consecutive);
    std::printf("phases: worst difference of a crossing time, ELP against Horizons: %.5f minutes (daily samples, 4-point interpolation)\n", worstH * 60);
    check("phases: crossing times agree within the regression guard (minutes)", worstH * 60 < kGuardPhaseCrossMinutes);

    // The full moon of 2026-01-03 from the hourly rows (group 2), root of elongation = 180 degrees.
    std::vector<double> hE, hH;
    for (int i = 0; i < kGroupCount[2]; i++) {
        const int r = kGroupFirst[2] + i;
        hE.push_back(elong(moonGeocentricElp(kTable[r][1]), sunRow(r)));
        hH.push_back(elong(moonRow(r), sunRow(r)));
    }
    const std::vector<double> vE = unwrapped(hE), vH = unwrapped(hH);
    const double h0 = kTable[kGroupFirst[2]][1], dh = 1.0 / 24;
    double tE = 0, tH = 0;
    for (std::size_t i = 1; i + 2 < vE.size(); i++)
        if (vH[i] < units::kPi && vH[i + 1] >= units::kPi) { tH = rootTime(vH, h0, dh, int(i), units::kPi); tE = rootTime(vE, h0, dh, int(i), units::kPi); }
    std::printf("full moon: Horizons JD %.5f (TDB), ELP JD %.5f: ELP is %+.6f minutes from Horizons\n", tH, tE, (tE - tH) * 1440);
    check("full moon: found inside 2026-01-03 (JD 2461043.5 .. 2461044.5)", tH > 2461043.5 && tH < 2461044.5 && tE > 2461043.5 && tE < 2461044.5);
    check("full moon: ELP within the regression guard of Horizons (minutes), and within the owner's 'few hours'", std::fabs(tE - tH) * 1440 < kGuardFullMoonMinutes && std::fabs(tE - tH) * 24 < 3);
}

// The C++ port against the Python port of the same series (checks the port; the Python port is what the truncation was measured with).
static void testPortMatchesPython() {
    double worst = 0;
    const int n = int(sizeof(kPyCheck) / sizeof(kPyCheck[0]));
    for (int i = 0; i < n; i++)
        worst = std::max(worst, length(moonGeocentricElp(kPyCheck[i][0]) - Vec3{kPyCheck[i][1], kPyCheck[i][2], kPyCheck[i][3]}));
    std::printf("port: C++ against the Python port at %d dates: worst difference %.6f m\n", n, worst);
    check("port: C++ equals the Python port within 0.01 m (regression guard; double-precision rounding only)", n >= 20 && worst < 0.01);
}

int main() {
    testData();
    testPortMatchesPython();
    testPositionError();
    testDistanceRange();
    testSiderealPeriod();
    testPhases();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
