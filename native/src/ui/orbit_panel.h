// The Orbit Lab controls on the right: Reset to today, Time, View, extras and Go to.
#pragma once

#include "orbit/orbit_view.h"
#include "orbit/solar_system.h"
#include "ui/frame_stats.h"
#include "ui/journey_dates.h"

#include <string>

namespace ui {

struct AppState {
    orbit::SolarSystem sys;
    orbit::ViewSettings set;
    orbit::OrbitView view;
    bool paused = false;
    bool showChangelog = false;
    double canvasW = 800, canvasH = 600;   // picture size in points (for the Go to zoom levels)
    bool vsync = true;                     // wait for the screen's refresh (off: draw as fast as possible, to measure headroom)
    FrameStats stats;
    Journey journey;
    char dateText[16] = "";                // Jump to a date: what is typed in
    char timeText[8] = "12:00";
    std::string dateNote;                  // the note under it
    int learn = 0;                         // the open Learn guide: 0 none, 1 What am I looking at?, 2 seasons, 3 timeline
    std::string learnJump;                 // a section to scroll to in the open guide ("bar" = back to the list of headings)
};

// Puts everything back where it is today and resets the view and time speed (the web's "☉ Reset to today").
void resetToToday(AppState& app);

// "1 s = 29.1 days": what a Time speed slider value means (as in the web version).
std::string tspeedText(double tspeed);
// The zoom at which something km in radius is wantPx points on screen, at true scale (the web's zoomFor).
double zoomFor(const AppState& app, double km, double wantPx);

// Changes the zoom, keeping the middle of the picture in place.
void setZoom(AppState& app, double zoom);

// Draws the panel in a window `width` points wide on the right of the main window.
void orbitPanel(AppState& app, float width);

}  // namespace ui
