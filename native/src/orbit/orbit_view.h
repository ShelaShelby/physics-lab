// Drawing the Orbit Lab picture (2D, top-down) and handling clicks, drags and zoom.
#pragma once
#include <array>
#include <vector>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "app/prefs.h"
#include "camera.h"                              // 0.75.0: the camera (ViewCam) and its field of view, kFieldOfViewDeg (45 degrees: one constant, there)
#include "belt_layer.h"
#include "tail_layer.h"
#include "card_text.h"
#include "render/renderer2d.h"
#include "solar_system.h"

struct ImDrawList;

namespace orbit {

enum class SkyMode { Soft, Full, Off };         // Stars in the background
enum class ConsMode { Off, Lines, LinesAndNames };   // Constellations

// The settings that change the picture.
// Real planets: "Exaggerate the Sun's wobble" draws the Sun's path (and the Sun's place on it) this many times too big, round the barycentre.
constexpr double kSunWobbleScale = 50;

struct ViewSettings {
    bool view3D = false;        // View: 2D (top-down) / 3D (rotatable)
    double zoom = -0.8;         // log10 of the zoom factor
    double tspeed = -0.92;      // Time speed slider
    bool trueScale = true;      // Sizes: True scale / Easy to see
    bool fullTrails = false;    // Trails: Short tail / Full orbit trails
    bool moons = true;          // Solar System extras
    bool dwarfs = true;
    bool comets = true;
    bool belts = true;          // Belts and Oort cloud
    bool gpuRocks = true;       // the rocks' places are worked out on the GPU; off, or a GPU without the rock program: the CPU does it (not saved)
    SkyMode sky = SkyMode::Soft;
    ConsMode cons = ConsMode::Off;
    bool realPlanets = false;   // Planet physics: Simple (false) / Real (true), see orbit/nbody.h
    bool exaggerateWobble = true;   // Real only: draw the Sun's path round the barycentre kSunWobbleScale times too big (on by default: the real path is under 2.3 Sun radii)
    bool relativity = true;     // Real only: "General relativity (1PN)" for the planets (0.29: on by default; with the accurate step it brings Mercury from 40,962 to 24 km of JPL; no effect in Simple)
    bool far = true;            // Far-away things (nearest stars, Voyagers, galactic centre)
    bool alphaCen = false;      // 0.73.0: System: Solar System (false) / Alpha Centauri (true): the scene of orbit/alphacen.h (not saved)
    bool orbits = true;         // 0.23 stage 2: Orbit paths: each planet's and dwarf planet's whole orbit, a faint line in its colour (orbit/solar_system.h orbitPath)
    bool paused = false;        // time is stopped (a fast spin is only held still while time runs)
    bool lightTheme = false;    // the theme is a light one (the picture follows the theme when Stars are Off)
    app::ThemeColors theme = app::themeColors("dark", false);   // the theme's colours (⚙ / 🎨)
    float textScale = 1;        // Settings → Text size: the labels drawn in the picture
    // 0.15 stage 3, the new UI (all 0 / false = today's picture, pixel for pixel): the picture fills the window, and the parts that moved into the bars are not drawn here
    bool shell = false;         // no date line, no info card, no scale bar (the chip, the subject card and the readout show them), and the first click is not eaten by a card
    double minTextU = 0;        // the smallest text in the picture, in units before Text size (the Caption size, 13): smaller labels are drawn at this size
    double minTextPx = 0;       // and never under this many pixels (the Caption floor, 12)
    double reserveBottom = 0;   // points kept clear at the bottom of the picture (the toolbar): the far-out note and the scale text sit above it
    double reserveRight = 0;    // points kept clear at the bottom right (the readout): the axes gizmo sits above it
    double insetTop = 0;        // 0.15 stage 5c: points covered at the top (the top bar): ring labels are not drawn there
    double insetRight = 0;      // points covered at the right (the Controls panel): the axes gizmo, the notes and the ring labels stay left of it
    std::vector<std::array<double, 4>> blocked;   // 0.22 stage 6: boxes the bars and floating boxes cover (x, y, w, h in points): no body, belt or ring label is drawn under one
};

// (dateLine, the line at the top left of the picture, is in orbit/card_text.h since 0.10.5 stage 5)

// The Sun's colour in the picture (from its 5,772 K surface), in bytes: the new UI's Legend shows the same colour.
void sunColourBytes(int rgb[3]);

class OrbitView {
public:
    const TailLayer& tailLayer() const { return tailLayer_; }   // 0.78.0: the comets' tails (the benchmark's Path line, tests)
    // Draw one frame. The picture is width × height points, placed at (originX, originY)
    // points in the window (for the text, which Dear ImGui draws). `uiScale` turns points
    // into window pixels.
    void draw(render::Renderer2D& r, ImDrawList* text, double originX, double originY, double uiScale,
              const SolarSystem& sys, const ViewSettings& set, double width, double height);

    // 0.74.0, the camera's rules: it always has a target (the followed thing, else the barycentre). Only a pick of something else (pick: the camera glides there),
    // Reset view, Escape or picking the followed thing again let go. An empty click changes nothing; a moving drag slides the view and stays attached (ux, uy).
    void click(double x, double y);              // picture points: a hit picks it (0.75.0: the camera glides to it, followed or not; a belt is only selected); an empty click does nothing
    void drag(double dx, double dy, bool rotate); // (3D) turns the view, or slides it: a followed thing keeps the new place on the screen (ux, uy)
    void zoomChanged(double oldZoom, double newZoom);   // keeps the middle of the picture in place
    void resetView();
    void tick(double dt, const ViewSettings& set);   // ⟳ Spin, a pick's glide and a released turn's momentum
    void select(Selection s, bool follow);       // a direct set (benchmark, Learn, the Journey's stops): select and keep it in the middle at once; cancels a glide and momentum
    void pick(Selection s);                      // 0.74.0: a user's pick: select and follow it, the camera gliding there over kGlideSeconds (Reduce motion: at once, as select)
    void grab();                                 // 0.74.0: a press on the picture: a released turn's momentum stops
    void release();                              // 0.74.0: the press ends: a turning drag keeps its last speed, fading (not with Reduce motion)
    void letGo() { sel = Selection{}; follow = false; glide_ = glidePending_ = false; }   // Escape: nothing selected; the view stays where it is, on the barycentre
    bool gliding() const { return glide_ || glidePending_; }
    static constexpr double kGlideSeconds = 0.8;  // a pick's glide (eased in and out)
    static constexpr double kMomentumFade = 7.5;  // a released turn's speed falls by e^-7.5 a second (5 % left after 0.4 s); it stops below kMomentumStop
    static constexpr double kMomentumStop = 0.05; // rad/s
    static constexpr double kMomentumMax = 2.5;   // rad/s: a fast flick coasts at most kMomentumMax / kMomentumFade = 0.33 rad
    static constexpr double kPitchMax = 89.5 * 3.14159265358979323846 / 180;   // the Solar System's tilt reaches ±89.5° (Alpha Centauri: ±3.1 rad, turned right over)
    // 0.67.0: a key of everything the picture is drawn from (this view's camera and choices, the settings, the system's time, bodies, trails and moons, the
    // picture's size and place): the same key, the same picture. The app keeps the last picture while the key stays the same (time stopped, nothing moving).
    static constexpr int kKeyParts = 6;                // 0.75.1: the key's groups, each hashed alone (kPictureKeyParts names them)
    static const char* const kPictureKeyParts[kKeyParts];
    std::uint64_t pictureKey(const SolarSystem& sys, const ViewSettings& set, double originX, double originY, double uiScale, double width, double height,
                             std::uint64_t* parts = nullptr) const;

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
    double fx = 0;                               // 0.22: the scene's middle sits this far right of the window's middle, points (negative: left), so it is the middle of the part the Controls panel leaves free (set by the shell)
    double cx = 0, cy = 0;                       // 0.15 stage 3: where a followed thing sits, from the middle of the picture, points (the centre shift keeps it out from under a card)
    double ux = 0, uy = 0;                       // 0.74.0: the user's slide (right drag, Shift-drag, a drag in 2D): where a followed thing sits, added to cx, cy; kept in the picture's free part; Reset view: 0
    bool reduceMotion = false;                   // 0.74.0: Settings → Motion: Reduce (set each frame by the app): no glide and no momentum
    Card lastCard;                               // 0.15 stage 4, the new UI: the selected thing's card text as of the last frame (cardOk: there is one); the subject card and the data sheet use it
    bool cardOk = false;
    Selection cardSel;                           // the selection lastCard is for (the shell uses the card only when it is the current one)
    double scaleLen = 0;                         // 0.22 stage 6: the scale bar's length in points (the readout draws the bar itself)
    std::string scaleText, lightText;            // 0.15 stage 3: the scale bar's words ("1 AU" / "light takes 8.3 minutes"), kept for the readout
    // The selected thing's place on the picture (points) and its radius, as last drawn. False: nothing is selected, or it has no round mark.
    bool selectedScreen(double& x, double& y, double& r) const;
    // 0.75.0: the highest zoom the last drawing allowed (3D, True scale, a followed body: the camera stays 1.5 of its radii from its middle; else kZoomMax)
    double zoomCap() const { return zoomCap_; }
    double zoomCapFor(const SolarSystem& sys, const ViewSettings& set, double W, double H) const;
    // 0.75.0: what the scale bar's length holds for (3D: "at Earth's distance", the followed thing's; empty in 2D)
    std::string scaleWhere;

    // 0.73.1 (for the automatic checks): where the selected moon and its planet were DRAWN last frame, in points from the middle the followed thing is held at (ok: a moon was drawn)
    struct DrawnSel { bool ok = false; double moonX = 0, moonY = 0, planetX = 0, planetY = 0; };
    const DrawnSel& drawnSel() const { return drawnSel_; }

private:
    // 0.74.0: a glide puts the followed thing at the held place minus (1 - e) of where it was on the screen when the glide began (dX0, dY0), and the view's depth from dep0 to the
    // thing's; e = smoothstep of glideS_ (0..1, advanced in tick). It ends exactly (glide_ off at glideS_ >= 1). A select (direct set) or a change of the selection cancels it.
    bool glide_ = false, glidePending_ = false;
    double glideS_ = 0, glideDX_ = 0, glideDY_ = 0, glideDep0_ = 0;
    Selection glideSel_;
    double holdDepth(double dep);                // the view's depth for the followed thing at depth dep (a glide: on the way)
    void holdAt(double qx, double qy, double W, double H, const ViewSettings& set);   // ox, oy so the thing projected at (qx, qy) without them sits at the held place
    double spinYaw_ = 0, spinPitch_ = 0;         // 0.74.0: a released turn's speed, rad/s
    double turnYaw_ = 0, turnPitch_ = 0;         // the turn the drag made since the last tick
    double heldYaw_ = 0, heldPitch_ = 0;         // what the view had after the last tick (a change by someone else stops the momentum)
    bool pressed_ = false, turned_ = false;
    bool held_ = false;                          // the last drawing held a followed thing (a moving drag then slides it: ux, uy)
    DrawnSel drawnSel_;
    double zoomCap_ = 5.5;                       // (units.h kZoomMax)
    struct Hit { Selection sel; double x, y, r; };
    std::vector<Hit> hits_;
    bool acScene = false;                        // 0.73.0: the last frame drew the Alpha Centauri scene (drag may turn the 3D view right over there)
    // 0.73.0: the Alpha Centauri scene (orbit/alphacen_view.inc), drawn instead of the Solar System when ViewSettings::alphaCen is on
    void drawAlphaCen(render::Renderer2D& r, ImDrawList* text, double originX, double originY, double uiScale, const SolarSystem& sys, const ViewSettings& set, double width, double height);
    std::map<std::pair<int, int>, double> exag_; // moon (body id, index) -> how many times too wide its orbit is drawn
    BeltLayer beltLayer_;                        // the belts' rocks: the CPU path (positions kept until the time moves) or the GPU path (orbit/belt_layer.h)
    TailLayer tailLayer_;                        // 0.78.0: the comets' calculated tails (orbit/tail_layer.h): the dust grains on the GPU or the CPU, the gas tail's direction
};

}  // namespace orbit
