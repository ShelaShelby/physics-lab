#include "ui/sysinfo.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#include <psapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "psapi.lib")
#endif
#else
#include <time.h>
#include <unistd.h>
#endif

namespace ui {

namespace {
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r' || s.back() == '\t' || s.back() == '\0')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}
}  // namespace

#ifdef _WIN32
SysInfo sysInfo() {
    SysInfo s;
    int r[4] = {0, 0, 0, 0};
    __cpuid(r, static_cast<int>(0x80000000));
    if (static_cast<unsigned>(r[0]) >= 0x80000004u) {                    // the processor's own brand string
        char b[49] = {};
        for (int k = 0; k < 3; k++) { __cpuid(r, static_cast<int>(0x80000002 + k)); std::memcpy(b + 16 * k, r, 16); }
        s.cpu = trim(b);
    }
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len > 0) {
        std::string buf(len, '\0');
        auto* p = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(&buf[0]);
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, p, &len)) {
            for (DWORD off = 0; off < len;) {
                auto* q = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(&buf[0] + off);
                if (q->Relationship == RelationProcessorCore) s.cores++;
                off += q->Size;
                if (q->Size == 0) break;
            }
        }
    }
    s.threads = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    ULONGLONG kb = 0;
    if (GetPhysicallyInstalledSystemMemory(&kb) && kb > 0) s.memoryMB = static_cast<double>(kb) / 1024.0;
    else { MEMORYSTATUSEX m; m.dwLength = sizeof m; if (GlobalMemoryStatusEx(&m)) s.memoryMB = static_cast<double>(m.ullTotalPhys) / (1024.0 * 1024.0); }
    return s;
}
double processMemoryMB() {
    PROCESS_MEMORY_COUNTERS c;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof c)) return -1;
    return static_cast<double>(c.WorkingSetSize) / (1024.0 * 1024.0);
}
double processCpuSeconds() {
    FILETIME a, b, k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &a, &b, &k, &u)) return -1;
    const auto t = [](const FILETIME& f) { return static_cast<double>((static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime) * 1e-7; };
    return t(k) + t(u);
}
#else
SysInfo sysInfo() {
    SysInfo s;
    if (std::FILE* f = std::fopen("/proc/cpuinfo", "r")) {
        char line[512];
        std::set<std::pair<int, int>> cores;
        int phys = 0, core = -1;
        while (std::fgets(line, sizeof line, f)) {
            const std::string l = line;
            const std::size_t c = l.find(':');
            if (c == std::string::npos) continue;
            const std::string key = trim(l.substr(0, c)), val = trim(l.substr(c + 1));
            if (key == "model name" && s.cpu.empty()) s.cpu = val;
            else if (key == "physical id") phys = std::atoi(val.c_str());
            else if (key == "core id") { core = std::atoi(val.c_str()); cores.insert({phys, core}); }
        }
        std::fclose(f);
        s.cores = static_cast<int>(cores.size());
    }
    s.threads = static_cast<int>(std::thread::hardware_concurrency());
    if (std::FILE* f = std::fopen("/proc/meminfo", "r")) {
        char line[256];
        while (std::fgets(line, sizeof line, f)) { long kb = 0; if (std::sscanf(line, "MemTotal: %ld kB", &kb) == 1) { s.memoryMB = kb / 1024.0; break; } }
        std::fclose(f);
    }
    return s;
}
double processMemoryMB() {
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long size = 0, rss = 0;
    const int n = std::fscanf(f, "%ld %ld", &size, &rss);
    std::fclose(f);
    return n == 2 ? static_cast<double>(rss) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0) : -1;
}
double processCpuSeconds() {
    timespec t{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t) != 0) return -1;
    return static_cast<double>(t.tv_sec) + 1e-9 * static_cast<double>(t.tv_nsec);
}
#endif

}  // namespace ui
