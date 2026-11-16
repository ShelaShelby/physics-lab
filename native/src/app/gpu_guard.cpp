#include "app/gpu_guard.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <io.h>        // _commit, _fileno
#else
#include <unistd.h>    // fsync, fileno
#endif

namespace app {

namespace {
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::string field(const std::string& v) {
    std::string o;
    for (char c : v) o += c == '|' ? '/' : (c == '\n' || c == '\r') ? ' ' : c;
    while (!o.empty() && o.back() == ' ') o.pop_back();
    std::size_t a = 0;
    while (a < o.size() && o[a] == ' ') a++;
    return o.substr(a);
}
void commitFile(std::FILE* f) {
    std::fflush(f);
#ifdef _WIN32
    _commit(_fileno(f));
#else
    fsync(fileno(f));
#endif
}
// the marker's text: version, driver and step on three lines ("version=...")
GpuGuardRecord readMarker(const std::string& path) {
    GpuGuardRecord r;
    std::ifstream in(path, std::ios::binary);
    if (!in) return r;
    std::string ln;
    r.state = "off";
    while (std::getline(in, ln)) {
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        if (ln.rfind("version=", 0) == 0) r.version = ln.substr(8);
        else if (ln.rfind("driver=", 0) == 0) r.driver = ln.substr(7);
        else if (ln.rfind("step=", 0) == 0) r.step = ln.substr(5);
    }
    if (r.step.empty()) r.step = "(not named)";
    return r;
}
}  // namespace

std::string gpuGuardText(const GpuGuardRecord& r) {
    if (r.state.empty()) return std::string();
    return field(r.state) + " | " + field(r.version) + " | " + field(r.driver) + " | " + field(r.step);
}

GpuGuardRecord gpuGuardParse(const std::string& line) {
    std::vector<std::string> f;
    std::size_t a = 0;
    while (a <= line.size()) {
        std::size_t b = line.find('|', a);
        if (b == std::string::npos) b = line.size();
        f.push_back(field(line.substr(a, b - a)));
        a = b + 1;
    }
    GpuGuardRecord r;
    if (f.size() != 4 || (f[0] != "off" && f[0] != "slow" && f[0] != "ok") || f[1].empty()) return r;
    r.state = f[0]; r.version = f[1]; r.driver = f[2]; r.step = f[3];
    return r;
}

GpuGuardDecision gpuGuardDecide(const GpuGuardRecord& saved, const GpuGuardRecord& marker, const std::string& version, const std::string& driver) {
    GpuGuardDecision d;
    d.keep = marker.state.empty() ? saved : marker;                // a marker left behind: the last run died inside its step
    // 0.56.0: a line of an older form (0.55.7 and 0.55.8 kept the app's version, not "shader <hash>"; 0.55.9 tried the dead shader once more because of it):
    // read as "off" for its driver, kept from now on in the new form with this shader's id (a later changed shader still tries again)
    if (!d.keep.state.empty() && d.keep.version.rfind("shader ", 0) != 0 && d.keep.version.rfind("sky ", 0) != 0 && d.keep.driver == field(driver))
        d.keep = GpuGuardRecord{"off", field(version), d.keep.driver, d.keep.step.empty() ? std::string("(kept by an older version of the app)") : d.keep.step};
    const bool same = d.keep.version == field(version) && d.keep.driver == field(driver);
    if ((d.keep.state == "off" || d.keep.state == "slow") && same) {
        d.allowed = false; d.guarded = false; d.crashed = d.keep.state == "off";
    } else if (d.keep.state == "ok" && same) {
        d.allowed = true; d.guarded = false;
    } else {
        d.allowed = true; d.guarded = true;                         // never tried, or another version or driver: tried (once more), guarded
    }
    return d;
}

bool writeCommitted(const std::string& path, const std::string& text) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    commitFile(f);
    return std::fclose(f) == 0 && ok;
}

void GpuGuard::start(const std::string& folder, const std::string& version, const std::string& driver, const std::string& saved, const std::vector<std::string>& head) {
    std::lock_guard<std::mutex> g(mx_);
    started_ = true;
    folder_ = folder; version_ = field(version); driver_ = field(driver);
    t0_ = nowMs();
    if (const char* e = std::getenv("PL_GPU_DIE")) die_ = e;
    GpuGuardRecord marker;
    if (!folder_.empty()) {
        marker = readMarker(folder_ + markerName_);
        staleMarker_ = !marker.state.empty();
        if (truncateLog_) if (std::FILE* f = std::fopen((folder_ + logName_).c_str(), "wb")) std::fclose(f);   // (overwritten at each start; 0.56.0: by the first guard)
    }
    d_ = gpuGuardDecide(gpuGuardParse(saved), marker, version_, driver_);
    lines_ = 0; logFull_ = false;
    for (const std::string& h : head) line(h, false);
    if (!marker.state.empty()) line("A marker was left by the last run (app " + marker.version + ", driver " + marker.driver + "): it died inside the step '" + marker.step + "'", true);
    if (!d_.allowed) line(std::string("The graphics card's light paths are switched off for this light-path shader and driver: ") +
                          (d_.crashed ? "the app died inside the step '" : "this step did not finish in time: '") + d_.keep.step + "' (Settings, Performance: Try again)", true);
    else line(d_.guarded ? "The graphics card's light paths: allowed, guarded (a marker before each step until the whole path has succeeded once)"
                         : "The graphics card's light paths: allowed (they succeeded once with this light-path shader and driver)", false);
}

std::string GpuGuard::offText() const {
    std::lock_guard<std::mutex> g(mx_);
    if (!started_ || d_.allowed) return std::string();
    if (d_.crashed) return "The graphics card's light paths stopped the app last time (in the step '" + d_.keep.step + "') and are switched off for this graphics driver.";
    return "The graphics card's light paths did not finish a step in time last time ('" + d_.keep.step + "') and are switched off for this graphics driver.";
}

void GpuGuard::writeMarker(const std::string& step) {
    if (folder_.empty()) return;
    writeCommitted(folder_ + markerName_, "version=" + version_ + "\ndriver=" + driver_ + "\nstep=" + field(step) + "\n");
    staleMarker_ = false;
}

void GpuGuard::removeMarker() {
    if (folder_.empty()) return;
    std::remove((folder_ + markerName_).c_str());
    staleMarker_ = false;
}

void GpuGuard::line(const std::string& text, bool commit) {
    if (folder_.empty() || logFull_) return;
    std::FILE* f = std::fopen((folder_ + logName_).c_str(), "ab");
    if (!f) return;
    char stamp[32];
    std::snprintf(stamp, sizeof stamp, "[%9.1f ms] ", nowMs() - t0_);
    if (!prefix_.empty()) std::fputs(prefix_.c_str(), f);
    if (++lines_ >= kLogMax) { std::fputs(stamp, f); std::fputs("(the log is full: later lines are not written)\n", f); logFull_ = true; }
    else { std::fputs(stamp, f); std::fputs(text.c_str(), f); std::fputc('\n', f); }
    if (commit) commitFile(f); else std::fflush(f);
    std::fclose(f);
}

void GpuGuard::begin(const std::string& step) {
    std::lock_guard<std::mutex> g(mx_);
    if (d_.guarded) writeMarker(step);
    line("begin " + step, d_.guarded);
    if (!die_.empty() && step.compare(0, die_.size(), die_) == 0) {
        line("(test switch PL_GPU_DIE: the process ends here, inside the step)", true);
        std::_Exit(3);
    }
}

void GpuGuard::end(const std::string& step, const std::string& result) {
    std::lock_guard<std::mutex> g(mx_);
    line("end " + step + (result.empty() ? std::string() : ": " + result), d_.guarded);
}

void GpuGuard::log(const std::string& text) {
    std::lock_guard<std::mutex> g(mx_);
    line(text, d_.guarded);
}

void GpuGuard::finished(bool ok, const std::string& why) {
    std::lock_guard<std::mutex> g(mx_);
    removeMarker();
    if (ok) {
        line("The graphics card's light paths: the whole path succeeded" + std::string(d_.guarded ? " (kept: no markers any more for this light-path shader and driver)" : ""), true);
        if (d_.guarded) { d_.keep = GpuGuardRecord{"ok", version_, driver_, ""}; d_.guarded = false; }
    } else {
        line("The graphics card's light paths: not used in this run (the processor's path), because " + why, true);
    }
}

void GpuGuard::timedOut(const std::string& step) {
    std::lock_guard<std::mutex> g(mx_);
    removeMarker();
    line("The step '" + step + "' did not finish within its time limit: the graphics card's light paths are switched off (kept for this light-path shader and driver)", true);
    d_.keep = GpuGuardRecord{"slow", version_, driver_, field(step)};
    d_.allowed = false; d_.guarded = false; d_.crashed = false;
}

void GpuGuard::tryAgain() {
    std::lock_guard<std::mutex> g(mx_);
    d_.keep = GpuGuardRecord{};
    d_.allowed = true; d_.guarded = true; d_.crashed = false;
    generation_++;
    line("Try again (Settings, Performance): the graphics card's light paths are allowed again, guarded", true);
}

void GpuGuard::closing() {
    std::lock_guard<std::mutex> g(mx_);
    if (!started_ || staleMarker_ || folder_.empty()) return;
    if (std::FILE* f = std::fopen((folder_ + markerName_).c_str(), "rb")) {
        std::fclose(f);
        removeMarker();
        line("The app closes normally: the marker of a step still open was removed", true);
    }
}

void GpuGuard::settingsWritten() {
    std::lock_guard<std::mutex> g(mx_);
    if (staleMarker_) { removeMarker(); line("The settings file keeps that: the marker was removed", false); }
}

std::vector<std::string> GpuGuard::logTail(int n) const {
    std::string path;
    {   std::lock_guard<std::mutex> g(mx_);
        path = logPath(); }
    std::vector<std::string> all;
    if (path.empty()) return all;
    std::ifstream in(path, std::ios::binary);
    std::string ln;
    while (std::getline(in, ln)) { if (!ln.empty() && ln.back() == '\r') ln.pop_back(); all.push_back(ln); }
    if (static_cast<int>(all.size()) > n) all.erase(all.begin(), all.end() - n);
    return all;
}

GpuGuard& gpuGuard() {
    static GpuGuard g;
    return g;
}

GpuGuard& gpuSkyGuard() {
    static GpuGuard g("gpu-sky-step.txt", GpuGuard::kLog, false, "[sky] ");
    return g;
}

}  // namespace app
