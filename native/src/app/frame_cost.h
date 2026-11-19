// 0.63.1: where a frame's time went, for the benchmark's worst frames (app/bench.h BenchFrameCost) and its "waited for the card" line. The graphics code
// adds to these counters as it works (any thread; the frame's own thread is told apart where it matters); main.cpp takes them once a frame.
#pragma once

namespace app {

enum class FrameCostKind {
    CardWait,        // the frame's thread waited for the card: the swapchain frame's fence (Vulkan)
    Acquire,         // the swapchain's next image (vkAcquireNextImageKHR)
    Present,         // vkQueuePresentKHR
    Swapchain,       // the swapchain made again (count and ms)
    Pipeline,        // a pipeline made on the frame's thread (count and ms)
    Readback,        // a card batch waited for on the frame's thread (a read-back; count and ms)
    WorkerWait,      // the frame's thread waited for a worker thread (count and ms)
    GuardDeferred,   // a guarded step moved to a later frame (another guard's step was open; count)
    BenchStep,       // the benchmark's own step before the frame's clock (main.cpp: benchmarkBeginFrame)
    Events,          // the window's events read (main.cpp)
    Bookkeeping,     // after the frame's clock: the benchmark's numbers (main.cpp: benchmarkEndFrame)
    Settings,        // the settings file saved when changed (main.cpp)
    kCount
};

struct FrameCost {
    double ms[static_cast<int>(FrameCostKind::kCount)] = {};
    int n[static_cast<int>(FrameCostKind::kCount)] = {};
    const char* what[static_cast<int>(FrameCostKind::kCount)] = {};   // the longest event's label (FrameCostLabel), or null
    double get(FrameCostKind k) const { return ms[static_cast<int>(k)]; }
    int count(FrameCostKind k) const { return n[static_cast<int>(k)]; }
    double waitMs() const { return get(FrameCostKind::CardWait) + get(FrameCostKind::Acquire) + get(FrameCostKind::Readback); }   // the card's share of the work time
};

void frameCostAdd(FrameCostKind k, double ms, const char* what = nullptr);   // one event of that kind (thread-safe); what: a string literal
// a label for the waits started on this thread while it lives ("the glare's blocks"): the worst frames name what was waited for
struct FrameCostLabel {
    const char* prev;
    explicit FrameCostLabel(const char* what);
    ~FrameCostLabel();
    FrameCostLabel(const FrameCostLabel&) = delete;
    FrameCostLabel& operator=(const FrameCostLabel&) = delete;
};
const char* frameCostLabel();
FrameCost frameCostPeek();                          // what was added since the last take
FrameCost frameCostTake();                          // ... and start afresh (main.cpp, once a frame)
bool frameCostFrameThread();                        // this is the frame's thread (app/gpu_guard.h gpuGuardFrameThread)
double frameCostNowMs();                            // a steady clock (ms) for the callers' own spans

}  // namespace app

namespace app {
// a span of one kind, added when it ends (frameOnly: only when on the frame's thread)
struct FrameCostSpan {
    FrameCostKind k;
    bool on;
    double t0;
    const char* what;
    explicit FrameCostSpan(FrameCostKind kind, bool frameOnly = true) : k(kind), on(!frameOnly || frameCostFrameThread()), t0(on ? frameCostNowMs() : 0.0), what(frameCostLabel()) {}
    ~FrameCostSpan() { if (on) frameCostAdd(k, frameCostNowMs() - t0, what); }
    FrameCostSpan(const FrameCostSpan&) = delete;
    FrameCostSpan& operator=(const FrameCostSpan&) = delete;
};
}  // namespace app
