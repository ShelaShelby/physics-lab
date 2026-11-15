// Black Hole Lab (0.55.0): the picture. The physics is the module's (schwarzschild.h, double precision); this file only hands it to the renderer:
//   - the bent sky: Renderer2D::lensSky with the module's LensTable for the camera's distance (filled again only when the distance changes), drawn by the
//     graphics card per pixel and kept until the view changes (plan section 6, the level "Basic": a table made on the processor, the closed shadow test per pixel);
//   - the catalogue stars as points, every image of each (starImagesFast) at its magnified brightness, by the night sky's one rule (orbit/sky.h starDraw);
//     the images are worked out again only when the view changes. An image brighter than the brightest star of the catalogue seen without the hole (Sirius)
//     is drawn as bright as that star: near the ring of light the magnification grows without limit for a true point, and a drawn dot cannot show it.
//   - the map at the Orbit Lab's full level (its night sky zoomed out, level 1: the map holds the bright stars faintly, the catalogue's dots carry their measured light).
//   - 0.55.1, the gas disc (disc.h, lab.h): its table (DiscTable, filled again only when the distance changes) and a colour table, handed to the same shader
//     pass, which draws the disc in front of the sky; a star image the disc covers (lab.h discPixel at the image's point) is not drawn. The exposure: lab.h discLook.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "blackhole/lab.h"
#include "render/renderer2d.h"

namespace blackhole {

struct ViewStats {
    int stars = 0, images = 0, extraImages = 0, capped = 0;   // stars with an image on the picture; images drawn; of them second or further; drawn at the cap
    double tableMs = 0, starsMs = 0;                           // the last refill of the table and of the stars' images (processor time)
    long tableFills = 0, starFills = 0;
    double shadowPx = 0;                                       // the shadow's radius on the picture, points
    double cx = 0, cy = 0;                                     // the hole's place on the picture, points
    // 0.55.1, the disc: its table's last refill (processor time), the picture's brightest part (lab.h discLook) and the screen's white in use, the stars it hides
    double discMs = 0;
    long discFills = 0;
    DiscLook look;
    int hidden = 0;
};

class BlackHoleView {
    friend void compareHook(render::Renderer2D& R, const char* spec);
public:
    // The picture W x H points with the hole at (cx, cy) points.
    void draw(render::Renderer2D& R, const Settings& s, double W, double H, double cx, double cy);
    const ViewStats& stats() const { return st_; }
    const LensTable* table() const { return table_.get(); }
    // the renderer's description of the bent sky for this view (also used by the tests and the benchmark)
    render::Renderer2D::LensSky lensFor(const Settings& s, const HoleView& v) const;
    void ensureTable(double dist);
    void ensureDisc(double dist);                              // 0.55.1: the disc's table for this distance
    const DiscTable* discTable() const { return disc_.get(); }
    // tests (PL_DRAWSTAT): up to n pixel centres where the disc of s (taken as switched on) covers the picture, and n where it does not, each at least 3 points
    // from an edge of the disc's image (lab.h discPixel), on a grid; points of the picture
    void discSamples(const Settings& s, double W, double H, double cx, double cy, int n, std::vector<double>& on, std::vector<double>& off);
    int discGridOn() const { return discGridOn_; }             // of the last discSamples: how many grid points (of 63 x 39) lie on the disc
    // the colour of a black body against log10 T for the card (blackhole/disc.h blackBodyLinear), made once
    static const std::vector<float>& colourTable(int& n, double& log0, double& dlog);

private:
    std::unique_ptr<DiscTable> disc_;
    double discDist_ = -1;
    int discGridOn_ = 0;
    std::uint64_t discVer_ = 0;
    std::unique_ptr<LensTable> table_;
    double tableDist_ = -1;
    std::uint64_t tableVer_ = 0;
    struct Dot { float x, y, r; render::Color c; };
    std::vector<Dot> dots_;
    std::vector<StarImage> scratch_;
    struct Key { double W = -1, H, cx, cy, dist, fov, yaw, pitch; bool bend, sky; bool disc; double outer, feed, mass, exp; bool expAuto; bool operator==(const Key& o) const; } key_;
    ViewStats st_;
};

// Tests (PL_BH_COMPARE): the card's picture against the double-precision reference (lensPixel) for the distances given, the plain sky against the Orbit
// Lab's sky map, and the frame times; prints PLBHCMP / PLBHTIME lines. Needs the renderer's OpenGL context.
void compareHook(render::Renderer2D& R, const char* spec);

}  // namespace blackhole
