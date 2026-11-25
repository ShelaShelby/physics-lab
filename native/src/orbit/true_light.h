// 0.85.0 (outside review, PLAN_REVIEW.md R1): planets, moons, dwarf planets and rings lit by the true sunlight at their distance, as a camera would show them.
// No new number: the sunlight on a body falls as 1/d^2 with its distance d from the Sun (inverse square: the Sun's power spread over a sphere of radius d).
// The distances are the lab's own (its JPL ephemeris). s(d) = (1 au / d)^2 is the sunlight relative to Earth's mean distance.
// The camera's exposure is set for one body (the followed one; nothing followed: 1 au), E = 1 / s(d_E), so that body looks as it does with even lighting
// and every other body gets s(d) E = (d_E / d)^2 times its light. A screen shows a limited range: a body far brighter clips white, a far dimmer one goes black.
// A camera, not an eye: no adaptation. The dots that mark bodies too small to see keep their brightness (they are markers, not pictures of the body).
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace orbit {

constexpr double kTrueLightRefAU = 1.0;          // the exposure with nothing followed: set for 1 au (Earth's mean distance)

inline double sunlightAt(double dAU) { return 1.0 / (dAU * dAU); }                       // s(d) = (1 au / d)^2
inline double exposureFor(double dExposureAU) { return 1.0 / sunlightAt(dExposureAU); }   // E = 1 / s(d_E)
// the factor a body's linear light is multiplied by: s(d) E. Even lighting (the non-realistic choice): 1.
inline double lightFactor(double dAU, double dExposureAU, bool trueLight) {
    if (!trueLight || !(dAU > 0) || !(dExposureAU > 0)) return 1.0;
    return sunlightAt(dAU) * exposureFor(dExposureAU);
}

// "N times" in words: 2 significant figures below 100, whole numbers with commas above
inline std::string timesText(double r) {
    char b[64];
    if (r < 10) std::snprintf(b, sizeof b, "%.1f", r);
    else if (r < 100) std::snprintf(b, sizeof b, "%.0f", r);
    else {
        const double p = std::pow(10.0, std::floor(std::log10(r)) - 1);   // two significant figures
        std::string s = std::to_string(static_cast<long long>(std::llround(r / p) * static_cast<long long>(std::llround(p))));
        for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
        return s;
    }
    return b;
}

// The readout: "Exposure set for Neptune (30.1 au): Earth gets 900 times more sunlight." exposureName empty: nothing followed (1 au).
inline std::string exposureReadout(const std::string& exposureName, double dExposureAU, const std::string& otherName, double dOtherAU) {
    char b[200];
    if (exposureName.empty()) std::snprintf(b, sizeof b, "Exposure set for 1 au (Earth's mean distance from the Sun).");
    else std::snprintf(b, sizeof b, "Exposure set for %s (%.3g au).", exposureName.c_str(), dExposureAU);
    std::string s = b;
    if (!otherName.empty() && dOtherAU > 0 && otherName != exposureName) {
        const double r = sunlightAt(dOtherAU) / sunlightAt(dExposureAU);
        s += " " + otherName + " gets " + timesText(r >= 1 ? r : 1 / r) + (r >= 1 ? " times more sunlight." : " times less sunlight.");
    }
    return s;
}

}  // namespace orbit
