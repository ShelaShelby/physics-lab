// The real Solar System data, copied from the web Orbit Lab (index.html).
// Units are kept exactly as in the sources (AU, degrees, km, days, Earth masses)
// so they can be checked against NASA/JPL by eye; they are turned into SI
// metres and seconds in solar_system.cpp.
#pragma once

#include <cstddef>

namespace orbit {

// Planets. Orbital elements: NASA/JPL "Keplerian elements for approximate
// positions of the major planets, 1800 AD - 2050 AD". Each element has its value
// at J2000 and its rate per Julian century:
//   a (AU), e, I (deg), L mean longitude (deg), long. of perihelion ϖ (deg), long. of node Ω (deg).
// Sizes, masses and mean temperatures: NASA planetary fact sheet. Gravity (0.10.5 stage 3): JPL "Planetary Physical Parameters" (https://ssd.jpl.nasa.gov/planets/phys_par.html, read 2026-10-03),
// column "Equatorial Gravity", which the page defines as "the gravitational acceleration on the planet's surface at the equator" (m/s^2; Mercury 3.70, Venus 8.87, Earth 9.80, Mars 3.71,
// Jupiter 24.79, Saturn 10.44, Uranus 8.87, Neptune 11.15). The page does not say which level the giant planets' value is for, so the card only says "at the equator".
struct PlanetData {
    const char* name;
    const char* color;          // "#rrggbb", as drawn in the web version
    const char* kind;           // "rocky planet", "gas giant", ...
    double el[12];              // a0, a1, e0, e1, I0, I1, L0, L1, w0, w1, O0, O1
    double radiusKm;            // equatorial radius, km
    double massEarths;          // mass in Earth masses
    double gravityMS2;          // surface gravity at the equator, m/s^2 (JPL Planetary Physical Parameters, "Equatorial Gravity")
    double tempC;               // mean temperature, °C
    const char* tempNote;       // where that temperature is measured
    const char* note;           // one fact for the info card
};

// Dwarf planets: JPL Small-Body Database elements at their own epoch.
struct DwarfData {
    const char* name;
    const char* color;
    const char* note;
    double epochJD;             // Julian day of the elements
    double e;                   // eccentricity
    double aAU;                 // semi-major axis, AU
    double inclDeg;             // inclination, deg
    double nodeDeg;             // longitude of ascending node Ω, deg
    double periDeg;             // argument of perihelion ω, deg
    double meanAnomDeg;         // mean anomaly at the epoch, deg
    double meanMotionDegPerDay; // deg/day
    double diameterKm;          // km
    double massKg;              // kg
};

// Moons: distance and period from the NASA satellite fact sheets. "tilt" is how
// far the moon's orbit plane leans from the ecliptic (roughly the planet's axial
// tilt). Their starting places are illustrative (as in the web version): the
// moons are drawn on a fixed Kepler ellipse round their planet, not integrated.
struct MoonData {
    const char* planet;
    const char* name;
    double aKm;                 // mean distance from the planet, km
    double periodDays;          // one orbit, days
    double e;                   // eccentricity
    double radiusKm;            // km
    double tiltDeg;             // deg
    bool retrograde;            // goes round backwards (Triton)
    const char* note;
};

// Famous comets: the elements of one JPL orbit record each (named in solar_data.cpp: JPL Small-Body Database orbit id and epoch), kept as a fixed two-body orbit.
// Far from the record's epoch the lab's comet drifts from the real one (tests/test_orbit.cpp section 15 measures it against Horizons at two dates).
struct CometData {
    const char* name;
    const char* color;
    double qAU;                 // perihelion distance (closest to the Sun), AU
    double e;                   // eccentricity
    double inclDeg, nodeDeg, periDeg;   // i, Ω, ω in degrees
    double perihelionJD;        // Julian day of a perihelion passage
    double periodDays;          // orbital period, days
    double nucleusKm;           // nucleus size, km
    double tail;                // tail length scale (drawing only), AU
    const char* title;
    const char* note;
    bool nucleusRough = false;  // the nucleus size has no named source (JPL SBDB gives none): the card says so (0.10.5 stage 5)
};

extern const PlanetData kPlanets[];
extern const std::size_t kPlanetCount;
extern const DwarfData kDwarfs[];
extern const std::size_t kDwarfCount;
extern const MoonData kMoons[];
extern const std::size_t kMoonCount;
extern const CometData kComets[];
extern const std::size_t kCometCount;

}  // namespace orbit
