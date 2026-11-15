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

#include "blackhole/schwarzschild.h"

namespace blackhole {

constexpr int kLabIndex = 3;               // AppState::lab (0.55.0: Orbit 0, Atom 1, Molecule 2, Black Hole 3; app::labName)

// The camera's distance, in GM/c^2. The closest is the closest distance the module's closed forms are tested at against the ray integrator
// (tests/test_blackhole.cpp testIntegratorVsClosed: 2.2 M); the farthest is where the shadow is 0.6 degrees across.
constexpr double kDistMin = 2.2, kDistMax = 1000, kDistDefault = 20;
constexpr double kFovMin = 10, kFovMax = 360, kFovDefault = 90;     // degrees across the picture's height (above 180: a fisheye; the corners reach at most pi from the middle)
constexpr double kMassMin = 1, kMassMax = 1e11;                      // Suns: the range of the slider (the picture does not depend on it)

struct Settings {
    std::string preset = "sgra";           // "sgra", "m87", or "" (the user's own mass)
    double massSun = 0;                    // set from the preset when the lab is made (settingsDefaults)
    double dist = kDistDefault;            // GM/c^2
    double fovDeg = kFovDefault;
    double yaw = 0, pitch = 0;             // the view (orbit/sky.h skyScreen's angles), radians
    bool bend = true;                      // gravity bends light (off: the plain sky, to compare)
    bool skyOurs = true;                   // the sky behind: ours (the Gaia map and the catalogue stars) or black
    bool operator==(const Settings& o) const;
};
Settings settingsDefaults();               // Sagittarius A*, 20 M, 90 degrees, looking at the galactic centre, bending on, our sky
void resetView(Settings& s);               // distance, field of view and turn back to the defaults (mass, preset, bending and sky stay)
bool applyPreset(Settings& s, const std::string& key);   // mass from presets.h; false if unknown
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
};
Readout readout(const Settings& s);

}  // namespace blackhole
