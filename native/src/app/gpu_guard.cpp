#include "app/gpu_guard.h"
#include "app/gpu_helper.h"
#include "app/frame_cost.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

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
        else if (ln.rfind("app=", 0) == 0) r.app = ln.substr(4);   // (0.63.1)
    }
    if (r.step.empty()) r.step = "(not named)";
    return r;
}
std::mutex g_appMx;
std::string g_app;                                                  // 0.63.1: the running app's version (gpuGuardSetApp)
// 0.63.0: the turn: the one guard whose guarded step (or run of steps: hold) is open in this process
std::mutex g_turnMx;
std::condition_variable g_turnCv;
const GpuGuard* g_turn = nullptr;
std::string g_turnName, g_turnStep;
std::thread::id g_frame;
bool g_frameSet = false;
bool g_endAll = false;                                           // 0.68.0: the app ends (gpuGuardEndAll): a worker waiting for the turn goes on
int g_waiting = 0;                                                  // guards whose step waits for its turn
}  // namespace

bool gpuGuardBusy() {
    std::lock_guard<std::mutex> t(g_turnMx);
    return g_turn != nullptr || g_waiting > 0;
}

void GpuGuard::setWaiting(bool w) {                                 // (with g_turnMx held)
    if (w == waiting_) return;
    waiting_ = w;
    g_waiting += w ? 1 : -1;
}

void gpuGuardFrameThread() {
    std::lock_guard<std::mutex> t(g_turnMx);
    g_frame = std::this_thread::get_id();
    g_frameSet = true;
}

bool frameCostFrameThread() {                                       // (0.63.1: app/frame_cost.h)
    std::lock_guard<std::mutex> t(g_turnMx);
    return g_frameSet && std::this_thread::get_id() == g_frame;
}

std::string gpuGuardTurn() {
    std::lock_guard<std::mutex> t(g_turnMx);
    return g_turn ? g_turnName + (g_turnStep.empty() ? std::string() : ", '" + g_turnStep + "'") : std::string();
}

std::string GpuGuard::name() const {
    if (!what_.empty()) return what_;
    if (prefix_.empty()) return "The graphics card's light paths";
    std::string p = prefix_;
    while (!p.empty() && p.back() == ' ') p.pop_back();
    return p;
}

// (never called with mx_ held: the log line is written after the turn's lock is let go)
bool GpuGuard::takeTurn(const std::string& step, bool mayWait) {
    std::unique_lock<std::mutex> t(g_turnMx);
    if (!g_turn || g_turn == this) {
        setWaiting(false);
        g_turn = this; g_turnName = name();
        if (!step.empty() || g_turnStep.empty()) g_turnStep = step;
        return true;
    }
    const std::string other = g_turnName + (g_turnStep.empty() ? std::string() : " (its step '" + g_turnStep + "')");
    if (!mayWait) {
        setWaiting(true);
        t.unlock();
        frameCostAdd(FrameCostKind::GuardDeferred, 0);              // 0.63.1: the benchmark's worst frames say so
        const std::string what = step.empty() ? std::string("the next step") : "the step '" + step + "'";
        std::lock_guard<std::mutex> g(mx_);
        if (waitLogged_ != other + what) { waitLogged_ = other + what; line(what + " waits for its turn (moved to a later frame): " + other + " is open", false); }
        return false;
    }
    t.unlock();
    {   std::lock_guard<std::mutex> g(mx_);
        line("the step '" + step + "' waits for its turn: " + other + " is open", false); }
    const double w0 = nowMs();
    t.lock();
    g_turnCv.wait(t, [] { return g_turn == nullptr || g_endAll; });
    if (g_turn != nullptr) { setWaiting(false); return true; }      // (the app ends: on without the turn; its step ends with the device)
    g_turn = this; g_turnName = name(); g_turnStep = step;
    setWaiting(false);
    t.unlock();
    char b[64];
    std::snprintf(b, sizeof b, " (waited %.0f ms)", nowMs() - w0);
    std::lock_guard<std::mutex> g(mx_);
    line("the step '" + step + "' has its turn" + b, false);
    return true;
}

void GpuGuard::giveTurn() {
    std::lock_guard<std::mutex> t(g_turnMx);
    setWaiting(false);
    if (g_turn != this) return;
    g_turn = nullptr; g_turnName.clear(); g_turnStep.clear();
    g_turnCv.notify_all();
}

bool GpuGuard::hold() {
    {   std::lock_guard<std::mutex> g(mx_);
        if (!d_.guarded) return true;
        if (held_) return true;
        if (!helper_.empty() && gpuHelperPending(helper_)) {         // 0.67.0: its helper has not finished: a later frame
            if (helperLogged_ != helper_) { helperLogged_ = helper_; line("(its first steps wait for the helper process that makes them first)", false); gpuHelperPrefer(helper_); }
            return false;
        } }
    if (!takeTurn(std::string(), false)) return false;
    std::lock_guard<std::mutex> g(mx_);
    held_ = true; waitLogged_.clear();
    return true;
}

void GpuGuard::rest() {
    {   std::lock_guard<std::mutex> g(mx_);
        held_ = false;
        if (open_ > 0) return; }
    giveTurn();
}

void gpuGuardSetApp(const std::string& app) {
    std::lock_guard<std::mutex> g(g_appMx);
    g_app = field(app);
}

bool gpuGuardAppTrusted(const std::string& app) {
    int major = -1, minor = -1, patch = 0;
    if (std::sscanf(app.c_str(), "%d.%d.%d", &major, &minor, &patch) < 2 || major < 0 || minor < 0) return false;
    return major > 0 || minor > 63 || (minor == 63 && patch >= 1);
}

std::string gpuGuardText(const GpuGuardRecord& r) {
    if (r.state.empty()) return std::string();
    return field(r.state) + " | " + field(r.version) + " | " + field(r.driver) + " | " + field(r.step) + (r.app.empty() ? std::string() : " | app " + field(r.app));
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
    if (f.size() == 5 && (f[4].rfind("app ", 0) != 0 || f[4].size() <= 4)) return r;   // (0.63.1: a fifth field names the app version that wrote it)
    if ((f.size() != 4 && f.size() != 5) || (f[0] != "off" && f[0] != "slow" && f[0] != "ok" && f[0] != "fail") || f[1].empty()) return r;
    r.state = f[0]; r.version = f[1]; r.driver = f[2]; r.step = f[3];
    if (f.size() == 5) r.app = f[4].substr(4);
    return r;
}

GpuGuardDecision gpuGuardDecide(const GpuGuardRecord& saved, const GpuGuardRecord& marker, const std::string& version, const std::string& driver, bool keepOld) {
    GpuGuardDecision d;
    d.keep = marker.state.empty() ? saved : marker;                // a marker left behind: the last run died inside its step
    // 0.56.0: a line of an older form (0.55.7 and 0.55.8 kept the app's version, not "shader <hash>"; 0.55.9 tried the dead shader once more because of it):
    // read as "off" for its driver, kept from now on in the new form with this shader's id (a later changed shader still tries again)
    if (!d.keep.state.empty() && d.keep.version.rfind("shader ", 0) != 0 && d.keep.version.rfind("sky ", 0) != 0 && d.keep.version.rfind("slice ", 0) != 0 && d.keep.version.rfind("jump ", 0) != 0 &&
        d.keep.version.rfind("molcloud ", 0) != 0 && d.keep.version.rfind("vulkan ", 0) != 0 && d.keep.driver == field(driver))   // (0.57.1: molcloud; 0.59.0: vulkan)
        d.keep = GpuGuardRecord{"off", field(version), d.keep.driver, d.keep.step.empty() ? std::string("(kept by an older version of the app)") : d.keep.step};
    const bool same = d.keep.version == field(version) && d.keep.driver == field(driver);
    if (d.keep.state == "off" && same && !keepOld && !gpuGuardAppTrusted(d.keep.app)) {
        d.allowed = true; d.guarded = true; d.untrusted = true;     // 0.63.1: a marker of 0.63.0 or older: tried once more, guarded (gpu_guard.h)
    } else if ((d.keep.state == "off" || d.keep.state == "slow" || d.keep.state == "fail") && same) {
        d.allowed = false; d.guarded = false; d.crashed = d.keep.state == "off"; d.failed = d.keep.state == "fail";
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
    {   std::lock_guard<std::mutex> a(g_appMx);
        app_ = g_app; }
    d_ = gpuGuardDecide(gpuGuardParse(saved), marker, version_, driver_, keepOld_);
    lines_ = 0; logFull_ = false;
    for (const std::string& h : head) line(h, false);
    if (!marker.state.empty()) line("A marker was left by the last run (app " + marker.version + ", driver " + marker.driver + (marker.app.empty() ? std::string() : ", written by " + marker.app) +
                                    "): it died inside the step '" + marker.step + "'", true);
    if (d_.untrusted)                                                // 0.63.1
        line("Not trusted: the crash in the step '" + d_.keep.step + "' was kept from a marker of " + (d_.keep.app.empty() ? std::string("0.63.0 or older (it names no app version)") : d_.keep.app) +
             "; 0.62.0 could leave one after a step that passed (its end kept the marker, and the Vulkan card programs' guards were not closed at the app's end): tried once more, guarded", true);
    const std::string w = what_.empty() ? std::string("The graphics card's light paths") : what_;
    if (!d_.allowed) line(w + (what_.empty() ? " are switched off for this light-path shader and driver: " : " is switched off for this version: ") +
                          (d_.crashed ? "the app died inside the step '" : d_.failed ? "this step failed: '" : "this step did not finish in time: '") + d_.keep.step + "' (Settings, Performance: Try again)", true);
    else line(d_.guarded ? w + ": allowed, guarded (a marker before each step until the whole path has succeeded once)"
                         : w + (what_.empty() ? ": allowed (they succeeded once with this light-path shader and driver)" : ": allowed (it succeeded once with this version)"), false);
}

std::string GpuGuard::offText() const {
    std::lock_guard<std::mutex> g(mx_);
    if (!started_ || d_.allowed) return std::string();
    if (!what_.empty()) {                                            // 0.59.0 (Vulkan): one thing, and its id is the app version
        if (d_.crashed) return what_ + " stopped the app last time (in the step '" + d_.keep.step + "').";
        if (d_.failed) return what_ + " could not start last time (in the step '" + d_.keep.step + "').";
        return what_ + " did not finish a step in time last time ('" + d_.keep.step + "').";
    }
    if (d_.crashed) return "The graphics card's light paths stopped the app last time (in the step '" + d_.keep.step + "') and are switched off for this graphics driver.";
    if (d_.failed) return "The graphics card's light paths failed (in the step '" + d_.keep.step + "') and are switched off for this graphics driver.";
    return "The graphics card's light paths did not finish a step in time last time ('" + d_.keep.step + "') and are switched off for this graphics driver.";
}

void GpuGuard::writeMarker(const std::string& step) {
    if (folder_.empty()) return;
    writeCommitted(folder_ + markerName_, "version=" + version_ + "\ndriver=" + driver_ + "\nstep=" + field(step) + "\n" + (app_.empty() ? std::string() : "app=" + app_ + "\n"));
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

// 0.68.0: the helper process's observer (gpuGuardObserve): every guarded step begun, ended, failed or timed out in this process
static void (*g_observer)(int what, const std::string& step, const std::string& result) = nullptr;
void gpuGuardEndAll() { std::lock_guard<std::mutex> t(g_turnMx); g_endAll = true; g_turnCv.notify_all(); }

void gpuGuardObserve(void (*f)(int what, const std::string& step, const std::string& result)) { g_observer = f; }

void GpuGuard::begin(const std::string& step) {
    bool guarded = false;
    std::string helper;
    {   std::lock_guard<std::mutex> g(mx_);
        guarded = d_.guarded; helper = helper_; }
    if (guarded && !helper.empty()) {                                  // 0.68.0: a worker's step waits for the program's helper (the frame's steps begin with hold())
        bool frame = false;
        {   std::lock_guard<std::mutex> t(g_turnMx);
            frame = g_frameSet && std::this_thread::get_id() == g_frame; }
        const auto t0 = std::chrono::steady_clock::now();
        while (!frame && gpuHelperPending(helper) && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(120)) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (guarded) {
        bool frame = false;
        {   std::lock_guard<std::mutex> t(g_turnMx);
            frame = g_frameSet && std::this_thread::get_id() == g_frame; }
        if (!takeTurn(step, !frame)) {                               // (a frame's step begun without hold(): it does not wait; logged above)
            std::lock_guard<std::mutex> g(mx_);
            line("(the frame never waits: the step '" + step + "' begins while another guard's step is open)", false);
        }
    }
    std::lock_guard<std::mutex> g(mx_);
    waitLogged_.clear();
    if (d_.guarded) open_++;
    if (d_.guarded) writeMarker(step);
    line("begin " + step, d_.guarded);
    if (g_observer) g_observer(0, step, std::string());
    if (!die_.empty() && step.compare(0, die_.size(), die_) == 0) {
        line("(test switch PL_GPU_DIE: the process ends here, inside the step)", true);
        std::_Exit(3);
    }
}

void GpuGuard::end(const std::string& step, const std::string& result) {
    if (g_observer) g_observer(1, step, result);
    bool give = false;
    {   std::lock_guard<std::mutex> g(mx_);
        line("end " + step + (result.empty() ? std::string() : ": " + result), d_.guarded);
        if (d_.guarded && open_ > 0) open_--;
        if (d_.guarded && open_ == 0) removeMarker();               // 0.63.0: no step open: a crash now names no step of this guard
        give = !held_ && open_ == 0; }
    if (give) giveTurn();
}

void GpuGuard::log(const std::string& text) {
    std::lock_guard<std::mutex> g(mx_);
    line(text, d_.guarded);
}

void GpuGuard::finished(bool ok, const std::string& why) {
    {   std::lock_guard<std::mutex> g(mx_);
    held_ = false; open_ = 0;
    removeMarker();
    if (ok) {
        line((what_.empty() ? std::string("The graphics card's light paths") : what_) + ": the whole path succeeded" +
             std::string(d_.guarded ? (what_.empty() ? " (kept: no markers any more for this light-path shader and driver)" : " (kept: no markers any more for this version)") : ""), true);
        if (d_.guarded) { d_.keep = GpuGuardRecord{"ok", version_, driver_, "", app_}; d_.guarded = false; }
    } else {
        line(what_.empty() ? "The graphics card's light paths: not used in this run (the processor's path), because " + why : what_ + ": not used in this run, because " + why, true);
    }
    }
    giveTurn();
}

void GpuGuard::timedOut(const std::string& step) {
    if (g_observer) g_observer(3, step, std::string());
    {   std::lock_guard<std::mutex> g(mx_);
    held_ = false; open_ = 0;
    removeMarker();
    line("The step '" + step + "' did not finish within its time limit: " + (what_.empty() ? std::string("the graphics card's light paths are switched off (kept for this light-path shader and driver)") : what_ + " is switched off (kept for this version)"), true);
    d_.keep = GpuGuardRecord{"slow", version_, driver_, field(step), app_};
    d_.allowed = false; d_.guarded = false; d_.crashed = false;
    }
    giveTurn();
}

void GpuGuard::failed(const std::string& step, const std::string& why) {
    if (g_observer) g_observer(2, step, why);
    {   std::lock_guard<std::mutex> g(mx_);
    held_ = false; open_ = 0;
    removeMarker();
    line("The step '" + step + "' failed: " + why + " (kept; Settings, Performance: Try again)", true);
    d_.keep = GpuGuardRecord{"fail", version_, driver_, field(step + ": " + why), app_};
    d_.allowed = false; d_.guarded = false; d_.crashed = false; d_.failed = true;
    }
    giveTurn();
}

void GpuGuard::setHelper(const std::string& name) { std::lock_guard<std::mutex> g(mx_); helper_ = name; }

void GpuGuard::helperDied(const std::string& step, const std::string& how, const char* verb) {   // 0.68.1: verb "hung" for the time limit
    {   std::lock_guard<std::mutex> g(mx_);
    held_ = false;
    line("the helper " + std::string(verb) + " making " + step + " (" + how + "): switched off, the app never makes it (kept; Settings, Performance: Try again)", true);
    d_.keep = GpuGuardRecord{"off", version_, driver_, field("the helper " + std::string(verb) + " making " + step), app_};
    d_.allowed = false; d_.guarded = false; d_.crashed = true; d_.failed = false;
    }
}

void GpuGuard::tryAgain() {
    {   std::lock_guard<std::mutex> g(mx_);
    held_ = false; open_ = 0;
    d_.keep = GpuGuardRecord{};
    d_.allowed = true; d_.guarded = true; d_.crashed = false; d_.failed = false;
    generation_++;
    line("Try again (Settings, Performance): " + (what_.empty() ? std::string("the graphics card's light paths are") : what_ + " is") + " allowed again, guarded", true);
    }
    giveTurn();
}

void GpuGuard::closing() {
    struct Give { GpuGuard* g; ~Give() { g->rest(); } } give{this};   // (0.63.0: the turn given back)
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

// 0.61.0: on Vulkan (step 3) the card programs have guards of their own (their markers, settings lines and versions differ from OpenGL's, so a failure
// on one API never switches the other off); the functions below give the guard of the API this start draws with
static bool g_vulkanGuards = false;

// 0.62.0 (step 4): the Black Hole Lab's light paths and its sky per pixel have Vulkan twins too
GpuGuard& gpuGuard() {
    static GpuGuard g;
    static GpuGuard v("gpu-vklightpaths-step.txt", GpuGuard::kLog, false, "[vulkan] [lightpaths] ", "The light paths on Vulkan");
    static const bool keep = (v.keepOldMarkers(), true);              // 0.63.1: their crash is real (AMD's shader compiler, both APIs)
    (void)keep;
    return g_vulkanGuards ? v : g;
}

GpuGuard& gpuSkyGuard() {
    static GpuGuard g("gpu-sky-step.txt", GpuGuard::kLog, false, "[sky] ");
    static GpuGuard v("gpu-vksky-step.txt", GpuGuard::kLog, false, "[vulkan] [sky] ", "The spinning hole's sky per pixel on Vulkan");
    return g_vulkanGuards ? v : g;
}

GpuGuard& gpuVkLensGuard() {
    static GpuGuard g("gpu-vklens-step.txt", GpuGuard::kLog, false, "[vulkan] [lens] ", "The Black Hole Lab's picture on Vulkan");
    return g;
}
void useVulkanGuards(bool on) { g_vulkanGuards = on; }
bool vulkanGuards() { return g_vulkanGuards; }

GpuGuard& gpuSliceGuard() {
    static GpuGuard g("gpu-slice-step.txt", GpuGuard::kLog, false, "[slice] ");
    static GpuGuard v("gpu-vkslice-step.txt", GpuGuard::kLog, false, "[vulkan] [slice] ", "The Slice's dots on Vulkan");
    return g_vulkanGuards ? v : g;
}

GpuGuard& gpuJumpGuard() {
    static GpuGuard g("gpu-jump-step.txt", GpuGuard::kLog, false, "[jump] ");
    static GpuGuard v("gpu-vkjump-step.txt", GpuGuard::kLog, false, "[vulkan] [jump] ", "The jump's groups on Vulkan");
    return g_vulkanGuards ? v : g;
}

GpuGuard& gpuVkGroupsGuard() {
    static GpuGuard g("gpu-vkgroups-step.txt", GpuGuard::kLog, false, "[vulkan] [groups] ", "The superposition and jump groups on Vulkan");
    return g;
}

GpuGuard& gpuVkOutlineGuard() {
    static GpuGuard g("gpu-vkoutline-step.txt", GpuGuard::kLog, false, "[vulkan] [outline] ", "The 90 % outline on Vulkan");
    return g;
}

GpuGuard& gpuVkRocksGuard() {
    static GpuGuard g("gpu-vkrocks-step.txt", GpuGuard::kLog, false, "[vulkan] [rocks] ", "The belts' rocks on Vulkan");
    return g;
}

GpuGuard& gpuVkCloudGuard() {
    static GpuGuard g("gpu-vkcloud-step.txt", GpuGuard::kLog, false, "[vulkan] [cloud] ", "The Cloud 3D Glow dots on Vulkan");
    return g;
}

GpuGuard& gpuVkRingsGuard() {   // 0.64.0
    static GpuGuard g("gpu-vkrings-step.txt", GpuGuard::kLog, false, "[vulkan] [rings] ", "Saturn's rings on Vulkan");
    return g;
}

GpuGuard& gpuVkClassicGuard() {   // 0.64.0
    static GpuGuard g("gpu-vkclassic-step.txt", GpuGuard::kLog, false, "[vulkan] [classic] ", "The Classic Cloud 3D dots on Vulkan");
    return g;
}

GpuGuard& gpuVkNucDensGuard() {   // 0.67.0
    static GpuGuard g("gpu-vknucdensity-step.txt", GpuGuard::kLog, false, "[vulkan] [nucdensity] ", "The nucleus' Density look on Vulkan");
    return g;
}

GpuGuard& gpuVkLinesGuard() {   // 0.68.0
    static GpuGuard g("gpu-vklines-step.txt", GpuGuard::kLog, false, "[vulkan] [lines] ", "The Orbit Lab's lines on Vulkan");
    return g;
}

GpuGuard& gpuVkSurfaceGuard() {   // 0.68.0
    static GpuGuard g("gpu-vksurface-step.txt", GpuGuard::kLog, false, "[vulkan] [surface] ", "The Density surface on Vulkan");
    return g;
}

GpuGuard& gpuVulkanGuard() {
    static GpuGuard g("gpu-vulkan-step.txt", GpuGuard::kLog, true, "[vulkan] ", "Vulkan");
    return g;
}

GpuGuard& gpuMolCloudGuard() {
    static GpuGuard g("gpu-molcloud-step.txt", GpuGuard::kLog, false, "[molcloud] ");
    static GpuGuard v("gpu-vkmolcloud-step.txt", GpuGuard::kLog, false, "[vulkan] [molcloud] ", "The Density cloud on Vulkan");
    return g_vulkanGuards ? v : g;
}

std::vector<std::pair<std::string, GpuGuard*>> vulkanCardGuards() {
    const bool was = g_vulkanGuards;
    g_vulkanGuards = true;
    std::vector<std::pair<std::string, GpuGuard*>> v = {{"groups", &gpuVkGroupsGuard()}, {"outline", &gpuVkOutlineGuard()}, {"rocks", &gpuVkRocksGuard()}, {"cloud", &gpuVkCloudGuard()},
                                                       {"slice", &gpuSliceGuard()}, {"jump", &gpuJumpGuard()}, {"molcloud", &gpuMolCloudGuard()},
                                                       {"lightpaths", &gpuGuard()}, {"sky", &gpuSkyGuard()}, {"lens", &gpuVkLensGuard()},   // (0.62.0)
                                                       {"rings", &gpuVkRingsGuard()}, {"classic", &gpuVkClassicGuard()},   // (0.64.0)
                                                       {"nucdensity", &gpuVkNucDensGuard()},   // (0.67.0)
                                                       {"lines", &gpuVkLinesGuard()}, {"surface", &gpuVkSurfaceGuard()}};   // (0.68.0)
    g_vulkanGuards = was;
    return v;
}

}  // namespace app
