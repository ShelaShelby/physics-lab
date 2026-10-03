// Physics Lab (native) — Orbit Lab pilot.
//
// One window: the Orbit Lab picture on the left (drawn with OpenGL 3.3), the controls on
// the right (Dear ImGui). The physics runs in fixed steps of 1/240 s of real time, like
// the web version, so the orbits do not depend on the frame rate.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "app/bench.h"
#include "app/frame_time.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "orbit/format.h"
#include "orbit/units.h"
#include "render/gl_loader.h"
#include "render/renderer2d.h"
#include "ui/changelog.h"
#include "ui/fonts.h"
#include "ui/learn.h"
#include "ui/look.h"
#include "ui/settings.h"
#include "ui/shot.h"
#include "ui/fps_counter.h"
#include "ui/benchmark.h"
#include "orbit/sky.h"
#include "ui/app_pages.h"
#include "ui/app_tabs.h"
#include "ui/atom_learn.h"
#include "ui/atom_panel.h"
#include "ui/orbit_panel.h"

namespace {

#include "ui/icon_data.inc"      // kIconRgba: the window icon (64 x 64)

SDL_Window* createWindow(float scale, bool multisample, const app::WindowState& ws) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, multisample ? 1 : 0);   // smooth edges on lines and dots
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample ? 4 : 0);
    const std::string title = std::string("Physics Lab ") + ui::appVersion() + " — Orbit Lab";
    int w = static_cast<int>(1400 * scale), h = static_cast<int>(860 * scale);          // the first time: 1400 × 860 points
    if (ws.w > 0) { w = ws.w; h = ws.h; }                                              // later: where it was (window-system units)
    if (const char* wh = SDL_getenv("PL_WINDOW")) { int a = 0, b = 0; if (std::sscanf(wh, "%dx%d", &a, &b) == 2) { w = static_cast<int>(a * scale); h = static_cast<int>(b * scale); } }   // for automatic checks: PL_WINDOW=1000x640
    return SDL_CreateWindow(title.c_str(), w, h,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

const char* const kDriverAdvice =
    "1. Update the graphics driver: get the newest one from the maker of your graphics card (NVIDIA, AMD or Intel), or run Windows Update.\n"
    "2. Restart the computer and try again.\n"
    "3. If you are using Remote Desktop or a virtual machine, run Physics Lab on the computer itself: those often have no 3D graphics.";

}  // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) { fail("Could not start the window system", "Physics Lab could not start its window system, so it has nothing to draw on.", "Restart the computer and try again. If it still fails, tell Alex what Windows version you have."); return 1; }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());   // 1.5 on a Windows screen at 150 %
    if (scale <= 0) scale = 1;
    // the saved settings come first: they say where the window was
    ui::AppState app;
    ui::resetToToday(app);
    app.atom.p = atom::AtomParams::appDefaults(); app.atom.restart();   // the app starts with the glowing, denser cloud
    ui::loadSettings(app);                              // the theme, text size, font, motion, units, unlocks and both labs' settings, and the window
    std::vector<app::ScreenRect> screens;
    {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        for (int i = 0; ids && i < n; i++) { SDL_Rect r; if (SDL_GetDisplayBounds(ids[i], &r)) screens.push_back({r.x, r.y, r.w, r.h}); }
        SDL_free(ids);
    }
    app::WindowState start;                             // size 0: the default
    if (app.prefs.window) start = app::fitWindow(*app.prefs.window, screens);
    SDL_Window* window = createWindow(scale, true, start);
    SDL_GLContext gl = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!gl) {                                          // no multisampling available: try again without
        if (window) SDL_DestroyWindow(window);
        window = createWindow(scale, false, start);
        gl = window ? SDL_GL_CreateContext(window) : nullptr;
    }
    if (SDL_getenv("PL_FAKE_GLFAIL")) { if (gl) SDL_GL_DestroyContext(gl); gl = nullptr; if (window) SDL_DestroyWindow(window); window = nullptr; }   // for automatic checks: pretend OpenGL is missing
    if (!window || !gl) { fail("Could not open an OpenGL 3.3 window", "Physics Lab draws with OpenGL 3.3 (3D graphics), and your computer did not give it a window to draw in. This usually means the graphics driver is too old, or missing.", kDriverAdvice); return 1; }
    if (SDL_Surface* icon = SDL_CreateSurfaceFrom(64, 64, SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char*>(kIconRgba), 64 * 4)) {   // the window's icon (title bar, taskbar)
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);                          // vsync: smooth, and no wasted work
    if (start.w > 0 && start.hasPos) SDL_SetWindowPosition(window, start.x, start.y);
    else SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);   // the first time, or the saved place is off every screen now
    SDL_ShowWindow(window);
    Uint64 maximizeUntil = 0;                           // the window system maximises a moment later: do not record the old state meanwhile
    if (start.w > 0 && start.maximized) { SDL_MaximizeWindow(window); maximizeUntil = SDL_GetTicks() + 1500; }

    const char* missing = nullptr;
    if (!plgl::loadGL(&missing)) { std::fprintf(stderr, "OpenGL function missing: %s\n", missing); fail("OpenGL 3.3 is not available", "Physics Lab needs OpenGL 3.3 (3D graphics), and your graphics driver does not offer all of it.", kDriverAdvice); return 1; }
    std::printf("OpenGL: %s | %s\n", plgl::glGetString(GL_VERSION), plgl::glGetString(GL_RENDERER));
    render::Renderer2D renderer;
    if (!renderer.init()) { fail("Could not set up the OpenGL shaders", "Your graphics driver started, but would not accept the drawing programs Physics Lab sends it.", kDriverAdvice); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                           // nothing saved next to the .exe
    io.ConfigDebugHighlightIdConflicts = false;         // never show ImGui's red "conflicting ID" box to a user (the conflicts themselves are fixed)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.WindowRounding = 0;
    std::printf("Font: %s\n", ui::loadFonts(16.0f));
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    if (SDL_getenv("PL_IMPERIAL")) fmt::setImperial(true);   // for automatic checks
    if (const char* t = SDL_getenv("PL_THEME")) { app.prefs.unlocked.insert(t); app.prefs.theme = t; }   // for automatic checks
    if (const char* t = SDL_getenv("PL_TEXTSIZE")) app.prefs.textSize = t;
    if (const char* t = SDL_getenv("PL_FONT")) app.prefs.font = t;
    if (const char* t = SDL_getenv("PL_MOTION")) app.prefs.motion = t;
    auto systemLight = [] { return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT || SDL_getenv("PL_LIGHT"); };   // Windows' light/dark setting (the web's "Auto")
    ui::updateLook(app.prefs, systemLight(), ui::systemReducedMotion());
    if (ui::look().reduced) app.paused = true;          // reduced motion: the lab starts still, with a ▶ Play button
    app.visited[app.lab] = true;

    const float panelWidth = 360 * scale;              // window coordinates
    double acc = 0;                                     // real time not yet simulated, s
    Uint64 last = SDL_GetPerformanceCounter();
    bool vsyncOn = true;
    double fps = -1, fpsTime = 0;                      // fps < 0: not measured yet (the web shows nothing for the first half second)
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
    if (const char* z = SDL_getenv("PL_ZOOM")) ui::setZoom(app, std::atof(z));
    if (SDL_getenv("PL_3D")) app.set.view3D = true;
    if (const char* tl = SDL_getenv("PL_TILT")) { double yw = 0, pt = 0; if (std::sscanf(tl, "%lf,%lf", &yw, &pt) == 2) { app.view.yaw = yw; app.view.pitch = pt; } }   // for automatic checks: PL_TILT=yaw,pitch (radians) of the 3D view
    if (const char* ph = SDL_getenv("PL_PHYSICS")) { bool r = false; if (orbit::planetPhysicsFromText(ph, r)) app.set.realPlanets = r; }   // for automatic checks: Planet physics Simple / Real
    if (const char* g = SDL_getenv("PL_GR")) app.set.relativity = std::atoi(g) != 0;   // for automatic checks: General relativity (1PN) on (1) / off (0)
    if (const char* d = SDL_getenv("PL_DATE")) { int64_t ms; if (ui::parseDateTime(d, "12:00", ms)) std::printf("%s\n", ui::jumpToDate(app, ms).c_str()); else std::printf("not a date\n"); }
    if (SDL_getenv("PL_JOURNEY")) ui::journeyStart(app);
    if (const char* l = SDL_getenv("PL_LEARN")) app.learn = std::atoi(l);     // open a Learn guide (1, 2 or 3)
    if (const char* o = SDL_getenv("PL_OPEN")) { const std::string w = o; app.showSettings = w == "settings"; app.showAbout = w == "about"; app.showHistory = w == "history"; app.showLabLog = w == "log"; }   // for automatic checks: open a window
    if (SDL_getenv("PL_CONS")) app.set.cons = orbit::ConsMode::LinesAndNames;
    // Atom Lab checks: PL_ATOM=1 opens the Atom Lab, PL_ATOM_SET="Z=26;view=Cloud;zoom=4.5" sets its settings,
    // PL_ATOM_DO="absorb" presses Emit / Absorb / Auto photons (or ⌕ Nucleus / ⌕ Nucleon: "nucleus", "nucleon"), PL_ATOM_FOCUS=n dives into nucleon n
    if (SDL_getenv("PL_ATOM")) app.lab = 1;
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
    const char* atomDo = SDL_getenv("PL_ATOM_DO");
    const int atomDoFrame = SDL_getenv("PL_ATOM_DO_FRAME") ? std::atoi(SDL_getenv("PL_ATOM_DO_FRAME")) : 0;   // PL_ATOM_DO_FRAME=n: press it at frame n (after the cloud has filled), not at the start
    const auto doAtom = [&] { if (!atomDo) return; const std::string a = atomDo; if (a == "emit") app.atom.emit(); else if (a == "absorb") app.atom.absorb(); else if (a == "auto") app.atom.toggleAuto();
                                               else if (a == "nucleus") app.atom.zoomNucleus(); else if (a == "nucleon") app.atom.zoomNucleon(); };
    if (atomDoFrame == 0) doAtom();
    if (const char* l = SDL_getenv("PL_ATOM_LEARN")) { app.atomLearn = std::atoi(l); if (const char* t = SDL_getenv("PL_ATOM_TAB")) { app.scaleTab = t; app.massTab = t; } }   // open an Atom Lab guide (1–4), on a tab
    int frame = 0;
    bool running = true;
    float atomBarH = 0;                                 // the bar under the Atom Lab picture, window coordinates

    LiveWatch live;
    int normalFrames = 0;
    double maxPhysicsDt = 0;                            // the biggest time step any frame gave the simulation (PL_LIVE_REPORT prints it)
    const double fakeModalS = SDL_getenv("PL_FAKE_MODAL") ? std::atof(SDL_getenv("PL_FAKE_MODAL")) : 0;   // for automatic checks: pretend the window system holds the loop for n seconds
    const bool fakeResize = SDL_getenv("PL_FAKE_MODAL_RESIZE") != nullptr;
    // One frame. live: drawn from the event watch while the window system holds the loop (no events are read then).
    int atomDoCount = 0;
    const auto runFrame = [&](bool isLive) {
        app.liveFrame = isLive;
        if (atomDoFrame > 0 && ++atomDoCount == atomDoFrame) doAtom();
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        float canvasWinW = 1, headerH = 0, pictureWinH = 1;
        const auto measure = [&] {                      // the window as it is now (it can change while the window system has the loop)
            SDL_GetWindowSize(window, &winW, &winH);
            SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
            canvasWinW = std::max(1.0f, winW - panelWidth);   // the picture's width in window coordinates
            // the header is across the top; the Atom Lab has a bar under its picture (view buttons and the hint); both are measured each frame
            headerH = ui::headerHeight();
            pictureWinH = std::max(1.0f, winH - headerH - (app.lab == 1 ? atomBarH : 0));
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
        SDL_Event e;
        while (!isLive && SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            switch (e.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if (e.window.windowID == SDL_GetWindowID(window)) running = false; break;
            case SDL_EVENT_KEY_DOWN:
                if (!app::labInputAllowed(app.benchRunning)) break;      // the benchmark is running: only Cancel and Esc do anything
                if (e.key.key == SDLK_F11 && !e.key.repeat) SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));   // F11: full screen on / off
                if (!io.WantCaptureKeyboard && e.key.key == SDLK_SPACE) app.paused = !app.paused;
                break;
            case SDL_EVENT_MOUSE_WHEEL:                 // the web: zoom −= deltaY × 0.0007, about 0.07 per wheel notch
                if (!app::labInputAllowed(app.benchRunning)) break;
                if (app.lab == 1) {                     // Atom Lab: zoom −= deltaY × 0.004, 0.4 per wheel notch
                    if (!io.WantCaptureMouse && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH && e.wheel.mouse_y < headerH + pictureWinH) app.atom.setParam("zoom", app.atom.p.zoom + e.wheel.y * 0.4);
                    break;
                }
                if (!io.WantCaptureMouse && e.wheel.mouse_x < canvasWinW && e.wheel.mouse_y >= headerH) {
                    if (app.journey.active) ui::journeyStop(app);   // touching the picture stops a Journey (and brings your settings back)
                    else ui::setZoom(app, app.set.zoom + e.wheel.y * 0.07);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!app::labInputAllowed(app.benchRunning)) break;
                if (app.lab == 1) {
                    if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && e.button.y < headerH + pictureWinH) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                    break;
                }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && app.journey.active) { ui::journeyStop(app); break; }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseDown && app::labInputAllowed(app.benchRunning)) {
                    if (!dragging && std::hypot(e.motion.x - downX, e.motion.y - downY) > 4 * scale) dragging = true;
                    // 3D: dragging turns the view, unless "Drag moves the view" is on, or with the right button or Shift held
                    const bool rotate = app.set.view3D && !app.view.panMode && !rightDrag && !(SDL_GetModState() & SDL_KMOD_SHIFT);
                    if (dragging && app.lab == 1) app.atom.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rightDrag || (SDL_GetModState() & SDL_KMOD_SHIFT));
                    else if (dragging) app.view.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rotate);
                    lastX = e.motion.x; lastY = e.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseDown && !dragging && app::labInputAllowed(app.benchRunning) && e.button.button == SDL_BUTTON_LEFT) {
                    if (app.lab == 1) app.atom.click(e.button.x / scale, (e.button.y - headerH) / scale);
                    else app.view.click(e.button.x / scale, (e.button.y - headerH) / scale);
                }
                mouseDown = dragging = false;
                break;
            default: break;
            }
        }
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

        ui::benchmarkTestHook(app);
        if (const char* at = SDL_getenv("PL_SAVE_IMAGE")) { static int n = 0; if (++n == std::atoi(at)) app.shot.requested = true; }   // for automatic checks: press 📷 Save image at frame n
        ui::benchmarkBeginFrame(app, renderer, fbW, fbH);   // a benchmark scene is set up here, before the frame
        app.sys.setRelativity(app.set.relativity);          // Real: General relativity (1PN) on / off (does nothing when it has not changed)
        app.sys.setRealPlanets(app.set.realPlanets);        // Planet physics: Simple / Real (does nothing when it has not changed)
        // ---- time and physics (fixed steps, as in the web version) ----
        const Uint64 now = SDL_GetPerformanceCounter();   // the frame starts here
        const double frameDt = static_cast<double>(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        const double dt = app::stepSeconds(frameDt, units::kMaxFrameDt);   // a slow frame counts as at most 0.05 s, a stall (window held by the system) as nothing: no jump, no catching up
        maxPhysicsDt = std::max(maxPhysicsDt, dt);
        if (app.pane) {                                 // the Learn or Achievements tab is open: the lab waits (the web draws nothing then)
        } else if (app.lab == 1) {                      // Atom Lab: tick every frame (decay clock, glides), fixed steps while running
            app.atom.paused = app.paused;
            app.atom.tick(dt);
            if (!app.paused) {
                acc += dt;
                while (acc >= units::kPhysicsStep) { app.atom.step(units::kPhysicsStep); acc -= units::kPhysicsStep; }
            }
        } else {
            app.view.tick(dt, app.set);
            ui::journeyTick(app, dt);
            if (!app.paused) {
                acc += dt;
                while (acc >= units::kPhysicsStep) { app.sys.step(units::kPhysicsStep, app.set.tspeed); acc -= units::kPhysicsStep; }
            }
        }
        fpsFrames++; fpsTime += frameDt;
        if (fpsTime >= 0.5) { fps = fpsFrames / fpsTime; fpsFrames = 0; fpsTime = 0; }

        // ---- build the frame ----
        ui::updateLook(app.prefs, systemLight(), ui::systemReducedMotion());   // theme, text size, font, motion
        ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, app.benchRunning ? 0.0f : 0.45f);   // the benchmark's input lock is a modal that must not dim the picture
        if (static int shownLab = -1; shownLab != app.lab) {   // the window title follows the lab on screen
            shownLab = app.lab;
            SDL_SetWindowTitle(window, (std::string("Physics Lab ") + ui::appVersion() + " — " + app::kLabInfo[app.lab].name).c_str());
        }
        app.atom.reducedMotion = ui::look().reduced;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ui::header(app);                                // across the top: title, subtitle and the app's buttons
        headerH = ui::headerHeight();
        pictureWinH = std::max(1.0f, winH - headerH - (app.lab == 1 ? atomBarH : 0));
        app.canvasW = canvasWinW / scale;               // picture size in points (like CSS pixels)
        app.canvasH = pictureWinH / scale;
        app.set.lightTheme = ui::look().c.light;
        app.set.theme = ui::look().c;
        app.set.textScale = ui::look().textScale;
        if (app.pane) ui::paneWindow(app, headerH);
        else if (app.lab == 1) ui::atomPanel(app, panelWidth);
        else ui::orbitPanel(app, panelWidth);
        renderer.begin(app.canvasW, app.canvasH);
        ImDrawList* text = ImGui::GetBackgroundDrawList();
        text->PushClipRect(ImVec2(0, headerH), ImVec2(canvasWinW, headerH + pictureWinH));
        if (app.pane) text->PopClipRect();              // a tab page covers the lab: nothing to draw
        else if (app.lab == 1) {
            app.atomView.draw(renderer, text, 0, headerH, scale, app.atom, app.canvasW, app.canvasH, ui::look().c, ui::look().textScale);
            text->PopClipRect();
            atomBarH = ui::atomBottomBar(app, 0, headerH + pictureWinH, canvasWinW, false);
            ui::atomBottomBar(app, 0, headerH + pictureWinH, canvasWinW, true);
        } else {
            atomBarH = 0;
            app.set.paused = app.paused;
            if (SDL_getenv("PL_SKYOFF") && frame == 0) app.set.sky = orbit::SkyMode::Off;
            app.view.draw(renderer, text, 0, headerH, scale, app.sys, app.set, app.canvasW, app.canvasH);
            text->PopClipRect();
        }
        // the frames-per-second counter, top right of the picture, for every lab (the web's #fps); not in a saved picture, not under a tab page
        if (!app.pane && !app.shot.requested) ui::drawFpsCounter(text, canvasWinW, headerH, scale, fps);
        ui::labLogWindow(&app.showLabLog, app.lab);
        ui::aboutWindow(app);
        ui::historyWindow(app);
        if (app.lab == 1) ui::atomLearnWindow(app);
        else ui::learnWindow(app);
        ui::settingsWindow(app);
        ui::benchmarkWindow(app);                       // the benchmark: what it will do, the progress, the result
        ui::appTabsFrame(app);                          // achievements and their pop-ups, the secret pop-ups
        ImGui::Render();

        // ---- draw: the picture first, then the panels and text on top ----
        ui::benchmarkGpuBegin(app);                     // the GPU time of this frame (only while the benchmark runs)
        plgl::glViewport(0, 0, fbW, fbH);
        const app::Rgb pageBg = ui::look().c.bg;           // the theme's page colour
        plgl::glClearColor(pageBg.r, pageBg.g, pageBg.b, 1);
        plgl::glClear(GL_COLOR_BUFFER_BIT);
        const int canvasFbW = static_cast<int>(std::lround(canvasWinW * fbW / std::max(1, winW)));
        const int pictureFbH = static_cast<int>(std::lround(pictureWinH * fbH / std::max(1, winH)));
        const int headerFbH = static_cast<int>(std::lround(headerH * fbH / std::max(1, winH)));
        renderer.setGrayscale(ui::look().grayscale, {pageBg.r, pageBg.g, pageBg.b, 1});
        renderer.end(canvasFbW, pictureFbH, fbH - headerFbH - pictureFbH);
        app.drawCalls = renderer.lastDrawCalls(); app.drawInstances = renderer.lastInstances();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ui::benchmarkGpuEnd(app);
        if (app.shot.requested) {                       // 📷 Save image: the picture area as it is on screen now, readout included
            app.shot.requested = false;
            ui::takeShot(app, renderer, {fbW, fbH, winW, winH, canvasFbW, pictureFbH, headerH});
        }
        static const double maxSeconds = SDL_getenv("PL_EXIT_AFTER_SECONDS") ? std::atof(SDL_getenv("PL_EXIT_AFTER_SECONDS")) : 0;   // for automatic checks
        if ((maxFrames && ++frame >= maxFrames) || (maxSeconds > 0 && SDL_GetTicks() / 1000.0 >= maxSeconds)) {
            running = false;
            if (screenshot) {                           // read the finished frame back and save it
                SDL_Surface* shot = SDL_CreateSurface(fbW, fbH, SDL_PIXELFORMAT_ABGR8888);
                plgl::glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, shot->pixels);
                SDL_FlipSurface(shot, SDL_FLIP_VERTICAL);
                SDL_SaveBMP(shot, screenshot);
                SDL_DestroySurface(shot);
            }
        }
        const double workMs = static_cast<double>(SDL_GetPerformanceCounter() - now) * 1000.0 / SDL_GetPerformanceFrequency();
        app.stats.add(frameDt * 1000.0, workMs);
        app.live.frame(isLive, frameDt * 1000.0);
        ui::benchmarkEndFrame(app, frameDt * 1000.0, workMs);
        SDL_GL_SwapWindow(window);
        ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0);
        if (app.vsync != vsyncOn) { vsyncOn = app.vsync; SDL_GL_SetSwapInterval(vsyncOn ? 1 : 0); }
    };

    live.draw = [&] { runFrame(true); };
    SDL_AddEventWatch(onLiveExpose, &live);
    while (running) runFrame(false);
    SDL_RemoveEventWatch(onLiveExpose, &live);
    if (SDL_getenv("PL_LIVE_REPORT"))
        std::printf("Live frames: %ld in %d drags, worst gap %.1f ms, lead-in %.1f ms, biggest physics step %.4f s\n", app.live.frames, app.live.drags, app.live.worstGapMs, app.live.leadInMs, maxPhysicsDt);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    renderer.shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
