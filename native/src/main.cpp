#include <chrono>
// Physics Lab: the app's start, the window, the frame loop and the input.
//
// One window: the Orbit Lab picture on the left (drawn with OpenGL 3.3), the controls on
// the right (Dear ImGui). The physics runs in fixed steps of 1/240 s of real time, like

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "app/gpu_guard.h"
#include "app/gpu_helper.h"
#include "app/frame_cost.h"
#include "app/app_data.h"
#include "app/bench.h"
#include "app/frame_time.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "orbit/lines_check.h"
#include "molecule/surface_check.h"
#include "orbit/format.h"
#include "orbit/units.h"
#include "render/gl_loader.h"
#include "render/renderer2d.h"
#include "render/vk_backend.h"
#include "render/vk_card.h"
#include "ui/changelog.h"
#include "ui/fonts.h"
#include "ui/learn.h"
#include "ui/shell_widgets.h"
#include "ui/look.h"
#include "ui/settings.h"
#include "ui/shot.h"
#include "ui/benchmark.h"
#include "orbit/sky.h"
#include "ui/app_pages.h"
#include "ui/app_tabs.h"
#include "atom/atom_bench.h"
#include "atom/outline.h"
#include "atom/atom_view.h"
#include "ui/atom_learn.h"
#include "ui/molecule_learn.h"
#include "ui/blackhole_learn.h"
#include "blackhole/blackhole_view.h"
#include "ui/power.h"
#include "app/display_scale.h"
#include "atom/atom_tools.h"
#include "ui/atom_panel.h"
#include "ui/orbit_panel.h"
#include "ui/reading.h"
#include "ui/input_log.h"
#include "imgui_internal.h"
#include "ui/shell.h"
#include "ui/shell_test.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"

namespace {

#include "ui/icon_data.inc"      // kIconRgba: the window icon (64 x 64)

SDL_Window* createWindow(float scale, bool multisample, const app::WindowState& ws, bool gl43, bool vulkan) {   // gl43: ask for an OpenGL 4.3 core context (0.13 stage 2: compute shaders), else 3.3; 0.59.0: vulkan: a Vulkan window
    if (!vulkan) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl43 ? 4 : 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, multisample ? 1 : 0);   // smooth edges on lines and dots
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample ? 4 : 0);
    }
    const std::string title = std::string("Physics Lab ") + ui::appVersion() + " — Orbit Lab";
    int w = static_cast<int>(1400 * scale), h = static_cast<int>(860 * scale);          // the first time: 1400 × 860 points
    if (ws.w > 0) { w = ws.w; h = ws.h; }                                              // later: where it was (window-system units)
    if (const char* wh = SDL_getenv("PL_WINDOW")) { int a = 0, b = 0; if (std::sscanf(wh, "%dx%d", &a, &b) == 2) { w = static_cast<int>(a * scale); h = static_cast<int>(b * scale); } }   // for automatic checks: PL_WINDOW=1000x640
    return SDL_CreateWindow(title.c_str(), w, h,
                            (vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL) | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
}

// line breaks at about 70 characters (not every message box wraps long lines by itself)
std::string wrap70(const std::string& s) {
    std::string out, line;
    std::size_t i = 0;
    while (i <= s.size()) {
        std::size_t e = s.find_first_of(" \n", i);
        if (e == std::string::npos) e = s.size();
        const std::string word = s.substr(i, e - i);
        if (!line.empty() && line.size() + 1 + word.size() > 70) { out += line + "\n"; line = word; } else line += (line.empty() ? "" : " ") + word;
        if (e < s.size() && s[e] == '\n') { out += line + "\n"; line.clear(); }
        i = e + 1;
    }
    return out + line;
}

// A start-up problem: say it in plain words, and what to try. `what` is the short technical name of the problem, kept on the last line.
void fail(const char* what, const char* explain, const char* tryThis) {
    std::fprintf(stderr, "%s: %s\n", what, SDL_GetError());
    if (SDL_getenv("PL_EXIT_AFTER_FRAMES") || SDL_getenv("PL_EXIT_AFTER_SECONDS")) return;    // an automatic check: no window waiting for a click
    const std::string text = std::string(explain) + "\n\nWhat to try:\n" + tryThis + "\n\n(Technical detail: " + what + ": " + SDL_GetError() + ")";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Physics Lab cannot start", wrap70(text).c_str(), nullptr);
}

// While the window system has the window in its own move / resize loop (Windows: a drag by the title bar or an edge) the normal main
// loop does not run. SDL then sends SDL_EVENT_WINDOW_EXPOSED with data1 = 1 about every 10 ms, from the main thread, straight to the event
// watchers, and asks the app to redraw from there. This is the documented way (see SDL_EVENT_WINDOW_EXPOSED in SDL_events.h); it keeps
// everything, Dear ImGui included, on one thread. `draw` runs one whole frame (physics, picture and panels, without reading events).
struct LiveWatch {
    std::function<void()> draw;
    bool busy = false;                                  // a frame is being built: do not start another inside it
};
bool SDLCALL onLiveExpose(void* userdata, SDL_Event* ev) {
    if (ev->type != SDL_EVENT_WINDOW_EXPOSED || ev->window.data1 != 1 || !SDL_IsMainThread()) return true;
    LiveWatch* w = static_cast<LiveWatch*>(userdata);
    if (!w->busy && w->draw) w->draw();
    return true;
}

// 0.59.0 (Vulkan step 1): the picture area without the lab's picture: one line in its middle (wrapped on a narrow window); 0.60.0: the Black Hole Lab on Vulkan
void pictureNote(ImDrawList* dl, ImVec2 pos, ImVec2 size, app::Rgb c, const char* note) {
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    const float wrap = std::max(40.0f, size.x - 2.0f * fs);
    const ImVec2 ts = font->CalcTextSizeA(fs, wrap, wrap, note);
    const ImVec2 at(std::floor(pos.x + (size.x - ts.x) * 0.5f), std::floor(pos.y + (size.y - ts.y) * 0.5f));
    dl->AddText(font, fs, at, ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1.0f)), note, nullptr, wrap);
}

const char* const kDriverAdvice =
    "1. Update the graphics driver: get the newest one from the maker of your graphics card (NVIDIA, AMD or Intel), or run Windows Update.\n"
    "2. Restart the computer and try again.\n"
    "3. If you are using Remote Desktop or a virtual machine, run Physics Lab on the computer itself: those often have no 3D graphics.";

}  // namespace

static const std::chrono::steady_clock::time_point g_appStart = std::chrono::steady_clock::now();   // 0.56.1: the start of the app (static initialisation, before main), for the start-up time
static double g_viewMs = 0;   // (PL_TIMESTAT)
// 0.66.0 (PL_SECTSTAT, measuring in development): the frame thread's time in the frame's parts, averaged over 60 frames, printed as PLSECTSTAT
namespace {
const char* const kSectNames[] = {"physics", "shell", "picture", "panels", "draw", "rest"};
double g_sectMs[6] = {}, g_sectT = 0;
int g_sectFrames = 0;
double sectNow() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void sectMark(int i) {   // the part i ends now (the next part starts)
    static const bool on = SDL_getenv("PL_SECTSTAT") != nullptr;
    if (!on) return;
    const double t = sectNow();
    if (i >= 0 && g_sectT > 0) g_sectMs[i] += t - g_sectT;
    g_sectT = t;
    if (i == 5 && ++g_sectFrames == 60) {
        std::printf("PLSECTSTAT");
        for (int k = 0; k < 6; k++) std::printf(" %s=%.3f", kSectNames[k], g_sectMs[k] / 60);
        std::printf("\n"); std::fflush(stdout);
        for (double& m : g_sectMs) m = 0;
        g_sectFrames = 0;
    }
}
}  // namespace
// 0.67.0: the Orbit Lab at rest keeps its picture: while OrbitView::pictureKey (and the picture's place, scale, grey choice and the font's texture) stay the
// same, the picture is not drawn again (Renderer2D::keepLast: Vulkan shows the window's kept picture; OpenGL draws the kept lists again, without the work of
// making them) and its labels are the kept vertices of the last drawing. PL_ORBIT_NOKEEP=1 (checks): always drawn, to compare.
namespace {
struct OrbitKeep {
    std::uint64_t key = 0;
    bool have = false, textOk = false;
    std::vector<ImDrawVert> vtx;
    std::vector<ImDrawIdx> idx;                        // (from the slice's first vertex)
    long kept = 0, drawn = 0;
};
OrbitKeep g_orbitKeep;
std::uint64_t orbitFrameKey(const ui::AppState& app, const render::Renderer2D& R, double headerH, double scale, bool gray) {
    std::uint64_t k = app.view.pictureKey(app.sys, app.set, 0, headerH, scale, app.canvasW, app.canvasH);
    for (int m = 0; m < int(render::Map::Count); m++) k = (k ^ (R.hasMap(render::Map(m)) ? 2u * unsigned(m) + 1u : 2u * unsigned(m))) * 1099511628211ull;   // (a map loaded later)
    const ImFontAtlas* fa = ImGui::GetIO().Fonts;
    const double extra[8] = {gray ? 1.0 : 0.0, double(reinterpret_cast<std::uintptr_t>(ImGui::GetFont())), ImGui::GetFontSize(), double(reinterpret_cast<std::uintptr_t>(fa->TexData)),
                             fa->TexData ? double(fa->TexData->Width) : 0.0, fa->TexData ? double(fa->TexData->Height) : 0.0, fa->TexUvScale.x, fa->TexUvScale.y};
    for (double e : extra) { std::uint64_t b = 0; std::memcpy(&b, &e, sizeof b); k = (k ^ b) * 1099511628211ull; }
    // 0.68.0: while the card's lines are being made and checked (their steps run as the picture is drawn) a new key each frame; then their path is in the key
    static std::uint64_t unsettled = 0;
    if (!orbit::linesCardSettled(R.vulkan())) k = (k ^ ++unsettled) * 1099511628211ull;
    k = (k ^ std::uint64_t(orbit::linesCardState() + 2)) * 1099511628211ull;
    return k;
}
}  // namespace
// 0.55.5 (automatic checks and measuring): PL_BH_MOVE="kind:first:count" moves the Black Hole Lab's view every frame from frame first for count frames
// (turn: round the axis; tilt: up; dist: closer by 1 % a frame; spin: +0.005 a frame); PL_BH_FRAMETIME prints, per frame, the frame thread's time in the lab
static double g_bhDrawMs = 0;
static void bhMoveStep(blackhole::Settings& s, const char* spec, int frame) {   // (a list: "turn:20:20,tilt:60:20")
    for (const char* q = spec; q && *q; q = std::strchr(q, ',') ? std::strchr(q, ',') + 1 : nullptr) {
        char kind[16] = {0};
        int first = 0, count = 0;
        if (std::sscanf(q, "%15[a-z]:%d:%d", kind, &first, &count) != 3 || frame < first || frame >= first + count) continue;
        const std::string k = kind;
        if (k == "turn") blackhole::turn(s, 0.01, 0);
        else if (k == "tilt") blackhole::turn(s, 0, -0.01);
        else if (k == "dist") blackhole::setDistance(s, s.dist * 0.99);
        else if (k == "spin") blackhole::setSpin(s, s.spin + 0.005);
    }
}
int main(int argc, char** argv) {
    // 0.67.0: the crash-safe helper (app/gpu_helper.h): the same exe, no window, makes one card program's pipelines and exits
    if (argc > 5 && std::strcmp(argv[1], "--gpu-helper") == 0) return app::gpuHelperMain(argc, argv);
    app::gpuGuardFrameThread();                         // 0.63.0: the guards never make this thread wait (app/gpu_guard.h)
    if (!SDL_Init(SDL_INIT_VIDEO)) { fail("Could not start the window system", "Physics Lab could not start its window system, so it has nothing to draw on.", "Restart the computer and try again. If it still fails, please report it with your Windows version."); return 1; }

    // 0.15 stage 2 (C3): ask Windows not to power-throttle the app (the owner saw the frame rate fall a lot when the window merely lost focus on his second monitor)
    const std::string powerNote = ui::requestNoPowerThrottling();
    std::printf("%s\n", powerNote.c_str());
    // the first reading is the primary monitor's scale (the window does not exist yet); it is read again from the window's own monitor right after the window is made,
    // and whenever SDL says the scale changed (0.15 stage 2, C4)
    // PL_SCALE=1.25 stands in for a Windows scale of 125 % (automatic checks of the text sizes at 1.0, 1.25, 1.5 and 2.0; the cloud session has no Windows)
    const float scaleOverride = SDL_getenv("PL_SCALE") ? static_cast<float>(std::atof(SDL_getenv("PL_SCALE"))) : 0.0f;
    app::DisplayScale displayScale(scaleOverride > 0 ? scaleOverride : SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()));   // 1.5 on a Windows screen at 150 %
    float scale = displayScale.scale();
    // the saved settings come first: they say where the window was
    ui::AppState app;
    ui::resetToToday(app);
    app.atom.p = atom::AtomParams::appDefaults(); app.atom.restart();   // the app starts with the glowing, denser cloud
    ui::loadSettings(app);                              // the theme, text size, font, motion, units and the labs' settings, and the window
    std::vector<app::ScreenRect> screens;
    {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        for (int i = 0; ids && i < n; i++) { SDL_Rect r; if (SDL_GetDisplayBounds(ids[i], &r)) screens.push_back({r.x, r.y, r.w, r.h}); }
        SDL_free(ids);
    }
    app::WindowState start;                             // size 0: the default
    if (app.prefs.window) start = app::fitWindow(*app.prefs.window, screens);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();                             // (0.59.0: before the window: Vulkan's start makes Dear ImGui's Vulkan pipeline, and falls back to OpenGL if it cannot)
    // 0.59.0: Vulkan, step 1 of 5 (PLAN_VULKAN.md): Settings, Performance, Graphics (the settings line "graphics="; PL_GRAPHICS=vulkan / opengl for tests).
    // Each step of Vulkan's start is guarded (app/gpu_guard.h gpuVulkanGuard, log lines "[vulkan] ..."): a step that fails or crashes is kept, and the app
    // starts on OpenGL (this start too, when a step fails). Vulkan never stops the app from starting.
    if (const char* g = SDL_getenv("PL_GRAPHICS")) if (std::string(g) == "vulkan" || std::string(g) == "opengl") app.prefs.graphics = g;
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;
    render::VkBackend vk;
    render::Renderer2D renderer;                        // (0.60.0: before Vulkan's start, whose last steps make the labs' pictures on Vulkan)
    bool useVulkan = false;
    app::gpuGuardSetApp(ui::appVersion());               // 0.63.1: each marker names the app version that wrote it
    if (app.prefs.graphics == "vulkan") {
        app::GpuGuard& V = app::gpuVulkanGuard();
        V.start(ui::settingsFolder(), std::string("vulkan ") + ui::appVersion(), "any", app.prefs.gpuVulkan,
                {std::string("Physics Lab ") + ui::appVersion() + ": the log of Vulkan's start and of the graphics card's paths; overwritten at each start",
                 "Graphics: Vulkan (the default; OpenGL the backup): its own guard, its lines start [vulkan]"});
        if (!V.allowed()) app.vulkanWhy = V.offText();
        else if (!(window = createWindow(scale, false, start, false, true))) { V.failed("window", std::string("SDL_CreateWindow: ") + SDL_GetError()); app.vulkanWhy = V.offText(); }
        else {
            render::VkBackend::Options o;
            o.validation = SDL_getenv("PL_VK_VALIDATION") != nullptr;
            if (const char* f = SDL_getenv("PL_VK_FAIL")) o.failAt = f;   // for automatic checks: that step fails, as on a PC whose driver says no
            std::string why;
            // 0.60.0 (step 2): then the labs' pictures (renderer2d on Vulkan: its pipelines, the maps, a start-up check of a tiny known picture), guarded too;
            // PL_NO_MSAA (checks): one sample, on OpenGL too; PL_VK_CHECK=break (checks): the start-up check's picture drawn wrong on purpose
            const char* chk = SDL_getenv("PL_VK_CHECK");
            if (vk.init(window, V, o, why) && vk.initPicture(renderer, !SDL_getenv("PL_NO_MSAA"), chk && std::string(chk) == "break", why)) useVulkan = true;
            else { renderer.useVulkan(nullptr); vk.shutdown(); SDL_DestroyWindow(window); window = nullptr; app.vulkanWhy = V.offText().empty() ? why : V.offText(); }
        }
        if (!useVulkan) { V.log("This start uses OpenGL: " + app.vulkanWhy); app::gpuGuard().appendToLog(); }
        if (V.saved() != app.prefs.gpuVulkan) ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);   // kept at once (a crash later must not lose it)
        else V.settingsWritten();
    }
    app.graphicsInUse = useVulkan ? "vulkan" : "opengl";
    if (useVulkan) {
        app.vulkanDevice = vk.deviceName(); app.vulkanDriver = vk.driverText();
        std::printf("Vulkan: %s | driver %s | Vulkan %s | %s\n", vk.deviceName().c_str(), vk.driverText().c_str(), vk.apiText().c_str(), vk.presentText().c_str());
        std::printf("Vulkan picture: %s\n", vk.pictureText().c_str());   // 0.60.0
        // 0.61.0 (step 3): the card programs on Vulkan, each under a guard of its own (their markers, the settings lines "gpuvk<name>=", the log's lines
        // "[vulkan] [<name>] "): the groups, the outline, the rocks and the Glow dots are made now; the Slice, the jump and the Density cloud by their labs'
        // paths when first shown, as on OpenGL. A program that fails takes the processor's path; the rest of Vulkan stays on.
        app::useVulkanGuards(true);
        bool cardsChanged = false;
        for (const auto& [name, g] : app::vulkanCardGuards()) {
            g->start(ui::settingsFolder(), render::vkCardVersion(name), vk.deviceName() + " / " + vk.driverText(), app.prefs.gpuVk[name], {});
            cardsChanged = cardsChanged || g->saved() != app.prefs.gpuVk[name];
        }
        if (cardsChanged) ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);   // kept at once (a crash later must not lose it)
        else for (const auto& [name, g] : app::vulkanCardGuards()) g->settingsWritten();
        // 0.67.0: the programs whose first steps have not passed with this driver and that the helper knows are made first by helper processes (one at a
        // time, while the app shows its first pictures); their guards hold those steps until their helper has finished. While the light paths are off on
        // this driver, the helpers' detective work on kKerrTableCS (once per driver). PL_NO_GPU_HELPER=1 (tests): no helper, as before.
        if (!SDL_getenv("PL_NO_GPU_HELPER") || std::string(SDL_getenv("PL_NO_GPU_HELPER")) == "0") {
            std::vector<std::pair<std::string, app::GpuGuard*>> helped;
            for (const auto& [name, g] : app::vulkanCardGuards()) if (app::gpuHelperKnows(name) && g->allowed() && g->guarded()) helped.push_back({name, g});
            app::gpuHelperQueue(helped, ui::settingsFolder(), vk.deviceName(), vk.driverText());
            app::GpuGuard& lp = app::gpuGuard();
            if (lp.started() && !lp.allowed()) app::gpuHelperVariants(&lp, ui::settingsFolder(), vk.deviceName(), vk.driverText());
            // 0.68.0: the four programs made at this start (the groups, the outline, the rocks, the Glow dots: their steps run in vk.initCards below, not
            // through hold()) wait here for their helpers, which are first in the queue (only while not passed with this driver: a known driver waits for none)
            const double helpT0 = SDL_GetTicks() / 1000.0;
            while ((app::gpuHelperPending("groups") || app::gpuHelperPending("outline") || app::gpuHelperPending("rocks") || app::gpuHelperPending("cloud")) &&
                   SDL_GetTicks() / 1000.0 - helpT0 < 60) {
                app::gpuHelperPump();
                SDL_Delay(5);
            }
        }
        std::vector<std::string> cardsWhy;
        vk.initCards(renderer, cardsWhy);
        for (const std::string& w : cardsWhy) { app::gpuVulkanGuard().log("card program on the processor: " + w); std::printf("Vulkan card program on the processor: %s\n", w.c_str()); }
        if (SDL_getenv("PL_NO_COMPUTE")) renderer.disableCompute();   // (as on OpenGL: the processor's paths, to compare)
        app.glMajor = 0; app.glMinor = 0;                             // (0: Vulkan, app/bench.h computePathText)
        app.computeOk = renderer.compute() != nullptr; app.rocksOk = renderer.rockCompute() != nullptr; app.rocksWhy = renderer.rocksWhy();
        atom::setOutlineCompute(renderer.outlineCompute());
        std::printf("Vulkan card programs: %zu bytes of SPIR-V; double precision in shaders: %s\n", vk.cardSpirvBytes(), vk.float64() ? "yes" : "no");
        std::printf("Atom Lab superposition and jumps: %s\n", app::computePathText(0, 0, app.computeOk, true).c_str());
        std::printf("Orbit Lab rocks: %s\n", app::rocksPathText(0, 0, app.rocksOk, true, app.rocksWhy).c_str());
    } else {
        // 0.13 stage 2: first ask for OpenGL 4.3 (the Atom Lab's superposition and jumps then run on the GPU), with smooth edges, then without them; if the PC cannot give 4.3, the same two tries
        // for 3.3 as before (and the CPU does that work).
        for (const bool want43 : {true, false}) {
            for (const bool multisample : {true, false}) {
                if (multisample && SDL_getenv("PL_NO_MSAA")) continue;   // 0.60.0 (checks): one sample, to compare with Vulkan's pictures pixel for pixel
                window = createWindow(scale, multisample, start, want43, false);
                gl = window ? SDL_GL_CreateContext(window) : nullptr;
                if (gl) break;
                if (window) { SDL_DestroyWindow(window); window = nullptr; }
            }
            if (gl) break;
        }
        if (SDL_getenv("PL_FAKE_GLFAIL")) { if (gl) SDL_GL_DestroyContext(gl); gl = nullptr; if (window) SDL_DestroyWindow(window); window = nullptr; }   // for automatic checks: pretend OpenGL is missing
        if (!window || !gl) { fail("Could not open an OpenGL 3.3 window", "Physics Lab draws with OpenGL 3.3 (3D graphics), and your computer did not give it a window to draw in. This usually means the graphics driver is too old, or missing.", kDriverAdvice); return 1; }
    }
    if (const char* c = SDL_getenv("PL_ATOM_CARD")) if (std::string(c) == "0") { app.atom.gpuSlice = false; app.atom.gpuJump = false; }   // 0.57.0 (tests: the processor for both, to compare)
    if (const char* c = SDL_getenv("PL_MOL_CARD")) if (std::string(c) == "0") app.moleculeView.gpuCloud = false;   // 0.57.1 (tests: the processor's Density cloud, to compare)
    if (SDL_Surface* icon = SDL_CreateSurfaceFrom(64, 64, SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char*>(kIconRgba), 64 * 4)) {   // the window's icon (title bar, taskbar)
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
    if (!useVulkan) {
        SDL_GL_MakeCurrent(window, gl);
        SDL_GL_SetSwapInterval(1);                      // vsync: smooth, and no wasted work (Vulkan: FIFO, render/vk_backend.cpp)
    }
    if (start.w > 0 && start.hasPos) SDL_SetWindowPosition(window, start.x, start.y);
    else SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);   // the first time, or the saved place is off every screen now
    SDL_ShowWindow(window);
    Uint64 maximizeUntil = 0;                           // the window system maximises a moment later: do not record the old state meanwhile
    if (start.w > 0 && start.maximized) { SDL_MaximizeWindow(window); maximizeUntil = SDL_GetTicks() + 1500; }

    if (!useVulkan) {
        const char* missing = nullptr;
        if (!plgl::loadGL(&missing)) { std::fprintf(stderr, "OpenGL function missing: %s\n", missing); fail("OpenGL 3.3 is not available", "Physics Lab needs OpenGL 3.3 (3D graphics), and your graphics driver does not offer all of it.", kDriverAdvice); return 1; }
        std::printf("OpenGL: %s | %s\n", plgl::glGetString(GL_VERSION), plgl::glGetString(GL_RENDERER));
        if (!renderer.init()) { fail("Could not set up the OpenGL shaders", "Your graphics driver started, but would not accept the drawing programs Physics Lab sends it.", kDriverAdvice); return 1; }
        if (SDL_getenv("PL_NO_COMPUTE")) renderer.disableCompute();   // for the owner and automatic checks: run the CPU path even on a PC that has OpenGL 4.3
        {   // 0.55.7: the guard round the graphics card's light paths and their log (app/gpu_guard.h), beside the settings file; a marker left by the last run
            // switches the path off before anything of it runs; the settings file keeps that at once
            const auto str = [](GLenum e) { const char* t = reinterpret_cast<const char*>(plgl::glGetString(e)); return std::string(t ? t : "?"); };
            const std::string vendor = str(GL_VENDOR), gpu = str(GL_RENDERER), version = str(GL_VERSION), glsl = str(0x8B8C /* GL_SHADING_LANGUAGE_VERSION */);
            const bool fp64 = SDL_GL_ExtensionSupported("GL_ARB_gpu_shader_fp64");
            GLint maj = 0, min = 0;
            plgl::glGetIntegerv(GL_MAJOR_VERSION, &maj); plgl::glGetIntegerv(GL_MINOR_VERSION, &min);
            const std::vector<std::string> head = {
                std::string("Physics Lab ") + ui::appVersion() + ": the log of the graphics card's light paths (the Black Hole Lab's spinning hole); overwritten at each start",
                "GPU: " + gpu, "Vendor: " + vendor, "OpenGL: " + version + " (context " + std::to_string(maj) + "." + std::to_string(min) + ")", "GLSL: " + glsl,
                std::string("Double precision (GL_ARB_gpu_shader_fp64): ") + (fp64 ? "reported" : "NOT reported") + (maj >= 4 ? " (core in OpenGL 4.0 and later)" : "")};
            const char* fakeDriver = SDL_getenv("PL_GPU_DRIVER");          // (tests: another driver's version string)
            // 0.55.9: the guard keeps the light-path shader's own text (its hash) in place of the app version: a new app version alone does not try again
            const char* fakeShader = SDL_getenv("PL_GPU_SHADER");          // (tests: another shader's text)
            app::gpuGuard().start(ui::settingsFolder(), fakeShader ? std::string(fakeShader) : render::kerrShaderId(), fakeDriver ? std::string(fakeDriver) : vendor + " / " + gpu + " / " + version, app.prefs.gpuPaths, head);
            // 0.56.0: the spinning hole drawn per pixel: a guard of its own (its marker, the settings line "gpusky=", the same log); PL_GPU_SKYSHADER (tests)
            const char* fakeSky = SDL_getenv("PL_GPU_SKYSHADER");
            app::gpuSkyGuard().start(ui::settingsFolder(), fakeSky ? std::string(fakeSky) : render::kerrSkyShaderId(), fakeDriver ? std::string(fakeDriver) : vendor + " / " + gpu + " / " + version, app.prefs.gpuSky,
                                     {"The spinning hole drawn per pixel by the graphics card (closed forms, single precision): its own guard, its lines start [sky]"});
            // 0.57.0: the Atom Lab's two card programs, a guard each (their markers, the settings lines "gpuslice=" and "gpujump=", the same log); PL_GPU_SLICESHADER,
            // PL_GPU_JUMPSHADER (tests: another program's text)
            const char* fakeSlice = SDL_getenv("PL_GPU_SLICESHADER");
            const char* fakeJump = SDL_getenv("PL_GPU_JUMPSHADER");
            app::gpuSliceGuard().start(ui::settingsFolder(), fakeSlice ? std::string(fakeSlice) : render::sliceShaderId(), fakeDriver ? std::string(fakeDriver) : vendor + " / " + gpu + " / " + version, app.prefs.gpuSlice,
                                       {"The Atom Lab's Slice dots made by the graphics card (single precision): its own guard, its lines start [slice]"});
            app::gpuJumpGuard().start(ui::settingsFolder(), fakeJump ? std::string(fakeJump) : render::jumpShaderId(), fakeDriver ? std::string(fakeDriver) : vendor + " / " + gpu + " / " + version, app.prefs.gpuJump,
                                      {"The Atom Lab's jump groups (every electron) made by the graphics card: its own guard, its lines start [jump]"});
            // 0.57.1: the Molecule Lab's Density cloud on the card (its marker, the settings line "gpumolcloud=", the same log); PL_GPU_MOLSHADER (tests)
            const char* fakeMol = SDL_getenv("PL_GPU_MOLSHADER");
            app::gpuMolCloudGuard().start(ui::settingsFolder(), fakeMol ? std::string(fakeMol) : render::molCloudShaderId(), fakeDriver ? std::string(fakeDriver) : vendor + " / " + gpu + " / " + version, app.prefs.gpuMolCloud,
                                          {"The Molecule Lab's Density cloud made and drawn by the graphics card (single precision): its own guard, its lines start [molcloud]"});
            if (app::gpuGuard().saved() != app.prefs.gpuPaths || app::gpuSkyGuard().saved() != app.prefs.gpuSky || app::gpuSliceGuard().saved() != app.prefs.gpuSlice ||
                app::gpuJumpGuard().saved() != app.prefs.gpuJump || app::gpuMolCloudGuard().saved() != app.prefs.gpuMolCloud) ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);
            else { app::gpuGuard().settingsWritten(); app::gpuSkyGuard().settingsWritten(); app::gpuSliceGuard().settingsWritten(); app::gpuJumpGuard().settingsWritten(); app::gpuMolCloudGuard().settingsWritten(); }
        }
        {   // what this PC gave us: shown in the Performance card and printed in the benchmark report (0.13 stage 2)
            GLint maj = 0, min = 0;
            plgl::glGetIntegerv(GL_MAJOR_VERSION, &maj); plgl::glGetIntegerv(GL_MINOR_VERSION, &min);
            app.glMajor = maj; app.glMinor = min; app.computeOk = renderer.compute() != nullptr; app.rocksOk = renderer.rockCompute() != nullptr;
            atom::setOutlineCompute(renderer.outlineCompute());           // the 90 % outline's compute object (null on a 3.3 PC or with PL_NO_COMPUTE: the CPU makes it)
            std::printf("Atom Lab superposition and jumps: %s\n", app::computePathText(maj, min, app.computeOk, true).c_str());
            std::printf("Orbit Lab rocks: %s\n", app::rocksPathText(maj, min, app.rocksOk, true).c_str());
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                           // nothing saved next to the .exe
    if (SDL_getenv("PL_FIXED_DT")) io.ConfigInputTextCursorBlink = false;       // for automatic checks: a text field's cursor does not blink (its phase follows the real clock), so two runs draw the same picture
    io.ConfigDebugHighlightIdConflicts = false;         // never show ImGui's red "conflicting ID" box to a user (the conflicts themselves are fixed)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiStyle unscaledStyle = style;             // kept so a changed display scale can be applied from scratch (ScaleAllSizes only multiplies)
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.WindowRounding = 0;
    std::printf("Font: %s\n", ui::loadFonts(16.0f));
    if (useVulkan) ImGui_ImplSDL3_InitForVulkan(window);   // (Dear ImGui's Vulkan backend was started with the device: render/vk_backend.cpp)
    else {
        ImGui_ImplSDL3_InitForOpenGL(window, gl);
        ImGui_ImplOpenGL3_Init("#version 330 core");
    }

    if (SDL_getenv("PL_IMPERIAL")) fmt::setImperial(true);   // for automatic checks
    if (const char* t = SDL_getenv("PL_THEME")) app.prefs.theme = t;   // for automatic checks
    if (const char* t = SDL_getenv("PL_TEXTSIZE")) app.prefs.textSize = t;
    if (const char* t = SDL_getenv("PL_FONT")) app.prefs.font = t;
    if (const char* t = SDL_getenv("PL_FACTS")) app.prefs.facts = t;                  // for automatic checks: the fact line "on" / "off"
    if (SDL_getenv("PL_FACT_LONGEST")) {                 // for automatic checks (0.15 stage 5d): show the longest fact first
        int best = 0;
        for (int i = 0; i < ui::factCount(); i++) if (ui::factTextAt(i).size() > ui::factTextAt(best).size()) best = i;
        ui::factShow(app, best);
    }
    if (const char* t = SDL_getenv("PL_MOTION")) app.prefs.motion = t;
    if (const char* sp = SDL_getenv("PL_PANELS")) app.prefs.shellPanels = sp;   // 0.22 stage 6, for automatic checks: the floating cards' open / folded state ("atom.card=o"), so a check does not depend on the size-based default
    auto systemLight = [] { return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT || SDL_getenv("PL_LIGHT"); };   // Windows' light/dark setting
    ui::updateLook(app.prefs, systemLight(), ui::systemReducedMotion());
    if (ui::look().reduced) app.paused = true;          // reduced motion: the lab starts still, with a ▶ Play button
    app.visited[app.lab] = true;

    // 0.15 stage 2, C4: read the scale of the monitor the window is on; when it differs from the one in use, redo the sizes (spacing, text) and the smallest window
    const auto applyScale = [&](float s) {
        scale = s;
        ImGuiStyle& st = ImGui::GetStyle();
        st = unscaledStyle;
        st.ScaleAllSizes(scale);
        st.FontScaleDpi = scale;
        st.WindowRounding = 0;                          // (the theme's colours and roundings are put back by ui::updateLook, whose key includes the scale)
        ui::invalidateLook();
        int mw = 0, mh = 0;
        displayScale.minimumWindow(mw, mh);
        if (!SDL_getenv("PL_WINDOW")) SDL_SetWindowMinimumSize(window, mw, mh);   // (automatic picture checks choose their own window size)
    };
    const auto rereadScale = [&] {
        const float s = scaleOverride > 0 ? scaleOverride : SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
        if (s > 0 && displayScale.update(s)) applyScale(displayScale.scale());
    };
    applyScale(scale);                                  // the smallest window, and the window's own monitor's scale (it may not be the primary one)
    rereadScale();
    app.powerNote = powerNote;
    double acc = 0;                                     // real time not yet simulated, s
    Uint64 last = SDL_GetPerformanceCounter();
    bool vsyncOn = true;
    double fps = -1, fpsTime = 0;                      // fps < 0: not measured yet
    int fpsFrames = 0;
    bool mouseDown = false, dragging = false, rightDrag = false;
    float downX = 0, downY = 0, lastX = 0, lastY = 0;
    // For automatic checks (no effect in normal use): PL_EXIT_AFTER_FRAMES=n quits after n frames,
    // PL_SCREENSHOT=file.bmp saves the last frame, PL_SELECT=Earth selects and follows a body, PL_ZOOM=2.3 sets the zoom.
    const int maxFrames = SDL_getenv("PL_EXIT_AFTER_FRAMES") ? std::atoi(SDL_getenv("PL_EXIT_AFTER_FRAMES")) : 0;
    const char* screenshot = SDL_getenv("PL_SCREENSHOT");
    if (const char* name = SDL_getenv("PL_SELECT")) {
        for (const auto& b : app.sys.bodies)
            if (b.name == name) app.view.select({orbit::Selection::Body, b.id}, true);
        if (std::string(name) == "moon:Earth")    // for automatic checks: select Earth's Moon (its card)
            for (const auto& b : app.sys.bodies)
                if (b.name == "Earth") { app.set.moons = true; app.view.select({orbit::Selection::Moon, b.id, 0}, true); }
        if (std::string(name).rfind("far:", 0) == 0) { app.set.far = true; app.view.select({orbit::Selection::Far, 0, orbit::farIndexOf(std::string(name).substr(4))}, true); }   // e.g. far:sgra
    }
    if (const char* z = SDL_getenv("PL_ZOOM")) { ui::setZoom(app, std::atof(z)); app.fitStart = -1; }
    if (SDL_getenv("PL_3D")) app.set.view3D = true;
    if (const char* tl = SDL_getenv("PL_TILT")) { double yw = 0, pt = 0; if (std::sscanf(tl, "%lf,%lf", &yw, &pt) == 2) { app.view.yaw = yw; app.view.pitch = pt; } }   // for automatic checks: PL_TILT=yaw,pitch (radians) of the 3D view
    if (const char* ph = SDL_getenv("PL_PHYSICS")) { bool r = false; if (orbit::planetPhysicsFromText(ph, r)) app.set.realPlanets = r; }   // for automatic checks: Planet physics Simple / Real
    if (const char* g = SDL_getenv("PL_GR")) app.set.relativity = std::atoi(g) != 0;   // for automatic checks: General relativity (1PN) on (1) / off (0)
    if (const char* d = SDL_getenv("PL_DATE")) { int64_t ms; if (ui::parseDateTime(d, SDL_getenv("PL_TIME") ? SDL_getenv("PL_TIME") : "12:00", ms)) std::printf("%s\n", ui::jumpToDate(app, ms).c_str()); else std::printf("not a date\n"); }
    if (SDL_getenv("PL_PAUSED")) { app.paused = true; app.set.paused = true; }   // for automatic checks and pictures (0.51.2): time stands still at the date given
    if (SDL_getenv("PL_JOURNEY")) ui::journeyStart(app);
    if (const char* pn = SDL_getenv("PL_PANE")) app.pane = std::atoi(pn) == 1 ? 1 : 0;     // for automatic checks: open the Learn tab (1)
    if (const char* l = SDL_getenv("PL_LEARN")) app.learn = std::atoi(l);     // open a Learn guide (1, 2 or 3)
    if (const char* o = SDL_getenv("PL_OPEN")) { const std::string w = o; app.showSettings = w == "settings"; app.showAbout = w == "about"; app.showHistory = w == "history";   // for automatic checks: open a window
                                              if (w.rfind("settings:", 0) == 0) ui::settingsGoto(app, w.substr(9).c_str()); }   // (0.55.7: "settings:<row>": at that row)
    if (SDL_getenv("PL_CONS")) app.set.cons = orbit::ConsMode::LinesAndNames;
    // Atom Lab checks: PL_ATOM=1 opens the Atom Lab, PL_ATOM_SET="Z=26;view=Cloud;zoom=4.5" sets its settings,
    // PL_ATOM_DO="absorb" presses Emit / Absorb / Auto photons (or ⌕ Nucleus / ⌕ Nucleon: "nucleus", "nucleon"), PL_ATOM_FOCUS=n dives into nucleon n
    if (SDL_getenv("PL_ATOM")) app.lab = 1;
    if (const char* mv = SDL_getenv("PL_MOLECULE")) {   // 0.18: PL_MOLECULE=<id> opens the Molecule Lab on that molecule; PL_MOLECULE_STYLE=fill, PL_MOLECULE_PICK=atom,bond (automatic checks)
        app.lab = molecule::kLabIndex;
        if (molecule::findId(mv) >= 0) app.molecule.choose(molecule::findId(mv));
        if (const char* st = SDL_getenv("PL_MOLECULE_STYLE")) app.molecule.setStyle(std::string(st) == "fill" ? molecule::Style::SpaceFill : std::string(st) == "real" ? molecule::Style::Realistic : molecule::Style::BallStick);
        if (const char* rs = SDL_getenv("PL_MOLECULE_REAL")) {   // 0.21: the Realistic options as in the settings line, e.g. "rdots=60000 riso=0.002 rcol=pot rshow=surface"
            const molecule::Settings t = molecule::loadSettings(std::string("v1 ") + rs);
            app.molecule.set.realDots = t.realDots; app.molecule.set.realIso = t.realIso; app.molecule.set.realPot = t.realPot; app.molecule.set.realShow = t.realShow;
        }
        if (const char* pk = SDL_getenv("PL_MOLECULE_PICK")) std::sscanf(pk, "%d,%d", &app.molecule.sel.atom, &app.molecule.sel.bond);
        app.visited[molecule::kLabIndex] = true;
    }
    // 0.55.5: the Black Hole Lab works out its pictures on worker threads (the frame never waits for the processor); the automatic checks keep the
    // synchronous path (PL_BH_SPINWAIT, or PL_DRAWSTAT without PL_BH_ASYNC): their pictures are of the settings of the frame they are taken in
    app.blackholeView.async = !SDL_getenv("PL_BH_SPINWAIT") && (!SDL_getenv("PL_DRAWSTAT") || SDL_getenv("PL_BH_ASYNC"));
    if (SDL_getenv("PL_BH_GLARENOW")) app.blackholeView.glareNow = true;   // 0.62.0 (checks): the eye's glare made at once in its frame, on both APIs
    if (const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window))) if (dm->refresh_rate > 0) app.blackholeView.refreshHz = dm->refresh_rate;   // 0.56.0
    if (SDL_getenv("PL_BLACKHOLE")) {                   // 0.55.0: PL_BLACKHOLE=1 opens the Black Hole Lab; PL_BH_SET="preset=m87;dist=3;bend=off;sky=black;fov=120;mass=1e6;yaw=..;pitch=..;disc=on;feed=0.1;outer=30;exposure=auto" (automatic checks)
        app.lab = blackhole::kLabIndex;
        app.visited[blackhole::kLabIndex] = true;
        if (const char* bs = SDL_getenv("PL_BH_SET")) {
            std::map<std::string, std::string> m;
            blackhole::saveSettings(app.blackhole, m);
            const std::string list = bs;
            bool mass = false, preset = false, disc = false;
            for (std::size_t i = 0; i < list.size();) {
                std::size_t e = list.find(';', i);
                if (e == std::string::npos) e = list.size();
                const std::string item = list.substr(i, e - i);
                const std::size_t q = item.find('=');
                if (q != std::string::npos) { m[item.substr(0, q)] = item.substr(q + 1); mass |= item.substr(0, q) == "mass"; preset |= item.substr(0, q) == "preset"; disc |= item.substr(0, q) == "disc"; }
                i = e + 1;
            }
            if (preset && !mass) m.erase("mass");
            if (preset && !disc) m.erase("disc");                       // (0.55.1: the preset sets the disc unless it is given)
            app.blackhole = blackhole::loadSettings(m);
        }
    }
    if (const char* l = SDL_getenv("PL_BH_LEARN")) app.bhLearn = std::atoi(l);   // 0.55.0: open the Black Hole Lab guide (1)
    if (const char* a = SDL_getenv("PL_ATOM_SET")) {
        std::string list = a;
        std::size_t i = 0;
        while (i < list.size()) {
            std::size_t e = list.find(';', i);
            if (e == std::string::npos) e = list.size();
            const std::string item = list.substr(i, e - i), key = item.substr(0, item.find('=')), val = item.substr(item.find('=') + 1);
            char* end = nullptr;
            const double v = std::strtod(val.c_str(), &end);
            if (end && *end == 0 && !val.empty()) app.atom.setParam(key, v); else app.atom.setParam(key, val);
            i = e + 1;
        }
    }
    if (const char* f = SDL_getenv("PL_ATOM_FOCUS")) app.atom.s.focus = std::atoi(f);
    if (const char* v = SDL_getenv("PL_ATOM_VALENCE")) {   // 0.47.2, for checks and pictures: "n,l,2J" puts the outer electron of Li..Fr in that level (e.g. "3,1,1": sodium 3p 2P1/2)
        int n = 0, l = 0, j2 = 0;
        if (std::sscanf(v, "%d,%d,%d", &n, &l, &j2) == 3) { app.atom.setValence(n, l); app.atom.s.valJ2 = j2; }
    }
    if (const char* v = SDL_getenv("PL_ATOM_LEVEL")) {     // 0.47.3, for checks and pictures: a level atom's level (an index into atom::jumpLevels); "r" after it: the round view
        app.atom.s.lvl = std::atoi(v); if (std::strchr(v, 'r')) app.atom.s.round = true;
    }
    if (const char* g = SDL_getenv("PL_GPU_CLOUD")) app.atom.gpuCloud = std::string(g) != "off";   // 0.46: for automatic checks: the CPU cloud path, to compare with the GPU one
    const char* atomDo = SDL_getenv("PL_ATOM_DO");
    const int atomDoFrame = SDL_getenv("PL_ATOM_DO_FRAME") ? std::atoi(SDL_getenv("PL_ATOM_DO_FRAME")) : 0;   // PL_ATOM_DO_FRAME=n: press it at frame n (after the cloud has filled), not at the start
    const auto doAtom = [&] { if (!atomDo) return; const std::string a = atomDo; if (a == "emit") app.atom.emit(); else if (a == "absorb") app.atom.absorb(); else if (a == "auto") app.atom.toggleAuto();
                                               else if (a == "nucleus") app.atom.zoomNucleus(); else if (a == "nucleon") app.atom.zoomNucleon(); };
    if (atomDoFrame == 0) doAtom();
    if (const char* l = SDL_getenv("PL_MOL_LEARN")) app.molLearn = std::atoi(l);     // 0.25: open the Molecule Lab guide (1)
    if (const char* l = SDL_getenv("PL_ATOM_LEARN")) { app.atomLearn = std::atoi(l); if (const char* t = SDL_getenv("PL_ATOM_TAB")) { app.scaleTab = t; app.massTab = t; } }   // open an Atom Lab guide (1–4), on a tab
    if (const char* j = SDL_getenv("PL_ATOM_JUMP")) app.atomLearnJump = j;   // 0.19: ... scrolled to a section ("scale": the "monochrome" word)
    int frame = 0;
    ui::benchmarkUse(window, useVulkan ? &vk : nullptr);   // 0.63.0 (Vulkan step 5): the benchmark runs on the API this start draws with
    int vkFirst = 0;                                    // 0.59.0: Vulkan's first frame: 0 not begun, 1 begun (guarded), 2 shown, 3 a call failed (kept)
    const bool pictureOff = SDL_getenv("PL_NO_PICTURE") != nullptr;   // 0.59.0: no lab picture (PL_NO_PICTURE, for the checks that compare OpenGL and Vulkan; 0.60.0: Vulkan draws the pictures)
    bool running = true;

    LiveWatch live;
    int normalFrames = 0;
    double maxPhysicsDt = 0;                            // the biggest time step any frame gave the simulation (PL_LIVE_REPORT prints it)
    double prevWorkMs = 0;                              // 0.63.1: the last frame's own work (ms), for the benchmark's worst frames
    const double fakeModalS = SDL_getenv("PL_FAKE_MODAL") ? std::atof(SDL_getenv("PL_FAKE_MODAL")) : 0;   // for automatic checks: pretend the window system holds the loop for n seconds
    const bool fakeResize = SDL_getenv("PL_FAKE_MODAL_RESIZE") != nullptr;
    // One frame. live: drawn from the event watch while the window system holds the loop (no events are read then).
    int atomDoCount = 0;
    const double atomSpin = SDL_getenv("PL_ATOM_SPIN") ? std::atof(SDL_getenv("PL_ATOM_SPIN")) : 0;
    const int atomBench = SDL_getenv("PL_ATOM_BENCHSCENE") ? std::atoi(SDL_getenv("PL_ATOM_BENCHSCENE")) : -1;
    int atomBenchFrame = 0;
    const int atomBenchStop = SDL_getenv("PL_ATOM_BENCHSTOP") ? std::atoi(SDL_getenv("PL_ATOM_BENCHSTOP")) : 0;
    const auto runFrame = [&](bool isLive) {
        // 0.67.0: a helper finished: its guard's record kept at once (its pipelines reach the app's cache at the next start: render/vk_blackhole.cpp merges
        // the helpers' cache file before any pipeline is made; merging now would race the Black Hole Lab's pipelines made on its threads)
        if (useVulkan && app::gpuHelperPump()) ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);
        app.liveFrame = isLive;
        if (atomDoFrame > 0 && ++atomDoCount == atomDoFrame) doAtom();
        if (atomSpin != 0) app.atom.s.oyaw += atomSpin;   // PL_ATOM_SPIN=0.15: the camera turns every frame as a fast drag does (0.16 stage 5, automatic checks)
        if (atomBench >= 0 && app.canvasW > 1 && (atomBenchStop <= 0 || atomBenchFrame < atomBenchStop)) {   // (PL_ATOM_BENCHSTOP=n: still from frame n on)          // 0.61.0: PL_ATOM_BENCHSCENE=n (checks, measuring): the benchmark's Atom scene n set up and moved as the benchmark does
            if (atomBenchFrame == 0) atom::benchSetup(app.atom, atomBench, app.canvasW, app.canvasH);
            app.atom.hold = true;                       // (as the benchmark: the decay clock held)
            atom::benchFrame(app.atom, atomBench, atomBenchFrame, atomBenchFrame);
            if (atomBench >= atom::kBenchAllEnlarged && atomBench <= atom::kBenchJumpAll) app.atom.s.yaw = 0.8 * 0.016 * atomBenchFrame;   // turning as the benchmark's (its 60 frames a second)
            atomBenchFrame++;
        }
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        float canvasWinW = 1, headerH = 0, pictureWinH = 1;
        const auto measure = [&] {                      // the window as it is now (it can change while the window system has the loop)
            SDL_GetWindowSize(window, &winW, &winH);
            SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
            canvasWinW = std::max(1.0f, float(winW));   // the picture's width in window coordinates
            pictureWinH = std::max(1.0f, float(winH));   // the picture fills the window in both labs; the bars float over it
        };
        measure();

        // ---- input ----
        if (!isLive && fakeModalS > 0 && ++normalFrames == 60) {   // automatic check: what the window system does during a drag, from inside the event loop
            const Uint64 t0 = SDL_GetTicks();
            int w0 = 0, h0 = 0, i = 0;
            SDL_GetWindowSize(window, &w0, &h0);
            while ((SDL_GetTicks() - t0) / 1000.0 < fakeModalS) {
                SDL_Delay(10);
                if (fakeResize) { i++; SDL_SetWindowSize(window, w0 + (i % 40) * 8, h0 + (i % 40) * 4); }   // a live resize
                SDL_Event ex; SDL_zero(ex);
                ex.type = SDL_EVENT_WINDOW_EXPOSED; ex.window.windowID = SDL_GetWindowID(window); ex.window.data1 = 1;
                SDL_PushEvent(&ex);                     // the event watchers see it at once, like SDL's own timer in the window system's loop
            }
        }
        if (!isLive) { static int scriptFrame = 0; ui::runScript(app, scriptFrame++, window); }   // PL_SCRIPT: automatic checks press things through the real input path
        SDL_Event e;
        const double ev0 = app::frameCostNowMs();         // 0.63.1: the events' time (the benchmark's worst frames)
        while (!isLive && SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_MOUSE_WHEEL) ui::inputNoteWheel(e.wheel.y, e.wheel.mouse_x, e.wheel.mouse_y);
            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) ui::inputNoteKey(static_cast<int>(e.key.key), e.type == SDL_EVENT_KEY_DOWN);
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F12 && !e.key.repeat && (e.key.mod & SDL_KMOD_CTRL) && (e.key.mod & SDL_KMOD_SHIFT)) {   // 5k: the hidden input log
                const std::string path = ui::inputLogWrite(app);
                app.shot.note = path.empty() ? "The input log could not be written." : "Input log written to " + path;
                app.shot.failed = path.empty();
                app.shot.noteUntil = ImGui::GetTime() + 30.0;
            }
            switch (e.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if (e.window.windowID == SDL_GetWindowID(window)) running = false; break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:           // the window moved to another monitor, or the monitor's scale changed (Windows' scale setting)
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED: rereadScale(); break;
            case SDL_EVENT_KEY_DOWN:
                if (!app::labInputAllowed(app.benchRunning)) break;      // the benchmark is running: only Cancel and Esc do anything
                if (e.key.key == SDLK_F11 && !e.key.repeat) SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));   // F11: full screen on / off
                if (!io.WantCaptureKeyboard && e.key.key == SDLK_SPACE) { ui::shellSpace(app, io.WantTextInput); }
                if (!io.WantCaptureKeyboard && !io.WantTextInput && !ui::readingShown() && !app.pane && !(e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) {   // 0.22: 1 2 3 open a lab; + and − zoom; R or Home resets the view
                    const SDL_Keycode k = e.key.key;
                    if (k == SDLK_1 || k == SDLK_2 || k == SDLK_3 || k == SDLK_4 || k == SDLK_KP_1 || k == SDLK_KP_2 || k == SDLK_KP_3 || k == SDLK_KP_4) {   // (0.55.0: 4, the Black Hole Lab)
                        const int lab = (k == SDLK_1 || k == SDLK_KP_1) ? 0 : (k == SDLK_2 || k == SDLK_KP_2) ? 1 : (k == SDLK_3 || k == SDLK_KP_3) ? molecule::kLabIndex : blackhole::kLabIndex;
                        if (app.journey.active && lab != app.lab) ui::journeyStop(app);
                        ui::openLab(app, lab);
                    }
                    const int dz = (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) ? 1 : (k == SDLK_MINUS || k == SDLK_KP_MINUS) ? -1 : 0;
                    const bool reset = (k == SDLK_R || k == SDLK_HOME) && !(app.lab == 1 && app.atom.chartOn() && k == SDLK_HOME);
                    if (app.lab == molecule::kLabIndex) {
                        if (dz) app.molecule.wheel(dz);           // one wheel notch (15 %)
                        if (reset) app.molecule.resetView();
                    } else if (app.lab == blackhole::kLabIndex) {   // 0.55.0: + closer, − farther (15 % of the distance a step, as a wheel notch); R / Home the first view
                        if (dz) blackhole::setDistance(app.blackhole, app.blackhole.dist * std::pow(1.15, -dz));
                        if (reset) blackhole::resetView(app.blackhole);
                    } else if (app.lab == 1) {
                        if (dz && app.atom.chartOn()) app.atom.chartZoomAt(std::pow(1.15, dz), (app.canvasW - app.set.insetRight) / 2, app.canvasH / 2);   // about the middle of the free picture
                        else if (dz) app.atom.setParam("zoom", app.atom.p.zoom + dz * 0.4);   // one wheel notch
                        if (reset) atom::pressTool(app.atom, atom::Tool::ResetView);
                    } else {
                        if ((dz || reset) && app.journey.active) ui::journeyStop(app);
                        if (dz) ui::setZoom(app, app.set.zoom + dz * 0.14);   // two wheel notches
                        if (reset) ui::resetViewAction(app);
                    }
                }
                if (app.lab == 1 && app.atom.chartOn() && !io.WantCaptureKeyboard && !ui::readingShown() && !ui::sh::sliderHoveredRecently()) {   // (0.50.3 outside review: a hovered slider takes the arrows; both moved)   // (5k: with a page open the arrow keys scroll the page)   // the chart of nuclides: arrow keys walk one proton or one neutron (up = more protons, right = more neutrons)
                    const int dZ = e.key.key == SDLK_UP ? 1 : e.key.key == SDLK_DOWN ? -1 : 0, dN = e.key.key == SDLK_RIGHT ? 1 : e.key.key == SDLK_LEFT ? -1 : 0;
                    if (dZ || dN) app.atom.chartStep(dZ, dN);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (!app::labInputAllowed(app.benchRunning)) break;
                if (app.lab == molecule::kLabIndex) {   // Molecule Lab: 15 % a notch (design)
                    if (!io.WantCaptureMouse && !ui::readingShown() && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH) app.molecule.wheel(e.wheel.y);
                    break;
                }
                if (app.lab == blackhole::kLabIndex) {  // 0.55.0: the wheel moves the camera closer (up) or farther (down), 15 % of the distance a notch
                    if (!io.WantCaptureMouse && !ui::readingShown() && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH) blackhole::setDistance(app.blackhole, app.blackhole.dist * std::pow(1.15, -e.wheel.y));
                    break;
                }
                if (app.lab == 1) {                     // Atom Lab: zoom −= deltaY × 0.004, 0.4 per wheel notch
                    if (!io.WantCaptureMouse && !ui::readingShown() && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH && e.wheel.mouse_y < headerH + pictureWinH) {   // (5k: with a page open the wheel scrolls the page, never zooms the picture behind it)
                        if (app.atom.chartOn()) app.atom.chartZoomAt(std::pow(1.15, e.wheel.y), e.wheel.mouse_x / scale, (e.wheel.mouse_y - headerH) / scale);   // the chart of nuclides: zoom about the pointer
                        else app.atom.setParam("zoom", app.atom.p.zoom + e.wheel.y * 0.4);
                    }
                    break;
                }
                if (!io.WantCaptureMouse && !ui::readingShown() && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH) {
                    if (app.journey.active) ui::journeyStop(app);   // touching the picture stops a Journey (and brings your settings back)
                    else ui::setZoom(app, app.set.zoom + e.wheel.y * 0.07);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!app::labInputAllowed(app.benchRunning)) break;
                if (app.lab >= 1) {                     // (Atom and Molecule Lab: a press on the picture starts a drag or a click)
                    if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && e.button.y < headerH + pictureWinH) {
                        mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y;
                    }
                    break;
                }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && app.journey.active) { ui::journeyStop(app); break; }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseDown && app::labInputAllowed(app.benchRunning)) {
                    if (!dragging && std::hypot(e.motion.x - downX, e.motion.y - downY) > 4 * scale) { dragging = true; ui::shellNoteDrag(); }
                    // 3D: dragging turns the view, unless "Drag moves the view" is on, or with the right button or Shift held
                    const bool rotate = app.set.view3D && !app.view.panMode && !rightDrag && !(SDL_GetModState() & SDL_KMOD_SHIFT);
                    if (dragging && app.lab == blackhole::kLabIndex) {   // 0.55.0: drag turns you round the hole: the sky follows the pointer at the picture's own scale
                        const double K = (app.canvasH / 2) / (app.blackhole.fovDeg * 3.14159265358979 / 360);
                        blackhole::turn(app.blackhole, (e.motion.x - lastX) / scale / K / std::max(0.2, std::sin(app.blackhole.pitch)), -(e.motion.y - lastY) / scale / K);   // (the sky moves with the pointer near the middle)
                    }
                    else if (dragging && app.lab == molecule::kLabIndex) app.molecule.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rightDrag || (SDL_GetModState() & SDL_KMOD_SHIFT));   // 0.18: turn; Shift / right / middle: move
                    else if (dragging && app.lab == 1) app.atom.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rightDrag || (SDL_GetModState() & SDL_KMOD_SHIFT));
                    else if (dragging) app.view.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rotate);
                    lastX = e.motion.x; lastY = e.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseDown && !dragging && app::labInputAllowed(app.benchRunning) && e.button.button == SDL_BUTTON_LEFT) {
                    if (app.lab == blackhole::kLabIndex) {}       // 0.55.0: nothing to pick
                    else if (app.lab == molecule::kLabIndex) app.molecule.click(e.button.x / scale, (e.button.y - headerH) / scale);
                    else if (app.lab == 1) {
                        const atom::MarksDrawn& md = atom::marksDrawn();   // 0.20: a click on the folded info text opens it (and folds it again)
                        const double px = e.button.x / scale, py = (e.button.y - headerH) / scale;
                        if (md.infoFoldable && md.info[2] > 0 && px >= md.info[0] && px <= md.info[0] + md.info[2] && py >= md.info[1] && py <= md.info[1] + md.info[3]) app.atom.infoOpen = !app.atom.infoOpen;
                        else app.atom.click(px, py);
                    }
                    else app.view.click(e.button.x / scale, (e.button.y - headerH) / scale);
                }
                mouseDown = dragging = false;
                break;
            default: break;
            }
        }
        app::frameCostAdd(app::FrameCostKind::Events, app::frameCostNowMs() - ev0);
        measure();                                      // a drag may have changed the window while the events were read
        if (!isLive) {   // remember the window: its size and place when it is a normal window, and whether it is maximised
            const SDL_WindowFlags fl = SDL_GetWindowFlags(window);
            if (fl & SDL_WINDOW_MAXIMIZED) maximizeUntil = 0;
            if (!(fl & SDL_WINDOW_MINIMIZED) && SDL_GetTicks() >= maximizeUntil) {
                app::WindowState ws = app.prefs.window ? *app.prefs.window : app::WindowState{};
                if (!(fl & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))) {
                    SDL_GetWindowSize(window, &ws.w, &ws.h);
                    SDL_GetWindowPosition(window, &ws.x, &ws.y);
                    ws.hasPos = true;
                    ws.maximized = false;
                } else if (!(fl & SDL_WINDOW_FULLSCREEN)) ws.maximized = true;
                if (ws.w > 0) app.prefs.window = ws;
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(20); last = SDL_GetPerformanceCounter(); return; }
        struct Busy { bool& f; explicit Busy(bool& x) : f(x) { f = true; } ~Busy() { f = false; } } busyGuard(live.busy);

        orbit::linesFrameEnd(renderer); molecule::surfaceFrameEnd(renderer);   // 0.68.0
        ui::benchmarkTestHook(app);
        if (frame == 0 && !useVulkan) if (const char* bc = SDL_getenv("PL_BH_COMPARE")) blackhole::compareHook(renderer, bc);   // 0.55.0: the Black Hole Lab's card against its reference (tests)
        if (const char* at = SDL_getenv("PL_SAVE_IMAGE")) { static int n = 0; if (++n == std::atoi(at)) app.shot.requested = true; }   // for automatic checks: press 📷 Save image at frame n
        { app::FrameCostSpan step(app::FrameCostKind::BenchStep, false);   // (0.63.1: before the frame's clock)
          ui::benchmarkBeginFrame(app, renderer, fbW, fbH); }   // a benchmark scene is set up here, before the frame
        app.sys.setRelativity(app.set.relativity);          // Real: General relativity (1PN) on / off (does nothing when it has not changed)
        app.sys.setRealPlanets(app.set.realPlanets);        // Planet physics: Simple / Real (does nothing when it has not changed)
        // ---- time and physics ----
        const Uint64 now = SDL_GetPerformanceCounter();   // the frame starts here
        static const double fixedDt = SDL_getenv("PL_FIXED_DT") ? std::atof(SDL_getenv("PL_FIXED_DT")) : 0.0;   // for automatic checks: every frame lasts exactly this many seconds, so two runs draw the same pictures
        const double frameDt = fixedDt > 0 ? fixedDt : static_cast<double>(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        // 0.63.1: where the last whole frame's time went (from its start to this one's: its work, its waits for the card, its present), for the benchmark
        app::BenchFrameCost lastCost;
        {   const app::FrameCost fc = app::frameCostTake();
            using K = app::FrameCostKind;
            lastCost.frameMs = frameDt * 1000.0; lastCost.workMs = prevWorkMs;
            lastCost.cardWaitMs = fc.get(K::CardWait); lastCost.acquireMs = fc.get(K::Acquire); lastCost.presentMs = fc.get(K::Present);
            lastCost.pipelineMs = fc.get(K::Pipeline); lastCost.pipelines = fc.count(K::Pipeline); lastCost.readbackMs = fc.get(K::Readback); lastCost.readbacks = fc.count(K::Readback);
            lastCost.swapchainMs = fc.get(K::Swapchain); lastCost.swapchains = fc.count(K::Swapchain); lastCost.workerWaitMs = fc.get(K::WorkerWait); lastCost.workerWaits = fc.count(K::WorkerWait);
            lastCost.deferred = fc.count(K::GuardDeferred); lastCost.beforeMs = fc.get(K::BenchStep);
            lastCost.eventsMs = fc.get(K::Events); lastCost.bookMs = fc.get(K::Bookkeeping); lastCost.settingsMs = fc.get(K::Settings);
            if (const char* w = fc.what[static_cast<int>(K::Readback)]) lastCost.readbackWhat = w; }
        static double fixedClockMs = 0;                  // with PL_FIXED_DT the Atom Lab's own clock (glides, messages, decay) follows the frames too
        if (fixedDt > 0) { fixedClockMs += fixedDt * 1000; app.atom.nowMs = [] { return fixedClockMs; }; }
        const double dt = app::stepSeconds(frameDt, units::kMaxFrameDt);   // a slow frame counts as at most 0.05 s, a stall (window held by the system) as nothing: no jump, no catching up
        maxPhysicsDt = std::max(maxPhysicsDt, dt);
        sectMark(5);                                    // (PL_SECTSTAT: the frame's parts from here)
        if (app.pane) {                                 // the Learn tab is open: the lab waits
        } else if (app.lab == blackhole::kLabIndex) {   // 0.55.0: nothing moves by itself (the camera hovers)
        } else if (app.lab == molecule::kLabIndex) {
            app.molecule.update(frameDt, ui::look().reduced, app.pane != 0);   // (0.23: not app.paused: this lab has no clock, and a pause made in another lab stopped its spin with no way to start it here; 0.56.1: not while the benchmark runs either: its molecule scenes play a vibration)
        } else if (app.lab == 1) {                      // Atom Lab: tick every frame (decay clock, glides), fixed steps while running
            app.atom.paused = app.paused;
            app.atom.dotBudgetMs = fixedDt > 0 ? 0.0 : atom::AtomLab::kDotBudgetMs;   // 0.66.0: the Slice's new dots on a time budget a frame (owed by the steps, made after them); PL_FIXED_DT (checks that compare two runs): made at once, as before
            app.atom.tick(dt);
            if (!app.paused) {
                acc += dt;
                while (acc >= units::kPhysicsStep) { app.atom.step(units::kPhysicsStep); acc -= units::kPhysicsStep; }
            }
            app.atom.makeOwedDots();
            app.atom.dotBudgetMs = 0;
        } else {
            app.view.tick(dt, app.set);
            ui::journeyTick(app, dt);
            ui::jumpTick(app);                          // 0.67.0: a far jump in Real, once its walk on a worker is taken in
            if (!app.paused) {
                acc += dt;
                while (acc >= units::kPhysicsStep) { app.sys.step(units::kPhysicsStep, app.set.tspeed); acc -= units::kPhysicsStep; }
            }
        }
        fpsFrames++; fpsTime += frameDt;
        sectMark(0);
        if (fpsTime >= 0.5) { fps = fpsFrames / fpsTime; fpsFrames = 0; fpsTime = 0; }

        // ---- build the frame ----
        ui::updateLook(app.prefs, systemLight(), ui::systemReducedMotion());   // theme, text size, font, motion
        ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, app.benchRunning ? 0.0f : 0.45f);   // the benchmark's input lock is a modal that must not dim the picture
        if (static int shownLab = -1; shownLab != app.lab) {   // the window title follows the lab on screen
            shownLab = app.lab;
            SDL_SetWindowTitle(window, (std::string("Physics Lab ") + ui::appVersion() + " — " + app::labName(app.lab)).c_str());
        }
        app.atom.reducedMotion = ui::look().reduced;
        if (useVulkan) vk.newFrame();
        else ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        {   // for automatic checks: PL_MOUSE=x,y puts the pointer there every frame (a hovered button, a tooltip); a mouse that is really used is not affected
            static const char* mouse = SDL_getenv("PL_MOUSE");
            float mx = 0, my = 0;
            if (mouse && std::sscanf(mouse, "%f,%f", &mx, &my) == 2) ImGui::GetIO().AddMousePosEvent(mx, my);
        }
        ImGui::NewFrame();
        app.atom.shellUi = true;                        // the picture's own "More / Less info" button is left out (the subject card has it)
        ui::shellFrame(app, winW, winH, app.prefs.fps == "on" ? fps : -1, frameDt);  // bars and panels over the full-window picture, in both labs
        sectMark(1);
        headerH = 0.0f;
        pictureWinH = std::max(1.0f, float(winH));
        app.atom.insetTop = ui::shellTopBarH() / scale + 4; app.atom.insetBottom = app.set.reserveBottom; app.atom.insetRight = app.set.insetRight; app.atom.reserveRight = app.set.reserveRight;   // (points; after shellFrame, so the first frame already has the bars' size)
        app.canvasW = canvasWinW / scale;               // picture size in points (like CSS pixels)
        app.canvasH = pictureWinH / scale;
        app.set.lightTheme = ui::look().c.light;
        app.set.theme = ui::look().c;
        app.set.textScale = ui::look().textScale;
        if (app.pane) ui::paneWindow(app, ui::shellTopBarH());
        bool orbitKept = false;                         // 0.67.0: the Orbit Lab at rest keeps its picture (OrbitKeep above)
        std::uint64_t orbitKey = 0;
        if (!pictureOff && !app.pane && app.lab == 0) {
            // (the checks' shell log, PL_LOG_SHELL, reads what each drawing notes: kept only when a check asks for it, PL_ORBIT_KEEP=1)
            static const bool noKeep = SDL_getenv("PL_ORBIT_NOKEEP") != nullptr || (ui::shellLogOn() && !SDL_getenv("PL_ORBIT_KEEP"));
            app.set.paused = app.paused;
            orbitKey = orbitFrameKey(app, renderer, headerH, scale, ui::look().grayscale);
            const OrbitKeep& K = g_orbitKeep;
            orbitKept = !noKeep && K.have && K.textOk && K.key == orbitKey && frame > 0;
        } else g_orbitKeep.have = false;
        if (!pictureOff) { if (orbitKept) renderer.keepLast(); else renderer.begin(app.canvasW, app.canvasH); }
        ImDrawList* text = ImGui::GetBackgroundDrawList();
        text->PushClipRect(ImVec2(0, headerH), ImVec2(canvasWinW, headerH + pictureWinH));
        if (app.pane) text->PopClipRect();              // a tab page covers the lab: nothing to draw
        else if (pictureOff) {                          // PL_NO_PICTURE (0.62.0: the Black Hole Lab is drawn on Vulkan too)
            pictureNote(text, ImVec2(0, headerH), ImVec2(canvasWinW, pictureWinH), ui::look().c.muted, "Test: the lab's picture is left out (PL_NO_PICTURE)");
            text->PopClipRect();
        }
        else if (app.lab == molecule::kLabIndex) {
            app.moleculeView.draw(renderer, text, scale, app.molecule, app.canvasW, app.canvasH, ui::look().c);
            text->PopClipRect();
        } else if (app.lab == blackhole::kLabIndex) {   // 0.55.0: the hole in the middle of the free part of the picture
            const double cx = app.bhFreeW > 0 ? app.bhFreeX + app.bhFreeW / 2 : app.canvasW / 2, cy = app.bhFreeH > 0 ? app.bhFreeY + app.bhFreeH / 2 : app.canvasH / 2;
            static const char* bhMove = SDL_getenv("PL_BH_MOVE");
            if (bhMove) bhMoveStep(app.blackhole, bhMove, frame);
            const auto tb0 = std::chrono::steady_clock::now();
            app.blackholeView.draw(renderer, blackhole::shown(app.blackhole), app.canvasW, app.canvasH, cx, cy);
            g_bhDrawMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tb0).count();
            text->PopClipRect();
        } else if (app.lab == 1) {
            const auto tv0 = std::chrono::steady_clock::now();
            app.atomView.draw(renderer, text, 0, headerH, scale, app.atom, app.canvasW, app.canvasH, ui::look().c, ui::look().textScale);
            g_viewMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tv0).count();   // (PL_TIMESTAT: the processor's time to build the Atom Lab picture, no GL)
            text->PopClipRect();
        } else {
            app.set.paused = app.paused;
            if (SDL_getenv("PL_SKYOFF") && frame == 0) app.set.sky = orbit::SkyMode::Off;
            OrbitKeep& K = g_orbitKeep;
            if (orbitKept) {                            // the kept labels (the same vertices, after this frame's own)
                K.kept++;
                const int nv = int(K.vtx.size()), ni = int(K.idx.size());
                if (ni > 0) {
                    text->PrimReserve(ni, nv);
                    std::memcpy(text->_VtxWritePtr, K.vtx.data(), K.vtx.size() * sizeof(ImDrawVert));
                    const unsigned base = text->_VtxCurrentIdx;
                    for (int i = 0; i < ni; i++) text->_IdxWritePtr[i] = static_cast<ImDrawIdx>(base + K.idx[std::size_t(i)]);
                    text->_VtxWritePtr += nv; text->_IdxWritePtr += ni; text->_VtxCurrentIdx += unsigned(nv);
                }
            } else {
                const int v0 = text->VtxBuffer.Size, i0 = text->IdxBuffer.Size, c0 = std::max(0, text->CmdBuffer.Size - 1);
                const unsigned base = text->_VtxCurrentIdx;
                const ImDrawCmdHeader hdr = text->_CmdHeader;
                app.view.draw(renderer, text, 0, headerH, scale, app.sys, app.set, app.canvasW, app.canvasH);
                K.drawn++; app.orbitPictures++;
                K.have = true; K.key = orbitKey;
                // the labels are kept when they are one run of vertices with one clip and texture (the picture's own, as here), under 60,000 vertices
                K.textOk = text->_CmdHeader.VtxOffset == hdr.VtxOffset && text->VtxBuffer.Size - v0 < 60000;
                for (int c = c0; c < text->CmdBuffer.Size && K.textOk; c++) {
                    const ImDrawCmd& cmd = text->CmdBuffer[c];
                    if (cmd.ElemCount == 0) continue;
                    K.textOk = std::memcmp(&cmd.ClipRect, &hdr.ClipRect, sizeof hdr.ClipRect) == 0 && std::memcmp(&cmd.TexRef, &hdr.TexRef, sizeof hdr.TexRef) == 0 &&
                               cmd.VtxOffset == hdr.VtxOffset && cmd.UserCallback == nullptr;
                }
                if (K.textOk) {
                    K.vtx.assign(text->VtxBuffer.Data + v0, text->VtxBuffer.Data + text->VtxBuffer.Size);
                    K.idx.resize(std::size_t(text->IdxBuffer.Size - i0));
                    for (int i = i0; i < text->IdxBuffer.Size; i++) K.idx[std::size_t(i - i0)] = static_cast<ImDrawIdx>(text->IdxBuffer[i] - base);
                }
            }
            text->PopClipRect();
        }
        sectMark(2);
        ui::aboutWindow(app);
        ui::historyWindow(app);
        if (app.lab == 1) ui::atomLearnWindow(app);
        else if (app.lab == molecule::kLabIndex) ui::moleculeLearnWindow(app);
        else if (app.lab == blackhole::kLabIndex) ui::blackholeLearnWindow(app);
        else ui::learnWindow(app);
        ui::settingsWindow(app);
        ui::benchmarkWindow(app);                       // the benchmark: what it will do, the progress, the result
        ui::appTabsFrame(app);                          // a guide opened from Learn goes back to its lab
        if (ui::shellLogOn()) {                         // 5k: the windows from back to front (the test of the handle's place)
            std::string order = "worder";
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) if (w->WasActive && !(w->Flags & ImGuiWindowFlags_ChildWindow)) order += std::string(" ") + w->Name;
            ui::auditNote(order);
        }
        ui::readingFrameEnd();                          // 5k: the open page scrolls by the wheel and keys whichever window Dear ImGui calls hovered or focused
        ImGui::Render();

        // ---- draw: the picture first, then the panels and text on top ----
        const app::Rgb pageBg = ui::look().c.bg;           // the theme's page colour
        const int canvasFbW = static_cast<int>(std::lround(canvasWinW * fbW / std::max(1, winW)));
        const int pictureFbH = static_cast<int>(std::lround(pictureWinH * fbH / std::max(1, winH)));
        const int headerFbH = static_cast<int>(std::lround(headerH * fbH / std::max(1, winH)));
        sectMark(3);
        const auto te0 = std::chrono::steady_clock::now();   // (0.63.0: before either API's drawing: the diagnostics below print on both)
        const long glareMade0 = renderer.glareStats().made;
        if (useVulkan) {                                // 0.59.0: cleared to the page colour, the picture area filled with it (step 2: the labs' pictures), the panels on top
            const float bgc[4] = {pageBg.r, pageBg.g, pageBg.b, 1.0f};
            if (vkFirst == 0 && app::gpuVulkanGuard().hold()) { app::gpuVulkanGuard().begin("first frame"); vkFirst = 1; }   // (0.63.0: its turn first)
            const bool drawPicture = !pictureOff && !app.pane;   // 0.60.0: the lab's picture (renderer2d on Vulkan; 0.62.0: the Black Hole Lab's too)
            if (drawPicture) renderer.setGrayscale(ui::look().grayscale, {pageBg.r, pageBg.g, pageBg.b, 1});
            ui::benchmarkGpuBegin(app);                 // 0.63.0: the GPU time of this frame on Vulkan (timestamps; only while the benchmark runs)
            vk.render(ImGui::GetDrawData(), bgc, bgc, 0, headerFbH, canvasFbW, pictureFbH, drawPicture ? &renderer : nullptr);
            ui::benchmarkGpuEnd(app);
            if (drawPicture) { app.drawCalls = renderer.lastDrawCalls(); app.drawInstances = renderer.lastInstances(); }
        } else {
            ui::benchmarkGpuBegin(app);                     // the GPU time of this frame (only while the benchmark runs)
            plgl::glViewport(0, 0, fbW, fbH);
            plgl::glClearColor(pageBg.r, pageBg.g, pageBg.b, 1);
            plgl::glClear(GL_COLOR_BUFFER_BIT);
            renderer.setGrayscale(ui::look().grayscale, {pageBg.r, pageBg.g, pageBg.b, 1});
            renderer.end(canvasFbW, pictureFbH, fbH - headerFbH - pictureFbH);
            app.drawCalls = renderer.lastDrawCalls(); app.drawInstances = renderer.lastInstances();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            ui::benchmarkGpuEnd(app);
        }
        sectMark(4);
        {   // the checks' diagnostics (PL_BH_FRAMETIME, PL_TIMESTAT, PL_DRAWSTAT ...): 0.63.0: printed on Vulkan too (they were in the OpenGL branch)
        static const bool bhTime = SDL_getenv("PL_BH_FRAMETIME") != nullptr;
        if (bhTime && app.lab == blackhole::kLabIndex && !app.pane) {
            const render::Renderer2D::GlareStats& g = renderer.glareStats();
            const double glareMs = g.frameMs, glareVeil = g.made != glareMade0 ? g.veilMs : 0, glareWait = g.made != glareMade0 ? g.waitMs : 0;
            const double endMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - te0).count();
            const blackhole::ViewStats& bs = app.blackholeView.stats();
            const blackhole::Settings& sk = app.blackholeView.shownSettings();
            const blackhole::Settings wk = blackhole::shown(app.blackhole);
            std::printf("PLBHFRAME f=%d draw=%.3f glare=%.3f glareveil=%.3f glarewait=%.3f end=%.3f redraws=%ld coarse=%ld fine=%ld stars=%ld behind=%d coarsems=%.3f binms=%.3f starms=%.3f lookms=%.3f finems=%.3f discms=%.3f pic=%llu sky=%.6f,%.6f,%.6f,%.6f made=%.6f,%.6f,%.6f,%.6f behindms=%.1f want=%.6f,%.6f,%.6f,%.6f card=%d cardstate=%d cardfills=%ld cardms=%.1f finethread=%d skycard=%d skystate=%d skyms=%.3f skyscale=%.3f skypics=%ld starpics=%ld lookcard=%d white=%.6g\n", frame, g_bhDrawMs, glareMs, glareVeil, glareWait, endMs, renderer.lensRedraws(),
                        bs.spinCoarseFills + bs.tableFills, bs.spinFineFills, bs.starFills, bs.behind ? 1 : 0, blackhole::shown(app.blackhole).spin > 0 ? bs.spinCoarseMs : bs.tableMs, bs.spinStarsMs, bs.starsMs, bs.lookMs, bs.spinFineMs, bs.discMs,
                        static_cast<unsigned long long>(bs.picture), sk.yaw, sk.pitch, sk.dist, sk.spin > 0 && sk.bend ? sk.spin : 0.0, bs.made[0], bs.made[1], bs.made[2], bs.made[3], bs.behindMs,
                        wk.yaw, wk.pitch, wk.dist, wk.spin > 0 && wk.bend ? wk.spin : 0.0, bs.spinCard ? 1 : 0, app.blackholeView.cardState(), bs.cardFills, bs.cardMs, app.blackholeView.fineThreadBusy() ? 1 : 0,
                        bs.skyCard ? 1 : 0, app.blackholeView.skyState(), renderer.skyStats().fullMs, renderer.skyStats().scale, renderer.skyStats().draws, bs.starPictures, bs.lookCard ? 1 : 0, bs.look.white);   // 0.55.6: want, the card; 0.56.0: the card's sky
        }
        static const bool timeStat = SDL_getenv("PL_TIMESTAT") != nullptr;   // for measuring in development (0.46): the Atom Lab picture's build time and the renderer's end(), averaged over 60 frames
        if (timeStat && app.lab == 1) {
            static double sv = 0, se = 0; static int nf = 0;
            sv += g_viewMs; se += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - te0).count();
            if (++nf == 60) { std::printf("PLTIME view=%.3f end=%.3f ms (60 frames)\n", sv / 60, se / 60); sv = se = 0; nf = 0; }
        }
        static const bool drawStat = SDL_getenv("PL_DRAWSTAT") != nullptr;   // for automatic checks (0.16 stage 3): what the picture cost this frame, in counts (not time)
        if (drawStat && app.lab == molecule::kLabIndex) { const molecule::ViewStats& ms = app.moleculeView.stats();
                        std::printf("PLMOLSTAT calls=%d instances=%d atoms=%d sticks=%d blobs=%d labels=%d stubs=%d drawus=%ld mol=%s mode=%s dots=%d tris=%d resorted=%d verts=%ld card=%d\n", renderer.lastDrawCalls(), renderer.lastInstances(), ms.atoms, ms.sticks, ms.blobs, ms.labels, ms.stubs, long(ms.cpuMs * 1000), app.molecule.mol().id.c_str(), ms.mode, ms.dots, ms.triangles, ms.resorted ? 1 : 0, renderer.lastVertices(), ms.cloudCard ? 1 : 0);
                        for (std::size_t i = 0; i < app.moleculeView.projected().size(); i++) { const molecule::Proj& q = app.moleculeView.projected()[i]; std::printf("PLMOLATOM i=%zu x=%.1f y=%.1f r=%.1f\n", i, q.x * scale, q.y * scale, q.r * scale); } }
        if (drawStat && app.lab == blackhole::kLabIndex) { const blackhole::ViewStats& bs = app.blackholeView.stats();   // 0.55.0
                        std::printf("PLBHSTAT redraws=%ld tablefills=%ld starfills=%ld stars=%d images=%d extra=%d capped=%d shadowpx=%.3f cx=%.2f cy=%.2f spinfills=%ld\n", renderer.lensRedraws(), bs.tableFills, bs.starFills, bs.stars, bs.images, bs.extraImages, bs.capped, bs.shadowPx * scale, bs.cx * scale, bs.cy * scale, bs.spinCoarseFills + bs.spinFineFills); }
        if (drawStat && app.lab == blackhole::kLabIndex && blackhole::shown(app.blackhole).spin > 0 && SDL_getenv("PL_BH_SPINEDGE")) {   // 0.55.2: the module's shadow edge on the picture
            const blackhole::ViewStats& bs = app.blackholeView.stats();
            std::vector<double> e;
            app.blackholeView.spinEdgePoints(blackhole::shown(app.blackhole), app.canvasW, app.canvasH, bs.cx, bs.cy, 24, e);
            std::printf("PLBHSPINEDGE fine=%d pts=", bs.spinFine ? 1 : 0);
            for (std::size_t q = 0; q + 3 < e.size(); q += 4) std::printf("%s%.2f,%.2f,%.5f,%.5f", q ? ";" : "", e[q] * scale, e[q + 1] * scale, e[q + 2], e[q + 3]);
            std::printf("\n");
        }
        if (drawStat && app.lab == blackhole::kLabIndex && SDL_getenv("PL_BH_DISCPTS")) {   // 0.55.1: where the module puts the disc (taken as on), in window pixels
            const blackhole::ViewStats& bs = app.blackholeView.stats();
            std::vector<double> on, off;
            app.blackholeView.discSamples(blackhole::shown(app.blackhole), app.canvasW, app.canvasH, bs.cx, bs.cy, 400, on, off);
            std::printf("PLBHDISCPT on=");
            for (std::size_t q = 0; q + 1 < on.size(); q += 2) std::printf("%s%.1f,%.1f", q ? ";" : "", on[q] * scale, on[q + 1] * scale);
            std::printf(" off=");
            for (std::size_t q = 0; q + 1 < off.size(); q += 2) std::printf("%s%.1f,%.1f", q ? ";" : "", off[q] * scale, off[q + 1] * scale);
            std::printf(" grid=%d hidden=%d white=%.6g bright=%.6g\n", app.blackholeView.discGridOn(), bs.hidden, bs.look.white, bs.look.brightY);
        }
        if (drawStat) { const atom::NucleusStats& ns = atom::nucleusStats();
                        std::printf("PLDRAWSTAT calls=%d instances=%d vertices=%ld nblobs=%ld nruns=%ld nlines=%ld nrings=%ld nucus=%ld ncover=%ld nlayers=%d nucruns=%ld\n", renderer.lastDrawCalls(), renderer.lastInstances(), renderer.lastVertices(),
                                    ns.blobs, ns.blobRuns, ns.lines, ns.ringCalls, long(ns.cpuMs * 1000), long(ns.cover), ns.layers, renderer.nucDensRuns()); }   // (0.67.0: nucruns, the nucleus density's card runs)
        }
        if (app.shot.requested) {                       // 📷 Save image: the picture area as it is on screen now, readout included
            app.shot.requested = false;
            if (useVulkan) ui::takeShotVulkan(app, vk, !pictureOff && !app.pane ? &renderer : nullptr, {fbW, fbH, winW, winH, canvasFbW, pictureFbH, headerH});
            else ui::takeShot(app, renderer, {fbW, fbH, winW, winH, canvasFbW, pictureFbH, headerH});
        }
        static const double maxSeconds = SDL_getenv("PL_EXIT_AFTER_SECONDS") ? std::atof(SDL_getenv("PL_EXIT_AFTER_SECONDS")) : 0;   // for automatic checks
        static int bhSettleFrom = -1, bhSettleN = 0, bhSettled = 0;          // 0.55.5 (automatic checks): PL_BH_SETTLE="from:n": the run ends once the Black Hole Lab
        if (bhSettleFrom < 0) {                                              // has had nothing to do for n frames after frame from (or at PL_EXIT_AFTER_FRAMES)
            bhSettleFrom = 0;
            if (const char* q = SDL_getenv("PL_BH_SETTLE")) std::sscanf(q, "%d:%d", &bhSettleFrom, &bhSettleN);
        }
        // (0.63.0: and no card program's guarded step open or waiting for its turn: one runs at a time, so at a first start the sky can wait for the light paths)
        bhSettled = bhSettleN > 0 && frame >= bhSettleFrom && app.lab == blackhole::kLabIndex && app.blackholeView.settled() && !renderer.glareBusy() && !app::gpuGuardBusy() ? bhSettled + 1 : 0;
        if ((maxFrames && ++frame >= maxFrames) || (maxSeconds > 0 && SDL_GetTicks() / 1000.0 >= maxSeconds) || (bhSettleN > 0 && bhSettled >= bhSettleN)) {
            running = false;
            if (screenshot && useVulkan) {              // 0.59.0: read back from the swapchain's image (RGBA, top row first: the same bytes as below)
                std::vector<uint8_t> px;
                int sw = 0, sh = 0;
                SDL_Surface* shot = vk.readback(px, sw, sh) ? SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_ABGR8888) : nullptr;
                if (shot) {
                    for (int y = 0; y < sh; y++) std::memcpy(static_cast<uint8_t*>(shot->pixels) + std::size_t(y) * std::size_t(shot->pitch), px.data() + std::size_t(y) * std::size_t(sw) * 4, std::size_t(sw) * 4);
                    SDL_SaveBMP(shot, screenshot);
                    SDL_DestroySurface(shot);
                } else SDL_Log("PL_SCREENSHOT: no picture (Vulkan: the swapchain could not be read)");
            } else if (screenshot) {                    // read the finished frame back and save it
                if (SDL_Surface* shot = fbW > 0 && fbH > 0 ? SDL_CreateSurface(fbW, fbH, SDL_PIXELFORMAT_ABGR8888) : nullptr) {   // 0.50.3 (outside review): no surface, no read
                    plgl::glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, shot->pixels);
                    SDL_FlipSurface(shot, SDL_FLIP_VERTICAL);
                    SDL_SaveBMP(shot, screenshot);
                    SDL_DestroySurface(shot);
                } else SDL_Log("PL_SCREENSHOT: no picture (framebuffer %d x %d)", fbW, fbH);
            }
        }
        // 0.63.1: the frame thread's own work: its waits for the card (the swapchain image's fence, the acquire, batches waited for) counted apart
        const double workMs = std::max(0.0, static_cast<double>(SDL_GetPerformanceCounter() - now) * 1000.0 / SDL_GetPerformanceFrequency() - app::frameCostPeek().waitMs());
        prevWorkMs = workMs;
        app.stats.add(frameDt * 1000.0, workMs);
        static const bool bhMs = SDL_getenv("PL_BH_FRAMETIME") != nullptr;   // 0.62.0 (measuring, both APIs): the frame's time and the sky pass's time on the card
        if (bhMs && app.lab == blackhole::kLabIndex && !app.pane) std::printf("PLBHMS f=%d frame=%.2f sky=%.3f scale=%.3f\n", frame, frameDt * 1000.0, renderer.skyStats().fullMs, renderer.skyStats().scale);
        app.live.frame(isLive, frameDt * 1000.0);
        const double bk0 = app::frameCostNowMs();
        ui::benchmarkEndFrame(app, frameDt * 1000.0, workMs, &lastCost);
        app::frameCostAdd(app::FrameCostKind::Bookkeeping, app::frameCostNowMs() - bk0);
        if (useVulkan) {
            vk.present();
            if (vkFirst == 1 && vk.ok()) { app::gpuVulkanGuard().end("first frame", "shown"); app::gpuVulkanGuard().finished(true); vkFirst = 2; }
            if (!vk.ok() && vkFirst != 3) {             // a call failed after the start (the device lost, say): kept, so the next start uses OpenGL
                app::gpuVulkanGuard().failed(vkFirst == 1 ? "first frame" : "a frame", "a Vulkan call failed (the log has it)");
                vkFirst = 3;
                ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);
            }
        } else SDL_GL_SwapWindow(window);
        if (app.startupMs < 0 && (useVulkan || ui::benchLabFinished(app, renderer)))      // 0.56.1: the start-up time for the benchmark report (to the first finished picture)
            app.startupMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_appStart).count();
        static const double paceMs = SDL_getenv("PL_FRAME_MS") ? std::atof(SDL_getenv("PL_FRAME_MS")) : 0;   // 0.55.5 (automatic checks, measuring): a frame takes at
        if (paceMs > 0) {                                                                                       // least this long, as with VSync on a real screen
            const double usedMs = static_cast<double>(SDL_GetPerformanceCounter() - now) * 1000.0 / SDL_GetPerformanceFrequency();
            if (usedMs < paceMs) SDL_Delay(static_cast<Uint32>(paceMs - usedMs));
        }
        {   const double sv0 = app::frameCostNowMs();
            ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0);
            app::frameCostAdd(app::FrameCostKind::Settings, app::frameCostNowMs() - sv0); }
        if (app.vsync != vsyncOn) { vsyncOn = app.vsync; if (useVulkan) vk.setVsync(vsyncOn); else SDL_GL_SetSwapInterval(vsyncOn ? 1 : 0); }
    };

    live.draw = [&] { runFrame(true); };
    SDL_AddEventWatch(onLiveExpose, &live);
    while (running) runFrame(false);
    if (g_orbitKeep.kept + g_orbitKeep.drawn > 0) std::printf("PLORBITKEEP kept=%ld drawn=%ld\n", g_orbitKeep.kept, g_orbitKeep.drawn);   // 0.67.0 (checks): the Orbit Lab's frames kept and drawn
    SDL_RemoveEventWatch(onLiveExpose, &live);
    ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0, true);   // 0.21: a change in the last quarter second before closing was lost (seen in molecule_pictures on a fast run)
    ui::logFontAtlas();                              // PL_LOG_ATLAS=1 (stage 5j memory check): once, at the end
    {   // PL_LOG_SHELL: what the new UI drew (text sizes, rectangles), for the automatic checks
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        ui::shellReport(w, h, scale, ui::look().textScale);
    }
    if (SDL_getenv("PL_LIVE_REPORT"))
        std::printf("Live frames: %ld in %d drags, worst gap %.1f ms, lead-in %.1f ms, biggest physics step %.4f s\n", app.live.frames, app.live.drags, app.live.worstGapMs, app.live.leadInMs, maxPhysicsDt);
    if (SDL_getenv("PL_LOG_DRAWKINDS")) std::printf("PLDRAWKINDS %s\n", renderer.drawKindsText().c_str());   // 0.60.0 (checks: which drawing calls this run made)

    app::gpuHelperStop();                               // 0.68.0: no more helpers (a worker waiting for one goes on, and is then stopped)
    app::gpuGuardEndAll();                              //   nor for the guards' turn (a frame guard may hold it while the app ends)
    if (useVulkan) {
        if (SDL_getenv("PL_VK_VALIDATION")) std::printf("PLVK validation errors=%d warnings=%d\n", vk.validationErrors(), vk.validationWarnings());
        renderer.useVulkan(nullptr);                    // 0.60.0 (its Vulkan objects go with the backend)
        vk.shutdown();                                  // (Dear ImGui's Vulkan backend too)
    } else ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    atom::setOutlineCompute(nullptr);
    app.blackholeView.stopWork();                    // 0.55.6: its worker may read the card's memory, which the renderer frees
    app::gpuGuard().closing();
    app::gpuSliceGuard().closing(); app::gpuJumpGuard().closing();   // 0.57.0
    app::gpuMolCloudGuard().closing();                               // 0.57.1
    app::gpuVulkanGuard().closing();                 // 0.59.0
    if (useVulkan) for (const auto& [name, g] : app::vulkanCardGuards()) g->closing();   // 0.63.1: all ten (0.62.0 and 0.63.0 closed only the lens:
                                                     // the outline's, groups', rocks' and cloud's markers of a step still open stayed after a normal end)
    app::gpuSkyGuard().closing();                    // 0.56.0                       // 0.55.7: the app ends normally: a marker of a step still open (the path not finished) is removed
    if (!useVulkan) {
        renderer.shutdown();
        SDL_GL_DestroyContext(gl);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
