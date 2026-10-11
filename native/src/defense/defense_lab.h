// Defense Lab (0.17 stage 1, stage 2): the game around the world: the camera, the chosen building and its rotation, placing and removing, the fixed 1/60 s steps,
// saving (the Save button, autosave every 60 s) and loading. No graphics (defense_view.cpp draws it; ui/shell_defense.cpp has its bar and panel).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "defense/defense_world.h"

namespace defense {

constexpr const char* kLabName = "Defense Lab";
constexpr double kZoomMin = 6, kZoomMax = 64;                // points a tile (design)
constexpr double kAutosaveSeconds = 60;                     // design: the game saves itself every 60 s while the lab is open
// the buildings on the build bar, in order (stage 2 adds splitters and junctions, stage 3 turrets)
constexpr Kind kBuildBar[] = {Kind::Belt, Kind::Miner, Kind::Furnace, Kind::Chest, Kind::Generator, Kind::Pole};
constexpr double kPanKeySpeed = 900;                         // design: WASD / arrow keys move the map 900 points a second
constexpr double kFeedRepeat = 0.25;                         // design: holding the click on a generator feeds it again every 0.25 s
constexpr double kLodZoom = 14;                              // design: below 14 points a tile the picture skips small details and glow (level of detail)

// ---- pure functions (no state; unit-tested) ----
// The keys 1-8: 1-6 pick the build bar's buildings, 7 turns, 8 jumps home to the core.
enum class HotkeyAction { None, Pick, Turn, Home };
struct Hotkey { HotkeyAction action = HotkeyAction::None; Kind kind = Kind::None; };
Hotkey hotkeyFor(int digit);
// A line of belts dragged from tile (x0, y0) to (x1, y1): along the longer axis first, then a corner and along the other. Each tile's dir points to the next
// tile (the last keeps the line's last direction), so the tile where the line turns is fed from its side and becomes a corner by itself.
// A drag of one tile gets dirSingle. Writes into out (cleared first; no allocation once it has grown).
struct BeltStep { int x = 0, y = 0, dir = 0; };
void beltLine(int x0, int y0, int x1, int y1, int dirSingle, std::vector<BeltStep>& out);
// The camera: kept so the view never shows more than map + border band; the lowest zoom is limited so the band always fills the window (centred if it cannot).
double minZoomFor(double viewW, double viewH);
void clampCamera(double& camX, double& camY, double& zoom, double viewW, double viewH);
// The arrow to the core when it is off screen: shown only when no part of the core's 3 x 3 tiles is on screen. (x, y): where it sits, `margin` points inside
// the screen edge; angle: radians on screen (0 = right, pi/2 = down); dist: tiles from the screen's middle to the core's middle.
struct CoreArrow { bool show = false; double x = 0, y = 0, angle = 0, dist = 0; };
CoreArrow coreArrow(double camX, double camY, double zoom, double viewW, double viewH, double coreCX, double coreCY, double margin);

struct DefenseLab {
    World world;
    bool loaded = false;                 // a world is in memory (read from the save or new)
    std::string savePath;                // next to the settings file
    std::string loadError;               // not empty: the save could not be read; nothing runs until a new game is chosen
    bool oldSave = false;                // the save is from an older version (stage 2: kept as .old, not .damaged)
    std::string note;                    // a short message (saved, cannot build there ...)
    double noteUntil = 0;
    Kind tool = Kind::None;              // the building being placed (None: just look)
    int dir = 1;                         // its rotation (R)
    bool overlay = false;                // the pollution overlay
    bool hoverOn = false;
    double hoverX = 0, hoverY = 0;       // the pointer, in tiles (continuous)
    char seedText[24] = "";              // the seed field for a new game
    double clock = 0;                    // seconds the lab has been open (for notes and the belt animation)
    double sinceSave = 0;
    double acc = 0;                      // time not yet stepped
    double viewW = 1, viewH = 1;         // the picture, points
    double freeW = 0, barH = 0;          // the picture's width not under the Controls panel, and the build bar's room at the bottom (points; set by the shell)
    double simMs = 0;                    // CPU time of the last frame's steps (for the report)
    int stepsLastFrame = 0;
    // stage 2 handling: a left drag with a building lays a line (belts) or places along the way; a right drag removes along the way
    bool dragBuild = false, dragRemove = false;
    int dragX0 = 0, dragY0 = 0, dragX1 = 0, dragY1 = 0;  // tiles (dragBuild: start and current)
    int lastRemoveX = 0, lastRemoveY = 0;
    bool removedAny = false;
    std::vector<BeltStep> line;          // the belt line being dragged (reused)
    bool feedHeld = false;               // the left button is held on a coal generator
    int feedX = 0, feedY = 0;
    double feedNext = 0;

    // opens the saved game (or makes a new one with freshSeed if there is no file); a damaged file is kept as <file>.damaged and loadError says why
    void open(const std::string& path, uint32_t freshSeed);
    void startNew(uint32_t seed);
    bool save();
    void update(double dt, bool paused); // steps, autosave
    void say(const std::string& s, double seconds = 3);

    // the camera
    void pan(double dxPt, double dyPt);
    void zoomAt(double factor, double xPt, double yPt);
    double tileX(double xPt) const { return world.camX + (xPt - viewW / 2) / world.zoom; }
    double tileY(double yPt) const { return world.camY + (yPt - viewH / 2) / world.zoom; }
    void anchor(Kind k, double tx, double ty, int& x, int& y) const;   // the top left tile of a building of kind k under the pointer

    // the mouse (points in the picture)
    void hover(double xPt, double yPt, bool on);
    void clickPlace(double xPt, double yPt);
    void clickRemove(double xPt, double yPt);
    void rotate() { dir = (dir + 1) & 3; }
    void pick(Kind k) { tool = tool == k ? Kind::None : k; }
    void home();                          // H / the Home button: the camera jumps to the core
    void keyPan(double dx, double dy, double dt);   // WASD / arrows held: dx, dy in -1..1
    void pickUnder();                     // Q: the building under the pointer becomes the tool (with its rotation)
    bool hotkey(int digit);               // 1-8; false if it does nothing
    // stage 2 mouse: press, move, release (points); left or right button. A press on a generator feeds it coal from the core (held: again every kFeedRepeat s).
    void press(double xPt, double yPt, bool left);
    void drag(double xPt, double yPt);
    void release(double xPt, double yPt, bool left, bool moved);
    void clampView() { clampCamera(world.camX, world.camY, world.zoom, viewW, viewH); }
};

}  // namespace defense
