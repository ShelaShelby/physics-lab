#include "ui/porkchop_runner.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "orbit/porkchop_gpu.h"
#include "ui/orbit_panel.h"

namespace ui {

namespace {
render::Renderer2D* g_renderer = nullptr;
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double limitS() {
    static const double s = [] { const char* e = std::getenv("PL_PORKCHOP_CARDLIMIT"); const double v = e ? std::atof(e) : 0; return v > 0 ? v : 10.0; }();
    return s;
}
std::string msText(const char* what, double s) { char b[96]; std::snprintf(b, sizeof b, "%s after %.0f ms", what, 1000 * s); return b; }
struct GuardSteps : render::KerrSteps {
    void begin(const std::string& step) override { app::gpuVkPorkchopGuard().begin(step); }
    void end(const std::string& step, const std::string& result) override { app::gpuVkPorkchopGuard().end(step, result); }
};
// the check's two grids: the lab's own Earth to Mars and Earth to Venus transfers (24 x 24 cells each, fixed dates)
orbit::lambert::PorkchopSetup checkSetup(int k) {
    orbit::lambert::PorkchopSetup s;
    s.nDep = s.nArr = 24;
    if (k == 0) { s.dep = 2; s.arr = 3; s.depJD0 = 2461200.5; s.depStep = 8; s.arrJD0 = 2461300.5; s.arrStep = 10; }
    else { s.dep = 2; s.arr = 1; s.depJD0 = 2461300.5; s.depStep = 4; s.arrJD0 = 2461380.5; s.arrStep = 4; }
    return s;
}
}  // namespace

void porkchopSetRenderer(render::Renderer2D* r) { g_renderer = r; }

PorkchopRunner& porkchopRunner() { static PorkchopRunner r; return r; }

void PorkchopRunner::off(const std::string& why) {
    state_ = -1;
    why_ = why;
    if (g_renderer) g_renderer->porkchopCard().setReady(false);
    std::printf("PLPORKCHOPCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void PorkchopRunner::startCpu(const orbit::lambert::PorkchopInput& in) {
    lastGpu_ = false;
    cpu_ = std::async(std::launch::async, [in] { return orbit::lambert::porkchopCpu(in); });
}

std::string PorkchopRunner::pathText(bool computeOk) const {
    const std::string what = "the porkchop grid (Lambert's problem per cell): ";
    if (!computeOk || !g_renderer) return what + "the processor (no Vulkan compute on this PC)";
    if (state_ == 1) return what + "the graphics card (its start-up check passed: " + check_ + ")";
    if (state_ == -1) return what + "the processor, because " + why_;
    return what + "the graphics card once its start-up check has passed (made when the first grid is asked for in the Data panel's transfer planner)";
}

// the card's steps, one a frame, each logged and guarded (the pattern of atom/atom_card_paths.cpp)
void PorkchopRunner::advance() {
    if (!g_renderer) return;
    render::Renderer2D& R = *g_renderer;
    render::PorkchopCard& C = R.porkchopCard();
    app::GpuGuard& G = app::gpuVkPorkchopGuard();
    if (gen_ != G.generation()) {                                            // the first call, and "Try again" (Settings, Performance)
        gen_ = G.generation();
        if (state_ == -1) { state_ = 0; step_ = 0; why_.clear(); firstUse_ = firstDone_ = false; }
    }
    const double now = nowS();
    if (state_ == 0) {
        if (!R.computeShaders()) { off(R.vulkan() ? "this Vulkan start has no compute programs on the card (or they are switched off)" : "no Vulkan renderer"); return; }
        if (G.started() && !G.allowed()) { off(G.offText().empty() ? std::string("its guard switched it off") : G.offText()); return; }
        if (!G.hold()) return;                                               // its start-up steps wait for the guards' turn (a later frame)
        state_ = 2; step_ = 0;
    }
    if (state_ == 1) {
        if (firstUse_ && !firstDone_ && now - stepAt_ > limitS()) {
            G.timedOut("the porkchop grid's first full use");
            C.setReady(false);
            off("a step of the porkchop grid on the card did not finish in time (the first full use)");
        }
        return;
    }
    if (state_ != 2) return;
    const std::string small = "the porkchop grid's first small run", checkName = "the porkchop grid's check";
    if (step_ == 0) {                                                        // compile and link (two steps, logged by the card's build)
        static GuardSteps steps;
        std::string why;
        if (!C.build(&steps, why)) { G.finished(false, why); off(why); return; }
        step_ = 1;
        return;
    }
    if (step_ == 1) {                                                        // a first small run: the first check grid, read back without waiting
        G.begin(small);
        for (int k = 0; k < 2; k++) {
            checkIn_[k] = orbit::lambert::porkchopInput(checkSetup(k));
            checkRef_[k] = orbit::lambert::porkchopCpu(checkIn_[k]);
        }
        checkJobs_[0] = C.begin(orbit::lambert::gpuJobOf(checkIn_[0]));
        if (!checkJobs_[0]) { G.end(small, "could not be started"); G.finished(false, "the card's first small run could not be started"); off("the card's first small run could not be started"); return; }
        stepAt_ = now; step_ = 2;
        return;
    }
    if (step_ == 2) {
        static std::vector<float> res0;
        const render::JobState st = C.poll(checkJobs_[0], res0);
        if (st == render::JobState::Pending) {
            if (now - stepAt_ > limitS()) { G.timedOut(small); off("a step of the card's program did not finish in time (the first small run)"); }
            return;
        }
        if (st == render::JobState::Failed) { G.end(small, "FAILED (the card's fence)"); G.finished(false, "the card's first small run failed"); off("the card's first small run failed"); return; }
        G.end(small, msText("read back", now - stepAt_));
        G.begin(checkName);
        allOk_ = true; doneJobs_ = 0; check_.clear(); why_.clear();
        for (int k = 0; k < 2; k++) {
            checkJobs_[k] = C.begin(orbit::lambert::gpuJobOf(checkIn_[k]));
            if (!checkJobs_[k]) { G.end(checkName, "could not be started"); G.finished(false, "the card's check could not be started"); off("the card's check could not be started"); return; }
        }
        stepAt_ = now; step_ = 3;
        return;
    }
    if (step_ == 3) {
        static std::vector<float> res[2];
        static bool got[2];
        if (doneJobs_ == 0) got[0] = got[1] = false;
        static orbit::lambert::GridCompare cmp[2];
        for (int k = 0; k < 2; k++) {
            if (got[k]) continue;
            const render::JobState st = C.poll(checkJobs_[k], res[k]);
            if (st == render::JobState::Pending) continue;
            got[k] = true; doneJobs_++;
            if (st == render::JobState::Failed) { allOk_ = false; why_ = "a batch failed on the card"; continue; }
            cmp[k] = orbit::lambert::compareGrids(checkIn_[k], checkRef_[k], orbit::lambert::gridOfCard(checkIn_[k], res[k]));
            const bool ok = cmp[k].over == 0 && cmp[k].flipLow == 0 && cmp[k].flips * 1000 <= cmp[k].cells;
            if (!ok) allOk_ = false;
            std::printf("PLPORKCHOPCARDCASE %d %s to %s: %ld cells, %ld solved by both, %ld by one side only, %ld beyond the bound, worst C3 miss %.2g of its bound\n", k,
                        orbit::kPlanets[checkIn_[k].s.dep].name, orbit::kPlanets[checkIn_[k].s.arr].name, cmp[k].cells, cmp[k].both, cmp[k].flips, cmp[k].over, cmp[k].worstC3OfBound);
        }
        std::fflush(stdout);
        if (doneJobs_ < 2) {
            if (now - stepAt_ > limitS()) { G.timedOut(checkName); off("a step of the card's program did not finish in time (the check)"); }
            return;
        }
        char b[300];
        std::snprintf(b, sizeof b, "2 grids of %ld cells against the processor's double precision: %ld solved by both, %ld by one side only, %ld beyond the derived bound (the largest miss %.2g of it)",
                      cmp[0].cells, cmp[0].both + cmp[1].both, cmp[0].flips + cmp[1].flips, cmp[0].over + cmp[1].over, std::max(cmp[0].worstC3OfBound, cmp[1].worstC3OfBound));
        static const bool forced = std::getenv("PL_PORKCHOP_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) allOk_ = false;
        check_ = b;
        const std::string why = forced ? "the check was made to fail (test switch PL_PORKCHOP_CARDFAIL)" : why_.empty() ? "its start-up check against the processor failed (" + std::string(b) + ")" : why_;
        G.end(checkName, msText(allOk_ ? "passed" : "FAILED", now - stepAt_) + ": " + b);
        if (!allOk_) { G.finished(false, why); off(why); return; }
        state_ = 1;
        C.setReady(true);
        G.rest();                                                            // the first full use comes with the first real grid (the turn given back meanwhile)
        std::printf("PLPORKCHOPCARD state=1 check=%s\n", b);
        std::fflush(stdout);
    }
}

void PorkchopRunner::start(AppState& app, const orbit::lambert::PorkchopInput& in) {
    (void)app;
    advance();
    if (gpuJob_ && g_renderer) { g_renderer->porkchopCard().cancel(gpuJob_); gpuJob_ = 0; }
    if (g_renderer && state_ == 1 && g_renderer->porkchopCard().ready() && app::gpuVkPorkchopGuard().hold()) {
        app::GpuGuard& G = app::gpuVkPorkchopGuard();
        if (!firstUse_) { G.begin("the porkchop grid's first full use"); firstUse_ = true; stepAt_ = nowS(); }
        gpuJob_ = g_renderer->porkchopCard().begin(orbit::lambert::gpuJobOf(in));
        if (gpuJob_) { gpuIn_ = in; hasQueued_ = false; discardCpu_ = cpu_.valid(); return; }
    }
    if (cpu_.valid()) { queued_ = in; hasQueued_ = true; return; }          // the worker is busy with an older grid: the newest request waits for it
    startCpu(in);
}

bool PorkchopRunner::poll(AppState& app, orbit::lambert::PorkchopGrid& out) {
    (void)app;
    advance();
    if (gpuJob_ && g_renderer) {
        std::vector<float> res;
        const render::JobState st = g_renderer->porkchopCard().poll(gpuJob_, res);
        if (st == render::JobState::Pending) return false;
        const int job = gpuJob_;
        gpuJob_ = 0;
        (void)job;
        if (st == render::JobState::Done) {
            out = orbit::lambert::gridOfCard(gpuIn_, res);
            lastGpu_ = true; gpuGrids_++;
            if (firstUse_ && !firstDone_) {
                app::GpuGuard& G = app::gpuVkPorkchopGuard();
                G.end("the porkchop grid's first full use", msText("a grid taken from the card", nowS() - stepAt_));
                G.finished(true);
                firstDone_ = true;
            }
            return true;
        }
        if (cpu_.valid()) { queued_ = gpuIn_; hasQueued_ = true; } else startCpu(gpuIn_);   // the batch failed: the processor makes this grid
        return false;
    }
    if (cpu_.valid() && cpu_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        out = cpu_.get(); lastGpu_ = false;
        if (discardCpu_) { discardCpu_ = false; return false; }              // (the card's grid is the newer)
        if (hasQueued_) { hasQueued_ = false; startCpu(queued_); return false; }   // a newer grid was asked for meanwhile: this one is stale
        return true;
    }
    return false;
}

}  // namespace ui
