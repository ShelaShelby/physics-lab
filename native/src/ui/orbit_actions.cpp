#include "ui/orbit_actions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "orbit/format.h"
#include "orbit/sky.h"
#include "orbit/units.h"

namespace ui {

using namespace orbit;
using namespace units;

namespace {

// The zoom that suits each body in Easy to see (moons use their planet's): the old panel's table
double easyZoom(const std::string& n) {
    static const struct { const char* n; double z; } Z[] = {{"Sun", 0.8}, {"Mercury", 0.8}, {"Venus", 0.8}, {"Earth", 0.8}, {"Mars", 0.6}, {"Jupiter", 1.3}, {"Saturn", 1.2},
                                                            {"Uranus", 0.5}, {"Neptune", 0.3}, {"Ceres", 0.4}, {"Pluto", 0.3}, {"Haumea", 0.3}, {"Makemake", 0.3}, {"Eris", 0.3}};
    for (const auto& e : Z) if (n == e.n) return e.z;
    return 0.4;
}

// select it, keep it in the middle, zoom to suit; the selected one again lets go
void fly(AppState& app, const Selection& s, double zoom) {
    OrbitView& v = app.view;
    if (v.sel == s) { v.sel = Selection{}; v.follow = false; return; }
    v.select(s, true);
    setZoom(app, zoom);
    v.ox = v.oy = 0;
}

}  // namespace

std::vector<PickGroup> pickGroups(const AppState& app) {
    std::vector<PickGroup> g;
    PickGroup star{"Star and planets", {}}, dwarfs{"Dwarf planets", {}}, comets{"Famous comets", {}}, moons{"Moons", {}}, belts{"Belts and the far edge", {}}, far{"Far away", {}};
    star.items.push_back({"Sun", "#f0b020", {Selection::Star}, "", 0});
    star.items.push_back({"Barycentre", "#9aa5b8", {Selection::Bary}, "", 0});
    for (const Body& b : app.sys.bodies) {
        PickGroup& grp = b.type == BodyKind::Planet ? star : b.type == BodyKind::Dwarf ? dwarfs : comets;
        grp.items.push_back({b.name, b.color, {Selection::Body, b.id}, "", 0});
    }
    for (const Body& b : app.sys.bodies)
        for (std::size_t j = 0; j < b.moons.size(); j++)
            moons.items.push_back({b.moons[j].data->name, b.color, {Selection::Moon, b.id, static_cast<int>(j)}, std::string(b.moons[j].data->name) + " · moon of " + b.name, 0});
    static const struct { const char* key; const char* name; const char* color; double zoom; } B[] = {
        {"main", "Asteroid belt", "#8a7f72", -0.15}, {"trojan", "Jupiter’s Trojans", "#a08c6a", -0.55}, {"kuiper", "Kuiper belt", "#6f9fc0", -1.55}, {"oort", "Oort cloud", "#9aa5b8", -4.4}};
    for (const auto& b : B)
        for (std::size_t gi = 0; gi < app.sys.belts.size(); gi++)
            if (app.sys.belts[gi].key == b.key) belts.items.push_back({b.name, b.color, {Selection::Belt, 0, static_cast<int>(gi)}, "", b.zoom});
    static const struct { const char* key; const char* name; const char* color; } F[] = {
        {"v1", "Voyager 1", "#ffd27a"}, {"v2", "Voyager 2", "#ffd27a"}, {"acen", "Alpha Centauri", "#ffe9b0"}, {"sirius", "Sirius", "#cfe0ff"},
        {"vega", "Vega", "#cfe0ff"}, {"pleia", "Pleiades", "#8fd0ff"}, {"m42", "Orion Nebula", "#8fd0ff"}, {"sgra", "Galactic centre", "#cfd6e6"}};
    for (const auto& f : F) far.items.push_back({f.name, f.color, {Selection::Far, 0, farIndexOf(f.key)}, "", 0});
    g.push_back(star); g.push_back(dwarfs); g.push_back(comets); g.push_back(moons); g.push_back(belts); g.push_back(far);
    return g;
}

void pickItem(AppState& app, const PickItem& it) {
    OrbitView& v = app.view;
    const bool ts2 = app.set.trueScale;
    const Selection& s = it.sel;
    switch (s.kind) {
    case Selection::Star: fly(app, s, ts2 ? zoomFor(app, kSunRadius / kKm, 90) : easyZoom("Sun")); break;
    case Selection::Bary: fly(app, s, ts2 ? 2.3 : 2.0); break;
    case Selection::Body: {
        const Body* b = findBody(app.sys, s.bodyId);
        if (!b) break;
        if (b->type == BodyKind::Dwarf) app.set.dwarfs = true;
        if (b->type == BodyKind::Comet) app.set.comets = true;
        fly(app, s, b->type == BodyKind::Comet ? 1.0 : !ts2 ? easyZoom(b->name) : zoomFor(app, b->radius / kKm, 55));   // true scale: the planet fills the picture
        break;
    }
    case Selection::Moon: {
        const Body* b = findBody(app.sys, s.bodyId);
        if (!b || s.moon < 0 || s.moon >= static_cast<int>(b->moons.size())) break;
        app.set.moons = true;
        if (b->type == BodyKind::Dwarf) app.set.dwarfs = true;
        fly(app, s, ts2 ? zoomFor(app, b->moons[s.moon].data->aKm, 0.34 * std::min(app.canvasW, app.canvasH)) : easyZoom(b->name));   // true scale: fit the moon's orbit
        break;
    }
    case Selection::Belt:                                  // belts: selected but not followed; each has a zoom that shows all of it
        app.set.belts = true;
        if (v.sel == s) { v.sel = Selection{}; v.follow = false; }
        else { v.select(s, false); setZoom(app, it.beltZoom); v.ox = v.oy = 0; }
        break;
    case Selection::Far: {
        app.set.far = true;
        FarItem fi;
        if (v.sel == s) { v.sel = Selection{}; v.follow = false; }
        else if (farItem(app.sys, s.moon, fi)) {           // far enough out that the Sun and the target are both in the picture
            v.select(s, true);
            setZoom(app, std::log10(13.6 / (fi.au * kAU / kSimLength)));
            v.ox = v.oy = 0;
        }
        break;
    }
    case Selection::None: break;
    }
}

ChipText chipText(const AppState& app) {
    ChipText c;
    const SolarSystem& sys = app.sys;
    const double yrs = sys.t / kYear;
    std::string epoch = fmt::dateUTC(sys.epochMs);
    if (sys.jumped) {                                      // a picked date also shows its time of day (Greenwich time), as the old date line did
        const int64_t m = ((sys.epochMs / 60000) % 1440 + 1440) % 1440;
        char hm[16];
        std::snprintf(hm, sizeof hm, ", %02d:%02d UT", static_cast<int>(m / 60), static_cast<int>(m % 60));
        epoch += hm;
    }
    if (yrs * 365.25 < 1) c.date = epoch;
    else { c.date = fmt::simDate(sys.epochMs, yrs); c.extra = fmt::years(yrs) + " after " + epoch; }
    c.speed = app.paused ? "Paused" : tspeedText(app.set.tspeed);
    return c;
}

std::string orbitStateLine(const AppState& app) {
    const ViewSettings& s = app.set;
    const Selection& sel = app.view.sel;
    char b[640];
    std::snprintf(b, sizeof b,
                  "epochMs=%lld t=%.9g jumped=%d paused=%d tspeed=%.6g | 3D=%d trueScale=%d fullTrails=%d moons=%d dwarfs=%d comets=%d belts=%d far=%d sky=%d cons=%d real=%d gr=%d wobble=%d | zoom=%.6g | sel=%d,%d,%d follow=%d pan=%d spin=%d",
                  static_cast<long long>(app.sys.epochMs), app.sys.t, app.sys.jumped ? 1 : 0, app.paused ? 1 : 0, s.tspeed,
                  s.view3D ? 1 : 0, s.trueScale ? 1 : 0, s.fullTrails ? 1 : 0, s.moons ? 1 : 0, s.dwarfs ? 1 : 0, s.comets ? 1 : 0, s.belts ? 1 : 0, s.far ? 1 : 0,
                  static_cast<int>(s.sky), static_cast<int>(s.cons), s.realPlanets ? 1 : 0, s.relativity ? 1 : 0, s.exaggerateWobble ? 1 : 0, s.zoom,
                  static_cast<int>(sel.kind), sel.bodyId, sel.moon, app.view.follow ? 1 : 0, app.view.panMode ? 1 : 0, app.view.spin ? 1 : 0);
    return b;
}

}  // namespace ui
