// Saturn's main rings, as drawn in the Orbit Lab (0.26).
// Source: NASA NSSDCA, Saturnian Rings Fact Sheet (D. R. Williams, last updated 19 April 2022,
// https://nssdc.gsfc.nasa.gov/planetary/factsheet/satringfact.html): edges in km, Saturn's
// equatorial radius 60,268 km, optical depth and albedo ranges. The fact sheet gives ranges;
// the app uses the middle of each range (a simplification, said in the app).
// The faint D, G and E rings (optical depth 1e-5 to 1e-6) are too faint to see and are not drawn.
// The F ring is narrower than 1,000 km; the fact sheet gives only its radius, so it is drawn
// as a hairline. Gaps narrower than 1,000 km (Encke, Keeler) have no width there and are not drawn.
#pragma once

#include <algorithm>
#include <cmath>

namespace orbit::rings {

constexpr double kSaturnEqRadiusKm = 60268;

struct Ring { const char* name; double innerKm, outerKm, tauLo, tauHi, albedoLo, albedoHi; };
constexpr Ring kRings[] = {
    {"C ring", 74658, 91975, 0.05, 0.35, 0.12, 0.30},
    {"B ring", 91975, 117507, 0.4, 2.5, 0.4, 0.6},
    {"Cassini Division", 117507, 122340, 0.0, 0.1, 0.2, 0.4},
    {"A ring", 122340, 136780, 0.4, 1.0, 0.4, 0.6},
};
constexpr double kFRingKm = 139826, kFRingTau = 0.1, kFRingAlbedo = 0.6;

// How much of the light from behind a ring of normal optical depth tau it stops, seen at
// mu = |sin(elevation)| above the ring plane: the slanted path is tau / mu deep, so
// 1 - exp(-tau / mu) (Beer–Lambert). Edge-on (mu -> 0) it is opaque but has no width.
inline double blocked(double tau, double mu) {
    return 1 - std::exp(-tau / std::max(mu, 1e-3));
}

// 0.26: the light a ring layer of optical depth tau sends you, by single scattering in a thin slab (Chandrasekhar 1960, Radiative
// Transfer), seen at mu and lit at mu0 (sines of the elevations above the ring plane), relative to an opaque layer seen face-on in
// full sunlight (formula value 0.5): on the sunlit face mu0 / (mu0 + mu) (1 - exp(-tau (1/mu + 1/mu0))); from the dark face, the light
// that comes through, mu0 / (mu0 - mu) (exp(-tau/mu0) - exp(-tau/mu)) (its limit tau/mu exp(-tau/mu) when mu0 = mu).
inline double shine(double tau, double mu, double mu0, bool sunlitFace) {
    mu = std::max(mu, 1e-3); mu0 = std::max(mu0, 1e-3);
    if (sunlitFace) return mu0 / (mu0 + mu) * (1 - std::exp(-tau * (1 / mu + 1 / mu0))) / 0.5;
    if (std::fabs(mu0 - mu) < 1e-6) return tau / mu * std::exp(-tau / mu) / 0.5;
    return mu0 / (mu0 - mu) * (std::exp(-tau / mu0) - std::exp(-tau / mu)) / 0.5;
}

}  // namespace orbit::rings
