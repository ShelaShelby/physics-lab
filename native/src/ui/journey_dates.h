// "Journey and dates": the 🚀 Journey (an automatic zoom from Earth to the centre of our galaxy)
// and Jump to a date. Ported from the web Orbit Lab (6.5–7.7.2), with two web bugs fixed:
// - when a Journey stops (at the end, or because you touched something) ALL your settings and your
//   view come back, not only the Time speed;
// - a date outside 1800–2100 (or one that is not a date) gets a note saying so, instead of a silent jump.
#pragma once

#include <cstdint>
#include <string>

#include "orbit/card_text.h"
#include "orbit/orbit_view.h"

namespace ui {

struct AppState;

struct Journey {
    bool active = false;
    int stop = 0;                  // which stop of the trip
    bool moving = false;           // zooming between two stops (else holding at one, with its caption)
    double t = 0;                  // seconds in this part
    double z0 = 0, z1 = 0, T = 0;  // zoom from, to, and how long the move takes (s)
    double zoom = 0;               // the zoom the Journey set last (if it changes otherwise, you touched it)
    bool pan = false;              // gliding from Earth to the Sun
    // everything that is put back when the Journey stops
    orbit::ViewSettings savedSet;
    orbit::Selection savedSel;
    bool savedFollow = false, savedCardHidden = false, savedPaused = false, savedSpin = false, savedPanMode = false;
    double savedOx = 0, savedOy = 0, savedCdep = 0, savedYaw = 0, savedPitch = 0;
};

void journeyStart(AppState& app);
void journeyStop(AppState& app);           // puts your settings and view back
void journeyTick(AppState& app, double dt);

// Jump to a date. Returns the note to show under the date.
std::string jumpToDate(AppState& app, int64_t unixMs);
// Reads "YYYY-MM-DD" and "HH:MM" (Greenwich time). False if it is not a real date and time.
bool parseDateTime(const char* date, const char* time, int64_t& unixMs);


}  // namespace ui
