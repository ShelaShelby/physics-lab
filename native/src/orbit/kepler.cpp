#include "kepler.h"

#include <algorithm>
#include <cmath>

#include "solar_data.h"
#include "units.h"

namespace orbit {

using units::kPi;

double keplerE(double e, double Mdeg) {
    // mean anomaly wrapped to ±180°, in radians (fmod keeps the sign, like JavaScript's %)
    const double M = (std::fmod(std::fmod(Mdeg, 360.0) + 540.0, 360.0) - 180.0) * kPi / 180.0;
    // first guess: near-parabolic orbits start at ±π, where Newton's method is safe
    double E = e > 0.8 ? (M < 0 ? -kPi : kPi) : M + e * std::sin(M);
    for (int k = 0; k < 100; k++) {                       // Newton's method on f(E) = E − e·sin E − M
        const double dE = (M - (E - e * std::sin(E))) / (1 - e * std::cos(E));
        E += dE;
        if (std::fabs(dE) < 1e-12) break;
    }
    if (e > 0.8 && std::fabs(E - e * std::sin(E) - M) > 1e-7) {   // safety net: bisection
        double lo = -kPi, hi = kPi;
        for (int k = 0; k < 80; k++) {
            E = (lo + hi) / 2;
            if (E - e * std::sin(E) - M > 0) hi = E; else lo = E;
        }
    }
    return E;
}

// Rotation from the orbit plane (x towards perihelion) into the ecliptic:
// R = Rz(Ω) · Rx(I) · Rz(ω).
struct PlaneToEcliptic {
    double co, so, cO, sO, cI, sI;
    PlaneToEcliptic(double Ideg, double nodeDeg, double periDeg) {
        const double rad = kPi / 180;
        co = std::cos(periDeg * rad); so = std::sin(periDeg * rad);
        cO = std::cos(nodeDeg * rad); sO = std::sin(nodeDeg * rad);
        cI = std::cos(Ideg * rad); sI = std::sin(Ideg * rad);
    }
    Vec3 operator()(double X, double Y) const {
        return {(co * cO - so * sO * cI) * X + (-so * cO - co * sO * cI) * Y,
                (co * sO + so * cO * cI) * X + (-so * sO + co * cO * cI) * Y,
                so * sI * X + co * sI * Y};
    }
};

State keplerState(double a, double e, double I, double node, double peri, double Mdeg) {
    const double GM = units::kGMSun;                      // m³/s²
    const double E = keplerE(e, Mdeg);
    const double r = a * (1 - e * std::cos(E));           // distance from the Sun, m
    const double q = std::sqrt(1 - e * e);
    const double kv = std::sqrt(GM * a) / r;              // m/s
    // position and velocity in the orbit plane
    const double xp = a * (std::cos(E) - e), yp = a * q * std::sin(E);
    const double vxp = -kv * std::sin(E), vyp = kv * q * std::cos(E);
    const PlaneToEcliptic rot(I, node, peri);
    return {rot(xp, yp), rot(vxp, vyp)};
}

static double cometMeanAnomalyDeg(const CometData& c, double jd) {
    return std::fmod(std::fmod((jd - c.perihelionJD) / c.periodDays * 360.0, 360.0) + 540.0, 360.0) - 180.0;
}

static double cometA(const CometData& c) { return c.qAU / (1 - c.e) * units::kAU; }   // a = q / (1 − e), m

State cometState(const CometData& c, double jd) {
    return keplerState(cometA(c), c.e, c.inclDeg, c.nodeDeg, c.periDeg, cometMeanAnomalyDeg(c, jd));
}

void cometLocalPath(const CometData& c, double jd, double px, std::vector<Vec3>& pts) {
    const double E0 = keplerE(c.e, cometMeanAnomalyDeg(c, jd));
    const double a = cometA(c), q = std::sqrt(1 - c.e * c.e);
    const PlaneToEcliptic rot(c.inclDeg, c.nodeDeg, c.periDeg);
    auto at = [&](double E) { return rot(a * (std::cos(E) - c.e), a * q * std::sin(E)); };
    // step in E so that next to the comet one step is under a pixel, growing by 12 % each step (at most 0.03 rad)
    const double speed = std::hypot(a * std::sin(E0), a * q * std::cos(E0));   // metres per radian of E
    const double d0 = std::min(0.02, 0.7 / std::max(1e-9, speed * px));
    std::vector<double> ds;
    for (double d = d0; d < kPi; d += std::min(d * 0.12, 0.03)) ds.push_back(d);
    pts.clear();
    for (std::size_t k = ds.size(); k-- > 0;) pts.push_back(at(E0 - ds[k]));
    pts.push_back(at(E0));
    for (double d : ds) pts.push_back(at(E0 + d));
}

}  // namespace orbit
