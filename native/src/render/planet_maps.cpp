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

const MapAlbedo* mapAlbedo(Map m) {
    // tools/map_albedo_scale.txt (0.77.0): p, factor, drawn p (the ball's albedo with the clip), reached
    static const struct { Map m; MapAlbedo a; } kAlbedo[] = {
        {Map::Mercury, {0.142, 1.9632, 0.1420, true, "Mallama et al. 2017, Table 7, V"}},
        {Map::Earth, {0.434, 1.0, 0.208, false, "Mallama et al. 2017, Table 7, V (the whole planet: clouds and air)"}},   // drawn: 0.208 with its clouds and air (tests/test_orbit.cpp)
        {Map::Mars, {0.170, 1.9239, 0.1700, true, "Mallama et al. 2017, Table 7, V"}},
        {Map::Jupiter, {0.538, 2.9245, 0.5380, true, "Mallama et al. 2017, Table 7, V"}},
        {Map::Saturn, {0.499, 1.3606, 0.4990, true, "Mallama et al. 2017, Table 7, V"}},
        {Map::Uranus, {0.488, 1.2779, 0.4880, true, "Mallama et al. 2017, Table 7, V"}},
        {Map::Neptune, {0.442, 1.2897, 0.4338, false, "Mallama et al. 2017, Table 7, V"}},
        {Map::Moon, {0.12, 0.2488, 0.1200, true, "NSSDC Moon fact sheet"}},
        {Map::Pluto, {0.52, 7.6678, 0.5200, true, "NSSDC Pluto fact sheet"}},
        {Map::Charon, {0.42, 2.7341, 0.4200, true, "NSSDC Pluto fact sheet (Charon)"}},
        {Map::Io, {0.62, 7.5682, 0.6200, true, "NSSDC Jovian satellite fact sheet"}},
        {Map::Europa, {0.68, 3.2629, 0.5827, false, "NSSDC Jovian satellite fact sheet"}},
        {Map::Ganymede, {0.44, 10.4116, 0.4400, true, "NSSDC Jovian satellite fact sheet"}},
        {Map::Callisto, {0.19, 6.0236, 0.1900, true, "NSSDC Jovian satellite fact sheet"}},
        {Map::Rhea, {0.7, 6.5450, 0.5869, false, "NSSDC Saturnian satellite fact sheet"}},
        {Map::Triton, {0.72, 3.3163, 0.5904, false, "NSSDC Neptunian satellite fact sheet"}},
        {Map::Phobos, {0.07, 0.3799, 0.0700, true, "NSSDC Mars fact sheet (Phobos)"}},
    };
    for (const auto& e : kAlbedo)
        if (e.m == m) return &e.a;
    return nullptr;
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
        if (out.w > 0 && m == Map::Earth) {   // 0.77.0: the clouds (assets/maps/earth_clouds.jpg, same grid) in the alpha channel: each byte / 255 is the cloud cover (render/shading.h cloudsOver)
            bool found = false;
            for (const EmbeddedMap* c = kEmbeddedMaps; c->name; c++) {
                if (std::strcmp(c->name, "earth_clouds") != 0) continue;
                int cw = 0, ch = 0, cn = 0;
                unsigned char* cp = stbi_load_from_memory(c->data, static_cast<int>(c->size), &cw, &ch, &cn, 1);
                if (cp && cw == w && ch == h) { for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; i++) out.rgba[4 * i + 3] = cp[i]; found = true; }
                if (cp) stbi_image_free(cp);
            }
            if (!found) for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; i++) out.rgba[4 * i + 3] = 0;   // no clouds rather than a white Earth
        }
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
