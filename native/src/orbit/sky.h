// - Sky: the 2,874 brightest stars (HYG v4.1 / Hipparcos) in ecliptic coordinates, the
//   constellation lines (Stellarium), and the plane of our galaxy (the Milky Way label; 0.26: the picture is the Gaia sky map, render/planet_maps.h).
//   The sky is so far away that it does not move when you zoom, only when you turn the 3D view.
// - Far-away things at their real direction and distance: nearest stars, the Pleiades, the
//   Orion Nebula, the galactic centre, and the two Voyagers (which move on with time).
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "camera.h"
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
// 0.26: the inverse of skyScreen: the direction (ecliptic, unit) seen at (X, Y). False beyond 100 degrees.
bool skyDirection(double X, double Y, double W, double H, double ca, double sa, double cb, double sb, Vec3& v);
// 0.26: for the Gaia sky photo: the matrix (row-major) that turns a direction in the view frame of skyScreen
// (x1 right, y2 up, dep depth) into galactic coordinates (rows: towards l = 0, towards l = 90°, the north
// galactic pole), from the sky's galactic centre and pole.
void skyViewToGalactic(const Sky& S, double ca, double sa, double cb, double sb, double M[9]);

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
// 0.75.0: the lens the sky is drawn with. pinhole false: the map above (skyScreen: 2D, the Black Hole Lab); true: the Orbit Lab's 3D camera (orbit/camera.h:
// principal point ppx, ppy, focal F points), the scene's own lens, so a star and a far planet in the same direction stay together while the view turns.
struct SkyLens { bool pinhole = false; double ppx = 0, ppy = 0, F = 1; };
bool skyScreenLens(const Vec3& v, double W, double H, double ca, double sa, double cb, double sb, const SkyLens& L, double& X, double& Y);
bool skyDirectionLens(double X, double Y, double W, double H, double ca, double sa, double cb, double sb, const SkyLens& L, Vec3& v);
void milkyWayBand(const Sky& S, double W, double H, double ca, double sa, double cb, double sb, std::vector<BandPoint>& band, const SkyLens& L = SkyLens{});

Vec3 lonLat(double lonDeg, double latDeg);          // unit vector from ecliptic longitude and latitude
std::array<int, 3> bvRGB(double bv);                // 0.50: a star's colour from its B−V: a black body at the Ballesteros (2012) temperature, CIE 1931, sRGB (sky.cpp)
double bucketBV(int k);                             // 0.84.0: the B−V a colour bucket stands for (-0.3 to 2.0 in 12 steps; Sky::colours)
double bvTemperatureK(double bv);                   // Ballesteros 2012, EPL 97, 34008, eq. (14), K
// 0.50 (review item 5): how bright a star is drawn: ONE rule for every star. Its light is F ∝ 10^(-0.4 m) (Pogson's ratio: 5 magnitudes = a factor 100;
// m = -2.5 log10(f / f0), as IAU 2015 Resolution B2 (arXiv:1510.06262, eq. 4) writes Pogson's scale; data/stars/README.txt). The drawn integrated light (opacity x disc area, in square pixels) is
// kStarFaintLight x (F / F(kStarFaintMag))^kStarGamma: a power-law tone map with exponent 0.5, so the 8 magnitudes of the catalogue (a light ratio of
// about 1,600) become a ratio of about 40 on the screen, and the faintest star stays visible. A disc of radius kStarMinRadius takes the light as opacity;
// above opacity 1 the disc grows instead. Stars Soft scales every star's opacity by the same factor.
constexpr double kStarGamma = 0.5, kStarFaintMag = 6.6, kStarFaintLight = 1.2, kStarMinRadius = 0.7;
struct StarDraw { double radius, alpha; };
StarDraw starDraw(double mag);
// 0.55.4: the dot for a drawn light (opacity x area, square points): a disc of radius kStarMinRadius with that light as opacity; above opacity 1 the disc grows
// instead. starDraw is this with the tone-mapped light above; the Black Hole Lab's true exposure (blackhole/light.h) gives it the star's true light.
StarDraw starDrawLight(double light);
double farBV(const char* key);                      // 0.50: a far-away star's measured B−V (HYG v4.1), NaN if it has none

// The far-away things. Index 0..kFarCount-1: stars and objects; then the two Voyagers.
struct FarItem {
    int index = -1;
    std::string key, name, kind;                    // kind: "star", "object", "bh", "probe"
    Vec3 dir;                                        // unit vector
    double au = 0, ly = 0;                           // distance from the Sun
    Vec3 pos;                                        // m
    const FarData* far = nullptr;
    const ProbeData* probe = nullptr;
    int year = 0;                                    // 0.49: the year of the date it was worked out for (a probe card says "will leave" before its heliopause year)
};
int farCount();                                     // stars and objects + probes
// The far thing number `index` at the simulation's current date. False if it does not
// exist then (the Voyagers are only shown from 1990 on).
bool farItem(const SolarSystem& sys, int index, FarItem& out);
int farIndexOf(const std::string& key);             // -1 if unknown
void farCard(const FarItem& f, Card& c);
// The Journey's Voyager 1 caption for the date shown (0.10.5 stage 5: moved here from ui/journey_dates.cpp, tested).
std::string voyagerCaption(const SolarSystem& sys);

// 1 km/s is this many AU per year
constexpr double kAuPerYearPerKms = 0.21095;
// The Voyagers are only shown from 1 Jan 1990 on.
constexpr int kProbesShownFromYear = 1990;
constexpr double kProbesShownFromMs = 631152000000.0;   // 1 Jan 1990 00:00 UTC
double voyagerKms();                                    // Voyager 1's speed from the Sun, km/s (the lab's own figure)
double voyagerYearsFromSun(double au);                  // years Voyager 1 would need to go `au` AU from the Sun in a straight line at that speed
int probeLaunchYear(const char* key);                   // "v1" / "v2": the launch year in the lab's data (0 if unknown)
double farDistanceLy(const char* key);                  // a far-away thing's distance in light years from the lab's table (0 if unknown)

constexpr double kVoyager1PassesPioneer10Ms = 887673600000.0;   // 0.49: 17 Feb 1998 00:00 UTC, when Voyager 1 passed Pioneer 10 (NASA)
constexpr double kSkyT0Ms = 1790769600000.0;        // 30 Sep 2026 12:00 UTC: the date the Voyager positions were worked out for

}  // namespace orbit
