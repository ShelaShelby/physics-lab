#include "app/gpu_timer.h"

#include <cstdio>

namespace app {

GpuVerdict judgeGpuTimer(double queryMs, double wallMs) {
    char b[200];
    GpuVerdict v;
    if (!(wallMs >= kGpuCheckMinWallMs)) {
        std::snprintf(b, sizeof b, "the self-check load was too small to judge the timer (the clock said %.2f ms)", wallMs > 0 ? wallMs : 0.0);
        v.why = b;
    } else if (!(queryMs > 0)) {
        v.why = "the timer returned 0 for a load that took the clock a measurable time";
    } else if (queryMs < kGpuCheckLow * wallMs || queryMs > kGpuCheckHigh * wallMs) {
        std::snprintf(b, sizeof b, "the timer said %.2f ms but the clock said %.2f ms for the same known load", queryMs, wallMs);
        v.why = b;
    } else {
        std::snprintf(b, sizeof b, "self-check with a known load: timer %.2f ms, clock %.2f ms", queryMs, wallMs);
        v.why = b;
        v.reliable = true;
    }
    return v;
}

std::string gpuSceneProblem(int framesCounted, int gpuSamples, double gpuAvgMs, double gpuSpanAvgMs, double frameAvgMs) {
    char b[200];
    if (framesCounted <= 0 || gpuSamples <= 0) return "the driver gave no result";
    if (gpuSamples * 2 < framesCounted) {
        std::snprintf(b, sizeof b, "only %d of %d frames gave a result", gpuSamples, framesCounted);
        return b;
    }
    if (gpuAvgMs > 1.5 * frameAvgMs) return "the GPU time is longer than the frame itself";
    if (gpuSpanAvgMs < 0.8 * gpuAvgMs) return "the two GPU measurements contradict each other";
    return "";
}

}  // namespace app
