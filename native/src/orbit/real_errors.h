// The worst error of each planet in "Planet physics: Real" against JPL Horizons over 1800-2100, in km, as quoted in the app's note (0.10.3). Heliocentric position
// (planet minus Sun) after integrating from the JPL start of 2026-01-01 12:00 TDB with a 1-day step, against Horizons' vector tables (source {source: DE441}, not DE440),
// measured by tests/test_jpl.cpp at the 301 tabulated dates (1 January 12:00 of every year). Each number is the measured maximum rounded UP to a whole km; the test
// checks that every one of them is that, so the text can only change together with a re-measurement. Between the tabulated dates the error is not measured.
// 0.10.4 stage 2, Earth: the Earth entries below were measured for the Earth-Moon barycentre (the N-body body). The app now draws Earth itself (barycentre minus the Moon's share);
// tests/test_moon_real.cpp measures that against Horizons' Earth (399): 3,959.9 km (relativity off) and 22,080.1 km (on), which round up to the same two numbers, 3,960 and 22,081.
#pragma once

#include <algorithm>
#include <string>

#include "format.h"

namespace orbit {

constexpr int kRealErrCount = 8;                                                     // Mercury, Venus, Earth (the Earth-Moon barycentre), Mars, Jupiter, Saturn, Uranus, Neptune
constexpr const char* kRealErrName[kRealErrCount] = {"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
constexpr long kRealErrKmRelOff[kRealErrCount] = {29231, 4762, 3960, 7293, 728, 151, 620, 2653};          // general relativity off (the default)
constexpr long kRealErrKmRelOn[kRealErrCount] = {70045, 45430, 22081, 22341, 2823, 1068, 876, 2791};      // general relativity (1PN, Sun's potential) on


// 0.10.5 stage 3: the same for "Planet physics: Simple" (planets from JPL's approximate Keplerian elements, 1800-2050 table, kept as fixed orbits when time runs), quoted in the Simple note.
// Heliocentric position error against the same Horizons tables at the same 301 dates, measured by tests/test_jpl.cpp (the "Simple" block; the maximum over all 301 dates, so it includes
// 2050-2100, which the table is not made for; Saturn's worst is in 2067), each rounded UP to a whole km; the test checks they are the measured maxima. Earth = the Earth-Moon barycentre.
// (They include the lab's au of 149.6e9 m, 1.4e-5 too big: audit row 2; they will move a little when that is fixed, and the test will say so.)
constexpr long kSimpleErrKm[kRealErrCount] = {7641, 13959, 16314, 90248, 1840647, 7999511, 2912393, 2302277};

// "Saturn 8.0, Uranus 2.9, Neptune 2.3, Jupiter 1.8, Mars 0.09, Earth 0.016, Venus 0.014, Mercury 0.0076": Simple's worst errors in million km, largest first
inline std::string simpleWorstErrorList() {
    int order[kRealErrCount];
    for (int i = 0; i < kRealErrCount; i++) order[i] = i;
    std::sort(order, order + kRealErrCount, [](int a, int b) { return kSimpleErrKm[a] > kSimpleErrKm[b]; });
    std::string t;
    for (int k = 0; k < kRealErrCount; k++) {
        const double mkm = kSimpleErrKm[order[k]] / 1e6;
        t += (k ? ", " : "") + std::string(kRealErrName[order[k]]) + " " + (mkm >= 1 ? fmt::fixed(mkm, 1) : fmt::precision(mkm, 2));
    }
    return t;
}

}  // namespace orbit
