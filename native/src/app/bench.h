// The automatic benchmark: the numbers, the run's timing, and the result text. No graphics in here, so it can be tested.
// What the app does with them (the scenes, the window, the restoring of the settings) is in ui/benchmark.cpp.
#pragma once

#include <string>
#include <vector>

#include "app/gpu_timer.h"
#include "app/shot.h"

namespace app {

constexpr double kBenchWarmupS = 2.0;      // not counted
constexpr double kBenchMeasureS = 8.0;     // counted

// What the frame times of one scene come to.
struct BenchStats {
    int frames = 0;
    double totalS = 0;           // the time the frames took together
    double avgMs = 0;            // total / frames
    double avgFps = 0;           // 1000 / avgMs
    double low1Fps = 0;          // the "1 % low": 1000 / (the average of the slowest 1 % of the frames, at least one frame)
    double worstMs = 0;          // the slowest frame
};
BenchStats benchStats(const std::vector<double>& frameMs);

struct BenchScene {
    std::string name;            // "1. Orbit Lab: ..."
    std::string what;            // the settings, in words
    BenchStats frames;
    double cpuAvgMs = 0;         // work per frame on the CPU (the card's "Work per frame")
    double gpuAvgMs = -1;        // GPU busy time per frame (GL_TIME_ELAPSED over the whole drawing of the frame); < 0: no result
    double gpuSpanAvgMs = -1;    // first to last GPU command of the frame (timestamps), waiting included; < 0: no result
    int gpuSamples = 0;          // frames that gave a GPU result
    std::string gpuProblem;      // "" if the GPU numbers look right; else why they are not reliable
    bool windowInactive = false; // 0.15 stage 2: the window lost focus or was minimised at some time during this scene (warm-up or measuring): Windows can slow a background app
};

// While the benchmark runs nothing may change the labs or the app except its Cancel button and Esc, so a run cannot be faked:
// the picture's wheel, drag, clicks and the keyboard shortcuts (Space, F11) ask this first; the panels are blocked by a modal box (ui/benchmark.cpp).
inline bool labInputAllowed(bool benchRunning) { return !benchRunning; }

// The sentence the Performance card and the benchmark report use for the Atom Lab's superposition and jumps (0.13 stage 2). computeOk: the renderer has the OpenGL 4.3 compute path;
// switchOn: the "GPU superposition and jumps" switch (AtomLab::gpuCompute).
inline std::string computePathText(int major, int minor, bool computeOk, bool switchOn) {
    const std::string v = "OpenGL " + std::to_string(major) + "." + std::to_string(minor);
    if (!computeOk) return v + ": CPU path";
    return v + (switchOn ? ": GPU superposition on" : ": GPU superposition off (switched off, CPU path)");
}

// The same for the 90 % outline (0.13 stage 3).
inline std::string outlinePathText(int major, int minor, bool computeOk, bool switchOn) {
    const std::string v = "OpenGL " + std::to_string(major) + "." + std::to_string(minor);
    if (!computeOk) return v + ": CPU path";
    return v + (switchOn ? ": GPU outline on" : ": GPU outline off (switched off, CPU path)");
}

// The same for the Orbit Lab's rocks (0.13 stage 4). rocksOk: the renderer has the rock compute object (OpenGL 4.3 and a shader with double precision that compiled).
inline std::string rocksPathText(int major, int minor, bool rocksOk, bool switchOn) {
    const std::string v = "OpenGL " + std::to_string(major) + "." + std::to_string(minor);
    if (!rocksOk) return v + ": CPU path";
    return v + (switchOn ? ": GPU rocks on" : ": GPU rocks off (switched off, CPU path)");
}

struct BenchEnv {
    std::string version, when;                   // the native version; the local date and time
    std::string gpu, glVersion, vendor;          // GL_RENDERER, GL_VERSION, GL_VENDOR
    std::string outlinePath;                     // outlinePathText() (0.13 stage 3)
    std::string rocksPath;                       // rocksPathText() (0.13 stage 4)
    std::string computePath;                     // computePathText(): "OpenGL 4.3: GPU superposition on" or "OpenGL 3.3: CPU path" (0.13 stage 2)
    int windowW = 0, windowH = 0;                // the window, in pixels
    int pictureW = 0, pictureH = 0;              // the picture area, in pixels
    double refreshHz = 0;                        // the display's refresh rate (0: unknown)
    std::string textSize, theme;
    std::string effects = "on";                  // Settings → Theme effects (0.14 stage 3): "on" or "off"
    std::string newUi = "off";                   // Settings → New UI (0.15 stage 5d): "on" or "off"
    bool fullscreen = false, gpuTimer = false;   // gpuTimer: the driver gave GPU times
    bool gpuReliable = false;                    // ... and the self-check with a known load agreed with a clock
    std::string gpuCheck;                        // the self-check in words (how well it agreed, or why it did not)
    double warmupS = kBenchWarmupS, measureS = kBenchMeasureS;
    bool cancelled = false;                      // stopped before the end: only the scenes done are listed
    std::string atomSettings;                    // the Atom Lab settings each Atom Lab scene really ran with (so a wrong run shows)
    std::string power;                           // 0.15 stage 2: app::powerNote() (Windows power throttling asked off or not)
};

// The plain text of a result (starts with the version, the date and the computer's graphics), for the clipboard and the file.
std::string benchResultText(const BenchEnv& env, const std::vector<BenchScene>& scenes);
std::string benchFileName(const ShotTime& t);   // physics-lab-benchmark-2026-10-01-153045.txt

// Steps a run: warm-up, then measuring, scene after scene. The caller sets a scene up whenever sceneToSetUp() is >= 0
// (and says setUpDone()), then calls frame() once per frame with that frame's numbers.
class BenchRun {
public:
    BenchRun(int sceneCount, double warmupS = kBenchWarmupS, double measureS = kBenchMeasureS);
    int sceneToSetUp() const { return setup_; }
    void setUpDone() { setup_ = -1; }
    // One finished frame: its time (ms) and the CPU work (ms).
    void frame(double frameMs, double cpuMs);
    // 0.15 stage 2: call once per frame BEFORE frame() with the window's state; a scene during which the window was not focused or was minimised is flagged (BenchScene::windowInactive).
    void noteWindow(bool focused, bool minimised);
    // A GPU result that came in (a frame or two late): the busy time and the span (ms). Counted only while a scene is being measured.
    void gpuSample(double busyMs, double spanMs);
    bool finished() const { return finished_; }
    int scene() const { return scene_; }
    int sceneCount() const { return count_; }
    bool measuring() const { return measuring_; }
    double sceneElapsedS() const { return elapsed_; }          // into the warm-up or the measuring part
    double totalS() const { return count_ * (warm_ + measure_); }
    double doneS() const;                                      // how much of totalS has passed
    BenchScene result(int i) const;                            // the numbers of scene i (names are filled in by the caller)
    int doneScenes() const { return finished_ ? count_ : scene_; }

private:
    int count_, scene_ = 0, setup_ = 0;
    double warm_, measure_, elapsed_ = 0;
    bool measuring_ = false, finished_ = false;
    std::vector<std::vector<double>> frameMs_;
    std::vector<double> cpuSum_, gpuSum_, spanSum_;
    std::vector<int> cpuN_, gpuN_;
    std::vector<char> inactive_;
};

}  // namespace app
