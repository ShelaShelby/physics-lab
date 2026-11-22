// The Orbit Lab controls on the right: Reset to today, Time, View, extras and Go to.
#pragma once

#include "app/frame_time.h"
#include "app/prefs.h"
#include "atom/atom_sim.h"
#include "atom/atom_view.h"
#include "blackhole/blackhole_view.h"
#include "blackhole/lab.h"
#include "molecule/molecule_view.h"
#include "orbit/orbit_view.h"
#include "orbit/solar_system.h"
#include "ui/frame_stats.h"
#include "ui/journey_dates.h"

#include <memory>
#include <string>

namespace ui {

struct BenchState;                     // the benchmark (ui/benchmark.cpp)

// 📷 Save image: what the button says and shows after a save (see ui/shot.h)
struct ShotUi {
    bool requested = false;                // the button was pressed: the main loop saves this frame's picture
    double flashUntil = 0;                 // the button's label shows `flash` ("Saved ✓" / "Could not save") until this time (ImGui time)
    std::string flash;
    double noteUntil = 0;                  // the line under the buttons: where the file went, or why it was not saved
    std::string note;
    bool failed = false;
};

struct AppState {
    orbit::SolarSystem sys;
    orbit::ViewSettings set;
    orbit::OrbitView view;
    bool paused = false;
    bool showAbout = false, showHistory = false;
    int pane = 0;                          // the tabs: 0 the labs (🔬 Simulations), 1 📚 Learn (2 was 🏆 Achievements, removed in 0.52.0)
    int learnBack = -1;                    // a guide opened from the Learn tab in another lab: the lab to go back to when it closes
    bool showSettings = false;
    ShotUi shot;
    std::shared_ptr<BenchState> bench;     // the benchmark: its window, its run and the settings it will give back
    bool benchRunning = false;             // a benchmark is running: nothing is written to the settings file
    double startupMs = -1;                 // 0.56.1: from the start of the app to its first finished picture (ms; the benchmark report's head), -1 not yet
    int fitStart = 2;                      // 0.24: the Orbit Lab opens fitted to Neptune's orbit, once per start, this many Orbit Lab frames after the start (the panels' sizes are known by then); 0 done, -1 never (PL_ZOOM)
    double canvasW = 800, canvasH = 600;   // picture size in points (for the Go to zoom levels)
    int glMajor = 0, glMinor = 0;          // 0 0: Vulkan (app/bench.h's path texts; 0.69.0: Vulkan is the only drawing system)
    bool rocksOk = false;                  // the renderer has the card's rock compute object (the Orbit Lab's GPU rocks, 0.13 stage 4); false: the CPU places the rocks
    std::string rocksWhy;                  // 0.61.0: on Vulkan, why the rocks are not on the card (no double precision in its shaders, a failed step)
    bool computeOk = false;                // the renderer has the card's compute path (the Atom Lab's GPU superposition and jumps); false: the CPU does that work
    bool vsync = true;                     // wait for the screen's refresh (off: draw as fast as possible, to measure headroom)
    std::string vulkanDevice, vulkanDriver;   // 0.59.0: Vulkan's card and driver, as the driver names them
    FrameStats stats;
    std::string powerNote;                 // 0.15 stage 2: app::powerNote() (Windows power throttling asked off), printed in the benchmark report
    app::LiveStats live;                   // frames drawn while the window system had the window in its move / resize loop
    bool liveFrame = false;                // the frame being built now is one of those
    Journey journey;
    char dateText[16] = "";                // Jump to a date: what is typed in
    char timeText[8] = "12:00";
    std::string dateNote;                  // the note under it
    bool jumpWaiting = false;              // 0.67.0: a jump in Real waits for its walk on a worker (ui jumpToDate, jumpTick)
    int64_t jumpWaitMs = 0;
    std::string jumpWaitNote;              // (its clamp note)
    long orbitPictures = 0;                // 0.67.0: how many times the Orbit Lab's picture was drawn (not kept; the benchmark's rest line)
    int learn = 0;                         // the open Learn guide: 0 none, 1 What am I looking at?, 2 seasons, 3 timeline
    std::string learnJump;                 // a section to scroll to in the open guide ("bar" = back to the list of headings)
    // ---- Atom Lab ----
    app::Prefs prefs;                      // everything remembered between runs (theme, text size, ...); see ui/settings.h
    int lab = 0;                           // the lab on screen: 0 Orbit Lab, 1 Atom Lab, 2 Molecule Lab, 3 Black Hole Lab (app::labName; each keeps its state while another is shown)
    bool visited[app::kLabCount] = {};      // labs already opened this run (reduced motion pauses a lab the first time it opens)
    atom::AtomLab atom;
    atom::AtomView atomView;
    double tempK = 293.15;                 // the Temperature slider, K
    bool tempDetails = false;              // "What is temperature, really?" is open
    int atomLearn = 0;                     // the open Atom Lab guide: 0 none, 1 look, 2 scale, 3 weight, 4 timeline
    int molLearn = 0;                      // 0.25: the open Molecule Lab guide: 0 none, 1 What am I looking at?
    std::string molLearnJump;              // "top": scroll to the top
    std::string atomLearnJump;             // a section to scroll to ("top", "bar" or a section key)
    std::string scaleTab = "scale", massTab = "parts";
    int scaleObj = 3;                      // the Scale guide's object (a football)
    // ---- Molecule Lab (0.18 stage 1) ----
    molecule::MoleculeLab molecule;
    molecule::MoleculeView moleculeView;
    // ---- Black Hole Lab (0.55.0) ----
    blackhole::Settings blackhole = blackhole::settingsDefaults();
    blackhole::BlackHoleView blackholeView;
    double bhFreeX = 0, bhFreeY = 0, bhFreeW = -1, bhFreeH = -1;
    double bhGasT = 0;                                                // 0.72.0: the moving gas's far time (GM/c^3; blackhole/lab.h gasTick)   // the part of the picture the bars and the Controls panel leave free (points): the hole is at its middle
    int bhLearn = 0;                       // the open Black Hole Lab guide: 0 none, 1 What is real in this picture
    std::string bhLearnJump;
    int drawCalls = 0, drawInstances = 0;  // what the renderer drew last frame (the Atom Lab's Performance card)
};

// Puts everything back where it is today and resets the view. The Time speed is NOT changed.
void resetToToday(AppState& app);

// "1 s = 29.1 days": what a Time speed slider value means.
std::string tspeedText(double tspeed);
// The zoom at which something km in radius is wantPx points on screen, at true scale.
double zoomFor(const AppState& app, double km, double wantPx);

// "×0.06": what the Zoom slider says (0.15 stage 3: the new UI shows the same words).
std::string zoomLabel(double v);

// The "↺ Reset view" button: the view, the zoom and the offset back (0.15 stage 3: shared with the new UI).
void resetViewAction(AppState& app);

// Changes the zoom, keeping the middle of the picture in place.
void setZoom(AppState& app, double zoom);        // at once (a direct set: the Journey, Learn, a benchmark scene, the Zoom slider); cancels a running zoom glide
// 0.74.0: an eased zoom (the wheel and the + / − keys: kWheelZoomSeconds; a pick: OrbitView::kGlideSeconds). Reduce motion, a Journey or the benchmark: at once.
void glideZoom(AppState& app, double zoom, double seconds);
void zoomBy(AppState& app, double dz);            // a wheel notch or a key: from where a running glide is going
void cameraTick(AppState& app, double dt);        // each frame: the zoom glide's step (it ends exactly on its value)
constexpr double kWheelZoomSeconds = 0.15;

// The Performance card: frame times (last, average, worst, a graph) and the VSync switch.
void performanceCard(AppState& app);

}  // namespace ui
