// The picture maps built into the program (0.26): the surfaces of Mercury, Earth, the Moon, Mars, Jupiter and
// Pluto, and the Gaia all-sky map behind the Orbit Lab. The JPEG files are in assets/maps (their sources, credits
// and how they were made are in assets/maps/README.md); cmake/embed_maps.cmake builds them into the .exe.
// All are equirectangular (simple cylindrical): latitude +90 at the top row to -90 at the bottom, longitude growing
// to the right, the map's middle column at lonCentreDeg. The Gaia map is in galactic coordinates and, as ESA
// draws the sky, galactic longitude grows to the LEFT, with l = 0 (the galactic centre) in the middle.
// No graphics here: decoding is plain C++ (stb_image), so the tests can read the maps.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace render {

enum class Map { None = 0, Mercury, Earth, Moon, Mars, Jupiter, Saturn, Uranus, Neptune, Pluto, Io, Europa, Ganymede, Callisto, Rhea, Triton, Charon, Phobos, Sky, Count };   // 0.50: 8 moons

struct MapInfo {
    Map id;
    const char* file;        // assets/maps/<file>.jpg
    double lonCentreDeg;     // the longitude of the map's middle column (planets: east longitude)
    int w, h;                // the expected size, checked when decoded
};
const MapInfo& mapInfo(Map m);

// The map of a body by its name ("Earth", "Moon", ...); Map::None for a body without one (Venus: no true-colour
// map exists; its clouds have no visible markings). Those are drawn in their measured colour (below).
Map mapForBody(const std::string& name);

// 0.26: a planet's colour in sunlight from its measured geometric albedos (Mallama, Krobusek & Pavlov 2017, Icarus 282, 19,
// arXiv:1609.05048, Table 7, all 12 bands), made into sRGB by tools/map_albedo_colour.py (CIE 1931 2°, D65).
// Used for a planet drawn without a map. False for a body not in the table. rgb: 0..255.
bool measuredColour(const std::string& body, int rgb[3]);
// 0.52.1: the colour a body is drawn in when it is smaller than a pixel: its measured colour where it has one (measuredColour, as its ball), else its
// marker colour ("#rrggbb", solar_data's hand-picked colour), so a planet keeps one colour at every zoom
void pointColour(const std::string& body, const char* markerHex, int rgb[3]);

// 0.77.0 (outside review): the measured brightness of each body with a map. p: its geometric albedo, V band (planets: Mallama, Krobusek & Pavlov 2017,
// Icarus 282, 19, arXiv:1609.05048, Table 7; the moons, the Moon and Pluto: the NASA NSSDC fact sheets, data/SOURCES.md). factor: the one number every
// pixel of the map's linear colour is multiplied by, so the app's ball seen fully lit has the albedo p (tools/map_albedo_scale.py, its output in
// tools/map_albedo_scale.txt). A Lambert ball's geometric albedo is 2/3 of its surface's albedo (Madhusudhan & Burrows 2012, ApJ 747, 25, Sec. 3.1);
// the Moon's law (McEwen) is flat at full phase, p = its mean. A pixel pushed above 1 in a channel is divided by its largest channel (render/shading.h
// albedoScaled). reached: false where no factor can reach p (Neptune, Europa, Rhea, Triton: the ball stays darker, drawnP). Earth: factor 1 (its p
// is of the whole planet with clouds and air; the ground map alone is scaled by nothing). Map::None and the sky: nullptr.
struct MapAlbedo { double p; double factor; double drawnP; bool reached; const char* source; };
const MapAlbedo* mapAlbedo(Map m);

struct MapImage { int w = 0, h = 0; std::vector<uint8_t> rgba; };
// Decodes a built-in map to RGBA (4 bytes per pixel). False if it is missing or not the expected size. 0.77.0: Earth's alpha is its cloud cover
// (NASA Earth Observatory, Blue Marble 2002 cloud layer, assets/maps/earth_clouds.jpg: byte / 255); every other map's alpha is 255 and unused.
bool decodeMap(Map m, MapImage& out);

// Where a direction falls on a map, as the shader does it: (u, v) in 0..1, u from the left edge, v from the top.
// Planets: east longitude and latitude in radians. The sky: galactic longitude l and latitude b in radians.
void planetUV(Map m, double lonRad, double latRad, double& u, double& v);
void skyUV(double lRad, double bRad, double& u, double& v);

}  // namespace render
