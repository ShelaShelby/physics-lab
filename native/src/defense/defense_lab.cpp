#include "defense/defense_lab.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace defense {

void DefenseLab::open(const std::string& path, uint32_t freshSeed) {
    savePath = path;
    std::string text;
    if (path.empty() || !readFile(path, text)) { startNew(freshSeed); return; }
    std::string err;
    if (world.load(text, &err)) { loaded = true; loadError.clear(); world.zoom = std::clamp(world.zoom, kZoomMin, kZoomMax); return; }
    writeFileAtomic(path + ".damaged", text);                      // kept, so nothing is lost if the owner wants to look at it
    loaded = false;
    loadError = "The saved planet could not be read: " + err + ". A copy was kept as " + path + ".damaged.";
}

void DefenseLab::startNew(uint32_t seed) {
    world.newGame(seed);
    loaded = true;
    loadError.clear();
    acc = 0; sinceSave = 0;
    tool = Kind::None;
    if (!savePath.empty()) save();
}

bool DefenseLab::save() {
    if (!loaded || savePath.empty()) return false;
    sinceSave = 0;
    const bool ok = writeFileAtomic(savePath, world.serialize());
    say(ok ? "Saved." : "The game could not be saved.");
    return ok;
}

void DefenseLab::say(const std::string& s, double seconds) { note = s; noteUntil = clock + seconds; }

void DefenseLab::update(double dt, bool paused) {
    clock += dt;
    stepsLastFrame = 0; simMs = 0;
    if (!loaded) return;
    sinceSave += dt;
    if (sinceSave >= kAutosaveSeconds) save();
    if (paused) { acc = 0; return; }
    const auto t0 = std::chrono::steady_clock::now();
    acc += dt;
    const double step = 1.0 / kTicksPerSecond;
    while (acc >= step && stepsLastFrame < 6) { world.step(); acc -= step; stepsLastFrame++; }   // (at most 6 a frame: a slow frame does not snowball)
    if (stepsLastFrame == 6) acc = 0;
    simMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void DefenseLab::pan(double dxPt, double dyPt) { world.camX -= dxPt / world.zoom; world.camY -= dyPt / world.zoom; }

void DefenseLab::zoomAt(double factor, double xPt, double yPt) {
    const double tx = tileX(xPt), ty = tileY(yPt);
    world.zoom = std::clamp(world.zoom * factor, kZoomMin, kZoomMax);
    world.camX = tx - (xPt - viewW / 2) / world.zoom;               // the tile under the pointer stays under it
    world.camY = ty - (yPt - viewH / 2) / world.zoom;
}

void DefenseLab::anchor(Kind k, double tx, double ty, int& x, int& y) const {
    const double s = footprint(k);
    x = static_cast<int>(std::floor(tx - s / 2 + 0.5));
    y = static_cast<int>(std::floor(ty - s / 2 + 0.5));
}

void DefenseLab::hover(double xPt, double yPt, bool on) { hoverOn = on; hoverX = tileX(xPt); hoverY = tileY(yPt); }

void DefenseLab::clickPlace(double xPt, double yPt) {
    if (!loaded || tool == Kind::None) return;
    int x, y;
    anchor(tool, tileX(xPt), tileY(yPt), x, y);
    std::string why;
    if (!world.canPlace(tool, x, y, &why)) { say(why); return; }
    world.place(tool, x, y, dir);
}

void DefenseLab::clickRemove(double xPt, double yPt) {
    if (!loaded) return;
    const int x = static_cast<int>(std::floor(tileX(xPt))), y = static_cast<int>(std::floor(tileY(yPt)));
    const int i = world.entityAt(x, y);
    if (i == 0) { say("The core cannot be removed."); return; }
    if (i > 0) world.remove(x, y);
    else if (tool != Kind::None) tool = Kind::None;                 // a right click on empty ground puts the building down
}

}  // namespace defense
