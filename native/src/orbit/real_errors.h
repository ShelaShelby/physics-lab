// The worst error of each planet in "Planet physics: Real" against JPL Horizons over 1800-2100, in km, as quoted in the app's note (0.10.3). Heliocentric position
// (planet minus Sun) after integrating from the JPL start of 2026-01-01 12:00 TDB with a 1-day step, against Horizons' vector tables (source {source: DE441}, not DE440),
// measured by tests/test_jpl.cpp at the 301 tabulated dates (1 January 12:00 of every year). Each number is the measured maximum rounded UP to a whole km; the test
// checks that every one of them is that, so the text can only change together with a re-measurement. Between the tabulated dates the error is not measured.
#pragma once

namespace orbit {

constexpr int kRealErrCount = 8;                                                     // Mercury, Venus, Earth (the Earth-Moon barycentre), Mars, Jupiter, Saturn, Uranus, Neptune
constexpr const char* kRealErrName[kRealErrCount] = {"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
constexpr long kRealErrKmRelOff[kRealErrCount] = {29231, 4762, 3960, 7293, 728, 151, 620, 2653};          // general relativity off (the default)
constexpr long kRealErrKmRelOn[kRealErrCount] = {70045, 45430, 22081, 22341, 2823, 1068, 876, 2791};      // general relativity (1PN, Sun's potential) on

}  // namespace orbit
