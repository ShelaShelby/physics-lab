// How bright the electron cloud's dots are drawn (Glow style). No graphics in here, so it can be tested.
//
// The dots are samples of the probability density |psi|^2, so the light of soft dots added together is a picture of that density.
// Only the brightness of ONE dot is chosen here; where the dots are, their number per place and their signs are never touched.
//   - Every dot carries the same light whatever the density setting, scaled by 12,000 / (dots kept): the cloud as a whole is
//     equally bright with 12,000, 24,000 or 48,000 dots (more dots only make it smoother).
//   - The exposure (one number for the whole picture, the same for every dot) keeps the brightest place of the picture just below
//     white, so the dense core is not burnt out and no structure is lost. It never makes a dot brighter than `base`.
#pragma once

#include <algorithm>

#include "atom_core.h"

namespace atom {

// The light of one dot relative to a 12,000-dot cloud.
inline double cloudDotWeight(long cap) { return double(kCap) / double(cap); }

// peak: the brightest place's summed light with every dot at light 1 and counted as if the cloud had 12,000 dots.
// target: the summed light that place should reach (0.85 for additive light on a dark page; about 2.2 for alpha blending on a light page,
// where the result is 1 - exp(-light), a smooth increasing curve). Returns the exposure, in (0, 1].
inline double cloudExposure(double peak, double target, double base) { return peak * base <= target ? 1.0 : target / (peak * base); }

// The light (opacity) of one dot: base × exposure × 12,000/cap, times its own depth weight.
inline double cloudDotLight(long cap, double exposure, double base, double depthWeight = 1) {
    return std::min(1.0, base * exposure * cloudDotWeight(cap) * depthWeight);
}

}  // namespace atom
