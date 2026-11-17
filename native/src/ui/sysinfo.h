// 0.56.1: what the benchmark report says about the computer and the app: the processor's name, cores and threads, the memory installed, the app's
// memory now and its processor time so far. Windows' own calls on Windows, /proc on Linux; a value that cannot be read is 0, -1 or "" (the report then
// says "not available").
#pragma once

#include <string>

namespace ui {

struct SysInfo {
    std::string cpu;          // "" not available
    int cores = 0, threads = 0;
    double memoryMB = -1;     // installed (Windows) or the total the system sees (Linux)
};
SysInfo sysInfo();
double processMemoryMB();     // the app's working set (resident memory) now; -1 not available
double processCpuSeconds();   // the app's processor time so far, all its threads, user and system; -1 not available

}  // namespace ui
