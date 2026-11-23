// The Orbit Lab simulation: the real Solar System, moved forward in time with the
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
#include "moon_elp.h"
#include "nbody.h"
#include "solar_data.h"
#include "units.h"
#include "vec3.h"

namespace orbit {

enum class BodyKind { Planet, Dwarf, Comet };

struct Moon {
    const MoonData* data;
    double phase;              // starting angle, radians
};

struct Body {
    int id = 0;
    std::string name;
    const char* color = "#ffffff";
    std::string kind;          // shown in the card title, e.g. "gas giant"
    BodyKind type = BodyKind::Planet;
    Vec3 pos, vel;             // m and m/s, Sun at the origin
    Vec3 drawShift{};          // 0.52.1: Earth (Simple and Real): drawn this far from its orbit's point, the Earth-Moon barycentre (pos - drawShift; vel is the barycentre's). Zero for the rest
    double radius = 0;         // m
    double massEarths = 0;     // Earth masses (0 for comets)
    double gravityG = -1;      // surface gravity at the equator in g (0.46.3: standard gravity 9.80665 m/s^2 = 1; was Earth's 9.80), the pull alone; < 0 when not given (then worked out from mass and radius)
    double feltG = -1;         // 0.46.3: what you would feel at the equator in g (gravity minus the spin's push, NASA fact sheets); < 0 when not given
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

// 0.75.2: the GM the heliocentric two-body formulas of a body belong with: G(M_sun + m) (solar_system.cpp).
double heliocentricGM(const Body& b);

// The orbit shape worked out from position and velocity (two-body formulas, GM = heliocentricGM).
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
// 0.23 stage 2: the whole orbit the body is on now (two-body, about the Sun, GM = kGMSun as orbitOf): n + 1 points, m, Sun at the origin, evenly spaced in the
// eccentric anomaly (closed: the last point is the first). In Simple mode this is the JPL Kepler ellipse itself; in Real mode the osculating one (the orbit it
// would keep if the other planets vanished now). Empty for an orbit that is not bound.
void orbitPath(const Body& b, int n, std::vector<Vec3>& out);
// 0.46.6: the same ellipse from eccentric anomaly E0 to E1 only (n + 1 points), for drawing the visible stretch finely when zoomed in
void orbitPath(const Body& b, double E0, double E1, int n, std::vector<Vec3>& out);
// 0.68.0: that ellipse's axes (orbitPath's own numbers): towards the perihelion P, Q in its plane, a, e and b; false when it is not bound
bool orbitAxes(const Body& b, Vec3& P, Vec3& Q, double& a, double& e, double& bAx);

// Real planets: the Sun's own path (its position from the barycentre) is kept as a trail with a point every 0.05 Sun radii of travel, at most 1,000 points.
constexpr double kSunTrailStep = 0.05 * units::kSunRadius;
// A moon that goes round more than this many times a real second is not drawn as a body (the whole orbit is lit): the drawing's rule for every moon.
constexpr double kMoonFastLaps = 8;

class SolarSystem {
public:
    // Put everything where it really is at `unixMs` (milliseconds since 1970, UTC).
    void loadAt(int64_t unixMs);

    // Advance by `realSeconds` of wall-clock time at a Time speed slider value `tspeed`
    // (1 real second = kSimTime × 10^tspeed simulated seconds).
    void step(double realSeconds, double tspeed);

    // "Planet physics": Simple or Real (true: the Sun and
    // the eight planets and (0.32) the Pluto system pull on each other, orbit/nbody.h; 0.36: the other dwarf planets ride along as test particles; moons, comets and belts are treated as in Simple). Switching on moves the planets
    // to the N-body state for the date shown; switching off reloads the system at the date shown from the orbital elements.
    void setRealPlanets(bool on);
    bool realPlanets() const { return real_; }
    // In Real: the planets also feel the Sun's first-order (1PN) general-relativity potential (orbit/nbody.h). Changing it moves the planets to the other
    // clock's state for the date shown and clears the planets' trails. No effect in Simple (the value is kept for when Real is switched on).
    void setRelativity(bool on);
    // 0.67.0: a jump in Real to a date far from the N-body clock's kept states (1850 from 2026: 1.8 s of steps on the owner's PC) is walked on a worker first:
    // jumpNeedsWalk says so, prepareJump starts it, jumpPrepared (each frame) is true once it is taken in; then loadAt there costs the frame no walk.
    // Meanwhile the system stays at the date shown (ui jumpToDate). The states are bit for bit the same (NBodyClock::prepare).
    bool jumpNeedsWalk(int64_t unixMs) const;
    void prepareJump(int64_t unixMs);
    bool jumpPrepared() { return nb_.prepared(); }
    bool relativity() const { return gr_; }

    // The balance point of the Sun and every planet, m from the Sun's centre.
    // Earths). Real: where the N-body integrator has it (the Sun's own barycentric position, minus), with the real GM values.
    Vec3 barycentre() const;

    // The real Moon (0.10.4 stage 2; Real planet physics only). In Real the N-body body "Earth" is the Earth-Moon barycentre (EMB: its GM is Earth + Moon, its start state JPL's EMB). So Earth
    // itself is drawn at EMB - g * mu / (1 + mu) and the Moon is at EMB + g / (1 + mu), where g is the ELP2000-82B vector from Earth to the Moon (orbit/moon_elp.h) and mu = GM_Moon / GM_Earth
    // (JPL astrodynamic parameters, orbit/nbody.h). Earth's velocity (and so its orbit numbers on the card) stays the barycentre's. Earth's trail follows the barycentre.
    // Outside 1800 to 1 Jan 2100 12:00 TDB realMoon() is false: the Moon falls back to its Kepler ellipse, Earth to the barycentre.
    // 0.46.3: the Moon's place (ELP2000-82B) is the real one in Simple too (it does not depend on the planet physics). 0.52.1: in Simple too Earth is drawn off the barycentre
    // (the JPL elements' Earth-Moon barycentre), EMB - g mu / (1 + mu), the shift kept out of the leapfrog's state (shiftEarthSimple).
    bool realMoon() const { return moonValid_; }
    bool earthOffBary() const { return moonValid_ && !moonFast_; }   // (0.52.1: Simple too)
    Vec3 moonGeocentric() const { return moonGeo_; }       // ELP vector Earth -> Moon at the date shown, m (valid when realMoon())
    Vec3 moonPosition() const;                             // the Moon's true place, from the Sun's centre (m), valid when realMoon()
    Vec3 embPosition() const { return emb_; }              // the Earth-Moon barycentre from the Sun's centre (m; 0.52.1: Simple too)
    Vec3 moonOrbitNormal() const { return moonNormal_; }   // unit normal of the Moon's instantaneous orbit plane (geocentric position x velocity, ELP), valid when realMoon()
    // True while the Time speed makes the Moon go round more than kMoonFastLaps times a real second (the drawing then lights the whole orbit instead of drawing the Moon, as for every
    // moon); Earth is then held at the barycentre, because its 4,300-5,000 km wobble at that rate would only flicker. Set by step().
    bool moonFast() const { return moonFast_; }

    double julianDay() const;               // the date being shown, as a Julian day in UT (the date bar's time)
    double julianDayTDB() const;            // 0.46.3: the same moment in TDB, the time scale of the JPL data (orbit/time_scale.h): use this for every position
    int64_t currentUnixMs() const;          // the same as Unix milliseconds

    std::vector<Body> bodies;
    std::deque<Vec3> sunTrail;              // Real only: the Sun's path, its positions from the barycentre (m); empty in Simple
    std::vector<BeltGroup> belts;           // asteroid belt, Trojans, Kuiper belt, Oort cloud (made for the start date)
    double t = 0;                           // simulated seconds since the epoch
    int64_t epochMs = 0;                    // the starting date (Unix ms, UTC)
    bool jumped = false;                    // the start date was picked with Jump to a date (not today)
    int crashed = 0, escaped = 0;

private:
    int nextId_ = 0;
    bool real_ = false;
    bool gr_ = false;
    Vec3 sunBary_;                         // Real: the Sun's position from the barycentre (m), as the N-body clock has it
    Vec3 emb_, moonGeo_, moonNormal_;      // Real: the Earth-Moon barycentre (m, from the Sun), the ELP vector Earth -> Moon (m), the Moon's orbit-plane normal
    bool moonValid_ = false;               // the date is inside the span of ELP2000-82B (0.46.3: in Simple too)
    void updateMoon(double jdTDB);         // ELP2000-82B at jdTDB: moonValid_, moonGeo_, moonNormal_
    bool moonFast_ = false;
    void shiftEarthSimple(double jdTDB);
    NBodyClock nb_;
    void applyReal(double extraSeconds, bool withMoon = true);   // put the eight planets where the N-body clock has them `extraSeconds` after the shown date; withMoon: and split Earth and Moon (once per frame, not per sub-step)
    void realTrail(Body& b);
    void sunTrailPoint();
};

}  // namespace orbit
