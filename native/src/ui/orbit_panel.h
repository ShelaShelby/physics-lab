// The Orbit Lab controls on the right: Reset to today, Time, View, extras and Go to.
#pragma once

#include "app/prefs.h"
#include "atom/atom_sim.h"
#include "atom/atom_view.h"
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
    bool showLabLog = false;               // the lab's changelog (the Update badge)
    bool showAbout = false, showHistory = false;
    int pane = 0;                          // the tabs: 0 the labs (🔬 Simulations), 1 📚 Learn, 2 🏆 Achievements
    int learnBack = -1;                    // a guide opened from the Learn tab in another lab: the lab to go back to when it closes
    bool showSettings = false;
    ShotUi shot;
    std::shared_ptr<BenchState> bench;     // the benchmark: its window, its run and the settings it will give back
    bool benchRunning = false;             // a benchmark is running: nothing is written to the settings file
    double canvasW = 800, canvasH = 600;   // picture size in points (for the Go to zoom levels)
    bool vsync = true;                     // wait for the screen's refresh (off: draw as fast as possible, to measure headroom)
    FrameStats stats;
    Journey journey;
    char dateText[16] = "";                // Jump to a date: what is typed in
    char timeText[8] = "12:00";
    std::string dateNote;                  // the note under it
    int learn = 0;                         // the open Learn guide: 0 none, 1 What am I looking at?, 2 seasons, 3 timeline
    std::string learnJump;                 // a section to scroll to in the open guide ("bar" = back to the list of headings)
    // ---- Atom Lab ----
    app::Prefs prefs;                      // everything remembered between runs (theme, text size, unlocks, ...); see ui/settings.h
    int lab = 0;                           // the lab on screen: 0 Orbit Lab, 1 Atom Lab (each keeps its state while the other is shown)
    bool visited[2] = {false, false};      // labs already opened this run (reduced motion pauses a lab the first time it opens)
    atom::AtomLab atom;
    atom::AtomView atomView;
    double tempK = 293.15;                 // the Temperature slider, K (kept while you switch elements, as in the web)
    bool bigTable = false;                 // the large periodic table window is open
    bool tempDetails = false;              // "What is temperature, really?" is open
    int atomLearn = 0;                     // the open Atom Lab guide: 0 none, 1 look, 2 scale, 3 weight, 4 timeline
    std::string atomLearnJump;             // a section to scroll to ("top", "bar" or a section key)
    std::string scaleTab = "scale", massTab = "parts";
    int scaleObj = 3;                      // the Scale guide's object (a football)
    int drawCalls = 0, drawInstances = 0;  // what the renderer drew last frame (the Atom Lab's Performance card)
};

// Puts everything back where it is today and resets the view and time speed (the web's "☉ Reset to today").
void resetToToday(AppState& app);

// "1 s = 29.1 days": what a Time speed slider value means (as in the web version).
std::string tspeedText(double tspeed);
// The zoom at which something km in radius is wantPx points on screen, at true scale (the web's zoomFor).
double zoomFor(const AppState& app, double km, double wantPx);

// Changes the zoom, keeping the middle of the picture in place.
void setZoom(AppState& app, double zoom);

// The Performance card: frame times (last, average, worst, a graph) and the VSync switch.
void performanceCard(AppState& app);

// Draws the panel in a window `width` points wide on the right of the main window.
void orbitPanel(AppState& app, float width);

}  // namespace ui
