// 0.55.5: worker threads below normal priority, so the window stays responsive while the Black Hole Lab works out a picture (the fine table, the
// pictures behind the view, the eye's glare). A thread lowered here is marked, and the pools of disc.cpp, light.cpp and spin_sky.cpp lower their own
// threads when the thread that starts them is marked (on Windows a new thread starts at normal priority whatever its parent's is).
#pragma once

#include <functional>

namespace blackhole {

void lowerThisThread();   // below normal priority for the calling thread (Windows: SetThreadPriority, one step below normal; Linux: nice +5 for this thread) and the mark
bool thisThreadLow();     // the mark: the calling thread was lowered
// 0.55.6: job(k) for every k in 0..n-1, shared out over up to `threads` threads (0: all cores, at most 16; one fewer when the caller is lowered, so the
// frame's thread keeps a core), the calling thread among them, each taking the next k
// in turn; the pool's threads are lowered when the caller is. Returns when every job is done. (A plain std::function: no generic lambdas nested in lambdas.)
void parallelFor(int n, const std::function<void(int)>& job, int threads = 0);

}  // namespace blackhole
