// 0.92.0: who solves the porkchop grid. The graphics card (render/porkchop_card.h) once its start-up check against the processor has passed, under its own guard (app/gpu_guard.h
// gpuVkPorkchopGuard: a marker before each step, a log, the settings line "gpuvkporkchop="); otherwise (no Vulkan compute, switched off, the guard, a failed check, or the check
// not yet through) the processor, on a worker thread so the frame does not wait. One grid at a time: a new request replaces the one under way.
// The steps, one a frame once the first grid is asked for: compile and link, a first small run, the check (two grids of the lab's own planets against the processor's double-precision
// answer, orbit/lambert.h compareGrids), then the first full use (until the card has answered a real grid).
#pragma once

#include <future>
#include <memory>
#include <string>

#include "orbit/lambert.h"
#include "render/renderer2d.h"

namespace ui {

struct AppState;

// the renderer the card's program is made on (main.cpp sets it once; none: the processor does every grid)
void porkchopSetRenderer(render::Renderer2D* r);

class PorkchopRunner {
public:
    // start a grid; the one under way (if any) is dropped
    void start(AppState& app, const orbit::lambert::PorkchopInput& in);
    // each frame: true when a finished grid was put into `out`
    bool poll(AppState& app, orbit::lambert::PorkchopGrid& out);
    bool busy() const { return gpuJob_ != 0 || cpu_.valid(); }
    bool lastWasGpu() const { return lastGpu_; }
    // 1 the card (its check passed), 0 not decided yet, -1 the processor
    int state() const { return state_ == 1 ? 1 : state_ == -1 ? -1 : 0; }
    std::string pathText(bool computeOk) const;
    long gridsOnCard() const { return gpuGrids_; }
private:
    void advance();
    void off(const std::string& why);
    void startCpu(const orbit::lambert::PorkchopInput& in);
    int state_ = 0;                          // 0 waiting for its first grid, 2 its steps under way, 1 ready, -1 off
    int step_ = 0, gen_ = -1;
    double stepAt_ = 0;
    bool firstUse_ = false, firstDone_ = false;
    int checkJobs_[2] = {0, 0}, doneJobs_ = 0;
    bool allOk_ = true;
    std::string why_, check_;
    orbit::lambert::PorkchopInput checkIn_[2];
    orbit::lambert::PorkchopGrid checkRef_[2];
    int gpuJob_ = 0;
    orbit::lambert::PorkchopInput gpuIn_;
    std::future<orbit::lambert::PorkchopGrid> cpu_;
    bool discardCpu_ = false;                // the card took over while a worker was running: its grid is dropped when it finishes
    bool hasQueued_ = false;                 // a newer request waits for the grid under way on the worker (a worker is never dropped: a future from std::async waits when it goes)
    orbit::lambert::PorkchopInput queued_;
    bool lastGpu_ = false;
    long gpuGrids_ = 0;
};
PorkchopRunner& porkchopRunner();

}  // namespace ui
