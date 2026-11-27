// 0.85.0 (outside review, PLAN_REVIEW.md R3; the owner's answers of 25 Nov 2026): real sunspot groups on the Orbit Lab's Sun, for dates where they were measured.
// Sources (tools/make_sunspot_table.py packs them into data/sun/spots/sunspot_groups.bin, built into the program):
//  - 1874-05-09 to 2016-10-02: the Royal Greenwich Observatory - USAF/NOAA sunspot group records from NASA Marshall (D. Hathaway, solarscience.msfc.nasa.gov/greenwch.shtml;
//    format: native/data/sun/spots/format.extract.txt). One row per group per day: the corrected whole-spot area (millionths of the solar hemisphere), the Carrington
//    longitude and the latitude. The raw files are used: NASA's derived data raise the USAF/NOAA areas after 1976 by a factor of 1.4 (msfc_greenwch_page.extract.txt); the app does not.
//  - from 2016-10-03: NOAA SWPC's daily Solar Region Summary (SRS), archived by NOAA NCEI (preliminary data, SWPC says): location, Carrington longitude (Lo), area.
// A group is drawn as one round spot of its measured area at its Carrington longitude and latitude: a spherical cap whose area is the group's
// (1 - cos rho = area x 1e-6). A group holds several spots: the round spot is where the group is and how much of the Sun it covers, not its shape (no record gives it).
// 0.94.0: the spots are drawn dark (umbra and penumbra, render/sun_surface.h). Until 0.93.0 only their outlines were drawn (no fitting contrast had been found).
// Carrington longitudes are fixed on the Sun's IAU frame (orbit/spin.cpp: W = 84.176 + 14.1844 d, the rate Carrington's longitudes turn at): the group turns with
// the Sun as one body from the record's time to the shown time (at most half a day: no differential rotation in between). East longitudes (tests/test_review085.inc
// checks the records' own disc places against this frame).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace orbit {

constexpr double kSpotsDay0JD = 2405524.5;        // day 0 of the table: 1874-01-01 00:00 UT (JD 2400000.5 is 1858-11-17 00:00 UT, 5,524 days before)

struct SpotGroup { double latDeg = 0, lonDeg = 0, area = 0; };   // area: millionths of the solar hemisphere
struct SpotDay {
    enum Status { NoRecord, Spotless, NotSaid, Groups } status = NoRecord;
    int day = -1;                                  // the record's day (days from 1874-01-01)
    double recordJD = 0;                           // the record's time (UT)
    std::vector<SpotGroup> groups;
};
struct SpotTableInfo {
    bool ok = false;
    std::string why;                               // when not ok
    int rows = 0, missing = 0, first = 0, last = 0, srsFirst = 0, midnightFrom = 0;
    double loadMs = 0;                             // the time the first use took to read the table (the benchmark's response line)
};

const SpotTableInfo& sunspotTable();              // reads the built-in table on its first call
void sunspotTableReset();                         // drops it (the benchmark's response line: the next use reads it again)
SpotDay sunspotsAt(double jdUT);                  // the record nearest the time (UT Julian day)
// the group's centre on the Sun's ball in its own IAU frame (0 meridian, 90 E, north pole) and its angular radius (radians)
void spotGeometry(const SpotGroup& g, double v[3], double& rho);
// "No sunspot record for this date" and the others; the card's lines
std::string spotSentence(const SpotDay& d);
std::vector<std::string> spotCardLines(const SpotDay& d);
// for the tests: the rows of a day exactly as stored (latitude x 10, longitude x 10, area)
std::vector<std::array<int, 3>> spotRowsOfDay(int day);
int spotDayOf(int y, int m, int d);               // days from 1874-01-01
void setSunSurfaceCardLines(std::vector<std::string> lines);   // 0.94.0: the photo or the drawn surface (orbit/sun_photo.h), for the card
const std::vector<std::string>& sunSurfaceCardLines();

}  // namespace orbit
