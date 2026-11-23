// 0.75.1: Saturn's drawn bands and their values at a ring point's own slant (shared by the Orbit Lab's drawing, the card's check and the tests; no graphics code).
#pragma once

#include <vector>

#include "orbit/giant_rings.h"
#include "orbit/ring_draw.h"

namespace orbit {

constexpr double kRingTint[3] = {0.91, 0.83, 0.66};      // 0.50 (review item 7): ONE drawn tint for every ring system, not measured (orbit_view.cpp)

// Saturn's drawn bands from Cassini's profile (orbit/saturn_profile.h) for a planet of r points: groups of whole 10 km measurements at least a pixel wide (and at
// most 700 over the profile), each with its place (planet radii; a narrow one widened to a pixel, none overlapping), its measurements (i0, n) and the share of its
// drawn width that is its own (cover). 0.75.1: no opacity and light here: those are the means over the measurements at each point's own slant (ringBandValue);
// kept while the two inputs stay the same
const std::vector<RingBand>& saturnDrawnBands(double eqKm, double r);
// 0.75.0's bands, for the tests: the opacity and light at one slant for the whole system (mu, mu0, the face), neighbours that look the same merged
const std::vector<RingBand>& saturnDrawnBandsFlat(double eqKm, double r, double mu, double mu0, bool sunlitFace);
// 0.75.2: the drawn bands of Jupiter's, Uranus's or Neptune's rings for a planet of r points (giant_rings.h; a narrow ring widened to a pixel, a band too faint to show left out):
// each carries its tau and albedo (tauBand) and takes each point's own slant through ringBandValue, as Saturn's do
std::vector<RingBand> giantDrawnBands(const std::vector<rings::Band>& bands, double eqKm, double r);
// a band's opacity (blocked share x cover) and light k at the slant mu, with the Sun's mu0 and face of f (f.mu0, f.sunlit); k: drawn colour x opacity = the light;
// faint: both under 0.002 (nothing to see)
struct RingValue { float alpha, k; bool faint; };
RingValue ringBandValue(const RingFrame& f, const RingBand& b, double mu);

}  // namespace orbit
