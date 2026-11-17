#include "molecule/molecule_card_paths.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"

namespace molecule {

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double limitS() {
    static const double s = [] { const char* e = std::getenv("PL_MOL_CARDLIMIT"); const double v = e ? std::atof(e) : 0; return v > 0 ? v : 10.0; }();
    return s;
}
std::string stepName(const char* what) { return std::string("the Density cloud's ") + what; }
struct GuardSteps : render::KerrSteps {
    void begin(const std::string& step) override { app::gpuMolCloudGuard().begin(step); }
    void end(const std::string& step, const std::string& result) override { app::gpuMolCloudGuard().end(step, result); }
};
std::string msText(const char* what, double s) { char b[96]; std::snprintf(b, sizeof b, "%s after %.0f ms", what, 1000 * s); return b; }
constexpr long kOwnDots = 8192, kBoxDots = 4096;                     // the check: the lab's own first dots and a box beyond every molecule, under each camera
}  // namespace

MolCardPaths& molCardPaths() {
    static MolCardPaths p;
    return p;
}

void MolCardPaths::off(const std::string& why) {
    state_ = -1;
    why_ = why;
    std::printf("PLMOLCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void MolCardPaths::frame(render::Renderer2D& R, const float* pts, long n) {
    app::GpuGuard& G = app::gpuMolCloudGuard();
    render::MoleculeCard& C = R.moleculeCard();
    if (gen_ != G.generation()) {                                        // the first frame, and "Try again" (Settings, Performance)
        gen_ = G.generation();
        if (state_ == -1) {
            const int keepFrames = frames_;
            state_ = 0; step_ = 0; stepAt_ = 0; firstUse_ = firstDone_ = false; jobs_.clear(); caseDots_.clear(); caseCam_.clear(); doneJobs_ = 0; allOk_ = true;
            why_.clear(); check_.clear(); worst_.clear();
            frames_ = keepFrames;
        }
    }
    if (state_ == 0) {
        if (!R.outlineCompute()) { off("this PC has no OpenGL 4.3 compute shaders (or they are switched off)"); return; }
        if (G.started() && !G.allowed()) { off(G.offText().empty() ? std::string("its guard switched it off") : G.offText()); return; }
        if (!pts || n <= 0 || ++frames_ < 3) return;                     // (its picture on screen first)
        state_ = 2; step_ = 0;
    }
    if (state_ == 2) { advance(R, pts, n); return; }
    if (state_ != 1 || !firstUse_ || firstDone_) return;
    const int fs = C.fenceState();
    if (fs == 1) { G.end(stepName("first full use"), msText("drawn", nowS() - stepAt_)); G.finished(true); firstDone_ = true; }
    else if (fs < 0 || nowS() - stepAt_ > limitS()) {
        if (fs < 0) { G.end(stepName("first full use"), "the card's fence failed"); G.finished(false, "the card's fence failed"); }
        else G.timedOut(stepName("first full use"));
        C.setReady(false);
        off("a step of the Density cloud on the card did not finish in time (the first full use)");
    }
}

bool MolCardPaths::use(render::Renderer2D& R, bool switchOn) {
    if (state_ != 1 || !switchOn || !R.moleculeCard().ready()) return false;
    if (!firstUse_) { app::gpuMolCloudGuard().begin(stepName("first full use")); firstUse_ = true; stepAt_ = nowS(); }
    return true;
}

void MolCardPaths::advance(render::Renderer2D& R, const float* pts, long n) {
    app::GpuGuard& G = app::gpuMolCloudGuard();
    render::MoleculeCard& C = R.moleculeCard();
    const double now = nowS();
    const std::string small = stepName("first small run"), checkName = stepName("check");
    const std::vector<CloudCam> cams = cloudCheckCams();
    if (step_ == 0) {                                                    // compile and link (two steps, logged by the card's build)
        static GuardSteps steps;
        std::string why;
        if (!C.build(&steps, why)) { G.finished(false, why); off(why); return; }
        step_ = 1;
        return;
    }
    if (step_ == 1) {                                                    // a first small run: 64 of the lab's dots, read back without waiting
        G.begin(small);
        const int job = C.beginProject(pts, std::min(n, 64L), cardCam(cams[0]));
        if (!job) { G.end(small, "could not be started"); G.finished(false, "the card's first small run could not be started"); off("the card's first small run could not be started"); return; }
        jobs_.assign(1, job); stepAt_ = now; step_ = 2;
        return;
    }
    if (step_ == 2) {
        std::vector<float> res;
        const render::JobState st = C.pollProject(jobs_[0], res);
        if (st == render::JobState::Pending) {
            if (now - stepAt_ > limitS()) { G.timedOut(small); off("a step of the card's program did not finish in time (the first small run)"); }
            return;
        }
        if (st == render::JobState::Failed) { G.end(small, "FAILED (the card's fence)"); G.finished(false, "the card's first small run failed"); off("the card's first small run failed"); return; }
        G.end(small, msText("read back", now - stepAt_));
        G.begin(checkName);                                              // the check: every case started now, compared when all have come back
        caseDots_.clear(); caseCam_.clear(); jobs_.clear(); doneJobs_ = 0; allOk_ = true; check_.clear(); worst_.clear();
        std::vector<float> box;
        cloudCheckDots(kBoxDots, box);
        const std::vector<float> own(pts, pts + 3 * std::min(n, kOwnDots));
        for (std::size_t k = 0; k < cams.size(); k++)
            for (int set = 0; set < 2; set++) {
                caseDots_.push_back(set == 0 ? own : box);
                caseCam_.push_back(static_cast<int>(k));
                jobs_.push_back(C.beginProject(caseDots_.back().data(), static_cast<long>(caseDots_.back().size() / 3), cardCam(cams[k])));
            }
        for (int j : jobs_) if (!j) { G.end(checkName, "could not be started"); G.finished(false, "the card's check could not be started"); off("the card's check could not be started"); return; }
        stepAt_ = now; step_ = 3;
        return;
    }
    if (step_ == 3) {
        static std::vector<CloudCompare> acc;
        acc.resize(jobs_.size());
        for (std::size_t i = 0; i < jobs_.size(); i++) {
            if (jobs_[i] == 0) continue;                                 // (already compared)
            std::vector<float> res;
            const render::JobState st = C.pollProject(jobs_[i], res);
            if (st == render::JobState::Pending) continue;
            jobs_[i] = 0; doneJobs_++;
            if (st == render::JobState::Failed) { allOk_ = false; worst_ = "a batch failed on the card"; continue; }
            const std::vector<float>& d = caseDots_[i];
            acc[i] = compareCloud(cams[std::size_t(caseCam_[i])], d.data(), static_cast<long>(d.size() / 3), res);
            if (!acc[i].ok()) allOk_ = false;
            std::printf("PLMOLCARDCASE %zu camera=%d dots=%s: %s\n", i, caseCam_[i], i % 2 == 0 ? "own" : "box", cloudCompareText(acc[i]).c_str());
        }
        std::fflush(stdout);
        if (doneJobs_ < static_cast<int>(jobs_.size())) {
            if (now - stepAt_ > limitS()) { G.timedOut(checkName); off("a step of the card's program did not finish in time (the check)"); }
            return;
        }
        CloudCompare all;
        for (const CloudCompare& k : acc) {
            all.dots += k.dots; all.bothDrawn += k.bothDrawn; all.flagDiff += k.flagDiff; all.flagExplained += k.flagExplained; all.pointBad += k.pointBad;
            all.worstRatio = std::max(all.worstRatio, k.worstRatio);
        }
        char b[400];
        std::snprintf(b, sizeof b, "%zu cases, %s", jobs_.size(), cloudCompareText(all).c_str());
        static const bool forced = std::getenv("PL_MOL_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) allOk_ = false;
        check_ = b;
        caseDots_.clear();
        const std::string why = forced ? "the check was made to fail (test switch PL_MOL_CARDFAIL)" : worst_.empty() ? "its start-up check against the processor failed (" + std::string(b) + ")" : worst_;
        G.end(checkName, msText(allOk_ ? "passed" : "FAILED", now - stepAt_) + ": " + b);
        if (!allOk_) { G.finished(false, why); off(why); return; }
        state_ = 1;
        C.setReady(true);
        std::printf("PLMOLCARD state=1 check=%s\n", b);
        std::fflush(stdout);
    }
}

std::string MolCardPaths::pathText(bool switchOn, bool computeOk) const {
    const std::string what = "the Density cloud's dots: ";
    if (!computeOk) return what + "the processor (no OpenGL 4.3 compute on this PC)";
    if (!switchOn) return what + "the processor (GPU Density cloud switched off)";
    if (state_ == 1) return what + "the graphics card (its start-up check passed: " + check_ + ")";
    if (state_ == -1) return what + "the processor, because " + why_;
    return what + "the graphics card once its start-up check has passed (made when the Density style's cloud is first shown)";
}

}  // namespace molecule
