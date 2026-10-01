// The Orbit Lab controls on the right: Reset to today, Time, View, extras and Go to.
#pragma once

#include "orbit/orbit_view.h"
#include "orbit/solar_system.h"

namespace ui {

struct AppState {
    orbit::SolarSystem sys;
    orbit::ViewSettings set;
    orbit::OrbitView view;
    bool paused = false;
    bool showChangelog = false;
    double canvasW = 800, canvasH = 600;   // picture size in points (for the Go to zoom levels)
};

// Puts everything back where it is today and resets the view and time speed (the web's "☉ Reset to today").
void resetToToday(AppState& app);

// Changes the zoom, keeping the middle of the picture in place.
void setZoom(AppState& app, double zoom);

// Draws the panel in a window `width` points wide on the right of the main window.
void orbitPanel(AppState& app, float width);

}  // namespace ui
