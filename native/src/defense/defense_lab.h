// Defense Lab (0.17 stage 1): the game around the world: the camera, the chosen building and its rotation, placing and removing, the fixed 1/60 s steps,
// saving (the Save button, autosave every 60 s) and loading. No graphics (defense_view.cpp draws it; ui/shell_defense.cpp has its bar and panel).
#pragma once

#include <cstdint>
#include <string>

#include "defense/defense_world.h"

namespace defense {

constexpr const char* kLabName = "Defense Lab";
constexpr double kZoomMin = 6, kZoomMax = 64;                // points a tile (design)
constexpr double kAutosaveSeconds = 60;                     // design: the game saves itself every 60 s while the lab is open
// the buildings on the build bar, in order (stage 2 adds splitters and junctions, stage 3 turrets)
constexpr Kind kBuildBar[] = {Kind::Belt, Kind::Miner, Kind::Furnace, Kind::Chest, Kind::Generator, Kind::Pole};

struct DefenseLab {
    World world;
    bool loaded = false;                 // a world is in memory (read from the save or new)
    std::string savePath;                // next to the settings file
    std::string loadError;               // not empty: the save could not be read; nothing runs until a new game is chosen
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
    double simMs = 0;                    // CPU time of the last frame's steps (for the report)
    int stepsLastFrame = 0;

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
};

}  // namespace defense
