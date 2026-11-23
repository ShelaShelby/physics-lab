#include "ui/benchmark.h"
#include "orbit/alphacen.h"
#include "ui/reading.h"
#include "ui/shell.h"
#include "ui/shell_test.h"
#include "ui/style_draw.h"
#include "atom/atom_card_paths.h"
#include "molecule/molecule_card_paths.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/gpu_guard.h"
#include "app/bench.h"
#include "app/frame_cost.h"
#include "app/bench_scenes.h"
#include "app/gpu_timer.h"
#include "app/shell.h"
#include "app/shot.h"
#include "atom/atom_bench.h"
#include "atom/atom_core.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "molecule/density.h"
#include "molecule/molecule.h"
#include "molecule/vibration.h"
#include "orbit/belts.h"
#include "orbit/ring_check.h"
#include "orbit/lines_check.h"
#include "molecule/surface_check.h"
#include "atom/classic_check.h"
#include "atom/nucdens_check.h"
#include "orbit/units.h"
#include "render/renderer2d.h"
#include "render/vk_backend.h"
#include "ui/app_pages.h"
#include "ui/changelog.h"
#include "ui/data_panel.h"
#include "ui/fonts.h"
#include "ui/journey_dates.h"
#include "ui/look.h"
#include "ui/orbit_actions.h"
#include "ui/shot.h"
#include "ui/sysinfo.h"

namespace ui {

// 0.68.1 (test switch): PL_BENCH_CAP seconds instead of the app's 15 s, for a software renderer that fills the clouds in more than 15 s; the app's own value stays 15 s
namespace {
double responseCapS() { static const double v = SDL_getenv("PL_BENCH_CAP") ? std::atof(SDL_getenv("PL_BENCH_CAP")) : app::kBenchResponseCapS; return v; }

constexpr int kStressBeltScale = 8;      // orbit.rocks8: 8 times the rocks of orbit.heaviest
constexpr double kPi = 3.14159265358979323846;
constexpr int64_t kEclipseMs = 1786556760000LL;   // 12 Aug 2026 17:46 UTC: the app's own Moon puts its shadow on Earth (gamma 0.90; tests/test_app.cpp testBenchEclipseDate)

const app::BenchSceneText& sceneText(int i) { return app::benchSceneTexts()[std::size_t(i)]; }
double envNum(const char* name, double def) { const char* v = SDL_getenv(name); return v ? std::atof(v) : def; }
bool envHas(const char* name) { return SDL_getenv(name) != nullptr; }

// ---- 0.56.1: the zoom, distance and field of view of the new scenes (the heaviest of four tried within the controls' range, REPORT 0.56.1) ----
// PL_BENCH_ZOOM (development: the trials) replaces the scene's zoom (the Black Hole Lab's: "distance,fov").
constexpr double kSaturnZoom = 4.1, kEarthZoom = 5.5, kSkyZoom = 0.0;
constexpr double kDensityZoom = 1.6, kSpaceZoom = 4.0;           // the Molecule Lab's zoom factor (its control: 0.3 to 8)
constexpr double kFilmFov = 30;                                   // degrees, at the paper's distance (74.1 GM/c^2)
double zoomOr(double z) { return envHas("PL_BENCH_ZOOM") ? envNum("PL_BENCH_ZOOM", z) : z; }

void commonSetup(AppState& app) {
    app.paused = false;
    app.vsync = false;
    app.pane = 0;
    app.learn = 0; app.atomLearn = 0; app.molLearn = 0; app.bhLearn = 0; app.showSettings = app.showAbout = app.showHistory = false;
    if (app.journey.active) journeyStop(app);
    app.set.realPlanets = false; app.sys.setRealPlanets(false);   // Simple planet physics unless the scene says Real, so the numbers stay comparable
    if (dataOpen()) dataClose();
}

int bodyId(const AppState& app, const char* name) {
    for (const orbit::Body& b : app.sys.bodies) if (b.name == name) return b.id;
    return -1;
}

void orbitHeaviest(AppState& app) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);                                   // the Solar System today
    orbit::ViewSettings& s = app.set;
    s = orbit::ViewSettings();                           // (Planet physics: Simple)
    s.view3D = true; s.trueScale = true; s.fullTrails = true;
    s.moons = s.dwarfs = s.comets = s.belts = s.far = true;
    s.sky = orbit::SkyMode::Full;
    s.cons = orbit::ConsMode::LinesAndNames;
    s.tspeed = units::kTspeedMax;                 // fastest time
    app.view = orbit::OrbitView();
    app.view.yaw = 0.6; app.view.pitch = 1.0; app.view.spin = false;
    setZoom(app, 0.0);
}
void orbitStress(AppState& app) { orbitHeaviest(app); app.sys.belts = orbit::makeBelts(app.sys.epochMs, kStressBeltScale); }

// a planet close up, followed, time stopped: Saturn, or Earth on the eclipse's date
void orbitClose(AppState& app, const char* body, double zoom) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);
    if (std::string(body) == "Earth") jumpToDate(app, kEclipseMs);
    app.set = orbit::ViewSettings();
    app.set.view3D = true; app.set.trueScale = true; app.set.moons = true;
    app.view = orbit::OrbitView();
    app.view.spin = false;
    app.paused = true;
    const int id = bodyId(app, body);
    if (id >= 0) app.view.select({orbit::Selection::Body, id}, true);
    app.view.ox = app.view.oy = 0;
    setZoom(app, zoomOr(zoom));
}
// 0.75.0: Saturn from the closest zoom the camera may come (1.5 radii: orbit_view.cpp zoomCapFor), every extra on, time stopped (orbit.flyby)
void orbitFlyby(AppState& app) {
    orbitClose(app, "Saturn", kSaturnZoom);
    orbit::ViewSettings& s = app.set;
    s.fullTrails = true; s.moons = s.dwarfs = s.comets = s.belts = s.far = s.orbits = true;
    s.sky = orbit::SkyMode::Full; s.cons = orbit::ConsMode::LinesAndNames;
    const int id = bodyId(app, "Saturn");
    double R = 0;
    for (const orbit::Body& b : app.sys.bodies) if (b.id == id) R = b.radius;
    const double W = app.canvasW > 0 ? app.canvasW : 1280, H = app.canvasH > 0 ? app.canvasH : 800;
    const double cap = R > 0 ? std::log10(orbit::focalFor(H) / (1.5 * R) * 34 * units::kSimLength / std::min(W, H)) : kSaturnZoom;
    setZoom(app, zoomOr(cap - 1e-9));               // (PL_BENCH_ZOOM: another zoom, for the trials)
}
void orbitRealFast(AppState& app) {
    orbitHeaviest(app);
    app.set.realPlanets = true; app.set.relativity = true;   // the main loop switches the Solar System to Real (n-body) from this
}
void orbitSky(AppState& app) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);
    orbit::ViewSettings& s = app.set;
    s = orbit::ViewSettings();
    s.view3D = true; s.sky = orbit::SkyMode::Full; s.cons = orbit::ConsMode::LinesAndNames;
    s.moons = s.dwarfs = s.comets = s.belts = s.far = false;   // the sky is the picture
    app.view = orbit::OrbitView();
    app.view.spin = false;
    app.paused = true;
    setZoom(app, zoomOr(kSkyZoom));
}
// 0.73.0: the Alpha Centauri system (orbit.alphacen): the whole system to Proxima's planets and back, the camera turning (the zooms tried: REPORT 0.73.0)
constexpr double kAcZoomFar = -3.6, kAcZoomNear = 4.0, kAcZoomPeriodS = 8.0;   // (4.0: Proxima fills the picture; the hardest of -3.6, -1, 2 and 4 on the box, REPORT 0.73.0)
void orbitAlphaCen(AppState& app) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);
    orbit::ViewSettings& s = app.set;
    s = orbit::ViewSettings();
    s.alphaCen = true;
    s.view3D = true; s.sky = orbit::SkyMode::Full; s.cons = orbit::ConsMode::LinesAndNames;
    s.tspeed = units::kTspeedMax;
    app.view = orbit::OrbitView();
    app.view.select(orbit::Selection{orbit::Selection::ACen, 0, orbit::acen::Proxima}, true);
    app.paused = false;
    setZoom(app, zoomOr(kAcZoomFar));
}
void orbitJourney(AppState& app) {
    commonSetup(app);
    app.lab = 0;
    resetToToday(app);
    app.set = orbit::ViewSettings();
    app.set.view3D = false;
    app.view = orbit::OrbitView();
    journeyStart(app);
}

// ---- the Molecule Lab ----
int moleculeIndex(const char* id) {
    for (int i = 0; i < molecule::count(); i++) if (molecule::get(i).id == id) return i;
    return 0;
}
int mostAtoms(bool needDensity) {
    int best = 0; std::size_t n = 0;
    for (int i = 0; i < molecule::count(); i++) {
        const molecule::Molecule& m = molecule::get(i);
        std::size_t sz = 0;
        if (needDensity && !molecule::dens::embedded(m.id, sz)) continue;
        if (m.atoms.size() > n) { n = m.atoms.size(); best = i; }
    }
    return best;
}
void moleculeSetup(AppState& app, app::BenchKind k) {
    commonSetup(app);
    app.lab = molecule::kLabIndex;
    molecule::MoleculeLab& M = app.molecule;
    M = molecule::MoleculeLab();
    M.set.spin = false; M.set.labels = true;
    const int which = k == app::BenchKind::MoleculeVibDensity ? moleculeIndex("H2O") : mostAtoms(k == app::BenchKind::MoleculeDensity);
    M.choose(which);
    M.resetView();
    if (k == app::BenchKind::MoleculeSpaceVib) {
        M.setStyle(molecule::Style::SpaceFill);
        M.vibSize = 1; M.vibPlaying = true; M.setVib(0);
    } else {
        M.setStyle(molecule::Style::Realistic);
        M.set.realDots = molecule::kRealDots[3]; M.set.realShow = 0; M.set.realPot = true;
        if (k == app::BenchKind::MoleculeVibDensity) {
            int mode = 0;
            while (mode < int(molecule::vib::modes(M.mol().id).size()) && !molecule::dens::vibDensity(M.mol().id, mode)) mode++;
            M.vibPlaying = true; M.setVib(mode);
        }
    }
    M.set.cam.zoom = zoomOr(k == app::BenchKind::MoleculeSpaceVib ? kSpaceZoom : kDensityZoom);
}

// ---- the Black Hole Lab ----
void blackholeScene(AppState& app) {
    commonSetup(app);
    app.lab = blackhole::kLabIndex;
    app.blackhole = blackhole::settingsDefaults();
    blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey);       // with the gas disc, 10 degrees above it
    blackhole::setDistance(app.blackhole, 20);
    app.blackhole.pitch = 80 * kPi / 180;
    blackhole::setSpin(app.blackhole, 0.9);                              // the spinning hole (its fine table made first: turning needs no new one)
    app.blackholeView.makeFineNow(app.blackhole);
}
void blackholeFilm(AppState& app) {
    commonSetup(app);
    app.lab = blackhole::kLabIndex;
    app.blackhole = blackhole::settingsDefaults();
    blackhole::setLook(app.blackhole, blackhole::Look::Film);
    blackhole::paperView(app.blackhole);
    blackhole::setFov(app.blackhole, kFilmFov);
    if (envHas("PL_BENCH_ZOOM")) { double d = 0, f = 0; if (std::sscanf(SDL_getenv("PL_BENCH_ZOOM"), "%lf,%lf", &d, &f) == 2) { blackhole::setDistance(app.blackhole, d); blackhole::setFov(app.blackhole, f); } }
}
void blackholeExtreme(AppState& app) {
    commonSetup(app);
    app.lab = blackhole::kLabIndex;
    app.blackhole = blackhole::settingsDefaults();
    blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey);
    blackhole::setSpin(app.blackhole, 0);
}

// 0.58.0: blackhole.all_light: Light: All light on the spinning hole; PL_BENCH_ZOOM="distance,fov" to try other views (REPORT 0.58.0)
void blackholeAllLight(AppState& app) {
    commonSetup(app);
    app.lab = blackhole::kLabIndex;
    app.blackhole = blackhole::settingsDefaults();
    blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey);
    app.blackhole.allLight = true;
    double d = 20, f = 90;                                               // the hardest of five views tried (REPORT 0.58.0)
    if (envHas("PL_BENCH_ZOOM")) std::sscanf(SDL_getenv("PL_BENCH_ZOOM"), "%lf,%lf", &d, &f);
    blackhole::setDistance(app.blackhole, d); blackhole::setFov(app.blackhole, f);
    app.blackhole.pitch = 80 * kPi / 180;
    blackhole::setSpin(app.blackhole, 0.9);
    app.blackholeView.makeFineNow(app.blackhole);
}

// 0.72.0: blackhole.moving_gas: the moving gas (clump and patches) at the fastest rate, Real, spin 0.9, the hardest view tried (REPORT 0.72.0; closer views
// show more shadow and fewer disc pixels; PL_BENCH_ZOOM="dist,fov" tries another)
void blackholeMovingGas(AppState& app) {
    commonSetup(app);
    app.lab = blackhole::kLabIndex;
    app.blackhole = blackhole::settingsDefaults();
    blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey);
    double d = 10, f = 60;                                               // the hardest of five views tried (REPORT 0.72.0)
    if (envHas("PL_BENCH_ZOOM")) std::sscanf(SDL_getenv("PL_BENCH_ZOOM"), "%lf,%lf", &d, &f);
    blackhole::setDistance(app.blackhole, d); blackhole::setFov(app.blackhole, f);
    app.blackhole.pitch = 80 * kPi / 180;
    blackhole::setSpin(app.blackhole, 0.9);
    app.blackhole.gasMoving = true; app.blackhole.gasRun = true; app.blackhole.gasClump = true; app.blackhole.gasPattern = true;
    blackhole::setGasRate(app.blackhole, blackhole::kGasRateMax);
    app.blackholeView.makeFineNow(app.blackhole);
}

// 0.55.5: blackhole.real_move, by the run's clock: in turn turning round the axis, tilting, coming closer, each followed by a rest, at spin 0.9, then the
// same at spin 0 going back. 0.56.1: the slots are 0.65 of 0.55.8's (the counted time is 0.65 of 8 s).
constexpr double kBhK = app::kBenchMeasureS / 8.0;
constexpr double kBhRest = 1.4 * kBhK;
constexpr double kBhMove[6] = {1 * kBhK, 1 * kBhK, 1 * kBhK, 0.5 * kBhK, 0.5 * kBhK, 0.5 * kBhK};
constexpr double kBhSlotEnd[6] = {4.0 / 3 * kBhK, (4.0 / 3 + 1 + 1.4) * kBhK, (4.0 / 3 + 2 * 2.4) * kBhK, (4.0 / 3 + 2 * 2.4 + 0.6) * kBhK, (4.0 / 3 + 2 * 2.4 + 1.2) * kBhK,
                                  (4.0 / 3 + 2 * 2.4 + 1.8) * kBhK};
static_assert(kBhSlotEnd[5] <= app::kBenchMeasureS, "the Black Hole Lab's slots fit into the counted time");

// ---- 0.56.1: blackhole.film_tilt's check: the picture at rest against the last moving one ----
struct PixelGrab { int w = 0, h = 0; std::vector<unsigned char> rgb; };

// ---- the GPU timer ----
// For every measured frame two numbers: the time the card was busy on it and the span from its first to its last command, waiting included. They come
// from timestamp queries (0.63.0, render/vk_backend.h benchTimer): the frame's command buffer and the card batches submitted for it, their times summed
// (busy), and the span from the first start to the last end; a frame or two late. (0.69.0: OpenGL's GL_TIME_ELAPSED queries are gone with OpenGL.)
SDL_Window* g_window = nullptr;
render::VkBackend* g_vk = nullptr;
int g_checkLoads = 0;                            // 0.63.1: the known loads the timer check drew (the report's start line)

struct GpuTimer {
    static constexpr int kSlots = 8;
    int active = -1;
    bool ok = false, made = false;
    long framesSkipped = 0;                      // frames whose slots were all still waiting (not measured)
    void make() {
        if (made) return;
        made = true;
        ok = g_vk && g_vk->benchTimer(true);              // (0.63.0)
    }
    void destroy() {
        if (!made) return;
        made = false;
        if (g_vk) g_vk->benchTimer(false);
    }
};

struct GpuSample { double busyMs, spanMs; long tag; };   // 0.75.1: tag: the frame it timed (BenchState::gpuFrameNo), so a frame left out of the count is left out of the card's times too

// Is the card's timer believable on this PC? A known heavy load (many see-through full-picture rectangles) is drawn a few times; the timer
// and a clock (up to the end of the wait) must roughly agree. The load is doubled until it takes about 2 ms.
app::GpuVerdict checkGpuTimer(render::Renderer2D& r, double wPts, double hPts, int fbW, int fbH) {
    if (g_vk) {                                          // 0.63.0: a known load drawn into an image of its own: timestamps against a clock
        double qMed = 0, wMed = 0;
        for (int n = 16; n <= 16384; n *= 2) {
            std::vector<double> qs, ws;
            for (int rep = 0; rep < 9; rep++) {
                r.begin(wPts, hPts);
                for (int k = 0; k < n; k++) r.rect(0, 0, wPts, hPts, render::Color{0.5f, 0.5f, 0.5f, 0.02f});
                double gq = -1, gw = -1;
                g_checkLoads++;
                if (!g_vk->benchLoad(r, fbW, fbH, gq, gw)) { app::GpuVerdict v; v.why = "the timestamps gave no result for the known load"; return v; }
                if (rep >= 2) { qs.push_back(gq); ws.push_back(gw); }
            }
            std::sort(qs.begin(), qs.end()); std::sort(ws.begin(), ws.end());
            qMed = qs[qs.size() / 2]; wMed = ws[ws.size() / 2];
            if (wMed >= 2.0) break;
        }
        return app::judgeGpuTimer(qMed, wMed);
    }
    app::GpuVerdict v; v.why = "no Vulkan";
    return v;
}

}  // namespace

// everything the benchmark must give back, and what it gathers
struct BenchState {
    enum class Stage { Closed, Ask, Running, Done } stage = Stage::Closed;
    std::unique_ptr<app::BenchRun> run;
    // ---- the user's state, put back at the end (or when cancelled) ----
    int lab = 0, pane = 0, learnBack = -1;
    bool paused = false, vsync = true;
    int learn = 0, atomLearn = 0, molLearn = 0, bhLearn = 0;
    std::string learnJump, atomLearnJump, molLearnJump, bhLearnJump;
    blackhole::Settings blackhole;
    bool showSettings = false, showAbout = false, showHistory = false;
    std::unique_ptr<orbit::SolarSystem> sys;
    std::unique_ptr<orbit::OrbitView> view;
    std::unique_ptr<atom::AtomLab> atom;
    std::unique_ptr<atom::AtomView> atomView;
    std::unique_ptr<molecule::MoleculeLab> molecule;
    std::unique_ptr<app::Prefs> prefs;
    std::shared_ptr<DataBenchKeep> data;
    orbit::ViewSettings set;
    Journey journey;
    double tempK = 293.15;
    std::string dateNote;
    char dateText[16] = "", timeText[8] = "";
    // ---- results ----
    app::BenchEnv env;
    std::vector<app::BenchScene> results;
    std::vector<app::BenchResponse> responses;
    std::vector<app::BenchIdle> idle;
    std::string text, note;
    std::vector<std::string> lines, paths;   // per scene: its own line (the Atom Lab's settings); the path that ran
    std::vector<long> pics;                  // per scene: new pictures while counted (-1: every frame is one)
    std::vector<double> picsS;               // 0.67.0: per scene: the counted seconds the view moved (the Black Hole Lab's: its view settings changed that frame)
    double bhViewKey[6] = {0, 0, 0, 0, 0, 0};
    std::vector<std::string> picsBy;
    std::vector<std::vector<std::string>> notes;
    long picLast = -1;
    // the Black Hole Lab's real_move: its clock, the movement of the frame being drawn (0 turn, 1 tilt, 2 distance; moving or at rest), the counters of the last frame
    double bhLastU = -1e9, bhRestS = -1;
    int bhKind = -1;
    bool bhMoving = false, bhHave = false;
    long bhCount[6] = {0, 0, 0, 0, 0, 0};
    double bhMoveS[6] = {0, 0, 0, 0, 0, 0};
    std::uint64_t bhPic = 0;
    long bhCard = 0, bhSkyPics = 0, bhDiscFills = 0;
    std::map<int, app::BenchBlackHole> bhByScene;
    // film_tilt: the last moving picture and the one at rest (read back), and whether the next frame reads one back (1 moving, 2 at rest)
    PixelGrab grabMoving, grabRest;
    int grabNext = 0, shotFor = -1, shotK = 0;
    int wFrames = 0;                         // frames since the wait's action
    bool grabSkip = false;                   // a development picture was saved this frame (not counted)
    bool filmStopped = false, filmDone = false, filmCompared = false;   // (0.63.1: filmCompared: the two pictures compared once)
    // the scene's own clock (s from its set-up, by the run's clock) and phase counters
    double lastU = 0;
    double split[2][3] = {{0, 0, 0}, {0, 0, 0}};   // atom.nucleus_density: per half (still, turning): frames, ms, cpu ms
    long gridsAt[2] = {0, 0};
    std::string pickedGuide, dataText;
    float boxX = 0, boxY = 0, boxW = 0, boxH = 0;  // the running box (ImGui pixels), left out of film_tilt's comparison
    double peakMB = -1, lastMemS = -1;
    render::Renderer2D* R = nullptr;
    bool noteFailed = false;
    GpuTimer gpu;
    std::vector<GpuSample> gpuReady;          // the card's results that have come back and wait for their frame's decision (counted or left out)
    long gpuFrameNo = 0, gpuFrameMarked = -1; // 0.75.1: the frames named so far, and the one marked this frame
    std::vector<std::pair<long, bool>> frameCounted;   // the last frames' decisions (tag, counted)
    app::GpuVerdict gpuCheck;
    bool gpuChecked = false;
    // 0.63.1: the benchmark's own start (the owner's PC: 5-7 s that looked frozen): measured part by part for the report's header
    double startT0 = -1, beginMs = -1, checkMs = -1, setupMs = -1, swapMs = 0, toSceneMs = -1;
    int prepFrames = 0, checkLoads = 0;
    bool grabPrev = false;                    // 0.63.1: the frame before read the picture back
    std::vector<int> ownLeft;                 // 0.68.1: per scene, the counted-time frames left out because the benchmark's own numbers took over 1 ms of them
    std::vector<double> ownLeftMs;            //   and those milliseconds
    bool allScenes = false;                   // the checkbox "All scenes"
    std::vector<int> list;                    // the scenes of this run (indexes into benchSceneTexts()), fixed when it starts
    int sceneFrame = 0, measFrame = -1;       // frames since the scene was set up; counted frames so far (-1: still warming up)
    double warm = app::kBenchWarmupS, measure = app::kBenchMeasureS;
    bool cancelled = false;
    // ---- 0.56.1, Parts 2 and 3: after the scenes, the waits, then the labs left alone ----
    int phase = 0;                            // 0 scenes, 1 waits, 2 rest
    std::vector<int> waits, rests;            // the waits and the rests of this run (indexes into waitDefs() / restDefs())
    int wAt = 0, wTry = 0, wStage = 0;        // the wait (index into waits), its try, and its stage: 0 prepare, 1 settling, 2 acted (waiting for the finished picture), 3 pause
    double wStageS = 0, wWorst = 0;           // seconds in the stage (the frames' times summed); the worst frame since the action
    Uint64 wT0 = 0;
    int rAt = 0, rStage = 0;                  // the rest (index into rests) and its stage: 0 prepare, 1 settling, 2 counting
    double rStageS = 0, rCpu0 = 0, rGpuSum = 0;
    Uint64 rT0 = 0;
    long rRedraw0 = 0;
    int rGpuN = 0;
    app::BenchIdle rNow;
};

namespace {

// the scene's clock: seconds from its set-up by the run's own clock (the frames' times summed), the warm-up included
double sceneU(const BenchState& b) { return b.run->measuring() ? b.run->warmupS() + b.run->sceneElapsedS() : b.run->sceneElapsedS(); }

void blackholeFrame(AppState& app, BenchState& b) {
    // 0.55.8: by the run's own clock; the warm-up at rest; then six slots: turning, tilting, distance at spin 0.9, then the same at spin 0 going back
    const double u = b.run->measuring() ? b.run->sceneElapsedS() : b.run->sceneElapsedS() - b.run->warmupS();
    const double dt = std::clamp(u - b.bhLastU, 0.0, 0.1);
    b.bhLastU = u;
    if (u < 0) { b.bhKind = -1; b.bhMoving = false; return; }
    int k = 0;
    while (k < 5 && u >= kBhSlotEnd[k]) k++;
    const double start = k == 0 ? 0 : kBhSlotEnd[k - 1];
    b.bhKind = k;
    b.bhMoving = u - start < kBhMove[k] && u < kBhSlotEnd[5];
    b.bhRestS = b.bhMoving ? -1 : u - start - kBhMove[k];
    const double spin = k < 3 ? 0.9 : 0.0;
    if (app.blackhole.spin != spin) blackhole::setSpin(app.blackhole, spin);
    if (!b.bhMoving) return;
    b.bhMoveS[k] += dt;
    const double dir = k < 3 ? 1 : -1, back = k < 3 ? 1.0 : kBhMove[k - 3] / kBhMove[k];   // (the way back at spin 0 in its shorter time)
    if (k % 3 == 0) blackhole::turn(app.blackhole, 0.2 * dir * back * dt, 0);    // 0.2 rad a second round the axis
    else if (k % 3 == 1) blackhole::turn(app.blackhole, 0, 0.1 * dir * back * dt);   // 0.1 rad a second up or down
    else blackhole::setDistance(app.blackhole, app.blackhole.dist * std::exp(-0.05 * dir * back * dt));   // 5 % a second closer or farther
}

// 0.55.5: the Black Hole Lab's numbers of the frame just drawn, while the scene is counted (the counters follow during the warm-up too)
void sampleBlackHole(AppState& app, BenchState& b, const render::Renderer2D& R) {
    const blackhole::ViewStats& s = app.blackholeView.stats();
    const long c[6] = {s.spinCoarseFills + s.tableFills, s.spinBins, s.starFills, s.lookFills, R.glareStats().made, s.spinFineFills};
    if (b.bhHave && b.bhKind >= 0 && b.run->measuring()) {
        app::BenchBlackHole& h = b.bhByScene[b.run->scene()];
        app::BenchBhMove& m = h.move[b.bhKind];
        if (s.picture != b.bhPic && b.bhMoving) m.pictures++;            // 0.55.6: new pictures while moving
        if (s.skyCard && R.skyStats().draws != b.bhSkyPics) {             // 0.56.0: the card's new pictures of sky and disc (this frame's), the pass's time, the resolution
            if (b.bhMoving) { m.skyPictures++; m.scaleMin = std::min(m.scaleMin, R.skyStats().scale); if (R.skyStats().scale >= 1) m.fullPictures++; }
            if (R.skyStats().fullMs >= 0) h.skyGpu.add(R.skyStats().fullMs);
        }
        if (s.discFills != b.bhDiscFills) m.disc.add(s.discMs);
        m.seconds = b.bhMoveS[b.bhKind];
        m.restS = (b.bhKind == 0 ? kBhSlotEnd[0] : kBhSlotEnd[b.bhKind] - kBhSlotEnd[b.bhKind - 1]) - kBhMove[b.bhKind];
        if (s.cardFills != b.bhCard) h.card.add(s.cardMs);
        if (b.bhMoving) {
            m.frames++;
            m.frameSum += s.frameMs;
            m.frameWorst = std::max(m.frameWorst, s.frameMs);
            if (s.behind) m.behindFrames++;
        }
        m.behindWorst = std::max(m.behindWorst, s.behindMs);
        // 0.55.8: at rest after moving at spin: the first frame that shows the fine table of the settings (not behind): its time after the stop
        if (!b.bhMoving && b.bhRestS >= 0 && app.blackhole.spin > 0 && m.sharpMs < 0 && s.spinFine && !s.behind) m.sharpMs = 1000 * b.bhRestS;
        if (c[0] != b.bhCount[0]) m.coarse.add(app.blackhole.spin > 0 && app.blackhole.bend ? s.spinCoarseMs : s.tableMs);
        if (c[1] != b.bhCount[1]) m.bins.add(s.spinStarsMs);
        if (c[2] != b.bhCount[2]) m.stars.add(s.starsMs);
        if (c[3] != b.bhCount[3]) m.look.add(s.lookMs);
        if (c[4] != b.bhCount[4]) { h.glare.add(R.glareStats().veilMs); h.glareWaitWorst = std::max(h.glareWaitWorst, R.glareStats().waitMs); }
        if (c[5] != b.bhCount[5] && !s.spinCard) h.fine.add(s.spinFineMs);   // (0.55.6: a table the card filled is counted as the card's fill, not the fine table)
    }
    std::copy(c, c + 6, b.bhCount);
    b.bhPic = s.picture; b.bhCard = s.cardFills; b.bhSkyPics = R.skyStats().draws; b.bhDiscFills = s.discFills;
    {   app::BenchBlackHole& h = b.bhByScene[b.run->scene()];               // 0.55.6: which path filled the spinning hole's table, and its start-up check
        const blackhole::BlackHoleView& v = app.blackholeView;
        const blackhole::CardCheck& ck = v.cardCheck();
        char buf[320];
        if (!v.gpuPaths) h.path = "the processor (GPU light paths switched off)";
        else if (v.cardState() == 1) { std::snprintf(buf, sizeof buf, "the graphics card (start-up check passed: %d edges, %d rays against the processor's; largest differences: direction %.1e rad, b %.1e, u %.1e; %.0f ms)",
                                                     ck.edges, ck.rays, ck.dir, ck.b, ck.u, ck.ms); h.path = buf; }
        else if (v.cardState() == -1) h.path = "the processor, because " + v.cardWhy();
        else h.path = "not decided yet";
        const blackhole::SkyCheck& sk = v.skyCheck();                     // 0.56.0: who drew the sky and disc, and why
        if (!v.gpuSky) h.skyPath = "the processor's table (GPU sky per pixel switched off)";
        else if (v.skyState() == 1) { std::snprintf(buf, sizeof buf, "the graphics card, every pixel from the closed formulas (start-up check passed: far direction %.1e / %.1e rad, disc r %.1e / %.1e, benchmark / Film)",
                                                    sk.m[0].dir, sk.m[1].dir, sk.m[0].r, sk.m[1].r); h.skyPath = buf; }
        else if (v.skyState() == -1) h.skyPath = "the processor's table, because " + v.skyWhy();
        else h.skyPath = "not decided yet";
        h.lookPath = s.lookCard ? "the graphics card (reduced in the same frame)" : "the processor's scan";
    }
    b.bhHave = true;
}

// ---- the scenes' set-up, their frames, their new pictures and paths (0.56.1) ----
void setupScene(AppState& app, BenchState& b, int idx) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    switch (t.kind) {
        case K::OrbitHeaviest: orbitHeaviest(app); break;
        case K::OrbitRocks8: orbitStress(app); break;
        case K::OrbitHeaviestCpuRocks: orbitHeaviest(app); app.set.gpuRocks = false; break;
        case K::OrbitRocks8CpuRocks: orbitStress(app); app.set.gpuRocks = false; break;
        case K::OrbitSaturn: orbitClose(app, "Saturn", kSaturnZoom); break;
        case K::OrbitEarth: orbitClose(app, "Earth", kEarthZoom); break;
        case K::OrbitRealFast: orbitRealFast(app); break;
        case K::OrbitSky: orbitSky(app); break;
        case K::OrbitJourney: orbitJourney(app); break;
        case K::OrbitAlphaCen: orbitAlphaCen(app); break;
        case K::OrbitFlyby: orbitFlyby(app); break;
        case K::Atom:
            commonSetup(app);
            app.lab = 1;
            atom::benchSetup(app.atom, t.atomWhich, app.canvasW, app.canvasH);
            app.atom.hold = true;
            if (envHas("PL_BENCH_ZOOM") && t.atomWhich >= atom::kBenchAllEnlarged) app.atom.setParam("zoom", envNum("PL_BENCH_ZOOM", 0) + (t.atomWhich == atom::kBenchNucleusDensity ? app.atom.zoomForNucleus() : 0));
            app.tempK = 293.15;                          // the Temperature card's little box is drawn every frame: the same temperature for every run
            break;
        case K::MoleculeDensity: case K::MoleculeVibDensity: case K::MoleculeSpaceVib: moleculeSetup(app, t.kind); break;
        case K::BlackHole: blackholeScene(app); break;
        case K::BlackHoleFilmTilt: blackholeFilm(app); break;
        case K::BlackHoleExtreme: blackholeExtreme(app); break;
        case K::BlackHoleAllLight: blackholeAllLight(app); break;
        case K::BlackHoleMovingGas: blackholeMovingGas(app); break;
        case K::UiGuideScroll: {
            commonSetup(app);
            // the longest Learn guide (measured in 0.56.1 at 1280 x 800: the Orbit Lab's timeline, 7,157 px of scrolling; the next the Orbit Lab's "What am I looking
            // at?", 6,221; PL_BENCH_GUIDE="lab:guide" measures another)
            int lab = 0, g = 3;
            if (const char* s = SDL_getenv("PL_BENCH_GUIDE")) std::sscanf(s, "%d:%d", &lab, &g);
            app.lab = lab;
            if (lab == 0) app.learn = g; else if (lab == 1) app.atomLearn = g; else if (lab == molecule::kLabIndex) app.molLearn = g; else app.bhLearn = g;
            b.pickedGuide = std::to_string(lab) + ":" + std::to_string(g);
            break;
        }
        case K::UiDataPlot: {
            commonSetup(app);
            app.atom.p = atom::AtomParams::appDefaults();
            app.atom.restart();
            int bestLab = 0, bestRows = -1;
            for (int lab = 0; lab < 3; lab++) { app.lab = lab; const int r = dataBenchLargest(app, nullptr, nullptr); if (r > bestRows) { bestRows = r; bestLab = lab; } }   // the largest table of the labs' tables
            app.lab = bestLab;
            b.dataText = std::string("Data panel over the ") + app::labName(bestLab) + ": " + dataBenchShow(app);
            break;
        }
        case K::UiSettingsOpen:
            orbitHeaviest(app);
            app.showSettings = true;
            app.prefs.shellPanels = app::shell::withPanelState(app.prefs.shellPanels, std::string(app::labKey(app.lab)) + ".controls", false);
            app.prefs.shellPanels = app::shell::withPanelState(app.prefs.shellPanels, std::string(app::labKey(app.lab)) + ".ctl.pick", false);
            break;
    }
}

// the scene's frame: what moves (by the scene's clock u, s since its set-up; dt the step since the last frame)
void frameScene(AppState& app, BenchState& b, int idx, double u, double dt) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    const double warm = b.run->warmupS(), meas = b.measure;
    switch (t.kind) {
        case K::OrbitSaturn: case K::OrbitSky:
            app.view.yaw = 0.6 + 0.5 * u;                                        // turning 0.5 rad a second, tilting with it
            app.view.pitch = 1.0 + 0.3 * std::sin(0.7 * u);
            break;
        case K::OrbitFlyby:                                                      // 0.75.0: turning, and tilting from 1 to 53 degrees above the ecliptic (through the ring plane)
            app.view.yaw = 0.6 + 0.5 * u;
            app.view.pitch = 1.1 + 0.45 * std::sin(0.9 * u);
            break;
        case K::OrbitAlphaCen:                                                   // 0.73.0: the whole system to Proxima's planets and back (log zoom), turning and tilting
            app.view.yaw = 0.6 + 0.5 * u;
            app.view.pitch = 1.0 + 0.3 * std::sin(0.7 * u);
            if (!envHas("PL_BENCH_ZOOM")) setZoom(app, 0.5 * (kAcZoomFar + kAcZoomNear) - 0.5 * (kAcZoomNear - kAcZoomFar) * std::cos(2 * kPi * u / kAcZoomPeriodS));
            break;
        case K::OrbitEarth: {                                                    // turning back and forth round the sunlit side, where the Moon's shadow falls
            // the camera on the line from Earth's centre towards the Moon (the shadow falls near its foot): orbit_view.cpp's viewer sits along
            // (sin yaw sin pitch, cos yaw sin pitch, cos pitch)
            // aimed at the point where the Moon's shadow axis (the line from the Sun's centre through the Moon's) meets Earth's sunlit side
            orbit::Vec3 n{0, 0, 1};
            for (const orbit::Body& e : app.sys.bodies) {
                if (e.name != "Earth") continue;
                const orbit::Vec3 M = e.pos + app.sys.moonGeocentric();
                const orbit::Vec3 ax = M * (1 / length(M));
                const orbit::Vec3 off = ax * dot(ax, e.pos) - e.pos;              // from Earth's centre to the axis, at right angles to it
                const double h2 = e.radius * e.radius - dot(off, off);
                const orbit::Vec3 p = off - ax * std::sqrt(std::max(0.0, h2));     // the axis's foot on the side facing the Sun
                n = p * (1 / std::max(1.0, length(p)));
            }
            app.view.yaw = std::atan2(n.x, n.y) + 0.35 * std::sin(0.6 * u);
            app.view.pitch = std::acos(std::clamp(n.z, -1.0, 1.0)) + 0.1 * std::sin(0.8 * u);
            break;
        }
        case K::OrbitJourney:
            if (!app.journey.active) journeyStart(app);                         // round again
            if (!app.journey.moving) app.journey.t = 1e9;                       // the holds at the stops are skipped: the next frame flies on
            else for (double left = 3 * dt; left > 1e-9; left -= 0.1) journeyTick(app, std::min(0.1, left));   // 4 times its own speed (with the main loop's tick)
            break;
        case K::Atom:
            app.atom.hold = true;                                               // the decay clock held: the nucleus stays the one the scene names (oganesson-294 lives about a millisecond)
            atom::benchFrame(app.atom, t.atomWhich, b.sceneFrame, b.measFrame);
            if (t.atomWhich >= atom::kBenchAllEnlarged && t.atomWhich <= atom::kBenchJumpAll) app.atom.s.yaw = 0.8 * u;   // turning 0.8 rad a second, as a steady drag
            if (t.atomWhich == atom::kBenchNucleusDensity) {
                const double half = warm + 0.5 * meas;
                if (u > half) app.atom.s.oyaw = 0.8 * (u - half);              // still for the first half of the counted time, then turning
            }
            break;
        case K::MoleculeDensity: case K::MoleculeSpaceVib: case K::MoleculeVibDensity:
            if (t.kind != K::MoleculeVibDensity) molecule::spin(app.molecule.set.cam, 0.8 * dt);
            if (t.kind != K::MoleculeDensity) { app.molecule.vibPlaying = false; app.molecule.vibT = u; }   // the swing by the scene's clock (the lab's own Play skips frames over 0.25 s)
            break;
        case K::BlackHole: blackholeFrame(app, b); break;
        case K::BlackHoleFilmTilt: {
            // three tilts 6 degrees down through the disc's plane and back (0.11 rad below the paper's view's 3.4 degrees above it), in the first 60 % of the counted
            // time; then at rest. The last moving frame and the first finished frame at rest are read back.
            const double m = u - warm, moveT = 0.6 * meas;
            blackhole::Settings& s = app.blackhole;
            const double p0 = blackhole::filmChoices().camTheta, amp = (kPi / 2 - p0) + 6 * kPi / 180;
            if (m < 0) break;
            if (m < moveT) { s.pitch = p0 + amp * std::pow(std::sin(kPi * 3 * m / moveT), 2.0); b.filmStopped = false; }
            else {
                if (!b.filmStopped) { b.filmStopped = true; s.pitch = p0; b.grabNext = 1; }   // this frame is the last that moved: it is read back
                else if (!b.filmDone && b.grabNext == 0 && app.blackholeView.settled() && b.R && !b.R->glareBusy()) { b.grabNext = 2; b.filmDone = true; }
            }
            break;
        }
        case K::BlackHoleExtreme: {
            const double m = u - warm, half = 0.4 * meas;
            blackhole::Settings& s = app.blackhole;
            if (m < 0) break;
            if (m < half) blackhole::setSpin(s, blackhole::kSpinTop * std::sin(kPi * m / half));      // the spin dragged from 0 to its top and back
            else {
                blackhole::setSpin(s, blackhole::kSpinTop);
                double d = blackhole::kDistMin, f = blackhole::kFovMax;
                if (envHas("PL_BENCH_ZOOM")) std::sscanf(SDL_getenv("PL_BENCH_ZOOM"), "%lf,%lf", &d, &f);
                blackhole::setDistance(s, d); blackhole::setFov(s, f);
                blackhole::turn(s, 0.3 * dt, 0.15 * std::cos(1.3 * (m - half)) * dt);   // turning and tilting
            }
            break;
        }
        case K::BlackHoleMovingGas: {                                         // 0.72.0: turning slowly round the axis (0.15 rad a second), as a user looks round
            if (u - warm < 0) break;
            blackhole::turn(app.blackhole, 0.15 * dt, 0.0);
            break;
        }
        case K::BlackHoleAllLight: {                                          // 0.58.0: turning 0.3 rad a second, tilting 12 degrees each way through the disc's plane
            const double m = u - warm;
            if (m < 0) break;
            blackhole::turn(app.blackhole, 0.3 * dt, 0.0);
            app.blackhole.pitch = kPi / 2 + 12 * kPi / 180 * std::sin(1.3 * m);
            break;
        }
        case K::UiGuideScroll: {
            ImGuiWindow* w = ImGui::FindWindowByName("##reading");               // (the page drawn last frame: its scroll range)
            const float mx = w ? w->ScrollMax.y : 0;
            if (envHas("PL_BENCH_SLOW") && b.sceneFrame % 20 == 0) std::printf("PLBENCH-GUIDE %s scroll range %.0f px (window %d, frame %d)\n", b.pickedGuide.c_str(), mx, w ? 1 : 0, b.sceneFrame);   // (which guide is longest)
            if (mx > 0) {
                const double f = u < warm ? 1 - u / warm : std::clamp((u - warm) / meas, 0.0, 1.0);   // back to the top in the warm-up, then down at a steady rate
                readingScrollTo(static_cast<float>(f * mx));
            }
            break;
        }
        case K::UiDataPlot:
            if (app.lab == 1) app.atom.s.yaw = 0.8 * u;                         // the lab's picture turning behind the panel, as a drag does
            else if (app.lab == molecule::kLabIndex) molecule::spin(app.molecule.set.cam, 0.8 * dt);
            break;
        case K::UiSettingsOpen:
            if (ImGuiWindow* w = ImGui::FindWindowByName("##shellControls")) {
                const double f = 0.5 - 0.5 * std::cos(2 * kPi * u / 2.6);      // down and up every 2.6 s
                ImGui::SetScrollY(w, static_cast<float>(f * w->ScrollMax.y));
            }
            break;
        default: break;
    }
}

// new pictures: a counter that goes up with each new picture, for the scenes whose picture can be older than the frame; -1: every frame draws one
long pictureCounter(AppState& app, const render::Renderer2D& R, int idx) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    if (t.kind == K::BlackHole || t.kind == K::BlackHoleFilmTilt || t.kind == K::BlackHoleExtreme || t.kind == K::BlackHoleAllLight || t.kind == K::BlackHoleMovingGas) {
        const blackhole::ViewStats& s = app.blackholeView.stats();
        return s.skyCard ? R.skyStats().draws : static_cast<long>(s.picture);   // the card's sky and disc, else the worker's finished pictures
    }
    if (t.kind == K::Atom && (t.atomWhich == 14 || t.atomWhich == 27)) return app.atomView.sliceContoursMade();   // the Slice view's contour
    return -1;
}
std::string picturesBy(AppState& app, int idx) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    if (t.kind == K::BlackHole || t.kind == K::BlackHoleFilmTilt || t.kind == K::BlackHoleExtreme || t.kind == K::BlackHoleAllLight || t.kind == K::BlackHoleMovingGas) return app.blackholeView.stats().skyCard ? "graphics card" : "processor";
    if (t.kind == K::Atom) return app.atom.gpuOutline && app.computeOk ? "graphics card" : "processor";
    return "";
}
std::string scenePath(AppState& app, BenchState& b, int idx, int runIdx) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    switch (t.kind) {
        case K::OrbitHeaviest: case K::OrbitRocks8: case K::OrbitHeaviestCpuRocks: case K::OrbitRocks8CpuRocks: case K::OrbitRealFast: case K::UiSettingsOpen:
            return "the belts' rocks: " + std::string(app.rocksOk && app.set.gpuRocks ? "the graphics card (Vulkan rock compute)" :   // (0.63.1: the API in use)
                                                      !app.rocksOk ? "the processor (no Vulkan rock compute on this PC)" : "the processor (GPU rocks switched off for this scene)") +
                   "; " + orbit::linesCardPathText(g_vk != nullptr);   // 0.68.0
        case K::OrbitSaturn: return orbit::ringCardPathText(g_vk);   // 0.64.0
        case K::OrbitFlyby: return orbit::ringCardPathText(g_vk) + "; " + orbit::linesCardPathText(g_vk != nullptr);   // 0.75.0
        case K::OrbitAlphaCen: return "the Alpha Centauri orbits: " + orbit::linesCardPathText(g_vk != nullptr);   // 0.73.0
        case K::Atom: {
            const atom::AtomLab& L = app.atom;
            if (t.atomWhich == atom::kBenchSliceSuper)                      // 0.64.0: the Slice's card program takes an orthogonal superposition too (an overlapping pair: the processor)
                return L.overlapPair() ? "the Slice's dots: the processor (an overlapping pair: its attempts have no fixed numbering for the card)" : atom::atomCardPaths().pathText(0, L, app.computeOk);
            if (t.atomWhich == 14 || t.atomWhich == 27) return atom::atomCardPaths().pathText(0, L, app.computeOk) + "; the outline's contour: " + std::string(L.gpuOutline && app.computeOk ? "the graphics card (Vulkan compute)" :
                                                                                                                               !app.computeOk ? "the processor (no Vulkan compute on this PC)" : "the processor (GPU outline switched off for this scene)");
            if (L.manyOn() && L.p.cstyle == "Glow" && L.s.trans) {         // 0.57.0: a jump: the card's buffer only with the card's groups
                const bool card = L.gpuCloud && L.gpuJump && atom::atomCardPaths().state(1) == 1;
                return std::string("the every-electron cloud: ") + (card ? "drawn from a buffer on the graphics card; " : !L.gpuCloud ? "the processor turns, sorts and sends every dot (GPU cloud switched off for this scene); "
                                                                         : "the processor turns, sorts and sends every dot of both levels; ") + atom::atomCardPaths().pathText(1, L, app.computeOk);
            }
            if (t.atomWhich == atom::kBenchAllClassic) return atom::classicCardPathText(g_vk != nullptr);   // 0.64.0
            if (t.atomWhich == atom::kBenchNucleusDensity) return atom::nucDensCardPathText(g_vk != nullptr);   // 0.67.0
            if (t.atomWhich == atom::kBenchSideAll || t.atomWhich == atom::kBenchSideAllClassic) return atom::classicCardPathText(g_vk != nullptr, "the side view's dots");   // 0.65.0
            if (t.atomWhich == atom::kBenchSliceAll)                        // 0.65.0
                return "the Slice's dots: made by the processor (the many-electron sampler); " + atom::classicCardPathText(g_vk != nullptr, "drawn");
            if (t.atomWhich == atom::kBenchSliceDirac)
                return "the Slice's dots: made by the processor (the Dirac state's own sampler); " + atom::classicCardPathText(g_vk != nullptr, "drawn");
            if (t.atomWhich == atom::kBenchSliceGlowUltra || t.atomWhich == atom::kBenchSliceClassicUltra)
                return atom::atomCardPaths().pathText(0, L, app.computeOk) + "; " + atom::classicCardPathText(g_vk != nullptr, "drawn");
            if (L.manyOn() && L.p.cstyle == "Glow") return "the every-electron cloud: " + std::string(L.gpuCloud ? "drawn from a buffer on the graphics card" : "the processor turns, sorts and sends every dot (GPU cloud switched off for this scene)");
            if (L.p.super == "On" || L.s.trans) return "the colour groups: " + app::computePathText(app.glMajor, app.glMinor, app.computeOk, L.gpuCompute);
            return "";
        }
        case K::MoleculeDensity: case K::MoleculeVibDensity:          // 0.57.1
            return molecule::molCardPaths().pathText(app.moleculeView.gpuCloud, app.computeOk) + "; " + (molecule::surfaceCardState() == 1 || g_vk ? molecule::surfaceCardPathText(g_vk != nullptr) :   // 0.68.0
                   std::string("the surface: the processor (its triangles ") + (t.kind == K::MoleculeVibDensity ? "made again for each new phase of the swing, and sorted and coloured each frame)" : "sorted again and coloured each frame as the camera turns)"));
        case K::BlackHole: case K::BlackHoleFilmTilt: case K::BlackHoleExtreme: case K::BlackHoleAllLight: case K::BlackHoleMovingGas: {
            const auto it = b.bhByScene.find(runIdx);
            if (it != b.bhByScene.end() && !it->second.skyPath.empty()) return "the sky and disc: " + it->second.skyPath;
            const blackhole::BlackHoleView& v = app.blackholeView;
            if (!v.gpuSky) return "the sky and disc: the processor's table (GPU sky per pixel switched off)";
            if (v.skyState() == 1) return "the sky and disc: the graphics card, every pixel from the closed formulas (its start-up check passed)";
            if (v.skyState() == -1) return "the sky and disc: the processor's table, because " + v.skyWhy();
            return "the sky and disc: not decided yet";
        }
        default: return "";
    }
}
std::string sceneLine(AppState& app, BenchState& b, int idx) {
    const app::BenchSceneText& t = sceneText(idx);
    using K = app::BenchKind;
    char z[200];
    switch (t.kind) {
        case K::Atom: return atom::benchShortText(app.atom);
        case K::OrbitSaturn: case K::OrbitEarth: case K::OrbitSky: case K::OrbitFlyby:
            std::snprintf(z, sizeof z, "Orbit Lab: zoom %.2f (log10), picture %.0f x %.0f points", app.set.zoom, app.canvasW, app.canvasH);
            return z;
        case K::OrbitAlphaCen:
            std::snprintf(z, sizeof z, "Orbit Lab, Alpha Centauri: zoom %.2f (log10) now, %.1f to %.1f every %.0f s, picture %.0f x %.0f points", app.set.zoom, kAcZoomFar, kAcZoomNear, kAcZoomPeriodS, app.canvasW, app.canvasH);
            return z;
        case K::MoleculeDensity: case K::MoleculeVibDensity: case K::MoleculeSpaceVib:
            std::snprintf(z, sizeof z, "Molecule Lab: %s (%zu atoms), zoom x%.2f%s", app.molecule.mol().id.c_str(), app.molecule.mol().atoms.size(), app.molecule.set.cam.zoom,
                          app.molecule.vibOn() ? (", vibration mode " + std::to_string(app.molecule.vibMode + 1)).c_str() : "");
            return z;
        case K::BlackHoleFilmTilt: case K::BlackHoleExtreme: case K::BlackHoleAllLight: case K::BlackHoleMovingGas:
            std::snprintf(z, sizeof z, "Black Hole Lab: distance %.1f GM/c\xC2\xB2, field of view %.0f\xC2\xB0, pitch %.3f rad (at the set-up)", app.blackhole.dist, app.blackhole.fovDeg, app.blackhole.pitch);
            return z;
        case K::UiGuideScroll: return "Learn guide " + b.pickedGuide + " (lab:guide)";
        case K::OrbitJourney:
            std::snprintf(z, sizeof z, "Orbit Lab: the Journey started at Earth, zoom %.2f", app.set.zoom);
            return z;
        case K::UiDataPlot: return b.dataText;
        default: return "";
    }
}

// blackhole.film_tilt's comparison: a pixel counts as disc where its brightest channel is at least a fifth of the brightest pixel of the picture at rest; the
// pixels whose kind differs between the two pictures, and the largest 4-connected patch of them (the running box left out)
std::vector<std::string> filmCompare(const PixelGrab& a, const PixelGrab& r) {
    if (a.w != r.w || a.h != r.h || a.w <= 0) return {"Film at rest against the last moving picture: not compared (no picture read back)"};
    const std::size_t n = std::size_t(a.w) * a.h;
    unsigned char top = 0;
    for (std::size_t k = 0; k < n; k++) for (int c = 0; c < 3; c++) top = std::max(top, r.rgb[3 * k + c]);
    const int th = std::max(1, top / 5);
    const auto disc = [&](const PixelGrab& p, std::size_t k) { return std::max({p.rgb[3 * k], p.rgb[3 * k + 1], p.rgb[3 * k + 2]}) >= th; };
    std::vector<char> diff(n, 0);
    long differ = 0, discPx = 0;
    for (std::size_t k = 0; k < n; k++) { const bool x = disc(a, k), y = disc(r, k); discPx += y; if (x != y) { diff[k] = 1; differ++; } }
    long patch = 0;
    std::vector<int> st;
    for (std::size_t k0 = 0; k0 < n; k0++) {
        if (diff[k0] != 1) continue;
        long cnt = 0;
        st.assign(1, int(k0)); diff[k0] = 2;
        while (!st.empty()) {
            const int q = st.back(); st.pop_back(); cnt++;
            const int qi = q % a.w, qj = q / a.w;
            const int nb[4][2] = {{qi - 1, qj}, {qi + 1, qj}, {qi, qj - 1}, {qi, qj + 1}};
            for (const auto& p : nb) {
                if (p[0] < 0 || p[0] >= a.w || p[1] < 0 || p[1] >= a.h) continue;
                const std::size_t m = std::size_t(p[1]) * a.w + p[0];
                if (diff[m] == 1) { diff[m] = 2; st.push_back(int(m)); }
            }
        }
        patch = std::max(patch, cnt);
    }
    char t[400];
    std::snprintf(t, sizeof t, "Film at rest against the last moving picture (%d x %d pixels, a pixel is disc where its brightest channel is at least a fifth of the brightest): "
                  "%ld disc pixels at rest, %ld pixels change between disc and not-disc, the largest connected patch %ld pixels", a.w, a.h, discPx, differ, patch);
    return {t};
}

// ---- 0.56.1, Part 2: the waits. Each is done app::kBenchResponseRepeats times: prepare (the state before), wait until its picture is finished, act, then time
// until the lab's picture is finished again (benchLabFinished), at the end of the frame that draws it. ----
struct WaitDef { const char* id; const char* name; const char* what; };
const std::vector<WaitDef>& waitDefs() {
    static const std::vector<WaitDef> v = {
        {"response.lab_switch.orbit_atom", "From the Orbit Lab to the Atom Lab", "each lab at its defaults; the time to the new lab's first finished picture"},
        {"response.lab_switch.atom_molecule", "From the Atom Lab to the Molecule Lab", ""},
        {"response.lab_switch.molecule_blackhole", "From the Molecule Lab to the Black Hole Lab", ""},
        {"response.lab_switch.blackhole_orbit", "From the Black Hole Lab to the Orbit Lab", ""},
        {"response.atom_element", "Carbon to the heaviest atom, every electron", "Cloud 3D, every electron, Ultra: from carbon to oganesson; until its cloud has all its dots"},
        {"response.atom_density", "Cloud density Normal to Ultra, every electron", "oganesson, Cloud 3D, every electron: Cloud density from Normal (60,000 dots) to Ultra (240,000); until the cloud is complete"},
        {"response.molecule_pick", "Another molecule in the Density style", "Density style, 100,000 dots, cloud and surface: water to benzene, benzene to methanol, methanol to ethylene; until its cloud and surface are made"},
        {"response.blackhole_spin", "A new spin and distance typed in", "a feeding hole, Real look: from spin 0.5 at 20 GM/c\xC2\xB2 to 0.8, 0.85, 0.9 at 15, 18, 21 GM/c\xC2\xB2; until the sharp picture"},
        {"response.blackhole_look", "Real to Film", "a feeding hole: the look changed from Real to Film; until the finished picture"},
        {"response.blackhole_look_back", "Film back to Real", "the look changed from Film back to Real (with the glare); until the finished picture"},
        {"response.orbit_date", "A jump to a far date in Real physics", "Planet physics Real: from today to 1850-06-01, 2095-01-01 and 1805-03-01; until the picture of that date is drawn"},
    };
    return v;
}
// ---- Part 3: each lab left alone (VSync on, nothing moving) ----
struct RestDef { const char* id; const char* name; };
const std::vector<RestDef>& restDefs() {
    static const std::vector<RestDef> v = {
        {"rest.orbit", "Orbit Lab at its defaults, time stopped"}, {"rest.atom", "Atom Lab at its defaults"}, {"rest.molecule", "Molecule Lab at its defaults"},
        {"rest.blackhole", "Black Hole Lab at its defaults"}, {"rest.blackhole_spin", "Black Hole Lab, the hole spinning at 0.9"},
        {"rest.blackhole_glare", "Black Hole Lab, a feeding hole with the eye's glare"}};
    return v;
}

// every lab at its defaults (the waits and the rests start from them, so the numbers do not depend on the user's settings)
void labsToDefaults(AppState& app) {
    commonSetup(app);
    resetToToday(app);
    app.set = orbit::ViewSettings();
    app.view = orbit::OrbitView();
    app.atom.p = atom::AtomParams::appDefaults();
    app.atom.restart();
    app.atom.hold = true;
    app.molecule = molecule::MoleculeLab();
    app.blackhole = blackhole::settingsDefaults();
}

void waitPrepare(AppState& app, int w, int t) {
    const std::string id = waitDefs()[std::size_t(w)].id;
    atom::AtomLab& L = app.atom;
    const auto atomAll = [&](int Z, const char* dens) {
        L.p = atom::AtomParams::appDefaults(); L.p.elec = "All"; L.p.cdens = dens; L.p.view = "Cloud 3D";
        L.setParam("Z", double(Z)); L.restart(); L.hold = true;
    };
    static const char* const mols[4] = {"H2O", "C6H6", "CH3OH", "C2H4"};
    if (id.rfind("response.lab_switch.", 0) == 0) app.lab = id == "response.lab_switch.orbit_atom" ? 0 : id == "response.lab_switch.atom_molecule" ? 1 : id == "response.lab_switch.molecule_blackhole" ? molecule::kLabIndex : blackhole::kLabIndex;
    else if (id == "response.atom_element") { app.lab = 1; atomAll(6, "Ultra"); }
    else if (id == "response.atom_density") { app.lab = 1; atomAll(atom::heaviestAll(), "Normal"); }
    else if (id == "response.molecule_pick") {
        app.lab = molecule::kLabIndex;
        app.molecule.setStyle(molecule::Style::Realistic); app.molecule.set.realDots = molecule::kRealDots[3]; app.molecule.set.realShow = 0;
        app.molecule.choose(moleculeIndex(mols[t]));
    } else if (id.rfind("response.blackhole", 0) == 0) {
        app.lab = blackhole::kLabIndex;
        app.blackhole = blackhole::settingsDefaults();
        blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey);
        if (id == "response.blackhole_spin") { blackhole::setSpin(app.blackhole, 0.5); blackhole::setDistance(app.blackhole, 20); }
        if (id == "response.blackhole_look_back") blackhole::setLook(app.blackhole, blackhole::Look::Film);
    } else if (id == "response.orbit_date") {
        app.lab = 0;
        resetToToday(app);
        app.set = orbit::ViewSettings(); app.set.realPlanets = true;
        app.paused = true;
    }
}
void waitAct(AppState& app, int w, int t) {
    const std::string id = waitDefs()[std::size_t(w)].id;
    static const char* const mols[4] = {"H2O", "C6H6", "CH3OH", "C2H4"};
    if (id == "response.lab_switch.orbit_atom") app.lab = 1;
    else if (id == "response.lab_switch.atom_molecule") app.lab = molecule::kLabIndex;
    else if (id == "response.lab_switch.molecule_blackhole") app.lab = blackhole::kLabIndex;
    else if (id == "response.lab_switch.blackhole_orbit") app.lab = 0;
    else if (id == "response.atom_element") app.atom.setParam("Z", double(atom::heaviestAll()));
    else if (id == "response.atom_density") app.atom.setParam("cdens", std::string("Ultra"));
    else if (id == "response.molecule_pick") app.molecule.choose(moleculeIndex(mols[t + 1]));
    else if (id == "response.blackhole_spin") { blackhole::setSpin(app.blackhole, 0.8 + 0.05 * t); blackhole::setDistance(app.blackhole, 15 + 3 * t); }
    else if (id == "response.blackhole_look") blackhole::setLook(app.blackhole, blackhole::Look::Film);
    else if (id == "response.blackhole_look_back") blackhole::setLook(app.blackhole, blackhole::Look::Real);
    else if (id == "response.orbit_date") {
        static const int64_t dates[3] = {-3773779200000LL, 3944678400000LL, -5201798400000LL};   // 1850-06-01, 2095-01-01, 1805-03-01 (00:00 UTC)
        jumpToDate(app, dates[t], true);   // (as the Go button: 0.67.0 walks a far date on a worker)
    }
}
void restPrepare(AppState& app, int r) {
    const std::string id = restDefs()[std::size_t(r)].id;
    labsToDefaults(app);
    if (id == "rest.orbit") { app.lab = 0; app.paused = true; }
    else if (id == "rest.atom") app.lab = 1;
    else if (id == "rest.molecule") app.lab = molecule::kLabIndex;
    else {
        app.lab = blackhole::kLabIndex;
        if (id == "rest.blackhole_spin") blackhole::setSpin(app.blackhole, 0.9);
        if (id == "rest.blackhole_glare") { blackhole::applyPreset(app.blackhole, blackhole::kFeedingKey); if (SDL_getenv("PL_BH_REST_NOGAS")) app.blackhole.gasMoving = false; }   // (0.75.1, development: the moving gas off, to see what the rest line says without it)
    }
    app.vsync = true;                                     // as the user has it
}
bool onlyHas(const char* id) {
    const char* only = SDL_getenv("PL_BENCH_ONLY");
    return !only || ("," + std::string(only) + ",").find("," + std::string(id) + ",") != std::string::npos;
}

// which scenes run: the Hard ones, or All. PL_BENCH_ONLY="atom.slice_drag,blackhole.extreme" (development and the UI checks): only those ids
std::vector<int> sceneList(const BenchState& b) {
    std::vector<int> v;
    const char* only = SDL_getenv("PL_BENCH_ONLY");
    const int n = b.allScenes || only ? app::kBenchAllScenes : app::kBenchHardScenes;
    for (int i = 0; i < n; i++) {
        if (only && ("," + std::string(only) + ",").find("," + sceneText(i).id + ",") == std::string::npos) continue;
        v.push_back(i);
    }
    return v;
}
int activeScenes(const BenchState& b) { return int(sceneList(b).size()); }

BenchState& st(AppState& app) {
    if (!app.bench) app.bench = std::make_shared<BenchState>();
    return *app.bench;
}

const char* whyNot(const AppState& app) {
    if (app.pane) return "Learn is open: close it first.";
    if (app.journey.active) return "A Journey is running: stop it first.";
    if (app.shot.requested) return "A picture is being saved: wait a moment.";
    if (app.bench && app.bench->stage == BenchState::Stage::Running) return "The benchmark is already running.";
    return nullptr;
}

void restore(AppState& app, BenchState& b) {
    if (app.journey.active) { app.journey.active = false; app.view.journey = false; app.view.caption.clear(); }
    app.sys = *b.sys; app.view = *b.view; app.atom = *b.atom; app.atomView = *b.atomView; app.molecule = *b.molecule;
    app.set = b.set; app.journey = b.journey; app.tempK = b.tempK;
    app.lab = b.lab; app.pane = b.pane; app.learnBack = b.learnBack; app.paused = b.paused; app.vsync = b.vsync;
    app.learn = b.learn; app.atomLearn = b.atomLearn; app.molLearn = b.molLearn; app.bhLearn = b.bhLearn;
    app.learnJump = b.learnJump; app.atomLearnJump = b.atomLearnJump; app.molLearnJump = b.molLearnJump; app.bhLearnJump = b.bhLearnJump;
    app.blackhole = b.blackhole;
    app.showSettings = b.showSettings; app.showAbout = b.showAbout; app.showHistory = b.showHistory;
    app.prefs = *b.prefs;
    dataBenchRestore(b.data);
    app.dateNote = b.dateNote;
    std::snprintf(app.dateText, sizeof app.dateText, "%s", b.dateText); std::snprintf(app.timeText, sizeof app.timeText, "%s", b.timeText);
    b.sys.reset(); b.view.reset(); b.atom.reset(); b.atomView.reset(); b.molecule.reset(); b.prefs.reset(); b.data.reset();
    app.benchRunning = false;
}

std::string whenText(SDL_DateTime& dt) {
    char b[64];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return b;
}

// (the checks: what the benchmark must give back, as one line: the lab, the pages and panels, every lab's settings)
std::string stateLine(AppState& app) {
    std::map<std::string, std::string> bh;
    blackhole::saveSettings(app.blackhole, bh);
    std::string t = "lab=" + std::to_string(app.lab) + " pane=" + std::to_string(app.pane) + " settings=" + std::to_string(app.showSettings) + " about=" + std::to_string(app.showAbout) +
                    " learn=" + std::to_string(app.learn) + "," + std::to_string(app.atomLearn) + "," + std::to_string(app.molLearn) + "," + std::to_string(app.bhLearn) +
                    " data=" + std::to_string(dataOpen()) + " panels=" + app.prefs.shellPanels + " vsync=" + std::to_string(app.vsync) + " paused=" + std::to_string(app.paused) +
                    " mol=" + molecule::saveSettings(app.molecule.set) + " vib=" + std::to_string(app.molecule.vibMode) +
                    " atom=" + atom::benchShortText(app.atom) + "," + app.atom.p.elec + "," + app.atom.p.cdens + " journey=" + std::to_string(app.journey.active) +
                    " orbit=" + std::to_string(app.set.view3D) + "," + std::to_string(app.set.zoom) + "," + std::to_string(app.set.realPlanets) + "," + std::to_string(int(app.set.sky)) + "," + std::to_string(app.view.sel.kind) + " bh=";
    for (const auto& kv : bh) t += kv.first + ":" + kv.second + ";";
    return t;
}

void begin(AppState& app) {
    BenchState& b = st(app);
    const double t0 = app::frameCostNowMs();                         // 0.63.1: the start, part by part
    b.startT0 = t0; b.checkMs = b.setupMs = b.toSceneMs = -1; b.swapMs = 0; b.prepFrames = 0; b.checkLoads = 0;
    if (SDL_getenv("PL_BENCH")) std::printf("PLBENCH-STATE before %s\n", stateLine(app).c_str());
    // the environment, as it is when it starts (the window is not changed meanwhile)
    app::BenchEnv& e = b.env = app::BenchEnv();
    e.version = appVersion();
    SDL_Time now = 0; SDL_DateTime dt{};
    SDL_GetCurrentTime(&now); SDL_TimeToDateTime(now, &dt, true);
    e.when = whenText(dt);
    e.api = "Vulkan";                                    // 0.63.0: the report's "Graphics:" line (0.69.0: Vulkan is the only drawing system)
    if (g_vk) { e.gpu = g_vk->deviceName(); e.glVersion = g_vk->apiText() + ", " + g_vk->presentText(); e.vendor = g_vk->driverText(); }
    e.rocksPath = app::rocksPathText(app.glMajor, app.glMinor, app.rocksOk, true);          // (the benchmark's own settings start from the defaults: On)
    e.outlinePath = app::outlinePathText(app.glMajor, app.glMinor, app.computeOk, true);
    e.computePath = app::computePathText(app.glMajor, app.glMinor, app.computeOk, true);
    if (g_vk) {                                          // 0.63.1: a card program its guard switched off says so (0.63.0 said "GPU outline on" with the outline off)
        const auto off = [](const app::GpuGuard& g) { return g.started() && !g.allowed() ? (g.offText().empty() ? std::string("switched off by its guard") : g.offText()) : std::string(); };
        if (const std::string w = off(app::gpuVkOutlineGuard()); !w.empty()) e.outlinePath = "Vulkan: GPU outline off by its guard: " + w;
        if (const std::string w = off(app::gpuVkRocksGuard()); !w.empty()) e.rocksPath = "Vulkan: GPU rocks off by its guard: " + w;
        if (const std::string w = off(app::gpuVkGroupsGuard()); !w.empty()) e.computePath = "Vulkan: GPU superposition off by its guard: " + w;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const float sc = ImGui::GetStyle().FontScaleDpi;
    e.windowW = int(io.DisplaySize.x * io.DisplayFramebufferScale.x + 0.5f); e.windowH = int(io.DisplaySize.y * io.DisplayFramebufferScale.y + 0.5f);
    e.pictureW = int(app.canvasW * sc * io.DisplayFramebufferScale.x + 0.5); e.pictureH = int(app.canvasH * sc * io.DisplayFramebufferScale.y + 0.5);
    if (SDL_Window* w = g_window) {
        e.fullscreen = (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
        if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w))) e.refreshHz = m->refresh_rate;
    }
    e.textSize = app.prefs.textSize; e.theme = app.prefs.theme;
    e.power = app.powerNote;
    const SysInfo si = sysInfo();
    e.cpu = si.cpu; e.cores = si.cores; e.threads = si.threads; e.memoryMB = si.memoryMB;
    e.startupMs = app.startupMs;
    e.allScenes = b.allScenes;
    b.peakMB = processMemoryMB(); b.lastMemS = -1;
    b.warm = envNum("PL_BENCH_WARMUP", app::kBenchWarmupS);          // for automatic checks: shorter times
    b.measure = envNum("PL_BENCH_MEASURE", app::kBenchMeasureS);
    e.warmupS = b.warm; e.measureS = b.measure;
    // keep everything the user has
    b.lab = app.lab; b.pane = app.pane; b.learnBack = app.learnBack; b.paused = app.paused; b.vsync = app.vsync;
    b.learn = app.learn; b.atomLearn = app.atomLearn; b.molLearn = app.molLearn; b.bhLearn = app.bhLearn;
    b.learnJump = app.learnJump; b.atomLearnJump = app.atomLearnJump; b.molLearnJump = app.molLearnJump; b.bhLearnJump = app.bhLearnJump;
    b.blackhole = app.blackhole;
    b.showSettings = app.showSettings; b.showAbout = app.showAbout; b.showHistory = app.showHistory;
    b.sys = std::make_unique<orbit::SolarSystem>(app.sys);
    b.view = std::make_unique<orbit::OrbitView>(app.view);
    b.atom = std::make_unique<atom::AtomLab>(app.atom);
    b.atomView = std::make_unique<atom::AtomView>(app.atomView);
    b.molecule = std::make_unique<molecule::MoleculeLab>(app.molecule);
    b.prefs = std::make_unique<app::Prefs>(app.prefs);
    b.data = dataBenchSave();
    b.set = app.set; b.journey = app.journey; b.tempK = app.tempK;
    b.dateNote = app.dateNote;
    std::snprintf(b.dateText, sizeof b.dateText, "%s", app.dateText); std::snprintf(b.timeText, sizeof b.timeText, "%s", app.timeText);
    app.prefs.motion = "off";                                       // the scenes move whatever Reduced motion says (a held jump or swing measures nothing); put back at the end
    b.list = sceneList(b);
    b.waits.clear(); b.rests.clear();
    for (int k = 0; k < int(waitDefs().size()); k++) if (onlyHas(waitDefs()[std::size_t(k)].id) || (SDL_getenv("PL_BENCH_ONLY") && onlyHas("response"))) b.waits.push_back(k);
    for (int k = 0; k < int(restDefs().size()); k++) if (onlyHas(restDefs()[std::size_t(k)].id) || (SDL_getenv("PL_BENCH_ONLY") && onlyHas("rest"))) b.rests.push_back(k);
    b.phase = 0; b.wAt = b.wTry = b.wStage = 0; b.rAt = b.rStage = 0;
    b.lines.assign(b.list.size(), std::string()); b.paths.assign(b.list.size(), std::string());
    b.pics.assign(b.list.size(), -1); b.picsS.assign(b.list.size(), -1.0); b.picsBy.assign(b.list.size(), std::string()); b.notes.assign(b.list.size(), {});
    b.results.clear(); b.responses.clear(); b.idle.clear(); b.text.clear(); b.note.clear(); b.cancelled = false; b.gpuChecked = false; b.gpuCheck = app::GpuVerdict(); b.gpuReady.clear();
    b.bhByScene.clear();
    b.gpu.make();
    b.run = std::make_unique<app::BenchRun>(activeScenes(b), b.warm, b.measure);
    b.stage = BenchState::Stage::Running;
    app.benchRunning = true;
    b.beginMs = app::frameCostNowMs() - t0;
}

void saveAndCopy(AppState& app, BenchState& b) {
    b.text = app::benchResultText(b.env, b.results, b.responses, b.idle);
    {   // 0.55.7: the report ends with the last 40 lines of the graphics card's light paths' log (app/gpu_guard.h), so one paste carries both
        const std::vector<std::string> tail = app::gpuGuard().logTail(40);
        b.text += "\nThe graphics card's light paths, the last " + std::to_string(tail.size()) + " lines of " + (app::gpuGuard().logPath().empty() ? std::string("their log") : app::gpuGuard().logPath()) + ":\n";
        for (const std::string& l : tail) b.text += l + "\n";
        if (tail.empty()) b.text += "(no log)\n";
    }
    {   // 0.63.0 (Vulkan step 5): the card programs' guards of this start's API and their states
        const auto state = [](const app::GpuGuard& g) {
            if (!g.started()) return std::string("not started in this run");
            if (!g.allowed()) return "off: " + (g.offText().empty() ? std::string("switched off by its guard") : g.offText());
            return std::string(g.guarded() ? "allowed, guarded (it has not passed yet with this version and driver)" : "passed (ok)");
        };
        std::vector<std::pair<std::string, app::GpuGuard*>> gs;
        if (g_vk) { gs.push_back({"vulkan", &app::gpuVulkanGuard()}); for (const auto& x : app::vulkanCardGuards()) gs.push_back(x); }
        else gs = {{"lightpaths", &app::gpuGuard()}, {"sky", &app::gpuSkyGuard()}, {"slice", &app::gpuSliceGuard()}, {"jump", &app::gpuJumpGuard()}, {"molcloud", &app::gpuMolCloudGuard()}};
        b.text += std::string("\nThe card programs' guards (") + "Vulkan" + "):\n";
        for (const auto& [name, g] : gs) b.text += "  " + name + ": " + state(*g) + "\n";
    }
    if (SDL_getenv("PL_BENCH")) std::printf("PLBENCH-RESULT-BEGIN\n%s\nPLBENCH-RESULT-END\n", b.text.c_str());   // (for automatic runs: the result on stdout)
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
        for (int i = 0; i < n; i++) {
            const app::BenchSceneText& t = sceneText(b.list[std::size_t(i)]);
            app::BenchScene r = b.run->result(i);
            r.id = t.id; r.name = t.name; r.what = t.what;
            r.line = b.lines[std::size_t(i)]; r.path = b.paths[std::size_t(i)]; r.pictures = b.pics[std::size_t(i)]; r.picturesS = b.picsS[std::size_t(i)]; r.picturesBy = b.picsBy[std::size_t(i)];
            r.notes = b.notes[std::size_t(i)];
            if (std::size_t(i) < b.ownLeft.size() && b.ownLeft[std::size_t(i)] > 0) {
                char lo[200];
                std::snprintf(lo, sizeof lo, "%d frame%s left out of the counted frames: the benchmark's own numbers (a result read, a picture comparison) took %.1f ms of them", b.ownLeft[std::size_t(i)], b.ownLeft[std::size_t(i)] == 1 ? "" : "s", b.ownLeftMs[std::size_t(i)]);
                r.notes.push_back(lo);
            }
            if (t.kind == app::BenchKind::BlackHoleFilmTilt && r.notes.empty())
                r.notes.push_back("Film at rest against the last moving picture: not compared (the picture at rest was not finished within the counted time)");
            const auto bh = b.bhByScene.find(i);
            if (bh != b.bhByScene.end() && t.kind == app::BenchKind::BlackHole) { const std::vector<std::string> l = app::benchBlackHoleLines(bh->second); r.notes.insert(r.notes.end(), l.begin(), l.end()); }
            b.results.push_back(r);
        }
    }
    b.bhByScene.clear();
    b.env.cancelled = cancelled;
    b.env.peakMB = b.peakMB;
    b.env.gpuTimer = b.gpu.ok && std::any_of(b.results.begin(), b.results.end(), [](const app::BenchScene& s) { return s.gpuSamples > 0; });
    b.env.gpuReliable = b.env.gpuTimer && b.gpuChecked && b.gpuCheck.reliable;
    b.env.gpuCheck = b.gpuChecked ? b.gpuCheck.why : std::string();
    if (g_vk && g_vk->benchRejected() > 0) b.env.gpuCheck += "; " + std::to_string(g_vk->benchRejected()) + " frame(s) whose timestamps were thrown away (a zero, an end before its start, or over 10 s)";   // 0.63.1
    if (b.toSceneMs >= 0) {                              // 0.63.1: the start, part by part
        char s[400];
        std::snprintf(s, sizeof s, "%.0f ms from Start to the first scene's warm-up (settings kept and the run made %.0f ms; two frames showing \"Preparing the benchmark ...\"; "
                      "the GPU timer's check %.0f ms, %d known loads; the first scene set up %.0f ms); then the swapchain made again for VSync off %.0f ms",
                      b.toSceneMs, b.beginMs, b.checkMs, b.checkLoads, b.setupMs, b.swapMs);
        b.env.startParts = s;
    }
    restore(app, b);
    if (SDL_getenv("PL_BENCH")) std::printf("PLBENCH-STATE after %s\n", stateLine(app).c_str());
    b.gpu.destroy();
    b.cancelled = cancelled;
    if (cancelled) { b.stage = BenchState::Stage::Closed; b.run.reset(); return; }
    saveAndCopy(app, b);
    b.stage = BenchState::Stage::Done;
    b.run.reset();
    if (envHas("PL_BENCH_QUIT")) { SDL_Event q{}; q.type = SDL_EVENT_QUIT; SDL_PushEvent(&q); }   // (automatic runs: the app ends with the report)
}


bool labFinishedNow(AppState& app, BenchState& b);
double ticksMs(Uint64 from) { return static_cast<double>(SDL_GetPerformanceCounter() - from) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency()); }

// after the scenes: the waits (phase 1), then the labs left alone (phase 2), then the end
void nextPhase(AppState& app, BenchState& b) {
    if (b.phase == 0) {
        b.phase = 1; b.wAt = 0; b.wTry = 0; b.wStage = 0; b.wStageS = 0;
        if (!b.waits.empty()) { labsToDefaults(app); return; }
    }
    if (b.phase == 1) {
        b.phase = 2; b.rAt = 0; b.rStage = 0; b.rStageS = 0;
        if (!b.rests.empty()) return;
    }
    finish(app, b, false);
}

void partsBeginFrame(AppState& app, BenchState& b) {
    if (b.phase == 1) {
        if (b.wAt >= int(b.waits.size())) { nextPhase(app, b); return; }
        const int w = b.waits[std::size_t(b.wAt)];
        if (b.wStage == 0) { waitPrepare(app, w, b.wTry); b.wStage = 1; b.wStageS = 0; }
        else if (b.wStage == 4) {                                    // settled: the action, timed from here
            b.wT0 = SDL_GetPerformanceCounter();
            waitAct(app, w, b.wTry);
            b.wStage = 2; b.wStageS = 0; b.wWorst = 0; b.wFrames = 0;
        }
        app.vsync = true;                                             // as the user waits: VSync on (with it off the frame loop takes a whole core and the
                                                                      // workers below normal priority wait for it on a PC with few cores)
    } else if (b.phase == 2) {
        if (b.rAt >= int(b.rests.size())) { nextPhase(app, b); return; }
        if (b.rStage == 0) { restPrepare(app, b.rests[std::size_t(b.rAt)]); b.rStage = 1; b.rStageS = 0; }
        if (b.rStage == 2 && envHas("PL_BENCH_BUSY")) {           // (the UI check: a lab made busy on purpose: 20 ms of the processor every frame)
            const Uint64 t = SDL_GetPerformanceCounter();
            while (ticksMs(t) < 20) {}
        }
        app.vsync = true;
    }
}

void partsEndFrame(AppState& app, BenchState& b, double frameMs) {
    if (b.phase == 1 && b.wAt < int(b.waits.size())) {
        const int w = b.waits[std::size_t(b.wAt)];
        b.wStageS += frameMs / 1000;
        if (b.wStage == 1 && ((labFinishedNow(app, b) && b.wStageS >= 0.3) || b.wStageS > responseCapS())) b.wStage = 4;   // the state before is finished: act next frame
        else if (b.wStage == 2) {
            b.wWorst = std::max(b.wWorst, frameMs);
            b.wFrames++;
            const bool done = labFinishedNow(app, b), capped = !done && b.wStageS > responseCapS();
            if (done || capped) {
                if (b.responses.size() <= std::size_t(b.wAt)) {
                    const WaitDef& d = waitDefs()[std::size_t(w)];
                    app::BenchResponse r; r.id = d.id; r.name = d.name; r.what = d.what;
                    b.responses.push_back(r);
                }
                app::BenchResponse& r = b.responses[std::size_t(b.wAt)];
                r.ms.push_back(capped ? responseCapS() * 1000 : ticksMs(b.wT0));
                r.worstFrameMs.push_back(b.wWorst);
                if (capped) r.capped++;
                std::printf("PLBENCH-WAIT %s %.1f ms frames=%d%s\n", r.id.c_str(), r.ms.back(), b.wFrames, capped ? " (capped)" : "");
                b.wStage = 3; b.wStageS = 0;
            }
        } else if (b.wStage == 3 && b.wStageS >= 0.2) {             // a short pause, then the next try
            if (++b.wTry >= app::kBenchResponseRepeats) { b.wTry = 0; b.wAt++; }
            b.wStage = 0;
        }
    } else if (b.phase == 2 && b.rAt < int(b.rests.size())) {
        const RestDef& d = restDefs()[std::size_t(b.rests[std::size_t(b.rAt)])];
        b.rStageS += frameMs / 1000;
        const bool bh = app.lab == blackhole::kLabIndex;
        if (b.rStage == 1) {
            const bool fin = labFinishedNow(app, b);
            // 0.75.1: the Orbit Lab's card lines are decided by guarded steps, one a frame, behind the helper processes that make each card program first (about 8 s here with
            // LP_NUM_THREADS=1 at 2560 x 1369): the cap is a minute for them, so the rest is not counted while the lab is still deciding how to draw (each frame of it is a new picture)
            const double capS = app.lab == 0 && !orbit::linesCardSettled(b.R ? b.R->vulkan() : false) ? 60.0 : app::kBenchIdleSettleCapS;
            if ((fin && b.rStageS >= 0.5) || b.rStageS > capS) {   // finished (or waited long enough): count from here
                b.rNow = app::BenchIdle();
                // (0.67.0: the Orbit Lab keeps its picture too: its pictures drawn anew count as redraws)
                b.rNow.id = d.id; b.rNow.name = d.name; b.rNow.threads = b.env.threads; b.rNow.keepsPicture = bh || app.lab == 0; b.rNow.settled = fin;
                b.rCpu0 = processCpuSeconds(); b.rT0 = SDL_GetPerformanceCounter(); b.rRedraw0 = app.lab == 0 ? app.orbitPictures : b.R ? b.R->lensRedraws() : 0; b.rGpuSum = 0; b.rGpuN = 0;
                app.orbitKeyWhy.clear();
                b.rStage = 2; b.rStageS = 0;
            }
        } else if (b.rStage == 2) {
            b.rNow.frames++;
            if (bh && b.R && (!app.blackholeView.settled() || b.R->glareBusy() || app.blackholeView.fineThreadBusy())) b.rNow.workerBusy = true;
            if (b.rStageS >= app::kBenchIdleS) {
                const double wall = ticksMs(b.rT0) / 1000, cpu = processCpuSeconds();
                b.rNow.cpuShare = b.rCpu0 >= 0 && cpu >= 0 && wall > 0 && b.env.threads > 0 ? (cpu - b.rCpu0) / wall / b.env.threads : -1;
                b.rNow.redraws = app.lab == 0 ? app.orbitPictures - b.rRedraw0 : b.R ? b.R->lensRedraws() - b.rRedraw0 : 0;
                b.rNow.gpuMs = b.rGpuN > 0 && b.gpuChecked && b.gpuCheck.reliable ? b.rGpuSum / b.rGpuN : -1;
                if (app.lab == 0 && b.rNow.redraws > 3) b.rNow.why = app.orbitKeyWhy.empty() ? "no run of 30 frames in a row was seen" : app.orbitKeyWhy;
                b.idle.push_back(b.rNow);
                std::printf("PLBENCH-REST %s %s cpu=%.3f redraws=%ld worker=%d frames=%d%s%s\n", b.rNow.id.c_str(), app::benchIdleBusy(b.rNow) ? "busy" : "quiet", b.rNow.cpuShare, b.rNow.redraws, b.rNow.workerBusy ? 1 : 0, b.rNow.frames, b.rNow.why.empty() ? "" : " why: ", b.rNow.why.c_str());
                b.rAt++; b.rStage = 0;
                if (b.rAt >= int(b.rests.size())) nextPhase(app, b);
            }
        }
    }
}
}  // namespace

// 0.56.1: the lab on screen has its finished picture (nothing being made for it): the start-up time, the waits and the rest use it
bool benchLabFinished(AppState& app, render::Renderer2D& R);
namespace { bool labFinishedNow(AppState& app, BenchState& b) { return b.R ? benchLabFinished(app, *b.R) : true; } }
bool benchLabFinished(AppState& app, render::Renderer2D& R) {
    if (app.pane) return true;
    if (app.lab == blackhole::kLabIndex) return app.blackholeView.settled() && !R.glareBusy();
    if (app.lab == 1) {
        const atom::AtomLab& L = app.atom;
        if (L.p.view == "Orbit" || L.chartOn()) return true;
        return L.s.k >= L.cloudCap() && !L.s.trans;
    }
    if (app.lab == 0 && app.jumpWaiting) return false;     // 0.67.0: a far jump's walk under way (its picture is not drawn yet)
    if (app.lab == 0 && !orbit::linesCardSettled(R.vulkan())) return false;   // 0.68.0: the card's lines still being made and checked (drawn anew each frame)
    if (app.lab == molecule::kLabIndex) {
        const molecule::MoleculeLab& M = app.molecule;
        if (!M.realOn()) return true;
        return M.real.dotsId == M.mol().id && M.real.dotsN == M.set.realDots && M.real.meshId == M.mol().id;
    }
    return true;
}

void benchmarkButton(AppState& app) {
    const char* why = whyNot(app);
    if (why) ImGui::BeginDisabled();
    if (ImGui::Button("Benchmark...")) { st(app).stage = BenchState::Stage::Ask; }
    if (why) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("%s", why ? why : "Runs the scenes that are hard to draw and says plainly what is slow");
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("A test of about %d minutes: it measures the scenes that are hard to draw, how long you wait for a picture, and whether each lab is quiet at rest.", int(std::lround(app::benchPlannedS() / 60)));
    ImGui::PopStyleColor(2);
}

void benchmarkTestHook(AppState& app) {
    static int frames = 0;
    // 0.68.0 (checks: the pictures of the card's programs against the processor's): PL_BENCH_SETUP=<id> sets up that scene as the benchmark does
    // (orbit.heaviest, orbit.real_fast, orbit.earth, orbit.saturn, molecule.density, molecule.vib_density) at the third frame; the app then runs on as
    // usual (PL_FIXED_DT: the same frames each run; the camera is not turned)
    static const char* orbitScene = SDL_getenv("PL_BENCH_SETUP");
    if (orbitScene && frames == 2) {
        const std::string id = orbitScene;
        if (id == "orbit.heaviest") orbitHeaviest(app);
        else if (id == "orbit.real_fast") orbitRealFast(app);
        else if (id == "orbit.earth") orbitClose(app, "Earth", kEarthZoom);
        else if (id == "orbit.saturn") orbitClose(app, "Saturn", kSaturnZoom);
        else if (id == "molecule.density") moleculeSetup(app, app::BenchKind::MoleculeDensity);
        else if (id == "molecule.vib_density") moleculeSetup(app, app::BenchKind::MoleculeVibDensity);
    }
    const char* v = SDL_getenv("PL_BENCH");
    if (!v) { if (orbitScene) frames++; return; }
    if (++frames != 5) return;
    st(app).allScenes = SDL_getenv("PL_BENCH_RISK") != nullptr || SDL_getenv("PL_BENCH_ALL") != nullptr;   // for automatic checks: ticks "All scenes"
    if (std::string(v) == "start") begin(app); else st(app).stage = BenchState::Stage::Ask;
}

void benchmarkBeginFrame(AppState& app, render::Renderer2D& renderer, int fbW, int fbH) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    BenchState& b = *app.bench;
    b.R = &renderer;
    if (app.liveFrame) { std::printf("Benchmark stopped: the window was moved or resized\n"); finish(app, b, true); return; }   // the numbers would be wrong
    if (app.pane || app.shot.requested) { finish(app, b, true); return; }        // something got in the way: stop and give everything back (a Journey is the benchmark's own)
    if (!b.gpuChecked) {                                 // once, before the first scene
        if (b.prepFrames++ < 2) return;                  // 0.63.1: two frames first, so the box shows "Preparing the benchmark ..." before the check
        b.gpuChecked = true;
        const double c0 = app::frameCostNowMs();
        g_checkLoads = 0;
        b.gpuCheck = checkGpuTimer(renderer, app.canvasW, app.canvasH, fbW, fbH);
        b.checkMs = app::frameCostNowMs() - c0; b.checkLoads = g_checkLoads;
        std::printf("GPU timer check: %s: %s\n", b.gpuCheck.reliable ? "reliable" : "NOT reliable", b.gpuCheck.why.c_str());
    }
    if (b.phase == 0 && b.run->sceneCount() == 0) nextPhase(app, b);
    if (b.phase >= 1) { if (b.stage == BenchState::Stage::Running) partsBeginFrame(app, b); return; }
    const int i = b.run->sceneToSetUp();
    if (i >= 0) {
        const int idx = b.list[std::size_t(i)];
        b.bhLastU = -1e9; b.bhRestS = -1; b.bhKind = -1; b.bhHave = false;
        for (double& x : b.bhMoveS) x = 0;
        b.grabNext = 0; b.filmStopped = b.filmDone = b.filmCompared = false; b.grabMoving = b.grabRest = PixelGrab();
        b.lastU = 0; for (auto& r : b.split) for (double& x : r) x = 0;
        const double s0 = app::frameCostNowMs();
        setupScene(app, b, idx);
        b.lines[std::size_t(i)] = sceneLine(app, b, idx);
        b.run->setUpDone();
        if (i == 0) { b.setupMs = app::frameCostNowMs() - s0; b.toSceneMs = app::frameCostNowMs() - b.startT0; }   // 0.63.1
        b.sceneFrame = 0; b.measFrame = -1; b.picLast = -1;
        if (sceneText(idx).kind == app::BenchKind::BlackHole) b.bhByScene[i].fineStartMs = app.blackholeView.fineFillMs();   // 0.55.8
        std::printf("PLBENCH-SCENE %s\n", sceneText(idx).id.c_str());
    }
    if (b.run->sceneToSetUp() < 0 && b.run->scene() < int(b.list.size())) {      // what moves in the scene
        const int idx = b.list[std::size_t(b.run->scene())];
        b.measFrame = b.run->measuring() ? b.measFrame + 1 : -1;
        const double u = sceneU(b), dt = std::clamp(u - b.lastU, 0.0, 0.1);
        b.lastU = u;
        if (sceneText(idx).kind == app::BenchKind::BlackHole) sampleBlackHole(app, b, renderer);   // 0.55.5
        frameScene(app, b, idx, u, dt);
        b.sceneFrame++;
    }
    app.vsync = false;
}

void benchmarkUse(SDL_Window* window, render::VkBackend* vk) { g_window = window; g_vk = vk; }

void benchmarkGpuBegin(AppState& app) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    GpuTimer& g = app.bench->gpu;
    if (!g.ok || g.active >= 0) return;
    if (g_vk) {                                          // 0.63.0: the next vk.render() is timed
        // 0.63.1: only frames that are counted (a scene's counted time, a lab at rest being counted): a warm-up frame's result, a frame or two late, was
        // counted in the scene before (here a slow scene's card time came out above its frame time)
        BenchState& bs_ = *app.bench;
        const BenchState& bs = bs_;
        const bool counted = (bs.phase == 0 && bs.run && bs.run->measuring()) || (bs.phase == 2 && bs.rStage == 2);
        if (!counted) return;
        bs_.gpuFrameNo++;
        if (!g_vk->benchMark(bs_.gpuFrameNo)) g.framesSkipped++; else bs_.gpuFrameMarked = bs_.gpuFrameNo;
    }
}

void benchmarkGpuEnd(AppState& app) {
    if (!app.bench) return;
    BenchState& b = *app.bench;
    GpuTimer& g = b.gpu;
    (void)g;                                             // (Vulkan: each frame waits for its slot's fence: PL_BENCH_FINISH needs nothing more)
    // (development and the checks: PL_BENCH_SHOT="a" or "a,b": each scene's picture saved as <id>.bmp a s into its counted time, and as <id>_2.bmp at b s)
    static double shotAt[2] = {-1, -1};
    static const bool shotRead = [] { if (const char* v = SDL_getenv("PL_BENCH_SHOT")) std::sscanf(v, "%lf,%lf", &shotAt[0], &shotAt[1]); return true; }();
    (void)shotRead;
    const bool scenesNow = b.stage == BenchState::Stage::Running && b.run && b.phase == 0;
    const int shotK = !scenesNow || b.shotFor != b.run->scene() ? 0 : b.shotK;
    if (scenesNow && shotK < 2 && shotAt[shotK] >= 0 && b.run->measuring() && b.run->sceneElapsedS() >= shotAt[shotK] && b.run->scene() < int(b.list.size())) {
        b.shotFor = b.run->scene(); b.shotK = shotK + 1;
        const ImGuiIO& io = ImGui::GetIO();
        const int W = int(io.DisplaySize.x * io.DisplayFramebufferScale.x + 0.5f), H = int(io.DisplaySize.y * io.DisplayFramebufferScale.y + 0.5f);
        std::vector<uint8_t> vpx;                       // 0.63.0: Vulkan: the frame just drawn, read back (RGBA, top row first)
        int vw = 0, vh = 0;
        const bool vok = g_vk && g_vk->readback(vpx, vw, vh) && vw == W && vh == H;
        if (SDL_Surface* shot = W > 0 && H > 0 && vok ? SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ABGR8888) : nullptr) {
            for (int y = 0; y < H; y++) std::memcpy(static_cast<uint8_t*>(shot->pixels) + std::size_t(y) * std::size_t(shot->pitch), vpx.data() + std::size_t(y) * std::size_t(W) * 4, std::size_t(W) * 4);
            SDL_SaveBMP(shot, (sceneText(b.list[std::size_t(b.run->scene())]).id + (shotK ? "_2" : "") + ".bmp").c_str());
            SDL_DestroySurface(shot);
        }
        b.grabSkip = true;
    }
    if (scenesNow && b.grabNext > 0) {   // 0.56.1, film_tilt: read the free part of the picture back (this frame is not counted)
        const ImGuiIO& io = ImGui::GetIO();
        const double k = ImGui::GetStyle().FontScaleDpi * io.DisplayFramebufferScale.x;
        const int x0 = std::max(0, int(app.bhFreeX * k)), y0 = std::max(0, int(app.bhFreeY * k));
        const int w = std::max(0, int((app.bhFreeW > 0 ? app.bhFreeW : app.canvasW) * k)), h = std::max(0, int((app.bhFreeH > 0 ? app.bhFreeH : app.canvasH) * k));
        PixelGrab& p = b.grabNext == 1 ? b.grabMoving : b.grabRest;
        p.w = w; p.h = h; p.rgb.assign(std::size_t(w) * h * 3, 0);
        std::vector<unsigned char> rgba(std::size_t(w) * h * 4);
        if (w > 0 && h > 0 && g_vk) {                    // 0.63.0: Vulkan: the frame read back (top row first), its rows put bottom row first as glReadPixels gives
            std::vector<uint8_t> all;
            int fw = 0, fh = 0;
            if (g_vk->readback(all, fw, fh))
                for (int j = 0; j < h; j++) {
                    const int yt = y0 + (h - 1 - j);
                    if (yt < 0 || yt >= fh) continue;
                    for (int i2 = 0; i2 < w && x0 + i2 < fw; i2++) std::memcpy(&rgba[(std::size_t(j) * w + i2) * 4], &all[(std::size_t(yt) * fw + std::size_t(x0 + i2)) * 4], 4);
                }
        }
        const double fx = io.DisplayFramebufferScale.x;
        const int bx0 = int(b.boxX * fx) - x0, by0 = int(b.boxY * fx) - y0, bx1 = int((b.boxX + b.boxW) * fx) - x0 + 2, by1 = int((b.boxY + b.boxH) * fx) - y0 + 2;
        for (int j = 0; j < h; j++) for (int i2 = 0; i2 < w; i2++) {
            const int jt = h - 1 - j;                    // (rows from the top)
            if (i2 >= bx0 && i2 < bx1 && jt >= by0 && jt < by1) continue;   // the running box is left black in both
            for (int c = 0; c < 3; c++) p.rgb[(std::size_t(jt) * w + i2) * 3 + c] = rgba[(std::size_t(j) * w + i2) * 4 + c];
        }
    }
}

void benchmarkEndFrame(AppState& app, double frameMs, double cpuMs, const app::BenchFrameCost* last) {
    if (!app.bench || app.bench->stage != BenchState::Stage::Running) return;
    BenchState& b = *app.bench;
    // the GPU results that are ready now (they come a frame or two late), oldest first: kept with their frame's tag until the frame's decision is known (below)
    GpuTimer& g = b.gpu;
    (void)g;
    if (g_vk) {                                          // 0.63.0: Vulkan's timestamps
        std::vector<std::pair<double, double>> v;
        std::vector<long> tags;
        g_vk->benchCollect(v, &tags);
        for (std::size_t i = 0; i < v.size(); i++) b.gpuReady.push_back(GpuSample{v[i].first, v[i].second, i < tags.size() ? tags[i] : -1});
    }
    // 0.75.1: a result goes to the run when its frame was counted and is dropped when the frame was left out of the count (a result comes back a frame or two after its
    // frame, untagged until now: 0.75.0 counted the card's time of frames whose own time was left out: blackhole.moving_gas under LP_NUM_THREADS=1 read 1.25-1.45 x its frame time)
    const auto settle = [&](bool counted) {
        if (b.gpuFrameMarked >= 0) { b.frameCounted.push_back({b.gpuFrameMarked, counted}); b.gpuFrameMarked = -1; if (b.frameCounted.size() > 64) b.frameCounted.erase(b.frameCounted.begin()); }
        for (std::size_t i = 0; i < b.gpuReady.size();) {
            const GpuSample r = b.gpuReady[i];
            const auto it = std::find_if(b.frameCounted.begin(), b.frameCounted.end(), [&](const std::pair<long, bool>& e) { return e.first == r.tag; });
            if (it == b.frameCounted.end()) {            // (its frame is not decided yet, or too old to be known: waits, at most 64 frames)
                if (r.tag < b.gpuFrameNo - 64) b.gpuReady.erase(b.gpuReady.begin() + std::ptrdiff_t(i)); else i++;
                continue;
            }
            if (it->second) {
                if (b.run) b.run->gpuSample(r.busyMs, r.spanMs);     // (the run may be gone: the last rest ended the benchmark in this frame)
                if (b.phase == 2 && b.rStage == 2) { b.rGpuSum += r.busyMs; b.rGpuN++; }
            }
            b.gpuReady.erase(b.gpuReady.begin() + std::ptrdiff_t(i));
        }
    };
    if (SDL_Window* w = g_window) {   // 0.15 stage 2: a scene during which the window lost focus or was minimised is flagged in the report
        const SDL_WindowFlags fl = SDL_GetWindowFlags(w);
        b.run->noteWindow((fl & SDL_WINDOW_INPUT_FOCUS) != 0, (fl & SDL_WINDOW_MINIMIZED) != 0);
    }
    {   // the app's memory, four times a second
        const double now = SDL_GetTicks() / 1000.0;
        if (now - b.lastMemS >= 0.25) { b.lastMemS = now; b.peakMB = std::max(b.peakMB, processMemoryMB()); }
    }
    if (b.phase >= 1) { partsEndFrame(app, b, frameMs); settle(true); return; }
    const int sc = b.run->scene();
    // (0.63.1: the frame after a read-back is not counted either: its time, from the frame before's start, holds the read-back; 0.63.0 counted it)
    // 0.68.1: the benchmark's own measuring must not land in a counted frame (the owner's 0.68.0 run: 19.6 ms of "the benchmark's numbers" in a film_tilt frame, the picture
    // comparison; this box: 220 ms of a result read). The frame whose time holds more than 1 ms of the benchmark's numbers is left out of the counted frames, and the report
    // says how many and how long (no other special case)
    const bool ownLeftOut = last && last->bookMs > 1.0 && b.run->measuring() && sc < int(b.list.size());
    if (ownLeftOut) {
        if (b.ownLeft.size() < b.list.size()) { b.ownLeft.assign(b.list.size(), 0); b.ownLeftMs.assign(b.list.size(), 0.0); }
        b.ownLeft[std::size_t(sc)]++; b.ownLeftMs[std::size_t(sc)] += last->bookMs;
    }
    const bool grabbed = b.grabNext > 0 || b.grabSkip || b.grabPrev || ownLeftOut;
    b.grabPrev = b.grabNext > 0 || b.grabSkip;
    b.grabSkip = false;
    if (sc < int(b.list.size()) && b.run->sceneToSetUp() < 0 && b.R) {
        const int idx = b.list[std::size_t(sc)];
        // new pictures while counted (0.56.1)
        const long c = pictureCounter(app, *b.R, idx);
        if (c >= 0) {
            if (b.run->measuring()) { if (b.pics[std::size_t(sc)] < 0) b.pics[std::size_t(sc)] = 0; if (b.picLast >= 0) b.pics[std::size_t(sc)] += std::clamp(c - b.picLast, 0L, 1L); }   // (at most one new picture a frame: the one shown)
            // 0.67.0: the Black Hole Lab's scenes: the time of the frames whose view changed (they needed a new picture; a frame at rest does not)
            if (app.lab == blackhole::kLabIndex) {
                const blackhole::Settings v = blackhole::shown(app.blackhole);
                const double key[6] = {v.yaw, v.pitch, v.dist, v.spin, v.fovDeg, double(static_cast<int>(v.look))};
                const bool moved = !std::equal(key, key + 6, b.bhViewKey);
                std::copy(key, key + 6, b.bhViewKey);
                if (b.run->measuring() && moved) { double& m = b.picsS[std::size_t(sc)]; m = std::max(m, 0.0) + frameMs / 1000; }
            }
            b.picLast = c;
            b.picsBy[std::size_t(sc)] = picturesBy(app, idx);
        }
        // atom.nucleus_density: the two halves apart (still, turning) and the density grids made in each
        const app::BenchSceneText& t = sceneText(idx);
        if (t.kind == app::BenchKind::Atom && t.atomWhich == atom::kBenchNucleusDensity && b.run->measuring()) {
            const int h = b.run->sceneElapsedS() < 0.5 * b.measure ? 0 : 1;
            if (b.split[h][0] == 0) b.gridsAt[h] = app.atomView.densityGridsMade();
            b.split[h][0] += 1; b.split[h][1] += frameMs; b.split[h][2] += cpuMs;
            const long made = app.atomView.densityGridsMade();
            char l[300];
            std::vector<std::string>& nt = b.notes[std::size_t(sc)];
            nt.clear();
            if (!app.atom.s.densityDrawn) nt.push_back("WARNING: this frame is not the Density look (the nucleons are drawn): the numbers below are not the Density look's");   // 0.68.1 (0.66.0-0.68.0 measured the nucleons at the owner's window size)
            for (int k = 0; k < 2; k++) if (b.split[k][0] > 0) {
                const long grids = (k == 0 ? (b.split[1][0] > 0 ? b.gridsAt[1] : made) : made) - b.gridsAt[k];
                std::snprintf(l, sizeof l, "%s: %.0f frames, %.2f ms a frame (%.1f fps), CPU work %.2f ms; the density grid made %ld times", k == 0 ? "Standing still" : "Turning",
                              b.split[k][0], b.split[k][1] / b.split[k][0], 1000.0 * b.split[k][0] / b.split[k][1], b.split[k][2] / b.split[k][0], grids);
                nt.push_back(l);
            }
        }
        // (0.63.1: once: 0.63.0 compared the two whole pictures again in every frame after the one at rest, in the counted time)
        if (t.kind == app::BenchKind::BlackHoleFilmTilt && b.filmDone && !b.filmCompared && b.grabNext == 0 && b.grabRest.w > 0) { b.notes[std::size_t(sc)] = filmCompare(b.grabMoving, b.grabRest); b.filmCompared = true; }
        if (b.run->measuring()) b.paths[std::size_t(sc)] = scenePath(app, b, idx, sc);
    }
    static const bool slowLog = envHas("PL_BENCH_SLOW");                  // (development: the frames over 250 ms, with their scene)
    if (slowLog && frameMs > 250 && sc < int(b.list.size())) std::printf("PLBENCH-SLOW %s %.0f ms %s at %.2f s\n", sceneText(b.list[std::size_t(sc)]).id.c_str(), frameMs, b.run->measuring() ? "counted" : "warm-up", b.run->sceneElapsedS());
    if (last && sc == 0 && !b.run->measuring()) b.swapMs += last->swapchainMs;   // 0.63.1: the swapchain made again for VSync off (the first scene's warm-up)
    b.run->frame(frameMs, cpuMs, !grabbed, last);
    settle(!grabbed);
    if (grabbed) b.grabNext = 0;
    if (b.run->finished()) nextPhase(app, b);
}

void benchmarkWindow(AppState& app) {
    if (!app.bench) return;
    auditNote(std::string("benchstage ") + std::to_string(static_cast<int>(app.bench->stage)) + " running=" + std::to_string(app.benchRunning ? 1 : 0));
    BenchState& b = *app.bench;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float fs = ImGui::GetFontSize();
    const ImGuiWindowFlags fl = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    const bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    // The running box is a modal pop-up: while it is open ImGui ignores every other window (panels, header, tabs, settings). The picture is not ImGui,
    // so main.cpp also drops its wheel / drag / click / key handling (app::labInputAllowed). The dim behind a modal is made clear there, so the picture stays as it is.
    static const char* const kRunBox = "##benchrun";
    const bool running = b.stage == BenchState::Stage::Running;
    bool cancel = running && esc;
    if (running || ImGui::IsPopupOpen(kRunBox)) {
        if (running && !ImGui::IsPopupOpen(kRunBox)) ImGui::OpenPopup(kRunBox);
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12 * fs / 16, vp->WorkPos.y + 12 * fs / 16), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::BeginPopupModal(kRunBox, nullptr, fl | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {
            panelDecor(app::PanelKind::Floating);
            if (running) {
                const app::BenchRun& r = *b.run;
                if (b.phase == 0 && !b.gpuChecked) {    // 0.63.1: the timer's check and the first scene's set-up (seconds on some PCs)
                    ImGui::Text("Preparing the benchmark ...");
                    ImGui::TextDisabled("Checking the GPU timer, then setting up the first scene.");
                } else if (b.phase == 0 && r.sceneCount() > 0) {
                    const int shown = std::min(r.scene() + 1, r.sceneCount());
                    const app::BenchSceneText& t = sceneText(b.list[std::size_t(std::min(r.scene(), r.sceneCount() - 1))]);
                    ImGui::Text("Scene %d of %d: %s", shown, r.sceneCount(), t.id.c_str());
                    ImGui::Text("%s  %.1f s", r.measuring() ? "Counting" : "Warming up", r.sceneElapsedS());
                } else if (b.phase == 1 && b.wAt < int(b.waits.size())) {
                    ImGui::Text("Wait %d of %d: %s", b.wAt + 1, int(b.waits.size()), waitDefs()[std::size_t(b.waits[std::size_t(b.wAt)])].id);
                    ImGui::Text("Try %d of %d", b.wTry + 1, app::kBenchResponseRepeats);
                } else if (b.phase == 2 && b.rAt < int(b.rests.size())) {
                    ImGui::Text("At rest %d of %d: %s", b.rAt + 1, int(b.rests.size()), restDefs()[std::size_t(b.rests[std::size_t(b.rAt)])].id);
                    ImGui::Text("%s", b.rStage == 2 ? "Counting" : "Waiting for the finished picture");
                }
                const double fps = app.stats.avgFrame() > 0 ? 1000.0 / app.stats.avgFrame() : 0;
                ImGui::Text("Live: %.0f fps", fps);
                {   const double total = r.totalS() + b.waits.size() * app::kBenchResponseRepeats * app::kBenchResponsePlanS + b.rests.size() * app::kBenchIdlePlanS;
                    const double done = r.doneS() + (b.phase >= 1 ? (b.wAt * app::kBenchResponseRepeats + b.wTry) * app::kBenchResponsePlanS : 0) + (b.phase >= 2 ? b.rAt * app::kBenchIdlePlanS : 0);
                    ImGui::ProgressBar(float(total > 0 ? std::min(1.0, done / total) : 1.0), ImVec2(24 * fs, 0)); }
                ImGui::TextDisabled("Everything else is locked until the test ends.");
                if (ImGui::Button("Cancel")) cancel = true;
            }
            b.boxX = ImGui::GetWindowPos().x; b.boxY = ImGui::GetWindowPos().y; b.boxW = ImGui::GetWindowWidth(); b.boxH = ImGui::GetWindowHeight();
            if (cancel || !running) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (cancel) { finish(app, b, true); return; }
    if (b.stage == BenchState::Stage::Closed) return;
    if (b.stage == BenchState::Stage::Ask) {
        {   // centred in the part below the top bar and never taller than it
            const float top = shellTopBarH(), room = vp->WorkSize.y - top - 16 * fs / 16;
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + top + (vp->WorkSize.y - top) * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(std::min(34 * fs, vp->WorkSize.x * 0.95f), 0), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, std::max(100.0f, room)));
        }
        bool open = true;
        pushFloatStyle();
        const bool benchShown = ImGui::Begin("Benchmark###bench", &open, fl);
        popFloatStyle();
        if (benchShown) {
            panelDecor(app::PanelKind::Floating);
            const std::vector<int> list = sceneList(b);
            const int count = int(list.size());
            const double planned = count * (app::kBenchWarmupS + app::kBenchMeasureS) + app::kBenchResponseActions * app::kBenchResponsePlanS + app::kBenchIdleEntries * app::kBenchIdlePlanS;
            ImGui::TextWrapped("This runs %d scenes that are hard to draw, each moving as you would move it (%.1f s of warm-up that is not counted, then %.1f s counted), then times %d waits "
                               "for a finished picture and leaves each lab alone for %.0f s. About %.0f minutes in all.",
                               count, app::kBenchWarmupS, app::kBenchMeasureS, app::kBenchResponseActions, app::kBenchIdleS, std::ceil(planned / 60));
            ImGui::Spacing();
            // everything between here and the buttons scrolls INSIDE the window, so Start / Cancel are always in view (the body's height is measured the frame before)
            static float bodyH = 0;
            const float buttonsH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y + ImGui::GetStyle().WindowPadding.y;
            const float room = std::max(60.0f, std::min(vp->WorkSize.y * 0.92f, vp->Size.y - std::max(0.0f, shellTopBarH()) - 2 * 8.0f) - ImGui::GetCursorPosY() - buttonsH - ImGui::GetStyle().ItemSpacing.y);
            const float childH = bodyH > 0 ? std::min(bodyH, room) : room;
            ImGui::BeginChild("##benchBody", ImVec2(0, childH), ImGuiChildFlags_None);
            for (int i = 0; i < count; i++) ImGui::BulletText("%s  %s", sceneText(list[std::size_t(i)]).id.c_str(), sceneText(list[std::size_t(i)]).name.c_str());
            ImGui::Checkbox("All scenes", &b.allScenes);
            if (ImGui::IsItemHovered()) ui::SetTooltip("Adds %d scenes: the easy ones and the old processor paths (the work done by the processor the way older versions did it). "
                                                          "Off by default; it makes the test longer.", app::kBenchAllScenes - app::kBenchHardScenes);
            ImGui::Spacing();
            ImGui::TextWrapped("VSync is switched off during the scenes (on for the waits and the rest), and the picture is drawn at the size it has now. "
                               "Close other programs for the most useful number. Do not move or resize the window: that stops the test. Your settings, the labs, the pages and the panels are put back when it ends, and Esc stops it. "
                               "Before the first scene the graphics card's timer is checked with a known load (about a second).");
            bodyH = ImGui::GetCursorPosY();
            ImGui::EndChild();
            const char* blocked = whyNot(app);
            if (blocked) ImGui::BeginDisabled();
            const bool startClicked = ImGui::Button("Start");
            if (blocked) ImGui::EndDisabled();
            if (blocked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("%s", blocked);
            noteItem("bench.start");
            auditRect("bench.start", ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y, ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);
            if (startClicked && !blocked) begin(app);
            if (blocked) { ImGui::SameLine(); ImGui::TextDisabled("%s", blocked); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || esc) b.stage = BenchState::Stage::Closed;
            auditRect("win.benchmark", ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
        }
        ImGui::End();
        if (!open) b.stage = BenchState::Stage::Closed;
    } else if (b.stage == BenchState::Stage::Done) {
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(52 * fs, vp->WorkSize.x * 0.97f), std::min(34 * fs, vp->WorkSize.y * 0.92f)), ImGuiCond_Appearing);
        bool open = true;
        pushFloatStyle();
        const bool resultShown = ImGui::Begin("Benchmark result###bench", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        popFloatStyle();
        if (resultShown) {
            panelDecor(app::PanelKind::Floating);
            ImGui::TextWrapped("%s, %s, Vulkan %s; %s", b.env.gpu.c_str(), b.env.vendor.c_str(), b.env.glVersion.c_str(), b.env.cpu.empty() ? "processor not available" : b.env.cpu.c_str());
            ImGui::TextWrapped("Window %d x %d, picture %d x %d pixels%s, screen %.0f Hz", b.env.windowW, b.env.windowH, b.env.pictureW, b.env.pictureH, b.env.fullscreen ? ", full screen" : "", b.env.refreshHz);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, vec(look().w.line));      // the header row in the theme's colours (the default is dark on every theme)
            if (ImGui::BeginTable("##benchtable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {   // 0.56.1: the summary, slowest first
                for (const char* h : {"Scene", "fps", "1 % low", "Pictures/s", "Reaches", "Limited by"}) ImGui::TableSetupColumn(h);
                ImGui::TableHeadersRow();
                for (const app::BenchSummaryRow& r : app::benchSummary(b.results, b.env.refreshHz, b.env.gpuReliable)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextWrapped("%s", r.id.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", r.avgFps);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", r.low1Fps);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", r.picsPerS);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(r.reaches > 0 ? "yes" : r.reaches == 0 ? "no" : "not known");
                    ImGui::TableNextColumn(); ImGui::TextWrapped("%s", r.limitedBy.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, b.noteFailed ? vec(look().w.danger) : vec(look().w.muted));
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
