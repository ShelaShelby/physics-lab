// "Planet physics: Real": the Sun and the eight planets as point masses that pull on each other with Newtonian gravity, moved by a
// symplectic, time-reversible integrator in barycentric Cartesian coordinates. No graphics, so it can be tested.
//
// What is in the model: Newtonian point-mass gravity between the Sun and the eight planets (each planet stands for its whole system, planet plus
// moons, so its GM is the system's). What is NOT: general relativity, the Sun's shape, the pull of moons (apart from being inside the system's GM),
// asteroids, dwarf planets or comets on the planets. The starting state is NOT from JPL's ephemeris (JPL Horizons could not be reached from the session
// that wrote this): it is the app's own simplified orbital elements (Standish's JPL "approximate positions", 1800-2050) at J2000, turned into barycentric
// positions and velocities, so the planets drift from their true positions over decades and more.
//
// The integrator: a Wisdom-Holman map, J. Wisdom & M. Holman, "Symplectic maps for the N-body problem", The Astronomical Journal 102 (1991) 1528-1538, in the
// "democratic heliocentric" variables of M. J. Duncan, H. F. Levison & M. H. Lee, "A Multiple Time Step Symplectic Algorithm for Integrating Close
// Encounters", The Astronomical Journal 116 (1998) 2067-2077. The Hamiltonian is split into each planet alone on its Kepler orbit round the Sun (solved
// exactly), the Sun's own motion, and the planet-planet pulls (small); the three are applied in a symmetric order, so the map is symplectic, time-reversible
// and second order. Chosen over a Yoshida composition of leapfrog steps (H. Yoshida 1990, Physics Letters A 150, 262), which I tried first: with the same cost
// that scheme made Mercury's perihelion creep by about 2,600 arcseconds per century at a half-day step (the real planetary effect is about 530), because it
// does not solve the Kepler motion exactly. The Wisdom-Holman error is of order (planet mass / Sun mass) times the step squared, so the orbits do not creep.
// The tests measure the order, the time reversal and the conserved quantities.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "vec3.h"

namespace orbit {

constexpr int kNBodies = 9;                   // the Sun, then Mercury ... Neptune

// GM (m^3/s^2): the Sun, then the system of each planet (Earth: Earth + Moon). Source: JPL "Astrodynamic Parameters"
// (https://ssd.jpl.nasa.gov/astro_par.html), the DE440 values (Park et al. 2021, The Astronomical Journal). Read from an excerpt of that page: the page itself
// was blocked from the session that wrote this. In nbody.cpp, with the source next to each number.
extern const double kRealGM[kNBodies];

struct NBodyState {
    Vec3 x[kNBodies], v[kNBodies];            // barycentric positions (m) and velocities (m/s)
};

// The step: one day. Over 1,000 years the largest relative energy error is 2.8e-9 at this step (1.1e-8 at 2 days, 7e-10 at half a day; REPORT.md).
constexpr double kNBodyStep = 86400.0;        // s

// The same integrator for any number n of bodies with GM values gm (used by the 9-body wrappers below, and by the tests for a Sun-and-one-planet system).
void nbodyAccelN(int n, const double* gm, const Vec3* x, Vec3* a);
void nbodyStepN(int n, const double* gm, Vec3* x, Vec3* v, double h);   // one Wisdom-Holman step of length h (negative h steps backwards); index 0 is the Sun

// Acceleration of every body from the others.
void nbodyAccel(const Vec3* x, Vec3* a);
// One step of length h (negative h steps backwards in time): the Wisdom-Holman map above.
void nbodyStep(NBodyState& s, double h);
// The conserved quantities. Energy is per "G" (GM instead of mass): E' = sum GM_i v_i^2 / 2 - sum_{i<j} GM_i GM_j / r_ij (m^5/s^4); the others the same way.
double nbodyEnergy(const NBodyState& s);
Vec3 nbodyAngularMomentum(const NBodyState& s);  // sum GM_i x_i cross v_i
Vec3 nbodyMomentum(const NBodyState& s);         // sum GM_i v_i
Vec3 nbodyCentreOfMass(const NBodyState& s);     // sum GM_i x_i / sum GM_i

// The starting state on Julian day `jd` (default J2000): the app's orbital elements for the eight planets, put into barycentric form with the real GMs.
// Each planet's velocity from the elements is scaled by sqrt((GM_sun + GM_i) / kGMSun) so its orbit keeps the shape the elements give under the real GM.
NBodyState nbodyStartState(double jd = 2451545.0);
constexpr double kNBodyRefJD = 2451545.0;     // the reference epoch, J2000.0

// The state at any time, by stepping from the reference epoch forward or backward, with stored checkpoints so a jump does not start from the beginning.
class NBodyClock {
public:
    explicit NBodyClock(double h = kNBodyStep);
    // Positions and velocities at `tau` seconds after the reference epoch (negative: before it), cubic-Hermite interpolated between the two nearest grid steps.
    void stateAt(double tau, Vec3* x, Vec3* v);
    long stepsTaken() const { return steps_; }            // for tests: how many steps were computed so far
    std::size_t checkpoints() const { return cp_.size(); }
private:
    static constexpr long kEvery = 2000;                  // a checkpoint every 2,000 steps
    static constexpr long kKeep = 200;                    // checkpoints are kept for |index| <= 200 (about +-1,100 years); beyond that the state is only played on
    double h_;
    std::map<long, NBodyState> cp_;                       // checkpoint j = the state at step j * kEvery
    NBodyState start_;
    long k_ = 0; bool have_ = false;                      // s0_ is the state at grid step k_, s1_ at k_ + 1
    NBodyState s0_, s1_, last_;
    long steps_ = 0;
    const NBodyState& checkpoint(long j);
    void ensure(long k);
};

}  // namespace orbit
