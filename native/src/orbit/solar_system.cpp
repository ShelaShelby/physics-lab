#include "solar_system.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "kepler.h"
#include "units.h"

namespace orbit {

using namespace units;

// A made-up but fixed starting angle for each moon, from its name (as in the web's hashPhase).
static double hashPhase(const char* name) {
    int h = 7;
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(name); *c; ++c) h = (h * 31 + *c) % 6283;
    return h / 1000.0;
}

static void attachMoons(Body& b) {
    for (std::size_t i = 0; i < kMoonCount; i++)
        if (b.name == kMoons[i].planet) b.moons.push_back({&kMoons[i], hashPhase(kMoons[i].name)});
}

// A planet's position and velocity (m, m/s, Sun at the origin, the web's GM for the velocity) from its JPL elements on Julian day `jd`.
State planetStateAt(std::size_t i, double jd) {
    const PlanetData& d = kPlanets[i];
    const double T = (jd - 2451545.0) / 36525;           // Julian centuries since J2000
    const double* el = d.el;
    const double a = el[0] + el[1] * T, e = el[2] + el[3] * T, I = el[4] + el[5] * T;
    const double L = el[6] + el[7] * T, w = el[8] + el[9] * T, O = el[10] + el[11] * T;
    // argument of perihelion ω = ϖ − Ω; mean anomaly M = L − ϖ
    return keplerState(a * kAU, e, I, O, w - O, L - w);
}

void SolarSystem::loadAt(int64_t unixMs) {
    bodies.clear();
    nextId_ = 0;
    t = 0;
    crashed = escaped = 0;
    epochMs = unixMs;
    const double jd = unixMs / 86400000.0 + 2440587.5;   // Julian day

    for (std::size_t i = 0; i < kPlanetCount; i++) {     // planets: JPL elements, each with its rate per century
        const PlanetData& d = kPlanets[i];
        const State st = planetStateAt(i, jd);
        Body b;
        b.id = ++nextId_;
        b.name = d.name; b.color = d.color; b.kind = d.kind; b.type = BodyKind::Planet;
        b.pos = st.pos; b.vel = st.vel;
        b.radius = d.radiusKm * kKm;
        b.massEarths = d.massEarths; b.gravityG = d.gravityG;
        b.hasTemp = true; b.tempC = d.tempC; b.tempNote = d.tempNote;
        b.note = d.note;
        b.giant = d.massEarths > 50;
        bodies.push_back(std::move(b));
    }
    for (std::size_t i = 0; i < kDwarfCount; i++) {      // dwarf planets: elements at their own epoch, mean anomaly grows by n per day
        const DwarfData& d = kDwarfs[i];
        const State st = keplerState(d.aAU * kAU, d.e, d.inclDeg, d.nodeDeg, d.periDeg,
                                     d.meanAnomDeg + d.meanMotionDegPerDay * (jd - d.epochJD));
        Body b;
        b.id = ++nextId_;
        b.name = d.name; b.color = d.color; b.kind = "dwarf planet"; b.type = BodyKind::Dwarf;
        b.pos = st.pos; b.vel = st.vel;
        b.radius = d.diameterKm / 2 * kKm;
        b.massEarths = d.massKg / kEarthMassKg;
        b.note = d.note;
        bodies.push_back(std::move(b));
    }
    for (std::size_t i = 0; i < kCometCount; i++) {      // comets: exact Kepler orbit from the perihelion date
        const CometData& c = kComets[i];
        const State st = cometState(c, jd);
        Body b;
        b.id = ++nextId_;
        b.name = c.name; b.color = c.color; b.kind = "comet"; b.type = BodyKind::Comet;
        b.pos = st.pos; b.vel = st.vel;
        b.radius = c.nucleusKm / 2 * kKm;
        b.massEarths = 0;
        b.note = c.note;
        b.comet = &c;
        bodies.push_back(std::move(b));
    }
    for (Body& b : bodies) attachMoons(b);
    belts = makeBelts(unixMs);
    if (real_) applyReal(0);
}

// the reference epoch of the N-body clock (Julian day kNBodyRefJD, 2026-01-01 12:00 UT) as Unix milliseconds
static constexpr double kRefUnixMs = (kNBodyRefJD - 2440587.5) * 86400000.0;

void SolarSystem::applyReal(double extra) {
    Vec3 x[kNBodies], v[kNBodies];
    nb_.stateAt((double(epochMs) - kRefUnixMs) / 1000.0 + t + extra, x, v);
    for (std::size_t i = 0; i < kPlanetCount && i < bodies.size(); i++) {
        if (bodies[i].type != BodyKind::Planet || bodies[i].name != kPlanets[i].name) continue;
        bodies[i].pos = x[i + 1] - x[0];                 // the picture stays Sun-centred
        bodies[i].vel = v[i + 1] - v[0];
    }
}

void SolarSystem::realTrail(Body& b) {                   // a trail point every 2 % of the distance to the Sun, as the leapfrog does
    const double r = length(b.pos), lim = 0.02 * (r > kSimLength ? r : kSimLength);
    if (b.trail.empty() || length(b.pos - b.trail.back()) > lim) { b.trail.push_back(b.pos); if (b.trail.size() > 1000) b.trail.pop_front(); }
}

void SolarSystem::setRealPlanets(bool on) {
    if (on == real_) return;
    real_ = on;
    if (on) applyReal(0);
    else loadAt(currentUnixMs());                        // back to Simple: everything reloaded from the elements at the date shown
}

void SolarSystem::step(double dt, double tspeed) {
    // Simulated time for this step, cut into equal sub-steps of at most kMaxSubStep
    // (about 5.6 simulated hours) so fast Time speeds stay accurate.
    const double total = dt * kSimTime * std::pow(10.0, tspeed);   // s
    const int nSub = std::max(1, static_cast<int>(std::ceil(total / kMaxSubStep)));
    const double h = total / nSub, hh = h / 2;
    const double GM = kGMSun;

    // Leapfrog (kick-drift-kick, a.k.a. velocity Verlet) around the Sun:
    //   v += a·h/2;  x += v·h;  a = −GM·x/(r³ + ε);  v += a·h/2
    // Each body only feels the Sun, so every body can run all its sub-steps in one go.
    // The pull at the end of one sub-step is the pull at the start of the next, so it is
    // worked out once and reused (as in the web version 7.7).
    for (Body& b : bodies) {
        if (b.type == BodyKind::Comet) continue;          // comets follow their exact orbit (set below)
        if (real_ && b.type == BodyKind::Planet) continue;   // Real: the planets come from the N-body clock (below)
        double x = b.pos.x, y = b.pos.y, z = b.pos.z, vx = b.vel.x, vy = b.vel.y, vz = b.vel.z;
        double r2 = x * x + y * y + z * z, r = std::sqrt(r2), q = -GM / (r2 * r + kSoftening);
        double ax = q * x, ay = q * y, az = q * z;        // acceleration, m/s²
        auto& tr = b.trail;
        bool haveLast = !tr.empty();
        Vec3 lt = haveLast ? tr.back() : Vec3{};
        for (int k = 0; k < nSub; k++) {
            vx += ax * hh; vy += ay * hh; vz += az * hh;  // half kick
            x += vx * h; y += vy * h; z += vz * h;        // drift
            r2 = x * x + y * y + z * z; r = std::sqrt(r2); q = -GM / (r2 * r + kSoftening);
            ax = q * x; ay = q * y; az = q * z;           // new pull of the Sun
            vx += ax * hh; vy += ay * hh; vz += az * hh;  // half kick
            // a trail point every 2 % of the distance to the Sun, so trails look the same at any time speed
            const double lim = 0.02 * (r > kSimLength ? r : kSimLength);
            const double dx = x - lt.x, dy = y - lt.y, dz = z - lt.z;
            if (!haveLast || dx * dx + dy * dy + dz * dz > lim * lim) {
                lt = {x, y, z}; haveLast = true;
                tr.push_back(lt);
                if (tr.size() > 1000) tr.pop_front();
            }
        }
        b.pos = {x, y, z};
        b.vel = {vx, vy, vz};
    }
    // bodies that hit the Sun or fly far away are removed
    bodies.erase(std::remove_if(bodies.begin(), bodies.end(), [&](const Body& b) {
                     if (b.type == BodyKind::Comet) return false;
                     if (real_ && b.type == BodyKind::Planet) return false;
                     const double r = length(b.pos);
                     if (r < kCrashRadius) { crashed++; return true; }
                     if (r > kLostRadius) { escaped++; return true; }
                     return false;
                 }), bodies.end());
    if (real_) {                                          // the planets: walk to the new date in steps of about 2 days, with a trail point where needed
        const int nSub = std::max(1, std::min(2000, static_cast<int>(std::ceil(total / (2 * kDay)))));
        for (int k = 1; k <= nSub; k++) {
            t += total / nSub;
            applyReal(0);
            for (Body& b : bodies) if (b.type == BodyKind::Planet) realTrail(b);
        }
    } else
        t += total;
    const double jd = julianDay();
    for (Body& b : bodies)
        if (b.type == BodyKind::Comet) {
            const State st = cometState(*b.comet, jd);
            b.pos = st.pos; b.vel = st.vel;
        }
}

Vec3 SolarSystem::barycentre() const {
    double mt = kSunInEarthMasses;                        // the Sun weighs 333,000 Earths
    Vec3 s;
    for (const Body& b : bodies) { s = s + b.pos * b.massEarths; mt += b.massEarths; }
    return s * (1.0 / mt);
}

double SolarSystem::julianDay() const { return epochMs / 86400000.0 + 2440587.5 + t / kDay; }

int64_t SolarSystem::currentUnixMs() const { return epochMs + static_cast<int64_t>(std::llround(t * 1000.0)); }

OrbitShape orbitOf(const Body& b) {
    const double GM = kGMSun;
    OrbitShape o{};
    o.r = length(b.pos);
    const double v2 = dot(b.vel, b.vel);
    o.v = std::sqrt(v2);
    o.energy = v2 / 2 - GM / o.r;                         // specific orbital energy, J/kg
    const double rv = dot(b.pos, b.vel);
    const Vec3 hv = cross(b.pos, b.vel);                  // specific angular momentum, m²/s
    double h = length(hv);
    if (h == 0) h = 1e-9;
    // eccentricity vector e = ((v² − GM/r)·r − (r·v)·v) / GM
    const Vec3 ev = (b.pos * (v2 - GM / o.r) - b.vel * rv) * (1.0 / GM);
    o.e = length(ev);
    o.bound = o.energy < 0;
    o.a = o.bound ? -GM / (2 * o.energy) : INFINITY;      // semi-major axis from the energy
    o.rPeri = o.bound ? o.a * (1 - o.e) : h * h / GM / (1 + o.e);
    o.rApo = o.bound ? o.a * (1 + o.e) : INFINITY;
    o.inclDeg = std::acos(std::max(-1.0, std::min(1.0, hv.z / h))) * 180 / kPi;
    o.periodYears = o.bound ? 2 * kPi * std::sqrt(o.a * o.a * o.a / GM) / kYear : INFINITY;   // Kepler's third law
    return o;
}

}  // namespace orbit
