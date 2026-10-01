// The info card: the title and facts shown for whatever is selected.
// Ported from the web Orbit Lab (cardText0, cometCard).
#pragma once

#include <string>
#include <vector>

#include "solar_system.h"

namespace orbit {

// What is selected in the picture.
struct Selection {
    enum Kind { None, Star, Bary, Body, Moon } kind = None;
    int bodyId = 0;     // Body and Moon: the body's id (for a moon: its planet)
    int moon = 0;       // Moon: index into the planet's moons
    bool operator==(const Selection& o) const { return kind == o.kind && (kind == None || kind == Star || kind == Bary || (bodyId == o.bodyId && (kind != Moon || moon == o.moon))); }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

struct Card {
    std::string title;
    std::vector<std::string> lines;
};

struct CardOptions {
    double tspeed = 0;          // Time speed slider value (moons: laps per second)
    bool trueScale = true;      // Sizes: True scale (else Easy to see)
    bool moonsOn = true;
    double moonExaggeration = 1;   // how many times too wide the selected moon's orbit is drawn
};

const Body* findBody(const SolarSystem& sys, int id);

// Returns false when there is nothing to show (nothing selected, or it is gone).
bool cardText(const SolarSystem& sys, const Selection& sel, const CardOptions& opt, Card& out);

}  // namespace orbit
