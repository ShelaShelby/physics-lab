#include "blackhole/lab.h"
#include "blackhole/gas.h"
#include "blackhole/low_priority.h"

#include <algorithm>
#include <array>
#include <list>
#include <chrono>
#include <future>
#include <map>
#include <mutex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "blackhole/kerr.h"
#include "blackhole/presets.h"
#include "blackhole/returning.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"
#include "orbit/camera_motion.h"
#include "orbit/sky.h"
#include "orbit/units.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;

bool num(const std::map<std::string, std::string>& in, const char* key, double lo, double hi, double& out) {
    const auto it = in.find(key);
    if (it == in.end() || it->second.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    if (!end || *end || !(v >= lo && v <= hi)) return false;
    out = v;
    return true;
}
std::string text(double v) { char b[40]; std::snprintf(b, sizeof b, "%.17g", v); return b; }
double logT(double v, double lo, double hi) { return std::clamp((std::log(v) - std::log(lo)) / (std::log(hi) - std::log(lo)), 0.0, 1.0); }
double logV(double t, double lo, double hi) { return std::exp(std::log(lo) + std::clamp(t, 0.0, 1.0) * (std::log(hi) - std::log(lo))); }
}  // namespace

bool Settings::operator==(const Settings& o) const {
    return preset == o.preset && massSun == o.massSun && dist == o.dist && fovDeg == o.fovDeg && yaw == o.yaw && pitch == o.pitch && bend == o.bend && skyOurs == o.skyOurs &&
           disc == o.disc && feed == o.feed && outer == o.outer && expAuto == o.expAuto && expEV == o.expEV && spin == o.spin && look == o.look &&
           trueSky == o.trueSky && glare == o.glare && allLight == o.allLight && gasMoving == o.gasMoving && gasRun == o.gasRun && gasRateAuto == o.gasRateAuto &&
           gasClump == o.gasClump && gasPattern == o.gasPattern && gasRate == o.gasRate && gasClumpR == o.gasClumpR;
}

Settings settingsDefaults() {
    Settings s;
    applyPreset(s, "sgra");
    resetView(s);
    return s;
}

void resetView(Settings& s) {
    s.dist = kDistDefault;
    s.fovDeg = kFovDefault;
    // looking at the galactic centre, as Sagittarius A* is seen from Earth (the yaw and pitch of skyScreen that put that direction in the middle:
    // skyDirection at the middle is (-sb sa, -sb ca, -cb))
    const orbit::Vec3 g = orbit::skyData().galCentre;
    const double cb = -g.z, sb = std::sqrt(std::max(0.0, 1 - cb * cb));
    s.yaw = std::atan2(-g.x / sb, -g.y / sb);
    s.pitch = kTiltDefaultDeg * kPi / 180;                              // 0.70.0: the camera 100 degrees from the disc's axis (lab.h); the yaw still towards the
                                                                        // galactic centre, now 15.5 degrees from the middle (until 0.69 the pitch, 84.4
                                                                        // degrees, put it in the middle, the camera above the disc)
}

bool applyPreset(Settings& s, const std::string& key) {
    if (key == kFeedingKey) {
        s.preset = kFeedingKey;
        s.massSun = kFeedingMass;
        s.disc = true;
        s.outer = kOuterDefault;                                        // 0.58.0: Goodman 2003's edge (lab.h)
        s.spin = 0;                                                     // (no invented spin: the model's start, as the real holes')
        s.expEV = 0;                                                    // 0.70.0: the manual exposure starts from automatic again
        s.gasRateAuto = true;                                           // 0.72.0: the moving gas's automatic rate again
        resetView(s);                                                   // 0.70.0: the starting view (lab.h)
        keepOutsideDisc(s);                                             // 0.94.1 (the starting view is far above the surface: no change)
        return true;
    }
    const Preset* p = preset(key.c_str());
    if (!p) return false;
    s.preset = p->key;
    s.massSun = p->massSun;
    s.disc = false;                                                     // (owner's decision of 15 Nov 2026: the real holes open with no disc)
    s.spin = 0;                                                         // (owner's decision of 15 Nov 2026: spin not measured; it starts at 0)
    s.expEV = 0;                                                        // 0.70.0: the manual exposure starts from automatic again
    s.gasRateAuto = true;                                               // 0.72.0: the moving gas's automatic rate again
    return true;
}

double gasRateOf(const Settings& s) {
    if (!s.gasRateAuto) return s.gasRate;
    const Settings v = shown(s);
    const double rIn = discGeom(v).rIn;
    return std::clamp(gasPeriod(rIn, v.spin) * massTimeSeconds(v.massSun) / kGasAutoSeconds, kGasRateMin, kGasRateMax);
}
void setGasRate(Settings& s, double r) { s.gasRate = std::clamp(r, kGasRateMin, kGasRateMax); s.gasRateAuto = false; }
double gasRateToSlider(double r) { return logT(r, kGasRateMin, kGasRateMax); }
double sliderToGasRate(double t) { return logV(t, kGasRateMin, kGasRateMax); }
bool gasOn(const Settings& s) { const Settings v = shown(s); return v.disc && v.bend && v.gasMoving && (v.gasClump || v.gasPattern); }
void gasTick(const Settings& s, double dt, double& T) {
    if (!gasOn(s) || !s.gasRun || !(dt > 0)) return;
    T += dt * gasRateOf(s) / massTimeSeconds(shown(s).massSun);
}
double gasClumpRadius(const Settings& s) {
    const DiscGeom g = discGeom(shown(s));
    return std::clamp(s.gasClumpR, g.rIn, std::max(g.rIn, g.rOut));
}

void setMass(Settings& s, double m) {
    s.massSun = std::clamp(m, kMassMin, kMassMax);
    if (s.preset == kFeedingKey) { if (s.massSun != kFeedingMass) s.preset.clear(); return; }
    const Preset* p = s.preset.empty() ? nullptr : preset(s.preset.c_str());
    if (!p || p->massSun != s.massSun) s.preset.clear();
}
void setFeed(Settings& s, double l) { s.feed = std::clamp(l, kThinMin, kThinMax); }
void setOuter(Settings& s, double r) { s.outer = std::clamp(r, kOuterMin, kOuterMax); }
void setExposure(Settings& s, double e) { s.expEV = std::clamp(e, kEvMin, kEvMax); s.expAuto = false; }
double exposureStops(const Settings& s) { return s.expAuto ? 0 : s.expEV; }
void setDisc(Settings& s, bool on) { if (s.disc != on) s.expEV = 0; s.disc = on; }
void setSpin(Settings& s, double a) { s.spin = std::clamp(a, 0.0, kSpinTop); }
void setLook(Settings& s, Look l) { if (s.look != l) s.expEV = 0; s.look = l; }
bool allLightOn(const Settings& s) { return s.look == Look::Real && s.disc && s.allLight; }
bool trueSkyOn(const Settings& s) { return s.look == Look::Real && s.disc && s.bend && s.skyOurs && s.trueSky && !s.allLight; }
bool glareOn(const Settings& s) { return s.look == Look::Real && s.disc && s.bend && s.glare && !s.allLight; }
Settings shown(const Settings& s) {
    if (s.look != Look::Film) return s;
    Settings o = s;
    const LookRule r = lookRule(Look::Film);
    o.spin = r.spin;                                                    // J15 section 4.1.1 p. 26 (looks.h)
    o.disc = true;
    return o;
}
void paperView(Settings& s) {
    const FilmChoices& c = filmChoices();
    setDistance(s, c.camR);                                             // J15 Figure 13's caption p. 24
    s.pitch = std::clamp(c.camTheta, kPitchMin, kPitchMax);             // the lab's pitch is the camera's angle from the spin axis (theta), as J15's theta_c
    keepOutsideDisc(s);                                                 // 0.94.1
}
void setDistance(Settings& s, double d) { s.dist = std::clamp(d, kDistMin, kDistMax); keepOutsideDisc(s); }
void setFov(Settings& s, double f) { s.fovDeg = std::clamp(f, kFovMin, kFovMax); }
void turn(Settings& s, double dy, double dp) {
    s.yaw = std::remainder(s.yaw + dy, 2 * kPi);
    const double p0 = s.pitch;
    s.pitch = std::clamp(s.pitch + dp, kPitchMin, kPitchMax);
    const int side = p0 < kPi / 2 ? 1 : p0 > kPi / 2 ? -1 : (dp < 0 ? 1 : -1);   // the side it came from (in the plane, beyond the disc: the way it moves)
    if (camDiscPitch(s, s.dist, side) != kPi / 2) {
        if (side > 0) s.pitch = std::min(s.pitch, camDiscPitch(s, s.dist, 1));
        else s.pitch = std::max(s.pitch, camDiscPitch(s, s.dist, -1));
    }
}

// 0.94.1: the camera outside the disc's thickness (lab.h)
double camDiscPitch(const Settings& s, double dist, int side) {
    const Settings v = shown(s);
    if (!v.disc || side == 0) return kPi / 2;
    const DiscGeom g = discGeom(v);
    const bool thick = v.look == Look::Real && heightOn(v.spin);
    DiscPhys dp; dp.massSun = v.massSun; dp.lEdd = v.feed; dp.aSigned = v.spin;   // (as discPhys, without the returning light's table: h does not use it)
    const auto h = [&](double R) { return thick ? dp.height(R) : 0.0; };
    // |cos(pitch)| = (h(R) + margin) / dist with R = dist sin(pitch): a fixed point (h changes slowly with R: 6 steps are far below a pixel)
    double c = std::min(1.0, (h(dist) + kCamDiscMargin) / dist);
    for (int i = 0; i < 6; i++) c = std::min(1.0, (h(dist * std::sqrt(std::max(0.0, 1 - c * c))) + kCamDiscMargin) / dist);
    const double R = dist * std::sqrt(std::max(0.0, 1 - c * c));
    if (R < g.rIn || R > g.rOut) return kPi / 2;                        // beyond the disc's edges: nothing stops the camera
    const double th = std::acos(c);
    return side > 0 ? std::max(kPitchMin, th) : std::min(kPitchMax, kPi - th);
}
bool keepOutsideDisc(Settings& s, int side) {
    if (side == 0) side = s.pitch > kPi / 2 ? -1 : 1;
    const double lim = camDiscPitch(s, s.dist, side);
    if (lim == kPi / 2) return false;
    const double p = side > 0 ? std::min(s.pitch, lim) : std::max(s.pitch, lim);
    if (p == s.pitch) return false;
    s.pitch = p;
    return true;
}
bool cameraAtDisc(const Settings& s) {
    const int side = s.pitch > kPi / 2 ? -1 : 1;
    const double lim = camDiscPitch(s, s.dist, side);
    return lim != kPi / 2 && std::fabs(s.pitch - lim) < 1e-9;
}

// 0.82.0: the camera's motion (lab.h CameraMotion), as orbit_view.cpp's drag, grab, release and tick
void dragTurn(Settings& s, CameraMotion& m, double dx, double dy) {
    double oYaw = 0, oPitch = 0;
    orbit::dragAngles(dx, dy, oYaw, oPitch);                             // the Orbit Lab's turn for this drag; this lab's picture is turned half a turn from it
    const double p0 = s.pitch, dYaw = -oYaw;
    turn(s, dYaw, -oPitch);
    m.turnYaw += dYaw; m.turnPitch += s.pitch - p0; m.turned = true;
}
void grab(CameraMotion& m) { m.pressed = true; m.vYaw = m.vPitch = 0; }
void release(CameraMotion& m, bool reduceMotion) { m.pressed = false; orbit::momentumRelease(m.vYaw, m.vPitch, reduceMotion); }
void stepDistance(Settings& s, CameraMotion& m, double notches, bool reduceMotion) {
    const double from = m.zoomOn ? m.zoomTo : s.dist;                    // from where a running step is going
    const double to = std::clamp(from * std::pow(kNotch, -notches), kDistMin, kDistMax);
    if (reduceMotion || to == s.dist) { m.zoomOn = false; setDistance(s, to); return; }   // (at the end of the range: nothing to ease)
    m.zoomOn = true; m.zoomFrom = s.dist; m.zoomTo = to; m.zoomS = 0; m.zoomLast = s.dist;
}
void stopMotion(CameraMotion& m) { m.vYaw = m.vPitch = 0; m.turnYaw = m.turnPitch = 0; m.turned = false; m.zoomOn = false; }
bool cameraMoving(const CameraMotion& m) { return m.vYaw != 0 || m.vPitch != 0 || m.zoomOn; }
void cameraTick(Settings& s, CameraMotion& m, double dt, bool reduceMotion) {
    // a turn or tilt set by someone else since the last tick (Reset view, the paper's view, a loaded setting) stops the coasting
    if ((!m.turned && (s.yaw != m.heldYaw || s.pitch != m.heldPitch)) || reduceMotion) { if (!m.pressed) m.vYaw = m.vPitch = 0; }
    if (m.pressed) orbit::momentumFollow(m.vYaw, m.vPitch, m.turnYaw, m.turnPitch, dt);
    else if (m.vYaw != 0 || m.vPitch != 0) {                             // a released turn coasts, fading; turn keeps the pitch inside its limits
        const double g = orbit::momentumCoastSeconds(dt);
        turn(s, m.vYaw * g, m.vPitch * g);
        orbit::momentumFade(m.vYaw, m.vPitch, dt);
    }
    m.turnYaw = m.turnPitch = 0; m.turned = false;
    if (m.zoomOn) {                                                      // the eased step, in ln distance; it ends exactly on its value
        if (s.dist != m.zoomLast || reduceMotion) m.zoomOn = false;     // the slider, Reset view or a preset moved the camera: that wins
        else {
            m.zoomS += dt / orbit::kWheelZoomSeconds;
            const double e = orbit::ease01(m.zoomS);
            const double d = std::exp(std::log(m.zoomFrom) + (std::log(m.zoomTo) - std::log(m.zoomFrom)) * e);
            setDistance(s, m.zoomS >= 1 ? m.zoomTo : std::clamp(d, std::min(m.zoomFrom, m.zoomTo), std::max(m.zoomFrom, m.zoomTo)));   // (exp of ln may miss by a bit)
            m.zoomLast = s.dist;
            if (m.zoomS >= 1) m.zoomOn = false;
        }
    }
    keepOutsideDisc(s);                                                  // 0.94.1: whatever else moved the disc or the camera this frame (a setting that grows or
                                                                         // thickens the disc, the spin, the look, a preset): out to the surface on the side it is on
    m.heldYaw = s.yaw; m.heldPitch = s.pitch;
}

void saveSettings(const Settings& s, std::map<std::string, std::string>& out) {
    out.clear();
    out["preset"] = s.preset.empty() ? "none" : s.preset;
    out["mass"] = text(s.massSun);
    out["dist"] = text(s.dist);
    out["fov"] = text(s.fovDeg);
    out["yaw"] = text(s.yaw);
    out["pitch"] = text(s.pitch);
    out["bend"] = s.bend ? "on" : "off";
    out["sky"] = s.skyOurs ? "ours" : "black";
    out["disc"] = s.disc ? "on" : "off";
    out["feed"] = text(s.feed);
    out["outer"] = text(s.outer);
    out["exposure"] = s.expAuto ? "auto" : "stops";                    // 0.70.0: your choice in stops from automatic (exposurestops)
    out["exposurestops"] = text(s.expEV);
    out["spin"] = text(s.spin);
    out["look"] = lookKey(s.look);
    out["skybeside"] = s.trueSky ? "true" : "shown";                   // 0.55.4 (light.h)
    out["glare"] = s.glare ? "eye" : "off";
    out["light"] = s.allLight ? "all" : "eye";                         // 0.58.0
    out["gas"] = s.gasMoving ? "on" : "off";                           // 0.72.0: the moving gas
    out["gasrun"] = s.gasRun ? "on" : "off";
    out["gasrate"] = s.gasRateAuto ? "auto" : text(s.gasRate);
    out["gasclump"] = s.gasClump ? "on" : "off";
    out["gaspattern"] = s.gasPattern ? "on" : "off";
    out["gasclumpr"] = text(s.gasClumpR);
}

Settings loadSettings(const std::map<std::string, std::string>& in) {
    Settings s = settingsDefaults();
    double v;
    if (const auto it = in.find("preset"); it != in.end()) { if (it->second == "none") s.preset.clear(); else applyPreset(s, it->second); }
    if (num(in, "mass", kMassMin, kMassMax, v)) setMass(s, v);
    if (num(in, "dist", kDistMin, kDistMax, v)) s.dist = v;
    if (num(in, "fov", kFovMin, kFovMax, v)) s.fovDeg = v;
    if (num(in, "yaw", -kPi - 1e-9, kPi + 1e-9, v)) s.yaw = v;
    if (num(in, "pitch", kPitchMin, kPitchMax, v)) s.pitch = v;
    if (const auto it = in.find("bend"); it != in.end() && (it->second == "on" || it->second == "off")) s.bend = it->second == "on";
    if (const auto it = in.find("sky"); it != in.end() && (it->second == "ours" || it->second == "black")) s.skyOurs = it->second == "ours";
    if (const auto it = in.find("disc"); it != in.end() && (it->second == "on" || it->second == "off")) s.disc = it->second == "on";
    if (num(in, "feed", kThinMin, kThinMax, v)) s.feed = v;
    if (num(in, "outer", kOuterMin, kOuterMax, v)) s.outer = v;
    if (const auto it = in.find("exposure"); it != in.end() && it->second == "auto") s.expAuto = true;
    else if (it != in.end() && it->second == "stops") s.expAuto = false;
    else if (num(in, "exposure", -1e300, 1e300, v)) { s.expAuto = false; s.expEV = 0; }   // 0.70.0: an old file's absolute screen white: your choice at 0 stops
    if (num(in, "exposurestops", kEvMin, kEvMax, v)) s.expEV = v;
    if (num(in, "spin", 0, kSpinTop, v)) s.spin = v;
    if (const auto it = in.find("look"); it != in.end()) { Look l; if (lookFromKey(it->second, l)) s.look = l; }   // (a file without the line: Real)
    if (const auto it = in.find("skybeside"); it != in.end() && (it->second == "true" || it->second == "shown")) s.trueSky = it->second == "true";   // 0.55.4: a file without it: Shown
    if (const auto it = in.find("glare"); it != in.end() && (it->second == "eye" || it->second == "off")) s.glare = it->second == "eye";            // (without it: the eye's, the default)
    if (const auto it = in.find("light"); it != in.end() && (it->second == "all" || it->second == "eye")) s.allLight = it->second == "all";          // 0.58.0 (without it: the eye's)
    const auto onOff = [&](const char* key, bool& b) { if (const auto it = in.find(key); it != in.end() && (it->second == "on" || it->second == "off")) b = it->second == "on"; };
    onOff("gas", s.gasMoving); onOff("gasrun", s.gasRun); onOff("gasclump", s.gasClump); onOff("gaspattern", s.gasPattern);   // 0.72.0 (without them: on)
    if (const auto it = in.find("gasrate"); it != in.end() && it->second == "auto") s.gasRateAuto = true;
    else if (num(in, "gasrate", kGasRateMin, kGasRateMax, v)) { s.gasRate = v; s.gasRateAuto = false; }
    if (num(in, "gasclumpr", 1, kGasClumpRMax, v)) s.gasClumpR = v;
    keepOutsideDisc(s);                                                 // 0.94.1: a saved or typed pitch inside the disc's thickness goes out to its surface
    return s;
}

double distToSlider(double d) { return logT(d, kDistMin, kDistMax); }
double sliderToDist(double t) { return logV(t, kDistMin, kDistMax); }
double massToSlider(double m) { return logT(m, kMassMin, kMassMax); }
double sliderToMass(double t) { return logV(t, kMassMin, kMassMax); }
double feedToSlider(double l) { return logT(l, kThinMin, kThinMax); }
double sliderToFeed(double t) { return logV(t, kThinMin, kThinMax); }
double outerToSlider(double r) { return logT(r, kOuterMin, kOuterMax); }
double sliderToOuter(double t) { return logV(t, kOuterMin, kOuterMax); }

DiscPhys discPhys(const Settings& s) {
    DiscPhys p; p.massSun = s.massSun; p.lEdd = s.feed; p.aSigned = s.spin;
    p.ret = discReturn(s);                                              // 0.87.0 R11
    return p;
}
const ReturnFlux* discReturn(const Settings& s) {
    return s.look == Look::Real && discLightRules().returning ? returnFluxFor(s.spin) : nullptr;
}
DiscGeom discGeom(const Settings& s) {
    DiscGeom g;
    if (s.look == Look::Film) { const LookRule r = lookRule(Look::Film); g.rIn = r.rIn; g.rOut = r.rOut; return g; }   // J15 Figure 13's caption (looks.h)
    g.rIn = discInner(s.spin); g.rOut = s.outer;
    g.height = discHeight(s);
    return g;
}
// 0.95.0: every spin (the spinning hole's surface: kerr_closed.h "THE DISC'S SURFACE"; 0.79.0 to 0.94: spin 0 only)
bool discThickDrawn(const Settings& s) { return s.look == Look::Real && heightOn(s.spin); }
std::string discShapeWords(const Settings& s) {
    if (discThickDrawn(s)) return "at its calculated thickness (Zhu et al. 2012, App. B), its surface opaque";
    return "flat in its plane, with no thickness, opaque";
}
std::string discEdgeOnWords(const Settings& s) {
    if (discThickDrawn(s))
        return "You are exactly in the disc's plane. The disc is drawn at its calculated thickness (Zhu et al. 2012, App. B), so edge on it is a band as high "
               "as the disc is thick, not a line; gravity bends the light of its far side's faces into view above and below the hole.";
    return "You are exactly in the disc's plane: this look's disc has no thickness, so seen exactly edge on it is a line.";
}
std::string discThicknessWords(const Settings& s) {
    if (discThickDrawn(s))
        return "The disc is drawn at its calculated thickness: its surface sits at the height h where the pressure of its light and gas holds it up against the hole's pull "
               "(Zhu et al. 2012, one zone in height; round a spinning hole with the spin's own term, Abramowicz and Fragile 2013). It thins to nothing at the "
               "innermost stable orbit, where the gas stops making heat. Limits: one zone in height, the surface drawn opaque at h, its light leaving at the angle "
               "it would leave the flat disc (the surface's tilt is left out), no light from inside the innermost orbit; the faint rings of light that circled "
               "the hole more than once are drawn where they cross the plane.";
    return "This look's disc has no thickness: it is drawn flat in its plane.";
}
const HeightTable* discHeight(const Settings& s) {
    if (!discThickDrawn(s)) return nullptr;
    static std::mutex mx;
    static std::list<std::pair<std::array<double, 3>, HeightTable>> kept;      // (the last 64 discs: a pointer stays good for many frames)
    std::lock_guard<std::mutex> lk(mx);
    const std::array<double, 3> key{s.feed, s.massSun, s.spin};
    for (auto& e : kept) if (e.first == key) return &e.second;
    if (kept.size() >= 64) kept.pop_back();
    kept.emplace_front(key, heightTable(discPhys(s)));
    return &kept.front().second;
}
void discLight(const Settings& s, double r, double g, double mu, DiscPixel& o) {
    o.fcol = 1; o.mu = mu; o.limb = 1;
    if (s.look == Look::Film) {                                         // one temperature, no shift, the film's white balance (looks.h)
        o.T = o.Tobs = lookRule(Look::Film).discT;
        o.rgb = filmDiscLinear();
        o.Y = filmDiscY();
        return;
    }
    discLightT(s, discPhys(s).temperature(r), g, mu, o);
}
void discLightT(const Settings& s, double T, double g, double mu, DiscPixel& o) {
    o.fcol = 1; o.mu = mu; o.limb = 1;
    o.T = T;
    const DiscLightRules& R = discLightRules();
    o.fcol = R.fcol ? colourCorrection(o.T) : 1;                        // 0.87.0 R9: f^-4 B(f T) (disc.h)
    o.limb = R.limb && mu >= 0 ? limbDarkening(mu) : 1;                 // 0.87.0 R10 (disc.h)
    o.Tobs = g * o.fcol * o.T;                                          // Riazuelo (53): the colour temperature seen
    const double k = 1 / (o.fcol * o.fcol * o.fcol * o.fcol) * o.limb;
    if (s.allLight) {                                                   // 0.58.0: all light, the eye's colour (disc.h allLightLinear)
        o.rgb = allLightLinear(o.Tobs);
        o.Y = o.rgb[0] > 0 || o.rgb[1] > 0 || o.rgb[2] > 0 ? bolometricVsSun(o.Tobs) : 0;
        if (k != 1) { for (double& c : o.rgb) c *= k; o.Y *= k; }
        return;
    }
    o.rgb = blackBodyLinearFast(o.Tobs);
    o.Y = visibleVsSunFast(o.Tobs);
    if (k != 1) { for (double& c : o.rgb) c *= k; o.Y *= k; }
}
// 0.84.0 (lab.h): the held camera's own shift, the disc and the stars as it sees them
double heldCameraShift(const Settings& s, double b) {
    if (!lookRule(s.look).shifts) return 1;                            // Film: no shifts (looks.h)
    return hoverShift(s.spin, s.dist, s.pitch, b);                      // J15 (A.16), beta = 0: (1 - b omega) / alpha at the camera (r = dist, theta = pitch)
}
double spinEmissionCos(const Settings& s, double th, double chi, double g, double r) {
    const KerrBL ch(s.spin);
    const Camera cam{s.dist, s.pitch, 0};
    Ray ray;
    if (!ch.launch(cam, lookAt(std::min(th, kPi), chi), ray)) return -1;
    return emissionCos(g, ray.c[1], r);
}
void discLightSeen(const Settings& s, double r, double g, double b, double mu, DiscPixel& o) { discLight(s, r, g * heldCameraShift(s, b), mu, o); }
StarSeen starSeen(const std::array<int, 3>& rgb, double T, double f) {
    StarSeen o;
    o.rgb = rgb;
    if (f == 1 || !(T > 0) || !(f > 0)) return o;
    const std::array<double, 3> a = blackBodyLinearFast(T), b = blackBodyLinearFast(f * T);
    double l[3], m = 0;
    for (int k = 0; k < 3; k++) {
        const double e = rgb[std::size_t(k)] / 255.0, lin = e <= 0.04045 ? e / 12.92 : std::pow((e + 0.055) / 1.055, 2.4);   // IEC 61966-2-1
        l[k] = a[std::size_t(k)] > 0 ? lin * b[std::size_t(k)] / a[std::size_t(k)] : 0;
        m = std::max(m, l[k]);
    }
    for (int k = 0; k < 3; k++) {
        const double x = m > 0 ? std::clamp(l[k] / m, 0.0, 1.0) : 0;
        o.rgb[std::size_t(k)] = int(std::lround(255 * (x <= 0.0031308 ? 12.92 * x : 1.055 * std::pow(x, 1 / 2.4) - 0.055)));
    }
    const double v0 = visibleVsSunFast(T);
    o.vRatio = v0 > 0 ? visibleVsSunFast(f * T) / v0 : 1;
    o.boloRatio = f * f * f * f;
    o.dmag = -2.5 * std::log10(o.vRatio);
    return o;
}
Vec3 discAxisView(const HoleView& v) { return Vec3{0, -v.sb, v.cb}; }

DiscPixel discPixel(const DiscTable& t, const Settings& s, const HoleView& v, double X, double Y, bool light, bool plane) {
    DiscGeom g = discGeom(s);
    if (plane) g.height = nullptr;
    return discPixelG(t, s, g, v, X, Y, light);
}
DiscPixel discPixelG(const DiscTable& t, const Settings& s, const DiscGeom& g, const HoleView& v, double X, double Y, bool light) {
    DiscPixel o;
    const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy);
    const double th = d / v.K;
    if (!(th <= kPi)) return o;
    const double x = th - t.alphaSh();
    const double nx = d > 0 ? dx / d : 0, ny = d > 0 ? dy / d : 0;
    const Vec3 N = discAxisView(v);
    o.hit = discRayTable(t, x, N.z, nx * N.x + ny * N.y, ny * N.x - nx * N.y, g);
    if (o.hit.order < 0 || !light) return o;
    const double mu = emissionCos(o.hit.g, o.hit.b * o.hit.b - o.hit.Lz * o.hit.Lz, o.hit.r);   // 0.87.0 R10: spin 0, q = b^2 - Lz^2 (disc.h)
    discLightSeen(s, o.hit.r, o.hit.g, o.hit.b, mu, o);                 // 0.84.0: with the camera's own shift (spin 0: one factor for all rays)
    return o;
}

DiscLook discLook(const DiscTable& t, const Settings& s, const HoleView& v, double W, double H, int step) {
    DiscLook L;
    // 0.55.6: the rows on all cores (only largest values are kept: the same result in any order); 0.70.0: each sample's Y kept in row order for the
    // log-average (tone.h), which needs the largest first (its delta)
    std::vector<double> rows;
    for (double Y = step / 2.0; Y < H; Y += step) rows.push_back(Y);
    const int nJob = static_cast<int>((rows.size() + 15) / 16);
    std::vector<DiscLook> part(static_cast<std::size_t>(nJob));
    std::vector<std::vector<double>> ys(static_cast<std::size_t>(nJob));
    const DiscGeom g = discGeom(s);
    parallelFor(nJob, [&](int k) {
        DiscLook& P = part[static_cast<std::size_t>(k)];
        for (std::size_t j = static_cast<std::size_t>(k) * 16; j < std::min(rows.size(), static_cast<std::size_t>(k + 1) * 16); j++)
            for (double X = step / 2.0; X < W; X += step) {
                const DiscPixel q = discPixelG(t, s, g, v, X, rows[j]);
                if (q.hit.order < 0) continue;
                P.any = true;
                P.brightY = std::max(P.brightY, q.Y);
                P.brightMax = std::max({P.brightMax, q.rgb[0], q.rgb[1], q.rgb[2]});
                ys[static_cast<std::size_t>(k)].push_back(q.Y);
            }
    });
    for (const DiscLook& P : part) { L.any = L.any || P.any; L.brightY = std::max(L.brightY, P.brightY); L.brightMax = std::max(L.brightMax, P.brightMax); }
    if (L.brightY > 0) {
        const double delta = kToneDelta * L.brightY;
        double sum = 0;
        std::size_t n = 0;
        for (const std::vector<double>& y : ys) { for (double q : y) sum += std::log(delta + q); n += y.size(); }
        L.logAvg = std::exp(sum / static_cast<double>(n));
    }
    if (!finishLook(L, s)) L.white = 1;
    return L;
}

bool finishLook(DiscLook& L, const Settings& s) {
    const double stops = exposureStops(s);
    if (s.look == Look::Real) {
        const ToneExposure e = toneExposure(L.logAvg, L.brightY, stops);
        if (!e.ok) return false;
        L.k = e.k; L.lw = e.lw; L.white = 1 / e.k;
        return true;
    }
    if (!(L.brightMax > 0)) return false;
    L.white = L.brightMax / std::exp2(stops);
    L.k = 1 / L.white; L.lw = 0;
    return true;
}

HoleView holeView(const Settings& s, double W, double H, double cx, double cy) {
    HoleView v;
    v.cx = cx; v.cy = cy;
    v.K = (H / 2) / (s.fovDeg * kPi / 360);
    double far = 0;                                                      // the farthest corner from the middle
    for (double x : {0.0, W}) for (double y : {0.0, H}) far = std::max(far, std::hypot(x - cx, y - cy));
    v.K = std::max(v.K, far / kPi);
    v.ca = std::cos(s.yaw); v.sa = std::sin(s.yaw); v.cb = std::cos(s.pitch); v.sb = std::sin(s.pitch);
    return v;
}

bool shadowFillsPicture(const Settings& s, double W, double H, double cx, double cy, bool latest) {
    const HoleView hv = holeView(s, W, H, cx, cy);
    double far = 0;
    for (double x : {0.0, W}) for (double y : {0.0, H}) far = std::max(far, std::hypot(x - cx, y - cy));
    return s.bend && (s.spin > 0 ? readout(s, latest).shadowHighDeg / 2 * kPi / 180 : shadowAngle(s.dist)) * hv.K > far;
}

namespace {
// the spinning hole's shadow on the camera's sky (the closed test along 128 azimuths), kept for the last settings: the readout is asked several times a frame
struct SpinShadow { double a = -1, r = 0, th = 0, wide = 0, high = 0, share = 0; };
SpinShadow makeSpinShadow(double a, double r, double th) {             // (no shared state: 0.67.0 runs it on a worker too)
    SpinShadow c;
    c.a = a; c.r = r; c.th = th;
    const Camera cam{r, th, 0};
    const int n = 128;
    double share = 0;
    for (int k = 0; k < n; k++) share += (1 - std::cos(shadowEdge(a, cam, 2 * kPi * (k + 0.5) / n))) / (2.0 * n);   // the cap under the edge over 4 pi
    c.share = share;
    // chi = 0 along e_theta (up on the picture, holeView), pi/2 along e_phi
    c.high = (shadowEdge(a, cam, 0) + shadowEdge(a, cam, kPi)) * 180 / kPi;
    c.wide = (shadowEdge(a, cam, kPi / 2) + shadowEdge(a, cam, -kPi / 2)) * 180 / kPi;
    return c;
}
const SpinShadow& spinShadow(double a, double r, double th) {
    static SpinShadow c;
    if (!(c.a == a && c.r == r && c.th == th)) c = makeSpinShadow(a, r, th);
    return c;
}
// 0.67.0: the panel's readout while spin, distance or tilt move (blackhole.extreme, all_light: 132 edges of 48 bisection steps, about 4 ms of each moving frame
// on the owner's PC, on the frame's thread): made on a worker, the last finished one shown meanwhile (a few frames behind while dragging; at rest, once the
// worker has caught up, the same numbers as made at once). The first one ever is made at once.
SpinShadow spinShadowLatest(double a, double r, double th) {
    static std::mutex mx;
    std::lock_guard<std::mutex> lk(mx);
    static SpinShadow done;
    static std::future<SpinShadow> job;
    if (job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) done = job.get();
    if (done.a == a && done.r == r && done.th == th) return done;
    static const bool fixedDt = std::getenv("PL_FIXED_DT") && std::atof(std::getenv("PL_FIXED_DT")) > 0;   // (the automatic checks: at once, as before)
    if (done.a < 0 || fixedDt) { done = makeSpinShadow(a, r, th); return done; }
    if (!job.valid()) job = std::async(std::launch::async, [a, r, th] { lowerThisThread(); return makeSpinShadow(a, r, th); });
    return done;
}
}  // namespace

Readout readout(const Settings& s, bool latest) {
    Readout r;
    const double a = s.spin;
    r.spin = a;
    r.massSun = s.massSun;
    r.rgM = gravRadiusM(s.massSun);
    r.horizonM = horizon(a);
    r.horizonKm = r.horizonM * r.rgM / 1000;
    r.horizonAU = r.horizonM * r.rgM / units::kAU;
    r.dist = s.dist;
    r.distKm = s.dist * r.rgM / 1000;
    r.distAU = s.dist * r.rgM / units::kAU;
    if (a == 0) {
        r.shadowDeg = 2 * shadowAngle(s.dist) * 180 / kPi;
        r.shadowWideDeg = r.shadowHighDeg = r.shadowDeg;
        r.skyShare = shadowSkyFraction(s.dist);
        r.clockRate = hoverClockRate(s.dist);
        r.blueshift = hoverBlueshift(s.dist);
        r.brightening = hoverBrightening(s.dist);
    } else {
        const SpinShadow sh = latest ? spinShadowLatest(a, s.dist, s.pitch) : spinShadow(a, s.dist, s.pitch);
        r.shadowWideDeg = sh.wide; r.shadowHighDeg = sh.high;
        r.shadowDeg = 0.5 * (sh.wide + sh.high);
        r.skyShare = sh.share;
        const KerrMetric m = kerrMetric(a, s.dist, s.pitch);
        r.clockRate = m.alpha;                                               // the FIDO's proper time per far-away time: the lapse (J15 (A.1))
        r.blueshift = hoverShift(a, s.dist, s.pitch, 0);                     // (A.16) for light with b = 0 (from the poles' and the hole's direction)
        r.brightening = std::pow(r.blueshift, 4);
        r.omega = m.omega;
        r.turnSeconds = 2 * kPi / m.omega * r.rgM / 299792458.0;             // GM/c^3 = r_g / c
    }
    r.photonOrbitM = photonOrbit(0, true);
    if (const Preset* p = s.preset.empty() ? nullptr : preset(s.preset.c_str())) {
        r.hasPreset = true;
        r.presetName = p->name;
        r.earthShadowMuas = shadowDiameterMuas(*p);
        r.ringMuas = p->ringMuas;
        r.ringErr = p->ringErr;
        r.presetDistPc = p->distPc;
        r.realPreset = true;
        if (a > 0) {                                                         // GLb (38)-(40), (55): the far-away edge's extent at inclination = the camera's latitude
            // 0.57.2: kept for the last spin and tilt (the readout is asked several times a frame; 4,001 points each time was 0.1 ms a call here)
            static double ca = -1, cth = 0, camin = 0, camax = 0, cbmax = 0;
            if (ca != a || cth != s.pitch) {
                double amin = 1e9, amax = -1e9, bmax = 0;
                const double rm = criticalRadiusMinus(a), rp = criticalRadiusPlus(a);
                for (int i = 0; i <= 4000; i++) {
                    double al, b2;
                    if (!criticalScreen(a, rm + (rp - rm) * i / 4000.0, s.pitch, al, b2)) continue;
                    amin = std::min(amin, al); amax = std::max(amax, al); bmax = std::max(bmax, std::sqrt(b2));
                }
                ca = a; cth = s.pitch; camin = amin; camax = amax; cbmax = bmax;
            }
            const double amin = camin, amax = camax, bmax = cbmax;
            const double perM = r.earthShadowMuas / (2 * std::sqrt(27.0));   // microarcseconds per GM/c^2 at the preset's distance (its a = 0 shadow is 2 sqrt(27) of them, GLb (71))
            r.earthWideMuas = (amax - amin) * perM; r.earthHighMuas = 2 * bmax * perM;
        }
    }
    if (s.preset == kFeedingKey) { r.hasPreset = true; r.presetName = kFeedingName; }
    r.disc = s.disc;
    r.feed = s.feed;
    const DiscPhys dp = discPhys(s);
    r.rIn = discInner(a);
    r.rInKm = r.rIn * r.rgM / 1000;
    r.rOut = s.outer;
    r.tHot = dp.hottest(&r.rHot);
    r.rHotKm = r.rHot * r.rgM / 1000;
    r.hotVsSun = visibleVsSun(r.tHot);
    r.fcolHot = s.look == Look::Real && discLightRules().fcol ? colourCorrection(r.tHot) : 1;   // 0.87.0
    if (dp.ret) r.retShare = dp.ret->fluxAt(r.rHot) / std::max(discFlux(r.rHot, s.spin), 1e-300);
    r.efficiency = discEfficiency(a);
    r.lumW = s.feed * eddingtonLuminosity(s.massSun);
    r.look = s.look;
    const DiscGeom dg = discGeom(s);
    r.discEdgeIn = dg.rIn; r.discEdgeOut = dg.rOut;
    if (s.look == Look::Film) r.filmT = lookRule(Look::Film).discT;
    return r;
}

}  // namespace blackhole
