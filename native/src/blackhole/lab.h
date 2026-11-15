// Black Hole Lab (0.55.0): the lab's settings, its camera and its readout, as plain data and pure functions (no graphics; blackhole_view.cpp draws,
// ui/shell_blackhole.cpp has the panel). Every number the readout shows comes from the physics module (kerr.h, schwarzschild.h, presets.h); nothing
// here is a formula of its own except unit changes (metres to km and AU, radians to degrees).
//
// The camera hovers (a static observer, J15 (A.3) at a = 0) at a distance in units of the hole's own length GM/c^2 and always looks at the hole, which
// stays in the middle of the picture. Turning moves the camera round the hole: the sky behind it changes, the hole stays put (yaw and pitch of the
// night sky's projection, orbit/sky.h skyScreen). The field of view is the angle across the picture's height.
// Settings file: the prefix "blackhole." (app/prefs.h), one key each; a bad value keeps the default.
#pragma once

#include <map>
#include <string>

#include "blackhole/disc.h"
#include "blackhole/schwarzschild.h"

namespace blackhole {

constexpr int kLabIndex = 3;               // AppState::lab (0.55.0: Orbit 0, Atom 1, Molecule 2, Black Hole 3; app::labName)

// The camera's distance, in GM/c^2. The closest is the closest distance the module's closed forms are tested at against the ray integrator
// (tests/test_blackhole.cpp testIntegratorVsClosed: 2.2 M); the farthest is where the shadow is 0.6 degrees across.
constexpr double kDistMin = 2.2, kDistMax = 1000, kDistDefault = 20;
constexpr double kFovMin = 10, kFovMax = 360, kFovDefault = 90;     // degrees across the picture's height (above 180: a fisheye; the corners reach at most pi from the middle)
constexpr double kMassMin = 1, kMassMax = 1e11;                      // Suns: the range of the slider (the picture does not depend on it)

// 0.55.1, the gas disc (disc.h). The feeding rate as L / L_Edd, inside the range where the thin-disc model applies (kThinMin..kThinMax, disc.h: YN 4.2.1, AF 6);
// its default 0.1 L_Edd is the plan's (section 2: inside YN's range). The outer edge in GM/c^2: the model leaves it free (PT section II); the default 30 is where
// L79 p. 233 says most of the light comes from inside of ("between the inner edge r = 6M and r ~ 30M ... the 'living core'"). The exposure: the screen's white as
// a multiple of the Sun's surface in visible light (log10), automatic = the brightest part of the disc in the picture is drawn at full brightness.
// The third preset, "A feeding black hole (model)": the disc on and 10^8 Suns, inside AF section 1's quasar range (10^6 to 10^9 Suns) where YN section 1 says
// luminous AGNs are modelled as thin discs; of AF's two ranges (about 10 Suns, and quasars) the quasar's gives the cooler disc (about 7 x 10^4 K at 0.1 L_Edd),
// whose visible light is the larger share of its light.
constexpr double kFeedDefault = 0.1;
constexpr double kOuterMin = 7, kOuterMax = 1000, kOuterDefault = 30;
constexpr double kExpLogMin = -1, kExpLogMax = 5, kExpLogDefault = 2;
constexpr double kFeedingMass = 1e8;
constexpr const char* kFeedingKey = "feeding";
constexpr const char* kFeedingName = "A feeding black hole (model)";
// The disc's plane: a chosen reference plane, the plane of the Earth's orbit (the ecliptic) of the sky behind: its axis is the sky frame's z (orbit/sky.h);
// the gas goes round anticlockwise seen from the north (as the planets do). Neither is measured: the guide says so.

struct Settings {
    std::string preset = "sgra";           // "sgra", "m87", "feeding", or "" (the user's own mass)
    double massSun = 0;                    // set from the preset when the lab is made (settingsDefaults)
    double dist = kDistDefault;            // GM/c^2
    double fovDeg = kFovDefault;
    double yaw = 0, pitch = 0;             // the view (orbit/sky.h skyScreen's angles), radians
    bool bend = true;                      // gravity bends light (off: the plain sky, to compare)
    bool skyOurs = true;                   // the sky behind: ours (the Gaia map and the catalogue stars) or black
    bool disc = false;                     // 0.55.1: the gas disc (model)
    double feed = kFeedDefault;            // L / L_Edd
    double outer = kOuterDefault;          // the disc's outer edge, GM/c^2
    bool expAuto = true;                   // exposure: automatic, or expLog
    double expLog = kExpLogDefault;        // log10 of the screen's white in units of the Sun's surface (visible light)
    double spin = 0;                       // 0.55.2: a / M, 0 .. kSpinTop (spin_sky.h); 0 is exactly the non-spinning hole of 0.55.1 (its closed forms)
    bool operator==(const Settings& o) const;
};
Settings settingsDefaults();               // Sagittarius A*, 20 M, 90 degrees, looking at the galactic centre, bending on, our sky
void resetView(Settings& s);               // distance, field of view and turn back to the defaults (mass, preset, bending and sky stay)
bool applyPreset(Settings& s, const std::string& key);   // mass from presets.h, the disc off; "feeding": kFeedingMass, the disc on; false if unknown
void setFeed(Settings& s, double lEdd);    // clamps to kThinMin..kThinMax
void setOuter(Settings& s, double r);      // clamps to kOuterMin..kOuterMax
void setExposure(Settings& s, double expLog);   // manual: clamps, expAuto off
void setSpin(Settings& s, double a);       // 0.55.2: clamps to 0 .. kSpinTop (the presets set 0: the real holes' spin is not measured)
void setMass(Settings& s, double massSun); // clamps; the preset is dropped unless the mass is the preset's own
void setDistance(Settings& s, double dist);
void setFov(Settings& s, double fovDeg);
void turn(Settings& s, double dYaw, double dPitch);       // the pitch stays inside (0, pi)
void saveSettings(const Settings& s, std::map<std::string, std::string>& out);
Settings loadSettings(const std::map<std::string, std::string>& in);    // unknown keys and bad values are ignored

// The log sliders: position 0..1 <-> value
double distToSlider(double dist);
double sliderToDist(double t);
double massToSlider(double massSun);
double sliderToMass(double t);
double feedToSlider(double lEdd);
double sliderToFeed(double t);
double outerToSlider(double r);
double sliderToOuter(double t);

// 0.55.1: the disc's numbers for a picture (from disc.h): the physical disc of these settings and its edges (0.55.2: at the settings' spin, the gas orbiting with it)
DiscPhys discPhys(const Settings& s);
DiscGeom discGeom(const Settings& s);
// the disc's axis on the view frame (right, up, towards the camera) of a view with these yaw and pitch: the sky frame's z, (0, -sin pitch, cos pitch)
Vec3 discAxisView(const HoleView& v);
// One pixel of the picture through the disc's table (as the card draws it, in double): x is the pixel's angle from the shadow's edge, (dx, dy) the unit direction
// from the middle (right, up). The hit, its observed temperature and its colour (linear sRGB in units of the Sun's surface luminance).
struct DiscPixel { DiscHit hit; double T = 0, Tobs = 0; std::array<double, 3> rgb{0, 0, 0}; double Y = 0; };
DiscPixel discPixel(const DiscTable& t, const Settings& s, const HoleView& v, double X, double Y, bool light = true);   // light false: the hit only
// The picture's brightest part of the disc (sampled every `step` pixels): its visible brightness Y and its largest linear sRGB channel, both in units of the Sun's surface
struct DiscLook { double brightY = 0, brightMax = 0, white = 1; bool any = false; };
DiscLook discLook(const DiscTable& t, const Settings& s, const HoleView& v, double W, double H, int step = 4);   // (the light from disc.h's tables: *Fast)

// The view for a picture of W x H pixels with the hole at (cx, cy): K from the field of view (at most so that every corner is within pi of the middle)
HoleView holeView(const Settings& s, double W, double H, double cx, double cy);

// What the readout shows. All from the module: gravRadiusM (presets.h), horizon (kerr.h, BPT (2.6)), shadowAngle (Synge, Perlick & Tsupko (7)),
// hoverClockRate, hoverBlueshift, hoverBrightening, shadowSkyFraction (schwarzschild.h), shadowDiameterMuas and the measured rings (presets.h).
struct Readout {
    double massSun = 0, rgM = 0;               // the mass and its GM/c^2 in metres
    double horizonM = 0;                       // GM/c^2
    double horizonKm = 0, horizonAU = 0;
    double dist = 0, distKm = 0, distAU = 0;   // the camera's distance
    double shadowDeg = 0;                      // the shadow's diameter on the camera's sky
    double skyShare = 0;                       // the share of the whole sky it covers
    double clockRate = 0;                      // the camera's clock against one far away (1 hour far away = clockRate hours here)
    double blueshift = 0, brightening = 0;     // the starlight's frequency and brightness factors (not drawn)
    double photonOrbitM = 0;                   // 3 M (kerr.h photonOrbit, a = 0): inside it the shadow covers more than half the sky
    bool hasPreset = false;
    std::string presetName;
    double earthShadowMuas = 0, ringMuas = 0, ringErr = 0, presetDistPc = 0;
    // 0.55.1, the disc (when on): its inner edge (r_ms; nothing shines inside it), the hottest ring (in the gas's own frame) and where, the efficiency,
    // the light in watts (feed x L_Edd), and the hottest ring's visible brightness against the Sun's surface
    bool disc = false, realPreset = false;     // realPreset: Sagittarius A* or M87*, which have no such disc
    double rIn = 0, rInKm = 0, rOut = 0, tHot = 0, rHot = 0, rHotKm = 0, efficiency = 0, lumW = 0, feed = 0, hotVsSun = 0;
    // 0.55.2, spin: the spin; the shadow's width (left-right) and height (up-down) on the camera's sky by the closed test (spin.h shadowEdge) and the share of the
    // sky inside its edge; the camera (a FIDO, J15 (A.3)) is carried round the axis by the dragged space at omega (J15 (A.2)): one turn in dragDays days of
    // far-away time (2 pi / omega, in GM/c^3 times the mass's GM/c^3); the disc's inner edge, efficiency and hottest ring above are at this spin.
    double spin = 0, shadowWideDeg = 0, shadowHighDeg = 0, omega = 0, turnSeconds = 0;
    // a real hole's shadow seen from Earth at this spin, the spin axis at the camera's latitude to the line of sight: GLb's critical curve (55) across and up-down
    // (in GM/c^2) times the preset's GM/c^2 over its distance, in microarcseconds
    double earthWideMuas = 0, earthHighMuas = 0;
};
Readout readout(const Settings& s);

}  // namespace blackhole
