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
#include "atom/atom_card_paths.h"
#include "molecule/molecule_card_paths.h"
#include "app/gpu_guard.h"
#include <SDL3/SDL_misc.h>
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

// The zoom at which something `km` in radius is `wantPx` points on screen, at true scale
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
    noteItem("vsync");                                    // (0.59.0: the checks switch it: Vulkan makes its swapchain again)
    if (ImGui::IsItemHovered()) ui::SetTooltip("Off: frames are drawn as fast as possible, so the frame time shows the real headroom. For this run only: the app always starts with VSync on.");
    {   // 0.59.0: Graphics: OpenGL (the default) or Vulkan (the test of step 1 of 5, PLAN_VULKAN.md); used from the next start. The line says what this start
        // draws with and, when Vulkan was chosen but OpenGL is in use, why (the Vulkan guard: a step that failed or stopped the app), with Try again
        settingMark("graphics");
        ImGui::TextUnformatted("Graphics:");
        ImGui::SameLine();
        if (ImGui::RadioButton("OpenGL##graphics", app.prefs.graphics != "vulkan")) app.prefs.graphics = "opengl";
        noteItem("graphics.opengl");
        ImGui::SameLine();
        if (ImGui::RadioButton("Vulkan (test)##graphics", app.prefs.graphics == "vulkan")) app.prefs.graphics = "vulkan";
        noteItem("graphics.vulkan");
        if (ImGui::IsItemHovered()) ui::SetTooltip("Vulkan, step 3 of 5: the menus, panels, pages and guides and the Orbit, Atom and Molecule Labs' pictures are drawn by Vulkan,\n"
                                                      "with the graphics card's own programs (the belts' rocks, the electron clouds, the outlines, the Density cloud). The Black Hole Lab comes in step 4.\n"
                                                      "Each step of Vulkan's start is guarded: if one fails or stops the app, the next start uses OpenGL and this page says why.");
        const app::GpuGuard& V = app::gpuVulkanGuard();
        const bool vk = app.graphicsInUse == "vulkan";
        std::string gline = "Takes effect at the next start. In use now: ";
        if (vk) {                                         // 0.61.0 (step 3): the card's own programs too; only the Black Hole Lab is left (step 4)
            gline += "Vulkan, on " + app.vulkanDevice + " (driver " + app.vulkanDriver + "). The Orbit, Atom and Molecule Labs are drawn, with the card's own "
                     "programs; the Black Hole Lab comes in step 4.";
            if (!app.rocksOk && !app.rocksWhy.empty()) gline += " The belts' rocks: the processor, because " + app.rocksWhy + ".";
            for (const auto& [name, g] : app::vulkanCardGuards())   // a card program its guard switched off on Vulkan (the Slice's, the jump's and the
                if (name != "slice" && name != "jump" && name != "molcloud" && !g->offText().empty()) gline += " " + g->offText();   // Density cloud's: their labs' rows)
        }
        else if (!app.vulkanWhy.empty()) gline += "OpenGL, because " + app.vulkanWhy;
        else gline += "OpenGL.";
        ImGui::TextWrapped("%s", gline.c_str());
        if (shellLogOn()) auditNote("graphics " + gline);
        bool cardsOff = false;                            // 0.61.0: Try again also gives the card programs switched off on Vulkan another start
        if (vk) for (const auto& [name, g] : app::vulkanCardGuards()) cardsOff = cardsOff || (name != "slice" && name != "jump" && name != "molcloud" && !g->offText().empty());
        if (!V.offText().empty() || cardsOff || (!vk && !app.vulkanWhy.empty() && V.started() && !V.allowed())) {
            if (ImGui::Button("Try again##graphics")) {
                app::gpuVulkanGuard().tryAgain(); app.prefs.graphics = "vulkan"; app.vulkanWhy.clear();
                if (vk) for (const auto& [name, g] : app::vulkanCardGuards()) if (name != "slice" && name != "jump" && name != "molcloud" && !g->offText().empty()) g->tryAgain();
            }
            noteItem("graphics.again");
            if (ImGui::IsItemHovered()) ui::SetTooltip("Vulkan is tried again at the next start, with a marker before each step:\n"
                                                          "if it fails or stops the app again, the start after that uses OpenGL, as now.");
        }
    }
    if (app.lab == 0) {                                   // 0.13 stage 4: who places the belts' rocks: the line says what this PC has, the switch compares the two ways
        ImGui::TextWrapped("%s", app::rocksPathText(app.glMajor, app.glMinor, app.rocksOk, app.set.gpuRocks, app.rocksWhy).c_str());
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
        // 0.57.0: the Slice's new dots and the every-electron jump's groups on the card: one line each (who and why), Try again when its guard switched it off, the switch
        for (int which = 0; which < 2; which++) {
            app::GpuGuard& A = which == 0 ? app::gpuSliceGuard() : app::gpuJumpGuard();
            const std::string aoff = A.offText();
            const char* name = which == 0 ? "The Slice view's new dots" : "A jump with every electron";
            const std::string aline = !aoff.empty() ? std::string(name) + ": the processor, because the graphics card's program for it " + aoff.substr(aoff.find("stopped") != std::string::npos ? aoff.find("stopped") : 0)
                                                    : std::string(name) + ": " + atom::atomCardPaths().pathText(which, app.atom, app.computeOk).substr(which == 0 ? 22 : 19) + ".";
            settingMark(which == 0 ? "gpuslice" : "gpujump");
            ImGui::TextWrapped("%s", aline.c_str());
            if (shellLogOn()) auditNote(std::string(which == 0 ? "atomslice " : "atomjump ") + aline);
            if (!aoff.empty()) {
                if (ImGui::Button(which == 0 ? "Try again##gpuslice" : "Try again##gpujump")) A.tryAgain();
                noteItem(which == 0 ? "gpuslice.again" : "gpujump.again");
                if (ImGui::IsItemHovered()) ui::SetTooltip("The graphics card's program is made and checked again when its picture is next shown, with a marker before each step:\n"
                                                              "if the app stops again, it starts the next time with it switched off, as now.");
            }
            ImGui::BeginDisabled(!app.computeOk);
            wrappedCheckbox(which == 0 ? "GPU Slice dots (OpenGL 4.3)" : "GPU jump groups (OpenGL 4.3)", which == 0 ? &app.atom.gpuSlice : &app.atom.gpuJump);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ui::SetTooltip(which == 0 ? "On (the default, needs OpenGL 4.3): in the Slice view (one electron) the new dots on a moved cut are made by a compute shader on the graphics card,\n"
                                            "from the same random numbers as the processor's, once its start-up check on this PC has passed; they arrive a frame after the cut moved.\n"
                                            "Off: the processor makes them. Used for comparing with the benchmark's atom.slice_drag. Not saved: it is On again at the next start."
                                          : "On (the default, needs OpenGL 4.3): a jump with every electron (Glow, Cloud 3D) is drawn from the cloud's buffer on the graphics card, which also\n"
                                            "decides each frame which dots are shown (exactly the processor's rule), once its start-up check on this PC has passed.\n"
                                            "Off: the processor does it dot by dot and sends every dot. Used for comparing with the benchmark's atom.jump_all. Not saved: it is On again at the next start.");
        }
    }
    if (app.lab == molecule::kLabIndex) {                 // 0.57.1: the Density cloud on the card: one line (who and why), Try again when its guard switched it off, the switch
        app::GpuGuard& A = app::gpuMolCloudGuard();
        const std::string aoff = A.offText();
        const std::string aline = !aoff.empty() ? std::string("The Density style's cloud: the processor, because the graphics card's program for it ") + aoff.substr(aoff.find("stopped") != std::string::npos ? aoff.find("stopped") : 0)
                                                : "The Density style's cloud: " + molecule::molCardPaths().pathText(app.moleculeView.gpuCloud, app.computeOk).substr(26) + ".";
        settingMark("gpumolcloud");
        ImGui::TextWrapped("%s", aline.c_str());
        if (shellLogOn()) auditNote("molcloud " + aline);
        if (!aoff.empty()) {
            if (ImGui::Button("Try again##gpumolcloud")) A.tryAgain();
            noteItem("gpumolcloud.again");
            if (ImGui::IsItemHovered()) ui::SetTooltip("The graphics card's program is made and checked again when the Density style's cloud is next shown, with a marker before each step:\n"
                                                          "if the app stops again, it starts the next time with it switched off, as now.");
        }
        ImGui::BeginDisabled(!app.computeOk);
        wrappedCheckbox("GPU Density cloud (OpenGL 4.3)", &app.moleculeView.gpuCloud);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ui::SetTooltip("On (the default, needs OpenGL 4.3): the Density style's cloud is made and drawn by the graphics card: each frame a compute shader turns every dot\n"
                           "(kept on the card) into its place, size and brightness, and the exposure is summed there, once its start-up check on this PC has passed.\n"
                           "The picture is the same as the processor's up to rounding in the last digits. Off: the processor turns, sizes and sends every dot each frame.\n"
                           "Used for comparing with the benchmark's molecule.density and molecule.vib_density. Not saved: it is On again at the next start.");
    }
    if (app.lab == blackhole::kLabIndex) {                // 0.55.6: who fills the spinning hole's light-path table: the line says which and why, the switch compares
        blackhole::BlackHoleView& v = app.blackholeView;
        // 0.55.7: switched off by the guard (a past run died inside the card's path, or a step did not finish in time, with this app version and driver):
        // that line instead, with "Try again"; the log's folder can be opened (the benchmark report ends with the log's last lines)
        app::GpuGuard& G = app::gpuGuard();
        const std::string off = G.offText();
        const char* line = !off.empty() ? nullptr
                         : !v.gpuPaths ? "Light paths of the spinning hole: the processor (GPU light paths is off)."
                         : v.cardState() == 1 ? "Light paths of the spinning hole: the graphics card (its check on this PC passed)."
                         : v.cardState() == 0 || v.cardState() == 2 ? "Light paths of the spinning hole: the graphics card, once its check on this PC has passed (made when a spinning hole is first drawn)."
                         : nullptr;
        const std::string text = !off.empty() ? off : line ? std::string(line) : "Light paths of the spinning hole: the processor, because " + v.cardWhy() + ".";
        settingMark("gpupaths");                          // (0.55.7: the row starts at its line, so "go to" shows the buttons too)
        ImGui::TextWrapped("%s", text.c_str());
        if (shellLogOn()) auditNote("bhpaths " + text);   // (the checks: the line as drawn)
        if (!off.empty()) {
            if (ImGui::Button("Try again##gpupaths")) G.tryAgain();
            noteItem("gpupaths.again");
            if (ImGui::IsItemHovered()) ui::SetTooltip("The graphics card's light paths are tried again at the next spinning picture, with a marker before each step:\n"
                                                          "if the app stops again, it starts the next time with them switched off, as now.");
            ImGui::SameLine();
        }
        if (!G.folder().empty()) {
            if (ImGui::Button("Open the log's folder##gpupaths")) SDL_OpenURL(("file:///" + G.folder()).c_str());
            noteItem("gpupaths.log");
            if (ImGui::IsItemHovered()) ui::SetTooltip("The folder with %s: what the graphics card's light paths did at this start, step by step\n"
                                                          "(the driver's messages, each step's time, the check's numbers). The benchmark report ends with its last 40 lines.", app::GpuGuard::kLog);
        }
        wrappedCheckbox("GPU light paths (OpenGL 4.3)", &v.gpuPaths);
        if (ImGui::IsItemHovered()) ui::SetTooltip("On (the default, needs OpenGL 4.3 with double precision): the light paths' table of the spinning hole is filled by a compute shader on the graphics card,\n"
                                                      "the same rays and steps as the processor's in double precision, at the fine table's size while the view moves and at rest. It is used only after a check\n"
                                                      "at the first spinning picture: rays followed by the card against the processor's own; if they differ, the processor's path is used and this page says so.\n"
                                                      "Off: the processor fills the table, a coarse one while the view moves and the fine one once it rests, as up to 0.55.5.\n"
                                                      "Used for comparing on your PC with the benchmark's scene 13. Not saved: it is On again at the next start.");
        // 0.56.0: who draws the spinning hole's sky and disc: one line which and why; the switch compares; its own guard (Try again)
        app::GpuGuard& S = app::gpuSkyGuard();
        const std::string soff = S.offText();
        const std::string sline = !soff.empty() ? "The spinning hole's sky and disc: the processor's table, because the graphics card's own drawing of them " +
                                                      soff.substr(soff.find("stopped") != std::string::npos ? soff.find("stopped") : 0)
                                : !v.gpuSky ? std::string("The spinning hole's sky and disc: the processor's table (GPU sky per pixel is off).")
                                : v.skyState() == 1 ? std::string("The spinning hole's sky and disc: the graphics card, every pixel from the closed formulas (its check on this PC passed); the stars from the processor.")
                                : v.skyState() == 0 || v.skyState() == 2 ? std::string("The spinning hole's sky and disc: the graphics card, once its check on this PC has passed (made when a spinning hole is first drawn).")
                                : "The spinning hole's sky and disc: the processor's table, because " + v.skyWhy() + ".";
        settingMark("gpusky");
        ImGui::TextWrapped("%s", sline.c_str());
        if (shellLogOn()) auditNote("bhsky " + sline);
        if (!soff.empty()) {
            if (ImGui::Button("Try again##gpusky")) S.tryAgain();
            noteItem("gpusky.again");
            if (ImGui::IsItemHovered()) ui::SetTooltip("The graphics card's drawing of the spinning hole is tried again at the next spinning picture, with a marker before each step:\n"
                                                          "if the app stops again, it starts the next time with it switched off, as now.");
        }
        // 0.56.2: only on a card too slow for 60 sharp pictures a second: one line, and the switch Lower resolution while moving (Auto / Off)
        const double slowMs = v.gpuSky ? v.skySlowMs() : -1;
        if (slowMs > blackhole::BlackHoleView::kSkySharpMs) {
            char lb[400];
            std::snprintf(lb, sizeof lb, v.lowResMode == 0
                ? "This graphics card takes %.1f ms for the whole picture (fewer than 60 sharp pictures a second): while you move the view it is drawn at a lower resolution, sharp again at rest."
                : "This graphics card takes %.1f ms for the whole picture (fewer than 60 sharp pictures a second): it stays sharp while you move the view, at fewer pictures a second.", slowMs);
            ImGui::TextWrapped("%s", lb);
            if (shellLogOn()) auditNote(std::string("bhlowres ") + lb);
            ImGui::TextUnformatted("Lower resolution while moving:");
            ImGui::SameLine();
            if (ImGui::RadioButton("Auto##bhlowres", v.lowResMode == 0)) v.lowResMode = 0;
            noteItem("bhlowres.auto");
            ImGui::SameLine();
            if (ImGui::RadioButton("Off##bhlowres", v.lowResMode == 1)) v.lowResMode = 1;
            noteItem("bhlowres.off");
            if (ImGui::IsItemHovered()) ui::SetTooltip("Auto: while the view moves, the spinning hole is drawn at a lower resolution so that it keeps up with your hand.\n"
                                                          "Off: every pixel always; the picture changes less often while you move it. Not saved: Auto again at the next start.");
        }
        wrappedCheckbox("GPU sky per pixel (spinning hole)", &v.gpuSky);
        if (ImGui::IsItemHovered()) ui::SetTooltip("On (the default): the spinning hole's sky and disc are drawn by the graphics card for every pixel from the closed formulas of the light paths\n"
                                                      "(Gralla and Lupsasca 2020) in single precision, after a check at the first spinning picture against the processor's double precision.\n"
                                                      "Off: the processor's table of light paths draws them, as in 0.55.9. Either way the stars come from the processor's table.\n"
                                                      "Used for comparing on your PC with the benchmark's scene 13. Not saved: it is On again at the next start.");
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
        bool showFps = app.prefs.fps == "on";              // 0.22 stage 6: the counter on the picture is off unless asked for
        if (wrappedCheckbox("Show fps on the picture", &showFps)) app.prefs.fps = showFps ? "on" : "off";
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
                                                      "If the picture or the fps counter still freezes while you drag, and this stays at 0, please report it.", lv.leadInMs);
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
