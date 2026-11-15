// The rings of Jupiter, Uranus and Neptune, as drawn in the Orbit Lab (0.26 stage 7). Saturn's are in saturn_rings.h.
// Sources: NASA NSSDCA ring fact sheets (D. R. Williams, last updated 14 October 2015):
//   Jupiter https://nssdc.gsfc.nasa.gov/planetary/factsheet/jupringfact.html
//   Uranus  https://nssdc.gsfc.nasa.gov/planetary/factsheet/uranringfact.html
//   Neptune https://nssdc.gsfc.nasa.gov/planetary/factsheet/nepringfact.html
// Each ring is drawn as in saturn_rings.h: it stops 1 - exp(-tau / mu) of the light from behind it and shines with its albedo.
// Narrow rings are a radius and a width (km); where the sheet gives a range, the middle is used (a simplification, said in
// the app); a width given only as "< 100 km" is taken as 100 km, an upper bound. Rings are drawn circular (the sheet's
// eccentricities, up to 0.0079 for Uranus's epsilon ring, are left out). Left out, with the reason: Jupiter's halo and
// gossamer rings (no albedo in the sheet; optical depth 3e-6 and 1e-7), Uranus's two outer dust rings (optical depth << 0.01,
// no albedo), Neptune's Arago ring (no optical depth) and the arcs of the Adams ring (the sheet gives no longitudes for them).
#pragma once

#include <string>
#include <vector>

namespace orbit::rings {

struct Band { double innerKm, outerKm, tau, albedo; double blockedPre = -1, shinePre = -1; };   // 0.51.2: blockedPre, shinePre >= 0: the averaged values of a Cassini profile bin (saturn_profile.h), used instead of tau and albedo

struct GiantRings { double eqRadiusKm; std::vector<Band> bands; };

// The ring bands of a giant planet other than Saturn; empty for any other body.
inline GiantRings giantRings(const std::string& planet) {
    const auto narrow = [](double r, double w, double tau, double albedo) { return Band{r - w / 2, r + w / 2, tau, albedo}; };
    if (planet == "Jupiter")
        return {71492, {{123000, 128940, 5e-6, 0.015}}};                                   // the main ring
    if (planet == "Uranus")
        return {25559, {narrow(41837, 1.5, 0.3, 0.015), narrow(42234, 2, 0.5, 0.015), narrow(42571, 2, 0.3, 0.015),   // rings 6, 5, 4
                        narrow(44718, 7, 0.4, 0.015), narrow(45661, 8, 0.3, 0.015),                                  // alpha (4-10 km), beta (5-11 km)
                        narrow(47176, 1.6, 0.4, 0.015), narrow(47627, 2.5, 0.3, 0.015), narrow(48300, 5, 0.5, 0.015), // eta, gamma (1-4 km), delta (3-7 km)
                        narrow(50024, 2, 0.1, 0.015), narrow(51149, 58, 1.4, 0.018)}};                               // lambda, epsilon (20-96 km, tau 0.5-2.3)
    if (planet == "Neptune")   // planet radius: NSSDC Neptune fact sheet, 24,764 km at 1 bar, as in solar_data.cpp (0.48: was 24,766, no source)
        return {24764, {narrow(41900, 2000, 1e-4, 0.015), narrow(53200, 100, 0.01, 0.015), Band{53200, 57200, 1e-4, 0.015},   // Galle, Le Verrier (< 100 km), Lassell (see below)
                        narrow(62933, 15, 0.055, 0.015)}};                                                                       // Adams (tau 0.01-0.1)
        // Lassell (2026-11-05 audit): the sheet gives "~53,200" and width "~4000"; its Arago row (~57,200, the same 1989N4R*) is Lassell's outer edge, so the
        // radius is the INNER edge: 53,200-57,200 km (it was drawn centred on 53,200, 2,000 km too far in).
    return {};
}

}  // namespace orbit::rings
