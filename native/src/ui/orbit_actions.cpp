#include "ui/orbit_actions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "orbit/alphacen.h"
#include "orbit/format.h"
#include "orbit/real_errors.h"
#include "orbit/sky.h"
#include "orbit/spin.h"
#include "orbit/units.h"
#include "ui/journey_dates.h"

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

// select it, keep it in the middle, zoom to suit; the selected one again lets go (0.74.0: the camera and the zoom glide there over OrbitView::kGlideSeconds; Reduce motion: at once)
void fly(AppState& app, const Selection& s, double zoom) {
    OrbitView& v = app.view;
    if (v.sel == s) { v.letGo(); return; }
    v.pick(s);
    glideZoom(app, zoom, OrbitView::kGlideSeconds);
}

}  // namespace

std::vector<PickGroup> pickGroups(const AppState& app) { return orbit::pickList(app.sys, app.set.alphaCen); }

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
        if (v.sel == s) v.letGo();
        else if (farItem(app.sys, s.moon, fi)) {           // far enough out that the Sun and the target are both in the picture
            v.pick(s);                                     // (0.74.0: a glide, as fly)
            glideZoom(app, std::log10(13.6 / (fi.au * kAU / kSimLength)), OrbitView::kGlideSeconds);
        }
        break;
    }
    case Selection::ACen: {                                // 0.73.0: fly to it, fitted (the radius of view each thing needs: its neighbourhood)
        const int k = s.moon;
        if (k == acen::Sun) {                              // the Sun is 4.3 light years towards +z: a 3D view far enough out to see it (in 2D it is behind you)
            if (v.sel == s) { v.sel = Selection{}; v.follow = false; break; }
            v.select(s, false);
            app.set.view3D = true;
            v.yaw = 0.6; v.pitch = 2.7;                    // turned over, looking back towards +z: the Sun's marker and the Sun among their stars (Cassiopeia) both in view
            setZoom(app, std::log10(13.6 / (1000.0 / acen::kAbPlxMas * 3.26156 * kLightYearAU * kAU / kSimLength)));
            v.ox = v.oy = 0;
            break;
        }
        const double rAU = k == acen::Bary ? 36 : k == acen::StarA || k == acen::StarB || k == acen::CandidateA ? 3.0 : k == acen::PlanetC ? 2.0 : 0.11;
        fly(app, s, zoomFor(app, rAU * kAU / kKm, 0.42 * std::min(app.canvasW, app.canvasH)));
        break;
    }
    case Selection::None: break;
    }
}

void setSystem(AppState& app, bool on) {
    if (app.set.alphaCen == on) return;
    if (app.journey.active) journeyStop(app);
    app.set.alphaCen = on;
    app.view.sel = Selection{}; app.view.follow = false;
    app.view.ox = app.view.oy = 0; app.view.cdep = 0;
    if (on) acenFit(app, AcFit::Pair);
    else resetViewAction(app);
}

void acenFit(AppState& app, AcFit f) {
    if (!app.set.alphaCen) setSystem(app, true);
    OrbitView& v = app.view;
    // the radius to fit (au): the pair's apastron (35.4 au) with room; Proxima's whole orbit about A-B (apastron 12,300 au, its focus 306 au off); Proxima's
    // habitable zone (out to 0.081 au) round its planets
    const double rAU = f == AcFit::Pair ? 36 : f == AcFit::System ? 13000 : 0.1;
    v.select(Selection{Selection::ACen, 0, f == AcFit::Planets ? acen::Proxima : acen::Bary}, true);
    setZoom(app, zoomFor(app, rAU * kAU / kKm, 0.42 * std::min(app.canvasW, app.canvasH)));
    v.ox = v.oy = 0;
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

// "29,231" for the worst-error lists in the Real note (orbit/real_errors.h)
static std::string withCommas(long v) {
    std::string d = std::to_string(v), out;
    for (std::size_t i = 0; i < d.size(); i++) { if (i && (d.size() - i) % 3 == 0) out += ','; out += d[i]; }
    return out;
}

std::string relativityTip() {
return "Adds Einstein's correction to the Sun's pull on each planet, so Mercury's orbit turns a little more each time round: about 43 arcseconds per century, as measured.\n\n"
        "It is the first-order (1PN) field of the Sun (the formula of the IERS Conventions 2010). With it on the planets stay closest to JPL Horizons, which is relativistic too: "
        "Mercury's worst error in 1800 to 2100 is " + withCommas(orbit::kRealErrKmRelOn[0]) + " km with it and " + withCommas(orbit::kRealErrKmRelOff[0]) + " km without. "
        "It leaves out the planets' relativistic pull on each other (under 10 km here), the Sun's spin and shape, and all higher orders.\n\n"
        "On is the default. Off: plain Newtonian gravity.";
}

std::string wobbleTip() {
    return std::string("The Sun is not fixed: it circles the balance point of the Solar System (the barycentre, the marked point), mostly because of Jupiter's pull. "
                                       "The real circle is smaller than about two Sun radii, far too small to see here.\n\n"
                                       "Ticked: the Sun's path is drawn 50 times too big, round the marked point, so you can see it. This is exaggerated, not to scale (the Sun itself and the planets are drawn at their true places).\n\n"
                                       "Unticked: the path is drawn at its true size.");
}

// The planet whose moons can be watched: the selected planet, or the planet of the selected moon
static const Body* moonTarget(const AppState& app) {
    const Selection& s = app.view.sel;
    if (s.kind != Selection::Body && s.kind != Selection::Moon) return nullptr;
    const Body* b = findBody(app.sys, s.bodyId);
    return b && !b->moons.empty() ? b : nullptr;
}

bool slowMoonsAvailable(const AppState& app) { return app.set.moons && moonTarget(app); }

void slowMoons(AppState& app) {
    const Body* b = moonTarget(app);
    if (!b) return;
    double P = 1e300;                                 // the selected moon's period, or the fastest moon's
    if (app.view.sel.kind == Selection::Moon) P = b->moons[app.view.sel.moon].data->periodDays;
    else for (const Moon& m : b->moons) P = std::min(P, m.data->periodDays);
    app.set.tspeed = std::clamp(std::log10(P / 4 / (kSimTime / kDay)), kTspeedMin, kTspeedMax);   // one lap in 4 seconds
    if (app.view.sel.kind == Selection::Moon) app.view.select(Selection{Selection::Body, b->id, 0}, true);   // 0.73.1: a selected moon is followed itself now, so the planet is selected and followed: the moons circle it
    else app.view.follow = true;
}

static const Body* spinTarget(const AppState& app) {   // a selected planet with real spin data
    const Body* b = app.view.sel.kind == Selection::Body ? findBody(app.sys, app.view.sel.bodyId) : nullptr;
    return b && hasSpin(b->name) ? b : nullptr;
}

bool slowSpinAvailable(const AppState& app) { return spinTarget(app) != nullptr; }

void slowSpin(AppState& app) {
    const Body* b = spinTarget(app);
    if (!b) return;
    const double daysPerTurn = 360 / std::fabs(spinRateDegPerDay(b->name));
    app.set.tspeed = std::clamp(std::log10(daysPerTurn / 24 / (kSimTime / kDay)), kTspeedMin, kTspeedMax);   // 1 s = 1/24 of a turn
    app.view.follow = true;
    if (app.set.trueScale) setZoom(app, zoomFor(app, b->radius / kKm, 80));   // close enough to see it turn
}

std::string orbitStateLine(const AppState& app) {
    const ViewSettings& s = app.set;
    const Selection& sel = app.view.sel;
    double sx = 9999, sy = 9999, sr = 0;                 // 0.74.0 (the camera checks): where the selected thing was drawn last frame (picture points; 9999: nothing), the turn and tilt, and the kept picture's key of the view
    if (!app.view.selectedScreen(sx, sy, sr)) sx = sy = 9999;
    const unsigned long long key = app.view.pictureKey(app.sys, app.set, 0, 0, 1, app.canvasW, app.canvasH);
    char b[900];
    std::snprintf(b, sizeof b,
                  "epochMs=%lld t=%.9g jumped=%d paused=%d tspeed=%.6g | 3D=%d trueScale=%d fullTrails=%d moons=%d dwarfs=%d comets=%d belts=%d far=%d sky=%d cons=%d real=%d gr=%d wobble=%d light=%d | zoom=%.6g | sel=%d,%d,%d follow=%d pan=%d spin=%d journey=%d acen=%d moonoff=%.2f,%.2f planetoff=%.2f,%.2f cam=%.6f,%.6f selxy=%.2f,%.2f key=%llu",
                  static_cast<long long>(app.sys.epochMs), app.sys.t, app.sys.jumped ? 1 : 0, app.paused ? 1 : 0, s.tspeed,
                  s.view3D ? 1 : 0, s.trueScale ? 1 : 0, s.fullTrails ? 1 : 0, s.moons ? 1 : 0, s.dwarfs ? 1 : 0, s.comets ? 1 : 0, s.belts ? 1 : 0, s.far ? 1 : 0,
                  static_cast<int>(s.sky), static_cast<int>(s.cons), s.realPlanets ? 1 : 0, s.relativity ? 1 : 0, s.exaggerateWobble ? 1 : 0, s.trueLight ? 1 : 0, s.zoom,
                  static_cast<int>(sel.kind), sel.bodyId, sel.moon, app.view.follow ? 1 : 0, app.view.panMode ? 1 : 0, app.view.spin ? 1 : 0, app.journey.active ? 1 : 0, s.alphaCen ? 1 : 0,
                  app.view.drawnSel().ok ? app.view.drawnSel().moonX : 9999.0, app.view.drawnSel().ok ? app.view.drawnSel().moonY : 9999.0, app.view.drawnSel().planetX, app.view.drawnSel().planetY,
                  app.view.yaw, app.view.pitch, sx, sy, key);
    return b;
}

}  // namespace ui
