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
    s.slowFrames = int(std::count_if(frameMs.begin(), frameMs.end(), [](double ms) { return ms > kBenchSlowMs; }));
    std::vector<double> sorted = frameMs;
    std::sort(sorted.begin(), sorted.end(), std::greater<double>());
    const std::size_t n = std::max<std::size_t>(1, std::size_t(std::ceil(frameMs.size() * 0.01)));
    const double slow = std::accumulate(sorted.begin(), sorted.begin() + n, 0.0) / n;
    s.low1Fps = slow > 0 ? 1000 / slow : 0;
    return s;
}

namespace {
std::string f(const char* fmt, double a) { char b[64]; std::snprintf(b, sizeof b, fmt, a); return b; }
std::string pad(std::string s, std::size_t w) { std::size_t n = 0; for (unsigned char c : s) n += (c & 0xC0) != 0x80; while (n < w) { s += ' '; n++; } return s; }   // (UTF-8: counted in characters)
std::string fps(double v) { return v >= 1000 ? f("%.0f", v) : v >= 10 ? f("%.1f", v) : f("%.2f", v); }
}

int benchReaches(double low1, double pics, double hz) {
    if (!(hz > 0)) return -1;
    return low1 >= hz && pics >= hz ? 1 : 0;
}

std::string benchLimitedBy(const BenchScene& s, bool gpuReliable) {
    if (s.pictures >= 0 && s.frames.avgFps > 0 && s.picturesPerS() < 0.9 * s.frames.avgFps && !s.picturesBy.empty())
        return s.picturesBy + " (the pictures, made off the frame)";
    const bool gpu = gpuReliable && s.gpuSamples > 0 && s.gpuProblem.empty() && s.gpuAvgMs >= 0;
    const double c = s.cpuAvgMs, g = s.gpuAvgMs;
    if (!gpu) return s.frames.avgMs > 0 && c >= 0.8 * s.frames.avgMs ? "processor? (no GPU time)" : "not known (no GPU time)";
    if (c >= 1.5 * g) return "processor";
    if (g >= 1.5 * c) return "graphics card";
    return "both";
}

std::vector<BenchSummaryRow> benchSummary(const std::vector<BenchScene>& scenes, double hz, bool gpuReliable) {
    std::vector<BenchSummaryRow> v;
    for (const BenchScene& s : scenes) {
        BenchSummaryRow r;
        r.id = s.id; r.name = s.name; r.avgFps = s.frames.avgFps; r.low1Fps = s.frames.low1Fps; r.picsPerS = s.picturesPerS();
        r.reaches = benchReaches(r.low1Fps, r.picsPerS, hz);
        r.limitedBy = benchLimitedBy(s, gpuReliable);
        v.push_back(r);
    }
    std::stable_sort(v.begin(), v.end(), [](const BenchSummaryRow& a, const BenchSummaryRow& b) { return std::min(a.low1Fps, a.picsPerS) < std::min(b.low1Fps, b.picsPerS); });
    return v;
}

double benchMiddle(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
double benchWorst(const std::vector<double>& v) { return v.empty() ? 0 : *std::max_element(v.begin(), v.end()); }

bool benchIdleBusy(const BenchIdle& i) {
    if (i.workerBusy) return true;
    if (i.keepsPicture && i.redraws > 0) return true;
    return i.cpuShare >= 0 && i.threads > 0 && i.cpuShare * i.threads > kBenchIdleBusyThread;
}

std::string benchResultText(const BenchEnv& e, const std::vector<BenchScene>& scenes, const std::vector<BenchResponse>& responses, const std::vector<BenchIdle>& idle) {
    std::string t;
    t += "Physics Lab benchmark\n";
    t += "Version: " + e.version + "\n";
    t += "Date: " + e.when + "\n";
    t += std::string("Run: ") + (e.allScenes ? "All scenes" : "Hard scenes") + "\n";
    t += "Processor: " + (e.cpu.empty() ? std::string("not available") : e.cpu) + ", " + (e.cores > 0 ? std::to_string(e.cores) : std::string("not available")) + " cores, " +
         (e.threads > 0 ? std::to_string(e.threads) : std::string("not available")) + " threads\n";
    t += "Memory installed: " + (e.memoryMB > 0 ? f("%.1f", e.memoryMB / 1024) + " GB" : std::string("not available")) + "; the app's peak during the run: " +
         (e.peakMB > 0 ? f("%.0f", e.peakMB) + " MB" : std::string("not available")) + "\n";
    t += "Start-up: " + (e.startupMs >= 0 ? f("%.0f", e.startupMs) + " ms from the start of the app to its first finished picture" : std::string("not measured")) + "\n";
    {   // 0.56.1: the worst single frame of the run and its scene
        double w = 0; std::string at;
        for (const BenchScene& s : scenes) if (s.frames.worstMs > w) { w = s.frames.worstMs; at = s.id; }
        t += "Worst single frame: " + (at.empty() ? std::string("none") : f("%.1f", w) + " ms, in " + at) + "\n";
    }
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
    if (!e.power.empty()) t += e.power + "\n";
    t += "VSync: off during the scenes, on during the waits and while the labs are left alone. Each scene: " + f("%.1f", e.warmupS) + " s warm-up (not counted), then " + f("%.1f", e.measureS) + " s counted.\n";
    t += std::string("GPU timer (GL_TIME_ELAPSED): ") + (e.gpuTimer ? "available" : "not available") + "\n";
    if (e.gpuTimer) t += std::string("GPU timer check: ") + (e.gpuReliable ? "reliable, " : "NOT RELIABLE, ") + (e.gpuCheck.empty() ? "no check was made" : e.gpuCheck) + "\n";
    t += "GPU time covers the whole drawing of a frame (clear, picture with its soft layer and instanced draws, panels and text), up to just before the buffer swap.\n";
    t += "Input during the test: blocked\n";
    if (e.cancelled) t += "THE TEST WAS CANCELLED: only the finished scenes are listed.\n";
    {   // 0.15 stage 2: scenes during which the window lost focus or was minimised (Windows can slow a background app, so those numbers may be lower than the PC can do)
        int inactive = 0;
        for (const BenchScene& s : scenes) if (s.windowInactive) inactive++;
        if (inactive) t += "WARNING: in " + std::to_string(inactive) + " scene(s) the window lost focus or was minimised; those numbers may be lower than this PC can do (they are marked below).\n";
    }
    // ---- 0.56.1: the summary, slowest first ----
    if (!scenes.empty()) {
        t += "\nSCENES, slowest first\n";
        std::size_t w = 10;
        for (const BenchScene& s : scenes) w = std::max(w, s.id.size());
        const std::vector<BenchSummaryRow> rows = benchSummary(scenes, e.refreshHz, e.gpuReliable);
        std::size_t lw = 10;
        for (const BenchSummaryRow& r : rows) lw = std::max(lw, r.limitedBy.size());
        t += pad("id", w) + "  " + pad("avg fps", 8) + "  " + pad("1 % low", 8) + "  " + pad("pictures/s", 10) + "  " + pad("reaches", 9) + "  " + pad("limited by", lw) + "  name\n";
        for (const BenchSummaryRow& r : rows)
            t += pad(r.id, w) + "  " + pad(fps(r.avgFps), 8) + "  " + pad(fps(r.low1Fps), 8) + "  " + pad(fps(r.picsPerS), 10) + "  " +
                 pad(r.reaches > 0 ? "yes" : r.reaches == 0 ? "no" : "not known", 9) + "  " + pad(r.limitedBy, lw) + "  " + r.name + "\n";
        t += "Rules: \"reaches\" is yes when the 1 % low and the new pictures a second are both at least the display's refresh rate (" +
             (e.refreshHz > 0 ? f("%.0f", e.refreshHz) + " Hz" : std::string("not known here")) + "). \"Limited by\" compares the frame's CPU work c with the GPU's busy time g: "
             "processor when c >= 1.5 g, graphics card when g >= 1.5 c, else both (no reliable GPU time: \"processor?\" when c is 80 % of the frame or more); where the new "
             "pictures a second fall under 90 % of the frames a second it names what makes the pictures off the frame. Slowest first by the lower of the 1 % low and the "
             "new pictures a second.\n";
    }
    if (!responses.empty()) {
        t += "\nWAITS (each done " + std::to_string(kBenchResponseRepeats) + " times: the middle and the worst, until the finished picture)\n";
        std::size_t w = 10;
        for (const BenchResponse& r : responses) w = std::max(w, r.id.size());
        t += pad("id", w) + "  " + pad("middle", 10) + "  " + pad("worst", 10) + "  " + pad("worst frame", 11) + "  name\n";
        for (const BenchResponse& r : responses) {
            const double mid = benchMiddle(r.ms), wst = benchWorst(r.ms);
            t += pad(r.id, w) + "  " + pad(r.ms.empty() ? "-" : f("%.0f", mid) + " ms", 10) + "  " + pad(r.ms.empty() ? "-" : (r.capped ? "> " : "") + f("%.0f", wst) + " ms", 10) + "  " +
                 pad(r.worstFrameMs.empty() ? "-" : f("%.1f", benchWorst(r.worstFrameMs)) + " ms", 11) + "  " + r.name + "\n";
        }
        for (const BenchResponse& r : responses) if (!r.what.empty()) t += "  " + r.id + ": " + r.what + "\n";
    }
    if (!idle.empty()) {
        t += "\nAT REST (each lab left alone " + f("%.0f", kBenchIdleS) + " s after its picture is finished, VSync on)\n";
        std::size_t w = 10;
        for (const BenchIdle& i : idle) w = std::max(w, i.id.size());
        t += pad("id", w) + "  " + pad("verdict", 7) + "  " + pad("processor", 22) + "  " + pad("GPU a frame", 11) + "  " + pad("drawn anew", 18) + "  " + pad("worker", 6) + "  name\n";
        for (const BenchIdle& i : idle) {
            const std::string cpu = i.cpuShare < 0 ? std::string("not available") : f("%.1f", 100 * i.cpuShare) + " % (" + (i.threads > 0 ? f("%.0f", 100 * i.cpuShare * i.threads) + " % of a thread)" : std::string("?)"));
            const std::string anew = i.keepsPicture ? (i.redraws > 0 ? std::to_string(i.redraws) + " times" : std::string("no")) : std::string("every frame (this lab)");
            t += pad(i.id, w) + "  " + pad(benchIdleBusy(i) ? "busy" : "quiet", 7) + "  " + pad(cpu, 22) + "  " + pad(i.gpuMs < 0 ? "not available" : f("%.2f", i.gpuMs) + " ms", 11) + "  " +
                 pad(anew, 18) + "  " + pad(i.workerBusy ? "busy" : "idle", 6) + "  " + i.name + (i.settled ? "" : " (the picture was not finished in " + f("%.0f", kBenchIdleSettleCapS) + " s)") + "\n";
        }
        t += "Rule: busy when a worker thread was busy, or a lab that keeps its picture drew it anew, or the app used more than " + f("%.0f", 100 * kBenchIdleBusyThread) +
             " % of one processor thread (all its threads together); else quiet. The processor share is of the whole processor (" +
             (idle[0].threads > 0 ? std::to_string(idle[0].threads) + " threads" : std::string("threads not known")) + ").\n";
    }
    t += "\nDETAILS\n";
    for (const BenchScene& s : scenes) {
        t += (s.id.empty() ? s.name : s.id + "  " + s.name) + "\n";
        if (s.windowInactive) t += "  WARNING: the window lost focus or was minimised during this scene.\n";
        t += "  What it tests: " + s.what + "\n";
        if (!s.line.empty()) t += "  " + s.line + "\n";
        if (!s.path.empty()) t += "  Path: " + s.path + "\n";
        t += "  Frames counted: " + std::to_string(s.frames.frames) + "\n";
        t += "  Average: " + f("%.2f", s.frames.avgMs) + " ms per frame = " + f("%.1f", s.frames.avgFps) + " fps\n";
        t += "  1 % low: " + f("%.1f", s.frames.low1Fps) + " fps (average of the slowest 1 % of the frames)\n";
        t += "  New pictures: " + (s.pictures < 0 ? std::string("every frame") : std::to_string(s.pictures) + " in " + f("%.1f", s.frames.totalS) + " s (" + f("%.1f", s.picturesPerS()) + " a second)") + "\n";
        t += "  Worst frame: " + f("%.2f", s.frames.worstMs) + " ms\n";
        t += "  Frames slower than 16.7 ms: " + std::to_string(s.frames.slowFrames) + " of " + std::to_string(s.frames.frames) + "\n";   // 0.16 stage 3: 1 = a one-time hitch, many = a repeating one
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
        for (const std::string& n : s.notes) t += "  " + n + "\n";   // 0.55.5
    }
    return t;
}

std::vector<std::string> benchBlackHoleLines(const BenchBlackHole& b) {
    const auto stage = [](const BenchStage& s) {
        return s.n > 0 ? f("%.1f", s.sum / s.n) + " ms average, " + f("%.1f", s.worst) + " ms worst (" + std::to_string(s.n) + "x)" : std::string("not needed");
    };
    static const char* const names[6] = {"spin 0.9, turning round the axis", "spin 0.9, tilting", "spin 0.9, changing distance",
                                         "spin 0, turning round the axis", "spin 0, tilting", "spin 0, changing distance"};
    std::vector<std::string> v;
    v.push_back("Black Hole Lab: the frame's own thread draws the last finished picture; the tables, star bins, stars' images and exposure are worked out on a worker "
                "thread, the glare's veil and the fine spin table on threads of their own (below normal priority).");
    v.push_back("Black Hole Lab, the spinning hole's light paths: " + (b.path.empty() ? std::string("not decided yet") : b.path) + "; the card's fills " + stage(b.card));
    if (!b.skyPath.empty())
        v.push_back("Black Hole Lab, the spinning hole's sky and disc: " + b.skyPath + "; the pass on the card " + stage(b.skyGpu) + " for the whole picture; the exposure: " +
                    (b.lookPath.empty() ? std::string("not decided yet") : b.lookPath));
    for (int k = 0; k < 6; k++) {
        const BenchBhMove& m = b.move[k];
        if (m.frames == 0) { v.push_back(std::string("Black Hole Lab, ") + names[k] + ": no frames counted"); continue; }
        v.push_back(std::string("Black Hole Lab, ") + names[k] + ": frame thread " + f("%.2f", m.frameSum / m.frames) + " ms average, " + f("%.2f", m.frameWorst) +
                    " ms worst; picture behind the settings in " + std::to_string(m.behindFrames) + " of " + std::to_string(m.frames) + " frames, at most " + f("%.0f", m.behindWorst) + " ms; " +
                    std::to_string(m.pictures) + " new pictures in " + f("%.1f", m.seconds) + " s" + (m.seconds > 0 ? " (" + f("%.0f", m.pictures / m.seconds) + " a second)" : std::string()));
        v.push_back("  worker: coarse table " + stage(m.coarse) + "; star bins " + stage(m.bins) + "; stars' images " + stage(m.stars) + "; exposure " + stage(m.look) +
                    (m.disc.n > 0 ? "; disc table " + stage(m.disc) : std::string()));
        if (m.skyPictures > 0)
            v.push_back("  the card: sky and disc " + std::to_string(m.skyPictures) + " new pictures in " + f("%.1f", m.seconds) + " s" +
                        (m.seconds > 0 ? " (" + f("%.0f", m.skyPictures / m.seconds) + " a second)" : std::string()) + ", at " + f("%.0f", 100 * m.scaleMin) +
                        " % of the picture's width at the lowest; the stars: " + std::to_string(m.pictures) + " new pictures" +
                        (m.seconds > 0 ? " (" + f("%.0f", m.pictures / m.seconds) + " a second)" : std::string()));
        if (k < 3) v.push_back(m.sharpMs >= 0 ? "  at rest: the sharp picture (the fine table) " + f("%.0f", m.sharpMs) + " ms after the stop"
                                              : "  at rest: no sharp picture within the " + f("%.1f", m.restS) + " s at rest");
    }
    v.push_back("Black Hole Lab, at rest: glare " + stage(b.glare) + (b.glare.n > 0 ? ", shown " + f("%.0f", b.glareWaitWorst) + " ms after its picture at most" : std::string()) +
                "; fine spin table " + stage(b.fine) + "; the fine spin table made at the scene's start (all cores) " + f("%.0f", b.fineStartMs) + " ms");
    return v;
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

void BenchRun::frame(double frameMs, double cpuMs, bool count) {
    if (finished_ || setup_ >= 0) return;
    if (!count && measuring_) return;                      // (a frame that read the picture back: neither counted nor on the scene's clock)
    elapsed_ += frameMs / 1000;
    if (!measuring_) {
        if (elapsed_ >= warm_) { measuring_ = true; elapsed_ = 0; }
        return;
    }
    if (count) { frameMs_[scene_].push_back(frameMs); cpuSum_[scene_] += cpuMs; cpuN_[scene_]++; }
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
