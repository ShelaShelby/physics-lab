// The Moon from ELP2000-82B. The series, the arguments and the change of coordinates follow the Fortran subroutine ELP82B (elp82b.f, CDS VI/79) term by term;
// the constants and the terms are in elp_data.inc, made by tools/make_elp_data.py from the published files (nothing typed by hand).
//
// What the series is: longitude and latitude (arcsec) and distance (km) are sums of  amplitude x T^p x sin(argument),  T in Julian centuries of 36525 days from
// JD 2451545.0 (TDB),  p = 0, 1 or 2. The arguments are integer combinations of the Delaunay arguments (D, l', l, F), the precession term zeta and the mean
// longitudes of the eight planets. ELP2000-82B contains the main problem, the Earth's and the Moon's figure, tides, relativity and the planetary perturbations
// (two tables, with their solar-eccentricity terms).
//
// What is left out HERE:
//  * Small terms. elp_data.inc keeps only the largest terms; the rule (measured against Horizons) and the errors with all terms and with the kept terms are in
//    the header of elp_data.inc and in native/REPORT.md. The dropped terms are mostly planetary-perturbation terms.
//  * Nothing is refitted: ELP2000-82B's own constants are used as published. They were fitted to the JPL numerical integration DE200/LE200 (1980s), not to DE441,
//    and the theory itself is a 1988 semi-analytical solution, so it differs from DE441 by more than JPL's own ephemerides differ from each other; the measured
//    difference is the accuracy of this function, not a statement about the Moon. The slow differences (tidal acceleration, the lunar libration/figure) are not corrected.
//  * The ELP frame is the mean dynamical ecliptic and inertial equinox of J2000; Horizons' ecliptic is tied to the ICRS (ecliptic and mean equinox of J2000). The small
//    fixed offset between them is not modelled and is part of the measured difference.
//  * Geometric only: no light time, no aberration. The Moon's centre, not the Earth-Moon barycentre.
#include "orbit/moon_elp.h"

#include <cmath>

namespace orbit {
namespace {

// One term of the merged series: coord 0 = longitude (arcsec), 1 = latitude (arcsec), 2 = distance (km); tpow = power of T the amplitude is multiplied by;
// basis 0 = the main problem (ELP1..3: Delaunay arguments as polynomials of degree 4 in T), basis 1 = the others (all arguments linear in T);
// m = integer multipliers of D, l', l, F, zeta, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune; phase in degrees.
struct ElpTerm {
    unsigned char coord, tpow, basis;
    signed char m[13];
    double phaseDeg;
    double amp;
};

#include "orbit/elp_data.inc"

}  // namespace

Vec3 moonGeocentricElp(double jdTDB) {
    const double T = (jdTDB - 2451545.0) / 36525.0;
    const double t[5] = {1.0, T, T * T, T * T * T, T * T * T * T};
    const double twoPi = 2.0 * kElp_cpi, halfPi = kElp_cpi / 2.0;
    static const double kRadPerDeg = kElp_deg;

    // Delaunay's arguments D, l', l, F as in the subroutine: D = W1 - Earth's mean longitude (+ pi), l' = Earth - perihelion, l = W1 - W2, F = W1 - W3.
    double del[4][5];
    for (int k = 0; k < 5; k++) {
        del[0][k] = kElpW[0][k] - kElpEart[k];
        del[1][k] = kElpEart[k] - kElpPeri[k];
        del[2][k] = kElpW[0][k] - kElpW[1][k];
        del[3][k] = kElpW[0][k] - kElpW[2][k];
    }
    del[0][0] += kElp_cpi;
    double full[13] = {0}, lin[13] = {0};
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 5; k++) full[i] += del[i][k] * t[k];
        lin[i] = del[i][0] + del[i][1] * T;
    }
    lin[4] = kElpW[0][0] + (kElpW[0][1] + kElp_preces) * T;   // zeta
    for (int i = 0; i < 8; i++) lin[5 + i] = kElpP[i][0] + kElpP[i][1] * T;

    double sum[3] = {0, 0, 0};
    for (int n = 0; n < kElpTermCount; n++) {
        const ElpTerm& e = kElpTerms[n];
        const double* ang = e.basis ? lin : full;
        double y = e.basis ? e.phaseDeg * kRadPerDeg : 0.0;
        for (int j = 0; j < 13; j++)
            if (e.m[j]) y += e.m[j] * ang[j];
        if (!e.basis && e.coord == 2) y += halfPi;            // the main-problem distance is a cosine series
        y = std::fmod(y, twoPi);
        sum[e.coord] += e.amp * t[e.tpow] * std::sin(y);
    }

    // Change of coordinates (end of the subroutine): mean longitude added, distance scaled by a0/ath, then the precession matrix to the inertial J2000 frame.
    const double lonRad = sum[0] / kElp_rad + kElpW[0][0] + kElpW[0][1] * t[1] + kElpW[0][2] * t[2] + kElpW[0][3] * t[3] + kElpW[0][4] * t[4];
    const double latRad = sum[1] / kElp_rad;
    const double r = sum[2] * kElp_a0 / kElp_ath;
    double x1 = r * std::cos(latRad);
    double x2 = x1 * std::sin(lonRad);
    x1 = x1 * std::cos(lonRad);
    const double x3 = r * std::sin(latRad);
    double pw = (kElp_p1 + kElp_p2 * t[1] + kElp_p3 * t[2] + kElp_p4 * t[3] + kElp_p5 * t[4]) * t[1];
    double qw = (kElp_q1 + kElp_q2 * t[1] + kElp_q3 * t[2] + kElp_q4 * t[3] + kElp_q5 * t[4]) * t[1];
    const double ra = 2.0 * std::sqrt(1.0 - pw * pw - qw * qw);
    const double pwqw = 2.0 * pw * qw, pw2 = 1.0 - 2.0 * pw * pw, qw2 = 1.0 - 2.0 * qw * qw;
    pw *= ra;
    qw *= ra;
    // x, y, z in km -> metres
    return {1000.0 * (pw2 * x1 + pwqw * x2 + pw * x3), 1000.0 * (pwqw * x1 + qw2 * x2 - qw * x3), 1000.0 * (-pw * x1 + qw * x2 + (pw2 + qw2 - 1.0) * x3)};
}

}  // namespace orbit
