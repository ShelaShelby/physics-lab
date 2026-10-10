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
    if (app.lab == 1) {                                   // 0.16 stage 1a: the old Atom panel's line under its Performance card (with the tooltip)
        ImGui::TextDisabled("Picture: %d draw calls, %d instanced shapes", app.drawCalls, app.drawInstances);
        if (shellLogOn()) auditNote("drawcalls shown");   // (for the automatic checks)
        if (ImGui::IsItemHovered()) ui::SetTooltip("Cloud dots, glows and gluon-field particles are drawn as instanced shapes,\na few draw calls for thousands of them.");
    }
    settingMark("benchmark");
    benchmarkButton(app);
}

}  // namespace ui
