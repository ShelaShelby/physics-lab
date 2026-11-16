#include "blackhole/low_priority.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

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

void parallelFor(int n, const std::function<void(int)>& job, int threads) {
    if (n <= 0) return;
    const int hw = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    const int nth = std::min(n, threads > 0 ? threads : std::min(tLow ? std::max(1, hw - 1) : hw, 16));   // (below normal: one core left for the frame)
    std::atomic<int> next{0};
    const std::function<void()> worker = [&] { for (int k = next++; k < n; k = next++) job(k); };
    if (nth <= 1) { worker(); return; }
    const bool low = tLow;
    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(nth - 1));
    for (int w = 1; w < nth; w++) pool.emplace_back([&worker, low] { if (low) lowerThisThread(); worker(); });
    worker();
    for (std::thread& t : pool) t.join();
}

}  // namespace blackhole
