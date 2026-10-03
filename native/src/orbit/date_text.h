// The wording of the Orbit Lab's "Jump to a date" notes and the Journey's Voyager fallback caption (0.10.5 stage 2, from the stage 1 audit, rows 1, 5, 6 and 10).
// No graphics, so it can be tested; ui/journey_dates.cpp only puts the pieces together.
#pragma once

#include <string>

namespace orbit {

// What the date box says before any date is picked.
inline const char* datePickHint() {
    return "Pick any date from 1800 to 2100 (time in UT, Greenwich time): the planets (and, in Real, Earth’s Moon) follow real data; dwarf planets and comets move on a simple orbit "
           "fixed at one date, so they drift away from their real places the further the date is from it.";
}

// The sentences that follow "Showing <date>." after a jump.
//  after2050: the date is after 2050 (the end of NASA's table of orbital elements, which only Simple uses)
//  real: Planet physics is Real; realMoon: Earth's Moon is the real one on that date (ELP2000-82B: 1800 to 1 Jan 2100 12:00 TDB)
// Measured against JPL Horizons (audit, REPORT of 0.10.5 stage 1): the lab's Pluto is about 1.07 AU off in 1800.
inline std::string dateNotes(bool after2050, bool real, bool realMoon) {
    std::string n;
    if (after2050 && !real) n += "After 2050 the planets’ places are extrapolated beyond NASA’s table (1800–2050), so they get slowly less exact. ";
    if (real) n += "In Real the planets’ errors were measured against JPL for 1800 to 1 Jan 2100 only. ";
    n += "Dwarf planets and comets move on a simple orbit fixed at one date, so they drift away from their real places the further the date is from it (Pluto is about 1 AU off in 1800). ";
    if (real && realMoon) n += "Earth’s Moon is the real one (ELP2000-82B, 1800 to 1 Jan 2100); the other moons and the belt rocks start at made-up places.";
    else if (real) n += "Earth’s Moon is real only from 1800 to 1 Jan 2100, so here it is on a simple orbit like the other moons; the moons and the belt rocks start at made-up places.";
    else n += "The moons and the belt rocks start at made-up places.";
    return n;
}

// The Journey's Voyager caption on a date when Voyager 1 cannot be shown (the app shows the Voyagers from `shownFromYear` on).
inline std::string voyagerFallbackCaption(int year, int launchYear, int shownFromYear) {
    const std::string kuiper = "The Kuiper belt: icy worlds such as Pluto, out to about 50 AU. ";
    if (year < launchYear) return kuiper + "Voyager 1 is not launched yet on this date.";
    return kuiper + "Voyager 1 launched in " + std::to_string(launchYear) + "; this lab shows it from " + std::to_string(shownFromYear) + " on.";
}

}  // namespace orbit
