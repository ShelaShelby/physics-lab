#include "atom/atom_card_paths.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "app/gpu_guard.h"
#include "atom/atom_card.h"
#include "atom/cloud_sample.h"

namespace atom {

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double limitS() {
    static const double s = [] { const char* e = std::getenv("PL_ATOM_CARDLIMIT"); const double v = e ? std::atof(e) : 0; return v > 0 ? v : 10.0; }();
    return s;
}
app::GpuGuard& guardOf(int which) { return which == 0 ? app::gpuSliceGuard() : app::gpuJumpGuard(); }
const char* nameOf(int which) { return which == 0 ? "slice" : "jump"; }
std::string stepName(int which, const char* what) { return std::string(which == 0 ? "the Slice's " : "the jump's ") + what; }
struct GuardSteps : render::KerrSteps {
    int which = 0;
    void begin(const std::string& step) override { guardOf(which).begin(step); }
    void end(const std::string& step, const std::string& result) override { guardOf(which).end(step, result); }
};
std::string msText(const char* what, double s) { char b[96]; std::snprintf(b, sizeof b, "%s after %.0f ms", what, 1000 * s); return b; }
// the jump check's cases kept from their start to their comparison
std::vector<std::vector<unsigned char>> g_jumpFixed;
}  // namespace

AtomCardPaths& atomCardPaths() {
    static AtomCardPaths p;
    return p;
}

void AtomCardPaths::off(int which, const std::string& why) {
    Path& P = p_[which];
    P.state = -1;
    P.why = why;
    std::printf("PLATOMCARD %s state=-1 why=%s\n", nameOf(which), why.c_str());
    std::fflush(stdout);
}

void AtomCardPaths::frame(render::Renderer2D& R, AtomLab& L) {
    render::AtomCard& C = R.atomCard();
    for (int which = 0; which < 2; which++) {
        Path& P = p_[which];
        app::GpuGuard& G = guardOf(which);
        if (P.gen != G.generation()) {                                       // the first frame, and "Try again" (Settings, Performance)
            P.gen = G.generation();
            if (P.state == -1) { const int keepFrames = P.frames; P = Path(); P.gen = G.generation(); P.frames = keepFrames; }
        }
        const bool shown = which == 0 ? L.p.view == "Slice" : (L.manyOn() && L.p.view == "Cloud 3D" && L.p.cstyle == "Glow" && L.s.trans);   // (the jump's: only once a jump is under way)
        if (P.state == 0) {
            if (!R.outlineCompute()) { off(which, "this PC has no OpenGL 4.3 compute shaders (or they are switched off)"); continue; }
            if (G.started() && !G.allowed()) { off(which, G.offText().empty() ? std::string("its guard switched it off") : G.offText()); continue; }
            if (!shown || ++P.frames < 3) continue;                          // (its picture on screen first)
            P.state = 2; P.step = 0;
        }
        if (P.state == 2) { advance(which, R, L); continue; }
        if (P.state != 1) continue;
        if (which == 0) {
            if (!C.ready() && !P.firstDone) {                                // ready for the lab: the first batch it asks for begins the step "the first full use" (its marker first)
                Path* pp = &P;
                C.beforeFirstUse = [pp] { app::gpuSliceGuard().begin(stepName(0, "first full use")); pp->firstUse = true; pp->stepAt = nowS(); };
                C.setSliceReady(true);
            }
            if (P.firstUse && !P.firstDone) {
                if (L.sliceCardDots() > 0) {
                    char b[160];
                    std::snprintf(b, sizeof b, "%ld dots taken from the card", L.sliceCardDots());
                    G.end(stepName(0, "first full use"), msText(b, nowS() - P.stepAt));
                    G.finished(true);
                    P.firstDone = true;
                } else if (shown && nowS() - P.stepAt > limitS()) {
                    G.timedOut(stepName(0, "first full use"));
                    C.setSliceReady(false);
                    off(0, "a step of the Slice's dots on the card did not finish in time (the first full use)");
                }
            }
        } else if (P.firstUse && !P.firstDone) {
            const int fs = C.fenceState();
            if (fs == 1) { G.end(stepName(1, "first full use"), msText("drawn", nowS() - P.stepAt)); G.finished(true); P.firstDone = true; }
            else if (fs < 0 || nowS() - P.stepAt > limitS()) {
                if (fs < 0) { G.end(stepName(1, "first full use"), "the card's fence failed"); G.finished(false, "the card's fence failed"); }
                else G.timedOut(stepName(1, "first full use"));
                C.setJumpReady(false);
                off(1, "a step of the jump's groups on the card did not finish in time (the first full use)");
            }
        }
    }
    L.sliceCard = p_[0].state == 1 && C.ready() ? &C : nullptr;
    if (p_[0].state != 1) C.setSliceReady(false);
}

void AtomCardPaths::advance(int which, render::Renderer2D& R, AtomLab& L) {
    (void)L;
    Path& P = p_[which];
    app::GpuGuard& G = guardOf(which);
    render::AtomCard& C = R.atomCard();
    const double now = nowS();
    const std::string small = stepName(which, "first small run"), checkName = stepName(which, "check");
    if (P.step == 0) {                                                       // compile and link (two steps, logged by the card's build)
        static GuardSteps steps[2];
        steps[which].which = which;
        std::string why;
        if (!C.build(which, &steps[which], why)) { G.finished(false, why); off(which, why); return; }
        P.step = 1;
        return;
    }
    if (P.step == 1) {                                                       // a first small run: one small batch, read back without waiting
        G.begin(small);
        int job = 0;
        if (which == 0) {
            const SliceCheckCase k = sliceCheckCases().front();
            job = C.beginSlice(sliceJobOf(planeSampler(k.id, k.Z, k.axis, k.off), k.seed, 64));
        } else {
            std::vector<unsigned char> fixedG;
            jumpCheckFixed(7, fixedG);
            job = C.beginJumpRead(fixedG.data(), 7, render::jumpShareThreshold(0.5), render::jumpShareThreshold(0.5));
        }
        if (!job) { G.end(small, "could not be started"); G.finished(false, "the card's first small run could not be started"); off(which, "the card's first small run could not be started"); return; }
        P.jobs[0] = job; P.nJobs = 1; P.stepAt = now; P.step = 2;
        return;
    }
    if (P.step == 2) {
        std::vector<float> res;
        std::vector<unsigned char> grp;
        const render::JobState st = which == 0 ? C.pollSlice(P.jobs[0], res) : C.pollJumpRead(P.jobs[0], grp);
        if (st == render::JobState::Pending) {
            if (now - P.stepAt > limitS()) { G.timedOut(small); off(which, "a step of the card's program did not finish in time (the first small run)"); }
            return;
        }
        if (st == render::JobState::Failed) { G.end(small, "FAILED (the card's fence)"); G.finished(false, "the card's first small run failed"); off(which, "the card's first small run failed"); return; }
        G.end(small, msText("read back", now - P.stepAt));
        G.begin(checkName);                                                  // the check: every case started now, compared when all have come back
        P.nJobs = 0; P.doneJobs = 0; P.allOk = true; P.check.clear(); P.worst.clear();
        if (which == 0) {
            for (const SliceCheckCase& k : sliceCheckCases()) P.jobs[P.nJobs++] = C.beginSlice(sliceJobOf(planeSampler(k.id, k.Z, k.axis, k.off), k.seed, k.tries));
        } else {
            g_jumpFixed.clear();
            for (const JumpCheckCase& k : jumpCheckCases()) {
                g_jumpFixed.emplace_back();
                jumpCheckFixed(k.jn, g_jumpFixed.back());
                P.jobs[P.nJobs++] = C.beginJumpRead(g_jumpFixed.back().data(), k.jn, render::jumpShareThreshold(1 - k.shareNew), render::jumpShareThreshold(k.shareNew));
            }
        }
        for (int i = 0; i < P.nJobs; i++) if (!P.jobs[i]) { G.end(checkName, "could not be started"); G.finished(false, "the card's check could not be started"); off(which, "the card's check could not be started"); return; }
        P.stepAt = now; P.step = 3;
        return;
    }
    if (P.step == 3) {
        long tries = 0, kept = 0, both = 0, diff = 0, expl = 0, bad = 0, dots = 0, wrong = 0;
        double ratio = 0, worstAbs = 0;
        static SliceCompare acc[8];
        static long jumpWrong[8], jumpDots[8];
        for (int i = 0; i < P.nJobs; i++) {
            if (P.jobs[i] == 0) continue;                                    // (already compared)
            std::vector<float> res;
            std::vector<unsigned char> grp;
            const render::JobState st = which == 0 ? C.pollSlice(P.jobs[i], res) : C.pollJumpRead(P.jobs[i], grp);
            if (st == render::JobState::Pending) continue;
            P.jobs[i] = 0; P.doneJobs++;
            if (st == render::JobState::Failed) { P.allOk = false; P.worst = "a batch failed on the card"; continue; }
            if (which == 0) {
                const SliceCheckCase k = sliceCheckCases()[std::size_t(i)];
                acc[i] = compareSlice(planeSampler(k.id, k.Z, k.axis, k.off), k.seed, res, k.tries);
                if (!acc[i].ok()) P.allOk = false;
                std::printf("PLATOMCARDCASE slice %d Z=%d id=%d axis=%d off=%.4f: %s\n", i, k.Z, k.id, k.axis, k.off, sliceCompareText(acc[i]).c_str());
            } else {
                const JumpCheckCase k = jumpCheckCases()[std::size_t(i)];
                std::vector<unsigned char> want;
                jumpCheckExpected(k.jn, k.shareNew, g_jumpFixed[std::size_t(i)], want);
                long w = 0;
                for (std::size_t j = 0; j < want.size(); j++) w += j >= grp.size() || grp[j] != want[j];
                jumpWrong[i] = w; jumpDots[i] = long(want.size());
                if (w) P.allOk = false;
                std::printf("PLATOMCARDCASE jump %d dots=%ld share=%.4f: %ld of %ld groups differ from the processor's\n", i, 2 * k.jn, k.shareNew, w, 2 * k.jn);
            }
        }
        std::fflush(stdout);
        if (P.doneJobs < P.nJobs) {
            if (now - P.stepAt > limitS()) { G.timedOut(checkName); off(which, "a step of the card's program did not finish in time (the check)"); }
            return;
        }
        for (int i = 0; i < P.nJobs; i++) {
            if (which == 0) { tries += acc[i].tries; kept += acc[i].kept; both += acc[i].bothKept; diff += acc[i].flagDiff; expl += acc[i].flagExplained; bad += acc[i].pointBad;
                              ratio = std::max(ratio, acc[i].worstRatio); worstAbs = std::max(worstAbs, acc[i].worstAbs); }
            else { dots += jumpDots[i]; wrong += jumpWrong[i]; }
        }
        char b[400];
        if (which == 0) std::snprintf(b, sizeof b, "%d planes, %ld attempts against the processor's: %ld kept by both, %ld answered differently (%ld within the rounding there), "
                                                   "%ld points beyond their limit, the largest difference %.2g of its limit", P.nJobs, tries, both, diff, expl, bad, ratio);
        else std::snprintf(b, sizeof b, "%d cases, %ld dots' groups against the processor's: %ld differ (the test is exact)", P.nJobs, dots, wrong);
        (void)kept; (void)worstAbs;
        static const bool forced = std::getenv("PL_ATOM_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) P.allOk = false;
        P.check = b;
        const std::string why = forced ? "the check was made to fail (test switch PL_ATOM_CARDFAIL)" : P.worst.empty() ? "its start-up check against the processor failed (" + std::string(b) + ")" : P.worst;
        G.end(checkName, msText(P.allOk ? "passed" : "FAILED", now - P.stepAt) + ": " + b);
        if (!P.allOk) { G.finished(false, why); off(which, why); return; }
        P.state = 1;
        std::printf("PLATOMCARD %s state=1 check=%s\n", nameOf(which), b);
        std::fflush(stdout);
        if (which == 1) C.setJumpReady(true);
    }
}

unsigned AtomCardPaths::jumpGroupsBuffer(render::Renderer2D& R, AtomLab& L) {
    Path& P = p_[1];
    render::AtomCard& C = R.atomCard();
    if (P.state != 1 || !L.gpuJump || !C.jumpReady() || !L.jumpDrawn() || !L.s.jb.many) return 0;
    static std::vector<unsigned char> fixedG;
    static std::uint64_t fixedEpoch = 0;
    if (fixedEpoch != L.s.jumpEpoch || fixedG.size() != std::size_t(2 * L.jumpDots())) { L.jumpFixedGroups(fixedG); fixedEpoch = L.s.jumpEpoch; }   // once per jump
    std::uint64_t t0 = 0, t1 = 0;
    L.jumpThresholds(t0, t1);
    if (!P.firstUse) { app::gpuJumpGuard().begin(stepName(1, "first full use")); P.firstUse = true; P.stepAt = nowS(); }
    if (!C.jumpGroups(fixedG.data(), L.jumpDots(), L.s.jumpEpoch, t0, t1)) return 0;
    if (!P.firstDone) C.fence();
    return C.jumpGroupsBuffer();
}

std::string AtomCardPaths::pathText(int which, const AtomLab& L, bool computeOk) const {
    const Path& P = p_[which & 1];
    const std::string what = which == 0 ? "the Slice's new dots: " : "the jump's groups: ";
    if (!computeOk) return what + "the processor (no OpenGL 4.3 compute on this PC)";
    if (which == 0 ? !L.gpuSlice : !L.gpuJump) return what + "the processor (" + (which == 0 ? "GPU Slice dots" : "GPU jump groups") + " switched off)";
    if (P.state == 1) return what + "the graphics card (its start-up check passed: " + P.check + ")";
    if (P.state == -1) return what + "the processor, because " + P.why;
    return what + "the graphics card once its start-up check has passed (made when " + (which == 0 ? "the Slice view" : "a jump in the every-electron Glow cloud") + " is first shown)";
}

}  // namespace atom
