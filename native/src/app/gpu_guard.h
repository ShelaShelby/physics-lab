// 0.55.7: the guard round the graphics card's light paths (the Black Hole Lab's compute shader, render/kerr_compute.h, blackhole/spin_card.h), and their log.
// A crash or a hang inside a graphics driver cannot be caught by the app. So before each step of the card's path (until the whole path has succeeded once
// for this app version and this driver) a small marker file is written beside the settings file, flushed and committed to disk, naming the step; it is
// removed when the steps have finished (passed or failed). A marker found at the next start means the last run died inside that step: the card's path is
// then switched off (the shader is not even compiled) and the settings file keeps that, with the light-path shader's own text (0.55.9: a hash of it, in
// place of the app version: the owner's app died once more at the first start of 0.55.8, whose shader had not changed) and the driver's version string. A
// changed shader or a new driver tries once more by itself; a new app version alone does not; Settings, Performance, "Try again" clears it.
// (The field is still called version below: it holds "shader <hash>" from 0.55.9 on; a line kept by 0.55.7 or 0.55.8 names an app version, which is
// another text, so the shader is tried once more, guarded, after this update.)
// A step that does not finish within its time limit (a fence that never signals) counts as failed and is kept the same way; the frame never waits for it.
// The log: one plain text file beside the settings file, overwritten at each start, each line flushed (and, while guarded, committed) before the step it
// names begins. No graphics here: the steps are named by the callers.
#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace app {

// what the settings file keeps (the line "gpupaths="): state "off" (the app died inside step), "slow" (step did not finish within its time limit), "fail"
// (0.59.0: step returned an error; the step field holds "step: why") or "ok" (the whole path succeeded once), for this app version and driver; "" = nothing kept
// 0.63.1: app: the app version that wrote it (a marker's line "app=", the settings line's fifth field "app 0.63.1"); "" = written by 0.63.0 or older
struct GpuGuardRecord {
    std::string state, version, driver, step, app;
    GpuGuardRecord() = default;
    GpuGuardRecord(std::string st, std::string v, std::string d, std::string sp, std::string a = std::string())
        : state(std::move(st)), version(std::move(v)), driver(std::move(d)), step(std::move(sp)), app(std::move(a)) {}
    bool operator==(const GpuGuardRecord& o) const { return state == o.state && version == o.version && driver == o.driver && step == o.step && app == o.app; }
};
std::string gpuGuardText(const GpuGuardRecord& r);       // "off | 0.55.7 | <driver> | <step>" (a '|' or line break inside a field becomes '/' or ' ')
GpuGuardRecord gpuGuardParse(const std::string& line);   // a broken line gives the empty record

// The decision at start, from the kept record, a marker left behind (empty state: none) and this run's version and driver
struct GpuGuardDecision {
    bool allowed = true;          // the card's path may be tried in this run
    bool guarded = true;          // markers before each step (the path has not yet succeeded once for this version and driver)
    GpuGuardRecord keep;          // what the settings file keeps from now on
    bool crashed = false;         // not allowed because a run died inside a step (false: a step timed out or failed)
    bool failed = false;          // 0.59.0: not allowed because a step returned an error (kept as "fail")
    bool untrusted = false;       // 0.63.1: a crash kept from a marker of 0.63.0 or older, not trusted (tried once more, guarded)
};
// 0.63.1: keepOld = false: a crash ("off") kept from a marker that names no app version or one before 0.63.0 is not trusted: 0.62.0's guard could leave a
// marker after its step had passed (end() kept it; the Vulkan card guards had no closing()); 0.63.0's markers name no app version either, so they are
// retried too (once, guarded: a real crash then writes a marker that names 0.63.1 and is trusted). keepOld = true (the light paths, both APIs: their
// crash inside AMD's shader compiler is real): kept as before.
GpuGuardDecision gpuGuardDecide(const GpuGuardRecord& saved, const GpuGuardRecord& marker, const std::string& version, const std::string& driver, bool keepOld = true);
bool gpuGuardAppTrusted(const std::string& app);               // 0.63.1: a marker written by this app version is trusted (0.63.1 or later; "" no)
void gpuGuardSetApp(const std::string& app);                   // 0.63.1: the running app's version, written into each marker (main.cpp, tests)

class GpuGuard {
public:
    GpuGuard() : keepOld_(true) {}                                // (the light paths' guard)
    ~GpuGuard() { giveTurn(); }                                   // (0.63.0: a guard dropped with its step open gives the turn back)
    GpuGuard(const GpuGuard&) = delete;
    GpuGuard& operator=(const GpuGuard&) = delete;
    // 0.56.0: a guard of its own (the spinning hole drawn per pixel, gpuSkyGuard): its marker file, the log it writes into (truncated at start only by
    // the guard that says so) and a prefix for its lines
    GpuGuard(const char* marker, const char* log, bool truncateLog, const char* prefix) : markerName_(marker), logName_(log), prefix_(prefix), truncateLog_(truncateLog) {}
    // 0.59.0: what the guarded thing is called in its lines and in offText ("" = the light paths' own wording)
    GpuGuard(const char* marker, const char* log, bool truncateLog, const char* prefix, const char* what) : markerName_(marker), logName_(log), prefix_(prefix), what_(what), truncateLog_(truncateLog) {}
    void appendToLog() { truncateLog_ = false; }                  // another guard started the log in this run
    void keepOldMarkers() { keepOld_ = true; }                    // 0.63.1: the light paths' guards: a crash kept by an older version stays trusted
    static constexpr const char* kMarker = "gpu-paths-step.txt";   // plain ASCII names (Windows)
    static constexpr const char* kLog = "gpu-paths-log.txt";
    static constexpr int kLogMax = 400;                           // lines; later ones are dropped (one line says so)
    // folder: the settings file's folder with its separator at the end ("" = no files: the marker's logic still runs); saved: the settings file's line.
    // Reads and keeps a marker left behind (removed once the settings file holds what it said: settingsWritten), opens the log and writes its first lines.
    void start(const std::string& folder, const std::string& version, const std::string& driver, const std::string& saved, const std::vector<std::string>& head);
    bool started() const { return started_; }
    bool allowed() const { return d_.allowed; }
    bool guarded() const { return d_.guarded; }
    bool crashed() const { return d_.crashed; }
    const GpuGuardRecord& kept() const { return d_.keep; }
    std::string saved() const { return gpuGuardText(d_.keep); }
    int generation() const { return generation_; }                // grows at each "Try again" (the view starts its path afresh)
    // the line for Settings, Performance when switched off by the guard ("" when not)
    std::string offText() const;
    // a step of the card's path: logged; while guarded the marker names it first (flushed and committed); PL_GPU_DIE=<start of a step's name> ends the
    // process inside that step (tests: as if the driver had crashed there)
    // 0.63.0: at most one guarded step open at a time in the whole process, across all guards (a crash then leaves one marker, naming the guilty step).
    // A worker's begin waits for its turn; on the frame's thread (gpuGuardFrameThread) begin never waits: a step the frame starts begins with hold() first,
    // and moves to a later frame while hold() says no. end() removes the marker (no step open) and gives the turn back unless the guard holds it.
    void begin(const std::string& step);
    void end(const std::string& step, const std::string& result = std::string());
    // 0.63.0: the frame takes the turn for a run of steps (never waits; false: another guard's step is open, logged once, ask again next frame); true at once
    // when the guard is not guarded (its steps write no markers). The turn is given back by rest() (a gap of unknown length follows, such as the wait for
    // the first full use), finished(), timedOut() or failed().
    bool hold();
    void rest();
    void log(const std::string& line);
    // the sequence of steps finished: the marker removed; ok and guarded: "ok" kept (no markers any more for this version and driver)
    void finished(bool ok, const std::string& why = std::string());
    // a step that did not finish within its time limit: the path off for this run, kept as "slow" with the step's name; the marker removed
    void timedOut(const std::string& step);
    // 0.59.0: a step that returned an error (the driver said no; no crash): the path off for this run, kept as "fail" with the step and why; the marker removed
    void failed(const std::string& step, const std::string& why);
    void tryAgain();                                              // Settings, Performance: forget the kept "off", try the path again (guarded)
    // 0.67.0 (app/gpu_helper.h): its first steps are made first by a helper process; hold() says no until that helper has finished
    void setHelper(const std::string& name);
    // the helper died inside step (the driver crashed there): the path off, kept as "off" with "the helper died making <step>" (as a crash in the app)
    void helperDied(const std::string& step, const std::string& how, const char* verb = "died");
    void closing();                                               // the app ends normally: a marker of this run's still open step is removed (logged)
    void settingsWritten();                                       // the settings file now holds saved(): a marker from the last run can go
    std::string folder() const { return folder_; }
    std::string logPath() const { return folder_.empty() ? std::string() : folder_ + logName_; }
    std::string markerPath() const { return folder_.empty() ? std::string() : folder_ + markerName_; }
    std::vector<std::string> logTail(int n) const;                // the log's last n lines (read back from the file)
private:
    std::string markerName_ = kMarker, logName_ = kLog, prefix_, what_;
    bool truncateLog_ = true;
    bool keepOld_ = false;                                        // (the default guard, the light paths, sets it in its constructor)
    mutable std::mutex mx_;
    bool started_ = false, staleMarker_ = false, logFull_ = false;
    std::string folder_, version_, driver_, die_, app_;
    GpuGuardDecision d_;
    int generation_ = 0, lines_ = 0;
    double t0_ = 0;
    bool held_ = false;
    std::string helper_, helperLogged_;                           // 0.67.0: the helper it waits for, and its "waits" line once
    int open_ = 0;                                                // guarded steps of this guard open now (two workers of one guard may overlap)
    std::string waitLogged_;
    bool waiting_ = false;                                        // its last hold() / frame begin was told to wait (counted in gpuGuardBusy)
    void setWaiting(bool w);
    std::string name() const;
    bool takeTurn(const std::string& step, bool mayWait);
    void giveTurn();
    void writeMarker(const std::string& step);
    void removeMarker();
    void line(const std::string& text, bool commit);
};

GpuGuard& gpuGuard();                                             // the app's one (0.55.6's compute shader for the table)
// 0.56.0: the spinning hole's sky and disc drawn per pixel (render/kerr_sky.h): its own marker (gpu-sky-step.txt), the settings line "gpusky=", the same
// log (its lines start "[sky] "), its version field "sky <hash>" (render/kerr_sky.h kerrSkyShaderId)
GpuGuard& gpuSkyGuard();
// 0.57.0: the Atom Lab's two card programs (render/atom_card.h), a guard each: the Slice's new dots (gpu-slice-step.txt, the settings line "gpuslice=",
// log lines "[slice] ", version "slice <hash>") and the groups of a jump with every electron (gpu-jump-step.txt, "gpujump=", "[jump] ", "jump <hash>")
GpuGuard& gpuSliceGuard();
GpuGuard& gpuJumpGuard();
// 0.57.1: the Molecule Lab's Density cloud on the card (render/molecule_card.h): its own marker (gpu-molcloud-step.txt), the settings line "gpumolcloud=",
// the same log (its lines start "[molcloud] "), its version field "molcloud <hash>"
GpuGuard& gpuMolCloudGuard();
// 0.59.0: Vulkan (step 1 of 5, the Vulkan plan, done in 0.69.0 (git keeps it)): the window's Vulkan start (instance, surface, device, swapchain, first frame), its own marker
// (gpu-vulkan-step.txt), the settings line "gpuvulkan=", the same log (its lines start "[vulkan] "; it starts the log when Vulkan is tried), its version
// field "vulkan <app version>" (the Vulkan code changes with each step, so a new version tries once more)
GpuGuard& gpuVulkanGuard();

// 0.61.0 (Vulkan step 3 of 5): the card programs' guards on Vulkan (markers gpu-vk<name>-step.txt, settings lines "gpuvk<name>=", log lines
// "[vulkan] [<name>] ", versions "vulkan <name> <hash>"): gpuSliceGuard, gpuJumpGuard, gpuMolCloudGuard, gpuGuard (the light paths) and gpuSkyGuard give
// Vulkan's, and the superposition and jump groups (gpuVkGroupsGuard, "gpuvkgroups="), the 90 % outline
// (gpuVkOutlineGuard, "gpuvkoutline="), the belts' rocks (gpuVkRocksGuard, "gpuvkrocks=") and the Cloud 3D Glow dots (gpuVkCloudGuard, "gpuvkcloud=").
GpuGuard& gpuVkGroupsGuard();
GpuGuard& gpuVkOutlineGuard();
GpuGuard& gpuVkRocksGuard();
GpuGuard& gpuVkCloudGuard();
// 0.62.0 (Vulkan step 4): gpuGuard (the light paths) and gpuSkyGuard (the sky per pixel) give Vulkan's twins too (gpu-vklightpaths-step.txt, "gpuvklightpaths=",
// "[vulkan] [lightpaths] ", "vulkan lightpaths <hash>"; gpu-vksky-step.txt, "gpuvksky=", "[vulkan] [sky] "), and the Black Hole Lab's own passes on Vulkan
// (the lens, the table pass, the exposure, the glow, the glare) have one: gpuVkLensGuard ("gpuvklens=", "[vulkan] [lens] ")
GpuGuard& gpuVkLensGuard();
// 0.64.0: Saturn's rings per pixel on Vulkan (render/ring_card.h, orbit/ring_check.h): gpuVkRingsGuard ("gpuvkrings=", "[vulkan] [rings] ")
GpuGuard& gpuVkRingsGuard();
// 0.64.0: the Classic Cloud 3D dots on Vulkan (render/classic_card.h, atom/classic_check.h): gpuVkClassicGuard ("gpuvkclassic=", "[vulkan] [classic] ")
GpuGuard& gpuVkClassicGuard();
// 0.67.0: the nucleus' Density look on Vulkan (render/nucdens_card.h, atom/nucdens_check.h): gpuVkNucDensGuard ("gpuvknucdensity=", "[vulkan] [nucdensity] ")
GpuGuard& gpuVkNucDensGuard();
// 0.68.0: the Orbit Lab's lines on Vulkan (render/lines_card.h, orbit/lines_check.h): gpuVkLinesGuard ("gpuvklines=", "[vulkan] [lines] ")
GpuGuard& gpuVkLinesGuard();
// 0.68.0: the Molecule Lab's Density surface on Vulkan (render/surface_card.h, molecule/surface_check.h): gpuVkSurfaceGuard ("gpuvksurface=", "[vulkan] [surface] ")
GpuGuard& gpuVkSurfaceGuard();
std::vector<std::pair<std::string, GpuGuard*>> vulkanCardGuards();   // the Vulkan guards by their settings name (gpuvk<name>=)

// 0.63.0: the frame's thread (main.cpp, at start): a guarded begin() there never waits for another guard's open step (every other thread does)
void gpuGuardFrameThread();
// the guard whose step (or run of steps) has the turn now, and that step ("" when none: tests)
std::string gpuGuardTurn();
// a guarded step is open or waits for its turn (the start-up of a card program is under way: the labs are not settled; tests, the benchmark's waits)
bool gpuGuardBusy();

// 0.68.0: the helper process (render/vk_helper.cpp): f is told of every guarded step of this process (what: 0 begun, 1 ended with result, 2 failed with
// why, 3 timed out), before PL_GPU_DIE could end it; null: none
void gpuGuardObserve(void (*f)(int what, const std::string& step, const std::string& result));
// 0.68.0: the app ends: a worker waiting for the turn goes on (a frame guard may still hold it: VkBlackHole's destroy waits for its worker)
void gpuGuardEndAll();

// the file helpers (tests): write a whole small file and commit it to disk (C runtime flush, then _commit on Windows, fsync elsewhere)
bool writeCommitted(const std::string& path, const std::string& text);

}  // namespace app
