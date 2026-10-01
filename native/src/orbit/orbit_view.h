// Drawing the Orbit Lab picture (2D, top-down) and handling clicks, drags and zoom.
// Ported from the web Orbit Lab draw(), onClick(), onDrag() and onParam().
#pragma once

#include <map>
#include <string>
#include <vector>

#include "card_text.h"
#include "render/renderer2d.h"
#include "solar_system.h"

struct ImDrawList;

namespace orbit {

// The settings that change the picture (the web version's params).
struct ViewSettings {
    double zoom = -1.2;         // log10 of the zoom factor (×0.06 at -1.2)
    double tspeed = -0.3;       // Time speed slider
    bool trueScale = true;      // Sizes: True scale / Easy to see
    bool fullTrails = false;    // Trails: Short tail / Full orbit trails
    bool moons = true;          // Solar System extras
    bool dwarfs = true;
    bool comets = true;
};

class OrbitView {
public:
    // Draw one frame. The picture is width × height points, placed at (originX, originY)
    // points in the window (for the text, which Dear ImGui draws). `uiScale` turns points
    // into window pixels.
    void draw(render::Renderer2D& r, ImDrawList* text, double originX, double originY, double uiScale,
              const SolarSystem& sys, const ViewSettings& set, double width, double height, double fps);

    void click(double x, double y);              // picture points
    void drag(double dx, double dy);             // moves the view
    void zoomChanged(double oldZoom, double newZoom);   // keeps the middle of the picture in place
    void resetView();                            // the web's "↺ Reset view" (zoom is reset by the caller)
    void select(Selection s, bool follow);       // the Go to tiles: select and keep it in the middle

    Selection sel;
    bool follow = false;
    bool cardHidden = false;
    double ox = 0, oy = 0;                       // how far the view is moved, points

private:
    struct Hit { Selection sel; double x, y, r; };
    std::vector<Hit> hits_;
    std::map<std::pair<int, int>, double> exag_; // moon (body id, index) -> how many times too wide its orbit is drawn
};

}  // namespace orbit
