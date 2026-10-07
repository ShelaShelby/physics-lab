#include "orbit_panel.h"
#include "orbit_actions.h"
#include "shell_test.h"
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
#include "style_draw.h"
#include "shot.h"
#include "benchmark.h"
#include "app/bench.h"
#include "orbit/format.h"
#include "orbit/moon_elp.h"
#include "orbit/date_text.h"
#include "orbit/real_errors.h"
#include "orbit/sky.h"
#include "orbit/spin.h"
#include "orbit/today.h"
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

void resetViewAction(AppState& app) { app.view.resetView(); setZoom(app, 0); app.view.ox = app.view.oy = 0; }

// Today: only the date (and the selection, Follow and moved view) go back; the zoom, the four switches and the Time speed are the user's own
// (orbit/today.h; Defaults puts them back itself).
void resetToToday(AppState& app) { orbit::backToToday(app.sys, app.set, app.view, nowUnixMs()); }

// "1 s = 29.1 days": what the Time speed slider means (orbit/date_text.h, tested)
std::string tspeedText(double v) { return orbit::tspeedText(v); }

std::string zoomLabel(double v);
static std::string zoomText(double v) {
    const double z = std::pow(10.0, v);
    return "×" + (z < 0.01 ? fmt::precision(z, 1) : fmt::fixed(z, z < 1 ? 2 : z < 10 ? 1 : 0));
}

std::string zoomLabel(double v) { return zoomText(v); }

// The zoom at which something `km` in radius is `wantPx` points on screen, at true scale (the web's zoomFor)
double zoomFor(const AppState& app, double km, double wantPx) {
    const double m = std::min(app.canvasW, app.canvasH) / 34;
    return std::clamp(std::log10(wantPx / (km * kKm / kSimLength) / m), -5.0, 5.5);
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
    const bool clicked = ui::Button("##tile", size);
    noteItem(label);
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
    const bool comboOpen = ui::BeginCombo(id, items.begin()[*cur].name);
    noteItem(id);
    if (comboOpen) {
        int i = 0;
        for (const ComboItem& it : items) {
            if (ImGui::Selectable(it.name, *cur == i)) { *cur = i; changed = true; }
            noteItem(it.name);
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
    settingMark("perf");
    ImGui::SeparatorText("Performance");
    settingMark("vsync");
    wrappedCheckbox("VSync (wait for the screen)", &app.vsync);
    if (ImGui::IsItemHovered()) ui::SetTooltip("Off: frames are drawn as fast as possible, so the frame time shows the real headroom");
    if (app.lab == 0) {                                   // 0.13 stage 4: who places the belts' rocks: the line says what this PC has, the switch compares the two ways
        ImGui::TextWrapped("%s", app::rocksPathText(app.glMajor, app.glMinor, app.rocksOk, app.set.gpuRocks).c_str());
        ImGui::BeginDisabled(!app.rocksOk);
        settingMark("gpurocks");
        wrappedCheckbox("GPU rocks (OpenGL 4.3)", &app.set.gpuRocks);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("On (the default, needs OpenGL 4.3): the places of the belts' rocks (Kepler's equation for every rock, and where it lands on the picture) are worked out by a compute shader on the graphics card; "
                                                      "the processor only sends the time and the camera.\n"
                                                      "Off, or on a PC without OpenGL 4.3: the processor does it rock by rock, as versions up to 0.13 stage 3 did.\n"
                                                      "The picture is the same either way (tested to a few thousandths of a point); only the speed differs, and it is expected, not yet measured, to be better On.\n"
                                                      "Used for comparing on your PC with the benchmark's scenes 1, 6, 31 and 32. Not saved: it is On again at the next start.");
    }
    if (app.lab == 1) {                                   // 0.13 stage 2: which way the superposition and the jumps are worked out: the line says what this PC has, the switch compares the two ways
        ImGui::TextWrapped("%s", app::computePathText(app.glMajor, app.glMinor, app.computeOk, app.atom.gpuCompute).c_str());
        ImGui::BeginDisabled(!app.computeOk);
        settingMark("gpuspin");
        wrappedCheckbox("GPU superposition and jumps (OpenGL 4.3)", &app.atom.gpuCompute);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("On (the default, needs OpenGL 4.3): for a superposition or a photon jump, a compute shader on the graphics card works out which dots are shown and their colours;\n"
                                                      "Off, or on a PC without OpenGL 4.3: the processor does it dot by dot, as versions up to 0.13 stage 1 did.\n"
                                                      "The picture is the same either way (tested, except a rare dot within rounding of a threshold); only the speed differs, and it is expected, not yet measured, to be better On.\n"
                                                      "Used for comparing on your PC with the benchmark's scenes 11, 13, 14, 15 and 25 to 28. Not saved: it is On again at the next start.");
        ImGui::TextWrapped("%s", app::outlinePathText(app.glMajor, app.glMinor, app.computeOk, app.atom.gpuOutline).c_str());
        ImGui::BeginDisabled(!app.computeOk);
        settingMark("gpuoutline");
        wrappedCheckbox("GPU outline (OpenGL 4.3)", &app.atom.gpuOutline);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("On (the default, needs OpenGL 4.3): the 90 %% outline's density grid and its 90 %% level (and, in the Slice view, the cut's contour) are made by compute shaders\n"
                                                      "on the graphics card, without the picture waiting for them: the outline appears a frame or two after it is switched on, instead of a half-frame hitch.\n"
                                                      "Off, or on a PC without OpenGL 4.3: the processor makes it (about 30 ms the first time for each orbital).\n"
                                                      "The outline is the same either way (tested, except a line a hair's breadth off where a cell is within rounding of the level); only the speed differs, and it is expected, not yet measured, to be better On.\n"
                                                      "Used for comparing on your PC with the benchmark's scenes 16, 17, 29 and 30. Not saved: it is On again at the next start.");
    }
    if (app.lab == 1) {                                   // the Atom Lab's cloud dots: who picks the phase colours (0.13 stage 1). A debug switch for comparing on a real PC; not saved
        settingMark("gpucolours");
        wrappedCheckbox("GPU colours (Atom Lab cloud dots)", &app.atom.gpuColours);
        if (ImGui::IsItemHovered()) ui::SetTooltip("On (the default): each dot of the electron cloud carries its colour number and the graphics card picks the colour.\n"
                                                      "Off: the processor colours every dot, going over all dots once per colour, as versions up to 0.12 did.\n"
                                                      "The picture is the same either way (tested); only the speed differs, and it is expected, not yet measured, to be better On.\n"
                                                      "Used for comparing on your PC with the benchmark's scenes 8 and 23, 10 and 24. Not saved: it is On again at the next start.");
    }
    {
        settingMark("frames");
        const FrameStats& st = app.stats;
        const float avg = st.avgFrame();
        ImGui::Text("Frame: %.2f ms   average %.2f ms", st.last(), avg);
        ImGui::Text("= %.0f fps on average   worst %.2f ms", avg > 0 ? 1000.0f / avg : 0.0f, st.worst());
        ImGui::Text("Work per frame (CPU): %.2f ms", st.avgWork());
        if (ImGui::IsItemHovered()) ui::SetTooltip("Time to run the physics and build the frame, before waiting for the screen.\nThe graphics card may still be drawing after this.");
        float hist[FrameStats::kHistory];
        st.ordered(hist);
        ImGui::PlotLines("##frametimes", hist, FrameStats::kHistory, 0, "frame time, last 240 frames", 0.0f, std::max(2 * avg, 1.0f), ImVec2(-1, 50));
    }
    {   // frames drawn while the window system had the window in its move / resize loop (Windows: dragging the title bar or an edge)
        const app::LiveStats& lv = app.live;
        if (lv.frames == 0) ImGui::TextDisabled("Window drag / resize: no frames drawn during one yet");
        else ImGui::TextDisabled("Window drag / resize: %ld frames drawn during %d drags, longest gap %.0f ms", lv.frames, lv.drags, lv.worstGapMs);
        if (ImGui::IsItemHovered()) ui::SetTooltip("While you hold or drag the window's title bar or edge, Windows pauses the app's own loop.\n"
                                                      "Frames drawn in that time are counted here (the longest time between two of them, and the longest pause before the first: %.0f ms).\n"
                                                      "If the picture or the fps counter still freezes while you drag, and this stays at 0, tell Alex.", lv.leadInMs);
    }
    settingMark("benchmark");
    benchmarkButton(app);
}

static void orbitPanelContents(AppState& app) {
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
    if (ui::SliderFloat("##tspeed", &ts, static_cast<float>(kTspeedMin), static_cast<float>(kTspeedMax), ""))
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
            if (ui::Button(real ? "Real" : "Simple", ImVec2(w, 0)) && !on) app.set.realPlanets = real == 1;
            if (on) popOn();
        }
        if (app.set.realPlanets) {                          // Real only: general relativity for the planets
            wrappedCheckbox("General relativity (1PN)", &app.set.relativity);
            if (ImGui::IsItemHovered()) {
                ui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
                const std::string tip = relativityTip();
                ImGui::TextUnformatted(tip.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            wrappedCheckbox("Exaggerate the Sun's wobble (×50)", &app.set.exaggerateWobble);
            if (ImGui::IsItemHovered()) {
                ui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
                ImGui::TextUnformatted(wobbleTip().c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        // the two notes are built in orbit/real_errors.h (tested: the numbers and a length guard)
        const std::string realNote = realNoteText(app.set.relativity), simpleNote = simpleNoteText();
        ImGui::TextWrapped("%s", app.set.realPlanets
            ? realNote.c_str()
            : simpleNote.c_str());
        ImGui::PopStyleColor();
    }

    // ---- View ----
    ImGui::SeparatorText("View");
    {
        int v3 = app.set.view3D ? 1 : 0;
        ImGui::SetNextItemWidth(-1);
        if (ui::Combo("##view", &v3, "2D (top-down)\0" "3D (rotatable)\0")) app.set.view3D = v3 == 1;
    }
    if (app.set.view3D) {
        if (wideButton(app.view.panMode ? "✥ Drag moves the view" : "⟲ Drag rotates the view")) app.view.panMode = !app.view.panMode;   // full width: it does not fit half a row at big text sizes
        if (wideButton(app.view.spin ? "⟳ Spin: on" : "⟳ Spin: off")) app.view.spin = !app.view.spin;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("Right-drag or Shift-drag always moves the view"); ImGui::PopStyleColor();
    }
    ImGui::Text("Zoom: %s", zoomText(app.set.zoom).c_str());
    float z = static_cast<float>(app.set.zoom);
    ImGui::SetNextItemWidth(-1);
    if (ui::SliderFloat("##zoom", &z, static_cast<float>(kZoomMin), static_cast<float>(kZoomMax), "")) setZoom(app, z);
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
    if (slowMoonsAvailable(app) && wideButton("Slow time to watch moons")) slowMoons(app);
    if (slowSpinAvailable(app) && wideButton("Slow time to watch it turn")) slowSpin(app);
    if (wideButton("↺ Reset view")) resetViewAction(app);

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
    ui::Checkbox("Moons", &app.set.moons);
    ui::Checkbox("Dwarf planets", &app.set.dwarfs);
    ui::Checkbox("Famous comets", &app.set.comets);
    ui::Checkbox("Belts and Oort cloud", &app.set.belts);

    performanceCard(app);

    // ---- Go to ----
    ImGui::SeparatorText("Go to");
    const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x, tw = (avail - gap) / 2;
    int col = 0;
    const auto next = [&]() { if (++col % 2) ImGui::SameLine(); };
    const auto newRow = [&]() { if (col % 2) ImGui::NewLine(); col = 0; };
    OrbitView& v = app.view;

    for (const PickGroup& grp : pickGroups(app)) {         // the same list the new toolbar's "pick a body" uses (ui/orbit_actions.h)
        ImGui::TextDisabled("%s", grp.title);
        for (const PickItem& it : grp.items) {
            if (tile(it.name.c_str(), it.color, v.sel == it.sel, tw)) pickItem(app, it);
            if (!it.tip.empty() && ImGui::IsItemHovered()) ui::SetTooltip("%s", it.tip.c_str());
            next();
        }
        newRow();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Click a tile to fly there. It stays in the middle of the picture. Your time speed is not changed.");
    ImGui::Spacing();
    ImGui::TextDisabled("Real orbits from NASA/JPL data. Moons show when you zoom in or click their planet (until you zoom in a lot, their orbits are drawn wider than real so they can be seen).");
}

void orbitPanel(AppState& app, float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - width, vp->WorkPos.y + headerHeight()));   // under the header
    ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y - headerHeight()));
    pushDockedBg();
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    popDockedBg();
    panelDecor(app::PanelKind::Docked);
    if (const char* sc = std::getenv("PL_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
    orbitPanelContents(app);
    ImGui::End();
}

// The window around the same contents, floating (the new UI's temporary "Old panel": 0.15 stage 3, removed when stages 4 and 5 move the controls)
void orbitPanelFloating(AppState& app, bool* open, float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Appearing);
    pushFloatStyle();
    const bool shown = ImGui::Begin("Old panel###oldpanel", open, ImGuiWindowFlags_NoCollapse);
    popFloatStyle();
    if (shown) { panelDecor(app::PanelKind::Floating); orbitPanelContents(app); }
    ImGui::End();
}

}  // namespace ui
