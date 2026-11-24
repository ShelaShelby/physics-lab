// 0.78.0: comet tails calculated, not drawn for show.
//
// The dust tail (Finson & Probstein 1968, ApJ 154, 327, "A theory of dust comets. I. Model and equations", sect. II: the tail as dust grains on Kepler
// orbits): a grain leaves the nucleus at a past time t0 with the nucleus' own velocity (no ejection speed) and then moves on a Kepler orbit around a Sun
// whose GM is reduced to (1 - beta) GM. beta is the ratio of the sunlight's radiation force to the Sun's gravity on that grain (both fall as 1/r^2).
// A syndyne is all grains of one beta, released at every t0. A synchrone is all grains released at one t0, of every beta. The lab draws a grid of grains:
// kBetas values of beta times one release every kReleaseStepDays for kReleases releases.
//
// The gas (ion) tail points along the solar wind as the comet sees it: w = V r_hat - v (the wind blows straight out from the Sun at speed V, the comet
// moves at v). V = 400 km/s: NASA Marshall Space Flight Center, "The Solar Wind" (https://solarscience.msfc.nasa.gov/SolarWind.shtml, read 2026-11-24:
// "speeds of about 400 km/s"; "high (800 km/s) over coronal holes and low (300 km/s) over streamers"; data/comets/msfc_solarwind_2026-11-24.extract.txt).
//
// What is a choice, not a measurement (the app's text says so): the 4 AU switch-on (kept from 0.48), the range of beta (0 to 1), the release grid
// (every 6 hours, 40 days back), the brightness (every grain the same: the same amount of dust each release and for each beta), the gas tail's length
// (as long as the dust tail). Left out: grain sizes beyond beta, ejection speeds (the tail's width), the ion tail's kinks, jets and outbursts.
//
// The CPU path here, in double, is the reference. The GPU twin is kGrainsCS (render/shaders.h), run by the rocks' program object (render/vk_card.cpp).
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "vec3.h"

namespace orbit {

struct CometData;

namespace tail {

constexpr double kSolarWindKmS = 400;        // NASA MSFC "The Solar Wind" (see above)
constexpr double kSwitchOnAU = 4;            // a drawing choice (0.48): dust is released only while the nucleus is within 4 AU of the Sun
constexpr double kReleaseStepDays = 0.25;    // a drawing choice: one release every 6 hours of simulated time
constexpr int kReleases = 160;               // a drawing choice: 160 releases, so the oldest grains drawn are 40 days old
constexpr int kBetas = 40;                   // a drawing choice: beta = (i + 0.5) / 40, i = 0..39 (0.0125 to 0.9875)
constexpr int kGrains = kReleases * kBetas;
constexpr int kStride = 8;                   // floats a grain for the GPU: r0 (3, AU), v0 (3, AU/day), mu (AU^3/day^2; below 0: no grain), 0
constexpr double kJ2000 = 2451545.0;         // the release times are days since JD 2451545.0 (TDB)

double gmSunAuDay();                          // units::kGMSun in AU^3/day^2
inline double betaOf(int i) { return (i + 0.5) / kBetas; }

// Kepler motion by the universal-variable formulation with Stumpff functions and Lagrange's f and g (the textbook method: H. D. Curtis, "Orbital Mechanics
// for Engineering Students", chapter 3): the state dt after (r0, v0) around a centre of gravitational parameter mu (any units that agree). Ellipses,
// parabolas and hyperbolas alike. mu <= 0: a straight line. Checked by tests/test_orbit.cpp testCometTail078 against conservation of energy and angular
// momentum and against the elliptic Kepler solution (orbit::keplerState).
void propagate(const Vec3& r0, const Vec3& v0, double mu, double dt, Vec3& r, Vec3& v);

// The gas tail's direction (a unit vector): along V r_hat - v. pos, m (Sun at the origin); vel, m/s; windKmS the solar wind's speed.
Vec3 ionTailDirection(const Vec3& pos, const Vec3& vel, double windKmS = kSolarWindKmS);

// One comet's grains: a ring of kReleases releases, kBetas grains each. Release k (time k * kReleaseStepDays after J2000) lives in ring row k mod kReleases.
class CometGrains {
public:
    // brings the ring to the time jdTDB: the releases k = kNow - kReleases + 1 .. kNow, kNow = floor((jdTDB - J2000) / step). Rows whose release changed
    // are worked out again and listed in dirty() (grain ranges: first, count). A release while the nucleus is beyond kSwitchOnAU has no grains (mu < 0).
    void update(const CometData& c, double jdTDB);
    void clearDirty() { dirty_.clear(); }
    const std::vector<std::pair<int, int>>& dirty() const { return dirty_; }
    const std::vector<float>& el() const { return el_; }          // kGrains x kStride floats (the GPU's copy)
    const std::vector<double>& t0() const { return t0_; }         // kGrains release times, days since J2000 (TDB)
    int live() const { return live_; }                            // how many grains exist now
    double now() const { return now_; }                           // days since J2000 of the last update
    // the CPU reference: grain g's place at the last update's time, AU, Sun at the origin (false: no grain there)
    bool position(int g, Vec3& r) const;
    // the place of the grain of the oldest release and the largest beta: the dust tail's reach (false: none)
    bool farthest(Vec3& r) const;
    std::uint64_t serial() const { return serial_; }
    void setSerial(std::uint64_t s) { serial_ = s; }
private:
    struct Row { std::int64_t k = INT64_MIN; bool on = false; Vec3 r0, v0; };
    std::vector<Row> rows_ = std::vector<Row>(kReleases);
    std::vector<float> el_ = std::vector<float>(std::size_t(kGrains) * kStride, 0.f);
    std::vector<double> t0_ = std::vector<double>(std::size_t(kGrains), 0.0);
    std::vector<std::pair<int, int>> dirty_;
    double now_ = 0;
    int live_ = 0;
    std::uint64_t serial_ = 0;
};

}  // namespace tail
}  // namespace orbit
