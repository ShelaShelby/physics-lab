#include "app/frame_cost.h"

#include <chrono>
#include <mutex>

namespace app {

namespace {
std::mutex g_mx;
FrameCost g_cost;
double g_longest[static_cast<int>(FrameCostKind::kCount)] = {};
thread_local const char* t_label = nullptr;
}  // namespace

void frameCostAdd(FrameCostKind k, double ms, const char* what) {
    const int i = static_cast<int>(k);
    if (i < 0 || i >= static_cast<int>(FrameCostKind::kCount)) return;
    std::lock_guard<std::mutex> g(g_mx);
    g_cost.ms[i] += ms > 0 ? ms : 0.0;
    g_cost.n[i]++;
    if (what && (!g_cost.what[i] || ms > g_longest[i])) { g_cost.what[i] = what; g_longest[i] = ms; }
}

FrameCostLabel::FrameCostLabel(const char* what) : prev(t_label) { t_label = what; }
FrameCostLabel::~FrameCostLabel() { t_label = prev; }
const char* frameCostLabel() { return t_label; }

FrameCost frameCostPeek() {
    std::lock_guard<std::mutex> g(g_mx);
    return g_cost;
}

FrameCost frameCostTake() {
    std::lock_guard<std::mutex> g(g_mx);
    const FrameCost c = g_cost;
    g_cost = FrameCost{};
    for (double& x : g_longest) x = 0;
    return c;
}

double frameCostNowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace app
