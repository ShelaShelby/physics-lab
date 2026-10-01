// The time step of a frame, and the counters for frames drawn while the window is being moved or resized.
// No graphics in here, so it can be tested.
#pragma once

#include <algorithm>
#include <cmath>

namespace app {

constexpr double kStallS = 1.0;      // a gap between two frames longer than this is a stall (the window was held, the PC slept, ...), not time to simulate;
                                     // a slow frame (even 2 fps) is not a stall: it still counts, up to maxDt

// The time the simulation advances for a frame that came `rawDt` seconds after the last one.
// - an ordinary slow frame counts as at most `maxDt` (as in the web);
// - a stall (more than kStallS) counts as nothing: the simulation does not "catch up" and does not jump;
// - a negative, zero or not-a-number gap counts as nothing.
inline double stepSeconds(double rawDt, double maxDt) {
    if (!(rawDt > 0) || rawDt > kStallS) return 0;
    return std::min(rawDt, maxDt);
}

// What happened while the window system had the window in its own move / resize loop (Windows: a drag by the title bar or an edge).
// Frames drawn from the SDL event watch are "live" frames. If the fix does not work on some PC these numbers show it.
struct LiveStats {
    long frames = 0;             // live frames drawn so far
    int drags = 0;               // how many times a move / resize started (the first live frame after a normal frame)
    double worstGapMs = 0;       // the longest time between two live frames of the same drag
    double leadInMs = 0;         // the longest time from the last normal frame to the first live frame of a drag
    bool prevLive = false;

    void frame(bool live, double frameMs) {
        if (live) {
            frames++;
            if (prevLive) worstGapMs = std::max(worstGapMs, frameMs);
            else { drags++; leadInMs = std::max(leadInMs, frameMs); }
        }
        prevLive = live;
    }
};

}  // namespace app
