#include "orbit/time_scale.h"

#include <cmath>

namespace timescale {

namespace {
// IERS Bulletin C leap-second table (data/time/iers-leap-second.dat): MJD from which TAI - UTC takes the value
struct Leap { double mjd; double taiMinusUtc; };
constexpr Leap kLeaps[] = {
    {41317.0, 10}, {41499.0, 11}, {41683.0, 12}, {42048.0, 13}, {42413.0, 14}, {42778.0, 15}, {43144.0, 16}, {43509.0, 17},
    {43874.0, 18}, {44239.0, 19}, {44786.0, 20}, {45151.0, 21}, {45516.0, 22}, {46247.0, 23}, {47161.0, 24}, {47892.0, 25},
    {48257.0, 26}, {48804.0, 27}, {49169.0, 28}, {49534.0, 29}, {50083.0, 30}, {50630.0, 31}, {51179.0, 32}, {53736.0, 33},
    {54832.0, 34}, {56109.0, 35}, {57204.0, 36}, {57754.0, 37},
};
constexpr double kTTminusTAI = 32.184;   // seconds, exact by definition

double espenakMeeus(double jdUT) {
    double y = 2000.0 + (jdUT - 2451545.0) / 365.25;    // decimal year
    if (y < 1800) y = 1800;
    // Espenak & Meeus (NASA "Polynomial Expressions for Delta T"), the pieces for 1800 to 1986
    if (y < 1860) {
        const double t = y - 1800;
        return 13.72 - 0.332447 * t + 0.0068612 * t * t + 0.0041116 * t * t * t - 0.00037436 * t * t * t * t
               + 0.0000121272 * t * t * t * t * t - 0.0000001699 * t * t * t * t * t * t + 0.000000000875 * t * t * t * t * t * t * t;
    }
    if (y < 1900) {
        const double t = y - 1860;
        return 7.62 + 0.5737 * t - 0.251754 * t * t + 0.01680668 * t * t * t - 0.0004473624 * t * t * t * t + t * t * t * t * t / 233174;
    }
    if (y < 1920) {
        const double t = y - 1900;
        return -2.79 + 1.494119 * t - 0.0598939 * t * t + 0.0061966 * t * t * t - 0.000197 * t * t * t * t;
    }
    if (y < 1941) {
        const double t = y - 1920;
        return 21.20 + 0.84493 * t - 0.076100 * t * t + 0.0020936 * t * t * t;
    }
    if (y < 1961) {
        const double t = y - 1950;
        return 29.07 + 0.407 * t - t * t / 233 + t * t * t / 2547;
    }
    const double t = y - 1975;                           // 1961 to 1986 (used here only up to 1 Jan 1972)
    return 45.45 + 1.067 * t - t * t / 260 - t * t * t / 718;
}
}  // namespace

long long unixMsOfTDB(double jdTDB) {
    // whole milliseconds of the date and of Delta T separately, so a whole-JD TDB date gives exactly its ms minus Delta T (no rounding of 2.46e6-day numbers)
    const double dT = deltaTSeconds(utFromTDB(jdTDB));
    return std::llround((jdTDB - 2440587.5) * 86400000.0) - std::llround(dT * 1000.0);
}

double deltaTSeconds(double jdUT) {
    const double mjd = jdUT - 2400000.5;
    if (mjd >= kLeaps[0].mjd) {
        double l = kLeaps[0].taiMinusUtc;
        for (const Leap& e : kLeaps) if (mjd >= e.mjd) l = e.taiMinusUtc;
        return kTTminusTAI + l;
    }
    // rounded to whole milliseconds (the polynomials are good to about a second): a TDB date given in whole ms then maps exactly onto the lab's ms clock
    return std::round(espenakMeeus(jdUT) * 1000.0) / 1000.0;
}

}  // namespace timescale
