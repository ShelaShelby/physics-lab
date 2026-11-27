// 0.92.0: the Data panel's ephemeris table (a planet's place on chosen dates, on the lab's own model) and the integrator's drift check (no graphics, so it can be tested).
//
// The two models are the Orbit Lab's: Simple (the planet is put where JPL's table of approximate elements says, solar_system.h planetStateAt) and Real (the Sun and the planets pull
// on each other, orbit/nbody.h, with or without the Sun's 1PN relativity). Heliocentric places in the ecliptic and mean equinox of J2000, the frame of the JPL Horizons tables
// the lab is tested against (tests/test_jpl.cpp; the worst errors are in orbit/real_errors.h). Earth is the Earth-Moon barycentre, the body the integrator carries. The dates are the
// lab's own: a Julian day in TDB here (the panel turns the user's UT date into TDB with orbit/time_scale.h).
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "nbody.h"
#include "vec3.h"

namespace orbit {
namespace ephem {

// A date as the lab takes it: "YYYY-MM-DD" is 12:00 UT of that day (orbit/date_text.h parseDateTime); the Julian day in UT, and the date back from one
bool dateToJdUT(const char* text, double& jdUT);
std::string jdUTToDate(double jdUT);

enum class Model { Simple, Real, RealNoRelativity };
const char* modelName(Model m);                  // "Simple", "Real (relativity on)", "Real (relativity off)"
// The worst error against Horizons over 1800-2100 for planet i (0 Mercury ... 7 Neptune), km: the quoted numbers of orbit/real_errors.h for the model
long worstErrorKm(Model m, int planet);

struct Row {
    double jdTDB = 0;
    Vec3 pos;                 // heliocentric, m
    double distAU = 0, lonDeg = 0, latDeg = 0;   // distance from the Sun, ecliptic longitude and latitude of J2000
};

class Ephemeris {
public:
    explicit Ephemeris(Model m);
    Model model() const { return model_; }
    // planet 0 Mercury ... 7 Neptune at jdTDB. Real: the clock walks from the reference epoch (2026-01-01 12:00 TDB), so a date far from it takes a while (1.8 s at 1850 on the owner's PC).
    Row at(int planet, double jdTDB);
    // the same for many dates at once (one walk)
    std::vector<Row> table(int planet, const std::vector<double>& jdTDB);
private:
    Model model_;
    std::unique_ptr<NBodyClock> clock_;
};

// ---- the drift check ----
enum class Integrator { Accurate, WisdomHolman };
const char* integratorName(Integrator i);
struct DriftResult {
    Integrator integrator = Integrator::Accurate;
    bool relativity = false;
    double years = 0;
    long steps = 0;
    double maxRelEnergy = 0, maxRelAngular = 0;          // the largest |E/E0 - 1| and |L - L0| / |L0| met, sampled every 50 steps (as tests/test_nbody.cpp)
    double endRelEnergy = 0, endRelAngular = 0;          // the same at the end (signed energy change E/E0 - 1)
    bool done = false;
};
// The eight planets from the lab's start (nbodyStartState), one-day steps, for `years` Julian years. Accurate: the app's step (nbodyStepAccurateN without the Earth-Moon split, whose
// Moon is moved by time and not by the planets, so the energy is only conserved without it: tests/test_nbody.cpp testAccurateEightPlanets). Energy: nbodyEnergy, or nbodyEnergyGR when relativity is on.
// progress (may be null) gets the steps done; stop (may be null) ends the run early (the result then has done = false).
DriftResult driftCheck(Integrator integrator, bool relativity, double years, std::atomic<long>* progress = nullptr, const std::atomic<bool>* stop = nullptr);
// the same for a whole number of one-day steps (the tests)
DriftResult driftCheckSteps(Integrator integrator, bool relativity, long steps, std::atomic<long>* progress = nullptr, const std::atomic<bool>* stop = nullptr);

}  // namespace ephem
}  // namespace orbit
