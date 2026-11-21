// The automatic benchmark: the numbers, the run's timing, and the result text. No graphics in here, so it can be tested.
// What the app does with them (the scenes, the window, the restoring of the settings) is in ui/benchmark.cpp.
#pragma once

#include <string>
#include <vector>

#include "app/gpu_timer.h"
#include "app/shot.h"

namespace app {

// 0.56.1: the default run ("Hard scenes") takes about four minutes: every scene's warm-up and counted time are 0.65 of 0.56.0's 2 s and 8 s (bench_scenes.cpp
// holds the whole run's planned length with a static_assert; app/bench.h kBenchPlannedS)
constexpr double kBenchWarmupS = 1.3;      // not counted
constexpr double kBenchMeasureS = 5.2;     // counted
constexpr double kBenchSlowMs = 16.7;      // a frame slower than this is counted as slow (60 fps = 16.67 ms; the report says 16.7)

// What the frame times of one scene come to.
struct BenchStats {
    int frames = 0;
    double totalS = 0;           // the time the frames took together
    double avgMs = 0;            // total / frames
    double avgFps = 0;           // 1000 / avgMs
    double low1Fps = 0;          // the "1 % low": 1000 / (the average of the slowest 1 % of the frames, at least one frame)
    double worstMs = 0;          // the slowest frame
    int slowFrames = 0;          // 0.16 stage 3: frames slower than kBenchSlowMs (below 60 fps): 1 = a one-time hitch, many = a repeating one
};
BenchStats benchStats(const std::vector<double>& frameMs);

// 0.63.1: where one frame's time went (main.cpp from app/frame_cost.h; the benchmark keeps each scene's three worst frames). The frame's time runs from
// one frame's start to the next: the frame thread's work, the waits for the card inside it, then the present.
struct BenchFrameCost {
    double frameMs = 0, workMs = 0;              // the frame's time; the frame thread's work (its waits for the card taken out)
    double cardWaitMs = 0, acquireMs = 0, presentMs = 0;   // the fence of the swapchain image's last frame; its next image; the present
    double pipelineMs = 0, readbackMs = 0, swapchainMs = 0, workerWaitMs = 0;
    int pipelines = 0, readbacks = 0, swapchains = 0, workerWaits = 0, deferred = 0;   // pipelines made, batches waited for, swapchain made again, guard steps deferred
    double beforeMs = 0;                         // the benchmark's own step before the frame's clock (its scene's set-up and movement)
    double eventsMs = 0, bookMs = 0, settingsMs = 0;   // the window's events; the benchmark's numbers; the settings file saved (after the frame's clock)
    std::string readbackWhat;                    // the longest batch waited for, by name ("" not named)
};
std::string benchFrameCostText(const BenchFrameCost& c);   // "969.2 ms: frame thread 3.1 ms, waited for the card 0.2 ms, ..."

struct BenchScene {
    std::string id;              // 0.56.1: the stable short id ("orbit.saturn"), the same in every version
    std::string name;            // "Orbit Lab, Saturn close up"
    std::string what;            // what the scene tests, in two or three lines
    std::string line;            // 0.56.1: one short line of its own (an Atom Lab scene: the settings it really ran with)
    std::string path;            // 0.56.1: which path ran (graphics card or processor) and why; "" where the scene has one path only
    long pictures = -1;          // 0.56.1: new pictures while counted (a picture made off the frame); -1: every frame draws a new picture
    double picturesS = -1;       // 0.67.0: the counted seconds whose frames needed a new picture (the view changed); < 0: all of the counted time
    std::string picturesBy;      //   what makes those pictures: "processor" or "graphics card" (the summary's "limited by" when they are behind the frames)
    BenchStats frames;
    double cpuAvgMs = 0;         // work per frame on the CPU (the card's "Work per frame")
    double gpuAvgMs = -1;        // GPU busy time per frame (GL_TIME_ELAPSED over the whole drawing of the frame); < 0: no result
    double gpuSpanAvgMs = -1;    // first to last GPU command of the frame (timestamps), waiting included; < 0: no result
    int gpuSamples = 0;          // frames that gave a GPU result
    std::string gpuProblem;      // "" if the GPU numbers look right; else why they are not reliable
    bool windowInactive = false; // 0.15 stage 2: the window lost focus or was minimised at some time during this scene (warm-up or measuring): Windows can slow a background app
    std::vector<std::string> notes;   // 0.55.5: lines of the scene's own (the Black Hole Lab's: benchBlackHoleLines), printed after its numbers
    double waitAvgMs = -1;       // 0.63.1: the frame thread's waits for the card a frame (fence, acquire, batches read back), not in cpuAvgMs; < 0 not measured
    std::vector<BenchFrameCost> worst;   // 0.63.1: the three worst counted frames, worst first, with where their time went
    // 0.67.0: over the time the view moved (picturesS) where the scene has rests: a frame at rest needs no new picture (blackhole.film_tilt rests 40 % of its
    // counted time, real_move about a third: until 0.67.0 their pictures were divided by the whole time, so they could never reach the frames' rate)
    double picturesPerS() const { const double t = picturesS > 0 ? picturesS : frames.totalS; return pictures < 0 ? frames.avgFps : t > 0 ? pictures / t : 0; }
};

// 0.56.1: the summary table at the head of the report, slowest first. Rules (said in the report under the table):
//   reaches the screen's rate: yes when the 1 % low AND the new pictures a second are at least the display's refresh rate; no when either is below; "not known"
//     when the refresh rate is not known.
//   limited by (from the frame's CPU work c and the GPU's busy time g, both ms a frame): processor when c >= 1.5 g, graphics card when g >= 1.5 c, else both; with no
//     reliable GPU time "processor?" when c is at least 80 % of the frame time, else "not known". Where the new pictures a second are under 90 % of the frames a
//     second, the pictures are made off the frame and the scene names what makes them (BenchScene::picturesBy) instead.
//   order: slowest first by the lower of the 1 % low and the new pictures a second.
struct BenchSummaryRow { std::string id, name; double avgFps = 0, low1Fps = 0, picsPerS = 0; int reaches = -1; std::string limitedBy; };   // reaches: 1 yes, 0 no, -1 not known
int benchReaches(double low1Fps, double picsPerS, double refreshHz);
std::string benchLimitedBy(const BenchScene& s, bool gpuReliable);
std::vector<BenchSummaryRow> benchSummary(const std::vector<BenchScene>& scenes, double refreshHz, bool gpuReliable);

// 0.56.1, Part 2: how long the user waits. Each action is done kBenchResponseRepeats times by the benchmark, VSync on as the user has it; the time is from
// the action to the end of the frame that draws the first finished picture (not the first frame), with the worst single frame during the wait.
constexpr int kBenchResponseRepeats = 3;
constexpr double kBenchResponseCapS = 15.0;     // a wait longer than this is stopped and reported as "more than 15 s"
struct BenchResponse {
    std::string id, name, what;
    std::vector<double> ms;                     // each try (ms); a try that hit the cap is kBenchResponseCapS * 1000
    std::vector<double> worstFrameMs;           // the worst frame during each try
    int capped = 0;                             // tries that hit the cap
};
double benchMiddle(std::vector<double> v);      // the middle value (the median: the mean of the two middle ones for an even count); 0 for none
double benchWorst(const std::vector<double>& v);

// 0.56.1, Part 3: each lab left alone for kBenchIdleS after its picture is finished (VSync on, as the user has it).
// Rule: "busy" when a worker thread was busy at any time, or the kept picture was drawn anew (a lab that keeps its picture), or the app used more than
// kBenchIdleBusyThread of one processor thread (all its threads together); else "quiet".
constexpr double kBenchIdleS = 3.0;
constexpr double kBenchIdleSettleCapS = 4.0;    // waiting for the finished picture before the 3 s: at most this long
constexpr double kBenchIdleBusyThread = 0.25;
struct BenchIdle {
    std::string id, name;
    double cpuShare = -1;                       // the app's share of the whole processor (all threads, 0..1); -1 not available
    int threads = 0;                            // the processor's threads (to say the share as a part of one thread)
    double gpuMs = -1;                          // the graphics card's busy time a frame; -1 not available
    int frames = 0;
    bool keepsPicture = false;                  // the lab keeps a finished picture (the Black Hole Lab); the others draw every frame
    long redraws = 0;                           // ... how many times it was drawn anew while left alone
    bool workerBusy = false;
    bool settled = true;                        // the picture was finished before the 3 s (else the waiting hit kBenchIdleSettleCapS)
};
bool benchIdleBusy(const BenchIdle& i);

// 0.55.5: the Black Hole Lab's own numbers in its scene: the frame thread's time in the lab and each worker stage's time, per movement (turning, tilting,
// changing distance), how long the picture was behind the settings, and the stages made only at rest (the glare's veil, the fine spin table).
struct BenchStage {
    int n = 0;
    double sum = 0, worst = 0;
    void add(double v) { n++; sum += v; if (v > worst) worst = v; }
};
struct BenchBhMove {
    int frames = 0, behindFrames = 0;
    int pictures = 0;                                       // 0.55.6: new pictures while moving, over seconds of moving
    double seconds = 0;
    double frameSum = 0, frameWorst = 0, behindWorst = 0;   // the frame thread's time in the lab (ms); the longest time the picture was behind (ms)
    BenchStage coarse, bins, stars, look;                   // the worker's: the coarse table, the star bins, the stars' images, the exposure (ms each)
    double sharpMs = -1;                                    // 0.55.8: at spin, the first frame at rest showing the fine table, after the stop (ms; -1 none)
    double restS = 0;                                       // 0.55.8: the slot's time at rest (s)
    int skyPictures = 0;                                    // 0.56.0: new pictures of sky and disc drawn by the card while moving
    double scaleMin = 1;                                    //   the lowest resolution's scale while moving (of the picture's width)
    int fullPictures = 0;                                   //   0.56.2: of those, drawn at every pixel
    BenchStage disc;                                        //   the worker's disc table (spin 0)
};
struct BenchBlackHole {
    BenchBhMove move[6];                                    // turning round the axis, tilting, changing distance; at spin 0.9, then at spin 0 (0.55.6)
    BenchStage glare, fine;                                 // at rest: the glare's veil (its own thread), the fine spin table (its own threads)
    BenchStage card;                                        // 0.55.6: the graphics card's fills of the spin table (from its start to the frame that found it done)
    std::string path;                                       // 0.55.6: which path filled the spin table and why (the start-up check)
    double glareWaitWorst = 0;                              // the longest wait from a picture to its glare (ms)
    double fineStartMs = 0;                                 // 0.55.8: the fine spin table made at the scene's start (all cores, normal priority)
    std::string skyPath, lookPath;                          // 0.56.0: which path drew the sky and disc and why; the exposure's path
    BenchStage skyGpu;                                      //   the card's pass for the whole picture (its own timer)
};
std::vector<std::string> benchBlackHoleLines(const BenchBlackHole& b);

// While the benchmark runs nothing may change the labs or the app except its Cancel button and Esc, so a run cannot be faked:
// the picture's wheel, drag, clicks and the keyboard shortcuts (Space, F11) ask this first; the panels are blocked by a modal box (ui/benchmark.cpp).
inline bool labInputAllowed(bool benchRunning) { return !benchRunning; }

// The sentence the Performance card and the benchmark report use for the Atom Lab's superposition and jumps (0.13 stage 2). computeOk: the renderer has the OpenGL 4.3 compute path;
// switchOn: the "GPU superposition and jumps" switch (AtomLab::gpuCompute).
inline std::string computePathText(int major, int minor, bool computeOk, bool switchOn) {
    const std::string v = major == 0 ? std::string("Vulkan") : "OpenGL " + std::to_string(major) + "." + std::to_string(minor);   // (0.61.0: major 0 is Vulkan)
    if (!computeOk) return v + ": CPU path";
    return v + (switchOn ? ": GPU superposition on" : ": GPU superposition off (switched off, CPU path)");
}

// The same for the 90 % outline (0.13 stage 3).
inline std::string outlinePathText(int major, int minor, bool computeOk, bool switchOn) {
    const std::string v = major == 0 ? std::string("Vulkan") : "OpenGL " + std::to_string(major) + "." + std::to_string(minor);
    if (!computeOk) return v + ": CPU path";
    return v + (switchOn ? ": GPU outline on" : ": GPU outline off (switched off, CPU path)");
}

// The same for the Orbit Lab's rocks (0.13 stage 4). rocksOk: the renderer has the rock compute object (OpenGL 4.3 and a shader with double precision that compiled).
// 0.61.0: why (Vulkan): why the card does not place them (no double precision in its shaders: the precision rule keeps the rocks' time in double)
inline std::string rocksPathText(int major, int minor, bool rocksOk, bool switchOn, const std::string& why = std::string()) {
    const std::string v = major == 0 ? std::string("Vulkan") : "OpenGL " + std::to_string(major) + "." + std::to_string(minor);
    if (!rocksOk) return v + ": CPU path" + (why.empty() ? std::string() : " (" + why + ")");
    return v + (switchOn ? ": GPU rocks on" : ": GPU rocks off (switched off, CPU path)");
}

struct BenchEnv {
    std::string version, when;                   // the app version; the local date and time
    std::string gpu, glVersion, vendor;          // GL_RENDERER, GL_VERSION, GL_VENDOR (0.63.0, Vulkan: the device, its Vulkan version and present mode, the driver)
    std::string api;                             // 0.63.0: "Vulkan" (0.69.0: the only drawing system)
    std::string outlinePath;                     // outlinePathText() (0.13 stage 3)
    std::string rocksPath;                       // rocksPathText() (0.13 stage 4)
    std::string computePath;                     // computePathText(): "OpenGL 4.3: GPU superposition on" or "OpenGL 3.3: CPU path" (0.13 stage 2)
    int windowW = 0, windowH = 0;                // the window, in pixels
    int pictureW = 0, pictureH = 0;              // the picture area, in pixels
    double refreshHz = 0;                        // the display's refresh rate (0: unknown)
    std::string textSize, theme;
    bool fullscreen = false, gpuTimer = false;   // gpuTimer: the driver gave GPU times
    bool gpuReliable = false;                    // ... and the self-check with a known load agreed with a clock
    std::string gpuCheck;                        // the self-check in words (how well it agreed, or why it did not)
    double warmupS = kBenchWarmupS, measureS = kBenchMeasureS;
    bool cancelled = false;                      // stopped before the end: only the scenes done are listed
    std::string power;                           // 0.15 stage 2: app::powerNote() (Windows power throttling asked off or not)
    // 0.56.1: the run's list and the computer
    bool allScenes = false;                      // "All scenes" (else "Hard scenes")
    std::string cpu;                             // the processor's name ("" not available)
    int cores = 0, threads = 0;                  // 0: not available
    double memoryMB = -1, peakMB = -1;           // installed memory; the app's largest working set sampled during the run (-1 not available)
    double startupMs = -1;                       // from the start of main() to the first finished picture (-1 not measured)
    std::string startParts;                      // 0.63.1: the benchmark's own start, part by part ("" not measured)
};

// The plain text of a result: the head (version, date, computer, graphics, the run's times), the summary table, the response and idle tables, then
// every scene's details. For the clipboard and the file.
std::string benchResultText(const BenchEnv& env, const std::vector<BenchScene>& scenes, const std::vector<BenchResponse>& responses = {},
                            const std::vector<BenchIdle>& idle = {});
std::string benchFileName(const ShotTime& t);   // physics-lab-benchmark-2026-10-01-153045.txt

// Steps a run: warm-up, then measuring, scene after scene. The caller sets a scene up whenever sceneToSetUp() is >= 0
// (and says setUpDone()), then calls frame() once per frame with that frame's numbers.
class BenchRun {
public:
    BenchRun(int sceneCount, double warmupS = kBenchWarmupS, double measureS = kBenchMeasureS);
    int sceneToSetUp() const { return setup_; }
    void setUpDone() { setup_ = -1; }
    // One finished frame: its time (ms) and the CPU work (ms). count false (0.56.1): a frame that read the picture back for the scene's own check: while
    // counting it is neither counted nor on the scene's clock (reading back waits for every frame the driver still holds)
    // 0.63.1: cost (may be null): where the frame's time went; cpuMs is then the work without the waits for the card (cost->workMs)
    void frame(double frameMs, double cpuMs, bool count = true, const BenchFrameCost* cost = nullptr);
    // 0.15 stage 2: call once per frame BEFORE frame() with the window's state; a scene during which the window was not focused or was minimised is flagged (BenchScene::windowInactive).
    void noteWindow(bool focused, bool minimised);
    // A GPU result that came in (a frame or two late): the busy time and the span (ms). Counted only while a scene is being measured.
    void gpuSample(double busyMs, double spanMs);
    bool finished() const { return finished_; }
    int scene() const { return scene_; }
    int sceneCount() const { return count_; }
    bool measuring() const { return measuring_; }
    double warmupS() const { return warm_; }                    // 0.55.8
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
    std::vector<double> cpuSum_, gpuSum_, spanSum_, waitSum_;
    std::vector<int> cpuN_, gpuN_, waitN_;
    std::vector<std::vector<BenchFrameCost>> worst_;
    std::vector<char> inactive_;
};

}  // namespace app
