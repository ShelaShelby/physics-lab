// The automatic benchmark: the numbers, the run's timing, and the result text. No graphics in here, so it can be tested.
// What the app does with them (the scenes, the window, the restoring of the settings) is in ui/benchmark.cpp.
#pragma once

#include <string>
#include <vector>

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
    double gpuAvgMs = -1;        // GPU time per frame (GL_TIME_ELAPSED); < 0: not available
    int gpuSamples = 0;
};

struct BenchEnv {
    std::string version, when;                   // the native version; the local date and time
    std::string gpu, glVersion, vendor;          // GL_RENDERER, GL_VERSION, GL_VENDOR
    int windowW = 0, windowH = 0;                // the window, in pixels
    int pictureW = 0, pictureH = 0;              // the picture area, in pixels
    double refreshHz = 0;                        // the display's refresh rate (0: unknown)
    std::string textSize, theme;
    bool fullscreen = false, gpuTimer = false;
    double warmupS = kBenchWarmupS, measureS = kBenchMeasureS;
    bool cancelled = false;                      // stopped before the end: only the scenes done are listed
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
    // One finished frame: its time (ms), the CPU work (ms), and the GPU time (ms; < 0 if not measured).
    void frame(double frameMs, double cpuMs, double gpuMs);
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
    std::vector<double> cpuSum_, gpuSum_;
    std::vector<int> cpuN_, gpuN_;
};

}  // namespace app
