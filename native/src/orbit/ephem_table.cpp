#include "ephem_table.h"

#include <algorithm>
#include <cmath>

#include <cstdio>

#include "date_text.h"
#include "real_errors.h"
#include "solar_system.h"
#include "units.h"

namespace orbit {
namespace ephem {

bool dateToJdUT(const char* text, double& jdUT) {
    int64_t ms = 0;
    if (!parseDateTime(text, "", ms)) return false;
    jdUT = double(ms) / 86400000.0 + 2440587.5;
    return true;
}
std::string jdUTToDate(double jdUT) {
    // the civil date of a Julian day: H. Hinnant's days-to-civil algorithm on the days since 1970 (the inverse of date_text.h daysFromCivil)
    const long long z = static_cast<long long>(std::floor(jdUT - 2440587.5)) + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    const long long d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    char b[32];
    std::snprintf(b, sizeof b, "%04lld-%02lld-%02lld", y + (m <= 2), m, d);
    return b;
}

const char* modelName(Model m) { return m == Model::Simple ? "Simple" : m == Model::Real ? "Real (relativity on)" : "Real (relativity off)"; }
const char* integratorName(Integrator i) { return i == Integrator::Accurate ? "Accurate step (the app's, Gragg-Bulirsch-Stoer)" : "Wisdom-Holman map (kept for the tests)"; }

long worstErrorKm(Model m, int planet) {
    if (planet < 0 || planet >= kRealErrCount) return 0;
    return m == Model::Simple ? kSimpleErrKm[planet] : m == Model::Real ? kRealErrKmRelOn[planet] : kRealErrKmRelOff[planet];
}

Ephemeris::Ephemeris(Model m) : model_(m) {
    if (m != Model::Simple) clock_.reset(new NBodyClock(kNBodyStep, m == Model::Real));
}

static Row makeRow(double jd, const Vec3& p) {
    Row r;
    r.jdTDB = jd; r.pos = p;
    const double d = length(p);
    r.distAU = d / units::kAU;
    r.lonDeg = std::atan2(p.y, p.x) * 180 / units::kPi;
    if (r.lonDeg < 0) r.lonDeg += 360;
    r.latDeg = d > 0 ? std::asin(p.z / d) * 180 / units::kPi : 0;
    return r;
}

Row Ephemeris::at(int planet, double jd) {
    if (!clock_) return makeRow(jd, planetStateAt(std::size_t(planet), jd).pos);
    Vec3 x[kNBodies], v[kNBodies];
    clock_->stateAt((jd - kNBodyRefJD) * 86400.0, x, v);
    return makeRow(jd, x[planet + 1] - x[0]);
}

std::vector<Row> Ephemeris::table(int planet, const std::vector<double>& jds) {
    std::vector<Row> out(jds.size());
    // visit the dates in order of distance from the reference epoch outward, so the clock's checkpoints serve the next one (the rows come back in the asked order)
    std::vector<std::size_t> order(jds.size());
    for (std::size_t i = 0; i < order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return std::fabs(jds[a] - kNBodyRefJD) < std::fabs(jds[b] - kNBodyRefJD); });
    for (std::size_t i : order) out[i] = at(planet, jds[i]);
    return out;
}

DriftResult driftCheck(Integrator integrator, bool gr, double years, std::atomic<long>* progress, const std::atomic<bool>* stop) {
    return driftCheckSteps(integrator, gr, long(years * (365.25 * 86400.0) / kNBodyStep), progress, stop);
}

DriftResult driftCheckSteps(Integrator integrator, bool gr, long n, std::atomic<long>* progress, const std::atomic<bool>* stop) {
    DriftResult R;
    R.integrator = integrator; R.relativity = gr; R.years = double(n) * kNBodyStep / (365.25 * 86400.0);
    NBodyState s = nbodyStartState();
    const auto energy = [&](const NBodyState& st) { return gr ? nbodyEnergyGR(st) : nbodyEnergy(st); };
    const double e0 = energy(s);
    const Vec3 l0 = nbodyAngularMomentum(s);
    R.steps = n;
    for (long i = 1; i <= n; i++) {
        if (integrator == Integrator::Accurate) nbodyStepAccurateN(kNBodies, kRealGM, s.x, s.v, kNBodyStep, gr);
        else nbodyStep(s, kNBodyStep, gr);
        if (i % 50 == 0 || i == n) {
            R.endRelEnergy = energy(s) / e0 - 1;
            R.endRelAngular = length(nbodyAngularMomentum(s) - l0) / length(l0);
            R.maxRelEnergy = std::max(R.maxRelEnergy, std::fabs(R.endRelEnergy));
            R.maxRelAngular = std::max(R.maxRelAngular, R.endRelAngular);
            if (progress) progress->store(i);
            if (stop && stop->load()) return R;
        }
    }
    R.done = true;
    return R;
}

}  // namespace ephem
}  // namespace orbit
