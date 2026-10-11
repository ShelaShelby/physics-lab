#include "defense/defense_lab.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

namespace defense {

// ---------------------------------------------------------------- pure functions
Hotkey hotkeyFor(int digit) {
    Hotkey h;
    constexpr int nBar = static_cast<int>(sizeof kBuildBar / sizeof kBuildBar[0]);
    if (digit >= 1 && digit <= nBar) { h.action = HotkeyAction::Pick; h.kind = kBuildBar[digit - 1]; }
    else if (digit == nBar + 1) h.action = HotkeyAction::Turn;
    else if (digit == nBar + 2) h.action = HotkeyAction::Home;
    return h;
}

void beltLine(int x0, int y0, int x1, int y1, int dirSingle, std::vector<BeltStep>& out) {
    out.clear();
    const int dx = x1 - x0, dy = y1 - y0;
    if (dx == 0 && dy == 0) { out.push_back({x0, y0, dirSingle & 3}); return; }
    const int hDir = dx > 0 ? 1 : 3, vDir = dy > 0 ? 2 : 0;
    const bool hFirst = std::abs(dx) >= std::abs(dy);
    int x = x0, y = y0;
    const auto walk = [&](bool horiz, int n) {
        for (int k = 0; k < n; k++) {
            out.push_back({x, y, horiz ? hDir : vDir});
            if (horiz) x += dx > 0 ? 1 : -1; else y += dy > 0 ? 1 : -1;
        }
    };
    walk(hFirst, hFirst ? std::abs(dx) : std::abs(dy));
    walk(!hFirst, hFirst ? std::abs(dy) : std::abs(dx));
    out.push_back({x, y, out.back().dir});                       // the end tile keeps the last direction
}

double minZoomFor(double viewW, double viewH) {
    const double span = kMapTiles + 2.0 * kBorder;                 // map + band on each side, tiles
    return std::max(kZoomMin, std::max(viewW, viewH) / span);
}

void clampCamera(double& camX, double& camY, double& zoom, double viewW, double viewH) {
    zoom = std::clamp(zoom, minZoomFor(viewW, viewH), std::max(kZoomMax, minZoomFor(viewW, viewH)));
    const double lo = kMapMin - kBorder, hi = kMapMax + 1 + kBorder;   // tile edges of map + band
    const auto one = [&](double& c, double view) {
        const double half = view / 2 / zoom;
        if (hi - lo <= 2 * half + 1e-9) c = (lo + hi) / 2;                 // the window is wider than map + band: centred
        else c = std::clamp(c, lo + half, hi - half);
    };
    one(camX, viewW); one(camY, viewH);
}

CoreArrow coreArrow(double camX, double camY, double zoom, double viewW, double viewH, double coreCX, double coreCY, double margin) {
    CoreArrow a;
    const double sx = (coreCX - camX) * zoom + viewW / 2, sy = (coreCY - camY) * zoom + viewH / 2, half = 1.5 * zoom;
    const bool visible = sx + half > 0 && sx - half < viewW && sy + half > 0 && sy - half < viewH;   // any part of the 3 x 3 core on screen
    a.dist = std::hypot(coreCX - camX, coreCY - camY);
    if (visible) return a;
    a.show = true;
    const double vx = sx - viewW / 2, vy = sy - viewH / 2;
    a.angle = std::atan2(vy, vx);
    const double hw = std::max(1.0, viewW / 2 - margin), hh = std::max(1.0, viewH / 2 - margin);
    const double t = std::min(vx != 0 ? hw / std::fabs(vx) : 1e300, vy != 0 ? hh / std::fabs(vy) : 1e300);   // from the middle toward the core, to the inset edge
    a.x = viewW / 2 + vx * t; a.y = viewH / 2 + vy * t;
    return a;
}

void DefenseLab::open(const std::string& path, uint32_t freshSeed) {
    savePath = path;
    std::string text;
    if (path.empty() || !readFile(path, text)) { startNew(freshSeed); return; }
    std::string err;
    if (world.load(text, &err)) { loaded = true; loadError.clear(); world.zoom = std::clamp(world.zoom, kZoomMin, kZoomMax); return; }
    loaded = false;
    const int v = World::saveVersion(text);
    if (v >= 1 && v < World::currentSaveVersion()) {              // stage 2: an older game's save (its map is different): a short message, the file kept as .old
        writeFileAtomic(path + ".old", text);
        oldSave = true;
        loadError = "This planet was saved with an older version. Start a new planet. (A copy was kept as " + path + ".old.)";
        return;
    }
    writeFileAtomic(path + ".damaged", text);                      // kept, so nothing is lost if the owner wants to look at it
    loadError = "The saved planet could not be read: " + err + ". A copy was kept as " + path + ".damaged.";
}

void DefenseLab::startNew(uint32_t seed) {
    world.newGame(seed);
    loaded = true;
    loadError.clear();
    oldSave = false;
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
    if (feedHeld && clock >= feedNext) { world.feedGenerator(feedX, feedY); feedNext = clock + kFeedRepeat; }
    if (paused) { acc = 0; return; }
    const auto t0 = std::chrono::steady_clock::now();
    acc += dt;
    const double step = 1.0 / kTicksPerSecond;
    while (acc >= step && stepsLastFrame < 6) { world.step(); acc -= step; stepsLastFrame++; }   // (at most 6 a frame: a slow frame does not snowball)
    if (stepsLastFrame == 6) acc = 0;
    simMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void DefenseLab::pan(double dxPt, double dyPt) { world.camX -= dxPt / world.zoom; world.camY -= dyPt / world.zoom; clampView(); }

void DefenseLab::zoomAt(double factor, double xPt, double yPt) {
    const double tx = tileX(xPt), ty = tileY(yPt);
    world.zoom = std::clamp(world.zoom * factor, minZoomFor(viewW, viewH), std::max(kZoomMax, minZoomFor(viewW, viewH)));
    world.camX = tx - (xPt - viewW / 2) / world.zoom;               // the tile under the pointer stays under it
    world.camY = ty - (yPt - viewH / 2) / world.zoom;
    clampView();
}

void DefenseLab::home() { if (!loaded) return; world.camX = world.coreCX(); world.camY = world.coreCY(); clampView(); }
void DefenseLab::keyPan(double dx, double dy, double dt) { if (loaded && (dx != 0 || dy != 0)) pan(-dx * kPanKeySpeed * dt, -dy * kPanKeySpeed * dt); }

void DefenseLab::pickUnder() {
    if (!loaded || !hoverOn) return;
    const int i = world.entityAt(static_cast<int>(std::floor(hoverX)), static_cast<int>(std::floor(hoverY)));
    if (i <= 0) return;                                             // nothing, or the core (it cannot be built)
    tool = world.entities()[std::size_t(i)].kind;
    dir = world.entities()[std::size_t(i)].dir;
}

bool DefenseLab::hotkey(int digit) {
    const Hotkey h = hotkeyFor(digit);
    switch (h.action) {
    case HotkeyAction::Pick: pick(h.kind); return true;
    case HotkeyAction::Turn: rotate(); return true;
    case HotkeyAction::Home: home(); return true;
    default: return false;
    }
}

void DefenseLab::press(double xPt, double yPt, bool left) {
    if (!loaded) return;
    const double tx = tileX(xPt), ty = tileY(yPt);
    const int x = static_cast<int>(std::floor(tx)), y = static_cast<int>(std::floor(ty));
    if (left) {
        const int i = world.entityAt(x, y);
        if (i > 0 && world.entities()[std::size_t(i)].kind == Kind::Generator) {   // a click on a coal generator: coal from the core into it
            const int n = world.feedGenerator(x, y);
            if (n == 0 && world.core().store[static_cast<int>(Item::Coal)] == 0) say("The core has no coal.");
            feedHeld = true; feedX = x; feedY = y; feedNext = clock + kFeedRepeat;
            return;
        }
        if (tool == Kind::None) return;
        dragBuild = true;
        anchor(tool, tx, ty, dragX0, dragY0);
        dragX1 = dragX0; dragY1 = dragY0;
        if (tool != Kind::Belt) clickPlace(xPt, yPt);               // other buildings: placed at once, and again along the drag
        return;
    }
    dragRemove = true;
    lastRemoveX = x; lastRemoveY = y;
    const int i = world.entityAt(x, y);
    removedAny = i != -1;
    if (i == 0) say("The core cannot be removed.");
    else if (i > 0) world.remove(x, y);
}

void DefenseLab::drag(double xPt, double yPt) {
    if (!loaded) return;
    if (dragBuild) {
        int x, y;
        anchor(tool, tileX(xPt), tileY(yPt), x, y);
        if (tool != Kind::Belt && (x != dragX1 || y != dragY1) && world.canPlace(tool, x, y)) world.place(tool, x, y, dir);
        dragX1 = x; dragY1 = y;
        return;
    }
    if (dragRemove) {                                               // every tile on the way from the last one (no gaps when the pointer moves fast)
        const int x = static_cast<int>(std::floor(tileX(xPt))), y = static_cast<int>(std::floor(tileY(yPt)));
        if (x == lastRemoveX && y == lastRemoveY) return;
        beltLine(lastRemoveX, lastRemoveY, x, y, 0, line);
        for (const BeltStep& s : line) if (world.entityAt(s.x, s.y) > 0) { world.remove(s.x, s.y); removedAny = true; }
        lastRemoveX = x; lastRemoveY = y;
    }
}

void DefenseLab::release(double xPt, double yPt, bool left, bool moved) {
    if (!loaded) { dragBuild = dragRemove = feedHeld = false; return; }
    if (left && dragBuild && tool == Kind::Belt) {
        drag(xPt, yPt);
        beltLine(dragX0, dragY0, dragX1, dragY1, dir, line);
        int placed = 0, blocked = 0;
        for (const BeltStep& s : line) { if (world.canPlace(Kind::Belt, s.x, s.y)) placed += world.place(Kind::Belt, s.x, s.y, s.dir) != 0; else blocked++; }
        if (placed && line.size() > 1) dir = line.back().dir;       // the next line goes on the same way
        if (!placed && blocked) { std::string why; world.canPlace(Kind::Belt, line[0].x, line[0].y, &why); say(why); }
    }
    if (!left && !moved && !removedAny) tool = Kind::None;         // a right click on empty ground puts the building down
    dragBuild = dragRemove = feedHeld = false;
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
