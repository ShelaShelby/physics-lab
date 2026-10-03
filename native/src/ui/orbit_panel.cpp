#include "orbit_panel.h"
#include "atom_panel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "changelog.h"
#include "learn.h"
#include "settings.h"
#include "fonts.h"
#include "imgui.h"
#include "app_pages.h"
#include "look.h"
#include "shot.h"
#include "benchmark.h"
#include "orbit/format.h"
#include "orbit/moon_elp.h"
#include "orbit/real_errors.h"
#include "orbit/sky.h"
#include "orbit/spin.h"
#include "orbit/today.h"
#include "orbit/units.h"

namespace ui {

using namespace orbit;
using namespace units;


// "29,231" for the worst-error lists in the Real note (orbit/real_errors.h)
static std::string withCommas(long v) {
    std::string d = std::to_string(v), out;
    for (std::size_t i = 0; i < d.size(); i++) { if (i && (d.size() - i) % 3 == 0) out += ','; out += d[i]; }
    return out;
}
// "Mercury 29,231 km, Venus 4,762, ..." (the worst error of each planet against JPL Horizons, 1800-2100, from the measured table)
static std::string worstErrorList(bool relativityOn) {
    const long* km = relativityOn ? orbit::kRealErrKmRelOn : orbit::kRealErrKmRelOff;
    std::string t;
    for (int i = 0; i < orbit::kRealErrCount; i++) t += (i ? ", " : "") + std::string(orbit::kRealErrName[i]) + " " + withCommas(km[i]) + (i == orbit::kRealErrCount - 1 ? " km" : "");
    return t;
}

static int64_t nowUnixMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void setZoom(AppState& app, double zoom) {
    zoom = std::clamp(zoom, kZoomMin, kZoomMax);
    app.view.zoomChanged(app.set.zoom, zoom);
    app.set.zoom = zoom;
}

// Today: only the date (and the selection, Follow and moved view) go back; the zoom, the four switches and the Time speed are the user's own
// (orbit/today.h; Defaults puts them back itself).
void resetToToday(AppState& app) { orbit::backToToday(app.sys, app.set, app.view, nowUnixMs()); }

// "1 s = 29.1 days": what the Time speed slider means (as in the web version)
std::string tspeedText(double v) {
    const double d = kSimTime / kDay * std::pow(10.0, v), sec = d * 86400;   // simulated days / seconds per real second
    if (sec < 1.05) return "1 s = 1 second (real time)";
    std::string t;
    if (sec < 60) t = fmt::fixed(sec, sec < 10 ? 1 : 0) + " seconds";
    else if (sec < 3600) t = fmt::fixed(sec / 60, sec < 600 ? 1 : 0) + " minutes";
    else if (d < 1) t = fmt::fixed(d * 24, d * 24 < 10 ? 1 : 0) + " hours";
    else if (d < 365) t = fmt::fixed(d, d < 10 ? 1 : 0) + " days";
    else t = fmt::fixed(d / 365.25, d < 3652 ? 1 : 0) + " years";
    return "1 s = " + t;
}

static std::string zoomText(double v) {
    const double z = std::pow(10.0, v);
    return "×" + (z < 0.01 ? fmt::precision(z, 1) : fmt::fixed(z, z < 1 ? 2 : z < 10 ? 1 : 0));
}

// The zoom at which something `km` in radius is `wantPx` points on screen, at true scale (the web's zoomFor)
double zoomFor(const AppState& app, double km, double wantPx) {
    const double m = std::min(app.canvasW, app.canvasH) / 34;
    return std::clamp(std::log10(wantPx / (km * kKm / kSimLength) / m), -5.0, 5.5);
}

// The planet whose moons can be watched: the selected planet, or the planet of the selected moon
static const Body* moonTarget(const AppState& app) {
    const Selection& s = app.view.sel;
    if (s.kind != Selection::Body && s.kind != Selection::Moon) return nullptr;
    const Body* b = findBody(app.sys, s.bodyId);
    return b && !b->moons.empty() ? b : nullptr;
}

static ImVec4 tileColor(const char* hex) {
    const render::Color c = render::hexColor(hex);
    return ImVec4(c.r, c.g, c.b, 1);
}

// One Go to tile: a coloured dot and a name. Highlighted when it is the selected one.
static bool tile(const char* label, const char* hex, bool selected, float width) {
    ImGui::PushID(label);
    const ImVec2 size(width, ImGui::GetFrameHeight());
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.17f, 0.84f, 0.45f));
    const bool clicked = ImGui::Button("##tile", size);
    if (selected) ImGui::PopStyleColor();
    const ImVec2 a = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = size.y;
    dl->AddCircleFilled(ImVec2(a.x + h * 0.45f, a.y + h * 0.5f), h * 0.2f, ImGui::ColorConvertFloat4ToU32(tileColor(hex)));
    dl->AddText(ImVec2(a.x + h * 0.85f, a.y + (h - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label);
    ImGui::PopID();
    return clicked;
}

// A drop-down whose closed box shows only the short name (the long text would be cut off at big Text sizes);
// the open list shows each short name with its explanation under it, wrapped.
struct ComboItem { const char* name; const char* note; };
bool shortCombo(const char* id, int* cur, std::initializer_list<ComboItem> items) {
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(id, items.begin()[*cur].name)) {
        int i = 0;
        for (const ComboItem& it : items) {
            if (ImGui::Selectable(it.name, *cur == i)) { *cur = i; changed = true; }
            if (it.note[0]) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", it.note);
                ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
            }
            i++;
        }
        ImGui::EndCombo();
    }
    return changed;
}

// The Performance card (both labs): frame times and the VSync switch
void performanceCard(AppState& app) {
    ImGui::SeparatorText("Performance");
    wrappedCheckbox("VSync (wait for the screen)", &app.vsync);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off: frames are drawn as fast as possible, so the frame time shows the real headroom");
    {
        const FrameStats& st = app.stats;
        const float avg = st.avgFrame();
        ImGui::Text("Frame: %.2f ms   average %.2f ms", st.last(), avg);
        ImGui::Text("= %.0f fps on average   worst %.2f ms", avg > 0 ? 1000.0f / avg : 0.0f, st.worst());
        ImGui::Text("Work per frame (CPU): %.2f ms", st.avgWork());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Time to run the physics and build the frame, before waiting for the screen.\nThe graphics card may still be drawing after this.");
        float hist[FrameStats::kHistory];
        st.ordered(hist);
        ImGui::PlotLines("##frametimes", hist, FrameStats::kHistory, 0, "frame time, last 240 frames", 0.0f, std::max(2 * avg, 1.0f), ImVec2(-1, 50));
    }
    {   // frames drawn while the window system had the window in its move / resize loop (Windows: dragging the title bar or an edge)
        const app::LiveStats& lv = app.live;
        if (lv.frames == 0) ImGui::TextDisabled("Window drag / resize: no frames drawn during one yet");
        else ImGui::TextDisabled("Window drag / resize: %ld frames drawn during %d drags, longest gap %.0f ms", lv.frames, lv.drags, lv.worstGapMs);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("While you hold or drag the window's title bar or edge, Windows pauses the app's own loop.\n"
                                                      "Frames drawn in that time are counted here (the longest time between two of them, and the longest pause before the first: %.0f ms).\n"
                                                      "If the picture or the fps counter still freezes while you drag, and this stays at 0, tell Alex.", lv.leadInMs);
    }
    benchmarkButton(app);
}

void orbitPanel(AppState& app, float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - width, vp->WorkPos.y + headerHeight()));   // under the header
    ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y - headerHeight()));
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    if (const char* sc = std::getenv("PL_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
    if (look().synthGlow) {                                   // Synthwave: the page's purple glow, seen through this panel
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        drawSynthGlow(ImGui::GetWindowDrawList(), vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
    }
    // touching the panel during a Journey stops it first (your settings come back), then the click does its job
    const bool journeyWasActive = app.journey.active;
    if (journeyWasActive && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) journeyStop(app);

    labPicker(app);
    defaultsButton(app);
    shotButton(app, true);                                    // the web's 📷 Save image, beside Defaults
    shotNote(app);

    // ---- Presets ----
    ImGui::SeparatorText("Presets");
    if (wideButton("☉ Reset to today")) { resetToToday(app); app.dateNote.clear(); }

    // ---- Journey and dates ----
    journeyDatesCard(app, journeyWasActive);

    // ---- Learn ----
    learnButtons(app);

    // ---- Time ----
    ImGui::SeparatorText("Time");
    if (wideButton(app.paused ? "▶ Play" : "❚❚ Pause")) app.paused = !app.paused;
    ImGui::Text("Time speed: %s", tspeedText(app.set.tspeed).c_str());   // the value on its own line: the slider's thumb would cover it
    float ts = static_cast<float>(app.set.tspeed);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##tspeed", &ts, static_cast<float>(kTspeedMin), static_cast<float>(kTspeedMax), ""))
        app.set.tspeed = ts;
    ImGui::TextDisabled("Space: play / pause");

    // ---- Planet physics: Simple / Real ----
    ImGui::SeparatorText("Planet physics");
    {
        const float gap = 6 * ImGui::GetFontSize() / 16, w = (ImGui::GetContentRegionAvail().x - gap) / 2;
        for (int real = 0; real < 2; real++) {
            if (real) ImGui::SameLine(0, gap);
            const bool on = app.set.realPlanets == (real == 1);
            if (on) pushOn();
            if (ImGui::Button(real ? "Real" : "Simple", ImVec2(w, 0)) && !on) app.set.realPlanets = real == 1;
            if (on) popOn();
        }
        if (app.set.realPlanets) {                          // Real only: general relativity for the planets
            wrappedCheckbox("General relativity (1PN)", &app.set.relativity);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
                const std::string tip = "Adds Einstein's correction to the Sun's pull on each planet, so Mercury's orbit turns a little more each time round: about 43 arcseconds per century, as measured.\n\n"
                                        "It is a first-order (1PN) approximation, made with one simple extra potential of the Sun (Nobili & Roxburgh, 1986). It gets the turning of the orbit right, "
                                        "but the motion along the orbit is slightly wrong, so, measured against JPL Horizons, it moves every planet's position away from JPL: Mercury's worst error in 1800 to 2100 is "
                                        + withCommas(orbit::kRealErrKmRelOn[0]) + " km with it and " + withCommas(orbit::kRealErrKmRelOff[0]) + " km without. "
                                        "It leaves out the planets' relativistic pull on each other, the Sun's spin and shape, and all higher orders.\n\n"
                                        "Off (the default): plain Newtonian gravity, no extra turn.";
                ImGui::TextUnformatted(tip.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            wrappedCheckbox("Exaggerate the Sun's wobble (×50)", &app.set.exaggerateWobble);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
                ImGui::TextUnformatted("The Sun is not fixed: it circles the balance point of the Solar System (the barycentre, the marked point), mostly because of Jupiter's pull. "
                                       "The real circle is smaller than about two Sun radii, far too small to see here.\n\n"
                                       "Ticked: the Sun's path is drawn 50 times too big, round the marked point, so you can see it. This is exaggerated, not to scale (the Sun itself and the planets are drawn at their true places).\n\n"
                                       "Unticked: the path is drawn at its true size.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        char moonBuf[32];
        std::snprintf(moonBuf, sizeof moonBuf, "%.1f km", orbit::kMoonElpWorstKm);
        const std::string moonErr = moonBuf;
        // 0.10.5 stage 2 (audit row 16): the whole Real note, one paragraph, no repeated relativity numbers (the checkbox tooltip says what relativity does; the Moon's card and About give the arcseconds and the light-time remark)
        const std::string realNote =
            "Real: Sun and eight planets pull on each other (Newtonian; each planet with its moons). "
            "Start: JPL Horizons (DE441), 1 Jan 2026 12:00 TDB; dates used as TDB. "
            "Moon: ELP2000-82B, 1800 to 1 Jan 2100 only; other moons as in Simple. "
            "Not in: dwarf planets, asteroids. "
            "Worst error vs Horizons, 1800-2100: Moon " + moonErr + "; planets, relativity " + std::string(app.set.relativity ? "on" : "off") + ": " + worstErrorList(app.set.relativity) + ".";
        ImGui::TextWrapped("%s", app.set.realPlanets
            ? realNote.c_str()
            : "Simple: each planet moves on its own round the Sun, and the planets do not pull on each other (as in the web version).");
        ImGui::PopStyleColor();
    }

    // ---- View ----
    ImGui::SeparatorText("View");
    {
        int v3 = app.set.view3D ? 1 : 0;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##view", &v3, "2D (top-down)\0" "3D (rotatable)\0")) app.set.view3D = v3 == 1;
    }
    if (app.set.view3D) {
        if (wideButton(app.view.panMode ? "✥ Drag moves the view" : "⟲ Drag rotates the view")) app.view.panMode = !app.view.panMode;   // full width: it does not fit half a row at big text sizes
        if (wideButton(app.view.spin ? "⟳ Spin: on" : "⟳ Spin: off")) app.view.spin = !app.view.spin;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("Right-drag or Shift-drag always moves the view"); ImGui::PopStyleColor();
    }
    ImGui::Text("Zoom: %s", zoomText(app.set.zoom).c_str());
    float z = static_cast<float>(app.set.zoom);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##zoom", &z, static_cast<float>(kZoomMin), static_cast<float>(kZoomMax), "")) setZoom(app, z);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("Mouse wheel zooms, drag moves the view"); ImGui::PopStyleColor();
    ImGui::TextUnformatted("Sizes");
    int sizes = app.set.trueScale ? 0 : 1;
    if (shortCombo("##sizes", &sizes, {{"True scale", "planets and Sun as big as they really are"}, {"Easy to see", "planets drawn bigger"}})) app.set.trueScale = sizes == 0;
    ImGui::TextUnformatted("Trails");
    int trails = app.set.fullTrails ? 1 : 0;
    if (shortCombo("##trails", &trails, {{"Short tail", "a little tail behind each object"}, {"Full orbit trails", ""}})) app.set.fullTrails = trails == 1;
    if (app.view.sel.kind != Selection::None && app.view.sel.kind != Selection::Belt) {
        if (wideButton(app.view.follow ? "◉ Following" : "○ Follow")) app.view.follow = !app.view.follow;
    }
    if (app.set.moons && moonTarget(app)) {
        if (wideButton("Slow time to watch moons")) {
            const Body* b = moonTarget(app);
            double P = 1e300;                             // the selected moon's period, or the fastest moon's
            if (app.view.sel.kind == Selection::Moon) P = b->moons[app.view.sel.moon].data->periodDays;
            else for (const Moon& m : b->moons) P = std::min(P, m.data->periodDays);
            app.set.tspeed = std::clamp(std::log10(P / 4 / (kSimTime / kDay)), kTspeedMin, kTspeedMax);   // one lap in 4 seconds
            app.view.follow = true;
        }
    }
    {   // a selected planet with real spin data: slow time so one turn takes about 24 seconds
        const Body* b = app.view.sel.kind == Selection::Body ? findBody(app.sys, app.view.sel.bodyId) : nullptr;
        if (b && hasSpin(b->name) && wideButton("Slow time to watch it turn")) {
            const double daysPerTurn = 360 / std::fabs(spinRateDegPerDay(b->name));
            app.set.tspeed = std::clamp(std::log10(daysPerTurn / 24 / (kSimTime / kDay)), kTspeedMin, kTspeedMax);   // 1 s = 1/24 of a turn
            app.view.follow = true;
            if (app.set.trueScale) setZoom(app, zoomFor(app, b->radius / kKm, 80));   // close enough to see it turn
        }
    }
    if (wideButton("↺ Reset view")) { app.view.resetView(); setZoom(app, 0); app.view.ox = app.view.oy = 0; }

    // ---- Sky ----
    ImGui::SeparatorText("Sky");
    {
        ImGui::TextUnformatted("Stars in the background");
        int sky = static_cast<int>(app.set.sky);
        if (shortCombo("##sky", &sky, {{"Soft", "dim close up, brighter as you zoom out"}, {"Full", "always bright"}, {"Off", "plain background"}})) app.set.sky = static_cast<SkyMode>(sky);
        if (app.set.sky != SkyMode::Off) {
            ImGui::TextUnformatted("Constellations");
            int c = static_cast<int>(app.set.cons);
            if (shortCombo("##cons", &c, {{"Off", ""}, {"Lines", "the patterns people drew"}, {"Lines and names", ""}})) app.set.cons = static_cast<ConsMode>(c);
        }
        wrappedCheckbox("Far-away things (zoom out to find them)", &app.set.far);
    }

    // ---- Solar System extras ----
    ImGui::SeparatorText("Solar System extras");
    ImGui::Checkbox("Moons", &app.set.moons);
    ImGui::Checkbox("Dwarf planets", &app.set.dwarfs);
    ImGui::Checkbox("Famous comets", &app.set.comets);
    ImGui::Checkbox("Belts and Oort cloud", &app.set.belts);

    performanceCard(app);

    // ---- Go to ----
    ImGui::SeparatorText("Go to");
    const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x, tw = (avail - gap) / 2;
    int col = 0;
    const auto next = [&]() { if (++col % 2) ImGui::SameLine(); };
    const auto newRow = [&]() { if (col % 2) ImGui::NewLine(); col = 0; };
    OrbitView& v = app.view;
    const auto fly = [&](Selection s, double zoom) {      // select it, keep it in the middle, zoom to suit; the selected tile again lets go
        if (v.sel == s) { v.sel = Selection{}; v.follow = false; return; }
        v.select(s, true);
        setZoom(app, zoom);
        v.ox = v.oy = 0;
    };
    const bool ts2 = app.set.trueScale;
    const auto easyZoom = [](const std::string& n) {      // zoom that suits each body in Easy to see (moons use their planet's)
        static const struct { const char* n; double z; } Z[] = {{"Sun", 0.8}, {"Mercury", 0.8}, {"Venus", 0.8}, {"Earth", 0.8}, {"Mars", 0.6}, {"Jupiter", 1.3}, {"Saturn", 1.2},
                                                                {"Uranus", 0.5}, {"Neptune", 0.3}, {"Ceres", 0.4}, {"Pluto", 0.3}, {"Haumea", 0.3}, {"Makemake", 0.3}, {"Eris", 0.3}};
        for (const auto& e : Z) if (n == e.n) return e.z;
        return 0.4;
    };

    ImGui::TextDisabled("Star and planets");
    if (tile("Sun", "#f0b020", v.sel.kind == Selection::Star, tw)) fly({Selection::Star}, ts2 ? zoomFor(app, 696000, 90) : easyZoom("Sun"));
    next();
    if (tile("Barycentre", "#9aa5b8", v.sel.kind == Selection::Bary, tw)) fly({Selection::Bary}, ts2 ? 2.3 : 2.0);
    next();
    const auto bodyTiles = [&](BodyKind kind) {
        for (const Body& b : app.sys.bodies) {
            if (b.type != kind) continue;
            const Selection s{Selection::Body, b.id};
            if (tile(b.name.c_str(), b.color, v.sel == s, tw)) {
                if (kind == BodyKind::Dwarf) app.set.dwarfs = true;
                if (kind == BodyKind::Comet) app.set.comets = true;
                fly(s, kind == BodyKind::Comet ? 1.0 : !ts2 ? easyZoom(b.name) : zoomFor(app, b.radius / kKm, 55));   // true scale: the planet fills the picture
            }
            next();
        }
    };
    bodyTiles(BodyKind::Planet);
    newRow();
    ImGui::TextDisabled("Dwarf planets");
    bodyTiles(BodyKind::Dwarf);
    newRow();
    ImGui::TextDisabled("Famous comets");
    bodyTiles(BodyKind::Comet);
    newRow();
    ImGui::TextDisabled("Moons");
    for (const Body& b : app.sys.bodies)
        for (std::size_t j = 0; j < b.moons.size(); j++) {
            const Selection s{Selection::Moon, b.id, static_cast<int>(j)};
            if (tile(b.moons[j].data->name, b.color, v.sel == s, tw)) {
                app.set.moons = true;
                if (b.type == BodyKind::Dwarf) app.set.dwarfs = true;
                fly(s, ts2 ? zoomFor(app, b.moons[j].data->aKm, 0.34 * std::min(app.canvasW, app.canvasH)) : easyZoom(b.name));   // true scale: fit the moon's orbit
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s · moon of %s", b.moons[j].data->name, b.name.c_str());
            next();
        }
    newRow();
    ImGui::TextDisabled("Belts and the far edge");
    {   // belts: selected but not followed; each has a zoom that shows all of it
        static const struct { const char* key; const char* name; const char* color; double zoom; } B[] = {
            {"main", "Asteroid belt", "#8a7f72", -0.15}, {"trojan", "Jupiter’s Trojans", "#a08c6a", -0.55}, {"kuiper", "Kuiper belt", "#6f9fc0", -1.55}, {"oort", "Oort cloud", "#9aa5b8", -4.4}};
        for (const auto& b : B)
            for (std::size_t gi = 0; gi < app.sys.belts.size(); gi++) {
                if (app.sys.belts[gi].key != b.key) continue;
                const Selection s{Selection::Belt, 0, static_cast<int>(gi)};
                if (tile(b.name, b.color, v.sel == s, tw)) {
                    app.set.belts = true;
                    if (v.sel == s) { v.sel = Selection{}; v.follow = false; }
                    else { v.select(s, false); setZoom(app, b.zoom); v.ox = v.oy = 0; }
                }
                next();
            }
    }
    newRow();
    ImGui::TextDisabled("Far away");
    {
        static const struct { const char* key; const char* name; const char* color; } F[] = {
            {"v1", "Voyager 1", "#ffd27a"}, {"v2", "Voyager 2", "#ffd27a"}, {"acen", "Alpha Centauri", "#ffe9b0"}, {"sirius", "Sirius", "#cfe0ff"},
            {"vega", "Vega", "#cfe0ff"}, {"pleia", "Pleiades", "#8fd0ff"}, {"m42", "Orion Nebula", "#8fd0ff"}, {"sgra", "Galactic centre", "#cfd6e6"}};
        for (const auto& f : F) {
            const int idx = farIndexOf(f.key);
            const Selection s{Selection::Far, 0, idx};
            if (tile(f.name, f.color, v.sel == s, tw)) {
                app.set.far = true;
                FarItem it;
                if (v.sel == s) { v.sel = Selection{}; v.follow = false; }
                else if (farItem(app.sys, idx, it)) {   // far enough out that the Sun and the target are both in the picture
                    v.select(s, true);
                    setZoom(app, std::log10(13.6 / (it.au * kAU / kSimLength)));
                    v.ox = v.oy = 0;
                }
            }
            next();
        }
    }
    newRow();
    ImGui::Spacing();
    ImGui::TextWrapped("Click a tile to fly there. It stays in the middle of the picture. Your time speed is not changed.");
    ImGui::Spacing();
    ImGui::TextDisabled("Real orbits from NASA/JPL data. Moons show when you zoom in or click their planet (until you zoom in a lot, their orbits are drawn wider than real so they can be seen).");
    ImGui::End();
}

}  // namespace ui
