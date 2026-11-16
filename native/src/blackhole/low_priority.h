// 0.55.5: worker threads below normal priority, so the window stays responsive while the Black Hole Lab works out a picture (the fine table, the
// pictures behind the view, the eye's glare). A thread lowered here is marked, and the pools of disc.cpp, light.cpp and spin_sky.cpp lower their own
// threads when the thread that starts them is marked (on Windows a new thread starts at normal priority whatever its parent's is).
#pragma once

namespace blackhole {

void lowerThisThread();   // below normal priority for the calling thread (Windows: SetThreadPriority, one step below normal; Linux: nice +5 for this thread) and the mark
bool thisThreadLow();     // the mark: the calling thread was lowered

}  // namespace blackhole
