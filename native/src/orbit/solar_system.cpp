#include "solar_system.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "kepler.h"
#include "time_scale.h"
#include "units.h"

namespace orbit {

using namespace units;

// A made-up but fixed starting angle for each moon, from its name.
static double hashPhase(const char* name) {
    int h = 7;
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(name); *c; ++c) h = (h * 31 + *c) % 6283;
    return h / 1000.0;
}

static void attachMoons(Body& b) {
    for (std::size_t i = 0; i < kMoonCount; i++)
        if (b.name == kMoons[i].planet) b.moons.push_back({&kMoons[i], hashPhase(kMoons[i].name)});
}

// A planet's position and velocity from its JPL elements on Julian day `jd`.
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
    sunTrail.clear();
    nextId_ = 0;
    t = 0;
    moonValid_ = false;                                  // Real: applyReal sets it again below
    crashed = escaped = 0;
    epochMs = unixMs;
    const double jd = timescale::tdbFromUT(unixMs / 86400000.0 + 2440587.5);   // Julian day (0.46.3: in TDB, the JPL data's time scale; the date is UT)

    for (std::size_t i = 0; i < kPlanetCount; i++) {     // planets: JPL elements, each with its rate per century
        const PlanetData& d = kPlanets[i];
        const State st = planetStateAt(i, jd);
        Body b;
        b.id = ++nextId_;
        b.name = d.name; b.color = d.color; b.kind = d.kind; b.type = BodyKind::Planet;
        b.pos = st.pos; b.vel = st.vel;
        b.radius = d.radiusKm * kKm;
        b.massEarths = d.massEarths; b.gravityG = d.gravityMS2 / kStandardGravity; b.feltG = d.feltMS2 / kStandardGravity;   // in g (0.46.3: standard gravity, units.h; was Earth's 9.80)
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
    if (real_) { applyReal(0); sunTrailPoint(); }
    else shiftEarthSimple(jd);                            // 0.46.3: Simple: the real Moon round the JPL elements' Earth-Moon barycentre (0.52.1: and Earth on its other side)
}

// the reference epoch of the N-body clock (Julian day kNBodyRefJD, 2026-01-01 12:00 TDB, the time scale of the JPL data) as if it were Unix milliseconds;
// 0.46.3: the date shown is UT, so applyReal adds TDB - UT (orbit/time_scale.h) to the seconds since this epoch.
static constexpr double kRefUnixMs = (kNBodyRefJD - 2440587.5) * 86400000.0;

// 0.32: the bodies the N-body clock moves in Real: the planets and the Pluto system (0.36: and every other dwarf planet, as test particles; nbody.h)
static bool fromClock(const Body& b) { return b.type == BodyKind::Planet || b.type == BodyKind::Dwarf; }

static double clockTau(int64_t unixMs) {                 // the clock's seconds for loadAt(unixMs) (as applyReal at t = 0, extra = 0)
    const double jdUT = unixMs / 86400000.0 + 2440587.5;
    return (double(unixMs) - kRefUnixMs) / 1000.0 + timescale::deltaTSeconds(jdUT);
}
bool SolarSystem::jumpNeedsWalk(int64_t unixMs) const { return real_ && nb_.needsWalk(clockTau(unixMs)); }
void SolarSystem::prepareJump(int64_t unixMs) { if (real_) nb_.prepare(clockTau(unixMs)); }

void SolarSystem::applyReal(double extra, bool withMoon) {
    Vec3 x[kNBodies], v[kNBodies], px, pv, tx[kNBodyTest], tv[kNBodyTest];
    const double jdUT = julianDay() + extra / kDay, dT = timescale::deltaTSeconds(jdUT);   // TDB - UT, s
    nb_.stateAt((double(epochMs) - kRefUnixMs) / 1000.0 + t + extra + dT, x, v, &px, &pv, tx, tv);
    for (Body& b : bodies) {                             // 0.32: Pluto is drawn at the Pluto system's centre of mass (Charon swings Pluto itself round it)
        if (b.type != BodyKind::Dwarf) continue;
        if (b.name == "Pluto") { b.pos = px - x[0]; b.vel = pv - v[0]; }
        for (int d = 0; d < kNBodyTest; d++)             // 0.36: Ceres, Haumea, Makemake, Eris (Haumea's and Eris's moons are not split off: their centres, as Horizons gives them)
            if (b.name == kNBodyTestName[d]) { b.pos = tx[d] - x[0]; b.vel = tv[d] - v[0]; }
    }
    sunBary_ = x[0];                                     // the Sun's own place: the centre of mass of the whole system is at the origin
    for (std::size_t i = 0; i < kPlanetCount && i < bodies.size(); i++) {
        if (bodies[i].type != BodyKind::Planet || bodies[i].name != kPlanets[i].name) continue;
        bodies[i].pos = x[i + 1] - x[0];                 // the picture stays Sun-centred
        bodies[i].vel = v[i + 1] - v[0];
        bodies[i].drawShift = Vec3{};
    }
    emb_ = x[3] - x[0];                                  // the N-body "Earth" is the Earth-Moon barycentre
    if (!withMoon) return;
    // The real Moon: ELP2000-82B at the date shown (+ extra). mu = GM_Moon / GM_Earth (JPL page, nbody.h). This ignores ELP's own Earth-Moon mass ratio (its constants were fitted to DE200; its value was not
    // looked up). The effect is that ratio's relative difference from the page's, times Earth's 4,300-5,000 km offset from the barycentre: for example 5 m if they differ by 1e-6 (for comparison, the
    // page's ratio and Horizons' header agree to 1e-8, tests/test_moon_real.cpp). Nothing is done to hide it: Earth is drawn at the barycentre minus mu / (1 + mu) of the ELP vector.
    updateMoon(jdUT + dT / kDay);
    if (!moonValid_) return;
    static const double mu = kRealGMMoon / kRealGMEarthAlone;
    for (std::size_t i = 0; i < kPlanetCount && i < bodies.size(); i++)
        if (bodies[i].type == BodyKind::Planet && bodies[i].name == "Earth" && !moonFast_) { bodies[i].drawShift = moonGeo_ * (-mu / (1.0 + mu)); bodies[i].pos = emb_ + bodies[i].drawShift; }
}

// 0.52.1: Simple: Earth's body (the leapfrog's state, from the JPL elements of the Earth-Moon barycentre) is drawn on the barycentre's other side from the
// Moon, EMB - g mu / (1 + mu), as in Real (applyReal): until 0.52.0 Simple drew Earth at the barycentre itself, 4,300-5,000 km off. The shift is kept in
// the body's drawShift and taken off again before the next leapfrog step, so the orbit itself is the barycentre's; Earth's velocity stays the barycentre's.
void SolarSystem::shiftEarthSimple(double jd) {
    static const double mu = kRealGMMoon / kRealGMEarthAlone;
    for (Body& b : bodies) {
        if (b.type != BodyKind::Planet || b.name != "Earth") continue;
        b.pos = b.pos - b.drawShift; b.drawShift = Vec3{};
        emb_ = b.pos;
        updateMoon(jd);
        if (moonValid_ && !moonFast_) { b.drawShift = moonGeo_ * (-mu / (1.0 + mu)); b.pos = b.pos + b.drawShift; }
    }
}

void SolarSystem::updateMoon(double jd) {
    moonValid_ = jd >= kMoonElpFirstJD && jd <= kMoonElpLastJD;
    if (!moonValid_) return;
    moonGeo_ = moonGeocentricElp(jd);
    const double h = 1.0 / 48;                           // half an hour: the direction of motion for the orbit plane
    const Vec3 dv = moonGeocentricElp(jd + h) - moonGeocentricElp(jd - h);
    const Vec3 nrm = cross(moonGeo_, dv);
    moonNormal_ = nrm * (1.0 / length(nrm));
}

Vec3 SolarSystem::moonPosition() const {
    static const double mu = kRealGMMoon / kRealGMEarthAlone;
    return emb_ + moonGeo_ * (1.0 / (1.0 + mu));
}

void SolarSystem::sunTrailPoint() {                       // the Sun's path: a point whenever it has moved kSunTrailStep since the last one
    if (sunTrail.empty() || length(sunBary_ - sunTrail.back()) >= kSunTrailStep) { sunTrail.push_back(sunBary_); if (sunTrail.size() > 1000) sunTrail.pop_front(); }
}

void SolarSystem::realTrail(Body& b) {                   // a trail point every 2 % of the distance to the Sun, as the leapfrog does (Earth: the barycentre's path, without the Moon's 4,300-5,000 km wobble)
    const Vec3 p = b.name == "Earth" ? emb_ : b.pos;
    const double r = length(p), lim = 0.02 * (r > kSimLength ? r : kSimLength);
    if (b.trail.empty() || length(p - b.trail.back()) > lim) { b.trail.push_back(p); if (b.trail.size() > 1000) b.trail.pop_front(); }
}

void SolarSystem::setRealPlanets(bool on) {
    if (on == real_) return;
    real_ = on;
    if (on) { applyReal(0); sunTrail.clear(); sunTrailPoint(); }
    else loadAt(currentUnixMs());                        // back to Simple: everything reloaded from the elements at the date shown (and no Sun's path)
}

void SolarSystem::setRelativity(bool on) {
    if (on == gr_) return;
    gr_ = on;
    nb_ = NBodyClock(kNBodyStep, on);                    // a new clock: its stored states were made with the other setting
    if (real_) {
        applyReal(0);
        for (Body& b : bodies) if (fromClock(b)) b.trail.clear();
        sunTrail.clear(); sunTrailPoint();                // (GR moves the Sun's path a little too)
    }
}

void SolarSystem::step(double dt, double tspeed) {
    // Simulated time for this step, cut into equal sub-steps of at most kMaxSubStep
    // (about 5.6 simulated hours) so fast Time speeds stay accurate.
    const double total = dt * kSimTime * std::pow(10.0, tspeed);   // s
    const int nSub = std::max(1, static_cast<int>(std::ceil(total / kMaxSubStep)));
    const double h = total / nSub, hh = h / 2;
    const double GM = kGMSun;
    if (!real_)                                           // 0.52.1: Simple: Earth's state is the Earth-Moon barycentre; the drawn shift (shiftEarthSimple) is taken off before the step
        for (Body& b : bodies) if (b.type == BodyKind::Planet && b.name == "Earth") { b.pos = b.pos - b.drawShift; b.drawShift = Vec3{}; }
    {
        double period = 27.3217;                          // the Moon's period in the data, days (the drawing's rule: more than kMoonFastLaps laps a real second is "fast")
        for (std::size_t i = 0; i < kMoonCount; i++) if (std::string(kMoons[i].planet) == "Earth") period = kMoons[i].periodDays;
        moonFast_ = kSimTime / kDay * std::pow(10.0, tspeed) / period > kMoonFastLaps;   // (0.52.1: in Simple too)
    }

    // Leapfrog (kick-drift-kick, a.k.a. velocity Verlet) around the Sun:
    //   v += a·h/2;  x += v·h;  a = −GM·x/(r³ + ε);  v += a·h/2
    // Each body only feels the Sun, so every body can run all its sub-steps in one go.
    // The pull at the end of one sub-step is the pull at the start of the next, so it is
    // worked out once and reused.
    for (Body& b : bodies) {
        if (b.type == BodyKind::Comet) continue;          // comets follow their exact orbit (set below)
        if (real_ && fromClock(b)) continue;              // Real: the planets (and since 0.32 Pluto) come from the N-body clock (below)
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
                     if (real_ && fromClock(b)) return false;
                     const double r = length(b.pos);
                     if (r < kCrashRadius) { crashed++; return true; }
                     if (r > kLostRadius) { escaped++; return true; }
                     return false;
                 }), bodies.end());
    if (real_) {                                          // the planets: walk to the new date in steps of about 2 days, with a trail point where needed
        const int nSub = std::max(1, std::min(2000, static_cast<int>(std::ceil(total / (2 * kDay)))));
        for (int k = 1; k <= nSub; k++) {
            t += total / nSub;
            applyReal(0, k == nSub);                      // the Moon (a few hundred sine terms) only once, at the end
            for (Body& b : bodies) if (fromClock(b)) realTrail(b);
            sunTrailPoint();
        }
    } else
        t += total;
    const double jd = julianDayTDB();
    if (!real_) shiftEarthSimple(jd);                    // 0.46.3: Simple: the real Moon (ELP2000-82B) round the barycentre, as in Real (0.52.1: and Earth drawn off it)
    for (Body& b : bodies)
        if (b.type == BodyKind::Comet) {
            const State st = cometState(*b.comet, jd);
            b.pos = st.pos; b.vel = st.vel;
        }
}

Vec3 SolarSystem::barycentre() const {
    if (real_) return sunBary_ * -1.0;                    // Real: the Sun's place seen from the barycentre, turned round
    double mt = kSunInEarthMasses;                        // the Sun weighs 332,946 Earths (units.h)
    Vec3 s;
    for (const Body& b : bodies) { s = s + b.pos * b.massEarths; mt += b.massEarths; }
    return s * (1.0 / mt);
}

double SolarSystem::julianDay() const { return epochMs / 86400000.0 + 2440587.5 + t / kDay; }
double SolarSystem::julianDayTDB() const { return timescale::tdbFromUT(julianDay()); }

int64_t SolarSystem::currentUnixMs() const { return epochMs + static_cast<int64_t>(std::llround(t * 1000.0)); }

void orbitPath(const Body& b, int n, std::vector<Vec3>& out) { orbitPath(b, 0, 2 * kPi, n, out); }
void orbitPath(const Body& b, double E0, double E1, int n, std::vector<Vec3>& out) {
    out.clear();
    const OrbitShape o = orbitOf(b);
    if (!o.bound || n < 3) return;
    const Vec3 bp = b.pos - b.drawShift;                  // 0.52.1: the orbit's own point (Earth: the Earth-Moon barycentre)
    const double GM = kGMSun, v2 = dot(b.vel, b.vel), rv = dot(bp, b.vel);
    const Vec3 hv = cross(bp, b.vel), ev = (bp * (v2 - GM / o.r) - b.vel * rv) * (1.0 / GM);
    const double h = length(hv);
    if (h <= 0) return;
    const Vec3 W = hv * (1 / h);
    Vec3 P = o.e > 1e-12 ? ev * (1 / o.e) : bp * (1 / o.r);   // towards the perihelion (a circle: any direction in the plane)
    const Vec3 Q = cross(W, P);
    const double bAx = o.a * std::sqrt(std::max(0.0, 1 - o.e * o.e));
    // 0.66.0: the whole orbit's cos E and sin E (the same angles for every body, every frame: about 27,000 sines and cosines a frame in orbit.heaviest)
    // from a table made once with the same expressions, so the same numbers bit for bit
    if (E0 == 0 && E1 == 2 * kPi && n == 256) {
        static const std::vector<double> tab = [] {
            std::vector<double> t(2 * 257);
            for (int i = 0; i <= 256; i++) { const double E = 0 + (2 * kPi - 0) * i / 256; t[std::size_t(2 * i)] = std::cos(E); t[std::size_t(2 * i + 1)] = std::sin(E); }
            return t;
        }();
        for (int i = 0; i <= n; i++) {
            const double x = o.a * (tab[std::size_t(2 * i)] - o.e), y = bAx * tab[std::size_t(2 * i + 1)];
            out.push_back(P * x + Q * y);
        }
        return;
    }
    for (int i = 0; i <= n; i++) {
        const double E = E0 + (E1 - E0) * i / n, x = o.a * (std::cos(E) - o.e), y = bAx * std::sin(E);
        out.push_back(P * x + Q * y);
    }
}

OrbitShape orbitOf(const Body& b) {
    const double GM = kGMSun;
    OrbitShape o{};
    const Vec3 bp = b.pos - b.drawShift;                  // 0.52.1: the orbit's own point (Earth: the Earth-Moon barycentre, as the card says)
    o.r = length(bp);
    const double v2 = dot(b.vel, b.vel);
    o.v = std::sqrt(v2);
    o.energy = v2 / 2 - GM / o.r;                         // specific orbital energy, J/kg
    const double rv = dot(bp, b.vel);
    const Vec3 hv = cross(bp, b.vel);                     // specific angular momentum, m²/s
    double h = length(hv);
    if (h == 0) h = 1e-9;
    // eccentricity vector e = ((v² − GM/r)·r − (r·v)·v) / GM
    const Vec3 ev = (bp * (v2 - GM / o.r) - b.vel * rv) * (1.0 / GM);
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
