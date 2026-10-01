#include "orbit_panel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "changelog.h"
#include "imgui.h"
#include "orbit/format.h"
#include "orbit/sky.h"
#include "orbit/units.h"

namespace ui {

using namespace orbit;
using namespace units;

static int64_t nowUnixMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void setZoom(AppState& app, double zoom) {
    zoom = std::clamp(zoom, kZoomMin, kZoomMax);
    app.view.zoomChanged(app.set.zoom, zoom);
    app.set.zoom = zoom;
}

void resetToToday(AppState& app) {
    app.sys.loadAt(nowUnixMs());
    app.set.tspeed = kTspeedDefault;
    app.set.zoom = kZoomDefault;
    app.set.moons = app.set.dwarfs = app.set.comets = app.set.belts = true;
    app.view.sel = Selection{};
    app.view.follow = false;
    app.view.ox = app.view.oy = 0;
    app.view.cdep = 0;
}

// "1 s = 29.1 days": what the Time speed slider means (as in the web version)
static std::string tspeedText(double v) {
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
static double zoomFor(const AppState& app, double km, double wantPx) {
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

void orbitPanel(AppState& app, float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - width, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y));
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // ---- title and update badge ----
    ImGui::TextUnformatted("Physics Lab · Orbit Lab");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.44f, 0.93f, 0.85f));
    if (ImGui::SmallButton((std::string("Update ") + appVersion()).c_str())) app.showChangelog = true;
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("What's new");
    ImGui::TextWrapped("The real Solar System, where everything is today: planets, moons, dwarf planets and comets. Click anything for details.");

    // ---- Presets ----
    ImGui::SeparatorText("Presets");
    if (ImGui::Button("☉ Reset to today", ImVec2(-1, 0))) resetToToday(app);

    // ---- Time ----
    ImGui::SeparatorText("Time");
    if (ImGui::Button(app.paused ? "▶ Play" : "❚❚ Pause", ImVec2(-1, 0))) app.paused = !app.paused;
    ImGui::TextUnformatted("Time speed");
    float ts = static_cast<float>(app.set.tspeed);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##tspeed", &ts, static_cast<float>(kTspeedMin), static_cast<float>(kTspeedMax), tspeedText(app.set.tspeed).c_str()))
        app.set.tspeed = ts;
    ImGui::TextDisabled("Space: play / pause");

    // ---- View ----
    ImGui::SeparatorText("View");
    {
        int v3 = app.set.view3D ? 1 : 0;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##view", &v3, "2D (top-down)\0" "3D (rotatable)\0")) app.set.view3D = v3 == 1;
    }
    if (app.set.view3D) {
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        if (ImGui::Button(app.view.panMode ? "✥ Drag moves the view" : "⟲ Drag rotates the view", ImVec2(half, 0))) app.view.panMode = !app.view.panMode;
        ImGui::SameLine();
        if (ImGui::Button(app.view.spin ? "⟳ Spin: on" : "⟳ Spin: off", ImVec2(-1, 0))) app.view.spin = !app.view.spin;
        ImGui::TextDisabled("Right-drag or Shift-drag always moves the view");
    }
    ImGui::TextUnformatted("Zoom");
    float z = static_cast<float>(app.set.zoom);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##zoom", &z, static_cast<float>(kZoomMin), static_cast<float>(kZoomMax), zoomText(app.set.zoom).c_str())) setZoom(app, z);
    ImGui::TextDisabled("Mouse wheel zooms, drag moves the view");
    ImGui::TextUnformatted("Sizes");
    int sizes = app.set.trueScale ? 0 : 1;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##sizes", &sizes, "True scale (planets and Sun as big as they really are)\0Easy to see (planets drawn bigger)\0")) app.set.trueScale = sizes == 0;
    ImGui::TextUnformatted("Trails");
    int trails = app.set.fullTrails ? 1 : 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##trails", &trails, "Short tail (a little tail behind each object)\0Full orbit trails\0")) app.set.fullTrails = trails == 1;
    if (app.view.sel.kind != Selection::None && app.view.sel.kind != Selection::Belt) {
        if (ImGui::Button(app.view.follow ? "◉ Following" : "○ Follow", ImVec2(-1, 0))) app.view.follow = !app.view.follow;
    }
    if (app.set.moons && moonTarget(app)) {
        if (ImGui::Button("⏱ Slow time to watch moons", ImVec2(-1, 0))) {
            const Body* b = moonTarget(app);
            double P = 1e300;                             // the selected moon's period, or the fastest moon's
            if (app.view.sel.kind == Selection::Moon) P = b->moons[app.view.sel.moon].data->periodDays;
            else for (const Moon& m : b->moons) P = std::min(P, m.data->periodDays);
            app.set.tspeed = std::clamp(std::log10(P / 4 / (kSimTime / kDay)), kTspeedMin, kTspeedMax);   // one lap in 4 seconds
            app.view.follow = true;
        }
    }
    if (ImGui::Button("↺ Reset view", ImVec2(-1, 0))) { app.view.resetView(); setZoom(app, 0); app.view.ox = app.view.oy = 0; }

    // ---- Sky ----
    ImGui::SeparatorText("Sky");
    {
        ImGui::TextUnformatted("Stars in the background");
        int sky = static_cast<int>(app.set.sky);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##sky", &sky, "Soft (dim close up, brighter as you zoom out)\0" "Full (always bright)\0" "Off (plain background)\0")) app.set.sky = static_cast<SkyMode>(sky);
        if (app.set.sky != SkyMode::Off) {
            ImGui::TextUnformatted("Constellations");
            int c = static_cast<int>(app.set.cons);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##cons", &c, "Off\0" "Lines (the patterns people drew)\0" "Lines and names\0")) app.set.cons = static_cast<ConsMode>(c);
        }
        ImGui::Checkbox("Far-away things (zoom out to find them)", &app.set.far);
    }

    // ---- Solar System extras ----
    ImGui::SeparatorText("Solar System extras");
    ImGui::Checkbox("Moons", &app.set.moons);
    ImGui::Checkbox("Dwarf planets", &app.set.dwarfs);
    ImGui::Checkbox("Famous comets", &app.set.comets);
    ImGui::Checkbox("Belts and Oort cloud", &app.set.belts);

    // ---- Performance ----
    ImGui::SeparatorText("Performance");
    ImGui::Checkbox("VSync (wait for the screen)", &app.vsync);
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
