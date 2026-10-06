#include "app/bench.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numeric>

namespace app {

BenchStats benchStats(const std::vector<double>& frameMs) {
    BenchStats s;
    s.frames = int(frameMs.size());
    if (frameMs.empty()) return s;
    const double sum = std::accumulate(frameMs.begin(), frameMs.end(), 0.0);
    s.totalS = sum / 1000;
    s.avgMs = sum / frameMs.size();
    s.avgFps = s.avgMs > 0 ? 1000 / s.avgMs : 0;
    s.worstMs = *std::max_element(frameMs.begin(), frameMs.end());
    std::vector<double> sorted = frameMs;
    std::sort(sorted.begin(), sorted.end(), std::greater<double>());
    const std::size_t n = std::max<std::size_t>(1, std::size_t(std::ceil(frameMs.size() * 0.01)));
    const double slow = std::accumulate(sorted.begin(), sorted.begin() + n, 0.0) / n;
    s.low1Fps = slow > 0 ? 1000 / slow : 0;
    return s;
}

namespace {
std::string f(const char* fmt, double a) { char b[64]; std::snprintf(b, sizeof b, fmt, a); return b; }
}

std::string benchResultText(const BenchEnv& e, const std::vector<BenchScene>& scenes) {
    std::string t;
    t += "Physics Lab (native) benchmark\n";
    t += "Version: " + e.version + "\n";
    t += "Date: " + e.when + "\n";
    t += "GPU: " + (e.gpu.empty() ? "unknown" : e.gpu) + "\n";
    t += "OpenGL: " + (e.glVersion.empty() ? "unknown" : e.glVersion) + "\n";
    t += "Atom Lab superposition and jumps: " + (e.computePath.empty() ? "unknown" : e.computePath) + "\n";
    t += "Atom Lab 90 % outline: " + (e.outlinePath.empty() ? "unknown" : e.outlinePath) + "\n";
    t += "Orbit Lab rocks: " + (e.rocksPath.empty() ? "unknown" : e.rocksPath) + "\n";
    t += "Driver / vendor: " + (e.vendor.empty() ? "not available" : e.vendor) + "\n";
    t += "Window: " + std::to_string(e.windowW) + " x " + std::to_string(e.windowH) + " pixels" + (e.fullscreen ? ", full screen" : ", not full screen") + "\n";
    t += "Picture area: " + std::to_string(e.pictureW) + " x " + std::to_string(e.pictureH) + " pixels\n";
    t += "Display refresh rate: " + (e.refreshHz > 0 ? f("%.0f", e.refreshHz) + " Hz" : std::string("not available")) + "\n";
    t += "Text size: " + e.textSize + ", theme: " + e.theme + "\n";
    t += std::string("Theme effects: ") + (e.effects == "off" ? "off" : "on") + "\n";
    t += std::string("New UI: ") + (e.newUi == "on" ? "On" : "Off") + "\n";             // (0.15 stage 5d: the two layouts can differ in speed)
    if (!e.power.empty()) t += e.power + "\n";
    t += "VSync: off during the test. Each scene: " + f("%.0f", e.warmupS) + " s warm-up (not counted), then " + f("%.0f", e.measureS) + " s counted.\n";
    t += std::string("GPU timer (GL_TIME_ELAPSED): ") + (e.gpuTimer ? "available" : "not available") + "\n";
    if (e.gpuTimer) t += std::string("GPU timer check: ") + (e.gpuReliable ? "reliable, " : "NOT RELIABLE, ") + (e.gpuCheck.empty() ? "no check was made" : e.gpuCheck) + "\n";
    t += "GPU time covers the whole drawing of a frame (clear, picture with its soft layer and instanced draws, panels and text), up to just before the buffer swap.\n";
    t += "Input during the test: blocked\n";
    if (!e.atomSettings.empty()) t += "Atom Lab settings used: " + e.atomSettings + "\n";
    if (e.cancelled) t += "THE TEST WAS CANCELLED: only the finished scenes are listed.\n";
    {   // 0.15 stage 2: scenes during which the window lost focus or was minimised (Windows can slow a background app, so those numbers may be lower than the PC can do)
        int inactive = 0;
        for (const BenchScene& s : scenes) if (s.windowInactive) inactive++;
        if (inactive) t += "WARNING: in " + std::to_string(inactive) + " scene(s) the window lost focus or was minimised; those numbers may be lower than this PC can do (they are marked below).\n";
    }
    t += "\n";
    for (const BenchScene& s : scenes) {
        t += s.name + "\n";
        if (s.windowInactive) t += "  WARNING: the window lost focus or was minimised during this scene.\n";
        t += "  Settings: " + s.what + "\n";
        t += "  Frames counted: " + std::to_string(s.frames.frames) + "\n";
        t += "  Average: " + f("%.2f", s.frames.avgMs) + " ms per frame = " + f("%.1f", s.frames.avgFps) + " fps\n";
        t += "  1 % low: " + f("%.1f", s.frames.low1Fps) + " fps (average of the slowest 1 % of the frames)\n";
        t += "  Worst frame: " + f("%.2f", s.frames.worstMs) + " ms\n";
        t += "  CPU work per frame: " + f("%.2f", s.cpuAvgMs) + " ms\n";
        if (s.gpuSamples <= 0) t += "  GPU time per frame: not available\n";
        else if (e.gpuReliable && s.gpuProblem.empty()) {
            t += "  GPU time per frame: " + f("%.2f", s.gpuAvgMs) + " ms\n";
            t += "  GPU span per frame: " + f("%.2f", s.gpuSpanAvgMs) + " ms (from the first to the last GPU command of the frame, waiting included)\n";
        } else {
            const std::string why = !s.gpuProblem.empty() ? s.gpuProblem : std::string("the timer failed its self-check");
            t += "  GPU time per frame: not reliable (" + why + ")\n";
            t += "  (raw values, do not trust them: busy " + f("%.2f", s.gpuAvgMs) + " ms, span " + f("%.2f", s.gpuSpanAvgMs) + " ms, " + std::to_string(s.gpuSamples) + " of " + std::to_string(s.frames.frames) + " frames)\n";
        }
    }
    if (!scenes.empty()) {
        double sum = 0;
        for (const BenchScene& s : scenes) sum += s.frames.avgFps;
        t += "\nAverage fps over the scenes: " + f("%.1f", sum / scenes.size()) + "\n";
    }
    return t;
}

std::string benchFileName(const ShotTime& t) {
    char b[80];
    std::snprintf(b, sizeof b, "physics-lab-benchmark-%04d-%02d-%02d-%02d%02d%02d.txt", t.year, t.month, t.day, t.hour, t.minute, t.second);
    return b;
}

BenchRun::BenchRun(int sceneCount, double warmupS, double measureS)
    : count_(sceneCount), warm_(warmupS), measure_(measureS), frameMs_(sceneCount), cpuSum_(sceneCount), gpuSum_(sceneCount), spanSum_(sceneCount), cpuN_(sceneCount), gpuN_(sceneCount), inactive_(sceneCount, 0) {}

void BenchRun::gpuSample(double busyMs, double spanMs) {
    if (finished_ || setup_ >= 0 || !measuring_) return;
    gpuSum_[scene_] += busyMs; spanSum_[scene_] += spanMs; gpuN_[scene_]++;
}

void BenchRun::noteWindow(bool focused, bool minimised) {
    if (finished_ || setup_ >= 0) return;                // nothing is running yet (or any more)
    if (!focused || minimised) inactive_[scene_] = 1;
}

void BenchRun::frame(double frameMs, double cpuMs) {
    if (finished_ || setup_ >= 0) return;
    elapsed_ += frameMs / 1000;
    if (!measuring_) {
        if (elapsed_ >= warm_) { measuring_ = true; elapsed_ = 0; }
        return;
    }
    frameMs_[scene_].push_back(frameMs);
    cpuSum_[scene_] += cpuMs; cpuN_[scene_]++;
    if (elapsed_ >= measure_) {
        measuring_ = false; elapsed_ = 0;
        if (++scene_ >= count_) finished_ = true; else setup_ = scene_;
    }
}

double BenchRun::doneS() const {
    if (finished_) return totalS();
    return scene_ * (warm_ + measure_) + (measuring_ ? warm_ : 0) + elapsed_;
}

BenchScene BenchRun::result(int i) const {
    BenchScene s;
    s.frames = benchStats(frameMs_[i]);
    s.cpuAvgMs = cpuN_[i] ? cpuSum_[i] / cpuN_[i] : 0;
    s.gpuSamples = gpuN_[i];
    s.gpuAvgMs = gpuN_[i] ? gpuSum_[i] / gpuN_[i] : -1;
    s.gpuSpanAvgMs = gpuN_[i] ? spanSum_[i] / gpuN_[i] : -1;
    s.windowInactive = inactive_[i] != 0;
    s.gpuProblem = gpuN_[i] ? gpuSceneProblem(s.frames.frames, gpuN_[i], s.gpuAvgMs, s.gpuSpanAvgMs, s.frames.avgMs) : "";
    return s;
}

}  // namespace app
