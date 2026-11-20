// "Planet physics: Real": the Sun and the eight planets (0.32: and the Pluto system, below) as point masses that pull on each other with Newtonian gravity, moved by a
// symplectic, time-reversible integrator in barycentric Cartesian coordinates. No graphics, so it can be tested.
//
// 0.29: THE APP NOW USES THE ACCURATE STEP (below, "0.29: the ACCURATE step"): Gragg-Bulirsch-Stoer, the Earth and the Moon pulled separately, and with relativity on the
// Sun's 1PN field from the IERS Conventions 2010. Against Horizons over 1800-2100 (tests/test_jpl.cpp, relativity on): Mercury 24 km, Venus 5, Earth 75, Mars 125, Jupiter 379,
// Saturn 336, Uranus 515, Neptune 2,582 km (0.28: Mercury 29,231 km at best). Now (0.32 Pluto, 0.35 every Moon term): orbit/real_errors.h, Earth 14 km. The Wisdom-Holman map and the Nobili-Roxburgh potential described next are kept for the tests.
//
// What is in the model: Newtonian point-mass gravity between the Sun and the eight planets (each planet stands for its whole system, planet plus
// moons, so its GM is the system's), and, as an option (0.10.1), a first-order general-relativity correction (below). What is NOT: higher orders of general
// relativity, the Sun's shape and spin, the pull of moons (apart from being inside the system's GM), asteroids, dwarf planets other than Pluto (0.32) or comets on the planets. The starting state (0.10.3) is JPL Horizons' (DE441) position and velocity of the Sun and each planet system (Earth: the Earth-Moon barycentre) at
// 2026-01-01 12:00 TDB, moved to the lab's own centre of mass (below). The clock here is TDB; the lab's dates are UT and are turned into TDB (orbit/time_scale.h, 0.46.3). Over 1800-2100 the planets then stay within a few
// thousand km of Horizons (Mercury 29,230 km at worst without relativity; tests/test_jpl.cpp measures it).
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

#include <cmath>
#include <cstdint>
#include <future>
#include <deque>
#include <map>
#include <string>
#include <utility>
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
// (https://ssd.jpl.nasa.gov/astro_par.html), the DE440 values (Park et al. 2021, The Astronomical Journal). First read from an excerpt of that page; on 2026-10-03 the
// owner read the values from the page itself and they matched the code. In nbody.cpp, with the source next to each number.
extern const double kRealGM[kNBodies];
// The Earth-system GM above is Earth + Moon. The two parts, from the same JPL page (DE440 values, read from the page itself on 2026-10-03: Earth 398600.435507, Moon 4902.800118 km^3/s^2),
// used in Real mode to split the Earth-Moon barycentre into Earth and Moon (m^3/s^2). mu = kRealGMMoon / kRealGMEarthAlone = 0.0123000... (Horizons' header of the Moon, DE441, gives
// Earth/Moon = 81.3005690769, which agrees to better than 1e-8; tests/test_moon_real.cpp checks it).
extern const double kRealGMEarthAlone, kRealGMMoon;

// 0.32: the Pluto system (Pluto, Charon and the small moons, as one body at their centre of mass) is the tenth body of the app's Real planets. It is carried apart
// from the nine above (pluto = true), so a state made by the tests from the nine bodies alone, and every 9-body function, means what it always meant.
// GM of the Pluto system: 975.5 km^3/s^2, JPL "Astrodynamic Parameters" (https://ssd.jpl.nasa.gov/astro_par.html, DE440, "Pluto system 975.500000"; read from the page 6 Nov 2026).
// Start: Horizons' Pluto Barycenter (9) at the reference epoch (data/jpl/horizons-pluto.txt, DE441). Measured with tests/test_jpl.cpp: with it, Uranus comes 4 times and Neptune
// almost 2 times closer to Horizons over 1800-2100. The biggest asteroids were tried and left out: Ceres, Vesta and Pallas alone (without the rest of the belt, which DE441 has)
// brought Saturn and Mars closer but moved Earth, Uranus and Neptune further away (REPORT.md of 0.32).
extern const double kRealGMPluto;
// 0.36: the other four dwarf planets, Ceres, Haumea, Makemake and Eris (the order of solar_data's kDwarfs without Pluto), ride along as test particles: the Sun, the planets,
// the Earth and Moon apart, Pluto and the Sun's 1PN field pull on them, and they pull on nothing (GM 0), so the planets and Pluto move exactly, bit for bit, as without them.
// Start: their Horizons rows at the reference epoch (data/jpl/horizons-ceres.txt and so on; JPL's small-body orbit solutions, integrated by Horizons with the 16 largest asteroids
// as perturbers, which the lab has not). Carried with the Pluto system (dwarfs = pluto in nbodyStartState).
constexpr int kNBodyTest = 4;
constexpr const char* kNBodyTestName[kNBodyTest] = {"Ceres", "Haumea", "Makemake", "Eris"};
constexpr int kNBodyMax = kNBodies + 1 + kNBodyTest;   // the most bodies the accurate step handles (the nine, Pluto, and the four test particles)
struct NBodyState {
    Vec3 x[kNBodies], v[kNBodies];            // barycentric positions (m) and velocities (m/s)
    bool pluto = false;                       // 0.32: the Pluto system is in (the app's start state); its position and velocity:
    Vec3 px, pv;
    bool dwarfs = false;                      // 0.36: the four test-particle dwarf planets are in (only together with Pluto); positions and velocities:
    Vec3 dx[kNBodyTest], dv[kNBodyTest];
};

// The step: one day. Over 1,000 years the largest relative energy error is 2.8e-9 at this step (1.1e-8 at 2 days, 7e-10 at half a day; REPORT.md).
constexpr double kNBodyStep = 86400.0;        // s

// The same integrator for any number n of bodies with GM values gm (used by the 9-body wrappers below, and by the tests for a Sun-and-one-planet system).
void nbodyAccelN(int n, const double* gm, const Vec3* x, Vec3* a);
// One Wisdom-Holman step of length h (negative h steps backwards); index 0 is the Sun. gr = true adds the Sun's general-relativity potential (above) to the planets.
void nbodyStepN(int n, const double* gm, Vec3* x, Vec3* v, double h, bool gr = false);

// One step of length h (negative h steps backwards in time): the Wisdom-Holman map above, with the general-relativity potential when gr is true. Since 0.29 for the tests only.
void nbodyStep(NBodyState& s, double h, bool gr = false);
// ---- 0.29: the ACCURATE step, which the clock (and so the app) uses since 0.29 ----
// Why: measured against the Horizons tables over 1800-2100, the Wisdom-Holman map above at a 1-day step is itself thousands of km off for the inner planets (Mercury about 11,700 km,
// Venus 15,500 km against the converged solution of the same equations; 0.29 REPORT.md), which hid the effect of relativity: the old potential seemed to make every planet worse.
// The step: the Gragg-Bulirsch-Stoer method (Gragg's modified midpoint rule, extrapolated; Hairer, Norsett & Wanner, Solving Ordinary Differential Equations I, 2nd ed., 1993,
// section II.9) with kGbsStages stages, one macro step per day. At this setting it agrees with 8 stages to about 1 km over 1800-2100 (tests/test_nbody.cpp measures it). Not
// symplectic: its energy error is measured instead (tests).
// What it adds to the Newtonian pulls:
//  * the Earth-Moon split (always): the Earth-Moon barycentre is pulled as the GM-weighted mean of the pulls at the Earth's and the Moon's real places (the Moon from ELP2000-82B,
//    its kMoonSplitTerms largest terms, a quadratic in time across each step), and the other bodies feel the Earth and the Moon at their places. Without it the barycentre is
//    23,000 km off Horizons by 1800 (the Sun's tide on the Earth-Moon pair);
//  * with gr: the Sun's first-order (1PN) Schwarzschild field on each planet, beta = gamma = 1 (IERS Conventions 2010, IERS Technical Note 36, eq. 10.12), and the opposite pull on
//    the Sun. Left out: the planets' own relativistic pulls (the full Einstein-Infeld-Hoffmann equations; tried in 0.29: under 10 km different over 1800-2100), the Sun's
//    shape and spin, the asteroids and all higher orders (0.32: Pluto is in, as a tenth body).
constexpr int kGbsStages = 5;                 // substep counts 2, 4, 6, 8, 10: 35 evaluations of the pulls per step
constexpr int kMoonSplitTerms = 487;          // 0.35: every kept term (the full series of moon_elp.h; tests/test_moon.cpp). 0.29-0.34 used the 100 largest (within 82 km of it; Earth then 75 km off Horizons, now 14): moon_elp.cpp's faster sum makes all 487 cheaper than the 100 were
// One accurate step of length h for n bodies (index 0 the Sun). emb: the index of the Earth-Moon barycentre, or -1 for no split; moon3: the Moon seen from the Earth at the
// start, the middle and the end of the step (used when emb >= 0).
void nbodyStepAccurateN(int n, const double* gm, Vec3* x, Vec3* v, double h, bool gr, int emb = -1, const Vec3* moon3 = nullptr);
// The 9 bodies, from the state at Julian date jdTDB (TDB) to jdTDB + h / 86400.
void nbodyStepAccurate(NBodyState& s, double h, bool gr, double jdTDB);

// The conserved quantities. Energy is per "G" (GM instead of mass): E' = sum GM_i v_i^2 / 2 - sum_{i<j} GM_i GM_j / r_ij (m^5/s^4); the others the same way.
double nbodyEnergy(const NBodyState& s);
// The same plus the general-relativity potential energy sum_i GM_i * (-3 GM_sun^2 / (c^2 r_i^2)); this is the quantity that stays constant when gr is on.
double nbodyEnergyGR(const NBodyState& s);
Vec3 nbodyAngularMomentum(const NBodyState& s);  // sum GM_i x_i cross v_i
Vec3 nbodyMomentum(const NBodyState& s);         // sum GM_i v_i
Vec3 nbodyCentreOfMass(const NBodyState& s);     // sum GM_i x_i / sum GM_i

// The reference epoch: 2026-01-01 12:00 TDB (Barycentric Dynamical Time), Julian day 2461042.0 (the time scale of JPL Horizons' rows). The lab's calendar dates are
// UT; 0.46.3 (outside review): they are turned into TDB with Delta T (orbit/time_scale.h: 69.184 s in 2026), before which they were used as TDB unconverted.
constexpr double kNBodyRefJD = 2461042.0;
// The default starting state (the clock's, and the app's): the JPL Horizons (DE441) state of the Sun and the eight planet systems at the reference epoch (jpl_epoch.inc,
// made by tools/make_jpl_data.py from data/jpl), moved to the lab's own 9-body centre of mass: the GM-weighted mean position and velocity (the lab's GMs) are subtracted,
// so the centre of mass is at rest at the origin as the integrator expects (the shift is 128.5 km and 0.039 mm/s; JPL's barycentre also holds Pluto, the asteroids and
// so on, which the lab has not). Heliocentric positions and velocities are exactly JPL's.
NBodyState nbodyStartState(bool withPluto = false);   // 0.32: withPluto = the app's start (the clock's default); without, the nine bodies as before
// LEGACY, for tests only (the app does not use it since 0.10.3): the app's own orbital elements on Julian day `jd`, put into barycentric form with the real GMs. Each planet's
// velocity from the elements is scaled by sqrt((GM_sun + GM_i) / kGMSun). Mean elements: the planets drift away from them (Saturn: about 0.1 degree per year), which is why
// the app now starts from JPL; kept to show the before-and-after errors in tests/test_jpl.cpp and that the integrator itself did not change.
NBodyState nbodyElementsStartState(double jd = kNBodyRefJD);

// The state at any time, by stepping from the reference epoch forward or backward with the accurate step (0.29), with stored checkpoints so a jump does not start from the
// beginning. 0.29: the steps are made a stretch (kEvery steps, between two checkpoints) at a time; the last few stretches are kept, so playing time backwards does not step
// the same stretch again every frame, and while time runs the NEXT stretch in the direction of travel is made on a background thread, so the drawing does not wait for it.
// The background stretch is made by exactly the same steps from exactly the same state, so the result never depends on which thread made it (the tests check it bit for bit).
// 0.36 stage 6: a grid state and its accelerations (the accurate step's own pulls, in its order: the nine, Pluto, the four test particles), for the interpolation
struct NBodyGridPoint { NBodyState s; Vec3 a[kNBodyMax]; };

class NBodyClock {
public:
    // gr: the planets feel the Sun's 1PN field (see above). start: the state at the reference epoch the clock is built from; nullptr (the default, and all the app does) =
    // nbodyStartState(true), the JPL state with the Pluto system (0.32). The tests pass the legacy elements state to compare.
    explicit NBodyClock(double h = kNBodyStep, bool gr = false, const NBodyState* start = nullptr);
    bool gr() const { return gr_; }
    // Positions and velocities at `tau` seconds after the reference epoch (negative: before it), interpolated between the two nearest grid steps: 0.36 stage 4, a quintic
    // Hermite through the positions, velocities and accelerations (the step's own pulls) at both ends; the cubic one before it (positions and velocities only) put Mercury
    // up to 19.5 km off between whole days, against 0.02 km on them (Horizons, every 7 hours in early 2026; tests/test_jpl.cpp). On a grid step it is that step's state exactly.
    void stateAt(double tau, Vec3* x, Vec3* v, Vec3* plutoX = nullptr, Vec3* plutoV = nullptr, Vec3* dwarfX = nullptr, Vec3* dwarfV = nullptr);   // (0.32: the Pluto system too, when the state has it; 0.36: dwarfX/dwarfV, kNBodyTest each, the four dwarf planets)
    long long stepsTaken() const { return steps_; }            // for tests: how many steps were computed so far (a background stretch counts when it is used)
    std::size_t checkpoints() const { return cp_.size(); }
    void setPrefetch(bool on) { prefetch_ = on; }              // for tests: the background stretch can be switched off (on by default)
    // 0.67.0: a jump far from every kept checkpoint (1850 from 2026: about 64,000 steps, 1.8 s on the owner's PC) walked on a worker, so the frame does not stop.
    // needsWalk: stateAt(tau) would walk from a checkpoint more than a stretch away. prepare(tau): start that walk and tau's stretch on a worker (one walk at a
    // time; a later prepare waits its turn). prepared(): called on the drawing thread; true once the walk for the last prepare is finished and taken in (its
    // checkpoints kept and its stretch stored as the drawing thread's own walk would have). The worker takes copies only, so stateAt may be used meanwhile; the
    // states are bit for bit those of the walk on the drawing thread (the same steps from the same state; tests/test_nbody.cpp).
    bool needsWalk(double tau) const;
    void prepare(double tau);
    bool prepared();
    static constexpr long kEvery = 256;                   // a checkpoint every 256 steps (0.29: was 2,000 with the cheaper step)
    static constexpr long kKeep = 1600;                   // checkpoints are kept for |index| <= 1,600 (about +-1,100 years); beyond that the state is only played on
    static constexpr std::size_t kFar = 64;               // 0.40: beyond kKeep, the most recent checkpoints kept (the walk on starts from the newest)
    static constexpr std::size_t kStretches = 6;          // the stretches whose every state is kept (the most recently used)
private:
    using Stretch = std::vector<NBodyGridPoint>;          // stretch j: the states at steps j * kEvery .. (j + 1) * kEvery (kEvery + 1 states), with their pulls (0.36 stage 6)
    double h_;
    bool gr_;
    bool prefetch_ = true;
    std::map<long, NBodyState> cp_;                       // checkpoint j = the state at step j * kEvery
    std::deque<long> farOrder_;                           // 0.40: the kept checkpoints beyond kKeep, oldest first
    void keep(long q, const NBodyState& s);
    NBodyState start_;
    std::map<long, Stretch> seg_;
    std::map<long, long long> segUse_;                    // when each kept stretch was last used
    long long useClock_ = 0;
    std::shared_future<Stretch> pending_;                 // the stretch being made in the background, and its index
    long pendingJ_ = 0;
    NBodyState s0_, s1_;                                  // the states at grid steps k_ and k_ + 1
    Vec3 a0_[kNBodyMax], a1_[kNBodyMax];                  // 0.36 stage 4: their accelerations (the order of the accurate step: nine, Pluto, the four test particles)
    long k_ = 0; bool have_ = false;
    long long steps_ = 0;                                 // (long is 32 bits on Windows: this counter would wrap after about 32 hours at the top Time speed)
    void step(NBodyState& s, long k, int dir);            // s, the state at grid step k, becomes the state at k + dir
    const NBodyState& checkpoint(long j);
    const NBodyState* knownCheckpoint(long j) const;      // a checkpoint already made, or nullptr
    void ensure(long k);
    void addStretch(long j, Stretch&& sg);                // stores a made stretch (forgetting the one used longest ago) and keeps its two ends as checkpoints
    struct Walk { std::vector<std::pair<long, NBodyState>> cps; long j = 0; Stretch sg; long long steps = 0; };
    std::shared_future<Walk> walk_;                       // 0.67.0: the far walk under way, and the stretch it is for
    long walkJ_ = 0;
    bool wantWalk_ = false; double wantTau_ = 0;          // the last prepare's date (started when the walk before it is taken in)
    static long stretchOf(long k) { return k >= 0 ? k / kEvery : -((-k + kEvery - 1) / kEvery); }
    long stepOf(double tau) const { return long(std::floor(tau / h_)); }
};

}  // namespace orbit
