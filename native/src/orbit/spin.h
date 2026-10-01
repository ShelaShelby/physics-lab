// The real spin of the planets and the Sun, and Earth's seasons. Ported from the web Orbit Lab (6.5–7.5).
// Data: IAU Working Group on Cartographic Coordinates and Rotational Elements, 2015 report
// (Archinal et al.): the direction of each body's north pole (right ascension and declination
// in the J2000 frame, with their drift per century) and the turning angle W of its prime
// meridian, W = W0 + rate × (days since 1 Jan 2000, 12:00 TT).
#pragma once

#include <string>
#include <vector>

#include "vec3.h"

namespace orbit {

struct Body;
class SolarSystem;

struct SpinFrame {
    Vec3 P;      // north pole direction (ecliptic J2000 unit vector)
    Vec3 A;      // where the prime meridian (0° longitude) points on the equator
    Vec3 B;      // 90° east of it
    double W;    // turning angle of the prime meridian, deg
};

bool hasSpin(const std::string& name);                     // Mercury–Neptune, Pluto and the Sun
double spinRateDegPerDay(const std::string& name);         // negative: turns backwards (Venus, Uranus)
bool spinFrame(const std::string& name, double jd, SpinFrame& out);
// Turns per real second at a Time speed; above 1.2 it is too fast to see, so it is held still (unless paused).
double turnsPerSecond(const std::string& name, double tspeed);

// Info-card lines (as the web version writes them)
std::vector<std::string> spinLines(const Body& b, const SolarSystem& sys, double tspeed, bool paused);
std::vector<std::string> sunSpinLines(const SolarSystem& sys, double tspeed, bool paused);
std::vector<std::string> seasonLines(const Body& earth, const SolarSystem& sys);

std::vector<std::string> wrapWords(const std::string& text, std::size_t width);   // the web's wrapTxt

}  // namespace orbit
