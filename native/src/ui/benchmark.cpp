#include "ui/benchmark.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>

#include "app/bench.h"
#include "app/gpu_timer.h"
#include "app/shot.h"
#include "atom/atom_bench.h"
#include "atom/atom_core.h"
#include "imgui.h"
#include "orbit/belts.h"
#include "orbit/units.h"
#include "render/gl_loader.h"
#include "render/renderer2d.h"
#include "ui/app_pages.h"
#include "ui/changelog.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/shot.h"

namespace ui {

namespace {

struct SceneDef { BenchSceneInfo info; std::function<void(AppState&)> setup; };
constexpr int kNormalScenes = 4;
constexpr int kStressBeltScale = 8;      // scenes 1 to 4 never change, so results stay comparable between versions; the stress scenes come after them

// ---- the scenes (chosen by measuring which settings cost the most, see REPORT.md) ----
void commonSetup(AppState& app) {
    app.paused = false;
    app.vsync = false;
    app.learn = 0; app.atomLearn = 0; app.showSettings = app.showAbout = app.showHistory = app.showLabLog = false;
    app.journey.active = false;
}

void orbitHeaviest(AppState& app) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);                                   // the Solar System on a fixed day
    orbit::ViewSettings& s = app.set;
    s = orbit::ViewSettings();
    s.view3D = true; s.trueScale = true; s.fullTrails = true;
    s.moons = s.dwarfs = s.comets = s.belts = s.far = true;
    s.sky = orbit::SkyMode::Full;
    s.cons = orbit::ConsMode::LinesAndNames;
    s.tspeed = units::kTspeedMax;                 // fastest time
    app.view = orbit::OrbitView();
    app.view.yaw = 0.6; app.view.pitch = 1.0; app.view.spin = false;
    setZoom(app, 0.0);
}

// stress scene: the same view as scene 1, with 8 times as many rocks in the belts and the Oort cloud (the rocks do not touch each other: the physics is the same)
void orbitStress(AppState& app) {
    orbitHeaviest(app);
    app.sys.belts = orbit::makeBelts(app.sys.epochMs, kStressBeltScale);
}

void atomScene(AppState& app, int which) {
    commonSetup(app);
    app.lab = 1;
    atom::benchSetup(app.atom, which, app.canvasW, app.canvasH);
}
void atomNucleus(AppState& app) { atomScene(app, 0); }
void atomCloud(AppState& app) { atomScene(app, 1); }
void atomNucleon(AppState& app) { atomScene(app, 2); }
void atomStress(AppState& app) { atomScene(app, 3); }

const char* const STRESS_ATOM_WHAT = "oganesson-294 (118 protons, 176 neutrons, the biggest nucleus the app has), Accurate quarks, + Sea detail, Fuzzy nucleons, zoomed in 0.6 (about 4 times) closer than the whole nucleus, so that every nucleon is drawn large";

const std::vector<SceneDef>& scenes() {
    static const std::vector<SceneDef> v = {
        {{"1. Orbit Lab, heaviest view", "3D view, belts and Oort cloud on, all Solar System extras on, full night sky with constellation lines and names, "
                                          "far-away things on, full orbit trails, true scale, fastest time speed (1 s = about 50 years), zoom x1, fixed camera"}, orbitHeaviest},
        {{"2. Atom Lab, uranium-238 nucleus", "uranium-238 (92 protons, 146 neutrons), zoomed to the whole nucleus, Cinematic quarks, + Sea detail, Fuzzy nucleons"}, atomNucleus},
        {{"3. Atom Lab, Cloud 3D", "uranium-238, Cloud 3D view, the 4f orbital (the biggest cloud), all 12,000 dots, fixed camera"}, atomCloud},
        {{"4. Atom Lab, one nucleon", "uranium-238, Accurate quarks, + Sea detail, zoomed all the way into one nucleon (the first one)"}, atomNucleon},
        {{"5. Stress: Atom Lab, the biggest nucleus", STRESS_ATOM_WHAT}, atomStress},
        {{"6. Stress: Orbit Lab, 8 times the rocks", "scene 1 (3D, everything on, true scale, zoom x1, fastest time) with 8 times as many rocks in the asteroid belt, the Trojans, the Kuiper belt and the Oort cloud "
                                                     "(48,000 instead of 6,000; the same kinds of orbits, a fixed random seed). The rocks do not pull on each other, so the physics is unchanged"}, orbitStress},
    };
    return v;
}

// ---- the GPU timer ----
// For every measured frame, from the first GL call of the drawing (the clear) to the last one before the buffer swap (the panels and text):
//   q[0] GL_TIME_ELAPSED   the time the GPU was busy on it, all passes together (the picture, its soft layer, the instanced draws, ImGui)
//   q[1] / q[2] GL_TIMESTAMP at the start and at the end: the span from the first to the last GPU command, waiting included
// The results come a frame or two late; a ring of slots is used in turn, and read oldest first (app/gpu_timer.h).
struct GpuTimer {
    static constexpr int kSlots = 8;
    unsigned q[kSlots][3] = {};
    app::QueryRing ring{kSlots};
    int active = -1;
    bool ok = false, made = false;
    long framesSkipped = 0;                      // frames whose slots were all still waiting (not measured)
    void make() { if (!made) { made = true; ok = true; ring.reset(); for (auto& s : q) plgl::glGenQueries(3, s); } }
    void destroy() { if (made) { for (auto& s : q) plgl::glDeleteQueries(3, s); made = false; } }
};

struct GpuSample { double busyMs, spanMs; };

// Is GL_TIME_ELAPSED believable on this PC? A known heavy load (many see-through full-picture rectangles) is drawn a few times; the timer
// and a clock (up to glFinish) must roughly agree. The load is doubled until it takes about 2 ms.
app::GpuVerdict checkGpuTimer(render::Renderer2D& r, double wPts, double hPts, int fbW, int fbH) {
    unsigned q = 0;
    plgl::glGenQueries(1, &q);
    const double toMs = 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
    double qMed = 0, wMed = 0;
    for (int n = 16; n <= 16384; n *= 2) {
        std::vector<double> qs, ws;
        for (int rep = 0; rep < 9; rep++) {
            r.begin(wPts, hPts);
            for (int k = 0; k < n; k++) r.rect(0, 0, wPts, hPts, render::Color{0.5f, 0.5f, 0.5f, 0.02f});
            plgl::glViewport(0, 0, fbW, fbH);
            plgl::glFinish();
            const Uint64 t0 = SDL_GetPerformanceCounter();
            plgl::glBeginQuery(GL_TIME_ELAPSED, q);
            r.end(fbW, fbH, 0);
            plgl::glEndQuery(GL_TIME_ELAPSED);
            plgl::glFinish();
            const double wall = static_cast<double>(SDL_GetPerformanceCounter() - t0) * toMs;
            unsigned long long ns = 0;
            plgl::glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
            if (rep >= 2) { qs.push_back(ns / 1e6); ws.push_back(wall); }      // the first two are warm-up (the GPU may still be raising its clock)
        }
        std::sort(qs.begin(), qs.end()); std::sort(ws.begin(), ws.end());
        qMed = qs[qs.size() / 2]; wMed = ws[ws.size() / 2];
        if (wMed >= 2.0) break;
    }
    plgl::glDeleteQueries(1, &q);
    return app::judgeGpuTimer(qMed, wMed);
}

}  // namespace

const std::vector<BenchSceneInfo>& benchmarkScenes() {
    static std::vector<BenchSceneInfo> v;
    if (v.empty()) for (const SceneDef& s : scenes()) v.push_back(s.info);
    return v;
}

// everything the benchmark must give back: the labs as they were
struct BenchState {
    enum class Stage { Closed, Ask, Running, Done } stage = Stage::Closed;
    std::unique_ptr<app::BenchRun> run;
    // ---- the user's state, put back at the end (or when cancelled) ----
    int lab = 0;
    bool paused = false, vsync = true;
    int learn = 0, atomLearn = 0;
    bool showSettings = false, showAbout = false, showHistory = false, showLabLog = false;
    std::unique_ptr<orbit::SolarSystem> sys;
    std::unique_ptr<orbit::OrbitView> view;
    std::unique_ptr<atom::AtomLab> atom;
    std::unique_ptr<atom::AtomView> atomView;
    orbit::ViewSettings set;
    Journey journey;
    // ---- results ----
    app::BenchEnv env;
    std::vector<app::BenchScene> results;
    std::string text, note;
    bool noteFailed = false;
    GpuTimer gpu;
    std::vector<GpuSample> gpuReady;
    app::GpuVerdict gpuCheck;
    bool gpuChecked = false;
    bool includeStress = false;               // the checkbox: run the stress scenes after the four normal ones
    double warm = app::kBenchWarmupS, measure = app::kBenchMeasureS;
    bool cancelled = false;
};

namespace {

int activeScenes(const BenchState& b) { return b.includeStress ? int(scenes().size()) : kNormalScenes; }

BenchState& st(AppState& app) {
    if (!app.bench) app.bench = std::make_shared<BenchState>();
    return *app.bench;
}

double envNum(const char* name, double def) { const char* v = SDL_getenv(name); return v ? std::atof(v) : def; }

const char* whyNot(const AppState& app) {
    if (app.pane) return "Learn or Achievements is open: close it first.";
    if (app.journey.active) return "A Journey is running: stop it first.";
    if (app.shot.requested) return "A picture is being saved: wait a moment.";
    if (app.bench && app.bench->stage == BenchState::Stage::Running) return "The benchmark is already running.";
    return nullptr;
}

void restore(AppState& app, BenchState& b) {
    app.sys = *b.sys; app.view = *b.view; app.atom = *b.atom; app.atomView = *b.atomView;
    app.set = b.set; app.journey = b.journey;
    app.lab = b.lab; app.paused = b.paused; app.vsync = b.vsync;
    app.learn = b.learn; app.atomLearn = b.atomLearn;
    app.showSettings = b.showSettings; app.showAbout = b.showAbout; app.showHistory = b.showHistory; app.showLabLog = b.showLabLog;
    b.sys.reset(); b.view.reset(); b.atom.reset(); b.atomView.reset();
    app.benchRunning = false;
}

std::string whenText(SDL_DateTime& dt) {
    char b[64];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return b;
}

void begin(AppState& app) {
    BenchState& b = st(app);
    // the environment, as it is when it starts (the window is not changed meanwhile)
    app::BenchEnv& e = b.env = app::BenchEnv();
    e.version = appVersion();
    SDL_Time now = 0; SDL_DateTime dt{};
    SDL_GetCurrentTime(&now); SDL_TimeToDateTime(now, &dt, true);
    e.when = whenText(dt);
    const char* s = reinterpret_cast<const char*>(plgl::glGetString(GL_RENDERER)); e.gpu = s ? s : "";
    s = reinterpret_cast<const char*>(plgl::glGetString(GL_VERSION)); e.glVersion = s ? s : "";
    s = reinterpret_cast<const char*>(plgl::glGetString(GL_VENDOR)); e.vendor = s ? s : "";
    const ImGuiIO& io = ImGui::GetIO();
    const float sc = ImGui::GetStyle().FontScaleDpi;
    e.windowW = int(io.DisplaySize.x * io.DisplayFramebufferScale.x + 0.5f); e.windowH = int(io.DisplaySize.y * io.DisplayFramebufferScale.y + 0.5f);
    e.pictureW = int(app.canvasW * sc * io.DisplayFramebufferScale.x + 0.5); e.pictureH = int(app.canvasH * sc * io.DisplayFramebufferScale.y + 0.5);
    if (SDL_Window* w = SDL_GL_GetCurrentWindow()) {
        e.fullscreen = (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
        if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w))) e.refreshHz = m->refresh_rate;
    }
    e.textSize = app.prefs.textSize; e.theme = app.prefs.theme;
    b.warm = envNum("PL_BENCH_WARMUP", app::kBenchWarmupS);          // for automatic checks: shorter times
    b.measure = envNum("PL_BENCH_MEASURE", app::kBenchMeasureS);
    e.warmupS = b.warm; e.measureS = b.measure;
    // keep everything the user has
    b.lab = app.lab; b.paused = app.paused; b.vsync = app.vsync; b.learn = app.learn; b.atomLearn = app.atomLearn;
    b.showSettings = app.showSettings; b.showAbout = app.showAbout; b.showHistory = app.showHistory; b.showLabLog = app.showLabLog;
    b.sys = std::make_unique<orbit::SolarSystem>(app.sys);
    b.view = std::make_unique<orbit::OrbitView>(app.view);
    b.atom = std::make_unique<atom::AtomLab>(app.atom);
    b.atomView = std::make_unique<atom::AtomView>(app.atomView);
    b.set = app.set; b.journey = app.journey;
    b.results.clear(); b.text.clear(); b.note.clear(); b.cancelled = false; b.gpuChecked = false; b.gpuCheck = app::GpuVerdict(); b.gpuReady.clear();
    b.gpu.make();
    b.run = std::make_unique<app::BenchRun>(activeScenes(b), b.warm, b.measure);
    b.stage = BenchState::Stage::Running;
    app.benchRunning = true;
}

void saveAndCopy(AppState& app, BenchState& b) {
    b.text = app::benchResultText(b.env, b.results);
    SDL_SetClipboardText(b.text.c_str());
    SDL_Time now = 0; SDL_DateTime dt{};
    SDL_GetCurrentTime(&now); SDL_TimeToDateTime(now, &dt, true);
    const std::string name = app::benchFileName({dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second});
    std::string why;
    b.noteFailed = true;
    b.note = "The result could not be saved to a file.";
    for (const std::string& folder : shotFolders()) {
        SDL_CreateDirectory(folder.c_str());
        const std::string path = app::joinPath(folder, name);
        if (SDL_SaveFile(path.c_str(), b.text.data(), b.text.size())) { b.note = "Saved to " + path + ". It is also on the clipboard."; b.noteFailed = false; std::printf("Benchmark result: %s\n", path.c_str()); break; }
        why = SDL_GetError();
    }
    if (b.noteFailed && !why.empty()) b.note += " (" + why + ") It is on the clipboard.";
    (void)app;
}

void finish(AppState& app, BenchState& b, bool cancelled) {
    if (b.run) {
        const int n = cancelled ? b.run->doneScenes() : b.run->sceneCount();
        b.results.clear();
        for (int i = 0; i < n; i++) { app::BenchScene r = b.run->result(i); r.name = scenes()[i].info.name; r.what = scenes()[i].info.what; b.results.push_back(r); }
    }
    b.env.cancelled = cancelled;
    b.env.gpuTimer = b.gpu.ok && std::any_of(b.results.begin(), b.results.end(), [](const app::BenchScene& s) { return s.gpuSamples > 0; });
    b.env.gpuReliable = b.env.gpuTimer && b.gpuChecked && b.gpuCheck.reliable;
    b.env.gpuCheck = b.gpuChecked ? b.gpuCheck.why : std::string();
    restore(app, b);
    b.gpu.destroy();
    b.cancelled = cancelled;
    if (cancelled) { b.stage = BenchState::Stage::Closed; b.run.reset(); return; }
    saveAndCopy(app, b);
    b.stage = BenchState::Stage::Done;
    b.run.reset();
}

}  // namespace

void benchmarkButton(AppState& app) {
    const char* why = whyNot(app);
    if (why) ImGui::BeginDisabled();
    if (ImGui::Button("Benchmark...")) { st(app).stage = BenchState::Stage::Ask; }
    if (why) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", why ? why : "Runs fixed scenes and gives numbers to compare between runs and PCs");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("A test of about %d seconds: it measures how fast the app draws, to compare between PCs or versions.", int(scenes().size() * (app::kBenchWarmupS + app::kBenchMeasureS)));
    ImGui::PopStyleColor();
}

void benchmarkTestHook(AppState& app) {
    static int frames = 0;
    const char* v = SDL_getenv("PL_BENCH");
    if (!v || ++frames != 5) return;
    st(app).includeStress = SDL_getenv("PL_BENCH_STRESS") != nullptr;      // for automatic checks: the checkbox
    if (std::string(v) == "start") begin(app); else st(app).stage = BenchState::Stage::Ask;
}

void benchmarkBeginFrame(AppState& app, render::Renderer2D& renderer, int fbW, int fbH) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    BenchState& b = *app.bench;
    if (app.liveFrame) { std::printf("Benchmark stopped: the window was moved or resized\n"); finish(app, b, true); return; }   // the numbers would be wrong
    if (whyNot(app) && std::string(whyNot(app)) != "The benchmark is already running.") { finish(app, b, true); return; }   // something got in the way: stop and give everything back
    if (!b.gpuChecked) {                                 // once, before the first scene
        b.gpuChecked = true;
        b.gpuCheck = checkGpuTimer(renderer, app.canvasW, app.canvasH, fbW, fbH);
        std::printf("GPU timer check: %s: %s\n", b.gpuCheck.reliable ? "reliable" : "NOT reliable", b.gpuCheck.why.c_str());
    }
    const int i = b.run->sceneToSetUp();
    if (i >= 0) { scenes()[i].setup(app); b.run->setUpDone(); }
    app.vsync = false;
}

void benchmarkGpuBegin(AppState& app) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    GpuTimer& g = app.bench->gpu;
    if (!g.ok || g.active >= 0) return;
    const int s = g.ring.acquire();
    if (s < 0) { g.framesSkipped++; return; }            // every slot still waits for its result: this frame is not measured
    plgl::glQueryCounter(g.q[s][1], GL_TIMESTAMP);
    plgl::glBeginQuery(GL_TIME_ELAPSED, g.q[s][0]);
    g.active = s;
}

void benchmarkGpuEnd(AppState& app) {
    if (!app.bench) return;
    GpuTimer& g = app.bench->gpu;
    if (g.active < 0) return;
    plgl::glEndQuery(GL_TIME_ELAPSED);
    plgl::glQueryCounter(g.q[g.active][2], GL_TIMESTAMP);
    g.ring.submitted(g.active);
    g.active = -1;
}

void benchmarkEndFrame(AppState& app, double frameMs, double cpuMs) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    BenchState& b = *app.bench;
    // the GPU results that are ready now (they come a frame or two late), oldest first
    GpuTimer& g = b.gpu;
    b.gpuReady.clear();
    g.ring.collect(
        [&](int s) {
            GLint a = 0, c = 0;
            plgl::glGetQueryObjectiv(g.q[s][0], GL_QUERY_RESULT_AVAILABLE, &a);
            plgl::glGetQueryObjectiv(g.q[s][2], GL_QUERY_RESULT_AVAILABLE, &c);
            return a && c;
        },
        [&](int s) {
            unsigned long long busy = 0, t0 = 0, t1 = 0;
            plgl::glGetQueryObjectui64v(g.q[s][0], GL_QUERY_RESULT, &busy);
            plgl::glGetQueryObjectui64v(g.q[s][1], GL_QUERY_RESULT, &t0);
            plgl::glGetQueryObjectui64v(g.q[s][2], GL_QUERY_RESULT, &t1);
            return GpuSample{busy / 1e6, (t1 >= t0 ? t1 - t0 : 0) / 1e6};
        },
        b.gpuReady);
    for (const GpuSample& r : b.gpuReady) b.run->gpuSample(r.busyMs, r.spanMs);
    b.run->frame(frameMs, cpuMs);
    if (b.run->finished()) finish(app, b, false);
}

void benchmarkWindow(AppState& app) {
    if (!app.bench) return;
    BenchState& b = *app.bench;
    if (b.stage == BenchState::Stage::Closed) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float fs = ImGui::GetFontSize();
    const ImGuiWindowFlags fl = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    const bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (b.stage == BenchState::Stage::Ask) {
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(34 * fs, vp->WorkSize.x * 0.95f), 0));
        bool open = true;
        if (ImGui::Begin("Benchmark###bench", &open, fl)) {
            const int count = activeScenes(b);
            const int secs = int(count * (app::kBenchWarmupS + app::kBenchMeasureS));
            ImGui::TextWrapped("This runs %d scenes one after the other, about %d seconds in all. Each one has %d seconds of warm-up that is not counted, then %d seconds that are counted.",
                               count, secs, int(app::kBenchWarmupS), int(app::kBenchMeasureS));
            ImGui::Spacing();
            for (int i = 0; i < count; i++) ImGui::BulletText("%s", scenes()[i].info.name);
            ImGui::Checkbox("Include stress scenes (2 extra, much heavier)", &b.includeStress);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n\n%s", scenes()[kNormalScenes].info.what, scenes()[kNormalScenes + 1].info.what);
            ImGui::Spacing();
            ImGui::TextWrapped("VSync is switched off during the test, and the picture is drawn at the size it has now. "
                               "Close other programs for the most useful number. Do not move or resize the window: that stops the test. Your settings, the lab and the view are put back when it ends, and Esc stops it. "
                               "Before the first scene the graphics card's timer is checked with a known load (about a second).");
            ImGui::Spacing();
            if (ImGui::Button("Start") && !whyNot(app)) begin(app);
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || esc) b.stage = BenchState::Stage::Closed;
        }
        ImGui::End();
        if (!open) b.stage = BenchState::Stage::Closed;
    } else if (b.stage == BenchState::Stage::Running) {
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12 * fs / 16, vp->WorkPos.y + 12 * fs / 16 + std::max(0.0f, headerHeight())), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("##benchrun", nullptr, fl | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {
            const app::BenchRun& r = *b.run;
            const int shown = std::min(r.scene() + 1, r.sceneCount());
            ImGui::Text("Scene %d of %d: %s", shown, r.sceneCount(), scenes()[std::min(r.scene(), r.sceneCount() - 1)].info.name);
            ImGui::Text("%s  %.1f s", r.measuring() ? "Counting" : "Warming up", r.sceneElapsedS());
            const double fps = app.stats.avgFrame() > 0 ? 1000.0 / app.stats.avgFrame() : 0;
            ImGui::Text("Live: %.0f fps", fps);
            ImGui::ProgressBar(float(r.doneS() / r.totalS()), ImVec2(24 * fs, 0));
            if (ImGui::Button("Cancel") || esc) { ImGui::End(); finish(app, b, true); return; }
        }
        ImGui::End();
    } else if (b.stage == BenchState::Stage::Done) {
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(46 * fs, vp->WorkSize.x * 0.97f), std::min(34 * fs, vp->WorkSize.y * 0.92f)), ImGuiCond_Appearing);
        bool open = true;
        if (ImGui::Begin("Benchmark result###bench", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextWrapped("%s, %s, OpenGL %s", b.env.gpu.c_str(), b.env.vendor.c_str(), b.env.glVersion.c_str());
            ImGui::TextWrapped("Window %d x %d, picture %d x %d pixels%s", b.env.windowW, b.env.windowH, b.env.pictureW, b.env.pictureH, b.env.fullscreen ? ", full screen" : "");
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, vec(look().c.line));      // the header row in the theme's colours (the default is dark on every theme)
            const bool table = ImGui::BeginTable("##benchtable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp);
            if (table) {
                const char* heads[] = {"Scene", "Average", "fps", "1 % low", "Worst", "CPU", "GPU"};
                for (const char* h : heads) ImGui::TableSetupColumn(h);
                ImGui::TableHeadersRow();
                for (const app::BenchScene& s : b.results) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextWrapped("%s", s.name.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.2f ms", s.frames.avgMs);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", s.frames.avgFps);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f fps", s.frames.low1Fps);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f ms", s.frames.worstMs);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f ms", s.cpuAvgMs);
                    ImGui::TableNextColumn(); if (s.gpuSamples <= 0) ImGui::TextDisabled("not available");
                    else if (b.env.gpuReliable && s.gpuProblem.empty()) ImGui::Text("%.2f ms", s.gpuAvgMs);
                    else ImGui::TextDisabled("not reliable");
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, b.noteFailed ? ImVec4(0.9f, 0.35f, 0.3f, 1) : vec(look().c.muted));
            ImGui::TextWrapped("%s", b.note.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("Copy result")) SDL_SetClipboardText(b.text.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Close") || esc) open = false;
            ImGui::TextDisabled("To send it: paste the result into your message to Claude.");
        }
        ImGui::End();
        if (!open) { b.stage = BenchState::Stage::Closed; b.text.clear(); }
    }
}

}  // namespace ui
