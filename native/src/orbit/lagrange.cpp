#include "lagrange.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "solar_data.h"
#include "units.h"
#include "nbody.h"

namespace orbit {
namespace lagrange {

namespace {
// the force along the line at rest in the rotating frame (Tatum 16.1.9 written as one function: gravity of M1 at -mu and M2 at 1 - mu, plus the centrifugal pull x)
double lineForce(double mu, double x) {
    const double d1 = x + mu, d2 = x - 1 + mu;
    return -(1 - mu) * d1 / std::fabs(d1 * d1 * d1) - mu * d2 / std::fabs(d2 * d2 * d2) + x;
}
// the root of lineForce in (lo, hi) where the force changes sign: bisection down to the last digits of a double
double root(double mu, double lo, double hi) {
    const bool lowNegative = lineForce(mu, lo) < 0;
    for (int k = 0; k < 400; k++) {
        const double mid = 0.5 * (lo + hi);
        if (mid <= lo || mid >= hi) break;
        if ((lineForce(mu, mid) < 0) == lowNegative) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}
}  // namespace

Points points(double mu) {
    Points P;
    P.mu = mu;
    const double rh = std::cbrt(mu / 3);                        // the Hill radius: L1 and L2 are close to it from M2, so the brackets stay clear of M2's pole
    const double m1 = -mu, m2 = 1 - mu;
    P.x[0] = root(mu, m1 + 1e-9, m2 - 1e-3 * rh);               // between the masses: the force goes from -infinity at M1 to +infinity at M2
    P.x[1] = root(mu, m2 + 1e-3 * rh, m2 + 2);                  // beyond M2
    P.x[2] = root(mu, m1 - 2, m1 - 1e-9);                       // beyond M1
    P.x[3] = P.x[4] = 0.5 - mu;
    P.y[0] = P.y[1] = P.y[2] = 0;
    P.y[3] = std::sqrt(3.0) / 2;
    P.y[4] = -P.y[3];
    return P;
}

void restAcceleration(double mu, double x, double y, double& ax, double& ay) {
    const double r1 = std::hypot(x + mu, y), r2 = std::hypot(x - 1 + mu, y);
    const double g1 = (1 - mu) / (r1 * r1 * r1), g2 = mu / (r2 * r2 * r2);
    ax = -g1 * (x + mu) - g2 * (x - 1 + mu) + x;
    ay = -g1 * y - g2 * y + y;
}

double routhCriticalMu() { return (1 - std::sqrt(23.0 / 27.0)) / 2; }
bool trianglePointsStable(double mu) { return 27 * mu * (1 - mu) < 1; }

Libration libration(double mu) {
    Libration L;
    const double disc = 1 - 27 * mu * (1 - mu);
    if (disc < 0) return L;
    L.ok = true;
    L.fast = std::sqrt((1 + std::sqrt(disc)) / 2);
    L.slow = std::sqrt((1 - std::sqrt(disc)) / 2);
    return L;
}

double hillRadius(double mu) { return std::cbrt(mu / 3); }

namespace {
// GM values, km^3/s^2. Sun and planet systems: nbody.cpp kRealGM (JPL astrodynamic parameters, DE440). Planets alone and the satellites: JPL Planetary Satellite Physical
// Parameters (https://ssd.jpl.nasa.gov/sats/phys_par/, read 2026-11-27, saved in the sources repo as data/jpl/sat_phys_par.html; the lines are in data/jpl/sat_phys_par.extract.txt)
struct Moon { const char* planet; const char* name; double gmPlanet, gmMoon; const char* ref; };
const Moon kMoonPairs[] = {
    {"Earth", "Moon", 398600.436, 4902.800, "DE440"},
    {"Jupiter", "Io", 126686531.9, 5959.91547, "JUP365"},
    {"Jupiter", "Europa", 126686531.9, 3202.71210, "JUP365"},
    {"Jupiter", "Ganymede", 126686531.9, 9887.83275, "JUP365"},
    {"Jupiter", "Callisto", 126686531.9, 7179.28340, "JUP365"},
    {"Saturn", "Titan", 37931206.23, 8978.13710, "SAT441"},
    {"Neptune", "Triton", 6835099.97, 1428.49546, "NEP097"},
};
}  // namespace

const std::vector<System>& systems() {
    static const std::vector<System> S = [] {
        std::vector<System> v;
        // kRealGM: the Sun, Mercury ... Neptune (Earth: Earth + Moon). The planet's semi-major axis a0 is JPL's elements (solar_data.h).
        for (int i = 0; i < 8; i++) {
            System s;
            s.heavy = "Sun"; s.light = kPlanets[i].name;
            s.name = "Sun - " + s.light;
            s.gmHeavy = kRealGM[0] / 1e9; s.gmLight = kRealGM[i + 1] / 1e9;    // m^3/s^2 -> km^3/s^2
            s.mu = s.gmLight / (s.gmHeavy + s.gmLight);
            s.sepKm = kPlanets[i].el[0] * units::kAU / 1000.0;
            s.source = i == 2 ? "GM: JPL astrodynamic parameters (DE440), the planet's whole system (Earth + Moon); a: JPL approximate elements" : "GM: JPL astrodynamic parameters (DE440), the planet's whole system; a: JPL approximate elements";
            v.push_back(s);
        }
        for (const Moon& m : kMoonPairs) {
            System s;
            s.heavy = m.planet; s.light = m.name; s.name = s.heavy + " - " + s.light;
            s.gmHeavy = m.gmPlanet; s.gmLight = m.gmMoon;
            s.mu = s.gmLight / (s.gmHeavy + s.gmLight);
            for (std::size_t k = 0; k < kMoonCount; k++) if (s.heavy == kMoons[k].planet && s.light == kMoons[k].name) s.sepKm = kMoons[k].aKm;
            s.source = std::string("GM: JPL planetary satellite physical parameters (") + m.ref + "); a: the moon's mean distance (NASA satellite fact sheets)";
            v.push_back(s);
        }
        return v;
    }();
    return S;
}

Table table(const System& s) {
    const Points P = points(s.mu);
    Table T;
    for (int k = 0; k < 5; k++) {
        T.xKm[k] = P.x[k] * s.sepKm;
        T.yKm[k] = P.y[k] * s.sepKm;
        const double dx = P.x[k] - (1 - s.mu);
        T.inSep[k] = std::hypot(dx, P.y[k]);
        T.fromLightKm[k] = T.inSep[k] * s.sepKm;
    }
    return T;
}

}  // namespace lagrange

namespace kirkwood {

namespace {
const double kRocks[][10] =
#include "asteroid_data.inc"
;
}

double jupiterAU() {
    for (std::size_t i = 0; i < kPlanetCount; i++) if (std::string(kPlanets[i].name) == "Jupiter") return kPlanets[i].el[0];
    return 0;
}

const std::vector<Resonance>& resonances() {
    static const std::vector<Resonance> R = [] {
        std::vector<Resonance> v;
        const int pq[5][2] = {{4, 1}, {3, 1}, {5, 2}, {7, 3}, {2, 1}};
        for (const auto& k : pq) v.push_back({k[0], k[1], jupiterAU() * std::pow(double(k[1]) / k[0], 2.0 / 3.0)});
        return v;
    }();
    return R;
}

Histogram mainBelt(double lo, double hi, int bins) {
    Histogram H;
    H.lo = lo; H.w = (hi - lo) / bins; H.count.assign(std::size_t(bins), 0);
    for (const auto& r : kRocks) {
        if (static_cast<int>(r[0]) != 0) continue;
        H.total++;
        if (r[1] < lo) H.below++;
        else if (r[1] >= hi) H.above++;
        else H.count[std::min<std::size_t>(std::size_t(bins) - 1, std::size_t(std::floor((r[1] - lo) / H.w)))]++;
    }
    return H;
}

}  // namespace kirkwood
}  // namespace orbit
