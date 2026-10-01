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
#include <string>

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
#include "orbit/sky.h"
#include "ui/app_pages.h"
#include "ui/app_tabs.h"
#include "ui/atom_learn.h"
#include "ui/atom_panel.h"
#include "ui/orbit_panel.h"

namespace {

SDL_Window* createWindow(float scale, bool multisample) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, multisample ? 1 : 0);   // smooth edges on lines and dots
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample ? 4 : 0);
    const std::string title = std::string("Physics Lab ") + ui::appVersion() + " — Orbit Lab";
    return SDL_CreateWindow(title.c_str(), static_cast<int>(1400 * scale), static_cast<int>(860 * scale),
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
}

void fail(const char* what) {
    std::fprintf(stderr, "%s: %s\n", what, SDL_GetError());
    if (SDL_getenv("PL_EXIT_AFTER_FRAMES")) return;    // an automatic check: no window waiting for a click
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Physics Lab", (std::string(what) + "\n" + SDL_GetError()).c_str(), nullptr);
}

}  // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) { fail("Could not start SDL"); return 1; }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());   // 1.5 on a Windows screen at 150 %
    if (scale <= 0) scale = 1;
    SDL_Window* window = createWindow(scale, true);
    SDL_GLContext gl = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!gl) {                                          // no multisampling available: try again without
        if (window) SDL_DestroyWindow(window);
        window = createWindow(scale, false);
        gl = window ? SDL_GL_CreateContext(window) : nullptr;
    }
    if (!window || !gl) { fail("Could not open an OpenGL 3.3 window"); return 1; }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);                          // vsync: smooth, and no wasted work
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    const char* missing = nullptr;
    if (!plgl::loadGL(&missing)) { std::fprintf(stderr, "OpenGL function missing: %s\n", missing); fail("OpenGL 3.3 is not available"); return 1; }
    std::printf("OpenGL: %s | %s\n", plgl::glGetString(GL_VERSION), plgl::glGetString(GL_RENDERER));
    render::Renderer2D renderer;
    if (!renderer.init()) { fail("Could not set up the OpenGL shaders"); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                           // nothing saved next to the .exe
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.WindowRounding = 0;
    std::printf("Font: %s\n", ui::loadFonts(16.0f));
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    ui::AppState app;
    ui::resetToToday(app);
    ui::loadSettings(app);                              // the theme, text size, font, motion, units, unlocks and both labs' settings
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
    double fps = 0, fpsTime = 0;
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
        if (std::string(name).rfind("far:", 0) == 0) { app.set.far = true; app.view.select({orbit::Selection::Far, 0, orbit::farIndexOf(std::string(name).substr(4))}, true); }   // e.g. far:sgra
    }
    if (const char* z = SDL_getenv("PL_ZOOM")) ui::setZoom(app, std::atof(z));
    if (SDL_getenv("PL_3D")) app.set.view3D = true;
    if (const char* d = SDL_getenv("PL_DATE")) { int64_t ms; if (ui::parseDateTime(d, "12:00", ms)) std::printf("%s\n", ui::jumpToDate(app, ms).c_str()); else std::printf("not a date\n"); }
    if (SDL_getenv("PL_JOURNEY")) ui::journeyStart(app);
    if (const char* l = SDL_getenv("PL_LEARN")) app.learn = std::atoi(l);     // open a Learn guide (1, 2 or 3)
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
    if (const char* d = SDL_getenv("PL_ATOM_DO")) { const std::string a = d; if (a == "emit") app.atom.emit(); else if (a == "absorb") app.atom.absorb(); else if (a == "auto") app.atom.toggleAuto();
                                               else if (a == "nucleus") app.atom.zoomNucleus(); else if (a == "nucleon") app.atom.zoomNucleon(); }
    if (const char* f = SDL_getenv("PL_ATOM_FOCUS")) app.atom.s.focus = std::atoi(f);
    if (const char* l = SDL_getenv("PL_ATOM_LEARN")) { app.atomLearn = std::atoi(l); if (const char* t = SDL_getenv("PL_ATOM_TAB")) { app.scaleTab = t; app.massTab = t; } }   // open an Atom Lab guide (1–4), on a tab
    int frame = 0;
    bool running = true;
    float atomBarH = 0;                                 // the bar under the Atom Lab picture, window coordinates

    while (running) {
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
        const float canvasWinW = std::max(1.0f, winW - panelWidth);   // the picture's width in window coordinates
        // the header is across the top; the Atom Lab has a bar under its picture (view buttons and the hint); both are measured each frame
        float headerH = ui::headerHeight();
        float pictureWinH = std::max(1.0f, winH - headerH - (app.lab == 1 ? atomBarH : 0));

        // ---- input ----
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            switch (e.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if (e.window.windowID == SDL_GetWindowID(window)) running = false; break;
            case SDL_EVENT_KEY_DOWN:
                if (!io.WantCaptureKeyboard && e.key.key == SDLK_SPACE) app.paused = !app.paused;
                break;
            case SDL_EVENT_MOUSE_WHEEL:                 // the web: zoom −= deltaY × 0.0007, about 0.07 per wheel notch
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
                if (app.lab == 1) {
                    if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && e.button.y < headerH + pictureWinH) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                    break;
                }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH && app.journey.active) { ui::journeyStop(app); break; }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && e.button.y >= headerH) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseDown) {
                    if (!dragging && std::hypot(e.motion.x - downX, e.motion.y - downY) > 4 * scale) dragging = true;
                    // 3D: dragging turns the view, unless "Drag moves the view" is on, or with the right button or Shift held
                    const bool rotate = app.set.view3D && !app.view.panMode && !rightDrag && !(SDL_GetModState() & SDL_KMOD_SHIFT);
                    if (dragging && app.lab == 1) app.atom.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rightDrag || (SDL_GetModState() & SDL_KMOD_SHIFT));
                    else if (dragging) app.view.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rotate);
                    lastX = e.motion.x; lastY = e.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseDown && !dragging && e.button.button == SDL_BUTTON_LEFT) {
                    if (app.lab == 1) app.atom.click(e.button.x / scale, (e.button.y - headerH) / scale);
                    else app.view.click(e.button.x / scale, (e.button.y - headerH) / scale);
                }
                mouseDown = dragging = false;
                break;
            default: break;
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(20); last = SDL_GetPerformanceCounter(); continue; }

        // ---- time and physics (fixed steps, as in the web version) ----
        const Uint64 now = SDL_GetPerformanceCounter();   // the frame starts here
        const double frameDt = static_cast<double>(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        const double dt = std::min(frameDt, units::kMaxFrameDt);
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
            app.view.draw(renderer, text, 0, headerH, scale, app.sys, app.set, app.canvasW, app.canvasH, app.shot.requested ? -1.0 : fps);
            text->PopClipRect();
        }
        ui::labLogWindow(&app.showLabLog, app.lab);
        ui::aboutWindow(app);
        ui::historyWindow(app);
        if (app.lab == 1) ui::atomLearnWindow(app);
        else ui::learnWindow(app);
        ui::settingsWindow(app);
        ui::appTabsFrame(app);                          // achievements and their pop-ups, the secret pop-ups
        ImGui::Render();

        // ---- draw: the picture first, then the panels and text on top ----
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
        SDL_GL_SwapWindow(window);
        ui::saveSettingsWhenChanged(app, SDL_GetTicks() / 1000.0);
        if (app.vsync != vsyncOn) { vsyncOn = app.vsync; SDL_GL_SetSwapInterval(vsyncOn ? 1 : 0); }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    renderer.shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
