#include "blackhole/lab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "blackhole/kerr.h"
#include "blackhole/presets.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"
#include "orbit/sky.h"
#include "orbit/units.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kPitchMin = 0.01, kPitchMax = kPi - 0.01;    // (straight up or down the view's roll would be undefined)

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
           disc == o.disc && feed == o.feed && outer == o.outer && expAuto == o.expAuto && expLog == o.expLog && spin == o.spin && look == o.look;
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
    s.pitch = std::atan2(sb, cb);
    s.yaw = std::atan2(-g.x / sb, -g.y / sb);
}

bool applyPreset(Settings& s, const std::string& key) {
    if (key == kFeedingKey) {
        s.preset = kFeedingKey;
        s.massSun = kFeedingMass;
        s.disc = true;
        s.spin = 0;                                                     // (no invented spin: the model's start, as the real holes')
        return true;
    }
    const Preset* p = preset(key.c_str());
    if (!p) return false;
    s.preset = p->key;
    s.massSun = p->massSun;
    s.disc = false;                                                     // (owner's decision of 15 Nov 2026: the real holes open with no disc)
    s.spin = 0;                                                         // (owner's decision of 15 Nov 2026: spin not measured; it starts at 0)
    return true;
}

void setMass(Settings& s, double m) {
    s.massSun = std::clamp(m, kMassMin, kMassMax);
    if (s.preset == kFeedingKey) { if (s.massSun != kFeedingMass) s.preset.clear(); return; }
    const Preset* p = s.preset.empty() ? nullptr : preset(s.preset.c_str());
    if (!p || p->massSun != s.massSun) s.preset.clear();
}
void setFeed(Settings& s, double l) { s.feed = std::clamp(l, kThinMin, kThinMax); }
void setOuter(Settings& s, double r) { s.outer = std::clamp(r, kOuterMin, kOuterMax); }
void setExposure(Settings& s, double e) { s.expLog = std::clamp(e, kExpLogMin, kExpLogMax); s.expAuto = false; }
void setSpin(Settings& s, double a) { s.spin = std::clamp(a, 0.0, kSpinTop); }
void setLook(Settings& s, Look l) { s.look = l; }
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
}
void setDistance(Settings& s, double d) { s.dist = std::clamp(d, kDistMin, kDistMax); }
void setFov(Settings& s, double f) { s.fovDeg = std::clamp(f, kFovMin, kFovMax); }
void turn(Settings& s, double dy, double dp) {
    s.yaw = std::remainder(s.yaw + dy, 2 * kPi);
    s.pitch = std::clamp(s.pitch + dp, kPitchMin, kPitchMax);
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
    out["exposure"] = s.expAuto ? "auto" : text(s.expLog);
    out["spin"] = text(s.spin);
    out["look"] = lookKey(s.look);
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
    else if (num(in, "exposure", kExpLogMin, kExpLogMax, v)) { s.expAuto = false; s.expLog = v; }
    if (num(in, "spin", 0, kSpinTop, v)) s.spin = v;
    if (const auto it = in.find("look"); it != in.end()) { Look l; if (lookFromKey(it->second, l)) s.look = l; }   // (a file without the line: Real)
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

DiscPhys discPhys(const Settings& s) { DiscPhys p; p.massSun = s.massSun; p.lEdd = s.feed; p.aSigned = s.spin; return p; }
DiscGeom discGeom(const Settings& s) {
    DiscGeom g;
    if (s.look == Look::Film) { const LookRule r = lookRule(Look::Film); g.rIn = r.rIn; g.rOut = r.rOut; return g; }   // J15 Figure 13's caption (looks.h)
    g.rIn = discInner(s.spin); g.rOut = s.outer;
    return g;
}
void discLight(const Settings& s, double r, double g, DiscPixel& o) {
    if (s.look == Look::Film) {                                         // one temperature, no shift, the film's white balance (looks.h)
        o.T = o.Tobs = lookRule(Look::Film).discT;
        o.rgb = filmDiscLinear();
        o.Y = filmDiscY();
        return;
    }
    const DiscPhys p = discPhys(s);
    o.T = p.temperature(r);
    o.Tobs = g * o.T;                                                   // Riazuelo (53)
    o.rgb = blackBodyLinearFast(o.Tobs);
    o.Y = visibleVsSunFast(o.Tobs);
}
Vec3 discAxisView(const HoleView& v) { return Vec3{0, -v.sb, v.cb}; }

DiscPixel discPixel(const DiscTable& t, const Settings& s, const HoleView& v, double X, double Y, bool light) {
    DiscPixel o;
    const double dx = X - v.cx, dy = v.cy - Y, d = std::hypot(dx, dy);
    const double th = d / v.K;
    if (!(th <= kPi)) return o;
    const double x = th - t.alphaSh();
    const double nx = d > 0 ? dx / d : 0, ny = d > 0 ? dy / d : 0;
    const Vec3 N = discAxisView(v);
    o.hit = discRayTable(t, x, N.z, nx * N.x + ny * N.y, ny * N.x - nx * N.y, discGeom(s));
    if (o.hit.order < 0 || !light) return o;
    discLight(s, o.hit.r, o.hit.g, o);
    return o;
}

DiscLook discLook(const DiscTable& t, const Settings& s, const HoleView& v, double W, double H, int step) {
    DiscLook L;
    for (double Y = step / 2.0; Y < H; Y += step)
        for (double X = step / 2.0; X < W; X += step) {
            const DiscPixel q = discPixel(t, s, v, X, Y);
            if (q.hit.order < 0) continue;
            L.any = true;
            L.brightY = std::max(L.brightY, q.Y);
            L.brightMax = std::max({L.brightMax, q.rgb[0], q.rgb[1], q.rgb[2]});
        }
    L.white = s.expAuto ? (L.brightMax > 0 ? L.brightMax : 1) : std::pow(10.0, s.expLog);
    return L;
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

namespace {
// the spinning hole's shadow on the camera's sky (the closed test along 128 azimuths), kept for the last settings: the readout is asked several times a frame
struct SpinShadow { double a = -1, r = 0, th = 0, wide = 0, high = 0, share = 0; };
const SpinShadow& spinShadow(double a, double r, double th) {
    static SpinShadow c;
    if (c.a == a && c.r == r && c.th == th) return c;
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
}  // namespace

Readout readout(const Settings& s) {
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
        const SpinShadow& sh = spinShadow(a, s.dist, s.pitch);
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
            double amin = 1e9, amax = -1e9, bmax = 0;
            const double rm = criticalRadiusMinus(a), rp = criticalRadiusPlus(a);
            for (int i = 0; i <= 4000; i++) {
                double al, b2;
                if (!criticalScreen(a, rm + (rp - rm) * i / 4000.0, s.pitch, al, b2)) continue;
                amin = std::min(amin, al); amax = std::max(amax, al); bmax = std::max(bmax, std::sqrt(b2));
            }
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
    r.efficiency = discEfficiency(a);
    r.lumW = s.feed * eddingtonLuminosity(s.massSun);
    r.look = s.look;
    const DiscGeom dg = discGeom(s);
    r.discEdgeIn = dg.rIn; r.discEdgeOut = dg.rOut;
    if (s.look == Look::Film) r.filmT = lookRule(Look::Film).discT;
    return r;
}

}  // namespace blackhole
