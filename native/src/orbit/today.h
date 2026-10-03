// "☉ Reset to today" (and the Journey and Learn "Today" buttons): the date and time go back to now and nothing about how the owner set up the view changes.
// Reset: the date (the Solar System loads today's positions), the selection, Follow, the moved view and the 3D depth.
// NOT touched: the zoom, the Moons / Dwarf planets / Comets / Belts switches, the Time speed and every other setting. `set` is a const reference on
// purpose, so this function cannot change them. (Defaults, in ui/settings.cpp, puts the settings back itself.)
#pragma once

#include <cstdint>

#include "orbit_view.h"

namespace orbit {

inline void backToToday(SolarSystem& sys, const ViewSettings& /*set: deliberately untouched*/, OrbitView& view, int64_t nowUnixMs) {
    sys.loadAt(nowUnixMs);
    sys.jumped = false;
    view.sel = Selection{};
    view.follow = false;
    view.ox = view.oy = 0;
    view.cdep = 0;
}

}  // namespace orbit
