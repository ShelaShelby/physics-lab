// "Planet physics: Real": the Sun and the eight planets as point masses that pull on each other with Newtonian gravity, moved by a
// symplectic, time-reversible integrator in barycentric Cartesian coordinates. No graphics, so it can be tested.
//
// What is in the model: Newtonian point-mass gravity between the Sun and the eight planets (each planet stands for its whole system, planet plus
// moons, so its GM is the system's), and, as an option (0.10.1), a first-order general-relativity correction (below). What is NOT: higher orders of general
// relativity, the Sun's shape and spin, the pull of moons (apart from being inside the system's GM), asteroids, dwarf planets or comets on the planets. The starting state is NOT from JPL's ephemeris (JPL Horizons could not be reached from the session
// that wrote this): it is the app's own simplified orbital elements (Standish's JPL "approximate positions", 1800-2050) at the reference epoch, turned into barycentric
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
//
// General relativity (option, off unless asked for): the simplest first-order (1PN) correction, the potential of D. Nobili & I. Roxburgh, "Simulation of
// general relativistic corrections in long term numerical integrations of planetary orbits", in Relativity in Celestial Mechanics and Astrometry, IAU Symposium
// 114 (1986) 105-110, as used by REBOUNDx's "gr_potential" (D. Tamayo, H. Rein, P. Shi & D. M. Hernandez, MNRAS 491 (2020) 2885; the formula was read from
// REBOUNDx's source file gr_potential.c on GitHub, not from the 1986 paper, which could not be reached). Each planet feels the extra potential
// Phi(r) = -3 (GM_sun)^2 / (c^2 r^2) of the Sun, r = its distance from the Sun, i.e. an extra acceleration -6 (GM_sun)^2 r_vec / (c^2 r^4). It depends only on
// position, so it fits into the planet-planet kick of the Wisdom-Holman step and keeps the map symplectic and time-reversible. It gives the standard 1PN advance
// of the perihelion per orbit, 6 pi GM / (c^2 a (1 - e^2)) (tested; this standard formula was not read from a source in this session, but it reproduces the published 42.98
// arcseconds per century of Mercury, C. M. Will 2014, Living Reviews in Relativity 17, 4, to 3e-5), but, as REBOUNDx says, gets the mean motion wrong by a relative error of the order of GM / (a c^2) (2.5e-8 for Mercury; the
// size of the coefficient was not checked), and the instantaneous orbital elements are off at the same order. It leaves out: the relativistic pull of the planets on each other and on
// the Sun (only the Sun's field acts), the velocity-dependent terms of the full 1PN equations of motion (the Einstein-Infeld-Hoffmann equations), the Sun's spin
// and shape (Lense-Thirring, J2) and all higher orders.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "vec3.h"

namespace orbit {

// The setting's two values, as written in the settings file. A value that is not one of them is refused (the loader keeps the default).
inline const char* planetPhysicsText(bool real) { return real ? "Real" : "Simple"; }
inline bool planetPhysicsFromText(const std::string& s, bool& real) {
    if (s == "Real") { real = true; return true; }
    if (s == "Simple") { real = false; return true; }
    return false;
}

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
// One Wisdom-Holman step of length h (negative h steps backwards); index 0 is the Sun. gr = true adds the Sun's general-relativity potential (above) to the planets.
void nbodyStepN(int n, const double* gm, Vec3* x, Vec3* v, double h, bool gr = false);

// Acceleration of every body from the others.
void nbodyAccel(const Vec3* x, Vec3* a);
// One step of length h (negative h steps backwards in time): the Wisdom-Holman map above, with the general-relativity potential when gr is true.
void nbodyStep(NBodyState& s, double h, bool gr = false);
// The conserved quantities. Energy is per "G" (GM instead of mass): E' = sum GM_i v_i^2 / 2 - sum_{i<j} GM_i GM_j / r_ij (m^5/s^4); the others the same way.
double nbodyEnergy(const NBodyState& s);
// The same plus the general-relativity potential energy sum_i GM_i * (-3 GM_sun^2 / (c^2 r_i^2)); this is the quantity that stays constant when gr is on.
double nbodyEnergyGR(const NBodyState& s);
Vec3 nbodyAngularMomentum(const NBodyState& s);  // sum GM_i x_i cross v_i
Vec3 nbodyMomentum(const NBodyState& s);         // sum GM_i v_i
Vec3 nbodyCentreOfMass(const NBodyState& s);     // sum GM_i x_i / sum GM_i

// The starting state on Julian day `jd` (default: the reference epoch below): the app's orbital elements for the eight planets, put into barycentric form with the real GMs.
// Each planet's velocity from the elements is scaled by sqrt((GM_sun + GM_i) / kGMSun) so its orbit keeps the shape the elements give under the real GM.
// The reference epoch is 2026-01-01 12:00 UT (Julian day 2461042.0), near "today": the elements are only mean elements, so the N-body planets drift away from
// them the further the date is from the epoch (Saturn: about 0.1 degree per year); near the epoch they are where the elements put them.
constexpr double kNBodyRefJD = 2461042.0;
NBodyState nbodyStartState(double jd = kNBodyRefJD);

// The state at any time, by stepping from the reference epoch forward or backward, with stored checkpoints so a jump does not start from the beginning.
class NBodyClock {
public:
    // gr: the planets feel the Sun's general-relativity potential (see above). start: the state at the reference epoch the clock is built from; nullptr (the
    // default, and all the app does) = nbodyStartState(), the app's orbital elements. Used by the tests to start from the JPL state (0.10.3 stage 1; the app does not).
    explicit NBodyClock(double h = kNBodyStep, bool gr = false, const NBodyState* start = nullptr);
    bool gr() const { return gr_; }
    // Positions and velocities at `tau` seconds after the reference epoch (negative: before it), cubic-Hermite interpolated between the two nearest grid steps.
    void stateAt(double tau, Vec3* x, Vec3* v);
    long stepsTaken() const { return steps_; }            // for tests: how many steps were computed so far
    std::size_t checkpoints() const { return cp_.size(); }
private:
    static constexpr long kEvery = 2000;                  // a checkpoint every 2,000 steps
    static constexpr long kKeep = 200;                    // checkpoints are kept for |index| <= 200 (about +-1,100 years); beyond that the state is only played on
    double h_;
    bool gr_;
    std::map<long, NBodyState> cp_;                       // checkpoint j = the state at step j * kEvery
    NBodyState start_;
    long k_ = 0; bool have_ = false;                      // s0_ is the state at grid step k_, s1_ at k_ + 1
    NBodyState s0_, s1_, last_;
    long steps_ = 0;
    const NBodyState& checkpoint(long j);
    void ensure(long k);
};

}  // namespace orbit
