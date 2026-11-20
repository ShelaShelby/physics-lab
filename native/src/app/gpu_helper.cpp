#include "app/gpu_helper.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include "app/gpu_guard.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace app {

namespace {
#ifdef _WIN32
HANDLE g_proc = nullptr;
std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
std::wstring quoted(const std::wstring& a) {          // one argument as CommandLineToArgvW reads it back
    std::wstring q = L"\"";
    int bs = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { bs++; continue; }
        if (c == L'"') { q.append(static_cast<std::size_t>(2 * bs + 1), L'\\'); q += L'"'; bs = 0; continue; }
        q.append(static_cast<std::size_t>(bs), L'\\'); bs = 0; q += c;
    }
    q.append(static_cast<std::size_t>(2 * bs), L'\\');
    return q + L"\"";
}
#else
pid_t g_pid = -1;
#endif
int g_exit = -1;

struct Job { std::string name; GpuGuard* guard = nullptr; bool variant = false; };
std::deque<Job> g_queue;
bool g_running = false;
Job g_now;
std::string g_folder, g_device, g_driver;
long g_finished = 0;
std::map<std::string, bool> g_pending;                // a program's helper not finished yet
std::mutex g_pendMx;                                  // 0.68.0: workers ask too (a guarded step on a worker waits for its helper)

std::string resultFile() { return g_folder + "gpu-helper-result.txt"; }
std::string variantsFile() { return g_folder + "gpu-helper-variants.txt"; }
std::string readAll(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

void finish() {
    const std::string text = readAll(resultFile());
    std::vector<std::string> lines;
    { std::istringstream is(text); std::string l; while (std::getline(is, l)) { if (!l.empty() && l.back() == '\r') l.pop_back(); lines.push_back(l); } }
    std::string open, failedStep, failedWhy, made;
    bool done = false, begun = false;
    for (const std::string& l : lines) {
        if (l.rfind("begin ", 0) == 0) { open = l.substr(6); begun = true; }
        else if (l.rfind("end ", 0) == 0) {
            const std::string r = l.substr(4);
            const std::size_t a = r.find('|'), b = a == std::string::npos ? a : r.find('|', a + 1);
            const std::string step = r.substr(0, a), how = a == std::string::npos ? std::string() : r.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1),
                              rest = b == std::string::npos ? std::string() : r.substr(b + 1);
            if (how == "ok") made += (made.empty() ? "" : "; ") + step + " (" + rest + " ms)";
            else if (failedStep.empty()) { failedStep = step; failedWhy = rest; }
            open.clear();
        } else if (l == "done") done = true;
    }
    char code[48];
    std::snprintf(code, sizeof code, "exit code %d", g_exit);
    GpuGuard* G = g_now.guard;
    if (g_now.variant) {                                  // the light paths' detective work: one variant a helper, its line kept and logged
        std::string line = "variant " + g_now.name + ": ";
        if (!failedStep.empty()) line += "the driver said no: " + failedWhy;
        else if (!open.empty()) line += std::string("the helper died making it (") + code + ")";
        else if (done && !made.empty()) line += "made: " + made;
        else line += std::string("not tried (the helper could not start its Vulkan device: ") + code + ")";
        std::ofstream(variantsFile(), std::ios::app) << line << "\n";
        if (G) G->log("the helper's detective work, kKerrTableCS " + line);
        std::printf("PLGPUHELPER %s\n", line.c_str());
    } else {
        if (!open.empty()) {
            if (G) G->helperDied(open, code);
            std::printf("PLGPUHELPER %s died making %s (%s)\n", g_now.name.c_str(), open.c_str(), code);
        } else if (!failedStep.empty()) {
            if (G) G->failed(failedStep, "the driver said no in the helper: " + failedWhy);
            std::printf("PLGPUHELPER %s failed at %s: %s\n", g_now.name.c_str(), failedStep.c_str(), failedWhy.c_str());
        } else if (done && begun) {
            if (G) G->log("the helper made it with this driver: " + made + "; the app makes it now, guarded as before");
            std::printf("PLGPUHELPER %s passed: %s\n", g_now.name.c_str(), made.c_str());
        } else {
            if (G) G->log(std::string("the helper could not start its Vulkan device (") + code + "): the app makes it itself, guarded as before");
            std::printf("PLGPUHELPER %s not tried (%s)\n", g_now.name.c_str(), code);
        }
        { std::lock_guard<std::mutex> l(g_pendMx); g_pending[g_now.name] = false; }
    }
    std::fflush(stdout);
    g_finished++;
}

bool startNext() {
    while (!g_queue.empty()) {
        g_now = g_queue.front();
        g_queue.pop_front();
        std::remove(resultFile().c_str());
        std::vector<std::string> args = {"--gpu-helper", g_folder, g_device, g_now.variant ? "variant" : "program", g_now.name};
        if (g_now.guard) g_now.guard->log(std::string(g_now.variant ? "the helper starts (kKerrTableCS variant " : "the helper starts (") + g_now.name + ")");
        if (gpuHelperSpawn(args)) { g_running = true; return true; }
        if (g_now.guard) g_now.guard->log("the helper could not be started: the app makes it itself, guarded as before");
        if (!g_now.variant) { std::lock_guard<std::mutex> l(g_pendMx); g_pending[g_now.name] = false; }
    }
    return false;
}
}  // namespace

bool gpuHelperSpawn(const std::vector<std::string>& args) {
    g_exit = -1;
#ifdef _WIN32
    wchar_t self[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, self, static_cast<DWORD>(sizeof self / sizeof self[0]));
    if (n == 0 || n >= sizeof self / sizeof self[0]) return false;
    std::wstring cmd = quoted(self);
    for (const std::string& a : args) cmd += L" " + quoted(widen(a));
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> line(cmd.begin(), cmd.end());
    line.push_back(L'\0');
    // no handles inherited, no window, no error boxes: a driver's crash ends the helper alone
    if (!CreateProcessW(self, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    g_proc = pi.hProcess;
    return true;
#else
    char self[4096];
    const ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) return false;
    self[n] = 0;
    std::vector<std::string> keep = args;
    std::vector<char*> av;
    av.push_back(self);
    for (std::string& a : keep) av.push_back(&a[0]);
    av.push_back(nullptr);
    const pid_t p = fork();
    if (p < 0) return false;
    if (p == 0) {
        for (int fd = 3; fd < 1024; fd++) close(fd);   // (no handles inherited)
        execv(self, av.data());
        _exit(127);
    }
    g_pid = p;
    return true;
#endif
}

int gpuHelperExitCode() {
#ifdef _WIN32
    if (!g_proc) return g_exit;
    if (WaitForSingleObject(g_proc, 0) != WAIT_OBJECT_0) return -1;
    DWORD c = 0;
    GetExitCodeProcess(g_proc, &c);
    CloseHandle(g_proc);
    g_proc = nullptr;
    g_exit = static_cast<int>(c);
    return g_exit;
#else
    if (g_pid < 0) return g_exit;
    int st = 0;
    const pid_t r = waitpid(g_pid, &st, WNOHANG);
    if (r == 0) return -1;
    g_pid = -1;
    g_exit = r < 0 ? 255 : WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    return g_exit;
#endif
}

void gpuHelperQueue(const std::vector<std::pair<std::string, GpuGuard*>>& programs, const std::string& folder, const std::string& device, const std::string& driver) {
    g_folder = folder; g_device = device; g_driver = driver;
    for (const auto& [name, g] : programs) {
        g_queue.push_back({name, g, false});
        { std::lock_guard<std::mutex> l(g_pendMx); g_pending[name] = true; }
        if (g) g->setHelper(name);
    }
}

void gpuHelperVariants(GpuGuard* lightpaths, const std::string& folder, const std::string& device, const std::string& driver) {
    g_folder = folder; g_device = device; g_driver = driver;
    const std::string head = "driver " + device + " / " + driver;
    const std::string kept = readAll(variantsFile());
    if (kept.rfind(head + "\n", 0) == 0) {                   // tried with this driver already: its lines again in the log
        if (lightpaths) lightpaths->log("the helper's detective work was done with this driver already (" + variantsFile() + ")");
        return;
    }
    std::ofstream(variantsFile(), std::ios::trunc) << head << "\n";
    for (const char* v : {"math", "step", "edge", "ray", "short"}) g_queue.push_back({v, lightpaths, true});
}

bool gpuHelperPump() {
    const long before = g_finished;
    if (g_running) {
        const int c = gpuHelperExitCode();
        if (c >= 0) { g_running = false; finish(); }
    }
    if (!g_running) startNext();
    return g_finished != before;
}

bool gpuHelperPending(const std::string& name) { std::lock_guard<std::mutex> l(g_pendMx); const auto it = g_pending.find(name); return it != g_pending.end() && it->second; }
bool gpuHelperBusy() { return g_running || !g_queue.empty(); }
void gpuHelperPrefer(const std::string& name) {   // 0.68.0: a program its lab asks for now goes first (its guard's hold() said no: it waits for it)
    for (std::size_t i = 0; i < g_queue.size(); i++)
        if (!g_queue[i].variant && g_queue[i].name == name) { const Job j = g_queue[i]; g_queue.erase(g_queue.begin() + static_cast<std::ptrdiff_t>(i)); g_queue.push_front(j); return; }
}
void gpuHelperStop() { g_queue.clear(); std::lock_guard<std::mutex> l(g_pendMx); for (auto& [name, on] : g_pending) on = false; }
std::string gpuHelperCacheFile(const std::string& folder) { return folder + "vulkan-pipeline-cache-helper.bin"; }
long gpuHelperFinishedCount() { return g_finished; }

}  // namespace app
