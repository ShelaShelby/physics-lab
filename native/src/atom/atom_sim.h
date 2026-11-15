// The Atom Lab simulation: its settings, its live state, and what happens each frame and each
// physics step (the readout texts info, info1, hint and the helpers it calls: startJump, beginTransition, manualJump, advance,
// decTick, decLine). No drawing here.
//
// All lengths are metres, speeds m/s, times s (see atom_core.h).
// radii (the size the picture is fitted to, the pan) are kept in metres too.
#pragma once

#include <cmath>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <array>
#include <vector>

#include "atom_core.h"
#include "atom/hf.h"
#include "atom/alkali.h"
#include "chart.h"
#include "chart_view.h"
#include "decay_chain.h"
#include "orbit/belts.h"
#include "render/group_compute.h"

namespace atom {

// 0.15 stage 5p: the smallest zoom (the furthest zoom-out) is twice as far out as before: before, the smallest picture scale was 1 (log10 = 0); now it is 1/2, log10(1/2) = -log10(2) = -0.30102999566398...
inline constexpr double kZoomMin = -0.301029995663981195;
inline double zoomFactorShown(double zoom) { const double f = std::pow(10.0, zoom); return f < 1 ? std::round(f * 100) / 100 : std::round(f); }   // the "×" number beside the zoom slider: whole numbers, but 0.5 below 1

struct AtomParams {
    std::string scale = "Auto";          // Auto / Fixed
    double zoom = 0;                     // log10 of the zoom, kZoomMin–7 (5p: it was 0–7; the picture can now be shown at half its old smallest size)
    std::string qstyle = "Accurate";     // Accurate / Simple (0.43: Cinematic gone, Classic renamed Simple; migrateChoice maps the old names)
    std::string detail = "+ Gluons";     // Valence / + Gluons / + Sea
    std::string dspin = "Unpolarised";  // 0.46.5: the deuteron's spin state: Unpolarised (the three states mixed: round) / M = ±1 (a dumbbell along the spin axis) / M = 0 (a doughnut round it). Not saved.
    std::string style = "Nucleons";      // Density (the app's default via appDefaults(), 0.22: a smooth charge density while the whole nucleus is in view) / Nucleons (0.43: Fuzzy renamed; Hard balls gone)
    int Z = 1, N = 0;                    // protons, neutrons
    std::string view = "Orbit";          // Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides (kChartView, 0.11 stage 2). The app opens in Cloud 3D (kNewUiAtomView, ui/settings.cpp); "Orbit" is only the bare struct's value
    std::string ostyle = "Modern";       // Modern / Bohr 1913
    std::string orb = "1s";              // the orbital's shell: 1s ... 4f
    std::string pAx = "z", dAx = "z2", fAx = "z3";   // orientation of p, d and f orbitals
    std::string cutAxis = "Vertical";    // Vertical / Horizontal
    double cut = 0;                      // cut position, −1 to 1
    int n = 1;                           // energy level (Orbit view)
    int speed = 5;                       // playback speed (Orbit view)
    std::string cstyle = "Classic";      // how the cloud's dots are drawn: Classic / Glow (soft dots that add up)
    std::string outline = "Off";         // 90 % outline on the cloud views: Off / On (atom/outline.h)
    std::string super = "Off";           // Superposition of two orbitals (cloud views): Off / On (atom_core.h, quantum beats)
    std::string orb2 = "2p";             // the second orbital's shell, and its orientation (the same choices as the first)
    std::string pAx2 = "z", dAx2 = "z2", fAx2 = "z3";
    double mix = 50;                     // the share of the second orbital, 0 to 100 %
    std::string phase = "Sign";          // Phase colours on the cloud views: Sign (two colours, the sign of the wave: today's look) / Wheel (the phase arg(psi), a colour ring)
    std::string cdens = "Normal";        // how many dots the cloud keeps: Normal 12,000 / High 24,000 / Ultra 48,000
    std::string jfrom = "2p", jto = "1s";    // the "Check a jump" card (0.10.8 stage 3, atom/lifetime.h): the upper and the lower shell asked about. They change nothing else and are not saved.
    std::string elec = "All";            // 0.39: Electrons: Hydrogen-like (0.46; was "One": one electron round the bare nucleus, exact) / All (the neutral atom, Dirac-Hartree-Fock, atom/hf.h; 0.46: the default, as the review asked; hydrogen and any Z without such an atom fall back to one electron: manyOn())
    std::string show = "Whole atom";
    std::string mrel = "Relativistic";   // 0.41, Electrons All: the model, Relativistic (Dirac-Hartree-Fock, atom/hf.h) / Non-relativistic (its c -> infinity limit, for comparison)
    std::string rscale = "True scale";   // 0.49.3 (outside review): True scale is the default (was Shells enlarged). 0.39, the whole atom: Shells enlarged (radius drawn as R asinh(r/r0) / asinh(R/r0): the outer edge true, the inner shells spread out) / True scale     // 0.39, with Electrons All: Whole atom (every electron, the shells as colours) / One orbital (the chosen orbital of that atom)
    std::string hrel = "Auto";          // 0.49.3 (outside review): the Hydrogen-like cloud's model: Auto (Dirac from kRelativityZ = 27, where relativity shows; the familiar real orbitals below) / Dirac / Non-relativistic
    std::string hj = "l + 1/2";         // 0.49.3: the Dirac cloud's j: l + 1/2 / l - 1/2 (an s orbital is always j = 1/2)
    std::string vdots = "Easier to see";   // 0.54.1, the whole atom with jumps (Cloud, Cloud 3D): the outer electrons' dots Easier to see (kValenceDotShare of the dots more than their share) / True share
    // 0.54.0: the magnetic field (atom/zeeman.h). Not saved: the app always opens with the field off.
    double bfield = 0;                   // T: 0 (off, the default: nothing in the lab changes) to 100
    std::string blook = "Across";        // where the light is seen from: Across (the field) / Along
    int zline = -1;                      // the line shown split: an index into zeeman::labLines(Z, N, zeemanKind()); -1: the strongest line of the level now
    std::string dirac = "On";            // Relativity (0.10.8): On (0.49.3: the default, outside review; Dirac levels with the Lamb shift of n = 1, 2, atom/dirac.h) / Off (the non-relativistic levels, one per n).
    // What the app starts with, and what "Defaults" gives
    static AtomParams appDefaults() { AtomParams p; p.cstyle = "Glow"; p.cdens = "High"; p.style = "Density"; return p; }   // (0.22: the nucleus as its charge density)
};

struct Vec3d { double x = 0, y = 0, z = 0; };
// 0.49.2: a photon is a wave train as long as its jump: an emitted one leaves the atom DURING the jump (front at the jump's start, tail at its end), an absorbed one is taken in during it
// (its front arrives when the jump starts, its tail when it ends). t: its age, s; dur: the flight between the atom and the picture's edge; jdur: the jump's length (0: Reduced motion);
// landed: an absorbed photon's front has reached the atom (its jump has begun). It is gone when its tail leaves the picture (emit) or reaches the atom (absorb): t = dur + jdur.
struct Photon { std::string kind; int from, to; double E, lam; std::string name, col; double ang, t, dur, jdur; bool landed; };
// 0.49.3: how = what drives the jump: kJumpPlace ("Put the outer electron in", no light: a plain cross-fade to help the eye), kJumpEmit, kJumpAbsorb (one photon: the exact shares, AtomLab::jumpShareTo)
enum { kJumpPlace = 0, kJumpEmit = 1, kJumpAbsorb = 2 };
struct Transition { int from, to; double t, dur; int how = kJumpPlace; };
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
    std::vector<float> jp;               // cloud jump: the new dots (the target orbital's), m (x, y, z for each); the old dots stay in pts
    std::vector<float> jA, jB;           // cloud jump: normalised amplitudes (A re, A im, B re, B im) at each old dot and at each new dot
    // 0.13 stage 2: what a GPU copy of those arrays needs to know to stay up to date (render/group_compute.h). The epoch is a new number whenever the array is made again; the dirty range
    // [lo, hi) is the dots of `sa` written since a GPU copy last took them (lo >= hi: none). The CPU path ignores all of this.
    std::uint64_t saEpoch = 0, jumpEpoch = 0;
    std::uint64_t ptsEpoch = 0;          // 0.46 (C4): a new number whenever pts is changed other than by recordDot adding dots (a new cloud, a landed jump): the GPU copy is then sent whole
    long saDirtyLo = 0, saDirtyHi = 0;
    long jn = 0;                         // dots in each population
    // The jump's dots are built over a few frames (0.10.6 stage 8), not all in the frame the jump starts: the new dots are independent samples of the target state (0.37), made with the
    // jump's own random generator (seeded from the cloud's at the start), so the result is the same whether it is built over the frames or at once; and the amplitudes at the old and the new dots.
    struct JumpBuild { std::uint32_t rng = 0; double nA = 0, nB = 0; long done = 0; int from = 0, to = 0; bool many = false; int fromJ2 = 0, toJ2 = 0; };   // fromJ2, toJ2 (0.49.3): a Dirac cloud's 2j at both ends (0: not a Dirac jump)   // many (0.46): a valence jump of an alkali atom (from, to: alkaliOrbitals indices)
    std::vector<float> jph;              // 0.46, a valence jump: each new dot's colour phase (its shell, or its wave's sign as a phase), as recordDot writes s.ph
    std::vector<signed char> jsg;        // ... and its sign
    int valN = 0, valL = 0;              // 0.46 (B3): the outer electron's orbital of an alkali atom with every electron shown (valN = 0: the ground level); reset when the element changes
    int lvl = 0;                         // 0.47.3: a level atom (levels.h, levelOn): the level the atom is in (an index into jumpLevels; 0 = the ground level); reset when the element changes
    bool round = false;                  // 0.47.3 (owner decision D6): draw the free atom's round average over M_J (only D_0) instead of the lined-up M_J = J state (the default)
    int valJ2 = 0, valJ2Next = 0;        // 0.47.2: 2J of its level (0: the term's default, graspDefaultJ2) and of the level a jump under way lands in (set by the photon's line)
    JumpBuild jb;                        // done < jn: still being built
    double gAng = 0;                     // Orbit glide: the electron's angle
    double landT = 99;                   // seconds since the last landing (the 90 % outline fades back in)
    double beatT = 0;                    // Superposition: seconds of beat time on screen so far (runs at 1 s per second at Playback speed 5; Space pauses it)
    std::vector<float> ph;               // the phase arg(ψ) at each dot, radians in (−π, π]: 0 or π for a real orbital (its sign), the real phase for a complex state
    long k = 0;                          // dots recorded so far
    std::uint32_t crng = 0;              // 0.37: the cloud's own random generator (Mulberry32 state; seeded from the app's generator by init()): the dots are exact, independent samples (cloud_sample.h)
    int sliceAxis = 0; double sliceOff = 0;   // Slice view: the plane new dots are made on (axis 1: y, 2: z; offset, Bohr radii)
    long sliceFresh = 0;                 // Slice: dots made on that plane since it last moved (when it reaches the ring's size, every dot shown lies on the plane)
    double yaw = 0.6, pitch = 0.35;      // Cloud 3D view angles
    double oyaw = 0, opitch = kPi / 2;  // Orbit view angles (default: straight down on the orbit)
    bool spin = true;
    Vec3d fc;                            // the point the camera follows (a nucleon), m
    int focus = -1;                      // the nucleon followed, or −1
    std::optional<double> zoomTo;        // an animated zoom in progress
    std::string layer = "Atom";          // Atom / Nucleus / Nucleon / Quarks (set when drawing)
    std::vector<Hit> hits;
    std::optional<Transition> trans;     // the cloud morphing to another orbital
    std::optional<Photon> photon;        // a photon on its way (0.49.2: also while its jump runs)
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
    double nucAxis[3] = {0, 0, 1};
    bool densityDrawn = false;           // 0.22: the picture drew the nucleus as its charge density this frame (set by atom_view)       // 0.22: the snapshot's symmetry axis (the Density look draws its shape about the same axis)
    int nucSeed = 0;
    // radioactive decay
    std::string decKey, decNext;
    double decAge = 0, decAt = INFINITY;
    bool decHeld = false;
    std::optional<DecayInfo> dec;
    int decZ = 0, decN = -1;               // the nucleus `dec` and `decAt` belong to (p may already be the daughter for one frame after a decay)
    std::optional<DecayFx> decFx;
    int decays = 0;                        // decays seen so far
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
    std::vector<ChainStep> chain;        // the decay history
    int chainVer = 0;
    bool paused = false;                 // the app's pause
    bool gpuCompute = true;              // 0.13 stage 2: the superposition's and the jump's colour groups come from a compute shader (OpenGL 4.3), if the PC has it; false: the CPU loops (the reference). Not saved. A second switch, so stage 1 and stage 2 can be compared apart
    bool gpuOutline = true;              // 0.13 stage 3: the 90 % outline's density grid and level (and the Slice's contour values) come from compute shaders (OpenGL 4.3), if the PC has it, without waiting; false: the CPU does it, as before. Not saved. A third switch, so each stage can be compared apart
    bool gpuChart = true;                // 0.13 stage 4 part B: the chart of nuclides' tiles / blocks and ground are kept in a GPU buffer and drawn from it (OpenGL 3.3 is enough); false: made and sent again every frame, as up to 0.13 stage 3 (the reference). Not saved
    bool gpuCloud = true;                // 0.46 (C4): Cloud 3D Glow dots from a GPU buffer, projected and lit in the vertex shader (Renderer2D::cloudGpu); false: the CPU builds every dot each frame (the reference). Not saved
    bool gpuColours = true;              // 0.13 stage 1: the cloud's dots carry a colour INDEX and the shader picks the colour (true), or the CPU colours every dot as in 0.12 (false: for comparing). Not saved; the Performance card has the switch
    bool hold = false;                   // a control is being held: the decay clock waits
    std::optional<bool> moreStored;      // the remembered ⓘ choice
    mutable ChartScene chartScene_;
    mutable ChainGraph chainGraph_;      // the chain of the chosen nuclide, computed once per chosen nuclide (decayGraph())
    mutable int chainZ_ = 0, chainN_ = -1;
    double viewW = 1000, viewH = 700;    // the picture's size in points
    std::function<double()> nowMs;       // the wall clock in ms
    double chartTiltH = 0;                // 0.22 stage 6: the height of the chart's tilt bar above the toolbar, points (0: not shown)
    double legendBottom = 0;              // 0.22 stage 6: the bottom of the chart's Legend card, points from the top (0: none); the chart starts below it
    double insetTop = 0, insetBottom = 0; // new UI: how much of the picture the top bar and the toolbar cover, points (the readout lines and the chart keep clear of it)
    double insetBottomLeft = 0;          // new UI (0.15 stage 5d): how high the toolbar row and the fact line reach at the bottom left, points (the scale bar and the decay lines sit above it); 0 in the old UI
    std::vector<std::array<double, 4>> overBoxes;   // 0.22 stage 7: what covers the picture (cards, Legend, bars), x, y, w, h in window pixels: the chart's labels keep clear of them
    double insetRight = 0;                // 0.15 stage 5c: points the Controls panel covers at the right: the picture's text lines wrap before it
    double reserveRight = 0;              // 0.39.2: points kept clear at the bottom right (the shell's readout and toolbar): the axes gizmo sits above them, left of the Controls panel, as in the Orbit Lab
    bool infoOpen = false;               // 0.20: the info text folded over the nucleus (Nucleus, Nucleon, Quarks views) was opened with a click
    bool infoHidden = false;             // 0.15 stage 5p: the info text at the top left is not drawn while the new UI's Element picker is open (the picker covers that corner)
    mutable double infoPlateRight = 0, infoPlateBottom = 0;   // 0.46.1: the readout plate as drawn last frame (points; 0: none), so the picture's free part can start below it
    double infoDown = 0;                 // 0.15 stage 5g: the info text starts this far lower (points): below the phase ring when there is no room beside it
    double infoBelowDown = -1;           // 0.25: where the info text may go instead, below the card, as wide as the picture (points; -1: no such place): used when two rows fit there
    double infoRight = -1;               // 0.15 stage 5g: the info text at the top left stops here (points; -1: the old width). The new UI sets it to the left edge of the subject card or the phase ring
    double ringX = -1, ringY = -1;       // 0.15 stage 5g: where the picture puts the phase ring's centre (points); -1: the old place (top right). The new UI puts it clear of the subject card
    double freeX = -1, freeY = -1, freeW = -1, freeH = -1;   // 0.15 stage 5h: the part of the picture no card or bar covers (points); the atom is centred in it. freeW < 0: the whole picture (the old app)
    // 0.22 stage 6: the span the auto-fit sizes the picture by: the free part's shorter side (was the whole picture's, so the n = 1 orbit ran under the top bar and the card)
    double fitSpan() const { return freeW > 0 && freeH > 0 ? std::min(freeW, freeH) : std::min(viewW, viewH); }
    double infoSize = 13;                // 0.15 stage 5h: the info text's size in points (the new UI: 14, the body size)
    bool shellUi = false;                // the new UI is on: the picture leaves out its own "More / Less info" button (the toggle is in the Old panel until stage 5b)
    bool reducedMotion = false;          // Settings → Motion: camera moves jump, Cloud 3D does not spin by itself
    // the values a setting may take (for settings loaded from the file): a select's options, or a number's range
    static bool validChoice(const std::string& key, const std::string& value);
    static std::string migrateChoice(const std::string& key, const std::string& value);   // 0.43: an old saved value -> today's (Cinematic -> Accurate, Classic -> Simple, Fuzzy / Hard balls -> Nucleons)

    // How many dots the cloud keeps (a ring: the newest replace the oldest). Only this and the drawing change with the density,
    // never the sampling of the dots.
    static int capFor(const std::string& density);
    // 0.40: with every electron (the whole atom, Electrons All) the cloud keeps capFor dots PER ELECTRON, up to kManyDotsCeiling x capFor (60,000 / 120,000 / 240,000):
    // each dot is a sample of the whole atom's density, so with a fixed 12,000 the dots per electron fell as 12,000 / Z (130 for uranium) and the shells grew noisy as
    // 1 / sqrt(dots). The ceiling is a guess at what a PC draws smoothly (no GPU here to measure it); see native/REPORT.md, the 0.40 research.
    static constexpr int kManyDotsCeiling = 5;
    int cloudCap() const;

    void restart();
    // numbers are clamped to their range, then onParam reacts.
    void setParam(const std::string& key, double v);
    void setParam(const std::string& key, const std::string& v);

    void tick(double dt);                // every frame, even when paused
    void step(double dt);                // every physics step (1/240 s) while running
    void ensureNucleus();                // build the nucleus if it changed

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
    // 0.40: a click on a line of the spectrum strip: the electron is put in the upper level (n = hi; in the cloud views its p orbital, pz, which light along z joins to every
    // s level below it) and jumps down to n = lo, giving off exactly that line. The lab's levels go up to n = kLineMaxN. False (nothing changes) when it cannot be made here.
    static constexpr int kLineMaxN = 6;   // 0.46 (B1): was 4; n = 5 and 6 bring H-gamma (434 nm) and H-delta (410 nm), the four visible Balmer lines
    bool emitLine(int hi, int lo);
    void absorb() { manualJump(1); }
    void toggleAuto() { s.autoPhotons = !s.autoPhotons; s.wait = 0.3; s.autoSaid.clear(); }
    void toggleSpin() { s.spin = !s.spin; }
    bool showBuildAnother() const { return s.decHeld; }
    bool showReshuffle() const { return p.Z + p.N > 3; }
    bool showCentre() const;
    bool showDrag() const { return p.view == "Cloud 3D" || p.view == "Orbit" || p.view == "Slice"; }
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
    void chartTurn(double dx, double dy);                 // 0.40: the 3D chart: a drag turns the camera round (dx) and tips it (dy), never under the floor
    void chartReset() { const ChartHeightMode m = s.chartView.height; const double t = s.chartView.tilt; s.chartView = ChartViewState(); s.chartView.height = m; s.chartView.tilt = t; }   // (the choice of what the height means is kept; 0.40: and 2D or 3D, the camera back to its first view)
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
    std::vector<std::string> infoBody();                  // info() without the field's lines
    std::vector<std::string> fieldInfo() const;           // 0.54.0: the level's g values in the field (LS and NIST's measured one, the gap); empty with the field off
    std::string info1();
    std::string hint() const;
    std::string decLine() const;
    bool moreShown() const { return s.more ? *s.more : false; }   // (0.22 stage 6: the extra lines wait for More info, on every screen; was: shown from 760 points wide)

    // ---- helpers the drawing needs too ----
    int fullOrb() const;                                  // the orbital shown (shell + orientation); for a complex state: the real orbital the Orientation falls back to
    int complexM() const;                                 // m of the complex state chosen in Orientation (+-1 .. +-l), 0 when a real orientation is chosen
    int stateId() const;                                  // what the cloud shows: a complex state's id (atom_core.h), or fullOrb()
    RadialMode radialMode() const { return p.view == "Orbit" || manyOn() ? RadialMode::Hidden : superOn() ? RadialMode::Superposition : jumpCloud() ? RadialMode::Jumping : RadialMode::Shown; }
    int outlineStateId() const;                           // the state the 90 % outline is drawn for: the one on screen (a complex state's own id), NOT the real orbital the Orientation falls back to
    // 0.52.1: the phase ring (the legend of the phase of psi) is hidden from the Nucleon zoom on (the layer where nucleons fill the view, some 20 fm across, and
    // the Quarks zoom inside one): the colours on screen there are the nucleons' and quarks', not psi's; an electron dot is almost never there (hydrogen's 1s: each
    // dot is within 20 fm with odds (4/3)(R/a0)^3 = 7e-11); and psi's phase hardly changes across a nucleon (the waves change over a0 / Z: 53,000 fm for hydrogen,
    // 450 fm at Z = 118). Until the Nucleus zoom the cloud's dots fill the view, so the ring stays there.
    bool phaseRingHidden() const { return s.layer == "Nucleon" || s.layer == "Quarks"; }
    bool usesWheel() const { return p.phase == "Wheel" || complexM() != 0 || superOn() || (manyOn() && manySub() < 0); }   // (0.39: the whole atom: one colour per shell, from the wheel)
    // ---- 0.39: many electrons (atom/hf.h) ----
    bool manyOn() const;                                  // Electrons All, any view of one atom (0.39.1: also Slice and Orbit; 0.39 had the cloud views only), and Hartree-Fock tables for this element (He..Lr)
    struct Ring { int n = 0, electrons = 0; double r = 0; };   // 0.39.1, the Orbit view: a shell (n), its electrons, and their average distance (occupation-weighted <r> of its subshells, Bohr radii)
    std::vector<Ring> manyRings() const;
    double ringElectronAngle(const Ring& g, int k) const;   // 0.50: where electron k of a shell ring is drawn (radians round the ring)
    const hf::Atom* manyAtom() const;                     // the neutral atom's HF data, or nullptr
    int manySub() const;                                  // One orbital: the index of the chosen orbital in manyAtom()->sub (-1: the whole atom, or the atom has no such orbital)
    bool manyWanted() const { return p.elec == "All"; }
    // ---- 0.46 (B3): photon jumps of the one-valence-electron atoms with every electron (atom/alkali.h) ----
    bool valenceOn() const;                               // every electron shown, and this atom has frozen-core valence orbitals and NIST levels (Li, Na, K, Rb, Cs)
    AlkOrbital valenceNow() const;                        // the outer electron's orbital now
    int valenceJ2() const;                                // 0.47.2: 2J of its level now (the ground level: 1)
    std::vector<AlkOrbital> valenceTargets(int dir) const;   // orbitals one photon reaches from it: joined by a NIST electric-dipole line, lower (dir < 0) or higher (dir > 0) in energy
    void setValence(int n, int l);                        // put the outer electron in n l at once (no photon; the cloud is made again)
    std::vector<AlkOrbital> valenceChoices() const;       // 0.47: the shells the outer electron can be put in (alkaliOrbitals with an orbital computed: alkaliAtom), in energy order; empty unless valenceOn()
    bool placeValence(int n, int l);                      // 0.47: put the outer electron in n l with the jump's cross-fade, no photon (Reduced motion: at once); false: no such shell here
    static double valenceShape(int l, double cz);         // 0.47: the outer electron's direction density relative to its top, P_l(cos theta)^2 (the real m = 0 orbital: s, pz, dz2, fz3)
    // ---- 0.47.3: level jumps of an atom with several outer electrons (atom/levels.h: He, Be, Mg, Ca, Sr, Ba, Ra, Zn, Cd, Hg, Yb with clouds) ----
    bool levelOn() const;                                 // every electron shown, the relativistic model, and this atom has level clouds (levelJumps)
    int levelNow() const { return s.lvl; }                // the level now (index into jumpLevels)
    int zeemanKind() const;                               // 0.54.0: which levels the field acts on: 2 a level atom, 1 an alkali, 0 the one-electron atom, -1 none (every electron shown, no jumps)
    int zeemanLine() const;                               // the line shown split (index into zeeman::labLines; -1: none)
    std::vector<int> levelTargets(int dir) const;         // the levels one photon reaches from it (a NIST E1 line, both levels measured and with clouds, D1), lower (dir < 0) or higher
    bool placeLevel(int i);                               // put the atom in level i with the jump's cross-fade and no photon; false: no cloud for it
    bool emitLevelLine(int line);                         // a click on a line (index into jumpLines): the atom is put in its upper level and emits it
    bool absorbLevelLine(int line);                       // ... or is put in its lower level and absorbs it
    std::string levelShapeNote() const;
    void setRound(bool on);                               // 0.47.3 (D6): the round free-atom view on or off (the cloud is made again)                   // the readout's sentence on what is drawn (lined up or round, and why)
    bool emitValenceLine(int nHi, int lHi, int nLo, int lLo, double nm = 0);   // a click on a measured line: the electron is put in the upper orbital and emits that line's photon (nm > 0: that fine-structure component; 0: one drawn by g A)
    std::string manyModel() const { return p.mrel == "Non-relativistic" ? "Hartree-Fock, non-relativistic limit" : "Dirac-Hartree-Fock"; }   // 0.41   // asked for, whether or not it can be shown here
    static double shellPhase(int n);
    double manyExtM() const;                              // the view radius of the many-electron cloud, m: 1.7 x the 90 % radius of its most extended shown orbital (kExt's ratio for 1s)
    double cloudExtM() const;
    bool shellsEnlarged() const { return manyOn() && manySub() < 0 && p.rscale == "Shells enlarged"; }
    double drawnRadius(double r) const;                   // Bohr radii in, Bohr radii out: the enlarged-shells mapping (identity when off)
    static double enlargedRadius(const hf::Atom& a, double r);   // the mapping itself, for atom a                             // the view radius of the cloud on screen, m: manyExtM() or the one-electron kExt[sizeN()] / Z                      // the wheel phase that colours shell n (K, L, M ...): seven well separated colours   // complex states and superpositions always use the phase wheel
    int stateIdOf(const std::string& shell, const std::string& pAx, const std::string& dAx, const std::string& fAx) const;   // the id of a shell + orientation (real or complex)
    // ---- smooth photon jumps ----
    void startJump(const std::string& kind, int to);      // emit: the jump runs first, the photon appears when it ends; absorb: the photon flies in, the jump starts when it arrives
    bool jumpCloud() const { return s.trans && p.view != "Orbit"; }   // a cloud jump is on screen
    double jumpDuration(int how) const;                   // a jump's length on screen: a photon's kJumpTauS ln(kJumpDotsEnd) (0.49.3), a placement 1.2 s, 0 with Reduced motion
    double jumpShareTo() const;                           // 0.49.3: the new level's share of the atom now (0 .. 1); the old level has the rest. See kJumpTauS.
    double jumpU() const;                                 // 0 .. 1 with sin^2(pi/2 u) = jumpShareTo() (the angle the cross-fades use)
    // 0.49.3 (outside review): a photon jump is the exact one-photon process, slowed down. Emission (Weisskopf-Wigner): the upper level's share falls as exp(-t / tau), and the atom is a MIXTURE of the
    // two levels, not a superposition: the photon carries the coherence away, so no charge sloshes (the sloshing picture is that of a classical light wave; Superposition shows a coherent mix).
    // Absorption: the time reverse, the single photon whose shape is the emitted one played backwards (the one that is absorbed completely): the upper share rises as exp(-(T - t) / tau).
    // tau on screen is kJumpTauS for every line (the real one is nanoseconds: Lifetimes card); the jump ends at T = tau ln(kJumpDotsEnd), when less than one dot of the largest cloud is left to change: the photon is cut there and renormalised
    // (the shares are then (1 - exp(-t / tau)) / (1 - exp(-T / tau)) and its time reverse, exactly 0 at the start and 1 at the end).
    static constexpr double kJumpTauS = 0.2;               // (0.2 s: the steepest frame, at a jump's sharp end, changes 1/12 of the cloud at 60 frames a second)
    static constexpr double kJumpDotsEnd = 48000;          // Ultra's dots
    long jumpDots() const { return s.jn; }
    static constexpr long kSliceDotsPerFrame = 1500;      // Slice: at most this many new dots a frame while the plane's dots are replaced (about 2 ms here; 0.37 stage 1 made up to 6,000 and lagged)
    bool jumpReady() const { return s.jb.done >= s.jn; }  // every dot of the jump is built (the picture shows the jump only then; until then it shows the cloud as it was: the jump has only just begun)
    bool jumpDrawn() const { return jumpCloud() && jumpReady(); }
    void completeJump() { if (!jumpReady()) { if (s.jb.many) buildManyJump(s.jn); else buildJump(s.jn); } }   // build what is left now (anything that reads the jump's arrays calls this first)
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
    bool superOn() const { return p.super == "On" && p.view != "Orbit" && !manyOn(); }
    // ---- 0.49.3 (outside review): the Dirac cloud of the one-electron atom (atom/dirac_cloud.h) ----
    bool diracCloudOn() const;                            // Hydrogen-like, a cloud view (not Orbit or the chart), no superposition, and the model is Dirac (Auto: from kRelativityZ)
    int diracJ2() const;                                  // 2j of the Dirac state shown: the orbital's l and the j setting (s: 1)
    std::string diracText() const;                        // "2p₃/₂"
    int pendingJ2_ = 0;                                   // the target's 2j picked by startJump for the jump about to begin (beginTransition copies it into s.jb.toJ2)   // (0.39: not with many electrons: a superposition is a one-electron state)
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
    double nucRA0() const;                                // nuclear radius in Bohr radii
    // 0.46.4 (outside review: lengths used the infinite-mass Bohr radius while energies used the reduced mass): the one-electron model's units are those of the reduced mass mu = m M / (m + M):
    // lengths a_mu = a0 m / mu and times t_mu = t_au m / mu (speeds unchanged), so every orbit, cloud, outline and <r> is (1 + m / M) times the infinite-mass one (H: +5.4e-4).
    // The many-electron atom (Electrons All) keeps a0: its Dirac-Hartree-Fock orbitals are for a fixed nucleus.
    double lenScale() const;                              // a_mu / a0 (1 with every electron)
    double aUnit() const { return kA0 * lenScale(); }     // metres per model length unit
    double tUnit() const { return kAuTime * lenScale(); } // seconds per model time unit
    double zoomForNucleus() const;
    double zoomForNucleon() const;
    double cutOffsetM() const;                            // position of the cut plane, m
    std::vector<int> targets(int dir) const;              // levels a photon can take the electron to
    static constexpr double kValenceDotShare = 0.25;      // 0.46.1: with valence jumps on, this share of the whole atom's 3D dots is the outer electron's (said in the readout)
    double extraOuterShare() const { return p.vdots == "True share" ? 0.0 : kValenceDotShare; }   // 0.54.1: the extra share actually used (True share: none, every electron its own share)
    bool outerDotsChoice() const;                         // 0.54.1: the "Outer electron's dots" choice applies: the whole atom with jumps (valence or level atoms) in Cloud or Cloud 3D
    double outerDotFraction(int n, unsigned seed) const;  // 0.54.1 (for the tests): of n whole-atom dots drawn as the cloud draws them, the share that are the outer electrons'
    std::string trueShareText(const hf::Atom& a) const;   // 0.54.1: "1 in 11" (an alkali) or "2 in 12" (the outer electrons' occupations against Z)
    static double denserFactor(double nOuter, int electrons);   // 0.52.1: how many times their true share the outer electrons' dots are drawn (the level atoms' main line)
    std::string outerDenserText(const hf::Atom& a) const;
    double emitRate(int to) const;                        // 0.46.1: the rate (1/s, hydrogen's scale for one electron) of the decay to target `to` by NIST ASD's A; NaN when it has none
    int pickEmit(const std::vector<int>& list, double* share = nullptr) const;   // a decay target drawn with those odds (equal odds when a rate is missing)
    struct PhotonInfo { double E, lam; std::string name; };
    PhotonInfo photonInfo(int from, int to) const;
    int curLevel() const;                                 // Orbit: n; clouds: the orbital index
    int lvlN(int l) const;
    std::string lvl(int l) const;

private:
    void init();
    void onParam(const std::string& key);
    // 0.37: the dots are exact, independent samples of the density (cloud_sample.h), not a Metropolis chain. One attempt at a dot of the state on screen (one orbital, or a superposition's
    // time-independent mixture (|A|^2 + |B|^2) / 2, or an overlapping pair's |a A + b B|^2): in the whole space, or (Slice) on the cut plane. False: rejected, try again.
    bool tryDot(orbit::Mulberry32& r, double& x, double& y, double& z);
    bool tryStatePoint(int id, orbit::Mulberry32& r, double& x, double& y, double& z, int diracJ2 = 0) const;   // 0.49.3: diracJ2 > 0 draws that Dirac state of id's n and l   // one attempt at a point of one state (always succeeds off the Slice view)
    void recordDot(double x, double y, double z);         // write a dot into the ring: its place, sign, phase (and a superposition's amplitudes)
    long addDots(long want);                              // up to `want` new dots (fewer only if the attempts run out: a plane with almost no probability); returns how many
    bool sliceDots() const { return p.view == "Slice"; }
    void syncSlicePlane();                                // Slice: when the plane moves, new dots are made on the new plane, replacing the oldest (sliceFresh counts them)
    int sliceAxisNow() const { return p.cutAxis == "Horizontal" ? 2 : 1; }
    double sliceOffNow() const;                           // Bohr radii (cutOffsetM() in metres): Cut position x half the view radius (0.39.1: the many-electron atom's with Electrons All)
    Superposition sup_;                                   // set when the cloud is (re)started and when a share changes
    double nA_ = 1, nB_ = 1;                              // normalisation factors of the two states at the current Z
    int manyShell_ = -1;                                  // 0.39: the subshell the last whole-atom dot came from (its colour)
    const hf::Atom* manyNow_ = nullptr;                   // 0.39.1: manyOn() / manyAtom() / manySub() worked out once per batch of dots (addDots), not several times per dot
    int manySubNow_ = -1;
    int valSubNow_ = -1;                                  // 0.47: the outer electron's subshell in manyNow_ (valence jumps on), else -1
    const GraspLevel* gNow_ = nullptr;                    // 0.47.2: the GRASP level drawn (its aligned-state shape), else nullptr
    std::vector<int> valSetNow_;                          // 0.47.3: a level atom: the subshells of its outer electrons in manyNow_ (drawn denser, as the alkalis' outer electron)
    hf::Plane manyPlane_;                                 // 0.39.1, Slice with many electrons: every subshell's mass on the cut plane, worked out once per batch
    bool overlap_ = false;                                // Superposition: the two states overlap (<A|B> != 0: the same n, l and a non-orthogonal angular part). They then share one energy, the state is static, and the walker samples its renormalised density directly (every dot is shown)
    void buildJump(long upTo);                            // build the jump's dots up to upTo (old and new dots with the same index)
    void beginTransition(int from, int to, int how);
    void finishTransition();
    void glideOrbit(double dt);
    void manualJump(int dir);
    void startValenceJump(const std::string& kind, int toIdx, double nm = 0);
    void startLevelJump(const std::string& kind, int to, int line = -1);   // 0.47.3: the jump between levels along a NIST line   // 0.46: the alkali jump: the photon is one of the NIST fine-structure components (alkaliPickLine)
    void buildManyJump(long upTo);                        // 0.46: the new dots of a valence jump: samples of the atom with the electron in the new orbital
    void manyColour(const hf::Atom& a, int sub, int shell, double& x, double& y, double& z, signed char& sg, float& ph) const;   // 0.46: a many-electron dot's colour (and the enlarged-shells mapping), as recordDot does
    int manySubOf(const hf::Atom& a) const;               // 0.46: manySub() for another atom (the target of a valence jump)
    struct NoJump { std::string text; double ms; };
    NoJump whyNoJump(int dir) const;                      // why targets(dir) is empty, in the words the Emit/Absorb buttons use (and Auto, when it finds no jump at all)
    void advance(double dt);
    void decTick(double dt);
};

}  // namespace atom
