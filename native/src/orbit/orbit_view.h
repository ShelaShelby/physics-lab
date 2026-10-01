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

enum class SkyMode { Soft, Full, Off };         // Stars in the background
enum class ConsMode { Off, Lines, LinesAndNames };   // Constellations

// The settings that change the picture (the web version's params).
struct ViewSettings {
    bool view3D = false;        // View: 2D (top-down) / 3D (rotatable)
    double zoom = -1.2;         // log10 of the zoom factor (×0.06 at -1.2)
    double tspeed = -0.3;       // Time speed slider
    bool trueScale = true;      // Sizes: True scale / Easy to see
    bool fullTrails = false;    // Trails: Short tail / Full orbit trails
    bool moons = true;          // Solar System extras
    bool dwarfs = true;
    bool comets = true;
    bool belts = true;          // Belts and Oort cloud
    SkyMode sky = SkyMode::Soft;
    ConsMode cons = ConsMode::Off;
    bool far = true;            // Far-away things (nearest stars, Voyagers, galactic centre)
    bool paused = false;        // time is stopped (a fast spin is only held still while time runs)
};

class OrbitView {
public:
    // Draw one frame. The picture is width × height points, placed at (originX, originY)
    // points in the window (for the text, which Dear ImGui draws). `uiScale` turns points
    // into window pixels.
    void draw(render::Renderer2D& r, ImDrawList* text, double originX, double originY, double uiScale,
              const SolarSystem& sys, const ViewSettings& set, double width, double height, double fps);

    void click(double x, double y);              // picture points
    void drag(double dx, double dy, bool rotate); // moves the view, or (3D) turns it
    void zoomChanged(double oldZoom, double newZoom);   // keeps the middle of the picture in place
    void resetView();                            // the web's "↺ Reset view" (zoom is reset by the caller)
    void tick(double dt, const ViewSettings& set) { if (set.view3D && spin) yaw += 0.15 * dt; }   // ⟳ Spin: the 3D view turns slowly
    void select(Selection s, bool follow);       // the Go to tiles: select and keep it in the middle

    Selection sel;
    bool follow = false;
    bool cardHidden = false;
    double ox = 0, oy = 0;                       // how far the view is moved, points
    double yaw = 0.6, pitch = 1.0;               // 3D view angles, radians
    double cdep = 0;                             // 3D: depth of the middle of the view (the followed object), m
    bool spin = false;                           // 3D: turn slowly by itself
    bool journey = false;                        // a Journey is running: no info card, a caption instead
    std::string caption;                         // the Journey's caption, and how visible it is (0–1)
    double captionAlpha = 0;
    bool panMode = false;                        // 3D: dragging moves the view instead of turning it

private:
    struct Hit { Selection sel; double x, y, r; };
    std::vector<Hit> hits_;
    std::map<std::pair<int, int>, double> exag_; // moon (body id, index) -> how many times too wide its orbit is drawn
    // belt rocks: positions only change with the simulated time, so they are worked out
    // once per time step (not again while paused, or when only the view moves)
    std::vector<std::vector<Vec3>> beltPos_;
    double beltT_ = -1;
    int64_t beltEpoch_ = 0;
    std::vector<float> beltScreen_;              // on-screen rock centres for one belt (reused every frame)
};

}  // namespace orbit
