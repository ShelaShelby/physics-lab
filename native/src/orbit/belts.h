// Belts: the asteroid belt, Jupiter's Trojans, the Kuiper belt and the Oort cloud.
// A few thousand stand-in rocks, made with a fixed random seed so they always look the same.
// Ported from the web Orbit Lab (rng, makeBelts, beltPoint, BELTCARDS).
//
// Each rock (except in the Oort cloud) moves on its own fixed Kepler ellipse round the Sun.
// The orbital elements are stored as 32-bit floats, exactly like the web version's
// Float32Array, so both versions place every rock in the same spot.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

// Mulberry32: the same small random number generator as the web version. Numbers in [0, 1).
struct Mulberry32 {
    uint32_t seed;
    explicit Mulberry32(uint32_t s) : seed(s) {}
    double next();
};

// All belts for a start date (Unix ms). Jupiter's place on that date sets where the Trojans sit.
// scale 1 is the normal number of rocks (always, in the app); the benchmark's stress scene asks for more of the same kinds.
std::vector<BeltGroup> makeBelts(int64_t unixMs, int scale = 1);

// Kepler's equation  M = E - e sin E  for an ellipse (0 <= e < 1): the eccentric anomaly E in [-pi, pi] for any mean anomaly M (radians; it is brought into -pi..pi first, in double).
// Newton's method, started at E0 = M + e sin M (at +-pi for e > 0.8, where that start can run away), at least 3 steps, stopped when a step moves E by less than 1e-9 (the next error is about
// that squared) and at most 60 steps; a bisection safety net if the result still misses by more than 1e-9. 0.13 stage 4 part A: up to 0.13 stage 3 the belts used exactly 3 steps from M + e sin M,
// which is exact for every rock the app makes (e <= 0.498) but not for e >= 0.8 (e = 0.99: 0.45 rad off). For e < 0.8 the first three steps are the same, so a rock of the app is where it was.
double solveKepler(double e, double M);
// The mean motion of rock k, rad/s: Kepler's third law with the rock's own speed distance aN (the Trojans use Jupiter's), n = sqrt(GM / aN^3). One function for the CPU and for the GPU upload.
double beltMeanMotion(const BeltGroup& g, int k);

// Where rock k of group g is, `t` seconds after the start date (m, Sun at the origin).
Vec3 beltPoint(const BeltGroup& g, int k, double t);

// The camera of the Orbit Lab picture, as OrbitView::draw has it (the web's base() and proj()): everything is drawn round the barycentre bc, turned by the yaw (ca, sa) about z and tipped by the pitch
// (cb, sb), scaled by px (points per metre), and in the 3D view given a mild perspective (D: the camera distance, cd: the depth of the middle of the view); then moved by (ox, oy) points.
struct BeltCamera {
    double bx = 0, by = 0, bz = 0;
    double ca = 1, sa = 0, cb = 1, sb = 0, px = 1, W = 1, H = 1, D = 1, cd = 0, ox = 0, oy = 0;
    bool d3 = false;
};
// A point (m) on the picture (points). The CPU reference of the GPU's screenOf (render/shaders.h, kRocksCS).
void beltScreen(const BeltCamera& c, const Vec3& p, double& X, double& Y);

// The Oort cloud's edges as the lab states them, AU (the card, the Journey caption and the travel times all read these).
constexpr double kOortInnerAU = 2000, kOortInnerEndAU = 20000, kOortOuterAU = 200000;
// The Journey's Oort cloud caption; the travel time is worked out from Voyager 1's speed (orbit/sky.h), so it always agrees with the distances in it.
std::string oortCaption();

// The facts card of a belt (title + lines), as in the web version.
const char* beltTitle(const std::string& key);
std::vector<std::string> beltLines(const std::string& key);

}  // namespace orbit
