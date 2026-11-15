// The GPU timer of the benchmark, without any OpenGL calls in it, so it can be tested.
//  - QueryRing: a few query slots used in turn; results are collected oldest first, when they are ready.
//  - judgeGpuTimer: compares what the timer says for a known load with a clock, and says whether the timer can be trusted.
//  - the check of one scene's numbers.
#pragma once

#include <deque>
#include <string>
#include <vector>

namespace app {

class QueryRing {
public:
    explicit QueryRing(int slots) : busy_(slots, false) {}
    int slots() const { return int(busy_.size()); }
    // A slot for this frame's queries, or -1 when every slot still waits for its result (the frame is then not measured).
    int acquire() {
        for (int k = 0; k < slots(); k++) {
            const int s = (next_ + k) % slots();
            if (!busy_[s]) { next_ = (s + 1) % slots(); return s; }
        }
        return -1;
    }
    // The frame's queries were issued.
    void submitted(int slot) { busy_[slot] = true; order_.push_back(slot); }
    // Collects the finished results oldest first; stops at the first one that is not ready yet (the GPU finishes in order).
    // ready(slot) -> bool; read(slot) -> a result; each result is added to `out`.
    template <class Ready, class Read, class T>
    void collect(Ready ready, Read read, std::vector<T>& out) {
        while (!order_.empty() && ready(order_.front())) {
            const int s = order_.front();
            order_.pop_front();
            busy_[s] = false;
            out.push_back(read(s));
        }
    }
    int waiting() const { return int(order_.size()); }
    void reset() { std::fill(busy_.begin(), busy_.end(), false); order_.clear(); next_ = 0; }

private:
    std::vector<bool> busy_;
    std::deque<int> order_;
    int next_ = 0;
};

// The self-check: one known load (many big see-through rectangles) is drawn a few times; `queryMs` is what GL_TIME_ELAPSED says for it
// and `wallMs` is what a clock says from the start of the drawing until glFinish() returned (the GPU has really finished).
struct GpuVerdict {
    bool reliable = false;
    std::string why;             // for the result text: why the timer cannot be trusted, or how well it agreed
};
constexpr double kGpuCheckMinWallMs = 0.5;      // a load shorter than this tells nothing
constexpr double kGpuCheckLow = 0.6, kGpuCheckHigh = 1.3;   // the timer must say 60 % to 130 % of the clock's time
GpuVerdict judgeGpuTimer(double queryMs, double wallMs);

// One scene's GPU numbers: how many of its frames got a result, and whether the average is believable next to the frame time.
// (The span, first to last GPU command, can never be shorter than the busy time inside it.)
// Returns "" when the numbers are fine, else the reason why they are not reliable.
std::string gpuSceneProblem(int framesCounted, int gpuSamples, double gpuAvgMs, double gpuSpanAvgMs, double frameAvgMs);

}  // namespace app
