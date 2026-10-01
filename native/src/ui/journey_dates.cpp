#include "journey_dates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "orbit/format.h"
#include "orbit/sky.h"
#include "orbit/units.h"
#include "ui/orbit_panel.h"

namespace ui {

using namespace orbit;
using namespace units;

namespace {

// Days since 1 Jan 1970 for a calendar date (proleptic Gregorian; H. Hinnant's algorithm)
int64_t daysFromCivil(int64_t y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
int64_t utcMs(int y, int mo, int d, int h = 0, int mi = 0) { return (daysFromCivil(y, mo, d) * 1440 + h * 60 + mi) * 60000; }

// NASA's orbital elements are made for 1800–2050; the lab goes to 2100, a little less exact
const int64_t kDateMin = utcMs(1800, 1, 1), kDateMax = utcMs(2100, 12, 31, 23, 59), kTableEnd = utcMs(2050, 12, 31);

struct Stop {
    double span;          // width of the picture in AU (0: use zoomFn)
    bool earth;           // stay on Earth
    bool end;             // the last stop
    const char* cap;
};
// each stop: how far out, and what to say (the web's JNY; the last caption now says your view comes back)
const Stop kStops[] = {
    {0, true, false, "Earth, where we live: 12,742 km across. It turns once in 23 h 56 min, with its axis leaning 23.4°. That lean gives us the seasons."},
    {0.0075, true, false, "The Moon circles Earth 384,400 km away. Light from it takes 1.3 seconds to reach us."},
    {2.6, false, false, "Earth’s path round the Sun. Its distance, 149.6 million km, is 1 AU. Sunlight takes 8.3 minutes to get here."},
    {70, false, false, "All eight planets. Neptune is 30 AU out: sunlight takes 4 hours to reach it."},
    {420, false, false, nullptr},   // Voyager 1 (worked out for the date shown)
    {60000, false, false, "The Oort cloud (never seen directly): maybe trillions of icy comets, 2,000 to 200,000 AU out. Voyager 1 needs about 300 years to reach it."},
    {1.0e6, false, false, "The nearest star system, Alpha Centauri, is 4.3 light years away: the light we see from it is 4.3 years old."},
    {2.5e8, false, false, "Real stars at their real places: the Pleiades (444 light years) and the Orion Nebula (1,267 light years)."},
    {0, false, true, "The centre of our galaxy, 27,000 light years away, where a black hole of 4.3 million Suns sits (Sagittarius A*). Then your own view and settings come back."},
};
constexpr int kStopCount = sizeof(kStops) / sizeof(kStops[0]);
constexpr double kSpeed = 0.65;              // decades of zoom per second between stops

std::string caption(const AppState& app, int k) {
    if (kStops[k].cap) return kStops[k].cap;
    FarItem f;
    if (farItem(app.sys, farIndexOf("v1"), f))
        return "Voyager 1 is the most distant spacecraft: " + fmt::commas(f.au) + " AU out, " + fmt::fixed(f.au * 499.005 / 3600, 0) +
               " light-hours. It left the Sun’s wind (the heliopause) at 121 AU in 2012.";
    return "The Kuiper belt: icy worlds such as Pluto, out to about 50 AU. Voyager 1 is not launched yet on this date.";
}
double holdTime(const std::string& cap) { return 2.2 + cap.size() / 18.0; }   // seconds at a stop: longer text, longer stay
double stopZoom(const AppState& app, int k) {
    if (k == 0) return zoomFor(app, 6371, 70);
    if (kStops[k].end) return -9;
    return std::clamp(std::log10(7.325 / kStops[k].span), -9.0, 5.5);   // 7.325 = 34 / (1 AU in sim units): a picture span AU wide
}

}  // namespace

void journeyStart(AppState& app) {
    Journey& J = app.journey;
    const Body* earth = nullptr;
    for (const Body& b : app.sys.bodies) if (b.name == "Earth") earth = &b;
    if (!earth) return;
    // remember everything the Journey changes
    J.savedSet = app.set; J.savedSel = app.view.sel; J.savedFollow = app.view.follow; J.savedCardHidden = app.view.cardHidden;
    J.savedPaused = app.paused; J.savedSpin = app.view.spin; J.savedPanMode = app.view.panMode;
    J.savedOx = app.view.ox; J.savedOy = app.view.oy; J.savedCdep = app.view.cdep; J.savedYaw = app.view.yaw; J.savedPitch = app.view.pitch;
    app.set.far = app.set.belts = app.set.moons = true;
    app.set.trueScale = true;
    app.set.tspeed = -3.145;                 // 1 s = about 1 hour: Earth can be seen turning and the planets stay where they are
    app.view.sel = {Selection::Body, earth->id};
    app.view.follow = true; app.view.ox = app.view.oy = 0; app.view.cdep = 0;
    J.active = true; J.stop = 0; J.moving = false; J.t = 0; J.pan = false;
    J.zoom = std::clamp(stopZoom(app, 0), kZoomMin, kZoomMax);
    app.set.zoom = J.zoom;
    app.view.journey = true;
    app.view.caption = caption(app, 0);
    app.view.captionAlpha = 0;
}

void journeyStop(AppState& app) {
    Journey& J = app.journey;
    if (!J.active) return;
    J.active = false;
    app.view.journey = false;
    app.view.caption.clear();
    // everything back as it was before the Journey (the web version only put the Time speed back)
    app.set = J.savedSet;
    app.view.sel = J.savedSel; app.view.follow = J.savedFollow; app.view.cardHidden = J.savedCardHidden;
    app.paused = J.savedPaused; app.view.spin = J.savedSpin; app.view.panMode = J.savedPanMode;
    app.view.ox = J.savedOx; app.view.oy = J.savedOy; app.view.cdep = J.savedCdep; app.view.yaw = J.savedYaw; app.view.pitch = J.savedPitch;
}

void journeyTick(AppState& app, double dt) {
    Journey& J = app.journey;
    if (!J.active) return;
    if (std::fabs(app.set.zoom - J.zoom) > 1e-6) { journeyStop(app); return; }   // you moved the zoom yourself
    dt = std::min(dt, 0.1);
    J.t += dt;
    if (J.pan) { const double f = std::exp(-2.4 * dt); app.view.ox *= f; app.view.oy *= f; }   // glide to the Sun
    if (!J.moving) {
        const std::string cap = caption(app, J.stop);
        const double hold = holdTime(cap), fade = 0.45;
        app.view.caption = cap;
        app.view.captionAlpha = std::clamp(std::min(J.t / fade, (hold - J.t) / fade), 0.0, 1.0);
        if (J.t >= hold) {
            if (kStops[J.stop].end) { journeyStop(app); return; }
            J.stop++; J.moving = true; J.t = 0;
            J.z0 = app.set.zoom; J.z1 = stopZoom(app, J.stop);
            J.T = std::max(2.0, std::fabs(J.z1 - J.z0) / kSpeed);
            app.view.captionAlpha = 0;
            if (!kStops[J.stop].earth && app.view.follow) {   // let go of Earth: the view glides to the Sun
                app.view.follow = false; app.view.sel = Selection{}; app.view.cdep = 0; J.pan = true;
            }
        }
    } else {
        const double u = std::min(1.0, J.t / J.T), e = u * u * (3 - 2 * u);   // smooth start and stop
        app.view.captionAlpha = 0;
        setZoom(app, J.z0 + (J.z1 - J.z0) * e);
        J.zoom = app.set.zoom;
        if (u >= 1) { J.moving = false; J.t = 0; }
    }
}

bool parseDateTime(const char* date, const char* time, int64_t& ms) {
    int y = 0, mo = 0, d = 0, h = 12, mi = 0;
    char extra = 0;
    if (std::sscanf(date, "%d-%d-%d%c", &y, &mo, &d, &extra) != 3) return false;
    if (time && time[0] && std::sscanf(time, "%d:%d%c", &h, &mi, &extra) != 2) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
    // reject dates such as 30 February: the day must come back the same
    const int64_t days = daysFromCivil(y, mo, d);
    if (daysFromCivil(y, mo, 1) + d - 1 != days || (mo < 12 ? daysFromCivil(y, mo + 1, 1) : daysFromCivil(y + 1, 1, 1)) <= days) return false;
    ms = utcMs(y, mo, d, h, mi);
    return true;
}

std::string jumpToDate(AppState& app, int64_t ms) {
    journeyStop(app);
    std::string note;
    if (ms < kDateMin || ms > kDateMax) {     // the web silently moved such a date; now it says so
        const int64_t asked = ms;
        ms = std::clamp(ms, kDateMin, kDateMax);
        note = fmt::dateUTC(asked) + " is outside 1800–2100, the years this lab can show (NASA/JPL’s orbit numbers are made for 1800–2050). "
               "Showing the nearest date it can: " + fmt::dateUTC(ms) + ". ";
    }
    // keep the selected planet or moon selected, by name
    std::string selName;
    int moonIdx = -1;
    if (app.view.sel.kind == Selection::Body || app.view.sel.kind == Selection::Moon)
        if (const Body* b = findBody(app.sys, app.view.sel.bodyId)) { selName = b->name; if (app.view.sel.kind == Selection::Moon) moonIdx = app.view.sel.moon; }
    app.sys.loadAt(ms);
    app.sys.jumped = true;
    if (!selName.empty()) {
        Selection s{};
        for (const Body& b : app.sys.bodies)
            if (b.name == selName) s = moonIdx >= 0 ? Selection{Selection::Moon, b.id, moonIdx} : Selection{Selection::Body, b.id};
        app.view.sel = s;
        if (s.kind == Selection::None) app.view.follow = false;
    }
    if (note.empty()) note = "Showing " + fmt::dateUTC(ms) + ". ";
    if (ms > kTableEnd) note += "After 2050 the planets’ places are extrapolated beyond NASA’s table (1800–2050), so they get slowly less exact. ";
    note += "The moons start at made-up places (only the planets, dwarf planets, comets and belts are real).";
    return note;
}

void journeyDatesCard(AppState& app, bool wasActive) {
    ImGui::SeparatorText("Journey and dates");
    if (ImGui::Button(app.journey.active ? "■ Stop the journey" : "Journey: Earth to the galactic centre", ImVec2(-1, 0))) {
        if (app.journey.active) journeyStop(app);
        else if (!wasActive) journeyStart(app);   // this click already stopped a running Journey
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("An automatic trip that zooms out, step by step, from Earth to the centre of our galaxy.\nTouch anything to stop it; your view and settings come back.");
    ImGui::TextUnformatted("Jump to a date (Greenwich time)");
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(w * 0.42f);
    ImGui::InputTextWithHint("##date", "YYYY-MM-DD", app.dateText, sizeof app.dateText);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w * 0.2f);
    ImGui::InputTextWithHint("##time", "HH:MM", app.timeText, sizeof app.timeText);
    ImGui::SameLine();
    if (ImGui::Button("Jump", ImVec2(-1, 0))) {
        int64_t ms;
        if (!app.dateText[0]) app.dateNote = "Pick a date first.";
        else if (!parseDateTime(app.dateText, app.timeText, ms)) app.dateNote = "That is not a date the lab can read: write it as YYYY-MM-DD (for example 2003-08-27), and the time as HH:MM.";
        else app.dateNote = jumpToDate(app, ms);
    }
    struct J { const char* name; const char* date; const char* tip; };
    static const J jumps[] = {{"Today", nullptr, "Back to now"},
                              {"Halley at its closest, 1986", "1986-02-09", "Halley’s Comet at its closest to the Sun: 9 Feb 1986"},
                              {"Neptune found, 1846", "1846-09-23", "Johann Galle found Neptune on the night of 23 Sep 1846, where Le Verrier’s sums said it would be"},
                              {"Mars closest, 2003", "2003-08-27", "Mars at its closest to Earth in 2003: 55.76 million km on 27 Aug 2003"},
                              {"Venus crosses the Sun, 2012", "2012-06-06", "The last transit of Venus: 5–6 Jun 2012"},
                              {"Great conjunction, 2020", "2020-12-21", "Jupiter and Saturn looked almost touching: 21 Dec 2020"},
                              {"Halley returns, 2061", "2061-07-28", "Halley’s Comet is due back on 28 Jul 2061 (a prediction)"}};
    for (int i = 0; i < 7; i++) {                         // one tile per row, so the names fit
        if (ImGui::Button(jumps[i].name, ImVec2(-1, 0))) {
            if (!jumps[i].date) { journeyStop(app); resetToToday(app); app.dateNote = "Back to today."; }
            else {
                std::snprintf(app.dateText, sizeof app.dateText, "%s", jumps[i].date);
                std::snprintf(app.timeText, sizeof app.timeText, "12:00");
                int64_t ms;
                if (parseDateTime(app.dateText, app.timeText, ms)) app.dateNote = jumpToDate(app, ms);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", jumps[i].tip);
    }
    ImGui::TextWrapped("%s", app.dateNote.empty() ? "Pick any date from 1800 to 2100 (time in UT, Greenwich time): the planets, dwarf planets, comets and belts move to where they really were or will be." : app.dateNote.c_str());
}

}  // namespace ui
