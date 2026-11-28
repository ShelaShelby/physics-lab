// Belts: the asteroid belt, Jupiter's Trojans, the Kuiper belt and the Oort cloud.
// 0.27: the asteroid belt, the Trojans and the Kuiper belt are 3,000 real asteroids (JPL Small-Body Database, asteroid_data.inc). The Oort cloud
// (never seen) is 3,000 dots, not objects. Since 0.85.0 their distances are a sample of a published model of the cloud (Vokrouhlicky, Nesvorny & Dones 2019:
// oortModelInverse below) and their directions are spread evenly. A fixed random seed keeps them in the same places. Until 0.84.1 the distances were made up.
//
// Each rock (except in the Oort cloud) moves on its own fixed Kepler ellipse round the Sun.
// Float32Array, so both versions place every rock in the same spot.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "camera.h"
#include "render/rock_compute.h"
#include "vec3.h"

namespace orbit {

struct BeltGroup {
    std::string key;            // "main", "trojan", "kuiper", "oort"
    const char* color;
    float alpha = 0.7f;         // how see-through the dots are
    bool isStatic = false;      // the Oort cloud does not move (its orbits take millions of years)
    int n = 0;
    std::uint64_t serial = 0;   // which set of rocks this is: makeBelts gives every group of every call a new number (a copy keeps it). The GPU keeps the rocks it was sent under this number (render/rock_compute.h)
    // moving rocks: Kepler elements (AU, radians); P and Q are the orbit's in-plane unit vectors in the ecliptic
    std::vector<float> aN, a, e, M0, libA, libPh, Px, Py, Pz, Qx, Qy, Qz;
    // still rocks (Oort cloud): positions, m
    std::vector<float> x, y, z;
};

// Numbers in [0, 1).
struct Mulberry32 {
    uint32_t seed;
    explicit Mulberry32(uint32_t s) : seed(s) {}
    double next();
};

// All belts for a start date (Unix ms). 0.75.2: every rock of the three real belts is a real asteroid on the fixed ellipse of its own JPL record (belts.cpp); Jupiter's place plays no part, and
// there is no Trojan sway (the libA / libPh arrays are zero: plain fixed ellipses, the card and Learn say Jupiter's pull on them is not modelled).
// scale 1 is the normal number of rocks (always, in the app); the benchmark's stress scene asks for more of the same kinds.
std::vector<BeltGroup> makeBelts(int64_t unixMs, int scale = 1);

// Kepler's equation  M = E - e sin E  for an ellipse (0 <= e < 1): the eccentric anomaly E in [-pi, pi] for any mean anomaly M (radians; it is brought into -pi..pi first, in double).
// Newton's method, started at E0 = M + e sin M (at +-pi for e > 0.8, where that start can run away), at least 3 steps, stopped when a step moves E by less than 1e-9 (the next error is about
// that squared) and at most 60 steps; a bisection safety net if the result still misses by more than 1e-9. 0.13 stage 4 part A: up to 0.13 stage 3 the belts used exactly 3 steps from M + e sin M,
// which is exact for every rock the app makes (e <= 0.498) but not for e >= 0.8 (e = 0.99: 0.45 rad off). For e < 0.8 the first three steps are the same, so a rock of the app is where it was.
double solveKepler(double e, double M);
// The mean motion of rock k, rad/s: Kepler's third law with the rock's own speed distance aN (from its record's mean motion, Trojans too), n = sqrt(GM / aN^3). One function for the CPU and for the GPU upload.
double beltMeanMotion(const BeltGroup& g, int k);

// Where rock k of group g is, `t` seconds after the start date (m, Sun at the origin).
Vec3 beltPoint(const BeltGroup& g, int k, double t);

// The camera of the Orbit Lab picture, as OrbitView::draw has it (0.75.0: the one camera of orbit/camera.h: 2D the top-down map, 3D the pinhole camera).
using BeltCamera = ViewCam;
// A point (m) on the picture (points); behind the camera or nearer than its near plane (3D): -1e9, -1e9 (off the picture). The CPU reference of the GPU's
// screenOf (render/shaders.h, kRocksCS).
void beltScreen(const BeltCamera& c, const Vec3& p, double& X, double& Y);
// the camera's numbers for the card's rocks program (kRocksCS uniforms), as orbit/belt_layer.cpp sends them
void rockFrameCamera(const BeltCamera& c, render::RockFrame& f);

// The Oort cloud's edges as the lab states them, AU (the card, the Journey caption and the travel times all read these).
// 0.49 (outside review: 2,000 / 20,000 / 200,000 AU had no source): NASA Science, Oort Cloud Facts (data/oort/nasa_oort_cloud_facts_read_2026-11-11.html): "The inner
// edge of the Oort Cloud, however, is thought to be located between 2,000 and 5,000 AU from the Sun, with the outer edge being located somewhere between 10,000 and
// 100,000 AU from the Sun." kOortInnerAU and kOortOuterAU are the nearest and farthest of those (the dots and the travel times use them).
constexpr double kOortInnerAU = 2000, kOortInnerEdgeFarAU = 5000, kOortOuterEdgeNearAU = 10000, kOortOuterAU = 100000;
// The Journey's Oort cloud caption; the travel time is worked out from Voyager 1's speed (orbit/sky.h), so it always agrees with the distances in it.
std::string oortCaption();

// 0.85.0 (PLAN_REVIEW.md R4): the published model the Oort cloud's dots are a sample of. D. Vokrouhlicky, D. Nesvorny, L. Dones, "Origin and evolution of
// long-period comets", AJ 157, 181 (2019), arXiv:1904.00728 (saved: data/oort/arxiv/1904.00728_vokrouhlicky_2019.pdf; extract
// native/data/oort/arxiv/1904.00728_vokrouhlicky_2019.extract.txt), section 4.1 and Figure 8: the number of bodies per distance, n(r) = dn/dr, "may be in parts
// approximated with n(r) proportional to r^-alpha. In the inner cloud, we find alpha = 0.72, while in the outer cloud, we find alpha = 1.35, steepening to a
// thermalized value of 1.5 at the very outer edge of the cloud (beyond 50000 au)". Below r = 2000 au "the population abruptly drops". The inner and outer parts
// are divided at 15,000 au ("15000 au approximately represents the division between the non-isotropic and isotropic portions"). The app joins the three power
// laws so that n(r) is continuous at 15,000 and 50,000 au (its choice: the paper's fits are "in parts"), from kOortInnerAU (2,000) to kOortOuterAU (100,000 au).
// The shape: "In the outer part of the Oort cloud, beyond semimajor axes 15000 au, the inclination distribution becomes nearly isotropic in space." The inner
// part (1,500 to 15,000 au) is not isotropic: its orbits gather near 35 degrees to the ecliptic, with a weaker gathering near 150 degrees (the paper's Figure 7,
// a plot with no table). The app spreads every dot's direction evenly: the inner part's lean is not drawn (the card says so; ROADMAP: source needed).
constexpr double kOortModelBreak1AU = 15000, kOortModelBreak2AU = 50000;
constexpr double kOortModelAlpha[3] = {0.72, 1.35, 1.5};
// the model's share of the bodies between r1 and r2 (au, clamped to the edges)
double oortModelShare(double r1AU, double r2AU);
// the distance (au) below which a share u (0..1) of the model's bodies lie: the dots' distances (u uniform)
double oortModelInverse(double u);

const char* beltTitle(const std::string& key);
std::vector<std::string> beltLines(const std::string& key);

}  // namespace orbit
