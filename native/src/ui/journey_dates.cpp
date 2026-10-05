#include "fonts.h"
#include "style_draw.h"
#include "journey_dates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "orbit/belts.h"
#include "orbit/date_text.h"
#include "orbit/format.h"
#include "orbit/sky.h"
#include "orbit/units.h"
#include "ui/orbit_panel.h"

namespace ui {

using namespace orbit;
using namespace units;

namespace {

struct Stop {
    double span;          // width of the picture in AU (0: use zoomFn)
    bool earth;           // stay on Earth
    bool end;             // the last stop
    const char* cap;
};
// each stop: how far out, and what to say (the web's JNY, word for word)
const Stop kStops[] = {
    {0, true, false, "Earth, where we live: 12,742 km across. It turns once in 23 h 56 min, with its axis leaning 23.4°. That lean gives us the seasons."},
    {0.0075, true, false, "The Moon circles Earth 384,400 km away. Light from it takes 1.3 seconds to reach us."},
    {2.6, false, false, "Earth’s path round the Sun. Its distance, 149.6 million km, is 1 AU. Sunlight takes 8.3 minutes to get here."},
    {70, false, false, "All eight planets. Neptune is 30 AU out: sunlight takes 4 hours to reach it."},
    {420, false, false, nullptr},   // Voyager 1 (worked out for the date shown)
    {60000, false, false, nullptr},   // the Oort cloud: its travel time is worked out from Voyager 1's speed (orbit/belts.cpp)
    {1.0e6, false, false, "The nearest star system, Alpha Centauri, is 4.3 light years away: the light we see from it is 4.3 years old."},
    {2.5e8, false, false, "Real stars at their real places: the Pleiades (444 light years) and the Orion Nebula (1,267 light years)."},
    {0, false, true, "The centre of our galaxy, 27,000 light years away, where a black hole of 4.3 million Suns sits (Sagittarius A*). Tap Reset view to come back."},
};
constexpr int kStopCount = sizeof(kStops) / sizeof(kStops[0]);
constexpr double kSpeed = 0.65;              // decades of zoom per second between stops

std::string caption(const AppState& app, int k) {
    if (kStops[k].cap) return kStops[k].cap;
    if (k == 5) return oortCaption();
    return voyagerCaption(app.sys);   // orbit/sky.cpp, tested (Voyager 1 where it is shown, the Kuiper belt text before 1990)
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

bool parseDateTime(const char* date, const char* time, int64_t& ms) { return orbit::parseDateTime(date, time, ms); }   // orbit/date_text.h, tested

std::string jumpToDate(AppState& app, int64_t ms) {
    journeyStop(app);
    std::string clampNote;
    ms = clampJumpDate(ms, clampNote);        // a date outside 1800-2100 is moved, and says so (orbit/date_text.h, tested)
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
    return jumpMessage(clampNote, ms, app.set.realPlanets, app.set.realPlanets && app.sys.realMoon());   // orbit/date_text.h, tested
}

void journeyDatesCard(AppState& app, bool wasActive) {
    ImGui::SeparatorText("Journey and dates");
    if (wideButton(app.journey.active ? "■ Stop the journey" : "Journey: Earth to the galactic centre")) {
        if (app.journey.active) journeyStop(app);
        else if (!wasActive) journeyStart(app);   // this click already stopped a running Journey
    }
    if (ImGui::IsItemHovered()) ui::SetTooltip("An automatic trip that zooms out, step by step, from Earth to the centre of our galaxy.\nTouch anything to stop it; your view and settings come back.");
    ImGui::TextUnformatted("Jump to a date (Greenwich time)");
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(w * 0.42f);
    ImGui::InputTextWithHint("##date", "YYYY-MM-DD", app.dateText, sizeof app.dateText);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w * 0.2f);
    ImGui::InputTextWithHint("##time", "HH:MM", app.timeText, sizeof app.timeText);
    ImGui::SameLine();
    if (wideButton("Jump")) {
        int64_t ms;
        if (!app.dateText[0]) app.dateNote = "Pick a date first.";
        else if (!parseDateTime(app.dateText, app.timeText, ms)) app.dateNote = "That is not a date the lab can read: write it as YYYY-MM-DD (for example 2003-08-27), and the time as HH:MM.";
        else app.dateNote = jumpToDate(app, ms);
    }
    struct J { const char* name; const char* date; const char* tip; const char* time = "12:00"; };
    static const J jumps[] = {{"Today", nullptr, "Back to now"},
                              {"Halley at its closest, 1986", "1986-02-09", "Halley’s Comet at its closest to the Sun: 9 Feb 1986"},
                              {"Neptune found, 1846", "1846-09-23", "Johann Galle found Neptune on the night of 23 Sep 1846, where Le Verrier’s sums said it would be"},
                              {"Mars closest, 2003", "2003-08-27", "Mars at its closest to Earth in 2003: 55.76 million km on 27 Aug 2003"},
                              // Time of closest approach: Venus is nearest the Sun's centre, 0.154 degrees, at 01:26 UT on 6 June 2012. JPL Horizons API (read 2026-10-03), Venus (299) seen from Earth (500@399),
                              // QUANTITIES=23 (solar elongation), START_TIME 2012-06-06 00:30, STOP_TIME 02:30, STEP_SIZE 1 min: the minimum row is 2012-Jun-06 01:26, 0.154 degrees.
                              {"Venus crosses the Sun, 2012", "2012-06-06", "The last transit of Venus: 5–6 Jun 2012 (the jump lands at its middle, 01:26 UT on 6 Jun)", "01:26"},
                              {"Great conjunction, 2020", "2020-12-21", "Jupiter and Saturn looked almost touching: 21 Dec 2020"},
                              {"Halley returns, 2061", "2061-07-28", "Halley’s Comet is due back on 28 Jul 2061 (JPL’s prediction; the lab’s simple orbit has it about 5 months later)"}};
    for (int i = 0; i < 7; i++) {                         // one tile per row, so the names fit
        if (wideButton(jumps[i].name)) {
            if (!jumps[i].date) { journeyStop(app); resetToToday(app); app.dateNote = "Back to today."; }
            else {
                std::snprintf(app.dateText, sizeof app.dateText, "%s", jumps[i].date);
                std::snprintf(app.timeText, sizeof app.timeText, "%s", jumps[i].time);
                int64_t ms;
                if (parseDateTime(app.dateText, app.timeText, ms)) app.dateNote = jumpToDate(app, ms);
            }
        }
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", jumps[i].tip);
    }
    ImGui::TextWrapped("%s", app.dateNote.empty() ? datePickHint() : app.dateNote.c_str());
}

}  // namespace ui
