// The worst error of each planet in "Planet physics: Real" against JPL Horizons over 1800-2100, in km, as quoted in the app's note (0.10.3). Heliocentric position
// (planet minus Sun) after integrating from the JPL start of 2026-01-01 12:00 TDB with a 1-day step, against Horizons' vector tables (source {source: DE441}, not DE440),
// measured by tests/test_jpl.cpp at the 301 tabulated dates (1 January 12:00 of every year). Each number is the measured maximum rounded UP to a whole km; the test
// checks that every one of them is that, so the text can only change together with a re-measurement. Between the tabulated dates the error is not measured.
// 0.10.4 stage 2, Earth: the Earth entries below were measured for the Earth-Moon barycentre (the N-body body). The app now draws Earth itself (barycentre minus the Moon's share);
// tests/test_moon_real.cpp measures that against Horizons' Earth (399): 3,959.9 km (relativity off) and 22,080.1 km (on), which round up to the same two numbers, 3,960 and 22,081.
#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

#include "format.h"
#include "moon_elp.h"

namespace orbit {

constexpr int kRealErrCount = 8;                                                     // Mercury, Venus, Earth (the Earth-Moon barycentre), Mars, Jupiter, Saturn, Uranus, Neptune
constexpr const char* kRealErrName[kRealErrCount] = {"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
constexpr long kRealErrKmRelOff[kRealErrCount] = {29231, 4762, 3960, 7293, 728, 151, 620, 2653};          // general relativity off (the default)
constexpr long kRealErrKmRelOn[kRealErrCount] = {70045, 45430, 22081, 22341, 2823, 1068, 876, 2791};      // general relativity (1PN, Sun's potential) on


// 0.10.5 stage 3: the same for "Planet physics: Simple" (planets from JPL's approximate Keplerian elements, 1800-2050 table, kept as fixed orbits when time runs), quoted in the Simple note.
// Heliocentric position error against the same Horizons tables at the same 301 dates, measured by tests/test_jpl.cpp (the "Simple" block; the maximum over all 301 dates, so it includes
// 2050-2100, which the table is not made for; Saturn's worst is in 2067), each rounded UP to a whole km; the test checks they are the measured maxima. Earth = the Earth-Moon barycentre.
// 0.10.5 stage 4 (the IAU au, 149,597,870,700 m, instead of the web's 149.6e9 m): they were 7641, 13959, 16314, 90248, 1840647, 7999511, 2912393, 2302277 km before; tests/test_jpl.cpp checks that no
// planet got worse by more than the au change can explain (1.42e-5 of its greatest distance from the Sun).
constexpr long kSimpleErrKm[kRealErrCount] = {7618, 13858, 16112, 90493, 1839933, 8000757, 2942873, 2341475};

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


// "Mercury 29,231 km, Venus 4,762, ..., Neptune 2,653 km": Real's worst errors from the table above (0.10.5 stage 5: moved here from ui/orbit_panel.cpp)
inline std::string realWorstErrorList(bool relativityOn) {
    const long* km = relativityOn ? kRealErrKmRelOn : kRealErrKmRelOff;
    std::string t;
    for (int i = 0; i < kRealErrCount; i++) t += (i ? ", " : "") + std::string(kRealErrName[i]) + " " + fmt::commas(double(km[i])) + (i == kRealErrCount - 1 ? " km" : "");
    return t;
}

// The notes under "Planet physics" (0.10.5 stage 2/3: one paragraph each; the Real one showed as 10 lines at 1280 x 800, normal text size).
inline std::string realNoteText(bool relativityOn) {
    char moon[32];
    std::snprintf(moon, sizeof moon, "%.1f km", kMoonElpWorstKm);
    return std::string("Real: Sun and eight planets pull on each other (Newtonian; each planet with its moons). "
                       "Start: JPL Horizons (DE441), 1 Jan 2026 12:00 TDB; dates used as TDB. "
                       "Moon: ELP2000-82B, 1800 to 1 Jan 2100 only; other moons as in Simple. "
                       "Not in: dwarf planets, asteroids. "
                       "Worst error vs Horizons, 1800-2100: Moon ") + moon + "; planets, relativity " + (relativityOn ? "on" : "off") + ": " + realWorstErrorList(relativityOn) + ".";
}
inline std::string simpleNoteText() {
    return "Simple: each planet moves alone on a fixed orbit round the Sun, and the planets do not pull on each other (as in the web version). Worst error vs JPL Horizons, 1800-2100 (JPL's table is made for 1800-2050), "
           "in million km: " + simpleWorstErrorList() + ".";
}

}  // namespace orbit
