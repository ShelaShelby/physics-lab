// Frame timing, so real headroom can be measured (turn VSync off to see past the monitor's refresh rate).
#pragma once

#include <algorithm>

namespace ui {

struct FrameStats {
    static constexpr int kHistory = 240;       // frames kept for the graph and the average
    float frameMs[kHistory] = {};              // time from one frame to the next, ms
    float workMs[kHistory] = {};               // time spent building and submitting the frame (before waiting for the screen), ms
    int head = 0, count = 0;

    void add(double frame, double work) {
        frameMs[head] = static_cast<float>(frame);
        workMs[head] = static_cast<float>(work);
        head = (head + 1) % kHistory;
        count = std::min(count + 1, kHistory);
    }
    float last() const { return count ? frameMs[(head + kHistory - 1) % kHistory] : 0; }
    float average(const float* a) const {
        if (!count) return 0;
        double s = 0;
        for (int i = 0; i < count; i++) s += a[i];
        return static_cast<float>(s / count);
    }
    float worst() const { return count ? *std::max_element(frameMs, frameMs + count) : 0; }
    float avgFrame() const { return average(frameMs); }
    float avgWork() const { return average(workMs); }
    // the frame times oldest first, for a graph
    void ordered(float out[kHistory]) const {
        for (int i = 0; i < kHistory; i++) out[i] = frameMs[(head + i) % kHistory];
    }
};

}  // namespace ui
