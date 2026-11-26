// The wording of the Orbit Lab's "Jump to a date" notes and the Journey's Voyager fallback caption (0.10.5 stage 2, from the stage 1 audit, rows 1, 5, 6 and 10).
// No graphics, so it can be tested; ui/journey_dates.cpp only puts the pieces together.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "format.h"
#include "units.h"

namespace orbit {

// ---- dates (0.10.5 stage 5: moved here from ui/journey_dates.cpp so they can be tested) ----
// Days since 1 Jan 1970 for a calendar date (proleptic Gregorian; H. Hinnant's algorithm).
constexpr int64_t daysFromCivil(int64_t y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
constexpr int64_t utcMs(int y, int mo, int d, int h = 0, int mi = 0) { return (daysFromCivil(y, mo, d) * 1440 + h * 60 + mi) * 60000; }
// NASA's orbital elements are made for 1800-2050; the lab goes to 2100, a little less exact.
constexpr int64_t kDateMinMs = utcMs(1800, 1, 1), kDateMaxMs = utcMs(2100, 12, 31, 23, 59), kTableEndMs = utcMs(2050, 12, 31);

// "YYYY-MM-DD" and "HH:MM" (empty time = 12:00) to Unix milliseconds (UT). False for anything that is not a real date and time.
inline bool parseDateTime(const char* date, const char* time, int64_t& ms) {
    long long y = 0, mo = 0, d = 0, h = 12, mi = 0;         // (0.19: read as long long, so a long number is not cut to a small one by an int)
    char extra = 0;
    if (std::sscanf(date, "%lld-%lld-%lld%c", &y, &mo, &d, &extra) != 3) return false;
    if (time && time[0] && std::sscanf(time, "%lld:%lld%c", &h, &mi, &extra) != 2) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
    if (y < -100000000 || y > 100000000) return false;      // 0.19: further away the milliseconds would not fit in 64 bits (the lab's range is 1800-2100 anyway)
    // reject dates such as 30 February: the day must come back the same
    const int64_t days = daysFromCivil(y, int(mo), int(d));
    if (daysFromCivil(y, int(mo), 1) + d - 1 != days || (mo < 12 ? daysFromCivil(y, int(mo) + 1, 1) : daysFromCivil(y + 1, 1, 1)) <= days) return false;
    ms = (days * 1440 + h * 60 + mi) * 60000;
    return true;
}

// A date outside 1800-2100 is moved to the nearest one the lab can show, and `note` says so (empty when the date was fine).
inline int64_t clampJumpDate(int64_t askedMs, std::string& note) {
    note.clear();
    if (askedMs >= kDateMinMs && askedMs <= kDateMaxMs) return askedMs;
    const int64_t ms = askedMs < kDateMinMs ? kDateMinMs : kDateMaxMs;
    note = fmt::dateUTC(askedMs) + " is outside 1800–2100, the years this lab can show (NASA/JPL’s orbit numbers are made for 1800–2050). Showing the nearest date it can: " + fmt::dateUTC(ms) + ". ";
    return ms;
}

// What the Time speed slider means: "1 s = 29 days".
inline std::string tspeedText(double v) {
    const double d = units::kSimTime / units::kDay * std::pow(10.0, v), sec = d * 86400;   // simulated days / seconds per real second
    if (sec < 1.05) return "1 s = 1 second (real time)";
    std::string t;
    if (sec < 60) t = fmt::fixed(sec, sec < 10 ? 1 : 0) + " seconds";
    else if (sec < 3600) t = fmt::fixed(sec / 60, sec < 600 ? 1 : 0) + " minutes";
    else if (d < 1) t = fmt::fixed(d * 24, d * 24 < 10 ? 1 : 0) + " hours";
    else if (d < 365) t = fmt::fixed(d, d < 10 ? 1 : 0) + " days";
    else t = fmt::fixed(d / 365.25, d < 3652 ? 1 : 0) + " years";
    return "1 s = " + t;
}

// 0.46.3 (outside review: the guide said "each planet feels only the Sun's pull" in Real too): the guide's paragraph on the planet physics, for the setting in use.
inline std::string physicsGuideText(bool real) {
    if (real)
        return "<p class=\"lrnP\"><b>Planet physics: Real.</b> The planets and the dwarf planets move under the pull of the Sun <b>and of each other</b> (the dwarf planets are pulled "
               "but too light to pull back), from JPL’s positions on 1 Jan 2026, with Einstein’s correction as an option. Measured against JPL, they stay close to the real places from "
               "1800 to 2100. Simplified: the asteroids do not pull, and the moons, comets and belt rocks keep to their own fixed paths.</p>";
    // 0.75.2 (outside review): until 0.75.1 running time followed a Sun-only leapfrog that left the table's slow rates out, so it drifted from a jump to the same date; now both place each planet from the table
    return "<p class=\"lrnP\"><b>Planet physics: Simple.</b> Each planet is put where JPL’s table of approximate orbits says it is on the date, the same whether you jump to the date or run time to it, "
           "and the planets do not pull on each other. The table is made for 1800–2050 and is off by up to a few million km for the outer planets (the note under the setting gives each "
           "planet’s worst error); set Planet physics to Real for the planets’ pull on each other.</p>";
}

// 0.75.2 (outside review): time running past the edges. A jump stops at 1800 / 2100, but the fastest time speed runs on. Chosen: the lab keeps running and says so under the date, instead of a stop at
// the edge (a stop would end a long run at the fastest speed with no way to look further; the owner can still read the date). Beyond the edge: Delta T (UT - TDB) is held at its value after the last
// leap second, 69.2 s (it has been held there since 2017, not only after 2100: ROADMAP), the 13 big moons follow JPL's table of mean elements (0.88.0: orbit/moon_sats.h; their angle along the orbit is not real), the Moon falls back to its Kepler ellipse, and the
// accuracy of Real was never measured (real_errors.h: 1800-2100). Empty inside 1800-2100.
inline std::string outsideRangeNote(int64_t nowMs) {
    if (nowMs >= kDateMinMs && nowMs <= kDateMaxMs) return "";
    return std::string(nowMs > kDateMaxMs ? "Past 2100" : "Before 1800") + ": outside the years this lab checks against JPL. The Moon is on a simple ellipse, the 13 other big moons follow JPL's mean elements "
           "(their orbits' size, shape, tilt and period; where each one is on its orbit is not the real place), the Earth's turning (Delta T) is held at today's value, and Real's accuracy was never measured here. "
           "Treat what you see as a picture, not a prediction.";
}

// What the date box says before any date is picked.
inline const char* datePickHint() {
    return "Pick any date from 1800 to 2100 (time in UT, Greenwich time): the planets and the big moons follow real data; dwarf planets (but not in Real) and comets move "
           "on a simple orbit fixed at one date, so they drift away from their real places the further the date is from it.";
}

// The sentences that follow "Showing <date>." after a jump.
//  after2050: the date is after 2050 (the end of NASA's table of orbital elements, which only Simple uses)
//  real: Planet physics is Real; realMoon: the date is inside 1800 to 1 Jan 2100 12:00 TDB, where Earth's Moon (ELP2000-82B) and the other big moons are real, in Simple and Real
// Measured against JPL Horizons (audit, REPORT of 0.10.5 stage 1): the lab's Pluto is about 1.07 AU off in 1800.
inline std::string dateNotes(bool after2050, bool real, bool realMoon) {
    std::string n;
    if (after2050 && !real) n += "After 2050 the planets’ places are extrapolated beyond NASA’s table (1800–2050), so they get slowly less exact. ";
    if (real) n += "In Real the planets’ errors were measured against JPL for 1800 to 1 Jan 2100 only. ";
    if (real) n += "The dwarf planets move with the planets (measured like them; Ceres the least well, as the asteroids that pull on it are not in). Comets move on a simple orbit fixed at one date, so they drift away from their real places the further the date is from it. ";
    else n += "Dwarf planets and comets move on a simple orbit fixed at one date, so they drift away from their real places the further the date is from it (Pluto is about 1 AU off in 1800). ";
    // 0.46.3: Earth's Moon is real in Simple too, and the 13 other big moons have been real since 0.27 (orbit/moon_sats.h); the belt rocks are real asteroids on fixed orbits (orbit/belts.h)
    if (realMoon) n += "Earth’s Moon and the 13 other big moons are at their real places (1800 to 1 Jan 2100). ";
    else n += "The moons are real only from 1800 to 1 Jan 2100. Here our Moon is on a simple orbit that starts at a made-up angle, and the 13 other big moons follow JPL’s mean elements: "
              "their orbits' size, shape, tilt and period are JPL’s, but where each one is on its orbit is not the real place. ";
    n += "The belt rocks are real asteroids on fixed orbits (they drift like the comets); the Oort cloud dots are made up.";
    return n;
}

// The whole note after a jump: the clamp note, or "Showing <date>.", then dateNotes (see above).
inline std::string jumpMessage(const std::string& clampNote, int64_t shownMs, bool real, bool realMoon) {
    return (clampNote.empty() ? "Showing " + fmt::dateUTC(shownMs) + ". " : clampNote) + dateNotes(shownMs > kTableEndMs, real, realMoon);
}

// The Journey's Voyager caption on a date when Voyager 1 cannot be shown (the app shows the Voyagers from `shownFromYear` on).
inline std::string voyagerFallbackCaption(int year, int launchYear, int shownFromYear) {
    const std::string kuiper = "The Kuiper belt: icy worlds such as Pluto, out to about 50 AU. ";
    if (year < launchYear) return kuiper + "Voyager 1 is not launched yet on this date.";
    return kuiper + "Voyager 1 launched in " + std::to_string(launchYear) + "; this lab shows it from " + std::to_string(shownFromYear) + " on.";
}

}  // namespace orbit
