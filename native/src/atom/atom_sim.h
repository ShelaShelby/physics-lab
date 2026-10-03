// The Atom Lab simulation: its settings, its live state, and what happens each frame and each
// physics step. Ported from the web's SIMS['Atom Lab'] (init, onParam, actions, tick, step,
// info, info1, hint) and the helpers it calls (startJump, beginTransition, manualJump, advance,
// decTick, decLine). No drawing here.
//
// All lengths are metres, speeds m/s, times s (see atom_core.h). Sizes the web measures in Bohr
// radii (the size the picture is fitted to, the pan) are kept in metres too.
#pragma once

#include <cmath>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "atom_core.h"

namespace atom {

// The settings (the web's params), with the web's default values.
struct AtomParams {
    std::string scale = "Auto";          // Auto / Fixed
    double zoom = 0;                     // log10 of the zoom, 0–7
    std::string qstyle = "Cinematic";    // Cinematic / Accurate / Classic
    std::string detail = "+ Gluons";     // Valence / + Gluons / + Sea
    std::string style = "Fuzzy";         // Fuzzy / Hard balls
    int Z = 1, N = 0;                    // protons, neutrons
    std::string view = "Orbit";          // Orbit / Cloud / Cloud 3D / Slice
    std::string ostyle = "Modern";       // Modern / Bohr 1913
    std::string orb = "1s";              // the orbital's shell: 1s ... 4f
    std::string pAx = "z", dAx = "z2", fAx = "z3";   // orientation of p, d and f orbitals
    std::string cutAxis = "Vertical";    // Vertical / Horizontal
    double cut = 0;                      // cut position, −1 to 1
    int n = 1;                           // energy level (Orbit view)
    int speed = 5;                       // playback speed (Orbit view)
    std::string cstyle = "Classic";      // how the cloud's dots are drawn: Classic (the web's small hard dots) / Glow (soft dots that add up)
    std::string outline = "Off";         // 90 % outline on the cloud views: Off / On (atom/outline.h)
    std::string cdens = "Normal";        // how many dots the cloud keeps: Normal 12,000 (the web's) / High 24,000 / Ultra 48,000
    // The struct's own values are the web's (the tests that compare the lab with the web's code step by step use them). What the app
    // starts with, and what "Defaults" gives, is this: the web's settings plus the glowing, denser cloud.
    static AtomParams appDefaults() { AtomParams p; p.cstyle = "Glow"; p.cdens = "High"; return p; }
};

struct Vec3d { double x = 0, y = 0, z = 0; };
struct Photon { std::string kind; int from, to; double E, lam; std::string name, col; double ang, t, dur; };
struct Transition { int from, to; double t, dur; };
struct SeenLine { double lam; std::string col; };
struct FlyingShot { Shot q; double a; };
struct DecayFx { double t0; std::vector<FlyingShot> shots; };   // t0: wall clock, ms
struct LogLine { double t; std::string txt; };                 // t: wall clock, ms
struct ChainStep { std::string nuc; std::string how; double lived = 0; std::optional<std::string> to; };   // the first entry only has `nuc`
struct Hit { double X, Y, r, dep; int i; };                    // a nucleon on screen (points), for clicks
struct Box { double x, y, w, h; };

struct AtomState {
    // ---- what init() sets (and a change of element, level, view or orbital resets) ----
    double t = 0;                        // simulated time, s
    int Zprev = 1;
    std::optional<bool> more;            // ⓘ More / Less info (none = by screen width)
    Vec3d pan;                           // how far the picture is moved, m (x, y)
    std::optional<Vec3d> panTo;
    std::string dragMode = "rotate";
    double x = 0, y = 0, vx = 0, vy = 0; // the electron (Orbit view), m and m/s
    std::deque<std::pair<double, double>> trail;   // its last 500 positions, m
    std::vector<float> pts;              // cloud dots, m (x, y, z for each of kCap)
    std::vector<signed char> sg;         // the sign of ψ at each dot (+1 / −1)
    long k = 0;                          // dots recorded so far
    Vec3d w, w2;                         // the Metropolis walkers, in Bohr radii (see atom_core.h)
    double yaw = 0.6, pitch = 0.35;      // Cloud 3D view angles
    double oyaw = 0, opitch = kPi / 2;  // Orbit view angles (default: straight down on the orbit)
    bool spin = true;
    Vec3d fc;                            // the point the camera follows (a nucleon), m
    int focus = -1;                      // the nucleon followed, or −1
    std::optional<double> zoomTo;        // an animated zoom in progress
    std::string layer = "Atom";          // Atom / Nucleus / Nucleon / Quarks (set when drawing)
    std::vector<Hit> hits;
    std::optional<Transition> trans;     // the cloud morphing to another orbital
    std::optional<Photon> photon;        // a photon on its way
    std::vector<SeenLine> lines;         // spectral lines seen so far
    bool autoPhotons = false;
    double wait = 0.3, boost = 0;
    std::string last, msg;
    std::optional<double> szCur;         // the size the picture is fitted to (glides), m
    // ---- kept across init() ----
    double msgUntil = 0;                 // wall clock, ms
    std::optional<double> flash;         // Bohr 1913: the landing flash, s since the jump
    bool jumping = false, selfSet = false;
    std::optional<double> cutF;
    std::optional<Box> infoBox;
    std::vector<Nucleon> nuc;            // the nucleus being shown (built when drawn)
    std::string nucKey;
    int nucSeed = 0;
    // radioactive decay
    std::string decKey, decNext;
    double decAge = 0, decAt = INFINITY;
    bool decHeld = false;
    std::optional<DecayInfo> dec;
    std::optional<DecayFx> decFx;
    int decays = 0;                        // decays seen so far (the web grants the Half-Life achievement at each one)
    std::vector<LogLine> decLog;
};

class AtomLab {
public:
    AtomLab();
    AtomParams p;
    AtomState s;
    std::vector<ChainStep> chain;        // the decay history (web: DEC_CHAIN)
    int chainVer = 0;
    bool paused = false;                 // the app's pause
    bool hold = false;                   // a control is being held: the decay clock waits
    std::optional<bool> moreStored;      // the remembered ⓘ choice (web: localStorage plAtomMore)
    double viewW = 1000, viewH = 700;    // the picture's size in points (the web's canvas size)
    std::function<double()> nowMs;       // the wall clock in ms (web: performance.now / Date.now)
    bool reducedMotion = false;          // Settings → Motion: camera moves jump, Cloud 3D does not spin by itself (web: plRM())
    // the values a setting may take (for settings loaded from the file): a select's options, or a number's range
    static bool validChoice(const std::string& key, const std::string& value);

    // How many dots the cloud keeps (a ring: the newest replace the oldest). Only this and the drawing change with the density,
    // never the sampling of the dots.
    static int capFor(const std::string& density);
    int cloudCap() const { return capFor(p.cdens); }

    void restart();                      // the web's init(p) for a fresh lab
    // One entry point for every change of a setting (sliders, wheel, buttons), as the web's setParam:
    // numbers are clamped to their range, then onParam reacts.
    void setParam(const std::string& key, double v);
    void setParam(const std::string& key, const std::string& v);

    void tick(double dt);                // every frame, even when paused
    void step(double dt);                // every physics step (1/240 s) while running
    void ensureNucleus();                // build the nucleus if it changed (the web does it when drawing)

    // ---- actions (buttons) ----
    void buildAnother() { s.decKey.clear(); }
    void reshuffle() { s.nucSeed++; s.focus = -1; }
    void zoomAtom();
    void zoomNucleus();
    void zoomNucleon();
    void centre() { s.panTo = Vec3d{}; }
    void toggleDrag() { s.dragMode = s.dragMode == "move" ? "rotate" : "move"; }
    void resetView();
    void emit() { manualJump(-1); }
    void absorb() { manualJump(1); }
    void toggleAuto() { s.autoPhotons = !s.autoPhotons; s.wait = 0.3; }
    void toggleSpin() { s.spin = !s.spin; }
    bool showBuildAnother() const { return s.decHeld; }
    bool showReshuffle() const { return p.Z + p.N > 3; }
    bool showCentre() const;
    bool showDrag() const { return p.view == "Cloud 3D" || p.view == "Orbit" || p.view == "Slice"; }
    std::string dragText() const;
    bool showResetView() const;
    bool showSpin() const { return p.view == "Cloud 3D"; }
    std::string autoText() const { return s.autoPhotons ? "⏸ Auto photons: on" : "▶ Auto photons"; }

    void click(double x, double y);                       // picture points
    void drag(double dx, double dy, bool forceMove);      // forceMove: Shift, right or middle button

    // ---- the readout ----
    std::vector<std::string> info();
    std::string info1();
    std::string hint() const;
    std::string decLine() const;
    bool moreShown() const { return s.more ? *s.more : viewW >= 760; }

    // ---- helpers the drawing needs too ----
    int fullOrb() const;                                  // the orbital shown (shell + orientation)
    int fullOrb(const std::string& shell) const;
    double sizeM() const;                                 // the size the picture is fitted to, m
    double pxPerM(double m, std::optional<double> sz = std::nullopt) const;   // auto-fit points per metre
    double nucRA0() const;                                // nuclear radius in Bohr radii (the web's nucR)
    double zoomForNucleus() const;
    double zoomForNucleon() const;
    double cutOffsetM() const;                            // position of the cut plane, m
    std::vector<int> targets(int dir) const;              // levels a photon can take the electron to
    struct PhotonInfo { double E, lam; std::string name; };
    PhotonInfo photonInfo(int from, int to) const;
    int curLevel() const;                                 // Orbit: n; clouds: the orbital index
    int lvlN(int l) const;
    std::string lvl(int l) const;

private:
    void init();                                          // the fields the web's init(p) sets
    void onParam(const std::string& key);
    void walk(int orb, Vec3d& w, bool record);
    void mcmc(bool record) { walk(fullOrb(), s.w, record); }
    void startJump(const std::string& kind, int to);
    void beginTransition(int from, int to);
    void manualJump(int dir);
    void advance(double dt);
    void decTick(double dt);
};

}  // namespace atom
