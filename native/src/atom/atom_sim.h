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
#include "chart.h"
#include "chart_view.h"
#include "decay_chain.h"
#include "render/group_compute.h"

namespace atom {

// The settings (the web's params), with the web's default values.
struct AtomParams {
    std::string scale = "Auto";          // Auto / Fixed
    double zoom = 0;                     // log10 of the zoom, 0–7
    std::string qstyle = "Cinematic";    // Cinematic / Accurate / Classic
    std::string detail = "+ Gluons";     // Valence / + Gluons / + Sea
    std::string style = "Fuzzy";         // Fuzzy / Hard balls
    int Z = 1, N = 0;                    // protons, neutrons
    std::string view = "Orbit";          // Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides (kChartView, 0.11 stage 2)
    std::string ostyle = "Modern";       // Modern / Bohr 1913
    std::string orb = "1s";              // the orbital's shell: 1s ... 4f
    std::string pAx = "z", dAx = "z2", fAx = "z3";   // orientation of p, d and f orbitals
    std::string cutAxis = "Vertical";    // Vertical / Horizontal
    double cut = 0;                      // cut position, −1 to 1
    int n = 1;                           // energy level (Orbit view)
    int speed = 5;                       // playback speed (Orbit view)
    std::string cstyle = "Classic";      // how the cloud's dots are drawn: Classic (the web's small hard dots) / Glow (soft dots that add up)
    std::string outline = "Off";         // 90 % outline on the cloud views: Off / On (atom/outline.h)
    std::string super = "Off";           // Superposition of two orbitals (cloud views): Off / On (atom_core.h, quantum beats)
    std::string orb2 = "2p";             // the second orbital's shell, and its orientation (the same choices as the first)
    std::string pAx2 = "z", dAx2 = "z2", fAx2 = "z3";
    double mix = 50;                     // the share of the second orbital, 0 to 100 %
    std::string phase = "Sign";          // Phase colours on the cloud views: Sign (two colours, the sign of the wave: today's look) / Wheel (the phase arg(psi), a colour ring)
    std::string cdens = "Normal";        // how many dots the cloud keeps: Normal 12,000 (the web's) / High 24,000 / Ultra 48,000
    std::string jfrom = "2p", jto = "1s";    // the "Check a jump" card (0.10.8 stage 3, atom/lifetime.h): the upper and the lower shell asked about. They change nothing else and are not saved.
    std::string dirac = "Off";           // Relativity (0.10.8): Off (the non-relativistic levels, as always) / On (Dirac levels and split lines in the two spectrum cards only, atom/dirac.h). Off in the app too.
    // The struct's own values are the web's (the tests that compare the lab with the web's code step by step use them). What the app
    // starts with, and what "Defaults" gives, is this: the web's settings plus the glowing, denser cloud.
    static AtomParams appDefaults() { AtomParams p; p.cstyle = "Glow"; p.cdens = "High"; return p; }
};

struct Vec3d { double x = 0, y = 0, z = 0; };
struct Photon { std::string kind; int from, to; double E, lam; std::string name, col; double ang, t, dur; };
struct Transition { int from, to; double t, dur; };
// What the radial probability graph (radial_graph.h) does: not shown (Orbit view), drawn, replaced by a note while a superposition is shown, or while a photon jump changes the cloud.
enum class RadialMode { Hidden, Shown, Superposition, Jumping };
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
    std::vector<float> sa;               // Superposition: the normalised amplitudes of the two states at each dot (A re, A im, B re, B im), made when the dot is made
    // ---- a photon jump (smooth): the emitted photon waits here until the jump ends ----
    std::optional<Photon> photonAfter;
    std::vector<float> jp;               // cloud jump: the new dots (the target orbital's), m (x, y, z for each); the old dots stay in pts
    std::vector<float> jA, jB;           // cloud jump: normalised amplitudes (A re, A im, B re, B im) at each old dot and at each new dot
    // 0.13 stage 2: what a GPU copy of those arrays needs to know to stay up to date (render/group_compute.h). The epoch is a new number whenever the array is made again; the dirty range
    // [lo, hi) is the dots of `sa` written since a GPU copy last took them (lo >= hi: none). The CPU path ignores all of this.
    std::uint64_t saEpoch = 0, jumpEpoch = 0;
    long saDirtyLo = 0, saDirtyHi = 0;
    long jn = 0;                         // dots in each population
    // The jump's dots are built over a few frames (0.10.6 stage 8), not all in the frame the jump starts: the chain that makes the new dots, with the random numbers it would have used
    // (the generator is moved past them at the start, so everything after the jump gets the numbers it always got), and the amplitudes at the old and the new dots.
    struct JumpBuild { std::uint32_t rng = 0; Vec3d w; double fo = 0, nA = 0, nB = 0; long done = 0; int from = 0, to = 0; bool settled = false; };
    JumpBuild jb;                        // done < jn: still being built
    double gAng = 0;                     // Orbit glide: the electron's angle
    double landT = 99;                   // seconds since the last landing (the 90 % outline fades back in)
    double beatT = 0;                    // Superposition: seconds of beat time on screen so far (runs at 1 s per second at Playback speed 5; Space pauses it)
    std::vector<float> ph;               // the phase arg(ψ) at each dot, radians in (−π, π]: 0 or π for a real orbital (its sign), the real phase for a complex state
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
    ChartViewState chartView;            // the chart of nuclides: tilt, zoom, pan (kept across init(); not saved)
    std::string autoSaid;                // Auto photons: the reason already given for finding no jump (state + cause), so it is said once, not every time Auto looks
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
    int decZ = 0, decN = -1;               // the nucleus `dec` and `decAt` belong to (p may already be the daughter for one frame after a decay)
    std::optional<DecayFx> decFx;
    int decays = 0;                        // decays seen so far (the web grants the Half-Life achievement at each one)
    std::vector<LogLine> decLog;
    // the decay chain on the chart (0.12 stage 1): the walk down the chosen nuclide's chain (restarts when another nuclide is chosen), whether the trail is shown, and the auto-walk
    ChainWalk walk;
    bool showChain = true, walkAuto = false;
};

class AtomLab {
public:
    AtomLab();
    AtomParams p;
    AtomState s;
    std::vector<ChainStep> chain;        // the decay history (web: DEC_CHAIN)
    int chainVer = 0;
    bool paused = false;                 // the app's pause
    bool gpuCompute = true;              // 0.13 stage 2: the superposition's and the jump's colour groups come from a compute shader (OpenGL 4.3), if the PC has it; false: the CPU loops (the reference). Not saved. A second switch, so stage 1 and stage 2 can be compared apart
    bool gpuOutline = true;              // 0.13 stage 3: the 90 % outline's density grid and level (and the Slice's contour values) come from compute shaders (OpenGL 4.3), if the PC has it, without waiting; false: the CPU does it, as before. Not saved. A third switch, so each stage can be compared apart
    bool gpuChart = true;                // 0.13 stage 4 part B: the chart of nuclides' tiles / blocks and ground are kept in a GPU buffer and drawn from it (OpenGL 3.3 is enough); false: made and sent again every frame, as up to 0.13 stage 3 (the reference). Not saved
    bool gpuColours = true;              // 0.13 stage 1: the cloud's dots carry a colour INDEX and the shader picks the colour (true), or the CPU colours every dot as in 0.12 (false: for comparing). Not saved; the Performance card has the switch
    bool hold = false;                   // a control is being held: the decay clock waits
    std::optional<bool> moreStored;      // the remembered ⓘ choice (web: localStorage plAtomMore)
    mutable ChartScene chartScene_;
    mutable ChainGraph chainGraph_;      // the chain of the chosen nuclide, computed once per chosen nuclide (decayGraph())
    mutable int chainZ_ = 0, chainN_ = -1;
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
    void toggleAuto() { s.autoPhotons = !s.autoPhotons; s.wait = 0.3; s.autoSaid.clear(); }
    void toggleSpin() { s.spin = !s.spin; }
    bool showBuildAnother() const { return s.decHeld; }
    bool showReshuffle() const { return p.Z + p.N > 3; }
    bool showCentre() const;
    bool showDrag() const { return p.view == "Cloud 3D" || p.view == "Orbit" || p.view == "Slice"; }
    std::string dragText() const;
    bool showResetView() const;
    bool showSpin() const { return p.view == "Cloud 3D"; }
    std::string autoText() const { return s.autoPhotons ? "⏸ Auto photons: on" : "▶ Auto photons"; }

    bool chartOn() const { return p.view == kChartView; }  // the map of all nuclides is shown instead of one atom
    void chooseNuclide(int Z, int N);                     // the chart's click: exactly this Z and N (the other views then show it)
    bool chartStep(int dZ, int dN);
    // the landscape (0.11 stage 3): tilt 0 = the flat chart, 1 = the full landscape; zoom about a picture point (factor > 1 = closer); pan in picture points (only while zoomed in). Each keeps the chart in the picture.
    void setChartTilt(double t);
    void chartZoomAt(double factor, double px, double py);
    void chartPan(double dx, double dy);
    void chartReset() { const ChartHeightMode m = s.chartView.height; s.chartView = ChartViewState(); s.chartView.height = m; }   // (the choice of what the height means is kept)
    // the decay chain of the chosen nuclide (0.12 stage 1): the graph is built once per chosen nuclide; the walk draws the lifetime and the branch with the same two functions as the decay clock (decDrawLifetime,
    // decDrawBranch) and the lab's random numbers. The walker is NOT the chosen nuclide: choosing stays where the user put it.
    const ChainGraph& decayGraph() const;
    void walkOnce();                                      // one decay of the walker (if it is not over)
    void walkRestart();                                   // the walker back on the chosen nuclide
    const ChartScene& chartScene() const;                 // the blocks for the picture's size and the view (rebuilt only when one of them changed)                       // an arrow key in the chart: one proton or one neutron, only onto an observed nuclide
    void click(double x, double y);                       // picture points
    void drag(double dx, double dy, bool forceMove);      // forceMove: Shift, right or middle button

    // ---- the readout ----
    std::vector<std::string> info();
    std::string info1();
    std::string hint() const;
    std::string decLine() const;
    bool moreShown() const { return s.more ? *s.more : viewW >= 760; }

    // ---- helpers the drawing needs too ----
    int fullOrb() const;                                  // the orbital shown (shell + orientation); for a complex state: the real orbital the Orientation falls back to
    int complexM() const;                                 // m of the complex state chosen in Orientation (+-1 .. +-l), 0 when a real orientation is chosen
    int stateId() const;                                  // what the cloud shows: a complex state's id (atom_core.h), or fullOrb()
    RadialMode radialMode() const { return p.view == "Orbit" ? RadialMode::Hidden : superOn() ? RadialMode::Superposition : jumpCloud() ? RadialMode::Jumping : RadialMode::Shown; }
    int outlineStateId() const;                           // the state the 90 % outline is drawn for: the one on screen (a complex state's own id), NOT the real orbital the Orientation falls back to
    bool usesWheel() const { return p.phase == "Wheel" || complexM() != 0 || superOn(); }   // complex states and superpositions always use the phase wheel
    int stateIdOf(const std::string& shell, const std::string& pAx, const std::string& dAx, const std::string& fAx) const;   // the id of a shell + orientation (real or complex)
    // ---- smooth photon jumps ----
    bool webJumps = false;                                // the tests that compare with the web: its instant leap, its dot-by-dot morph and its single leapfrog step per frame (no sub-steps). The app never sets it.
    void startJump(const std::string& kind, int to);      // emit: the jump runs first, the photon appears when it ends; absorb: the photon flies in, the jump starts when it arrives
    bool jumpCloud() const { return s.trans && p.view != "Orbit" && !webJumps; }   // a cloud jump is on screen
    double jumpU() const;                                 // 0 .. 1: smooth progress of the jump (theta = pi/2 u)
    double jumpPhi() const;                               // the slowed beat phase during the jump
    long jumpDots() const { return s.jn; }
    bool jumpReady() const { return s.jb.done >= s.jn; }  // every dot of the jump is built (the picture shows the jump only then; until then it shows the cloud as it was: the jump has only just begun)
    bool jumpDrawn() const { return jumpCloud() && jumpReady(); }
    void completeJump() { if (!jumpReady()) buildJump(s.jn); }   // build what is left now (anything that reads the jump's arrays calls this first)
    // 0.13 stage 2: the same two results from the GPU. They return false when the compute path cannot run (the caller then uses the CPU functions). The CPU functions stay the reference.
    render::SuperGroupParams superGpuParams(double theta) const;      // a, b, exp(-i theta), overlap: what the kernel needs each frame
    render::JumpGroupParams jumpGpuParams() const;                    // the terms of this frame's jump
    bool superGroupsGpu(render::IGroupCompute& gc, long cnt, double theta, unsigned char* out);   // out[i] = superGroup(i, theta) for i < cnt
    bool jumpGroupsGpu(render::IGroupCompute& gc, unsigned char* out);                            // out = jumpGroups()'s 2 jn entries
    void jumpGroups(unsigned char* grp) const;            // jumpGroup(0, i) into grp[i] and jumpGroup(1, i) into grp[jumpDots() + i], all dots, the terms that do not depend on the dot worked out once
    bool jumpShown(int pop, long i) const;                // pop 0: the old dots, 1: the new ones
    double jumpPhase(int pop, long i) const;              // arg of the wave at the dot (gauge: ends on the new orbital's own phase)
    int jumpGroup(int pop, long i) const;                 // 255 = not shown, else the colour group (phase bin, or 0 / 1 for the sign)
    double outlineAlpha() const;                          // how strongly the 90 % outline is drawn (it fades out as the cloud leaves its orbital, and back in after landing)
    // ---- quantum beats ----
    bool superOn() const { return p.super == "On" && p.view != "Orbit"; }
    Superposition superState() const;                     // the two states and the shares (a^2 = 1 - mix, b^2 = mix)
    int sizeN() const;                                    // the n the picture is fitted to: the bigger of the two when a superposition is shown
    static constexpr double kBeatScreenS = 4.0;           // one beat takes this long on screen at Playback speed 5 (and scales with the speed)
    double beatTheta() const;                             // the beat phase on screen now (0 when the energies are equal: no beats)
    double beatRealPeriodS() const;                       // the real period h / |EB - EA| (infinity: no beats)
    double beatSlowdown() const;                          // on-screen period / real period
    std::string beatKindText() const;                     // what the two states do together: sloshes, circles, breathes, no beats
    std::string beatSlowText() const;                     // "real beat period 400 as, slowed by 1×10^16"
    double superPhase(long dot, double theta) const;      // the colour phase of a dot: arg of (a A + b B e^(-i theta)) relative to A, from its stored amplitudes
    bool superShown(long dot, double theta) const;        // is the dot on screen now? (a fixed random number per dot against the share of this density: exact thinning)
    int superGroup(long dot, double theta) const;         // 255 = not shown, else the phase colour bin
    std::string stateText() const;                        // "2pz" or "2p (m = +1)"
    int fullOrb(const std::string& shell) const;
    double orbitEnergyEV() const;                         // Orbit view: the electron's energy from its position and speed, eV (scaled to the reduced-mass level, see atom_sim.cpp)
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
    void walk(int orb, Vec3d& w, bool record);            // orb: a real orbital index or a complex state's id
    void walkSuper(Vec3d& w, bool record);                // the walker for a superposition: it samples the time-independent mixture (|A|^2 + |B|^2) / 2
    void mcmc(bool record) { if (superOn()) walkSuper(s.w, record); else walk(stateId(), s.w, record); }
    Superposition sup_;                                   // set when the cloud is (re)started and when a share changes
    double nA_ = 1, nB_ = 1;                              // normalisation factors of the two states at the current Z
    bool overlap_ = false;                                // Superposition: the two states overlap (<A|B> != 0: the same n, l and a non-orthogonal angular part). They then share one energy, the state is static, and the walker samples its renormalised density directly (every dot is shown)
    void buildJump(long upTo);                            // build the jump's dots up to upTo (old and new dots with the same index)
    void beginTransition(int from, int to);
    void finishTransition(double overshoot);
    void glideOrbit(double dt);
    void manualJump(int dir);
    struct NoJump { std::string text; double ms; };
    NoJump whyNoJump(int dir) const;                      // why targets(dir) is empty, in the words the Emit/Absorb buttons use (and Auto, when it finds no jump at all)
    void advance(double dt);
    void decTick(double dt);
};

}  // namespace atom
