// The night sky and the far-away things, ported from the web Orbit Lab (version 6).
// - Sky: the 2,874 brightest stars (HYG v4.1 / Hipparcos) in ecliptic coordinates, the
//   constellation lines (Stellarium), and the plane of our galaxy for the Milky Way band.
//   The sky is so far away that it does not move when you zoom, only when you turn the 3D view.
// - Far-away things at their real direction and distance: nearest stars, the Pleiades, the
//   Orion Nebula, the galactic centre, and the two Voyagers (which move on with time).
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "vec3.h"

namespace orbit {

class SolarSystem;
struct Card;

struct FarData { const char* key; const char* name; const char* kind; double lon, lat, ly; const char* sp; const char* con; const char* note; };
struct ProbeData { const char* key; const char* name; double lon, lat, au, kms; int year; double cross; int crossYear; };

constexpr int kSkyColourBuckets = 12;              // star colours are grouped in 12 steps of B−V

struct SkyStar { Vec3 dir; float mag; int bucket; };
struct Sky {
    std::vector<SkyStar> stars;                     // unit vectors in ecliptic coordinates
    std::vector<std::vector<std::pair<int, int>>> lines;   // constellation lines, as star index pairs
    std::vector<std::pair<std::string, Vec3>> cons; // constellation names and where to write them
    std::vector<std::pair<int, std::string>> names; // names of bright stars
    Vec3 galCentre, galPole;                        // directions of the galactic centre and the north galactic pole
    std::array<std::array<int, 3>, kSkyColourBuckets> colours;   // RGB 0–255 of each colour bucket
};
const Sky& skyData();

// The night sky's map: the angle from straight ahead becomes the distance from the middle of the picture, so 100 degrees (1.75 rad) reaches the corner. A direction (in
// ecliptic coordinates) -> a point on a W x H picture, for the view given by the cosine and sine of the yaw (ca, sa) and of the pitch (cb, sb). False when the direction
// is more than 100 degrees from straight ahead (off the map).
bool skyScreen(const Vec3& v, double W, double H, double ca, double sa, double cb, double sb, double& X, double& Y);

// The Milky Way: one band along the plane of our galaxy (a great circle through the galactic centre, brightest towards it), sampled at kBandPieces + 1 points
// (the last is the first again). A point is `ok` (drawn) when it is on the map and has a neighbour on the map to take the direction of the band from.
constexpr int kBandPieces = 240;
struct BandPoint {
    double X = 0, Y = 0;        // on the picture
    double tx = 0, ty = 0;      // the band's unit normal there, on the picture
    double fade = 0;            // 1 inside the map, falling smoothly to 0 over the last 0.2 rad before the map's edge
    double bright = 0;          // 0.4 (away from the galactic centre) to 1 (towards it)
    bool ok = false;
};
void milkyWayBand(const Sky& S, double W, double H, double ca, double sa, double cb, double sb, std::vector<BandPoint>& band);

Vec3 lonLat(double lonDeg, double latDeg);          // unit vector from ecliptic longitude and latitude
std::array<int, 3> bvRGB(double bv);                // a star's colour from its B−V index, a little paler, as the eye sees it
double spBV(const char* spectralType);              // rough B−V from the spectral type letter

// The far-away things. Index 0..kFarCount-1: stars and objects; then the two Voyagers.
struct FarItem {
    int index = -1;
    std::string key, name, kind;                    // kind: "star", "object", "bh", "probe"
    Vec3 dir;                                        // unit vector
    double au = 0, ly = 0;                           // distance from the Sun
    Vec3 pos;                                        // m
    const FarData* far = nullptr;
    const ProbeData* probe = nullptr;
};
int farCount();                                     // stars and objects + probes
// The far thing number `index` at the simulation's current date. False if it does not
// exist then (the Voyagers are only shown from 1990 on).
bool farItem(const SolarSystem& sys, int index, FarItem& out);
int farIndexOf(const std::string& key);             // -1 if unknown
void farCard(const FarItem& f, Card& c);

constexpr double kSkyT0Ms = 1790769600000.0;        // 30 Sep 2026 12:00 UTC: the date the Voyager positions were worked out for

}  // namespace orbit
