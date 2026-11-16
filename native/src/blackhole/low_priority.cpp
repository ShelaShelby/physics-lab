#include "blackhole/low_priority.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace blackhole {

namespace {
thread_local bool tLow = false;
}

void lowerThisThread() {
    if (tLow) return;
    tLow = true;
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#elif defined(__linux__)
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 5);    // (Linux: the nice value belongs to the thread)
#endif
}

bool thisThreadLow() { return tLow; }

}  // namespace blackhole
