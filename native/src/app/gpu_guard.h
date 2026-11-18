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
struct GpuGuardRecord {
    std::string state, version, driver, step;
    bool operator==(const GpuGuardRecord& o) const { return state == o.state && version == o.version && driver == o.driver && step == o.step; }
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
};
GpuGuardDecision gpuGuardDecide(const GpuGuardRecord& saved, const GpuGuardRecord& marker, const std::string& version, const std::string& driver);

class GpuGuard {
public:
    GpuGuard() {}
    ~GpuGuard() { giveTurn(); }                                   // (0.63.0: a guard dropped with its step open gives the turn back)
    GpuGuard(const GpuGuard&) = delete;
    GpuGuard& operator=(const GpuGuard&) = delete;
    // 0.56.0: a guard of its own (the spinning hole drawn per pixel, gpuSkyGuard): its marker file, the log it writes into (truncated at start only by
    // the guard that says so) and a prefix for its lines
    GpuGuard(const char* marker, const char* log, bool truncateLog, const char* prefix) : markerName_(marker), logName_(log), prefix_(prefix), truncateLog_(truncateLog) {}
    // 0.59.0: what the guarded thing is called in its lines and in offText ("" = the light paths' own wording)
    GpuGuard(const char* marker, const char* log, bool truncateLog, const char* prefix, const char* what) : markerName_(marker), logName_(log), prefix_(prefix), what_(what), truncateLog_(truncateLog) {}
    void appendToLog() { truncateLog_ = false; }                  // 0.59.0: another guard started the log in this run (the Vulkan guard, before OpenGL's)
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
    void closing();                                               // the app ends normally: a marker of this run's still open step is removed (logged)
    void settingsWritten();                                       // the settings file now holds saved(): a marker from the last run can go
    std::string folder() const { return folder_; }
    std::string logPath() const { return folder_.empty() ? std::string() : folder_ + logName_; }
    std::string markerPath() const { return folder_.empty() ? std::string() : folder_ + markerName_; }
    std::vector<std::string> logTail(int n) const;                // the log's last n lines (read back from the file)
private:
    std::string markerName_ = kMarker, logName_ = kLog, prefix_, what_;
    bool truncateLog_ = true;
    mutable std::mutex mx_;
    bool started_ = false, staleMarker_ = false, logFull_ = false;
    std::string folder_, version_, driver_, die_;
    GpuGuardDecision d_;
    int generation_ = 0, lines_ = 0;
    double t0_ = 0;
    bool held_ = false;
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
// 0.59.0: Vulkan (step 1 of 5, native/PLAN_VULKAN.md): the window's Vulkan start (instance, surface, device, swapchain, first frame), its own marker
// (gpu-vulkan-step.txt), the settings line "gpuvulkan=", the same log (its lines start "[vulkan] "; it starts the log when Vulkan is tried), its version
// field "vulkan <app version>" (the Vulkan code changes with each step, so a new version tries once more)
GpuGuard& gpuVulkanGuard();

// 0.61.0 (Vulkan step 3 of 5): useVulkanGuards(true) at a Vulkan start: gpuSliceGuard, gpuJumpGuard and gpuMolCloudGuard then give Vulkan's twins (markers
// gpu-vk<name>-step.txt, settings lines "gpuvk<name>=", log lines "[vulkan] [<name>] ", versions "vulkan <name> <hash>"), so a failure on one API never switches
// the other off. The four card programs that have no guard of their own on OpenGL (made with the renderer there) have one on Vulkan: the superposition and
// jump groups (gpuVkGroupsGuard, "gpuvkgroups="), the 90 % outline (gpuVkOutlineGuard, "gpuvkoutline="), the belts' rocks (gpuVkRocksGuard, "gpuvkrocks=")
// and the Cloud 3D Glow dots (gpuVkCloudGuard, "gpuvkcloud=").
void useVulkanGuards(bool on);
bool vulkanGuards();
GpuGuard& gpuVkGroupsGuard();
GpuGuard& gpuVkOutlineGuard();
GpuGuard& gpuVkRocksGuard();
GpuGuard& gpuVkCloudGuard();
// 0.62.0 (Vulkan step 4): gpuGuard (the light paths) and gpuSkyGuard (the sky per pixel) give Vulkan's twins too (gpu-vklightpaths-step.txt, "gpuvklightpaths=",
// "[vulkan] [lightpaths] ", "vulkan lightpaths <hash>"; gpu-vksky-step.txt, "gpuvksky=", "[vulkan] [sky] "), and the Black Hole Lab's own passes on Vulkan
// (the lens, the table pass, the exposure, the glow, the glare) have one: gpuVkLensGuard ("gpuvklens=", "[vulkan] [lens] ")
GpuGuard& gpuVkLensGuard();
std::vector<std::pair<std::string, GpuGuard*>> vulkanCardGuards();   // the ten Vulkan guards by their settings name (gpuvk<name>=)

// 0.63.0: the frame's thread (main.cpp, at start): a guarded begin() there never waits for another guard's open step (every other thread does)
void gpuGuardFrameThread();
// the guard whose step (or run of steps) has the turn now, and that step ("" when none: tests)
std::string gpuGuardTurn();
// a guarded step is open or waits for its turn (the start-up of a card program is under way: the labs are not settled; tests, the benchmark's waits)
bool gpuGuardBusy();

// the file helpers (tests): write a whole small file and commit it to disk (C runtime flush, then _commit on Windows, fsync elsewhere)
bool writeCommitted(const std::string& path, const std::string& text);

}  // namespace app
