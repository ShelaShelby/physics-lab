// Black Hole Lab (0.55.0): the lab's settings, its camera and its readout, as plain data and pure functions (no graphics; blackhole_view.cpp draws,
// ui/shell_blackhole.cpp has the panel). Every number the readout shows comes from the physics module (kerr.h, schwarzschild.h, presets.h); nothing
// here is a formula of its own except unit changes (metres to km and AU, radians to degrees).
//
// The camera is held at a fixed distance in units of the hole's own length GM/c^2 (not falling, not orbiting). Without spin it hovers: a static observer.
// Round a spinning hole it is a FIDO (zero angular momentum observer, J15 (A.3)): held at this distance and turned round with the space at the rate omega.
// 0.84.0: its own shift of colour and brightness is drawn (heldCameraShift below). It always looks at the hole, which
// stays in the middle of the picture. Turning moves the camera round the hole: the sky behind it changes, the hole stays put (yaw and pitch of the
// night sky's projection, orbit/sky.h skyScreen). The field of view is the angle across the picture's height.
// Settings file: the prefix "blackhole." (app/prefs.h), one key each; a bad value keeps the default.
#pragma once

#include <map>
#include <string>

#include "blackhole/disc.h"
#include "blackhole/looks.h"
#include "blackhole/schwarzschild.h"
#include "blackhole/tone.h"

namespace blackhole {

constexpr int kLabIndex = 3;               // AppState::lab (0.55.0: Orbit 0, Atom 1, Molecule 2, Black Hole 3; app::labName)

// The camera's distance, in GM/c^2. The closest is the closest distance the module's closed forms are tested at against the ray integrator
// (tests/test_blackhole.cpp testIntegratorVsClosed: 2.2 M); the farthest is where the shadow is 0.6 degrees across.
// 0.70.0: the starting view, 50 GM/c^2 and 45 degrees, 80 degrees from the disc's axis (100 from its north: the disc below, its image over the top of
// the shadow), turned towards the galactic centre: picked from 5 distances x 3 fields of view x 3 tilts rendered (REPORT 0.70.0): the shadow about a
// quarter of the picture's height, the disc and its image over the shadow both in view. Until 0.69: 20 GM/c^2 and 90 degrees, inside the disc.
constexpr double kDistMin = 2.2, kDistMax = 1000, kDistDefault = 50;
// the pitch (the camera's angle from the spin axis) stays 0.01 rad from each pole: straight up or down the view's roll would be undefined
constexpr double kPitchMin = 0.01, kPitchMax = 3.14159265358979323846 - 0.01;
constexpr double kTiltDefaultDeg = 100;    // the camera's angle from the disc's (the sky frame's z) axis
constexpr double kFovMin = 10, kFovMax = 360, kFovDefault = 45;     // degrees across the picture's height (above 180: a fisheye; the corners reach at most pi from the middle)
constexpr double kMassMin = 1, kMassMax = 1e11;                      // Suns: the range of the slider (the picture does not depend on it)

// 0.55.1, the gas disc (disc.h). The feeding rate as L / L_Edd, inside the range where the thin-disc model applies (kThinMin..kThinMax, disc.h: YN 4.2.1, AF 6);
// its default 0.1 L_Edd is the plan's (section 2: inside YN's range). The outer edge in GM/c^2: the model leaves it free (PT section II); the default 30 is where
// L79 p. 233 says most of the light comes from inside of ("between the inner edge r = 6M and r ~ 30M ... the 'living core'"). The exposure: the screen's white as
// a multiple of the Sun's surface in visible light (log10), automatic = the brightest part of the disc in the picture is drawn at full brightness.
// The third preset, "A feeding black hole (model)": the disc on and 10^8 Suns, inside AF section 1's quasar range (10^6 to 10^9 Suns) where YN section 1 says
// luminous AGNs are modelled as thin discs; of AF's two ranges (about 10 Suns, and quasars) the quasar's gives the cooler disc (about 7 x 10^4 K at 0.1 L_Edd),
// whose visible light is the larger share of its light.
constexpr double kFeedDefault = 0.1;
// 0.58.0: the outer edge's default (and the feeding preset's) and top, 2000 GM/c^2 = 1000 Schwarzschild radii: J. Goodman, MNRAS 339, 937 (2003),
// arXiv:astro-ph/0201001 (data/blackhole/arxiv/astro-ph_0201001_goodman2003.extract.txt): its abstract, "thin disks may not exist beyond a thousand Schwarzshild
// radii or so (0.01 parsec)", and section 1: the Toomre Q "falls to unity at a distance r ~ 10^-2 pc from a central black hole of mass 10^8 M_sun accreting at the
// Eddington rate" (beyond it the disc's own gravity breaks it up). Until 0.57 the default was 30 (L79's "living core", above): a cut, not the disc's end.
// The same Page-Thorne flux all the way out: the disc fades by its own physics (T ~ r^-3/4 far out), no fade is added.
constexpr double kOuterMin = 7, kOuterMax = 2000, kOuterDefault = 2000;
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
    bool expAuto = true;                   // exposure: automatic, or expEV
    double expEV = 0;                      // 0.70.0: your choice, in photographic stops brighter (+) or darker (-) than automatic (tone.h kEvMin..kEvMax); back to 0
                                           // when the preset, the look or the disc changes. (Until 0.69: an absolute screen's white, kept across presets.)
    double spin = 0;                       // 0.55.2: a / M, 0 .. kSpinTop (spin_sky.h); 0 is exactly the non-spinning hole of 0.55.1 (its closed forms)
    Look look = Look::Real;                // 0.55.3 (looks.h): Real (the dot) or Film. Film draws with the film's spin and disc (shown(), below); the user's own spin,
                                           // disc, feeding rate and outer edge stay as they were and come back with Real
    // 0.55.4 (light.h), only while the disc is on and the look is Real: the sky beside the disc on the disc's own exposure (trueSky; off: Shown, today's
    // picture) and the eye's glare (on by default; off: 0.55.3's picture)
    bool trueSky = false;
    bool glare = true;
    // 0.58.0, Real only: "Light": What an eye sees (false, the default: the visible light by the CIE observer) or All light (true: each point's brightness the
    // total light of all wavelengths, disc.h allLightLinear; its colour the eye's). All light is not an eye's picture: no glare, the sky beside the disc as Shown.
    bool allLight = false;
    // 0.72.0 (gas.h; PLAN_MOVINGGAS.md; owner's choice of 17 Nov 2026: Real and Film both): the moving gas, while the disc is drawn. gasMoving off: 0.71's still
    // disc. gasRun: Pause / play. gasRate: seconds of the far clock per second on the screen (1: real time); gasRateAuto: set so that one turn of the innermost
    // gas takes kGasAutoSeconds on the screen at the hole shown (the start; any choice of the rate ends it, a preset starts it again). gasClump, gasPattern:
    // the clump (BL06) and the disc's streaks (0.79.0, gas.h; 0.72-0.78: SKH06 patches), both labelled models; gasClumpR: the clump's orbit (GM/c^2, at least the disc's inner edge)
    bool gasMoving = true, gasRun = true, gasRateAuto = true, gasClump = true, gasPattern = true;
    double gasRate = 1, gasClumpR = 8;
    bool operator==(const Settings& o) const;
};
Settings settingsDefaults();               // Sagittarius A*, the starting view (kDistDefault, kFovDefault, kTiltDefaultDeg, towards the galactic centre), bending on, our sky
void resetView(Settings& s);               // distance, field of view and turn back to the defaults (mass, preset, bending and sky stay)
bool applyPreset(Settings& s, const std::string& key);   // mass from presets.h, the disc off; "feeding": kFeedingMass, the disc on, the starting view (0.70.0); false if unknown
void setFeed(Settings& s, double lEdd);    // clamps to kThinMin..kThinMax
void setOuter(Settings& s, double r);      // clamps to kOuterMin..kOuterMax
void setExposure(Settings& s, double stops);    // manual (0.70.0: stops from automatic): clamps, expAuto off
double exposureStops(const Settings& s);   // 0 when automatic, else expEV
void setDisc(Settings& s, bool on);        // 0.70.0: the disc on or off; a change sets the manual exposure back to 0 stops
void setSpin(Settings& s, double a);       // 0.55.2: clamps to 0 .. kSpinTop (the presets set 0: the real holes' spin is not measured)
void setMass(Settings& s, double massSun); // clamps; the preset is dropped unless the mass is the preset's own
// 0.72.0: the moving gas's rate (log slider kGasRateMin .. kGasRateMax: from 10^-6 of real time, for a 1-Sun hole whose innermost orbit is a tenth of a
// millisecond, to 10^7, for 10^11 Suns whose takes a year); gasRateOf: the rate in use (the automatic one: one innermost turn in kGasAutoSeconds)
constexpr double kGasRateMin = 1e-6, kGasRateMax = 1e7, kGasAutoSeconds = 20, kGasClumpRMax = 30;
double gasRateOf(const Settings& s);
void setGasRate(Settings& s, double rate); // clamps; the automatic rate ends
double gasRateToSlider(double rate);
double sliderToGasRate(double t);
double gasClumpRadius(const Settings& s);  // gasClumpR, at least the shown disc's inner edge and at most its outer
bool gasOn(const Settings& s);             // the moving gas is drawn: the shown disc on, bending on, gasMoving, the clump or the patches
void gasTick(const Settings& s, double dt, double& T);   // T (GM/c^3) on by dt seconds of the screen at gasRateOf while gasOn and gasRun
// 0.55.3, the looks (looks.h). shown: the settings the picture and the readout use: s itself for Real; for Film the film's spin and the disc on (J15, looks.h).
// paperView: the camera where J15's Figure 13 has it (74.1 M, theta 1.511 rad from the spin axis; the turn round the axis and the field of view stay).
void setLook(Settings& s, Look l);
Settings shown(const Settings& s);
// 0.55.4: whether the true exposure and the eye's glare are drawn: Real, the disc on and bending on (the true sky also needs our sky)
bool trueSkyOn(const Settings& s);
bool glareOn(const Settings& s);
bool allLightOn(const Settings& s);        // 0.58.0: Real, the disc on and allLight
void paperView(Settings& s);
void setDistance(Settings& s, double dist);
void setFov(Settings& s, double fovDeg);
void turn(Settings& s, double dYaw, double dPitch);       // the pitch stays inside [kPitchMin, kPitchMax] (0.94.1: and outside the disc's thickness, below)
// 0.94.1 (the owner's decision, 27 Nov 2026, DECISIONS.md): THE CAMERA STAYS OUTSIDE THE DISC'S THICKNESS. The gas is opaque and the lab has no model of the view
// from inside it. Where the shown disc is (on, and the camera's distance from the axis R = dist sin(pitch) between its inner and outer edges), the camera's
// height |z| = |dist cos(pitch)| is kept at least h(R) + kCamDiscMargin, on the side it is on (or came from). h: the calculated thickness (disc.h
// DiscPhys::height, every spin, Real) whether or not the picture draws it; Film's disc has none (J15's thin disc), so there only the margin.
// Crossing the plane beyond the outer edge or inside the inner edge stays allowed. side: +1 above the plane (pitch < pi/2), -1 below, 0 the side it is on
// now (exactly in the plane: above). camDiscPitch: the pitch nearest the plane allowed on that side at this distance (pi/2 where nothing stops the camera).
constexpr double kCamDiscMargin = 0.1;                      // GM/c^2: a stated margin above the surface (about a quarter of h at 20 GM/c^2 in the feeding preset)
double camDiscPitch(const Settings& s, double dist, int side);
bool keepOutsideDisc(Settings& s, int side = 0);           // true when the pitch was moved
bool cameraAtDisc(const Settings& s);                       // the camera is on that limit (the Camera section says why it stops)
void saveSettings(const Settings& s, std::map<std::string, std::string>& out);

// 0.82.0: the camera handles as the Orbit Lab's (orbit/camera_motion.h holds the shared numbers). A drag circles the camera round the hole at
// orbit::kTurnPerPoint rad a point, the same way as the Orbit Lab's for the same drag: right, the camera goes to its left; down, the camera goes down
// (orbit_view.cpp puts its camera at (sin yaw sin pitch, cos yaw sin pitch, cos pitch) with its picture turned half a turn from this lab's, so its yaw
// and pitch go up where these go down). On release the turn keeps its speed, fading (none with Reduce motion); a press stops it. The wheel and + -
// move the camera in eased steps (orbit::kWheelZoomSeconds, eased in ln distance) that end exactly where 0.81.0's 15 % jump ended.
// The camera still hovers at each place it is put (J15 (A.3)): this is how the view is moved, not a moving camera. Not saved.
struct CameraMotion {
    bool pressed = false;                  // a press on the picture is held
    double vYaw = 0, vPitch = 0;           // the turn's speed (rad/s): while pressed the drag's, after release the coasting one
    double turnYaw = 0, turnPitch = 0;     // the turn the drag made since the last tick
    bool turned = false;
    double heldYaw = 0, heldPitch = 0;     // the view after the last tick (a change by someone else stops the coasting)
    bool zoomOn = false;                   // an eased step of the distance
    double zoomFrom = 0, zoomTo = 0, zoomS = 0, zoomLast = 0;   // GM/c^2 (zoomLast: the distance the step set last; a change by someone else stops it)
};
constexpr double kNotch = 1.15;            // a wheel notch or a key: 15 % of the distance (0.55.0)
void dragTurn(Settings& s, CameraMotion& m, double dx, double dy);   // a drag of dx, dy points (y down)
void grab(CameraMotion& m);                // a press on the picture: a coasting turn stops
void release(CameraMotion& m, bool reduceMotion);   // the press ends
void stepDistance(Settings& s, CameraMotion& m, double notches, bool reduceMotion);   // + closer, - farther; at once with Reduce motion
void stopMotion(CameraMotion& m);          // Reset view, Escape, the benchmark: no coasting, no eased step (the view stays where it is)
void cameraTick(Settings& s, CameraMotion& m, double dt, bool reduceMotion);
bool cameraMoving(const CameraMotion& m);  // coasting or in an eased step
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

// 0.55.1: the disc's numbers for a picture (from disc.h): the physical disc of these settings and its edges (0.55.2: at the settings' spin, the gas orbiting with it;
// 0.55.3: in the Film look the film's edges, looks.h)
DiscPhys discPhys(const Settings& s);
// 0.87.0 R11: the returning flux the disc is drawn with (returning.h): the Real look, the rule on and the spin's table made (else null: none yet)
const ReturnFlux* discReturn(const Settings& s);
DiscGeom discGeom(const Settings& s);
// 0.55.3: the light of the disc at r for a ray with shift g, by the look's rule (looks.h lookRule): Real: the thin disc's T(r) seen at g T (Riazuelo (53)),
// true colour; Film: one temperature, no shift, white-balanced (filmDiscLinear). T, Tobs, rgb, Y of a DiscPixel (hit untouched).

// the disc's axis on the view frame (right, up, towards the camera) of a view with these yaw and pitch: the sky frame's z, (0, -sin pitch, cos pitch)
Vec3 discAxisView(const HoleView& v);
// One pixel of the picture through the disc's table (as the card draws it, in double): x is the pixel's angle from the shadow's edge, (dx, dy) the unit direction
// from the middle (right, up). The hit, its observed temperature and its colour (linear sRGB in units of the Sun's surface luminance).
struct DiscPixel { DiscHit hit; double T = 0, Tobs = 0; std::array<double, 3> rgb{0, 0, 0}; double Y = 0; double fcol = 1, mu = -1, limb = 1; };   // 0.87.0: fcol, mu, limb (discLight)
DiscPixel discPixel(const DiscTable& t, const Settings& s, const HoleView& v, double X, double Y, bool light = true, bool plane = false);   // light false: the hit only
// 0.79.0: the same with the disc's geometry given (discGeom(s), made once for many pixels); plane above: the crossing of the plane even where the surface is drawn
DiscPixel discPixelG(const DiscTable& t, const Settings& s, const DiscGeom& g, const HoleView& v, double X, double Y, bool light = true);
// 0.79.0: the disc's thickness table (disc.h heightTable) for these settings: where discThickDrawn; null otherwise. Kept for the last 64 discs.
const HeightTable* discHeight(const Settings& s);
// 0.86.0: the disc is drawn at its calculated thickness: the Real look (0.95.0: at every spin, kerr_closed.h "THE DISC'S SURFACE"; 0.86.0 to 0.94: spin 0
// only). Film: in its plane (J15's thin disc).
bool discThickDrawn(const Settings& s);
// 0.86.0: the words that say which shape the disc is drawn with, so they follow the picture (tests/test_blackhole.cpp testDiscWords):
//   discShapeWords: the disc's shape in "What is real in this picture" (Real look); discEdgeOnWords: the caption when the camera is exactly in the disc's plane;
//   discThicknessWords: the Disc section's caption under the thickness numbers (Real look).
std::string discShapeWords(const Settings& s);
std::string discEdgeOnWords(const Settings& s);
std::string discThicknessWords(const Settings& s);
// 0.55.3: the light of the disc at r for a ray with shift g, by the look's rule (looks.h lookRule): Real: the thin disc's T(r) seen at g T (Riazuelo (53)),
// true colour; Film: one temperature, no shift, white-balanced (filmDiscLinear). Sets T, Tobs, rgb and Y (the hit is left as it is).
// 0.87.0 (disc.h, THE DISC'S REAL LIGHT): Real: T(r) includes the returning light (R11), the colour is f^-4 of a black body at g f T (R9: Tobs = g f T, the
// colour temperature seen; T stays the effective temperature), times the limb-darkening factor at mu (R10). mu: the cosine of the angle from the disc's
// normal at which the gas sends this ray, in the gas's own frame (disc.h emissionCos); mu < 0: no direction (numbers that are not a pixel's): factor 1.
void discLight(const Settings& s, double r, double g, double mu, DiscPixel& o);
inline void discLight(const Settings& s, double r, double g, DiscPixel& o) { discLight(s, r, g, -1.0, o); }
// 0.87.0: the same from the gas's own effective temperature T (K) in place of T(r) (the moving gas: T x its factor^(1/4)); Real only
void discLightT(const Settings& s, double T, double g, double mu, DiscPixel& o);
// 0.84.0: THE CAMERA'S OWN SHIFT, drawn. The lab's camera is held at a fixed distance (not falling, not orbiting). Without spin it is a static observer;
// round a spinning hole it is a FIDO (J15 (A.3)): held at this distance and carried round by the turning space at the rate omega. A ray with axial angular
// momentum b per unit energy (p_t = -1) reaches it at f_c/f' = (1 - b omega) / alpha over the frequency sent at rest far away (J15 (A.16) with beta = 0;
// kerr.h hoverShift). At spin 0 that is (1 - 2M/r)^(-1/2) for every ray; with spin it changes across the picture with each ray's b. Real only: Film is 1
// (J15 draws no colour or brightness shifts, looks.h). Exact for that camera.
double heldCameraShift(const Settings& s, double b);
// the disc's light as the camera sees it: discLight at g x heldCameraShift(s, b), g against far away (Riazuelo (53): the colour temperature g f T; a black
// body stays a black body, so its whole light goes up by f^4 and the eye's light as the black body's at that temperature, R19 (52))
void discLightSeen(const Settings& s, double r, double g, double b, double mu, DiscPixel& o);
inline void discLightSeen(const Settings& s, double r, double g, double b, DiscPixel& o) { discLightSeen(s, r, g, b, -1.0, o); }
// 0.87.0 R10: the emission angle's cosine (disc.h emissionCos) round a spinning hole, for the ray the held camera sees at th from the hole's direction and
// azimuth chi (spin.h lookAt): its Carter constant q by KerrBL::launch at the camera (J15 (A.12); the shaders do the same in single precision, kKerrFS
// emitCosK, kKerrSkyFS from traceRay's q); g the disc's own shift (without the camera's), r the crossing's radius
double spinEmissionCos(const Settings& s, double th, double chi, double g, double r);
// a catalogue star seen by the camera at factor f: its dot's colour (sRGB 0-255, largest channel 255 as the catalogue's colours) is that of a black body at
// f x its colour temperature T (the catalogue colour times the black body's ratio at f T over T, channel by channel, in linear light); its V light by the
// black body's eye's light at f T over T (vRatio; dmag = -2.5 log10 vRatio); its whole light by f^4 (boloRatio). f = 1: the catalogue's dot, unchanged.
struct StarSeen { std::array<int, 3> rgb{0, 0, 0}; double vRatio = 1, boloRatio = 1, dmag = 0; };
StarSeen starSeen(const std::array<int, 3>& rgb, double T, double f);
// The picture's brightest part of the disc (sampled every `step` pixels): its visible brightness Y and its largest linear sRGB channel, both in units of the Sun's surface
// (0.58.0: in All light (allLightOn) both are the total light against the Sun's surface's total light, disc.h bolometricVsSun)
// 0.70.0: logAvg, Reinhard et al.'s log-average of the samples' Y (tone.h, delta = kToneDelta x brightY); k and lw: Real's exposure (L = k Y, the burn-out white
// lw = k brightY; tone.h toneExposure at exposureStops). white: the screen's white in units of the Sun's surface for the faint light, which the curve shows
// linearly (Real: 1 / k, the stars' true exposure, light.h; Film: brightMax over 2^stops, as until 0.69 with automatic).
struct DiscLook { double brightY = 0, brightMax = 0, white = 1; bool any = false; double logAvg = 0, k = 0, lw = 0; };
DiscLook discLook(const DiscTable& t, const Settings& s, const HoleView& v, double W, double H, int step = 4);   // (the light from disc.h's tables: *Fast)
// white, k and lw of L from its brightY, brightMax and logAvg (the card's read-back too); false: nothing to expose by (L keeps white, k and lw)
bool finishLook(DiscLook& L, const Settings& s);

// The view for a picture of W x H pixels with the hole at (cx, cy): K from the field of view (at most so that every corner is within pi of the middle)
HoleView holeView(const Settings& s, double W, double H, double cx, double cy);
// 0.82.0 (moved from the Camera section's caption): the shadow covers the whole W x H picture round (cx, cy): its half-height (spin: readout's, latest as
// there; spin 0: shadowAngle) times holeView's K reaches the farthest corner. Bending off: no shadow.
bool shadowFillsPicture(const Settings& s, double W, double H, double cx, double cy, bool latest = false);

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
    double fcolHot = 1;                      // 0.87.0 R9: the colour-correction factor at the hottest ring (disc.h colourCorrection; 1 when off)
    double retShare = -1;                    // 0.87.0 R11: the returning flux over the disc's own at the hottest ring; -1: its table not made yet (or off)
    // 0.55.2, spin: the spin; the shadow's width (left-right) and height (up-down) on the camera's sky by the closed test (spin.h shadowEdge) and the share of the
    // sky inside its edge; the camera (a FIDO, J15 (A.3)) is carried round the axis by the dragged space at omega (J15 (A.2)): one turn in dragDays days of
    // far-away time (2 pi / omega, in GM/c^3 times the mass's GM/c^3); the disc's inner edge, efficiency and hottest ring above are at this spin.
    double spin = 0, shadowWideDeg = 0, shadowHighDeg = 0, omega = 0, turnSeconds = 0;
    // a real hole's shadow seen from Earth at this spin, the spin axis at the camera's latitude to the line of sight: GLb's critical curve (55) across and up-down
    // (in GM/c^2) times the preset's GM/c^2 over its distance, in microarcseconds
    double earthWideMuas = 0, earthHighMuas = 0;
    // 0.55.3: the look; the disc's drawn edges (Real: rIn and rOut above; Film: the paper's) and, for Film, its one temperature
    Look look = Look::Real;
    double discEdgeIn = 0, discEdgeOut = 0, filmT = 0;
};
// latest (0.67.0; the panels): a spinning hole's shadow size from a worker (the last finished one, a few frames behind while spin, distance or tilt move;
// the same numbers once it has caught up); false: worked out at once (the tests, the Data panel)
Readout readout(const Settings& s, bool latest = false);

}  // namespace blackhole
