// The built-in picture maps (see planet_maps.h; sources in assets/maps/README.md).
#include "planet_maps.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

namespace render {

struct EmbeddedMap { const char* name; const unsigned char* data; std::size_t size; };
extern const EmbeddedMap kEmbeddedMaps[];   // embedded_maps.cpp, made by cmake/embed_maps.cmake

namespace {
constexpr double kPi = 3.14159265358979323846;
// lonCentreDeg: the USGS files' own metadata (Mercury, Mars: central meridian 0; Pluto: 180), the NASA SVS Moon
// Kit page ("centered on 0° longitude"), Blue Marble (-180 to 180). Jupiter's Cassini map (PIA07782) states no
// longitude, and its clouds drift against any fixed rotation, so its middle is taken as 0 (said in the app).
// Saturn, Uranus and Neptune (0.26): true colour from the Hubble OPAL FITS maps (tools/map_opal.py), turned to 0..360° E left
// to right, so 180° E is in the middle (Saturn: System III; Uranus: mirrored from its right-to-left layout).
const MapInfo kInfo[] = {
    {Map::None, "", 0, 0, 0},
    {Map::Mercury, "mercury", 0, 2048, 1024},
    {Map::Earth, "earth", 0, 2048, 1024},
    {Map::Moon, "moon", 0, 2048, 1024},
    {Map::Mars, "mars", 0, 2048, 1024},
    {Map::Jupiter, "jupiter", 0, 2048, 1024},
    {Map::Saturn, "saturn", 180, 2048, 1024},
    {Map::Uranus, "uranus", 180, 2048, 1024},
    {Map::Neptune, "neptune", 180, 2048, 1024},
    {Map::Pluto, "pluto", 180, 2048, 1024},
    // 0.50: the moons, 1024 x 512, black and white (tools/map_moon.py; assets/maps/README.md). Rhea's mosaic starts at 0 degrees, so its middle is 180 E.
    {Map::Io, "io", 0, 1024, 512},
    {Map::Europa, "europa", 0, 1024, 512},
    {Map::Ganymede, "ganymede", 0, 1024, 512},
    {Map::Callisto, "callisto", 0, 1024, 512},
    {Map::Rhea, "rhea", 180, 1024, 512},
    {Map::Triton, "triton", 0, 1024, 512},
    {Map::Charon, "charon", 0, 1024, 512},
    {Map::Phobos, "phobos", 0, 1024, 512},
    {Map::Sky, "gaia_sky", 0, 4000, 2000},
};
}  // namespace

const MapInfo& mapInfo(Map m) {
    const int i = static_cast<int>(m);
    return kInfo[(i >= 0 && i < static_cast<int>(Map::Count)) ? i : 0];
}

Map mapForBody(const std::string& name) {
    for (int i = 1; i < static_cast<int>(Map::Sky); i++) {
        const MapInfo& mi = kInfo[i];
        if (name.size() == std::strlen(mi.file) && std::equal(name.begin(), name.end(), mi.file, [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }))
            return mi.id;
    }
    return Map::None;
}

bool measuredColour(const std::string& body, int rgb[3]) {
    // tools/map_albedo_colour.py: the albedo spectrum of Table 7, all 12 bands (Johnson U B V R I, Cousins Rc Ic, Sloan u' g' r' i' z'),
    // linear between their effective wavelengths and flat outside them -> CIE 1931 XYZ under D65 -> sRGB. The Sun's light is taken as
    // white, as a screen's white. (Venus is almost exactly white: CIE x, y 0.318, 0.336 against D65's 0.313, 0.329.)
    static const struct { const char* name; int r, g, b; } kColour[] = {
        {"Mercury", 114, 106, 95}, {"Venus", 218, 216, 211}, {"Earth", 165, 177, 190}, {"Mars", 140, 114, 85},
        {"Jupiter", 197, 194, 182}, {"Saturn", 209, 188, 164}, {"Uranus", 149, 186, 198}, {"Neptune", 138, 178, 197},
    };
    for (const auto& c : kColour)
        if (body == c.name) { rgb[0] = c.r; rgb[1] = c.g; rgb[2] = c.b; return true; }
    return false;
}

void pointColour(const std::string& body, const char* markerHex, int rgb[3]) {
    if (measuredColour(body, rgb)) return;
    unsigned v = 0;
    const char* h = markerHex && markerHex[0] == '#' ? markerHex + 1 : "a4a4a4";
    for (int i = 0; i < 6 && h[i]; i++) v = v * 16 + unsigned(std::isdigit(static_cast<unsigned char>(h[i])) ? h[i] - '0' : std::tolower(static_cast<unsigned char>(h[i])) - 'a' + 10);
    rgb[0] = int(v >> 16 & 255); rgb[1] = int(v >> 8 & 255); rgb[2] = int(v & 255);
}

bool decodeMap(Map m, MapImage& out) {
    out = MapImage{};
    const MapInfo& mi = mapInfo(m);
    if (mi.id == Map::None) return false;
    for (const EmbeddedMap* e = kEmbeddedMaps; e->name; e++) {
        if (std::strcmp(e->name, mi.file) != 0) continue;
        int w = 0, h = 0, n = 0;
        unsigned char* px = stbi_load_from_memory(e->data, static_cast<int>(e->size), &w, &h, &n, 4);
        if (!px) return false;
        if (w == mi.w && h == mi.h) { out.w = w; out.h = h; out.rgba.assign(px, px + static_cast<std::size_t>(w) * h * 4); }
        stbi_image_free(px);
        return out.w > 0;
    }
    return false;
}

void planetUV(Map m, double lon, double lat, double& u, double& v) {
    const double c = mapInfo(m).lonCentreDeg * kPi / 180;
    u = (lon - c) / (2 * kPi) + 0.5;
    u -= std::floor(u);
    v = 0.5 - lat / kPi;
}

void skyUV(double l, double b, double& u, double& v) {
    u = 0.5 - l / (2 * kPi);
    u -= std::floor(u);
    v = 0.5 - b / kPi;
}

}  // namespace render
