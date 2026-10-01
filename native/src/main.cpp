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
#include "ui/settings.h"
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
    ui::loadSettings(app);
    if (SDL_getenv("PL_IMPERIAL")) fmt::setImperial(true);   // for automatic checks

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
    if (const char* name = SDL_getenv("PL_SELECT"))
        for (const auto& b : app.sys.bodies)
            if (b.name == name) app.view.select({orbit::Selection::Body, b.id}, true);
    if (const char* z = SDL_getenv("PL_ZOOM")) ui::setZoom(app, std::atof(z));
    if (SDL_getenv("PL_3D")) app.set.view3D = true;
    if (const char* d = SDL_getenv("PL_DATE")) { int64_t ms; if (ui::parseDateTime(d, "12:00", ms)) std::printf("%s\n", ui::jumpToDate(app, ms).c_str()); else std::printf("not a date\n"); }
    if (SDL_getenv("PL_JOURNEY")) ui::journeyStart(app);
    if (const char* l = SDL_getenv("PL_LEARN")) app.learn = std::atoi(l);     // open a Learn guide (1, 2 or 3)
    if (SDL_getenv("PL_CONS")) app.set.cons = orbit::ConsMode::LinesAndNames;
    int frame = 0;
    bool running = true;

    while (running) {
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
        const float canvasWinW = std::max(1.0f, winW - panelWidth);   // the picture's width in window coordinates

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
                if (!io.WantCaptureMouse && e.wheel.mouse_x < canvasWinW) {
                    if (app.journey.active) ui::journeyStop(app);   // touching the picture stops a Journey (and brings your settings back)
                    else ui::setZoom(app, app.set.zoom + e.wheel.y * 0.07);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!io.WantCaptureMouse && e.button.x < canvasWinW && app.journey.active) { ui::journeyStop(app); break; }
                if (!io.WantCaptureMouse && e.button.x < canvasWinW) { mouseDown = true; dragging = false; rightDrag = e.button.button != SDL_BUTTON_LEFT; downX = lastX = e.button.x; downY = lastY = e.button.y; }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseDown) {
                    if (!dragging && std::hypot(e.motion.x - downX, e.motion.y - downY) > 4 * scale) dragging = true;
                    // 3D: dragging turns the view, unless "Drag moves the view" is on, or with the right button or Shift held
                    const bool rotate = app.set.view3D && !app.view.panMode && !rightDrag && !(SDL_GetModState() & SDL_KMOD_SHIFT);
                    if (dragging) app.view.drag((e.motion.x - lastX) / scale, (e.motion.y - lastY) / scale, rotate);
                    lastX = e.motion.x; lastY = e.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseDown && !dragging && e.button.button == SDL_BUTTON_LEFT) app.view.click(e.button.x / scale, e.button.y / scale);
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
        app.view.tick(dt, app.set);
        ui::journeyTick(app, dt);
        if (!app.paused) {
            acc += dt;
            while (acc >= units::kPhysicsStep) { app.sys.step(units::kPhysicsStep, app.set.tspeed); acc -= units::kPhysicsStep; }
        }
        fpsFrames++; fpsTime += frameDt;
        if (fpsTime >= 0.5) { fps = fpsFrames / fpsTime; fpsFrames = 0; fpsTime = 0; }

        // ---- build the frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        app.canvasW = canvasWinW / scale;               // picture size in points (like CSS pixels)
        app.canvasH = winH / scale;
        ui::orbitPanel(app, panelWidth);
        renderer.begin(app.canvasW, app.canvasH);
        ImDrawList* text = ImGui::GetBackgroundDrawList();
        text->PushClipRect(ImVec2(0, 0), ImVec2(canvasWinW, static_cast<float>(winH)));
        app.set.paused = app.paused;
        app.set.lightTheme = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT;   // Windows' light/dark setting (the web's "Auto")
        if (SDL_getenv("PL_LIGHT")) app.set.lightTheme = true;              // for automatic checks
        if (SDL_getenv("PL_SKYOFF") && frame == 0) app.set.sky = orbit::SkyMode::Off;
        app.view.draw(renderer, text, 0, 0, scale, app.sys, app.set, app.canvasW, app.canvasH, fps);
        text->PopClipRect();
        ui::changelogWindow(&app.showChangelog);
        ui::learnWindow(app);
        ui::settingsWindow(app);
        ImGui::Render();

        // ---- draw: the picture first, then the panels and text on top ----
        plgl::glViewport(0, 0, fbW, fbH);
        plgl::glClearColor(0.016f, 0.024f, 0.05f, 1);
        plgl::glClear(GL_COLOR_BUFFER_BIT);
        const int canvasFbW = static_cast<int>(std::lround(canvasWinW * fbW / std::max(1, winW)));
        renderer.end(canvasFbW, fbH);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (maxFrames && ++frame >= maxFrames) {
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
