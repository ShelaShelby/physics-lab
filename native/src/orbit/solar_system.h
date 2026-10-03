// The Orbit Lab simulation: the real Solar System, moved forward in time with the
// same method as the web version (leapfrog / velocity Verlet around the Sun).
//
// Planets and dwarf planets are test particles: each feels only the Sun, not the
// other planets. Comets follow their exact Kepler orbit (no integration drift).
// Moons are drawn on fixed Kepler ellipses round their planet.
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "belts.h"
#include "kepler.h"
#include "nbody.h"
#include "solar_data.h"
#include "vec3.h"

namespace orbit {

enum class BodyKind { Planet, Dwarf, Comet };

struct Moon {
    const MoonData* data;
    double phase;              // starting angle, radians (made up, as in the web: from the moon's name)
};

struct Body {
    int id = 0;
    std::string name;
    const char* color = "#ffffff";
    std::string kind;          // shown in the card title, e.g. "gas giant"
    BodyKind type = BodyKind::Planet;
    Vec3 pos, vel;             // m and m/s, Sun at the origin
    double radius = 0;         // m
    double massEarths = 0;     // Earth masses (0 for comets)
    double gravityG = -1;      // surface gravity in g; < 0 when not given (then worked out from mass and radius)
    bool hasTemp = false;
    double tempC = 0;          // real mean temperature, °C (planets only)
    const char* tempNote = "";
    const char* note = "";
    bool giant = false;        // heavier than 50 Earths
    const CometData* comet = nullptr;
    std::vector<Moon> moons;
    std::deque<Vec3> trail;    // past positions, m (at most 1000)
};

// A planet's position and velocity from its JPL elements on a Julian day (m, m/s, Sun at the origin; planet number 0 = Mercury ... 7 = Neptune).
State planetStateAt(std::size_t i, double jd);

// The orbit shape worked out from position and velocity (two-body formulas).
struct OrbitShape {
    double r;        // distance from the Sun, m
    double v;        // speed, m/s
    double energy;   // specific orbital energy v²/2 − GM/r, J/kg
    double e;        // eccentricity
    double a;        // semi-major axis, m (infinite when not bound)
    bool bound;      // energy < 0: it goes round, it does not escape
    double rPeri;    // closest to the Sun, m
    double rApo;     // farthest from the Sun, m
    double inclDeg;  // tilt of the orbit plane from the ecliptic, deg
    double periodYears;
};
OrbitShape orbitOf(const Body& b);

class SolarSystem {
public:
    // Put everything where it really is at `unixMs` (milliseconds since 1970, UTC).
    void loadAt(int64_t unixMs);

    // Advance by `realSeconds` of wall-clock time at a Time speed slider value `tspeed`
    // (1 real second = kSimTime × 10^tspeed simulated seconds). Exactly the web's step().
    void step(double realSeconds, double tspeed);

    // "Planet physics": Simple (false, the default: every planet alone on a Kepler orbit round the Sun, as in the web version) or Real (true: the Sun and
    // the eight planets pull on each other, orbit/nbody.h; moons, dwarf planets, comets and belts are treated as in Simple). Switching on moves the planets
    // to the N-body state for the date shown; switching off reloads the system at the date shown from the orbital elements.
    void setRealPlanets(bool on);
    bool realPlanets() const { return real_; }
    // In Real: the planets also feel the Sun's first-order (1PN) general-relativity potential (orbit/nbody.h). Changing it moves the planets to the other
    // clock's state for the date shown and clears the planets' trails. No effect in Simple (the value is kept for when Real is switched on).
    void setRelativity(bool on);
    bool relativity() const { return gr_; }

    // The balance point of the Sun and every planet, m from the Sun's centre.
    Vec3 barycentre() const;

    double julianDay() const;               // the date being shown, as a Julian day
    int64_t currentUnixMs() const;          // the same as Unix milliseconds

    std::vector<Body> bodies;
    std::vector<BeltGroup> belts;           // asteroid belt, Trojans, Kuiper belt, Oort cloud (made for the start date)
    double t = 0;                           // simulated seconds since the epoch
    int64_t epochMs = 0;                    // the starting date (Unix ms, UTC)
    bool jumped = false;                    // the start date was picked with Jump to a date (not today)
    int crashed = 0, escaped = 0;

private:
    int nextId_ = 0;
    bool real_ = false;
    bool gr_ = false;
    NBodyClock nb_;
    void applyReal(double extraSeconds);   // put the eight planets where the N-body clock has them `extraSeconds` after the shown date
    void realTrail(Body& b);
};

}  // namespace orbit
