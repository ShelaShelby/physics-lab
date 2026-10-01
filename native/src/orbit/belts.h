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
std::vector<BeltGroup> makeBelts(int64_t unixMs);

// Where rock k of group g is, `t` seconds after the start date (m, Sun at the origin).
Vec3 beltPoint(const BeltGroup& g, int k, double t);

// The facts card of a belt (title + lines), as in the web version.
const char* beltTitle(const std::string& key);
std::vector<std::string> beltLines(const std::string& key);

}  // namespace orbit
