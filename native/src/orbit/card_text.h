// The info card: the title and facts shown for whatever is selected.
#pragma once

#include <string>
#include <vector>

#include "solar_system.h"

namespace orbit {

// What is selected in the picture.
struct Selection {
    enum Kind { None, Star, Bary, Body, Moon, Belt, Far, ACen } kind = None;   // ACen (0.73.0): a thing of the Alpha Centauri scene (orbit/alphacen.h Thing)
    int bodyId = 0;     // Body and Moon: the body's id (for a moon: its planet)
    int moon = 0;       // Moon: index into the planet's moons; Belt: index into SolarSystem::belts; Far: far-away thing number; ACen: acen::Thing
    bool operator==(const Selection& o) const {
        if (kind != o.kind) return false;
        if (kind == Body) return bodyId == o.bodyId;
        if (kind == Moon) return bodyId == o.bodyId && moon == o.moon;
        if (kind == Belt || kind == Far || kind == ACen) return moon == o.moon;
        return true;
    }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

// 0.75.2: the spin's outward push at the equator, ω² R_eq in m/s² (ω from NASA's sidereal rotation period); radiusM the equatorial radius in metres.
double spinPushMS2(const std::string& name, double radiusM);

struct Card {
    std::string title;
    std::vector<std::string> lines;
};

struct CardOptions {
    double tspeed = 0;          // Time speed slider value (moons: laps per second)
    bool trueScale = true;      // Sizes: True scale (else Easy to see)
    bool moonsOn = true;
    bool beltsOn = true;
    bool farOn = true;
    bool paused = false;        // a fast spin is only "held still" while time runs
    double moonExaggeration = 1;   // how many times too wide the selected moon's orbit is drawn
};

const Body* findBody(const SolarSystem& sys, int id);

// Which things the settings show (Solar System extras and Far-away things).
struct SelectionSwitches { bool dwarfs = true, comets = true, moons = true, belts = true, far = true; };
// Does the selected thing still exist and show? False when it is gone (a body removed), hidden by a switch (Dwarf planets, Comets, Moons, Belts, Far-away things), not there on this date (the
// Voyagers before 1990), or a number that does not exist. Then the drawing drops the selection. (0.10.5 stage 5: moved here from orbit_view.cpp so it can be tested.)
bool selectionStillValid(const SolarSystem& sys, const Selection& sel, const SelectionSwitches& sw);
// The line at the top left of the picture: the date shown, and how long after the start date.
std::string dateLine(const SolarSystem& sys);

// Returns false when there is nothing to show (nothing selected, or it is gone).
bool cardText(const SolarSystem& sys, const Selection& sel, const CardOptions& opt, Card& out);

}  // namespace orbit
