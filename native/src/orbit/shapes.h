// 0.27 (owner's choice, 5 Nov 2026): the real shapes of the planets, Pluto and our Moon. None is a perfect ball: a turning
// body bulges at its equator. Drawn as an ellipsoid round its real (IAU) spin axis (render/ellipsoid.h, the planet shader).
//
// Equatorial and polar radii, km: NASA NSSDCA planetary fact sheets (nssdc.gsfc.nasa.gov/planetary/factsheet/<planet>fact.html,
// read 5 Nov 2026), rows "Equatorial radius" and "Polar radius". For the four giants these are the 1 bar level (the sheets say so).
// The sheets also give the flattening (eq - polar) / eq, which tests/test_orbit.cpp checks against these radii.
// The two equatorial radii are the same for all of these (a spheroid): the sheets give one equatorial radius.
#pragma once

#include <cstring>

namespace orbit {

struct BodyShape {
    const char* name;
    double eqKm;       // equatorial radius
    double polarKm;    // polar radius
};

inline constexpr BodyShape kShapes[] = {
    {"Mercury", 2440.5, 2438.3},
    {"Venus", 6051.8, 6051.8},      // the sheet: no measurable flattening (0.000)
    {"Earth", 6378.137, 6356.752},
    {"Mars", 3396.2, 3376.2},
    {"Jupiter", 71492, 66854},
    {"Saturn", 60268, 54364},
    {"Uranus", 25559, 24973},
    {"Neptune", 24764, 24341},
    {"Pluto", 1188, 1188},          // Pluto fact sheet: flattening 0.0000
    {"Moon", 1738.1, 1736.0},       // Moon fact sheet
};

// The body's shape, or nullptr for one drawn as a ball (no measured shape here).
inline const BodyShape* bodyShape(const char* name) {
    for (const auto& s : kShapes)
        if (std::strcmp(s.name, name) == 0) return &s;
    return nullptr;
}

// Polar over equatorial radius (1 for a ball or a body without a shape here).
inline double polarRatio(const char* name) {
    const BodyShape* s = bodyShape(name);
    return s ? s->polarKm / s->eqKm : 1.0;
}

}  // namespace orbit
